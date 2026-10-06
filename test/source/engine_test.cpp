#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "ucilib/engine.hpp"

#include <catch2/catch_test_macros.hpp>

namespace
{

auto environment_variable(char const* name) -> std::string
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }
    std::string result {value};
    std::free(value);
    return result;
#else
    if (auto const* value = std::getenv(name)) {
        return value;
    }
    return {};
#endif
}

auto engine_path() -> std::string
{
    if (auto env = environment_variable("UCILIB_ENGINE_PATH"); !env.empty()) {
        return env;
    }
    return "stockfish";
}

auto start_engine_or_skip(uci::engine& eng) -> uci::engine_id
{
    auto result = eng.start(engine_path());
    if (!result) {
        if (environment_variable("UCILIB_ENGINE_PATH").empty()) {
            SKIP("stockfish not available");
        }
        REQUIRE(result.has_value());
        return {};
    }
    return *result;
}

class hanging_uci_engine
{
  public:
    hanging_uci_engine()
        : path_ {std::filesystem::temp_directory_path()
                 / "ucilib_hanging_uci_engine.sh"}
        , entered_ {path_.string() + ".entered"}
    {
        std::filesystem::remove(entered_);
        auto script = std::ofstream {path_};
        script << "#!/usr/bin/env sh\n"
                  "while IFS= read -r line; do\n"
                  "  if [ \"$line\" = uci ]; then\n"
                  "    : > '"
               << entered_.string()
               << "'\n"
                  "  fi\n"
                  "done\n";
        script.close();
        std::filesystem::permissions(path_,
                                     std::filesystem::perms::owner_read
                                         | std::filesystem::perms::owner_write
                                         | std::filesystem::perms::owner_exec);
    }

    ~hanging_uci_engine()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
        std::filesystem::remove(entered_, ignored);
    }

    auto wait_until_entered() const -> bool
    {
        auto const deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds {1};
        while (!std::filesystem::exists(entered_)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds {5});
        }
        return true;
    }

    auto path() const -> std::string { return path_.string(); }

  private:
    std::filesystem::path path_;
    std::filesystem::path entered_;
};

}  // namespace

TEST_CASE("engine start and quit", "[engine]")
{
    uci::engine eng;
    auto result = start_engine_or_skip(eng);
    CHECK_FALSE(result.name.empty());
    CHECK(eng.running());

    auto quit_result = eng.quit();
    CHECK(quit_result.has_value());
    CHECK_FALSE(eng.running());
}

TEST_CASE("engine start cancels a blocked UCI handshake", "[engine]")
{
    auto const fake_engine = hanging_uci_engine {};
    auto stop_source = std::stop_source {};
    auto result =
        std::promise<tl::expected<uci::engine_id, std::error_code>> {};
    auto finished = result.get_future();
    auto engine = uci::engine {};
    auto starter =
        std::thread {[&]() -> void
                     {
                         result.set_value(engine.start(
                             fake_engine.path(), stop_source.get_token()));
                     }};

    REQUIRE(fake_engine.wait_until_entered());
    stop_source.request_stop();
    REQUIRE(finished.wait_for(std::chrono::seconds {1})
            == std::future_status::ready);
    auto const started = finished.get();
    CHECK_FALSE(started.has_value());
    CHECK(started.error()
          == uci::make_error_code(uci::errc::operation_cancelled));
    CHECK_FALSE(engine.running());
    starter.join();
}

TEST_CASE("engine id and options populated after start", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));

    CHECK_FALSE(eng.id().name.empty());
    CHECK_FALSE(eng.id().author.empty());
    CHECK_FALSE(eng.options().empty());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine isready", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));

    auto ready = eng.is_ready();
    CHECK(ready.has_value());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine set_option and isready", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));

    auto opt_result = eng.set_option("Hash", "32");
    CHECK(opt_result.has_value());

    auto ready = eng.is_ready();
    CHECK(ready.has_value());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine go with depth and callbacks", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));

    std::atomic<int> info_count {0};
    std::atomic<bool> got_bestmove {false};
    std::string bestmove_str;

    eng.on_info([&](uci::info const& /*info*/) { ++info_count; });

    eng.on_bestmove(
        [&](uci::best_move const& bm)
        {
            bestmove_str = bm.move;
            got_bestmove.store(true, std::memory_order_relaxed);
        });

    REQUIRE(eng.set_position_startpos().has_value());
    REQUIRE(eng.go({.depth = 5}).has_value());

    // Wait for bestmove with timeout.
    for (int i = 0; i < 100 && !got_bestmove.load(std::memory_order_relaxed);
         ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CHECK(got_bestmove.load());
    CHECK(info_count.load() > 0);
    CHECK_FALSE(bestmove_str.empty());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine set_position with fen", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));
    REQUIRE(eng.is_ready().has_value());

    // Sicilian defense position
    auto result = eng.set_position(
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");
    CHECK(result.has_value());

    std::atomic<bool> got_bestmove {false};
    eng.on_bestmove([&](uci::best_move const& /*bm*/)
                    { got_bestmove.store(true); });

    REQUIRE(eng.go({.depth = 3}).has_value());

    for (int i = 0; i < 50 && !got_bestmove.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    CHECK(got_bestmove.load());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine multipv", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));

    REQUIRE(eng.set_option("MultiPV", "3").has_value());
    REQUIRE(eng.is_ready().has_value());
    REQUIRE(eng.set_position_startpos().has_value());

    std::atomic<int> max_multipv {0};
    eng.on_info(
        [&](uci::info const& info)
        {
            if (info.multipv.has_value()) {
                int current = max_multipv.load(std::memory_order_relaxed);
                while (*info.multipv > current) {
                    if (max_multipv.compare_exchange_weak(
                            current, *info.multipv, std::memory_order_relaxed))
                    {
                        break;
                    }
                }
            }
        });

    std::atomic<bool> got_bestmove {false};
    eng.on_bestmove([&](uci::best_move const& /*bm*/)
                    { got_bestmove.store(true); });

    REQUIRE(eng.go({.depth = 5}).has_value());

    for (int i = 0; i < 100 && !got_bestmove.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CHECK(max_multipv.load() >= 3);

    static_cast<void>(eng.quit());
}

TEST_CASE("engine crash isolation", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));
    REQUIRE(eng.set_position_startpos().has_value());
    REQUIRE(eng.go({.infinite = true}).has_value());

    // Let the engine start searching.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Force quit (simulates crash scenario).
    static_cast<void>(eng.quit());
    CHECK_FALSE(eng.running());

    // Subsequent commands should fail gracefully.
    auto result = eng.is_ready();
    CHECK_FALSE(result.has_value());
}

TEST_CASE("engine start with invalid path", "[engine]")
{
    uci::engine eng;
    auto result = eng.start("/nonexistent/engine");
    CHECK_FALSE(result.has_value());
    CHECK_FALSE(eng.running());
}

TEST_CASE("engine stop", "[engine]")
{
    uci::engine eng;
    static_cast<void>(start_engine_or_skip(eng));
    REQUIRE(eng.set_position_startpos().has_value());

    REQUIRE(eng.go({.infinite = true}).has_value());

    // Let it search briefly.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::atomic<bool> got_bestmove {false};
    eng.on_bestmove([&](uci::best_move const& /*bm*/)
                    { got_bestmove.store(true); });

    REQUIRE(eng.stop().has_value());

    for (int i = 0; i < 50 && !got_bestmove.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    CHECK(got_bestmove.load());

    static_cast<void>(eng.quit());
}

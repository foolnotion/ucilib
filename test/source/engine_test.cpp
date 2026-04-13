#include "ucilib/engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

auto engine_path() -> std::string
{
    if (auto const* env = std::getenv("UCILIB_ENGINE_PATH")) {
        return env;
    }
    return "stockfish";
}

} // namespace

TEST_CASE("engine start and quit", "[engine]")
{
    uci::engine eng;
    auto result = eng.start(engine_path());
    REQUIRE(result.has_value());
    CHECK_FALSE(result->name.empty());
    CHECK(eng.running());

    auto quit_result = eng.quit();
    CHECK(quit_result.has_value());
    CHECK_FALSE(eng.running());
}

TEST_CASE("engine id and options populated after start", "[engine]")
{
    uci::engine eng;
    auto result = eng.start(engine_path());
    REQUIRE(result.has_value());

    CHECK_FALSE(eng.id().name.empty());
    CHECK_FALSE(eng.id().author.empty());
    CHECK_FALSE(eng.options().empty());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine isready", "[engine]")
{
    uci::engine eng;
    REQUIRE(eng.start(engine_path()).has_value());

    auto ready = eng.is_ready();
    CHECK(ready.has_value());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine set_option and isready", "[engine]")
{
    uci::engine eng;
    REQUIRE(eng.start(engine_path()).has_value());

    auto opt_result = eng.set_option("Hash", "32");
    CHECK(opt_result.has_value());

    auto ready = eng.is_ready();
    CHECK(ready.has_value());

    static_cast<void>(eng.quit());
}

TEST_CASE("engine go with depth and callbacks", "[engine]")
{
    uci::engine eng;
    REQUIRE(eng.start(engine_path()).has_value());

    std::atomic<int> info_count{0};
    std::atomic<bool> got_bestmove{false};
    std::string bestmove_str;

    eng.on_info([&](uci::info const& /*info*/) { ++info_count; });

    eng.on_bestmove([&](uci::best_move const& bm) {
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
    REQUIRE(eng.start(engine_path()).has_value());
    REQUIRE(eng.is_ready().has_value());

    // Sicilian defense position
    auto result = eng.set_position(
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");
    CHECK(result.has_value());

    std::atomic<bool> got_bestmove{false};
    eng.on_bestmove(
        [&](uci::best_move const& /*bm*/) { got_bestmove.store(true); });

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
    REQUIRE(eng.start(engine_path()).has_value());

    REQUIRE(eng.set_option("MultiPV", "3").has_value());
    REQUIRE(eng.is_ready().has_value());
    REQUIRE(eng.set_position_startpos().has_value());

    std::atomic<int> max_multipv{0};
    eng.on_info([&](uci::info const& info) {
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

    std::atomic<bool> got_bestmove{false};
    eng.on_bestmove(
        [&](uci::best_move const& /*bm*/) { got_bestmove.store(true); });

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
    REQUIRE(eng.start(engine_path()).has_value());
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
    REQUIRE(eng.start(engine_path()).has_value());
    REQUIRE(eng.set_position_startpos().has_value());

    REQUIRE(eng.go({.infinite = true}).has_value());

    // Let it search briefly.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::atomic<bool> got_bestmove{false};
    eng.on_bestmove(
        [&](uci::best_move const& /*bm*/) { got_bestmove.store(true); });

    REQUIRE(eng.stop().has_value());

    for (int i = 0; i < 50 && !got_bestmove.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    CHECK(got_bestmove.load());

    static_cast<void>(eng.quit());
}

#include "ucilib/engine.hpp"

#include "detail/parser.hpp"

#include <reproc++/drain.hpp>
#include <reproc++/reproc.hpp>

#include <fmt/core.h>

#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <thread>

namespace uci {

struct engine::impl {
    reproc::process process;
    std::jthread reader_thread;

    engine_id id_;                            // NOLINT(readability-identifier-naming)
    std::vector<option_info> options_;          // NOLINT(readability-identifier-naming)

    info_callback on_info_;                     // NOLINT(readability-identifier-naming)
    bestmove_callback on_bestmove_;             // NOLINT(readability-identifier-naming)

    std::atomic<bool> running_{false};          // NOLINT(readability-identifier-naming)

    // Synchronization for blocking commands (uci, isready).
    std::mutex sync_mutex;
    std::promise<void> uciok_promise;
    std::future<void> uciok_future;
    std::promise<void> readyok_promise;
    std::future<void> readyok_future;
    bool waiting_uciok_{false};                 // NOLINT(readability-identifier-naming)
    bool waiting_readyok_{false};               // NOLINT(readability-identifier-naming)

    // Line buffer for accumulating partial reads.
    std::string line_buffer;

    auto send(std::string_view cmd) -> tl::expected<void, std::error_code>
    {
        if (!running_.load(std::memory_order_relaxed)) {
            return tl::unexpected(make_error_code(errc::engine_not_running));
        }

        auto msg = fmt::format("{}\n", cmd);
        auto [bytes, ec] = process.write(
            reinterpret_cast<uint8_t const*>(msg.data()), msg.size());

        if (ec) {
            return tl::unexpected(
                std::error_code(ec.value(), ec.category()));
        }
        if (bytes != msg.size()) {
            return tl::unexpected(make_error_code(errc::write_failed));
        }
        return {};
    }

    void dispatch_line(std::string_view line)
    {
        if (line == "uciok") {
            std::lock_guard lock(sync_mutex);
            if (waiting_uciok_) {
                waiting_uciok_ = false;
                uciok_promise.set_value();
            }
            return;
        }

        if (line == "readyok") {
            std::lock_guard lock(sync_mutex);
            if (waiting_readyok_) {
                waiting_readyok_ = false;
                readyok_promise.set_value();
            }
            return;
        }

        if (auto parsed = detail::parse_id(line)) {
            if (parsed->first == "name") {
                id_.name = std::move(parsed->second);
            } else if (parsed->first == "author") {
                id_.author = std::move(parsed->second);
            }
            return;
        }

        if (auto parsed = detail::parse_option(line)) {
            options_.push_back(std::move(*parsed));
            return;
        }

        if (auto parsed = detail::parse_info_line(line)) {
            if (on_info_) {
                try {
                    on_info_(*parsed);
                } catch (...) { // NOLINT(bugprone-empty-catch)
                }
            }
            return;
        }

        if (auto parsed = detail::parse_bestmove(line)) {
            if (on_bestmove_) {
                try {
                    on_bestmove_(*parsed);
                } catch (...) { // NOLINT(bugprone-empty-catch)
                }
            }
            return;
        }

        // Unknown line — silently ignored.
    }

    void process_buffer()
    {
        std::size_t pos = 0;
        while ((pos = line_buffer.find('\n')) != std::string::npos) {
            auto line = std::string_view(line_buffer).substr(0, pos);
            // Strip trailing \r if present.
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            dispatch_line(line);
            line_buffer.erase(0, pos + 1);
        }
    }

    void reader_loop()
    {
        auto sink = [this](reproc::stream /*stream*/, uint8_t const* buffer,
                           std::size_t size) -> std::error_code {
            if (size > 0) {
                line_buffer.append(reinterpret_cast<char const*>(buffer),
                                   size);
                process_buffer();
            }
            return {};
        };

        static_cast<void>(reproc::drain(process, sink, reproc::sink::null)); // NOLINT(bugprone-unused-return-value)
        running_.store(false, std::memory_order_relaxed);

        // Wake up any waiting futures in case the engine died unexpectedly.
        std::lock_guard lock(sync_mutex);
        if (waiting_uciok_) {
            waiting_uciok_ = false;
            try {
                uciok_promise.set_exception(
                    std::make_exception_ptr(std::runtime_error(
                        "engine exited before uciok")));
            } catch (...) { // NOLINT(bugprone-empty-catch)
            }
        }
        if (waiting_readyok_) {
            waiting_readyok_ = false;
            try {
                readyok_promise.set_exception(
                    std::make_exception_ptr(std::runtime_error(
                        "engine exited before readyok")));
            } catch (...) { // NOLINT(bugprone-empty-catch)
            }
        }
    }
};

engine::engine()
    : impl_(std::make_unique<impl>())
{
}

engine::~engine() noexcept
{
    if (impl_ && impl_->running_.load(std::memory_order_relaxed)) {
        // Best-effort quit.
        static_cast<void>(quit());
    }
}

engine::engine(engine&&) noexcept = default;
auto engine::operator=(engine&&) noexcept -> engine& = default;

auto engine::start(std::string const& path)
    -> tl::expected<engine_id, std::error_code>
{
    if (impl_->running_.load(std::memory_order_relaxed)) {
        return tl::unexpected(
            make_error_code(errc::invalid_argument));
    }

    // Reset state.
    impl_->id_ = {};
    impl_->options_.clear();
    impl_->line_buffer.clear();

    reproc::options opts;
    opts.redirect.err.type = reproc::redirect::discard;
    opts.stop = {
        {reproc::stop::wait, reproc::milliseconds(500)},
        {reproc::stop::terminate, reproc::milliseconds(500)},
        {reproc::stop::kill, reproc::milliseconds(500)},
    };

    std::vector<std::string> args = {path};
    auto ec = impl_->process.start(args, opts);
    if (ec) {
        return tl::unexpected(
            std::error_code(ec.value(), ec.category()));
    }

    impl_->running_.store(true, std::memory_order_relaxed);

    // Set up uciok synchronization.
    {
        std::lock_guard lock(impl_->sync_mutex);
        impl_->uciok_promise = std::promise<void>();
        impl_->uciok_future = impl_->uciok_promise.get_future();
        impl_->waiting_uciok_ = true;
    }

    // Start reader thread before sending "uci".
    impl_->reader_thread = std::jthread([this] { impl_->reader_loop(); });

    // Send "uci" command.
    auto send_result = impl_->send("uci");
    if (!send_result) {
        return tl::unexpected(send_result.error());
    }

    // Wait for uciok with a 10s timeout.
    auto status
        = impl_->uciok_future.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::timeout) {
        // Kill the engine — it didn't respond.
        static_cast<void>(quit());
        return tl::unexpected(
            make_error_code(errc::uci_handshake_timeout));
    }

    try {
        impl_->uciok_future.get();
    } catch (...) {
        return tl::unexpected(make_error_code(errc::engine_crashed));
    }

    return impl_->id_;
}

auto engine::quit() -> tl::expected<void, std::error_code>
{
    if (!impl_->running_.load(std::memory_order_relaxed)) {
        return {};
    }

    // Best-effort send quit.
    static_cast<void>(impl_->send("quit"));

    // Stop the process with escalating signals.
    impl_->process.stop({
        {reproc::stop::wait, reproc::milliseconds(500)},
        {reproc::stop::terminate, reproc::milliseconds(500)},
        {reproc::stop::kill, reproc::milliseconds(500)},
    });

    impl_->running_.store(false, std::memory_order_relaxed);

    // If quit() is called from a callback on the reader thread, joining
    // would deadlock.  Detach and let the thread finish naturally instead.
    if (impl_->reader_thread.joinable()) {
        if (impl_->reader_thread.get_id() == std::this_thread::get_id()) {
            impl_->reader_thread.detach();
        } else {
            impl_->reader_thread.join();
        }
    }

    return {};
}

auto engine::is_ready(milliseconds timeout)
    -> tl::expected<void, std::error_code>
{
    if (!impl_->running_.load(std::memory_order_relaxed)) {
        return tl::unexpected(make_error_code(errc::engine_not_running));
    }

    {
        std::lock_guard lock(impl_->sync_mutex);
        impl_->readyok_promise = std::promise<void>();
        impl_->readyok_future = impl_->readyok_promise.get_future();
        impl_->waiting_readyok_ = true;
    }

    auto send_result = impl_->send("isready");
    if (!send_result) {
        return tl::unexpected(send_result.error());
    }

    auto status = impl_->readyok_future.wait_for(timeout);
    if (status == std::future_status::timeout) {
        std::lock_guard lock(impl_->sync_mutex);
        impl_->waiting_readyok_ = false;
        return tl::unexpected(make_error_code(errc::ready_timeout));
    }

    try {
        impl_->readyok_future.get();
    } catch (...) {
        return tl::unexpected(make_error_code(errc::engine_crashed));
    }

    return {};
}

auto engine::set_option(std::string_view name, std::string_view value)
    -> tl::expected<void, std::error_code>
{
    if (value.empty()) {
        return impl_->send(fmt::format("setoption name {}", name));
    }
    return impl_->send(
        fmt::format("setoption name {} value {}", name, value));
}

auto engine::set_position(std::string_view fen,
                          std::vector<std::string> const& moves)
    -> tl::expected<void, std::error_code>
{
    auto cmd = fmt::format("position fen {}", fen);
    if (!moves.empty()) {
        cmd += " moves";
        for (auto const& m : moves) {
            cmd += ' ';
            cmd += m;
        }
    }
    return impl_->send(cmd);
}

auto engine::set_position_startpos(std::vector<std::string> const& moves)
    -> tl::expected<void, std::error_code>
{
    std::string cmd = "position startpos";
    if (!moves.empty()) {
        cmd += " moves";
        for (auto const& m : moves) {
            cmd += ' ';
            cmd += m;
        }
    }
    return impl_->send(cmd);
}

auto engine::go(go_params const& params)
    -> tl::expected<void, std::error_code>
{
    std::string cmd = "go";

    if (params.infinite) {
        cmd += " infinite";
    }
    if (params.ponder) {
        cmd += " ponder";
    }
    if (params.depth) {
        cmd += fmt::format(" depth {}", *params.depth);
    }
    if (params.nodes) {
        cmd += fmt::format(" nodes {}", *params.nodes);
    }
    if (params.mate) {
        cmd += fmt::format(" mate {}", *params.mate);
    }
    if (params.movetime) {
        cmd += fmt::format(" movetime {}", params.movetime->count());
    }
    if (params.wtime) {
        cmd += fmt::format(" wtime {}", params.wtime->count());
    }
    if (params.btime) {
        cmd += fmt::format(" btime {}", params.btime->count());
    }
    if (params.winc) {
        cmd += fmt::format(" winc {}", params.winc->count());
    }
    if (params.binc) {
        cmd += fmt::format(" binc {}", params.binc->count());
    }
    if (params.movestogo) {
        cmd += fmt::format(" movestogo {}", *params.movestogo);
    }
    if (!params.searchmoves.empty()) {
        cmd += " searchmoves";
        for (auto const& m : params.searchmoves) {
            cmd += ' ';
            cmd += m;
        }
    }

    return impl_->send(cmd);
}

auto engine::stop() -> tl::expected<void, std::error_code>
{
    return impl_->send("stop");
}

void engine::on_info(info_callback cb)
{
    impl_->on_info_ = std::move(cb);
}

void engine::on_bestmove(bestmove_callback cb)
{
    impl_->on_bestmove_ = std::move(cb);
}

auto engine::id() const -> engine_id const&
{
    return impl_->id_;
}

auto engine::options() const -> std::vector<option_info> const&
{
    return impl_->options_;
}

auto engine::running() const -> bool
{
    return impl_->running_.load(std::memory_order_relaxed);
}

} // namespace uci

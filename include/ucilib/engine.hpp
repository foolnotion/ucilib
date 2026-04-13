#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <tl/expected.hpp>

#include "ucilib/types.hpp"
#include "ucilib/ucilib_export.hpp"

namespace uci {

using milliseconds = std::chrono::milliseconds;

struct go_params {
    std::optional<int> depth;
    std::optional<int> nodes;
    std::optional<int> mate;
    std::optional<milliseconds> movetime;
    std::optional<milliseconds> wtime;
    std::optional<milliseconds> btime;
    std::optional<milliseconds> winc;
    std::optional<milliseconds> binc;
    std::optional<int> movestogo;
    bool infinite{};
    bool ponder{};
    std::vector<std::string> searchmoves;
};

class UCILIB_EXPORT engine
{
public:
    using info_callback = std::function<void(info const&)>;
    using bestmove_callback = std::function<void(best_move const&)>;

    engine();
    ~engine() noexcept;

    engine(engine&&) noexcept;
    auto operator=(engine&&) noexcept -> engine&;

    engine(engine const&) = delete;
    auto operator=(engine const&) -> engine& = delete;

    // Lifecycle
    auto start(std::string const& path)
        -> tl::expected<engine_id, std::error_code>;
    auto quit() -> tl::expected<void, std::error_code>;

    // UCI protocol
    auto is_ready(milliseconds timeout = milliseconds{5000})
        -> tl::expected<void, std::error_code>;
    auto set_option(std::string_view name, std::string_view value = "")
        -> tl::expected<void, std::error_code>;

    // Position
    auto set_position(std::string_view fen,
                      std::vector<std::string> const& moves = {})
        -> tl::expected<void, std::error_code>;
    auto set_position_startpos(std::vector<std::string> const& moves = {})
        -> tl::expected<void, std::error_code>;

    // Search
    auto go(go_params const& params = {})
        -> tl::expected<void, std::error_code>;
    auto stop() -> tl::expected<void, std::error_code>;

    // Callbacks
    void on_info(info_callback cb);
    void on_bestmove(bestmove_callback cb);

    // Query
    auto id() const -> engine_id const&;
    auto options() const -> std::vector<option_info> const&;
    auto running() const -> bool;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace uci

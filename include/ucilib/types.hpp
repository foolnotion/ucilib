#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace uci {

enum class score_type : std::uint8_t { cp, mate };

struct score {
    score_type type{};
    int value{};
    bool lower_bound{};
    bool upper_bound{};
};

struct info {
    std::optional<int> depth;
    std::optional<int> seldepth;
    std::optional<int> multipv;
    std::optional<score> score;
    std::optional<std::int64_t> nodes;
    std::optional<int> nps;
    std::optional<int> time_ms;
    std::optional<int> hashfull;
    std::vector<std::string> pv;
    std::optional<std::string> currmove;
    std::optional<int> currmovenumber;
    std::optional<std::string> string;
};

struct best_move {
    std::string move;
    std::optional<std::string> ponder;
};

struct engine_id {
    std::string name;
    std::string author;
};

enum class option_type : std::uint8_t { check, spin, combo, button, string };

struct option_info {
    std::string name;
    option_type type{};
    std::string default_value;
    std::optional<int> min;
    std::optional<int> max;
    std::vector<std::string> vars;
};

enum class errc : std::uint8_t {
    engine_not_running = 1,
    uci_handshake_timeout,
    ready_timeout,
    write_failed,
    engine_crashed,
    invalid_argument,
};

auto uci_category() -> std::error_category const&;
auto make_error_code(errc e) -> std::error_code;

} // namespace uci

template<>
struct std::is_error_code_enum<uci::errc> : std::true_type {};

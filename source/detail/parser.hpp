#pragma once

#include "ucilib/types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace uci::detail {

auto parse_info_line(std::string_view line) -> std::optional<info>;
auto parse_bestmove(std::string_view line) -> std::optional<best_move>;
auto parse_option(std::string_view line) -> std::optional<option_info>;
auto parse_id(std::string_view line)
    -> std::optional<std::pair<std::string, std::string>>;

} // namespace uci::detail

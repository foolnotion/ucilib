#include "detail/parser.hpp"

#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace uci::detail {

namespace {

// Split a string_view into whitespace-delimited tokens.
auto tokenize(std::string_view line) -> std::vector<std::string_view>
{
    std::vector<std::string_view> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        auto start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
        tokens.push_back(line.substr(start, i - start));
    }
    return tokens;
}

template<typename T>
auto parse_number(std::string_view sv) -> std::optional<T>
{
    T val{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) {
        return std::nullopt;
    }
    return val;
}

} // namespace

auto parse_info_line(std::string_view line) -> std::optional<info>
{
    auto tokens = tokenize(line);
    if (tokens.empty() || tokens[0] != "info") {
        return std::nullopt;
    }

    info result;
    std::size_t i = 1;

    while (i < tokens.size()) {
        auto const token = tokens[i];

        if (token == "depth" && i + 1 < tokens.size()) {
            result.depth = parse_number<int>(tokens[++i]);
        } else if (token == "seldepth" && i + 1 < tokens.size()) {
            result.seldepth = parse_number<int>(tokens[++i]);
        } else if (token == "multipv" && i + 1 < tokens.size()) {
            result.multipv = parse_number<int>(tokens[++i]);
        } else if (token == "nodes" && i + 1 < tokens.size()) {
            result.nodes = parse_number<std::int64_t>(tokens[++i]);
        } else if (token == "nps" && i + 1 < tokens.size()) {
            result.nps = parse_number<int>(tokens[++i]);
        } else if (token == "time" && i + 1 < tokens.size()) {
            result.time_ms = parse_number<int>(tokens[++i]);
        } else if (token == "hashfull" && i + 1 < tokens.size()) {
            result.hashfull = parse_number<int>(tokens[++i]);
        } else if (token == "currmove" && i + 1 < tokens.size()) {
            result.currmove = std::string(tokens[++i]);
        } else if (token == "currmovenumber" && i + 1 < tokens.size()) {
            result.currmovenumber = parse_number<int>(tokens[++i]);
        } else if (token == "score" && i + 1 < tokens.size()) {
            score sc;
            ++i;
            if (tokens[i] == "cp" && i + 1 < tokens.size()) {
                sc.type = score_type::cp;
                sc.value = parse_number<int>(tokens[++i]).value_or(0);
            } else if (tokens[i] == "mate" && i + 1 < tokens.size()) {
                sc.type = score_type::mate;
                sc.value = parse_number<int>(tokens[++i]).value_or(0);
            }
            // Check for lowerbound/upperbound after score value.
            if (i + 1 < tokens.size() && tokens[i + 1] == "lowerbound") {
                sc.lower_bound = true;
                ++i;
            } else if (i + 1 < tokens.size()
                       && tokens[i + 1] == "upperbound")
            {
                sc.upper_bound = true;
                ++i;
            }
            result.score = sc;
        } else if (token == "pv") {
            // All remaining tokens are PV moves.
            ++i;
            while (i < tokens.size()) {
                result.pv.emplace_back(tokens[i]);
                ++i;
            }
            break;
        } else if (token == "string") {
            // Everything after "string" is the message.
            ++i;
            if (i < tokens.size()) {
                auto pos = static_cast<std::size_t>(
                    tokens[i].data() - line.data());
                result.string = std::string(line.substr(pos));
            }
            break;
        }

        ++i;
    }

    return result;
}

auto parse_bestmove(std::string_view line) -> std::optional<best_move>
{
    auto tokens = tokenize(line);
    if (tokens.size() < 2 || tokens[0] != "bestmove") {
        return std::nullopt;
    }

    best_move result;
    result.move = std::string(tokens[1]);

    if (tokens.size() >= 4 && tokens[2] == "ponder") {
        result.ponder = std::string(tokens[3]);
    }

    return result;
}

auto parse_option(std::string_view line) -> std::optional<option_info>
{
    auto tokens = tokenize(line);
    if (tokens.size() < 5 || tokens[0] != "option" || tokens[1] != "name") {
        return std::nullopt;
    }

    // Find "type" keyword to delimit the option name.
    std::size_t type_idx = 0;
    for (std::size_t i = 2; i < tokens.size(); ++i) {
        if (tokens[i] == "type") {
            type_idx = i;
            break;
        }
    }
    if (type_idx == 0 || type_idx + 1 >= tokens.size()) {
        return std::nullopt;
    }

    option_info result;

    // Name is everything between "name" and "type".
    for (std::size_t i = 2; i < type_idx; ++i) {
        if (!result.name.empty()) {
            result.name += ' ';
        }
        result.name += tokens[i];
    }

    // Parse type.
    auto const type_str = tokens[type_idx + 1];
    if (type_str == "check") {
        result.type = option_type::check;
    } else if (type_str == "spin") {
        result.type = option_type::spin;
    } else if (type_str == "combo") {
        result.type = option_type::combo;
    } else if (type_str == "button") {
        result.type = option_type::button;
    } else if (type_str == "string") {
        result.type = option_type::string;
    } else {
        return std::nullopt;
    }

    // Collect consecutive tokens up to the next known attribute keyword.
    // 'default' and 'var' values can legally be multi-word in the UCI spec.
    auto collect_value = [&](std::size_t start) -> std::pair<std::string, std::size_t> {
        std::string val;
        std::size_t j = start;
        while (j < tokens.size()) {
            auto const t = tokens[j];
            if (t == "default" || t == "min" || t == "max" || t == "var") {
                break;
            }
            if (!val.empty()) {
                val += ' ';
            }
            val += t;
            ++j;
        }
        return {std::move(val), j};
    };

    // Parse remaining key-value pairs.
    std::size_t i = type_idx + 2;
    while (i < tokens.size()) {
        if (tokens[i] == "default") {
            auto [val, next] = collect_value(i + 1);
            result.default_value = std::move(val);
            i = next;
            continue;
        }
        if (tokens[i] == "min" && i + 1 < tokens.size()) {
            result.min = parse_number<int>(tokens[++i]);
        } else if (tokens[i] == "max" && i + 1 < tokens.size()) {
            result.max = parse_number<int>(tokens[++i]);
        } else if (tokens[i] == "var") {
            auto [val, next] = collect_value(i + 1);
            if (!val.empty()) {
                result.vars.emplace_back(std::move(val));
            }
            i = next;
            continue;
        }
        ++i;
    }

    return result;
}

auto parse_id(std::string_view line)
    -> std::optional<std::pair<std::string, std::string>>
{
    auto tokens = tokenize(line);
    if (tokens.size() < 3 || tokens[0] != "id") {
        return std::nullopt;
    }

    auto key = std::string(tokens[1]);

    // Value is everything after "id <key> ".
    std::string value;
    for (std::size_t i = 2; i < tokens.size(); ++i) {
        if (!value.empty()) {
            value += ' ';
        }
        value += tokens[i];
    }

    return std::pair{std::move(key), std::move(value)};
}

} // namespace uci::detail

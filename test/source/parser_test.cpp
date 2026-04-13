#include "detail/parser.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace uci;
using namespace uci::detail;

// NOLINTBEGIN(bugprone-unchecked-optional-access)
TEST_CASE("parse_info_line: depth, score cp, pv", "[parser]")
{
    auto result = parse_info_line(
        "info depth 20 seldepth 25 multipv 1 score cp 35 "
        "nodes 1234567 nps 2000000 time 617 pv e2e4 e7e5 g1f3");
    REQUIRE(result.has_value());
    auto const& i = *result;
    CHECK(i.depth == 20);
    CHECK(i.seldepth == 25);
    CHECK(i.multipv == 1);
    REQUIRE(i.score.has_value());
    CHECK(i.score->type == score_type::cp);
    CHECK(i.score->value == 35);
    CHECK_FALSE(i.score->lower_bound);
    CHECK_FALSE(i.score->upper_bound);
    CHECK(i.nodes == 1234567);
    CHECK(i.nps == 2000000);
    CHECK(i.time_ms == 617);
    REQUIRE(i.pv.size() == 3);
    CHECK(i.pv[0] == "e2e4");
    CHECK(i.pv[1] == "e7e5");
    CHECK(i.pv[2] == "g1f3");
}

TEST_CASE("parse_info_line: score mate", "[parser]")
{
    auto result = parse_info_line("info depth 15 score mate 3 pv e2e4");
    REQUIRE(result.has_value());
    REQUIRE(result->score.has_value());
    CHECK(result->score->type == score_type::mate);
    CHECK(result->score->value == 3);
    REQUIRE(result->pv.size() == 1);
    CHECK(result->pv[0] == "e2e4");
}

TEST_CASE("parse_info_line: score with lowerbound", "[parser]")
{
    auto result = parse_info_line("info depth 10 score cp 100 lowerbound");
    REQUIRE(result.has_value());
    REQUIRE(result->score.has_value());
    CHECK(result->score->lower_bound);
    CHECK_FALSE(result->score->upper_bound);
}

TEST_CASE("parse_info_line: score with upperbound", "[parser]")
{
    auto result = parse_info_line("info depth 10 score cp -50 upperbound");
    REQUIRE(result.has_value());
    REQUIRE(result->score.has_value());
    CHECK(result->score->upper_bound);
    CHECK(result->score->value == -50);
}

TEST_CASE("parse_info_line: string message", "[parser]")
{
    auto result = parse_info_line("info string some debug message here");
    REQUIRE(result.has_value());
    REQUIRE(result->string.has_value());
    CHECK(*result->string == "some debug message here");
}

TEST_CASE("parse_info_line: currmove and currmovenumber", "[parser]")
{
    auto result
        = parse_info_line("info depth 5 currmove e2e4 currmovenumber 1");
    REQUIRE(result.has_value());
    CHECK(result->depth == 5);
    REQUIRE(result->currmove.has_value());
    CHECK(*result->currmove == "e2e4");
    CHECK(result->currmovenumber == 1);
}

TEST_CASE("parse_info_line: hashfull", "[parser]")
{
    auto result = parse_info_line("info depth 10 hashfull 500");
    REQUIRE(result.has_value());
    CHECK(result->hashfull == 500);
}

TEST_CASE("parse_info_line: rejects non-info line", "[parser]")
{
    CHECK_FALSE(parse_info_line("bestmove e2e4").has_value());
    CHECK_FALSE(parse_info_line("").has_value());
    CHECK_FALSE(parse_info_line("readyok").has_value());
}

TEST_CASE("parse_bestmove: move with ponder", "[parser]")
{
    auto result = parse_bestmove("bestmove e2e4 ponder e7e5");
    REQUIRE(result.has_value());
    CHECK(result->move == "e2e4");
    REQUIRE(result->ponder.has_value());
    CHECK(*result->ponder == "e7e5");
}

TEST_CASE("parse_bestmove: move without ponder", "[parser]")
{
    auto result = parse_bestmove("bestmove e2e4");
    REQUIRE(result.has_value());
    CHECK(result->move == "e2e4");
    CHECK_FALSE(result->ponder.has_value());
}

TEST_CASE("parse_bestmove: rejects non-bestmove", "[parser]")
{
    CHECK_FALSE(parse_bestmove("info depth 5").has_value());
    CHECK_FALSE(parse_bestmove("").has_value());
}

TEST_CASE("parse_option: spin type", "[parser]")
{
    auto result = parse_option(
        "option name Hash type spin default 16 min 1 max 1024");
    REQUIRE(result.has_value());
    CHECK(result->name == "Hash");
    CHECK(result->type == option_type::spin);
    CHECK(result->default_value == "16");
    CHECK(result->min == 1);
    CHECK(result->max == 1024);
}

TEST_CASE("parse_option: check type", "[parser]")
{
    auto result
        = parse_option("option name UCI_Chess960 type check default false");
    REQUIRE(result.has_value());
    CHECK(result->name == "UCI_Chess960");
    CHECK(result->type == option_type::check);
    CHECK(result->default_value == "false");
}

TEST_CASE("parse_option: combo type with vars", "[parser]")
{
    auto result = parse_option(
        "option name Style type combo default Normal "
        "var Solid var Normal var Risky");
    REQUIRE(result.has_value());
    CHECK(result->name == "Style");
    CHECK(result->type == option_type::combo);
    CHECK(result->default_value == "Normal");
    REQUIRE(result->vars.size() == 3);
    CHECK(result->vars[0] == "Solid");
    CHECK(result->vars[1] == "Normal");
    CHECK(result->vars[2] == "Risky");
}

TEST_CASE("parse_option: button type", "[parser]")
{
    auto result = parse_option("option name Clear Hash type button");
    REQUIRE(result.has_value());
    CHECK(result->name == "Clear Hash");
    CHECK(result->type == option_type::button);
}

TEST_CASE("parse_option: string type", "[parser]")
{
    auto result
        = parse_option("option name NalimovPath type string default <empty>");
    REQUIRE(result.has_value());
    CHECK(result->name == "NalimovPath");
    CHECK(result->type == option_type::string);
    CHECK(result->default_value == "<empty>");
}

TEST_CASE("parse_option: rejects malformed", "[parser]")
{
    CHECK_FALSE(parse_option("option name").has_value());
    CHECK_FALSE(parse_option("").has_value());
    CHECK_FALSE(parse_option("info depth 5").has_value());
}

TEST_CASE("parse_id: name", "[parser]")
{
    auto result = parse_id("id name Stockfish 16");
    REQUIRE(result.has_value());
    CHECK(result->first == "name");
    CHECK(result->second == "Stockfish 16");
}

TEST_CASE("parse_id: author", "[parser]")
{
    auto result = parse_id("id author T. Romstad, M. Costalba, J. Kiiski");
    REQUIRE(result.has_value());
    CHECK(result->first == "author");
    CHECK(result->second == "T. Romstad, M. Costalba, J. Kiiski");
}

TEST_CASE("parse_id: rejects non-id", "[parser]")
{
    CHECK_FALSE(parse_id("info depth 5").has_value());
    CHECK_FALSE(parse_id("").has_value());
    CHECK_FALSE(parse_id("id").has_value());
}
// NOLINTEND(bugprone-unchecked-optional-access)

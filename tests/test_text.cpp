#include "core/text.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace fin;

TEST_CASE("trim removes ASCII whitespace on both ends", "[text]") {
  CHECK(trim("  a b \t\n") == "a b");
  CHECK(trim("") == "");
  CHECK(trim(" \t\r\n\f\v") == "");
  CHECK(trim("x") == "x");
}

TEST_CASE("all_digits", "[text]") {
  CHECK(all_digits("0123456789"));
  CHECK(all_digits("")); // callers check emptiness themselves
  CHECK_FALSE(all_digits("12a"));
  CHECK_FALSE(all_digits("-1"));
  CHECK_FALSE(all_digits("1 "));
}

TEST_CASE("text_length counts code points of well-formed text", "[text]") {
  CHECK(text_length("") == 0);
  CHECK(text_length("abc") == 3);
  CHECK(text_length("ñandú") == 5);
  CHECK(text_length("€") == 1);
  CHECK(text_length("😀") == 1);
}

TEST_CASE("text_length rejects malformed UTF-8", "[text]") {
  const auto bad = GENERATE(std::string_view{"\xC0\xAF"},         // overlong
                            std::string_view{"\xED\xA0\x80"},     // surrogate
                            std::string_view{"\xF4\x90\x80\x80"}, // > U+10FFFF
                            std::string_view{"\xE2\x82"},         // truncated
                            std::string_view{"\x80"},             // stray tail
                            std::string_view{"\xFF"});
  CHECK_FALSE(text_length(bad).has_value());
}

TEST_CASE("text_length rejects control characters", "[text]") {
  const auto bad = GENERATE(std::string_view{"a\tb"}, std::string_view{"a\nb"},
                            std::string_view{"a\x7F"},
                            std::string_view{"\xC2\x85"}); // C1 (NEL)
  CHECK_FALSE(text_length(bad).has_value());
}

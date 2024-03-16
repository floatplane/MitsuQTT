#pragma once

#include <ArduinoJson.h>
#include <doctest.h>

// clang-format off
/* generated via
cat test/test_ministache/mustache_specs/partials.json | jq -r '.tests[] | {name: .name, desc: .desc, data: .data|tojson, template: .template, expected: .expected, partials: .partials|to_entries|map("{\(.key|tojson), \(.value|tojson)}")|join(", ")} | .partials |= "{"+.+"}" | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"(\(.data))\"); CHECK_MESSAGE(ministache::render(R\"(\(.template))\", data, \(.partials)) == R\"(\(.expected))\", R\"(\(.desc))\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("ministache/specs/partials");

TEST_CASE("Basic Behavior") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(
      ministache::render(R"("{{>text}}")", data, {{"text", "from partial"}}) == R"("from partial")",
      R"(The greater-than operator should expand to the named partial.)");
}

TEST_CASE("Failed Lookup") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"("{{>text}}")", data, {}) == R"("")",
                R"(The empty string should be used when the named partial is not found.)");
}

TEST_CASE("Context") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"text":"content"})");
  CHECK_MESSAGE(ministache::render(R"("{{>partial}}")", data, {{"partial", "*{{text}}*"}}) ==
                    R"("*content*")",
                R"(The greater-than operator should operate within the current context.)");
}

TEST_CASE("Recursion") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"content":"X","nodes":[{"content":"Y","nodes":[]}]})");
  CHECK_MESSAGE(
      ministache::render(R"({{>node}})", data,
                         {{"node", "{{content}}<{{#nodes}}{{>node}}{{/nodes}}>"}}) == R"(X<Y<>>)",
      R"(The greater-than operator should properly recurse.)");
}

TEST_CASE("Nested") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":"hello","b":"world"})");
  CHECK_MESSAGE(ministache::render(R"({{>outer}})", data,
                                   {{"outer", "*{{a}} {{>inner}}*"}, {"inner", "{{b}}!"}}) ==
                    R"(*hello world!*)",
                R"(The greater-than operator should work from within partials.)");
}

TEST_CASE("Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(| {{>partial}} |)", data, {{"partial", "\t|\t"}}) ==
                    R"(| 	|	 |)",
                R"(The greater-than operator should not alter surrounding whitespace.)");
}

TEST_CASE("Inline Indentation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"data":"|"})");
  CHECK_MESSAGE(ministache::render(R"(  {{data}}  {{> partial}}
)",
                                   data, {{"partial", ">\n>"}}) == R"(  |  >
>
)",
                R"(Whitespace should be left untouched.)");
}

TEST_CASE("Standalone Line Endings") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(|
{{>partial}}
|)",
                                   data, {{"partial", ">"}}) == R"(|
>|)",
                R"("\r\n" should be considered a newline for standalone tags.)");
}

TEST_CASE("Standalone Without Previous Line") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(  {{>partial}}
>)",
                                   data, {{"partial", ">\n>"}}) == R"(  >
  >>)",
                R"(Standalone tags should not require a newline to precede them.)");
}

TEST_CASE("Standalone Without Newline") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(>
  {{>partial}})",
                                   data, {{"partial", ">\n>"}}) == R"(>
  >
  >)",
                R"(Standalone tags should not require a newline to follow them.)");
}

TEST_CASE("Standalone Indentation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"content":"<\n->"})");
  CHECK_MESSAGE(ministache::render(R"(\
 {{>partial}}
/
)",
                                   data, {{"partial", "|\n{{{content}}}\n|\n"}}) == R"(\
 |
 <
->
 |
/
)",
                R"(Each line of the partial should be indented before rendering.)");
}

TEST_CASE("Padding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"boolean":true})");
  CHECK_MESSAGE(ministache::render(R"(|{{> partial }}|)", data, {{"partial", "[]"}}) == R"(|[]|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_SUITE_END();
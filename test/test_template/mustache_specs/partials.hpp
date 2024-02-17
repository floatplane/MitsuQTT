#pragma once

#include <ArduinoJson.h>
#include <doctest.h>

// clang-format off
/* generated via
cat test/test_template/mustache_specs/partials.json| jq -r '.tests[] | .partials |= to_entries | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"(\(.data|tojson))\"); CHECK_MESSAGE(Template(R\"(\(.template))\").render(data, \(.partials|tojson)) == R\"(\(.expected))\", R\"(\(.desc))\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("mustache/partials");

TEST_CASE("Basic Behavior") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(
      Template(R"("{{>text}}")").render(data, {{"text", "from partial"}}) == R"("from partial")",
      R"(The greater-than operator should expand to the named partial.)");
}

TEST_CASE("Failed Lookup") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"("{{>text}}")").render(data, {}) == R"("")",
                R"(The empty string should be used when the named partial is not found.)");
}

TEST_CASE("Context") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"text":"content"})");
  CHECK_MESSAGE(
      Template(R"("{{>partial}}")").render(data, {{"partial", "*{{text}}*"}}) == R"("*content*")",
      R"(The greater-than operator should operate within the current context.)");
}

TEST_CASE("Recursion") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"content":"X","nodes":[{"content":"Y","nodes":[]}]})");
  CHECK_MESSAGE(Template(R"({{>node}})")
                        .render(data, {{"node", "{{content}}<{{#nodes}}{{>node}}{{/nodes}}>"}}) ==
                    R"(X<Y<>>)",
                R"(The greater-than operator should properly recurse.)");
}

TEST_CASE("Nested") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":"hello","b":"world"})");
  CHECK_MESSAGE(Template(R"({{>outer}})")
                        .render(data, {{"outer", "*{{a}} {{>inner}}*"}, {"inner", "{{b}}!"}}) ==
                    R"(*hello world!*)",
                R"(The greater-than operator should work from within partials.)");
}

TEST_CASE("Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(| {{>partial}} |)").render(data, {{"partial", "\t|\t"}}) ==
                    R"(| 	|	 |)",
                R"(The greater-than operator should not alter surrounding whitespace.)");
}

TEST_CASE("Inline Indentation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"data":"|"})");
  CHECK_MESSAGE(Template(R"(  {{data}}  {{> partial}}
)")
                        .render(data, {{"partial", ">\n>"}}) == R"(  |  >
>
)",
                R"(Whitespace should be left untouched.)");
}

TEST_CASE("Standalone Line Endings") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(|
{{>partial}}
|)")
                        .render(data, {{"partial", ">"}}) == R"(|
>|)",
                R"("\r\n" should be considered a newline for standalone tags.)");
}

TEST_CASE("Standalone Without Previous Line") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(  {{>partial}}
>)")
                        .render(data, {{"partial", ">\n>"}}) == R"(  >
  >>)",
                R"(Standalone tags should not require a newline to precede them.)");
}

TEST_CASE("Standalone Without Newline") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(>
  {{>partial}})")
                        .render(data, {{"partial", ">\n>"}}) == R"(>
  >
  >)",
                R"(Standalone tags should not require a newline to follow them.)");
}

TEST_CASE("Standalone Indentation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"content":"<\n->"})");
  CHECK_MESSAGE(Template(R"(\
 {{>partial}}
/
)")
                        .render(data, {{"partial", "|\n{{{content}}}\n|\n"}}) == R"(\
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
  CHECK_MESSAGE(Template(R"(|{{> partial }}|)").render(data, {{"partial", "[]"}}) == R"(|[]|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_SUITE_END();
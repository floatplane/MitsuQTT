#pragma once

#include <ArduinoJson.h>
#include <doctest.h>

// clang-format off
/* generated via
cat test/native/test_template/mustache_specs/interpolation.json| jq -r '.tests[] | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"(\(.data))\"); CHECK_MESSAGE(Template(R\"(\(.template|rtrimstr("\n")))\").render(data) == R\"(\(.expected|rtrimstr("\n")))\", R\"(\(.desc))\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("mustache/interpolation");

TEST_CASE("No Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(Hello from {Mustache}!)").render(data) == R"(Hello from {Mustache}!)",
                R"(Mustache-free templates should render as-is.)");
}

TEST_CASE("Basic Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"subject":"world"})");
  CHECK_MESSAGE(Template(R"(Hello, {{subject}}!)").render(data) == R"(Hello, world!)",
                R"(Unadorned tags should interpolate content into the template.)");
}

TEST_CASE("HTML Escaping") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"forbidden":"& \" < >"})");
  CHECK_MESSAGE(
      Template(R"(These characters should be HTML escaped: {{forbidden}})").render(data) ==
          R"(These characters should be HTML escaped: &amp; &quot; &lt; &gt;)",
      R"(Basic interpolation should be HTML escaped.)");
}

TEST_CASE("Triple Mustache") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"forbidden":"& \" < >"})");
  CHECK_MESSAGE(
      Template(R"(These characters should not be HTML escaped: {{{forbidden}}})").render(data) ==
          R"(These characters should not be HTML escaped: & " < >)",
      R"(Triple mustaches should interpolate without HTML escaping.)");
}

TEST_CASE("Ampersand") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"forbidden":"& \" < >"})");
  CHECK_MESSAGE(
      Template(R"(These characters should not be HTML escaped: {{&forbidden}})").render(data) ==
          R"(These characters should not be HTML escaped: & " < >)",
      R"(Ampersand should interpolate without HTML escaping.)");
}

TEST_CASE("Basic Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"mph":85})");
  CHECK_MESSAGE(Template(R"("{{mph}} miles an hour!")").render(data) == R"("85 miles an hour!")",
                R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Triple Mustache Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"mph":85})");
  CHECK_MESSAGE(Template(R"("{{{mph}}} miles an hour!")").render(data) == R"("85 miles an hour!")",
                R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Ampersand Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"mph":85})");
  CHECK_MESSAGE(Template(R"("{{&mph}} miles an hour!")").render(data) == R"("85 miles an hour!")",
                R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Basic Decimal Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"power":1.21})");
  CHECK_MESSAGE(Template(R"("{{power}} jiggawatts!")").render(data) == R"("1.21 jiggawatts!")",
                R"(Decimals should interpolate seamlessly with proper significance.)");
}

TEST_CASE("Triple Mustache Decimal Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"power":1.21})");
  CHECK_MESSAGE(Template(R"("{{{power}}} jiggawatts!")").render(data) == R"("1.21 jiggawatts!")",
                R"(Decimals should interpolate seamlessly with proper significance.)");
}

TEST_CASE("Ampersand Decimal Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"power":1.21})");
  CHECK_MESSAGE(Template(R"("{{&power}} jiggawatts!")").render(data) == R"("1.21 jiggawatts!")",
                R"(Decimals should interpolate seamlessly with proper significance.)");
}

TEST_CASE("Basic Null Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"cannot":null})");
  CHECK_MESSAGE(Template(R"(I ({{cannot}}) be seen!)").render(data) == R"(I () be seen!)",
                R"(Nulls should interpolate as the empty string.)");
}

TEST_CASE("Triple Mustache Null Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"cannot":null})");
  CHECK_MESSAGE(Template(R"(I ({{{cannot}}}) be seen!)").render(data) == R"(I () be seen!)",
                R"(Nulls should interpolate as the empty string.)");
}

TEST_CASE("Ampersand Null Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"cannot":null})");
  CHECK_MESSAGE(Template(R"(I ({{&cannot}}) be seen!)").render(data) == R"(I () be seen!)",
                R"(Nulls should interpolate as the empty string.)");
}

TEST_CASE("Basic Context Miss Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(I ({{cannot}}) be seen!)").render(data) == R"(I () be seen!)",
                R"(Failed context lookups should default to empty strings.)");
}

TEST_CASE("Triple Mustache Context Miss Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(I ({{{cannot}}}) be seen!)").render(data) == R"(I () be seen!)",
                R"(Failed context lookups should default to empty strings.)");
}

TEST_CASE("Ampersand Context Miss Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Template(R"(I ({{&cannot}}) be seen!)").render(data) == R"(I () be seen!)",
                R"(Failed context lookups should default to empty strings.)");
}

#if 0
TEST_CASE("Dotted Names - Basic Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"person":{"name":"Joe"}})");
  CHECK_MESSAGE(Template(R"("{{person.name}}" == "{{#person}}{{name}}{{/person}}")").render(data) ==
                    R"("Joe" == "Joe")",
                R"(Dotted names should be considered a form of shorthand for sections.)");
}

TEST_CASE("Dotted Names - Triple Mustache Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"person":{"name":"Joe"}})");
  CHECK_MESSAGE(
      Template(R"("{{{person.name}}}" == "{{#person}}{{{name}}}{{/person}}")").render(data) ==
          R"("Joe" == "Joe")",
      R"(Dotted names should be considered a form of shorthand for sections.)");
}

TEST_CASE("Dotted Names - Ampersand Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"person":{"name":"Joe"}})");
  CHECK_MESSAGE(
      Template(R"("{{&person.name}}" == "{{#person}}{{&name}}{{/person}}")").render(data) ==
          R"("Joe" == "Joe")",
      R"(Dotted names should be considered a form of shorthand for sections.)");
}
#endif

TEST_CASE("Dotted Names - Arbitrary Depth") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{"b":{"c":{"d":{"e":{"name":"Phil"}}}}}})");
  CHECK_MESSAGE(Template(R"("{{a.b.c.d.e.name}}" == "Phil")").render(data) == R"("Phil" == "Phil")",
                R"(Dotted names should be functional to any level of nesting.)");
}

TEST_CASE("Dotted Names - Broken Chains") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{}})");
  CHECK_MESSAGE(Template(R"("{{a.b.c}}" == "")").render(data) == R"("" == "")",
                R"(Any falsey value prior to the last part of the name should yield ''.)");
}

TEST_CASE("Dotted Names - Broken Chain Resolution") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{"b":{}},"c":{"name":"Jim"}})");
  CHECK_MESSAGE(Template(R"("{{a.b.c.name}}" == "")").render(data) == R"("" == "")",
                R"(Each part of a dotted name should resolve only against its parent.)");
}

#if 0
TEST_CASE("Dotted Names - Initial Resolution") {
  ArduinoJson::JsonDocument data;
  deserializeJson(
      data,
      R"({"a":{"b":{"c":{"d":{"e":{"name":"Phil"}}}}},"b":{"c":{"d":{"e":{"name":"Wrong"}}}}})");
  CHECK_MESSAGE(
      Template(R"("{{#a}}{{b.c.d.e.name}}{{/a}}" == "Phil")").render(data) == R"("Phil" == "Phil")",
      R"(The first part of a dotted name should resolve as any other name.)");
}

TEST_CASE("Dotted Names - Context Precedence") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{"b":{}},"b":{"c":"ERROR"}})");
  CHECK_MESSAGE(Template(R"({{#a}}{{b.c}}{{/a}})").render(data) == R"()",
                R"(Dotted names should be resolved against former resolutions.)");
}

TEST_CASE("Implicit Iterators - Basic Interpolation") {
  ArduinoJson::JsonDocument data;
  auto error = deserializeJson(data, R"(world)");
  CHECK_MESSAGE(Template(R"(Hello, {{.}}!)").render(data) == R"(Hello, world!)",
                R"(Unadorned tags should interpolate content into the template.)");
}

TEST_CASE("Implicit Iterators - HTML Escaping") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"(& " < >)");
  CHECK_MESSAGE(Template(R"(These characters should be HTML escaped: {{.}})").render(data) ==
                    R"(These characters should be HTML escaped: &amp; &quot; &lt; &gt;)",
                R"(Basic interpolation should be HTML escaped.)");
}

TEST_CASE("Implicit Iterators - Triple Mustache") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"(& " < >)");
  CHECK_MESSAGE(Template(R"(These characters should not be HTML escaped: {{{.}}})").render(data) ==
                    R"(These characters should not be HTML escaped: & " < >)",
                R"(Triple mustaches should interpolate without HTML escaping.)");
}

TEST_CASE("Implicit Iterators - Ampersand") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"(& " < >)");
  CHECK_MESSAGE(Template(R"(These characters should not be HTML escaped: {{&.}})").render(data) ==
                    R"(These characters should not be HTML escaped: & " < >)",
                R"(Ampersand should interpolate without HTML escaping.)");
}

TEST_CASE("Implicit Iterators - Basic Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"(85)");
  CHECK_MESSAGE(Template(R"("{{.}} miles an hour!")").render(data) == R"("85 miles an hour!")",
                R"(Integers should interpolate seamlessly.)");
}
#endif

TEST_CASE("Interpolation - Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(| {{string}} |)").render(data) == R"(| --- |)",
                R"(Interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Triple Mustache - Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(| {{{string}}} |)").render(data) == R"(| --- |)",
                R"(Interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Ampersand - Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(| {{&string}} |)").render(data) == R"(| --- |)",
                R"(Interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Interpolation - Standalone") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(  {{string}})").render(data) == R"(  ---)",
                R"(Standalone interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Triple Mustache - Standalone") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(  {{{string}}})").render(data) == R"(  ---)",
                R"(Standalone interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Ampersand - Standalone") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(  {{&string}})").render(data) == R"(  ---)",
                R"(Standalone interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Interpolation With Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(|{{ string }}|)").render(data) == R"(|---|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_CASE("Triple Mustache With Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(|{{{ string }}}|)").render(data) == R"(|---|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_CASE("Ampersand With Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(Template(R"(|{{& string }}|)").render(data) == R"(|---|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_SUITE_END();
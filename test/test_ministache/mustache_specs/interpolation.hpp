#pragma once

#include <ArduinoJson.h>
#include <doctest.h>

// clang-format off
/* generated via
cat test/test_ministache/mustache_specs/interpolation.json| jq -r '.tests[] | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"(\(.data|tojson))\"); CHECK_MESSAGE(ministache::render(R\"(\(.template))\", data) == R\"(\(.expected))\", R\"(\(.desc))\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("minimustache/specs/interpolation");

TEST_CASE("No Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(Hello from {Mustache}!
)",
                                   data) == R"(Hello from {Mustache}!
)",
                R"(Mustache-free templates should render as-is.)");
}

TEST_CASE("Basic Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"subject":"world"})");
  CHECK_MESSAGE(ministache::render(R"(Hello, {{subject}}!
)",
                                   data) == R"(Hello, world!
)",
                R"(Unadorned tags should interpolate content into the template.)");
}

TEST_CASE("HTML Escaping") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"forbidden":"& \" < >"})");
  CHECK_MESSAGE(
      ministache::render(R"(These characters should be HTML escaped: {{forbidden}}
)",
                         data) == R"(These characters should be HTML escaped: &amp; &quot; &lt; &gt;
)",
      R"(Basic interpolation should be HTML escaped.)");
}

TEST_CASE("Triple Mustache") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"forbidden":"& \" < >"})");
  CHECK_MESSAGE(ministache::render(R"(These characters should not be HTML escaped: {{{forbidden}}}
)",
                                   data) == R"(These characters should not be HTML escaped: & " < >
)",
                R"(Triple mustaches should interpolate without HTML escaping.)");
}

TEST_CASE("Ampersand") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"forbidden":"& \" < >"})");
  CHECK_MESSAGE(ministache::render(R"(These characters should not be HTML escaped: {{&forbidden}}
)",
                                   data) == R"(These characters should not be HTML escaped: & " < >
)",
                R"(Ampersand should interpolate without HTML escaping.)");
}

TEST_CASE("Basic Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"mph":85})");
  CHECK_MESSAGE(ministache::render(R"("{{mph}} miles an hour!")", data) == R"("85 miles an hour!")",
                R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Triple Mustache Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"mph":85})");
  CHECK_MESSAGE(
      ministache::render(R"("{{{mph}}} miles an hour!")", data) == R"("85 miles an hour!")",
      R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Ampersand Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"mph":85})");
  CHECK_MESSAGE(
      ministache::render(R"("{{&mph}} miles an hour!")", data) == R"("85 miles an hour!")",
      R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Basic Decimal Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"power":1.21})");
  CHECK_MESSAGE(ministache::render(R"("{{power}} jiggawatts!")", data) == R"("1.21 jiggawatts!")",
                R"(Decimals should interpolate seamlessly with proper significance.)");
}

TEST_CASE("Triple Mustache Decimal Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"power":1.21})");
  CHECK_MESSAGE(ministache::render(R"("{{{power}}} jiggawatts!")", data) == R"("1.21 jiggawatts!")",
                R"(Decimals should interpolate seamlessly with proper significance.)");
}

TEST_CASE("Ampersand Decimal Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"power":1.21})");
  CHECK_MESSAGE(ministache::render(R"("{{&power}} jiggawatts!")", data) == R"("1.21 jiggawatts!")",
                R"(Decimals should interpolate seamlessly with proper significance.)");
}

TEST_CASE("Basic Null Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"cannot":null})");
  CHECK_MESSAGE(ministache::render(R"(I ({{cannot}}) be seen!)", data) == R"(I () be seen!)",
                R"(Nulls should interpolate as the empty string.)");
}

TEST_CASE("Triple Mustache Null Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"cannot":null})");
  CHECK_MESSAGE(ministache::render(R"(I ({{{cannot}}}) be seen!)", data) == R"(I () be seen!)",
                R"(Nulls should interpolate as the empty string.)");
}

TEST_CASE("Ampersand Null Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"cannot":null})");
  CHECK_MESSAGE(ministache::render(R"(I ({{&cannot}}) be seen!)", data) == R"(I () be seen!)",
                R"(Nulls should interpolate as the empty string.)");
}

TEST_CASE("Basic Context Miss Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(I ({{cannot}}) be seen!)", data) == R"(I () be seen!)",
                R"(Failed context lookups should default to empty strings.)");
}

TEST_CASE("Triple Mustache Context Miss Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(I ({{{cannot}}}) be seen!)", data) == R"(I () be seen!)",
                R"(Failed context lookups should default to empty strings.)");
}

TEST_CASE("Ampersand Context Miss Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(ministache::render(R"(I ({{&cannot}}) be seen!)", data) == R"(I () be seen!)",
                R"(Failed context lookups should default to empty strings.)");
}

TEST_CASE("Dotted Names - Basic Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"person":{"name":"Joe"}})");
  CHECK_MESSAGE(ministache::render(R"("{{person.name}}" == "{{#person}}{{name}}{{/person}}")",
                                   data) == R"("Joe" == "Joe")",
                R"(Dotted names should be considered a form of shorthand for sections.)");
}

TEST_CASE("Dotted Names - Triple Mustache Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"person":{"name":"Joe"}})");
  CHECK_MESSAGE(ministache::render(R"("{{{person.name}}}" == "{{#person}}{{{name}}}{{/person}}")",
                                   data) == R"("Joe" == "Joe")",
                R"(Dotted names should be considered a form of shorthand for sections.)");
}

TEST_CASE("Dotted Names - Ampersand Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"person":{"name":"Joe"}})");
  CHECK_MESSAGE(ministache::render(R"("{{&person.name}}" == "{{#person}}{{&name}}{{/person}}")",
                                   data) == R"("Joe" == "Joe")",
                R"(Dotted names should be considered a form of shorthand for sections.)");
}

TEST_CASE("Dotted Names - Arbitrary Depth") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{"b":{"c":{"d":{"e":{"name":"Phil"}}}}}})");
  CHECK_MESSAGE(
      ministache::render(R"("{{a.b.c.d.e.name}}" == "Phil")", data) == R"("Phil" == "Phil")",
      R"(Dotted names should be functional to any level of nesting.)");
}

TEST_CASE("Dotted Names - Broken Chains") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{}})");
  CHECK_MESSAGE(ministache::render(R"("{{a.b.c}}" == "")", data) == R"("" == "")",
                R"(Any falsey value prior to the last part of the name should yield ''.)");
}

TEST_CASE("Dotted Names - Broken Chain Resolution") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{"b":{}},"c":{"name":"Jim"}})");
  CHECK_MESSAGE(ministache::render(R"("{{a.b.c.name}}" == "")", data) == R"("" == "")",
                R"(Each part of a dotted name should resolve only against its parent.)");
}

TEST_CASE("Dotted Names - Initial Resolution") {
  ArduinoJson::JsonDocument data;
  deserializeJson(
      data,
      R"({"a":{"b":{"c":{"d":{"e":{"name":"Phil"}}}}},"b":{"c":{"d":{"e":{"name":"Wrong"}}}}})");
  CHECK_MESSAGE(ministache::render(R"("{{#a}}{{b.c.d.e.name}}{{/a}}" == "Phil")", data) ==
                    R"("Phil" == "Phil")",
                R"(The first part of a dotted name should resolve as any other name.)");
}

TEST_CASE("Dotted Names - Context Precedence") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"a":{"b":{}},"b":{"c":"ERROR"}})");
  CHECK_MESSAGE(ministache::render(R"({{#a}}{{b.c}}{{/a}})", data) == R"()",
                R"(Dotted names should be resolved against former resolutions.)");
}

TEST_CASE("Implicit Iterators - Basic Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"("world")");
  CHECK_MESSAGE(ministache::render(R"(Hello, {{.}}!
)",
                                   data) == R"(Hello, world!
)",
                R"(Unadorned tags should interpolate content into the template.)");
}

TEST_CASE("Implicit Iterators - HTML Escaping") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"("& \" < >")");
  CHECK_MESSAGE(
      ministache::render(R"(These characters should be HTML escaped: {{.}}
)",
                         data) == R"(These characters should be HTML escaped: &amp; &quot; &lt; &gt;
)",
      R"(Basic interpolation should be HTML escaped.)");
}

TEST_CASE("Implicit Iterators - Triple Mustache") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"("& \" < >")");
  CHECK_MESSAGE(ministache::render(R"(These characters should not be HTML escaped: {{{.}}}
)",
                                   data) == R"(These characters should not be HTML escaped: & " < >
)",
                R"(Triple mustaches should interpolate without HTML escaping.)");
}

TEST_CASE("Implicit Iterators - Ampersand") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"("& \" < >")");
  CHECK_MESSAGE(ministache::render(R"(These characters should not be HTML escaped: {{&.}}
)",
                                   data) == R"(These characters should not be HTML escaped: & " < >
)",
                R"(Ampersand should interpolate without HTML escaping.)");
}

TEST_CASE("Implicit Iterators - Basic Integer Interpolation") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"(85)");
  CHECK_MESSAGE(ministache::render(R"("{{.}} miles an hour!")", data) == R"("85 miles an hour!")",
                R"(Integers should interpolate seamlessly.)");
}

TEST_CASE("Interpolation - Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(| {{string}} |)", data) == R"(| --- |)",
                R"(Interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Triple Mustache - Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(| {{{string}}} |)", data) == R"(| --- |)",
                R"(Interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Ampersand - Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(| {{&string}} |)", data) == R"(| --- |)",
                R"(Interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Interpolation - Standalone") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(  {{string}}
)",
                                   data) == R"(  ---
)",
                R"(Standalone interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Triple Mustache - Standalone") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(  {{{string}}}
)",
                                   data) == R"(  ---
)",
                R"(Standalone interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Ampersand - Standalone") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(  {{&string}}
)",
                                   data) == R"(  ---
)",
                R"(Standalone interpolation should not alter surrounding whitespace.)");
}

TEST_CASE("Interpolation With Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(|{{ string }}|)", data) == R"(|---|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_CASE("Triple Mustache With Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(|{{{ string }}}|)", data) == R"(|---|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_CASE("Ampersand With Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"string":"---"})");
  CHECK_MESSAGE(ministache::render(R"(|{{& string }}|)", data) == R"(|---|)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_SUITE_END();
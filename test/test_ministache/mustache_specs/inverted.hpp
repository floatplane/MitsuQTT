#pragma once

#include <ArduinoJson.h>
#include <doctest/doctest.h>

// clang-format off
/* generated via
cat test/test_ministache/mustache_specs/inverted.json| jq -r '.tests[] | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"-(\(.data))-\"); CHECK_MESSAGE(ministache::render(R\"-(\(.template))-\", data) == R\"-(\(.expected))-\", R\"-(\(.desc))-\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("minimustache/specs/inverted");

TEST_CASE("Falsey") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-("{{^boolean}}This should be rendered.{{/boolean}}")-",
                                   data) == R"-("This should be rendered.")-",
                R"-(Falsey sections should have their contents rendered.)-");
}

TEST_CASE("Truthy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(ministache::render(R"-("{{^boolean}}This should not be rendered.{{/boolean}}")-",
                                   data) == R"-("")-",
                R"-(Truthy sections should have their contents omitted.)-");
}

TEST_CASE("Null is falsey") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"null":null})-");
  CHECK_MESSAGE(ministache::render(R"-("{{^null}}This should be rendered.{{/null}}")-", data) ==
                    R"-("This should be rendered.")-",
                R"-(Null is falsey.)-");
}

TEST_CASE("Context") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"context":{"name":"Joe"}})-");
  CHECK_MESSAGE(
      ministache::render(R"-("{{^context}}Hi {{name}}.{{/context}}")-", data) == R"-("")-",
      R"-(Objects and hashes should behave like truthy values.)-");
}

TEST_CASE("List") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[{"n":1},{"n":2},{"n":3}]})-");
  CHECK_MESSAGE(ministache::render(R"-("{{^list}}{{n}}{{/list}}")-", data) == R"-("")-",
                R"-(Lists should behave like truthy values.)-");
}

TEST_CASE("Empty List") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[]})-");
  CHECK_MESSAGE(
      ministache::render(R"-("{{^list}}Yay lists!{{/list}}")-", data) == R"-("Yay lists!")-",
      R"-(Empty lists should behave like falsey values.)-");
}

TEST_CASE("Doubled") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"bool":false,"two":"second"})-");
  CHECK_MESSAGE(ministache::render(R"-({{^bool}}
* first
{{/bool}}
* {{two}}
{{^bool}}
* third
{{/bool}}
)-",
                                   data) == R"-(* first
* second
* third
)-",
                R"-(Multiple inverted sections per template should be permitted.)-");
}

TEST_CASE("Nested (Falsey)") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"bool":false})-");
  CHECK_MESSAGE(ministache::render(R"-(| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |)-",
                                   data) == R"-(| A B C D E |)-",
                R"-(Nested falsey sections should have their contents rendered.)-");
}

TEST_CASE("Nested (Truthy)") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"bool":true})-");
  CHECK_MESSAGE(ministache::render(R"-(| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |)-",
                                   data) == R"-(| A  E |)-",
                R"-(Nested truthy sections should be omitted.)-");
}

TEST_CASE("Context Misses") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({})-");
  CHECK_MESSAGE(ministache::render(R"-([{{^missing}}Cannot find key 'missing'!{{/missing}}])-",
                                   data) == R"-([Cannot find key 'missing'!])-",
                R"-(Failed context lookups should be considered falsey.)-");
}

TEST_CASE("Dotted Names - Truthy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":{"b":{"c":true}}})-");
  CHECK_MESSAGE(
      ministache::render(R"-("{{^a.b.c}}Not Here{{/a.b.c}}" == "")-", data) == R"-("" == "")-",
      R"-(Dotted names should be valid for Inverted Section tags.)-");
}

TEST_CASE("Dotted Names - Falsey") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":{"b":{"c":false}}})-");
  CHECK_MESSAGE(ministache::render(R"-("{{^a.b.c}}Not Here{{/a.b.c}}" == "Not Here")-", data) ==
                    R"-("Not Here" == "Not Here")-",
                R"-(Dotted names should be valid for Inverted Section tags.)-");
}

TEST_CASE("Dotted Names - Broken Chains") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":{}})-");
  CHECK_MESSAGE(ministache::render(R"-("{{^a.b.c}}Not Here{{/a.b.c}}" == "Not Here")-", data) ==
                    R"-("Not Here" == "Not Here")-",
                R"-(Dotted names that cannot be resolved should be considered falsey.)-");
}

TEST_CASE("Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-( | {{^boolean}}	|	{{/boolean}} | 
)-",
                                   data) == R"-( | 	|	 | 
)-",
                R"-(Inverted sections should not alter surrounding whitespace.)-");
}

TEST_CASE("Internal Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-( | {{^boolean}} {{! Important Whitespace }}
 {{/boolean}} | 
)-",
                                   data) == R"-( |  
  | 
)-",
                R"-(Inverted should not alter internal whitespace.)-");
}

TEST_CASE("Indented Inline Sections") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-( {{^boolean}}NO{{/boolean}}
 {{^boolean}}WAY{{/boolean}}
)-",
                                   data) == R"-( NO
 WAY
)-",
                R"-(Single-line sections should not alter surrounding whitespace.)-");
}

TEST_CASE("Standalone Lines") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-(| This Is
{{^boolean}}
|
{{/boolean}}
| A Line
)-",
                                   data) == R"-(| This Is
|
| A Line
)-",
                R"-(Standalone lines should be removed from the template.)-");
}

TEST_CASE("Standalone Indented Lines") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-(| This Is
  {{^boolean}}
|
  {{/boolean}}
| A Line
)-",
                                   data) == R"-(| This Is
|
| A Line
)-",
                R"-(Standalone indented lines should be removed from the template.)-");
}

TEST_CASE("Standalone Line Endings") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-(|
{{^boolean}}
{{/boolean}}
|)-",
                                   data) == R"-(|
|)-",
                R"-("\r\n" should be considered a newline for standalone tags.)-");
}

TEST_CASE("Standalone Without Previous Line") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-(  {{^boolean}}
^{{/boolean}}
/)-",
                                   data) == R"-(^
/)-",
                R"-(Standalone tags should not require a newline to precede them.)-");
}

TEST_CASE("Standalone Without Newline") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-(^{{^boolean}}
/
  {{/boolean}})-",
                                   data) == R"-(^
/
)-",
                R"-(Standalone tags should not require a newline to follow them.)-");
}

TEST_CASE("Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(ministache::render(R"-(|{{^ boolean }}={{/ boolean }}|)-", data) == R"-(|=|)-",
                R"-(Superfluous in-tag whitespace should be ignored.)-");
}

TEST_SUITE_END();
#pragma once

#include <ArduinoJson.h>
#include <doctest/doctest.h>

// clang-format off
/* generated via
cat test/test_template/mustache_specs/sections.json| jq -r '.tests[] | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"-(\(.data))-\"); CHECK_MESSAGE(Template(R\"-(\(.template|rtrimstr("\n")))-\").render(data) == R\"-(\(.expected|rtrimstr("\n")))-\", R\"-(\(.desc))-\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("mustache/sections");

TEST_CASE("Truthy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-("{{#boolean}}This should be rendered.{{/boolean}}")-").render(data) ==
                    R"-("This should be rendered.")-",
                R"-(Truthy sections should have their contents rendered.)-");
}

TEST_CASE("Falsey") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK_MESSAGE(
      Template(R"-("{{#boolean}}This should not be rendered.{{/boolean}}")-").render(data) ==
          R"-("")-",
      R"-(Falsey sections should have their contents omitted.)-");
}

TEST_CASE("Null is falsey") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"null":null})-");
  CHECK_MESSAGE(
      Template(R"-("{{#null}}This should not be rendered.{{/null}}")-").render(data) == R"-("")-",
      R"-(Null is falsey.)-");
}

TEST_CASE("Context") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"context":{"name":"Joe"}})-");
  CHECK_MESSAGE(
      Template(R"-("{{#context}}Hi {{name}}.{{/context}}")-").render(data) == R"-("Hi Joe.")-",
      R"-(Objects and hashes should be pushed onto the context stack.)-");
}

TEST_CASE("Parent contexts") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":"foo","b":"wrong","sec":{"b":"bar"},"c":{"d":"baz"}})-");
  CHECK_MESSAGE(Template(R"-("{{#sec}}{{a}}, {{b}}, {{c.d}}{{/sec}}")-").render(data) ==
                    R"-("foo, bar, baz")-",
                R"-(Names missing in the current context are looked up in the stack.)-");
}

TEST_CASE("Variable test") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"foo":"bar"})-");
  CHECK_MESSAGE(
      Template(R"-("{{#foo}}{{.}} is {{foo}}{{/foo}}")-").render(data) == R"-("bar is bar")-",
      R"-(Non-false sections have their value at the top of context,
accessible as {{.}} or through the parent context. This gives
a simple way to display content conditionally if a variable exists.
)-");
}

TEST_CASE("List Contexts") {
  ArduinoJson::JsonDocument data;
  deserializeJson(
      data,
      R"-({"tops":[{"tname":{"upper":"A","lower":"a"},"middles":[{"mname":"1","bottoms":[{"bname":"x"},{"bname":"y"}]}]}]})-");
  CHECK_MESSAGE(
      Template(
          R"-({{#tops}}{{#middles}}{{tname.lower}}{{mname}}.{{#bottoms}}{{tname.upper}}{{mname}}{{bname}}.{{/bottoms}}{{/middles}}{{/tops}})-")
              .render(data) == R"-(a1.A1x.A1y.)-",
      R"-(All elements on the context stack should be accessible within lists.)-");
}

TEST_CASE("Deeply Nested Contexts") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data,
                  R"-({"a":{"one":1},"b":{"two":2},"c":{"three":3,"d":{"four":4,"five":5}}})-");
  CHECK_MESSAGE(Template(R"-({{#a}}
{{one}}
{{#b}}
{{one}}{{two}}{{one}}
{{#c}}
{{one}}{{two}}{{three}}{{two}}{{one}}
{{#d}}
{{one}}{{two}}{{three}}{{four}}{{three}}{{two}}{{one}}
{{#five}}
{{one}}{{two}}{{three}}{{four}}{{five}}{{four}}{{three}}{{two}}{{one}}
{{one}}{{two}}{{three}}{{four}}{{.}}6{{.}}{{four}}{{three}}{{two}}{{one}}
{{one}}{{two}}{{three}}{{four}}{{five}}{{four}}{{three}}{{two}}{{one}}
{{/five}}
{{one}}{{two}}{{three}}{{four}}{{three}}{{two}}{{one}}
{{/d}}
{{one}}{{two}}{{three}}{{two}}{{one}}
{{/c}}
{{one}}{{two}}{{one}}
{{/b}}
{{one}}
{{/a}})-")
                        .render(data) == R"-(1
121
12321
1234321
123454321
12345654321
123454321
1234321
12321
121
1)-",
                R"-(All elements on the context stack should be accessible.)-");
}

TEST_CASE("List") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[{"item":1},{"item":2},{"item":3}]})-");
  CHECK_MESSAGE(Template(R"-("{{#list}}{{item}}{{/list}}")-").render(data) == R"-("123")-",
                R"-(Lists should be iterated; list items should visit the context stack.)-");
}

TEST_CASE("Empty List") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[]})-");
  CHECK_MESSAGE(Template(R"-("{{#list}}Yay lists!{{/list}}")-").render(data) == R"-("")-",
                R"-(Empty lists should behave like falsey values.)-");
}

TEST_CASE("Doubled") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"bool":true,"two":"second"})-");
  CHECK_MESSAGE(Template(R"-({{#bool}}
* first
{{/bool}}
* {{two}}
{{#bool}}
* third
{{/bool}})-")
                        .render(data) == R"-(* first
* second
* third)-",
                R"-(Multiple sections per template should be permitted.)-");
}

TEST_CASE("Nested (Truthy)") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"bool":true})-");
  CHECK_MESSAGE(Template(R"-(| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |)-").render(data) ==
                    R"-(| A B C D E |)-",
                R"-(Nested truthy sections should have their contents rendered.)-");
}

TEST_CASE("Nested (Falsey)") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"bool":false})-");
  CHECK_MESSAGE(Template(R"-(| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |)-").render(data) ==
                    R"-(| A  E |)-",
                R"-(Nested falsey sections should be omitted.)-");
}

TEST_CASE("Context Misses") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({})-");
  CHECK_MESSAGE(
      Template(R"-([{{#missing}}Found key 'missing'!{{/missing}}])-").render(data) == R"-([])-",
      R"-(Failed context lookups should be considered falsey.)-");
}

TEST_CASE("Implicit Iterator - String") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":["a","b","c","d","e"]})-");
  CHECK_MESSAGE(
      Template(R"-("{{#list}}({{.}}){{/list}}")-").render(data) == R"-("(a)(b)(c)(d)(e)")-",
      R"-(Implicit iterators should directly interpolate strings.)-");
}

TEST_CASE("Implicit Iterator - Integer") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[1,2,3,4,5]})-");
  CHECK_MESSAGE(
      Template(R"-("{{#list}}({{.}}){{/list}}")-").render(data) == R"-("(1)(2)(3)(4)(5)")-",
      R"-(Implicit iterators should cast integers to strings and interpolate.)-");
}

TEST_CASE("Implicit Iterator - Decimal") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[1.1,2.2,3.3,4.4,5.5]})-");
  CHECK_MESSAGE(Template(R"-("{{#list}}({{.}}){{/list}}")-").render(data) ==
                    R"-("(1.1)(2.2)(3.3)(4.4)(5.5)")-",
                R"-(Implicit iterators should cast decimals to strings and interpolate.)-");
}

TEST_CASE("Implicit Iterator - Array") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[[1,2,3],["a","b","c"]]})-");
  CHECK_MESSAGE(
      Template(R"-("{{#list}}({{#.}}{{.}}{{/.}}){{/list}}")-").render(data) == R"-("(123)(abc)")-",
      R"-(Implicit iterators should allow iterating over nested arrays.)-");
}

TEST_CASE("Implicit Iterator - HTML Escaping") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":["&","\"","<",">"]})-");
  CHECK_MESSAGE(Template(R"-("{{#list}}({{.}}){{/list}}")-").render(data) ==
                    R"-("(&amp;)(&quot;)(&lt;)(&gt;)")-",
                R"-(Implicit iterators with basic interpolation should be HTML escaped.)-");
}

TEST_CASE("Implicit Iterator - Triple mustache") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":["&","\"","<",">"]})-");
  CHECK_MESSAGE(
      Template(R"-("{{#list}}({{{.}}}){{/list}}")-").render(data) == R"-("(&)(")(<)(>)")-",
      R"-(Implicit iterators in triple mustache should interpolate without HTML escaping.)-");
}

TEST_CASE("Implicit Iterator - Ampersand") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":["&","\"","<",">"]})-");
  CHECK_MESSAGE(
      Template(R"-("{{#list}}({{&.}}){{/list}}")-").render(data) == R"-("(&)(")(<)(>)")-",
      R"-(Implicit iterators in an Ampersand tag should interpolate without HTML escaping.)-");
}

TEST_CASE("Implicit Iterator - Root-level") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-([{"value":"a"},{"value":"b"}])-");
  CHECK_MESSAGE(Template(R"-("{{#.}}({{value}}){{/.}}")-").render(data) == R"-("(a)(b)")-",
                R"-(Implicit iterators should work on root-level lists.)-");
}

TEST_CASE("Dotted Names - Truthy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":{"b":{"c":true}}})-");
  CHECK_MESSAGE(
      Template(R"-("{{#a.b.c}}Here{{/a.b.c}}" == "Here")-").render(data) == R"-("Here" == "Here")-",
      R"-(Dotted names should be valid for Section tags.)-");
}

TEST_CASE("Dotted Names - Falsey") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":{"b":{"c":false}}})-");
  CHECK_MESSAGE(Template(R"-("{{#a.b.c}}Here{{/a.b.c}}" == "")-").render(data) == R"-("" == "")-",
                R"-(Dotted names should be valid for Section tags.)-");
}

TEST_CASE("Dotted Names - Broken Chains") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"a":{}})-");
  CHECK_MESSAGE(Template(R"-("{{#a.b.c}}Here{{/a.b.c}}" == "")-").render(data) == R"-("" == "")-",
                R"-(Dotted names that cannot be resolved should be considered falsey.)-");
}

TEST_CASE("Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-( | {{#boolean}}	|	{{/boolean}} | )-").render(data) ==
                    R"-( | 	|	 | )-",
                R"-(Sections should not alter surrounding whitespace.)-");
}

TEST_CASE("Internal Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-( | {{#boolean}} {{! Important Whitespace }}
 {{/boolean}} | )-")
                        .render(data) == R"-( |  
  | )-",
                R"-(Sections should not alter internal whitespace.)-");
}

TEST_CASE("Indented Inline Sections") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-( {{#boolean}}YES{{/boolean}}
 {{#boolean}}GOOD{{/boolean}})-")
                        .render(data) == R"-( YES
 GOOD)-",
                R"-(Single-line sections should not alter surrounding whitespace.)-");
}

TEST_CASE("Standalone Lines") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-(| This Is
{{#boolean}}
|
{{/boolean}}
| A Line)-")
                        .render(data) == R"-(| This Is
|
| A Line)-",
                R"-(Standalone lines should be removed from the template.)-");
}

TEST_CASE("Indented Standalone Lines") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-(| This Is
  {{#boolean}}
|
  {{/boolean}}
| A Line)-")
                        .render(data) == R"-(| This Is
|
| A Line)-",
                R"-(Indented standalone lines should be removed from the template.)-");
}

TEST_CASE("Standalone Line Endings") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-(|
{{#boolean}}
{{/boolean}}
|)-")
                        .render(data) == R"-(|
|)-",
                R"-("\r\n" should be considered a newline for standalone tags.)-");
}

TEST_CASE("Standalone Without Previous Line") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-(  {{#boolean}}
#{{/boolean}}
/)-")
                        .render(data) == R"-(#
/)-",
                R"-(Standalone tags should not require a newline to precede them.)-");
}

TEST_CASE("Standalone Without Newline") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-(#{{#boolean}}
/
  {{/boolean}})-")
                        .render(data) == R"-(#
/)-",
                R"-(Standalone tags should not require a newline to follow them.)-");
}

TEST_CASE("Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_MESSAGE(Template(R"-(|{{# boolean }}={{/ boolean }}|)-").render(data) == R"-(|=|)-",
                R"-(Superfluous in-tag whitespace should be ignored.)-");
}

TEST_SUITE_END();
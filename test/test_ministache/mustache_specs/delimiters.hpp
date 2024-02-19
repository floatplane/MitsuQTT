#pragma once

#include <ArduinoJson.h>
#include <doctest.h>

// clang-format off
/* generated via
cat test/test_ministache/mustache_specs/delimiters.json | jq -r '.tests[] | {name: .name, desc: .desc, data: .data|tojson, template: .template, expected: .expected, partials: (.partials // {})|to_entries|map("{\(.key|tojson), \(.value|tojson)}")|join(", ")} | .partials |= "{"+.+"}" | "TEST_CASE(\"\(.name)\") { ArduinoJson::JsonDocument data; deserializeJson(data, R\"(\(.data))\"); CHECK_MESSAGE(Ministache(R\"(\(.template))\").render(data, \(.partials)) == R\"(\(.expected))\", R\"(\(.desc))\"); }\n"' | pbcopy
*/
// clang-format on

TEST_SUITE_BEGIN("minimustache/specs/delimiters");

TEST_CASE("Pair Behavior") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"text":"Hey!"})");
  CHECK_MESSAGE(Ministache(R"({{=<% %>=}}(<%text%>))").render(data, {}) == R"((Hey!))",
                R"(The equals sign (used on both sides) should permit delimiter changes.)");
}

TEST_CASE("Special Characters") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"text":"It worked!"})");
  CHECK_MESSAGE(Ministache(R"(({{=[ ]=}}[text]))").render(data, {}) == R"((It worked!))",
                R"(Characters with special meaning regexen should be valid delimiters.)");
}

TEST_CASE("Sections") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"section":true,"data":"I got interpolated."})");
  CHECK_MESSAGE(Ministache(R"([
{{#section}}
  {{data}}
  |data|
{{/section}}

{{= | | =}}
|#section|
  {{data}}
  |data|
|/section|
]
)")
                        .render(data, {}) == R"([
  I got interpolated.
  |data|

  {{data}}
  I got interpolated.
]
)",
                R"(Delimiters set outside sections should persist.)");
}

TEST_CASE("Inverted Sections") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"section":false,"data":"I got interpolated."})");
  CHECK_MESSAGE(Ministache(R"([
{{^section}}
  {{data}}
  |data|
{{/section}}

{{= | | =}}
|^section|
  {{data}}
  |data|
|/section|
]
)")
                        .render(data, {}) == R"([
  I got interpolated.
  |data|

  {{data}}
  I got interpolated.
]
)",
                R"(Delimiters set outside inverted sections should persist.)");
}

TEST_CASE("Partial Inheritence") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"value":"yes"})");
  CHECK_MESSAGE(Ministache(R"([ {{>include}} ]
{{= | | =}}
[ |>include| ]
)")
                        .render(data, {{"include", ".{{value}}."}}) == R"([ .yes. ]
[ .yes. ]
)",
                R"(Delimiters set in a parent template should not affect a partial.)");
}

TEST_CASE("Post-Partial Behavior") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({"value":"yes"})");
  CHECK_MESSAGE(Ministache(R"([ {{>include}} ]
[ .{{value}}.  .|value|. ]
)")
                        .render(data, {{"include", ".{{value}}. {{= | | =}} .|value|."}}) ==
                    R"([ .yes.  .yes. ]
[ .yes.  .|value|. ]
)",
                R"(Delimiters set in a partial should not affect the parent template.)");
}

TEST_CASE("Surrounding Whitespace") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(| {{=@ @=}} |)").render(data, {}) == R"(|  |)",
                R"(Surrounding whitespace should be left untouched.)");
}

TEST_CASE("Outlying Whitespace (Inline)") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"( | {{=@ @=}}
)")
                        .render(data, {}) == R"( | 
)",
                R"(Whitespace should be left untouched.)");
}

TEST_CASE("Standalone Tag") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(Begin.
{{=@ @=}}
End.
)")
                        .render(data, {}) == R"(Begin.
End.
)",
                R"(Standalone lines should be removed from the template.)");
}

TEST_CASE("Indented Standalone Tag") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(Begin.
  {{=@ @=}}
End.
)")
                        .render(data, {}) == R"(Begin.
End.
)",
                R"(Indented standalone lines should be removed from the template.)");
}

TEST_CASE("Standalone Line Endings") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(|
{{= @ @ =}}
|)")
                        .render(data, {}) == R"(|
|)",
                R"("\r\n" should be considered a newline for standalone tags.)");
}

TEST_CASE("Standalone Without Previous Line") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(  {{=@ @=}}
=)")
                        .render(data, {}) == R"(=)",
                R"(Standalone tags should not require a newline to precede them.)");
}

TEST_CASE("Standalone Without Newline") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(=
  {{=@ @=}})")
                        .render(data, {}) == R"(=
)",
                R"(Standalone tags should not require a newline to follow them.)");
}

TEST_CASE("Pair with Padding") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"({})");
  CHECK_MESSAGE(Ministache(R"(|{{= @   @ =}}|)").render(data, {}) == R"(||)",
                R"(Superfluous in-tag whitespace should be ignored.)");
}

TEST_SUITE_END();
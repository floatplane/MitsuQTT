#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <Arduino.h>
#include <ArduinoJson.h>
#include <doctest.h>

#include <template.hpp>

#include "mustache_specs/interpolation.hpp"

TEST_CASE("testing render with no substitutions") {
  ArduinoJson::JsonDocument values;
  CHECK(Template("Hello, world!").render(values) == "Hello, world!");
  CHECK(Template("").render(values) == "");
}

TEST_CASE("testing render with string substitution") {
  ArduinoJson::JsonDocument values;
  values["name"] = "floatplane";
  CHECK(Template("{{name}}").render(values) == "floatplane");
  CHECK(Template("{{name}} is a name").render(values) == "floatplane is a name");
  CHECK(Template("a name is {{name}}").render(values) == "a name is floatplane");
  CHECK(Template("a name is {{name}} is a name").render(values) ==
        "a name is floatplane is a name");
  CHECK(Template("test: {{name}} == {{name}} is true").render(values) ==
        "test: floatplane == floatplane is true");
}

TEST_CASE("testing render with missing values") {
  ArduinoJson::JsonDocument values;
  CHECK(Template("{{name}}").render(values) == "");
  CHECK(Template("{{name}} is a name").render(values) == " is a name");
  CHECK(Template("a name is {{name}}").render(values) == "a name is ");
  CHECK(Template("a name is {{name}} is a name").render(values) == "a name is  is a name");
  CHECK(Template("test: {{name}} == {{name}} is true").render(values) == "test:  ==  is true");
}

TEST_CASE("testing render with malformed values") {
  ArduinoJson::JsonDocument values;
  values["name"] = "floatplane";
  values["  name  "] = "Brian";
  CHECK(Template("{{tag is unclosed at start!").render(values) == "");
  CHECK(Template("tag is unclosed at end!{{").render(values) == "tag is unclosed at end!");
  CHECK(Template("tag is unclosed {{in middle").render(values) == "tag is unclosed ");
  CHECK(Template("Hello, {{name!").render(values) == "Hello, ");
  CHECK(Template("Hello, {{ name}}!").render(values) == "Hello, !");
  CHECK(Template("Hello, {{ name}}!").render(values) == "Hello, !");
  CHECK(Template("Hello, {{  name  }}!").render(values) == "Hello, Brian!");
  CHECK(Template("Hello, {name}}{{name}}!").render(values) == "Hello, {name}}floatplane!");
}

int main(int argc, char **argv) {
  doctest::Context context;

  // BEGIN:: PLATFORMIO REQUIRED OPTIONS
  context.setOption("success", true);      // Report successful tests
  context.setOption("no-exitcode", true);  // Do not return non-zero code on failed test case
  // END:: PLATFORMIO REQUIRED OPTIONS

  // YOUR CUSTOM DOCTEST OPTIONS

  context.applyCommandLine(argc, argv);
  return context.run();
}
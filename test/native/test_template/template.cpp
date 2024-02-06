#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <doctest.h>

#include <template.hpp>

TEST_CASE("testing contentLength with no substitutions") {
  CHECK(Template("Hello, world!").contentLength(ArduinoJson::JsonDocument()) == 13);
  CHECK(Template("").contentLength(ArduinoJson::JsonDocument()) == 0);
}

TEST_CASE("testing contentLength with string substitution") {
  ArduinoJson::JsonDocument values;
  values["name"] = "floatplane";
  CHECK(Template("{{name}}").contentLength(values) == 10);
  CHECK(Template("{{name}} is a name").contentLength(values) == 20);
  CHECK(Template("a name is {{name}}").contentLength(values) == 20);
  CHECK(Template("a name is {{name}} is a name").contentLength(values) == 30);
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
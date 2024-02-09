#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <doctest.h>

#include <string>
#include <template.hpp>

typedef Template<std::string> StringTemplate;
typedef StringTemplate::DataMap StringDataMap;

TEST_CASE("testing render with no substitutions") {
  CHECK(StringTemplate("Hello, world!").render({}) == "Hello, world!");
  CHECK(StringTemplate("").render({}) == "");
}

TEST_CASE("testing render with string substitution") {
  StringDataMap values;
  values.insert({"name", "floatplane"});
  CHECK(StringTemplate("{{name}}").render(values) == "floatplane");
  CHECK(StringTemplate("{{name}} is a name").render(values) == "floatplane is a name");
  CHECK(StringTemplate("a name is {{name}}").render(values) == "a name is floatplane");
  CHECK(StringTemplate("a name is {{name}} is a name").render(values) ==
        "a name is floatplane is a name");
  CHECK(StringTemplate("test: {{name}} == {{name}} is true").render(values) ==
        "test: floatplane == floatplane is true");
}

TEST_CASE("testing render with missing values") {
  StringDataMap values;
  CHECK(StringTemplate("{{name}}").render(values) == "");
  CHECK(StringTemplate("{{name}} is a name").render(values) == " is a name");
  CHECK(StringTemplate("a name is {{name}}").render(values) == "a name is ");
  CHECK(StringTemplate("a name is {{name}} is a name").render(values) == "a name is  is a name");
  CHECK(StringTemplate("test: {{name}} == {{name}} is true").render(values) ==
        "test:  ==  is true");
}

TEST_CASE("testing render with malformed values") {
  StringDataMap values;
  values.insert({"name", "floatplane"});
  values.insert({"  name  ", "Brian"});
  CHECK(StringTemplate("{{tag is unclosed at start!").render(values) == "");
  CHECK(StringTemplate("tag is unclosed at end!{{").render(values) == "tag is unclosed at end!");
  CHECK(StringTemplate("tag is unclosed {{in middle").render(values) == "tag is unclosed ");
  CHECK(StringTemplate("Hello, {{name!").render(values) == "Hello, ");
  CHECK(StringTemplate("Hello, {{ name}}!").render(values) == "Hello, !");
  CHECK(StringTemplate("Hello, {{ name}}!").render(values) == "Hello, !");
  CHECK(StringTemplate("Hello, {{  name  }}!").render(values) == "Hello, Brian!");
  CHECK(StringTemplate("Hello, {name}}{{name}}!").render(values) == "Hello, {name}}floatplane!");
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
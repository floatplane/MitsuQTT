#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <Arduino.h>
#include <ArduinoJson.h>
#include <doctest.h>

#include <template.hpp>

#include "mustache_specs/comments.hpp"
#include "mustache_specs/interpolation.hpp"
#include "mustache_specs/inverted.hpp"
#include "mustache_specs/partials.hpp"
#include "mustache_specs/sections.hpp"

TEST_SUITE_BEGIN("isFalsy");

TEST_CASE("Null value is falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"null":null})-");
  CHECK(Template::isFalsy(data["null"]));
}

TEST_CASE("Empty list is falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[]})-");
  CHECK(Template::isFalsy(data["list"]));
}

TEST_CASE("List with items is not falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"list":[{"n":1},{"n":2},{"n":3}]})-");
  CHECK_FALSE(Template::isFalsy(data["list"]));
}

TEST_CASE("Object value is not falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"context":{"name":"Joe"}})-");
  CHECK_FALSE(Template::isFalsy(data["context"]));
}

TEST_CASE("String is not falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"context":"Joe"})-");
  CHECK_FALSE(Template::isFalsy(data["context"]));
}

TEST_CASE("Boolean false is falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":false})-");
  CHECK(Template::isFalsy(data["boolean"]));
}

TEST_CASE("Boolean true is not falsy") {
  ArduinoJson::JsonDocument data;
  deserializeJson(data, R"-({"boolean":true})-");
  CHECK_FALSE(Template::isFalsy(data["boolean"]));
}

TEST_SUITE_END();

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

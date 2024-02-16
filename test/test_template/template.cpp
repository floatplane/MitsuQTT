#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <Arduino.h>
#include <ArduinoJson.h>
#include <doctest.h>

#include <template.hpp>

#include "mustache_specs/interpolation.hpp"

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

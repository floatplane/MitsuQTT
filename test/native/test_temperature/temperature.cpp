#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <doctest.h>

#include <temperature.hpp>

TEST_CASE("testing celsius to fahrenheit") {
  CHECK(std::round(Temperature::celsiusToFahrenheit(0.f)) == 32.f);
  CHECK(std::round(Temperature::celsiusToFahrenheit(37.f)) == 99.f);
  CHECK(std::round(Temperature::celsiusToFahrenheit(100.f)) == 212.f);
}

TEST_CASE("testing fahrenheit to celsius") {
  CHECK(std::round(Temperature::fahrenheitToCelsius(32.f)) == 0.f);
  CHECK(std::round(Temperature::fahrenheitToCelsius(98.6f)) == 37.f);
  CHECK(std::round(Temperature::fahrenheitToCelsius(212.f)) == 100.f);
}

TEST_CASE("test construction of temperature") {
  Temperature t1(37.56f, Temperature::Unit::Celsius);

  CHECK(std::round(t1.get(Temperature::Unit::Celsius)) == 38.f);
  CHECK(std::round(t1.get(Temperature::Unit::Fahrenheit)) == 100.f);

  Temperature t2(37.56f, Temperature::Unit::Fahrenheit);

  CHECK(std::round(t2.get(Temperature::Unit::Fahrenheit)) == 38.f);
  CHECK(std::round(t2.get(Temperature::Unit::Celsius)) == 3.f);
}

TEST_CASE("test rendering celsius with temperature step") {
  Temperature t(37.56f, Temperature::Unit::Celsius);

  // Can't do exact comparisons with floating point numbers - 37.6f ends up being 37.600002 or
  // something like that. So, round to 1 decimal place and compare.
  CHECK(std::round(t.get(Temperature::Unit::Celsius, 1.0f) * 10.0f) == 380.f);
  CHECK(std::round(t.get(Temperature::Unit::Celsius, 0.5f) * 10.0f) == 375.f);
  CHECK(std::round(t.get(Temperature::Unit::Celsius, 0.1f) * 10.0f) == 376.f);

  CHECK(std::round(t.get(Temperature::Unit::Fahrenheit, 1.0f) * 10.0f) == 1000.f);
  CHECK(std::round(t.get(Temperature::Unit::Fahrenheit, 0.5f) * 10.0f) == 995.f);
  CHECK(std::round(t.get(Temperature::Unit::Fahrenheit, 0.1f) * 10.0f) == 996.f);
}

TEST_CASE("test toString with temperature step") {
  Temperature t(37.56f, Temperature::Unit::Celsius);

  CHECK(t.toString(Temperature::Unit::Celsius, 1.0f) == "38.0");
  CHECK(t.toString(Temperature::Unit::Celsius, 0.5f) == "37.5");
  CHECK(t.toString(Temperature::Unit::Celsius, 0.1f) == "37.6");

  CHECK(t.toString(Temperature::Unit::Fahrenheit, 1.0f) == "100.0");
  CHECK(t.toString(Temperature::Unit::Fahrenheit, 0.5f) == "99.5");
  CHECK(t.toString(Temperature::Unit::Fahrenheit, 0.1f) == "99.6");
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
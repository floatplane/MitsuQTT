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
  Temperature t1(37.56f, Temperature::Unit::C);

  CHECK(std::round(t1.get(Temperature::Unit::C)) == 38.f);
  CHECK(std::round(t1.getCelsius()) == 38.f);  // shorthand for get(Unit::C)
  CHECK(std::round(t1.get(Temperature::Unit::F)) == 100.f);
  CHECK(std::round(t1.getFahrenheit()) == 100.f);  // shorthand for get(Unit::F)

  Temperature t2(37.56f, Temperature::Unit::F);

  CHECK(std::round(t2.getFahrenheit()) == 38.f);
  CHECK(std::round(t2.getCelsius()) == 3.f);
}

TEST_CASE("test rendering celsius with temperature step") {
  Temperature t(37.56f, Temperature::Unit::C);

  // Can't do exact comparisons with floating point numbers - 37.6f ends up being 37.600002 or
  // something like that. So, round to 1 decimal place and compare.
  CHECK(std::round(t.getCelsius(1.0f) * 10.0f) == 380.f);
  CHECK(std::round(t.getCelsius(0.5f) * 10.0f) == 375.f);
  CHECK(std::round(t.getCelsius(0.1f) * 10.0f) == 376.f);

  CHECK(std::round(t.getFahrenheit(1.0f) * 10.0f) == 1000.f);
  CHECK(std::round(t.getFahrenheit(0.5f) * 10.0f) == 995.f);
  CHECK(std::round(t.getFahrenheit(0.1f) * 10.0f) == 996.f);
}

TEST_CASE("test toString with temperature step") {
  Temperature t(37.560001f, Temperature::Unit::C);

  CHECK(t.toString(Temperature::Unit::C, 25.0f) == "50");
  CHECK(t.toString(Temperature::Unit::C, 5.0f) == "40");
  CHECK(t.toString(Temperature::Unit::C, 1.0f) == "38");
  CHECK(t.toString(Temperature::Unit::C, 0.5f) == "37.5");
  CHECK(t.toString(Temperature::Unit::C, 0.1f) == "37.6");
  CHECK(t.toString(Temperature::Unit::C, 0.0f) == "37.560001");
  CHECK(t.toString(Temperature::Unit::C, -0.1f) == "37.560001");

  CHECK(t.toString(Temperature::Unit::F, 1.0f) == "100");
  CHECK(t.toString(Temperature::Unit::F, 0.5f) == "99.5");
  CHECK(t.toString(Temperature::Unit::F, 0.1f) == "99.6");
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
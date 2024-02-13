#pragma once

#include <array>
#include <cmath>
#include <string>

class Temperature {
 private:
  // Temperature always stored internally as Celsius
  float celsius;

 public:
  enum class Unit {
    C,
    F
  };

  // Constructor
  explicit Temperature(float value, Unit unit)
      : celsius(unit == Unit::C ? value : fahrenheitToCelsius(value)) {
  }

  Temperature(const Temperature &other) : celsius(other.celsius) {
  }

  Temperature &operator=(const Temperature &other) {
    this->celsius = other.celsius;
    return *this;
  }

  float get(const Unit unit, const float tempStep = 0.0f) const {
    const auto value = unit == Unit::C ? this->celsius : celsiusToFahrenheit(this->celsius);
    if (tempStep > 0.0f) {
      return std::round(value / tempStep) * tempStep;
    }
    return value;
  }

  // Convenience methods
  float getCelsius(const float tempStep = 0.0f) const {
    return get(Unit::C, tempStep);
  }

  float getFahrenheit(const float tempStep = 0.0f) const {
    return get(Unit::F, tempStep);
  }

  std::string toString(const Unit unit, const float tempStep = 1.0f) const {
    // Would like to use std::to_chars here, but it's not available in the Arduino environment
    std::array<char, 10> str;
    const auto value = get(unit, tempStep);
    const auto digits =
        tempStep > 0.f ? static_cast<int>(std::ceil(std::log10(1.0f / tempStep))) : 6;
    snprintf(str.data(), str.size(), "%.*f", std::max(0, digits), value);
    return std::string(str.data());
  }

  void set(float value, const Unit unit) {
    this->celsius = unit == Unit::C ? value : fahrenheitToCelsius(value);
  }

  Temperature clamp(const Temperature &min, const Temperature &max) const {
    return Temperature(std::max(std::min(this->celsius, max.celsius), min.celsius), Unit::C);
  }

  Temperature &clamp(const Temperature &min, const Temperature &max) {
    celsius = std::max(std::min(this->celsius, max.celsius), min.celsius);
    return *this;
  }

  static float celsiusToFahrenheit(const float celsius) {
    return celsius * 9.0f / 5.0f + 32.0f;
  }

  static float fahrenheitToCelsius(const float fahrenheit) {
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
  }
};
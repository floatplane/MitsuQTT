#pragma once

#include <array>
#include <cmath>
#include <string>

class Temperature {
 private:
  float celsius;

 public:
  enum class Unit {
    Celsius,
    Fahrenheit
  };

  // Constructor
  explicit Temperature(float v, Unit unit)
      : celsius(unit == Unit::Celsius ? v : fahrenheitToCelsius(v)) {
  }

  Temperature(const Temperature &other) : celsius(other.celsius) {
  }
  Temperature &operator=(const Temperature &other) {
    celsius = other.celsius;
    return *this;
  }

  // Getters
  float get(const Unit unit, const float tempStep = 0.0f) const {
    const auto value = unit == Unit::Celsius ? celsius : celsiusToFahrenheit(celsius);
    if (tempStep > 0.0f) {
      return std::round(value / tempStep) * tempStep;
    }
    return value;
  }

  std::string toString(const Unit unit, const float tempStep = 0.0f) const {
    // Would like to use std::to_chars here, but it's not available in the Arduino environment
    std::array<char, 10> str;
    snprintf(str.data(), str.size(), "%.1f", get(unit, tempStep));
    return std::string(str.data());
  }

  // Setters
  void set(float value, const Unit unit) {
    celsius = unit == Unit::Celsius ? value : fahrenheitToCelsius(value);
  }

  static float celsiusToFahrenheit(const float celsius) {
    return celsius * 9.0f / 5.0f + 32.0f;
  }

  static float fahrenheitToCelsius(const float fahrenheit) {
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
  }
};
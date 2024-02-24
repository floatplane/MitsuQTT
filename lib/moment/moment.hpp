#include <cstdint>
#include <cstdio>

class Moment {
 public:
  struct MomentParts {
    // ordered for packing
    uint16_t milliseconds;
    uint16_t days;
    uint8_t years;
    uint8_t hours;
    uint8_t seconds;
    uint8_t minutes;
  };

  static void resetRolloverCount() {
    _rolloverCount = 0;
    _lastValue = 0;
  }

#ifdef ARDUINO_H_INCLUDED
 public:
  Moment() : Moment(millis()) {
  }
#endif

  Moment(uint32_t value) {
    if (value < _lastValue) {
      _rolloverCount++;
    }
    _lastValue = value;
    hours = value / millisecondsPerHour + _rolloverCount * hoursPerRollover;
    milliseconds = value % millisecondsPerHour + _rolloverCount * millisecondsPerRollover;
    normalize();
  }

 public:
  MomentParts get() const {
    return {
        .milliseconds = static_cast<uint16_t>(milliseconds % 1000UL),
        .seconds = static_cast<uint8_t>((milliseconds / 1000UL) % 60UL),
        .minutes = static_cast<uint8_t>(milliseconds / 1000UL / 60UL),
        .hours = static_cast<uint8_t>(hours % 24UL),
        .days = static_cast<uint16_t>((hours / 24UL) % 365UL),
        .years = static_cast<uint8_t>(hours / 24UL / 365UL),
    };
  }

 private:
  static constexpr uint32_t millisecondsPerHour = 60UL * 60UL * 1000UL;
  static constexpr uint32_t millisecondsPerRollover = 0xFFFFFFFFUL % millisecondsPerHour;
  static constexpr uint32_t hoursPerRollover = 0xFFFFFFFFUL / millisecondsPerHour;

  void normalize() {
    while (milliseconds > millisecondsPerHour) {
      milliseconds -= millisecondsPerHour;
      hours++;
    }
    printf("post normalize: %u milliseconds, %u hours\n", milliseconds, hours);
  }

  uint32_t milliseconds;
  uint32_t hours;
  static uint32_t _rolloverCount;
  static uint32_t _lastValue;
};

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
    _milliseconds =
        static_cast<int64_t>(value) + static_cast<int64_t>(_rolloverCount) * 0xFFFFFFFFLL;
  }

  int64_t operator-(const Moment &other) const {
    return _milliseconds - other._milliseconds;
  }

 public:
  MomentParts get() const {
    return {
        .milliseconds = static_cast<uint16_t>(_milliseconds % 1000UL),
        .seconds = static_cast<uint8_t>(_milliseconds / 1000UL % 60UL),
        .minutes = static_cast<uint8_t>(_milliseconds / 1000UL / 60UL % 60UL),
        .hours = static_cast<uint8_t>(_milliseconds / 1000UL / 60UL / 60UL % 24UL),
        .days = static_cast<uint16_t>(_milliseconds / 1000UL / 60UL / 60UL / 24UL % 365UL),
        .years = static_cast<uint8_t>(_milliseconds / 1000UL / 60UL / 60UL / 24UL / 365UL),
    };
  }

 private:
  int64_t _milliseconds;

  static uint32_t _rolloverCount;
  static uint32_t _lastValue;
};

/*
  MitsuQTT Copyright (c) 2024 floatplane

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU
  Lesser General Public License as published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version. This library is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with this library;
  if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

#include <climits>
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

#ifdef Arduino_h
  static Moment now() {
    return Moment(millis());
  }
#endif

  static Moment never() {
    return Moment(Never::never);
  }

  explicit Moment(uint32_t value) {
    assign(value);
  }

  Moment(const Moment &rhs) = default;
  Moment(Moment &&rhs) = default;
  Moment &operator=(const Moment &rhs) = default;
  Moment &operator=(Moment &&rhs) = default;

  Moment &offset(int32_t value) {
    _milliseconds += value;
    return *this;
  }

  int64_t operator-(const Moment &other) const {
    if (other._milliseconds == LLONG_MIN) {
      return LLONG_MAX;
    }
    if (_milliseconds == LLONG_MIN) {
      return -other._milliseconds;
    }
    return _milliseconds - other._milliseconds;
  }

  bool operator<(const Moment &other) const {
    return _milliseconds < other._milliseconds;
  }
  bool operator>(const Moment &other) const {
    return _milliseconds > other._milliseconds;
  }
  bool operator<=(const Moment &other) const {
    return _milliseconds <= other._milliseconds;
  }
  bool operator>=(const Moment &other) const {
    return _milliseconds >= other._milliseconds;
  }
  bool operator==(const Moment &other) const {
    return _milliseconds == other._milliseconds;
  }
  bool operator!=(const Moment &other) const {
    return _milliseconds != other._milliseconds;
  }

  MomentParts get() const {
    return {
        .milliseconds = static_cast<uint16_t>(_milliseconds % 1000UL),
        .days = static_cast<uint16_t>(_milliseconds / 1000UL / 60UL / 60UL / 24UL % 365UL),
        .years = static_cast<uint8_t>(_milliseconds / 1000UL / 60UL / 60UL / 24UL / 365UL),
        .hours = static_cast<uint8_t>(_milliseconds / 1000UL / 60UL / 60UL % 24UL),
        .seconds = static_cast<uint8_t>(_milliseconds / 1000UL % 60UL),
        .minutes = static_cast<uint8_t>(_milliseconds / 1000UL / 60UL % 60UL),
    };
  }

 private:
  void assign(uint32_t value) {
    if (value < _lastValue) {
      _rolloverCount++;
    }
    _lastValue = value;
    this->_milliseconds =
        static_cast<int64_t>(value) + static_cast<int64_t>(_rolloverCount) * 0xFFFFFFFFLL;
  }

  enum class Never {
    never,
  };

  explicit Moment(Never /*never*/) : _milliseconds(LLONG_MIN) {
  }

  int64_t _milliseconds;

  static uint32_t _rolloverCount;
  static uint32_t _lastValue;
};

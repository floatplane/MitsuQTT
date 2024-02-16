#pragma once

#include <Arduino.h>
#include <HeatPump.h>

#include "temperature.hpp"

class HeatpumpStatus {
 public:
  HeatpumpStatus() = delete;

  explicit HeatpumpStatus(const heatpumpStatus& status)
      : roomTemperature(status.roomTemperature, Temperature::Unit::C),
        operating(status.operating),
        timers(status.timers),
        compressorFrequency(status.compressorFrequency) {
  }

  Temperature roomTemperature;
  bool operating;  // if true, the heatpump is operating to reach the desired temperature
  heatpumpTimers timers;
  int compressorFrequency;
};

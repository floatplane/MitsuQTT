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

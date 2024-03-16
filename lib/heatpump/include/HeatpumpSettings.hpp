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

class HeatpumpSettings {
 public:
  HeatpumpSettings() = delete;

  explicit HeatpumpSettings(const heatpumpSettings& settings)
      : power(settings.power),
        mode(settings.mode),
        temperature(settings.temperature, Temperature::Unit::C),
        fan(settings.fan),
        vane(settings.vane),
        wideVane(settings.wideVane),
        iSee(settings.iSee),
        connected(settings.connected) {
  }

  heatpumpSettings getRaw() const {
    return heatpumpSettings{.power = power.c_str(),
                            .mode = mode.c_str(),
                            .temperature = temperature.getCelsius(),
                            .fan = fan.c_str(),
                            .vane = vane.c_str(),
                            .wideVane = wideVane.c_str(),
                            .iSee = iSee,
                            .connected = connected};
  }

  String power;
  String mode;
  Temperature temperature;
  String fan;
  String vane;      // vertical vane, up/down
  String wideVane;  // horizontal vane, left/right
  bool iSee;        // iSee sensor, at the moment can only detect it, not set it
  bool connected;
};

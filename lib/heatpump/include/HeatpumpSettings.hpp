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

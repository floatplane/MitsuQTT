#pragma once

#include <Arduino.h>
#include <HeatPump.h>

class HeatpumpSettings {
 public:
  HeatpumpSettings() = delete;

  explicit HeatpumpSettings(const heatpumpSettings& settings)
      : power(settings.power),
        mode(settings.mode),
        temperature(settings.temperature),
        fan(settings.fan),
        vane(settings.vane),
        wideVane(settings.wideVane),
        iSee(settings.iSee),
        connected(settings.connected) {}

  heatpumpSettings getRaw() const {
    return heatpumpSettings{.power = power.c_str(),
                            .mode = mode.c_str(),
                            .temperature = temperature,
                            .fan = fan.c_str(),
                            .vane = vane.c_str(),
                            .wideVane = wideVane.c_str(),
                            .iSee = iSee,
                            .connected = connected};
  }

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  String power;
  String mode;
  float temperature;
  String fan;
  String vane;      // vertical vane, up/down
  String wideVane;  // horizontal vane, left/right
  bool iSee;        // iSee sensor, at the moment can only detect it, not set it
  bool connected;
  // NOLINTEND(misc-non-private-member-variables-in-classes)
};

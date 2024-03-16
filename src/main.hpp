/*
  MitsuQTT Copyright (c) 2024 floatplane
  Based on mitsubishi2mqtt Copyright (c) 2022 gysmo38, dzungpv, shampeon, endeavour,
  jascdk, chrdavis, alekslyse. All rights reserved.

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU
  Lesser General Public License as published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version. This library is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with this library;
  if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

#include <Arduino.h>
#include <HeatPump.h>

#include "HeatpumpSettings.hpp"
#include "HeatpumpStatus.hpp"

String getId();

void loadWifiConfig();
void loadOthersConfig();
void loadUnitConfig();
void loadMqttConfig();

bool initWifi();
void handleRoot();
void handleNotFound();
void handleInitSetup();
void handleSaveWifi();
void handleReboot();
void handleSetup();
void handleMqtt();
void handleUnitGet();
void handleUnitPost();
void handleWifi();
void handleStatus();
void handleOthers();
void handleMetrics();
void handleMetricsJson();
void handleLogin();
void handleAuth();
void handleLogout();
void handleUpgrade();
void handleUploadDone();
void handleUploadLoop();
void handleControlGet();
void handleControlPost();
void initMqtt();
void initCaptivePortal();
void hpPacketDebug(byte *packet_, unsigned int length, char *packetDirection_);
float convertCelsiusToLocalUnit(float temperature, bool isFahrenheit);
float convertLocalUnitToCelsius(float temperature, bool isFahrenheit);
String hpGetMode(const HeatpumpSettings &hpSettings);
String hpGetAction(const HeatpumpStatus &hpStatus, const HeatpumpSettings &hpSettings);
void mqttCallback(const char *topic, const byte *payload, unsigned int length);
void onSetCustomPacket(const char *message);
void onSetDebugLogs(const char *message);
void onSetDebugPackets(const char *message);
void onSetSystem(const char *message);
void onSetRemoteTemp(const char *message);
void onSetWideVane(const char *message);
void onSetVane(const char *message);
void onSetFan(const char *message);
void onSetTemp(const char *message);
void onSetMode(const char *message);
void mqttConnect();
bool connectWifi();
bool checkLogin();
HeatpumpSettings change_states(const HeatpumpSettings &settings);
String getTemperatureScale();
bool is_authenticated();
void hpCheckRemoteTemp();
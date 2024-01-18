#include <Arduino.h>
#include <HeatPump.h>

#include "HeatpumpSettings.hpp"

String getId();
void setDefaults();
bool loadWifi();
bool loadOthers();
bool loadUnit();
bool initWifi();
void handleRoot();
void handleNotFound();
void handleInitSetup();
void handleSaveWifi();
void handleReboot();
void handleSetup();
void handleMqtt();
void handleUnit();
void handleWifi();
void handleStatus();
void handleOthers();
void handleMetrics();
void handleLogin();
void handleUpgrade();
void handleUploadDone();
void handleUploadLoop();
void handleControl();
bool loadMqtt();
void initMqtt();
void initOTA();
void initCaptivePortal();
void hpSettingsChanged();
void hpStatusChanged(heatpumpStatus newStatus);
void hpPacketDebug(byte *packet, unsigned int length, const char *packetDirection);
float convertCelsiusToLocalUnit(float temperature, bool isFahrenheit);
float convertLocalUnitToCelsius(float temperature, bool isFahrenheit);
String hpGetMode(const HeatpumpSettings &hpSettings);
String hpGetAction(heatpumpStatus hpStatus, const HeatpumpSettings &hpSettings);
void mqttCallback(char *topic, byte *payload, unsigned int length);
void mqttConnect();
bool connectWifi();
bool checkLogin();
HeatpumpSettings change_states(const HeatpumpSettings &settings);
String getTemperatureScale();
bool is_authenticated();
void hpCheckRemoteTemp();
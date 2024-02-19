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
void handleUpgrade();
void handleUploadDone();
void handleUploadLoop();
void handleControl();
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
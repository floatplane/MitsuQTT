/*
  mitsubishi2mqtt - Mitsubishi Heat Pump to MQTT control for Home Assistant.
  Copyright (c) 2022 gysmo38, dzungpv, shampeon, endeavour, jascdk, chrdavis,
  alekslyse.  All right reserved. This library is free software; you can
  redistribute it and/or modify it under the terms of the GNU Lesser General
  Public License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <FS.h>  // SPIFFS for store config
#ifdef ESP32
#include <ESPmDNS.h>    // mDNS for ESP32
#include <SPIFFS.h>     // ESP32 SPIFFS for store config
#include <WebServer.h>  // webServer for ESP32
#include <WiFi.h>       // WIFI for ESP32
#include <WiFiUdp.h>

WebServer server(80);  // ESP32 web
#else
#include <ESP8266WebServer.h>  // webServer for ESP8266
#include <ESP8266WiFi.h>       // WIFI for ESP8266
#include <ESP8266mDNS.h>       // mDNS for ESP8266
#include <WiFiClient.h>
ESP8266WebServer server(80);  // ESP8266 web
#endif
#include <ArduinoJson.h>   // json to process MQTT: ArduinoJson 6.11.4
#include <ArduinoOTA.h>    // for OTA
#include <DNSServer.h>     // DNS for captive portal
#include <HeatPump.h>      // SwiCago library: https://github.com/SwiCago/HeatPump
#include <PubSubClient.h>  // MQTT: PubSubClient 2.8.0
#include <math.h>          // for rounding to Fahrenheit values
// #include <Ticker.h>     // for LED status (Using a Wemos D1-Mini)

#include "HeatpumpSettings.hpp"
// #include "config.hpp"             // config file
#include "html_common.hpp"        // common code HTML (like header, footer)
#include "html_init.hpp"          // code html for initial config
#include "html_menu.hpp"          // code html for menu
#include "html_metrics.hpp"       // prometheus metrics
#include "html_pages.hpp"         // code html for pages
#include "javascript_common.hpp"  // common code javascript (like refresh page)
#include "mitsubishi2mqtt.hpp"

// BEGIN include the contents of config.h
#ifndef LANG_PATH
#define LANG_PATH "languages/en-GB.h"  // default language English
#endif

// Define global variables for files
const size_t maxFileSize = 1024;
#ifdef ESP32
const PROGMEM char *const wifi_conf = "/wifi.json";
const PROGMEM char *const mqtt_conf = "/mqtt.json";
const PROGMEM char *const unit_conf = "/unit.json";
const PROGMEM char *const console_file = "/console.log";
const PROGMEM char *const others_conf = "/others.json";
// pinouts
const PROGMEM uint8_t blueLedPin = 2;  // The ESP32 has an internal blue LED at D2 (GPIO 02)
#else
const PROGMEM char *const wifi_conf = "wifi.json";
const PROGMEM char *const mqtt_conf = "mqtt.json";
const PROGMEM char *const unit_conf = "unit.json";
const PROGMEM char *const console_file = "console.log";
const PROGMEM char *const others_conf = "others.json";
// pinouts
const PROGMEM uint8_t blueLedPin = LED_BUILTIN;  // Onboard LED = digital pin 2 "D4" (blue LED on
                                                 // WEMOS D1-Mini)
#endif
const PROGMEM uint8_t redLedPin = 0;

// Define global variables for network
const PROGMEM char *const hostnamePrefix = "HVAC_";
const PROGMEM uint32_t WIFI_RETRY_INTERVAL_MS = 300000;
unsigned long wifi_timeout;
bool wifi_config_exists;
String hostname = "";
String ap_ssid;
String ap_pwd;
String ota_pwd;

enum HttpStatusCodes {
  httpOk = 200,
  httpFound = 302,
  httpSeeOther = 303,
  httpBadRequest = 400,
  httpUnauthorized = 401,
  httpForbidden = 403,
  httpNotFound = 404,
  httpMethodNotAllowed = 405,
  httpInternalServerError = 500,
  httpNotImplemented = 501,
  httpServiceUnavailable = 503,
};

// Define global variables for MQTT
String mqtt_fn;
String mqtt_server;
String mqtt_port;
String mqtt_username;
String mqtt_password;
String mqtt_topic = "mitsubishi2mqtt";
String mqtt_client_id;
const PROGMEM char *const mqtt_payload_available = "online";
const PROGMEM char *const mqtt_payload_unavailable = "offline";
const size_t maxCustomPacketLength = 20;  // max custom packet bytes is 20

// Define global variables for Others settings
bool others_haa;
String others_haa_topic;

// Define global variables for HA topics
String ha_system_set_topic;
String ha_mode_set_topic;
String ha_temp_set_topic;
String ha_remote_temp_set_topic;
String ha_fan_set_topic;
String ha_vane_set_topic;
String ha_wideVane_set_topic;
String ha_settings_topic;
String ha_state_topic;
String ha_debug_pckts_topic;
String ha_debug_pckts_set_topic;
String ha_debug_logs_topic;
String ha_debug_logs_set_topic;
String ha_config_topic;
String ha_discovery_topic;
String ha_custom_packet;
String ha_availability_topic;
String hvac_name;

// login
String login_username = "admin";
String login_password;

// debug mode logs, when true, will send all debug messages to topic
// heatpump_debug_logs_topic this can also be set by sending "on" to
// heatpump_debug_set_topic
bool g_debugModeLogs = false;
// debug mode packets, when true, will send all packets received from the
// heatpump to topic heatpump_debug_packets_topic this can also be set by
// sending "on" to heatpump_debug_set_topic
bool g_debugModePckts = false;

// Customization
const uint8_t defaultMinimumTemp = 16;
const uint8_t defaultMaximumTemp = 31;
const char *defaultTempStep = "1";
uint8_t min_temp{defaultMinimumTemp};  // Minimum temperature, in your selected unit, check
                                       // value from heatpump remote control
uint8_t max_temp{defaultMaximumTemp};  // Maximum temperature, in your selected unit, check
                                       // value from heatpump remote control
String temp_step{defaultTempStep};     // Temperature setting step, check
                                       // value from heatpump remote control

// sketch settings
const PROGMEM uint32_t SEND_ROOM_TEMP_INTERVAL_MS =
    30000;  // 45 seconds (anything less may cause bouncing)
const PROGMEM uint32_t CHECK_REMOTE_TEMP_INTERVAL_MS = 300000;  // 5 minutes
const PROGMEM uint32_t MQTT_RETRY_INTERVAL_MS = 1000;           // 1 second
const PROGMEM uint32_t HP_RETRY_INTERVAL_MS = 1000;             // 1 second
const PROGMEM uint32_t HP_MAX_RETRIES =
    10;  // Double the interval between retries up to this many times, then keep
         // retrying forever at that maximum interval.
// Default values give a final retry interval of 1000ms * 2^10, which is 1024
// seconds, about 17 minutes.

// temp settings
bool useFahrenheit = false;
// support heat mode settings, some model do not support heat mode
bool supportHeatMode = true;
// END include the contents of config.h

// Languages
#include LANG_PATH  // defined in config.h

// wifi, mqtt and heatpump client instances
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Captive portal variables, only used for config page
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;

boolean captive = false;
boolean mqtt_config = false;
boolean wifi_config = false;
boolean remoteTempActive = false;

// HVAC
HeatPump hp;  // NOLINT(readability-identifier-length)
unsigned long lastTempSend;
unsigned long lastMqttRetry;
unsigned long lastHpSync;
unsigned int hpConnectionRetries;
unsigned int hpConnectionTotalRetries;
unsigned long lastRemoteTemp;

// Local state
StaticJsonDocument<JSON_OBJECT_SIZE(12)> rootInfo;

// Web OTA
enum UploadError {
  noError = 0,
  noFileSelected,
  fileTooLarge,
  fileMagicHeaderIncorrect,
  fileTooBigForDeviceFlash,
  fileUploadBufferMiscompare,
  fileUploadFailed,
  fileUploadAborted,
};
UploadError uploaderror = UploadError::noError;

// cppcheck-suppress unusedFunction
void setup() {
  // Start serial for debug before HVAC connect to serial
  Serial.begin(115200);
  // Serial.println(F("Starting Mitsubishi2MQTT"));
  // Mount SPIFFS filesystem
  if (SPIFFS.begin()) {  // NOLINT(bugprone-branch-clone)
    // Serial.println(F("Mounted file system"));
  } else {
    // Serial.println(F("Failed to mount FS -> formating"));
    SPIFFS.format();
    // if (SPIFFS.begin())
    // Serial.println(F("Mounted file system after formating"));
  }
  // set led pin as output
  pinMode(blueLedPin, OUTPUT);
  /*
    ticker.attach(0.6, tick);
  */

  // Define hostname
  hostname += hostnamePrefix;
  hostname += getId();
  setDefaults();
  wifi_config_exists = loadWifi();
  loadOthers();
  loadUnit();
  mqtt_client_id = hostname;
#ifdef ESP32
  WiFi.setHostname(hostname.c_str());
#else
  WiFi.hostname(hostname.c_str());
#endif
  if (initWifi()) {
    if (SPIFFS.exists(console_file)) {
      SPIFFS.remove(console_file);
    }
    // write_log("Starting Mitsubishi2MQTT");
    // Web interface
    server.on("/", handleRoot);
    server.on("/control", handleControl);
    server.on("/setup", handleSetup);
    server.on("/mqtt", handleMqtt);
    server.on("/wifi", handleWifi);
    server.on("/unit", handleUnit);
    server.on("/status", handleStatus);
    server.on("/others", handleOthers);
    server.on("/metrics", handleMetrics);
    server.onNotFound(handleNotFound);
    if (login_password.length() > 0) {
      server.on("/login", handleLogin);
      // here the list of headers to be recorded, use for authentication
      const char *headerkeys[] = {"User-Agent", "Cookie"};
      size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
      // ask server to track these headers
      server.collectHeaders(headerkeys, headerkeyssize);
    }
    server.on("/upgrade", handleUpgrade);
    server.on("/upload", HTTP_POST, handleUploadDone, handleUploadLoop);

    server.begin();
    lastMqttRetry = 0;
    lastHpSync = 0;
    hpConnectionRetries = 0;
    hpConnectionTotalRetries = 0;
    if (loadMqtt()) {
      // write_log("Starting MQTT");
      //  setup HA topics
      ha_mode_set_topic = mqtt_topic + "/" + mqtt_fn + "/mode/set";
      ha_temp_set_topic = mqtt_topic + "/" + mqtt_fn + "/temp/set";
      ha_remote_temp_set_topic = mqtt_topic + "/" + mqtt_fn + "/remote_temp/set";
      ha_fan_set_topic = mqtt_topic + "/" + mqtt_fn + "/fan/set";
      ha_vane_set_topic = mqtt_topic + "/" + mqtt_fn + "/vane/set";
      ha_wideVane_set_topic = mqtt_topic + "/" + mqtt_fn + "/wideVane/set";
      ha_settings_topic = mqtt_topic + "/" + mqtt_fn + "/settings";
      ha_state_topic = mqtt_topic + "/" + mqtt_fn + "/state";
      ha_debug_pckts_topic = mqtt_topic + "/" + mqtt_fn + "/debug/packets";
      ha_debug_pckts_set_topic = mqtt_topic + "/" + mqtt_fn + "/debug/packets/set";
      ha_debug_logs_topic = mqtt_topic + "/" + mqtt_fn + "/debug/logs";
      ha_debug_logs_set_topic = mqtt_topic + "/" + mqtt_fn + "/debug/logs/set";
      ha_custom_packet = mqtt_topic + "/" + mqtt_fn + "/custom/send";
      ha_availability_topic = mqtt_topic + "/" + mqtt_fn + "/availability";
      ha_system_set_topic = mqtt_topic + "/" + mqtt_fn + "/system/set";

      if (others_haa) {
        ha_config_topic = others_haa_topic + "/climate/" + mqtt_fn + "/config";
      }
      // startup mqtt connection
      initMqtt();
    } else {
      // write_log("Not found MQTT config go to configuration page");
    }
    // Serial.println(F("Connection to HVAC. Stop serial log."));
    // write_log("Connection to HVAC");
    hp.setSettingsChangedCallback(hpSettingsChanged);
    hp.setStatusChangedCallback(hpStatusChanged);
    hp.setPacketCallback(hpPacketDebug);
    // Allow Remote/Panel
    hp.enableExternalUpdate();
    hp.enableAutoUpdate();
    hp.connect(&Serial);
    const heatpumpStatus currentStatus = hp.getStatus();
    const HeatpumpSettings currentSettings(hp.getSettings());
    rootInfo["roomTemperature"] =
        convertCelsiusToLocalUnit(currentStatus.roomTemperature, useFahrenheit);
    rootInfo["temperature"] = convertCelsiusToLocalUnit(currentSettings.temperature, useFahrenheit);
    rootInfo["fan"] = currentSettings.fan;
    rootInfo["vane"] = currentSettings.vane;
    rootInfo["wideVane"] = currentSettings.wideVane;
    rootInfo["mode"] = hpGetMode(currentSettings);
    rootInfo["action"] = hpGetAction(currentStatus, currentSettings);
    rootInfo["compressorFrequency"] = currentStatus.compressorFrequency;
    lastTempSend = millis();
  } else {
    dnsServer.start(DNS_PORT, "*", apIP);
    initCaptivePortal();
  }
  initOTA();
}

bool loadWifi() {
  ap_ssid = "";
  ap_pwd = "";
  if (!SPIFFS.exists(wifi_conf)) {
    // Serial.println(F("Wifi config file not exist!"));
    return false;
  }
  File configFile = SPIFFS.open(wifi_conf, "r");
  if (!configFile) {
    // Serial.println(F("Failed to open wifi config file"));
    return false;
  }
  size_t size = configFile.size();
  if (size > maxFileSize) {
    // Serial.println(F("Wifi config file size is too large"));
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  const size_t capacity = JSON_OBJECT_SIZE(4) + 130;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, buf.get());
  hostname = doc["hostname"].as<String>();
  ap_ssid = doc["ap_ssid"].as<String>();
  ap_pwd = doc["ap_pwd"].as<String>();
  // prevent ota password is "null" if not exist key
  if (doc.containsKey("ota_pwd")) {
    ota_pwd = doc["ota_pwd"].as<String>();
  } else {
    ota_pwd = "";
  }
  return true;
}
bool loadMqtt() {
  if (!SPIFFS.exists(mqtt_conf)) {
    Serial.println(F("MQTT config file not exist!"));
    return false;
  }
  // write_log("Loading MQTT configuration");
  File configFile = SPIFFS.open(mqtt_conf, "r");
  if (!configFile) {
    // write_log("Failed to open MQTT config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > maxFileSize) {
    // write_log("Config file size is too large");
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  const size_t capacity = JSON_OBJECT_SIZE(6) + 400;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, buf.get());
  mqtt_fn = doc["mqtt_fn"].as<String>();
  mqtt_server = doc["mqtt_host"].as<String>();
  mqtt_port = doc["mqtt_port"].as<String>();
  mqtt_username = doc["mqtt_user"].as<String>();
  mqtt_password = doc["mqtt_pwd"].as<String>();
  mqtt_topic = doc["mqtt_topic"].as<String>();

  // write_log("=== START DEBUG MQTT ===");
  // write_log("Friendly Name" + mqtt_fn);
  // write_log("IP Server " + mqtt_server);
  // write_log("IP Port " + mqtt_port);
  // write_log("Username " + mqtt_username);
  // write_log("Password " + mqtt_password);
  // write_log("Topic " + mqtt_topic);
  // write_log("=== END DEBUG MQTT ===");

  mqtt_config = true;
  return true;
}

bool loadUnit() {
  if (!SPIFFS.exists(unit_conf)) {
    // Serial.println(F("Unit config file not exist!"));
    return false;
  }
  File configFile = SPIFFS.open(unit_conf, "r");
  if (!configFile) {
    return false;
  }

  size_t size = configFile.size();
  if (size > maxFileSize) {
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  const size_t capacity = JSON_OBJECT_SIZE(3) + 200;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, buf.get());
  // unit
  String unit_tempUnit = doc["unit_tempUnit"].as<String>();
  if (unit_tempUnit == "fah") {
    useFahrenheit = true;
  }
  min_temp = doc["min_temp"].as<uint8_t>();
  max_temp = doc["max_temp"].as<uint8_t>();
  temp_step = doc["temp_step"].as<String>();
  // mode
  String supportMode = doc["support_mode"].as<String>();
  if (supportMode == "nht") {
    supportHeatMode = false;
  }
  // prevent login password is "null" if not exist key
  if (doc.containsKey("login_password")) {
    login_password = doc["login_password"].as<String>();
  } else {
    login_password = "";
  }
  return true;
}

bool loadOthers() {
  if (!SPIFFS.exists(others_conf)) {
    // Serial.println(F("Others config file not exist!"));
    return false;
  }
  File configFile = SPIFFS.open(others_conf, "r");
  if (!configFile) {
    return false;
  }

  size_t size = configFile.size();
  if (size > maxFileSize) {
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  const size_t capacity = JSON_OBJECT_SIZE(4) + 200;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, buf.get());
  // unit
  String unit_tempUnit = doc["unit_tempUnit"].as<String>();
  if (unit_tempUnit == "fah") {
    useFahrenheit = true;
  }
  others_haa_topic = doc["haat"].as<String>();
  String haa = doc["haa"].as<String>();
  String debugPckts = doc["debugPckts"].as<String>();
  String debugLogs = doc["debugLogs"].as<String>();
  if (strcmp(haa.c_str(), "OFF") == 0) {
    others_haa = false;
  }
  if (strcmp(debugPckts.c_str(), "ON") == 0) {
    g_debugModePckts = true;
  }
  if (strcmp(debugLogs.c_str(), "ON") == 0) {
    g_debugModeLogs = true;
  }
  return true;
}

struct SaveMqttArgs {
  String mqttFn{};
  String mqttHost{};
  String mqttPort{};
  String mqttUser{};
  String mqttPwd{};
  String mqttTopic{};
};
void saveMqtt(const SaveMqttArgs &args) {
  const size_t capacity = JSON_OBJECT_SIZE(6) + 400;
  DynamicJsonDocument doc(capacity);
  doc["mqtt_fn"] = args.mqttFn;
  doc["mqtt_host"] = args.mqttHost;
  // if mqtt port is empty, we use default port
  doc["mqtt_port"] = args.mqttPort[0] == '\0' ? "1883" : args.mqttPort;
  doc["mqtt_user"] = args.mqttUser;
  doc["mqtt_pwd"] = args.mqttPwd;
  doc["mqtt_topic"] = args.mqttTopic;
  File configFile = SPIFFS.open(mqtt_conf, "w");
  if (!configFile) {
    // Serial.println(F("Failed to open config file for writing"));
  }
  serializeJson(doc, configFile);
  configFile.close();
}

struct SaveUnitArgs {
  String tempUnit{};
  String supportMode{};
  String loginPassword{};
  String minTemp{};
  String maxTemp{};
  String tempStep{};
};
void saveUnit(const SaveUnitArgs &args) {
  const size_t capacity = JSON_OBJECT_SIZE(6) + 200;
  DynamicJsonDocument doc(capacity);
  // if temp unit is empty, we use default celsius
  doc["unit_tempUnit"] = args.tempUnit.isEmpty() ? "cel" : args.tempUnit;
  // if minTemp is empty, we use default 16
  doc["min_temp"] = args.minTemp.isEmpty() ? String(defaultMinimumTemp) : args.minTemp;
  // if maxTemp is empty, we use default 31
  doc["max_temp"] = args.maxTemp.isEmpty() ? String(defaultMaximumTemp) : args.maxTemp;
  // if tempStep is empty, we use default 1
  doc["temp_step"] = args.tempStep.isEmpty() ? defaultTempStep : args.tempStep;
  // if support mode is empty, we use default all mode
  doc["support_mode"] = args.supportMode.isEmpty() ? "all" : args.supportMode;
  // if login password is empty, we use empty
  doc["login_password"] = args.loginPassword.isEmpty() ? "" : args.loginPassword;
  File configFile = SPIFFS.open(unit_conf, "w");
  if (!configFile) {
    // Serial.println(F("Failed to open config file for writing"));
  }
  serializeJson(doc, configFile);
  configFile.close();
}

struct SaveWifiArgs {
  String apSsid{};
  String apPwd{};
  String hostName{};
  String otaPwd{};
};
void saveWifi(const SaveWifiArgs &args) {
  const size_t capacity = JSON_OBJECT_SIZE(4) + 130;
  DynamicJsonDocument doc(capacity);
  doc["ap_ssid"] = args.apSsid;
  doc["ap_pwd"] = args.apPwd;
  doc["hostname"] = args.hostName;
  doc["ota_pwd"] = args.otaPwd;
  File configFile = SPIFFS.open(wifi_conf, "w");
  if (!configFile) {
    // Serial.println(F("Failed to open wifi file for writing"));
  }
  serializeJson(doc, configFile);
  delay(10);
  configFile.close();
}

struct SaveOthersArgs {
  String haa{};
  String haat{};
  String debugPckts{};
  String debugLogs{};
};
void saveOthers(const SaveOthersArgs &args) {
  const size_t capacity = JSON_OBJECT_SIZE(4) + 130;
  DynamicJsonDocument doc(capacity);
  doc["haa"] = args.haa;
  doc["haat"] = args.haat;
  doc["debugPckts"] = args.debugPckts;
  doc["debugLogs"] = args.debugLogs;
  File configFile = SPIFFS.open(others_conf, "w");
  if (!configFile) {
    // Serial.println(F("Failed to open wifi file for writing"));
  }
  serializeJson(doc, configFile);
  delay(10);
  configFile.close();
}

// Initialize captive portal page
void initCaptivePortal() {
  // Serial.println(F("Starting captive portal"));
  server.on("/", handleInitSetup);
  server.on("/save", handleSaveWifi);
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);
  server.begin();
  captive = true;
}

void initMqtt() {
  mqtt_client.setServer(mqtt_server.c_str(), atoi(mqtt_port.c_str()));
  mqtt_client.setCallback(mqttCallback);
  mqttConnect();
}

// Enable OTA only when connected as a client.
void initOTA() {
  // write_log("Start OTA Listener");
  ArduinoOTA.setHostname(hostname.c_str());
  if (ota_pwd.length() > 0) {
    ArduinoOTA.setPassword(ota_pwd.c_str());
  }
  ArduinoOTA.onStart([]() {
    // write_log("Start");
  });
  ArduinoOTA.onEnd([]() {
    // write_log("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //    write_log("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //    write_log("Error[%u]: ", error);
    // if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    // else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    // else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    // else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    // else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();
}
void setDefaults() {
  ap_ssid = "";
  ap_pwd = "";
  others_haa = true;
  others_haa_topic = "homeassistant";
}

bool initWifi() {
  bool connectWifiSuccess = true;
  if (ap_ssid[0] != '\0') {
    connectWifiSuccess = wifi_config = connectWifi();
    if (connectWifiSuccess) {
      return true;
    }
    // reset hostname back to default before starting AP mode for privacy
    hostname = hostnamePrefix;
    hostname += getId();
  }

  // Serial.println(F("\n\r \n\rStarting in AP mode"));
  WiFi.mode(WIFI_AP);
  wifi_timeout = millis() + WIFI_RETRY_INTERVAL_MS;
  WiFi.persistent(false);  // fix crash esp32
                           // https://github.com/espressif/arduino-esp32/issues/2025
  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (!connectWifiSuccess and login_password != "") {  // NOLINT(bugprone-branch-clone)
    // Set AP password when falling back to AP on fail
    WiFi.softAP(hostname.c_str(), login_password.c_str());
  } else {
    // First time setup does not require password
    WiFi.softAP(hostname.c_str());
  }
  delay(2000);  // VERY IMPORTANT

  // Serial.print(F("IP address: "));
  // Serial.println(WiFi.softAPIP());
  // ticker.attach(0.2, tick); // Start LED to flash rapidly to indicate we are
  // ready for setting up the wifi-connection (entered captive portal).
  wifi_config = false;
  return false;
}

// Handler webserver response

void sendWrappedHTML(String content) {
  String headerContent = FPSTR(html_common_header);
  String footerContent = FPSTR(html_common_footer);
  String toSend = headerContent + content + footerContent;
  toSend.replace(F("_UNIT_NAME_"), hostname);
  toSend.replace(F("_VERSION_"), BUILD_DATE);
  toSend.replace(F("_GIT_HASH_"), COMMIT_HASH);
  server.send(HttpStatusCodes::httpOk, F("text/html"), toSend);
}

void handleNotFound() {
  if (captive) {
    String initSetupContent = FPSTR(html_init_setup);
    initSetupContent.replace("_TXT_INIT_TITLE_", FPSTR(txt_init_title));
    initSetupContent.replace("_TXT_INIT_HOST_", FPSTR(txt_wifi_hostname));
    initSetupContent.replace("_UNIT_NAME_", hostname);
    initSetupContent.replace("_TXT_INIT_SSID_", FPSTR(txt_wifi_SSID));
    initSetupContent.replace("_TXT_INIT_PSK_", FPSTR(txt_wifi_psk));
    initSetupContent.replace("_TXT_INIT_OTA_", FPSTR(txt_wifi_otap));
    initSetupContent.replace("_TXT_SAVE_", FPSTR(txt_save));
    initSetupContent.replace("_TXT_REBOOT_", FPSTR(txt_reboot));

    sendWrappedHTML(initSetupContent);
  } else {
    server.sendHeader("Location", "/");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(HttpStatusCodes::httpFound);
    return;
  }
}

void handleSaveWifi() {
  if (!checkLogin()) {
    return;
  }

  // Serial.println(F("Saving wifi config"));
  if (server.method() == HTTP_POST) {
    saveWifi({.apSsid = server.arg("ssid"),
              .apPwd = server.arg("psk"),
              .hostName = server.arg("hn"),
              .otaPwd = server.arg("otapwd")});
  }
  String initSavePage = FPSTR(html_init_save);
  initSavePage.replace("_TXT_INIT_REBOOT_MESS_", FPSTR(txt_init_reboot_mes));
  sendWrappedHTML(initSavePage);
  delay(500);
  ESP.restart();
}

void handleReboot() {
  if (!checkLogin()) {
    return;
  }

  String initRebootPage = FPSTR(html_init_reboot);
  initRebootPage.replace("_TXT_INIT_REBOOT_", FPSTR(txt_init_reboot));
  sendWrappedHTML(initRebootPage);
  delay(500);
  ESP.restart();
}

void handleRoot() {
  if (!checkLogin()) {
    return;
  }

  if (server.hasArg("REBOOT")) {
    String rebootPage = FPSTR(html_page_reboot);
    String countDown = FPSTR(count_down_script);
    rebootPage.replace("_TXT_M_REBOOT_", FPSTR(txt_m_reboot));
    sendWrappedHTML(rebootPage + countDown);
    delay(500);
#ifdef ESP32
    ESP.restart();
#else
    ESP.reset();
#endif
  } else {
    String menuRootPage = FPSTR(html_menu_root);
    menuRootPage.replace("_SHOW_LOGOUT_", (String)(login_password.length() > 0));
    // not show control button if hp not connected
    menuRootPage.replace("_SHOW_CONTROL_", (String)(hp.isConnected()));
    menuRootPage.replace("_TXT_CONTROL_", FPSTR(txt_control));
    menuRootPage.replace("_TXT_SETUP_", FPSTR(txt_setup));
    menuRootPage.replace("_TXT_STATUS_", FPSTR(txt_status));
    menuRootPage.replace("_TXT_FW_UPGRADE_", FPSTR(txt_firmware_upgrade));
    menuRootPage.replace("_TXT_REBOOT_", FPSTR(txt_reboot));
    menuRootPage.replace("_TXT_LOGOUT_", FPSTR(txt_logout));
    sendWrappedHTML(menuRootPage);
  }
}

void handleInitSetup() {
  String initSetupPage = FPSTR(html_init_setup);
  initSetupPage.replace("_TXT_INIT_TITLE_", FPSTR(txt_init_title));
  initSetupPage.replace("_TXT_INIT_HOST_", FPSTR(txt_wifi_hostname));
  initSetupPage.replace("_TXT_INIT_SSID_", FPSTR(txt_wifi_SSID));
  initSetupPage.replace("_TXT_INIT_PSK_", FPSTR(txt_wifi_psk));
  initSetupPage.replace("_TXT_INIT_OTA_", FPSTR(txt_wifi_otap));
  initSetupPage.replace("_TXT_SAVE_", FPSTR(txt_save));
  initSetupPage.replace("_TXT_REBOOT_", FPSTR(txt_reboot));

  sendWrappedHTML(initSetupPage);
}

void handleSetup() {
  if (!checkLogin()) {
    return;
  }

  if (server.hasArg("RESET")) {
    String pageReset = FPSTR(html_page_reset);
    String ssid = hostnamePrefix;
    ssid += getId();
    pageReset.replace("_TXT_M_RESET_", FPSTR(txt_m_reset));
    pageReset.replace("_SSID_", ssid);
    sendWrappedHTML(pageReset);
    SPIFFS.format();
    delay(500);
#ifdef ESP32
    ESP.restart();
#else
    ESP.reset();
#endif
  } else {
    String menuSetupPage = FPSTR(html_menu_setup);
    menuSetupPage.replace("_TXT_MQTT_", FPSTR(txt_MQTT));
    menuSetupPage.replace("_TXT_WIFI_", FPSTR(txt_WIFI));
    menuSetupPage.replace("_TXT_UNIT_", FPSTR(txt_unit));
    menuSetupPage.replace("_TXT_OTHERS_", FPSTR(txt_others));
    menuSetupPage.replace("_TXT_RESET_", FPSTR(txt_reset));
    menuSetupPage.replace("_TXT_BACK_", FPSTR(txt_back));
    menuSetupPage.replace("_TXT_RESETCONFIRM_", FPSTR(txt_reset_confirm));
    sendWrappedHTML(menuSetupPage);
  }
}

void rebootAndSendPage() {
  String saveRebootPage = FPSTR(html_page_save_reboot);
  String countDown = FPSTR(count_down_script);
  saveRebootPage.replace("_TXT_M_SAVE_", FPSTR(txt_m_save));
  sendWrappedHTML(saveRebootPage + countDown);
  delay(500);
  ESP.restart();
}

void handleOthers() {
  if (!checkLogin()) {
    return;
  }

  if (server.method() == HTTP_POST) {
    saveOthers({
        .haa = server.arg("HAA"),
        .haat = server.arg("haat"),
        .debugPckts = server.arg("DebugPckts"),
        .debugLogs = server.arg("DebugLogs"),
    });
    rebootAndSendPage();
  } else {
    String othersPage = FPSTR(html_page_others);
    othersPage.replace("_TXT_SAVE_", FPSTR(txt_save));
    othersPage.replace("_TXT_BACK_", FPSTR(txt_back));
    othersPage.replace("_TXT_F_ON_", FPSTR(txt_f_on));
    othersPage.replace("_TXT_F_OFF_", FPSTR(txt_f_off));
    othersPage.replace("_TXT_OTHERS_TITLE_", FPSTR(txt_others_title));
    othersPage.replace("_TXT_OTHERS_HAAUTO_", FPSTR(txt_others_haauto));
    othersPage.replace("_TXT_OTHERS_HATOPIC_", FPSTR(txt_others_hatopic));
    othersPage.replace("_TXT_OTHERS_DEBUG_PCKTS_", FPSTR(txt_others_debug_packets));
    othersPage.replace("_TXT_OTHERS_DEBUG_LOGS_", FPSTR(txt_others_debug_log));

    othersPage.replace("_HAA_TOPIC_", others_haa_topic);
    if (others_haa) {
      othersPage.replace("_HAA_ON_", "selected");
    } else {
      othersPage.replace("_HAA_OFF_", "selected");
    }
    if (g_debugModePckts) {
      othersPage.replace("_DEBUG_PCKTS_ON_", "selected");
    } else {
      othersPage.replace("_DEBUG_PCKTS_OFF_", "selected");
    }
    if (g_debugModeLogs) {
      othersPage.replace("_DEBUG_LOGS_ON_", "selected");
    } else {
      othersPage.replace("_DEBUG_LOGS_OFF_", "selected");
    }
    sendWrappedHTML(othersPage);
  }
}

void handleMqtt() {
  if (!checkLogin()) {
    return;
  }

  if (server.method() == HTTP_POST) {
    saveMqtt({.mqttFn = server.arg("fn"),
              .mqttHost = server.arg("mh"),
              .mqttPort = server.arg("ml"),
              .mqttUser = server.arg("mu"),
              .mqttPwd = server.arg("mp"),
              .mqttTopic = server.arg("mt")});
    rebootAndSendPage();
  } else {
    String mqttPage = FPSTR(html_page_mqtt);
    mqttPage.replace("_TXT_SAVE_", FPSTR(txt_save));
    mqttPage.replace("_TXT_BACK_", FPSTR(txt_back));
    mqttPage.replace("_TXT_MQTT_TITLE_", FPSTR(txt_mqtt_title));
    mqttPage.replace("_TXT_MQTT_FN_", FPSTR(txt_mqtt_fn));
    mqttPage.replace("_TXT_MQTT_HOST_", FPSTR(txt_mqtt_host));
    mqttPage.replace("_TXT_MQTT_PORT_", FPSTR(txt_mqtt_port));
    mqttPage.replace("_TXT_MQTT_USER_", FPSTR(txt_mqtt_user));
    mqttPage.replace("_TXT_MQTT_PASSWORD_", FPSTR(txt_mqtt_password));
    mqttPage.replace("_TXT_MQTT_TOPIC_", FPSTR(txt_mqtt_topic));
    mqttPage.replace(F("_MQTT_FN_"), mqtt_fn);
    mqttPage.replace(F("_MQTT_HOST_"), mqtt_server);
    mqttPage.replace(F("_MQTT_PORT_"), String(mqtt_port));
    mqttPage.replace(F("_MQTT_USER_"), mqtt_username);
    mqttPage.replace(F("_MQTT_PASSWORD_"), mqtt_password);
    mqttPage.replace(F("_MQTT_TOPIC_"), mqtt_topic);
    sendWrappedHTML(mqttPage);
  }
}

void handleUnit() {
  if (!checkLogin()) {
    return;
  }

  if (server.method() == HTTP_POST) {
    saveUnit({.tempUnit = server.arg("tu"),
              .supportMode = server.arg("md"),
              .loginPassword = server.arg("lpw"),
              .minTemp =
                  (String)convertLocalUnitToCelsius(server.arg("min_temp").toInt(), useFahrenheit),
              .maxTemp =
                  (String)convertLocalUnitToCelsius(server.arg("max_temp").toInt(), useFahrenheit),
              .tempStep = server.arg("temp_step")});
    rebootAndSendPage();
  } else {
    String unitPage = FPSTR(html_page_unit);
    unitPage.replace("_TXT_SAVE_", FPSTR(txt_save));
    unitPage.replace("_TXT_BACK_", FPSTR(txt_back));
    unitPage.replace("_TXT_UNIT_TITLE_", FPSTR(txt_unit_title));
    unitPage.replace("_TXT_UNIT_TEMP_", FPSTR(txt_unit_temp));
    unitPage.replace("_TXT_UNIT_MINTEMP_", FPSTR(txt_unit_mintemp));
    unitPage.replace("_TXT_UNIT_MAXTEMP_", FPSTR(txt_unit_maxtemp));
    unitPage.replace("_TXT_UNIT_STEPTEMP_", FPSTR(txt_unit_steptemp));
    unitPage.replace("_TXT_UNIT_MODES_", FPSTR(txt_unit_modes));
    unitPage.replace("_TXT_UNIT_PASSWORD_", FPSTR(txt_unit_password));
    unitPage.replace("_TXT_F_CELSIUS_", FPSTR(txt_f_celsius));
    unitPage.replace("_TXT_F_FH_", FPSTR(txt_f_fh));
    unitPage.replace("_TXT_F_ALLMODES_", FPSTR(txt_f_allmodes));
    unitPage.replace("_TXT_F_NOHEAT_", FPSTR(txt_f_noheat));
    unitPage.replace(F("_MIN_TEMP_"), String(convertCelsiusToLocalUnit(min_temp, useFahrenheit)));
    unitPage.replace(F("_MAX_TEMP_"), String(convertCelsiusToLocalUnit(max_temp, useFahrenheit)));
    unitPage.replace(F("_TEMP_STEP_"), String(temp_step));
    // temp
    if (useFahrenheit) {
      unitPage.replace(F("_TU_FAH_"), F("selected"));
    } else {
      unitPage.replace(F("_TU_CEL_"), F("selected"));
    }
    // mode
    if (supportHeatMode) {
      unitPage.replace(F("_MD_ALL_"), F("selected"));
    } else {
      unitPage.replace(F("_MD_NONHEAT_"), F("selected"));
    }
    unitPage.replace(F("_LOGIN_PASSWORD_"), login_password);
    sendWrappedHTML(unitPage);
  }
}

void handleWifi() {
  if (!checkLogin()) {
    return;
  }

  if (server.method() == HTTP_POST) {
    saveWifi({.apSsid = server.arg("ssid"),
              .apPwd = server.arg("psk"),
              .hostName = server.arg("hn"),
              .otaPwd = server.arg("otapwd")});
    rebootAndSendPage();
#ifdef ESP32
    ESP.restart();
#else
    ESP.reset();
#endif
  } else {
    String wifiPage = FPSTR(html_page_wifi);
    String str_ap_ssid = ap_ssid;
    String str_ap_pwd = ap_pwd;
    String str_ota_pwd = ota_pwd;
    str_ap_ssid.replace("'", F("&apos;"));
    str_ap_pwd.replace("'", F("&apos;"));
    str_ota_pwd.replace("'", F("&apos;"));
    wifiPage.replace("_TXT_SAVE_", FPSTR(txt_save));
    wifiPage.replace("_TXT_BACK_", FPSTR(txt_back));
    wifiPage.replace("_TXT_WIFI_TITLE_", FPSTR(txt_wifi_title));
    wifiPage.replace("_TXT_WIFI_HOST_", FPSTR(txt_wifi_hostname));
    wifiPage.replace("_TXT_WIFI_SSID_", FPSTR(txt_wifi_SSID));
    wifiPage.replace("_TXT_WIFI_PSK_", FPSTR(txt_wifi_psk));
    wifiPage.replace("_TXT_WIFI_OTAP_", FPSTR(txt_wifi_otap));
    wifiPage.replace(F("_SSID_"), str_ap_ssid);
    wifiPage.replace(F("_PSK_"), str_ap_pwd);
    wifiPage.replace(F("_OTA_PWD_"), str_ota_pwd);
    sendWrappedHTML(wifiPage);
  }
}

void handleStatus() {
  if (!checkLogin()) {
    return;
  }

  String statusPage = FPSTR(html_page_status);
  statusPage.replace("_TXT_BACK_", FPSTR(txt_back));
  statusPage.replace("_TXT_STATUS_TITLE_", FPSTR(txt_status_title));
  statusPage.replace("_TXT_STATUS_HVAC_", FPSTR(txt_status_hvac));
  statusPage.replace("_TXT_STATUS_MQTT_", FPSTR(txt_status_mqtt));
  statusPage.replace("_TXT_STATUS_WIFI_", FPSTR(txt_status_wifi));
  statusPage.replace("_TXT_RETRIES_HVAC_", FPSTR(txt_retries_hvac));

  if (server.hasArg("mrconn")) {
    mqttConnect();
  }

  String connected = F("<span style='color:#47c266'><b>");
  connected += FPSTR(txt_status_connect);
  connected += F("</b><span>");

  String disconnected = F("<span style='color:#d43535'><b>");
  disconnected += FPSTR(txt_status_disconnect);
  disconnected += F("</b></span>");

  if ((Serial) and hp.isConnected()) {
    statusPage.replace(F("_HVAC_STATUS_"), connected);
  } else {
    statusPage.replace(F("_HVAC_STATUS_"), disconnected);
  }
  if (mqtt_client.connected()) {
    statusPage.replace(F("_MQTT_STATUS_"), connected);
  } else {
    statusPage.replace(F("_MQTT_STATUS_"), disconnected);
  }
  statusPage.replace(F("_HVAC_RETRIES_"), String(hpConnectionTotalRetries));
  statusPage.replace(F("_MQTT_REASON_"), String(mqtt_client.state()));
  statusPage.replace(F("_WIFI_STATUS_"), String(WiFi.RSSI()));
  sendWrappedHTML(statusPage);
}

void handleControl() {  // NOLINT(readability-function-cognitive-complexity)
  if (!checkLogin()) {
    return;
  }

  // not connected to hp, redirect to status page
  if (!hp.isConnected()) {
    server.sendHeader("Location", "/status");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(httpFound);
    return;
  }
  HeatpumpSettings settings(hp.getSettings());
  settings = change_states(settings);
  String controlPage = FPSTR(html_page_control);
  String headerContent = FPSTR(html_common_header);
  String footerContent = FPSTR(html_common_footer);
  // write_log("Enter HVAC control");
  headerContent.replace("_UNIT_NAME_", hostname);
  footerContent.replace("_VERSION_", BUILD_DATE);
  footerContent.replace("_GIT_HASH_", COMMIT_HASH);
  controlPage.replace("_TXT_BACK_", FPSTR(txt_back));
  controlPage.replace("_UNIT_NAME_", hostname);
  controlPage.replace("_RATE_", "60");
  controlPage.replace("_ROOMTEMP_",
                      String(convertCelsiusToLocalUnit(hp.getRoomTemperature(), useFahrenheit)));
  controlPage.replace("_USE_FAHRENHEIT_", (String)useFahrenheit);
  controlPage.replace("_TEMP_SCALE_", getTemperatureScale());
  controlPage.replace("_HEAT_MODE_SUPPORT_", (String)supportHeatMode);
  controlPage.replace(F("_MIN_TEMP_"), String(convertCelsiusToLocalUnit(min_temp, useFahrenheit)));
  controlPage.replace(F("_MAX_TEMP_"), String(convertCelsiusToLocalUnit(max_temp, useFahrenheit)));
  controlPage.replace(F("_TEMP_STEP_"), String(temp_step));
  controlPage.replace("_TXT_CTRL_CTEMP_", FPSTR(txt_ctrl_ctemp));
  controlPage.replace("_TXT_CTRL_TEMP_", FPSTR(txt_ctrl_temp));
  controlPage.replace("_TXT_CTRL_TITLE_", FPSTR(txt_ctrl_title));
  controlPage.replace("_TXT_CTRL_POWER_", FPSTR(txt_ctrl_power));
  controlPage.replace("_TXT_CTRL_MODE_", FPSTR(txt_ctrl_mode));
  controlPage.replace("_TXT_CTRL_FAN_", FPSTR(txt_ctrl_fan));
  controlPage.replace("_TXT_CTRL_VANE_", FPSTR(txt_ctrl_vane));
  controlPage.replace("_TXT_CTRL_WVANE_", FPSTR(txt_ctrl_wvane));
  controlPage.replace("_TXT_F_ON_", FPSTR(txt_f_on));
  controlPage.replace("_TXT_F_OFF_", FPSTR(txt_f_off));
  controlPage.replace("_TXT_F_AUTO_", FPSTR(txt_f_auto));
  controlPage.replace("_TXT_F_HEAT_", FPSTR(txt_f_heat));
  controlPage.replace("_TXT_F_DRY_", FPSTR(txt_f_dry));
  controlPage.replace("_TXT_F_COOL_", FPSTR(txt_f_cool));
  controlPage.replace("_TXT_F_FAN_", FPSTR(txt_f_fan));
  controlPage.replace("_TXT_F_QUIET_", FPSTR(txt_f_quiet));
  controlPage.replace("_TXT_F_SPEED_", FPSTR(txt_f_speed));
  controlPage.replace("_TXT_F_SWING_", FPSTR(txt_f_swing));
  controlPage.replace("_TXT_F_POS_", FPSTR(txt_f_pos));

  if (settings.power == "ON") {
    controlPage.replace("_POWER_ON_", "selected");
  } else if (settings.power == "OFF") {
    controlPage.replace("_POWER_OFF_", "selected");
  }

  if (settings.mode == "HEAT") {
    controlPage.replace("_MODE_H_", "selected");
  } else if (settings.mode == "DRY") {
    controlPage.replace("_MODE_D_", "selected");
  } else if (settings.mode == "COOL") {
    controlPage.replace("_MODE_C_", "selected");
  } else if (settings.mode == "FAN") {
    controlPage.replace("_MODE_F_", "selected");
  } else if (settings.mode == "AUTO") {
    controlPage.replace("_MODE_A_", "selected");
  }

  if (settings.fan == "AUTO") {
    controlPage.replace("_FAN_A_", "selected");
  } else if (settings.fan == "QUIET") {
    controlPage.replace("_FAN_Q_", "selected");
  } else if (settings.fan == "1") {
    controlPage.replace("_FAN_1_", "selected");
  } else if (settings.fan == "2") {
    controlPage.replace("_FAN_2_", "selected");
  } else if (settings.fan == "3") {
    controlPage.replace("_FAN_3_", "selected");
  } else if (settings.fan == "4") {
    controlPage.replace("_FAN_4_", "selected");
  }

  controlPage.replace("_VANE_V_", settings.vane);
  if (settings.vane == "AUTO") {
    controlPage.replace("_VANE_A_", "selected");
  } else if (settings.vane == "1") {
    controlPage.replace("_VANE_1_", "selected");
  } else if (settings.vane == "2") {
    controlPage.replace("_VANE_2_", "selected");
  } else if (settings.vane == "3") {
    controlPage.replace("_VANE_3_", "selected");
  } else if (settings.vane == "4") {
    controlPage.replace("_VANE_4_", "selected");
  } else if (settings.vane == "5") {
    controlPage.replace("_VANE_5_", "selected");
  } else if (settings.vane == "SWING") {
    controlPage.replace("_VANE_S_", "selected");
  }

  controlPage.replace("_WIDEVANE_V_", settings.wideVane);
  if (settings.wideVane == "<<") {
    controlPage.replace("_WVANE_1_", "selected");
  } else if (settings.wideVane == "<") {
    controlPage.replace("_WVANE_2_", "selected");
  } else if (settings.wideVane == "|") {
    controlPage.replace("_WVANE_3_", "selected");
  } else if (settings.wideVane == ">") {
    controlPage.replace("_WVANE_4_", "selected");
  } else if (settings.wideVane == ">>") {
    controlPage.replace("_WVANE_5_", "selected");
  } else if (settings.wideVane == "<>") {
    controlPage.replace("_WVANE_6_", "selected");
  } else if (settings.wideVane == "SWING") {
    controlPage.replace("_WVANE_S_", "selected");
  }
  controlPage.replace("_TEMP_",
                      String(convertCelsiusToLocalUnit(hp.getTemperature(), useFahrenheit)));

  // We need to send the page content in chunks to overcome
  // a limitation on the maximum size we can send at one
  // time (approx 6k).
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(HttpStatusCodes::httpOk, "text/html", headerContent);
  server.sendContent(controlPage);
  server.sendContent(footerContent);
  // Signal the end of the content
  server.sendContent("");
  // delay(100);
}

void handleMetrics() {
  String metrics = FPSTR(html_metrics);

  const HeatpumpSettings currentSettings(hp.getSettings());
  const heatpumpStatus currentStatus = hp.getStatus();

  String hppower = currentSettings.power == "ON" ? "1" : "0";

  String hpfan = currentSettings.fan;
  if (hpfan == "AUTO") {
    hpfan = "-1";
  }
  if (hpfan == "QUIET") {
    hpfan = "0";
  }

  String hpvane = currentSettings.vane;
  if (hpvane == "AUTO") {
    hpvane = "-1";
  }
  if (hpvane == "SWING") {
    hpvane = "0";
  }

  String hpwidevane = currentSettings.wideVane;
  if (hpwidevane == "SWING") {
    hpwidevane = "0";
  } else if (hpwidevane == "<<") {
    hpwidevane = "1";
  } else if (hpwidevane == "<") {
    hpwidevane = "2";
  } else if (hpwidevane == "|") {
    hpwidevane = "3";
  } else if (hpwidevane == ">") {
    hpwidevane = "4";
  } else if (hpwidevane == ">>") {
    hpwidevane = "5";
  } else if (hpwidevane == "<>") {
    hpwidevane = "6";
  } else {
    hpwidevane = "-2";
  }

  String hpmode = currentSettings.mode;
  if (hpmode == "AUTO") {
    hpmode = "-1";
  } else if (hpmode == "COOL") {
    hpmode = "1";
  } else if (hpmode == "DRY") {
    hpmode = "2";
  } else if (hpmode == "HEAT") {
    hpmode = "3";
  } else if (hpmode == "FAN") {
    hpmode = "4";
  } else if (hppower == "0") {
    hpmode = "0";
  } else {
    hpmode = "-2";
  }

  metrics.replace("_UNIT_NAME_", hostname);
  metrics.replace("_VERSION_", BUILD_DATE);
  metrics.replace("_GIT_HASH_", COMMIT_HASH);
  metrics.replace("_POWER_", hppower);
  metrics.replace("_ROOMTEMP_", String(currentStatus.roomTemperature));
  metrics.replace("_TEMP_", String(currentSettings.temperature));
  metrics.replace("_FAN_", hpfan);
  metrics.replace("_VANE_", hpvane);
  metrics.replace("_WIDEVANE_", hpwidevane);
  metrics.replace("_MODE_", hpmode);
  metrics.replace("_OPER_", String(currentStatus.operating));
  metrics.replace("_COMPFREQ_", String(currentStatus.compressorFrequency));
  server.send(HttpStatusCodes::httpOk, F("text/plain"), metrics);
}

// login page, also called for logout
void handleLogin() {
  bool loginSuccess = false;
  String msg{};
  String loginPage = FPSTR(html_page_login);
  loginPage.replace("_TXT_LOGIN_TITLE_", FPSTR(txt_login_title));
  loginPage.replace("_TXT_LOGIN_PASSWORD_", FPSTR(txt_login_password));
  loginPage.replace("_TXT_LOGIN_", FPSTR(txt_login));

  if (server.hasArg("USERNAME") || server.hasArg("PASSWORD") || server.hasArg("LOGOUT")) {
    if (server.hasArg("LOGOUT")) {
      // logout
      server.sendHeader("Cache-Control", "no-cache");
      server.sendHeader("Set-Cookie", "M2MSESSIONID=0");
      loginSuccess = false;
    }
    if (server.hasArg("USERNAME") && server.hasArg("PASSWORD")) {
      if (server.arg("USERNAME") == "admin" && server.arg("PASSWORD") == login_password) {
        server.sendHeader("Cache-Control", "no-cache");
        server.sendHeader("Set-Cookie", "M2MSESSIONID=1");
        loginSuccess = true;
        msg = F("<span style='color:#47c266;font-weight:bold;'>");
        msg += FPSTR(txt_login_sucess);
        msg += F("<span>");
        loginPage += F("<script>");
        loginPage += F("setTimeout(function () {");
        loginPage += F("window.location.href= '/';");
        loginPage += F("}, 3000);");
        loginPage += F("</script>");
        // Log in Successful;
      } else {
        msg = F("<span style='color:#d43535;font-weight:bold;'>");
        msg += FPSTR(txt_login_fail);
        msg += F("</span>");
        // Log in Failed;
      }
    }
  } else {
    if (is_authenticated() or login_password.length() == 0) {
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      // use javascript in the case browser disable redirect
      String redirectPage = F("<html lang=\"en\" class=\"\"><head><meta charset='utf-8'>");
      redirectPage += F("<script>");
      redirectPage += F("setTimeout(function () {");
      redirectPage += F("window.location.href= '/';");
      redirectPage += F("}, 1000);");
      redirectPage += F("</script>");
      redirectPage += F("</body></html>");
      server.send(httpFound, F("text/html"), redirectPage);
      return;
    }
  }
  loginPage.replace(F("_LOGIN_SUCCESS_"), (String)loginSuccess);
  loginPage.replace(F("_LOGIN_MSG_"), msg);
  sendWrappedHTML(loginPage);
}

void handleUpgrade() {
  if (!checkLogin()) {
    return;
  }

  uploaderror = UploadError::noError;
  String upgradePage = FPSTR(html_page_upgrade);
  upgradePage.replace("_TXT_B_UPGRADE_", FPSTR(txt_upgrade));
  upgradePage.replace("_TXT_BACK_", FPSTR(txt_back));
  upgradePage.replace("_TXT_UPGRADE_TITLE_", FPSTR(txt_upgrade_title));
  upgradePage.replace("_TXT_UPGRADE_INFO_", FPSTR(txt_upgrade_info));
  upgradePage.replace("_TXT_UPGRADE_START_", FPSTR(txt_upgrade_start));

  sendWrappedHTML(upgradePage);
}

void handleUploadDone() {
  // Serial.printl(PSTR("HTTP: Firmware upload done"));
  bool restartflag = false;
  String uploadDonePage = FPSTR(html_page_upload);
  String content = F("<div style='text-align:center;'><b>Upload ");
  if (uploaderror != UploadError::noError) {
    content += F("<span style='color:#d43535'>failed</span></b><br/><br/>");
    if (uploaderror == UploadError::noFileSelected) {
      content += FPSTR(txt_upload_nofile);
    } else if (uploaderror == UploadError::fileTooLarge) {
      content += FPSTR(txt_upload_filetoolarge);
    } else if (uploaderror == UploadError::fileMagicHeaderIncorrect) {
      content += FPSTR(txt_upload_fileheader);
    } else if (uploaderror == UploadError::fileTooBigForDeviceFlash) {
      content += FPSTR(txt_upload_flashsize);
    } else if (uploaderror == UploadError::fileUploadBufferMiscompare) {
      content += FPSTR(txt_upload_buffer);
    } else if (uploaderror == UploadError::fileUploadFailed) {
      content += FPSTR(txt_upload_failed);
    } else if (uploaderror == UploadError::fileUploadAborted) {
      content += FPSTR(txt_upload_aborted);
    } else {
      content += FPSTR(txt_upload_error);
      content += String(uploaderror);
    }
    if (Update.hasError()) {
      content += FPSTR(txt_upload_code);
      content += String(Update.getError());
    }
  } else {
    content += F("<span style='color:#47c266; font-weight: bold;'>");
    content += FPSTR(txt_upload_sucess);
    content += F("</span><br/><br/>");
    content += FPSTR(txt_upload_refresh);
    content += F("<span id='count'>10s</span>...");
    content += FPSTR(count_down_script);
    restartflag = true;
  }
  content += F("</div><br/>");
  uploadDonePage.replace("_UPLOAD_MSG_", content);
  uploadDonePage.replace("_TXT_BACK_", FPSTR(txt_back));
  sendWrappedHTML(uploadDonePage);
  if (restartflag) {
    delay(500);
#ifdef ESP32
    ESP.restart();
#else
    ESP.reset();
#endif
  }
}

void handleUploadLoop() {  // NOLINT(readability-function-cognitive-complexity)
  if (!checkLogin()) {
    return;
  }

  // Based on ESP8266HTTPUpdateServer.cpp uses ESP8266WebServer Parsing.cpp and
  // Cores Updater.cpp (Update) char log[200];
  if (uploaderror != UploadError::noError) {
    Update.end();
    return;
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (upload.filename.c_str()[0] == 0) {
      uploaderror = UploadError::noFileSelected;
      return;
    }
    // save cpu by disconnect/stop retry mqtt server
    if (mqtt_client.state() == MQTT_CONNECTED) {
      mqtt_client.disconnect();
      lastMqttRetry = millis();
    }
    // snprintf_P(log, sizeof(log), PSTR("Upload: File %s ..."),
    // upload.filename.c_str()); Serial.printl(log);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {  // start with max available size
      // Update.printError(Serial);
      uploaderror = UploadError::fileTooLarge;
      return;
    }
  } else if (!uploaderror && (upload.status == UPLOAD_FILE_WRITE)) {
    if (upload.totalSize == 0) {
      if (upload.buf[0] != 0xE9) {
        // Serial.println(PSTR("Upload: File magic header does not start with
        // 0xE9"));
        uploaderror = UploadError::fileMagicHeaderIncorrect;
        return;
      }
      uint32_t bin_flash_size = ESP.magicFlashChipSize((upload.buf[3] & 0xf0) >> 4);
#ifdef ESP32
      if (bin_flash_size > ESP.getFlashChipSize()) {
#else
      if (bin_flash_size > ESP.getFlashChipRealSize()) {
#endif
        // Serial.printl(PSTR("Upload: File flash size is larger than device
        // flash size"));
        uploaderror = UploadError::fileTooBigForDeviceFlash;
        return;
      }
      if (ESP.getFlashChipMode() == 3) {
        upload.buf[2] = 3;  // DOUT - ESP8285
      } else {
        upload.buf[2] = 2;  // DIO - ESP8266
      }
    }
    if (!uploaderror && (Update.write(upload.buf, upload.currentSize) != upload.currentSize)) {
      // Update.printError(Serial);
      uploaderror = UploadError::fileUploadBufferMiscompare;
      return;
    }
  } else if (!uploaderror && (upload.status == UPLOAD_FILE_END)) {
    if (Update.end(true)) {  // true to set the size to the current progress
                             // snprintf_P(log, sizeof(log), PSTR("Upload:
                             // Successful %u bytes. Restarting"),
                             // upload.totalSize); Serial.printl(log)
    } else {
      // Update.printError(Serial);
      uploaderror = UploadError::fileUploadFailed;
      return;
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    // Serial.println(PSTR("Upload: Update was aborted"));
    uploaderror = UploadError::fileUploadAborted;
    Update.end();
  }
  delay(0);
}

// cppcheck-suppress unusedFunction
void write_log(String log) {
  File logFile = SPIFFS.open(console_file, "a");
  logFile.println(log);
  logFile.close();
}

HeatpumpSettings change_states(const HeatpumpSettings &settings) {
  HeatpumpSettings newSettings = settings;
  if (server.hasArg("CONNECT")) {
    hp.connect(&Serial);
  } else {
    bool update = false;
    if (server.hasArg("POWER")) {
      newSettings.power = server.arg("POWER");
      update = true;
    }
    if (server.hasArg("MODE")) {
      newSettings.mode = server.arg("MODE");
      update = true;
    }
    if (server.hasArg("TEMP")) {
      newSettings.temperature =
          convertLocalUnitToCelsius(server.arg("TEMP").toInt(), useFahrenheit);
      update = true;
    }
    if (server.hasArg("FAN")) {
      newSettings.fan = server.arg("FAN").c_str();
      update = true;
    }
    if (server.hasArg("VANE")) {
      newSettings.vane = server.arg("VANE").c_str();
      update = true;
    }
    if (server.hasArg("WIDEVANE")) {
      newSettings.wideVane = server.arg("WIDEVANE").c_str();
      update = true;
    }
    if (update) {
      hp.setSettings(newSettings.getRaw());
    }
  }
  return newSettings;
}

void readHeatPumpSettings() {
  const HeatpumpSettings currentSettings(hp.getSettings());

  rootInfo.clear();
  rootInfo["temperature"] = convertCelsiusToLocalUnit(currentSettings.temperature, useFahrenheit);
  rootInfo["fan"] = currentSettings.fan;
  rootInfo["vane"] = currentSettings.vane;
  rootInfo["wideVane"] = currentSettings.wideVane;
  rootInfo["mode"] = hpGetMode(currentSettings);
}

void hpSettingsChanged() {
  // send room temp, operating info and all information
  readHeatPumpSettings();

  String mqttOutput;
  serializeJson(rootInfo, mqttOutput);

  if (!mqtt_client.publish(ha_settings_topic.c_str(), mqttOutput.c_str(), true)) {
    if (g_debugModeLogs) {
      mqtt_client.publish(ha_debug_logs_topic.c_str(), "Failed to publish hp settings");
    }
  }

  hpStatusChanged(hp.getStatus());
}

String hpGetMode(const HeatpumpSettings &hpSettings) {
  // Map the heat pump state to one of HA's HVAC_MODE_* values.
  // https://github.com/home-assistant/core/blob/master/homeassistant/components/climate/const.py#L3-L23

  String hppower = hpSettings.power;
  if (hppower.equalsIgnoreCase("off")) {
    return "off";
  }

  String hpmode = hpSettings.mode;
  hpmode.toLowerCase();

  if (hpmode == "fan") {
    return "fan_only";
  }
  if (hpmode == "auto") {
    return "heat_cool";
  }
  return hpmode;  // cool, heat, dry
}

String hpGetAction(heatpumpStatus hpStatus, const HeatpumpSettings &hpSettings) {
  // Map heat pump state to one of HA's CURRENT_HVAC_* values.
  // https://github.com/home-assistant/core/blob/master/homeassistant/components/climate/const.py#L80-L86

  String hppower = hpSettings.power;
  if (hppower.equalsIgnoreCase("off")) {
    return "off";
  }

  String hpmode = hpSettings.mode;
  hpmode.toLowerCase();

  if (hpmode == "fan") {
    return "fan";
  }
  if (!hpStatus.operating) {
    return "idle";
  }
  if (hpmode == "auto") {
    return "idle";
  }
  if (hpmode == "cool") {
    return "cooling";
  }
  if (hpmode == "heat") {
    return "heating";
  }
  if (hpmode == "dry") {
    return "drying";
  }
  return hpmode;  // unknown
}

void hpStatusChanged(heatpumpStatus newStatus) {
  if (millis() - lastTempSend > SEND_ROOM_TEMP_INTERVAL_MS) {  // only send the temperature every
                                                               // SEND_ROOM_TEMP_INTERVAL_MS
                                                               // (millis rollover tolerant)
    hpCheckRemoteTemp();  // if the remote temperature feed from mqtt is stale,
                          // disable it and revert to the internal thermometer.

    // send room temp, operating info and all information
    HeatpumpSettings const currentSettings(hp.getSettings());

    if (newStatus.roomTemperature == 0) {
      return;
    }

    rootInfo.clear();
    rootInfo["roomTemperature"] =
        convertCelsiusToLocalUnit(newStatus.roomTemperature, useFahrenheit);
    rootInfo["temperature"] = convertCelsiusToLocalUnit(currentSettings.temperature, useFahrenheit);
    rootInfo["fan"] = currentSettings.fan;
    rootInfo["vane"] = currentSettings.vane;
    rootInfo["wideVane"] = currentSettings.wideVane;
    rootInfo["mode"] = hpGetMode(currentSettings);
    rootInfo["action"] = hpGetAction(newStatus, currentSettings);
    rootInfo["compressorFrequency"] = newStatus.compressorFrequency;
    String mqttOutput;
    serializeJson(rootInfo, mqttOutput);

    if (!mqtt_client.publish_P(ha_state_topic.c_str(), mqttOutput.c_str(), false)) {
      if (g_debugModeLogs) {
        mqtt_client.publish(ha_debug_logs_topic.c_str(), "Failed to publish hp status change");
      }
    }

    lastTempSend = millis();
  }
}

void hpCheckRemoteTemp() {
  if (remoteTempActive && (millis() - lastRemoteTemp >
                           CHECK_REMOTE_TEMP_INTERVAL_MS)) {  // if it's been 5 minutes since last
                                                              // remote_temp message, revert back
                                                              // to HP internal temp sensor
    remoteTempActive = false;
    float const temperature = 0;
    hp.setRemoteTemperature(temperature);
    hp.update();
  }
}

void hpPacketDebug(const byte *packet, unsigned int length, const char *packetDirection) {
  if (g_debugModePckts) {
    String message;
    for (unsigned int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0";  // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    StaticJsonDocument<JSON_OBJECT_SIZE(10)> root;

    root[packetDirection] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    if (!mqtt_client.publish(ha_debug_pckts_topic.c_str(), mqttOutput.c_str())) {
      mqtt_client.publish(ha_debug_logs_topic.c_str(), "Failed to publish to heatpump/debug topic");
    }
  }
}

// Used to send a dummy packet in state topic to validate action in HA interface
// HA change GUI appareance before having a valid state from the unit
void hpSendLocalState() {
  String mqttOutput;
  serializeJson(rootInfo, mqttOutput);
  mqtt_client.publish(ha_debug_pckts_topic.c_str(), mqttOutput.c_str(), false);
  if (!mqtt_client.publish_P(ha_state_topic.c_str(), mqttOutput.c_str(), false)) {
    if (g_debugModeLogs) {
      mqtt_client.publish(ha_debug_logs_topic.c_str(), "Failed to publish dummy hp status change");
    }
  }

  // Restart counter for waiting enought time for the unit to update before
  // sending a state packet
  lastTempSend = millis();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void mqttCallback(const char *topic, const byte *payload, unsigned int length) {
  // Copy payload into message buffer
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = static_cast<char>(payload[i]);
  }
  message[length] = '\0';

  // HA topics
  // Receive power topic
  if (strcmp(topic, ha_mode_set_topic.c_str()) == 0) {
    String modeUpper = message;
    modeUpper.toUpperCase();
    if (modeUpper == "OFF") {
      rootInfo["mode"] = "off";
      rootInfo["action"] = "off";
      hpSendLocalState();
      hp.setPowerSetting("OFF");
    } else {
      if (modeUpper == "HEAT_COOL") {
        rootInfo["mode"] = "heat_cool";
        rootInfo["action"] = "idle";
        modeUpper = "AUTO";
      } else if (modeUpper == "HEAT") {
        rootInfo["mode"] = "heat";
        rootInfo["action"] = "heating";
      } else if (modeUpper == "COOL") {
        rootInfo["mode"] = "cool";
        rootInfo["action"] = "cooling";
      } else if (modeUpper == "DRY") {
        rootInfo["mode"] = "dry";
        rootInfo["action"] = "drying";
      } else if (modeUpper == "FAN_ONLY") {
        rootInfo["mode"] = "fan_only";
        rootInfo["action"] = "fan";
        modeUpper = "FAN";
      } else {
        return;
      }
      hpSendLocalState();
      hp.setPowerSetting("ON");
      hp.setModeSetting(modeUpper.c_str());
    }
  } else if (strcmp(topic, ha_temp_set_topic.c_str()) == 0) {
    float temperature = strtof(message, NULL);
    float temperature_c = convertLocalUnitToCelsius(temperature, useFahrenheit);
    if (temperature_c < (float)min_temp || temperature_c > (float)max_temp) {
      temperature_c = ((float)min_temp + (float)max_temp) / 2.0F;
      rootInfo["temperature"] = convertCelsiusToLocalUnit(temperature_c, useFahrenheit);
    } else {
      rootInfo["temperature"] = temperature;
    }
    hpSendLocalState();
    hp.setTemperature(temperature_c);
  } else if (strcmp(topic, ha_fan_set_topic.c_str()) == 0) {
    rootInfo["fan"] = (String)message;
    hpSendLocalState();
    hp.setFanSpeed(message);
  } else if (strcmp(topic, ha_vane_set_topic.c_str()) == 0) {
    rootInfo["vane"] = (String)message;
    hpSendLocalState();
    hp.setVaneSetting(message);
  } else if (strcmp(topic, ha_wideVane_set_topic.c_str()) == 0) {
    rootInfo["wideVane"] = (String)message;
    hpSendLocalState();
    hp.setWideVaneSetting(message);
  } else if (strcmp(topic, ha_remote_temp_set_topic.c_str()) == 0) {
    float temperature = strtof(message, NULL);
    if (temperature == 0) {      // Remote temp disabled by mqtt topic set
      remoteTempActive = false;  // clear the remote temp flag
      hp.setRemoteTemperature(0.0);
    } else {
      remoteTempActive = true;    // Remote temp has been pushed.
      lastRemoteTemp = millis();  // Note time
      hp.setRemoteTemperature(convertLocalUnitToCelsius(temperature, useFahrenheit));
    }
  } else if (strcmp(topic, ha_system_set_topic.c_str()) == 0) {  // We receive command for board
    if (strcmp(message, "reboot") == 0) {                        // We receive reboot command
      ESP.restart();
    }
  } else if (strcmp(topic, ha_debug_pckts_set_topic.c_str()) ==
             0) {  // if the incoming message is on the heatpump_debug_set_topic
                   // topic...
    if (strcmp(message, "on") == 0) {
      g_debugModePckts = true;
      mqtt_client.publish(ha_debug_pckts_topic.c_str(), "Debug packets mode enabled");
    } else if (strcmp(message, "off") == 0) {
      g_debugModePckts = false;
      mqtt_client.publish(ha_debug_pckts_topic.c_str(), "Debug packets mode disabled");
    }
  } else if (strcmp(topic, ha_debug_logs_set_topic.c_str()) ==
             0) {  // if the incoming message is on the heatpump_debug_set_topic
                   // topic...
    if (strcmp(message, "on") == 0) {
      g_debugModeLogs = true;
      mqtt_client.publish(ha_debug_logs_topic.c_str(), "Debug logs mode enabled");
    } else if (strcmp(message, "off") == 0) {
      g_debugModeLogs = false;
      mqtt_client.publish(ha_debug_logs_topic.c_str(), "Debug logs mode disabled");
    }
  } else if (strcmp(topic, ha_custom_packet.c_str()) == 0) {  // send custom packet for advance user
    String custom = message;

    // copy custom packet to char array
    char buffer[(custom.length() + 1)];  // +1 for the NULL at the end
    custom.toCharArray(buffer, (custom.length() + 1));

    byte bytes[maxCustomPacketLength];
    int byteCount = 0;

    // loop over the byte string, breaking it up by spaces (or at the end of the
    // line - \n)
    char *nextByte = strtok(buffer, " ");
    while (nextByte != NULL && byteCount < 20) {
      bytes[byteCount] = strtol(nextByte, NULL, 16);  // convert from hex string
      nextByte = strtok(NULL, "   ");
      byteCount++;
    }

    // dump the packet so we can see what it is. handy because you can run the
    // code without connecting the ESP to the heatpump, and test sending custom
    // packets
    hpPacketDebug(bytes, byteCount, "customPacket");

    hp.sendCustomPacket(bytes, byteCount);
  } else {
    String msg = String("heatpump: wrong mqtt topic: ") + topic;
    mqtt_client.publish(ha_debug_logs_topic.c_str(), msg.c_str());
  }
}

void haConfig() {
  // send HA config packet
  // setup HA payload device
  const size_t capacity = JSON_ARRAY_SIZE(5) + 2 * JSON_ARRAY_SIZE(6) + JSON_ARRAY_SIZE(7) +
                          JSON_OBJECT_SIZE(24) + 2048;
  DynamicJsonDocument haConfig(capacity);

  haConfig["name"] = nullptr;
  haConfig["unique_id"] = getId();

  JsonArray haConfigModes = haConfig.createNestedArray("modes");
  haConfigModes.add("heat_cool");  // native AUTO mode
  haConfigModes.add("cool");
  haConfigModes.add("dry");
  if (supportHeatMode) {
    haConfigModes.add("heat");
  }
  haConfigModes.add("fan_only");  // native FAN mode
  haConfigModes.add("off");

  haConfig["mode_cmd_t"] = ha_mode_set_topic;
  haConfig["mode_stat_t"] = ha_state_topic;
  haConfig["mode_stat_tpl"] =
      F("{{ value_json.mode if (value_json is defined and value_json.mode is "
        "defined and value_json.mode|length) "
        "else 'off' }}");  // Set default value for fix "Could not parse data
                           // for HA"
  haConfig["temp_cmd_t"] = ha_temp_set_topic;
  haConfig["temp_stat_t"] = ha_state_topic;
  haConfig["avty_t"] = ha_availability_topic;           // MQTT last will (status) messages topic
  haConfig["pl_not_avail"] = mqtt_payload_unavailable;  // MQTT offline message payload
  haConfig["pl_avail"] = mqtt_payload_available;        // MQTT online message payload
  // Set default value for fix "Could not parse data for HA"
  String temp_stat_tpl_str =
      F("{% if (value_json is defined and value_json.temperature is defined) "
        "%}{% if (value_json.temperature|int >= ");
  temp_stat_tpl_str += (String)convertCelsiusToLocalUnit(min_temp, useFahrenheit) +
                       " and value_json.temperature|int <= ";
  temp_stat_tpl_str += (String)convertCelsiusToLocalUnit(max_temp, useFahrenheit) +
                       ") %}{{ value_json.temperature }}";
  temp_stat_tpl_str += "{% elif (value_json.temperature|int < " +
                       (String)convertCelsiusToLocalUnit(min_temp, useFahrenheit) + ") %}" +
                       (String)convertCelsiusToLocalUnit(min_temp, useFahrenheit) +
                       "{% elif (value_json.temperature|int > " +
                       (String)convertCelsiusToLocalUnit(max_temp, useFahrenheit) + ") %}" +
                       (String)convertCelsiusToLocalUnit(max_temp, useFahrenheit) +
                       "{% endif %}{% else %}" +
                       (String)convertCelsiusToLocalUnit(22, useFahrenheit) + "{% endif %}";
  haConfig["temp_stat_tpl"] = temp_stat_tpl_str;
  haConfig["curr_temp_t"] = ha_state_topic;
  String curr_temp_tpl_str =
      F("{{ value_json.roomTemperature if (value_json is defined and "
        "value_json.roomTemperature is defined and "
        "value_json.roomTemperature|int > ");
  curr_temp_tpl_str += (String)convertCelsiusToLocalUnit(1, useFahrenheit) +
                       ") }}";  // Set default value for fix "Could not parse data for HA"
  haConfig["curr_temp_tpl"] = curr_temp_tpl_str;
  haConfig["min_temp"] = convertCelsiusToLocalUnit(min_temp, useFahrenheit);
  haConfig["max_temp"] = convertCelsiusToLocalUnit(max_temp, useFahrenheit);
  haConfig["temp_step"] = temp_step;
  haConfig["temperature_unit"] = useFahrenheit ? "F" : "C";

  JsonArray haConfigFan_modes = haConfig.createNestedArray("fan_modes");
  haConfigFan_modes.add("AUTO");
  haConfigFan_modes.add("QUIET");
  haConfigFan_modes.add("1");
  haConfigFan_modes.add("2");
  haConfigFan_modes.add("3");
  haConfigFan_modes.add("4");

  haConfig["fan_mode_cmd_t"] = ha_fan_set_topic;
  haConfig["fan_mode_stat_t"] = ha_state_topic;
  haConfig["fan_mode_stat_tpl"] =
      F("{{ value_json.fan if (value_json is defined and value_json.fan is "
        "defined and value_json.fan|length) else "
        "'AUTO' }}");  // Set default value for fix "Could not parse data for HA"

  JsonArray haConfigSwing_modes = haConfig.createNestedArray("swing_modes");
  haConfigSwing_modes.add("AUTO");
  haConfigSwing_modes.add("1");
  haConfigSwing_modes.add("2");
  haConfigSwing_modes.add("3");
  haConfigSwing_modes.add("4");
  haConfigSwing_modes.add("5");
  haConfigSwing_modes.add("SWING");

  haConfig["swing_mode_cmd_t"] = ha_vane_set_topic;
  haConfig["swing_mode_stat_t"] = ha_state_topic;
  haConfig["swing_mode_stat_tpl"] =
      F("{{ value_json.vane if (value_json is defined and value_json.vane is "
        "defined and value_json.vane|length) "
        "else 'AUTO' }}");  // Set default value for fix "Could not parse data
                            // for HA"
  haConfig["action_topic"] = ha_state_topic;
  haConfig["action_template"] =
      F("{{ value_json.action if (value_json is defined and value_json.action "
        "is defined and "
        "value_json.action|length) else 'idle' }}");  // Set default value for
                                                      // fix "Could not parse
                                                      // data for HA"

  JsonObject haConfigDevice = haConfig.createNestedObject("device");

  haConfigDevice["ids"] = mqtt_fn;   // NOLINT(readability-misplaced-array-index)
  haConfigDevice["name"] = mqtt_fn;  // NOLINT(readability-misplaced-array-index)
  haConfigDevice["sw"] =             // NOLINT(readability-misplaced-array-index)
      "Mitsubishi2MQTT " + String(BUILD_DATE) + " (" + String(COMMIT_HASH) + ")";
  haConfigDevice["mdl"] = "HVAC MITSUBISHI";     // NOLINT(readability-misplaced-array-index)
  haConfigDevice["mf"] = "MITSUBISHI ELECTRIC";  // NOLINT(readability-misplaced-array-index)
  haConfigDevice["configuration_url"] =
      "http://" + WiFi.localIP().toString();  // NOLINT(readability-misplaced-array-index)

  // Additional attributes are in the state
  // For now, only compressorFrequency
  haConfig["json_attr_t"] = ha_state_topic;
  haConfig["json_attr_tpl"] =
      F("{{ {'compressorFrequency': value_json.compressorFrequency if "
        "(value_json is defined "
        "and value_json.compressorFrequency is defined) else '-1' } | tojson }}");

  String mqttOutput;
  serializeJson(haConfig, mqttOutput);
  mqtt_client.beginPublish(ha_config_topic.c_str(), mqttOutput.length(), true);
  mqtt_client.print(mqttOutput);
  mqtt_client.endPublish();
}

void mqttConnect() {
  // Loop until we're reconnected
  int attempts = 0;
  const int maxAttempts = 5;
  while (!mqtt_client.connected()) {
    // Attempt to connect
    mqtt_client.connect(mqtt_client_id.c_str(), mqtt_username.c_str(), mqtt_password.c_str(),
                        ha_availability_topic.c_str(), 1, true, mqtt_payload_unavailable);
    // If state < 0 (MQTT_CONNECTED) => network problem we retry 5 times and
    // then waiting for MQTT_RETRY_INTERVAL_MS and retry reapeatly
    if (mqtt_client.state() < MQTT_CONNECTED) {
      if (attempts == maxAttempts) {
        lastMqttRetry = millis();
        return;
      }
      delay(10);
      attempts++;
    }
    // If state > 0 (MQTT_CONNECTED) => config or server problem we stop retry
    else if (mqtt_client.state() > MQTT_CONNECTED) {
      return;
    }
    // We are connected
    else {
      mqtt_client.subscribe(ha_system_set_topic.c_str());
      mqtt_client.subscribe(ha_debug_pckts_set_topic.c_str());
      mqtt_client.subscribe(ha_debug_logs_set_topic.c_str());
      mqtt_client.subscribe(ha_mode_set_topic.c_str());
      mqtt_client.subscribe(ha_fan_set_topic.c_str());
      mqtt_client.subscribe(ha_temp_set_topic.c_str());
      mqtt_client.subscribe(ha_vane_set_topic.c_str());
      mqtt_client.subscribe(ha_wideVane_set_topic.c_str());
      mqtt_client.subscribe(ha_remote_temp_set_topic.c_str());
      mqtt_client.subscribe(ha_custom_packet.c_str());
      mqtt_client.publish(ha_availability_topic.c_str(), mqtt_payload_available,
                          true);  // publish status as available
      if (others_haa) {
        haConfig();
      }
    }
  }
}

bool connectWifi() {
  const int connectTimeoutMs = 30000;
#ifdef ESP32
  WiFi.setHostname(hostname.c_str());
#else
  WiFi.hostname(hostname.c_str());
#endif
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }
#ifdef ESP32
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
#endif
  WiFi.begin(ap_ssid.c_str(), ap_pwd.c_str());
  // Serial.println("Connecting to " + ap_ssid);
  wifi_timeout = millis() + connectTimeoutMs;
  while (WiFi.status() != WL_CONNECTED && millis() < wifi_timeout) {
    Serial.write('.');
    // Serial.print(WiFi.status());
    //  wait 500ms, flashing the blue LED to indicate WiFi connecting...
    digitalWrite(blueLedPin, LOW);
    delay(250);
    digitalWrite(blueLedPin, HIGH);
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    // Serial.println(F("Failed to connect to wifi"));
    return false;
  }
  // Serial.println(F("Connected to "));
  // Serial.println(ap_ssid);
  // Serial.println(F("Ready"));
  // Serial.print("IP address: ");
  while (WiFi.localIP().toString() == "0.0.0.0" || WiFi.localIP().toString() == "") {
    // Serial.write('.');
    delay(500);
  }

  // This conditional can't be true if we fell out of the loop
  // if (WiFi.localIP().toString() == "0.0.0.0" || WiFi.localIP().toString() == "") {
  //   // Serial.println(F("Failed to get IP address"));
  //   return false;
  // }

  // Serial.println(WiFi.localIP());
  // ticker.detach(); // Stop blinking the LED because now we are connected:)
  // keep LED off (For Wemos D1-Mini)
  digitalWrite(blueLedPin, HIGH);
  return true;
}

// temperature helper functions
float toFahrenheit(float fromCelsius) { return round(1.8F * fromCelsius + 32.0F); }

float toCelsius(float fromFahrenheit) { return (fromFahrenheit - 32.0F) / 1.8F; }

float convertCelsiusToLocalUnit(float temperature, bool isFahrenheit) {
  if (isFahrenheit) {
    return toFahrenheit(temperature);
  }
  return temperature;
}

float convertLocalUnitToCelsius(float temperature, bool isFahrenheit) {
  if (isFahrenheit) {
    return toCelsius(temperature);
  }
  return temperature;
}

String getTemperatureScale() {
  if (useFahrenheit) {
    return "F";
  }
  return "C";
}

String getId() {
#ifdef ESP32
  uint64_t macAddress = ESP.getEfuseMac();
  const uint32_t chipID = macAddress >> 24;
#else
  uint32_t chipID = ESP.getChipId();
#endif
  return String(chipID, HEX);
}

// Check if header is present and correct
bool is_authenticated() {
  if (server.hasHeader("Cookie")) {
    // Found cookie;
    String cookie = server.header("Cookie");
    if (cookie.indexOf("M2MSESSIONID=1") != -1) {
      // Authentication Successful
      return true;
    }
  }
  // Authentication Failed
  return false;
}

bool checkLogin() {
  if (!is_authenticated() and login_password.length() > 0) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    // use javascript in the case browser disable redirect
    String redirectPage = F("<html lang=\"en\" class=\"\"><head><meta charset='utf-8'>");
    redirectPage += F("<script>");
    redirectPage += F("setTimeout(function () {");
    redirectPage += F("window.location.href= '/login';");
    redirectPage += F("}, 1000);");
    redirectPage += F("</script>");
    redirectPage += F("</body></html>");
    server.send(httpFound, F("text/html"), redirectPage);
    return false;
  }
  return true;
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  // reset board to attempt to connect to wifi again if in ap mode or wifi
  // dropped out and time limit passed
  if (WiFi.getMode() == WIFI_STA and
      WiFi.status() == WL_CONNECTED) {  // NOLINT(bugprone-branch-clone)
    wifi_timeout = millis() + WIFI_RETRY_INTERVAL_MS;
  } else if (wifi_config_exists and millis() > wifi_timeout) {
    ESP.restart();
  }

  if (!captive) {
    // Sync HVAC UNIT
    if (!hp.isConnected()) {
      // Use exponential backoff for retries, where each retry is double the
      // length of the previous one.
      unsigned long durationNextSync = (1 << hpConnectionRetries) * HP_RETRY_INTERVAL_MS;
      if (((millis() - lastHpSync > durationNextSync) or lastHpSync == 0)) {
        lastHpSync = millis();
        // If we've retried more than the max number of tries, keep retrying at
        // that fixed interval, which is several minutes.
        hpConnectionRetries = min(hpConnectionRetries + 1u, HP_MAX_RETRIES);
        hpConnectionTotalRetries++;
        hp.sync();
      }
    } else {
      hpConnectionRetries = 0;
      hp.sync();
    }

    if (mqtt_config) {
      // MQTT failed retry to connect
      if (mqtt_client.state() < MQTT_CONNECTED) {
        if ((millis() - lastMqttRetry > MQTT_RETRY_INTERVAL_MS) or lastMqttRetry == 0) {
          mqttConnect();
        }
      }
      // MQTT config problem on MQTT do nothing
      else if (mqtt_client.state() > MQTT_CONNECTED) {
        return;
      }
      // MQTT connected send status
      else {
        hpStatusChanged(hp.getStatus());
        mqtt_client.loop();
      }
    }
  } else {
    dnsServer.processNextRequest();
  }
}

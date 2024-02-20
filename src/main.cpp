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
#include <ArduinoJson.h>
#include <ArduinoOTA.h>    // for Update
#include <DNSServer.h>     // DNS for captive portal
#include <HeatPump.h>      // SwiCago library: https://github.com/SwiCago/HeatPump
#include <PubSubClient.h>  // MQTT: PubSubClient 2.8.0

#include <map>
#include <temperature.hpp>

#include "HeatpumpSettings.hpp"
#include "HeatpumpStatus.hpp"
#include "filesystem.hpp"
#include "html_common.hpp"        // common code HTML (like header, footer)
#include "html_init.hpp"          // code html for initial config
#include "html_menu.hpp"          // code html for menu
#include "html_pages.hpp"         // code html for pages
#include "javascript_common.hpp"  // common code javascript (like refresh page)
#include "logger.hpp"
#include "main.hpp"
#include "ota.hpp"
#include "timer.hpp"

// BEGIN include the contents of config.h
#ifndef LANG_PATH
#define LANG_PATH "languages/en-GB.h"  // default language English
#endif

#include "frontend/templates.hpp"
#include "ministache.hpp"
#include "views/metrics.hpp"
#include "views/mqtt.hpp"

using ministache::Ministache;

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

typedef Temperature::Unit TempUnit;

struct Config {
  // Note: I'm being pretty blase about alignment and padding here, since there's only
  // one instance of this struct.

  // WiFi
  struct Network {
    String hostname{defaultHostname()};
    String accessPointSsid;
    String accessPointPassword;
    String otaUpdatePassword;

    bool configured() const {
      return accessPointSsid.length() > 0;
    }

    static String defaultHostname() {
      return String(F("HVAC_")) +
             getId();  // default hostname, will be used if no hostname is set in config
    }

  } network;

  // Others
  struct Other {  // TODO(floatplane): "other" is a terrible name
    bool haAutodiscovery;
    String haAutodiscoveryTopic;
    // debug mode logs, when true, will send all debug messages to topic
    // heatpump_debug_logs_topic this can also be set by sending "on" to
    // heatpump_debug_set_topic
    bool logToMqtt;
    // debug mode packets, when true, will send all packets received from the
    // heatpump to topic heatpump_debug_packets_topic this can also be set by
    // sending "on" to heatpump_debug_set_topic
    bool dumpPacketsToMqtt;
    // Safe mode: when true, will turn the heat pump off if remote temperature messages stop.
    // This is useful if you want to prevent the heat pump from running away if the MQTT server goes
    // down.
    bool safeMode;
    // Optimistic updates: when true, will push state updates to MQTT before the heat pump has
    // confirmed the change. This is useful if you want to make the UI feel more responsive, but
    // could lead to the UI showing the wrong state if the heat pump fails to change state.
    bool optimisticUpdates;
    Other()
        : haAutodiscovery(true),
          haAutodiscoveryTopic(F("homeassistant")),
          logToMqtt(false),
          dumpPacketsToMqtt(false),
          safeMode(false),
          optimisticUpdates(true) {
    }
  } other;

  // Unit
  struct Unit {
    TempUnit tempUnit = TempUnit::C;
    // support heat mode settings, some model do not support heat mode
    bool supportHeatMode = true;
    Temperature minTemp = Temperature(16.0f,
                                      TempUnit::C);  // Minimum temperature, check
                                                     // value from heatpump remote control
    Temperature maxTemp = Temperature(31.0f,
                                      TempUnit::C);  // Maximum temperature, check
                                                     // value from heatpump remote control

    // TODO(floatplane) why isn't this a float?
    // TODO(floatplane) more importantly...why isn't this even used? unreal 🙄
    String tempStep{"1"};  // Temperature setting step, check
                           // value from heatpump remote control

    String login_password;
  } unit;

  // MQTT
  struct MQTT {
    String friendlyName;
    String server;
    uint32_t port{1883};
    String username;
    String password;
    String rootTopic;
    MQTT() : rootTopic(F("mitsubishi2mqtt")) {
    }

    bool configured() const {
      return friendlyName.length() > 0 && server.length() > 0 && username.length() > 0 &&
             password.length() > 0 && rootTopic.length() > 0;
    }
    const String &ha_availability_topic() const {
      static const String topicPath{rootTopic + "/" + friendlyName + F("/availability")};
      return topicPath;
    }
    const String &ha_custom_packet() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/custom/send")};
      return topicPath;
    }
    const String &ha_debug_logs_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/debug/logs/set")};
      return topicPath;
    }
    const String &ha_debug_logs_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/debug/logs")};
      return topicPath;
    }
    const String &ha_debug_pckts_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/debug/packets/set")};
      return topicPath;
    }
    const String &ha_debug_pckts_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/debug/packets")};
      return topicPath;
    }
    const String &ha_fan_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/fan/set")};
      return topicPath;
    }
    const String &ha_mode_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/mode/set")};
      return topicPath;
    }
    const String &ha_remote_temp_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/remote_temp/set")};
      return topicPath;
    }
    const String &ha_state_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/state")};
      return topicPath;
    }
    const String &ha_system_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/system/set")};
      return topicPath;
    }
    const String &ha_temp_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/temp/set")};
      return topicPath;
    }
    const String &ha_vane_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/vane/set")};
      return topicPath;
    }
    const String &ha_wideVane_set_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/wideVane/set")};
      return topicPath;
    }
  } mqtt;
} config;

// Define global variables for network
const PROGMEM uint32_t WIFI_RETRY_INTERVAL_MS = 300000;
uint64_t wifi_timeout;

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
const PROGMEM char *const mqtt_payload_available = "online";
const PROGMEM char *const mqtt_payload_unavailable = "offline";
const size_t maxCustomPacketLength = 20;  // max custom packet bytes is 20

// Define global variables for HA topics
String ha_config_topic;

// Customization

// sketch settings
const PROGMEM uint32_t CHECK_REMOTE_TEMP_INTERVAL_MS = 300000;  // 5 minutes
const PROGMEM uint32_t MQTT_RETRY_INTERVAL_MS = 1000;           // 1 second
const PROGMEM uint64_t HP_RETRY_INTERVAL_MS = 1000UL;           // 1 second
const PROGMEM uint32_t HP_MAX_RETRIES =
    10;  // Double the interval between retries up to this many times, then keep
         // retrying forever at that maximum interval.
// Default values give a final retry interval of 1000ms * 2^10, which is 1024
// seconds, about 17 minutes.

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
boolean remoteTempActive = false;

// HVAC
HeatPump hp;  // NOLINT(readability-identifier-length)
uint64_t lastMqttStatePacketSend;
uint64_t lastMqttRetry;
uint64_t lastHpSync;
unsigned int hpConnectionRetries;
unsigned int hpConnectionTotalRetries;
uint64_t lastRemoteTemp;

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

static bool restartPending = false;
void restartAfterDelay(uint32_t delayMs) {
  LOG(F("Restarting after delay of %u ms"), delayMs);
  if (restartPending) {
    return;
  }
  restartPending = true;
  // TODO(floatplane): optionally power down the heat pump to prevent runaways
  getTimer()->in(delayMs, []() {
    ESP.restart();
    return Timers::TimerStatus::completed;
  });
}

void logConfig() {
  for (const auto file :
       std::array<const char *const, 4>{wifi_conf, mqtt_conf, unit_conf, others_conf}) {
    LOG(F("Loading ") + String(file));
    JsonDocument doc = FileSystem::loadJSON(file);
    if (doc.isNull()) {
      LOG(F("File is empty"));
      continue;
    }
    if (doc.containsKey("ap_pwd")) {
      doc["ap_pwd"] = F("********");
    }
    String contents;
    serializeJsonPretty(doc, contents);
    LOG(contents);
  }
}

void setup() {
  // Start serial for debug before HVAC connect to serial
  Serial.begin(115200);

#ifdef ENABLE_LOGGING
  Logger::initialize();
#endif

  // Serial.println(F("Starting Mitsubishi2MQTT"));
  FileSystem::init();

  // set led pin as output
  pinMode(blueLedPin, OUTPUT);
  /*
    ticker.attach(0.6, tick);
  */

  loadWifiConfig();
  loadOthersConfig();
  loadUnitConfig();
  loadMqttConfig();
#ifdef ESP32
  WiFi.setHostname(config.network.hostname.c_str());
#else
  WiFi.hostname(config.network.hostname.c_str());
#endif
  if (initWifi()) {
    FileSystem::deleteFile(console_file);
    LOG(F("Starting Mitsubishi2MQTT"));
    // Web interface
    server.on(F("/"), handleRoot);
    server.on(F("/control"), handleControl);
    server.on(F("/setup"), handleSetup);
    server.on(F("/mqtt"), handleMqtt);
    server.on(F("/wifi"), handleWifi);
    server.on(F("/unit"), HTTPMethod::HTTP_GET, handleUnitGet);
    server.on(F("/unit"), HTTPMethod::HTTP_POST, handleUnitPost);
    server.on(F("/status"), handleStatus);
    server.on(F("/others"), handleOthers);
    server.on(F("/metrics"), handleMetrics);
    server.on(F("/metrics.json"), handleMetricsJson);
    server.on(F("/css"), HTTPMethod::HTTP_GET,
              []() { server.send(200, F("text/css"), statics::css); });
    server.onNotFound(handleNotFound);
    if (config.unit.login_password.length() > 0) {
      server.on(F("/login"), handleLogin);
      // here the list of headers to be recorded, use for authentication
      const char *headerkeys[] = {"User-Agent", "Cookie"};
      const size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
      // ask server to track these headers
      server.collectHeaders(headerkeys, headerkeyssize);
    }
    server.on(F("/upgrade"), handleUpgrade);
    server.on(F("/upload"), HTTP_POST, handleUploadDone, handleUploadLoop);

    server.begin();
    lastMqttRetry = 0;
    lastHpSync = 0;
    hpConnectionRetries = 0;
    hpConnectionTotalRetries = 0;
    if (config.mqtt.configured()) {
      LOG(F("Starting MQTT"));
      if (config.other.haAutodiscovery) {
        ha_config_topic = config.other.haAutodiscoveryTopic + F("/climate/") +
                          config.mqtt.friendlyName + F("/config");
      }
      // startup mqtt connection
      initMqtt();
      if (config.other.logToMqtt) {
        Logger::enableMqttLogging(mqtt_client, config.mqtt.ha_debug_logs_topic().c_str());
      }
    } else {
      LOG(F("Not found MQTT config go to configuration page"));
    }
    // Serial.println(F("Connection to HVAC. Stop serial log."));
    LOG(F("MQTT initialized, trying to connect to HVAC"));
    hp.setPacketCallback(hpPacketDebug);

    // Merge settings from remote control with settings driven from MQTT
    hp.enableExternalUpdate();

    // Automatically send new settings to heat pump when `sync()` is called
    // Without this, you need to call update() after every state change
    hp.enableAutoUpdate();

    hp.connect(&Serial);

    lastMqttStatePacketSend = millis();
  } else {
    dnsServer.start(DNS_PORT, "*", apIP);
    initCaptivePortal();
  }
  LOG(F("calling initOTA"));
  initOTA(config.network.hostname, config.network.otaUpdatePassword);
  LOG(F("Setup complete"));
  logConfig();
}

void loadWifiConfig() {
  LOG(F("Loading WiFi configuration"));
  config.network.accessPointSsid = "";
  config.network.accessPointPassword = "";

  const JsonDocument doc = FileSystem::loadJSON(wifi_conf);
  if (doc.isNull()) {
    return;
  }
  config.network.hostname = doc["hostname"].as<String>();
  config.network.accessPointSsid = doc["ap_ssid"].as<String>();
  config.network.accessPointPassword = doc["ap_pwd"].as<String>();
  // prevent ota password is "null" if not exist key
  if (doc.containsKey("ota_pwd")) {
    config.network.otaUpdatePassword = doc["ota_pwd"].as<String>();
  } else {
    config.network.otaUpdatePassword = "";
  }
}

void loadMqttConfig() {
  LOG(F("Loading MQTT configuration"));

  const JsonDocument doc = FileSystem::loadJSON(mqtt_conf);
  if (doc.isNull()) {
    return;
  }
  config.mqtt.friendlyName = doc["mqtt_fn"].as<String>();
  config.mqtt.server = doc["mqtt_host"].as<String>();
  const String portString = doc["mqtt_port"].as<String>();
  config.mqtt.port = portString.toInt();
  config.mqtt.username = doc["mqtt_user"].as<String>();
  config.mqtt.password = doc["mqtt_pwd"].as<String>();
  config.mqtt.rootTopic = doc["mqtt_topic"].as<String>();
}

void loadUnitConfig() {
  const JsonDocument doc = FileSystem::loadJSON(unit_conf);
  if (doc.isNull()) {
    return;
  }
  // unit
  String unit_tempUnit = doc["unit_tempUnit"].as<String>();
  config.unit.tempUnit = unit_tempUnit == "fah" ? TempUnit::F : TempUnit::C;
  config.unit.minTemp = Temperature(doc["min_temp"].as<String>().toFloat(), TempUnit::C);
  config.unit.maxTemp = Temperature(doc["max_temp"].as<String>().toFloat(), TempUnit::C);
  config.unit.tempStep = doc["temp_step"].as<String>();
  // mode
  config.unit.supportHeatMode = doc["support_mode"].as<String>() == "all";
  // prevent login password is "null" if not exist key
  if (doc.containsKey("login_password")) {
    config.unit.login_password = doc["login_password"].as<String>();
  } else {
    config.unit.login_password = "";
  }
}

void loadOthersConfig() {
  const JsonDocument doc = FileSystem::loadJSON(others_conf);
  if (doc.isNull()) {
    return;
  }
  config.other.haAutodiscoveryTopic = doc["haat"].as<String>();
  config.other.haAutodiscovery = doc["haa"].as<String>() == "ON";
  config.other.dumpPacketsToMqtt = doc["debugPckts"].as<String>() == "ON";
  config.other.logToMqtt = doc["debugLogs"].as<String>() == "ON";
  config.other.safeMode = doc["safeMode"].as<String>() == "ON";
  // make optimisticUpdates default to true if it's not present in the config
  config.other.optimisticUpdates = doc["optimisticUpdates"].as<String>() != "OFF";
}

void saveMqttConfig(const Config &config) {
  JsonDocument doc;
  doc["mqtt_fn"] = config.mqtt.friendlyName;
  doc["mqtt_host"] = config.mqtt.server;
  doc["mqtt_port"] = String(config.mqtt.port);
  doc["mqtt_user"] = config.mqtt.username;
  doc["mqtt_pwd"] = config.mqtt.password;
  doc["mqtt_topic"] = config.mqtt.rootTopic;
  FileSystem::saveJSON(mqtt_conf, doc);
}

void saveUnitConfig(const Config &config) {
  JsonDocument doc;
  doc["unit_tempUnit"] = config.unit.tempUnit == TempUnit::F ? "fah" : "cel";
  doc["min_temp"] = String(config.unit.minTemp.getCelsius());
  doc["max_temp"] = String(config.unit.maxTemp.getCelsius());
  doc["temp_step"] = config.unit.tempStep;
  doc["support_mode"] = config.unit.supportHeatMode ? "all" : "nht";
  doc["login_password"] = config.unit.login_password;
  FileSystem::saveJSON(unit_conf, doc);
}

void saveWifiConfig(const Config &config) {
  JsonDocument doc;
  doc["ap_ssid"] = config.network.accessPointSsid;
  doc["ap_pwd"] = config.network.accessPointPassword;
  doc["hostname"] = config.network.hostname;
  doc["ota_pwd"] = config.network.accessPointPassword;
  FileSystem::saveJSON(wifi_conf, doc);
}

void saveOthersConfig(const Config &config) {
  JsonDocument doc;
  doc["haa"] = config.other.haAutodiscovery ? "ON" : "OFF";
  doc["haat"] = config.other.haAutodiscoveryTopic;
  doc["debugPckts"] = config.other.dumpPacketsToMqtt ? "ON" : "OFF";
  doc["debugLogs"] = config.other.logToMqtt ? "ON" : "OFF";
  doc["safeMode"] = config.other.safeMode ? "ON" : "OFF";
  doc["optimisticUpdates"] = config.other.optimisticUpdates ? "ON" : "OFF";
  FileSystem::saveJSON(others_conf, doc);
}

// Initialize captive portal page
void initCaptivePortal() {
  // Serial.println(F("Starting captive portal"));
  server.on(F("/"), handleInitSetup);
  server.on(F("/save"), handleSaveWifi);
  server.on(F("/reboot"), handleReboot);
  server.onNotFound(handleNotFound);
  server.begin();
  captive = true;
}

void initMqtt() {
  mqtt_client.setServer(config.mqtt.server.c_str(), config.mqtt.port);
  mqtt_client.setCallback(mqttCallback);
  mqttConnect();
}

bool initWifi() {
  if (config.network.configured()) {
    if (connectWifi()) {
      return true;
    }
    // reset hostname back to default before starting AP mode for privacy
    config.network.hostname = Config::Network::defaultHostname();
  }

  // Serial.println(F("\n\r \n\rStarting in AP mode"));
  WiFi.mode(WIFI_AP);
  wifi_timeout = millis() + WIFI_RETRY_INTERVAL_MS;
  WiFi.persistent(false);  // fix crash esp32
                           // https://github.com/espressif/arduino-esp32/issues/2025
  WiFi.softAPConfig(apIP, apIP, netMsk);
  if (config.unit.login_password != "") {  // NOLINT(bugprone-branch-clone)
    // Set AP password when falling back to AP on fail
    WiFi.softAP(config.network.hostname.c_str(), config.unit.login_password.c_str());
  } else {
    // First time setup does not require password
    WiFi.softAP(config.network.hostname.c_str());
  }
  delay(2000);  // VERY IMPORTANT to delay here while the softAP is set up; we shouldn't return from
                // setup() and enter the loop() method until the softAP is ready

  // Serial.print(F("IP address: "));
  // Serial.println(WiFi.softAPIP());
  // ticker.attach(0.2, tick); // Start LED to flash rapidly to indicate we are
  // ready for setting up the wifi-connection (entered captive portal).
  return false;
}

bool remoteTempStale() {
  return (millis() - lastRemoteTemp > CHECK_REMOTE_TEMP_INTERVAL_MS);
}

bool safeModeActive() {
  return config.other.safeMode && remoteTempStale();
}

// Handler webserver response

void sendWrappedHTML(const String &content) {
  const String headerContent = FPSTR(html_common_header);
  const String footerContent = FPSTR(html_common_footer);
  String toSend = headerContent + content + footerContent;
  toSend.replace(F("_UNIT_NAME_"), config.network.hostname);
  toSend.replace(F("_VERSION_"), BUILD_DATE);
  toSend.replace(F("_GIT_HASH_"), COMMIT_HASH);
  server.send(HttpStatusCodes::httpOk, F("text/html"), toSend);
}

void handleNotFound() {
  LOG(F("handleNotFound()"));
  server.send(HttpStatusCodes::httpNotFound, "text/plain", "Not found.");
}

void handleSaveWifi() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleSaveWifi()"));

  // Serial.println(F("Saving wifi config"));
  if (server.method() == HTTP_POST) {
    config.network.accessPointSsid = server.arg("ssid");
    config.network.accessPointPassword = server.arg("psk");
    config.network.hostname = server.arg("hn");
    config.network.otaUpdatePassword = server.arg("otapwd");
    saveWifiConfig(config);
  }
  String initSavePage = FPSTR(html_init_save);
  initSavePage.replace("_TXT_INIT_REBOOT_MESS_", FPSTR(txt_init_reboot_mes));
  sendWrappedHTML(initSavePage);
  restartAfterDelay(500);
}

void handleReboot() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleReboot()"));

  String initRebootPage = FPSTR(html_init_reboot);
  initRebootPage.replace("_TXT_INIT_REBOOT_", FPSTR(txt_init_reboot));
  sendWrappedHTML(initRebootPage);
  restartAfterDelay(500);
}

void handleRoot() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleRoot()"));

  if (server.hasArg("REBOOT")) {
    String rebootPage = FPSTR(html_page_reboot);
    const String countDown = FPSTR(count_down_script);
    rebootPage.replace("_TXT_M_REBOOT_", FPSTR(txt_m_reboot));
    sendWrappedHTML(rebootPage + countDown);
    restartAfterDelay(500);
  } else {
    String menuRootPage = FPSTR(html_menu_root);
    menuRootPage.replace("_SHOW_LOGOUT_", String(config.unit.login_password.length() > 0 ? 1 : 0));
    // not show control button if hp not connected
    menuRootPage.replace("_SHOW_CONTROL_", String(hp.isConnected() ? 1 : 0));
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
  LOG(F("handleInitSetup()"));

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

  LOG(F("handleSetup()"));

  if (server.hasArg("RESET")) {
    String pageReset = FPSTR(html_page_reset);
    const String ssid = Config::Network::defaultHostname();
    pageReset.replace("_TXT_M_RESET_", FPSTR(txt_m_reset));
    pageReset.replace("_SSID_", ssid);
    sendWrappedHTML(pageReset);
    FileSystem::format();
    restartAfterDelay(500);
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
  const String countDown = FPSTR(count_down_script);
  saveRebootPage.replace("_TXT_M_SAVE_", FPSTR(txt_m_save));
  sendWrappedHTML(saveRebootPage + countDown);
  restartAfterDelay(500);
}

void renderView(const Ministache &view, JsonDocument &data,
                const std::vector<std::pair<String, String>> &partials = {}) {
  auto header = data[F("header")].to<JsonObject>();
  header[F("hostname")] = config.network.hostname;

  auto footer = data[F("footer")].to<JsonObject>();
  footer[F("version")] = BUILD_DATE;
  footer[F("git_hash")] = COMMIT_HASH;

  server.send(HttpStatusCodes::httpOk, F("text/html"), view.render(data, partials));
}

void handleOthers() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleOthers()"));

  if (server.method() == HTTP_POST) {
    config.other.haAutodiscovery = server.arg("HAA") == "ON";
    config.other.haAutodiscoveryTopic = server.arg("haat");
    config.other.dumpPacketsToMqtt = server.arg("DebugPckts") == "ON";
    config.other.logToMqtt = server.arg("DebugLogs") == "ON";
    config.other.safeMode = server.arg("SafeMode") == "ON";
    config.other.optimisticUpdates = server.arg("OptimisticUpdates") == "ON";
    saveOthersConfig(config);
    rebootAndSendPage();
  } else {
    JsonDocument data;
    data[F("topic")] = config.other.haAutodiscoveryTopic;

    const auto toggles = data[F("toggles")].to<JsonArray>();
    const auto autodiscovery = toggles.add<JsonObject>();
    autodiscovery[F("title")] = F("Home Assistant autodiscovery");
    autodiscovery[F("name")] = F("HAA");
    autodiscovery[F("value")] = config.other.haAutodiscovery;

    const auto safeMode = toggles.add<JsonObject>();
    safeMode[F("title")] = F("Safe mode");
    safeMode[F("name")] = F("SafeMode");
    safeMode[F("value")] = config.other.safeMode;

    const auto optimisticUpdates = toggles.add<JsonObject>();
    optimisticUpdates[F("title")] = F("Optimistic updates");
    optimisticUpdates[F("name")] = F("OptimisticUpdates");
    optimisticUpdates[F("value")] = config.other.optimisticUpdates;

    const auto debugLog = toggles.add<JsonObject>();
    debugLog[F("title")] = F("MQTT topic debug logs");
    debugLog[F("name")] = F("DebugLogs");
    debugLog[F("value")] = config.other.logToMqtt;

    const auto debugPackets = toggles.add<JsonObject>();
    debugPackets[F("title")] = F("MQTT topic debug packets");
    debugPackets[F("name")] = F("DebugPckts");
    debugPackets[F("value")] = config.other.dumpPacketsToMqtt;

    data[F("dumpPacketsToMqtt")] = config.other.dumpPacketsToMqtt;
    data[F("logToMqtt")] = config.other.logToMqtt;
    renderView(Ministache(views::others), data,
               {{"header", partials::header}, {"footer", partials::footer}});
  }
}

void handleMqtt() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleMqtt()"));

  if (server.method() == HTTP_POST) {
    config.mqtt.friendlyName = server.arg("fn");
    config.mqtt.server = server.arg("mh");
    config.mqtt.port = server.arg("ml").isEmpty() ? 1883 : server.arg("ml").toInt();
    config.mqtt.username = server.arg("mu");
    config.mqtt.password = server.arg("mp");
    config.mqtt.rootTopic = server.arg("mt");
    saveMqttConfig(config);
    rebootAndSendPage();
  } else {
    JsonDocument data;
    auto friendlyName = data[F("friendlyName")].to<JsonObject>();
    friendlyName[F("label")] = views::mqttFriendlyNameLabel;
    friendlyName[F("value")] = config.mqtt.friendlyName;
    friendlyName[F("param")] = F("fn");

    auto mqttServer = data[F("server")].to<JsonObject>();
    mqttServer[F("label")] = views::mqttHostLabel;
    mqttServer[F("value")] = config.mqtt.server;
    mqttServer[F("param")] = F("mh");

    auto port = data[F("port")].to<JsonObject>();
    port[F("value")] = config.mqtt.port;

    auto password = data[F("password")].to<JsonObject>();
    password[F("value")] = config.mqtt.password;

    auto username = data[F("user")].to<JsonObject>();
    username[F("label")] = views::mqttUserLabel;
    username[F("value")] = config.mqtt.username;
    username[F("param")] = F("mu");
    username[F("placeholder")] = F("mqtt_user");

    auto topic = data[F("topic")].to<JsonObject>();
    topic[F("label")] = views::mqttTopicLabel;
    topic[F("value")] = config.mqtt.rootTopic;
    topic[F("param")] = F("mt");
    topic[F("placeholder")] = F("topic");

    renderView(Ministache(views::mqtt), data,
               {{"mqttTextField", views::mqttTextFieldPartial},
                {"header", partials::header},
                {"footer", partials::footer}});
  }
}

void handleUnitGet() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleUnitGet()"));

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
  unitPage.replace(F("_MIN_TEMP_"), config.unit.minTemp.toString(config.unit.tempUnit));
  unitPage.replace(F("_MAX_TEMP_"), config.unit.maxTemp.toString(config.unit.tempUnit));
  unitPage.replace(F("_TEMP_STEP_"), String(config.unit.tempStep));
  // temp
  if (config.unit.tempUnit == TempUnit::F) {
    unitPage.replace(F("_TU_FAH_"), F("selected"));
  } else {
    unitPage.replace(F("_TU_CEL_"), F("selected"));
  }
  // mode
  if (config.unit.supportHeatMode) {
    unitPage.replace(F("_MD_ALL_"), F("selected"));
  } else {
    unitPage.replace(F("_MD_NONHEAT_"), F("selected"));
  }
  unitPage.replace(F("_LOGIN_PASSWORD_"), config.unit.login_password);
  sendWrappedHTML(unitPage);
}

void handleUnitPost() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleUnitPost()"));

  if (!server.arg("tu").isEmpty()) {
    config.unit.tempUnit = server.arg("tu") == "fah" ? TempUnit::F : TempUnit::C;
  }
  if (!server.arg("md").isEmpty()) {
    config.unit.supportHeatMode = server.arg("md") == "all";
  }
  if (!server.arg("lpw").isEmpty()) {
    config.unit.login_password = server.arg("lpw");
  }
  if (!server.arg("temp_step").isEmpty()) {
    config.unit.tempStep = server.arg("temp_step");
  }

  // In this POST handler, it's not entirely clear whether the min and max temp should be
  // interpreted as celsius or fahrenheit. If you change the unit on the web page, the web page
  // isn't smart enough to adjust the numeric values, so if the user just submits then there will
  // be a mismatch. On the other hand, the user might be careful and adjust the values at the same
  // time, in which case the values match the current value of useFahrenheit.
  //
  // We'll assume a dumb heuristic: get the values from the form, and try to figure out if they're
  // both valid celsius temps or valid fahrenheit temps.
  if (!server.arg("min_temp").isEmpty() && !server.arg("max_temp").isEmpty()) {
    // if both are non-empty, we're changing something
    const auto nextMinTemp = server.arg("min_temp").toFloat();
    const auto nextMaxTemp = server.arg("max_temp").toFloat();
    if (nextMaxTemp < nextMinTemp) {
      LOG(F("ERROR: min_temp > max_temp, not saving (min_temp: %f, max_temp: %f)"), nextMinTemp,
          nextMaxTemp);
      return;
    }
    // Both temperatures under 50 would be expected for Celsius, and not at all expected for
    // Fahrenheit
    TempUnit unit = (nextMinTemp < 50.0f && nextMaxTemp < 50.0f) ? TempUnit::C : TempUnit::F;
    config.unit.minTemp = Temperature(nextMinTemp, unit);
    config.unit.maxTemp = Temperature(nextMaxTemp, unit);
  }
  saveUnitConfig(config);
  rebootAndSendPage();
}

void handleWifi() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleWifi()"));

  if (server.method() == HTTP_POST) {
    config.network.accessPointSsid = server.arg("ssid");
    config.network.accessPointPassword = server.arg("psk");
    config.network.hostname = server.arg("hn");
    config.network.otaUpdatePassword = server.arg("otapwd");
    saveWifiConfig(config);
    rebootAndSendPage();
  } else {
    String wifiPage = FPSTR(html_page_wifi);
    String str_ap_ssid = config.network.accessPointSsid;
    String str_ap_pwd = config.network.accessPointPassword;
    String str_ota_pwd = config.network.otaUpdatePassword;
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
  LOG(F("handleStatus()"));

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

  const String connected =
      String(F("<span style='color:#47c266'><b>")) + FPSTR(txt_status_connect) + F("</b><span>");

  const String disconnected = String(F("<span style='color:#d43535'><b>")) +
                              FPSTR(txt_status_disconnect) + F("</b></span>");

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

  LOG(F("handleControl()"));

  HeatpumpSettings settings(hp.getSettings());
  settings = change_states(settings);
  String controlPage = FPSTR(html_page_control);
  String headerContent = FPSTR(html_common_header);
  String footerContent = FPSTR(html_common_footer);
  headerContent.replace("_UNIT_NAME_", config.network.hostname);
  footerContent.replace("_VERSION_", BUILD_DATE);
  footerContent.replace("_GIT_HASH_", COMMIT_HASH);
  controlPage.replace("_TXT_BACK_", FPSTR(txt_back));
  controlPage.replace("_UNIT_NAME_", config.network.hostname);
  controlPage.replace("_RATE_", "60");
  controlPage.replace(
      "_ROOMTEMP_",
      Temperature(hp.getRoomTemperature(), TempUnit::C).toString(config.unit.tempUnit, 0.1f));
  controlPage.replace("_USE_FAHRENHEIT_", String(config.unit.tempUnit == TempUnit::F ? 1 : 0));
  controlPage.replace("_TEMP_SCALE_", getTemperatureScale());
  controlPage.replace("_HEAT_MODE_SUPPORT_", String(config.unit.supportHeatMode ? 1 : 0));
  controlPage.replace(F("_MIN_TEMP_"), config.unit.minTemp.toString(config.unit.tempUnit));
  controlPage.replace(F("_MAX_TEMP_"), config.unit.maxTemp.toString(config.unit.tempUnit));
  controlPage.replace(F("_TEMP_STEP_"), String(config.unit.tempStep));
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
                      Temperature(hp.getTemperature(), TempUnit::C).toString(config.unit.tempUnit));

  // We need to send the page content in chunks to overcome
  // a limitation on the maximum size we can send at one
  // time (approx 6k).
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(HttpStatusCodes::httpOk, "text/html", headerContent);
  server.sendContent(controlPage);
  server.sendContent(footerContent);
  // Signal the end of the content
  server.sendContent("");
}

void handleMetrics() {
  LOG(F("handleMetrics()"));

  const HeatpumpSettings currentSettings(hp.getSettings());
  const HeatpumpStatus currentStatus(hp.getStatus());
  ArduinoJson::JsonDocument data;
  data["unit_name"] = config.network.hostname;
  data["version"] = BUILD_DATE;
  data["git_hash"] = COMMIT_HASH;
  data["power"] = currentSettings.power == "ON" ? 1 : 0;
  data["roomtemp"] = currentStatus.roomTemperature.toString(TempUnit::C);
  data["temp"] = currentSettings.temperature.toString(TempUnit::C);
  data["oper"] = currentStatus.operating ? 1 : 0;
  data["compfreq"] = currentStatus.compressorFrequency;

  data["fan"] = currentSettings.fan;
  if (currentSettings.fan == "AUTO") {  // NOLINT(bugprone-branch-clone) false positive
    data["fan"] = "-1";
  } else if (currentSettings.fan == "QUIET") {
    data["fan"] = "0";
  }

  data["vane"] = currentSettings.vane;
  if (currentSettings.vane == "AUTO") {  // NOLINT(bugprone-branch-clone) false positive
    data["vane"] = "-1";
  } else if (currentSettings.vane == "SWING") {
    data["vane"] = "0";
  }

  if (currentSettings.wideVane == "SWING") {  // NOLINT(bugprone-branch-clone) false positive
    data["widevane"] = "0";
  } else if (currentSettings.wideVane == "<<") {
    data["widevane"] = "1";
  } else if (currentSettings.wideVane == "<") {
    data["widevane"] = "2";
  } else if (currentSettings.wideVane == "|") {
    data["widevane"] = "3";
  } else if (currentSettings.wideVane == ">") {
    data["widevane"] = "4";
  } else if (currentSettings.wideVane == ">>") {
    data["widevane"] = "5";
  } else if (currentSettings.wideVane == "<>") {
    data["widevane"] = "6";
  } else {
    data["widevane"] = "-2";
  }

  if (currentSettings.mode == "AUTO") {  // NOLINT(bugprone-branch-clone) false positive
    data["mode"] = "-1";
  } else if (currentSettings.mode == "COOL") {
    data["mode"] = "1";
  } else if (currentSettings.mode == "DRY") {
    data["mode"] = "2";
  } else if (currentSettings.mode == "HEAT") {
    data["mode"] = "3";
  } else if (currentSettings.mode == "FAN") {
    data["mode"] = "4";
  } else if (data["power"] == "0") {
    data["mode"] = "0";
  } else {
    data["mode"] = "-2";
  }

  Ministache metricsTemplate(views::metricsView);
  server.send(HttpStatusCodes::httpOk, F("text/plain"), metricsTemplate.render(data));
}

void handleMetricsJson() {
  JsonDocument doc;
  doc[F("hostname")] = config.network.hostname;
  doc[F("version")] = BUILD_DATE;
  doc[F("git_hash")] = COMMIT_HASH;

  auto systemStatus = doc[F("status")].to<JsonObject>();
  systemStatus[F("safeModeLockout")] = safeModeActive();

  // auto mallocStats = mallinfo();
  // doc[F("memory")] = JsonObject();
  // doc[F("memory")][F("free")] = mallocStats.fordblks;
  // doc[F("memory")][F("used")] = mallocStats.uordblks;
  // doc[F("memory")][F("percent")] = mallocStats.uordblks / (mallocStats.uordblks +
  // mallocStats.fordblks);

  auto heatpump = doc[F("heatpump")].to<JsonObject>();
  heatpump[F("connected")] = hp.isConnected();
  if (hp.isConnected()) {
    const HeatpumpStatus currentStatus(hp.getStatus());
    auto status = heatpump[F("status")].to<JsonObject>();
    status[F("compressorFrequency")] = currentStatus.compressorFrequency;
    status[F("operating")] = currentStatus.operating;
    status[F("roomTemperature_F")] = currentStatus.roomTemperature.toString(TempUnit::F, 0.1f);
    status[F("roomTemperature")] = currentStatus.roomTemperature.toString(TempUnit::C, 0.1f);

    const HeatpumpSettings currentSettings(hp.getSettings());
    auto settings = heatpump[F("settings")].to<JsonObject>();
    settings[F("connected")] = currentSettings.connected;
    settings[F("fan")] = currentSettings.fan;
    settings[F("iSee")] = currentSettings.iSee;
    settings[F("mode")] = currentSettings.mode;
    settings[F("power")] = currentSettings.power;
    settings[F("temperature_F")] = currentSettings.temperature.toString(TempUnit::F, 0.1f);
    settings[F("temperature")] = currentSettings.temperature.toString(TempUnit::C, 0.1f);
    settings[F("vane")] = currentSettings.vane;
    settings[F("wideVane")] = currentSettings.wideVane;
  }

  String response;
  serializeJsonPretty(doc, response);
  server.send(HttpStatusCodes::httpOk, F("application/json"), response);
}

// login page, also called for logout
void handleLogin() {
  LOG(F("handleLogin()"));

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
      if (server.arg("USERNAME") == "admin" &&
          server.arg("PASSWORD") == config.unit.login_password) {
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
    if (is_authenticated() or config.unit.login_password.length() == 0) {
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
  loginPage.replace(F("_LOGIN_SUCCESS_"), String(loginSuccess ? 1 : 0));
  loginPage.replace(F("_LOGIN_MSG_"), msg);
  sendWrappedHTML(loginPage);
}

void handleUpgrade() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleUpgrade()"));

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
  LOG(F("handleUploadDone()"));

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
    LOG(F("Restarting in 500ms..."));
    restartAfterDelay(500);
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
    const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {  // start with max available size
      // Update.printError(Serial);
      uploaderror = UploadError::fileTooLarge;
      return;
    }
  } else if (uploaderror == UploadError::noError && (upload.status == UPLOAD_FILE_WRITE)) {
    if (upload.totalSize == 0) {
      if (upload.buf[0] != 0xE9) {
        // Serial.println(PSTR("Upload: File magic header does not start with
        // 0xE9"));
        uploaderror = UploadError::fileMagicHeaderIncorrect;
        return;
      }
      const uint32_t bin_flash_size = ESP.magicFlashChipSize((upload.buf[3] & 0xf0) >> 4);
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
    if (uploaderror == UploadError::noError &&
        (Update.write(upload.buf, upload.currentSize) != upload.currentSize)) {
      // Update.printError(Serial);
      uploaderror = UploadError::fileUploadBufferMiscompare;
      return;
    }
  } else if (uploaderror == UploadError::noError && (upload.status == UPLOAD_FILE_END)) {
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
      newSettings.temperature = Temperature(server.arg("TEMP").toFloat(), config.unit.tempUnit);
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

JsonDocument getHeatPumpStatusJson() {
  const HeatpumpStatus currentStatus(hp.getStatus());
  const HeatpumpSettings currentSettings(hp.getSettings());

  JsonDocument doc;

  doc["operating"] = currentStatus.operating;
  doc["roomTemperature"] = currentStatus.roomTemperature.get(config.unit.tempUnit, 0.5f);
  doc["temperature"] = currentSettings.temperature.get(config.unit.tempUnit, 0.5f);
  doc["fan"] = currentSettings.fan;
  doc["vane"] = currentSettings.vane;
  doc["wideVane"] = currentSettings.wideVane;
  doc["mode"] = hpGetMode(currentSettings);
  doc["action"] = hpGetAction(currentStatus, currentSettings);
  doc["compressorFrequency"] = currentStatus.compressorFrequency;

  return doc;
}

String hpGetMode(const HeatpumpSettings &hpSettings) {
  // Map the heat pump state to one of HA's HVAC_MODE_* values.
  // https://github.com/home-assistant/core/blob/master/homeassistant/components/climate/const.py#L3-L23

  if (hpSettings.power.equalsIgnoreCase("off")) {
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

String hpGetAction(const HeatpumpStatus &hpStatus, const HeatpumpSettings &hpSettings) {
  // Map heat pump state to one of HA's CURRENT_HVAC_* values.
  // https://github.com/home-assistant/core/blob/master/homeassistant/components/climate/const.py#L80-L86

  if (hpSettings.power.equalsIgnoreCase("off")) {
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

void pushHeatPumpStateToMqtt() {
  // If we're not pushing optimistic updates on every incoming change, then we should send the
  // state to MQTT at a higher cadence
  const uint32_t interval = config.other.optimisticUpdates ? 30000UL : 10000UL;
  if (millis() - lastMqttStatePacketSend > interval) {
    String mqttOutput;
    serializeJson(getHeatPumpStatusJson(), mqttOutput);

    if (!mqtt_client.publish_P(config.mqtt.ha_state_topic().c_str(), mqttOutput.c_str(), false)) {
      LOG(F("Failed to publish hp status change"));
    }

    lastMqttStatePacketSend = millis();
  }
}

// NOLINTNEXTLINE(readability-non-const-parameter)
void hpPacketDebug(byte *packet_, unsigned int length, char *packetDirection_) {
  // `packet_` & `packetDirection_` should be const, but we don't control the signature of the
  // callback function, so we stuff them into more restrictive pointers before moving on.
  const byte *const packet = packet_;
  const char *const packetDirection = packetDirection_;

  if (config.other.dumpPacketsToMqtt) {
    String message;
    for (unsigned int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0";  // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    JsonDocument root;

    root[packetDirection] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    if (!mqtt_client.publish(config.mqtt.ha_debug_pckts_topic().c_str(), mqttOutput.c_str())) {
      mqtt_client.publish(config.mqtt.ha_debug_logs_topic().c_str(),
                          "Failed to publish to heatpump/debug topic");
    }
  }
}

// This is used to send an optimistic state update to MQTT, which in turn causes the Home Assistant
// UI to update without waiting to apply a setting and read it back.
void publishOptimisticStateChange(JsonDocument &override) {
  if (!config.other.optimisticUpdates) {
    return;
  }

  JsonDocument status(getHeatPumpStatusJson());
  auto override2 = override.as<JsonObject>();
  for (auto pair : override2) {
    status[pair.key()] = pair.value();
  }
  String mqttOutput;
  serializeJson(status, mqttOutput);
  if (config.other.dumpPacketsToMqtt) {
    mqtt_client.publish(config.mqtt.ha_debug_pckts_topic().c_str(), mqttOutput.c_str(), false);
  }
  if (!mqtt_client.publish_P(config.mqtt.ha_state_topic().c_str(), mqttOutput.c_str(), false)) {
    LOG(F("Failed to publish dummy hp status change"));
  }

  // Restart counter for waiting enought time for the unit to update before
  // sending a state packet
  lastMqttStatePacketSend = millis();
}

static std::map<String, std::function<void(const char *)>> mqttTopicHandlers;

void mqttCallback(const char *topic, const byte *payload, unsigned int length) {
  // Copy payload into message buffer
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = static_cast<char>(payload[i]);
  }
  message[length] = '\0';

  auto handler = mqttTopicHandlers.find(topic);
  if (handler != mqttTopicHandlers.end()) {
    handler->second(message);
  } else {
    const String msg = String("heatpump: unrecognized mqtt topic: ") + topic;
    mqtt_client.publish(config.mqtt.ha_debug_logs_topic().c_str(), msg.c_str());
  }
}

void onSetCustomPacket(const char *message) {
  const String custom = message;

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
  // packetDirection should have been declared as const char *, but since hpPacketDebug is a
  // callback function, it can't be.  So we'll just have to be careful not to modify it.
  hpPacketDebug(bytes, byteCount, const_cast<char *>("customPacket"));

  hp.sendCustomPacket(bytes, byteCount);
}

void onSetDebugLogs(const char *message) {
  if (strcmp(message, "on") == 0) {
    Logger::enableMqttLogging(mqtt_client, config.mqtt.ha_debug_logs_topic().c_str());
    config.other.logToMqtt = true;
    LOG(F("Debug logs mode enabled"));
  } else if (strcmp(message, "off") == 0) {
    config.other.logToMqtt = false;
    LOG(F("Debug logs mode disabled"));
    Logger::disableMqttLogging();
  }
}

void onSetDebugPackets(const char *message) {
  if (strcmp(message, "on") == 0) {
    config.other.dumpPacketsToMqtt = true;
    mqtt_client.publish(config.mqtt.ha_debug_pckts_topic().c_str(), "Debug packets mode enabled");
  } else if (strcmp(message, "off") == 0) {
    config.other.dumpPacketsToMqtt = false;
    mqtt_client.publish(config.mqtt.ha_debug_pckts_topic().c_str(), "Debug packets mode disabled");
  }
}

void onSetSystem(const char *message) {
  if (strcmp(message, "reboot") == 0) {  // We receive reboot command
    restartAfterDelay(0);
  }
}

void onSetRemoteTemp(const char *message) {
  const float temperature = strtof(message, NULL);
  if (temperature == 0) {      // Remote temp disabled by mqtt topic set
    remoteTempActive = false;  // clear the remote temp flag
    hp.setRemoteTemperature(0.0);
  } else {
    if (safeModeActive()) {
      LOG(F("Safe mode lockout turned off: we got a remote temp message to %f"), temperature);
    }
    remoteTempActive = true;    // Remote temp has been pushed.
    lastRemoteTemp = millis();  // Note time
    hp.setRemoteTemperature(Temperature(temperature, config.unit.tempUnit).getCelsius());
  }
}

void onSetWideVane(const char *message) {
  JsonDocument stateOverride;
  stateOverride["wideVane"] = String(message);
  publishOptimisticStateChange(stateOverride);
  hp.setWideVaneSetting(message);
}

void onSetVane(const char *message) {
  JsonDocument stateOverride;
  stateOverride["vane"] = String(message);
  publishOptimisticStateChange(stateOverride);
  hp.setVaneSetting(message);
}

void onSetFan(const char *message) {
  JsonDocument stateOverride;
  stateOverride["fan"] = String(message);
  publishOptimisticStateChange(stateOverride);
  hp.setFanSpeed(message);
}

void onSetTemp(const char *message) {
  JsonDocument stateOverride;
  const float value = strtof(message, NULL);
  const Temperature temperature =
      Temperature(value, config.unit.tempUnit).clamp(config.unit.minTemp, config.unit.maxTemp);

  stateOverride["temperature"] = temperature.get(config.unit.tempUnit);

  publishOptimisticStateChange(stateOverride);
  hp.setTemperature(temperature.getCelsius());
}

void onSetMode(const char *message) {
  JsonDocument stateOverride;
  String modeUpper = message;
  modeUpper.toUpperCase();
  if (modeUpper == "OFF" || safeModeActive()) {
    if (modeUpper != "OFF") {
      LOG(F("Safe mode lockout enabled, ignoring mode change to %s"), modeUpper.c_str());
    }
    stateOverride["mode"] = "off";
    stateOverride["action"] = "off";
    publishOptimisticStateChange(stateOverride);
    hp.setPowerSetting("OFF");
  } else {
    if (modeUpper == "HEAT_COOL") {
      stateOverride["mode"] = "heat_cool";
      modeUpper = "AUTO";
      // NOLINTNEXTLINE(bugprone-branch-clone) why is the next branch getting flagged as a clone?
    } else if (modeUpper == "HEAT") {
      stateOverride["mode"] = "heat";
    } else if (modeUpper == "COOL") {
      stateOverride["mode"] = "cool";
    } else if (modeUpper == "DRY") {
      stateOverride["mode"] = "dry";
    } else if (modeUpper == "FAN_ONLY") {
      stateOverride["mode"] = "fan_only";
      modeUpper = "FAN";
    } else {
      return;
    }
    publishOptimisticStateChange(stateOverride);
    hp.setPowerSetting("ON");
    hp.setModeSetting(modeUpper.c_str());
  }
}

void sendHomeAssistantConfig() {
  // send HA config packet
  // setup HA payload device
  JsonDocument haConfig;

  haConfig[F("name")] = config.network.hostname;
  haConfig[F("unique_id")] = getId();

  haConfig[F("supportHeatMode")] = config.unit.supportHeatMode;

  haConfig[F("mode_cmd_t")] = config.mqtt.ha_mode_set_topic();
  haConfig[F("mode_stat_t")] = config.mqtt.ha_state_topic();
  haConfig[F("temp_cmd_t")] = config.mqtt.ha_temp_set_topic();
  haConfig[F("temp_stat_t")] = config.mqtt.ha_state_topic();
  haConfig[F("avty_t")] =
      config.mqtt.ha_availability_topic();                 // MQTT last will (status) messages topic
  haConfig[F("pl_not_avail")] = mqtt_payload_unavailable;  // MQTT offline message payload
  haConfig[F("pl_avail")] = mqtt_payload_available;        // MQTT online message payload

  auto tempStatTpl = haConfig[F("tempStatTpl")].to<JsonObject>();
  tempStatTpl[F("fieldName")] = F("temperature");
  tempStatTpl[F("minTemp")] = config.unit.minTemp.toString(config.unit.tempUnit);
  tempStatTpl[F("maxTemp")] = config.unit.maxTemp.toString(config.unit.tempUnit);
  tempStatTpl[F("defaultTemp")] = Temperature(22.f, TempUnit::C).toString(config.unit.tempUnit);

  haConfig[F("curr_temp_t")] = config.mqtt.ha_state_topic();

  auto currTempTpl = haConfig[F("currTempTpl")].to<JsonObject>();
  currTempTpl[F("fieldName")] = F("roomTemperature");
  currTempTpl[F("minTemp")] = Temperature(1.f, TempUnit::C).toString(config.unit.tempUnit);

  haConfig[F("min_temp")] = config.unit.minTemp.toString(config.unit.tempUnit);
  haConfig[F("max_temp")] = config.unit.maxTemp.toString(config.unit.tempUnit);
  haConfig[F("temp_step")] = config.unit.tempStep;
  haConfig[F("temperature_unit")] = getTemperatureScale();

  haConfig[F("fan_mode_cmd_t")] = config.mqtt.ha_fan_set_topic();
  haConfig[F("fan_mode_stat_t")] = config.mqtt.ha_state_topic();

  haConfig[F("swing_mode_cmd_t")] = config.mqtt.ha_vane_set_topic();
  haConfig[F("swing_mode_stat_t")] = config.mqtt.ha_state_topic();
  haConfig[F("action_topic")] = config.mqtt.ha_state_topic();

  haConfig[F("friendlyName")] = config.mqtt.friendlyName;
  haConfig[F("buildDate")] = BUILD_DATE;
  haConfig[F("commitHash")] = COMMIT_HASH;
  haConfig[F("localIP")] = WiFi.localIP().toString();

  // Additional attributes are in the state
  // For now, only compressorFrequency
  haConfig[F("json_attr_t")] = config.mqtt.ha_state_topic();

  String mqttOutput = Ministache(views::autoconfig).render(haConfig);

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
    mqtt_client.connect(config.network.hostname.c_str(), config.mqtt.username.c_str(),
                        config.mqtt.password.c_str(), config.mqtt.ha_availability_topic().c_str(),
                        1, true, mqtt_payload_unavailable);
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
      mqttTopicHandlers = {{config.mqtt.ha_mode_set_topic(), onSetMode},
                           {config.mqtt.ha_temp_set_topic(), onSetTemp},
                           {config.mqtt.ha_fan_set_topic(), onSetFan},
                           {config.mqtt.ha_vane_set_topic(), onSetVane},
                           {config.mqtt.ha_wideVane_set_topic(), onSetWideVane},
                           {config.mqtt.ha_remote_temp_set_topic(), onSetRemoteTemp},
                           {config.mqtt.ha_system_set_topic(), onSetSystem},
                           {config.mqtt.ha_debug_pckts_set_topic(), onSetDebugPackets},
                           {config.mqtt.ha_debug_logs_set_topic(), onSetDebugLogs},
                           {config.mqtt.ha_custom_packet(), onSetCustomPacket}};
      for (const auto &[topic, _] : mqttTopicHandlers) {
        mqtt_client.subscribe(topic.c_str());
      }
      mqtt_client.publish(config.mqtt.ha_availability_topic().c_str(), mqtt_payload_available,
                          true);  // publish status as available
      if (config.other.haAutodiscovery) {
        sendHomeAssistantConfig();
      }
    }
  }
}

bool connectWifi() {
  const int connectTimeoutMs = 30000;
#ifdef ESP32
  WiFi.setHostname(config.network.hostname.c_str());
#else
  WiFi.hostname(config.network.hostname.c_str());
#endif
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }
#ifdef ESP32
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
#endif
  WiFi.begin(config.network.accessPointSsid.c_str(), config.network.accessPointPassword.c_str());
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

String getTemperatureScale() {
  if (config.unit.tempUnit == TempUnit::F) {
    return "F";
  }
  return "C";
}

String getId() {
#ifdef ESP32
  const uint64_t macAddress = ESP.getEfuseMac();
  const uint32_t chipID = macAddress >> 24;
#else
  const uint32_t chipID = ESP.getChipId();
#endif
  return String(chipID, HEX);
}

// Check if header is present and correct
bool is_authenticated() {
  if (server.hasHeader("Cookie")) {
    // Found cookie;
    if (server.header("Cookie").indexOf("M2MSESSIONID=1") != -1) {
      // Authentication Successful
      return true;
    }
  }
  // Authentication Failed
  return false;
}

bool checkLogin() {
  if (!is_authenticated() and config.unit.login_password.length() > 0) {
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

void loop() {  // NOLINT(readability-function-cognitive-complexity)
  getTimer()->tick();

  if (restartPending) {
    // We're waiting for the timeout specified in restartAfterDelay, we shouldn't process anything
    // else in the meantime
    return;
  }

  server.handleClient();
  processOTALoop();

  // reset board to attempt to connect to wifi again if in ap mode or wifi
  // dropped out and time limit passed
  if (WiFi.getMode() == WIFI_STA and
      WiFi.status() == WL_CONNECTED) {  // NOLINT(bugprone-branch-clone)
    wifi_timeout = millis() + WIFI_RETRY_INTERVAL_MS;
  } else if (config.network.configured() and millis() > wifi_timeout) {
    LOG(F("Lost network connection, restarting..."));
    // TODO(floatplane) do we really need to reboot here? Can we just try to reconnect?
    restartAfterDelay(0);
  }

  if (captive) {
    dnsServer.processNextRequest();
    return;
  }

  // Sync HVAC UNIT
  if (hp.isConnected()) {
    hpConnectionRetries = 0;
    // if it's been CHECK_REMOTE_TEMP_INTERVAL_MS since last remote_temp
    // message was received, either revert back to HP internal temp sensor
    // or shut down.
    if (remoteTempStale() && (remoteTempActive || config.other.safeMode)) {
      if (config.other.safeMode) {
        if (strcmp(hp.getPowerSetting(), "ON") == 0) {
          LOG(F("Remote temperature updates aren't coming in, shutting down"));
          hp.setPowerSetting("OFF");
        }
      } else if (remoteTempActive) {
        LOG(F("Remote temperature feed is stale, reverting to internal thermometer"));
        remoteTempActive = false;
        hp.setRemoteTemperature(0.0f);
      }
    }
    hp.sync();
  } else {
    LOG(F("HVAC not connected"));
    // Use exponential backoff for retries, where each retry is double the
    // length of the previous one.
    const uint64_t durationNextSync = (1UL << hpConnectionRetries) * HP_RETRY_INTERVAL_MS;
    if (((millis() - lastHpSync > durationNextSync) or lastHpSync == 0)) {
      lastHpSync = millis();
      // If we've retried more than the max number of tries, keep retrying at
      // that fixed interval, which is several minutes.
      hpConnectionRetries = min(hpConnectionRetries + 1U, HP_MAX_RETRIES);
      hpConnectionTotalRetries++;
      LOG(F("Trying to reconnect to HVAC"));
      hp.sync();
    }
  }

  if (config.mqtt.configured()) {
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
      mqtt_client.loop();
      pushHeatPumpStateToMqtt();
    }
  }
}

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
#include <math.h>          // for rounding to Fahrenheit values
// #include <Ticker.h>     // for LED status (Using a Wemos D1-Mini)

#include <map>

#include "HeatpumpSettings.hpp"
#include "filesystem.hpp"
#include "html_common.hpp"        // common code HTML (like header, footer)
#include "html_init.hpp"          // code html for initial config
#include "html_menu.hpp"          // code html for menu
#include "html_metrics.hpp"       // prometheus metrics
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
    Other()
        : haAutodiscovery(true),
          haAutodiscoveryTopic(F("homeassistant")),
          logToMqtt(false),
          dumpPacketsToMqtt(false) {
    }
  } other;

  // Unit
  struct Unit {
    bool useFahrenheit = false;
    // support heat mode settings, some model do not support heat mode
    bool supportHeatMode = true;
    uint8_t minTempCelsius{16};  // Minimum temperature, check
                                 // value from heatpump remote control
    uint8_t maxTempCelsius{31};  // Maximum temperature, check
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
    const String &ha_settings_topic() const {
      static const String topicPath{rootTopic + F("/") + friendlyName + F("/settings")};
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
const PROGMEM uint32_t SEND_ROOM_TEMP_INTERVAL_MS =
    30000;  // 45 seconds (anything less may cause bouncing)
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
uint64_t lastTempSend;
uint64_t lastMqttRetry;
uint64_t lastHpSync;
unsigned int hpConnectionRetries;
unsigned int hpConnectionTotalRetries;
uint64_t lastRemoteTemp;

// Local state
JsonDocument rootInfo;

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

  loadWifi();
  loadOthers();
  loadUnit();
  loadMqtt();
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
    server.on(F("/unit"), handleUnit);
    server.on(F("/status"), handleStatus);
    server.on(F("/others"), handleOthers);
    server.on(F("/metrics"), handleMetrics);
    server.on(F("/metrics.json"), handleMetricsJson);
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
    hp.setSettingsChangedCallback(hpSettingsChanged);
    hp.setStatusChangedCallback(hpStatusChanged);
    hp.setPacketCallback(hpPacketDebug);
    // Allow Remote/Panel
    hp.enableExternalUpdate();
    hp.enableAutoUpdate();
    hp.connect(&Serial);

    // TODO(floatplane) not sure this is necessary, we're not using it in setup, and we'll reload in
    // loop()
    const heatpumpStatus currentStatus = hp.getStatus();
    const HeatpumpSettings currentSettings(hp.getSettings());
    rootInfo["roomTemperature"] =
        convertCelsiusToLocalUnit(currentStatus.roomTemperature, config.unit.useFahrenheit);
    rootInfo["temperature"] =
        convertCelsiusToLocalUnit(currentSettings.temperature, config.unit.useFahrenheit);
    rootInfo["fan"] = currentSettings.fan;
    rootInfo["vane"] = currentSettings.vane;
    rootInfo["wideVane"] = currentSettings.wideVane;
    rootInfo["mode"] = hpGetMode(currentSettings);
    rootInfo["action"] = hpGetAction(currentStatus, currentSettings);
    rootInfo["compressorFrequency"] = currentStatus.compressorFrequency;
    lastTempSend = millis();
    // END TODO
  } else {
    dnsServer.start(DNS_PORT, "*", apIP);
    initCaptivePortal();
  }
  LOG(F("calling initOTA"));
  initOTA(config.network.hostname, config.network.otaUpdatePassword);
  LOG(F("Setup complete"));
  logConfig();
}

void loadWifi() {
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

void loadMqtt() {
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

  LOG(F("=== START DEBUG MQTT ==="));
  LOG(F("Friendly Name") + config.mqtt.friendlyName);
  LOG(F("IP Server ") + config.mqtt.server);
  LOG(F("IP Port ") + portString);
  LOG(F("Username ") + config.mqtt.username);
  LOG(F("Password ") + config.mqtt.password);
  LOG(F("Root topic ") + config.mqtt.rootTopic);
  LOG(F("=== END DEBUG MQTT ==="));
}

void loadUnit() {
  const JsonDocument doc = FileSystem::loadJSON(unit_conf);
  if (doc.isNull()) {
    return;
  }
  // unit
  String unit_tempUnit = doc["unit_tempUnit"].as<String>();
  if (unit_tempUnit == "fah") {
    config.unit.useFahrenheit = true;
  }
  config.unit.minTempCelsius = static_cast<uint8_t>(doc["min_temp"].as<String>().toInt());
  config.unit.maxTempCelsius = static_cast<uint8_t>(doc["max_temp"].as<String>().toInt());
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

void loadOthers() {
  const JsonDocument doc = FileSystem::loadJSON(others_conf);
  if (doc.isNull()) {
    return;
  }
  config.other.haAutodiscoveryTopic = doc["haat"].as<String>();
  config.other.haAutodiscovery = doc["haa"].as<String>() == "ON";
  config.other.dumpPacketsToMqtt = doc["debugPckts"].as<String>() == "ON";
  config.other.logToMqtt = doc["debugLogs"].as<String>() == "ON";
}

void saveMqtt(const Config &config) {
  JsonDocument doc;  // NOLINT(misc-const-correctness)
  doc["mqtt_fn"] = config.mqtt.friendlyName;
  doc["mqtt_host"] = config.mqtt.server;
  doc["mqtt_port"] = String(config.mqtt.port);
  doc["mqtt_user"] = config.mqtt.username;
  doc["mqtt_pwd"] = config.mqtt.password;
  doc["mqtt_topic"] = config.mqtt.rootTopic;
  FileSystem::saveJSON(mqtt_conf, doc);
}

void saveUnit(const Config &config) {
  JsonDocument doc;  // NOLINT(misc-const-correctness)
  doc["unit_tempUnit"] = config.unit.useFahrenheit ? "fah" : "cel";
  doc["min_temp"] = String(config.unit.minTempCelsius);
  doc["max_temp"] = String(config.unit.maxTempCelsius);
  doc["temp_step"] = config.unit.tempStep;
  doc["support_mode"] = config.unit.supportHeatMode ? "all" : "nht";
  doc["login_password"] = config.unit.login_password;
  FileSystem::saveJSON(unit_conf, doc);
}

void saveWifi(const Config &config) {
  JsonDocument doc;  // NOLINT(misc-const-correctness)
  doc["ap_ssid"] = config.network.accessPointSsid;
  doc["ap_pwd"] = config.network.accessPointPassword;
  doc["hostname"] = config.network.hostname;
  doc["ota_pwd"] = config.network.accessPointPassword;
  FileSystem::saveJSON(wifi_conf, doc);
}

void saveOthers(const Config &config) {
  JsonDocument doc;  // NOLINT(misc-const-correctness)
  doc["haa"] = config.other.haAutodiscovery ? "ON" : "OFF";
  doc["haat"] = config.other.haAutodiscoveryTopic;
  doc["debugPckts"] = config.other.dumpPacketsToMqtt ? "ON" : "OFF";
  doc["debugLogs"] = config.other.logToMqtt ? "ON" : "OFF";
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
    saveWifi(config);
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
    saveOthers(config);
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

    othersPage.replace("_HAA_TOPIC_", config.other.haAutodiscoveryTopic);
    if (config.other.haAutodiscovery) {
      othersPage.replace("_HAA_ON_", "selected");
    } else {
      othersPage.replace("_HAA_OFF_", "selected");
    }
    if (config.other.dumpPacketsToMqtt) {
      othersPage.replace("_DEBUG_PCKTS_ON_", "selected");
    } else {
      othersPage.replace("_DEBUG_PCKTS_OFF_", "selected");
    }
    if (config.other.logToMqtt) {
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

  LOG(F("handleMqtt()"));

  if (server.method() == HTTP_POST) {
    config.mqtt.friendlyName = server.arg("fn");
    config.mqtt.server = server.arg("mh");
    config.mqtt.port = server.arg("ml").isEmpty() ? 1883 : server.arg("ml").toInt();
    config.mqtt.username = server.arg("mu");
    config.mqtt.password = server.arg("mp");
    config.mqtt.rootTopic = server.arg("mt");
    saveMqtt(config);
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
    mqttPage.replace(F("_MQTT_FN_"), config.mqtt.friendlyName);
    mqttPage.replace(F("_MQTT_HOST_"), config.mqtt.server);
    mqttPage.replace(F("_MQTT_PORT_"), String(config.mqtt.port));
    mqttPage.replace(F("_MQTT_USER_"), config.mqtt.username);
    mqttPage.replace(F("_MQTT_PASSWORD_"), config.mqtt.password);
    mqttPage.replace(F("_MQTT_TOPIC_"), config.mqtt.rootTopic);
    sendWrappedHTML(mqttPage);
  }
}

void handleUnit() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleUnit()"));

  if (server.method() == HTTP_POST) {
    config.unit.useFahrenheit =
        server.arg("tu").isEmpty() ? config.unit.useFahrenheit : server.arg("tu") == "fah";
    config.unit.supportHeatMode =
        server.arg("md").isEmpty() ? config.unit.supportHeatMode : server.arg("md") == "all";
    config.unit.login_password =
        server.arg("lpw").isEmpty() ? config.unit.login_password : server.arg("lpw");
    config.unit.tempStep =
        server.arg("temp_step").isEmpty() ? config.unit.tempStep : server.arg("temp_step");

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
      // TODO(floatplane): using std::round to maintain behavior from the code I forked, though I'm
      // not sure it's correct
      // TODO(floatplane): should probably be clamping these to reasonable numbers
      // TODO(floatplane): there's no form validation, a user can submit min > max or negative temps
      // or whatever.
      if (nextMinTemp < 50 && nextMaxTemp < 50) {
        // almost certainly celsius
        config.unit.minTempCelsius = static_cast<uint8_t>(std::round(nextMinTemp));
        config.unit.maxTempCelsius = static_cast<uint8_t>(std::round(nextMaxTemp));
      } else {
        // almost certainly fahrenheit
        config.unit.minTempCelsius =
            static_cast<uint8_t>(std::round(convertLocalUnitToCelsius(nextMinTemp, true)));
        config.unit.maxTempCelsius =
            static_cast<uint8_t>(std::round(convertLocalUnitToCelsius(nextMaxTemp, true)));
      }
    }
    saveUnit(config);
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
    unitPage.replace(F("_MIN_TEMP_"), String(convertCelsiusToLocalUnit(config.unit.minTempCelsius,
                                                                       config.unit.useFahrenheit)));
    unitPage.replace(F("_MAX_TEMP_"), String(convertCelsiusToLocalUnit(config.unit.maxTempCelsius,
                                                                       config.unit.useFahrenheit)));
    unitPage.replace(F("_TEMP_STEP_"), String(config.unit.tempStep));
    // temp
    if (config.unit.useFahrenheit) {
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
    saveWifi(config);
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
  controlPage.replace("_ROOMTEMP_", String(convertCelsiusToLocalUnit(hp.getRoomTemperature(),
                                                                     config.unit.useFahrenheit)));
  controlPage.replace("_USE_FAHRENHEIT_", String(config.unit.useFahrenheit ? 1 : 0));
  controlPage.replace("_TEMP_SCALE_", getTemperatureScale());
  controlPage.replace("_HEAT_MODE_SUPPORT_", String(config.unit.supportHeatMode ? 1 : 0));
  controlPage.replace(F("_MIN_TEMP_"), String(convertCelsiusToLocalUnit(
                                           config.unit.minTempCelsius, config.unit.useFahrenheit)));
  controlPage.replace(F("_MAX_TEMP_"), String(convertCelsiusToLocalUnit(
                                           config.unit.maxTempCelsius, config.unit.useFahrenheit)));
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
  controlPage.replace(
      "_TEMP_", String(convertCelsiusToLocalUnit(hp.getTemperature(), config.unit.useFahrenheit)));

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

  String metrics = FPSTR(html_metrics);

  const HeatpumpSettings currentSettings(hp.getSettings());
  const heatpumpStatus currentStatus = hp.getStatus();

  const String hppower = currentSettings.power == "ON" ? "1" : "0";

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

  metrics.replace("_UNIT_NAME_", config.network.hostname);
  metrics.replace("_VERSION_", BUILD_DATE);
  metrics.replace("_GIT_HASH_", COMMIT_HASH);
  metrics.replace("_POWER_", hppower);
  metrics.replace("_ROOMTEMP_", String(currentStatus.roomTemperature));
  metrics.replace("_TEMP_", String(currentSettings.temperature));
  metrics.replace("_FAN_", hpfan);
  metrics.replace("_VANE_", hpvane);
  metrics.replace("_WIDEVANE_", hpwidevane);
  metrics.replace("_MODE_", hpmode);
  metrics.replace("_OPER_", String(currentStatus.operating ? 1 : 0));
  metrics.replace("_COMPFREQ_", String(currentStatus.compressorFrequency));
  server.send(HttpStatusCodes::httpOk, F("text/plain"), metrics);
}

void handleMetricsJson() {
  LOG(F("handleMetricsJson()"));

  JsonDocument doc;
  doc[F("hostname")] = config.network.hostname;
  doc[F("version")] = BUILD_DATE;
  doc[F("git_hash")] = COMMIT_HASH;

  // auto mallocStats = mallinfo();
  // doc[F("memory")] = JsonObject();
  // doc[F("memory")][F("free")] = mallocStats.fordblks;
  // doc[F("memory")][F("used")] = mallocStats.uordblks;
  // doc[F("memory")][F("percent")] = mallocStats.uordblks / (mallocStats.uordblks +
  // mallocStats.fordblks);

  auto heatpump = doc[F("heatpump")].to<JsonObject>();
  heatpump[F("connected")] = hp.isConnected();
  if (hp.isConnected()) {
    const heatpumpStatus currentStatus = hp.getStatus();
    auto status = heatpump[F("status")].to<JsonObject>();
    status[F("operating")] = currentStatus.operating;
    status[F("roomTemperature")] = currentStatus.roomTemperature;
    status[F("roomTemperature_F")] = convertCelsiusToLocalUnit(currentStatus.roomTemperature, true);
    status[F("compressorFrequency")] = currentStatus.compressorFrequency;

    const HeatpumpSettings currentSettings(hp.getSettings());
    auto settings = heatpump[F("settings")].to<JsonObject>();
    settings[F("power")] = currentSettings.power;
    settings[F("temperature")] = currentSettings.temperature;
    settings[F("temperature_F")] = convertCelsiusToLocalUnit(currentSettings.temperature, true);
    settings[F("fan")] = currentSettings.fan;
    settings[F("vane")] = currentSettings.vane;
    settings[F("wideVane")] = currentSettings.wideVane;
    settings[F("mode")] = currentSettings.mode;
    settings[F("iSee")] = currentSettings.iSee;
    settings[F("connected")] = currentSettings.connected;
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
  // NOLINTNEXTLINE(misc-const-correctness)
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
      // using std::round to maintain existing behavior, though I'm not sure it's correct
      newSettings.temperature = convertLocalUnitToCelsius(std::round(server.arg("TEMP").toFloat()),
                                                          config.unit.useFahrenheit);
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
  rootInfo["temperature"] =
      convertCelsiusToLocalUnit(currentSettings.temperature, config.unit.useFahrenheit);
  rootInfo["fan"] = currentSettings.fan;
  rootInfo["vane"] = currentSettings.vane;
  rootInfo["wideVane"] = currentSettings.wideVane;
  rootInfo["mode"] = hpGetMode(currentSettings);
}

void hpSettingsChanged() {
  // send room temp, operating info and all information
  readHeatPumpSettings();

  String mqttOutput;  // NOLINT(misc-const-correctness)
  serializeJson(rootInfo, mqttOutput);

  if (!mqtt_client.publish(config.mqtt.ha_settings_topic().c_str(), mqttOutput.c_str(), true)) {
    LOG(F("Failed to publish hp settings"));
  }

  hpStatusChanged(hp.getStatus());
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

String hpGetAction(heatpumpStatus hpStatus, const HeatpumpSettings &hpSettings) {
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
        convertCelsiusToLocalUnit(newStatus.roomTemperature, config.unit.useFahrenheit);
    rootInfo["temperature"] =
        convertCelsiusToLocalUnit(currentSettings.temperature, config.unit.useFahrenheit);
    rootInfo["fan"] = currentSettings.fan;
    rootInfo["vane"] = currentSettings.vane;
    rootInfo["wideVane"] = currentSettings.wideVane;
    rootInfo["mode"] = hpGetMode(currentSettings);
    rootInfo["action"] = hpGetAction(newStatus, currentSettings);
    rootInfo["compressorFrequency"] = newStatus.compressorFrequency;
    String mqttOutput;  // NOLINT(misc-const-correctness)
    serializeJson(rootInfo, mqttOutput);

    if (!mqtt_client.publish_P(config.mqtt.ha_state_topic().c_str(), mqttOutput.c_str(), false)) {
      LOG(F("Failed to publish hp status change"));
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

// NOLINTNEXTLINE(readability-non-const-parameter)
void hpPacketDebug(byte *packet, unsigned int length, char *packetDirection) {
  // packetDirection should have been declared as const char *, but since hpPacketDebug is a
  // callback function, it can't be.  So we'll just have to be careful not to modify it.
  if (config.other.dumpPacketsToMqtt) {
    String message;  // NOLINT(misc-const-correctness)
    for (unsigned int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0";  // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    JsonDocument root;  // NOLINT(misc-const-correctness)

    root[packetDirection] = message;
    String mqttOutput;  // NOLINT(misc-const-correctness)
    serializeJson(root, mqttOutput);
    if (!mqtt_client.publish(config.mqtt.ha_debug_pckts_topic().c_str(), mqttOutput.c_str())) {
      mqtt_client.publish(config.mqtt.ha_debug_logs_topic().c_str(),
                          "Failed to publish to heatpump/debug topic");
    }
  }
}

// Used to send a dummy packet in state topic to validate action in HA interface
// HA change GUI appareance before having a valid state from the unit
void hpSendLocalState() {
  String mqttOutput;  // NOLINT(misc-const-correctness)
  serializeJson(rootInfo, mqttOutput);
  mqtt_client.publish(config.mqtt.ha_debug_pckts_topic().c_str(), mqttOutput.c_str(), false);
  if (!mqtt_client.publish_P(config.mqtt.ha_state_topic().c_str(), mqttOutput.c_str(), false)) {
    LOG(F("Failed to publish dummy hp status change"));
  }

  // Restart counter for waiting enought time for the unit to update before
  // sending a state packet
  lastTempSend = millis();
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
    remoteTempActive = true;    // Remote temp has been pushed.
    lastRemoteTemp = millis();  // Note time
    hp.setRemoteTemperature(convertLocalUnitToCelsius(temperature, config.unit.useFahrenheit));
  }
}

void onSetWideVane(const char *message) {
  rootInfo["wideVane"] = String(message);
  hpSendLocalState();
  hp.setWideVaneSetting(message);
}

void onSetVane(const char *message) {
  rootInfo["vane"] = String(message);
  hpSendLocalState();
  hp.setVaneSetting(message);
}

void onSetFan(const char *message) {
  rootInfo["fan"] = String(message);
  hpSendLocalState();
  hp.setFanSpeed(message);
}

void onSetTemp(const char *message) {
  const float temperature = strtof(message, NULL);
  const float minTemp = config.unit.minTempCelsius;
  const float maxTemp = config.unit.maxTempCelsius;

  float temperature_c = convertLocalUnitToCelsius(temperature, config.unit.useFahrenheit);
  if (temperature_c < minTemp || temperature_c > maxTemp) {
    temperature_c = (minTemp + maxTemp) / 2.0F;
    rootInfo["temperature"] = convertCelsiusToLocalUnit(temperature_c, config.unit.useFahrenheit);
  } else {
    rootInfo["temperature"] = temperature;
  }
  hpSendLocalState();
  hp.setTemperature(temperature_c);
}

void onSetMode(const char *message) {
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
      // NOLINTNEXTLINE(bugprone-branch-clone) why is the next branch getting flagged as a clone?
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
}

void haConfig() {
  // send HA config packet
  // setup HA payload device
  JsonDocument haConfig;  // NOLINT(misc-const-correctness)

  haConfig["name"] = config.network.hostname;
  haConfig["unique_id"] = getId();

  JsonArray haConfigModes = haConfig["modes"].to<JsonArray>();
  haConfigModes.add("heat_cool");  // native AUTO mode
  haConfigModes.add("cool");
  haConfigModes.add("dry");
  if (config.unit.supportHeatMode) {
    haConfigModes.add("heat");
  }
  haConfigModes.add("fan_only");  // native FAN mode
  haConfigModes.add("off");

  haConfig["mode_cmd_t"] = config.mqtt.ha_mode_set_topic();
  haConfig["mode_stat_t"] = config.mqtt.ha_state_topic();
  haConfig["mode_stat_tpl"] =
      F("{{ value_json.mode if (value_json is defined and value_json.mode is "
        "defined and value_json.mode|length) "
        "else 'off' }}");  // Set default value for fix "Could not parse data
                           // for HA"
  haConfig["temp_cmd_t"] = config.mqtt.ha_temp_set_topic();
  haConfig["temp_stat_t"] = config.mqtt.ha_state_topic();
  haConfig["avty_t"] =
      config.mqtt.ha_availability_topic();              // MQTT last will (status) messages topic
  haConfig["pl_not_avail"] = mqtt_payload_unavailable;  // MQTT offline message payload
  haConfig["pl_avail"] = mqtt_payload_available;        // MQTT online message payload
  // Set default value for fix "Could not parse data for HA"
  // NOLINTNEXTLINE(misc-const-correctness)
  String temp_stat_tpl_str =
      F("{% if (value_json is defined and value_json.temperature is defined) "
        "%}{% if (value_json.temperature|int >= ");
  temp_stat_tpl_str +=
      String(convertCelsiusToLocalUnit(config.unit.minTempCelsius, config.unit.useFahrenheit)) +
      " and value_json.temperature|int <= ";
  temp_stat_tpl_str +=
      String(convertCelsiusToLocalUnit(config.unit.maxTempCelsius, config.unit.useFahrenheit)) +
      ") %}{{ value_json.temperature }}";
  temp_stat_tpl_str +=
      "{% elif (value_json.temperature|int < " +
      String(convertCelsiusToLocalUnit(config.unit.minTempCelsius, config.unit.useFahrenheit)) +
      ") %}" +
      String(convertCelsiusToLocalUnit(config.unit.minTempCelsius, config.unit.useFahrenheit)) +
      "{% elif (value_json.temperature|int > " +
      String(convertCelsiusToLocalUnit(config.unit.maxTempCelsius, config.unit.useFahrenheit)) +
      ") %}" +
      String(convertCelsiusToLocalUnit(config.unit.maxTempCelsius, config.unit.useFahrenheit)) +
      "{% endif %}{% else %}" + String(convertCelsiusToLocalUnit(22, config.unit.useFahrenheit)) +
      "{% endif %}";
  haConfig["temp_stat_tpl"] = temp_stat_tpl_str;
  haConfig["curr_temp_t"] = config.mqtt.ha_state_topic();
  // NOLINTNEXTLINE(misc-const-correctness)
  const String curr_temp_tpl_str =
      String(F("{{ value_json.roomTemperature if (value_json is defined and "
               "value_json.roomTemperature is defined and "
               "value_json.roomTemperature|int > ")) +
      String(convertCelsiusToLocalUnit(1, config.unit.useFahrenheit)) +
      ") }}";  // Set default value for fix "Could not parse data for HA"
  haConfig["curr_temp_tpl"] = curr_temp_tpl_str;
  haConfig["min_temp"] =
      convertCelsiusToLocalUnit(config.unit.minTempCelsius, config.unit.useFahrenheit);
  haConfig["max_temp"] =
      convertCelsiusToLocalUnit(config.unit.maxTempCelsius, config.unit.useFahrenheit);
  haConfig["temp_step"] = config.unit.tempStep;
  haConfig["temperature_unit"] = config.unit.useFahrenheit ? "F" : "C";

  JsonArray haConfigFan_modes = haConfig["fan_modes"].to<JsonArray>();
  haConfigFan_modes.add("AUTO");
  haConfigFan_modes.add("QUIET");
  haConfigFan_modes.add("1");
  haConfigFan_modes.add("2");
  haConfigFan_modes.add("3");
  haConfigFan_modes.add("4");

  haConfig["fan_mode_cmd_t"] = config.mqtt.ha_fan_set_topic();
  haConfig["fan_mode_stat_t"] = config.mqtt.ha_state_topic();
  haConfig["fan_mode_stat_tpl"] =
      F("{{ value_json.fan if (value_json is defined and value_json.fan is "
        "defined and value_json.fan|length) else "
        "'AUTO' }}");  // Set default value for fix "Could not parse data for HA"

  JsonArray haConfigSwing_modes = haConfig["swing_modes"].to<JsonArray>();
  haConfigSwing_modes.add("AUTO");
  haConfigSwing_modes.add("1");
  haConfigSwing_modes.add("2");
  haConfigSwing_modes.add("3");
  haConfigSwing_modes.add("4");
  haConfigSwing_modes.add("5");
  haConfigSwing_modes.add("SWING");

  haConfig["swing_mode_cmd_t"] = config.mqtt.ha_vane_set_topic();
  haConfig["swing_mode_stat_t"] = config.mqtt.ha_state_topic();
  haConfig["swing_mode_stat_tpl"] =
      F("{{ value_json.vane if (value_json is defined and value_json.vane is "
        "defined and value_json.vane|length) "
        "else 'AUTO' }}");  // Set default value for fix "Could not parse data
                            // for HA"
  haConfig["action_topic"] = config.mqtt.ha_state_topic();
  haConfig["action_template"] =
      F("{{ value_json.action if (value_json is defined and value_json.action "
        "is defined and "
        "value_json.action|length) else 'idle' }}");  // Set default value for
                                                      // fix "Could not parse
                                                      // data for HA"

  JsonObject haConfigDevice = haConfig["device"].to<JsonObject>();

  haConfigDevice["ids"] = config.mqtt.friendlyName;
  haConfigDevice["name"] = config.mqtt.friendlyName;
  haConfigDevice["sw"] = "Mitsubishi2MQTT " + String(BUILD_DATE) + " (" + String(COMMIT_HASH) + ")";
  haConfigDevice["mdl"] = "HVAC MITSUBISHI";
  haConfigDevice["mf"] = "MITSUBISHI ELECTRIC";
  haConfigDevice["configuration_url"] = "http://" + WiFi.localIP().toString();

  // Additional attributes are in the state
  // For now, only compressorFrequency
  haConfig["json_attr_t"] = config.mqtt.ha_state_topic();
  haConfig["json_attr_tpl"] =
      F("{{ {'compressorFrequency': value_json.compressorFrequency if "
        "(value_json is defined "
        "and value_json.compressorFrequency is defined) else '-1' } | tojson }}");

  String mqttOutput;  // NOLINT(misc-const-correctness)
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
        haConfig();
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

// temperature helper functions
float toFahrenheit(float fromCelsius) {
  return round(1.8F * fromCelsius + 32.0F);
}

float toCelsius(float fromFahrenheit) {
  return (fromFahrenheit - 32.0F) / 1.8F;
}

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
  if (config.unit.useFahrenheit) {
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

  if (!captive) {
    // Sync HVAC UNIT
    if (!hp.isConnected()) {
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
    } else {
      hpConnectionRetries = 0;
      hp.sync();
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
        hpStatusChanged(hp.getStatus());
        mqtt_client.loop();
      }
    }
  } else {
    dnsServer.processNextRequest();
  }
}

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
#include <DNSServer.h>     // DNS for captive portal
#include <HeatPump.h>      // SwiCago library: https://github.com/SwiCago/HeatPump
#include <PubSubClient.h>  // MQTT: PubSubClient 2.8.0

#include <map>
#include <temperature.hpp>

#include "HeatpumpSettings.hpp"
#include "HeatpumpStatus.hpp"
#include "filesystem.hpp"
#include "frontend/templates.hpp"
#include "logger.hpp"
#include "main.hpp"
#include "ministache.hpp"
#include "moment.hpp"
#include "timer.hpp"
#include "views/mqtt/strings.hpp"

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
Moment wifi_timeout(Moment::now());

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
const PROGMEM int64_t HP_RETRY_INTERVAL_MS = 1000LL;            // 1 second
const PROGMEM uint32_t HP_MAX_RETRIES =
    10;  // Double the interval between retries up to this many times, then keep
         // retrying forever at that maximum interval.
// Default values give a final retry interval of 1000ms * 2^10, which is 1024
// seconds, about 17 minutes.

// END include the contents of config.h

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
Moment lastMqttStatePacketSend(Moment::never());
Moment lastMqttRetry(Moment::never());
Moment lastHpSync(Moment::never());
unsigned int hpConnectionRetries;
unsigned int hpConnectionTotalRetries;
Moment lastRemoteTemp(Moment::now());

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
    server.on(F("/control"), HTTPMethod::HTTP_GET, handleControlGet);
    server.on(F("/control"), HTTPMethod::HTTP_POST, handleControlPost);
    server.on(F("/setup"), handleSetup);
    server.on(F("/mqtt"), handleMqtt);
    server.on(F("/wifi"), handleWifi);
    server.on(F("/unit"), HTTPMethod::HTTP_GET, handleUnitGet);
    server.on(F("/unit"), HTTPMethod::HTTP_POST, handleUnitPost);
    server.on(F("/status"), handleStatus);
    server.on(F("/others"), handleOthers);
    server.on(F("/metrics"), handleMetrics);
    server.on(F("/metrics.json"), handleMetricsJson);
    server.on(F("/css"), HTTPMethod::HTTP_GET, []() {
      // We always add the git_hash as a query param on the CSS request, so we can
      // use a very long cache expiry here. This makes browsing way faster.
      server.sendHeader(F("Cache-Control"), F("public, max-age=604800, immutable"));
      server.send(200, F("text/css"), statics::css);
    });
    server.onNotFound(handleNotFound);
    if (config.unit.login_password.length() > 0) {
      server.on(F("/login"), HTTPMethod::HTTP_GET, handleLogin);
      server.on(F("/login"), HTTPMethod::HTTP_POST, handleAuth);
      server.on(F("/logout"), HTTPMethod::HTTP_POST, handleLogout);
      // here the list of headers to be recorded, use for authentication
      const char *headerkeys[] = {"Cookie"};
      const size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
      // ask server to track these headers
      server.collectHeaders(headerkeys, headerkeyssize);
    }
    server.on(F("/upgrade"), handleUpgrade);
    server.on(F("/upload"), HTTP_POST, handleUploadDone, handleUploadLoop);

    server.begin();
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
  } else {
    dnsServer.start(DNS_PORT, "*", apIP);
    initCaptivePortal();
  }
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
  server.on(F("/css"), HTTPMethod::HTTP_GET,
            []() { server.send(200, F("text/css"), statics::css); });
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
  wifi_timeout = Moment::now().offset(WIFI_RETRY_INTERVAL_MS);
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
  return (Moment::now() - lastRemoteTemp > CHECK_REMOTE_TEMP_INTERVAL_MS);
}

bool safeModeActive() {
  return config.other.safeMode && remoteTempStale();
}

void renderView(const Ministache &view, JsonDocument &data,
                const std::vector<std::pair<String, String>> &partials = {}) {
  auto header = data[F("header")].to<JsonObject>();
  header[F("hostname")] = config.network.hostname;
  header[F("git_hash")] = COMMIT_HASH;

  auto footer = data[F("footer")].to<JsonObject>();
  footer[F("version")] = BUILD_DATE;
  footer[F("git_hash")] = COMMIT_HASH;

  server.send(HttpStatusCodes::httpOk, F("text/html"), view.render(data, partials));
}

void handleNotFound() {
  LOG(F("handleNotFound()"));
  server.send(HttpStatusCodes::httpNotFound, "text/plain", "Not found.");
}

void handleInitSetup() {
  LOG(F("handleInitSetup()"));

  JsonDocument data;
  data[F("hostname")] = config.network.hostname;
  renderView(Ministache(views::captive::index), data,
             {{"header", partials::header}, {"footer", partials::footer}});
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
    saveWifiConfig(config);
  }
  JsonDocument data;
  data[F("access_point")] = config.network.accessPointSsid;
  data[F("hostname")] = config.network.hostname;
  renderView(Ministache(views::captive::save), data,
             {{"header", partials::header}, {"footer", partials::footer}});
  restartAfterDelay(2000);
}

void handleReboot() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleReboot()"));

  JsonDocument data;
  renderView(Ministache(views::captive::reboot), data,
             {{"header", partials::header}, {"footer", partials::footer}});
  restartAfterDelay(2000);
}

void handleRoot() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleRoot()"));

  if (server.hasArg("REBOOT")) {
    JsonDocument data;
    renderView(Ministache(views::reboot), data,
               {{"header", partials::header},
                {"footer", partials::footer},
                {"countdown", partials::countdown}});
    restartAfterDelay(500);
  } else {
    JsonDocument data;
    data[F("showControl")] = hp.isConnected();
    data[F("showLogout")] = config.unit.login_password.length() > 0;
    renderView(Ministache(views::index), data,
               {{"header", partials::header}, {"footer", partials::footer}});
  }
}

void handleSetup() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleSetup()"));

  if (server.hasArg("RESET")) {
    JsonDocument data;
    data["SSID"] = Config::Network::defaultHostname();
    renderView(Ministache(views::reset), data,
               {{"header", partials::header},
                {"footer", partials::footer},
                {"countdown", partials::countdown}});
    FileSystem::format();
    restartAfterDelay(500);
  } else {
    JsonDocument data;
    renderView(Ministache(views::setup), data,
               {{"header", partials::header}, {"footer", partials::footer}});
  }
}

void rebootAndSendPage() {
  JsonDocument data;
  data["saving"] = true;
  renderView(Ministache(views::reboot), data,
             {{"header", partials::header},
              {"footer", partials::footer},
              {"countdown", partials::countdown}});
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
    friendlyName[F("label")] = views::mqtt::friendlyNameLabel;
    friendlyName[F("value")] = config.mqtt.friendlyName;
    friendlyName[F("param")] = F("fn");

    auto mqttServer = data[F("server")].to<JsonObject>();
    mqttServer[F("label")] = views::mqtt::hostLabel;
    mqttServer[F("value")] = config.mqtt.server;
    mqttServer[F("param")] = F("mh");

    auto port = data[F("port")].to<JsonObject>();
    port[F("value")] = config.mqtt.port;

    auto password = data[F("password")].to<JsonObject>();
    password[F("value")] = config.mqtt.password;

    auto username = data[F("user")].to<JsonObject>();
    username[F("label")] = views::mqtt::userLabel;
    username[F("value")] = config.mqtt.username;
    username[F("param")] = F("mu");
    username[F("placeholder")] = F("mqtt_user");

    auto topic = data[F("topic")].to<JsonObject>();
    topic[F("label")] = views::mqtt::topicLabel;
    topic[F("value")] = config.mqtt.rootTopic;
    topic[F("param")] = F("mt");
    topic[F("placeholder")] = F("topic");

    renderView(Ministache(views::mqtt::index), data,
               {{"mqttTextField", views::mqtt::textField},
                {"header", partials::header},
                {"footer", partials::footer}});
  }
}

void handleUnitGet() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleUnitGet()"));

  JsonDocument data;
  data[F("min_temp")] = config.unit.minTemp.toString(config.unit.tempUnit);
  data[F("max_temp")] = config.unit.maxTemp.toString(config.unit.tempUnit);
  data[F("temp_step")] = config.unit.tempStep;
  data[F("temp_unit_c")] = config.unit.tempUnit == TempUnit::C;
  data[F("mode_selection_all")] = config.unit.supportHeatMode;
  data[F("login_password")] = config.unit.login_password;
  renderView(Ministache(views::unit), data,
             {{"header", partials::header}, {"footer", partials::footer}});
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
  if (server.hasArg("lpw")) {
    // an empty value in "lpw" means we clear the password
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
    saveWifiConfig(config);
    rebootAndSendPage();
  } else {
    JsonDocument data;
    data[F("access_point")] = config.network.accessPointSsid;
    data[F("hostname")] = config.network.hostname;
    data[F("password")] = config.network.accessPointPassword;
    renderView(Ministache(views::wifi), data,
               {{"header", partials::header}, {"footer", partials::footer}});
  }
}

void handleStatus() {
  if (!checkLogin()) {
    return;
  }
  LOG(F("handleStatus()"));

  JsonDocument data;
  const auto uptime = Moment::now().get();
  auto uptimeData = data[F("uptime")].to<JsonObject>();
  if (uptime.years > 0) {
    uptimeData[F("years")] = uptime.years;
  }
  uptimeData[F("days")] = uptime.days;
  uptimeData[F("hours")] = uptime.hours;
  uptimeData[F("minutes")] = uptime.minutes;
  uptimeData[F("seconds")] = String(
      (static_cast<float>(uptime.seconds) * 1000.f + static_cast<float>(uptime.milliseconds)) /
          1000.f,
      3);

  data[F("hvac_connected")] = (Serial) and hp.isConnected();
  data[F("hvac_retries")] = hpConnectionTotalRetries;
  data[F("mqtt_connected")] = mqtt_client.connected();
  data[F("mqtt_error_code")] = mqtt_client.state();
  data[F("wifi_access_point")] = WiFi.SSID();
  data[F("wifi_signal_strength")] = WiFi.RSSI();

  if (server.hasArg("mrconn")) {
    mqttConnect();
  }

  renderView(Ministache(views::status), data,
             {{"header", partials::header}, {"footer", partials::footer}});
}

void handleControlGet() {
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

  LOG(F("handleControlGet()"));

  HeatpumpSettings settings(hp.getSettings());
  JsonDocument data;
  data[F("min_temp")] = config.unit.minTemp.toString(config.unit.tempUnit);
  data[F("max_temp")] = config.unit.maxTemp.toString(config.unit.tempUnit);
  data[F("current_temp")] =
      Temperature(hp.getRoomTemperature(), TempUnit::C).toString(config.unit.tempUnit, 0.1f);
  data[F("target_temp")] =
      Temperature(hp.getTemperature(), TempUnit::C).toString(config.unit.tempUnit);
  data[F("temp_step")] = config.unit.tempStep;
  data[F("temp_unit")] = config.unit.tempUnit == TempUnit::C ? "C" : "F";
  data[F("supportHeatMode")] = config.unit.supportHeatMode;
  data[F("power")] = settings.power;

  const auto mode = data[F("mode")].to<JsonObject>();
  mode[F("cool")] = settings.mode == "COOL";
  mode[F("heat")] = settings.mode == "HEAT";
  mode[F("dry")] = settings.mode == "DRY";
  mode[F("fan")] = settings.mode == "FAN";
  mode[F("auto")] = settings.mode == "AUTO";

  const auto fan = data[F("fan")].to<JsonObject>();
  fan[F("auto")] = settings.fan == "AUTO";
  fan[F("quiet")] = settings.fan == "QUIET";
  fan[F("1")] = settings.fan == "1";
  fan[F("2")] = settings.fan == "2";
  fan[F("3")] = settings.fan == "3";
  fan[F("4")] = settings.fan == "4";

  const auto vane = data[F("vane")].to<JsonObject>();
  vane[F("auto")] = settings.vane == "AUTO";
  vane[F("1")] = settings.vane == "1";
  vane[F("2")] = settings.vane == "2";
  vane[F("3")] = settings.vane == "3";
  vane[F("4")] = settings.vane == "4";
  vane[F("5")] = settings.vane == "5";
  vane[F("swing")] = settings.vane == "SWING";

  const auto widevane = data[F("widevane")].to<JsonObject>();
  widevane[F("swing")] = settings.wideVane == "SWING";
  widevane[F("1")] = settings.wideVane == "<<";
  widevane[F("2")] = settings.wideVane == "<";
  widevane[F("3")] = settings.wideVane == "|";
  widevane[F("4")] = settings.wideVane == ">";
  widevane[F("5")] = settings.wideVane == ">>";
  widevane[F("6")] = settings.wideVane == "<>";

  // settings = change_states(settings);
  // String controlPage = FPSTR(html_page_control);
  // String headerContent = FPSTR(html_common_header);
  // String footerContent = FPSTR(html_common_footer);
  // headerContent.replace("_UNIT_NAME_", config.network.hostname);
  // footerContent.replace("_VERSION_", BUILD_DATE);
  // footerContent.replace("_GIT_HASH_", COMMIT_HASH);
  // controlPage.replace("_TXT_BACK_", FPSTR(txt_back));
  // controlPage.replace("_UNIT_NAME_", config.network.hostname);
  // controlPage.replace("_RATE_", "60");
  // controlPage.replace(
  //     "_ROOMTEMP_",
  //     Temperature(hp.getRoomTemperature(), TempUnit::C).toString(config.unit.tempUnit, 0.1f));
  // controlPage.replace("_USE_FAHRENHEIT_", String(config.unit.tempUnit == TempUnit::F ? 1 : 0));
  // controlPage.replace("_TEMP_SCALE_", getTemperatureScale());
  // controlPage.replace("_HEAT_MODE_SUPPORT_", String(config.unit.supportHeatMode ? 1 : 0));
  // controlPage.replace(F("_MIN_TEMP_"), config.unit.minTemp.toString(config.unit.tempUnit));
  // controlPage.replace(F("_MAX_TEMP_"), config.unit.maxTemp.toString(config.unit.tempUnit));
  // controlPage.replace(F("_TEMP_STEP_"), String(config.unit.tempStep));
  // controlPage.replace("_TXT_CTRL_CTEMP_", FPSTR(txt_ctrl_ctemp));
  // controlPage.replace("_TXT_CTRL_TEMP_", FPSTR(txt_ctrl_temp));
  // controlPage.replace("_TXT_CTRL_TITLE_", FPSTR(txt_ctrl_title));
  // controlPage.replace("_TXT_CTRL_POWER_", FPSTR(txt_ctrl_power));
  // controlPage.replace("_TXT_CTRL_MODE_", FPSTR(txt_ctrl_mode));
  // controlPage.replace("_TXT_CTRL_FAN_", FPSTR(txt_ctrl_fan));
  // controlPage.replace("_TXT_CTRL_VANE_", FPSTR(txt_ctrl_vane));
  // controlPage.replace("_TXT_CTRL_WVANE_", FPSTR(txt_ctrl_wvane));
  // controlPage.replace("_TXT_F_ON_", FPSTR(txt_f_on));
  // controlPage.replace("_TXT_F_OFF_", FPSTR(txt_f_off));
  // controlPage.replace("_TXT_F_AUTO_", FPSTR(txt_f_auto));
  // controlPage.replace("_TXT_F_HEAT_", FPSTR(txt_f_heat));
  // controlPage.replace("_TXT_F_DRY_", FPSTR(txt_f_dry));
  // controlPage.replace("_TXT_F_COOL_", FPSTR(txt_f_cool));
  // controlPage.replace("_TXT_F_FAN_", FPSTR(txt_f_fan));
  // controlPage.replace("_TXT_F_QUIET_", FPSTR(txt_f_quiet));
  // controlPage.replace("_TXT_F_SPEED_", FPSTR(txt_f_speed));
  // controlPage.replace("_TXT_F_SWING_", FPSTR(txt_f_swing));
  // controlPage.replace("_TXT_F_POS_", FPSTR(txt_f_pos));

  // if (settings.power == "ON") {
  //   controlPage.replace("_POWER_ON_", "selected");
  // } else if (settings.power == "OFF") {
  //   controlPage.replace("_POWER_OFF_", "selected");
  // }

  // if (settings.mode == "HEAT") {
  //   controlPage.replace("_MODE_H_", "selected");
  // } else if (settings.mode == "DRY") {
  //   controlPage.replace("_MODE_D_", "selected");
  // } else if (settings.mode == "COOL") {
  //   controlPage.replace("_MODE_C_", "selected");
  // } else if (settings.mode == "FAN") {
  //   controlPage.replace("_MODE_F_", "selected");
  // } else if (settings.mode == "AUTO") {
  //   controlPage.replace("_MODE_A_", "selected");
  // }

  // if (settings.fan == "AUTO") {
  //   controlPage.replace("_FAN_A_", "selected");
  // } else if (settings.fan == "QUIET") {
  //   controlPage.replace("_FAN_Q_", "selected");
  // } else if (settings.fan == "1") {
  //   controlPage.replace("_FAN_1_", "selected");
  // } else if (settings.fan == "2") {
  //   controlPage.replace("_FAN_2_", "selected");
  // } else if (settings.fan == "3") {
  //   controlPage.replace("_FAN_3_", "selected");
  // } else if (settings.fan == "4") {
  //   controlPage.replace("_FAN_4_", "selected");
  // }

  // controlPage.replace("_VANE_V_", settings.vane);
  // if (settings.vane == "AUTO") {
  //   controlPage.replace("_VANE_A_", "selected");
  // } else if (settings.vane == "1") {
  //   controlPage.replace("_VANE_1_", "selected");
  // } else if (settings.vane == "2") {
  //   controlPage.replace("_VANE_2_", "selected");
  // } else if (settings.vane == "3") {
  //   controlPage.replace("_VANE_3_", "selected");
  // } else if (settings.vane == "4") {
  //   controlPage.replace("_VANE_4_", "selected");
  // } else if (settings.vane == "5") {
  //   controlPage.replace("_VANE_5_", "selected");
  // } else if (settings.vane == "SWING") {
  //   controlPage.replace("_VANE_S_", "selected");
  // }

  // controlPage.replace("_WIDEVANE_V_", settings.wideVane);
  // if (settings.wideVane == "<<") {
  //   controlPage.replace("_WVANE_1_", "selected");
  // } else if (settings.wideVane == "<") {
  //   controlPage.replace("_WVANE_2_", "selected");
  // } else if (settings.wideVane == "|") {
  //   controlPage.replace("_WVANE_3_", "selected");
  // } else if (settings.wideVane == ">") {
  //   controlPage.replace("_WVANE_4_", "selected");
  // } else if (settings.wideVane == ">>") {
  //   controlPage.replace("_WVANE_5_", "selected");
  // } else if (settings.wideVane == "<>") {
  //   controlPage.replace("_WVANE_6_", "selected");
  // } else if (settings.wideVane == "SWING") {
  //   controlPage.replace("_WVANE_S_", "selected");
  // }
  // controlPage.replace("_TEMP_",
  //                     Temperature(hp.getTemperature(),
  //                     TempUnit::C).toString(config.unit.tempUnit));

  // // We need to send the page content in chunks to overcome
  // // a limitation on the maximum size we can send at one
  // // time (approx 6k).
  // server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  // server.send(HttpStatusCodes::httpOk, "text/html", headerContent);
  // server.sendContent(controlPage);
  // server.sendContent(footerContent);
  // // Signal the end of the content
  // server.sendContent("");
  renderView(Ministache(views::control), data,
             {{"header", partials::header}, {"footer", partials::footer}});
}

void handleControlPost() {
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

  LOG(F("handleControlPost()"));

  // Apply changes and try to flush them
  HeatpumpSettings settings(hp.getSettings());
  settings = change_states(settings);
  hp.sync();

  server.send(httpOk);
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

  Ministache metricsTemplate(views::metrics);
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

// Render the login form
void handleLogin() {
  LOG(F("handleLogin()"));

  // Don't render the login form if login is not required; just redirect back to the home page
  if (is_authenticated() || config.unit.login_password.length() == 0) {
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Location", "/");
    server.send(httpFound, F("text/plain"), "Redirect to home page");
  }

  JsonDocument data;
  data[F("authError")] = server.hasArg("authError");
  renderView(Ministache(views::login), data,
             {{"header", partials::header}, {"footer", partials::footer}});
}

// Handle the auth via POST
// If the password is correct, set the session cookie and redirect to the home page
// If the password is incorrect, redirect back to the login page with an error message
void handleAuth() {
  LOG(F("handleAuth()"));

  if (server.hasArg("PASSWORD") && server.arg("PASSWORD") == config.unit.login_password) {
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "M2MSESSIONID=1");
    server.sendHeader("Location", "/");
    server.send(httpFound, F("text/plain"), "Redirect to home page");
  } else {
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "M2MSESSIONID=0");
    server.sendHeader("Location", "/login?authError");
    server.send(httpFound, F("text/plain"), "Redirect to login");
  }
}

// Handle logout via POST
void handleLogout() {
  LOG(F("handleLogout()"));

  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Set-Cookie", "M2MSESSIONID=0");
  server.sendHeader("Location", "/login");
  server.send(httpFound, F("text/plain"), "Redirect to login");
}

void handleUpgrade() {
  if (!checkLogin()) {
    return;
  }

  LOG(F("handleUpgrade()"));

  uploaderror = UploadError::noError;
  JsonDocument data;
  renderView(Ministache(views::upgrade), data,
             {{"header", partials::header}, {"footer", partials::footer}});
}

void handleUploadDone() {
  LOG(F("handleUploadDone()"));

  // Serial.printl(PSTR("HTTP: Firmware upload done"));
  bool restartflag = false;
  JsonDocument data;
  if (uploaderror != UploadError::noError) {
    auto error = data[F("error")].to<JsonObject>();
    error[F("errorCode")] = uploaderror;
    if (uploaderror == UploadError::noFileSelected) {
      error[F("noFileSelected")] = true;
    } else if (uploaderror == UploadError::fileTooLarge) {
      error[F("fileTooLarge")] = true;
    } else if (uploaderror == UploadError::fileMagicHeaderIncorrect) {
      error[F("fileMagicHeaderIncorrect")] = true;
    } else if (uploaderror == UploadError::fileTooBigForDeviceFlash) {
      error[F("fileTooBigForDeviceFlash")] = true;
    } else if (uploaderror == UploadError::fileUploadBufferMiscompare) {
      error[F("fileUploadBufferMiscompare")] = true;
    } else if (uploaderror == UploadError::fileUploadFailed) {
      error[F("fileUploadFailed")] = true;
    } else if (uploaderror == UploadError::fileUploadAborted) {
      error[F("fileUploadAborted")] = true;
    } else {
      error[F("genericError")] = true;
    }
    if (Update.hasError()) {
      error[F("updaterErrorCode")] = Update.getError();
    }
  } else {
    data["success"] = true;
    restartflag = true;
  }

  renderView(Ministache(views::upload), data,
             {{"header", partials::header},
              {"footer", partials::footer},
              {"countdown", partials::countdown}});

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
    // TODO(floatplane): should we do this? I feel like we should log to MQTT instead
    if (mqtt_client.state() == MQTT_CONNECTED) {
      mqtt_client.disconnect();
      lastMqttRetry = Moment::now();
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
  if (Moment::now() - lastMqttStatePacketSend > interval) {
    String mqttOutput;
    serializeJson(getHeatPumpStatusJson(), mqttOutput);

    if (!mqtt_client.publish_P(config.mqtt.ha_state_topic().c_str(), mqttOutput.c_str(), false)) {
      LOG(F("Failed to publish hp status change"));
    }

    lastMqttStatePacketSend = Moment::now();
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
  lastMqttStatePacketSend = Moment::now();
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
    remoteTempActive = true;         // Remote temp has been pushed.
    lastRemoteTemp = Moment::now();  // Note time
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
        lastMqttRetry = Moment::now();
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
  wifi_timeout = Moment::now().offset(connectTimeoutMs);
  while (WiFi.status() != WL_CONNECTED && Moment::now() < wifi_timeout) {
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
    server.send(httpFound, F("text/plain"), "Login required");
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

  // reset board to attempt to connect to wifi again if in ap mode or wifi
  // dropped out and time limit passed
  if (WiFi.getMode() == WIFI_STA and
      WiFi.status() == WL_CONNECTED) {  // NOLINT(bugprone-branch-clone)
    wifi_timeout = Moment::now().offset(WIFI_RETRY_INTERVAL_MS);
  } else if (config.network.configured() and Moment::now() > wifi_timeout) {
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
    const int64_t durationNextSync = (1LL << hpConnectionRetries) * HP_RETRY_INTERVAL_MS;
    if (Moment::now() - lastHpSync > durationNextSync) {
      lastHpSync = Moment::now();
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
      if (Moment::now() - lastMqttRetry > MQTT_RETRY_INTERVAL_MS) {
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

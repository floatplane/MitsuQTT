#include "ota.hpp"

#include <ArduinoOTA.h>

#include "logger.hpp"

// Enable OTA only when connected as a client.
void initOTA(const String& hostname, const String& otaPassword) {
  LOG(F("Start OTA Listener"));
  ArduinoOTA.setHostname(hostname.c_str());
  if (otaPassword.length() > 0) {
    ArduinoOTA.setPassword(otaPassword.c_str());
  }
  ArduinoOTA.onStart([]() { LOG("Start"); });
  ArduinoOTA.onEnd([]() { LOG("\nEnd"); });
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

void processOTALoop() {
  ArduinoOTA.handle();
}
#pragma once

#include <ArduinoJson.h>
#include <FS.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

class FileSystem {
 public:
  static void init() {
    // Mount SPIFFS filesystem
    if (!SPIFFS.begin()) {
      SPIFFS.format();
    }
  }

  static JsonDocument loadJSON(const char *filename) {
    if (!SPIFFS.exists(filename)) {
      return JsonDocument();
    }

    File file = SPIFFS.open(filename, "r");
    if (!file) {
      return JsonDocument();
    }

    JsonDocument doc;
    deserializeJson(doc, file);
    file.close();
    return doc;
  }

  static void saveJSON(const char *filename, JsonDocument &doc) {
    File configFile = SPIFFS.open(filename, "w");
    if (!configFile) {
      // Serial.println(F("Failed to open config file for writing"));
      return;
    }
    serializeJson(doc, configFile);
    configFile.flush();
    configFile.close();
  }

  static void deleteFile(const char *filename) {
    if (SPIFFS.exists(filename)) {
      SPIFFS.remove(filename);
    }
  }

  static void format() {
    SPIFFS.format();
  }
};

#pragma GCC diagnostic pop

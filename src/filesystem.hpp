/*
  MitsuQTT Copyright (c) 2024 floatplane

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU
  Lesser General Public License as published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version. This library is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with this library;
  if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

#pragma once

#include <ArduinoJson.h>
#include <FS.h>

#ifdef USE_SPIFFS
#ifdef ESP32
#include <SPIFFS.h>
#endif
#define FILESYSTEM SPIFFS
#else
#include <LittleFS.h>
#define FILESYSTEM LittleFS
#endif

#ifdef USE_SPIFFS
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

class FileSystem {
 public:
  static void init() {
    // Mount filesystem
    if (!FILESYSTEM.begin()) {
      FILESYSTEM.format();
    }
  }

  static JsonDocument loadJSON(const char *filename) {
    if (!FILESYSTEM.exists(filename)) {
      return JsonDocument();
    }

    File file = FILESYSTEM.open(filename, "r");
    if (!file) {
      return JsonDocument();
    }

    JsonDocument doc;
    deserializeJson(doc, file);
    file.close();
    return doc;
  }

  static void saveJSON(const char *filename, JsonDocument &doc) {
    File configFile = FILESYSTEM.open(filename, "w");
    if (!configFile) {
      // Serial.println(F("Failed to open config file for writing"));
      return;
    }
    serializeJson(doc, configFile);
    configFile.flush();
    configFile.close();
  }

  static void deleteFile(const char *filename) {
    if (FILESYSTEM.exists(filename)) {
      FILESYSTEM.remove(filename);
    }
  }

  static void format() {
    FILESYSTEM.format();
  }
};

#ifdef USE_SPIFFS
#pragma GCC diagnostic pop
#endif
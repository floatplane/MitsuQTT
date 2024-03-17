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

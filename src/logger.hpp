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

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <Arduino.h>

class PubSubClient;

namespace Logger {
void initialize();
void enableMqttLogging(PubSubClient& mqttClient, const char* mqttTopic);
void disableMqttLogging();
void log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void log(const __FlashStringHelper* format, ...);
inline void log(const String& message) {
  return log(message.c_str());
}
}  // namespace Logger

#ifdef ENABLE_LOGGING
#define LOG Logger::log
#else
#define LOG(...)
#endif

#endif  // LOGGER_HPP

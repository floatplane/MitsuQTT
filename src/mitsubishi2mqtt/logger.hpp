#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <Arduino.h>

namespace Logger {
void initialize();
void log(const char* message, ...) __attribute__((format(printf, 1, 2)));
void log(const __FlashStringHelper* message, ...);
inline void log(const String& message) { log(message.c_str()); }
}  // namespace Logger

#endif  // LOGGER_HPP

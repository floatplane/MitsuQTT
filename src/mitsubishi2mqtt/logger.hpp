#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <Arduino.h>

namespace Logger {
void initialize();
void log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void log(const __FlashStringHelper* format, ...);
inline void log(const String& message) { return log(message.c_str()); }
}  // namespace Logger

#endif  // LOGGER_HPP

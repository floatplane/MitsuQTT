#include "logger.hpp"

#include <ESPAsyncWebServer.h>

static AsyncWebServer server(81);
static AsyncWebSocket webSocket("/log");

const auto LOG_BUFFER_SIZE = 256;

void onEvent(const AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             [[maybe_unused]] void *arg, [[maybe_unused]] uint8_t *data,
             [[maybe_unused]] size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->ping();
  } else if (type == WS_EVT_PONG) {
    Logger::log(F("log client connected ws://%s client ID:%u\n"), server->url(), client->id());
  }
}

// cppcheck-suppress unusedFunction
void Logger::initialize() {
  webSocket.onEvent(onEvent);
  server.addHandler(&webSocket);
  server.begin();
}

void Logger::log(const char *format, ...) {
  char logBuffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(logBuffer, LOG_BUFFER_SIZE, format, args);
  webSocket.printfAll(logBuffer);
  va_end(args);
}

void Logger::log(const __FlashStringHelper *format, ...) {
  char logBuffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  // See https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html for more about PGM_P, PSTR,
  // F, FlashStringHelper, etc.
  vsnprintf_P(logBuffer, LOG_BUFFER_SIZE, reinterpret_cast<PGM_P>(format), args);
  webSocket.printfAll(logBuffer);
  va_end(args);
}
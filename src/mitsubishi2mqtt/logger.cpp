#include "logger.hpp"

#include <ESPAsyncWebServer.h>

static AsyncWebServer server(81);
static AsyncWebSocket ws("/log");

const auto LOG_BUFFER_SIZE = 256;

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg,
             uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->ping();
  } else if (type == WS_EVT_PONG) {
    Logger::log(F("log client connected ws[%s][%u]\n"), server->url(), client->id());
  }
}

void Logger::initialize() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void Logger::log(const char *format, ...) {
  char logBuffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(logBuffer, LOG_BUFFER_SIZE, format, args);
  ws.printfAll(logBuffer);
  va_end(args);
}

void Logger::log(const __FlashStringHelper *formatP, ...) {
  char logBuffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, formatP);
  vsnprintf(logBuffer, LOG_BUFFER_SIZE, String(formatP).c_str(), args);
  ws.printfAll(logBuffer);
  va_end(args);
}
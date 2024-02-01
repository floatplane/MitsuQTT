#include "logger.hpp"

#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>

#ifdef ENABLE_WEBSOCKET_LOGGING
static AsyncWebServer server(81);
static AsyncWebSocket webSocket("/log");

void onEvent(const AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             [[maybe_unused]] void *arg, [[maybe_unused]] uint8_t *data,
             [[maybe_unused]] size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->ping();
  } else if (type == WS_EVT_PONG) {
    Logger::log(F("log client connected ws://%s client ID:%u\n"), server->url(), client->id());
  }
}
#endif

const auto LOG_BUFFER_SIZE = 256;

void Logger::initialize() {
#ifdef ENABLE_WEBSOCKET_LOGGING
  webSocket.onEvent(onEvent);
  server.addHandler(&webSocket);
  server.begin();
#endif
}

static PubSubClient *mqttClient = nullptr;
static const char *mqttTopic = nullptr;
void Logger::enableMqttLogging(PubSubClient &mqttClient, const char *mqttTopic) {
  ::mqttClient = &mqttClient;
  ::mqttTopic = mqttTopic;
}

void Logger::disableMqttLogging() {
  ::mqttClient = nullptr;
  ::mqttTopic = nullptr;
}

void Logger::log(const char *format, ...) {
#ifndef ENABLE_WEBSOCKET_LOGGING
  if (mqttClient == nullptr) {
    // early out if no log output is enabled
    return;
  }
#endif
  char logBuffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(logBuffer, LOG_BUFFER_SIZE, format, args);
#ifdef ENABLE_WEBSOCKET_LOGGING
  webSocket.printfAll(logBuffer);
#endif
  if (mqttClient != nullptr) {
    mqttClient->publish(mqttTopic, logBuffer);
  }
  va_end(args);
}

void Logger::log(const __FlashStringHelper *format, ...) {
#ifndef ENABLE_WEBSOCKET_LOGGING
  if (mqttClient == nullptr) {
    // early out if no log output is enabled
    return;
  }
#endif
  char logBuffer[LOG_BUFFER_SIZE];
  va_list args;
  va_start(args, format);
  // See https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html for more about PGM_P, PSTR,
  // F, FlashStringHelper, etc.
  vsnprintf_P(logBuffer, LOG_BUFFER_SIZE, reinterpret_cast<PGM_P>(format), args);
#ifdef ENABLE_WEBSOCKET_LOGGING
  webSocket.printfAll(logBuffer);
#endif
  if (mqttClient != nullptr) {
    mqttClient->publish(mqttTopic, logBuffer);
  }
  va_end(args);
}
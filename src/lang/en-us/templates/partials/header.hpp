#pragma once
#include <Arduino.h>

namespace partials {

// clang-format off
const char* header PROGMEM = //R"====(
"<!DOCTYPE html>"
"<html lang=\"en\" class=\"\" color-mode=\"user\">"
"<head>"
    "<meta charset='utf-8' />"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\" />"
    "<title>Mitsubishi2MQTT - {{header.hostname}}</title>"
    "<link rel=\"stylesheet\" href=\"/css\" />"
"</head>"
"<body>"
    "<header>"
        "<noscript>To use Mitsubishi2MQTT, you need to activate Javascript<br/></noscript>"
        "<h1>{{header.hostname}}</h1>"
    "</header>"
;
// )====";
// clang-format on

};  // namespace partials

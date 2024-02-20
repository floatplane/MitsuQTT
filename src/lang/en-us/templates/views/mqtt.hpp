#pragma once
#include <Arduino.h>

namespace views {

const char* mqttFriendlyNameLabel PROGMEM = "Friendly Name";
const char* mqttHostLabel PROGMEM = "Host";
const char* mqttUserLabel PROGMEM = "User";
const char* mqttTopicLabel PROGMEM = "Topic";

// clang-format off
const char* mqttTextFieldPartial PROGMEM =
"<p><b>{{label}}</b>"
    "<br/>"
    "<input id='{{param}}' name='{{param}}' "
    "autocomplete='off' autocorrect='off' autocapitalize='off' spellcheck='false' "
    "placeholder='{{placeholder}}' value='{{value}}'>"
"</p>";
// clang-format on

// clang-format off
const char* mqtt PROGMEM = // R"====(
"{{> header}}"
"<main>"
    "<h2>MQTT Parameters</h2>"
    "<form method='post'>"
        "{{#friendlyName}}{{> mqttTextField}}{{/friendlyName}}"
        "{{#server}}{{> mqttTextField}}{{/server}}"
        "<p><b>Port (default 1883)</b>"
            "<br/>"
            "<input id='ml' name='ml' type='numeric' placeholder='1883' value='{{port.value}}'>"
        "</p>"
        "{{#user}}{{> mqttTextField}}{{/user}}"
        "<p><b>Password</b>"
            "<br/>"
            "<input id='mp' name='mp' type='password' placeholder='Password' value='{{password.value}}'>"
        "</p>"
        "{{#topic}}{{> mqttTextField}}{{/topic}}"
        "<br/>"
        "<a href='/setup'><em>&lt; Back</em></a>"
        "<button name='save' type='submit' class='button bgrn'>Save & Reboot</button>"
    "</form>"
"</main>"
"{{> footer}}";
// )====";
// clang-format on
};  // namespace views

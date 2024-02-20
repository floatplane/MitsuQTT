#pragma once
#include <Arduino.h>

namespace views {

// clang-format off
const char* others PROGMEM = // R"====(
"{{> header}}"
"<main>"
    "<h2>Other Parameters</h2>"
    "<form method='post'>"
        "<p>"
        "<b>HA Autodiscovery topic</b>"
        "<br/><input id='haat' name='haat' autocomplete='off' autocorrect='off' autocapitalize='off' spellcheck='false' placeholder='homeassistant' value='{{topic}}'>"
        "</p>"
        "{{#toggles}}"
        "<p>"
        "<b>{{title}}</b>"
        "<select name='{{name}}'>"
            "<option value='ON'{{#value}} selected{{/value}}>On</option>"
            "<option value='OFF'{{^value}} selected{{/value}}>Off</option>"
        "</select>"
        "</p>"
        "{{/toggles}}"
        "<br/>"
        "<a class=\"buttonLink\" href='/setup'>&lt; Back</a>"
        "<button name='save' type='submit' class='button bgrn'>Save & Reboot</button>"
    "</form>"
"</main>"
"{{> footer}}";
// )====";
// clang-format on
};  // namespace views

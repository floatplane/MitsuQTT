#pragma once
#include <Arduino.h>

namespace templates {
namespace views {

// clang-format off
const char* others PROGMEM = // R"====(
"{{> header}}"
    "<div id='l1' name='l1'>"
      "<fieldset>"
        "<legend><b>&nbsp; Other Parameters &nbsp;</b></legend>"
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
          "<button name='save' type='submit' class='button bgrn'>Save & Reboot</button>"
        "</form>"
      "</fieldset>"
      "<p><a class='button back' href='/setup'>Back</a></p>"
    "</div>"
"{{> footer}}";
// )====";
// clang-format on
};  // namespace views
};  // namespace templates

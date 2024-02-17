#pragma once
#include <Arduino.h>

namespace templates {
// This is a hack to allow the use of the `{{` and `}}` characters in the templates
// The real way to do this with mustache is to allow changing the delimiters but I punted on that
const char *lbrace PROGMEM = "{";
const char *rbraces PROGMEM = "}}";
};  // namespace templates
#pragma once

#include <Arduino.h>

namespace partials {
extern const __FlashStringHelper *countdown;
extern const __FlashStringHelper *footer;
extern const __FlashStringHelper *header;
};  // namespace partials

namespace views {
extern const __FlashStringHelper *autoconfig;

namespace captive {
extern const __FlashStringHelper *index;
extern const __FlashStringHelper *reboot;
extern const __FlashStringHelper *save;
}  // namespace captive

extern const __FlashStringHelper *index;
extern const __FlashStringHelper *metrics;

namespace mqtt {
extern const __FlashStringHelper *index;
extern const __FlashStringHelper *textField;
}  // namespace mqtt

extern const __FlashStringHelper *others;
extern const __FlashStringHelper *reboot;
extern const __FlashStringHelper *reset;
extern const __FlashStringHelper *setup;
extern const __FlashStringHelper *status;
extern const __FlashStringHelper *unit;
extern const __FlashStringHelper *upgrade;
extern const __FlashStringHelper *upload;
extern const __FlashStringHelper *wifi;
};  // namespace views

namespace statics {
extern const __FlashStringHelper *css;
}

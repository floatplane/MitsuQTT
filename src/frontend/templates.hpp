/*
  MitsuQTT Copyright (c) 2024 floatplane

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU
  Lesser General Public License as published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version. This library is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with this library;
  if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

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

extern const __FlashStringHelper *control;
extern const __FlashStringHelper *index;
extern const __FlashStringHelper *login;
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

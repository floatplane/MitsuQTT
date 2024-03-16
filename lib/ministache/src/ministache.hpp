#pragma once

#ifndef MINISTACHE_MINISTACHE_HPP
#define MINISTACHE_MINISTACHE_HPP

#include <Arduino.h>
#include <ArduinoJson.h>

#include <algorithm>
#include <cassert>
#include <vector>

namespace ministache {

String render(const String& templateContents, const ArduinoJson::JsonDocument& data,
              const std::vector<std::pair<String, String>>& partials = {});

};  // namespace ministache

#endif  // MINISTACHE_MINISTACHE_HPP
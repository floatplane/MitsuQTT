#pragma once

#ifndef MINISTACHE_FALSY_HPP
#define MINISTACHE_FALSY_HPP

#include <ArduinoJson.h>

namespace ministache {
namespace internals {

inline bool isFalsy(const JsonVariantConst& value) {
  if (value.isNull()) {
    return true;
  }
  if (value.is<JsonArrayConst>()) {
    const auto asArray = value.as<ArduinoJson::JsonArrayConst>();
    return asArray.size() == 0;
  }
  if (value.is<bool>()) {
    return !value.as<bool>();
  }
  return false;
}

};  // namespace internals
};  // namespace ministache

#endif  // MINISTACHE_FALSY_HPP

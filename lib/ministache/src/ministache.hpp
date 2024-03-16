
#include <Arduino.h>
#include <ArduinoJson.h>

#include <algorithm>
#include <cassert>
#include <vector>

namespace ministache {

String render(const String& templateContents, const ArduinoJson::JsonDocument& data,
              const std::vector<std::pair<String, String>>& partials = {});

// This is only public because there are tests for it
bool isFalsy(const JsonVariantConst& value);

};  // namespace ministache

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cassert>
#include <vector>

class Template {
 public:
  explicit Template(const String& templateContents) : templateContents(templateContents){};

  String render(const ArduinoJson::JsonDocument& data) const {
    String result;
    const char* currentLocation = this->templateContents.c_str();
    while (*currentLocation != '\0') {
      const char* nextToken = strstr(currentLocation, "{{");
      if (nextToken != nullptr) {
        result.concat(constructStringOfLength(currentLocation, nextToken - currentLocation));
        // get the name of the token
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const String tokenName = parsedToken.first;
        result.concat(renderToken(tokenName, data));
        // set currentLocation to the end of the token (accounting for the "}}" characters)
        currentLocation = parsedToken.second;
      } else {
        result.concat(currentLocation);
        break;
      }
    }
    return result;
  }

  size_t contentLength(const ArduinoJson::JsonDocument& data) const {
    const auto rendered = render(data);
    return rendered.length();
  }

 private:
  String templateContents;

  String renderToken(const String& tokenName, const ArduinoJson::JsonDocument& data) const {
    if (data.containsKey(tokenName)) {
      const auto variant = data[tokenName];
      if (variant.is<String>()) {
        return variant.as<String>();
      } else if (variant.is<int>()) {
        return String(variant.as<int>());
      } else if (variant.is<float>()) {
        return String(variant.as<float>());
      } else if (variant.is<double>()) {
        return String(variant.as<double>());
      }
    }
    return String();
  }

  // Given a const char * at the start of a token sequence "{{", extract the token name and return a
  // pair of the token name and the const char * at the end of the token sequence
  std::pair<String, const char*> parseTokenAtPoint(const char* tokenStart) const {
    assert(tokenStart[0] == '{' && tokenStart[1] == '{');

    // figure out the token name
    tokenStart += 2;
    const auto tokenEnd = strstr(tokenStart, "}}");
    if (tokenEnd == nullptr) {
      return std::make_pair(String(), tokenStart + strlen(tokenStart));
    }
    const auto tokenLength = tokenEnd - tokenStart;
    return std::make_pair(constructStringOfLength(tokenStart, tokenLength), tokenEnd + 2);
  }

  static String constructStringOfLength(const char* cString, size_t length) {
    auto result = String();
    result.concat(cString, length);
    return result;
  }
};

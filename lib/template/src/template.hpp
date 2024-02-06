#include <ArduinoJson.h>

#include <cassert>

template <typename CharPtr = const char*>
class Template {
 public:
  Template(CharPtr templateContents) : templateContents(templateContents){};

  template <typename Destination>
  Destination render(const ArduinoJson::JsonDocument& data) const {
    const auto length = this->contentLength(data);
    // TODO: not actually rendering yet
    Destination result;
    serializeJson(data, result);
    return result;
  }

  size_t contentLength(const ArduinoJson::JsonDocument& data) const {
    auto t = this->templateContents;
    size_t length = 0;
    while (*t != '\0') {
      const auto nextToken = strstr(t, "{{");
      if (nextToken) {
        length += nextToken - t;
        // get the name of the token
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const auto tokenName = parsedToken.first;
        // figure out the length of the token, rendered
        const auto renderedSize = renderToken(data, tokenName).size();
        // add the token length to the total length
        length += renderedSize;
        // set t to the end of the token (accounting for the "}}" characters)
        t = parsedToken.second;
      } else {
        length += strlen(t);
        break;
      }
    }
    return length;
  }

 private:
  CharPtr templateContents;

  static std::string renderToken(const ArduinoJson::JsonDocument& data,
                                 const std::string& tokenName) {
    const auto variant = data[tokenName];
    if (variant.is<const char*>()) {
      return std::string(variant.as<const char*>());
    } else {
      return std::string();
    }
  }

  // Given a CharPtr at the start of a token sequence "{{", extract the token name and return a pair
  // of the token name and the CharPtr at the end of the token sequence
  static std::pair<std::string, CharPtr> parseTokenAtPoint(CharPtr tokenStart) {
    assert(tokenStart[0] == '{' && tokenStart[1] == '{');

    // figure out the token name
    tokenStart += 2;
    const auto tokenEnd = strstr(tokenStart, "}}");
    const auto tokenLength = tokenEnd - tokenStart;
    return std::make_pair(std::string(tokenStart, tokenLength), tokenEnd + 2);
  }
};

#include <Arduino.h>

#include <cassert>
#include <vector>

class Template {
 public:
  struct DataMap {
    std::vector<std::pair<String, String>> data;
    String emptyString;
    DataMap() = default;
    DataMap(std::initializer_list<std::pair<String, String>> list) : data(list){};
    void insert(const std::pair<String, String>& pair) {
      data.push_back(pair);
    }
    const String& at(const String& key) const {
      const auto pair =
          std::find_if(data.begin(), data.end(),
                       [&key](const std::pair<String, String>& pair) { return pair.first == key; });
      if (pair != data.end()) {
        return pair->second;
      }
      return emptyString;
    }
  };

  explicit Template(const String& templateContents) : templateContents(templateContents){};

  String render(const DataMap& data) const {
    String result;
    const char* currentLocation = this->templateContents.c_str();
    while (*currentLocation != '\0') {
      const char* nextToken = strstr(currentLocation, "{{");
      if (nextToken != nullptr) {
        result.concat(constructStringOfLength(currentLocation, nextToken - currentLocation));
        // get the name of the token
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const String tokenName = parsedToken.first;
        result.concat(data.at(tokenName));
        // set currentLocation to the end of the token (accounting for the "}}" characters)
        currentLocation = parsedToken.second;
      } else {
        result.concat(currentLocation);
        break;
      }
    }
    return result;
  }

  size_t contentLength(const DataMap& data) const {
    const auto rendered = render(data);
    return rendered.length();
  }

 private:
  String templateContents;

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


#include <cassert>
#include <vector>

template <typename StringType>
class Template {
 public:
  struct DataMap {
    std::vector<std::pair<StringType, StringType>> data;
    StringType emptyString;
    DataMap() = default;
    DataMap(std::initializer_list<std::pair<StringType, StringType>> list) : data(list){};
    void insert(const std::pair<StringType, StringType>& pair) {
      data.push_back(pair);
    }
    const StringType& at(const StringType& key) const {
      const auto pair = std::find_if(
          data.begin(), data.end(),
          [&key](const std::pair<StringType, StringType>& pair) { return pair.first == key; });
      if (pair != data.end()) {
        return pair->second;
      }
      return emptyString;
    }
  };

  explicit Template(const StringType& templateContents) : templateContents(templateContents){};

  StringType render(const DataMap& data) const {
    StringType result;
    const char* currentLocation = this->templateContents.c_str();
    while (*currentLocation != '\0') {
      const char* nextToken = strstr(currentLocation, "{{");
      if (nextToken != nullptr) {
        result += constructStringOfLength(currentLocation, nextToken - currentLocation);
        // get the name of the token
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const StringType tokenName = parsedToken.first;
        result += data.at(tokenName);
        // set currentLocation to the end of the token (accounting for the "}}" characters)
        currentLocation = parsedToken.second;
      } else {
        result += currentLocation;
        break;
      }
    }
    return result;
  }

  size_t contentLength(const DataMap& data) const {
    const auto rendered = render(data);
    return rendered.size();
  }

 private:
  StringType templateContents;

  // Given a const char * at the start of a token sequence "{{", extract the token name and return a
  // pair of the token name and the const char * at the end of the token sequence
  std::pair<StringType, const char*> parseTokenAtPoint(const char* tokenStart) const {
    assert(tokenStart[0] == '{' && tokenStart[1] == '{');

    // figure out the token name
    tokenStart += 2;
    const auto tokenEnd = strstr(tokenStart, "}}");
    if (tokenEnd == nullptr) {
      return std::make_pair(StringType(), tokenStart + strlen(tokenStart));
    }
    const auto tokenLength = tokenEnd - tokenStart;
    return std::make_pair(constructStringOfLength(tokenStart, tokenLength), tokenEnd + 2);
  }

  static StringType constructStringOfLength(const char* cString, size_t length) {
    return StringType(cString, length);
  }
};

#ifdef String_class_h  // Arduino String class is defined
template <>
String Template<String>::constructStringOfLength(const char* cString, size_t length) {
  auto result = String();
  result.concat(cString, length);
  return result;
}
#endif

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cassert>
#include <vector>

class Template {
 private:
  struct Token {
    String name;
    bool isSubstitution;
    bool htmlEscape;
    bool isComment;
  };

 public:
  explicit Template(const String& templateContents) : templateContents(templateContents){};

  String render(const ArduinoJson::JsonDocument& data) const {
    String result;
    const char* currentLocation = this->templateContents.c_str();
    while (*currentLocation != '\0') {
      const char* nextToken = strstr(currentLocation, "{{");
      if (nextToken != nullptr) {
        result.concat(currentLocation, nextToken - currentLocation);
        // get the name of the token
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const Token token = parsedToken.first;
        if (!token.isComment) {
          result.concat(renderToken(token, data));
        }
        // set currentLocation to the end of the token
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

  String renderToken(const Token& token, const ArduinoJson::JsonDocument& data) const {
    String result;
    if (token.name == ".") {
      return result;
    }
    std::vector<String> path;
    const char* iter = token.name.c_str();
    while (true) {
      const char* nextDot = strchr(iter, '.');
      if (nextDot == nullptr) {
        path.push_back(constructStringOfLength(iter, strlen(iter)));
        break;
      }
      path.push_back(constructStringOfLength(iter, nextDot - iter));
      iter = nextDot + 1;
    }
    auto node = data[path[0]];
    for (auto i = 1; i < path.size(); i++) {
      node = node[path[i]];
    }
    if (!node.isNull()) {
      result = node.as<String>();
    }

    if (token.htmlEscape) {
      // replace ampersands first, so that we don't replace the ampersands in the other escapes
      result.replace(("&"), ("&amp;"));
      result.replace(("\""), ("&quot;"));
      result.replace(("<"), ("&lt;"));
      result.replace((">"), ("&gt;"));
    }
    return result;
  }

  // Given a const char * at the start of a token sequence "{{", extract the token name and return a
  // pair of the token name and the const char * at the end of the token sequence
  std::pair<Token, const char*> parseTokenAtPoint(const char* tokenStart) const {
    assert(tokenStart[0] == '{' && tokenStart[1] == '{');
    tokenStart += 2;
    String closeTag("}}");

    bool escapeHtml = true;
    bool isComment = false;
    if (*tokenStart == '{') {
      escapeHtml = false;
      tokenStart++;
      closeTag = "}}}";
    } else if (*tokenStart == '!') {
      isComment = true;
    }

    while (*tokenStart == ' ') {
      tokenStart++;
    }

    if (*tokenStart == '&') {
      escapeHtml = false;
      tokenStart++;
    }

    while (*tokenStart == ' ') {
      tokenStart++;
    }

    // figure out the token name
    const char* tokenEnd = strstr(tokenStart, closeTag.c_str());
    if (tokenEnd == nullptr) {
      return std::make_pair(Token{.name = String(),
                                  .isSubstitution = true,
                                  .htmlEscape = escapeHtml,
                                  .isComment = isComment},
                            tokenStart + strlen(tokenStart));
    }
    size_t tokenLength = tokenEnd - tokenStart;
    while (tokenLength > 0 && tokenStart[tokenLength - 1] == ' ') {
      tokenLength--;
    }
    return std::make_pair(Token{.name = constructStringOfLength(tokenStart, tokenLength),
                                .isSubstitution = true,
                                .htmlEscape = escapeHtml,
                                .isComment = isComment},
                          tokenEnd + closeTag.length());
  }

  static String constructStringOfLength(const char* cString, size_t length) {
    auto result = String();
    result.concat(cString, length);
    return result;
  }
};

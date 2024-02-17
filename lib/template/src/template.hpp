
#include <Arduino.h>
#include <ArduinoJson.h>

#include <cassert>
#include <vector>

class Template {
 private:
  enum class TokenType {
    Text,
    Comment,
    Section,
    InvertedSection,
    EndSection,
    Partial,
  };
  struct Token {
    String name;
    TokenType type;
    bool htmlEscape;
  };

 public:
  explicit Template(const String& templateContents) : templateContents(templateContents){};

  String render(const ArduinoJson::JsonDocument& data) const {
    // set up context stack
    // root of stack: object/array/null/string
    // each recursive call does the following:
    // 1. push new context onto stack
    // 2. render template until end of section/end of template
    // 3. pop context off stack
    // 4. return rendered section string
    JsonVariantConst currentContext = data.as<JsonVariantConst>();
    std::vector<JsonVariantConst> contextStack;
    contextStack.push_back(currentContext);

    const auto result = renderWithContextStack(0, contextStack, true);
    // assert(result.second == templateContents.length());
    return result.first;
  }

  static bool isFalsy(const JsonVariantConst& value) {
    if (value.isNull()) {
      return true;
    } else if (value.is<JsonArrayConst>()) {
      const auto asArray = value.as<ArduinoJson::JsonArrayConst>();
      return asArray.size() == 0;
    } else if (value.is<bool>()) {
      return !value.as<bool>();
    }
    return false;
  }

 private:
  String templateContents;

  std::pair<String, size_t> renderWithContextStack(size_t position,
                                                   std::vector<JsonVariantConst>& contextStack,
                                                   bool renderSection) const {
    String result;
    while (position < templateContents.length()) {
      auto nextToken = templateContents.indexOf("{{", position);
      if (nextToken != -1) {
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const auto tokenRenderExtents =
            getExclusionRangeForToken(nextToken, parsedToken.second, parsedToken.first.type);
        // printf("Next token: %lu\n", nextToken);
        if (renderSection) {
          result.concat(templateContents.substring(position, tokenRenderExtents.first));
        }
        // printf("parsed token: %s type: %d end: %lu\n", parsedToken.first.name.c_str(),
        //        (int)parsedToken.first.type, parsedToken.second);
        const Token token = parsedToken.first;
        if (token.type == TokenType::Text) {
          if (renderSection) {
            result.concat(renderToken(token, contextStack));
          }
          position = tokenRenderExtents.second;
        } else if (token.type == TokenType::Section) {
          auto context = lookupTokenInContextStack(token.name, contextStack);
          auto falsy = isFalsy(context);
          if (!falsy && context.is<JsonArrayConst>()) {
            auto array = context.as<JsonArrayConst>();
            for (auto i = 0; i < array.size(); i++) {
              contextStack.push_back(array[i]);
              auto sectionResult =
                  renderWithContextStack(tokenRenderExtents.second, contextStack, renderSection);
              if (renderSection) {
                result.concat(sectionResult.first);
              }
              contextStack.pop_back();
              position = sectionResult.second;
            }
          } else {
            contextStack.push_back(context);
            auto sectionResult = renderWithContextStack(tokenRenderExtents.second, contextStack,
                                                        renderSection && !falsy);
            if (renderSection) {
              result.concat(sectionResult.first);
            }
            contextStack.pop_back();
            position = sectionResult.second;
          }
        } else if (token.type == TokenType::InvertedSection) {
          // printf("Inverted section: %s %lu %s\n", token.name.c_str(), parsedToken.second,
          //        templateContents.c_str());
          // check token for falsy value, render if falsy
          auto context = lookupTokenInContextStack(token.name, contextStack);
          auto falsy = isFalsy(context);
          auto sectionResult = renderWithContextStack(tokenRenderExtents.second, contextStack,
                                                      renderSection && falsy);
          // printf("Inverted section result: %s %lu\n", sectionResult.first.c_str(),
          //        sectionResult.second);
          if (renderSection) {
            result.concat(sectionResult.first);
          }
          position = sectionResult.second;
        } else if (token.type == TokenType::EndSection) {
          // printf("End section: %s %lu %s\n", token.name.c_str(), parsedToken.second,
          //        templateContents.c_str());
          return std::make_pair(result, tokenRenderExtents.second);
        } else {
          position = tokenRenderExtents.second;
        }
      } else {
        if (renderSection) {
          result.concat(templateContents.substring(position, templateContents.length()));
        }
        break;
      }
    }
    return std::make_pair(result, position);
  }

  std::pair<size_t, size_t> getExclusionRangeForToken(size_t tokenStart, size_t tokenEnd,
                                                      TokenType tokenType) const {
    if (tokenType == TokenType::Text || tokenType == TokenType::Partial) {
      // These token types output content, so we treat the whitespace around them as significant
      return std::make_pair(tokenStart, tokenEnd);
    }

    auto lineStart = templateContents.lastIndexOf('\n', static_cast<int>(tokenStart)) +
                     1;                                       // 0 if this is the first line
    auto lineEnd = templateContents.indexOf('\n', tokenEnd);  // -1 if this is the last line
    if (lineEnd == -1) {
      lineEnd = templateContents.length();
    }
    bool standalone = true;
    for (auto i = lineStart; i < tokenStart; i++) {
      if (!isspace(templateContents[i])) {
        standalone = false;
        break;
      }
    }
    for (auto i = tokenEnd; i < lineEnd; i++) {
      if (!isspace(templateContents[i])) {
        standalone = false;
        break;
      }
    }
    if (!standalone) {
      return std::make_pair(tokenStart, tokenEnd);
    }

    // If the token is on the very last line of the template, then remove the preceding newline
    if (lineEnd == templateContents.length() && lineStart > 0) {
      lineStart--;
      // Also remove any preceding carriage return
      if (templateContents[lineStart] == '\r') {
        lineStart--;
      }
    } else {
      // Remove the trailing newline
      lineEnd++;
    }
    return std::make_pair(lineStart, lineEnd);

    // // If the token is on a line by itself (with leading and trailing whitespace allowed), and
    // // the token doesn't generate any output itself, then we should not output the blank line.
    // // This means pushing the start of the exclusion range back to the start of the line, and the
    // // end to the start of the next line.
    // return std::make_pair(startOfTokenLine, startOfNextLine);
  }

  String renderToken(const Token& token, const std::vector<JsonVariantConst>& contextStack) const {
    auto node = lookupTokenInContextStack(token.name, contextStack);
    String result = node.isNull() ? "" : node.as<String>();

    if (token.htmlEscape) {
      // replace ampersands first, so that we don't replace the ampersands in the other escapes
      result.replace(("&"), ("&amp;"));
      result.replace(("\""), ("&quot;"));
      result.replace(("<"), ("&lt;"));
      result.replace((">"), ("&gt;"));
    }
    return result;
  }

  JsonVariantConst lookupTokenInContextStack(
      const String& name, const std::vector<JsonVariantConst>& contextStack) const {
    JsonVariantConst node;
    if (name == ".") {
      return contextStack.back();
    }
    std::vector<String> path = splitPath(name);
    for (auto context = contextStack.rbegin(); context != contextStack.rend(); context++) {
      node = lookupTokenInContext(path, *context);
      if (!node.isNull()) {
        break;
      }
    }
    return node;
  }

  JsonVariantConst lookupTokenInContext(const std::vector<String>& path,
                                        const JsonVariantConst& context) const {
    String result;
    auto node = context[path[0]];
    for (auto i = 1; i < path.size(); i++) {
      node = node[path[i]];
    }
    return node;
  }

  std::vector<String> splitPath(const String& path) const {
    std::vector<String> result;
    size_t start = 0;
    size_t end = path.indexOf('.');
    while (end != -1) {
      result.push_back(path.substring(start, end));
      start = end + 1;
      end = path.indexOf('.', start);
    }
    result.push_back(path.substring(start, path.length()));
    return result;
  }

  // Given a const char * at the start of a token sequence "{{", extract the token name and return a
  // pair of the token name and the const char * at the end of the token sequence
  std::pair<Token, size_t> parseTokenAtPoint(size_t position) const {
    assert(templateContents[position] == '{' && templateContents[position + 1] == '{');
    position += 2;
    String closeTag("}}");

    bool escapeHtml = true;
    TokenType type = TokenType::Text;
    auto nextChar = templateContents[position];
    switch (nextChar) {
      case '{':
        escapeHtml = false;
        position++;
        closeTag = "}}}";
        break;
      case '!':
        type = TokenType::Comment;
        position++;
        break;
      case '#':
        type = TokenType::Section;
        position++;
        break;
      case '^':
        type = TokenType::InvertedSection;
        position++;
        break;
      case '/':
        type = TokenType::EndSection;
        position++;
        break;
      // case '>':
      //   type = TokenType::Partial;
      //   break;
      default:
        break;
    }

    while (templateContents[position] == ' ') {
      position++;
    }

    if (templateContents[position] == '&') {
      escapeHtml = false;
      position++;
    }

    while (templateContents[position] == ' ') {
      position++;
    }

    // figure out the token name
    auto closeTagPosition = templateContents.indexOf(closeTag, position);
    // printf("Close tag position: %lu\n", closeTagPosition);
    if (closeTagPosition == -1) {
      return std::make_pair(Token{.name = String(), .type = type, .htmlEscape = escapeHtml},
                            templateContents.length());
    }
    size_t tokenEnd = closeTagPosition;
    while (tokenEnd > position && templateContents[tokenEnd - 1] == ' ') {
      tokenEnd--;
    }
    return std::make_pair(Token{.name = templateContents.substring(position, tokenEnd),
                                .type = type,
                                .htmlEscape = escapeHtml},
                          closeTagPosition + closeTag.length());
  }
};

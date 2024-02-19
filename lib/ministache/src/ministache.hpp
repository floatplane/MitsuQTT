
#include <Arduino.h>
#include <ArduinoJson.h>

#include <algorithm>
#include <cassert>
#include <vector>

namespace ministache {

class Ministache {
 private:
  enum class TokenType {
    Text,
    Comment,
    Section,
    InvertedSection,
    EndSection,
    Partial,
    Delimiter,
  };
  struct Token {
    String name;
    TokenType type;
    bool htmlEscape;
  };
  struct DelimiterPair {
    String open;
    String close;
  };

 public:
  explicit Ministache(const String& templateContents) : templateContents(templateContents){};

  // TODO(floatplane) I know that this method can be static, but not doing it today
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  String render(const ArduinoJson::JsonDocument& data,
                const std::vector<std::pair<String, String>>& partials = {}) const {
    // set up context stack
    // root of stack: object/array/null/string
    // each recursive call to renderWithContextStack does the following:
    // 1. push new context onto stack
    // 2. render template until end of section/end of template
    // 3. pop context off stack
    // 4. return rendered section string
    JsonVariantConst currentContext = data.as<JsonVariantConst>();
    std::vector<JsonVariantConst> contextStack;
    contextStack.push_back(currentContext);

    const auto result = renderWithContextStack(0, contextStack, partials, true);
    return result.first;
  }

  static bool isFalsy(const JsonVariantConst& value) {
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

  static String indentLines(const String& input, size_t indentation) {
    String result;
    std::string indent(indentation, ' ');
    size_t start = 0;
    while (start < input.length()) {
      auto end = input.indexOf('\n', start);
      if (end == -1) {
        result.concat(indent.c_str());
        result.concat(input.substring(start, input.length()));
        break;
      }
      result.concat(indent.c_str());
      result.concat(input.substring(start, end + 1));
      start = end + 1;
    }
    return result;
  }

 private:
  String templateContents;

  // TODO(floatplane) yeah, I know it's a mess
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  std::pair<String, size_t> renderWithContextStack(
      size_t position, std::vector<JsonVariantConst>& contextStack,

      const std::vector<std::pair<String, String>>& partials, bool renderSection,
      const DelimiterPair& initialDelimiters = {.open = "{{", .close = "}}"}) const {
    String result;
    DelimiterPair delimiters = initialDelimiters;
    while (position < templateContents.length()) {
      auto nextToken = templateContents.indexOf(delimiters.open, position);
      if (nextToken != -1) {
        const auto parsedToken = parseTokenAtPoint(nextToken, delimiters);
        const auto tokenRenderExtents =
            getExclusionRangeForToken(nextToken, parsedToken.second, parsedToken.first.type);
        if (renderSection) {
          result.concat(templateContents.substring(position, tokenRenderExtents.start));
        }
        const Token token = parsedToken.first;
        switch (token.type) {
          case TokenType::Text:
            if (renderSection) {
              result.concat(renderToken(token, contextStack));
            }
            position = tokenRenderExtents.end;
            break;

          case TokenType::Section: {
            auto context = lookupTokenInContextStack(token.name, contextStack);
            auto falsy = isFalsy(context);
            if (!falsy && context.is<JsonArrayConst>()) {
              auto array = context.as<JsonArrayConst>();
              for (size_t i = 0; i < array.size(); i++) {
                contextStack.push_back(array[i]);
                auto sectionResult = renderWithContextStack(tokenRenderExtents.end, contextStack,
                                                            partials, renderSection, delimiters);
                if (renderSection) {
                  result.concat(sectionResult.first);
                }
                contextStack.pop_back();
                position = sectionResult.second;
              }
            } else {
              contextStack.push_back(context);
              auto sectionResult =
                  renderWithContextStack(tokenRenderExtents.end, contextStack, partials,
                                         renderSection && !falsy, delimiters);
              if (renderSection) {
                result.concat(sectionResult.first);
              }
              contextStack.pop_back();
              position = sectionResult.second;
            }
            break;
          }

          case TokenType::InvertedSection: {
            // check token for falsy value, render if falsy
            auto context = lookupTokenInContextStack(token.name, contextStack);
            auto falsy = isFalsy(context);
            auto sectionResult = renderWithContextStack(
                tokenRenderExtents.end, contextStack, partials, renderSection && falsy, delimiters);
            if (renderSection) {
              result.concat(sectionResult.first);
            }
            position = sectionResult.second;
            break;
          }

          case TokenType::EndSection:
            return std::make_pair(result, tokenRenderExtents.end);

          case TokenType::Partial:
            if (renderSection) {
              // find partial in partials list and render with it
              auto partial = std::find_if(
                  partials.begin(), partials.end(),
                  [&token](const auto& partial) { return partial.first == token.name; });
              if (partial != partials.end()) {
                // Partials must match the indentation of the token that references them
                String indentedPartial =
                    indentLines(partial->second, tokenRenderExtents.indentation);
                // NB: we don't pass custom delimiters down to a partial
                String partialResult =
                    Ministache(indentedPartial)
                        .renderWithContextStack(0, contextStack, partials, renderSection)
                        .first;
                result.concat(partialResult);
              }
            }
            position = tokenRenderExtents.end;
            break;

          case TokenType::Delimiter:
            // The name will look like "<% %>" - we need to split it on whitespace, and
            // assign the start and end delimiters
            delimiters.open = token.name.substring(0, token.name.indexOf(' '));
            delimiters.close = token.name.substring(
                token.name.lastIndexOf(' ', token.name.length() - 1) + 1, token.name.length());
            position = tokenRenderExtents.end;
            break;

          default:
            position = tokenRenderExtents.end;
            break;
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

  // Tokens that don't output content and are standalone (i.e. not surrounded by non-whitespace)
  // should not leave blank lines in the content. This function returns the range of the template
  // that should be excluded from the output when the token is standalone.
  struct ExclusionRange {
    size_t start;
    size_t end;
    size_t indentation;
  };
  ExclusionRange getExclusionRangeForToken(size_t tokenStart, size_t tokenEnd,
                                           TokenType tokenType) const {
    ExclusionRange defaultResult{
        .start = tokenStart,
        .end = tokenEnd,
        .indentation = 0,
    };

    if (tokenType == TokenType::Text) {
      // Text tokens are never standalone
      return defaultResult;
    }

    auto lineStart = templateContents.lastIndexOf('\n', static_cast<int>(tokenStart)) +
                     1;                                       // 0 if this is the first line
    auto lineEnd = templateContents.indexOf('\n', tokenEnd);  // -1 if this is the last line
    if (lineEnd == -1) {
      lineEnd = static_cast<int>(templateContents.length());
    }
    bool standalone = true;
    for (int i = lineStart; i < static_cast<int>(tokenStart); i++) {
      if (isspace(templateContents[i]) == 0) {
        standalone = false;
        break;
      }
    }
    for (int i = static_cast<int>(tokenEnd); i < lineEnd; i++) {
      if (isspace(templateContents[i]) == 0) {
        standalone = false;
        break;
      }
    }
    if (!standalone) {
      return defaultResult;
    }

    // If the token is on the very last line of the template, then remove the preceding newline,
    // but only if there's no leading whitespace before the token
    size_t indentation = tokenStart - lineStart;
    if (lineEnd == static_cast<int>(templateContents.length()) && lineStart > 0 &&
        lineStart == static_cast<int>(tokenStart)) {
      lineStart--;
      // Also remove any preceding carriage return
      if (lineStart > 0 && templateContents[lineStart] == '\r') {
        lineStart--;
      }
    } else {
      // Remove the trailing newline
      lineEnd++;
    }
    return {
        .start = static_cast<size_t>(lineStart),
        .end = static_cast<size_t>(lineEnd),
        .indentation = indentation,
    };
  }

  static String renderToken(const Token& token, const std::vector<JsonVariantConst>& contextStack) {
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

  static JsonVariantConst lookupTokenInContextStack(
      const String& name, const std::vector<JsonVariantConst>& contextStack) {
    if (name == ".") {
      return contextStack.back();
    }
    std::vector<String> path = splitPath(name);
    for (auto context = contextStack.rbegin(); context != contextStack.rend(); context++) {
      if (isValidContextForPath(*context, path)) {
        return lookupTokenInContext(path, *context);
      }
    }
    return JsonVariantConst();
  }

  static JsonVariantConst lookupTokenInContext(const std::vector<String>& path,
                                               const JsonVariantConst& context) {
    String result;
    auto node = context;
    for (size_t i = 0; i < path.size(); i++) {
      node = node[path[i]];
    }
    return node;
  }

  static bool isValidContextForPath(const JsonVariantConst& context,
                                    const std::vector<String>& path) {
    if (path.size() == 1) {
      return context.containsKey(path[0]);
    }
    auto parent = context;
    for (size_t i = 0; i < path.size() - 1; i++) {
      parent = parent[path[i]];
    }
    return !parent.isNull();
  }

  static std::vector<String> splitPath(const String& path) {
    std::vector<String> result;
    int start = 0;
    int end = path.indexOf('.');
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
  std::pair<Token, size_t> parseTokenAtPoint(size_t position,
                                             const DelimiterPair& delimiters) const {
    assert(templateContents.substring(position, position + delimiters.open.length()) ==
           delimiters.open);
    position += delimiters.open.length();
    String closeTag(delimiters.close);

    bool escapeHtml = true;
    TokenType type = TokenType::Text;
    auto nextChar = templateContents[position];
    switch (nextChar) {
      case '{':
        escapeHtml = false;
        position++;
        closeTag = String("}");
        closeTag.concat(delimiters.close);
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
      case '>':
        type = TokenType::Partial;
        position++;
        break;
      case '=':
        type = TokenType::Delimiter;
        position++;
        closeTag = String("=");
        closeTag.concat(delimiters.close);
        break;
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

};  // namespace ministache
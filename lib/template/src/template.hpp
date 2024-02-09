
#include <cassert>
#include <unordered_map>
#include <vector>

template <typename StringType>
class Template {
 public:
  typedef std::unordered_map<StringType, StringType> DataMap;

  Template(const StringType& templateContents) : templateContents(templateContents){};

  StringType render(const DataMap& data) const {
    StringType result;
    std::unordered_map<StringType, StringType> tokenCache;
    const char* t = this->templateContents.c_str();
    while (*t != '\0') {
      const char* nextToken = strstr(t, "{{");
      if (nextToken) {
        result += StringType(t, nextToken - t);
        // get the name of the token
        const auto parsedToken = parseTokenAtPoint(nextToken);
        const StringType tokenName = parsedToken.first;
        // check if we've already rendered this token
        const auto cacheIter = tokenCache.find(tokenName);
        if (cacheIter != tokenCache.end()) {
          result += cacheIter->second;
        } else {
          const StringType renderedToken = renderToken(data, tokenName);
          result += renderedToken;
          tokenCache[tokenName] = renderedToken;
        }
        // set t to the end of the token (accounting for the "}}" characters)
        t = parsedToken.second;
      } else {
        result += t;
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

  StringType renderToken(const DataMap& data, const StringType& tokenName) const {
    const auto iter = data.find(tokenName);
    if (iter == data.end()) {
      return StringType();
    }
    return iter->second;
  }

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
    return std::make_pair(StringType(tokenStart, tokenLength), tokenEnd + 2);
  }
};
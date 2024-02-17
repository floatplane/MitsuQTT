// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

#include <cstring>
#include <string>

class __FlashStringHelper;

// Reproduces Arduino's String class
class String {
 public:
  String() = default;
  String(const char* s) {
    if (s) {
      str_.assign(s);
    }
  }

  void limitCapacityTo(size_t maxCapacity) {
    maxCapacity_ = maxCapacity;
  }

  unsigned char concat(const String& s) {
    return concat(s.c_str(), s.length());
  }

  unsigned char concat(const String& s, size_t n) {
    return concat(s.c_str(), n);
  }

  unsigned char concat(const char* s) {
    return concat(s, strlen(s));
  }

  unsigned char concat(const char* s, size_t n) {
    if (str_.size() + n > maxCapacity_) {
      return 0;
    }
    str_.append(s, n);
    return 1;
  }

  size_t length() const {
    return str_.size();
  }

  const char* c_str() const {
    return str_.c_str();
  }

  bool operator==(const char* s) const {
    return str_ == s;
  }

  bool operator==(const String& s) const {
    return str_ == s.str_;
  }

  String& operator=(const char* s) {
    if (s) {
      str_.assign(s);
    } else {
      str_.clear();
    }
    return *this;
  }

  char operator[](unsigned int index) const {
    if (index >= str_.size()) {
      return 0;
    }
    return str_[index];
  }

  void replace(const char* from, const char* to) {
    size_t start = 0;
    while (true) {
      const auto pos = str_.find(from, start);
      if (pos == std::string::npos) {
        return;
      }
      str_.replace(pos, strlen(from), to);
      start = pos + strlen(to);
    }
  }

  void replace(const __FlashStringHelper* from, const __FlashStringHelper* to) {
    replace(reinterpret_cast<const char*>(from), reinterpret_cast<const char*>(to));
  }

  int indexOf(char ch, unsigned int fromIndex = 0) const {
    const auto pos = str_.find(ch, fromIndex);
    if (pos == std::string::npos) {
      return -1;
    }
    return pos;
  }

  int lastIndexOf(char ch, unsigned int fromIndex = 0) const {
    const auto pos = str_.rfind(ch, fromIndex);
    if (pos == std::string::npos) {
      return -1;
    }
    return pos;
  }

  int indexOf(const char* str, unsigned int fromIndex = 0) const {
    const auto pos = str_.find(str, fromIndex);
    if (pos == std::string::npos) {
      return -1;
    }
    return pos;
  }

  int indexOf(const String& str, unsigned int fromIndex = 0) const {
    return indexOf(str.c_str(), fromIndex);
  }

  String substring(unsigned int beginIndex, unsigned int endIndex) const {
    return String(str_.substr(beginIndex, endIndex - beginIndex).c_str());
  }

  friend std::ostream& operator<<(std::ostream& lhs, const ::String& rhs) {
    lhs << rhs.str_;
    return lhs;
  }

 protected:
  std::string str_;
  size_t maxCapacity_ = 1024;
};

class StringSumHelper : public ::String {};

inline bool operator==(const std::string& lhs, const ::String& rhs) {
  return lhs == rhs.c_str();
}

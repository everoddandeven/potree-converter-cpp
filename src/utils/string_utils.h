#pragma once

#include <string>

namespace potree {
namespace string_utils {

  static inline std::string replace(const std::string& str, const std::string& search, const std::string& replacement) {
    auto index = str.find(search);
    if (index == str.npos) return str;
    std::string strCopy = str;
    strCopy.replace(index, search.length(), replacement);
    return strCopy;
  }

  static inline bool icompare_pred(unsigned char a, unsigned char b) {
    return std::tolower(a) == std::tolower(b);
  }

  static inline bool icompare(std::string const& a, std::string const& b) {
    if (a.length() != b.length()) return false;
    return std::equal(b.begin(), b.end(), a.begin(), icompare_pred);
  }

  static inline bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    auto tstr = str.substr(str.size() - suffix.size());
    return tstr.compare(suffix) == 0;
  }

  static inline bool iends_with(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    auto tstr = str.substr(str.size() - suffix.size());
    return icompare(tstr, suffix);
  }

  static inline std::string left_pad(const std::string& in, int length, const char character = ' ') {
    int tmp = length - in.size();
    auto reps = std::max(tmp, 0);
    return std::string(reps, character) + in;
  }

  static inline std::string right_pad(const std::string& in, int64_t length, const char character = ' ') {
    auto reps = std::max(length - int64_t(in.size()), int64_t(0));
    return in + std::string(reps, character);
  }

}
}
/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlrcpp {

class ANTLR4CPP_PUBLIC Arrays {
 public:
  static std::string listToString(const std::vector<std::string>& list,
                                  const std::string& separator);

  template <typename T>
  static bool equals(const std::vector<T>& a, const std::vector<T>& b) {
    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); ++i)
      if (!(a[i] == b[i])) return false;

    return true;
  }

  template <typename T>
  static bool equals(const std::vector<T*>& a, const std::vector<T*>& b) {
    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); ++i) {
      if (a[i] == b[i]) continue;
      if (!(*a[i] == *b[i])) return false;
    }

    return true;
  }

  template <typename T>
  static bool equals(const std::vector<Ref<T>>& a,
                     const std::vector<Ref<T>>& b) {
    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); ++i) {
      if (!a[i] && !b[i]) continue;
      if (!a[i] || !b[i]) return false;
      if (a[i] == b[i]) continue;

      if (!(*a[i] == *b[i])) return false;
    }

    return true;
  }

  template <typename T>
  static std::string toString(const std::vector<T>& source) {
    std::string result = "[";
    bool firstEntry = true;
    for (auto& value : source) {
      result += value.toString();
      if (firstEntry) {
        result += ", ";
        firstEntry = false;
      }
    }
    return result + "]";
  }

  template <typename T>
  static std::string toString(const std::vector<Ref<T>>& source) {
    std::string result = "[";
    bool firstEntry = true;
    for (auto& value : source) {
      result += value->toString();
      if (firstEntry) {
        result += ", ";
        firstEntry = false;
      }
    }
    return result + "]";
  }

  template <typename T>
  static std::string toString(const std::vector<T*>& source) {
    std::string result = "[";
    bool firstEntry = true;
    for (auto value : source) {
      result += value->toString();
      if (firstEntry) {
        result += ", ";
        firstEntry = false;
      }
    }
    return result + "]";
  }
};

template <>
std::string Arrays::toString(
    const std::vector<antlr4::tree::ParseTree*>& source);
}  // namespace antlrcpp

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_UTILS_H_
#define V8_TORQUE_UTILS_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "src/base/functional.h"

namespace v8 {
namespace internal {
namespace torque {

typedef std::vector<std::string> NameVector;

std::string StringLiteralUnquote(const std::string& s);
std::string StringLiteralQuote(const std::string& s);

[[noreturn]] void ReportError(const std::string& error);

std::string CamelifyString(const std::string& underscore_string);
std::string DashifyString(const std::string& underscore_string);

void ReplaceFileContentsIfDifferent(const std::string& file_path,
                                    const std::string& contents);

std::string CurrentPositionAsString();

template <class T>
class Deduplicator {
 public:
  const T* Add(T x) { return &*(storage_.insert(std::move(x)).first); }

 private:
  std::unordered_set<T, base::hash<T>> storage_;
};

template <class C, class T>
void PrintCommaSeparatedList(std::ostream& os, const T& list, C transform) {
  bool first = true;
  for (auto& e : list) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << transform(e);
  }
}

template <class T,
          typename std::enable_if<
              std::is_pointer<typename T::value_type>::value, int>::type = 0>
void PrintCommaSeparatedList(std::ostream& os, const T& list) {
  bool first = true;
  for (auto& e : list) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << *e;
  }
}

template <class T,
          typename std::enable_if<
              !std::is_pointer<typename T::value_type>::value, int>::type = 0>
void PrintCommaSeparatedList(std::ostream& os, const T& list) {
  bool first = true;
  for (auto& e : list) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << e;
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_UTILS_H_

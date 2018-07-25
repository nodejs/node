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

void ReportError(const std::string& error);

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

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_UTILS_H_

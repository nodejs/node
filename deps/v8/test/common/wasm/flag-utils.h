// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_FLAG_UTILS_H
#define V8_TEST_COMMON_FLAG_UTILS_H

namespace v8 {
namespace internal {

template <typename T>
class FlagScope {
 public:
  FlagScope(T* flag, T new_value) : flag_(flag), previous_value_(*flag) {
    *flag = new_value;
  }
  ~FlagScope() { *flag_ = previous_value_; }

 private:
  T* flag_;
  T previous_value_;
};

#define FLAG_SCOPE(flag) \
  FlagScope<bool> __scope_##flag##__LINE__(&FLAG_##flag, true)

#define EXPERIMENTAL_FLAG_SCOPE(flag) FLAG_SCOPE(experimental_wasm_##flag)

}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_COMMON_FLAG_UTILS_H

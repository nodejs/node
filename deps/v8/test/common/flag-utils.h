// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_FLAG_UTILS_H
#define V8_TEST_COMMON_FLAG_UTILS_H

namespace v8 {
namespace internal {

template <typename T>
class V8_NODISCARD FlagScope {
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

}  // namespace internal
}  // namespace v8

#define FLAG_SCOPE_EXTERNAL(flag)                         \
  v8::internal::FlagScope<bool> __scope_##flag##__LINE__( \
      &v8::internal::FLAG_##flag, true)

#define UNFLAG_SCOPE_EXTERNAL(flag)                       \
  v8::internal::FlagScope<bool> __scope_##flag##__LINE__( \
      &v8::internal::FLAG_##flag, false)

#endif  // V8_TEST_COMMON_FLAG_UTILS_H

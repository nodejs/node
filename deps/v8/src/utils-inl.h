// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_INL_H_
#define V8_UTILS_INL_H_

#include "list-inl.h"

namespace v8 {
namespace internal {

template<typename T, int growth_factor, int max_growth>
void Collector<T, growth_factor, max_growth>::Reset() {
  for (int i = chunks_.length() - 1; i >= 0; i--) {
    chunks_.at(i).Dispose();
  }
  chunks_.Rewind(0);
  index_ = 0;
  size_ = 0;
}

} }  // namespace v8::internal

#endif  // V8_UTILS_INL_H_

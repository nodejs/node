// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8CHECKS_H_
#define V8_V8CHECKS_H_

#include "checks.h"

namespace v8 {
  class Value;
  template <class T> class Handle;

namespace internal {
  intptr_t HeapObjectTagMask();

} }  // namespace v8::internal


void CheckNonEqualsHelper(const char* file,
                          int line,
                          const char* unexpected_source,
                          v8::Handle<v8::Value> unexpected,
                          const char* value_source,
                          v8::Handle<v8::Value> value);

void CheckEqualsHelper(const char* file,
                       int line,
                       const char* expected_source,
                       v8::Handle<v8::Value> expected,
                       const char* value_source,
                       v8::Handle<v8::Value> value);

#define ASSERT_TAG_ALIGNED(address) \
  ASSERT((reinterpret_cast<intptr_t>(address) & HeapObjectTagMask()) == 0)

#define ASSERT_SIZE_TAG_ALIGNED(size) ASSERT((size & HeapObjectTagMask()) == 0)

#endif  // V8_V8CHECKS_H_

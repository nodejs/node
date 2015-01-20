// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CHECKS_H_
#define V8_CHECKS_H_

#include "src/base/logging.h"

namespace v8 {

class Value;
template <class T> class Handle;

namespace internal {

intptr_t HeapObjectTagMask();

#ifdef ENABLE_SLOW_DCHECKS
#define SLOW_DCHECK(condition) \
  CHECK(!v8::internal::FLAG_enable_slow_asserts || (condition))
extern bool FLAG_enable_slow_asserts;
#else
#define SLOW_DCHECK(condition) ((void) 0)
const bool FLAG_enable_slow_asserts = false;
#endif

} }  // namespace v8::internal


void CheckNonEqualsHelper(const char* file, int line,
                          const char* expected_source, double expected,
                          const char* value_source, double value);

void CheckEqualsHelper(const char* file, int line, const char* expected_source,
                       double expected, const char* value_source, double value);

void CheckNonEqualsHelper(const char* file, int line,
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

#define DCHECK_TAG_ALIGNED(address) \
  DCHECK((reinterpret_cast<intptr_t>(address) & HeapObjectTagMask()) == 0)

#define DCHECK_SIZE_TAG_ALIGNED(size) DCHECK((size & HeapObjectTagMask()) == 0)

#endif  // V8_CHECKS_H_

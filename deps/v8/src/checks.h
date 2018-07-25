// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CHECKS_H_
#define V8_CHECKS_H_

#include "include/v8.h"
#include "src/base/logging.h"
#include "src/globals.h"

namespace v8 {

class Value;

namespace internal {

#ifdef ENABLE_SLOW_DCHECKS
#define SLOW_DCHECK(condition) \
  CHECK(!v8::internal::FLAG_enable_slow_asserts || (condition))
V8_EXPORT_PRIVATE extern bool FLAG_enable_slow_asserts;
#else
#define SLOW_DCHECK(condition) ((void) 0)
static const bool FLAG_enable_slow_asserts = false;
#endif

}  // namespace internal
}  // namespace v8

#define DCHECK_TAG_ALIGNED(address) \
  DCHECK((address & ::v8::internal::kHeapObjectTagMask) == 0)

#define DCHECK_SIZE_TAG_ALIGNED(size) \
  DCHECK((size & ::v8::internal::kHeapObjectTagMask) == 0)

#endif  // V8_CHECKS_H_

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_CHECKS_H_
#define V8_COMMON_CHECKS_H_

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/common/globals.h"

namespace v8 {

class Value;

namespace internal {

#ifdef ENABLE_SLOW_DCHECKS
#define SLOW_DCHECK(condition) \
  CHECK(!v8::internal::FLAG_enable_slow_asserts || (condition))
#define SLOW_DCHECK_IMPLIES(lhs, rhs) SLOW_DCHECK(!(lhs) || (rhs))
V8_EXPORT_PRIVATE extern bool FLAG_enable_slow_asserts;
#else
#define SLOW_DCHECK(condition) ((void)0)
#define SLOW_DCHECK_IMPLIES(v1, v2) ((void)0)
static const bool FLAG_enable_slow_asserts = false;
#endif

}  // namespace internal
}  // namespace v8

#define DCHECK_TAG_ALIGNED(address) \
  DCHECK((address & ::v8::internal::kHeapObjectTagMask) == 0)

#define DCHECK_SIZE_TAG_ALIGNED(size) \
  DCHECK((size & ::v8::internal::kHeapObjectTagMask) == 0)

#endif  // V8_COMMON_CHECKS_H_

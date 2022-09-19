// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OSR_OPTIMIZED_CODE_CACHE_INL_H_
#define V8_OBJECTS_OSR_OPTIMIZED_CODE_CACHE_INL_H_

#include "src/objects/osr-optimized-code-cache.h"

#include "src/objects/fixed-array-inl.h"
// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(OSROptimizedCodeCache, WeakFixedArray)
CAST_ACCESSOR(OSROptimizedCodeCache)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_OSR_OPTIMIZED_CODE_CACHE_INL_H_

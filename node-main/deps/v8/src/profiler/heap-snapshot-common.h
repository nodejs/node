// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_HEAP_SNAPSHOT_COMMON_H_
#define V8_PROFILER_HEAP_SNAPSHOT_COMMON_H_

#include <unordered_set>

#include "src/objects/objects.h"
#include "src/objects/tagged.h"

namespace v8::internal {

using UnorderedCppHeapExternalObjectSet =
    std::unordered_set<Tagged<CppHeapExternalObject>, Object::Hasher,
                       Object::KeyEqualSafe>;

}  // namespace v8::internal

#endif  // V8_PROFILER_HEAP_SNAPSHOT_COMMON_H_

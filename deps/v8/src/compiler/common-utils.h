// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_UTILS_H_
#define V8_COMPILER_COMMON_UTILS_H_

#include "src/handles/handles.h"
#include "src/objects/string.h"

namespace v8::internal::compiler {

class JSHeapBroker;

namespace utils {

MaybeHandle<String> ConcatenateStrings(Handle<String> left,
                                       Handle<String> right,
                                       JSHeapBroker* broker);

}  // namespace utils
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_COMMON_UTILS_H_

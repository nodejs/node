// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OBJECT_ACCESS_H_
#define V8_WASM_OBJECT_ACCESS_H_

#include "src/common/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {
namespace wasm {

class ObjectAccess : public AllStatic {
 public:
  // Convert an offset into an object to an offset into a tagged object.
  static constexpr int ToTagged(int offset) { return offset - kHeapObjectTag; }

  // Get the offset into a fixed array for a given {index}.
  static constexpr int ElementOffsetInTaggedFixedArray(int index) {
    return ToTagged(FixedArray::OffsetOfElementAt(index));
  }

  // Get the offset of the context stored in a {JSFunction} object.
  static constexpr int ContextOffsetInTaggedJSFunction() {
    return ToTagged(JSFunction::kContextOffset);
  }

  // Get the offset of the shared function info in a {JSFunction} object.
  static constexpr int SharedFunctionInfoOffsetInTaggedJSFunction() {
    return ToTagged(JSFunction::kSharedFunctionInfoOffset);
  }

  // Get the offset of the formal parameter count in a {SharedFunctionInfo}
  // object.
  static constexpr int FormalParameterCountOffsetInSharedFunctionInfo() {
    return ToTagged(SharedFunctionInfo::kFormalParameterCountOffset);
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OBJECT_ACCESS_H_

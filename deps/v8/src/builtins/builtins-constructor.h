// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_CONSTRUCTOR_H_
#define V8_BUILTINS_BUILTINS_CONSTRUCTOR_H_

#include "src/contexts.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class ConstructorBuiltins {
 public:
  static int MaximumFunctionContextSlots() {
    return FLAG_test_small_max_function_context_stub_size ? kSmallMaximumSlots
                                                          : kMaximumSlots;
  }

  // Maximum number of elements in copied array (chosen so that even an array
  // backed by a double backing store will fit into new-space).
  static const int kMaximumClonedShallowArrayElements =
      JSArray::kInitialMaxFastElementArray * kPointerSize / kDoubleSize;

  // Maximum number of properties in copied objects.
  static const int kMaximumClonedShallowObjectProperties = 6;
  static int FastCloneShallowObjectPropertiesCount(int literal_length) {
    // This heuristic of setting empty literals to have
    // kInitialGlobalObjectUnusedPropertiesCount must remain in-sync with the
    // runtime.
    // TODO(verwaest): Unify this with the heuristic in the runtime.
    return literal_length == 0
               ? JSObject::kInitialGlobalObjectUnusedPropertiesCount
               : literal_length;
  }

 private:
  static const int kMaximumSlots = 0x8000;
  static const int kSmallMaximumSlots = 10;

  // FastNewFunctionContext can only allocate closures which fit in the
  // new space.
  STATIC_ASSERT(((kMaximumSlots + Context::MIN_CONTEXT_SLOTS) * kPointerSize +
                 FixedArray::kHeaderSize) < kMaxRegularHeapObjectSize);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CONSTRUCTOR_H_

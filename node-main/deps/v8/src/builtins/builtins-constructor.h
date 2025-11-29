// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_CONSTRUCTOR_H_
#define V8_BUILTINS_BUILTINS_CONSTRUCTOR_H_

#include "src/objects/contexts.h"
#include "src/objects/dictionary.h"
#include "src/objects/js-array.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class ConstructorBuiltins {
 public:
  static int MaximumFunctionContextSlots() {
    return v8_flags.test_small_max_function_context_stub_size
               ? kSmallMaximumSlots
               : kMaximumSlots;
  }

  // Maximum number of elements in copied array (chosen so that even an array
  // backed by a double backing store will fit into new-space).
  static const int kMaximumClonedShallowArrayElements =
      JSArray::kInitialMaxFastElementArray;
  // Maximum number of properties in copied object so that the properties store
  // will fit into new-space. This constant is based on the assumption that
  // NameDictionaries are 50% over-allocated.
  static const int kMaximumClonedShallowObjectProperties =
      NameDictionary::kMaxRegularCapacity / 3 * 2;

 private:
  static const int kMaximumSlots =
      (kMaxRegularHeapObjectSize - Context::kTodoHeaderSize) / kTaggedSize - 1;
  static const int kSmallMaximumSlots = 10;

  // FastNewFunctionContext can only allocate closures which fit in the
  // new space.
  static_assert(Context::SizeFor(kMaximumSlots + Context::MIN_CONTEXT_SLOTS) <
                kMaxRegularHeapObjectSize);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CONSTRUCTOR_H_

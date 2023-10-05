// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/accessors.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/js-shared-array-inl.h"

namespace v8 {
namespace internal {

// We cannot allocate large objects with |AllocationType::kSharedOld|,
// see |HeapAllocator::AllocateRawLargeInternal|.
constexpr int kMaxJSSharedArraySize = (1 << 14) - 2;
static_assert(FixedArray::SizeFor(kMaxJSSharedArraySize) <=
              kMaxRegularHeapObjectSize);

BUILTIN(SharedArrayConstructor) {
  DCHECK(v8_flags.shared_string_table);

  HandleScope scope(isolate);

  Handle<Object> length_arg = args.atOrUndefined(isolate, 1);
  Handle<Object> length_number;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, length_number,
                                     Object::ToInteger(isolate, length_arg));
  if (!IsSmi(*length_number)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kSharedArraySizeOutOfRange));
  }

  int length = Smi::cast(*length_number).value();
  if (length < 0 || length > kMaxJSSharedArraySize) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kSharedArraySizeOutOfRange));
  }

  return *isolate->factory()->NewJSSharedArray(args.target(), length);
}

}  // namespace internal
}  // namespace v8

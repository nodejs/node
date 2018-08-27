// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/growable-fixed-array-gen.h"

#include "src/compiler/code-assembler.h"

namespace v8 {
namespace internal {

void GrowableFixedArray::Push(TNode<Object> const value) {
  TNode<IntPtrT> const length = var_length_.value();
  TNode<IntPtrT> const capacity = var_capacity_.value();

  Label grow(this), store(this);
  Branch(IntPtrEqual(capacity, length), &grow, &store);

  BIND(&grow);
  {
    var_capacity_ = NewCapacity(capacity);
    var_array_ = ResizeFixedArray(length, var_capacity_.value());

    Goto(&store);
  }

  BIND(&store);
  {
    TNode<FixedArray> const array = var_array_.value();
    StoreFixedArrayElement(array, length, value);

    var_length_ = IntPtrAdd(length, IntPtrConstant(1));
  }
}

TNode<JSArray> GrowableFixedArray::ToJSArray(TNode<Context> const context) {
  const ElementsKind kind = PACKED_ELEMENTS;

  TNode<Context> const native_context = LoadNativeContext(context);
  TNode<Map> const array_map = LoadJSArrayElementsMap(kind, native_context);

  // Shrink to fit if necessary.
  {
    Label next(this);

    TNode<IntPtrT> const length = var_length_.value();
    TNode<IntPtrT> const capacity = var_capacity_.value();

    GotoIf(WordEqual(length, capacity), &next);

    var_array_ = ResizeFixedArray(length, length);
    var_capacity_ = length;
    Goto(&next);

    BIND(&next);
  }

  TNode<Smi> const result_length = SmiTag(length());
  TNode<JSArray> const result =
      CAST(AllocateUninitializedJSArrayWithoutElements(array_map, result_length,
                                                       nullptr));

  StoreObjectField(result, JSObject::kElementsOffset, var_array_.value());

  return result;
}

TNode<IntPtrT> GrowableFixedArray::NewCapacity(
    TNode<IntPtrT> current_capacity) {
  CSA_ASSERT(this,
             IntPtrGreaterThanOrEqual(current_capacity, IntPtrConstant(0)));

  // Growth rate is analog to JSObject::NewElementsCapacity:
  // new_capacity = (current_capacity + (current_capacity >> 1)) + 16.

  TNode<IntPtrT> const new_capacity =
      IntPtrAdd(IntPtrAdd(current_capacity, WordShr(current_capacity, 1)),
                IntPtrConstant(16));

  return new_capacity;
}

TNode<FixedArray> GrowableFixedArray::ResizeFixedArray(
    TNode<IntPtrT> const element_count, TNode<IntPtrT> const new_capacity) {
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(element_count, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(new_capacity, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(new_capacity, element_count));

  TNode<FixedArray> const from_array = var_array_.value();

  CodeStubAssembler::ExtractFixedArrayFlags flags;
  flags |= CodeStubAssembler::ExtractFixedArrayFlag::kFixedArrays;
  TNode<FixedArray> to_array = ExtractFixedArray(
      from_array, nullptr, element_count, new_capacity, flags);

  return to_array;
}

}  // namespace internal
}  // namespace v8

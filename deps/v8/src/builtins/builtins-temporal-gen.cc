// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/growable-fixed-array-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/js-temporal-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class TemporalBuiltinsAssembler : public IteratorBuiltinsAssembler {
 public:
  explicit TemporalBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : IteratorBuiltinsAssembler(state) {}

  // For the use inside Temporal GetPossibleInstantFor
  TNode<FixedArray> TemporalInstantFixedArrayFromIterable(
      TNode<Context> context, TNode<Object> iterable);
};

// #sec-iterabletolistoftype
TNode<FixedArray>
TemporalBuiltinsAssembler::TemporalInstantFixedArrayFromIterable(
    TNode<Context> context, TNode<Object> iterable) {
  GrowableFixedArray list(state());
  Label done(this);
  // 1. If iterable is undefined, then
  //   a. Return a new empty List.
  GotoIf(IsUndefined(iterable), &done);

  // 2. Let iteratorRecord be ? GetIterator(items).
  IteratorRecord iterator_record = GetIterator(context, iterable);

  // 3. Let list be a new empty List.

  Label loop_start(this,
                   {list.var_array(), list.var_length(), list.var_capacity()});
  Goto(&loop_start);
  // 4. Let next be true.
  // 5. Repeat, while next is not false
  Label if_isnottemporalinstant(this, Label::kDeferred),
      if_exception(this, Label::kDeferred);
  BIND(&loop_start);
  {
    //  a. Set next to ? IteratorStep(iteratorRecord).
    TNode<JSReceiver> next = IteratorStep(context, iterator_record, &done);
    //  b. If next is not false, then
    //   i. Let nextValue be ? IteratorValue(next).
    TNode<Object> next_value = IteratorValue(context, next);
    //   ii. If Type(nextValue) is not Object or nextValue does not have an
    //   [[InitializedTemporalInstant]] internal slot
    GotoIf(TaggedIsSmi(next_value), &if_isnottemporalinstant);
    TNode<Uint16T> next_value_type = LoadInstanceType(CAST(next_value));
    GotoIfNot(IsTemporalInstantInstanceType(next_value_type),
              &if_isnottemporalinstant);
    //   iii. Append nextValue to the end of the List list.
    list.Push(next_value);
    Goto(&loop_start);
    // 5.b.ii
    BIND(&if_isnottemporalinstant);
    {
      // 1. Let error be ThrowCompletion(a newly created TypeError object).
      TVARIABLE(Object, var_exception);
      {
        compiler::ScopedExceptionHandler handler(this, &if_exception,
                                                 &var_exception);
        CallRuntime(Runtime::kThrowTypeError, context,
                    SmiConstant(MessageTemplate::kIterableYieldedNonString),
                    next_value);
      }
      Unreachable();

      // 2. Return ? IteratorClose(iteratorRecord, error).
      BIND(&if_exception);
      IteratorCloseOnException(context, iterator_record);
      CallRuntime(Runtime::kReThrow, context, var_exception.value());
      Unreachable();
    }
  }

  BIND(&done);
  return list.ToFixedArray();
}

TF_BUILTIN(TemporalInstantFixedArrayFromIterable, TemporalBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto iterable = Parameter<Object>(Descriptor::kIterable);

  Return(TemporalInstantFixedArrayFromIterable(context, iterable));
}

}  // namespace internal
}  // namespace v8

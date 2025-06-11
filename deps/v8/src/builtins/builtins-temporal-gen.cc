// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/growable-fixed-array-gen.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/objects/js-temporal-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

class TemporalBuiltinsAssembler : public IteratorBuiltinsAssembler {
 public:
  explicit TemporalBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : IteratorBuiltinsAssembler(state) {}

  // For the use inside Temporal GetPossibleInstantFor
  TNode<FixedArray> TemporalInstantFixedArrayFromIterable(
      TNode<Context> context, TNode<JSAny> iterable);
};

// #sec-iterabletolistoftype
TNode<FixedArray>
TemporalBuiltinsAssembler::TemporalInstantFixedArrayFromIterable(
    TNode<Context> context, TNode<JSAny> iterable) {
  GrowableFixedArray list(state());
  Label done(this);
  // 1. If iterable is undefined, then
  //   a. Return a new empty List.
  GotoIf(IsUndefined(iterable), &done);

  // 2. Let iteratorRecord be ? GetIterator(items) (handled by Iterate).

  // 3. Let list be a new empty List.

  // 3. Let next be true. (handled by Iterate).
  // 4. Repeat, while next is not false (handled by Iterate).
  Iterate(context, iterable,
          [&](TNode<Object> next_value) {
            // Handled by Iterate:
            //  a. Set next to ? IteratorStep(iteratorRecord).
            //  b. If next is not false, then
            //   i. Let nextValue be ? IteratorValue(next).

            //   ii. If Type(nextValue) is not Object or nextValue does not have
            //   an [[InitializedTemporalInstant]] internal slot
            Label if_isnottemporalinstant(this, Label::kDeferred),
                loop_body_end(this);
            GotoIf(TaggedIsSmi(next_value), &if_isnottemporalinstant);
            TNode<Uint16T> next_value_type = LoadInstanceType(CAST(next_value));
            GotoIfNot(IsTemporalInstantInstanceType(next_value_type),
                      &if_isnottemporalinstant);

            //   iii. Append nextValue to the end of the List list.
            list.Push(next_value);
            Goto(&loop_body_end);

            // 5.b.ii
            BIND(&if_isnottemporalinstant);
            {
              // 1. Let error be ThrowCompletion(a newly created TypeError
              // object).
              CallRuntime(
                  Runtime::kThrowTypeError, context,
                  SmiConstant(MessageTemplate::kIterableYieldedNonString),
                  next_value);

              // 2. Return ? IteratorClose(iteratorRecord, error). (handled by
              // Iterate).
              Unreachable();
            }

            BIND(&loop_body_end);
          },
          {list.var_array(), list.var_length(), list.var_capacity()});

  Goto(&done);

  BIND(&done);
  return list.ToFixedArray();
}

TF_BUILTIN(TemporalInstantFixedArrayFromIterable, TemporalBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto iterable = Parameter<JSAny>(Descriptor::kIterable);

  Return(TemporalInstantFixedArrayFromIterable(context, iterable));
}


#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8

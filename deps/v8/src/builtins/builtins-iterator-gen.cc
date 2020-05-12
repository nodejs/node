// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/growable-fixed-array-gen.h"

#include "src/builtins/builtins-collections-gen.h"
#include "src/builtins/builtins-string-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/heap/factory-inl.h"

namespace v8 {
namespace internal {

using IteratorRecord = TorqueStructIteratorRecord;
using compiler::Node;

TNode<Object> IteratorBuiltinsAssembler::GetIteratorMethod(
    TNode<Context> context, TNode<Object> object) {
  return GetProperty(context, object, factory()->iterator_symbol());
}

IteratorRecord IteratorBuiltinsAssembler::GetIterator(TNode<Context> context,
                                                      TNode<Object> object) {
  TNode<Object> method = GetIteratorMethod(context, object);
  return GetIterator(context, object, method);
}

IteratorRecord IteratorBuiltinsAssembler::GetIterator(TNode<Context> context,
                                                      TNode<Object> object,
                                                      TNode<Object> method) {
  Label if_not_callable(this, Label::kDeferred), if_callable(this);
  GotoIf(TaggedIsSmi(method), &if_not_callable);
  Branch(IsCallable(CAST(method)), &if_callable, &if_not_callable);

  BIND(&if_not_callable);
  CallRuntime(Runtime::kThrowIteratorError, context, object);
  Unreachable();

  BIND(&if_callable);
  {
    TNode<Object> iterator = Call(context, method, object);

    Label get_next(this), if_notobject(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(iterator), &if_notobject);
    Branch(IsJSReceiver(CAST(iterator)), &get_next, &if_notobject);

    BIND(&if_notobject);
    CallRuntime(Runtime::kThrowSymbolIteratorInvalid, context);
    Unreachable();

    BIND(&get_next);
    TNode<Object> next =
        GetProperty(context, iterator, factory()->next_string());
    return IteratorRecord{TNode<JSReceiver>::UncheckedCast(iterator),
                          TNode<Object>::UncheckedCast(next)};
  }
}

TNode<JSReceiver> IteratorBuiltinsAssembler::IteratorStep(
    TNode<Context> context, const IteratorRecord& iterator, Label* if_done,
    base::Optional<TNode<Map>> fast_iterator_result_map) {
  DCHECK_NOT_NULL(if_done);
  // 1. a. Let result be ? Invoke(iterator, "next", « »).
  TNode<Object> result = Call(context, iterator.next, iterator.object);

  // 3. If Type(result) is not Object, throw a TypeError exception.
  Label if_notobject(this, Label::kDeferred), return_result(this);
  GotoIf(TaggedIsSmi(result), &if_notobject);
  TNode<HeapObject> heap_object_result = CAST(result);
  TNode<Map> result_map = LoadMap(heap_object_result);

  if (fast_iterator_result_map) {
    // Fast iterator result case:
    Label if_generic(this);

    // 4. Return result.
    GotoIfNot(TaggedEqual(result_map, *fast_iterator_result_map), &if_generic);

    // IteratorComplete
    // 2. Return ToBoolean(? Get(iterResult, "done")).
    TNode<Object> done =
        LoadObjectField(heap_object_result, JSIteratorResult::kDoneOffset);
    BranchIfToBooleanIsTrue(done, if_done, &return_result);

    BIND(&if_generic);
  }

  // Generic iterator result case:
  {
    // 3. If Type(result) is not Object, throw a TypeError exception.
    GotoIfNot(IsJSReceiverMap(result_map), &if_notobject);

    // IteratorComplete
    // 2. Return ToBoolean(? Get(iterResult, "done")).
    TNode<Object> done =
        GetProperty(context, heap_object_result, factory()->done_string());
    BranchIfToBooleanIsTrue(done, if_done, &return_result);
  }

  BIND(&if_notobject);
  CallRuntime(Runtime::kThrowIteratorResultNotAnObject, context, result);
  Unreachable();

  BIND(&return_result);
  return CAST(heap_object_result);
}

TNode<Object> IteratorBuiltinsAssembler::IteratorValue(
    TNode<Context> context, TNode<JSReceiver> result,
    base::Optional<TNode<Map>> fast_iterator_result_map) {
  Label exit(this);
  TVARIABLE(Object, var_value);
  if (fast_iterator_result_map) {
    // Fast iterator result case:
    Label if_generic(this);
    TNode<Map> map = LoadMap(result);
    GotoIfNot(TaggedEqual(map, *fast_iterator_result_map), &if_generic);
    var_value = LoadObjectField(result, JSIteratorResult::kValueOffset);
    Goto(&exit);

    BIND(&if_generic);
  }

  // Generic iterator result case:
  var_value = GetProperty(context, result, factory()->value_string());
  Goto(&exit);

  BIND(&exit);
  return var_value.value();
}

void IteratorBuiltinsAssembler::IteratorCloseOnException(
    TNode<Context> context, const IteratorRecord& iterator, Label* if_exception,
    TVariable<Object>* exception) {
  // Perform ES #sec-iteratorclose when an exception occurs. This simpler
  // algorithm does not include redundant steps which are never reachable from
  // the spec IteratorClose algorithm.
  DCHECK((if_exception != nullptr && exception != nullptr));
  CSA_ASSERT(this, IsNotTheHole(exception->value()));
  CSA_ASSERT(this, IsJSReceiver(iterator.object));

  // Let return be ? GetMethod(iterator, "return").
  TNode<Object> method;
  {
    compiler::ScopedExceptionHandler handler(this, if_exception, exception);
    method = GetProperty(context, iterator.object, factory()->return_string());
  }

  // If return is undefined, return Completion(completion).
  GotoIf(Word32Or(IsUndefined(method), IsNull(method)), if_exception);

  {
    // Let innerResult be Call(return, iterator, « »).
    // If an exception occurs, the original exception remains bound.
    compiler::ScopedExceptionHandler handler(this, if_exception, nullptr);
    Call(context, method, iterator.object);
  }

  // (If completion.[[Type]] is throw) return Completion(completion).
  Goto(if_exception);
}

void IteratorBuiltinsAssembler::IteratorCloseOnException(
    TNode<Context> context, const IteratorRecord& iterator,
    TNode<Object> exception) {
  Label rethrow(this, Label::kDeferred);
  TVARIABLE(Object, exception_variable, exception);
  IteratorCloseOnException(context, iterator, &rethrow, &exception_variable);

  BIND(&rethrow);
  CallRuntime(Runtime::kReThrow, context, exception_variable.value());
  Unreachable();
}

TNode<JSArray> IteratorBuiltinsAssembler::IterableToList(
    TNode<Context> context, TNode<Object> iterable, TNode<Object> iterator_fn) {
  // 1. Let iteratorRecord be ? GetIterator(items, method).
  IteratorRecord iterator_record = GetIterator(context, iterable, iterator_fn);

  // 2. Let values be a new empty List.
  GrowableFixedArray values(state());

  Label loop_start(
      this, {values.var_array(), values.var_length(), values.var_capacity()}),
      done(this);
  Goto(&loop_start);
  // 3. Let next be true.
  // 4. Repeat, while next is not false
  BIND(&loop_start);
  {
    //  a. Set next to ? IteratorStep(iteratorRecord).
    TNode<JSReceiver> next = IteratorStep(context, iterator_record, &done);
    //  b. If next is not false, then
    //   i. Let nextValue be ? IteratorValue(next).
    TNode<Object> next_value = IteratorValue(context, next);
    //   ii. Append nextValue to the end of the List values.
    values.Push(next_value);
    Goto(&loop_start);
  }

  BIND(&done);
  return values.ToJSArray(context);
}

TF_BUILTIN(IterableToList, IteratorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> iterable = CAST(Parameter(Descriptor::kIterable));
  TNode<Object> iterator_fn = CAST(Parameter(Descriptor::kIteratorFn));

  Return(IterableToList(context, iterable, iterator_fn));
}

TF_BUILTIN(IterableToFixedArrayForWasm, IteratorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> iterable = CAST(Parameter(Descriptor::kIterable));
  TNode<Smi> expected_length = CAST(Parameter(Descriptor::kExpectedLength));

  TNode<Object> iterator_fn = GetIteratorMethod(context, iterable);

  IteratorRecord iterator_record = GetIterator(context, iterable, iterator_fn);

  GrowableFixedArray values(state());

  Label loop_start(
      this, {values.var_array(), values.var_length(), values.var_capacity()}),
      compare_length(this), done(this);
  Goto(&loop_start);
  BIND(&loop_start);
  {
    TNode<JSReceiver> next =
        IteratorStep(context, iterator_record, &compare_length);
    TNode<Object> next_value = IteratorValue(context, next);
    values.Push(next_value);
    Goto(&loop_start);
  }

  BIND(&compare_length);
  GotoIf(WordEqual(SmiUntag(expected_length), values.var_length()->value()),
         &done);
  Return(CallRuntime(
      Runtime::kThrowTypeError, context,
      SmiConstant(MessageTemplate::kWasmTrapMultiReturnLengthMismatch)));

  BIND(&done);
  Return(values.var_array()->value());
}

TNode<JSArray> IteratorBuiltinsAssembler::StringListFromIterable(
    TNode<Context> context, TNode<Object> iterable) {
  Label done(this);
  GrowableFixedArray list(state());
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
  Label if_isnotstringtype(this, Label::kDeferred),
      if_exception(this, Label::kDeferred);
  BIND(&loop_start);
  {
    //  a. Set next to ? IteratorStep(iteratorRecord).
    TNode<JSReceiver> next = IteratorStep(context, iterator_record, &done);
    //  b. If next is not false, then
    //   i. Let nextValue be ? IteratorValue(next).
    TNode<Object> next_value = IteratorValue(context, next);
    //   ii. If Type(nextValue) is not String, then
    GotoIf(TaggedIsSmi(next_value), &if_isnotstringtype);
    TNode<Uint16T> next_value_type = LoadInstanceType(CAST(next_value));
    GotoIfNot(IsStringInstanceType(next_value_type), &if_isnotstringtype);
    //   iii. Append nextValue to the end of the List list.
    list.Push(next_value);
    Goto(&loop_start);
    // 5.b.ii
    BIND(&if_isnotstringtype);
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
      IteratorCloseOnException(context, iterator_record, var_exception.value());
    }
  }

  BIND(&done);
  // 6. Return list.
  return list.ToJSArray(context);
}

TF_BUILTIN(StringListFromIterable, IteratorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> iterable = CAST(Parameter(Descriptor::kIterable));

  Return(StringListFromIterable(context, iterable));
}

// This builtin always returns a new JSArray and is thus safe to use even in the
// presence of code that may call back into user-JS. This builtin will take the
// fast path if the iterable is a fast array and the Array prototype and the
// Symbol.iterator is untouched. The fast path skips the iterator and copies the
// backing store to the new array. Note that if the array has holes, the holes
// will be copied to the new array, which is inconsistent with the behavior of
// an actual iteration, where holes should be replaced with undefined (if the
// prototype has no elements). To maintain the correct behavior for holey
// arrays, use the builtins IterableToList or IterableToListWithSymbolLookup.
TF_BUILTIN(IterableToListMayPreserveHoles, IteratorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> iterable = CAST(Parameter(Descriptor::kIterable));
  TNode<Object> iterator_fn = CAST(Parameter(Descriptor::kIteratorFn));

  Label slow_path(this);

  GotoIfNot(IsFastJSArrayWithNoCustomIteration(context, iterable), &slow_path);

  // The fast path will copy holes to the new array.
  TailCallBuiltin(Builtins::kCloneFastJSArray, context, iterable);

  BIND(&slow_path);
  TailCallBuiltin(Builtins::kIterableToList, context, iterable, iterator_fn);
}

void IteratorBuiltinsAssembler::FastIterableToList(
    TNode<Context> context, TNode<Object> iterable,
    TVariable<JSArray>* var_result, Label* slow) {
  Label done(this), check_string(this), check_map(this), check_set(this);

  GotoIfNot(
      Word32Or(IsFastJSArrayWithNoCustomIteration(context, iterable),
               IsFastJSArrayForReadWithNoCustomIteration(context, iterable)),
      &check_string);

  // Fast path for fast JSArray.
  *var_result = CAST(
      CallBuiltin(Builtins::kCloneFastJSArrayFillingHoles, context, iterable));
  Goto(&done);

  BIND(&check_string);
  {
    Label string_maybe_fast_call(this);
    StringBuiltinsAssembler string_assembler(state());
    string_assembler.BranchIfStringPrimitiveWithNoCustomIteration(
        iterable, context, &string_maybe_fast_call, &check_map);

    BIND(&string_maybe_fast_call);
    const TNode<IntPtrT> length = LoadStringLengthAsWord(CAST(iterable));
    // Use string length as conservative approximation of number of codepoints.
    GotoIf(
        IntPtrGreaterThan(length, IntPtrConstant(JSArray::kMaxFastArrayLength)),
        slow);
    *var_result = CAST(CallBuiltin(Builtins::kStringToList, context, iterable));
    Goto(&done);
  }

  BIND(&check_map);
  {
    Label map_fast_call(this);
    BranchIfIterableWithOriginalKeyOrValueMapIterator(
        state(), iterable, context, &map_fast_call, &check_set);

    BIND(&map_fast_call);
    *var_result =
        CAST(CallBuiltin(Builtins::kMapIteratorToList, context, iterable));
    Goto(&done);
  }

  BIND(&check_set);
  {
    Label set_fast_call(this);
    BranchIfIterableWithOriginalValueSetIterator(state(), iterable, context,
                                                 &set_fast_call, slow);

    BIND(&set_fast_call);
    *var_result =
        CAST(CallBuiltin(Builtins::kSetOrSetIteratorToList, context, iterable));
    Goto(&done);
  }

  BIND(&done);
}

TNode<JSArray> IteratorBuiltinsAssembler::FastIterableToList(
    TNode<Context> context, TNode<Object> iterable, Label* slow) {
  TVARIABLE(JSArray, var_fast_result);
  FastIterableToList(context, iterable, &var_fast_result, slow);
  return var_fast_result.value();
}

// This builtin loads the property Symbol.iterator as the iterator, and has fast
// paths for fast arrays, for primitive strings, for sets and set iterators, and
// for map iterators. These fast paths will only be taken if Symbol.iterator and
// the Iterator prototype are not modified in a way that changes the original
// iteration behavior.
// * In case of fast holey arrays, holes will be converted to undefined to
//   reflect iteration semantics. Note that replacement by undefined is only
//   correct when the NoElements protector is valid.
// * In case of map/set iterators, there is an additional requirement that the
//   iterator is not partially consumed. To be spec-compliant, after spreading
//   the iterator is set to be exhausted.
TF_BUILTIN(IterableToListWithSymbolLookup, IteratorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> iterable = CAST(Parameter(Descriptor::kIterable));

  Label slow_path(this);

  GotoIfForceSlowPath(&slow_path);

  TVARIABLE(JSArray, var_result);
  FastIterableToList(context, iterable, &var_result, &slow_path);
  Return(var_result.value());

  BIND(&slow_path);
  {
    TNode<Object> iterator_fn = GetIteratorMethod(context, iterable);
    TailCallBuiltin(Builtins::kIterableToList, context, iterable, iterator_fn);
  }
}

TF_BUILTIN(GetIteratorWithFeedbackLazyDeoptContinuation,
           IteratorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  // TODO(v8:10047): Use TaggedIndex here once TurboFan supports it.
  TNode<Smi> call_slot_smi = CAST(Parameter(Descriptor::kCallSlot));
  TNode<TaggedIndex> call_slot = SmiToTaggedIndex(call_slot_smi);
  TNode<FeedbackVector> feedback = CAST(Parameter(Descriptor::kFeedback));
  TNode<Object> iterator_method = CAST(Parameter(Descriptor::kResult));

  TNode<Object> result =
      CallBuiltin(Builtins::kCallIteratorWithFeedback, context, receiver,
                  iterator_method, call_slot, feedback);
  Return(result);
}

}  // namespace internal
}  // namespace v8

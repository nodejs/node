// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-array-gen.h"

#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-string-gen.h"
#include "src/builtins/builtins-typed-array-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/frame-constants.h"
#include "src/heap/factory-inl.h"
#include "src/objects/arguments-inl.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;

ArrayBuiltinsAssembler::ArrayBuiltinsAssembler(
    compiler::CodeAssemblerState* state)
    : BaseBuiltinsFromDSLAssembler(state),
      k_(this, MachineRepresentation::kTagged),
      a_(this, MachineRepresentation::kTagged),
      to_(this, MachineRepresentation::kTagged, SmiConstant(0)),
      fully_spec_compliant_(this, {&k_, &a_, &to_}) {}

void ArrayBuiltinsAssembler::FindResultGenerator() {
  a_.Bind(UndefinedConstant());
}

Node* ArrayBuiltinsAssembler::FindProcessor(Node* k_value, Node* k) {
  Node* value = CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                       this_arg(), k_value, k, o());
  Label false_continue(this), return_true(this);
  BranchIfToBooleanIsTrue(value, &return_true, &false_continue);
  BIND(&return_true);
  ReturnFromBuiltin(k_value);
  BIND(&false_continue);
  return a();
  }

  void ArrayBuiltinsAssembler::FindIndexResultGenerator() {
    a_.Bind(SmiConstant(-1));
  }

  Node* ArrayBuiltinsAssembler::FindIndexProcessor(Node* k_value, Node* k) {
    Node* value = CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                         this_arg(), k_value, k, o());
    Label false_continue(this), return_true(this);
    BranchIfToBooleanIsTrue(value, &return_true, &false_continue);
    BIND(&return_true);
    ReturnFromBuiltin(k);
    BIND(&false_continue);
    return a();
  }

  void ArrayBuiltinsAssembler::ForEachResultGenerator() {
    a_.Bind(UndefinedConstant());
  }

  Node* ArrayBuiltinsAssembler::ForEachProcessor(Node* k_value, Node* k) {
    CallJS(CodeFactory::Call(isolate()), context(), callbackfn(), this_arg(),
           k_value, k, o());
    return a();
  }

  void ArrayBuiltinsAssembler::SomeResultGenerator() {
    a_.Bind(FalseConstant());
  }

  Node* ArrayBuiltinsAssembler::SomeProcessor(Node* k_value, Node* k) {
    Node* value = CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                         this_arg(), k_value, k, o());
    Label false_continue(this), return_true(this);
    BranchIfToBooleanIsTrue(value, &return_true, &false_continue);
    BIND(&return_true);
    ReturnFromBuiltin(TrueConstant());
    BIND(&false_continue);
    return a();
  }

  void ArrayBuiltinsAssembler::EveryResultGenerator() {
    a_.Bind(TrueConstant());
  }

  Node* ArrayBuiltinsAssembler::EveryProcessor(Node* k_value, Node* k) {
    Node* value = CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                         this_arg(), k_value, k, o());
    Label true_continue(this), return_false(this);
    BranchIfToBooleanIsTrue(value, &true_continue, &return_false);
    BIND(&return_false);
    ReturnFromBuiltin(FalseConstant());
    BIND(&true_continue);
    return a();
  }

  void ArrayBuiltinsAssembler::ReduceResultGenerator() {
    return a_.Bind(this_arg());
  }

  Node* ArrayBuiltinsAssembler::ReduceProcessor(Node* k_value, Node* k) {
    VARIABLE(result, MachineRepresentation::kTagged);
    Label done(this, {&result}), initial(this);
    GotoIf(WordEqual(a(), TheHoleConstant()), &initial);
    result.Bind(CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                       UndefinedConstant(), a(), k_value, k, o()));
    Goto(&done);

    BIND(&initial);
    result.Bind(k_value);
    Goto(&done);

    BIND(&done);
    return result.value();
  }

  void ArrayBuiltinsAssembler::ReducePostLoopAction() {
    Label ok(this);
    GotoIf(WordNotEqual(a(), TheHoleConstant()), &ok);
    ThrowTypeError(context(), MessageTemplate::kReduceNoInitial);
    BIND(&ok);
  }

  void ArrayBuiltinsAssembler::FilterResultGenerator() {
    // 7. Let A be ArraySpeciesCreate(O, 0).
    // This version of ArraySpeciesCreate will create with the correct
    // ElementsKind in the fast case.
    GenerateArraySpeciesCreate();
  }

  Node* ArrayBuiltinsAssembler::FilterProcessor(Node* k_value, Node* k) {
    // ii. Let selected be ToBoolean(? Call(callbackfn, T, kValue, k, O)).
    Node* selected = CallJS(CodeFactory::Call(isolate()), context(),
                            callbackfn(), this_arg(), k_value, k, o());
    Label true_continue(this, &to_), false_continue(this);
    BranchIfToBooleanIsTrue(selected, &true_continue, &false_continue);
    BIND(&true_continue);
    // iii. If selected is true, then...
    {
      Label after_work(this, &to_);
      Node* kind = nullptr;

      // If a() is a JSArray, we can have a fast path.
      Label fast(this);
      Label runtime(this);
      Label object_push_pre(this), object_push(this), double_push(this);
      BranchIfFastJSArray(a(), context(), &fast, &runtime);

      BIND(&fast);
      {
        GotoIf(WordNotEqual(LoadJSArrayLength(a()), to_.value()), &runtime);
        kind = EnsureArrayPushable(LoadMap(a()), &runtime);
        GotoIf(IsElementsKindGreaterThan(kind, HOLEY_SMI_ELEMENTS),
               &object_push_pre);

        BuildAppendJSArray(HOLEY_SMI_ELEMENTS, a(), k_value, &runtime);
        Goto(&after_work);
      }

      BIND(&object_push_pre);
      {
        Branch(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS), &double_push,
               &object_push);
      }

      BIND(&object_push);
      {
        BuildAppendJSArray(HOLEY_ELEMENTS, a(), k_value, &runtime);
        Goto(&after_work);
      }

      BIND(&double_push);
      {
        BuildAppendJSArray(HOLEY_DOUBLE_ELEMENTS, a(), k_value, &runtime);
        Goto(&after_work);
      }

      BIND(&runtime);
      {
        // 1. Perform ? CreateDataPropertyOrThrow(A, ToString(to), kValue).
        CallRuntime(Runtime::kCreateDataProperty, context(), a(), to_.value(),
                    k_value);
        Goto(&after_work);
      }

      BIND(&after_work);
      {
        // 2. Increase to by 1.
        to_.Bind(NumberInc(to_.value()));
        Goto(&false_continue);
      }
    }
    BIND(&false_continue);
    return a();
  }

  void ArrayBuiltinsAssembler::MapResultGenerator() {
    GenerateArraySpeciesCreate(len_);
  }

  void ArrayBuiltinsAssembler::TypedArrayMapResultGenerator() {
    // 6. Let A be ? TypedArraySpeciesCreate(O, len).
    TNode<JSTypedArray> original_array = CAST(o());
    TNode<Smi> length = CAST(len_);
    const char* method_name = "%TypedArray%.prototype.map";

    TypedArrayBuiltinsAssembler typedarray_asm(state());
    TNode<JSTypedArray> a = typedarray_asm.SpeciesCreateByLength(
        context(), original_array, length, method_name);
    // In the Spec and our current implementation, the length check is already
    // performed in TypedArraySpeciesCreate.
    CSA_ASSERT(this, SmiLessThanOrEqual(CAST(len_), LoadTypedArrayLength(a)));
    fast_typed_array_target_ =
        Word32Equal(LoadInstanceType(LoadElements(original_array)),
                    LoadInstanceType(LoadElements(a)));
    a_.Bind(a);
  }

  Node* ArrayBuiltinsAssembler::SpecCompliantMapProcessor(Node* k_value,
                                                          Node* k) {
    //  i. Let kValue be ? Get(O, Pk). Performed by the caller of
    //  SpecCompliantMapProcessor.
    // ii. Let mapped_value be ? Call(callbackfn, T, kValue, k, O).
    Node* mapped_value = CallJS(CodeFactory::Call(isolate()), context(),
                                callbackfn(), this_arg(), k_value, k, o());

    // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mapped_value).
    CallRuntime(Runtime::kCreateDataProperty, context(), a(), k, mapped_value);
    return a();
  }

  Node* ArrayBuiltinsAssembler::FastMapProcessor(Node* k_value, Node* k) {
    //  i. Let kValue be ? Get(O, Pk). Performed by the caller of
    //  FastMapProcessor.
    // ii. Let mapped_value be ? Call(callbackfn, T, kValue, k, O).
    Node* mapped_value = CallJS(CodeFactory::Call(isolate()), context(),
                                callbackfn(), this_arg(), k_value, k, o());

    // mode is SMI_PARAMETERS because k has tagged representation.
    ParameterMode mode = SMI_PARAMETERS;
    Label runtime(this), finished(this);
    Label transition_pre(this), transition_smi_fast(this),
        transition_smi_double(this);
    Label array_not_smi(this), array_fast(this), array_double(this);

    TNode<Int32T> kind = LoadElementsKind(a());
    Node* elements = LoadElements(a());
    GotoIf(IsElementsKindGreaterThan(kind, HOLEY_SMI_ELEMENTS), &array_not_smi);
    TryStoreArrayElement(HOLEY_SMI_ELEMENTS, mode, &transition_pre, elements, k,
                         mapped_value);
    Goto(&finished);

    BIND(&transition_pre);
    {
      // array is smi. Value is either tagged or a heap number.
      CSA_ASSERT(this, TaggedIsNotSmi(mapped_value));
      GotoIf(IsHeapNumberMap(LoadMap(mapped_value)), &transition_smi_double);
      Goto(&transition_smi_fast);
    }

    BIND(&array_not_smi);
    {
      Branch(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS), &array_double,
             &array_fast);
    }

    BIND(&transition_smi_fast);
    {
      // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mapped_value).
      Node* const native_context = LoadNativeContext(context());
      Node* const fast_map = LoadContextElement(
          native_context, Context::JS_ARRAY_HOLEY_ELEMENTS_MAP_INDEX);

      // Since this transition is only a map change, just do it right here.
      // Since a() doesn't have an allocation site, it's safe to do the
      // map store directly, otherwise I'd call TransitionElementsKind().
      StoreMap(a(), fast_map);
      Goto(&array_fast);
    }

    BIND(&array_fast);
    {
      TryStoreArrayElement(HOLEY_ELEMENTS, mode, &runtime, elements, k,
                           mapped_value);
      Goto(&finished);
    }

    BIND(&transition_smi_double);
    {
      // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mapped_value).
      Node* const native_context = LoadNativeContext(context());
      Node* const double_map = LoadContextElement(
          native_context, Context::JS_ARRAY_HOLEY_DOUBLE_ELEMENTS_MAP_INDEX);

      const ElementsKind kFromKind = HOLEY_SMI_ELEMENTS;
      const ElementsKind kToKind = HOLEY_DOUBLE_ELEMENTS;
      const bool kIsJSArray = true;

      Label transition_in_runtime(this, Label::kDeferred);
      TransitionElementsKind(a(), double_map, kFromKind, kToKind, kIsJSArray,
                             &transition_in_runtime);
      Goto(&array_double);

      BIND(&transition_in_runtime);
      CallRuntime(Runtime::kTransitionElementsKind, context(), a(), double_map);
      Goto(&array_double);
    }

    BIND(&array_double);
    {
      // TODO(mvstanton): If we use a variable for elements and bind it
      // appropriately, we can avoid an extra load of elements by binding the
      // value only after a transition from smi to double.
      elements = LoadElements(a());
      // If the mapped_value isn't a number, this will bail out to the runtime
      // to make the transition.
      TryStoreArrayElement(HOLEY_DOUBLE_ELEMENTS, mode, &runtime, elements, k,
                           mapped_value);
      Goto(&finished);
    }

    BIND(&runtime);
    {
      // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mapped_value).
      CallRuntime(Runtime::kCreateDataProperty, context(), a(), k,
                  mapped_value);
      Goto(&finished);
    }

    BIND(&finished);
    return a();
  }

  // See tc39.github.io/ecma262/#sec-%typedarray%.prototype.map.
  Node* ArrayBuiltinsAssembler::TypedArrayMapProcessor(Node* k_value, Node* k) {
    // 8. c. Let mapped_value be ? Call(callbackfn, T, « kValue, k, O »).
    Node* mapped_value = CallJS(CodeFactory::Call(isolate()), context(),
                                callbackfn(), this_arg(), k_value, k, o());
    Label fast(this), slow(this), done(this), detached(this, Label::kDeferred);

    // 8. d. Perform ? Set(A, Pk, mapped_value, true).
    // Since we know that A is a TypedArray, this always ends up in
    // #sec-integer-indexed-exotic-objects-set-p-v-receiver and then
    // tc39.github.io/ecma262/#sec-integerindexedelementset .
    Branch(fast_typed_array_target_, &fast, &slow);

    BIND(&fast);
    // #sec-integerindexedelementset
    // 5. If arrayTypeName is "BigUint64Array" or "BigInt64Array", let
    // numValue be ? ToBigInt(v).
    // 6. Otherwise, let numValue be ? ToNumber(value).
    Node* num_value;
    if (source_elements_kind_ == BIGINT64_ELEMENTS ||
        source_elements_kind_ == BIGUINT64_ELEMENTS) {
      num_value = ToBigInt(context(), mapped_value);
    } else {
      num_value = ToNumber_Inline(context(), mapped_value);
    }
    // The only way how this can bailout is because of a detached buffer.
    EmitElementStore(a(), k, num_value, false, source_elements_kind_,
                     KeyedAccessStoreMode::STANDARD_STORE, &detached,
                     context());
    Goto(&done);

    BIND(&slow);
    SetPropertyStrict(context(), CAST(a()), CAST(k), CAST(mapped_value));
    Goto(&done);

    BIND(&detached);
    // tc39.github.io/ecma262/#sec-integerindexedelementset
    // 8. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
    ThrowTypeError(context_, MessageTemplate::kDetachedOperation, name_);

    BIND(&done);
    return a();
  }

  void ArrayBuiltinsAssembler::NullPostLoopAction() {}

  void ArrayBuiltinsAssembler::FillFixedArrayWithSmiZero(
      TNode<FixedArray> array, TNode<Smi> smi_length) {
    CSA_ASSERT(this, Word32BinaryNot(IsFixedDoubleArray(array)));

    TNode<IntPtrT> length = SmiToIntPtr(smi_length);
    TNode<WordT> byte_length = TimesPointerSize(length);
    CSA_ASSERT(this, UintPtrLessThan(length, byte_length));

    static const int32_t fa_base_data_offset =
        FixedArray::kHeaderSize - kHeapObjectTag;
    TNode<IntPtrT> backing_store = IntPtrAdd(
        BitcastTaggedToWord(array), IntPtrConstant(fa_base_data_offset));

    // Call out to memset to perform initialization.
    TNode<ExternalReference> memset =
        ExternalConstant(ExternalReference::libc_memset_function());
    STATIC_ASSERT(kSizetSize == kIntptrSize);
    CallCFunction3(MachineType::Pointer(), MachineType::Pointer(),
                   MachineType::IntPtr(), MachineType::UintPtr(), memset,
                   backing_store, IntPtrConstant(0), byte_length);
  }

  void ArrayBuiltinsAssembler::ReturnFromBuiltin(Node* value) {
    if (argc_ == nullptr) {
      Return(value);
    } else {
      // argc_ doesn't include the receiver, so it has to be added back in
      // manually.
      PopAndReturn(IntPtrAdd(argc_, IntPtrConstant(1)), value);
    }
  }

  void ArrayBuiltinsAssembler::InitIteratingArrayBuiltinBody(
      TNode<Context> context, TNode<Object> receiver, Node* callbackfn,
      Node* this_arg, TNode<IntPtrT> argc) {
    context_ = context;
    receiver_ = receiver;
    callbackfn_ = callbackfn;
    this_arg_ = this_arg;
    argc_ = argc;
  }

  void ArrayBuiltinsAssembler::GenerateIteratingArrayBuiltinBody(
      const char* name, const BuiltinResultGenerator& generator,
      const CallResultProcessor& processor, const PostLoopAction& action,
      const Callable& slow_case_continuation,
      MissingPropertyMode missing_property_mode, ForEachDirection direction) {
    Label non_array(this), array_changes(this, {&k_, &a_, &to_});

    // TODO(danno): Seriously? Do we really need to throw the exact error
    // message on null and undefined so that the webkit tests pass?
    Label throw_null_undefined_exception(this, Label::kDeferred);
    GotoIf(IsNullOrUndefined(receiver()), &throw_null_undefined_exception);

    // By the book: taken directly from the ECMAScript 2015 specification

    // 1. Let O be ToObject(this value).
    // 2. ReturnIfAbrupt(O)
    o_ = ToObject_Inline(context(), receiver());

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    TVARIABLE(Number, merged_length);
    Label has_length(this, &merged_length), not_js_array(this);
    GotoIf(DoesntHaveInstanceType(o(), JS_ARRAY_TYPE), &not_js_array);
    merged_length = LoadJSArrayLength(CAST(o()));
    Goto(&has_length);

    BIND(&not_js_array);
    {
      Node* len_property =
          GetProperty(context(), o(), isolate()->factory()->length_string());
      merged_length = ToLength_Inline(context(), len_property);
      Goto(&has_length);
    }
    BIND(&has_length);
    {
      len_ = merged_length.value();

      // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
      Label type_exception(this, Label::kDeferred);
      Label done(this);
      GotoIf(TaggedIsSmi(callbackfn()), &type_exception);
      Branch(IsCallableMap(LoadMap(callbackfn())), &done, &type_exception);

      BIND(&throw_null_undefined_exception);
      ThrowTypeError(context(), MessageTemplate::kCalledOnNullOrUndefined,
                     name);

      BIND(&type_exception);
      ThrowTypeError(context(), MessageTemplate::kCalledNonCallable,
                     callbackfn());

      BIND(&done);
    }

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    // [Already done by the arguments adapter]

    if (direction == ForEachDirection::kForward) {
      // 7. Let k be 0.
      k_.Bind(SmiConstant(0));
    } else {
      k_.Bind(NumberDec(len()));
    }

    generator(this);

    HandleFastElements(processor, action, &fully_spec_compliant_, direction,
                       missing_property_mode);

    BIND(&fully_spec_compliant_);

    Node* result =
        CallStub(slow_case_continuation, context(), receiver(), callbackfn(),
                 this_arg(), a_.value(), o(), k_.value(), len(), to_.value());
    ReturnFromBuiltin(result);
  }

  void ArrayBuiltinsAssembler::InitIteratingArrayBuiltinLoopContinuation(
      TNode<Context> context, TNode<Object> receiver, Node* callbackfn,
      Node* this_arg, Node* a, TNode<JSReceiver> o, Node* initial_k,
      TNode<Number> len, Node* to) {
    context_ = context;
    this_arg_ = this_arg;
    callbackfn_ = callbackfn;
    a_.Bind(a);
    k_.Bind(initial_k);
    o_ = o;
    len_ = len;
    to_.Bind(to);
  }

  void ArrayBuiltinsAssembler::GenerateIteratingTypedArrayBuiltinBody(
      const char* name, const BuiltinResultGenerator& generator,
      const CallResultProcessor& processor, const PostLoopAction& action,
      ForEachDirection direction) {
    name_ = name;

    // ValidateTypedArray: tc39.github.io/ecma262/#sec-validatetypedarray

    Label throw_not_typed_array(this, Label::kDeferred);

    GotoIf(TaggedIsSmi(receiver_), &throw_not_typed_array);
    GotoIfNot(HasInstanceType(CAST(receiver_), JS_TYPED_ARRAY_TYPE),
              &throw_not_typed_array);

    TNode<JSTypedArray> typed_array = CAST(receiver_);
    o_ = typed_array;

    TNode<JSArrayBuffer> array_buffer = LoadArrayBufferViewBuffer(typed_array);
    ThrowIfArrayBufferIsDetached(context_, array_buffer, name_);

    len_ = LoadTypedArrayLength(typed_array);

    Label throw_not_callable(this, Label::kDeferred);
    Label distinguish_types(this);
    GotoIf(TaggedIsSmi(callbackfn_), &throw_not_callable);
    Branch(IsCallableMap(LoadMap(callbackfn_)), &distinguish_types,
           &throw_not_callable);

    BIND(&throw_not_typed_array);
    ThrowTypeError(context_, MessageTemplate::kNotTypedArray);

    BIND(&throw_not_callable);
    ThrowTypeError(context_, MessageTemplate::kCalledNonCallable, callbackfn_);

    Label unexpected_instance_type(this);
    BIND(&unexpected_instance_type);
    Unreachable();

    std::vector<int32_t> instance_types = {
#define INSTANCE_TYPE(Type, type, TYPE, ctype) FIXED_##TYPE##_ARRAY_TYPE,
        TYPED_ARRAYS(INSTANCE_TYPE)
#undef INSTANCE_TYPE
    };
    std::vector<Label> labels;
    for (size_t i = 0; i < instance_types.size(); ++i) {
      labels.push_back(Label(this));
    }
    std::vector<Label*> label_ptrs;
    for (Label& label : labels) {
      label_ptrs.push_back(&label);
    }

    BIND(&distinguish_types);

    generator(this);

    if (direction == ForEachDirection::kForward) {
      k_.Bind(SmiConstant(0));
    } else {
      k_.Bind(NumberDec(len()));
    }
    CSA_ASSERT(this, IsSafeInteger(k()));
    Node* instance_type = LoadInstanceType(LoadElements(typed_array));
    Switch(instance_type, &unexpected_instance_type, instance_types.data(),
           label_ptrs.data(), labels.size());

    for (size_t i = 0; i < labels.size(); ++i) {
      BIND(&labels[i]);
      Label done(this);
      source_elements_kind_ = ElementsKindForInstanceType(
          static_cast<InstanceType>(instance_types[i]));
      // TODO(tebbi): Silently cancelling the loop on buffer detachment is a
      // spec violation. Should go to &throw_detached and throw a TypeError
      // instead.
      VisitAllTypedArrayElements(array_buffer, processor, &done, direction,
                                 typed_array);
      Goto(&done);
      // No exception, return success
      BIND(&done);
      action(this);
      ReturnFromBuiltin(a_.value());
    }
  }

  void ArrayBuiltinsAssembler::GenerateIteratingArrayBuiltinLoopContinuation(
      const CallResultProcessor& processor, const PostLoopAction& action,
      MissingPropertyMode missing_property_mode, ForEachDirection direction) {
    Label loop(this, {&k_, &a_, &to_});
    Label after_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      if (direction == ForEachDirection::kForward) {
        // 8. Repeat, while k < len
        GotoIfNumberGreaterThanOrEqual(k(), len_, &after_loop);
      } else {
        // OR
        // 10. Repeat, while k >= 0
        GotoIfNumberGreaterThanOrEqual(SmiConstant(-1), k(), &after_loop);
      }

      Label done_element(this, &to_);
      // a. Let Pk be ToString(k).
      // k() is guaranteed to be a positive integer, hence ToString is
      // side-effect free and HasProperty/GetProperty do the conversion inline.
      CSA_ASSERT(this, IsSafeInteger(k()));

      if (missing_property_mode == MissingPropertyMode::kSkip) {
        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        TNode<Oddball> k_present =
            HasProperty(context(), o(), k(), kHasProperty);

        // d. If kPresent is true, then
        GotoIf(IsFalse(k_present), &done_element);
      }

      // i. Let kValue be Get(O, Pk).
      // ii. ReturnIfAbrupt(kValue).
      Node* k_value = GetProperty(context(), o(), k());

      // iii. Let funcResult be Call(callbackfn, T, «kValue, k, O»).
      // iv. ReturnIfAbrupt(funcResult).
      a_.Bind(processor(this, k_value, k()));
      Goto(&done_element);

      BIND(&done_element);

      if (direction == ForEachDirection::kForward) {
        // e. Increase k by 1.
        k_.Bind(NumberInc(k()));
      } else {
        // e. Decrease k by 1.
        k_.Bind(NumberDec(k()));
      }
      Goto(&loop);
    }
    BIND(&after_loop);

    action(this);
    Return(a_.value());
  }

  ElementsKind ArrayBuiltinsAssembler::ElementsKindForInstanceType(
      InstanceType type) {
    switch (type) {
#define INSTANCE_TYPE_TO_ELEMENTS_KIND(Type, type, TYPE, ctype) \
  case FIXED_##TYPE##_ARRAY_TYPE:                               \
    return TYPE##_ELEMENTS;

      TYPED_ARRAYS(INSTANCE_TYPE_TO_ELEMENTS_KIND)
#undef INSTANCE_TYPE_TO_ELEMENTS_KIND

      default:
        UNREACHABLE();
    }
  }

  void ArrayBuiltinsAssembler::VisitAllTypedArrayElements(
      Node* array_buffer, const CallResultProcessor& processor, Label* detached,
      ForEachDirection direction, TNode<JSTypedArray> typed_array) {
    VariableList list({&a_, &k_, &to_}, zone());

    FastLoopBody body = [&](Node* index) {
      GotoIf(IsDetachedBuffer(array_buffer), detached);
      Node* elements = LoadElements(typed_array);
      Node* base_ptr =
          LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
      Node* external_ptr =
          LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                          MachineType::Pointer());
      Node* data_ptr = IntPtrAdd(BitcastTaggedToWord(base_ptr), external_ptr);
      Node* value = LoadFixedTypedArrayElementAsTagged(
          data_ptr, index, source_elements_kind_, SMI_PARAMETERS);
      k_.Bind(index);
      a_.Bind(processor(this, value, index));
    };
    Node* start = SmiConstant(0);
    Node* end = len_;
    IndexAdvanceMode advance_mode = IndexAdvanceMode::kPost;
    int incr = 1;
    if (direction == ForEachDirection::kReverse) {
      std::swap(start, end);
      advance_mode = IndexAdvanceMode::kPre;
      incr = -1;
    }
    BuildFastLoop(list, start, end, body, incr, ParameterMode::SMI_PARAMETERS,
                  advance_mode);
  }

  void ArrayBuiltinsAssembler::VisitAllFastElementsOneKind(
      ElementsKind kind, const CallResultProcessor& processor,
      Label* array_changed, ParameterMode mode, ForEachDirection direction,
      MissingPropertyMode missing_property_mode, TNode<Smi> length) {
    Comment("begin VisitAllFastElementsOneKind");
    // We only use this kind of processing if the no-elements protector is
    // in place at the start. We'll continue checking during array iteration.
    CSA_ASSERT(this, Word32BinaryNot(IsNoElementsProtectorCellInvalid()));
    VARIABLE(original_map, MachineRepresentation::kTagged);
    original_map.Bind(LoadMap(o()));
    VariableList list({&original_map, &a_, &k_, &to_}, zone());
    Node* start = IntPtrOrSmiConstant(0, mode);
    Node* end = TaggedToParameter(length, mode);
    IndexAdvanceMode advance_mode = direction == ForEachDirection::kReverse
                                        ? IndexAdvanceMode::kPre
                                        : IndexAdvanceMode::kPost;
    if (direction == ForEachDirection::kReverse) std::swap(start, end);
    BuildFastLoop(
        list, start, end,
        [=, &original_map](Node* index) {
          k_.Bind(ParameterToTagged(index, mode));
          Label one_element_done(this), hole_element(this),
              process_element(this);

          // Check if o's map has changed during the callback. If so, we have to
          // fall back to the slower spec implementation for the rest of the
          // iteration.
          Node* o_map = LoadMap(o());
          GotoIf(WordNotEqual(o_map, original_map.value()), array_changed);

          TNode<JSArray> o_array = CAST(o());
          // Check if o's length has changed during the callback and if the
          // index is now out of range of the new length.
          GotoIf(SmiGreaterThanOrEqual(CAST(k_.value()),
                                       CAST(LoadJSArrayLength(o_array))),
                 array_changed);

          // Re-load the elements array. If may have been resized.
          Node* elements = LoadElements(o_array);

          // Fast case: load the element directly from the elements FixedArray
          // and call the callback if the element is not the hole.
          DCHECK(kind == PACKED_ELEMENTS || kind == PACKED_DOUBLE_ELEMENTS);
          int base_size = kind == PACKED_ELEMENTS
                              ? FixedArray::kHeaderSize
                              : (FixedArray::kHeaderSize - kHeapObjectTag);
          Node* offset = ElementOffsetFromIndex(index, kind, mode, base_size);
          VARIABLE(value, MachineRepresentation::kTagged);
          if (kind == PACKED_ELEMENTS) {
            value.Bind(LoadObjectField(elements, offset));
            GotoIf(WordEqual(value.value(), TheHoleConstant()), &hole_element);
          } else {
            Node* double_value =
                LoadDoubleWithHoleCheck(elements, offset, &hole_element);
            value.Bind(AllocateHeapNumberWithValue(double_value));
          }
          Goto(&process_element);

          BIND(&hole_element);
          if (missing_property_mode == MissingPropertyMode::kSkip) {
            // The NoElementsProtectorCell could go invalid during callbacks.
            Branch(IsNoElementsProtectorCellInvalid(), array_changed,
                   &one_element_done);
          } else {
            value.Bind(UndefinedConstant());
            Goto(&process_element);
          }
          BIND(&process_element);
          {
            a_.Bind(processor(this, value.value(), k()));
            Goto(&one_element_done);
          }
          BIND(&one_element_done);
        },
        1, mode, advance_mode);
    Comment("end VisitAllFastElementsOneKind");
  }

  void ArrayBuiltinsAssembler::HandleFastElements(
      const CallResultProcessor& processor, const PostLoopAction& action,
      Label* slow, ForEachDirection direction,
      MissingPropertyMode missing_property_mode) {
    Label switch_on_elements_kind(this), fast_elements(this),
        maybe_double_elements(this), fast_double_elements(this);

    Comment("begin HandleFastElements");
    // Non-smi lengths must use the slow path.
    GotoIf(TaggedIsNotSmi(len()), slow);

    BranchIfFastJSArray(o(), context(),
                        &switch_on_elements_kind, slow);

    BIND(&switch_on_elements_kind);
    TNode<Smi> smi_len = CAST(len());
    // Select by ElementsKind
    Node* o_map = LoadMap(o());
    Node* bit_field2 = LoadMapBitField2(o_map);
    Node* kind = DecodeWord32<Map::ElementsKindBits>(bit_field2);
    Branch(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS),
           &maybe_double_elements, &fast_elements);

    ParameterMode mode = OptimalParameterMode();
    BIND(&fast_elements);
    {
      VisitAllFastElementsOneKind(PACKED_ELEMENTS, processor, slow, mode,
                                  direction, missing_property_mode, smi_len);

      action(this);

      // No exception, return success
      ReturnFromBuiltin(a_.value());
    }

    BIND(&maybe_double_elements);
    Branch(IsElementsKindGreaterThan(kind, HOLEY_DOUBLE_ELEMENTS), slow,
           &fast_double_elements);

    BIND(&fast_double_elements);
    {
      VisitAllFastElementsOneKind(PACKED_DOUBLE_ELEMENTS, processor, slow, mode,
                                  direction, missing_property_mode, smi_len);

      action(this);

      // No exception, return success
      ReturnFromBuiltin(a_.value());
    }
  }

  // Perform ArraySpeciesCreate (ES6 #sec-arrayspeciescreate).
  // This version is specialized to create a zero length array
  // of the elements kind of the input array.
  void ArrayBuiltinsAssembler::GenerateArraySpeciesCreate() {
    Label runtime(this, Label::kDeferred), done(this);

    TNode<Smi> len = SmiConstant(0);
    TNode<Map> original_map = LoadMap(o());
    GotoIfNot(
        InstanceTypeEqual(LoadMapInstanceType(original_map), JS_ARRAY_TYPE),
        &runtime);

    GotoIfNot(IsPrototypeInitialArrayPrototype(context(), original_map),
              &runtime);

    Node* species_protector = ArraySpeciesProtectorConstant();
    Node* value =
        LoadObjectField(species_protector, PropertyCell::kValueOffset);
    TNode<Smi> const protector_invalid =
        SmiConstant(Isolate::kProtectorInvalid);
    GotoIf(WordEqual(value, protector_invalid), &runtime);

    // Respect the ElementsKind of the input array.
    TNode<Int32T> elements_kind = LoadMapElementsKind(original_map);
    GotoIfNot(IsFastElementsKind(elements_kind), &runtime);
    TNode<Context> native_context = LoadNativeContext(context());
    TNode<Map> array_map =
        LoadJSArrayElementsMap(elements_kind, native_context);
    TNode<JSArray> array =
        CAST(AllocateJSArray(GetInitialFastElementsKind(), array_map, len, len,
                             nullptr, CodeStubAssembler::SMI_PARAMETERS));
    a_.Bind(array);

    Goto(&done);

    BIND(&runtime);
    {
      // 5. Let A be ? ArraySpeciesCreate(O, len).
      Node* constructor =
          CallRuntime(Runtime::kArraySpeciesConstructor, context(), o());
      a_.Bind(ConstructJS(CodeFactory::Construct(isolate()), context(),
                          constructor, len));
      Goto(&fully_spec_compliant_);
    }

    BIND(&done);
  }

  // Perform ArraySpeciesCreate (ES6 #sec-arrayspeciescreate).
  void ArrayBuiltinsAssembler::GenerateArraySpeciesCreate(TNode<Number> len) {
    Label runtime(this, Label::kDeferred), done(this);

    Node* const original_map = LoadMap(o());
    GotoIfNot(
        InstanceTypeEqual(LoadMapInstanceType(original_map), JS_ARRAY_TYPE),
        &runtime);

    GotoIfNot(IsPrototypeInitialArrayPrototype(context(), original_map),
              &runtime);

    Node* species_protector = ArraySpeciesProtectorConstant();
    Node* value =
        LoadObjectField(species_protector, PropertyCell::kValueOffset);
    Node* const protector_invalid = SmiConstant(Isolate::kProtectorInvalid);
    GotoIf(WordEqual(value, protector_invalid), &runtime);

    GotoIfNot(TaggedIsPositiveSmi(len), &runtime);
    GotoIf(
        SmiAbove(CAST(len), SmiConstant(JSArray::kInitialMaxFastElementArray)),
        &runtime);

    // We need to be conservative and start with holey because the builtins
    // that create output arrays aren't guaranteed to be called for every
    // element in the input array (maybe the callback deletes an element).
    const ElementsKind elements_kind =
        GetHoleyElementsKind(GetInitialFastElementsKind());
    TNode<Context> native_context = LoadNativeContext(context());
    TNode<Map> array_map =
        LoadJSArrayElementsMap(elements_kind, native_context);
    a_.Bind(AllocateJSArray(PACKED_SMI_ELEMENTS, array_map, len, len, nullptr,
                            CodeStubAssembler::SMI_PARAMETERS));

    Goto(&done);

    BIND(&runtime);
    {
      // 5. Let A be ? ArraySpeciesCreate(O, len).
      Node* constructor =
          CallRuntime(Runtime::kArraySpeciesConstructor, context(), o());
      a_.Bind(ConstructJS(CodeFactory::Construct(isolate()), context(),
                          constructor, len));
      Goto(&fully_spec_compliant_);
    }

    BIND(&done);
  }

TF_BUILTIN(ArrayPrototypePop, CodeStubAssembler) {
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CSA_ASSERT(this, IsUndefined(Parameter(Descriptor::kJSNewTarget)));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  TNode<Object> receiver = args.GetReceiver();

  Label runtime(this, Label::kDeferred);
  Label fast(this);

  // Only pop in this stub if
  // 1) the array has fast elements
  // 2) the length is writable,
  // 3) the elements backing store isn't copy-on-write,
  // 4) we aren't supposed to shrink the backing store.

  // 1) Check that the array has fast elements.
  BranchIfFastJSArray(receiver, context, &fast, &runtime);

  BIND(&fast);
  {
    TNode<JSArray> array_receiver = CAST(receiver);
    CSA_ASSERT(this, TaggedIsPositiveSmi(LoadJSArrayLength(array_receiver)));
    Node* length =
        LoadAndUntagObjectField(array_receiver, JSArray::kLengthOffset);
    Label return_undefined(this), fast_elements(this);
    GotoIf(IntPtrEqual(length, IntPtrConstant(0)), &return_undefined);

    // 2) Ensure that the length is writable.
    EnsureArrayLengthWritable(LoadMap(array_receiver), &runtime);

    // 3) Check that the elements backing store isn't copy-on-write.
    Node* elements = LoadElements(array_receiver);
    GotoIf(WordEqual(LoadMap(elements),
                     LoadRoot(Heap::kFixedCOWArrayMapRootIndex)),
           &runtime);

    Node* new_length = IntPtrSub(length, IntPtrConstant(1));

    // 4) Check that we're not supposed to shrink the backing store, as
    //    implemented in elements.cc:ElementsAccessorBase::SetLengthImpl.
    Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
    GotoIf(IntPtrLessThan(
               IntPtrAdd(IntPtrAdd(new_length, new_length),
                         IntPtrConstant(JSObject::kMinAddedElementsCapacity)),
               capacity),
           &runtime);

    StoreObjectFieldNoWriteBarrier(array_receiver, JSArray::kLengthOffset,
                                   SmiTag(new_length));

    TNode<Int32T> elements_kind = LoadElementsKind(array_receiver);
    GotoIf(Int32LessThanOrEqual(elements_kind,
                                Int32Constant(TERMINAL_FAST_ELEMENTS_KIND)),
           &fast_elements);

    Node* value = LoadFixedDoubleArrayElement(
        elements, new_length, MachineType::Float64(), 0, INTPTR_PARAMETERS,
        &return_undefined);

    int32_t header_size = FixedDoubleArray::kHeaderSize - kHeapObjectTag;
    Node* offset = ElementOffsetFromIndex(new_length, HOLEY_DOUBLE_ELEMENTS,
                                          INTPTR_PARAMETERS, header_size);
    if (Is64()) {
      Node* double_hole = Int64Constant(kHoleNanInt64);
      StoreNoWriteBarrier(MachineRepresentation::kWord64, elements, offset,
                          double_hole);
    } else {
      STATIC_ASSERT(kHoleNanLower32 == kHoleNanUpper32);
      Node* double_hole = Int32Constant(kHoleNanLower32);
      StoreNoWriteBarrier(MachineRepresentation::kWord32, elements, offset,
                          double_hole);
      StoreNoWriteBarrier(MachineRepresentation::kWord32, elements,
                          IntPtrAdd(offset, IntPtrConstant(kPointerSize)),
                          double_hole);
    }
    args.PopAndReturn(AllocateHeapNumberWithValue(value));

    BIND(&fast_elements);
    {
      Node* value = LoadFixedArrayElement(CAST(elements), new_length);
      StoreFixedArrayElement(CAST(elements), new_length, TheHoleConstant());
      GotoIf(WordEqual(value, TheHoleConstant()), &return_undefined);
      args.PopAndReturn(value);
    }

    BIND(&return_undefined);
    { args.PopAndReturn(UndefinedConstant()); }
  }

  BIND(&runtime);
  {
    // We are not using Parameter(Descriptor::kJSTarget) and loading the value
    // from the current frame here in order to reduce register pressure on the
    // fast path.
    TNode<JSFunction> target = LoadTargetFromFrame();
    TailCallBuiltin(Builtins::kArrayPop, context, target, UndefinedConstant(),
                    argc);
  }
}

TF_BUILTIN(ArrayPrototypePush, CodeStubAssembler) {
  TVARIABLE(IntPtrT, arg_index);
  Label default_label(this, &arg_index);
  Label smi_transition(this);
  Label object_push_pre(this);
  Label object_push(this, &arg_index);
  Label double_push(this, &arg_index);
  Label double_transition(this);
  Label runtime(this, Label::kDeferred);

  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CSA_ASSERT(this, IsUndefined(Parameter(Descriptor::kJSNewTarget)));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  TNode<Object> receiver = args.GetReceiver();
  TNode<JSArray> array_receiver;
  Node* kind = nullptr;

  Label fast(this);
  BranchIfFastJSArray(receiver, context, &fast, &runtime);

  BIND(&fast);
  {
    array_receiver = CAST(receiver);
    arg_index = IntPtrConstant(0);
    kind = EnsureArrayPushable(LoadMap(array_receiver), &runtime);
    GotoIf(IsElementsKindGreaterThan(kind, HOLEY_SMI_ELEMENTS),
           &object_push_pre);

    Node* new_length = BuildAppendJSArray(PACKED_SMI_ELEMENTS, array_receiver,
                                          &args, &arg_index, &smi_transition);
    args.PopAndReturn(new_length);
  }

  // If the argument is not a smi, then use a heavyweight SetProperty to
  // transition the array for only the single next element. If the argument is
  // a smi, the failure is due to some other reason and we should fall back on
  // the most generic implementation for the rest of the array.
  BIND(&smi_transition);
  {
    Node* arg = args.AtIndex(arg_index.value());
    GotoIf(TaggedIsSmi(arg), &default_label);
    Node* length = LoadJSArrayLength(array_receiver);
    // TODO(danno): Use the KeyedStoreGeneric stub here when possible,
    // calling into the runtime to do the elements transition is overkill.
    SetPropertyStrict(context, array_receiver, CAST(length), CAST(arg));
    Increment(&arg_index);
    // The runtime SetProperty call could have converted the array to dictionary
    // mode, which must be detected to abort the fast-path.
    Node* map = LoadMap(array_receiver);
    Node* bit_field2 = LoadMapBitField2(map);
    Node* kind = DecodeWord32<Map::ElementsKindBits>(bit_field2);
    GotoIf(Word32Equal(kind, Int32Constant(DICTIONARY_ELEMENTS)),
           &default_label);

    GotoIfNotNumber(arg, &object_push);
    Goto(&double_push);
  }

  BIND(&object_push_pre);
  {
    Branch(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS), &double_push,
           &object_push);
  }

  BIND(&object_push);
  {
    Node* new_length = BuildAppendJSArray(PACKED_ELEMENTS, array_receiver,
                                          &args, &arg_index, &default_label);
    args.PopAndReturn(new_length);
  }

  BIND(&double_push);
  {
    Node* new_length =
        BuildAppendJSArray(PACKED_DOUBLE_ELEMENTS, array_receiver, &args,
                           &arg_index, &double_transition);
    args.PopAndReturn(new_length);
  }

  // If the argument is not a double, then use a heavyweight SetProperty to
  // transition the array for only the single next element. If the argument is
  // a double, the failure is due to some other reason and we should fall back
  // on the most generic implementation for the rest of the array.
  BIND(&double_transition);
  {
    Node* arg = args.AtIndex(arg_index.value());
    GotoIfNumber(arg, &default_label);
    Node* length = LoadJSArrayLength(array_receiver);
    // TODO(danno): Use the KeyedStoreGeneric stub here when possible,
    // calling into the runtime to do the elements transition is overkill.
    SetPropertyStrict(context, array_receiver, CAST(length), CAST(arg));
    Increment(&arg_index);
    // The runtime SetProperty call could have converted the array to dictionary
    // mode, which must be detected to abort the fast-path.
    Node* map = LoadMap(array_receiver);
    Node* bit_field2 = LoadMapBitField2(map);
    Node* kind = DecodeWord32<Map::ElementsKindBits>(bit_field2);
    GotoIf(Word32Equal(kind, Int32Constant(DICTIONARY_ELEMENTS)),
           &default_label);
    Goto(&object_push);
  }

  // Fallback that stores un-processed arguments using the full, heavyweight
  // SetProperty machinery.
  BIND(&default_label);
  {
    args.ForEach(
        [this, array_receiver, context](Node* arg) {
          Node* length = LoadJSArrayLength(array_receiver);
          SetPropertyStrict(context, array_receiver, CAST(length), CAST(arg));
        },
        arg_index.value());
    args.PopAndReturn(LoadJSArrayLength(array_receiver));
  }

  BIND(&runtime);
  {
    // We are not using Parameter(Descriptor::kJSTarget) and loading the value
    // from the current frame here in order to reduce register pressure on the
    // fast path.
    TNode<JSFunction> target = LoadTargetFromFrame();
    TailCallBuiltin(Builtins::kArrayPush, context, target, UndefinedConstant(),
                    argc);
  }
}

class ArrayPrototypeSliceCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit ArrayPrototypeSliceCodeStubAssembler(
      compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* HandleFastSlice(TNode<Context> context, Node* array, Node* from,
                        Node* count, Label* slow) {
    VARIABLE(result, MachineRepresentation::kTagged);
    Label done(this);

    GotoIf(TaggedIsNotSmi(from), slow);
    GotoIf(TaggedIsNotSmi(count), slow);

    Label try_fast_arguments(this), try_simple_slice(this);

    Node* map = LoadMap(array);
    GotoIfNot(IsJSArrayMap(map), &try_fast_arguments);

    // Check prototype chain if receiver does not have packed elements
    GotoIfNot(IsPrototypeInitialArrayPrototype(context, map), slow);

    GotoIf(IsNoElementsProtectorCellInvalid(), slow);

    GotoIf(IsArraySpeciesProtectorCellInvalid(), slow);

    // Bailout if receiver has slow elements.
    Node* elements_kind = LoadMapElementsKind(map);
    GotoIfNot(IsFastElementsKind(elements_kind), &try_simple_slice);

    // Make sure that the length hasn't been changed by side-effect.
    Node* array_length = LoadJSArrayLength(array);
    GotoIf(TaggedIsNotSmi(array_length), slow);
    GotoIf(SmiAbove(SmiAdd(CAST(from), CAST(count)), CAST(array_length)), slow);

    CSA_ASSERT(this, SmiGreaterThanOrEqual(CAST(from), SmiConstant(0)));

    result.Bind(CallBuiltin(Builtins::kExtractFastJSArray, context, array, from,
                            count));
    Goto(&done);

    BIND(&try_fast_arguments);

    Node* const native_context = LoadNativeContext(context);
    Node* const fast_aliasted_arguments_map = LoadContextElement(
        native_context, Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);
    GotoIf(WordNotEqual(map, fast_aliasted_arguments_map), &try_simple_slice);

    TNode<SloppyArgumentsElements> sloppy_elements = CAST(LoadElements(array));
    TNode<Smi> sloppy_elements_length =
        LoadFixedArrayBaseLength(sloppy_elements);
    TNode<Smi> parameter_map_length =
        SmiSub(sloppy_elements_length,
               SmiConstant(SloppyArgumentsElements::kParameterMapStart));
    VARIABLE(index_out, MachineType::PointerRepresentation());

    int max_fast_elements =
        (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - JSArray::kSize -
         AllocationMemento::kSize) /
        kPointerSize;
    GotoIf(SmiAboveOrEqual(CAST(count), SmiConstant(max_fast_elements)),
           &try_simple_slice);

    GotoIf(SmiLessThan(CAST(from), SmiConstant(0)), slow);

    TNode<Smi> end = SmiAdd(CAST(from), CAST(count));

    TNode<FixedArray> unmapped_elements = CAST(LoadFixedArrayElement(
        sloppy_elements, SloppyArgumentsElements::kArgumentsIndex));
    TNode<Smi> unmapped_elements_length =
        LoadFixedArrayBaseLength(unmapped_elements);

    GotoIf(SmiAbove(end, unmapped_elements_length), slow);

    Node* array_map = LoadJSArrayElementsMap(HOLEY_ELEMENTS, native_context);
    result.Bind(AllocateJSArray(HOLEY_ELEMENTS, array_map, count, count,
                                nullptr, SMI_PARAMETERS));

    index_out.Bind(IntPtrConstant(0));
    TNode<FixedArray> result_elements = CAST(LoadElements(result.value()));
    TNode<Smi> from_mapped = SmiMin(parameter_map_length, CAST(from));
    TNode<Smi> to = SmiMin(parameter_map_length, end);
    Node* arguments_context = LoadFixedArrayElement(
        sloppy_elements, SloppyArgumentsElements::kContextIndex);
    VariableList var_list({&index_out}, zone());
    BuildFastLoop(
        var_list, from_mapped, to,
        [this, result_elements, arguments_context, sloppy_elements,
         unmapped_elements, &index_out](Node* current) {
          Node* context_index = LoadFixedArrayElement(
              sloppy_elements, current,
              kPointerSize * SloppyArgumentsElements::kParameterMapStart,
              SMI_PARAMETERS);
          Label is_the_hole(this), done(this);
          GotoIf(IsTheHole(context_index), &is_the_hole);
          Node* mapped_argument =
              LoadContextElement(arguments_context, SmiUntag(context_index));
          StoreFixedArrayElement(result_elements, index_out.value(),
                                 mapped_argument, SKIP_WRITE_BARRIER);
          Goto(&done);
          BIND(&is_the_hole);
          Node* argument = LoadFixedArrayElement(unmapped_elements, current, 0,
                                                 SMI_PARAMETERS);
          StoreFixedArrayElement(result_elements, index_out.value(), argument,
                                 SKIP_WRITE_BARRIER);
          Goto(&done);
          BIND(&done);
          index_out.Bind(IntPtrAdd(index_out.value(), IntPtrConstant(1)));
        },
        1, SMI_PARAMETERS, IndexAdvanceMode::kPost);

    TNode<Smi> unmapped_from =
        SmiMin(SmiMax(parameter_map_length, CAST(from)), end);

    BuildFastLoop(
        var_list, unmapped_from, end,
        [this, unmapped_elements, result_elements, &index_out](Node* current) {
          Node* argument = LoadFixedArrayElement(unmapped_elements, current, 0,
                                                 SMI_PARAMETERS);
          StoreFixedArrayElement(result_elements, index_out.value(), argument,
                                 SKIP_WRITE_BARRIER);
          index_out.Bind(IntPtrAdd(index_out.value(), IntPtrConstant(1)));
        },
        1, SMI_PARAMETERS, IndexAdvanceMode::kPost);

    Goto(&done);

    BIND(&try_simple_slice);
    Node* simple_result = CallRuntime(Runtime::kTrySliceSimpleNonFastElements,
                                      context, array, from, count);
    GotoIfNumber(simple_result, slow);
    result.Bind(simple_result);

    Goto(&done);

    BIND(&done);
    return result.value();
  }

  void CopyOneElement(TNode<Context> context, Node* o, Node* a, Node* p_k,
                      Variable& n) {
    // b. Let kPresent be HasProperty(O, Pk).
    // c. ReturnIfAbrupt(kPresent).
    TNode<Oddball> k_present = HasProperty(context, o, p_k, kHasProperty);

    // d. If kPresent is true, then
    Label done_element(this);
    GotoIf(IsFalse(k_present), &done_element);

    // i. Let kValue be Get(O, Pk).
    // ii. ReturnIfAbrupt(kValue).
    Node* k_value = GetProperty(context, o, p_k);

    // iii. Let status be CreateDataPropertyOrThrow(A, ToString(n), kValue).
    // iv. ReturnIfAbrupt(status).
    CallRuntime(Runtime::kCreateDataProperty, context, a, n.value(), k_value);

    Goto(&done_element);
    BIND(&done_element);
  }
};

TF_BUILTIN(ArrayPrototypeSlice, ArrayPrototypeSliceCodeStubAssembler) {
  Node* const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Label slow(this, Label::kDeferred), fast_elements_kind(this);

  CodeStubArguments args(this, argc);
  TNode<Object> receiver = args.GetReceiver();

  TVARIABLE(JSReceiver, o);
  VARIABLE(len, MachineRepresentation::kTagged);
  Label length_done(this), generic_length(this), check_arguments_length(this),
      load_arguments_length(this);

  GotoIf(TaggedIsSmi(receiver), &generic_length);
  GotoIfNot(IsJSArray(CAST(receiver)), &check_arguments_length);

  TNode<JSArray> array_receiver = CAST(receiver);
  o = array_receiver;
  len.Bind(LoadJSArrayLength(array_receiver));

  // Check for the array clone case. There can be no arguments to slice, the
  // array prototype chain must be intact and have no elements, the array has to
  // have fast elements.
  GotoIf(WordNotEqual(argc, IntPtrConstant(0)), &length_done);

  Label clone(this);
  BranchIfFastJSArrayForCopy(receiver, context, &clone, &length_done);
  BIND(&clone);

  args.PopAndReturn(
      CallBuiltin(Builtins::kCloneFastJSArray, context, receiver));

  BIND(&check_arguments_length);

  Node* map = LoadMap(array_receiver);
  Node* native_context = LoadNativeContext(context);
  GotoIfContextElementEqual(map, native_context,
                            Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX,
                            &load_arguments_length);
  GotoIfContextElementEqual(map, native_context,
                            Context::SLOW_ALIASED_ARGUMENTS_MAP_INDEX,
                            &load_arguments_length);
  GotoIfContextElementEqual(map, native_context,
                            Context::STRICT_ARGUMENTS_MAP_INDEX,
                            &load_arguments_length);
  GotoIfContextElementEqual(map, native_context,
                            Context::SLOPPY_ARGUMENTS_MAP_INDEX,
                            &load_arguments_length);

  Goto(&generic_length);

  BIND(&load_arguments_length);
  Node* arguments_length =
      LoadObjectField(array_receiver, JSArgumentsObject::kLengthOffset);
  GotoIf(TaggedIsNotSmi(arguments_length), &generic_length);
  o = CAST(receiver);
  len.Bind(arguments_length);
  Goto(&length_done);

  BIND(&generic_length);
  // 1. Let O be ToObject(this value).
  // 2. ReturnIfAbrupt(O).
  o = ToObject_Inline(context, receiver);

  // 3. Let len be ToLength(Get(O, "length")).
  // 4. ReturnIfAbrupt(len).
  len.Bind(ToLength_Inline(
      context,
      GetProperty(context, o.value(), isolate()->factory()->length_string())));
  Goto(&length_done);

  BIND(&length_done);

  // 5. Let relativeStart be ToInteger(start).
  // 6. ReturnIfAbrupt(relativeStart).
  TNode<Object> arg0 = args.GetOptionalArgumentValue(0, SmiConstant(0));
  Node* relative_start = ToInteger_Inline(context, arg0);

  // 7. If relativeStart < 0, let k be max((len + relativeStart),0);
  //    else let k be min(relativeStart, len.value()).
  VARIABLE(k, MachineRepresentation::kTagged);
  Label relative_start_positive(this), relative_start_done(this);
  GotoIfNumberGreaterThanOrEqual(relative_start, SmiConstant(0),
                                 &relative_start_positive);
  k.Bind(NumberMax(NumberAdd(len.value(), relative_start), NumberConstant(0)));
  Goto(&relative_start_done);
  BIND(&relative_start_positive);
  k.Bind(NumberMin(relative_start, len.value()));
  Goto(&relative_start_done);
  BIND(&relative_start_done);

  // 8. If end is undefined, let relativeEnd be len;
  //    else let relativeEnd be ToInteger(end).
  // 9. ReturnIfAbrupt(relativeEnd).
  TNode<Object> end = args.GetOptionalArgumentValue(1, UndefinedConstant());
  Label end_undefined(this), end_done(this);
  VARIABLE(relative_end, MachineRepresentation::kTagged);
  GotoIf(WordEqual(end, UndefinedConstant()), &end_undefined);
  relative_end.Bind(ToInteger_Inline(context, end));
  Goto(&end_done);
  BIND(&end_undefined);
  relative_end.Bind(len.value());
  Goto(&end_done);
  BIND(&end_done);

  // 10. If relativeEnd < 0, let final be max((len + relativeEnd),0);
  //     else let final be min(relativeEnd, len).
  VARIABLE(final, MachineRepresentation::kTagged);
  Label relative_end_positive(this), relative_end_done(this);
  GotoIfNumberGreaterThanOrEqual(relative_end.value(), NumberConstant(0),
                                 &relative_end_positive);
  final.Bind(NumberMax(NumberAdd(len.value(), relative_end.value()),
                       NumberConstant(0)));
  Goto(&relative_end_done);
  BIND(&relative_end_positive);
  final.Bind(NumberMin(relative_end.value(), len.value()));
  Goto(&relative_end_done);
  BIND(&relative_end_done);

  // 11. Let count be max(final – k, 0).
  Node* count =
      NumberMax(NumberSub(final.value(), k.value()), NumberConstant(0));

  // Handle FAST_ELEMENTS
  Label non_fast(this);
  Node* fast_result =
      HandleFastSlice(context, o.value(), k.value(), count, &non_fast);
  args.PopAndReturn(fast_result);

  // 12. Let A be ArraySpeciesCreate(O, count).
  // 13. ReturnIfAbrupt(A).
  BIND(&non_fast);

  Node* constructor =
      CallRuntime(Runtime::kArraySpeciesConstructor, context, o.value());
  Node* a = ConstructJS(CodeFactory::Construct(isolate()), context, constructor,
                        count);

  // 14. Let n be 0.
  VARIABLE(n, MachineRepresentation::kTagged);
  n.Bind(SmiConstant(0));

  Label loop(this, {&k, &n});
  Label after_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // 15. Repeat, while k < final
    GotoIfNumberGreaterThanOrEqual(k.value(), final.value(), &after_loop);

    Node* p_k = k.value();  //  ToString(context, k.value()) is no-op

    CopyOneElement(context, o.value(), a, p_k, n);

    // e. Increase k by 1.
    k.Bind(NumberInc(k.value()));

    // f. Increase n by 1.
    n.Bind(NumberInc(n.value()));

    Goto(&loop);
  }

  BIND(&after_loop);

  // 16. Let setStatus be Set(A, "length", n, true).
  // 17. ReturnIfAbrupt(setStatus).
  SetPropertyStrict(context, CAST(a), CodeStubAssembler::LengthStringConstant(),
                    CAST(n.value()));
  args.PopAndReturn(a);
}

TF_BUILTIN(ArrayPrototypeShift, CodeStubAssembler) {
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CSA_ASSERT(this, IsUndefined(Parameter(Descriptor::kJSNewTarget)));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  TNode<Object> receiver = args.GetReceiver();

  Label runtime(this, Label::kDeferred);
  Label fast(this);

  // Only shift in this stub if
  // 1) the array has fast elements
  // 2) the length is writable,
  // 3) the elements backing store isn't copy-on-write,
  // 4) we aren't supposed to shrink the backing store,
  // 5) we aren't supposed to left-trim the backing store.

  // 1) Check that the array has fast elements.
  BranchIfFastJSArray(receiver, context, &fast, &runtime);

  BIND(&fast);
  {
    TNode<JSArray> array_receiver = CAST(receiver);
    CSA_ASSERT(this, TaggedIsPositiveSmi(LoadJSArrayLength(array_receiver)));
    Node* length =
        LoadAndUntagObjectField(array_receiver, JSArray::kLengthOffset);
    Label return_undefined(this), fast_elements_tagged(this),
        fast_elements_smi(this);
    GotoIf(IntPtrEqual(length, IntPtrConstant(0)), &return_undefined);

    // 2) Ensure that the length is writable.
    EnsureArrayLengthWritable(LoadMap(array_receiver), &runtime);

    // 3) Check that the elements backing store isn't copy-on-write.
    Node* elements = LoadElements(array_receiver);
    GotoIf(WordEqual(LoadMap(elements),
                     LoadRoot(Heap::kFixedCOWArrayMapRootIndex)),
           &runtime);

    Node* new_length = IntPtrSub(length, IntPtrConstant(1));

    // 4) Check that we're not supposed to right-trim the backing store, as
    //    implemented in elements.cc:ElementsAccessorBase::SetLengthImpl.
    Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
    GotoIf(IntPtrLessThan(
               IntPtrAdd(IntPtrAdd(new_length, new_length),
                         IntPtrConstant(JSObject::kMinAddedElementsCapacity)),
               capacity),
           &runtime);

    // 5) Check that we're not supposed to left-trim the backing store, as
    //    implemented in elements.cc:FastElementsAccessor::MoveElements.
    GotoIf(IntPtrGreaterThan(new_length,
                             IntPtrConstant(JSArray::kMaxCopyElements)),
           &runtime);

    StoreObjectFieldNoWriteBarrier(array_receiver, JSArray::kLengthOffset,
                                   SmiTag(new_length));

    TNode<Int32T> elements_kind = LoadElementsKind(array_receiver);
    GotoIf(
        Int32LessThanOrEqual(elements_kind, Int32Constant(HOLEY_SMI_ELEMENTS)),
        &fast_elements_smi);
    GotoIf(Int32LessThanOrEqual(elements_kind, Int32Constant(HOLEY_ELEMENTS)),
           &fast_elements_tagged);

    // Fast double elements kind:
    {
      CSA_ASSERT(this,
                 Int32LessThanOrEqual(elements_kind,
                                      Int32Constant(HOLEY_DOUBLE_ELEMENTS)));

      VARIABLE(result, MachineRepresentation::kTagged, UndefinedConstant());

      Label move_elements(this);
      result.Bind(AllocateHeapNumberWithValue(LoadFixedDoubleArrayElement(
          elements, IntPtrConstant(0), MachineType::Float64(), 0,
          INTPTR_PARAMETERS, &move_elements)));
      Goto(&move_elements);
      BIND(&move_elements);

      int32_t header_size = FixedDoubleArray::kHeaderSize - kHeapObjectTag;
      Node* memmove =
          ExternalConstant(ExternalReference::libc_memmove_function());
      Node* start = IntPtrAdd(
          BitcastTaggedToWord(elements),
          ElementOffsetFromIndex(IntPtrConstant(0), HOLEY_DOUBLE_ELEMENTS,
                                 INTPTR_PARAMETERS, header_size));
      CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                     MachineType::Pointer(), MachineType::UintPtr(), memmove,
                     start, IntPtrAdd(start, IntPtrConstant(kDoubleSize)),
                     IntPtrMul(new_length, IntPtrConstant(kDoubleSize)));
      Node* offset = ElementOffsetFromIndex(new_length, HOLEY_DOUBLE_ELEMENTS,
                                            INTPTR_PARAMETERS, header_size);
      if (Is64()) {
        Node* double_hole = Int64Constant(kHoleNanInt64);
        StoreNoWriteBarrier(MachineRepresentation::kWord64, elements, offset,
                            double_hole);
      } else {
        STATIC_ASSERT(kHoleNanLower32 == kHoleNanUpper32);
        Node* double_hole = Int32Constant(kHoleNanLower32);
        StoreNoWriteBarrier(MachineRepresentation::kWord32, elements, offset,
                            double_hole);
        StoreNoWriteBarrier(MachineRepresentation::kWord32, elements,
                            IntPtrAdd(offset, IntPtrConstant(kPointerSize)),
                            double_hole);
      }
      args.PopAndReturn(result.value());
    }

    BIND(&fast_elements_tagged);
    {
      TNode<FixedArray> elements_fixed_array = CAST(elements);
      Node* value = LoadFixedArrayElement(elements_fixed_array, 0);
      BuildFastLoop(
          IntPtrConstant(0), new_length,
          [&](Node* index) {
            StoreFixedArrayElement(
                elements_fixed_array, index,
                LoadFixedArrayElement(elements_fixed_array,
                                      IntPtrAdd(index, IntPtrConstant(1))));
          },
          1, ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      StoreFixedArrayElement(elements_fixed_array, new_length,
                             TheHoleConstant());
      GotoIf(WordEqual(value, TheHoleConstant()), &return_undefined);
      args.PopAndReturn(value);
    }

    BIND(&fast_elements_smi);
    {
      TNode<FixedArray> elements_fixed_array = CAST(elements);
      Node* value = LoadFixedArrayElement(elements_fixed_array, 0);
      BuildFastLoop(
          IntPtrConstant(0), new_length,
          [&](Node* index) {
            StoreFixedArrayElement(
                elements_fixed_array, index,
                LoadFixedArrayElement(elements_fixed_array,
                                      IntPtrAdd(index, IntPtrConstant(1))),
                SKIP_WRITE_BARRIER);
          },
          1, ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      StoreFixedArrayElement(elements_fixed_array, new_length,
                             TheHoleConstant());
      GotoIf(WordEqual(value, TheHoleConstant()), &return_undefined);
      args.PopAndReturn(value);
    }

    BIND(&return_undefined);
    { args.PopAndReturn(UndefinedConstant()); }
  }

  BIND(&runtime);
  {
    // We are not using Parameter(Descriptor::kJSTarget) and loading the value
    // from the current frame here in order to reduce register pressure on the
    // fast path.
    TNode<JSFunction> target = LoadTargetFromFrame();
    TailCallBuiltin(Builtins::kArrayShift, context, target, UndefinedConstant(),
                    argc);
  }
}

TF_BUILTIN(ExtractFastJSArray, ArrayBuiltinsAssembler) {
  ParameterMode mode = OptimalParameterMode();
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* array = Parameter(Descriptor::kSource);
  Node* begin = TaggedToParameter(Parameter(Descriptor::kBegin), mode);
  Node* count = TaggedToParameter(Parameter(Descriptor::kCount), mode);

  CSA_ASSERT(this, IsJSArray(array));
  CSA_ASSERT(this, Word32BinaryNot(IsNoElementsProtectorCellInvalid()));

  Return(ExtractFastJSArray(context, array, begin, count, mode));
}

TF_BUILTIN(CloneFastJSArray, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* array = Parameter(Descriptor::kSource);

  CSA_ASSERT(this, IsJSArray(array));
  CSA_ASSERT(this, Word32BinaryNot(IsNoElementsProtectorCellInvalid()));

  ParameterMode mode = OptimalParameterMode();
  Return(CloneFastJSArray(context, array, mode));
}

TF_BUILTIN(ArrayFindLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::FindProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      MissingPropertyMode::kUseUndefined, ForEachDirection::kForward);
}

// Continuation that is called after an eager deoptimization from TF (ex. the
// array changes during iteration).
TF_BUILTIN(ArrayFindLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayFindLoopContinuation, context, receiver,
                     callbackfn, this_arg, UndefinedConstant(), receiver,
                     initial_k, len, UndefinedConstant()));
}

// Continuation that is called after a lazy deoptimization from TF (ex. the
// callback function is no longer callable).
TF_BUILTIN(ArrayFindLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayFindLoopContinuation, context, receiver,
                     callbackfn, this_arg, UndefinedConstant(), receiver,
                     initial_k, len, UndefinedConstant()));
}

// Continuation that is called after a lazy deoptimization from TF that happens
// right after the callback and it's returned value must be handled before
// iteration continues.
TF_BUILTIN(ArrayFindLoopAfterCallbackLazyDeoptContinuation,
           ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* found_value = Parameter(Descriptor::kFoundValue);
  Node* is_found = Parameter(Descriptor::kIsFound);

  // This custom lazy deopt point is right after the callback. find() needs
  // to pick up at the next step, which is returning the element if the callback
  // value is truthy.  Otherwise, continue the search by calling the
  // continuation.
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(is_found, &if_true, &if_false);
  BIND(&if_true);
  Return(found_value);
  BIND(&if_false);
  Return(CallBuiltin(Builtins::kArrayFindLoopContinuation, context, receiver,
                     callbackfn, this_arg, UndefinedConstant(), receiver,
                     initial_k, len, UndefinedConstant()));
}

// ES #sec-get-%typedarray%.prototype.find
TF_BUILTIN(ArrayPrototypeFind, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.find", &ArrayBuiltinsAssembler::FindResultGenerator,
      &ArrayBuiltinsAssembler::FindProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayFindLoopContinuation),
      MissingPropertyMode::kUseUndefined, ForEachDirection::kForward);
}

TF_BUILTIN(ArrayFindIndexLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::FindIndexProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      MissingPropertyMode::kUseUndefined, ForEachDirection::kForward);
}

TF_BUILTIN(ArrayFindIndexLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayFindIndexLoopContinuation, context,
                     receiver, callbackfn, this_arg, SmiConstant(-1), receiver,
                     initial_k, len, UndefinedConstant()));
}

TF_BUILTIN(ArrayFindIndexLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayFindIndexLoopContinuation, context,
                     receiver, callbackfn, this_arg, SmiConstant(-1), receiver,
                     initial_k, len, UndefinedConstant()));
}

TF_BUILTIN(ArrayFindIndexLoopAfterCallbackLazyDeoptContinuation,
           ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* found_value = Parameter(Descriptor::kFoundValue);
  Node* is_found = Parameter(Descriptor::kIsFound);

  // This custom lazy deopt point is right after the callback. find() needs
  // to pick up at the next step, which is returning the element if the callback
  // value is truthy.  Otherwise, continue the search by calling the
  // continuation.
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(is_found, &if_true, &if_false);
  BIND(&if_true);
  Return(found_value);
  BIND(&if_false);
  Return(CallBuiltin(Builtins::kArrayFindIndexLoopContinuation, context,
                     receiver, callbackfn, this_arg, SmiConstant(-1), receiver,
                     initial_k, len, UndefinedConstant()));
}

// ES #sec-get-%typedarray%.prototype.findIndex
TF_BUILTIN(ArrayPrototypeFindIndex, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.findIndex",
      &ArrayBuiltinsAssembler::FindIndexResultGenerator,
      &ArrayBuiltinsAssembler::FindIndexProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(),
                            Builtins::kArrayFindIndexLoopContinuation),
      MissingPropertyMode::kUseUndefined, ForEachDirection::kForward);
}

class ArrayPopulatorAssembler : public CodeStubAssembler {
 public:
  explicit ArrayPopulatorAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<Object> ConstructArrayLike(TNode<Context> context,
                                   TNode<Object> receiver) {
    TVARIABLE(Object, array);
    Label is_constructor(this), is_not_constructor(this), done(this);
    GotoIf(TaggedIsSmi(receiver), &is_not_constructor);
    Branch(IsConstructor(CAST(receiver)), &is_constructor, &is_not_constructor);

    BIND(&is_constructor);
    {
      array = CAST(
          ConstructJS(CodeFactory::Construct(isolate()), context, receiver));
      Goto(&done);
    }

    BIND(&is_not_constructor);
    {
      Label allocate_js_array(this);

      TNode<Map> array_map = CAST(LoadContextElement(
          context, Context::JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX));

      array = CAST(AllocateJSArray(PACKED_SMI_ELEMENTS, array_map,
                                   SmiConstant(0), SmiConstant(0), nullptr,
                                   ParameterMode::SMI_PARAMETERS));
      Goto(&done);
    }

    BIND(&done);
    return array.value();
  }

  TNode<Object> ConstructArrayLike(TNode<Context> context,
                                   TNode<Object> receiver,
                                   TNode<Number> length) {
    TVARIABLE(Object, array);
    Label is_constructor(this), is_not_constructor(this), done(this);
    CSA_ASSERT(this, IsNumberNormalized(length));
    GotoIf(TaggedIsSmi(receiver), &is_not_constructor);
    Branch(IsConstructor(CAST(receiver)), &is_constructor, &is_not_constructor);

    BIND(&is_constructor);
    {
      array = CAST(ConstructJS(CodeFactory::Construct(isolate()), context,
                               receiver, length));
      Goto(&done);
    }

    BIND(&is_not_constructor);
    {
      Label allocate_js_array(this);

      Label next(this), runtime(this, Label::kDeferred);
      TNode<Smi> limit = SmiConstant(JSArray::kInitialMaxFastElementArray);
      CSA_ASSERT_BRANCH(this, [=](Label* ok, Label* not_ok) {
        BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual,
                                           length, SmiConstant(0), ok, not_ok);
      });
      // This check also transitively covers the case where length is too big
      // to be representable by a SMI and so is not usable with
      // AllocateJSArray.
      BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, length,
                                         limit, &runtime, &next);

      BIND(&runtime);
      {
        TNode<Context> native_context = LoadNativeContext(context);
        TNode<JSFunction> array_function = CAST(
            LoadContextElement(native_context, Context::ARRAY_FUNCTION_INDEX));
        array = CallRuntime(Runtime::kNewArray, context, array_function, length,
                            array_function, UndefinedConstant());
        Goto(&done);
      }

      BIND(&next);
      CSA_ASSERT(this, TaggedIsSmi(length));

      TNode<Map> array_map = CAST(LoadContextElement(
          context, Context::JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX));

      // TODO(delphick): Consider using
      // AllocateUninitializedJSArrayWithElements to avoid initializing an
      // array and then writing over it.
      array = CAST(AllocateJSArray(PACKED_SMI_ELEMENTS, array_map, length,
                                   SmiConstant(0), nullptr,
                                   ParameterMode::SMI_PARAMETERS));
      Goto(&done);
    }

    BIND(&done);
    return array.value();
  }

  void GenerateSetLength(TNode<Context> context, TNode<Object> array,
                         TNode<Number> length) {
    Label fast(this), runtime(this), done(this);
    // There's no need to set the length, if
    // 1) the array is a fast JS array and
    // 2) the new length is equal to the old length.
    // as the set is not observable. Otherwise fall back to the run-time.

    // 1) Check that the array has fast elements.
    // TODO(delphick): Consider changing this since it does an an unnecessary
    // check for SMIs.
    // TODO(delphick): Also we could hoist this to after the array construction
    // and copy the args into array in the same way as the Array constructor.
    BranchIfFastJSArray(array, context, &fast, &runtime);

    BIND(&fast);
    {
      TNode<JSArray> fast_array = CAST(array);

      TNode<Smi> length_smi = CAST(length);
      TNode<Smi> old_length = LoadFastJSArrayLength(fast_array);
      CSA_ASSERT(this, TaggedIsPositiveSmi(old_length));

      // 2) If the created array's length matches the required length, then
      //    there's nothing else to do. Otherwise use the runtime to set the
      //    property as that will insert holes into excess elements or shrink
      //    the backing store as appropriate.
      Branch(SmiNotEqual(length_smi, old_length), &runtime, &done);
    }

    BIND(&runtime);
    {
      SetPropertyStrict(context, array,
                        CodeStubAssembler::LengthStringConstant(), length);
      Goto(&done);
    }

    BIND(&done);
  }
};

// ES #sec-array.from
TF_BUILTIN(ArrayFrom, ArrayPopulatorAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));

  TNode<Object> map_function = args.GetOptionalArgumentValue(1);

  // If map_function is not undefined, then ensure it's callable else throw.
  {
    Label no_error(this), error(this);
    GotoIf(IsUndefined(map_function), &no_error);
    GotoIf(TaggedIsSmi(map_function), &error);
    Branch(IsCallable(CAST(map_function)), &no_error, &error);

    BIND(&error);
    ThrowTypeError(context, MessageTemplate::kCalledNonCallable, map_function);

    BIND(&no_error);
  }

  Label iterable(this), not_iterable(this), finished(this), if_exception(this);

  TNode<Object> this_arg = args.GetOptionalArgumentValue(2);
  TNode<Object> items = args.GetOptionalArgumentValue(0);
  // The spec doesn't require ToObject to be called directly on the iterable
  // branch, but it's part of GetMethod that is in the spec.
  TNode<JSReceiver> array_like = ToObject_Inline(context, items);

  TVARIABLE(Object, array);
  TVARIABLE(Number, length);

  // Determine whether items[Symbol.iterator] is defined:
  IteratorBuiltinsAssembler iterator_assembler(state());
  Node* iterator_method =
      iterator_assembler.GetIteratorMethod(context, array_like);
  Branch(IsNullOrUndefined(iterator_method), &not_iterable, &iterable);

  BIND(&iterable);
  {
    TVARIABLE(Number, index, SmiConstant(0));
    TVARIABLE(Object, var_exception);
    Label loop(this, &index), loop_done(this),
        on_exception(this, Label::kDeferred),
        index_overflow(this, Label::kDeferred);

    // Check that the method is callable.
    {
      Label get_method_not_callable(this, Label::kDeferred), next(this);
      GotoIf(TaggedIsSmi(iterator_method), &get_method_not_callable);
      GotoIfNot(IsCallable(CAST(iterator_method)), &get_method_not_callable);
      Goto(&next);

      BIND(&get_method_not_callable);
      ThrowTypeError(context, MessageTemplate::kCalledNonCallable,
                     iterator_method);

      BIND(&next);
    }

    // Construct the output array with empty length.
    array = ConstructArrayLike(context, args.GetReceiver());

    // Actually get the iterator and throw if the iterator method does not yield
    // one.
    IteratorRecord iterator_record =
        iterator_assembler.GetIterator(context, items, iterator_method);

    TNode<Context> native_context = LoadNativeContext(context);
    TNode<Object> fast_iterator_result_map =
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);

    Goto(&loop);

    BIND(&loop);
    {
      // Loop while iterator is not done.
      TNode<Object> next = CAST(iterator_assembler.IteratorStep(
          context, iterator_record, &loop_done, fast_iterator_result_map));
      TVARIABLE(Object, value,
                CAST(iterator_assembler.IteratorValue(
                    context, next, fast_iterator_result_map)));

      // If a map_function is supplied then call it (using this_arg as
      // receiver), on the value returned from the iterator. Exceptions are
      // caught so the iterator can be closed.
      {
        Label next(this);
        GotoIf(IsUndefined(map_function), &next);

        CSA_ASSERT(this, IsCallable(CAST(map_function)));
        Node* v = CallJS(CodeFactory::Call(isolate()), context, map_function,
                         this_arg, value.value(), index.value());
        GotoIfException(v, &on_exception, &var_exception);
        value = CAST(v);
        Goto(&next);
        BIND(&next);
      }

      // Store the result in the output object (catching any exceptions so the
      // iterator can be closed).
      Node* define_status =
          CallRuntime(Runtime::kCreateDataProperty, context, array.value(),
                      index.value(), value.value());
      GotoIfException(define_status, &on_exception, &var_exception);

      index = NumberInc(index.value());

      // The spec requires that we throw an exception if index reaches 2^53-1,
      // but an empty loop would take >100 days to do this many iterations. To
      // actually run for that long would require an iterator that never set
      // done to true and a target array which somehow never ran out of memory,
      // e.g. a proxy that discarded the values. Ignoring this case just means
      // we would repeatedly call CreateDataProperty with index = 2^53.
      CSA_ASSERT_BRANCH(this, [&](Label* ok, Label* not_ok) {
        BranchIfNumberRelationalComparison(Operation::kLessThan, index.value(),
                                           NumberConstant(kMaxSafeInteger), ok,
                                           not_ok);
      });
      Goto(&loop);
    }

    BIND(&loop_done);
    {
      length = index;
      Goto(&finished);
    }

    BIND(&on_exception);
    {
      // Close the iterator, rethrowing either the passed exception or
      // exceptions thrown during the close.
      iterator_assembler.IteratorCloseOnException(context, iterator_record,
                                                  &var_exception);
    }
  }

  BIND(&not_iterable);
  {
    // Treat array_like as an array and try to get its length.
    length = ToLength_Inline(
        context, GetProperty(context, array_like, factory()->length_string()));

    // Construct an array using the receiver as constructor with the same length
    // as the input array.
    array = ConstructArrayLike(context, args.GetReceiver(), length.value());

    TVARIABLE(Number, index, SmiConstant(0));

    // TODO(ishell): remove <Object, Object>
    GotoIf(WordEqual<Object, Object>(length.value(), SmiConstant(0)),
           &finished);

    // Loop from 0 to length-1.
    {
      Label loop(this, &index);
      Goto(&loop);
      BIND(&loop);
      TVARIABLE(Object, value);

      value = GetProperty(context, array_like, index.value());

      // If a map_function is supplied then call it (using this_arg as
      // receiver), on the value retrieved from the array.
      {
        Label next(this);
        GotoIf(IsUndefined(map_function), &next);

        CSA_ASSERT(this, IsCallable(CAST(map_function)));
        value = CAST(CallJS(CodeFactory::Call(isolate()), context, map_function,
                            this_arg, value.value(), index.value()));
        Goto(&next);
        BIND(&next);
      }

      // Store the result in the output object.
      CallRuntime(Runtime::kCreateDataProperty, context, array.value(),
                  index.value(), value.value());
      index = NumberInc(index.value());
      BranchIfNumberRelationalComparison(Operation::kLessThan, index.value(),
                                         length.value(), &loop, &finished);
    }
  }

  BIND(&finished);

  // Finally set the length on the output and return it.
  GenerateSetLength(context, array.value(), length.value());
  args.PopAndReturn(array.value());
}

// ES #sec-array.of
TF_BUILTIN(ArrayOf, ArrayPopulatorAssembler) {
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Smi> length = SmiFromInt32(argc);

  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CodeStubArguments args(this, length, nullptr, ParameterMode::SMI_PARAMETERS);

  TNode<Object> array = ConstructArrayLike(context, args.GetReceiver(), length);

  // TODO(delphick): Avoid using CreateDataProperty on the fast path.
  BuildFastLoop(SmiConstant(0), length,
                [=](Node* index) {
                  CallRuntime(
                      Runtime::kCreateDataProperty, context,
                      static_cast<Node*>(array), index,
                      args.AtIndex(index, ParameterMode::SMI_PARAMETERS));
                },
                1, ParameterMode::SMI_PARAMETERS, IndexAdvanceMode::kPost);

  GenerateSetLength(context, array, length);
  args.PopAndReturn(array);
}

// ES #sec-get-%typedarray%.prototype.find
TF_BUILTIN(TypedArrayPrototypeFind, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.find",
      &ArrayBuiltinsAssembler::FindResultGenerator,
      &ArrayBuiltinsAssembler::FindProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction);
}

// ES #sec-get-%typedarray%.prototype.findIndex
TF_BUILTIN(TypedArrayPrototypeFindIndex, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.findIndex",
      &ArrayBuiltinsAssembler::FindIndexResultGenerator,
      &ArrayBuiltinsAssembler::FindIndexProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction);
}

TF_BUILTIN(TypedArrayPrototypeForEach, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.forEach",
      &ArrayBuiltinsAssembler::ForEachResultGenerator,
      &ArrayBuiltinsAssembler::ForEachProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArraySomeLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* result = Parameter(Descriptor::kResult);

  // This custom lazy deopt point is right after the callback. every() needs
  // to pick up at the next step, which is either continuing to the next
  // array element or returning false if {result} is false.
  Label true_continue(this), false_continue(this);

  // iii. If selected is true, then...
  BranchIfToBooleanIsTrue(result, &true_continue, &false_continue);
  BIND(&true_continue);
  { Return(TrueConstant()); }
  BIND(&false_continue);
  {
    // Increment k.
    initial_k = NumberInc(initial_k);

    Return(CallBuiltin(Builtins::kArraySomeLoopContinuation, context, receiver,
                       callbackfn, this_arg, FalseConstant(), receiver,
                       initial_k, len, UndefinedConstant()));
  }
}

TF_BUILTIN(ArraySomeLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArraySomeLoopContinuation, context, receiver,
                     callbackfn, this_arg, FalseConstant(), receiver, initial_k,
                     len, UndefinedConstant()));
}

TF_BUILTIN(ArraySomeLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::SomeProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction, MissingPropertyMode::kSkip);
}

TF_BUILTIN(ArraySome, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.some", &ArrayBuiltinsAssembler::SomeResultGenerator,
      &ArrayBuiltinsAssembler::SomeProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArraySomeLoopContinuation),
      MissingPropertyMode::kSkip);
}

TF_BUILTIN(TypedArrayPrototypeSome, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.some",
      &ArrayBuiltinsAssembler::SomeResultGenerator,
      &ArrayBuiltinsAssembler::SomeProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayEveryLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* result = Parameter(Descriptor::kResult);

  // This custom lazy deopt point is right after the callback. every() needs
  // to pick up at the next step, which is either continuing to the next
  // array element or returning false if {result} is false.
  Label true_continue(this), false_continue(this);

  // iii. If selected is true, then...
  BranchIfToBooleanIsTrue(result, &true_continue, &false_continue);
  BIND(&true_continue);
  {
    // Increment k.
    initial_k = NumberInc(initial_k);

    Return(CallBuiltin(Builtins::kArrayEveryLoopContinuation, context, receiver,
                       callbackfn, this_arg, TrueConstant(), receiver,
                       initial_k, len, UndefinedConstant()));
  }
  BIND(&false_continue);
  { Return(FalseConstant()); }
}

TF_BUILTIN(ArrayEveryLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayEveryLoopContinuation, context, receiver,
                     callbackfn, this_arg, TrueConstant(), receiver, initial_k,
                     len, UndefinedConstant()));
}

TF_BUILTIN(ArrayEveryLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::EveryProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction, MissingPropertyMode::kSkip);
}

TF_BUILTIN(ArrayEvery, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.every", &ArrayBuiltinsAssembler::EveryResultGenerator,
      &ArrayBuiltinsAssembler::EveryProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayEveryLoopContinuation),
      MissingPropertyMode::kSkip);
}

TF_BUILTIN(TypedArrayPrototypeEvery, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.every",
      &ArrayBuiltinsAssembler::EveryResultGenerator,
      &ArrayBuiltinsAssembler::EveryProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayReduceLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* accumulator = Parameter(Descriptor::kAccumulator);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, accumulator, object,
                                            initial_k, len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::ReduceProcessor,
      &ArrayBuiltinsAssembler::ReducePostLoopAction,
      MissingPropertyMode::kSkip);
}

TF_BUILTIN(ArrayReducePreLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  // Simulate starting the loop at 0, but ensuring that the accumulator is
  // the hole. The continuation stub will search for the initial non-hole
  // element, rightly throwing an exception if not found.
  Return(CallBuiltin(Builtins::kArrayReduceLoopContinuation, context, receiver,
                     callbackfn, UndefinedConstant(), TheHoleConstant(),
                     receiver, SmiConstant(0), len, UndefinedConstant()));
}

TF_BUILTIN(ArrayReduceLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* accumulator = Parameter(Descriptor::kAccumulator);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayReduceLoopContinuation, context, receiver,
                     callbackfn, UndefinedConstant(), accumulator, receiver,
                     initial_k, len, UndefinedConstant()));
}

TF_BUILTIN(ArrayReduceLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* result = Parameter(Descriptor::kResult);

  Return(CallBuiltin(Builtins::kArrayReduceLoopContinuation, context, receiver,
                     callbackfn, UndefinedConstant(), result, receiver,
                     initial_k, len, UndefinedConstant()));
}

TF_BUILTIN(ArrayReduce, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.reduce", &ArrayBuiltinsAssembler::ReduceResultGenerator,
      &ArrayBuiltinsAssembler::ReduceProcessor,
      &ArrayBuiltinsAssembler::ReducePostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayReduceLoopContinuation),
      MissingPropertyMode::kSkip);
}

TF_BUILTIN(TypedArrayPrototypeReduce, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.reduce",
      &ArrayBuiltinsAssembler::ReduceResultGenerator,
      &ArrayBuiltinsAssembler::ReduceProcessor,
      &ArrayBuiltinsAssembler::ReducePostLoopAction);
}

TF_BUILTIN(ArrayReduceRightLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* accumulator = Parameter(Descriptor::kAccumulator);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, accumulator, object,
                                            initial_k, len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::ReduceProcessor,
      &ArrayBuiltinsAssembler::ReducePostLoopAction, MissingPropertyMode::kSkip,
      ForEachDirection::kReverse);
}

TF_BUILTIN(ArrayReduceRightPreLoopEagerDeoptContinuation,
           ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  TNode<Smi> len = CAST(Parameter(Descriptor::kLength));

  // Simulate starting the loop at 0, but ensuring that the accumulator is
  // the hole. The continuation stub will search for the initial non-hole
  // element, rightly throwing an exception if not found.
  Return(CallBuiltin(Builtins::kArrayReduceRightLoopContinuation, context,
                     receiver, callbackfn, UndefinedConstant(),
                     TheHoleConstant(), receiver, SmiSub(len, SmiConstant(1)),
                     len, UndefinedConstant()));
}

TF_BUILTIN(ArrayReduceRightLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* accumulator = Parameter(Descriptor::kAccumulator);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayReduceRightLoopContinuation, context,
                     receiver, callbackfn, UndefinedConstant(), accumulator,
                     receiver, initial_k, len, UndefinedConstant()));
}

TF_BUILTIN(ArrayReduceRightLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* result = Parameter(Descriptor::kResult);

  Return(CallBuiltin(Builtins::kArrayReduceRightLoopContinuation, context,
                     receiver, callbackfn, UndefinedConstant(), result,
                     receiver, initial_k, len, UndefinedConstant()));
}

TF_BUILTIN(ArrayReduceRight, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.reduceRight",
      &ArrayBuiltinsAssembler::ReduceResultGenerator,
      &ArrayBuiltinsAssembler::ReduceProcessor,
      &ArrayBuiltinsAssembler::ReducePostLoopAction,
      Builtins::CallableFor(isolate(),
                            Builtins::kArrayReduceRightLoopContinuation),
      MissingPropertyMode::kSkip, ForEachDirection::kReverse);
}

TF_BUILTIN(TypedArrayPrototypeReduceRight, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.reduceRight",
      &ArrayBuiltinsAssembler::ReduceResultGenerator,
      &ArrayBuiltinsAssembler::ReduceProcessor,
      &ArrayBuiltinsAssembler::ReducePostLoopAction,
      ForEachDirection::kReverse);
}

TF_BUILTIN(ArrayFilterLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::FilterProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction, MissingPropertyMode::kSkip);
}

TF_BUILTIN(ArrayFilterLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  Return(CallBuiltin(Builtins::kArrayFilterLoopContinuation, context, receiver,
                     callbackfn, this_arg, array, receiver, initial_k, len,
                     to));
}

TF_BUILTIN(ArrayFilterLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* value_k = Parameter(Descriptor::kValueK);
  Node* result = Parameter(Descriptor::kResult);

  VARIABLE(to, MachineRepresentation::kTagged, Parameter(Descriptor::kTo));

  // This custom lazy deopt point is right after the callback. filter() needs
  // to pick up at the next step, which is setting the callback result in
  // the output array. After incrementing k and to, we can glide into the loop
  // continuation builtin.

  Label true_continue(this, &to), false_continue(this);

  // iii. If selected is true, then...
  BranchIfToBooleanIsTrue(result, &true_continue, &false_continue);
  BIND(&true_continue);
  {
    // 1. Perform ? CreateDataPropertyOrThrow(A, ToString(to), kValue).
    CallRuntime(Runtime::kCreateDataProperty, context, array, to.value(),
                value_k);
    // 2. Increase to by 1.
    to.Bind(NumberInc(to.value()));
    Goto(&false_continue);
  }
  BIND(&false_continue);

  // Increment k.
  initial_k = NumberInc(initial_k);

  Return(CallBuiltin(Builtins::kArrayFilterLoopContinuation, context, receiver,
                     callbackfn, this_arg, array, receiver, initial_k, len,
                     to.value()));
}

TF_BUILTIN(ArrayFilter, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.filter", &ArrayBuiltinsAssembler::FilterResultGenerator,
      &ArrayBuiltinsAssembler::FilterProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayFilterLoopContinuation),
      MissingPropertyMode::kSkip);
}

TF_BUILTIN(ArrayMapLoopContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  TNode<JSReceiver> object = CAST(Parameter(Descriptor::kObject));
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinsAssembler::SpecCompliantMapProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction, MissingPropertyMode::kSkip);
}

TF_BUILTIN(ArrayMapLoopEagerDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));

  Return(CallBuiltin(Builtins::kArrayMapLoopContinuation, context, receiver,
                     callbackfn, this_arg, array, receiver, initial_k, len,
                     UndefinedConstant()));
}

TF_BUILTIN(ArrayMapLoopLazyDeoptContinuation, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  TNode<Number> len = CAST(Parameter(Descriptor::kLength));
  Node* result = Parameter(Descriptor::kResult);

  // This custom lazy deopt point is right after the callback. map() needs
  // to pick up at the next step, which is setting the callback result in
  // the output array. After incrementing k, we can glide into the loop
  // continuation builtin.

  // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mappedValue).
  CallRuntime(Runtime::kCreateDataProperty, context, array, initial_k, result);
  // Then we have to increment k before going on.
  initial_k = NumberInc(initial_k);

  Return(CallBuiltin(Builtins::kArrayMapLoopContinuation, context, receiver,
                     callbackfn, this_arg, array, receiver, initial_k, len,
                     UndefinedConstant()));
}

TF_BUILTIN(ArrayMap, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.map", &ArrayBuiltinsAssembler::MapResultGenerator,
      &ArrayBuiltinsAssembler::FastMapProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayMapLoopContinuation),
      MissingPropertyMode::kSkip);
}

TF_BUILTIN(TypedArrayPrototypeMap, ArrayBuiltinsAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0);
  Node* this_arg = args.GetOptionalArgumentValue(1);

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.map",
      &ArrayBuiltinsAssembler::TypedArrayMapResultGenerator,
      &ArrayBuiltinsAssembler::TypedArrayMapProcessor,
      &ArrayBuiltinsAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayIsArray, CodeStubAssembler) {
  TNode<Object> object = CAST(Parameter(Descriptor::kArg));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label call_runtime(this), return_true(this), return_false(this);

  GotoIf(TaggedIsSmi(object), &return_false);
  TNode<Int32T> instance_type = LoadInstanceType(CAST(object));

  GotoIf(InstanceTypeEqual(instance_type, JS_ARRAY_TYPE), &return_true);

  // TODO(verwaest): Handle proxies in-place.
  Branch(InstanceTypeEqual(instance_type, JS_PROXY_TYPE), &call_runtime,
         &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());

  BIND(&call_runtime);
  Return(CallRuntime(Runtime::kArrayIsArray, context, object));
}

class ArrayIncludesIndexofAssembler : public CodeStubAssembler {
 public:
  explicit ArrayIncludesIndexofAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  enum SearchVariant { kIncludes, kIndexOf };

  void Generate(SearchVariant variant, TNode<IntPtrT> argc,
                TNode<Context> context);
  void GenerateSmiOrObject(SearchVariant variant, Node* context, Node* elements,
                           Node* search_element, Node* array_length,
                           Node* from_index);
  void GeneratePackedDoubles(SearchVariant variant, Node* elements,
                             Node* search_element, Node* array_length,
                             Node* from_index);
  void GenerateHoleyDoubles(SearchVariant variant, Node* elements,
                            Node* search_element, Node* array_length,
                            Node* from_index);
};

void ArrayIncludesIndexofAssembler::Generate(SearchVariant variant,
                                             TNode<IntPtrT> argc,
                                             TNode<Context> context) {
  const int kSearchElementArg = 0;
  const int kFromIndexArg = 1;

  CodeStubArguments args(this, argc);

  TNode<Object> receiver = args.GetReceiver();
  TNode<Object> search_element =
      args.GetOptionalArgumentValue(kSearchElementArg);

  Node* intptr_zero = IntPtrConstant(0);

  Label init_index(this), return_not_found(this), call_runtime(this);

  // Take slow path if not a JSArray, if retrieving elements requires
  // traversing prototype, or if access checks are required.
  BranchIfFastJSArray(receiver, context, &init_index, &call_runtime);

  BIND(&init_index);
  VARIABLE(index_var, MachineType::PointerRepresentation(), intptr_zero);
  TNode<JSArray> array = CAST(receiver);

  // JSArray length is always a positive Smi for fast arrays.
  CSA_ASSERT(this, TaggedIsPositiveSmi(LoadJSArrayLength(array)));
  Node* array_length = LoadFastJSArrayLength(array);
  Node* array_length_untagged = SmiUntag(array_length);

  {
    // Initialize fromIndex.
    Label is_smi(this), is_nonsmi(this), done(this);

    // If no fromIndex was passed, default to 0.
    GotoIf(IntPtrLessThanOrEqual(argc, IntPtrConstant(kFromIndexArg)), &done);

    Node* start_from = args.AtIndex(kFromIndexArg);
    // Handle Smis and undefined here and everything else in runtime.
    // We must be very careful with side effects from the ToInteger conversion,
    // as the side effects might render previously checked assumptions about
    // the receiver being a fast JSArray and its length invalid.
    Branch(TaggedIsSmi(start_from), &is_smi, &is_nonsmi);

    BIND(&is_nonsmi);
    {
      GotoIfNot(IsUndefined(start_from), &call_runtime);
      Goto(&done);
    }
    BIND(&is_smi);
    {
      Node* intptr_start_from = SmiUntag(start_from);
      index_var.Bind(intptr_start_from);

      GotoIf(IntPtrGreaterThanOrEqual(index_var.value(), intptr_zero), &done);
      // The fromIndex is negative: add it to the array's length.
      index_var.Bind(IntPtrAdd(array_length_untagged, index_var.value()));
      // Clamp negative results at zero.
      GotoIf(IntPtrGreaterThanOrEqual(index_var.value(), intptr_zero), &done);
      index_var.Bind(intptr_zero);
      Goto(&done);
    }
    BIND(&done);
  }

  // Fail early if startIndex >= array.length.
  GotoIf(IntPtrGreaterThanOrEqual(index_var.value(), array_length_untagged),
         &return_not_found);

  Label if_smiorobjects(this), if_packed_doubles(this), if_holey_doubles(this);

  TNode<Int32T> elements_kind = LoadElementsKind(array);
  Node* elements = LoadElements(array);
  STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(PACKED_ELEMENTS == 2);
  STATIC_ASSERT(HOLEY_ELEMENTS == 3);
  GotoIf(Uint32LessThanOrEqual(elements_kind, Int32Constant(HOLEY_ELEMENTS)),
         &if_smiorobjects);
  GotoIf(Word32Equal(elements_kind, Int32Constant(PACKED_DOUBLE_ELEMENTS)),
         &if_packed_doubles);
  GotoIf(Word32Equal(elements_kind, Int32Constant(HOLEY_DOUBLE_ELEMENTS)),
         &if_holey_doubles);
  Goto(&return_not_found);

  BIND(&if_smiorobjects);
  {
    Callable callable =
        (variant == kIncludes)
            ? Builtins::CallableFor(isolate(),
                                    Builtins::kArrayIncludesSmiOrObject)
            : Builtins::CallableFor(isolate(),
                                    Builtins::kArrayIndexOfSmiOrObject);
    Node* result = CallStub(callable, context, elements, search_element,
                            array_length, SmiTag(index_var.value()));
    args.PopAndReturn(result);
  }

  BIND(&if_packed_doubles);
  {
    Callable callable =
        (variant == kIncludes)
            ? Builtins::CallableFor(isolate(),
                                    Builtins::kArrayIncludesPackedDoubles)
            : Builtins::CallableFor(isolate(),
                                    Builtins::kArrayIndexOfPackedDoubles);
    Node* result = CallStub(callable, context, elements, search_element,
                            array_length, SmiTag(index_var.value()));
    args.PopAndReturn(result);
  }

  BIND(&if_holey_doubles);
  {
    Callable callable =
        (variant == kIncludes)
            ? Builtins::CallableFor(isolate(),
                                    Builtins::kArrayIncludesHoleyDoubles)
            : Builtins::CallableFor(isolate(),
                                    Builtins::kArrayIndexOfHoleyDoubles);
    Node* result = CallStub(callable, context, elements, search_element,
                            array_length, SmiTag(index_var.value()));
    args.PopAndReturn(result);
  }

  BIND(&return_not_found);
  if (variant == kIncludes) {
    args.PopAndReturn(FalseConstant());
  } else {
    args.PopAndReturn(NumberConstant(-1));
  }

  BIND(&call_runtime);
  {
    Node* start_from =
        args.GetOptionalArgumentValue(kFromIndexArg, UndefinedConstant());
    Runtime::FunctionId function = variant == kIncludes
                                       ? Runtime::kArrayIncludes_Slow
                                       : Runtime::kArrayIndexOf;
    args.PopAndReturn(
        CallRuntime(function, context, array, search_element, start_from));
  }
}

void ArrayIncludesIndexofAssembler::GenerateSmiOrObject(
    SearchVariant variant, Node* context, Node* elements, Node* search_element,
    Node* array_length, Node* from_index) {
  VARIABLE(index_var, MachineType::PointerRepresentation(),
           SmiUntag(from_index));
  VARIABLE(search_num, MachineRepresentation::kFloat64);
  Node* array_length_untagged = SmiUntag(array_length);

  Label ident_loop(this, &index_var), heap_num_loop(this, &search_num),
      string_loop(this), bigint_loop(this, &index_var),
      undef_loop(this, &index_var), not_smi(this), not_heap_num(this),
      return_found(this), return_not_found(this);

  GotoIfNot(TaggedIsSmi(search_element), &not_smi);
  search_num.Bind(SmiToFloat64(search_element));
  Goto(&heap_num_loop);

  BIND(&not_smi);
  if (variant == kIncludes) {
    GotoIf(IsUndefined(search_element), &undef_loop);
  }
  Node* map = LoadMap(search_element);
  GotoIfNot(IsHeapNumberMap(map), &not_heap_num);
  search_num.Bind(LoadHeapNumberValue(search_element));
  Goto(&heap_num_loop);

  BIND(&not_heap_num);
  Node* search_type = LoadMapInstanceType(map);
  GotoIf(IsStringInstanceType(search_type), &string_loop);
  GotoIf(IsBigIntInstanceType(search_type), &bigint_loop);
  Goto(&ident_loop);

  BIND(&ident_loop);
  {
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);
    Node* element_k = LoadFixedArrayElement(CAST(elements), index_var.value());
    GotoIf(WordEqual(element_k, search_element), &return_found);

    Increment(&index_var);
    Goto(&ident_loop);
  }

  if (variant == kIncludes) {
    BIND(&undef_loop);

    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);
    Node* element_k = LoadFixedArrayElement(CAST(elements), index_var.value());
    GotoIf(IsUndefined(element_k), &return_found);
    GotoIf(IsTheHole(element_k), &return_found);

    Increment(&index_var);
    Goto(&undef_loop);
  }

  BIND(&heap_num_loop);
  {
    Label nan_loop(this, &index_var), not_nan_loop(this, &index_var);
    Label* nan_handling = variant == kIncludes ? &nan_loop : &return_not_found;
    BranchIfFloat64IsNaN(search_num.value(), nan_handling, &not_nan_loop);

    BIND(&not_nan_loop);
    {
      Label continue_loop(this), not_smi(this);
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
                &return_not_found);
      Node* element_k =
          LoadFixedArrayElement(CAST(elements), index_var.value());
      GotoIfNot(TaggedIsSmi(element_k), &not_smi);
      Branch(Float64Equal(search_num.value(), SmiToFloat64(element_k)),
             &return_found, &continue_loop);

      BIND(&not_smi);
      GotoIfNot(IsHeapNumber(element_k), &continue_loop);
      Branch(Float64Equal(search_num.value(), LoadHeapNumberValue(element_k)),
             &return_found, &continue_loop);

      BIND(&continue_loop);
      Increment(&index_var);
      Goto(&not_nan_loop);
    }

    // Array.p.includes uses SameValueZero comparisons, where NaN == NaN.
    if (variant == kIncludes) {
      BIND(&nan_loop);
      Label continue_loop(this);
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
                &return_not_found);
      Node* element_k =
          LoadFixedArrayElement(CAST(elements), index_var.value());
      GotoIf(TaggedIsSmi(element_k), &continue_loop);
      GotoIfNot(IsHeapNumber(CAST(element_k)), &continue_loop);
      BranchIfFloat64IsNaN(LoadHeapNumberValue(element_k), &return_found,
                           &continue_loop);

      BIND(&continue_loop);
      Increment(&index_var);
      Goto(&nan_loop);
    }
  }

  BIND(&string_loop);
  {
    TNode<String> search_element_string = CAST(search_element);
    Label continue_loop(this), next_iteration(this, &index_var),
        slow_compare(this), runtime(this, Label::kDeferred);
    TNode<IntPtrT> search_length =
        LoadStringLengthAsWord(search_element_string);
    Goto(&next_iteration);
    BIND(&next_iteration);
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);
    Node* element_k = LoadFixedArrayElement(CAST(elements), index_var.value());
    GotoIf(TaggedIsSmi(element_k), &continue_loop);
    GotoIf(WordEqual(search_element_string, element_k), &return_found);
    Node* element_k_type = LoadInstanceType(element_k);
    GotoIfNot(IsStringInstanceType(element_k_type), &continue_loop);
    Branch(WordEqual(search_length, LoadStringLengthAsWord(element_k)),
           &slow_compare, &continue_loop);

    BIND(&slow_compare);
    StringBuiltinsAssembler string_asm(state());
    string_asm.StringEqual_Core(context, search_element_string, search_type,
                                element_k, element_k_type, search_length,
                                &return_found, &continue_loop, &runtime);
    BIND(&runtime);
    TNode<Object> result = CallRuntime(Runtime::kStringEqual, context,
                                       search_element_string, element_k);
    Branch(WordEqual(result, TrueConstant()), &return_found, &continue_loop);

    BIND(&continue_loop);
    Increment(&index_var);
    Goto(&next_iteration);
  }

  BIND(&bigint_loop);
  {
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);

    Node* element_k = LoadFixedArrayElement(CAST(elements), index_var.value());
    Label continue_loop(this);
    GotoIf(TaggedIsSmi(element_k), &continue_loop);
    GotoIfNot(IsBigInt(CAST(element_k)), &continue_loop);
    TNode<Object> result = CallRuntime(Runtime::kBigIntEqualToBigInt, context,
                                       search_element, element_k);
    Branch(WordEqual(result, TrueConstant()), &return_found, &continue_loop);

    BIND(&continue_loop);
    Increment(&index_var);
    Goto(&bigint_loop);
  }
  BIND(&return_found);
  if (variant == kIncludes) {
    Return(TrueConstant());
  } else {
    Return(SmiTag(index_var.value()));
  }

  BIND(&return_not_found);
  if (variant == kIncludes) {
    Return(FalseConstant());
  } else {
    Return(NumberConstant(-1));
  }
}

void ArrayIncludesIndexofAssembler::GeneratePackedDoubles(SearchVariant variant,
                                                          Node* elements,
                                                          Node* search_element,
                                                          Node* array_length,
                                                          Node* from_index) {
  VARIABLE(index_var, MachineType::PointerRepresentation(),
           SmiUntag(from_index));
  Node* array_length_untagged = SmiUntag(array_length);

  Label nan_loop(this, &index_var), not_nan_loop(this, &index_var),
      hole_loop(this, &index_var), search_notnan(this), return_found(this),
      return_not_found(this);
  VARIABLE(search_num, MachineRepresentation::kFloat64);
  search_num.Bind(Float64Constant(0));

  GotoIfNot(TaggedIsSmi(search_element), &search_notnan);
  search_num.Bind(SmiToFloat64(search_element));
  Goto(&not_nan_loop);

  BIND(&search_notnan);
  GotoIfNot(IsHeapNumber(search_element), &return_not_found);

  search_num.Bind(LoadHeapNumberValue(search_element));

  Label* nan_handling = variant == kIncludes ? &nan_loop : &return_not_found;
  BranchIfFloat64IsNaN(search_num.value(), nan_handling, &not_nan_loop);

  BIND(&not_nan_loop);
  {
    Label continue_loop(this);
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);
    Node* element_k = LoadFixedDoubleArrayElement(elements, index_var.value(),
                                                  MachineType::Float64());
    Branch(Float64Equal(element_k, search_num.value()), &return_found,
           &continue_loop);
    BIND(&continue_loop);
    Increment(&index_var);
    Goto(&not_nan_loop);
  }

  // Array.p.includes uses SameValueZero comparisons, where NaN == NaN.
  if (variant == kIncludes) {
    BIND(&nan_loop);
    Label continue_loop(this);
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);
    Node* element_k = LoadFixedDoubleArrayElement(elements, index_var.value(),
                                                  MachineType::Float64());
    BranchIfFloat64IsNaN(element_k, &return_found, &continue_loop);
    BIND(&continue_loop);
    Increment(&index_var);
    Goto(&nan_loop);
  }

  BIND(&return_found);
  if (variant == kIncludes) {
    Return(TrueConstant());
  } else {
    Return(SmiTag(index_var.value()));
  }

  BIND(&return_not_found);
  if (variant == kIncludes) {
    Return(FalseConstant());
  } else {
    Return(NumberConstant(-1));
  }
}

void ArrayIncludesIndexofAssembler::GenerateHoleyDoubles(SearchVariant variant,
                                                         Node* elements,
                                                         Node* search_element,
                                                         Node* array_length,
                                                         Node* from_index) {
  VARIABLE(index_var, MachineType::PointerRepresentation(),
           SmiUntag(from_index));
  Node* array_length_untagged = SmiUntag(array_length);

  Label nan_loop(this, &index_var), not_nan_loop(this, &index_var),
      hole_loop(this, &index_var), search_notnan(this), return_found(this),
      return_not_found(this);
  VARIABLE(search_num, MachineRepresentation::kFloat64);
  search_num.Bind(Float64Constant(0));

  GotoIfNot(TaggedIsSmi(search_element), &search_notnan);
  search_num.Bind(SmiToFloat64(search_element));
  Goto(&not_nan_loop);

  BIND(&search_notnan);
  if (variant == kIncludes) {
    GotoIf(IsUndefined(search_element), &hole_loop);
  }
  GotoIfNot(IsHeapNumber(search_element), &return_not_found);

  search_num.Bind(LoadHeapNumberValue(search_element));

  Label* nan_handling = variant == kIncludes ? &nan_loop : &return_not_found;
  BranchIfFloat64IsNaN(search_num.value(), nan_handling, &not_nan_loop);

  BIND(&not_nan_loop);
  {
    Label continue_loop(this);
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);

    // No need for hole checking here; the following Float64Equal will
    // return 'not equal' for holes anyway.
    Node* element_k = LoadFixedDoubleArrayElement(elements, index_var.value(),
                                                  MachineType::Float64());

    Branch(Float64Equal(element_k, search_num.value()), &return_found,
           &continue_loop);
    BIND(&continue_loop);
    Increment(&index_var);
    Goto(&not_nan_loop);
  }

  // Array.p.includes uses SameValueZero comparisons, where NaN == NaN.
  if (variant == kIncludes) {
    BIND(&nan_loop);
    Label continue_loop(this);
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);

    // Load double value or continue if it's the hole NaN.
    Node* element_k = LoadFixedDoubleArrayElement(
        elements, index_var.value(), MachineType::Float64(), 0,
        INTPTR_PARAMETERS, &continue_loop);

    BranchIfFloat64IsNaN(element_k, &return_found, &continue_loop);
    BIND(&continue_loop);
    Increment(&index_var);
    Goto(&nan_loop);
  }

  // Array.p.includes treats the hole as undefined.
  if (variant == kIncludes) {
    BIND(&hole_loop);
    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);

    // Check if the element is a double hole, but don't load it.
    LoadFixedDoubleArrayElement(elements, index_var.value(),
                                MachineType::None(), 0, INTPTR_PARAMETERS,
                                &return_found);

    Increment(&index_var);
    Goto(&hole_loop);
  }

  BIND(&return_found);
  if (variant == kIncludes) {
    Return(TrueConstant());
  } else {
    Return(SmiTag(index_var.value()));
  }

  BIND(&return_not_found);
  if (variant == kIncludes) {
    Return(FalseConstant());
  } else {
    Return(NumberConstant(-1));
  }
}

TF_BUILTIN(ArrayIncludes, ArrayIncludesIndexofAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(kIncludes, argc, context);
}

TF_BUILTIN(ArrayIncludesSmiOrObject, ArrayIncludesIndexofAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* elements = Parameter(Descriptor::kElements);
  Node* search_element = Parameter(Descriptor::kSearchElement);
  Node* array_length = Parameter(Descriptor::kLength);
  Node* from_index = Parameter(Descriptor::kFromIndex);

  GenerateSmiOrObject(kIncludes, context, elements, search_element,
                      array_length, from_index);
}

TF_BUILTIN(ArrayIncludesPackedDoubles, ArrayIncludesIndexofAssembler) {
  Node* elements = Parameter(Descriptor::kElements);
  Node* search_element = Parameter(Descriptor::kSearchElement);
  Node* array_length = Parameter(Descriptor::kLength);
  Node* from_index = Parameter(Descriptor::kFromIndex);

  GeneratePackedDoubles(kIncludes, elements, search_element, array_length,
                        from_index);
}

TF_BUILTIN(ArrayIncludesHoleyDoubles, ArrayIncludesIndexofAssembler) {
  Node* elements = Parameter(Descriptor::kElements);
  Node* search_element = Parameter(Descriptor::kSearchElement);
  Node* array_length = Parameter(Descriptor::kLength);
  Node* from_index = Parameter(Descriptor::kFromIndex);

  GenerateHoleyDoubles(kIncludes, elements, search_element, array_length,
                       from_index);
}

TF_BUILTIN(ArrayIndexOf, ArrayIncludesIndexofAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(kIndexOf, argc, context);
}

TF_BUILTIN(ArrayIndexOfSmiOrObject, ArrayIncludesIndexofAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* elements = Parameter(Descriptor::kElements);
  Node* search_element = Parameter(Descriptor::kSearchElement);
  Node* array_length = Parameter(Descriptor::kLength);
  Node* from_index = Parameter(Descriptor::kFromIndex);

  GenerateSmiOrObject(kIndexOf, context, elements, search_element, array_length,
                      from_index);
}

TF_BUILTIN(ArrayIndexOfPackedDoubles, ArrayIncludesIndexofAssembler) {
  Node* elements = Parameter(Descriptor::kElements);
  Node* search_element = Parameter(Descriptor::kSearchElement);
  Node* array_length = Parameter(Descriptor::kLength);
  Node* from_index = Parameter(Descriptor::kFromIndex);

  GeneratePackedDoubles(kIndexOf, elements, search_element, array_length,
                        from_index);
}

TF_BUILTIN(ArrayIndexOfHoleyDoubles, ArrayIncludesIndexofAssembler) {
  Node* elements = Parameter(Descriptor::kElements);
  Node* search_element = Parameter(Descriptor::kSearchElement);
  Node* array_length = Parameter(Descriptor::kLength);
  Node* from_index = Parameter(Descriptor::kFromIndex);

  GenerateHoleyDoubles(kIndexOf, elements, search_element, array_length,
                       from_index);
}

// ES #sec-array.prototype.values
TF_BUILTIN(ArrayPrototypeValues, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Return(CreateArrayIterator(context, ToObject_Inline(context, receiver),
                             IterationKind::kValues));
}

// ES #sec-array.prototype.entries
TF_BUILTIN(ArrayPrototypeEntries, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Return(CreateArrayIterator(context, ToObject_Inline(context, receiver),
                             IterationKind::kEntries));
}

// ES #sec-array.prototype.keys
TF_BUILTIN(ArrayPrototypeKeys, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Return(CreateArrayIterator(context, ToObject_Inline(context, receiver),
                             IterationKind::kKeys));
}

// ES #sec-%arrayiteratorprototype%.next
TF_BUILTIN(ArrayIteratorPrototypeNext, CodeStubAssembler) {
  const char* method_name = "Array Iterator.prototype.next";

  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* iterator = Parameter(Descriptor::kReceiver);

  VARIABLE(var_done, MachineRepresentation::kTagged, TrueConstant());
  VARIABLE(var_value, MachineRepresentation::kTagged, UndefinedConstant());

  Label allocate_entry_if_needed(this);
  Label allocate_iterator_result(this);
  Label if_typedarray(this), if_other(this, Label::kDeferred), if_array(this),
      if_generic(this, Label::kDeferred);
  Label set_done(this, Label::kDeferred);

  // If O does not have all of the internal slots of an Array Iterator Instance
  // (22.1.5.3), throw a TypeError exception
  ThrowIfNotInstanceType(context, iterator, JS_ARRAY_ITERATOR_TYPE,
                         method_name);

  // Let a be O.[[IteratedObject]].
  TNode<JSReceiver> array =
      CAST(LoadObjectField(iterator, JSArrayIterator::kIteratedObjectOffset));

  // Let index be O.[[ArrayIteratorNextIndex]].
  TNode<Number> index =
      CAST(LoadObjectField(iterator, JSArrayIterator::kNextIndexOffset));
  CSA_ASSERT(this, IsNumberNonNegativeSafeInteger(index));

  // Dispatch based on the type of the {array}.
  TNode<Map> array_map = LoadMap(array);
  TNode<Int32T> array_type = LoadMapInstanceType(array_map);
  GotoIf(InstanceTypeEqual(array_type, JS_ARRAY_TYPE), &if_array);
  Branch(InstanceTypeEqual(array_type, JS_TYPED_ARRAY_TYPE), &if_typedarray,
         &if_other);

  BIND(&if_array);
  {
    // If {array} is a JSArray, then the {index} must be in Unsigned32 range.
    CSA_ASSERT(this, IsNumberArrayIndex(index));

    // Check that the {index} is within range for the {array}. We handle all
    // kinds of JSArray's here, so we do the computation on Uint32.
    TNode<Uint32T> index32 = ChangeNumberToUint32(index);
    TNode<Uint32T> length32 =
        ChangeNumberToUint32(LoadJSArrayLength(CAST(array)));
    GotoIfNot(Uint32LessThan(index32, length32), &set_done);
    StoreObjectField(
        iterator, JSArrayIterator::kNextIndexOffset,
        ChangeUint32ToTagged(Unsigned(Int32Add(index32, Int32Constant(1)))));

    var_done.Bind(FalseConstant());
    var_value.Bind(index);

    GotoIf(Word32Equal(LoadAndUntagToWord32ObjectField(
                           iterator, JSArrayIterator::kKindOffset),
                       Int32Constant(static_cast<int>(IterationKind::kKeys))),
           &allocate_iterator_result);

    Label if_hole(this, Label::kDeferred);
    TNode<Int32T> elements_kind = LoadMapElementsKind(array_map);
    TNode<FixedArrayBase> elements = LoadElements(CAST(array));
    var_value.Bind(LoadFixedArrayBaseElementAsTagged(
        elements, Signed(ChangeUint32ToWord(index32)), elements_kind,
        &if_generic, &if_hole));
    Goto(&allocate_entry_if_needed);

    BIND(&if_hole);
    {
      GotoIf(IsNoElementsProtectorCellInvalid(), &if_generic);
      var_value.Bind(UndefinedConstant());
      Goto(&allocate_entry_if_needed);
    }
  }

  BIND(&if_other);
  {
    // We cannot enter here with either JSArray's or JSTypedArray's.
    CSA_ASSERT(this, Word32BinaryNot(IsJSArray(array)));
    CSA_ASSERT(this, Word32BinaryNot(IsJSTypedArray(array)));

    // Check that the {index} is within the bounds of the {array}s "length".
    TNode<Number> length = CAST(
        CallBuiltin(Builtins::kToLength, context,
                    GetProperty(context, array, factory()->length_string())));
    GotoIfNumberGreaterThanOrEqual(index, length, &set_done);
    StoreObjectField(iterator, JSArrayIterator::kNextIndexOffset,
                     NumberInc(index));

    var_done.Bind(FalseConstant());
    var_value.Bind(index);

    Branch(Word32Equal(LoadAndUntagToWord32ObjectField(
                           iterator, JSArrayIterator::kKindOffset),
                       Int32Constant(static_cast<int>(IterationKind::kKeys))),
           &allocate_iterator_result, &if_generic);
  }

  BIND(&set_done);
  {
    // Change the [[ArrayIteratorNextIndex]] such that the {iterator} will
    // never produce values anymore, because it will always fail the bounds
    // check. Note that this is different from what the specification does,
    // which is changing the [[IteratedObject]] to undefined, because leaving
    // [[IteratedObject]] alone helps TurboFan to generate better code with
    // the inlining in JSCallReducer::ReduceArrayIteratorPrototypeNext().
    //
    // The terminal value we chose here depends on the type of the {array},
    // for JSArray's we use kMaxUInt32 so that TurboFan can always use
    // Word32 representation for fast-path indices (and this is safe since
    // the "length" of JSArray's is limited to Unsigned32 range). For other
    // JSReceiver's we have to use kMaxSafeInteger, since the "length" can
    // be any arbitrary value in the safe integer range.
    //
    // Note specifically that JSTypedArray's will never take this path, so
    // we don't need to worry about their maximum value.
    CSA_ASSERT(this, Word32BinaryNot(IsJSTypedArray(array)));
    TNode<Number> max_length =
        SelectConstant(IsJSArray(array), NumberConstant(kMaxUInt32),
                       NumberConstant(kMaxSafeInteger));
    StoreObjectField(iterator, JSArrayIterator::kNextIndexOffset, max_length);
    Goto(&allocate_iterator_result);
  }

  BIND(&if_generic);
  {
    var_value.Bind(GetProperty(context, array, index));
    Goto(&allocate_entry_if_needed);
  }

  BIND(&if_typedarray);
  {
    // If {array} is a JSTypedArray, the {index} must always be a Smi.
    CSA_ASSERT(this, TaggedIsSmi(index));

    // Check that the {array}s buffer wasn't neutered.
    ThrowIfArrayBufferViewBufferIsDetached(context, CAST(array), method_name);

    // If we go outside of the {length}, we don't need to update the
    // [[ArrayIteratorNextIndex]] anymore, since a JSTypedArray's
    // length cannot change anymore, so this {iterator} will never
    // produce values again anyways.
    TNode<Smi> length = LoadTypedArrayLength(CAST(array));
    GotoIfNot(SmiBelow(CAST(index), length), &allocate_iterator_result);
    StoreObjectFieldNoWriteBarrier(iterator, JSArrayIterator::kNextIndexOffset,
                                   SmiInc(CAST(index)));

    var_done.Bind(FalseConstant());
    var_value.Bind(index);

    GotoIf(Word32Equal(LoadAndUntagToWord32ObjectField(
                           iterator, JSArrayIterator::kKindOffset),
                       Int32Constant(static_cast<int>(IterationKind::kKeys))),
           &allocate_iterator_result);

    TNode<Int32T> elements_kind = LoadMapElementsKind(array_map);
    Node* elements = LoadElements(CAST(array));
    Node* base_ptr =
        LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
    Node* external_ptr =
        LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                        MachineType::Pointer());
    TNode<WordT> data_ptr =
        IntPtrAdd(BitcastTaggedToWord(base_ptr), external_ptr);
    var_value.Bind(LoadFixedTypedArrayElementAsTagged(data_ptr, CAST(index),
                                                      elements_kind));
    Goto(&allocate_entry_if_needed);
  }

  BIND(&allocate_entry_if_needed);
  {
    GotoIf(Word32Equal(LoadAndUntagToWord32ObjectField(
                           iterator, JSArrayIterator::kKindOffset),
                       Int32Constant(static_cast<int>(IterationKind::kValues))),
           &allocate_iterator_result);

    Node* result =
        AllocateJSIteratorResultForEntry(context, index, var_value.value());
    Return(result);
  }

  BIND(&allocate_iterator_result);
  {
    Node* result =
        AllocateJSIteratorResult(context, var_value.value(), var_done.value());
    Return(result);
  }
}

namespace {

class ArrayFlattenAssembler : public CodeStubAssembler {
 public:
  explicit ArrayFlattenAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  // https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray
  Node* FlattenIntoArray(Node* context, Node* target, Node* source,
                         Node* source_length, Node* start, Node* depth,
                         Node* mapper_function = nullptr,
                         Node* this_arg = nullptr) {
    CSA_ASSERT(this, IsJSReceiver(target));
    CSA_ASSERT(this, IsJSReceiver(source));
    CSA_ASSERT(this, IsNumberPositive(source_length));
    CSA_ASSERT(this, IsNumberPositive(start));
    CSA_ASSERT(this, IsNumber(depth));

    // 1. Let targetIndex be start.
    VARIABLE(var_target_index, MachineRepresentation::kTagged, start);

    // 2. Let sourceIndex be 0.
    VARIABLE(var_source_index, MachineRepresentation::kTagged, SmiConstant(0));

    // 3. Repeat...
    Label loop(this, {&var_target_index, &var_source_index}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* const source_index = var_source_index.value();
      Node* const target_index = var_target_index.value();

      // ...while sourceIndex < sourceLen
      GotoIfNumberGreaterThanOrEqual(source_index, source_length, &done_loop);

      // a. Let P be ! ToString(sourceIndex).
      // b. Let exists be ? HasProperty(source, P).
      CSA_ASSERT(this,
                 SmiGreaterThanOrEqual(CAST(source_index), SmiConstant(0)));
      Node* const exists =
          HasProperty(context, source, source_index, kHasProperty);

      // c. If exists is true, then
      Label next(this);
      GotoIfNot(IsTrue(exists), &next);
      {
        // i. Let element be ? Get(source, P).
        Node* element = GetProperty(context, source, source_index);

        // ii. If mapperFunction is present, then
        if (mapper_function != nullptr) {
          CSA_ASSERT(this, Word32Or(IsUndefined(mapper_function),
                                    IsCallable(mapper_function)));
          DCHECK_NOT_NULL(this_arg);

          // 1. Set element to ? Call(mapperFunction, thisArg , « element,
          //                          sourceIndex, source »).
          element =
              CallJS(CodeFactory::Call(isolate()), context, mapper_function,
                     this_arg, element, source_index, source);
        }

        // iii. Let shouldFlatten be false.
        Label if_flatten_array(this), if_flatten_proxy(this, Label::kDeferred),
            if_noflatten(this);
        // iv. If depth > 0, then
        GotoIfNumberGreaterThanOrEqual(SmiConstant(0), depth, &if_noflatten);
        // 1. Set shouldFlatten to ? IsArray(element).
        GotoIf(TaggedIsSmi(element), &if_noflatten);
        GotoIf(IsJSArray(element), &if_flatten_array);
        GotoIfNot(IsJSProxy(element), &if_noflatten);
        Branch(IsTrue(CallRuntime(Runtime::kArrayIsArray, context, element)),
               &if_flatten_proxy, &if_noflatten);

        BIND(&if_flatten_array);
        {
          CSA_ASSERT(this, IsJSArray(element));

          // 1. Let elementLen be ? ToLength(? Get(element, "length")).
          Node* const element_length =
              LoadObjectField(element, JSArray::kLengthOffset);

          // 2. Set targetIndex to ? FlattenIntoArray(target, element,
          //                                          elementLen, targetIndex,
          //                                          depth - 1).
          var_target_index.Bind(
              CallBuiltin(Builtins::kFlattenIntoArray, context, target, element,
                          element_length, target_index, NumberDec(depth)));
          Goto(&next);
        }

        BIND(&if_flatten_proxy);
        {
          CSA_ASSERT(this, IsJSProxy(element));

          // 1. Let elementLen be ? ToLength(? Get(element, "length")).
          Node* const element_length = ToLength_Inline(
              context, GetProperty(context, element, LengthStringConstant()));

          // 2. Set targetIndex to ? FlattenIntoArray(target, element,
          //                                          elementLen, targetIndex,
          //                                          depth - 1).
          var_target_index.Bind(
              CallBuiltin(Builtins::kFlattenIntoArray, context, target, element,
                          element_length, target_index, NumberDec(depth)));
          Goto(&next);
        }

        BIND(&if_noflatten);
        {
          // 1. If targetIndex >= 2^53-1, throw a TypeError exception.
          Label throw_error(this, Label::kDeferred);
          GotoIfNumberGreaterThanOrEqual(
              target_index, NumberConstant(kMaxSafeInteger), &throw_error);

          // 2. Perform ? CreateDataPropertyOrThrow(target,
          //                                        ! ToString(targetIndex),
          //                                        element).
          CallRuntime(Runtime::kCreateDataProperty, context, target,
                      target_index, element);

          // 3. Increase targetIndex by 1.
          var_target_index.Bind(NumberInc(target_index));
          Goto(&next);

          BIND(&throw_error);
          ThrowTypeError(context, MessageTemplate::kFlattenPastSafeLength,
                         source_length, target_index);
        }
      }
      BIND(&next);

      // d. Increase sourceIndex by 1.
      var_source_index.Bind(NumberInc(source_index));
      Goto(&loop);
    }

    BIND(&done_loop);
    return var_target_index.value();
  }
};

}  // namespace

// https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray
TF_BUILTIN(FlattenIntoArray, ArrayFlattenAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const target = Parameter(Descriptor::kTarget);
  Node* const source = Parameter(Descriptor::kSource);
  Node* const source_length = Parameter(Descriptor::kSourceLength);
  Node* const start = Parameter(Descriptor::kStart);
  Node* const depth = Parameter(Descriptor::kDepth);

  Return(
      FlattenIntoArray(context, target, source, source_length, start, depth));
}

// https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray
TF_BUILTIN(FlatMapIntoArray, ArrayFlattenAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const target = Parameter(Descriptor::kTarget);
  Node* const source = Parameter(Descriptor::kSource);
  Node* const source_length = Parameter(Descriptor::kSourceLength);
  Node* const start = Parameter(Descriptor::kStart);
  Node* const depth = Parameter(Descriptor::kDepth);
  Node* const mapper_function = Parameter(Descriptor::kMapperFunction);
  Node* const this_arg = Parameter(Descriptor::kThisArg);

  Return(FlattenIntoArray(context, target, source, source_length, start, depth,
                          mapper_function, this_arg));
}

// https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flat
TF_BUILTIN(ArrayPrototypeFlat, CodeStubAssembler) {
  Node* const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = args.GetReceiver();
  Node* const depth = args.GetOptionalArgumentValue(0);

  // 1. Let O be ? ToObject(this value).
  Node* const o = ToObject_Inline(CAST(context), CAST(receiver));

  // 2. Let sourceLen be ? ToLength(? Get(O, "length")).
  Node* const source_length =
      ToLength_Inline(context, GetProperty(context, o, LengthStringConstant()));

  // 3. Let depthNum be 1.
  VARIABLE(var_depth_num, MachineRepresentation::kTagged, SmiConstant(1));

  // 4. If depth is not undefined, then
  Label done(this);
  GotoIf(IsUndefined(depth), &done);
  {
    // a. Set depthNum to ? ToInteger(depth).
    var_depth_num.Bind(ToInteger_Inline(context, depth));
    Goto(&done);
  }
  BIND(&done);

  // 5. Let A be ? ArraySpeciesCreate(O, 0).
  Node* const constructor =
      CallRuntime(Runtime::kArraySpeciesConstructor, context, o);
  Node* const a = ConstructJS(CodeFactory::Construct(isolate()), context,
                              constructor, SmiConstant(0));

  // 6. Perform ? FlattenIntoArray(A, O, sourceLen, 0, depthNum).
  CallBuiltin(Builtins::kFlattenIntoArray, context, a, o, source_length,
              SmiConstant(0), var_depth_num.value());

  // 7. Return A.
  args.PopAndReturn(a);
}

// https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap
TF_BUILTIN(ArrayPrototypeFlatMap, CodeStubAssembler) {
  Node* const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = args.GetReceiver();
  Node* const mapper_function = args.GetOptionalArgumentValue(0);

  // 1. Let O be ? ToObject(this value).
  Node* const o = ToObject_Inline(CAST(context), CAST(receiver));

  // 2. Let sourceLen be ? ToLength(? Get(O, "length")).
  Node* const source_length =
      ToLength_Inline(context, GetProperty(context, o, LengthStringConstant()));

  // 3. If IsCallable(mapperFunction) is false, throw a TypeError exception.
  Label if_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(mapper_function), &if_not_callable);
  GotoIfNot(IsCallable(mapper_function), &if_not_callable);

  // 4. If thisArg is present, let T be thisArg; else let T be undefined.
  Node* const t = args.GetOptionalArgumentValue(1);

  // 5. Let A be ? ArraySpeciesCreate(O, 0).
  Node* const constructor =
      CallRuntime(Runtime::kArraySpeciesConstructor, context, o);
  Node* const a = ConstructJS(CodeFactory::Construct(isolate()), context,
                              constructor, SmiConstant(0));

  // 6. Perform ? FlattenIntoArray(A, O, sourceLen, 0, 1, mapperFunction, T).
  CallBuiltin(Builtins::kFlatMapIntoArray, context, a, o, source_length,
              SmiConstant(0), SmiConstant(1), mapper_function, t);

  // 7. Return A.
  args.PopAndReturn(a);

  BIND(&if_not_callable);
  { ThrowTypeError(context, MessageTemplate::kMapperFunctionNonCallable); }
}

TF_BUILTIN(ArrayConstructor, ArrayBuiltinsAssembler) {
  // This is a trampoline to ArrayConstructorImpl which just adds
  // allocation_site parameter value and sets new_target if necessary.
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));

  // If new_target is undefined, then this is the 'Call' case, so set new_target
  // to function.
  new_target =
      SelectConstant<Object>(IsUndefined(new_target), function, new_target);

  // Run the native code for the Array function called as a normal function.
  TNode<Object> no_allocation_site = UndefinedConstant();
  TailCallBuiltin(Builtins::kArrayConstructorImpl, context, function,
                  new_target, argc, no_allocation_site);
}

void ArrayBuiltinsAssembler::TailCallArrayConstructorStub(
    const Callable& callable, TNode<Context> context, TNode<JSFunction> target,
    TNode<HeapObject> allocation_site_or_undefined, TNode<Int32T> argc) {
  TNode<Code> code = HeapConstant(callable.code());

  // We are going to call here ArrayNoArgumentsConstructor or
  // ArraySingleArgumentsConstructor which in addition to the register arguments
  // also expect some number of arguments on the expression stack.
  // Since
  // 1) incoming JS arguments are still on the stack,
  // 2) the ArrayNoArgumentsConstructor, ArraySingleArgumentsConstructor and
  //    ArrayNArgumentsConstructor are defined so that the register arguments
  //    are passed on the same registers,
  // in order to be able to generate a tail call to those builtins we do the
  // following trick here: we tail call to the constructor builtin using
  // ArrayNArgumentsConstructorDescriptor, so the tail call instruction
  // pops the current frame but leaves all the incoming JS arguments on the
  // expression stack so that the target builtin can still find them where it
  // expects.
  TailCallStub(ArrayNArgumentsConstructorDescriptor{}, code, context, target,
               allocation_site_or_undefined, argc);
}

void ArrayBuiltinsAssembler::CreateArrayDispatchNoArgument(
    TNode<Context> context, TNode<JSFunction> target, TNode<Int32T> argc,
    AllocationSiteOverrideMode mode, TNode<AllocationSite> allocation_site) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    Callable callable = CodeFactory::ArrayNoArgumentConstructor(
        isolate(), GetInitialFastElementsKind(), mode);

    TailCallArrayConstructorStub(callable, context, target, UndefinedConstant(),
                                 argc);
  } else {
    DCHECK_EQ(mode, DONT_OVERRIDE);
    TNode<Int32T> elements_kind = LoadElementsKind(allocation_site);

    // TODO(ishell): Compute the builtin index dynamically instead of
    // iterating over all expected elements kinds.
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next(this);
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      GotoIfNot(Word32Equal(elements_kind, Int32Constant(kind)), &next);

      Callable callable =
          CodeFactory::ArrayNoArgumentConstructor(isolate(), kind, mode);

      TailCallArrayConstructorStub(callable, context, target, allocation_site,
                                   argc);

      BIND(&next);
    }

    // If we reached this point there is a problem.
    Abort(AbortReason::kUnexpectedElementsKindInArrayConstructor);
  }
}

void ArrayBuiltinsAssembler::CreateArrayDispatchSingleArgument(
    TNode<Context> context, TNode<JSFunction> target, TNode<Int32T> argc,
    AllocationSiteOverrideMode mode, TNode<AllocationSite> allocation_site) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);
    Callable callable = CodeFactory::ArraySingleArgumentConstructor(
        isolate(), holey_initial, mode);

    TailCallArrayConstructorStub(callable, context, target, UndefinedConstant(),
                                 argc);
  } else {
    DCHECK_EQ(mode, DONT_OVERRIDE);
    TNode<Smi> transition_info = LoadTransitionInfo(allocation_site);

    // Least significant bit in fast array elements kind means holeyness.
    STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
    STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(PACKED_ELEMENTS == 2);
    STATIC_ASSERT(HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == 5);

    Label normal_sequence(this);
    TVARIABLE(Int32T, var_elements_kind,
              Signed(DecodeWord32<AllocationSite::ElementsKindBits>(
                  SmiToInt32(transition_info))));
    // Is the low bit set? If so, we are holey and that is good.
    int fast_elements_kind_holey_mask =
        AllocationSite::ElementsKindBits::encode(static_cast<ElementsKind>(1));
    GotoIf(IsSetSmi(transition_info, fast_elements_kind_holey_mask),
           &normal_sequence);
    {
      // Make elements kind holey and update elements kind in the type info.
      var_elements_kind =
          Signed(Word32Or(var_elements_kind.value(), Int32Constant(1)));
      StoreObjectFieldNoWriteBarrier(
          allocation_site, AllocationSite::kTransitionInfoOrBoilerplateOffset,
          SmiOr(transition_info, SmiConstant(fast_elements_kind_holey_mask)));
      Goto(&normal_sequence);
    }
    BIND(&normal_sequence);

    // TODO(ishell): Compute the builtin index dynamically instead of
    // iterating over all expected elements kinds.
    // TODO(ishell): Given that the code above ensures that the elements kind
    // is holey we can skip checking with non-holey elements kinds.
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next(this);
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      GotoIfNot(Word32Equal(var_elements_kind.value(), Int32Constant(kind)),
                &next);

      Callable callable =
          CodeFactory::ArraySingleArgumentConstructor(isolate(), kind, mode);

      TailCallArrayConstructorStub(callable, context, target, allocation_site,
                                   argc);

      BIND(&next);
    }

    // If we reached this point there is a problem.
    Abort(AbortReason::kUnexpectedElementsKindInArrayConstructor);
  }
}

void ArrayBuiltinsAssembler::GenerateDispatchToArrayStub(
    TNode<Context> context, TNode<JSFunction> target, TNode<Int32T> argc,
    AllocationSiteOverrideMode mode, TNode<AllocationSite> allocation_site) {
  Label check_one_case(this), fallthrough(this);
  GotoIfNot(Word32Equal(argc, Int32Constant(0)), &check_one_case);
  CreateArrayDispatchNoArgument(context, target, argc, mode, allocation_site);

  BIND(&check_one_case);
  GotoIfNot(Word32Equal(argc, Int32Constant(1)), &fallthrough);
  CreateArrayDispatchSingleArgument(context, target, argc, mode,
                                    allocation_site);

  BIND(&fallthrough);
}

TF_BUILTIN(ArrayConstructorImpl, ArrayBuiltinsAssembler) {
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<HeapObject> maybe_allocation_site =
      CAST(Parameter(Descriptor::kAllocationSite));

  // Initial map for the builtin Array functions should be Map.
  CSA_ASSERT(this, IsMap(CAST(LoadObjectField(
                       target, JSFunction::kPrototypeOrInitialMapOffset))));

  // We should either have undefined or a valid AllocationSite
  CSA_ASSERT(this, Word32Or(IsUndefined(maybe_allocation_site),
                            IsAllocationSite(maybe_allocation_site)));

  // "Enter" the context of the Array function.
  TNode<Context> context =
      CAST(LoadObjectField(target, JSFunction::kContextOffset));

  Label runtime(this, Label::kDeferred);
  GotoIf(WordNotEqual(target, new_target), &runtime);

  Label no_info(this);
  // If the feedback vector is the undefined value call an array constructor
  // that doesn't use AllocationSites.
  GotoIf(IsUndefined(maybe_allocation_site), &no_info);

  GenerateDispatchToArrayStub(context, target, argc, DONT_OVERRIDE,
                              CAST(maybe_allocation_site));
  Goto(&runtime);

  BIND(&no_info);
  GenerateDispatchToArrayStub(context, target, argc, DISABLE_ALLOCATION_SITES);
  Goto(&runtime);

  BIND(&runtime);
  GenerateArrayNArgumentsConstructor(context, target, new_target, argc,
                                     maybe_allocation_site);
}

void ArrayBuiltinsAssembler::GenerateConstructor(
    Node* context, Node* array_function, Node* array_map, Node* array_size,
    Node* allocation_site, ElementsKind elements_kind,
    AllocationSiteMode mode) {
  Label ok(this);
  Label smi_size(this);
  Label small_smi_size(this);
  Label call_runtime(this, Label::kDeferred);

  Branch(TaggedIsSmi(array_size), &smi_size, &call_runtime);

  BIND(&smi_size);

  if (IsFastPackedElementsKind(elements_kind)) {
    Label abort(this, Label::kDeferred);
    Branch(SmiEqual(CAST(array_size), SmiConstant(0)), &small_smi_size, &abort);

    BIND(&abort);
    Node* reason = SmiConstant(AbortReason::kAllocatingNonEmptyPackedArray);
    TailCallRuntime(Runtime::kAbort, context, reason);
  } else {
    int element_size =
        IsDoubleElementsKind(elements_kind) ? kDoubleSize : kPointerSize;
    int max_fast_elements =
        (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - JSArray::kSize -
         AllocationMemento::kSize) /
        element_size;
    Branch(SmiAboveOrEqual(CAST(array_size), SmiConstant(max_fast_elements)),
           &call_runtime, &small_smi_size);
  }

  BIND(&small_smi_size);
  {
    Node* array = AllocateJSArray(
        elements_kind, array_map, array_size, array_size,
        mode == DONT_TRACK_ALLOCATION_SITE ? nullptr : allocation_site,
        CodeStubAssembler::SMI_PARAMETERS);
    Return(array);
  }

  BIND(&call_runtime);
  {
    TailCallRuntime(Runtime::kNewArray, context, array_function, array_size,
                    array_function, allocation_site);
  }
}

void ArrayBuiltinsAssembler::GenerateArrayNoArgumentConstructor(
    ElementsKind kind, AllocationSiteOverrideMode mode) {
  typedef ArrayNoArgumentConstructorDescriptor Descriptor;
  Node* native_context = LoadObjectField(Parameter(Descriptor::kFunction),
                                         JSFunction::kContextOffset);
  bool track_allocation_site =
      AllocationSite::ShouldTrack(kind) && mode != DISABLE_ALLOCATION_SITES;
  Node* allocation_site =
      track_allocation_site ? Parameter(Descriptor::kAllocationSite) : nullptr;
  Node* array_map = LoadJSArrayElementsMap(kind, native_context);
  Node* array = AllocateJSArray(
      kind, array_map, IntPtrConstant(JSArray::kPreallocatedArrayElements),
      SmiConstant(0), allocation_site);
  Return(array);
}

void ArrayBuiltinsAssembler::GenerateArraySingleArgumentConstructor(
    ElementsKind kind, AllocationSiteOverrideMode mode) {
  typedef ArraySingleArgumentConstructorDescriptor Descriptor;
  Node* context = Parameter(Descriptor::kContext);
  Node* function = Parameter(Descriptor::kFunction);
  Node* native_context = LoadObjectField(function, JSFunction::kContextOffset);
  Node* array_map = LoadJSArrayElementsMap(kind, native_context);

  AllocationSiteMode allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  if (mode == DONT_OVERRIDE) {
    allocation_site_mode = AllocationSite::ShouldTrack(kind)
                               ? TRACK_ALLOCATION_SITE
                               : DONT_TRACK_ALLOCATION_SITE;
  }

  Node* array_size = Parameter(Descriptor::kArraySizeSmiParameter);
  Node* allocation_site = Parameter(Descriptor::kAllocationSite);

  GenerateConstructor(context, function, array_map, array_size, allocation_site,
                      kind, allocation_site_mode);
}

void ArrayBuiltinsAssembler::GenerateArrayNArgumentsConstructor(
    TNode<Context> context, TNode<JSFunction> target, TNode<Object> new_target,
    TNode<Int32T> argc, TNode<HeapObject> maybe_allocation_site) {
  // Replace incoming JS receiver argument with the target.
  // TODO(ishell): Avoid replacing the target on the stack and just add it
  // as another additional parameter for Runtime::kNewArray.
  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  args.SetReceiver(target);

  // Adjust arguments count for the runtime call: +1 for implicit receiver
  // and +2 for new_target and maybe_allocation_site.
  argc = Int32Add(argc, Int32Constant(3));
  TailCallRuntime(Runtime::kNewArray, argc, context, new_target,
                  maybe_allocation_site);
}

TF_BUILTIN(ArrayNArgumentsConstructor, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kFunction));
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<HeapObject> maybe_allocation_site =
      CAST(Parameter(Descriptor::kAllocationSite));

  GenerateArrayNArgumentsConstructor(context, target, target, argc,
                                     maybe_allocation_site);
}

void ArrayBuiltinsAssembler::GenerateInternalArrayNoArgumentConstructor(
    ElementsKind kind) {
  typedef ArrayNoArgumentConstructorDescriptor Descriptor;
  Node* array_map = LoadObjectField(Parameter(Descriptor::kFunction),
                                    JSFunction::kPrototypeOrInitialMapOffset);
  Node* array = AllocateJSArray(
      kind, array_map, IntPtrConstant(JSArray::kPreallocatedArrayElements),
      SmiConstant(0));
  Return(array);
}

void ArrayBuiltinsAssembler::GenerateInternalArraySingleArgumentConstructor(
    ElementsKind kind) {
  typedef ArraySingleArgumentConstructorDescriptor Descriptor;
  Node* context = Parameter(Descriptor::kContext);
  Node* function = Parameter(Descriptor::kFunction);
  Node* array_map =
      LoadObjectField(function, JSFunction::kPrototypeOrInitialMapOffset);
  Node* array_size = Parameter(Descriptor::kArraySizeSmiParameter);
  Node* allocation_site = UndefinedConstant();

  GenerateConstructor(context, function, array_map, array_size, allocation_site,
                      kind, DONT_TRACK_ALLOCATION_SITE);
}

#define GENERATE_ARRAY_CTOR(name, kind_camel, kind_caps, mode_camel, \
                            mode_caps)                               \
  TF_BUILTIN(Array##name##Constructor_##kind_camel##_##mode_camel,   \
             ArrayBuiltinsAssembler) {                               \
    GenerateArray##name##Constructor(kind_caps, mode_caps);          \
  }

// The ArrayNoArgumentConstructor builtin family.
GENERATE_ARRAY_CTOR(NoArgument, PackedSmi, PACKED_SMI_ELEMENTS, DontOverride,
                    DONT_OVERRIDE);
GENERATE_ARRAY_CTOR(NoArgument, HoleySmi, HOLEY_SMI_ELEMENTS, DontOverride,
                    DONT_OVERRIDE);
GENERATE_ARRAY_CTOR(NoArgument, PackedSmi, PACKED_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(NoArgument, HoleySmi, HOLEY_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(NoArgument, Packed, PACKED_ELEMENTS, DisableAllocationSites,
                    DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(NoArgument, Holey, HOLEY_ELEMENTS, DisableAllocationSites,
                    DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(NoArgument, PackedDouble, PACKED_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(NoArgument, HoleyDouble, HOLEY_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);

// The ArraySingleArgumentConstructor builtin family.
GENERATE_ARRAY_CTOR(SingleArgument, PackedSmi, PACKED_SMI_ELEMENTS,
                    DontOverride, DONT_OVERRIDE);
GENERATE_ARRAY_CTOR(SingleArgument, HoleySmi, HOLEY_SMI_ELEMENTS, DontOverride,
                    DONT_OVERRIDE);
GENERATE_ARRAY_CTOR(SingleArgument, PackedSmi, PACKED_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(SingleArgument, HoleySmi, HOLEY_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(SingleArgument, Packed, PACKED_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(SingleArgument, Holey, HOLEY_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(SingleArgument, PackedDouble, PACKED_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);
GENERATE_ARRAY_CTOR(SingleArgument, HoleyDouble, HOLEY_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES);

#undef GENERATE_ARRAY_CTOR

#define GENERATE_INTERNAL_ARRAY_CTOR(name, kind_camel, kind_caps) \
  TF_BUILTIN(InternalArray##name##Constructor_##kind_camel,       \
             ArrayBuiltinsAssembler) {                            \
    GenerateInternalArray##name##Constructor(kind_caps);          \
  }

GENERATE_INTERNAL_ARRAY_CTOR(NoArgument, Packed, PACKED_ELEMENTS);
GENERATE_INTERNAL_ARRAY_CTOR(NoArgument, Holey, HOLEY_ELEMENTS);
GENERATE_INTERNAL_ARRAY_CTOR(SingleArgument, Packed, PACKED_ELEMENTS);
GENERATE_INTERNAL_ARRAY_CTOR(SingleArgument, Holey, HOLEY_ELEMENTS);

#undef GENERATE_INTERNAL_ARRAY_CTOR

}  // namespace internal
}  // namespace v8

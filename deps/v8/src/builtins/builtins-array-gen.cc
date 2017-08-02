// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-string-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ArrayBuiltinCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit ArrayBuiltinCodeStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state),
        k_(this, MachineRepresentation::kTagged),
        a_(this, MachineRepresentation::kTagged),
        to_(this, MachineRepresentation::kTagged, SmiConstant(0)),
        fully_spec_compliant_(this, {&k_, &a_, &to_}) {}

  typedef std::function<void(ArrayBuiltinCodeStubAssembler* masm)>
      BuiltinResultGenerator;

  typedef std::function<Node*(ArrayBuiltinCodeStubAssembler* masm,
                              Node* k_value, Node* k)>
      CallResultProcessor;

  typedef std::function<void(ArrayBuiltinCodeStubAssembler* masm)>
      PostLoopAction;

  void ForEachResultGenerator() { a_.Bind(UndefinedConstant()); }

  Node* ForEachProcessor(Node* k_value, Node* k) {
    CallJS(CodeFactory::Call(isolate()), context(), callbackfn(), this_arg(),
           k_value, k, o());
    return a();
  }

  void SomeResultGenerator() { a_.Bind(FalseConstant()); }

  Node* SomeProcessor(Node* k_value, Node* k) {
    Node* value = CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                         this_arg(), k_value, k, o());
    Label false_continue(this), return_true(this);
    BranchIfToBooleanIsTrue(value, &return_true, &false_continue);
    BIND(&return_true);
    ReturnFromBuiltin(TrueConstant());
    BIND(&false_continue);
    return a();
  }

  void EveryResultGenerator() { a_.Bind(TrueConstant()); }

  Node* EveryProcessor(Node* k_value, Node* k) {
    Node* value = CallJS(CodeFactory::Call(isolate()), context(), callbackfn(),
                         this_arg(), k_value, k, o());
    Label true_continue(this), return_false(this);
    BranchIfToBooleanIsTrue(value, &true_continue, &return_false);
    BIND(&return_false);
    ReturnFromBuiltin(FalseConstant());
    BIND(&true_continue);
    return a();
  }

  void ReduceResultGenerator() { return a_.Bind(this_arg()); }

  Node* ReduceProcessor(Node* k_value, Node* k) {
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

  void ReducePostLoopAction() {
    Label ok(this);
    GotoIf(WordNotEqual(a(), TheHoleConstant()), &ok);
    CallRuntime(Runtime::kThrowTypeError, context(),
                SmiConstant(MessageTemplate::kReduceNoInitial));
    Unreachable();
    BIND(&ok);
  }

  void FilterResultGenerator() {
    // 7. Let A be ArraySpeciesCreate(O, 0).
    Node* len = SmiConstant(0);
    ArraySpeciesCreate(len);
  }

  Node* FilterProcessor(Node* k_value, Node* k) {
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
      BranchIfFastJSArray(a(), context(), FastJSArrayAccessMode::ANY_ACCESS,
                          &fast, &runtime);

      BIND(&fast);
      {
        kind = EnsureArrayPushable(a(), &runtime);
        GotoIf(IsElementsKindGreaterThan(kind, FAST_HOLEY_SMI_ELEMENTS),
               &object_push_pre);

        BuildAppendJSArray(FAST_SMI_ELEMENTS, a(), k_value, &runtime);
        Goto(&after_work);
      }

      BIND(&object_push_pre);
      {
        Branch(IsElementsKindGreaterThan(kind, FAST_HOLEY_ELEMENTS),
               &double_push, &object_push);
      }

      BIND(&object_push);
      {
        BuildAppendJSArray(FAST_ELEMENTS, a(), k_value, &runtime);
        Goto(&after_work);
      }

      BIND(&double_push);
      {
        BuildAppendJSArray(FAST_DOUBLE_ELEMENTS, a(), k_value, &runtime);
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

  void MapResultGenerator() { ArraySpeciesCreate(len_); }

  void TypedArrayMapResultGenerator() {
    // 6. Let A be ? TypedArraySpeciesCreate(O, len).
    Node* a = TypedArraySpeciesCreateByLength(context(), o(), len_);
    // In the Spec and our current implementation, the length check is already
    // performed in TypedArraySpeciesCreate. Repeating the check here to
    // keep this invariant local.
    // TODO(tebbi): Change this to a release mode check.
    CSA_ASSERT(
        this, WordEqual(len_, LoadObjectField(a, JSTypedArray::kLengthOffset)));
    fast_typed_array_target_ = Word32Equal(LoadInstanceType(LoadElements(o_)),
                                           LoadInstanceType(LoadElements(a)));
    a_.Bind(a);
  }

  Node* SpecCompliantMapProcessor(Node* k_value, Node* k) {
    //  i. Let kValue be ? Get(O, Pk). Performed by the caller of
    //  SpecCompliantMapProcessor.
    // ii. Let mappedValue be ? Call(callbackfn, T, kValue, k, O).
    Node* mappedValue = CallJS(CodeFactory::Call(isolate()), context(),
                               callbackfn(), this_arg(), k_value, k, o());

    // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mappedValue).
    CallRuntime(Runtime::kCreateDataProperty, context(), a(), k, mappedValue);
    return a();
  }

  Node* FastMapProcessor(Node* k_value, Node* k) {
    //  i. Let kValue be ? Get(O, Pk). Performed by the caller of
    //  FastMapProcessor.
    // ii. Let mappedValue be ? Call(callbackfn, T, kValue, k, O).
    Node* mappedValue = CallJS(CodeFactory::Call(isolate()), context(),
                               callbackfn(), this_arg(), k_value, k, o());

    Label finished(this);
    Node* kind = nullptr;
    Node* elements = nullptr;

    // If a() is a JSArray, we can have a fast path.
    // mode is SMI_PARAMETERS because k has tagged representation.
    ParameterMode mode = SMI_PARAMETERS;
    Label fast(this);
    Label runtime(this);
    Label object_push_pre(this), object_push(this), double_push(this);
    BranchIfFastJSArray(a(), context(), FastJSArrayAccessMode::ANY_ACCESS,
                        &fast, &runtime);

    BIND(&fast);
    {
      kind = EnsureArrayPushable(a(), &runtime);
      elements = LoadElements(a());
      GotoIf(IsElementsKindGreaterThan(kind, FAST_HOLEY_SMI_ELEMENTS),
             &object_push_pre);
      TryStoreArrayElement(FAST_SMI_ELEMENTS, mode, &runtime, elements, k,
                           mappedValue);
      Goto(&finished);
    }

    BIND(&object_push_pre);
    {
      Branch(IsElementsKindGreaterThan(kind, FAST_HOLEY_ELEMENTS), &double_push,
             &object_push);
    }

    BIND(&object_push);
    {
      TryStoreArrayElement(FAST_ELEMENTS, mode, &runtime, elements, k,
                           mappedValue);
      Goto(&finished);
    }

    BIND(&double_push);
    {
      TryStoreArrayElement(FAST_DOUBLE_ELEMENTS, mode, &runtime, elements, k,
                           mappedValue);
      Goto(&finished);
    }

    BIND(&runtime);
    {
      // iii. Perform ? CreateDataPropertyOrThrow(A, Pk, mappedValue).
      CallRuntime(Runtime::kCreateDataProperty, context(), a(), k, mappedValue);
      Goto(&finished);
    }

    BIND(&finished);
    return a();
  }

  // See tc39.github.io/ecma262/#sec-%typedarray%.prototype.map.
  Node* TypedArrayMapProcessor(Node* k_value, Node* k) {
    // 8. c. Let mappedValue be ? Call(callbackfn, T, « kValue, k, O »).
    Node* mappedValue = CallJS(CodeFactory::Call(isolate()), context(),
                               callbackfn(), this_arg(), k_value, k, o());
    Label fast(this), slow(this), done(this), detached(this, Label::kDeferred);

    // 8. d. Perform ? Set(A, Pk, mappedValue, true).
    // Since we know that A is a TypedArray, this always ends up in
    // #sec-integer-indexed-exotic-objects-set-p-v-receiver and then
    // tc39.github.io/ecma262/#sec-integerindexedelementset .
    Branch(fast_typed_array_target_, &fast, &slow);

    BIND(&fast);
    // #sec-integerindexedelementset 3. Let numValue be ? ToNumber(value).
    Node* num_value = ToNumber(context(), mappedValue);
    // The only way how this can bailout is because of a detached buffer.
    EmitElementStore(
        a(), k, num_value, false, source_elements_kind_,
        KeyedAccessStoreMode::STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS,
        &detached);
    Goto(&done);

    BIND(&slow);
    CallRuntime(Runtime::kSetProperty, context(), a(), k, mappedValue,
                SmiConstant(STRICT));
    Goto(&done);

    BIND(&detached);
    {
      // tc39.github.io/ecma262/#sec-integerindexedelementset
      // 5. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
      CallRuntime(Runtime::kThrowTypeError, context_,
                  SmiConstant(MessageTemplate::kDetachedOperation),
                  name_string_);
      Unreachable();
    }

    BIND(&done);
    return a();
  }

  void NullPostLoopAction() {}

 protected:
  Node* context() { return context_; }
  Node* receiver() { return receiver_; }
  Node* new_target() { return new_target_; }
  Node* argc() { return argc_; }
  Node* o() { return o_; }
  Node* len() { return len_; }
  Node* callbackfn() { return callbackfn_; }
  Node* this_arg() { return this_arg_; }
  Node* k() { return k_.value(); }
  Node* a() { return a_.value(); }

  void ReturnFromBuiltin(Node* value) {
    if (argc_ == nullptr) {
      Return(value);
    } else {
      // argc_ doesn't include the receiver, so it has to be added back in
      // manually.
      PopAndReturn(IntPtrAdd(argc_, IntPtrConstant(1)), value);
    }
  }

  void InitIteratingArrayBuiltinBody(Node* context, Node* receiver,
                                     Node* callbackfn, Node* this_arg,
                                     Node* new_target, Node* argc) {
    context_ = context;
    receiver_ = receiver;
    new_target_ = new_target;
    callbackfn_ = callbackfn;
    this_arg_ = this_arg;
    argc_ = argc;
  }

  void GenerateIteratingArrayBuiltinBody(
      const char* name, const BuiltinResultGenerator& generator,
      const CallResultProcessor& processor, const PostLoopAction& action,
      const Callable& slow_case_continuation,
      ForEachDirection direction = ForEachDirection::kForward) {
    Label non_array(this), array_changes(this, {&k_, &a_, &to_});

    // TODO(danno): Seriously? Do we really need to throw the exact error
    // message on null and undefined so that the webkit tests pass?
    Label throw_null_undefined_exception(this, Label::kDeferred);
    GotoIf(WordEqual(receiver(), NullConstant()),
           &throw_null_undefined_exception);
    GotoIf(WordEqual(receiver(), UndefinedConstant()),
           &throw_null_undefined_exception);

    // By the book: taken directly from the ECMAScript 2015 specification

    // 1. Let O be ToObject(this value).
    // 2. ReturnIfAbrupt(O)
    o_ = CallStub(CodeFactory::ToObject(isolate()), context(), receiver());

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    VARIABLE(merged_length, MachineRepresentation::kTagged);
    Label has_length(this, &merged_length), not_js_array(this);
    GotoIf(DoesntHaveInstanceType(o(), JS_ARRAY_TYPE), &not_js_array);
    merged_length.Bind(LoadJSArrayLength(o()));
    Goto(&has_length);
    BIND(&not_js_array);
    Node* len_property =
        GetProperty(context(), o(), isolate()->factory()->length_string());
    merged_length.Bind(ToLength_Inline(context(), len_property));
    Goto(&has_length);
    BIND(&has_length);
    len_ = merged_length.value();

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    Label type_exception(this, Label::kDeferred);
    Label done(this);
    GotoIf(TaggedIsSmi(callbackfn()), &type_exception);
    Branch(IsCallableMap(LoadMap(callbackfn())), &done, &type_exception);

    BIND(&throw_null_undefined_exception);
    {
      CallRuntime(
          Runtime::kThrowTypeError, context(),
          SmiConstant(MessageTemplate::kCalledOnNullOrUndefined),
          HeapConstant(isolate()->factory()->NewStringFromAsciiChecked(name)));
      Unreachable();
    }

    BIND(&type_exception);
    {
      CallRuntime(Runtime::kThrowTypeError, context(),
                  SmiConstant(MessageTemplate::kCalledNonCallable),
                  callbackfn());
      Unreachable();
    }

    BIND(&done);

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    // [Already done by the arguments adapter]

    if (direction == ForEachDirection::kForward) {
      // 7. Let k be 0.
      k_.Bind(SmiConstant(0));
    } else {
      k_.Bind(NumberDec(len()));
    }

    generator(this);

    HandleFastElements(processor, action, &fully_spec_compliant_, direction);

    BIND(&fully_spec_compliant_);

    Node* result =
        CallStub(slow_case_continuation, context(), receiver(), callbackfn(),
                 this_arg(), a_.value(), o(), k_.value(), len(), to_.value());
    ReturnFromBuiltin(result);
  }

  void InitIteratingArrayBuiltinLoopContinuation(Node* context, Node* receiver,
                                                 Node* callbackfn,
                                                 Node* this_arg, Node* a,
                                                 Node* o, Node* initial_k,
                                                 Node* len, Node* to) {
    context_ = context;
    this_arg_ = this_arg;
    callbackfn_ = callbackfn;
    argc_ = nullptr;
    a_.Bind(a);
    k_.Bind(initial_k);
    o_ = o;
    len_ = len;
    to_.Bind(to);
  }

  void GenerateIteratingTypedArrayBuiltinBody(
      const char* name, const BuiltinResultGenerator& generator,
      const CallResultProcessor& processor, const PostLoopAction& action,
      ForEachDirection direction = ForEachDirection::kForward) {
    name_string_ =
        HeapConstant(isolate()->factory()->NewStringFromAsciiChecked(name));

    // ValidateTypedArray: tc39.github.io/ecma262/#sec-validatetypedarray

    Label throw_not_typed_array(this, Label::kDeferred),
        throw_detached(this, Label::kDeferred);

    GotoIf(TaggedIsSmi(receiver_), &throw_not_typed_array);
    GotoIfNot(HasInstanceType(receiver_, JS_TYPED_ARRAY_TYPE),
              &throw_not_typed_array);

    o_ = receiver_;
    Node* array_buffer = LoadObjectField(o_, JSTypedArray::kBufferOffset);
    GotoIf(IsDetachedBuffer(array_buffer), &throw_detached);

    len_ = LoadObjectField(o_, JSTypedArray::kLengthOffset);

    Label throw_not_callable(this, Label::kDeferred);
    Label distinguish_types(this);
    GotoIf(TaggedIsSmi(callbackfn_), &throw_not_callable);
    Branch(IsCallableMap(LoadMap(callbackfn_)), &distinguish_types,
           &throw_not_callable);

    BIND(&throw_not_typed_array);
    {
      CallRuntime(Runtime::kThrowTypeError, context_,
                  SmiConstant(MessageTemplate::kNotTypedArray));
      Unreachable();
    }

    BIND(&throw_detached);
    {
      CallRuntime(Runtime::kThrowTypeError, context_,
                  SmiConstant(MessageTemplate::kDetachedOperation),
                  name_string_);
      Unreachable();
    }

    BIND(&throw_not_callable);
    {
      CallRuntime(Runtime::kThrowTypeError, context_,
                  SmiConstant(MessageTemplate::kCalledNonCallable),
                  callbackfn_);
      Unreachable();
    }

    Label unexpected_instance_type(this);
    BIND(&unexpected_instance_type);
    Unreachable();

    std::vector<int32_t> instance_types = {
#define INSTANCE_TYPE(Type, type, TYPE, ctype, size) FIXED_##TYPE##_ARRAY_TYPE,
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

    if (direction == ForEachDirection::kForward) {
      k_.Bind(SmiConstant(0));
    } else {
      k_.Bind(NumberDec(len()));
    }
    Node* instance_type = LoadInstanceType(LoadElements(o_));
    Switch(instance_type, &unexpected_instance_type, instance_types.data(),
           label_ptrs.data(), labels.size());

    for (size_t i = 0; i < labels.size(); ++i) {
      BIND(&labels[i]);
      Label done(this);
      source_elements_kind_ = ElementsKindForInstanceType(
          static_cast<InstanceType>(instance_types[i]));
      generator(this);
      // TODO(tebbi): Silently cancelling the loop on buffer detachment is a
      // spec violation. Should go to &throw_detached and throw a TypeError
      // instead.
      VisitAllTypedArrayElements(array_buffer, processor, &done, direction);
      Goto(&done);
      // No exception, return success
      BIND(&done);
      action(this);
      ReturnFromBuiltin(a_.value());
    }
  }

  void GenerateIteratingArrayBuiltinLoopContinuation(
      const CallResultProcessor& processor, const PostLoopAction& action,
      ForEachDirection direction = ForEachDirection::kForward) {
    Label loop(this, {&k_, &a_, &to_});
    Label after_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      if (direction == ForEachDirection::kForward) {
        // 8. Repeat, while k < len
        GotoUnlessNumberLessThan(k(), len_, &after_loop);
      } else {
        // OR
        // 10. Repeat, while k >= 0
        GotoUnlessNumberLessThan(SmiConstant(-1), k(), &after_loop);
      }

      Label done_element(this, &to_);
      // a. Let Pk be ToString(k).
      Node* p_k = ToString(context(), k());

      // b. Let kPresent be HasProperty(O, Pk).
      // c. ReturnIfAbrupt(kPresent).
      Node* k_present = HasProperty(o(), p_k, context());

      // d. If kPresent is true, then
      GotoIf(WordNotEqual(k_present, TrueConstant()), &done_element);

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

 private:
  static ElementsKind ElementsKindForInstanceType(InstanceType type) {
    switch (type) {
#define INSTANCE_TYPE_TO_ELEMENTS_KIND(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:                                     \
    return TYPE##_ELEMENTS;

      TYPED_ARRAYS(INSTANCE_TYPE_TO_ELEMENTS_KIND)
#undef INSTANCE_TYPE_TO_ELEMENTS_KIND

      default:
        UNREACHABLE();
        return static_cast<ElementsKind>(-1);
    }
  }

  void VisitAllTypedArrayElements(Node* array_buffer,
                                  const CallResultProcessor& processor,
                                  Label* detached, ForEachDirection direction) {
    VariableList list({&a_, &k_, &to_}, zone());

    FastLoopBody body = [&](Node* index) {
      GotoIf(IsDetachedBuffer(array_buffer), detached);
      Node* elements = LoadElements(o_);
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

  void VisitAllFastElementsOneKind(ElementsKind kind,
                                   const CallResultProcessor& processor,
                                   Label* array_changed, ParameterMode mode,
                                   ForEachDirection direction) {
    Comment("begin VisitAllFastElementsOneKind");
    VARIABLE(original_map, MachineRepresentation::kTagged);
    original_map.Bind(LoadMap(o()));
    VariableList list({&original_map, &a_, &k_, &to_}, zone());
    Node* start = IntPtrOrSmiConstant(0, mode);
    Node* end = TaggedToParameter(len(), mode);
    IndexAdvanceMode advance_mode = direction == ForEachDirection::kReverse
                                        ? IndexAdvanceMode::kPre
                                        : IndexAdvanceMode::kPost;
    if (direction == ForEachDirection::kReverse) std::swap(start, end);
    BuildFastLoop(
        list, start, end,
        [=, &original_map](Node* index) {
          k_.Bind(ParameterToTagged(index, mode));
          Label one_element_done(this), hole_element(this);

          // Check if o's map has changed during the callback. If so, we have to
          // fall back to the slower spec implementation for the rest of the
          // iteration.
          Node* o_map = LoadMap(o());
          GotoIf(WordNotEqual(o_map, original_map.value()), array_changed);

          // Check if o's length has changed during the callback and if the
          // index is now out of range of the new length.
          GotoIf(SmiGreaterThanOrEqual(k_.value(), LoadJSArrayLength(o())),
                 array_changed);

          // Re-load the elements array. If may have been resized.
          Node* elements = LoadElements(o());

          // Fast case: load the element directly from the elements FixedArray
          // and call the callback if the element is not the hole.
          DCHECK(kind == FAST_ELEMENTS || kind == FAST_DOUBLE_ELEMENTS);
          int base_size = kind == FAST_ELEMENTS
                              ? FixedArray::kHeaderSize
                              : (FixedArray::kHeaderSize - kHeapObjectTag);
          Node* offset = ElementOffsetFromIndex(index, kind, mode, base_size);
          Node* value = nullptr;
          if (kind == FAST_ELEMENTS) {
            value = LoadObjectField(elements, offset);
            GotoIf(WordEqual(value, TheHoleConstant()), &hole_element);
          } else {
            Node* double_value =
                LoadDoubleWithHoleCheck(elements, offset, &hole_element);
            value = AllocateHeapNumberWithValue(double_value);
          }
          a_.Bind(processor(this, value, k()));
          Goto(&one_element_done);

          BIND(&hole_element);
          // Check if o's prototype change unexpectedly has elements after the
          // callback in the case of a hole.
          BranchIfPrototypesHaveNoElements(o_map, &one_element_done,
                                           array_changed);

          BIND(&one_element_done);
        },
        1, mode, advance_mode);
    Comment("end VisitAllFastElementsOneKind");
  }

  void HandleFastElements(const CallResultProcessor& processor,
                          const PostLoopAction& action, Label* slow,
                          ForEachDirection direction) {
    Label switch_on_elements_kind(this), fast_elements(this),
        maybe_double_elements(this), fast_double_elements(this);

    Comment("begin HandleFastElements");
    // Non-smi lengths must use the slow path.
    GotoIf(TaggedIsNotSmi(len()), slow);

    BranchIfFastJSArray(o(), context(),
                        CodeStubAssembler::FastJSArrayAccessMode::INBOUNDS_READ,
                        &switch_on_elements_kind, slow);

    BIND(&switch_on_elements_kind);
    // Select by ElementsKind
    Node* o_map = LoadMap(o());
    Node* bit_field2 = LoadMapBitField2(o_map);
    Node* kind = DecodeWord32<Map::ElementsKindBits>(bit_field2);
    Branch(IsElementsKindGreaterThan(kind, FAST_HOLEY_ELEMENTS),
           &maybe_double_elements, &fast_elements);

    ParameterMode mode = OptimalParameterMode();
    BIND(&fast_elements);
    {
      VisitAllFastElementsOneKind(FAST_ELEMENTS, processor, slow, mode,
                                  direction);

      action(this);

      // No exception, return success
      ReturnFromBuiltin(a_.value());
    }

    BIND(&maybe_double_elements);
    Branch(IsElementsKindGreaterThan(kind, FAST_HOLEY_DOUBLE_ELEMENTS), slow,
           &fast_double_elements);

    BIND(&fast_double_elements);
    {
      VisitAllFastElementsOneKind(FAST_DOUBLE_ELEMENTS, processor, slow, mode,
                                  direction);

      action(this);

      // No exception, return success
      ReturnFromBuiltin(a_.value());
    }
  }

  // Perform ArraySpeciesCreate (ES6 #sec-arrayspeciescreate).
  void ArraySpeciesCreate(Node* len) {
    Label runtime(this, Label::kDeferred), done(this);

    Node* const original_map = LoadMap(o());
    GotoIf(Word32NotEqual(LoadMapInstanceType(original_map),
                          Int32Constant(JS_ARRAY_TYPE)),
           &runtime);

    Node* const native_context = LoadNativeContext(context());
    Node* const initial_array_prototype = LoadContextElement(
        native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
    Node* proto = LoadMapPrototype(original_map);
    GotoIf(WordNotEqual(proto, initial_array_prototype), &runtime);

    Node* species_protector = SpeciesProtectorConstant();
    Node* value = LoadObjectField(species_protector, Cell::kValueOffset);
    Node* const protector_invalid = SmiConstant(Isolate::kProtectorInvalid);
    GotoIf(WordEqual(value, protector_invalid), &runtime);

    GotoIfNot(TaggedIsPositiveSmi(len), &runtime);
    GotoIf(SmiAbove(len, SmiConstant(JSArray::kInitialMaxFastElementArray)),
           &runtime);

    const ElementsKind elements_kind =
        GetHoleyElementsKind(GetInitialFastElementsKind());
    Node* array_map = LoadJSArrayElementsMap(elements_kind, native_context);
    a_.Bind(AllocateJSArray(FAST_SMI_ELEMENTS, array_map, len, len, nullptr,
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

  Node* callbackfn_ = nullptr;
  Node* o_ = nullptr;
  Node* this_arg_ = nullptr;
  Node* len_ = nullptr;
  Node* context_ = nullptr;
  Node* receiver_ = nullptr;
  Node* new_target_ = nullptr;
  Node* argc_ = nullptr;
  Node* fast_typed_array_target_ = nullptr;
  Node* name_string_ = nullptr;
  Variable k_;
  Variable a_;
  Variable to_;
  Label fully_spec_compliant_;
  ElementsKind source_elements_kind_ = ElementsKind::NO_ELEMENTS;
};

TF_BUILTIN(FastArrayPop, CodeStubAssembler) {
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  CSA_ASSERT(this, WordEqual(Parameter(BuiltinDescriptor::kNewTarget),
                             UndefinedConstant()));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  Node* receiver = args.GetReceiver();

  Label runtime(this, Label::kDeferred);
  Label fast(this);

  // Only pop in this stub if
  // 1) the array has fast elements
  // 2) the length is writable,
  // 3) the elements backing store isn't copy-on-write,
  // 4) we aren't supposed to shrink the backing store.

  // 1) Check that the array has fast elements.
  BranchIfFastJSArray(receiver, context, FastJSArrayAccessMode::INBOUNDS_READ,
                      &fast, &runtime);

  BIND(&fast);
  {
    CSA_ASSERT(this, TaggedIsPositiveSmi(
                         LoadObjectField(receiver, JSArray::kLengthOffset)));
    Node* length = LoadAndUntagObjectField(receiver, JSArray::kLengthOffset);
    Label return_undefined(this), fast_elements(this);
    GotoIf(IntPtrEqual(length, IntPtrConstant(0)), &return_undefined);

    // 2) Ensure that the length is writable.
    EnsureArrayLengthWritable(LoadMap(receiver), &runtime);

    // 3) Check that the elements backing store isn't copy-on-write.
    Node* elements = LoadElements(receiver);
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

    StoreObjectFieldNoWriteBarrier(receiver, JSArray::kLengthOffset,
                                   SmiTag(new_length));

    Node* elements_kind = LoadMapElementsKind(LoadMap(receiver));
    GotoIf(Int32LessThanOrEqual(elements_kind,
                                Int32Constant(TERMINAL_FAST_ELEMENTS_KIND)),
           &fast_elements);

    Node* value = LoadFixedDoubleArrayElement(
        elements, new_length, MachineType::Float64(), 0, INTPTR_PARAMETERS,
        &return_undefined);

    int32_t header_size = FixedDoubleArray::kHeaderSize - kHeapObjectTag;
    Node* offset = ElementOffsetFromIndex(
        new_length, FAST_HOLEY_DOUBLE_ELEMENTS, INTPTR_PARAMETERS, header_size);
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
      Node* value = LoadFixedArrayElement(elements, new_length);
      StoreFixedArrayElement(elements, new_length, TheHoleConstant());
      GotoIf(WordEqual(value, TheHoleConstant()), &return_undefined);
      args.PopAndReturn(value);
    }

    BIND(&return_undefined);
    { args.PopAndReturn(UndefinedConstant()); }
  }

  BIND(&runtime);
  {
    Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                                 MachineType::TaggedPointer());
    TailCallStub(CodeFactory::ArrayPop(isolate()), context, target,
                 UndefinedConstant(), argc);
  }
}

TF_BUILTIN(FastArrayPush, CodeStubAssembler) {
  VARIABLE(arg_index, MachineType::PointerRepresentation());
  Label default_label(this, &arg_index);
  Label smi_transition(this);
  Label object_push_pre(this);
  Label object_push(this, &arg_index);
  Label double_push(this, &arg_index);
  Label double_transition(this);
  Label runtime(this, Label::kDeferred);

  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  CSA_ASSERT(this, WordEqual(Parameter(BuiltinDescriptor::kNewTarget),
                             UndefinedConstant()));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  Node* receiver = args.GetReceiver();
  Node* kind = nullptr;

  Label fast(this);
  BranchIfFastJSArray(receiver, context, FastJSArrayAccessMode::ANY_ACCESS,
                      &fast, &runtime);

  BIND(&fast);
  {
    arg_index.Bind(IntPtrConstant(0));
    kind = EnsureArrayPushable(receiver, &runtime);
    GotoIf(IsElementsKindGreaterThan(kind, FAST_HOLEY_SMI_ELEMENTS),
           &object_push_pre);

    Node* new_length = BuildAppendJSArray(FAST_SMI_ELEMENTS, receiver, args,
                                          arg_index, &smi_transition);
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
    Node* length = LoadJSArrayLength(receiver);
    // TODO(danno): Use the KeyedStoreGeneric stub here when possible,
    // calling into the runtime to do the elements transition is overkill.
    CallRuntime(Runtime::kSetProperty, context, receiver, length, arg,
                SmiConstant(STRICT));
    Increment(arg_index);
    // The runtime SetProperty call could have converted the array to dictionary
    // mode, which must be detected to abort the fast-path.
    Node* map = LoadMap(receiver);
    Node* bit_field2 = LoadMapBitField2(map);
    Node* kind = DecodeWord32<Map::ElementsKindBits>(bit_field2);
    GotoIf(Word32Equal(kind, Int32Constant(DICTIONARY_ELEMENTS)),
           &default_label);

    GotoIfNotNumber(arg, &object_push);
    Goto(&double_push);
  }

  BIND(&object_push_pre);
  {
    Branch(IsElementsKindGreaterThan(kind, FAST_HOLEY_ELEMENTS), &double_push,
           &object_push);
  }

  BIND(&object_push);
  {
    Node* new_length = BuildAppendJSArray(FAST_ELEMENTS, receiver, args,
                                          arg_index, &default_label);
    args.PopAndReturn(new_length);
  }

  BIND(&double_push);
  {
    Node* new_length = BuildAppendJSArray(FAST_DOUBLE_ELEMENTS, receiver, args,
                                          arg_index, &double_transition);
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
    Node* length = LoadJSArrayLength(receiver);
    // TODO(danno): Use the KeyedStoreGeneric stub here when possible,
    // calling into the runtime to do the elements transition is overkill.
    CallRuntime(Runtime::kSetProperty, context, receiver, length, arg,
                SmiConstant(STRICT));
    Increment(arg_index);
    // The runtime SetProperty call could have converted the array to dictionary
    // mode, which must be detected to abort the fast-path.
    Node* map = LoadMap(receiver);
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
        [this, receiver, context](Node* arg) {
          Node* length = LoadJSArrayLength(receiver);
          CallRuntime(Runtime::kSetProperty, context, receiver, length, arg,
                      SmiConstant(STRICT));
        },
        arg_index.value());
    args.PopAndReturn(LoadJSArrayLength(receiver));
  }

  BIND(&runtime);
  {
    Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                                 MachineType::TaggedPointer());
    TailCallStub(CodeFactory::ArrayPush(isolate()), context, target,
                 UndefinedConstant(), argc);
  }
}

TF_BUILTIN(FastArrayShift, CodeStubAssembler) {
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  CSA_ASSERT(this, WordEqual(Parameter(BuiltinDescriptor::kNewTarget),
                             UndefinedConstant()));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  Node* receiver = args.GetReceiver();

  Label runtime(this, Label::kDeferred);
  Label fast(this);

  // Only shift in this stub if
  // 1) the array has fast elements
  // 2) the length is writable,
  // 3) the elements backing store isn't copy-on-write,
  // 4) we aren't supposed to shrink the backing store,
  // 5) we aren't supposed to left-trim the backing store.

  // 1) Check that the array has fast elements.
  BranchIfFastJSArray(receiver, context, FastJSArrayAccessMode::INBOUNDS_READ,
                      &fast, &runtime);

  BIND(&fast);
  {
    CSA_ASSERT(this, TaggedIsPositiveSmi(
                         LoadObjectField(receiver, JSArray::kLengthOffset)));
    Node* length = LoadAndUntagObjectField(receiver, JSArray::kLengthOffset);
    Label return_undefined(this), fast_elements_tagged(this),
        fast_elements_untagged(this);
    GotoIf(IntPtrEqual(length, IntPtrConstant(0)), &return_undefined);

    // 2) Ensure that the length is writable.
    EnsureArrayLengthWritable(LoadMap(receiver), &runtime);

    // 3) Check that the elements backing store isn't copy-on-write.
    Node* elements = LoadElements(receiver);
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

    StoreObjectFieldNoWriteBarrier(receiver, JSArray::kLengthOffset,
                                   SmiTag(new_length));

    Node* elements_kind = LoadMapElementsKind(LoadMap(receiver));
    GotoIf(Int32LessThanOrEqual(elements_kind,
                                Int32Constant(FAST_HOLEY_SMI_ELEMENTS)),
           &fast_elements_untagged);
    GotoIf(Int32LessThanOrEqual(elements_kind,
                                Int32Constant(TERMINAL_FAST_ELEMENTS_KIND)),
           &fast_elements_tagged);
    Node* value = LoadFixedDoubleArrayElement(
        elements, IntPtrConstant(0), MachineType::Float64(), 0,
        INTPTR_PARAMETERS, &return_undefined);

    int32_t header_size = FixedDoubleArray::kHeaderSize - kHeapObjectTag;
    Node* memmove =
        ExternalConstant(ExternalReference::libc_memmove_function(isolate()));
    Node* start = IntPtrAdd(
        BitcastTaggedToWord(elements),
        ElementOffsetFromIndex(IntPtrConstant(0), FAST_HOLEY_DOUBLE_ELEMENTS,
                               INTPTR_PARAMETERS, header_size));
    CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                   MachineType::Pointer(), MachineType::UintPtr(), memmove,
                   start, IntPtrAdd(start, IntPtrConstant(kDoubleSize)),
                   IntPtrMul(new_length, IntPtrConstant(kDoubleSize)));
    Node* offset = ElementOffsetFromIndex(
        new_length, FAST_HOLEY_DOUBLE_ELEMENTS, INTPTR_PARAMETERS, header_size);
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

    BIND(&fast_elements_tagged);
    {
      Node* value = LoadFixedArrayElement(elements, 0);
      BuildFastLoop(IntPtrConstant(0), new_length,
                    [&](Node* index) {
                      StoreFixedArrayElement(
                          elements, index,
                          LoadFixedArrayElement(
                              elements, IntPtrAdd(index, IntPtrConstant(1))));
                    },
                    1, ParameterMode::INTPTR_PARAMETERS,
                    IndexAdvanceMode::kPost);
      StoreFixedArrayElement(elements, new_length, TheHoleConstant());
      GotoIf(WordEqual(value, TheHoleConstant()), &return_undefined);
      args.PopAndReturn(value);
    }

    BIND(&fast_elements_untagged);
    {
      Node* value = LoadFixedArrayElement(elements, 0);
      Node* memmove =
          ExternalConstant(ExternalReference::libc_memmove_function(isolate()));
      Node* start = IntPtrAdd(
          BitcastTaggedToWord(elements),
          ElementOffsetFromIndex(IntPtrConstant(0), FAST_HOLEY_SMI_ELEMENTS,
                                 INTPTR_PARAMETERS, header_size));
      CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                     MachineType::Pointer(), MachineType::UintPtr(), memmove,
                     start, IntPtrAdd(start, IntPtrConstant(kPointerSize)),
                     IntPtrMul(new_length, IntPtrConstant(kPointerSize)));
      StoreFixedArrayElement(elements, new_length, TheHoleConstant());
      GotoIf(WordEqual(value, TheHoleConstant()), &return_undefined);
      args.PopAndReturn(value);
    }

    BIND(&return_undefined);
    { args.PopAndReturn(UndefinedConstant()); }
  }

  BIND(&runtime);
  {
    Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                                 MachineType::TaggedPointer());
    TailCallStub(CodeFactory::ArrayShift(isolate()), context, target,
                 UndefinedConstant(), argc);
  }
}

TF_BUILTIN(ArrayForEachLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::ForEachProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayForEach, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.forEach",
      &ArrayBuiltinCodeStubAssembler::ForEachResultGenerator,
      &ArrayBuiltinCodeStubAssembler::ForEachProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(),
                            Builtins::kArrayForEachLoopContinuation));
}

TF_BUILTIN(ArraySomeLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::SomeProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArraySome, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.some",
      &ArrayBuiltinCodeStubAssembler::SomeResultGenerator,
      &ArrayBuiltinCodeStubAssembler::SomeProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArraySomeLoopContinuation));
}

TF_BUILTIN(TypedArrayPrototypeSome, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.some",
      &ArrayBuiltinCodeStubAssembler::SomeResultGenerator,
      &ArrayBuiltinCodeStubAssembler::SomeProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayEveryLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::EveryProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayEvery, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.every",
      &ArrayBuiltinCodeStubAssembler::EveryResultGenerator,
      &ArrayBuiltinCodeStubAssembler::EveryProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayEveryLoopContinuation));
}

TF_BUILTIN(TypedArrayPrototypeEvery, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.every",
      &ArrayBuiltinCodeStubAssembler::EveryResultGenerator,
      &ArrayBuiltinCodeStubAssembler::EveryProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayReduceLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* accumulator = Parameter(Descriptor::kAccumulator);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, accumulator, object,
                                            initial_k, len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::ReduceProcessor,
      &ArrayBuiltinCodeStubAssembler::ReducePostLoopAction);
}

TF_BUILTIN(ArrayReduce, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.reduce",
      &ArrayBuiltinCodeStubAssembler::ReduceResultGenerator,
      &ArrayBuiltinCodeStubAssembler::ReduceProcessor,
      &ArrayBuiltinCodeStubAssembler::ReducePostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayReduceLoopContinuation));
}

TF_BUILTIN(TypedArrayPrototypeReduce, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                new_target, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.reduce",
      &ArrayBuiltinCodeStubAssembler::ReduceResultGenerator,
      &ArrayBuiltinCodeStubAssembler::ReduceProcessor,
      &ArrayBuiltinCodeStubAssembler::ReducePostLoopAction);
}

TF_BUILTIN(ArrayReduceRightLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* accumulator = Parameter(Descriptor::kAccumulator);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, accumulator, object,
                                            initial_k, len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::ReduceProcessor,
      &ArrayBuiltinCodeStubAssembler::ReducePostLoopAction,
      ForEachDirection::kReverse);
}

TF_BUILTIN(ArrayReduceRight, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.reduceRight",
      &ArrayBuiltinCodeStubAssembler::ReduceResultGenerator,
      &ArrayBuiltinCodeStubAssembler::ReduceProcessor,
      &ArrayBuiltinCodeStubAssembler::ReducePostLoopAction,
      Builtins::CallableFor(isolate(),
                            Builtins::kArrayReduceRightLoopContinuation),
      ForEachDirection::kReverse);
}

TF_BUILTIN(TypedArrayPrototypeReduceRight, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* initial_value = args.GetOptionalArgumentValue(1, TheHoleConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, initial_value,
                                new_target, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.reduceRight",
      &ArrayBuiltinCodeStubAssembler::ReduceResultGenerator,
      &ArrayBuiltinCodeStubAssembler::ReduceProcessor,
      &ArrayBuiltinCodeStubAssembler::ReducePostLoopAction,
      ForEachDirection::kReverse);
}

TF_BUILTIN(ArrayFilterLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::FilterProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayFilter, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.filter",
      &ArrayBuiltinCodeStubAssembler::FilterResultGenerator,
      &ArrayBuiltinCodeStubAssembler::FilterProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayFilterLoopContinuation));
}

TF_BUILTIN(ArrayMapLoopContinuation, ArrayBuiltinCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* callbackfn = Parameter(Descriptor::kCallbackFn);
  Node* this_arg = Parameter(Descriptor::kThisArg);
  Node* array = Parameter(Descriptor::kArray);
  Node* object = Parameter(Descriptor::kObject);
  Node* initial_k = Parameter(Descriptor::kInitialK);
  Node* len = Parameter(Descriptor::kLength);
  Node* to = Parameter(Descriptor::kTo);

  InitIteratingArrayBuiltinLoopContinuation(context, receiver, callbackfn,
                                            this_arg, array, object, initial_k,
                                            len, to);

  GenerateIteratingArrayBuiltinLoopContinuation(
      &ArrayBuiltinCodeStubAssembler::SpecCompliantMapProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayMap, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingArrayBuiltinBody(
      "Array.prototype.map", &ArrayBuiltinCodeStubAssembler::MapResultGenerator,
      &ArrayBuiltinCodeStubAssembler::FastMapProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction,
      Builtins::CallableFor(isolate(), Builtins::kArrayMapLoopContinuation));
}

TF_BUILTIN(TypedArrayPrototypeMap, ArrayBuiltinCodeStubAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* receiver = args.GetReceiver();
  Node* callbackfn = args.GetOptionalArgumentValue(0, UndefinedConstant());
  Node* this_arg = args.GetOptionalArgumentValue(1, UndefinedConstant());

  InitIteratingArrayBuiltinBody(context, receiver, callbackfn, this_arg,
                                new_target, argc);

  GenerateIteratingTypedArrayBuiltinBody(
      "%TypedArray%.prototype.map",
      &ArrayBuiltinCodeStubAssembler::TypedArrayMapResultGenerator,
      &ArrayBuiltinCodeStubAssembler::TypedArrayMapProcessor,
      &ArrayBuiltinCodeStubAssembler::NullPostLoopAction);
}

TF_BUILTIN(ArrayIsArray, CodeStubAssembler) {
  Node* object = Parameter(Descriptor::kArg);
  Node* context = Parameter(Descriptor::kContext);

  Label call_runtime(this), return_true(this), return_false(this);

  GotoIf(TaggedIsSmi(object), &return_false);
  Node* instance_type = LoadInstanceType(object);

  GotoIf(Word32Equal(instance_type, Int32Constant(JS_ARRAY_TYPE)),
         &return_true);

  // TODO(verwaest): Handle proxies in-place.
  Branch(Word32Equal(instance_type, Int32Constant(JS_PROXY_TYPE)),
         &call_runtime, &return_false);

  BIND(&return_true);
  Return(BooleanConstant(true));

  BIND(&return_false);
  Return(BooleanConstant(false));

  BIND(&call_runtime);
  Return(CallRuntime(Runtime::kArrayIsArray, context, object));
}

class ArrayIncludesIndexofAssembler : public CodeStubAssembler {
 public:
  explicit ArrayIncludesIndexofAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  enum SearchVariant { kIncludes, kIndexOf };

  void Generate(SearchVariant variant);
};

void ArrayIncludesIndexofAssembler::Generate(SearchVariant variant) {
  const int kSearchElementArg = 0;
  const int kFromIndexArg = 1;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* array = args.GetReceiver();
  Node* search_element =
      args.GetOptionalArgumentValue(kSearchElementArg, UndefinedConstant());
  Node* context = Parameter(BuiltinDescriptor::kContext);

  Node* intptr_zero = IntPtrConstant(0);

  Label init_index(this), return_found(this), return_not_found(this),
      call_runtime(this);

  // Take slow path if not a JSArray, if retrieving elements requires
  // traversing prototype, or if access checks are required.
  BranchIfFastJSArray(array, context, FastJSArrayAccessMode::INBOUNDS_READ,
                      &init_index, &call_runtime);

  BIND(&init_index);
  VARIABLE(index_var, MachineType::PointerRepresentation(), intptr_zero);

  // JSArray length is always a positive Smi for fast arrays.
  CSA_ASSERT(this, TaggedIsPositiveSmi(LoadJSArrayLength(array)));
  Node* array_length = SmiUntag(LoadJSArrayLength(array));

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
      index_var.Bind(IntPtrAdd(array_length, index_var.value()));
      // Clamp negative results at zero.
      GotoIf(IntPtrGreaterThanOrEqual(index_var.value(), intptr_zero), &done);
      index_var.Bind(intptr_zero);
      Goto(&done);
    }
    BIND(&done);
  }

  // Fail early if startIndex >= array.length.
  GotoIf(IntPtrGreaterThanOrEqual(index_var.value(), array_length),
         &return_not_found);

  Label if_smiorobjects(this), if_packed_doubles(this), if_holey_doubles(this);

  Node* elements_kind = LoadMapElementsKind(LoadMap(array));
  Node* elements = LoadElements(array);
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  GotoIf(
      Uint32LessThanOrEqual(elements_kind, Int32Constant(FAST_HOLEY_ELEMENTS)),
      &if_smiorobjects);
  GotoIf(Word32Equal(elements_kind, Int32Constant(FAST_DOUBLE_ELEMENTS)),
         &if_packed_doubles);
  GotoIf(Word32Equal(elements_kind, Int32Constant(FAST_HOLEY_DOUBLE_ELEMENTS)),
         &if_holey_doubles);
  Goto(&return_not_found);

  BIND(&if_smiorobjects);
  {
    VARIABLE(search_num, MachineRepresentation::kFloat64);
    Label ident_loop(this, &index_var), heap_num_loop(this, &search_num),
        string_loop(this), undef_loop(this, &index_var), not_smi(this),
        not_heap_num(this);

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
    Goto(&ident_loop);

    BIND(&ident_loop);
    {
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);
      Node* element_k = LoadFixedArrayElement(elements, index_var.value());
      GotoIf(WordEqual(element_k, search_element), &return_found);

      Increment(index_var);
      Goto(&ident_loop);
    }

    if (variant == kIncludes) {
      BIND(&undef_loop);

      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);
      Node* element_k = LoadFixedArrayElement(elements, index_var.value());
      GotoIf(IsUndefined(element_k), &return_found);
      GotoIf(IsTheHole(element_k), &return_found);

      Increment(index_var);
      Goto(&undef_loop);
    }

    BIND(&heap_num_loop);
    {
      Label nan_loop(this, &index_var), not_nan_loop(this, &index_var);
      Label* nan_handling =
          variant == kIncludes ? &nan_loop : &return_not_found;
      BranchIfFloat64IsNaN(search_num.value(), nan_handling, &not_nan_loop);

      BIND(&not_nan_loop);
      {
        Label continue_loop(this), not_smi(this);
        GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                  &return_not_found);
        Node* element_k = LoadFixedArrayElement(elements, index_var.value());
        GotoIfNot(TaggedIsSmi(element_k), &not_smi);
        Branch(Float64Equal(search_num.value(), SmiToFloat64(element_k)),
               &return_found, &continue_loop);

        BIND(&not_smi);
        GotoIfNot(IsHeapNumber(element_k), &continue_loop);
        Branch(Float64Equal(search_num.value(), LoadHeapNumberValue(element_k)),
               &return_found, &continue_loop);

        BIND(&continue_loop);
        Increment(index_var);
        Goto(&not_nan_loop);
      }

      // Array.p.includes uses SameValueZero comparisons, where NaN == NaN.
      if (variant == kIncludes) {
        BIND(&nan_loop);
        Label continue_loop(this);
        GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                  &return_not_found);
        Node* element_k = LoadFixedArrayElement(elements, index_var.value());
        GotoIf(TaggedIsSmi(element_k), &continue_loop);
        GotoIfNot(IsHeapNumber(element_k), &continue_loop);
        BranchIfFloat64IsNaN(LoadHeapNumberValue(element_k), &return_found,
                             &continue_loop);

        BIND(&continue_loop);
        Increment(index_var);
        Goto(&nan_loop);
      }
    }

    BIND(&string_loop);
    {
      CSA_ASSERT(this, IsString(search_element));
      Label continue_loop(this), next_iteration(this, &index_var),
          slow_compare(this), runtime(this, Label::kDeferred);
      Node* search_length = LoadStringLength(search_element);
      Goto(&next_iteration);
      BIND(&next_iteration);
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);
      Node* element_k = LoadFixedArrayElement(elements, index_var.value());
      GotoIf(TaggedIsSmi(element_k), &continue_loop);
      GotoIf(WordEqual(search_element, element_k), &return_found);
      Node* element_k_type = LoadInstanceType(element_k);
      GotoIfNot(IsStringInstanceType(element_k_type), &continue_loop);
      Branch(WordEqual(search_length, LoadStringLength(element_k)),
             &slow_compare, &continue_loop);

      BIND(&slow_compare);
      StringBuiltinsAssembler string_asm(state());
      string_asm.StringEqual_Core(context, search_element, search_type,
                                  search_length, element_k, element_k_type,
                                  &return_found, &continue_loop, &runtime);
      BIND(&runtime);
      Node* result = CallRuntime(Runtime::kStringEqual, context, search_element,
                                 element_k);
      Branch(WordEqual(BooleanConstant(true), result), &return_found,
             &continue_loop);

      BIND(&continue_loop);
      Increment(index_var);
      Goto(&next_iteration);
    }
  }

  BIND(&if_packed_doubles);
  {
    Label nan_loop(this, &index_var), not_nan_loop(this, &index_var),
        hole_loop(this, &index_var), search_notnan(this);
    VARIABLE(search_num, MachineRepresentation::kFloat64);

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
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);
      Node* element_k = LoadFixedDoubleArrayElement(elements, index_var.value(),
                                                    MachineType::Float64());
      Branch(Float64Equal(element_k, search_num.value()), &return_found,
             &continue_loop);
      BIND(&continue_loop);
      Increment(index_var);
      Goto(&not_nan_loop);
    }

    // Array.p.includes uses SameValueZero comparisons, where NaN == NaN.
    if (variant == kIncludes) {
      BIND(&nan_loop);
      Label continue_loop(this);
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);
      Node* element_k = LoadFixedDoubleArrayElement(elements, index_var.value(),
                                                    MachineType::Float64());
      BranchIfFloat64IsNaN(element_k, &return_found, &continue_loop);
      BIND(&continue_loop);
      Increment(index_var);
      Goto(&nan_loop);
    }
  }

  BIND(&if_holey_doubles);
  {
    Label nan_loop(this, &index_var), not_nan_loop(this, &index_var),
        hole_loop(this, &index_var), search_notnan(this);
    VARIABLE(search_num, MachineRepresentation::kFloat64);

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
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);

      // No need for hole checking here; the following Float64Equal will
      // return 'not equal' for holes anyway.
      Node* element_k = LoadFixedDoubleArrayElement(elements, index_var.value(),
                                                    MachineType::Float64());

      Branch(Float64Equal(element_k, search_num.value()), &return_found,
             &continue_loop);
      BIND(&continue_loop);
      Increment(index_var);
      Goto(&not_nan_loop);
    }

    // Array.p.includes uses SameValueZero comparisons, where NaN == NaN.
    if (variant == kIncludes) {
      BIND(&nan_loop);
      Label continue_loop(this);
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);

      // Load double value or continue if it's the hole NaN.
      Node* element_k = LoadFixedDoubleArrayElement(
          elements, index_var.value(), MachineType::Float64(), 0,
          INTPTR_PARAMETERS, &continue_loop);

      BranchIfFloat64IsNaN(element_k, &return_found, &continue_loop);
      BIND(&continue_loop);
      Increment(index_var);
      Goto(&nan_loop);
    }

    // Array.p.includes treats the hole as undefined.
    if (variant == kIncludes) {
      BIND(&hole_loop);
      GotoIfNot(UintPtrLessThan(index_var.value(), array_length),
                &return_not_found);

      // Check if the element is a double hole, but don't load it.
      LoadFixedDoubleArrayElement(elements, index_var.value(),
                                  MachineType::None(), 0, INTPTR_PARAMETERS,
                                  &return_found);

      Increment(index_var);
      Goto(&hole_loop);
    }
  }

  BIND(&return_found);
  args.PopAndReturn(variant == kIncludes ? TrueConstant()
                                         : SmiTag(index_var.value()));

  BIND(&return_not_found);
  args.PopAndReturn(variant == kIncludes ? FalseConstant()
                                         : NumberConstant(-1));

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

TF_BUILTIN(ArrayIncludes, ArrayIncludesIndexofAssembler) {
  Generate(kIncludes);
}

TF_BUILTIN(ArrayIndexOf, ArrayIncludesIndexofAssembler) { Generate(kIndexOf); }

class ArrayPrototypeIterationAssembler : public CodeStubAssembler {
 public:
  explicit ArrayPrototypeIterationAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_ArrayPrototypeIterationMethod(Node* context, Node* receiver,
                                              IterationKind iteration_kind) {
    VARIABLE(var_array, MachineRepresentation::kTagged);
    VARIABLE(var_map, MachineRepresentation::kTagged);
    VARIABLE(var_type, MachineRepresentation::kWord32);

    Label if_isnotobject(this, Label::kDeferred);
    Label create_array_iterator(this);

    GotoIf(TaggedIsSmi(receiver), &if_isnotobject);
    var_array.Bind(receiver);
    var_map.Bind(LoadMap(receiver));
    var_type.Bind(LoadMapInstanceType(var_map.value()));
    Branch(IsJSReceiverInstanceType(var_type.value()), &create_array_iterator,
           &if_isnotobject);

    BIND(&if_isnotobject);
    {
      Callable callable = CodeFactory::ToObject(isolate());
      Node* result = CallStub(callable, context, receiver);
      var_array.Bind(result);
      var_map.Bind(LoadMap(result));
      var_type.Bind(LoadMapInstanceType(var_map.value()));
      Goto(&create_array_iterator);
    }

    BIND(&create_array_iterator);
    Return(CreateArrayIterator(var_array.value(), var_map.value(),
                               var_type.value(), context, iteration_kind));
  }
};

TF_BUILTIN(ArrayPrototypeValues, ArrayPrototypeIterationAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_ArrayPrototypeIterationMethod(context, receiver,
                                         IterationKind::kValues);
}

TF_BUILTIN(ArrayPrototypeEntries, ArrayPrototypeIterationAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_ArrayPrototypeIterationMethod(context, receiver,
                                         IterationKind::kEntries);
}

TF_BUILTIN(ArrayPrototypeKeys, ArrayPrototypeIterationAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_ArrayPrototypeIterationMethod(context, receiver,
                                         IterationKind::kKeys);
}

TF_BUILTIN(ArrayIteratorPrototypeNext, CodeStubAssembler) {
  Handle<String> operation = factory()->NewStringFromAsciiChecked(
      "Array Iterator.prototype.next", TENURED);

  Node* context = Parameter(Descriptor::kContext);
  Node* iterator = Parameter(Descriptor::kReceiver);

  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_done, MachineRepresentation::kTagged);

  // Required, or else `throw_bad_receiver` fails a DCHECK due to these
  // variables not being bound along all paths, despite not being used.
  var_done.Bind(TrueConstant());
  var_value.Bind(UndefinedConstant());

  Label throw_bad_receiver(this, Label::kDeferred);
  Label set_done(this);
  Label allocate_key_result(this);
  Label allocate_entry_if_needed(this);
  Label allocate_iterator_result(this);
  Label generic_values(this);

  // If O does not have all of the internal slots of an Array Iterator Instance
  // (22.1.5.3), throw a TypeError exception
  GotoIf(TaggedIsSmi(iterator), &throw_bad_receiver);
  Node* instance_type = LoadInstanceType(iterator);
  GotoIf(
      Uint32LessThan(
          Int32Constant(LAST_ARRAY_ITERATOR_TYPE - FIRST_ARRAY_ITERATOR_TYPE),
          Int32Sub(instance_type, Int32Constant(FIRST_ARRAY_ITERATOR_TYPE))),
      &throw_bad_receiver);

  // Let a be O.[[IteratedObject]].
  Node* array =
      LoadObjectField(iterator, JSArrayIterator::kIteratedObjectOffset);

  // Let index be O.[[ArrayIteratorNextIndex]].
  Node* index = LoadObjectField(iterator, JSArrayIterator::kNextIndexOffset);
  Node* orig_map =
      LoadObjectField(iterator, JSArrayIterator::kIteratedObjectMapOffset);
  Node* array_map = LoadMap(array);

  Label if_isfastarray(this), if_isnotfastarray(this),
      if_isdetached(this, Label::kDeferred);

  Branch(WordEqual(orig_map, array_map), &if_isfastarray, &if_isnotfastarray);

  BIND(&if_isfastarray);
  {
    CSA_ASSERT(this, Word32Equal(LoadMapInstanceType(array_map),
                                 Int32Constant(JS_ARRAY_TYPE)));

    Node* length = LoadObjectField(array, JSArray::kLengthOffset);

    CSA_ASSERT(this, TaggedIsSmi(length));
    CSA_ASSERT(this, TaggedIsSmi(index));

    GotoIfNot(SmiBelow(index, length), &set_done);

    Node* one = SmiConstant(Smi::FromInt(1));
    StoreObjectFieldNoWriteBarrier(iterator, JSArrayIterator::kNextIndexOffset,
                                   SmiAdd(index, one));

    var_done.Bind(FalseConstant());
    Node* elements = LoadElements(array);

    static int32_t kInstanceType[] = {
        JS_FAST_ARRAY_KEY_ITERATOR_TYPE,
        JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE,
        JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE,
        JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE,
        JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE,
        JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE,
        JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE,
        JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE,
        JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE,
        JS_FAST_ARRAY_VALUE_ITERATOR_TYPE,
        JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE,
        JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE,
        JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE,
    };

    Label packed_object_values(this), holey_object_values(this),
        packed_double_values(this), holey_double_values(this);
    Label* kInstanceTypeHandlers[] = {
        &allocate_key_result,  &packed_object_values, &holey_object_values,
        &packed_object_values, &holey_object_values,  &packed_double_values,
        &holey_double_values,  &packed_object_values, &holey_object_values,
        &packed_object_values, &holey_object_values,  &packed_double_values,
        &holey_double_values};

    Switch(instance_type, &throw_bad_receiver, kInstanceType,
           kInstanceTypeHandlers, arraysize(kInstanceType));

    BIND(&packed_object_values);
    {
      var_value.Bind(LoadFixedArrayElement(elements, index, 0, SMI_PARAMETERS));
      Goto(&allocate_entry_if_needed);
    }

    BIND(&packed_double_values);
    {
      Node* value = LoadFixedDoubleArrayElement(
          elements, index, MachineType::Float64(), 0, SMI_PARAMETERS);
      var_value.Bind(AllocateHeapNumberWithValue(value));
      Goto(&allocate_entry_if_needed);
    }

    BIND(&holey_object_values);
    {
      // Check the array_protector cell, and take the slow path if it's invalid.
      Node* invalid = SmiConstant(Smi::FromInt(Isolate::kProtectorInvalid));
      Node* cell = LoadRoot(Heap::kArrayProtectorRootIndex);
      Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
      GotoIf(WordEqual(cell_value, invalid), &generic_values);

      var_value.Bind(UndefinedConstant());
      Node* value = LoadFixedArrayElement(elements, index, 0, SMI_PARAMETERS);
      GotoIf(WordEqual(value, TheHoleConstant()), &allocate_entry_if_needed);
      var_value.Bind(value);
      Goto(&allocate_entry_if_needed);
    }

    BIND(&holey_double_values);
    {
      // Check the array_protector cell, and take the slow path if it's invalid.
      Node* invalid = SmiConstant(Smi::FromInt(Isolate::kProtectorInvalid));
      Node* cell = LoadRoot(Heap::kArrayProtectorRootIndex);
      Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
      GotoIf(WordEqual(cell_value, invalid), &generic_values);

      var_value.Bind(UndefinedConstant());
      Node* value = LoadFixedDoubleArrayElement(
          elements, index, MachineType::Float64(), 0, SMI_PARAMETERS,
          &allocate_entry_if_needed);
      var_value.Bind(AllocateHeapNumberWithValue(value));
      Goto(&allocate_entry_if_needed);
    }
  }

  BIND(&if_isnotfastarray);
  {
    Label if_istypedarray(this), if_isgeneric(this);

    // If a is undefined, return CreateIterResultObject(undefined, true)
    GotoIf(WordEqual(array, UndefinedConstant()), &allocate_iterator_result);

    Node* array_type = LoadInstanceType(array);
    Branch(Word32Equal(array_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
           &if_istypedarray, &if_isgeneric);

    BIND(&if_isgeneric);
    {
      Label if_wasfastarray(this);

      Node* length = nullptr;
      {
        VARIABLE(var_length, MachineRepresentation::kTagged);
        Label if_isarray(this), if_isnotarray(this), done(this);
        Branch(Word32Equal(array_type, Int32Constant(JS_ARRAY_TYPE)),
               &if_isarray, &if_isnotarray);

        BIND(&if_isarray);
        {
          var_length.Bind(LoadObjectField(array, JSArray::kLengthOffset));

          // Invalidate protector cell if needed
          Branch(WordNotEqual(orig_map, UndefinedConstant()), &if_wasfastarray,
                 &done);

          BIND(&if_wasfastarray);
          {
            Label if_invalid(this, Label::kDeferred);
            // A fast array iterator transitioned to a slow iterator during
            // iteration. Invalidate fast_array_iteration_prtoector cell to
            // prevent potential deopt loops.
            StoreObjectFieldNoWriteBarrier(
                iterator, JSArrayIterator::kIteratedObjectMapOffset,
                UndefinedConstant());
            GotoIf(Uint32LessThanOrEqual(
                       instance_type,
                       Int32Constant(JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE)),
                   &done);

            Node* invalid =
                SmiConstant(Smi::FromInt(Isolate::kProtectorInvalid));
            Node* cell = LoadRoot(Heap::kFastArrayIterationProtectorRootIndex);
            StoreObjectFieldNoWriteBarrier(cell, Cell::kValueOffset, invalid);
            Goto(&done);
          }
        }

        BIND(&if_isnotarray);
        {
          Node* length =
              GetProperty(context, array, factory()->length_string());
          var_length.Bind(ToLength_Inline(context, length));
          Goto(&done);
        }

        BIND(&done);
        length = var_length.value();
      }

      GotoUnlessNumberLessThan(index, length, &set_done);

      StoreObjectField(iterator, JSArrayIterator::kNextIndexOffset,
                       NumberInc(index));
      var_done.Bind(FalseConstant());

      Branch(
          Uint32LessThanOrEqual(
              instance_type, Int32Constant(JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE)),
          &allocate_key_result, &generic_values);

      BIND(&generic_values);
      {
        var_value.Bind(GetProperty(context, array, index));
        Goto(&allocate_entry_if_needed);
      }
    }

    BIND(&if_istypedarray);
    {
      Node* buffer = LoadObjectField(array, JSTypedArray::kBufferOffset);
      GotoIf(IsDetachedBuffer(buffer), &if_isdetached);

      Node* length = LoadObjectField(array, JSTypedArray::kLengthOffset);

      CSA_ASSERT(this, TaggedIsSmi(length));
      CSA_ASSERT(this, TaggedIsSmi(index));

      GotoIfNot(SmiBelow(index, length), &set_done);

      Node* one = SmiConstant(1);
      StoreObjectFieldNoWriteBarrier(
          iterator, JSArrayIterator::kNextIndexOffset, SmiAdd(index, one));
      var_done.Bind(FalseConstant());

      Node* elements = LoadElements(array);
      Node* base_ptr =
          LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
      Node* external_ptr =
          LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                          MachineType::Pointer());
      Node* data_ptr = IntPtrAdd(BitcastTaggedToWord(base_ptr), external_ptr);

      static int32_t kInstanceType[] = {
          JS_TYPED_ARRAY_KEY_ITERATOR_TYPE,
          JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE,
          JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE,
          JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE,
          JS_INT8_ARRAY_VALUE_ITERATOR_TYPE,
          JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE,
          JS_INT16_ARRAY_VALUE_ITERATOR_TYPE,
          JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE,
          JS_INT32_ARRAY_VALUE_ITERATOR_TYPE,
          JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE,
          JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE,
      };

      Label uint8_values(this), int8_values(this), uint16_values(this),
          int16_values(this), uint32_values(this), int32_values(this),
          float32_values(this), float64_values(this);
      Label* kInstanceTypeHandlers[] = {
          &allocate_key_result, &uint8_values,  &uint8_values,
          &int8_values,         &uint16_values, &int16_values,
          &uint32_values,       &int32_values,  &float32_values,
          &float64_values,      &uint8_values,  &uint8_values,
          &int8_values,         &uint16_values, &int16_values,
          &uint32_values,       &int32_values,  &float32_values,
          &float64_values,
      };

      var_done.Bind(FalseConstant());
      Switch(instance_type, &throw_bad_receiver, kInstanceType,
             kInstanceTypeHandlers, arraysize(kInstanceType));

      BIND(&uint8_values);
      {
        Node* value_uint8 = LoadFixedTypedArrayElement(
            data_ptr, index, UINT8_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(SmiFromWord32(value_uint8));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&int8_values);
      {
        Node* value_int8 = LoadFixedTypedArrayElement(
            data_ptr, index, INT8_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(SmiFromWord32(value_int8));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&uint16_values);
      {
        Node* value_uint16 = LoadFixedTypedArrayElement(
            data_ptr, index, UINT16_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(SmiFromWord32(value_uint16));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&int16_values);
      {
        Node* value_int16 = LoadFixedTypedArrayElement(
            data_ptr, index, INT16_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(SmiFromWord32(value_int16));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&uint32_values);
      {
        Node* value_uint32 = LoadFixedTypedArrayElement(
            data_ptr, index, UINT32_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(ChangeUint32ToTagged(value_uint32));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&int32_values);
      {
        Node* value_int32 = LoadFixedTypedArrayElement(
            data_ptr, index, INT32_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(ChangeInt32ToTagged(value_int32));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&float32_values);
      {
        Node* value_float32 = LoadFixedTypedArrayElement(
            data_ptr, index, FLOAT32_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(
            AllocateHeapNumberWithValue(ChangeFloat32ToFloat64(value_float32)));
        Goto(&allocate_entry_if_needed);
      }
      BIND(&float64_values);
      {
        Node* value_float64 = LoadFixedTypedArrayElement(
            data_ptr, index, FLOAT64_ELEMENTS, SMI_PARAMETERS);
        var_value.Bind(AllocateHeapNumberWithValue(value_float64));
        Goto(&allocate_entry_if_needed);
      }
    }
  }

  BIND(&set_done);
  {
    StoreObjectFieldNoWriteBarrier(
        iterator, JSArrayIterator::kIteratedObjectOffset, UndefinedConstant());
    Goto(&allocate_iterator_result);
  }

  BIND(&allocate_key_result);
  {
    var_value.Bind(index);
    var_done.Bind(FalseConstant());
    Goto(&allocate_iterator_result);
  }

  BIND(&allocate_entry_if_needed);
  {
    GotoIf(Int32GreaterThan(instance_type,
                            Int32Constant(LAST_ARRAY_KEY_VALUE_ITERATOR_TYPE)),
           &allocate_iterator_result);

    Node* elements = AllocateFixedArray(FAST_ELEMENTS, IntPtrConstant(2));
    StoreFixedArrayElement(elements, 0, index, SKIP_WRITE_BARRIER);
    StoreFixedArrayElement(elements, 1, var_value.value(), SKIP_WRITE_BARRIER);

    Node* entry = Allocate(JSArray::kSize);
    Node* map = LoadContextElement(LoadNativeContext(context),
                                   Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX);

    StoreMapNoWriteBarrier(entry, map);
    StoreObjectFieldRoot(entry, JSArray::kPropertiesOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldNoWriteBarrier(entry, JSArray::kElementsOffset, elements);
    StoreObjectFieldNoWriteBarrier(entry, JSArray::kLengthOffset,
                                   SmiConstant(Smi::FromInt(2)));

    var_value.Bind(entry);
    Goto(&allocate_iterator_result);
  }

  BIND(&allocate_iterator_result);
  {
    Node* result = Allocate(JSIteratorResult::kSize);
    Node* map = LoadContextElement(LoadNativeContext(context),
                                   Context::ITERATOR_RESULT_MAP_INDEX);
    StoreMapNoWriteBarrier(result, map);
    StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset,
                                   var_value.value());
    StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kDoneOffset,
                                   var_done.value());
    Return(result);
  }

  BIND(&throw_bad_receiver);
  {
    // The {receiver} is not a valid JSArrayIterator.
    CallRuntime(Runtime::kThrowIncompatibleMethodReceiver, context,
                HeapConstant(operation), iterator);
    Unreachable();
  }

  BIND(&if_isdetached);
  {
    Node* message = SmiConstant(MessageTemplate::kDetachedOperation);
    CallRuntime(Runtime::kThrowTypeError, context, message,
                HeapConstant(operation));
    Unreachable();
  }
}

}  // namespace internal
}  // namespace v8

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
#include "src/objects/allocation-site-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/property-cell.h"
#include "torque-generated/builtins-typed-array-createtypedarray-from-dsl-gen.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;
using IteratorRecord = IteratorBuiltinsFromDSLAssembler::IteratorRecord;

ArrayBuiltinsAssembler::ArrayBuiltinsAssembler(
    compiler::CodeAssemblerState* state)
    : CodeStubAssembler(state),
      k_(this, MachineRepresentation::kTagged),
      a_(this, MachineRepresentation::kTagged),
      to_(this, MachineRepresentation::kTagged, SmiConstant(0)),
      fully_spec_compliant_(this, {&k_, &a_, &to_}) {}

  void ArrayBuiltinsAssembler::TypedArrayMapResultGenerator() {
    // 6. Let A be ? TypedArraySpeciesCreate(O, len).
    TNode<JSTypedArray> original_array = CAST(o());
    TNode<Smi> length = CAST(len_);
    const char* method_name = "%TypedArray%.prototype.map";

    TypedArrayCreatetypedarrayBuiltinsFromDSLAssembler typedarray_asm(state());
    TNode<JSTypedArray> a = typedarray_asm.TypedArraySpeciesCreateByLength(
        context(), method_name, original_array, length);
    // In the Spec and our current implementation, the length check is already
    // performed in TypedArraySpeciesCreate.
    CSA_ASSERT(this, SmiLessThanOrEqual(CAST(len_), LoadJSTypedArrayLength(a)));
    fast_typed_array_target_ =
        Word32Equal(LoadInstanceType(LoadElements(original_array)),
                    LoadInstanceType(LoadElements(a)));
    a_.Bind(a);
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
    EmitElementStore(a(), k, num_value, source_elements_kind_,
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
    TNode<WordT> byte_length = TimesTaggedSize(length);
    CSA_ASSERT(this, UintPtrLessThan(length, byte_length));

    static const int32_t fa_base_data_offset =
        FixedArray::kHeaderSize - kHeapObjectTag;
    TNode<IntPtrT> backing_store = IntPtrAdd(
        BitcastTaggedToWord(array), IntPtrConstant(fa_base_data_offset));

    // Call out to memset to perform initialization.
    TNode<ExternalReference> memset =
        ExternalConstant(ExternalReference::libc_memset_function());
    STATIC_ASSERT(kSizetSize == kIntptrSize);
    CallCFunction(memset, MachineType::Pointer(),
                  std::make_pair(MachineType::Pointer(), backing_store),
                  std::make_pair(MachineType::IntPtr(), IntPtrConstant(0)),
                  std::make_pair(MachineType::UintPtr(), byte_length));
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

    TNode<JSArrayBuffer> array_buffer =
        LoadJSArrayBufferViewBuffer(typed_array);
    ThrowIfArrayBufferIsDetached(context_, array_buffer, name_);

    len_ = LoadJSTypedArrayLength(typed_array);

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
    std::list<Label> labels;
    for (size_t i = 0; i < instance_types.size(); ++i) {
      labels.emplace_back(this);
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

    size_t i = 0;
    for (auto it = labels.begin(); it != labels.end(); ++i, ++it) {
      BIND(&*it);
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
    GotoIfNot(
        IsValidFastJSArrayCapacity(len, CodeStubAssembler::SMI_PARAMETERS),
        &runtime);

    // We need to be conservative and start with holey because the builtins
    // that create output arrays aren't guaranteed to be called for every
    // element in the input array (maybe the callback deletes an element).
    const ElementsKind elements_kind =
        GetHoleyElementsKind(GetInitialFastElementsKind());
    TNode<Context> native_context = LoadNativeContext(context());
    TNode<Map> array_map =
        LoadJSArrayElementsMap(elements_kind, native_context);
    a_.Bind(AllocateJSArray(PACKED_SMI_ELEMENTS, array_map, len, CAST(len),
                            nullptr, CodeStubAssembler::SMI_PARAMETERS,
                            kAllowLargeObjectAllocation));

    Goto(&done);

    BIND(&runtime);
    {
      // 5. Let A be ? ArraySpeciesCreate(O, len).
      TNode<JSReceiver> constructor =
          CAST(CallRuntime(Runtime::kArraySpeciesConstructor, context(), o()));
      a_.Bind(Construct(context(), constructor, len));
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
    TNode<IntPtrT> length =
        LoadAndUntagObjectField(array_receiver, JSArray::kLengthOffset);
    Label return_undefined(this), fast_elements(this);
    GotoIf(IntPtrEqual(length, IntPtrConstant(0)), &return_undefined);

    // 2) Ensure that the length is writable.
    EnsureArrayLengthWritable(LoadMap(array_receiver), &runtime);

    // 3) Check that the elements backing store isn't copy-on-write.
    TNode<FixedArrayBase> elements = LoadElements(array_receiver);
    GotoIf(WordEqual(LoadMap(elements), LoadRoot(RootIndex::kFixedCOWArrayMap)),
           &runtime);

    TNode<IntPtrT> new_length = IntPtrSub(length, IntPtrConstant(1));

    // 4) Check that we're not supposed to shrink the backing store, as
    //    implemented in elements.cc:ElementsAccessorBase::SetLengthImpl.
    TNode<IntPtrT> capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
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

    Node* value = LoadFixedDoubleArrayElement(CAST(elements), new_length,
                                              &return_undefined);

    StoreFixedDoubleArrayHole(CAST(elements), new_length);
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
    Node* kind = LoadElementsKind(array_receiver);
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
    Node* kind = LoadElementsKind(array_receiver);
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
  TNode<JSArray> array = CAST(Parameter(Descriptor::kSource));

  CSA_ASSERT(this,
             Word32Or(Word32BinaryNot(
                          IsHoleyFastElementsKind(LoadElementsKind(array))),
                      Word32BinaryNot(IsNoElementsProtectorCellInvalid())));

  ParameterMode mode = OptimalParameterMode();
  Return(CloneFastJSArray(context, array, mode));
}

// This builtin copies the backing store of fast arrays, while converting any
// holes to undefined.
// - If there are no holes in the source, its ElementsKind will be preserved. In
// that case, this builtin should perform as fast as CloneFastJSArray. (In fact,
// for fast packed arrays, the behavior is equivalent to CloneFastJSArray.)
// - If there are holes in the source, the ElementsKind of the "copy" will be
// PACKED_ELEMENTS (such that undefined can be stored).
TF_BUILTIN(CloneFastJSArrayFillingHoles, ArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSArray> array = CAST(Parameter(Descriptor::kSource));

  CSA_ASSERT(this,
             Word32Or(Word32BinaryNot(
                          IsHoleyFastElementsKind(LoadElementsKind(array))),
                      Word32BinaryNot(IsNoElementsProtectorCellInvalid())));

  ParameterMode mode = OptimalParameterMode();
  Return(CloneFastJSArray(context, array, mode, nullptr,
                          HoleConversionMode::kConvertToUndefined));
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
      array = Construct(context, CAST(receiver));
      Goto(&done);
    }

    BIND(&is_not_constructor);
    {
      Label allocate_js_array(this);

      TNode<Map> array_map = CAST(LoadContextElement(
          context, Context::JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX));

      array = AllocateJSArray(PACKED_SMI_ELEMENTS, array_map, SmiConstant(0),
                              SmiConstant(0), nullptr,
                              ParameterMode::SMI_PARAMETERS);
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
      array = Construct(context, CAST(receiver), length);
      Goto(&done);
    }

    BIND(&is_not_constructor);
    {
      array = ArrayCreate(context, length);
      Goto(&done);
    }

    BIND(&done);
    return array.value();
  }
};

// ES #sec-array.from
TF_BUILTIN(ArrayFrom, ArrayPopulatorAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  TNode<Object> items = args.GetOptionalArgumentValue(0);
  TNode<Object> receiver = args.GetReceiver();

  Label fast_iterate(this), normal_iterate(this);

  // Use fast path if:
  // * |items| is the only argument, and
  // * the receiver is the Array function.
  GotoIfNot(Word32Equal(argc, Int32Constant(1)), &normal_iterate);
  TNode<Object> array_function = LoadContextElement(
      LoadNativeContext(context), Context::ARRAY_FUNCTION_INDEX);
  Branch(WordEqual(array_function, receiver), &fast_iterate, &normal_iterate);

  BIND(&fast_iterate);
  {
    IteratorBuiltinsAssembler iterator_assembler(state());
    TVARIABLE(Object, var_fast_result);
    iterator_assembler.FastIterableToList(context, items, &var_fast_result,
                                          &normal_iterate);
    args.PopAndReturn(var_fast_result.value());
  }

  BIND(&normal_iterate);
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
    array = ConstructArrayLike(context, receiver);

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
      TNode<Object> next = iterator_assembler.IteratorStep(
          context, iterator_record, &loop_done, fast_iterator_result_map);
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
                                                  var_exception.value());
    }
  }

  BIND(&not_iterable);
  {
    // Treat array_like as an array and try to get its length.
    length = ToLength_Inline(
        context, GetProperty(context, array_like, factory()->length_string()));

    // Construct an array using the receiver as constructor with the same length
    // as the input array.
    array = ConstructArrayLike(context, receiver, length.value());

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
  SetPropertyLength(context, array.value(), length.value());
  args.PopAndReturn(array.value());
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
    Node* element_k =
        UnsafeLoadFixedArrayElement(CAST(elements), index_var.value());
    GotoIf(WordEqual(element_k, search_element), &return_found);

    Increment(&index_var);
    Goto(&ident_loop);
  }

  if (variant == kIncludes) {
    BIND(&undef_loop);

    GotoIfNot(UintPtrLessThan(index_var.value(), array_length_untagged),
              &return_not_found);
    Node* element_k =
        UnsafeLoadFixedArrayElement(CAST(elements), index_var.value());
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
          UnsafeLoadFixedArrayElement(CAST(elements), index_var.value());
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
          UnsafeLoadFixedArrayElement(CAST(elements), index_var.value());
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
    Node* element_k =
        UnsafeLoadFixedArrayElement(CAST(elements), index_var.value());
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

    Node* element_k =
        UnsafeLoadFixedArrayElement(CAST(elements), index_var.value());
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
    GotoIfForceSlowPath(&if_generic);
    var_value.Bind(LoadFixedArrayBaseElementAsTagged(
        elements, Signed(ChangeUint32ToWord(index32)), elements_kind,
        &if_generic, &if_hole));
    Goto(&allocate_entry_if_needed);

    BIND(&if_hole);
    {
      GotoIf(IsNoElementsProtectorCellInvalid(), &if_generic);
      GotoIfNot(IsPrototypeInitialArrayPrototype(context, array_map),
                &if_generic);
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

    // Check that the {array}s buffer wasn't detached.
    ThrowIfArrayBufferViewBufferIsDetached(context, CAST(array), method_name);

    // If we go outside of the {length}, we don't need to update the
    // [[ArrayIteratorNextIndex]] anymore, since a JSTypedArray's
    // length cannot change anymore, so this {iterator} will never
    // produce values again anyways.
    TNode<Smi> length = LoadJSTypedArrayLength(CAST(array));
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

// https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray
TF_BUILTIN(FlattenIntoArray, ArrayFlattenAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const target = Parameter(Descriptor::kTarget);
  Node* const source = Parameter(Descriptor::kSource);
  Node* const source_length = Parameter(Descriptor::kSourceLength);
  Node* const start = Parameter(Descriptor::kStart);
  Node* const depth = Parameter(Descriptor::kDepth);

  // FlattenIntoArray might get called recursively, check stack for overflow
  // manually as it has stub linkage.
  PerformStackCheck(CAST(context));

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
  TNode<IntPtrT> const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> const context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> const receiver = args.GetReceiver();
  TNode<Object> const depth = args.GetOptionalArgumentValue(0);

  // 1. Let O be ? ToObject(this value).
  TNode<JSReceiver> const o = ToObject_Inline(context, receiver);

  // 2. Let sourceLen be ? ToLength(? Get(O, "length")).
  TNode<Number> const source_length =
      ToLength_Inline(context, GetProperty(context, o, LengthStringConstant()));

  // 3. Let depthNum be 1.
  TVARIABLE(Number, var_depth_num, SmiConstant(1));

  // 4. If depth is not undefined, then
  Label done(this);
  GotoIf(IsUndefined(depth), &done);
  {
    // a. Set depthNum to ? ToInteger(depth).
    var_depth_num = ToInteger_Inline(context, depth);
    Goto(&done);
  }
  BIND(&done);

  // 5. Let A be ? ArraySpeciesCreate(O, 0).
  TNode<JSReceiver> const constructor =
      CAST(CallRuntime(Runtime::kArraySpeciesConstructor, context, o));
  Node* const a = Construct(context, constructor, SmiConstant(0));

  // 6. Perform ? FlattenIntoArray(A, O, sourceLen, 0, depthNum).
  CallBuiltin(Builtins::kFlattenIntoArray, context, a, o, source_length,
              SmiConstant(0), var_depth_num.value());

  // 7. Return A.
  args.PopAndReturn(a);
}

// https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap
TF_BUILTIN(ArrayPrototypeFlatMap, CodeStubAssembler) {
  TNode<IntPtrT> const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  TNode<Context> const context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> const receiver = args.GetReceiver();
  TNode<Object> const mapper_function = args.GetOptionalArgumentValue(0);

  // 1. Let O be ? ToObject(this value).
  TNode<JSReceiver> const o = ToObject_Inline(context, receiver);

  // 2. Let sourceLen be ? ToLength(? Get(O, "length")).
  TNode<Number> const source_length =
      ToLength_Inline(context, GetProperty(context, o, LengthStringConstant()));

  // 3. If IsCallable(mapperFunction) is false, throw a TypeError exception.
  Label if_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(mapper_function), &if_not_callable);
  GotoIfNot(IsCallable(CAST(mapper_function)), &if_not_callable);

  // 4. If thisArg is present, let T be thisArg; else let T be undefined.
  TNode<Object> const t = args.GetOptionalArgumentValue(1);

  // 5. Let A be ? ArraySpeciesCreate(O, 0).
  TNode<JSReceiver> const constructor =
      CAST(CallRuntime(Runtime::kArraySpeciesConstructor, context, o));
  TNode<JSReceiver> const a = Construct(context, constructor, SmiConstant(0));

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
        IsDoubleElementsKind(elements_kind) ? kDoubleSize : kTaggedSize;
    int max_fast_elements =
        (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - JSArray::kSize -
         AllocationMemento::kSize) /
        element_size;
    Branch(SmiAboveOrEqual(CAST(array_size), SmiConstant(max_fast_elements)),
           &call_runtime, &small_smi_size);
  }

  BIND(&small_smi_size);
  {
    TNode<JSArray> array = AllocateJSArray(
        elements_kind, CAST(array_map), array_size, CAST(array_size),
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
  TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);
  TNode<JSArray> array = AllocateJSArray(
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

#define GENERATE_ARRAY_CTOR(name, kind_camel, kind_caps, mode_camel, \
                            mode_caps)                               \
  TF_BUILTIN(Array##name##Constructor_##kind_camel##_##mode_camel,   \
             ArrayBuiltinsAssembler) {                               \
    GenerateArray##name##Constructor(kind_caps, mode_caps);          \
  }

// The ArrayNoArgumentConstructor builtin family.
GENERATE_ARRAY_CTOR(NoArgument, PackedSmi, PACKED_SMI_ELEMENTS, DontOverride,
                    DONT_OVERRIDE)
GENERATE_ARRAY_CTOR(NoArgument, HoleySmi, HOLEY_SMI_ELEMENTS, DontOverride,
                    DONT_OVERRIDE)
GENERATE_ARRAY_CTOR(NoArgument, PackedSmi, PACKED_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(NoArgument, HoleySmi, HOLEY_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(NoArgument, Packed, PACKED_ELEMENTS, DisableAllocationSites,
                    DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(NoArgument, Holey, HOLEY_ELEMENTS, DisableAllocationSites,
                    DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(NoArgument, PackedDouble, PACKED_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(NoArgument, HoleyDouble, HOLEY_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)

// The ArraySingleArgumentConstructor builtin family.
GENERATE_ARRAY_CTOR(SingleArgument, PackedSmi, PACKED_SMI_ELEMENTS,
                    DontOverride, DONT_OVERRIDE)
GENERATE_ARRAY_CTOR(SingleArgument, HoleySmi, HOLEY_SMI_ELEMENTS, DontOverride,
                    DONT_OVERRIDE)
GENERATE_ARRAY_CTOR(SingleArgument, PackedSmi, PACKED_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(SingleArgument, HoleySmi, HOLEY_SMI_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(SingleArgument, Packed, PACKED_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(SingleArgument, Holey, HOLEY_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(SingleArgument, PackedDouble, PACKED_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)
GENERATE_ARRAY_CTOR(SingleArgument, HoleyDouble, HOLEY_DOUBLE_ELEMENTS,
                    DisableAllocationSites, DISABLE_ALLOCATION_SITES)

#undef GENERATE_ARRAY_CTOR

TF_BUILTIN(InternalArrayNoArgumentConstructor_Packed, ArrayBuiltinsAssembler) {
  typedef ArrayNoArgumentConstructorDescriptor Descriptor;
  TNode<Map> array_map =
      CAST(LoadObjectField(Parameter(Descriptor::kFunction),
                           JSFunction::kPrototypeOrInitialMapOffset));
  TNode<JSArray> array = AllocateJSArray(
      PACKED_ELEMENTS, array_map,
      IntPtrConstant(JSArray::kPreallocatedArrayElements), SmiConstant(0));
  Return(array);
}

}  // namespace internal
}  // namespace v8

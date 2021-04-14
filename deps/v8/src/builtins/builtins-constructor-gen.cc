// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-constructor-gen.h"

#include "src/ast/ast.h"
#include "src/builtins/builtins-call-gen.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/macro-assembler.h"
#include "src/common/globals.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

void Builtins::Generate_ConstructVarargs(MacroAssembler* masm) {
  Generate_CallOrConstructVarargs(masm,
                                  BUILTIN_CODE(masm->isolate(), Construct));
}

void Builtins::Generate_ConstructForwardVarargs(MacroAssembler* masm) {
  Generate_CallOrConstructForwardVarargs(
      masm, CallOrConstructMode::kConstruct,
      BUILTIN_CODE(masm->isolate(), Construct));
}

void Builtins::Generate_ConstructFunctionForwardVarargs(MacroAssembler* masm) {
  Generate_CallOrConstructForwardVarargs(
      masm, CallOrConstructMode::kConstruct,
      BUILTIN_CODE(masm->isolate(), ConstructFunction));
}

TF_BUILTIN(Construct_Baseline, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);

  BuildConstruct(
      target, new_target, argc, [=] { return LoadContextFromBaseline(); },
      [=] { return LoadFeedbackVectorFromBaseline(); }, slot,
      UpdateFeedbackMode::kGuaranteedFeedback);
}

TF_BUILTIN(Construct_WithFeedback, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto argc = UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);

  BuildConstruct(
      target, new_target, argc, [=] { return context; },
      [=] { return feedback_vector; }, slot,
      UpdateFeedbackMode::kOptionalFeedback);
}

void CallOrConstructBuiltinsAssembler::BuildConstruct(
    TNode<Object> target, TNode<Object> new_target, TNode<Int32T> argc,
    const LazyNode<Context>& context,
    const LazyNode<HeapObject>& feedback_vector, TNode<UintPtrT> slot,
    UpdateFeedbackMode mode) {
  TVARIABLE(AllocationSite, allocation_site);
  Label if_construct_generic(this), if_construct_array(this);
  TNode<Context> eager_context = context();
  CollectConstructFeedback(eager_context, target, new_target, feedback_vector(),
                           slot, mode, &if_construct_generic,
                           &if_construct_array, &allocation_site);

  BIND(&if_construct_generic);
  TailCallBuiltin(Builtins::kConstruct, eager_context, target, new_target,
                  argc);

  BIND(&if_construct_array);
  TailCallBuiltin(Builtins::kArrayConstructorImpl, eager_context, target,
                  new_target, argc, allocation_site.value());
}

TF_BUILTIN(ConstructWithArrayLike, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto arguments_list = Parameter<Object>(Descriptor::kArgumentsList);
  auto context = Parameter<Context>(Descriptor::kContext);
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(ConstructWithArrayLike_WithFeedback,
           CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto arguments_list = Parameter<Object>(Descriptor::kArgumentsList);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);

  TVARIABLE(AllocationSite, allocation_site);
  Label if_construct_generic(this), if_construct_array(this);
  CollectConstructFeedback(context, target, new_target, feedback_vector, slot,
                           UpdateFeedbackMode::kOptionalFeedback,
                           &if_construct_generic, &if_construct_array,
                           &allocation_site);

  BIND(&if_construct_array);
  Goto(&if_construct_generic);  // Not implemented.

  BIND(&if_construct_generic);
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(ConstructWithSpread, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto spread = Parameter<Object>(Descriptor::kSpread);
  auto args_count =
      UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

TF_BUILTIN(ConstructWithSpread_Baseline, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto spread = Parameter<Object>(Descriptor::kSpread);
  auto args_count =
      UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);
  return BuildConstructWithSpread(
      target, new_target, spread, args_count,
      [=] { return LoadContextFromBaseline(); },
      [=] { return LoadFeedbackVectorFromBaseline(); }, slot,
      UpdateFeedbackMode::kGuaranteedFeedback);
}

TF_BUILTIN(ConstructWithSpread_WithFeedback, CallOrConstructBuiltinsAssembler) {
  auto target = Parameter<Object>(Descriptor::kTarget);
  auto new_target = Parameter<Object>(Descriptor::kNewTarget);
  auto spread = Parameter<Object>(Descriptor::kSpread);
  auto args_count =
      UncheckedParameter<Int32T>(Descriptor::kActualArgumentsCount);
  auto context = Parameter<Context>(Descriptor::kContext);
  auto feedback_vector = Parameter<HeapObject>(Descriptor::kFeedbackVector);
  auto slot = UncheckedParameter<UintPtrT>(Descriptor::kSlot);

  return BuildConstructWithSpread(
      target, new_target, spread, args_count, [=] { return context; },
      [=] { return feedback_vector; }, slot,
      UpdateFeedbackMode::kGuaranteedFeedback);
}

void CallOrConstructBuiltinsAssembler::BuildConstructWithSpread(
    TNode<Object> target, TNode<Object> new_target, TNode<Object> spread,
    TNode<Int32T> argc, const LazyNode<Context>& context,
    const LazyNode<HeapObject>& feedback_vector, TNode<UintPtrT> slot,
    UpdateFeedbackMode mode) {
  TVARIABLE(AllocationSite, allocation_site);
  Label if_construct_generic(this), if_construct_array(this);
  TNode<Context> eager_context = context();
  CollectConstructFeedback(eager_context, target, new_target, feedback_vector(),
                           slot, UpdateFeedbackMode::kGuaranteedFeedback,
                           &if_construct_generic, &if_construct_array,
                           &allocation_site);

  BIND(&if_construct_array);
  Goto(&if_construct_generic);  // Not implemented.

  BIND(&if_construct_generic);
  CallOrConstructWithSpread(target, new_target, spread, argc, eager_context);
}

TF_BUILTIN(FastNewClosure, ConstructorBuiltinsAssembler) {
  auto shared_function_info =
      Parameter<SharedFunctionInfo>(Descriptor::kSharedFunctionInfo);
  auto feedback_cell = Parameter<FeedbackCell>(Descriptor::kFeedbackCell);
  auto context = Parameter<Context>(Descriptor::kContext);

  IncrementCounter(isolate()->counters()->fast_new_closure_total(), 1);

  // Bump the closure counter encoded the {feedback_cell}s map.
  {
    const TNode<Map> feedback_cell_map = LoadMap(feedback_cell);
    Label no_closures(this), one_closure(this), cell_done(this);

    GotoIf(IsNoClosuresCellMap(feedback_cell_map), &no_closures);
    GotoIf(IsOneClosureCellMap(feedback_cell_map), &one_closure);
    CSA_ASSERT(this, IsManyClosuresCellMap(feedback_cell_map),
               feedback_cell_map, feedback_cell);
    Goto(&cell_done);

    BIND(&no_closures);
    StoreMapNoWriteBarrier(feedback_cell, RootIndex::kOneClosureCellMap);
    Goto(&cell_done);

    BIND(&one_closure);
    StoreMapNoWriteBarrier(feedback_cell, RootIndex::kManyClosuresCellMap);
    Goto(&cell_done);

    BIND(&cell_done);
  }

  // The calculation of |function_map_index| must be in sync with
  // SharedFunctionInfo::function_map_index().
  TNode<Uint32T> flags = LoadObjectField<Uint32T>(
      shared_function_info, SharedFunctionInfo::kFlagsOffset);
  const TNode<IntPtrT> function_map_index = Signed(IntPtrAdd(
      DecodeWordFromWord32<SharedFunctionInfo::FunctionMapIndexBits>(flags),
      IntPtrConstant(Context::FIRST_FUNCTION_MAP_INDEX)));
  CSA_ASSERT(this, UintPtrLessThanOrEqual(
                       function_map_index,
                       IntPtrConstant(Context::LAST_FUNCTION_MAP_INDEX)));

  // Get the function map in the current native context and set that
  // as the map of the allocated object.
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<Map> function_map =
      CAST(LoadContextElement(native_context, function_map_index));

  // Create a new closure from the given function info in new space
  TNode<IntPtrT> instance_size_in_bytes =
      TimesTaggedSize(LoadMapInstanceSizeInWords(function_map));
  TNode<HeapObject> result = Allocate(instance_size_in_bytes);
  StoreMapNoWriteBarrier(result, function_map);
  InitializeJSObjectBodyNoSlackTracking(result, function_map,
                                        instance_size_in_bytes,
                                        JSFunction::kSizeWithoutPrototype);

  // Initialize the rest of the function.
  StoreObjectFieldRoot(result, JSObject::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(result, JSObject::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  {
    // Set function prototype if necessary.
    Label done(this), init_prototype(this);
    Branch(IsFunctionWithPrototypeSlotMap(function_map), &init_prototype,
           &done);

    BIND(&init_prototype);
    StoreObjectFieldRoot(result, JSFunction::kPrototypeOrInitialMapOffset,
                         RootIndex::kTheHoleValue);
    Goto(&done);
    BIND(&done);
  }

  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kTaggedSize);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kFeedbackCellOffset,
                                 feedback_cell);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kSharedFunctionInfoOffset,
                                 shared_function_info);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kContextOffset, context);
  Handle<Code> lazy_builtin_handle =
      isolate()->builtins()->builtin_handle(Builtins::kCompileLazy);
  TNode<Code> lazy_builtin = HeapConstant(lazy_builtin_handle);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kCodeOffset, lazy_builtin);
  Return(result);
}

TF_BUILTIN(FastNewObject, ConstructorBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto target = Parameter<JSFunction>(Descriptor::kTarget);
  auto new_target = Parameter<JSReceiver>(Descriptor::kNewTarget);

  Label call_runtime(this);

  TNode<JSObject> result =
      FastNewObject(context, target, new_target, &call_runtime);
  Return(result);

  BIND(&call_runtime);
  TailCallRuntime(Runtime::kNewObject, context, target, new_target);
}

TNode<JSObject> ConstructorBuiltinsAssembler::FastNewObject(
    TNode<Context> context, TNode<JSFunction> target,
    TNode<JSReceiver> new_target) {
  TVARIABLE(JSObject, var_obj);
  Label call_runtime(this), end(this);

  var_obj = FastNewObject(context, target, new_target, &call_runtime);
  Goto(&end);

  BIND(&call_runtime);
  var_obj = CAST(CallRuntime(Runtime::kNewObject, context, target, new_target));
  Goto(&end);

  BIND(&end);
  return var_obj.value();
}

TNode<JSObject> ConstructorBuiltinsAssembler::FastNewObject(
    TNode<Context> context, TNode<JSFunction> target,
    TNode<JSReceiver> new_target, Label* call_runtime) {
  // Verify that the new target is a JSFunction.
  Label end(this);
  TNode<JSFunction> new_target_func =
      HeapObjectToJSFunctionWithPrototypeSlot(new_target, call_runtime);
  // Fast path.

  // Load the initial map and verify that it's in fact a map.
  TNode<Object> initial_map_or_proto =
      LoadJSFunctionPrototypeOrInitialMap(new_target_func);
  GotoIf(TaggedIsSmi(initial_map_or_proto), call_runtime);
  GotoIf(DoesntHaveInstanceType(CAST(initial_map_or_proto), MAP_TYPE),
         call_runtime);
  TNode<Map> initial_map = CAST(initial_map_or_proto);

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  TNode<Object> new_target_constructor = LoadObjectField(
      initial_map, Map::kConstructorOrBackPointerOrNativeContextOffset);
  GotoIf(TaggedNotEqual(target, new_target_constructor), call_runtime);

  TVARIABLE(HeapObject, properties);

  Label instantiate_map(this), allocate_properties(this);
  GotoIf(IsDictionaryMap(initial_map), &allocate_properties);
  {
    properties = EmptyFixedArrayConstant();
    Goto(&instantiate_map);
  }
  BIND(&allocate_properties);
  {
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      properties =
          AllocateSwissNameDictionary(SwissNameDictionary::kInitialCapacity);
    } else {
      properties = AllocateNameDictionary(NameDictionary::kInitialCapacity);
    }
    Goto(&instantiate_map);
  }

  BIND(&instantiate_map);
  return AllocateJSObjectFromMap(initial_map, properties.value(), base::nullopt,
                                 kNone, kWithSlackTracking);
}

TNode<Context> ConstructorBuiltinsAssembler::FastNewFunctionContext(
    TNode<ScopeInfo> scope_info, TNode<Uint32T> slots, TNode<Context> context,
    ScopeType scope_type) {
  TNode<IntPtrT> slots_intptr = Signed(ChangeUint32ToWord(slots));
  TNode<IntPtrT> size = ElementOffsetFromIndex(slots_intptr, PACKED_ELEMENTS,
                                               Context::kTodoHeaderSize);

  // Create a new closure from the given function info in new space
  TNode<Context> function_context =
      UncheckedCast<Context>(AllocateInNewSpace(size));

  TNode<NativeContext> native_context = LoadNativeContext(context);
  Context::Field index;
  switch (scope_type) {
    case EVAL_SCOPE:
      index = Context::EVAL_CONTEXT_MAP_INDEX;
      break;
    case FUNCTION_SCOPE:
      index = Context::FUNCTION_CONTEXT_MAP_INDEX;
      break;
    default:
      UNREACHABLE();
  }
  TNode<Map> map = CAST(LoadContextElement(native_context, index));
  // Set up the header.
  StoreMapNoWriteBarrier(function_context, map);
  TNode<IntPtrT> min_context_slots = IntPtrConstant(Context::MIN_CONTEXT_SLOTS);
  // TODO(ishell): for now, length also includes MIN_CONTEXT_SLOTS.
  TNode<IntPtrT> length = IntPtrAdd(slots_intptr, min_context_slots);
  StoreObjectFieldNoWriteBarrier(function_context, Context::kLengthOffset,
                                 SmiTag(length));
  StoreObjectFieldNoWriteBarrier(function_context, Context::kScopeInfoOffset,
                                 scope_info);
  StoreObjectFieldNoWriteBarrier(function_context, Context::kPreviousOffset,
                                 context);

  // Initialize the varrest of the slots to undefined.
  TNode<Oddball> undefined = UndefinedConstant();
  TNode<IntPtrT> start_offset = IntPtrConstant(Context::kTodoHeaderSize);
  CodeStubAssembler::VariableList vars(0, zone());
  BuildFastLoop<IntPtrT>(
      vars, start_offset, size,
      [=](TNode<IntPtrT> offset) {
        StoreObjectFieldNoWriteBarrier(function_context, offset, undefined);
      },
      kTaggedSize, IndexAdvanceMode::kPost);
  return function_context;
}

TNode<JSRegExp> ConstructorBuiltinsAssembler::CreateRegExpLiteral(
    TNode<HeapObject> maybe_feedback_vector, TNode<TaggedIndex> slot,
    TNode<Object> pattern, TNode<Smi> flags, TNode<Context> context) {
  Label call_runtime(this, Label::kDeferred), end(this);

  GotoIf(IsUndefined(maybe_feedback_vector), &call_runtime);

  TVARIABLE(JSRegExp, result);
  TNode<FeedbackVector> feedback_vector = CAST(maybe_feedback_vector);
  TNode<Object> literal_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  GotoIfNot(HasBoilerplate(literal_site), &call_runtime);
  {
    STATIC_ASSERT(JSRegExp::kDataOffset == JSObject::kHeaderSize);
    STATIC_ASSERT(JSRegExp::kSourceOffset ==
                  JSRegExp::kDataOffset + kTaggedSize);
    STATIC_ASSERT(JSRegExp::kFlagsOffset ==
                  JSRegExp::kSourceOffset + kTaggedSize);
    STATIC_ASSERT(JSRegExp::kHeaderSize ==
                  JSRegExp::kFlagsOffset + kTaggedSize);
    STATIC_ASSERT(JSRegExp::kLastIndexOffset == JSRegExp::kHeaderSize);
    DCHECK_EQ(JSRegExp::Size(), JSRegExp::kLastIndexOffset + kTaggedSize);

    TNode<RegExpBoilerplateDescription> boilerplate = CAST(literal_site);
    TNode<HeapObject> new_object = Allocate(JSRegExp::Size());

    // Initialize Object fields.
    TNode<JSFunction> regexp_function = CAST(LoadContextElement(
        LoadNativeContext(context), Context::REGEXP_FUNCTION_INDEX));
    TNode<Map> initial_map = CAST(LoadObjectField(
        regexp_function, JSFunction::kPrototypeOrInitialMapOffset));
    StoreMapNoWriteBarrier(new_object, initial_map);
    // Initialize JSReceiver fields.
    StoreObjectFieldRoot(new_object, JSReceiver::kPropertiesOrHashOffset,
                         RootIndex::kEmptyFixedArray);
    // Initialize JSObject fields.
    StoreObjectFieldRoot(new_object, JSObject::kElementsOffset,
                         RootIndex::kEmptyFixedArray);
    // Initialize JSRegExp fields.
    StoreObjectFieldNoWriteBarrier(
        new_object, JSRegExp::kDataOffset,
        LoadObjectField(boilerplate,
                        RegExpBoilerplateDescription::kDataOffset));
    StoreObjectFieldNoWriteBarrier(
        new_object, JSRegExp::kSourceOffset,
        LoadObjectField(boilerplate,
                        RegExpBoilerplateDescription::kSourceOffset));
    StoreObjectFieldNoWriteBarrier(
        new_object, JSRegExp::kFlagsOffset,
        LoadObjectField(boilerplate,
                        RegExpBoilerplateDescription::kFlagsOffset));
    StoreObjectFieldNoWriteBarrier(
        new_object, JSRegExp::kLastIndexOffset,
        SmiConstant(JSRegExp::kInitialLastIndexValue));

    result = CAST(new_object);
    Goto(&end);
  }

  BIND(&call_runtime);
  {
    result = CAST(CallRuntime(Runtime::kCreateRegExpLiteral, context,
                              maybe_feedback_vector, slot, pattern, flags));
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

TNode<JSArray> ConstructorBuiltinsAssembler::CreateShallowArrayLiteral(
    TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
    TNode<Context> context, AllocationSiteMode allocation_site_mode,
    Label* call_runtime) {
  Label zero_capacity(this), cow_elements(this), fast_elements(this),
      return_result(this);

  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  GotoIfNot(HasBoilerplate(maybe_allocation_site), call_runtime);

  TNode<AllocationSite> allocation_site = CAST(maybe_allocation_site);
  TNode<JSArray> boilerplate = CAST(LoadBoilerplate(allocation_site));

  if (allocation_site_mode == TRACK_ALLOCATION_SITE) {
    return CloneFastJSArray(context, boilerplate, allocation_site);
  } else {
    return CloneFastJSArray(context, boilerplate);
  }
}

TNode<JSArray> ConstructorBuiltinsAssembler::CreateEmptyArrayLiteral(
    TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
    TNode<Context> context) {
  // Array literals always have a valid AllocationSite to properly track
  // elements transitions.
  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  TVARIABLE(AllocationSite, allocation_site);

  Label create_empty_array(this),
      initialize_allocation_site(this, Label::kDeferred), done(this);
  GotoIf(TaggedIsSmi(maybe_allocation_site), &initialize_allocation_site);
  {
    allocation_site = CAST(maybe_allocation_site);
    Goto(&create_empty_array);
  }
  // TODO(cbruni): create the AllocationSite in CSA.
  BIND(&initialize_allocation_site);
  {
    allocation_site = CreateAllocationSiteInFeedbackVector(
        feedback_vector,
        // TODO(v8:10047): pass slot as TaggedIndex here
        Unsigned(TaggedIndexToIntPtr(slot)));
    Goto(&create_empty_array);
  }

  BIND(&create_empty_array);
  TNode<Int32T> kind = LoadElementsKind(allocation_site.value());
  TNode<NativeContext> native_context = LoadNativeContext(context);
  Comment("LoadJSArrayElementsMap");
  TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);
  TNode<IntPtrT> zero_intptr = IntPtrConstant(0);
  TNode<Smi> zero = SmiConstant(0);
  Comment("Allocate JSArray");
  TNode<JSArray> result =
      AllocateJSArray(GetInitialFastElementsKind(), array_map, zero_intptr,
                      zero, allocation_site.value());

  Goto(&done);
  BIND(&done);

  return result;
}

TNode<HeapObject> ConstructorBuiltinsAssembler::CreateShallowObjectLiteral(
    TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
    Label* call_runtime) {
  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  GotoIfNot(HasBoilerplate(maybe_allocation_site), call_runtime);

  TNode<AllocationSite> allocation_site = CAST(maybe_allocation_site);
  TNode<JSObject> boilerplate = LoadBoilerplate(allocation_site);
  TNode<Map> boilerplate_map = LoadMap(boilerplate);
  CSA_ASSERT(this, IsJSObjectMap(boilerplate_map));

  TVARIABLE(HeapObject, var_properties);
  {
    TNode<Uint32T> bit_field_3 = LoadMapBitField3(boilerplate_map);
    GotoIf(IsSetWord32<Map::Bits3::IsDeprecatedBit>(bit_field_3), call_runtime);
    // Directly copy over the property store for dict-mode boilerplates.
    Label if_dictionary(this), if_fast(this), done(this);
    Branch(IsSetWord32<Map::Bits3::IsDictionaryMapBit>(bit_field_3),
           &if_dictionary, &if_fast);
    BIND(&if_dictionary);
    {
      Comment("Copy dictionary properties");
      if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
        var_properties =
            CopySwissNameDictionary(CAST(LoadSlowProperties(boilerplate)));
      } else {
        var_properties = CopyNameDictionary(
            CAST(LoadSlowProperties(boilerplate)), call_runtime);
      }
      // Slow objects have no in-object properties.
      Goto(&done);
    }
    BIND(&if_fast);
    {
      // TODO(cbruni): support copying out-of-object properties.
      TNode<HeapObject> boilerplate_properties =
          LoadFastProperties(boilerplate);
      GotoIfNot(IsEmptyFixedArray(boilerplate_properties), call_runtime);
      var_properties = EmptyFixedArrayConstant();
      Goto(&done);
    }
    BIND(&done);
  }

  TVARIABLE(FixedArrayBase, var_elements);
  {
    // Copy the elements backing store, assuming that it's flat.
    Label if_empty_fixed_array(this), if_copy_elements(this), done(this);
    TNode<FixedArrayBase> boilerplate_elements = LoadElements(boilerplate);
    Branch(IsEmptyFixedArray(boilerplate_elements), &if_empty_fixed_array,
           &if_copy_elements);

    BIND(&if_empty_fixed_array);
    var_elements = boilerplate_elements;
    Goto(&done);

    BIND(&if_copy_elements);
    CSA_ASSERT(this, Word32BinaryNot(
                         IsFixedCOWArrayMap(LoadMap(boilerplate_elements))));
    ExtractFixedArrayFlags flags;
    flags |= ExtractFixedArrayFlag::kAllFixedArrays;
    flags |= ExtractFixedArrayFlag::kNewSpaceAllocationOnly;
    flags |= ExtractFixedArrayFlag::kDontCopyCOW;
    var_elements = CloneFixedArray(boilerplate_elements, flags);
    Goto(&done);
    BIND(&done);
  }

  // Ensure new-space allocation for a fresh JSObject so we can skip write
  // barriers when copying all object fields.
  STATIC_ASSERT(JSObject::kMaxInstanceSize < kMaxRegularHeapObjectSize);
  TNode<IntPtrT> instance_size =
      TimesTaggedSize(LoadMapInstanceSizeInWords(boilerplate_map));
  TNode<IntPtrT> allocation_size = instance_size;
  bool needs_allocation_memento = FLAG_allocation_site_pretenuring;
  if (needs_allocation_memento) {
    // Prepare for inner-allocating the AllocationMemento.
    allocation_size =
        IntPtrAdd(instance_size, IntPtrConstant(AllocationMemento::kSize));
  }

  TNode<HeapObject> copy =
      UncheckedCast<HeapObject>(AllocateInNewSpace(allocation_size));
  {
    Comment("Initialize Literal Copy");
    // Initialize Object fields.
    StoreMapNoWriteBarrier(copy, boilerplate_map);
    StoreObjectFieldNoWriteBarrier(copy, JSObject::kPropertiesOrHashOffset,
                                   var_properties.value());
    StoreObjectFieldNoWriteBarrier(copy, JSObject::kElementsOffset,
                                   var_elements.value());
  }

  // Initialize the AllocationMemento before potential GCs due to heap number
  // allocation when copying the in-object properties.
  if (needs_allocation_memento) {
    InitializeAllocationMemento(copy, instance_size, allocation_site);
  }

  {
    // Copy over in-object properties.
    Label continue_with_write_barrier(this), done_init(this);
    TVARIABLE(IntPtrT, offset, IntPtrConstant(JSObject::kHeaderSize));
    {
      Comment("Copy in-object properties fast");
      Label continue_fast(this, &offset);
      Branch(IntPtrEqual(offset.value(), instance_size), &done_init,
             &continue_fast);
      BIND(&continue_fast);
      TNode<Object> field = LoadObjectField(boilerplate, offset.value());
      Label store_field(this);
      GotoIf(TaggedIsSmi(field), &store_field);
      // TODO(leszeks): Read the field descriptor to decide if this heap
      // number is mutable or not.
      GotoIf(IsHeapNumber(CAST(field)), &continue_with_write_barrier);
      Goto(&store_field);
      BIND(&store_field);
      StoreObjectFieldNoWriteBarrier(copy, offset.value(), field);
      offset = IntPtrAdd(offset.value(), IntPtrConstant(kTaggedSize));
      Branch(WordNotEqual(offset.value(), instance_size), &continue_fast,
             &done_init);
    }

    // Continue initializing the literal after seeing the first sub-object
    // potentially causing allocation. In this case we prepare the new literal
    // by copying all pending fields over from the boilerplate and emit full
    // write barriers from here on.
    BIND(&continue_with_write_barrier);
    {
      Comment("Copy in-object properties slow");
      BuildFastLoop<IntPtrT>(
          offset.value(), instance_size,
          [=](TNode<IntPtrT> offset) {
            // TODO(ishell): value decompression is not necessary here.
            TNode<Object> field = LoadObjectField(boilerplate, offset);
            StoreObjectFieldNoWriteBarrier(copy, offset, field);
          },
          kTaggedSize, IndexAdvanceMode::kPost);
      CopyMutableHeapNumbersInObject(copy, offset.value(), instance_size);
      Goto(&done_init);
    }
    BIND(&done_init);
  }
  return copy;
}

// Used by the CreateEmptyObjectLiteral bytecode and the Object constructor.
TNode<JSObject> ConstructorBuiltinsAssembler::CreateEmptyObjectLiteral(
    TNode<Context> context) {
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> map = LoadObjectFunctionInitialMap(native_context);
  // Ensure that slack tracking is disabled for the map.
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  CSA_ASSERT(this, IsClearWord32<Map::Bits3::ConstructionCounterBits>(
                       LoadMapBitField3(map)));
  TNode<FixedArray> empty_fixed_array = EmptyFixedArrayConstant();
  TNode<JSObject> result =
      AllocateJSObjectFromMap(map, empty_fixed_array, empty_fixed_array);
  return result;
}

void ConstructorBuiltinsAssembler::CopyMutableHeapNumbersInObject(
    TNode<HeapObject> copy, TNode<IntPtrT> start_offset,
    TNode<IntPtrT> end_offset) {
  // Iterate over all object properties of a freshly copied object and
  // duplicate mutable heap numbers.
  Comment("Copy mutable HeapNumber values");
  BuildFastLoop<IntPtrT>(
      start_offset, end_offset,
      [=](TNode<IntPtrT> offset) {
        TNode<Object> field = LoadObjectField(copy, offset);
        Label copy_heap_number(this, Label::kDeferred), continue_loop(this);
        // We only have to clone complex field values.
        GotoIf(TaggedIsSmi(field), &continue_loop);
        // TODO(leszeks): Read the field descriptor to decide if this heap
        // number is mutable or not.
        Branch(IsHeapNumber(CAST(field)), &copy_heap_number, &continue_loop);
        BIND(&copy_heap_number);
        {
          TNode<Float64T> double_value = LoadHeapNumberValue(CAST(field));
          TNode<HeapNumber> heap_number =
              AllocateHeapNumberWithValue(double_value);
          StoreObjectField(copy, offset, heap_number);
          Goto(&continue_loop);
        }
        BIND(&continue_loop);
      },
      kTaggedSize, IndexAdvanceMode::kPost);
}

}  // namespace internal
}  // namespace v8

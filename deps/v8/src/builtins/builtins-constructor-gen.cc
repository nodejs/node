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

TF_BUILTIN(ConstructWithArrayLike, CallOrConstructBuiltinsAssembler) {
  TNode<Object> target = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Object> arguments_list = CAST(Parameter(Descriptor::kArgumentsList));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(ConstructWithSpread, CallOrConstructBuiltinsAssembler) {
  TNode<Object> target = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Object> spread = CAST(Parameter(Descriptor::kSpread));
  TNode<Int32T> args_count =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

using Node = compiler::Node;

TF_BUILTIN(FastNewClosure, ConstructorBuiltinsAssembler) {
  TNode<SharedFunctionInfo> shared_function_info =
      CAST(Parameter(Descriptor::kSharedFunctionInfo));
  TNode<FeedbackCell> feedback_cell =
      CAST(Parameter(Descriptor::kFeedbackCell));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

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
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kTarget));
  TNode<JSReceiver> new_target = CAST(Parameter(Descriptor::kNewTarget));

  Label call_runtime(this);

  TNode<JSObject> result =
      EmitFastNewObject(context, target, new_target, &call_runtime);
  Return(result);

  BIND(&call_runtime);
  TailCallRuntime(Runtime::kNewObject, context, target, new_target);
}

TNode<JSObject> ConstructorBuiltinsAssembler::EmitFastNewObject(
    TNode<Context> context, TNode<JSFunction> target,
    TNode<JSReceiver> new_target) {
  TVARIABLE(JSObject, var_obj);
  Label call_runtime(this), end(this);

  var_obj = EmitFastNewObject(context, target, new_target, &call_runtime);
  Goto(&end);

  BIND(&call_runtime);
  var_obj = CAST(CallRuntime(Runtime::kNewObject, context, target, new_target));
  Goto(&end);

  BIND(&end);
  return var_obj.value();
}

TNode<JSObject> ConstructorBuiltinsAssembler::EmitFastNewObject(
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
    properties = AllocateNameDictionary(NameDictionary::kInitialCapacity);
    Goto(&instantiate_map);
  }

  BIND(&instantiate_map);
  return AllocateJSObjectFromMap(initial_map, properties.value(), base::nullopt,
                                 kNone, kWithSlackTracking);
}

TNode<Context> ConstructorBuiltinsAssembler::EmitFastNewFunctionContext(
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

TF_BUILTIN(FastNewFunctionContextEval, ConstructorBuiltinsAssembler) {
  TNode<ScopeInfo> scope_info = CAST(Parameter(Descriptor::kScopeInfo));
  TNode<Uint32T> slots = UncheckedCast<Uint32T>(Parameter(Descriptor::kSlots));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Return(EmitFastNewFunctionContext(scope_info, slots, context,
                                    ScopeType::EVAL_SCOPE));
}

TF_BUILTIN(FastNewFunctionContextFunction, ConstructorBuiltinsAssembler) {
  TNode<ScopeInfo> scope_info = CAST(Parameter(Descriptor::kScopeInfo));
  TNode<Uint32T> slots = UncheckedCast<Uint32T>(Parameter(Descriptor::kSlots));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Return(EmitFastNewFunctionContext(scope_info, slots, context,
                                    ScopeType::FUNCTION_SCOPE));
}

TNode<JSRegExp> ConstructorBuiltinsAssembler::EmitCreateRegExpLiteral(
    TNode<HeapObject> maybe_feedback_vector, TNode<TaggedIndex> slot,
    TNode<Object> pattern, TNode<Smi> flags, TNode<Context> context) {
  Label call_runtime(this, Label::kDeferred), end(this);

  GotoIf(IsUndefined(maybe_feedback_vector), &call_runtime);

  TVARIABLE(JSRegExp, result);
  TNode<FeedbackVector> feedback_vector = CAST(maybe_feedback_vector);
  TNode<Object> literal_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  GotoIf(NotHasBoilerplate(literal_site), &call_runtime);
  {
    TNode<JSRegExp> boilerplate = CAST(literal_site);
    int size =
        JSRegExp::kHeaderSize + JSRegExp::kInObjectFieldCount * kTaggedSize;
    TNode<HeapObject> copy = Allocate(size);
    for (int offset = 0; offset < size; offset += kTaggedSize) {
      TNode<Object> value = LoadObjectField(boilerplate, offset);
      StoreObjectFieldNoWriteBarrier(copy, offset, value);
    }
    result = CAST(copy);
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

TF_BUILTIN(CreateRegExpLiteral, ConstructorBuiltinsAssembler) {
  TNode<HeapObject> maybe_feedback_vector =
      CAST(Parameter(Descriptor::kFeedbackVector));
  TNode<TaggedIndex> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<Object> pattern = CAST(Parameter(Descriptor::kPattern));
  TNode<Smi> flags = CAST(Parameter(Descriptor::kFlags));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSRegExp> result = EmitCreateRegExpLiteral(maybe_feedback_vector, slot,
                                                   pattern, flags, context);
  Return(result);
}

TNode<JSArray> ConstructorBuiltinsAssembler::EmitCreateShallowArrayLiteral(
    TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
    TNode<Context> context, Label* call_runtime,
    AllocationSiteMode allocation_site_mode) {
  Label zero_capacity(this), cow_elements(this), fast_elements(this),
      return_result(this);

  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  GotoIf(NotHasBoilerplate(maybe_allocation_site), call_runtime);

  TNode<AllocationSite> allocation_site = CAST(maybe_allocation_site);
  TNode<JSArray> boilerplate = CAST(LoadBoilerplate(allocation_site));

  if (allocation_site_mode == TRACK_ALLOCATION_SITE) {
    return CloneFastJSArray(context, boilerplate, allocation_site);
  } else {
    return CloneFastJSArray(context, boilerplate);
  }
}

TF_BUILTIN(CreateShallowArrayLiteral, ConstructorBuiltinsAssembler) {
  TNode<FeedbackVector> feedback_vector =
      CAST(Parameter(Descriptor::kFeedbackVector));
  TNode<TaggedIndex> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<ArrayBoilerplateDescription> constant_elements =
      CAST(Parameter(Descriptor::kConstantElements));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Label call_runtime(this, Label::kDeferred);
  Return(EmitCreateShallowArrayLiteral(feedback_vector, slot, context,
                                       &call_runtime,
                                       DONT_TRACK_ALLOCATION_SITE));

  BIND(&call_runtime);
  {
    Comment("call runtime");
    int const flags =
        AggregateLiteral::kDisableMementos | AggregateLiteral::kIsShallow;
    Return(CallRuntime(Runtime::kCreateArrayLiteral, context, feedback_vector,
                       slot, constant_elements, SmiConstant(flags)));
  }
}

TNode<JSArray> ConstructorBuiltinsAssembler::EmitCreateEmptyArrayLiteral(
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

TF_BUILTIN(CreateEmptyArrayLiteral, ConstructorBuiltinsAssembler) {
  TNode<FeedbackVector> feedback_vector =
      CAST(Parameter(Descriptor::kFeedbackVector));
  TNode<TaggedIndex> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSArray> result =
      EmitCreateEmptyArrayLiteral(feedback_vector, slot, context);
  Return(result);
}

TNode<HeapObject> ConstructorBuiltinsAssembler::EmitCreateShallowObjectLiteral(
    TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
    Label* call_runtime) {
  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot));
  GotoIf(NotHasBoilerplate(maybe_allocation_site), call_runtime);

  TNode<AllocationSite> allocation_site = CAST(maybe_allocation_site);
  TNode<JSObject> boilerplate = LoadBoilerplate(allocation_site);
  TNode<Map> boilerplate_map = LoadMap(boilerplate);
  CSA_ASSERT(this, IsJSObjectMap(boilerplate_map));

  TVARIABLE(FixedArray, var_properties);
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
      var_properties = CopyNameDictionary(CAST(LoadSlowProperties(boilerplate)),
                                          call_runtime);
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
    // Heap numbers are only mutable on 32-bit platforms.
    bool may_use_mutable_heap_numbers = !FLAG_unbox_double_fields;
    {
      Comment("Copy in-object properties fast");
      Label continue_fast(this, &offset);
      Branch(IntPtrEqual(offset.value(), instance_size), &done_init,
             &continue_fast);
      BIND(&continue_fast);
      if (may_use_mutable_heap_numbers) {
        TNode<Object> field = LoadObjectField(boilerplate, offset.value());
        Label store_field(this);
        GotoIf(TaggedIsSmi(field), &store_field);
        // TODO(leszeks): Read the field descriptor to decide if this heap
        // number is mutable or not.
        GotoIf(IsHeapNumber(CAST(field)), &continue_with_write_barrier);
        Goto(&store_field);
        BIND(&store_field);
        StoreObjectFieldNoWriteBarrier(copy, offset.value(), field);
      } else {
        // Copy fields as raw data.
        TNode<TaggedT> field =
            LoadObjectField<TaggedT>(boilerplate, offset.value());
        StoreObjectFieldNoWriteBarrier(copy, offset.value(), field);
      }
      offset = IntPtrAdd(offset.value(), IntPtrConstant(kTaggedSize));
      Branch(WordNotEqual(offset.value(), instance_size), &continue_fast,
             &done_init);
    }

    if (!may_use_mutable_heap_numbers) {
      BIND(&done_init);
      return copy;
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
      Comment("Copy mutable HeapNumber values");
      BuildFastLoop<IntPtrT>(
          offset.value(), instance_size,
          [=](TNode<IntPtrT> offset) {
            TNode<Object> field = LoadObjectField(copy, offset);
            Label copy_heap_number(this, Label::kDeferred), continue_loop(this);
            // We only have to clone complex field values.
            GotoIf(TaggedIsSmi(field), &continue_loop);
            // TODO(leszeks): Read the field descriptor to decide if this heap
            // number is mutable or not.
            Branch(IsHeapNumber(CAST(field)), &copy_heap_number,
                   &continue_loop);
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
      Goto(&done_init);
    }
    BIND(&done_init);
  }
  return copy;
}

TF_BUILTIN(CreateShallowObjectLiteral, ConstructorBuiltinsAssembler) {
  Label call_runtime(this);
  TNode<FeedbackVector> feedback_vector =
      CAST(Parameter(Descriptor::kFeedbackVector));
  TNode<TaggedIndex> slot = CAST(Parameter(Descriptor::kSlot));
  TNode<HeapObject> copy =
      EmitCreateShallowObjectLiteral(feedback_vector, slot, &call_runtime);
  Return(copy);

  BIND(&call_runtime);
  TNode<ObjectBoilerplateDescription> object_boilerplate_description =
      CAST(Parameter(Descriptor::kObjectBoilerplateDescription));
  TNode<Smi> flags = CAST(Parameter(Descriptor::kFlags));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TailCallRuntime(Runtime::kCreateObjectLiteral, context, feedback_vector, slot,
                  object_boilerplate_description, flags);
}

// Used by the CreateEmptyObjectLiteral bytecode and the Object constructor.
TNode<JSObject> ConstructorBuiltinsAssembler::EmitCreateEmptyObjectLiteral(
    TNode<Context> context) {
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<JSFunction> object_function =
      CAST(LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX));
  TNode<Map> map = LoadObjectField<Map>(
      object_function, JSFunction::kPrototypeOrInitialMapOffset);
  // Ensure that slack tracking is disabled for the map.
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  CSA_ASSERT(this, IsClearWord32<Map::Bits3::ConstructionCounterBits>(
                       LoadMapBitField3(map)));
  TNode<FixedArray> empty_fixed_array = EmptyFixedArrayConstant();
  TNode<JSObject> result =
      AllocateJSObjectFromMap(map, empty_fixed_array, empty_fixed_array);
  return result;
}

// ES #sec-object-constructor
TF_BUILTIN(ObjectConstructor, ConstructorBuiltinsAssembler) {
  int const kValueArg = 0;
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount)));
  CodeStubArguments args(this, argc);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));

  TVARIABLE(Object, var_result);
  Label if_subclass(this, Label::kDeferred), if_notsubclass(this),
      return_result(this);
  GotoIf(IsUndefined(new_target), &if_notsubclass);
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kJSTarget));
  Branch(TaggedEqual(new_target, target), &if_notsubclass, &if_subclass);

  BIND(&if_subclass);
  {
    var_result =
        CallBuiltin(Builtins::kFastNewObject, context, target, new_target);
    Goto(&return_result);
  }

  BIND(&if_notsubclass);
  {
    Label if_newobject(this, Label::kDeferred), if_toobject(this);

    TNode<IntPtrT> value_index = IntPtrConstant(kValueArg);
    GotoIf(UintPtrGreaterThanOrEqual(value_index, argc), &if_newobject);
    TNode<Object> value = args.AtIndex(value_index);
    GotoIf(IsNull(value), &if_newobject);
    Branch(IsUndefined(value), &if_newobject, &if_toobject);

    BIND(&if_newobject);
    {
      var_result = EmitCreateEmptyObjectLiteral(context);
      Goto(&return_result);
    }

    BIND(&if_toobject);
    {
      var_result = CallBuiltin(Builtins::kToObject, context, value);
      Goto(&return_result);
    }
  }

  BIND(&return_result);
  args.PopAndReturn(var_result.value());
}

// ES #sec-number-constructor
TF_BUILTIN(NumberConstructor, ConstructorBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount)));
  CodeStubArguments args(this, argc);

  // 1. If no arguments were passed to this function invocation, let n be +0.
  TVARIABLE(Number, var_n, SmiConstant(0));
  Label if_nloaded(this, &var_n);
  GotoIf(IntPtrEqual(argc, IntPtrConstant(0)), &if_nloaded);

  // 2. Else,
  //    a. Let prim be ? ToNumeric(value).
  //    b. If Type(prim) is BigInt, let n be the Number value for prim.
  //    c. Otherwise, let n be prim.
  TNode<Object> value = args.AtIndex(0);
  var_n = ToNumber(context, value, BigIntHandling::kConvertToNumber);
  Goto(&if_nloaded);

  BIND(&if_nloaded);
  {
    // 3. If NewTarget is undefined, return n.
    TNode<Number> n_value = var_n.value();
    TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
    Label return_n(this), constructnumber(this, Label::kDeferred);
    Branch(IsUndefined(new_target), &return_n, &constructnumber);

    BIND(&return_n);
    { args.PopAndReturn(n_value); }

    BIND(&constructnumber);
    {
      // 4. Let O be ? OrdinaryCreateFromConstructor(NewTarget,
      //    "%NumberPrototype%", « [[NumberData]] »).
      // 5. Set O.[[NumberData]] to n.
      // 6. Return O.

      // We are not using Parameter(Descriptor::kJSTarget) and loading the value
      // from the current frame here in order to reduce register pressure on the
      // fast path.
      TNode<JSFunction> target = LoadTargetFromFrame();
      TNode<HeapObject> result = CAST(
          CallBuiltin(Builtins::kFastNewObject, context, target, new_target));
      StoreObjectField(result, JSPrimitiveWrapper::kValueOffset, n_value);
      args.PopAndReturn(result);
    }
  }
}

TF_BUILTIN(GenericLazyDeoptContinuation, ConstructorBuiltinsAssembler) {
  TNode<Object> result = CAST(Parameter(Descriptor::kResult));
  Return(result);
}

}  // namespace internal
}  // namespace v8

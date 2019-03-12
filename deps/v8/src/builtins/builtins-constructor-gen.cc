// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-constructor-gen.h"

#include "src/ast/ast.h"
#include "src/builtins/builtins-call-gen.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/counters.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"

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
  SloppyTNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Object> arguments_list = CAST(Parameter(Descriptor::kArgumentsList));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(ConstructWithSpread, CallOrConstructBuiltinsAssembler) {
  TNode<Object> target = CAST(Parameter(Descriptor::kTarget));
  SloppyTNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Object> spread = CAST(Parameter(Descriptor::kSpread));
  TNode<Int32T> args_count =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

typedef compiler::Node Node;

TF_BUILTIN(FastNewClosure, ConstructorBuiltinsAssembler) {
  Node* shared_function_info = Parameter(Descriptor::kSharedFunctionInfo);
  Node* feedback_cell = Parameter(Descriptor::kFeedbackCell);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsFeedbackCell(feedback_cell));
  CSA_ASSERT(this, IsSharedFunctionInfo(shared_function_info));

  IncrementCounter(isolate()->counters()->fast_new_closure_total(), 1);

  // Bump the closure counter encoded the {feedback_cell}s map.
  {
    Node* const feedback_cell_map = LoadMap(feedback_cell);
    Label no_closures(this), one_closure(this), cell_done(this);

    GotoIf(IsNoFeedbackCellMap(feedback_cell_map), &cell_done);
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
  Node* const flags =
      LoadObjectField(shared_function_info, SharedFunctionInfo::kFlagsOffset,
                      MachineType::Uint32());
  Node* const function_map_index = IntPtrAdd(
      DecodeWordFromWord32<SharedFunctionInfo::FunctionMapIndexBits>(flags),
      IntPtrConstant(Context::FIRST_FUNCTION_MAP_INDEX));
  CSA_ASSERT(this, UintPtrLessThanOrEqual(
                       function_map_index,
                       IntPtrConstant(Context::LAST_FUNCTION_MAP_INDEX)));

  // Get the function map in the current native context and set that
  // as the map of the allocated object.
  Node* const native_context = LoadNativeContext(context);
  Node* const function_map =
      LoadContextElement(native_context, function_map_index);

  // Create a new closure from the given function info in new space
  TNode<IntPtrT> instance_size_in_bytes =
      TimesTaggedSize(LoadMapInstanceSizeInWords(function_map));
  TNode<Object> result = Allocate(instance_size_in_bytes);
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
  Node* lazy_builtin = HeapConstant(lazy_builtin_handle);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kCodeOffset, lazy_builtin);
  Return(result);
}

TF_BUILTIN(FastNewObject, ConstructorBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* target = Parameter(Descriptor::kTarget);
  Node* new_target = Parameter(Descriptor::kNewTarget);

  Label call_runtime(this);

  Node* result = EmitFastNewObject(context, target, new_target, &call_runtime);
  Return(result);

  BIND(&call_runtime);
  TailCallRuntime(Runtime::kNewObject, context, target, new_target);
}

Node* ConstructorBuiltinsAssembler::EmitFastNewObject(Node* context,
                                                      Node* target,
                                                      Node* new_target) {
  VARIABLE(var_obj, MachineRepresentation::kTagged);
  Label call_runtime(this), end(this);

  Node* result = EmitFastNewObject(context, target, new_target, &call_runtime);
  var_obj.Bind(result);
  Goto(&end);

  BIND(&call_runtime);
  var_obj.Bind(CallRuntime(Runtime::kNewObject, context, target, new_target));
  Goto(&end);

  BIND(&end);
  return var_obj.value();
}

Node* ConstructorBuiltinsAssembler::EmitFastNewObject(Node* context,
                                                      Node* target,
                                                      Node* new_target,
                                                      Label* call_runtime) {
  CSA_ASSERT(this, HasInstanceType(target, JS_FUNCTION_TYPE));
  CSA_ASSERT(this, IsJSReceiver(new_target));

  // Verify that the new target is a JSFunction.
  Label fast(this), end(this);
  GotoIf(HasInstanceType(new_target, JS_FUNCTION_TYPE), &fast);
  Goto(call_runtime);

  BIND(&fast);

  // Load the initial map and verify that it's in fact a map.
  Node* initial_map =
      LoadObjectField(new_target, JSFunction::kPrototypeOrInitialMapOffset);
  GotoIf(TaggedIsSmi(initial_map), call_runtime);
  GotoIf(DoesntHaveInstanceType(initial_map, MAP_TYPE), call_runtime);

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  Node* new_target_constructor =
      LoadObjectField(initial_map, Map::kConstructorOrBackPointerOffset);
  GotoIf(WordNotEqual(target, new_target_constructor), call_runtime);

  VARIABLE(properties, MachineRepresentation::kTagged);

  Label instantiate_map(this), allocate_properties(this);
  GotoIf(IsDictionaryMap(initial_map), &allocate_properties);
  {
    properties.Bind(EmptyFixedArrayConstant());
    Goto(&instantiate_map);
  }
  BIND(&allocate_properties);
  {
    properties.Bind(AllocateNameDictionary(NameDictionary::kInitialCapacity));
    Goto(&instantiate_map);
  }

  BIND(&instantiate_map);
  return AllocateJSObjectFromMap(initial_map, properties.value(), nullptr,
                                 kNone, kWithSlackTracking);
}

Node* ConstructorBuiltinsAssembler::EmitFastNewFunctionContext(
    Node* scope_info, Node* slots_uint32, Node* context, ScopeType scope_type) {
  TNode<IntPtrT> slots = Signed(ChangeUint32ToWord(slots_uint32));
  TNode<IntPtrT> size = ElementOffsetFromIndex(
      slots, PACKED_ELEMENTS, INTPTR_PARAMETERS, Context::kTodoHeaderSize);

  // Create a new closure from the given function info in new space
  TNode<Context> function_context =
      UncheckedCast<Context>(AllocateInNewSpace(size));

  RootIndex context_type;
  switch (scope_type) {
    case EVAL_SCOPE:
      context_type = RootIndex::kEvalContextMap;
      break;
    case FUNCTION_SCOPE:
      context_type = RootIndex::kFunctionContextMap;
      break;
    default:
      UNREACHABLE();
  }
  // Set up the header.
  StoreMapNoWriteBarrier(function_context, context_type);
  TNode<IntPtrT> min_context_slots = IntPtrConstant(Context::MIN_CONTEXT_SLOTS);
  // TODO(ishell): for now, length also includes MIN_CONTEXT_SLOTS.
  TNode<IntPtrT> length = IntPtrAdd(slots, min_context_slots);
  StoreObjectFieldNoWriteBarrier(function_context, Context::kLengthOffset,
                                 SmiTag(length));
  StoreObjectFieldNoWriteBarrier(function_context, Context::kScopeInfoOffset,
                                 scope_info);
  StoreObjectFieldNoWriteBarrier(function_context, Context::kPreviousOffset,
                                 context);
  StoreObjectFieldNoWriteBarrier(function_context, Context::kExtensionOffset,
                                 TheHoleConstant());
  TNode<Context> native_context = LoadNativeContext(context);
  StoreObjectFieldNoWriteBarrier(function_context,
                                 Context::kNativeContextOffset, native_context);

  // Initialize the varrest of the slots to undefined.
  TNode<HeapObject> undefined = UndefinedConstant();
  TNode<IntPtrT> start_offset = IntPtrConstant(Context::kTodoHeaderSize);
  CodeStubAssembler::VariableList vars(0, zone());
  BuildFastLoop(
      vars, start_offset, size,
      [=](Node* offset) {
        StoreObjectFieldNoWriteBarrier(
            function_context, UncheckedCast<IntPtrT>(offset), undefined);
      },
      kTaggedSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
  return function_context;
}

TF_BUILTIN(FastNewFunctionContextEval, ConstructorBuiltinsAssembler) {
  Node* scope_info = Parameter(Descriptor::kScopeInfo);
  Node* slots = Parameter(Descriptor::kSlots);
  Node* context = Parameter(Descriptor::kContext);
  Return(EmitFastNewFunctionContext(scope_info, slots, context,
                                    ScopeType::EVAL_SCOPE));
}

TF_BUILTIN(FastNewFunctionContextFunction, ConstructorBuiltinsAssembler) {
  Node* scope_info = Parameter(Descriptor::kScopeInfo);
  Node* slots = Parameter(Descriptor::kSlots);
  Node* context = Parameter(Descriptor::kContext);
  Return(EmitFastNewFunctionContext(scope_info, slots, context,
                                    ScopeType::FUNCTION_SCOPE));
}

Node* ConstructorBuiltinsAssembler::EmitCreateRegExpLiteral(
    Node* feedback_vector, Node* slot, Node* pattern, Node* flags,
    Node* context) {
  Label call_runtime(this, Label::kDeferred), end(this);

  VARIABLE(result, MachineRepresentation::kTagged);
  TNode<Object> literal_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS));
  GotoIf(NotHasBoilerplate(literal_site), &call_runtime);
  {
    Node* boilerplate = literal_site;
    CSA_ASSERT(this, IsJSRegExp(boilerplate));
    int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kTaggedSize;
    Node* copy = Allocate(size);
    for (int offset = 0; offset < size; offset += kTaggedSize) {
      Node* value = LoadObjectField(boilerplate, offset);
      StoreObjectFieldNoWriteBarrier(copy, offset, value);
    }
    result.Bind(copy);
    Goto(&end);
  }

  BIND(&call_runtime);
  {
    result.Bind(CallRuntime(Runtime::kCreateRegExpLiteral, context,
                            feedback_vector, SmiTag(slot), pattern, flags));
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

TF_BUILTIN(CreateRegExpLiteral, ConstructorBuiltinsAssembler) {
  Node* feedback_vector = Parameter(Descriptor::kFeedbackVector);
  Node* slot = SmiUntag(Parameter(Descriptor::kSlot));
  Node* pattern = Parameter(Descriptor::kPattern);
  Node* flags = Parameter(Descriptor::kFlags);
  Node* context = Parameter(Descriptor::kContext);
  Node* result =
      EmitCreateRegExpLiteral(feedback_vector, slot, pattern, flags, context);
  Return(result);
}

Node* ConstructorBuiltinsAssembler::EmitCreateShallowArrayLiteral(
    Node* feedback_vector, Node* slot, Node* context, Label* call_runtime,
    AllocationSiteMode allocation_site_mode) {
  Label zero_capacity(this), cow_elements(this), fast_elements(this),
      return_result(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS));
  GotoIf(NotHasBoilerplate(maybe_allocation_site), call_runtime);

  TNode<AllocationSite> allocation_site = CAST(maybe_allocation_site);
  TNode<JSArray> boilerplate = CAST(LoadBoilerplate(allocation_site));

  ParameterMode mode = OptimalParameterMode();
  if (allocation_site_mode == TRACK_ALLOCATION_SITE) {
    return CloneFastJSArray(context, boilerplate, mode, allocation_site);
  } else {
    return CloneFastJSArray(context, boilerplate, mode);
  }
}

TF_BUILTIN(CreateShallowArrayLiteral, ConstructorBuiltinsAssembler) {
  Node* feedback_vector = Parameter(Descriptor::kFeedbackVector);
  Node* slot = SmiUntag(Parameter(Descriptor::kSlot));
  Node* constant_elements = Parameter(Descriptor::kConstantElements);
  Node* context = Parameter(Descriptor::kContext);
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
                       SmiTag(slot), constant_elements, SmiConstant(flags)));
  }
}

Node* ConstructorBuiltinsAssembler::EmitCreateEmptyArrayLiteral(
    Node* feedback_vector, Node* slot, Node* context) {
  // Array literals always have a valid AllocationSite to properly track
  // elements transitions.
  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS));
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
    allocation_site =
        CreateAllocationSiteInFeedbackVector(feedback_vector, SmiTag(slot));
    Goto(&create_empty_array);
  }

  BIND(&create_empty_array);
  TNode<Int32T> kind = LoadElementsKind(allocation_site.value());
  TNode<Context> native_context = LoadNativeContext(context);
  Comment("LoadJSArrayElementsMap");
  TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);
  TNode<Smi> zero = SmiConstant(0);
  Comment("Allocate JSArray");
  TNode<JSArray> result =
      AllocateJSArray(GetInitialFastElementsKind(), array_map, zero, zero,
                      allocation_site.value(), ParameterMode::SMI_PARAMETERS);

  Goto(&done);
  BIND(&done);

  return result;
}

TF_BUILTIN(CreateEmptyArrayLiteral, ConstructorBuiltinsAssembler) {
  Node* feedback_vector = Parameter(Descriptor::kFeedbackVector);
  Node* slot = SmiUntag(Parameter(Descriptor::kSlot));
  Node* context = Parameter(Descriptor::kContext);
  Node* result = EmitCreateEmptyArrayLiteral(feedback_vector, slot, context);
  Return(result);
}

Node* ConstructorBuiltinsAssembler::EmitCreateShallowObjectLiteral(
    Node* feedback_vector, Node* slot, Label* call_runtime) {
  TNode<Object> maybe_allocation_site =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS));
  GotoIf(NotHasBoilerplate(maybe_allocation_site), call_runtime);

  TNode<AllocationSite> allocation_site = CAST(maybe_allocation_site);
  TNode<JSObject> boilerplate = LoadBoilerplate(allocation_site);
  TNode<Map> boilerplate_map = LoadMap(boilerplate);
  CSA_ASSERT(this, IsJSObjectMap(boilerplate_map));

  VARIABLE(var_properties, MachineRepresentation::kTagged);
  {
    Node* bit_field_3 = LoadMapBitField3(boilerplate_map);
    GotoIf(IsSetWord32<Map::IsDeprecatedBit>(bit_field_3), call_runtime);
    // Directly copy over the property store for dict-mode boilerplates.
    Label if_dictionary(this), if_fast(this), done(this);
    Branch(IsSetWord32<Map::IsDictionaryMapBit>(bit_field_3), &if_dictionary,
           &if_fast);
    BIND(&if_dictionary);
    {
      Comment("Copy dictionary properties");
      var_properties.Bind(CopyNameDictionary(
          CAST(LoadSlowProperties(boilerplate)), call_runtime));
      // Slow objects have no in-object properties.
      Goto(&done);
    }
    BIND(&if_fast);
    {
      // TODO(cbruni): support copying out-of-object properties.
      Node* boilerplate_properties = LoadFastProperties(boilerplate);
      GotoIfNot(IsEmptyFixedArray(boilerplate_properties), call_runtime);
      var_properties.Bind(EmptyFixedArrayConstant());
      Goto(&done);
    }
    BIND(&done);
  }

  VARIABLE(var_elements, MachineRepresentation::kTagged);
  {
    // Copy the elements backing store, assuming that it's flat.
    Label if_empty_fixed_array(this), if_copy_elements(this), done(this);
    Node* boilerplate_elements = LoadElements(boilerplate);
    Branch(IsEmptyFixedArray(boilerplate_elements), &if_empty_fixed_array,
           &if_copy_elements);

    BIND(&if_empty_fixed_array);
    var_elements.Bind(boilerplate_elements);
    Goto(&done);

    BIND(&if_copy_elements);
    CSA_ASSERT(this, Word32BinaryNot(
                         IsFixedCOWArrayMap(LoadMap(boilerplate_elements))));
    ExtractFixedArrayFlags flags;
    flags |= ExtractFixedArrayFlag::kAllFixedArrays;
    flags |= ExtractFixedArrayFlag::kNewSpaceAllocationOnly;
    flags |= ExtractFixedArrayFlag::kDontCopyCOW;
    var_elements.Bind(CloneFixedArray(boilerplate_elements, flags));
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
    // Mutable heap numbers only occur on 32-bit platforms.
    bool may_use_mutable_heap_numbers = !FLAG_unbox_double_fields;
    {
      Comment("Copy in-object properties fast");
      Label continue_fast(this, &offset);
      Branch(WordEqual(offset.value(), instance_size), &done_init,
             &continue_fast);
      BIND(&continue_fast);
      if (may_use_mutable_heap_numbers) {
        TNode<Object> field = LoadObjectField(boilerplate, offset.value());
        Label store_field(this);
        GotoIf(TaggedIsSmi(field), &store_field);
        GotoIf(IsMutableHeapNumber(CAST(field)), &continue_with_write_barrier);
        Goto(&store_field);
        BIND(&store_field);
        StoreObjectFieldNoWriteBarrier(copy, offset.value(), field);
      } else {
        // Copy fields as raw data.
        TNode<IntPtrT> field =
            LoadObjectField<IntPtrT>(boilerplate, offset.value());
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
      BuildFastLoop(
          offset.value(), instance_size,
          [=](Node* offset) {
            // TODO(ishell): value decompression is not necessary here.
            Node* field = LoadObjectField(boilerplate, offset);
            StoreObjectFieldNoWriteBarrier(copy, offset, field);
          },
          kTaggedSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      Comment("Copy mutable HeapNumber values");
      BuildFastLoop(
          offset.value(), instance_size,
          [=](Node* offset) {
            Node* field = LoadObjectField(copy, offset);
            Label copy_mutable_heap_number(this, Label::kDeferred),
                continue_loop(this);
            // We only have to clone complex field values.
            GotoIf(TaggedIsSmi(field), &continue_loop);
            Branch(IsMutableHeapNumber(field), &copy_mutable_heap_number,
                   &continue_loop);
            BIND(&copy_mutable_heap_number);
            {
              Node* double_value = LoadHeapNumberValue(field);
              Node* mutable_heap_number =
                  AllocateMutableHeapNumberWithValue(double_value);
              StoreObjectField(copy, offset, mutable_heap_number);
              Goto(&continue_loop);
            }
            BIND(&continue_loop);
          },
          kTaggedSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      Goto(&done_init);
    }
    BIND(&done_init);
  }
  return copy;
}

TF_BUILTIN(CreateShallowObjectLiteral, ConstructorBuiltinsAssembler) {
  Label call_runtime(this);
  Node* feedback_vector = Parameter(Descriptor::kFeedbackVector);
  Node* slot = SmiUntag(Parameter(Descriptor::kSlot));
  Node* copy =
      EmitCreateShallowObjectLiteral(feedback_vector, slot, &call_runtime);
  Return(copy);

  BIND(&call_runtime);
  Node* object_boilerplate_description =
      Parameter(Descriptor::kObjectBoilerplateDescription);
  Node* flags = Parameter(Descriptor::kFlags);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kCreateObjectLiteral, context, feedback_vector,
                  SmiTag(slot), object_boilerplate_description, flags);
}

// Used by the CreateEmptyObjectLiteral bytecode and the Object constructor.
Node* ConstructorBuiltinsAssembler::EmitCreateEmptyObjectLiteral(
    Node* context) {
  Node* native_context = LoadNativeContext(context);
  Node* object_function =
      LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX);
  Node* map = LoadObjectField(object_function,
                              JSFunction::kPrototypeOrInitialMapOffset);
  CSA_ASSERT(this, IsMap(map));
  // Ensure that slack tracking is disabled for the map.
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  CSA_ASSERT(
      this, IsClearWord32<Map::ConstructionCounterBits>(LoadMapBitField3(map)));
  Node* empty_fixed_array = EmptyFixedArrayConstant();
  Node* result =
      AllocateJSObjectFromMap(map, empty_fixed_array, empty_fixed_array);
  return result;
}

// ES #sec-object-constructor
TF_BUILTIN(ObjectConstructor, ConstructorBuiltinsAssembler) {
  int const kValueArg = 0;
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* context = Parameter(Descriptor::kContext);
  Node* new_target = Parameter(Descriptor::kJSNewTarget);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_subclass(this, Label::kDeferred), if_notsubclass(this),
      return_result(this);
  GotoIf(IsUndefined(new_target), &if_notsubclass);
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kJSTarget));
  Branch(WordEqual(new_target, target), &if_notsubclass, &if_subclass);

  BIND(&if_subclass);
  {
    Node* result =
        CallBuiltin(Builtins::kFastNewObject, context, target, new_target);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&if_notsubclass);
  {
    Label if_newobject(this, Label::kDeferred), if_toobject(this);

    Node* value_index = IntPtrConstant(kValueArg);
    GotoIf(UintPtrGreaterThanOrEqual(value_index, argc), &if_newobject);
    Node* value = args.AtIndex(value_index);
    GotoIf(IsNull(value), &if_newobject);
    Branch(IsUndefined(value), &if_newobject, &if_toobject);

    BIND(&if_newobject);
    {
      Node* result = EmitCreateEmptyObjectLiteral(context);
      var_result.Bind(result);
      Goto(&return_result);
    }

    BIND(&if_toobject);
    {
      Node* result = CallBuiltin(Builtins::kToObject, context, value);
      var_result.Bind(result);
      Goto(&return_result);
    }
  }

  BIND(&return_result);
  args.PopAndReturn(var_result.value());
}

// ES #sec-number-constructor
TF_BUILTIN(NumberConstructor, ConstructorBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  // 1. If no arguments were passed to this function invocation, let n be +0.
  VARIABLE(var_n, MachineRepresentation::kTagged, SmiConstant(0));
  Label if_nloaded(this, &var_n);
  GotoIf(WordEqual(argc, IntPtrConstant(0)), &if_nloaded);

  // 2. Else,
  //    a. Let prim be ? ToNumeric(value).
  //    b. If Type(prim) is BigInt, let n be the Number value for prim.
  //    c. Otherwise, let n be prim.
  Node* value = args.AtIndex(0);
  var_n.Bind(ToNumber(context, value, BigIntHandling::kConvertToNumber));
  Goto(&if_nloaded);

  BIND(&if_nloaded);
  {
    // 3. If NewTarget is undefined, return n.
    Node* n_value = var_n.value();
    Node* new_target = Parameter(Descriptor::kJSNewTarget);
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
      Node* result =
          CallBuiltin(Builtins::kFastNewObject, context, target, new_target);
      StoreObjectField(result, JSValue::kValueOffset, n_value);
      args.PopAndReturn(result);
    }
  }
}

TF_BUILTIN(GenericConstructorLazyDeoptContinuation,
           ConstructorBuiltinsAssembler) {
  Node* result = Parameter(Descriptor::kResult);
  Return(result);
}

// https://tc39.github.io/ecma262/#sec-string-constructor
TF_BUILTIN(StringConstructor, ConstructorBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));

  // 1. If no arguments were passed to this function invocation, let s be "".
  VARIABLE(var_s, MachineRepresentation::kTagged, EmptyStringConstant());
  Label if_sloaded(this, &var_s);
  GotoIf(WordEqual(argc, IntPtrConstant(0)), &if_sloaded);

  // 2. Else,
  //    a. If NewTarget is undefined [...]
  Node* value = args.AtIndex(0);
  Label if_tostring(this, &var_s);
  GotoIfNot(IsUndefined(new_target), &if_tostring);

  // 2a. [...] and Type(value) is Symbol, return SymbolDescriptiveString(value).
  GotoIf(TaggedIsSmi(value), &if_tostring);
  GotoIfNot(IsSymbol(value), &if_tostring);
  {
    Node* result =
        CallRuntime(Runtime::kSymbolDescriptiveString, context, value);
    args.PopAndReturn(result);
  }

  // 2b. Let s be ? ToString(value).
  BIND(&if_tostring);
  {
    var_s.Bind(CallBuiltin(Builtins::kToString, context, value));
    Goto(&if_sloaded);
  }

  // 3. If NewTarget is undefined, return s.
  BIND(&if_sloaded);
  {
    Node* s_value = var_s.value();
    Label return_s(this), constructstring(this, Label::kDeferred);
    Branch(IsUndefined(new_target), &return_s, &constructstring);

    BIND(&return_s);
    { args.PopAndReturn(s_value); }

    BIND(&constructstring);
    {
      // We are not using Parameter(Descriptor::kJSTarget) and loading the value
      // from the current frame here in order to reduce register pressure on the
      // fast path.
      TNode<JSFunction> target = LoadTargetFromFrame();

      Node* result =
          CallBuiltin(Builtins::kFastNewObject, context, target, new_target);
      StoreObjectField(result, JSValue::kValueOffset, s_value);
      args.PopAndReturn(result);
    }
  }
}

}  // namespace internal
}  // namespace v8

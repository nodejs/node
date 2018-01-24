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
  Node* target = Parameter(ConstructWithArrayLikeDescriptor::kTarget);
  Node* new_target = Parameter(ConstructWithArrayLikeDescriptor::kNewTarget);
  Node* arguments_list =
      Parameter(ConstructWithArrayLikeDescriptor::kArgumentsList);
  Node* context = Parameter(ConstructWithArrayLikeDescriptor::kContext);
  CallOrConstructWithArrayLike(target, new_target, arguments_list, context);
}

TF_BUILTIN(ConstructWithSpread, CallOrConstructBuiltinsAssembler) {
  Node* target = Parameter(ConstructWithSpreadDescriptor::kTarget);
  Node* new_target = Parameter(ConstructWithSpreadDescriptor::kNewTarget);
  Node* spread = Parameter(ConstructWithSpreadDescriptor::kSpread);
  Node* args_count = Parameter(ConstructWithSpreadDescriptor::kArgumentsCount);
  Node* context = Parameter(ConstructWithSpreadDescriptor::kContext);
  CallOrConstructWithSpread(target, new_target, spread, args_count, context);
}

typedef compiler::Node Node;

Node* ConstructorBuiltinsAssembler::EmitFastNewClosure(Node* shared_info,
                                                       Node* feedback_vector,
                                                       Node* slot,
                                                       Node* context) {
  Isolate* isolate = this->isolate();
  Factory* factory = isolate->factory();
  IncrementCounter(isolate->counters()->fast_new_closure_total(), 1);

  Node* compiler_hints =
      LoadObjectField(shared_info, SharedFunctionInfo::kCompilerHintsOffset,
                      MachineType::Uint32());

  // The calculation of |function_map_index| must be in sync with
  // SharedFunctionInfo::function_map_index().
  Node* function_map_index =
      IntPtrAdd(DecodeWordFromWord32<SharedFunctionInfo::FunctionMapIndexBits>(
                    compiler_hints),
                IntPtrConstant(Context::FIRST_FUNCTION_MAP_INDEX));
  CSA_ASSERT(this, UintPtrLessThanOrEqual(
                       function_map_index,
                       IntPtrConstant(Context::LAST_FUNCTION_MAP_INDEX)));

  // Get the function map in the current native context and set that
  // as the map of the allocated object.
  Node* native_context = LoadNativeContext(context);
  Node* function_map = LoadContextElement(native_context, function_map_index);

  // Create a new closure from the given function info in new space
  Node* instance_size_in_bytes =
      TimesPointerSize(LoadMapInstanceSizeInWords(function_map));
  Node* result = Allocate(instance_size_in_bytes);
  StoreMapNoWriteBarrier(result, function_map);
  InitializeJSObjectBodyNoSlackTracking(result, function_map,
                                        instance_size_in_bytes,
                                        JSFunction::kSizeWithoutPrototype);

  // Initialize the rest of the function.
  Node* empty_fixed_array = HeapConstant(factory->empty_fixed_array());
  StoreObjectFieldNoWriteBarrier(result, JSObject::kPropertiesOrHashOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(result, JSObject::kElementsOffset,
                                 empty_fixed_array);
  {
    // Set function prototype if necessary.
    Label done(this), init_prototype(this);
    Branch(IsFunctionWithPrototypeSlotMap(function_map), &init_prototype,
           &done);

    BIND(&init_prototype);
    StoreObjectFieldNoWriteBarrier(
        result, JSFunction::kPrototypeOrInitialMapOffset, TheHoleConstant());
    Goto(&done);

    BIND(&done);
  }

  Node* literals_cell = LoadFeedbackVectorSlot(
      feedback_vector, slot, 0, CodeStubAssembler::SMI_PARAMETERS);
  {
    // Bump the closure counter encoded in the cell's map.
    Node* cell_map = LoadMap(literals_cell);
    Label no_closures(this), one_closure(this), cell_done(this);

    GotoIf(IsNoClosuresCellMap(cell_map), &no_closures);
    GotoIf(IsOneClosureCellMap(cell_map), &one_closure);
    CSA_ASSERT(this, IsManyClosuresCellMap(cell_map), cell_map, literals_cell,
               feedback_vector, slot);
    Goto(&cell_done);

    BIND(&no_closures);
    StoreMapNoWriteBarrier(literals_cell, Heap::kOneClosureCellMapRootIndex);
    Goto(&cell_done);

    BIND(&one_closure);
    StoreMapNoWriteBarrier(literals_cell, Heap::kManyClosuresCellMapRootIndex);
    Goto(&cell_done);

    BIND(&cell_done);
  }
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kFeedbackVectorOffset,
                                 literals_cell);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kSharedFunctionInfoOffset,
                                 shared_info);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kContextOffset, context);
  Handle<Code> lazy_builtin_handle(
      isolate->builtins()->builtin(Builtins::kCompileLazy));
  Node* lazy_builtin = HeapConstant(lazy_builtin_handle);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kCodeOffset, lazy_builtin);
  return result;
}

Node* ConstructorBuiltinsAssembler::NotHasBoilerplate(Node* literal_site) {
  return TaggedIsSmi(literal_site);
}

Node* ConstructorBuiltinsAssembler::LoadAllocationSiteBoilerplate(Node* site) {
  CSA_ASSERT(this, IsAllocationSite(site));
  return LoadObjectField(site,
                         AllocationSite::kTransitionInfoOrBoilerplateOffset);
}

TF_BUILTIN(FastNewClosure, ConstructorBuiltinsAssembler) {
  Node* shared = Parameter(FastNewClosureDescriptor::kSharedFunctionInfo);
  Node* context = Parameter(FastNewClosureDescriptor::kContext);
  Node* vector = Parameter(FastNewClosureDescriptor::kVector);
  Node* slot = Parameter(FastNewClosureDescriptor::kSlot);
  Return(EmitFastNewClosure(shared, vector, slot, context));
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
    Node* function, Node* slots, Node* context, ScopeType scope_type) {
  slots = ChangeUint32ToWord(slots);

  // TODO(ishell): Use CSA::OptimalParameterMode() here.
  ParameterMode mode = INTPTR_PARAMETERS;
  Node* min_context_slots = IntPtrConstant(Context::MIN_CONTEXT_SLOTS);
  Node* length = IntPtrAdd(slots, min_context_slots);
  Node* size = GetFixedArrayAllocationSize(length, PACKED_ELEMENTS, mode);

  // Create a new closure from the given function info in new space
  Node* function_context = AllocateInNewSpace(size);

  Heap::RootListIndex context_type;
  switch (scope_type) {
    case EVAL_SCOPE:
      context_type = Heap::kEvalContextMapRootIndex;
      break;
    case FUNCTION_SCOPE:
      context_type = Heap::kFunctionContextMapRootIndex;
      break;
    default:
      UNREACHABLE();
  }
  StoreMapNoWriteBarrier(function_context, context_type);
  StoreObjectFieldNoWriteBarrier(function_context, Context::kLengthOffset,
                                 SmiTag(length));

  // Set up the fixed slots.
  StoreFixedArrayElement(function_context, Context::CLOSURE_INDEX, function,
                         SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(function_context, Context::PREVIOUS_INDEX, context,
                         SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(function_context, Context::EXTENSION_INDEX,
                         TheHoleConstant(), SKIP_WRITE_BARRIER);

  // Copy the native context from the previous context.
  Node* native_context = LoadNativeContext(context);
  StoreFixedArrayElement(function_context, Context::NATIVE_CONTEXT_INDEX,
                         native_context, SKIP_WRITE_BARRIER);

  // Initialize the rest of the slots to undefined.
  Node* undefined = UndefinedConstant();
  BuildFastFixedArrayForEach(
      function_context, PACKED_ELEMENTS, min_context_slots, length,
      [this, undefined](Node* context, Node* offset) {
        StoreNoWriteBarrier(MachineRepresentation::kTagged, context, offset,
                            undefined);
      },
      mode);

  return function_context;
}

TF_BUILTIN(FastNewFunctionContextEval, ConstructorBuiltinsAssembler) {
  Node* function = Parameter(FastNewFunctionContextDescriptor::kFunction);
  Node* slots = Parameter(FastNewFunctionContextDescriptor::kSlots);
  Node* context = Parameter(FastNewFunctionContextDescriptor::kContext);
  Return(EmitFastNewFunctionContext(function, slots, context,
                                    ScopeType::EVAL_SCOPE));
}

TF_BUILTIN(FastNewFunctionContextFunction, ConstructorBuiltinsAssembler) {
  Node* function = Parameter(FastNewFunctionContextDescriptor::kFunction);
  Node* slots = Parameter(FastNewFunctionContextDescriptor::kSlots);
  Node* context = Parameter(FastNewFunctionContextDescriptor::kContext);
  Return(EmitFastNewFunctionContext(function, slots, context,
                                    ScopeType::FUNCTION_SCOPE));
}

Node* ConstructorBuiltinsAssembler::EmitCreateRegExpLiteral(
    Node* feedback_vector, Node* slot, Node* pattern, Node* flags,
    Node* context) {
  Label call_runtime(this, Label::kDeferred), end(this);

  VARIABLE(result, MachineRepresentation::kTagged);
  Node* literal_site =
      LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS);
  GotoIf(NotHasBoilerplate(literal_site), &call_runtime);
  {
    Node* boilerplate = literal_site;
    CSA_ASSERT(this, IsJSRegExp(boilerplate));
    int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
    Node* copy = Allocate(size);
    for (int offset = 0; offset < size; offset += kPointerSize) {
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

  Node* allocation_site =
      LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS);
  GotoIf(NotHasBoilerplate(allocation_site), call_runtime);

  Node* boilerplate = LoadAllocationSiteBoilerplate(allocation_site);
  allocation_site =
      allocation_site_mode == TRACK_ALLOCATION_SITE ? allocation_site : nullptr;

  CSA_ASSERT(this, IsJSArrayMap(LoadMap(boilerplate)));
  ParameterMode mode = OptimalParameterMode();
  return CloneFastJSArray(context, boilerplate, mode, allocation_site);
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
  VARIABLE(allocation_site, MachineRepresentation::kTagged,
           LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS));

  Label create_empty_array(this),
      initialize_allocation_site(this, Label::kDeferred), done(this);
  Branch(TaggedIsSmi(allocation_site.value()), &initialize_allocation_site,
         &create_empty_array);

  // TODO(cbruni): create the AllocationSite in CSA.
  BIND(&initialize_allocation_site);
  {
    allocation_site.Bind(
        CreateAllocationSiteInFeedbackVector(feedback_vector, SmiTag(slot)));
    Goto(&create_empty_array);
  }

  BIND(&create_empty_array);
  CSA_ASSERT(this, IsAllocationSite(allocation_site.value()));
  Node* kind = SmiToWord32(CAST(
      LoadObjectField(allocation_site.value(),
                      AllocationSite::kTransitionInfoOrBoilerplateOffset)));
  CSA_ASSERT(this, IsFastElementsKind(kind));
  Node* native_context = LoadNativeContext(context);
  Comment("LoadJSArrayElementsMap");
  Node* array_map = LoadJSArrayElementsMap(kind, native_context);
  Node* zero = SmiConstant(0);
  Comment("Allocate JSArray");
  Node* result =
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
  Node* allocation_site =
      LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS);
  GotoIf(NotHasBoilerplate(allocation_site), call_runtime);

  Node* boilerplate = LoadAllocationSiteBoilerplate(allocation_site);
  Node* boilerplate_map = LoadMap(boilerplate);
  CSA_ASSERT(this, IsJSObjectMap(boilerplate_map));

  VARIABLE(var_properties, MachineRepresentation::kTagged);
  {
    Node* bit_field_3 = LoadMapBitField3(boilerplate_map);
    GotoIf(IsSetWord32<Map::Deprecated>(bit_field_3), call_runtime);
    // Directly copy over the property store for dict-mode boilerplates.
    Label if_dictionary(this), if_fast(this), done(this);
    Branch(IsSetWord32<Map::DictionaryMap>(bit_field_3), &if_dictionary,
           &if_fast);
    BIND(&if_dictionary);
    {
      Comment("Copy dictionary properties");
      var_properties.Bind(
          CopyNameDictionary(LoadSlowProperties(boilerplate), call_runtime));
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
  Node* instance_size =
      TimesPointerSize(LoadMapInstanceSizeInWords(boilerplate_map));
  Node* allocation_size = instance_size;
  bool needs_allocation_memento = FLAG_allocation_site_pretenuring;
  if (needs_allocation_memento) {
    // Prepare for inner-allocating the AllocationMemento.
    allocation_size =
        IntPtrAdd(instance_size, IntPtrConstant(AllocationMemento::kSize));
  }

  Node* copy = AllocateInNewSpace(allocation_size);
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
    VARIABLE(offset, MachineType::PointerRepresentation(),
             IntPtrConstant(JSObject::kHeaderSize));
    // Mutable heap numbers only occur on 32-bit platforms.
    bool may_use_mutable_heap_numbers =
        FLAG_track_double_fields && !FLAG_unbox_double_fields;
    {
      Comment("Copy in-object properties fast");
      Label continue_fast(this, &offset);
      Branch(WordEqual(offset.value(), instance_size), &done_init,
             &continue_fast);
      BIND(&continue_fast);
      Node* field = LoadObjectField(boilerplate, offset.value());
      if (may_use_mutable_heap_numbers) {
        Label store_field(this);
        GotoIf(TaggedIsSmi(field), &store_field);
        GotoIf(IsMutableHeapNumber(field), &continue_with_write_barrier);
        Goto(&store_field);
        BIND(&store_field);
      }
      StoreObjectFieldNoWriteBarrier(copy, offset.value(), field);
      offset.Bind(IntPtrAdd(offset.value(), IntPtrConstant(kPointerSize)));
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
      BuildFastLoop(offset.value(), instance_size,
                    [=](Node* offset) {
                      Node* field = LoadObjectField(boilerplate, offset);
                      StoreObjectFieldNoWriteBarrier(copy, offset, field);
                    },
                    kPointerSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      Comment("Copy mutable HeapNumber values");
      BuildFastLoop(offset.value(), instance_size,
                    [=](Node* offset) {
                      Node* field = LoadObjectField(copy, offset);
                      Label copy_mutable_heap_number(this, Label::kDeferred),
                          continue_loop(this);
                      // We only have to clone complex field values.
                      GotoIf(TaggedIsSmi(field), &continue_loop);
                      Branch(IsMutableHeapNumber(field),
                             &copy_mutable_heap_number, &continue_loop);
                      BIND(&copy_mutable_heap_number);
                      {
                        Node* double_value = LoadHeapNumberValue(field);
                        Node* mutable_heap_number =
                            AllocateHeapNumberWithValue(double_value, MUTABLE);
                        StoreObjectField(copy, offset, mutable_heap_number);
                        Goto(&continue_loop);
                      }
                      BIND(&continue_loop);
                    },
                    kPointerSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
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
  Node* boilerplate_description =
      Parameter(Descriptor::kBoilerplateDescription);
  Node* flags = Parameter(Descriptor::kFlags);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kCreateObjectLiteral, context, feedback_vector,
                  SmiTag(slot), boilerplate_description, flags);
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
  CSA_ASSERT(this,
             IsClearWord32<Map::ConstructionCounter>(LoadMapBitField3(map)));
  Node* empty_fixed_array = EmptyFixedArrayConstant();
  Node* result =
      AllocateJSObjectFromMap(map, empty_fixed_array, empty_fixed_array);
  return result;
}

TF_BUILTIN(ObjectConstructor, ConstructorBuiltinsAssembler) {
  int const kValueArg = 0;
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* value = args.GetOptionalArgumentValue(kValueArg);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  CSA_ASSERT(this, IsUndefined(Parameter(BuiltinDescriptor::kNewTarget)));

  Label return_to_object(this);

  GotoIf(Word32And(IsNotUndefined(value), IsNotNull(value)), &return_to_object);

  args.PopAndReturn(EmitCreateEmptyObjectLiteral(context));

  BIND(&return_to_object);
  args.PopAndReturn(CallBuiltin(Builtins::kToObject, context, value));
}

TF_BUILTIN(ObjectConstructor_ConstructStub, ConstructorBuiltinsAssembler) {
  int const kValueArg = 0;
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* value = args.GetOptionalArgumentValue(kValueArg);

  Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                               MachineType::TaggedPointer());
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  CSA_ASSERT(this, IsNotUndefined(new_target));

  Label return_to_object(this);

  GotoIf(Word32And(WordEqual(target, new_target),
                   Word32And(IsNotUndefined(value), IsNotNull(value))),
         &return_to_object);
  args.PopAndReturn(EmitFastNewObject(context, target, new_target));

  BIND(&return_to_object);
  args.PopAndReturn(CallBuiltin(Builtins::kToObject, context, value));
}

TF_BUILTIN(NumberConstructor, ConstructorBuiltinsAssembler) {
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Label return_zero(this);

  GotoIf(IntPtrEqual(IntPtrConstant(0), argc), &return_zero);

  Node* context = Parameter(BuiltinDescriptor::kContext);
  args.PopAndReturn(
      ToNumber(context, args.AtIndex(0), BigIntHandling::kConvertToNumber));

  BIND(&return_zero);
  args.PopAndReturn(SmiConstant(0));
}

TF_BUILTIN(NumberConstructor_ConstructStub, ConstructorBuiltinsAssembler) {
  Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                               MachineType::TaggedPointer());
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Label wrap(this);

  VARIABLE(var_result, MachineRepresentation::kTagged, SmiConstant(0));

  GotoIf(IntPtrEqual(IntPtrConstant(0), argc), &wrap);
  var_result.Bind(
      ToNumber(context, args.AtIndex(0), BigIntHandling::kConvertToNumber));
  Goto(&wrap);

  BIND(&wrap);
  Node* result = EmitFastNewObject(context, target, new_target);
  StoreObjectField(result, JSValue::kValueOffset, var_result.value());
  args.PopAndReturn(result);
}

Node* ConstructorBuiltinsAssembler::EmitConstructString(Node* argc,
                                                        CodeStubArguments& args,
                                                        Node* context,
                                                        bool convert_symbol) {
  VARIABLE(var_result, MachineRepresentation::kTagged);

  Label return_empty_string(this), to_string(this),
      check_symbol(this, Label::kDeferred), done(this);

  GotoIf(IntPtrEqual(IntPtrConstant(0), argc), &return_empty_string);

  Node* argument = args.AtIndex(0);

  GotoIf(TaggedIsSmi(argument), &to_string);

  Node* instance_type = LoadInstanceType(argument);

  Label* non_string = convert_symbol ? &check_symbol : &to_string;
  GotoIfNot(IsStringInstanceType(instance_type), non_string);
  {
    var_result.Bind(argument);
    Goto(&done);
  }

  if (convert_symbol) {
    BIND(&check_symbol);
    GotoIfNot(IsSymbolInstanceType(instance_type), &to_string);
    {
      var_result.Bind(
          CallRuntime(Runtime::kSymbolDescriptiveString, context, argument));
      Goto(&done);
    }
  }

  BIND(&to_string);
  {
    var_result.Bind(ToString(context, argument));
    Goto(&done);
  }

  BIND(&return_empty_string);
  {
    var_result.Bind(EmptyStringConstant());
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

TF_BUILTIN(StringConstructor, ConstructorBuiltinsAssembler) {
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  args.PopAndReturn(EmitConstructString(argc, args, context, true));
}

TF_BUILTIN(StringConstructor_ConstructStub, ConstructorBuiltinsAssembler) {
  Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                               MachineType::TaggedPointer());
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* string = EmitConstructString(argc, args, context, false);
  Node* result = EmitFastNewObject(context, target, new_target);
  StoreObjectField(result, JSValue::kValueOffset, string);
  args.PopAndReturn(result);
}

}  // namespace internal
}  // namespace v8

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

Node* ConstructorBuiltinsAssembler::CopyFixedArrayBase(Node* fixed_array) {
  Label if_fixed_array(this), if_fixed_double_array(this), done(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  Node* capacity = LoadAndUntagFixedArrayBaseLength(fixed_array);
  Branch(IsFixedDoubleArrayMap(LoadMap(fixed_array)), &if_fixed_double_array,
         &if_fixed_array);
  BIND(&if_fixed_double_array);
  {
    ElementsKind kind = PACKED_DOUBLE_ELEMENTS;
    Node* copy = AllocateFixedArray(kind, capacity);
    CopyFixedArrayElements(kind, fixed_array, kind, copy, capacity, capacity,
                           SKIP_WRITE_BARRIER);
    result.Bind(copy);
    Goto(&done);
  }

  BIND(&if_fixed_array);
  {
    ElementsKind kind = PACKED_ELEMENTS;
    Node* copy = AllocateFixedArray(kind, capacity);
    CopyFixedArrayElements(kind, fixed_array, kind, copy, capacity, capacity,
                           UPDATE_WRITE_BARRIER);
    result.Bind(copy);
    Goto(&done);
  }
  BIND(&done);
  // Manually copy over the map of the incoming array to preserve the elements
  // kind.
  StoreMap(result.value(), LoadMap(fixed_array));
  return result.value();
}

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
      TimesPointerSize(LoadMapInstanceSize(function_map));
  Node* result = Allocate(instance_size_in_bytes);
  StoreMapNoWriteBarrier(result, function_map);
  InitializeJSObjectBody(result, function_map, instance_size_in_bytes,
                         JSFunction::kSize);

  // Initialize the rest of the function.
  Node* empty_fixed_array = HeapConstant(factory->empty_fixed_array());
  StoreObjectFieldNoWriteBarrier(result, JSObject::kPropertiesOrHashOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(result, JSObject::kElementsOffset,
                                 empty_fixed_array);
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
  {
    // If the feedback vector has optimized code, check whether it is marked
    // for deopt and, if so, clear the slot.
    Label optimized_code_ok(this), clear_optimized_code(this);
    Node* literals = LoadObjectField(literals_cell, Cell::kValueOffset);
    GotoIfNot(IsFeedbackVector(literals), &optimized_code_ok);
    Node* optimized_code_cell_slot =
        LoadObjectField(literals, FeedbackVector::kOptimizedCodeOffset);
    GotoIf(TaggedIsSmi(optimized_code_cell_slot), &optimized_code_ok);

    Node* optimized_code =
        LoadWeakCellValue(optimized_code_cell_slot, &clear_optimized_code);
    Node* code_flags = LoadObjectField(
        optimized_code, Code::kKindSpecificFlags1Offset, MachineType::Uint32());
    Node* marked_for_deopt =
        DecodeWord32<Code::MarkedForDeoptimizationField>(code_flags);
    Branch(Word32Equal(marked_for_deopt, Int32Constant(0)), &optimized_code_ok,
           &clear_optimized_code);

    // Cell is empty or code is marked for deopt, clear the optimized code slot.
    BIND(&clear_optimized_code);
    StoreObjectFieldNoWriteBarrier(
        literals, FeedbackVector::kOptimizedCodeOffset, SmiConstant(0));
    Goto(&optimized_code_ok);

    BIND(&optimized_code_ok);
  }
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kFeedbackVectorOffset,
                                 literals_cell);
  StoreObjectFieldNoWriteBarrier(
      result, JSFunction::kPrototypeOrInitialMapOffset, TheHoleConstant());
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kSharedFunctionInfoOffset,
                                 shared_info);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kContextOffset, context);
  Handle<Code> lazy_builtin_handle(
      isolate->builtins()->builtin(Builtins::kCompileLazy));
  Node* lazy_builtin = HeapConstant(lazy_builtin_handle);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kCodeOffset, lazy_builtin);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kNextFunctionLinkOffset,
                                 UndefinedConstant());

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

  Node* object = AllocateJSObjectFromMap(initial_map, properties.value());

  // Perform in-object slack tracking if requested.
  HandleSlackTracking(context, object, initial_map, JSObject::kHeaderSize);
  return object;
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

Node* ConstructorBuiltinsAssembler::EmitFastCloneRegExp(Node* closure,
                                                        Node* literal_index,
                                                        Node* pattern,
                                                        Node* flags,
                                                        Node* context) {
  Label call_runtime(this, Label::kDeferred), end(this);

  VARIABLE(result, MachineRepresentation::kTagged);
  Node* feedback_vector = LoadFeedbackVector(closure);
  Node* literal_site =
      LoadFeedbackVectorSlot(feedback_vector, literal_index, 0, SMI_PARAMETERS);
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
    result.Bind(CallRuntime(Runtime::kCreateRegExpLiteral, context, closure,
                            literal_index, pattern, flags));
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

TF_BUILTIN(FastCloneRegExp, ConstructorBuiltinsAssembler) {
  Node* closure = Parameter(FastCloneRegExpDescriptor::kClosure);
  Node* literal_index = Parameter(FastCloneRegExpDescriptor::kLiteralIndex);
  Node* pattern = Parameter(FastCloneRegExpDescriptor::kPattern);
  Node* flags = Parameter(FastCloneRegExpDescriptor::kFlags);
  Node* context = Parameter(FastCloneRegExpDescriptor::kContext);

  Return(EmitFastCloneRegExp(closure, literal_index, pattern, flags, context));
}

Node* ConstructorBuiltinsAssembler::NonEmptyShallowClone(
    Node* boilerplate, Node* boilerplate_map, Node* boilerplate_elements,
    Node* allocation_site, Node* capacity, ElementsKind kind) {
  ParameterMode param_mode = OptimalParameterMode();

  Node* length = LoadJSArrayLength(boilerplate);
  capacity = TaggedToParameter(capacity, param_mode);

  Node *array, *elements;
  std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
      kind, boilerplate_map, length, allocation_site, capacity, param_mode);

  length = TaggedToParameter(length, param_mode);

  Comment("copy boilerplate elements");
  CopyFixedArrayElements(kind, boilerplate_elements, elements, length,
                         SKIP_WRITE_BARRIER, param_mode);
  IncrementCounter(isolate()->counters()->inlined_copied_elements(), 1);

  return array;
}

Node* ConstructorBuiltinsAssembler::EmitFastCloneShallowArray(
    Node* closure, Node* literal_index, Node* context, Label* call_runtime,
    AllocationSiteMode allocation_site_mode) {
  Label zero_capacity(this), cow_elements(this), fast_elements(this),
      return_result(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  Node* feedback_vector = LoadFeedbackVector(closure);
  Node* allocation_site =
      LoadFeedbackVectorSlot(feedback_vector, literal_index, 0, SMI_PARAMETERS);
  GotoIf(NotHasBoilerplate(allocation_site), call_runtime);

  Node* boilerplate = LoadAllocationSiteBoilerplate(allocation_site);
  Node* boilerplate_map = LoadMap(boilerplate);
  CSA_ASSERT(this, IsJSArrayMap(boilerplate_map));
  Node* boilerplate_elements = LoadElements(boilerplate);
  Node* capacity = LoadFixedArrayBaseLength(boilerplate_elements);
  allocation_site =
      allocation_site_mode == TRACK_ALLOCATION_SITE ? allocation_site : nullptr;

  Node* zero = SmiConstant(0);
  GotoIf(SmiEqual(capacity, zero), &zero_capacity);

  Node* elements_map = LoadMap(boilerplate_elements);
  GotoIf(IsFixedCOWArrayMap(elements_map), &cow_elements);

  GotoIf(IsFixedArrayMap(elements_map), &fast_elements);
  {
    Comment("fast double elements path");
    if (FLAG_debug_code) CSA_CHECK(this, IsFixedDoubleArrayMap(elements_map));
    Node* array =
        NonEmptyShallowClone(boilerplate, boilerplate_map, boilerplate_elements,
                             allocation_site, capacity, PACKED_DOUBLE_ELEMENTS);
    result.Bind(array);
    Goto(&return_result);
  }

  BIND(&fast_elements);
  {
    Comment("fast elements path");
    Node* array =
        NonEmptyShallowClone(boilerplate, boilerplate_map, boilerplate_elements,
                             allocation_site, capacity, PACKED_ELEMENTS);
    result.Bind(array);
    Goto(&return_result);
  }

  VARIABLE(length, MachineRepresentation::kTagged);
  VARIABLE(elements, MachineRepresentation::kTagged);
  Label allocate_without_elements(this);

  BIND(&cow_elements);
  {
    Comment("fixed cow path");
    length.Bind(LoadJSArrayLength(boilerplate));
    elements.Bind(boilerplate_elements);

    Goto(&allocate_without_elements);
  }

  BIND(&zero_capacity);
  {
    Comment("zero capacity path");
    length.Bind(zero);
    elements.Bind(LoadRoot(Heap::kEmptyFixedArrayRootIndex));

    Goto(&allocate_without_elements);
  }

  BIND(&allocate_without_elements);
  {
    Node* array = AllocateUninitializedJSArrayWithoutElements(
        PACKED_ELEMENTS, boilerplate_map, length.value(), allocation_site);
    StoreObjectField(array, JSObject::kElementsOffset, elements.value());
    result.Bind(array);
    Goto(&return_result);
  }

  BIND(&return_result);
  return result.value();
}

void ConstructorBuiltinsAssembler::CreateFastCloneShallowArrayBuiltin(
    AllocationSiteMode allocation_site_mode) {
  Node* closure = Parameter(FastCloneShallowArrayDescriptor::kClosure);
  Node* literal_index =
      Parameter(FastCloneShallowArrayDescriptor::kLiteralIndex);
  Node* constant_elements =
      Parameter(FastCloneShallowArrayDescriptor::kConstantElements);
  Node* context = Parameter(FastCloneShallowArrayDescriptor::kContext);
  Label call_runtime(this, Label::kDeferred);
  Return(EmitFastCloneShallowArray(closure, literal_index, context,
                                   &call_runtime, allocation_site_mode));

  BIND(&call_runtime);
  {
    Comment("call runtime");
    int flags = AggregateLiteral::kIsShallow;
    if (allocation_site_mode == TRACK_ALLOCATION_SITE) {
      // Force initial allocation sites on the initial literal setup step.
      flags |= AggregateLiteral::kNeedsInitialAllocationSite;
    } else {
      flags |= AggregateLiteral::kDisableMementos;
    }
    Return(CallRuntime(Runtime::kCreateArrayLiteral, context, closure,
                       literal_index, constant_elements, SmiConstant(flags)));
  }
}

TF_BUILTIN(FastCloneShallowArrayTrack, ConstructorBuiltinsAssembler) {
  CreateFastCloneShallowArrayBuiltin(TRACK_ALLOCATION_SITE);
}

TF_BUILTIN(FastCloneShallowArrayDontTrack, ConstructorBuiltinsAssembler) {
  CreateFastCloneShallowArrayBuiltin(DONT_TRACK_ALLOCATION_SITE);
}

Node* ConstructorBuiltinsAssembler::EmitCreateEmptyArrayLiteral(
    Node* closure, Node* literal_index, Node* context) {
  // Array literals always have a valid AllocationSite to properly track
  // elements transitions.
  Node* feedback_vector = LoadFeedbackVector(closure);
  VARIABLE(allocation_site, MachineRepresentation::kTagged,
           LoadFeedbackVectorSlot(feedback_vector, literal_index, 0,
                                  SMI_PARAMETERS));

  Label create_empty_array(this),
      initialize_allocation_site(this, Label::kDeferred), done(this);
  Branch(TaggedIsSmi(allocation_site.value()), &initialize_allocation_site,
         &create_empty_array);

  // TODO(cbruni): create the AllocationSite in CSA.
  BIND(&initialize_allocation_site);
  {
    allocation_site.Bind(
        CreateAllocationSiteInFeedbackVector(feedback_vector, literal_index));
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
  Node* closure = Parameter(Descriptor::kClosure);
  Node* literal_index = Parameter(Descriptor::kLiteralIndex);
  Node* context = Parameter(Descriptor::kContext);
  Node* result = EmitCreateEmptyArrayLiteral(closure, literal_index, context);
  Return(result);
}

Node* ConstructorBuiltinsAssembler::EmitFastCloneShallowObject(
    Label* call_runtime, Node* closure, Node* literals_index) {
  Node* feedback_vector = LoadFeedbackVector(closure);
  Node* allocation_site = LoadFeedbackVectorSlot(
      feedback_vector, literals_index, 0, SMI_PARAMETERS);
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
    var_elements.Bind(CopyFixedArrayBase(boilerplate_elements));
    Goto(&done);
    BIND(&done);
  }

  // Ensure new-space allocation for a fresh JSObject so we can skip write
  // barriers when copying all object fields.
  STATIC_ASSERT(JSObject::kMaxInstanceSize < kMaxRegularHeapObjectSize);
  Node* instance_size = TimesPointerSize(LoadMapInstanceSize(boilerplate_map));
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

TF_BUILTIN(FastCloneShallowObject, ConstructorBuiltinsAssembler) {
  Label call_runtime(this);
  Node* closure = Parameter(Descriptor::kClosure);
  Node* literals_index = Parameter(Descriptor::kLiteralIndex);
  Node* copy =
      EmitFastCloneShallowObject(&call_runtime, closure, literals_index);
  Return(copy);

  BIND(&call_runtime);
  Node* boilerplate_description =
      Parameter(Descriptor::kBoilerplateDescription);
  Node* flags = Parameter(Descriptor::kFlags);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kCreateObjectLiteral, context, closure,
                  literals_index, boilerplate_description, flags);
}

// Used by the CreateEmptyObjectLiteral stub and bytecode.
Node* ConstructorBuiltinsAssembler::EmitCreateEmptyObjectLiteral(
    Node* context) {
  Node* native_context = LoadNativeContext(context);
  Node* object_function =
      LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX);
  Node* map = LoadObjectField(object_function,
                              JSFunction::kPrototypeOrInitialMapOffset);
  CSA_ASSERT(this, IsMap(map));
  Node* empty_fixed_array = EmptyFixedArrayConstant();
  Node* result =
      AllocateJSObjectFromMap(map, empty_fixed_array, empty_fixed_array);
  HandleSlackTracking(context, result, map, JSObject::kHeaderSize);
  return result;
}

TF_BUILTIN(CreateEmptyObjectLiteral, ConstructorBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* result = EmitCreateEmptyObjectLiteral(context);
  Return(result);
}
}  // namespace internal
}  // namespace v8

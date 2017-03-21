// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-constructor.h"
#include "src/ast/ast.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;

Node* ConstructorBuiltinsAssembler::EmitFastNewClosure(Node* shared_info,
                                                       Node* feedback_vector,
                                                       Node* slot,
                                                       Node* context) {
  typedef compiler::CodeAssembler::Label Label;
  typedef compiler::CodeAssembler::Variable Variable;

  Isolate* isolate = this->isolate();
  Factory* factory = isolate->factory();
  IncrementCounter(isolate->counters()->fast_new_closure_total(), 1);

  // Create a new closure from the given function info in new space
  Node* result = Allocate(JSFunction::kSize);

  // Calculate the index of the map we should install on the function based on
  // the FunctionKind and LanguageMode of the function.
  // Note: Must be kept in sync with Context::FunctionMapIndex
  Node* compiler_hints =
      LoadObjectField(shared_info, SharedFunctionInfo::kCompilerHintsOffset,
                      MachineType::Uint32());
  Node* is_strict = Word32And(
      compiler_hints, Int32Constant(1 << SharedFunctionInfo::kStrictModeBit));

  Label if_normal(this), if_generator(this), if_async(this),
      if_class_constructor(this), if_function_without_prototype(this),
      load_map(this);
  Variable map_index(this, MachineType::PointerRepresentation());

  STATIC_ASSERT(FunctionKind::kNormalFunction == 0);
  Node* is_not_normal =
      Word32And(compiler_hints,
                Int32Constant(SharedFunctionInfo::kAllFunctionKindBitsMask));
  GotoUnless(is_not_normal, &if_normal);

  Node* is_generator = Word32And(
      compiler_hints, Int32Constant(FunctionKind::kGeneratorFunction
                                    << SharedFunctionInfo::kFunctionKindShift));
  GotoIf(is_generator, &if_generator);

  Node* is_async = Word32And(
      compiler_hints, Int32Constant(FunctionKind::kAsyncFunction
                                    << SharedFunctionInfo::kFunctionKindShift));
  GotoIf(is_async, &if_async);

  Node* is_class_constructor = Word32And(
      compiler_hints, Int32Constant(FunctionKind::kClassConstructor
                                    << SharedFunctionInfo::kFunctionKindShift));
  GotoIf(is_class_constructor, &if_class_constructor);

  if (FLAG_debug_code) {
    // Function must be a function without a prototype.
    CSA_ASSERT(
        this,
        Word32And(compiler_hints,
                  Int32Constant((FunctionKind::kAccessorFunction |
                                 FunctionKind::kArrowFunction |
                                 FunctionKind::kConciseMethod)
                                << SharedFunctionInfo::kFunctionKindShift)));
  }
  Goto(&if_function_without_prototype);

  Bind(&if_normal);
  {
    map_index.Bind(SelectIntPtrConstant(is_strict,
                                        Context::STRICT_FUNCTION_MAP_INDEX,
                                        Context::SLOPPY_FUNCTION_MAP_INDEX));
    Goto(&load_map);
  }

  Bind(&if_generator);
  {
    map_index.Bind(IntPtrConstant(Context::GENERATOR_FUNCTION_MAP_INDEX));
    Goto(&load_map);
  }

  Bind(&if_async);
  {
    map_index.Bind(IntPtrConstant(Context::ASYNC_FUNCTION_MAP_INDEX));
    Goto(&load_map);
  }

  Bind(&if_class_constructor);
  {
    map_index.Bind(IntPtrConstant(Context::CLASS_FUNCTION_MAP_INDEX));
    Goto(&load_map);
  }

  Bind(&if_function_without_prototype);
  {
    map_index.Bind(
        IntPtrConstant(Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
    Goto(&load_map);
  }

  Bind(&load_map);

  // Get the function map in the current native context and set that
  // as the map of the allocated object.
  Node* native_context = LoadNativeContext(context);
  Node* map_slot_value =
      LoadFixedArrayElement(native_context, map_index.value());
  StoreMapNoWriteBarrier(result, map_slot_value);

  // Initialize the rest of the function.
  Node* empty_fixed_array = HeapConstant(factory->empty_fixed_array());
  Node* empty_literals_array = HeapConstant(factory->empty_literals_array());
  StoreObjectFieldNoWriteBarrier(result, JSObject::kPropertiesOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(result, JSObject::kElementsOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kLiteralsOffset,
                                 empty_literals_array);
  StoreObjectFieldNoWriteBarrier(
      result, JSFunction::kPrototypeOrInitialMapOffset, TheHoleConstant());
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kSharedFunctionInfoOffset,
                                 shared_info);
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kContextOffset, context);
  Handle<Code> lazy_builtin_handle(
      isolate->builtins()->builtin(Builtins::kCompileLazy));
  Node* lazy_builtin = HeapConstant(lazy_builtin_handle);
  Node* lazy_builtin_entry =
      IntPtrAdd(BitcastTaggedToWord(lazy_builtin),
                IntPtrConstant(Code::kHeaderSize - kHeapObjectTag));
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kCodeEntryOffset,
                                 lazy_builtin_entry,
                                 MachineType::PointerRepresentation());
  StoreObjectFieldNoWriteBarrier(result, JSFunction::kNextFunctionLinkOffset,
                                 UndefinedConstant());

  return result;
}

TF_BUILTIN(FastNewClosure, ConstructorBuiltinsAssembler) {
  Node* shared = Parameter(FastNewClosureDescriptor::kSharedFunctionInfo);
  Node* context = Parameter(FastNewClosureDescriptor::kContext);
  Node* vector = Parameter(FastNewClosureDescriptor::kVector);
  Node* slot = Parameter(FastNewClosureDescriptor::kSlot);
  Return(EmitFastNewClosure(shared, vector, slot, context));
}

TF_BUILTIN(FastNewObject, ConstructorBuiltinsAssembler) {
  typedef FastNewObjectDescriptor Descriptor;
  Node* context = Parameter(Descriptor::kContext);
  Node* target = Parameter(Descriptor::kTarget);
  Node* new_target = Parameter(Descriptor::kNewTarget);

  Label call_runtime(this);

  Node* result = EmitFastNewObject(context, target, new_target, &call_runtime);
  Return(result);

  Bind(&call_runtime);
  TailCallRuntime(Runtime::kNewObject, context, target, new_target);
}

Node* ConstructorBuiltinsAssembler::EmitFastNewObject(Node* context,
                                                      Node* target,
                                                      Node* new_target) {
  Variable var_obj(this, MachineRepresentation::kTagged);
  Label call_runtime(this), end(this);

  Node* result = EmitFastNewObject(context, target, new_target, &call_runtime);
  var_obj.Bind(result);
  Goto(&end);

  Bind(&call_runtime);
  var_obj.Bind(CallRuntime(Runtime::kNewObject, context, target, new_target));
  Goto(&end);

  Bind(&end);
  return var_obj.value();
}

Node* ConstructorBuiltinsAssembler::EmitFastNewObject(
    Node* context, Node* target, Node* new_target,
    CodeAssemblerLabel* call_runtime) {
  CSA_ASSERT(this, HasInstanceType(target, JS_FUNCTION_TYPE));
  CSA_ASSERT(this, IsJSReceiver(new_target));

  // Verify that the new target is a JSFunction.
  Label fast(this), end(this);
  GotoIf(HasInstanceType(new_target, JS_FUNCTION_TYPE), &fast);
  Goto(call_runtime);

  Bind(&fast);

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

  Node* instance_size_words = ChangeUint32ToWord(LoadObjectField(
      initial_map, Map::kInstanceSizeOffset, MachineType::Uint8()));
  Node* instance_size =
      WordShl(instance_size_words, IntPtrConstant(kPointerSizeLog2));

  Node* object = Allocate(instance_size);
  StoreMapNoWriteBarrier(object, initial_map);
  Node* empty_array = LoadRoot(Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(object, JSObject::kPropertiesOffset,
                                 empty_array);
  StoreObjectFieldNoWriteBarrier(object, JSObject::kElementsOffset,
                                 empty_array);

  instance_size_words = ChangeUint32ToWord(LoadObjectField(
      initial_map, Map::kInstanceSizeOffset, MachineType::Uint8()));
  instance_size =
      WordShl(instance_size_words, IntPtrConstant(kPointerSizeLog2));

  // Perform in-object slack tracking if requested.
  Node* bit_field3 = LoadMapBitField3(initial_map);
  Label slack_tracking(this), finalize(this, Label::kDeferred), done(this);
  GotoIf(IsSetWord32<Map::ConstructionCounter>(bit_field3), &slack_tracking);

  // Initialize remaining fields.
  {
    Comment("no slack tracking");
    InitializeFieldsWithRoot(object, IntPtrConstant(JSObject::kHeaderSize),
                             instance_size, Heap::kUndefinedValueRootIndex);
    Goto(&end);
  }

  {
    Bind(&slack_tracking);

    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    Comment("update allocation count");
    Node* new_bit_field3 = Int32Sub(
        bit_field3, Int32Constant(1 << Map::ConstructionCounter::kShift));
    StoreObjectFieldNoWriteBarrier(initial_map, Map::kBitField3Offset,
                                   new_bit_field3,
                                   MachineRepresentation::kWord32);
    GotoIf(IsClearWord32<Map::ConstructionCounter>(new_bit_field3), &finalize);

    Node* unused_fields = LoadObjectField(
        initial_map, Map::kUnusedPropertyFieldsOffset, MachineType::Uint8());
    Node* used_size =
        IntPtrSub(instance_size, WordShl(ChangeUint32ToWord(unused_fields),
                                         IntPtrConstant(kPointerSizeLog2)));

    Comment("initialize filler fields (no finalize)");
    InitializeFieldsWithRoot(object, used_size, instance_size,
                             Heap::kOnePointerFillerMapRootIndex);

    Comment("initialize undefined fields (no finalize)");
    InitializeFieldsWithRoot(object, IntPtrConstant(JSObject::kHeaderSize),
                             used_size, Heap::kUndefinedValueRootIndex);
    Goto(&end);
  }

  {
    // Finalize the instance size.
    Bind(&finalize);

    Node* unused_fields = LoadObjectField(
        initial_map, Map::kUnusedPropertyFieldsOffset, MachineType::Uint8());
    Node* used_size =
        IntPtrSub(instance_size, WordShl(ChangeUint32ToWord(unused_fields),
                                         IntPtrConstant(kPointerSizeLog2)));

    Comment("initialize filler fields (finalize)");
    InitializeFieldsWithRoot(object, used_size, instance_size,
                             Heap::kOnePointerFillerMapRootIndex);

    Comment("initialize undefined fields (finalize)");
    InitializeFieldsWithRoot(object, IntPtrConstant(JSObject::kHeaderSize),
                             used_size, Heap::kUndefinedValueRootIndex);

    CallRuntime(Runtime::kFinalizeInstanceSize, context, initial_map);
    Goto(&end);
  }

  Bind(&end);
  return object;
}

Node* ConstructorBuiltinsAssembler::EmitFastNewFunctionContext(
    Node* function, Node* slots, Node* context, ScopeType scope_type) {
  slots = ChangeUint32ToWord(slots);

  // TODO(ishell): Use CSA::OptimalParameterMode() here.
  CodeStubAssembler::ParameterMode mode = CodeStubAssembler::INTPTR_PARAMETERS;
  Node* min_context_slots = IntPtrConstant(Context::MIN_CONTEXT_SLOTS);
  Node* length = IntPtrAdd(slots, min_context_slots);
  Node* size = GetFixedArrayAllocationSize(length, FAST_ELEMENTS, mode);

  // Create a new closure from the given function info in new space
  Node* function_context = Allocate(size);

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
      function_context, FAST_ELEMENTS, min_context_slots, length,
      [this, undefined](Node* context, Node* offset) {
        StoreNoWriteBarrier(MachineRepresentation::kTagged, context, offset,
                            undefined);
      },
      mode);

  return function_context;
}

// static
int ConstructorBuiltinsAssembler::MaximumFunctionContextSlots() {
  return FLAG_test_small_max_function_context_stub_size ? kSmallMaximumSlots
                                                        : kMaximumSlots;
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

Handle<Code> Builtins::NewFunctionContext(ScopeType scope_type) {
  switch (scope_type) {
    case ScopeType::EVAL_SCOPE:
      return FastNewFunctionContextEval();
    case ScopeType::FUNCTION_SCOPE:
      return FastNewFunctionContextFunction();
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

Node* ConstructorBuiltinsAssembler::EmitFastCloneRegExp(Node* closure,
                                                        Node* literal_index,
                                                        Node* pattern,
                                                        Node* flags,
                                                        Node* context) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  typedef compiler::Node Node;

  Label call_runtime(this, Label::kDeferred), end(this);

  Variable result(this, MachineRepresentation::kTagged);

  Node* literals_array = LoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* boilerplate =
      LoadFixedArrayElement(literals_array, literal_index,
                            LiteralsArray::kFirstLiteralIndex * kPointerSize,
                            CodeStubAssembler::SMI_PARAMETERS);
  GotoIf(IsUndefined(boilerplate), &call_runtime);

  {
    int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
    Node* copy = Allocate(size);
    for (int offset = 0; offset < size; offset += kPointerSize) {
      Node* value = LoadObjectField(boilerplate, offset);
      StoreObjectFieldNoWriteBarrier(copy, offset, value);
    }
    result.Bind(copy);
    Goto(&end);
  }

  Bind(&call_runtime);
  {
    result.Bind(CallRuntime(Runtime::kCreateRegExpLiteral, context, closure,
                            literal_index, pattern, flags));
    Goto(&end);
  }

  Bind(&end);
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
  typedef CodeStubAssembler::ParameterMode ParameterMode;

  ParameterMode param_mode = OptimalParameterMode();

  Node* length = LoadJSArrayLength(boilerplate);
  capacity = TaggedToParameter(capacity, param_mode);

  Node *array, *elements;
  std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
      kind, boilerplate_map, length, allocation_site, capacity, param_mode);

  Comment("copy elements header");
  // Header consists of map and length.
  STATIC_ASSERT(FixedArrayBase::kHeaderSize == 2 * kPointerSize);
  StoreMap(elements, LoadMap(boilerplate_elements));
  {
    int offset = FixedArrayBase::kLengthOffset;
    StoreObjectFieldNoWriteBarrier(
        elements, offset, LoadObjectField(boilerplate_elements, offset));
  }

  length = TaggedToParameter(length, param_mode);

  Comment("copy boilerplate elements");
  CopyFixedArrayElements(kind, boilerplate_elements, elements, length,
                         SKIP_WRITE_BARRIER, param_mode);
  IncrementCounter(isolate()->counters()->inlined_copied_elements(), 1);

  return array;
}

Node* ConstructorBuiltinsAssembler::EmitFastCloneShallowArray(
    Node* closure, Node* literal_index, Node* context,
    CodeAssemblerLabel* call_runtime, AllocationSiteMode allocation_site_mode) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  typedef compiler::Node Node;

  Label zero_capacity(this), cow_elements(this), fast_elements(this),
      return_result(this);
  Variable result(this, MachineRepresentation::kTagged);

  Node* literals_array = LoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* allocation_site =
      LoadFixedArrayElement(literals_array, literal_index,
                            LiteralsArray::kFirstLiteralIndex * kPointerSize,
                            CodeStubAssembler::SMI_PARAMETERS);

  GotoIf(IsUndefined(allocation_site), call_runtime);
  allocation_site =
      LoadFixedArrayElement(literals_array, literal_index,
                            LiteralsArray::kFirstLiteralIndex * kPointerSize,
                            CodeStubAssembler::SMI_PARAMETERS);

  Node* boilerplate =
      LoadObjectField(allocation_site, AllocationSite::kTransitionInfoOffset);
  Node* boilerplate_map = LoadMap(boilerplate);
  Node* boilerplate_elements = LoadElements(boilerplate);
  Node* capacity = LoadFixedArrayBaseLength(boilerplate_elements);
  allocation_site =
      allocation_site_mode == TRACK_ALLOCATION_SITE ? allocation_site : nullptr;

  Node* zero = SmiConstant(Smi::kZero);
  GotoIf(SmiEqual(capacity, zero), &zero_capacity);

  Node* elements_map = LoadMap(boilerplate_elements);
  GotoIf(IsFixedCOWArrayMap(elements_map), &cow_elements);

  GotoIf(IsFixedArrayMap(elements_map), &fast_elements);
  {
    Comment("fast double elements path");
    if (FLAG_debug_code) {
      Label correct_elements_map(this), abort(this, Label::kDeferred);
      Branch(IsFixedDoubleArrayMap(elements_map), &correct_elements_map,
             &abort);

      Bind(&abort);
      {
        Node* abort_id = SmiConstant(
            Smi::FromInt(BailoutReason::kExpectedFixedDoubleArrayMap));
        CallRuntime(Runtime::kAbort, context, abort_id);
        result.Bind(UndefinedConstant());
        Goto(&return_result);
      }
      Bind(&correct_elements_map);
    }

    Node* array =
        NonEmptyShallowClone(boilerplate, boilerplate_map, boilerplate_elements,
                             allocation_site, capacity, FAST_DOUBLE_ELEMENTS);
    result.Bind(array);
    Goto(&return_result);
  }

  Bind(&fast_elements);
  {
    Comment("fast elements path");
    Node* array =
        NonEmptyShallowClone(boilerplate, boilerplate_map, boilerplate_elements,
                             allocation_site, capacity, FAST_ELEMENTS);
    result.Bind(array);
    Goto(&return_result);
  }

  Variable length(this, MachineRepresentation::kTagged),
      elements(this, MachineRepresentation::kTagged);
  Label allocate_without_elements(this);

  Bind(&cow_elements);
  {
    Comment("fixed cow path");
    length.Bind(LoadJSArrayLength(boilerplate));
    elements.Bind(boilerplate_elements);

    Goto(&allocate_without_elements);
  }

  Bind(&zero_capacity);
  {
    Comment("zero capacity path");
    length.Bind(zero);
    elements.Bind(LoadRoot(Heap::kEmptyFixedArrayRootIndex));

    Goto(&allocate_without_elements);
  }

  Bind(&allocate_without_elements);
  {
    Node* array = AllocateUninitializedJSArrayWithoutElements(
        FAST_ELEMENTS, boilerplate_map, length.value(), allocation_site);
    StoreObjectField(array, JSObject::kElementsOffset, elements.value());
    result.Bind(array);
    Goto(&return_result);
  }

  Bind(&return_result);
  return result.value();
}

void ConstructorBuiltinsAssembler::CreateFastCloneShallowArrayBuiltin(
    AllocationSiteMode allocation_site_mode) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;

  Node* closure = Parameter(FastCloneShallowArrayDescriptor::kClosure);
  Node* literal_index =
      Parameter(FastCloneShallowArrayDescriptor::kLiteralIndex);
  Node* constant_elements =
      Parameter(FastCloneShallowArrayDescriptor::kConstantElements);
  Node* context = Parameter(FastCloneShallowArrayDescriptor::kContext);
  Label call_runtime(this, Label::kDeferred);
  Return(EmitFastCloneShallowArray(closure, literal_index, context,
                                   &call_runtime, allocation_site_mode));

  Bind(&call_runtime);
  {
    Comment("call runtime");
    Node* flags =
        SmiConstant(Smi::FromInt(ArrayLiteral::kShallowElements |
                                 (allocation_site_mode == TRACK_ALLOCATION_SITE
                                      ? 0
                                      : ArrayLiteral::kDisableMementos)));
    Return(CallRuntime(Runtime::kCreateArrayLiteral, context, closure,
                       literal_index, constant_elements, flags));
  }
}

TF_BUILTIN(FastCloneShallowArrayTrack, ConstructorBuiltinsAssembler) {
  CreateFastCloneShallowArrayBuiltin(TRACK_ALLOCATION_SITE);
}

TF_BUILTIN(FastCloneShallowArrayDontTrack, ConstructorBuiltinsAssembler) {
  CreateFastCloneShallowArrayBuiltin(DONT_TRACK_ALLOCATION_SITE);
}

Handle<Code> Builtins::NewCloneShallowArray(
    AllocationSiteMode allocation_mode) {
  switch (allocation_mode) {
    case TRACK_ALLOCATION_SITE:
      return FastCloneShallowArrayTrack();
    case DONT_TRACK_ALLOCATION_SITE:
      return FastCloneShallowArrayDontTrack();
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

// static
int ConstructorBuiltinsAssembler::FastCloneShallowObjectPropertiesCount(
    int literal_length) {
  // This heuristic of setting empty literals to have
  // kInitialGlobalObjectUnusedPropertiesCount must remain in-sync with the
  // runtime.
  // TODO(verwaest): Unify this with the heuristic in the runtime.
  return literal_length == 0
             ? JSObject::kInitialGlobalObjectUnusedPropertiesCount
             : literal_length;
}

Node* ConstructorBuiltinsAssembler::EmitFastCloneShallowObject(
    CodeAssemblerLabel* call_runtime, Node* closure, Node* literals_index,
    Node* properties_count) {
  Node* literals_array = LoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* allocation_site =
      LoadFixedArrayElement(literals_array, literals_index,
                            LiteralsArray::kFirstLiteralIndex * kPointerSize,
                            CodeStubAssembler::SMI_PARAMETERS);
  GotoIf(IsUndefined(allocation_site), call_runtime);

  // Calculate the object and allocation size based on the properties count.
  Node* object_size = IntPtrAdd(WordShl(properties_count, kPointerSizeLog2),
                                IntPtrConstant(JSObject::kHeaderSize));
  Node* allocation_size = object_size;
  if (FLAG_allocation_site_pretenuring) {
    allocation_size =
        IntPtrAdd(object_size, IntPtrConstant(AllocationMemento::kSize));
  }
  Node* boilerplate =
      LoadObjectField(allocation_site, AllocationSite::kTransitionInfoOffset);
  Node* boilerplate_map = LoadMap(boilerplate);
  Node* instance_size = LoadMapInstanceSize(boilerplate_map);
  Node* size_in_words = WordShr(object_size, kPointerSizeLog2);
  GotoUnless(WordEqual(instance_size, size_in_words), call_runtime);

  Node* copy = Allocate(allocation_size);

  // Copy boilerplate elements.
  Variable offset(this, MachineType::PointerRepresentation());
  offset.Bind(IntPtrConstant(-kHeapObjectTag));
  Node* end_offset = IntPtrAdd(object_size, offset.value());
  Label loop_body(this, &offset), loop_check(this, &offset);
  // We should always have an object size greater than zero.
  Goto(&loop_body);
  Bind(&loop_body);
  {
    // The Allocate above guarantees that the copy lies in new space. This
    // allows us to skip write barriers. This is necessary since we may also be
    // copying unboxed doubles.
    Node* field = Load(MachineType::IntPtr(), boilerplate, offset.value());
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), copy,
                        offset.value(), field);
    Goto(&loop_check);
  }
  Bind(&loop_check);
  {
    offset.Bind(IntPtrAdd(offset.value(), IntPtrConstant(kPointerSize)));
    GotoUnless(IntPtrGreaterThanOrEqual(offset.value(), end_offset),
               &loop_body);
  }

  if (FLAG_allocation_site_pretenuring) {
    Node* memento = InnerAllocate(copy, object_size);
    StoreMapNoWriteBarrier(memento, Heap::kAllocationMementoMapRootIndex);
    StoreObjectFieldNoWriteBarrier(
        memento, AllocationMemento::kAllocationSiteOffset, allocation_site);
    Node* memento_create_count = LoadObjectField(
        allocation_site, AllocationSite::kPretenureCreateCountOffset);
    memento_create_count =
        SmiAdd(memento_create_count, SmiConstant(Smi::FromInt(1)));
    StoreObjectFieldNoWriteBarrier(allocation_site,
                                   AllocationSite::kPretenureCreateCountOffset,
                                   memento_create_count);
  }

  // TODO(verwaest): Allocate and fill in double boxes.
  return copy;
}

void ConstructorBuiltinsAssembler::CreateFastCloneShallowObjectBuiltin(
    int properties_count) {
  DCHECK_GE(properties_count, 0);
  DCHECK_LE(properties_count, kMaximumClonedShallowObjectProperties);
  Label call_runtime(this);
  Node* closure = Parameter(0);
  Node* literals_index = Parameter(1);

  Node* properties_count_node =
      IntPtrConstant(FastCloneShallowObjectPropertiesCount(properties_count));
  Node* copy = EmitFastCloneShallowObject(
      &call_runtime, closure, literals_index, properties_count_node);
  Return(copy);

  Bind(&call_runtime);
  Node* constant_properties = Parameter(2);
  Node* flags = Parameter(3);
  Node* context = Parameter(4);
  TailCallRuntime(Runtime::kCreateObjectLiteral, context, closure,
                  literals_index, constant_properties, flags);
}

#define SHALLOW_OBJECT_BUILTIN(props)                                       \
  TF_BUILTIN(FastCloneShallowObject##props, ConstructorBuiltinsAssembler) { \
    CreateFastCloneShallowObjectBuiltin(props);                             \
  }

SHALLOW_OBJECT_BUILTIN(0);
SHALLOW_OBJECT_BUILTIN(1);
SHALLOW_OBJECT_BUILTIN(2);
SHALLOW_OBJECT_BUILTIN(3);
SHALLOW_OBJECT_BUILTIN(4);
SHALLOW_OBJECT_BUILTIN(5);
SHALLOW_OBJECT_BUILTIN(6);

Handle<Code> Builtins::NewCloneShallowObject(int length) {
  switch (length) {
    case 0:
      return FastCloneShallowObject0();
    case 1:
      return FastCloneShallowObject1();
    case 2:
      return FastCloneShallowObject2();
    case 3:
      return FastCloneShallowObject3();
    case 4:
      return FastCloneShallowObject4();
    case 5:
      return FastCloneShallowObject5();
    case 6:
      return FastCloneShallowObject6();
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

}  // namespace internal
}  // namespace v8

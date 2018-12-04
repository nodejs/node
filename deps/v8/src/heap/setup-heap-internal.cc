// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/accessors.h"
#include "src/ast/context-slot-cache.h"
#include "src/compilation-cache.h"
#include "src/contexts.h"
#include "src/heap-symbols.h"
#include "src/heap/factory.h"
#include "src/heap/heap.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/layout-descriptor.h"
#include "src/lookup-cache.h"
#include "src/objects-inl.h"
#include "src/objects/arguments.h"
#include "src/objects/data-handler.h"
#include "src/objects/debug-objects.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/map.h"
#include "src/objects/microtask-queue.h"
#include "src/objects/microtask.h"
#include "src/objects/module.h"
#include "src/objects/promise.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/stack-frame-info.h"
#include "src/objects/string.h"
#include "src/regexp/jsregexp.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

bool SetupIsolateDelegate::SetupHeapInternal(Heap* heap) {
  return heap->CreateHeapObjects();
}

bool Heap::CreateHeapObjects() {
  // Create initial maps.
  if (!CreateInitialMaps()) return false;
  CreateApiObjects();

  // Create initial objects
  CreateInitialObjects();
  CreateInternalAccessorInfoObjects();
  CHECK_EQ(0u, gc_count_);

  set_native_contexts_list(ReadOnlyRoots(this).undefined_value());
  set_allocation_sites_list(ReadOnlyRoots(this).undefined_value());

  return true;
}

const Heap::StringTypeTable Heap::string_type_table[] = {
#define STRING_TYPE_ELEMENT(type, size, name, CamelName) \
  {type, size, RootIndex::k##CamelName##Map},
    STRING_TYPE_LIST(STRING_TYPE_ELEMENT)
#undef STRING_TYPE_ELEMENT
};

const Heap::ConstantStringTable Heap::constant_string_table[] = {
    {"", RootIndex::kempty_string},
#define CONSTANT_STRING_ELEMENT(_, name, contents) \
  {contents, RootIndex::k##name},
    INTERNALIZED_STRING_LIST_GENERATOR(CONSTANT_STRING_ELEMENT, /* not used */)
#undef CONSTANT_STRING_ELEMENT
};

const Heap::StructTable Heap::struct_table[] = {
#define STRUCT_TABLE_ELEMENT(TYPE, Name, name) \
  {TYPE, Name::kSize, RootIndex::k##Name##Map},
    STRUCT_LIST(STRUCT_TABLE_ELEMENT)
#undef STRUCT_TABLE_ELEMENT

#define ALLOCATION_SITE_ELEMENT(_, TYPE, Name, Size, name) \
  {TYPE, Name::kSize##Size, RootIndex::k##Name##Size##Map},
        ALLOCATION_SITE_LIST(ALLOCATION_SITE_ELEMENT, /* not used */)
#undef ALLOCATION_SITE_ELEMENT

#define DATA_HANDLER_ELEMENT(_, TYPE, Name, Size, name) \
  {TYPE, Name::kSizeWithData##Size, RootIndex::k##Name##Size##Map},
            DATA_HANDLER_LIST(DATA_HANDLER_ELEMENT, /* not used */)
#undef DATA_HANDLER_ELEMENT
};

AllocationResult Heap::AllocateMap(InstanceType instance_type,
                                   int instance_size,
                                   ElementsKind elements_kind,
                                   int inobject_properties) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  bool is_js_object = InstanceTypeChecker::IsJSObject(instance_type);
  DCHECK_IMPLIES(is_js_object &&
                     !Map::CanHaveFastTransitionableElementsKind(instance_type),
                 IsDictionaryElementsKind(elements_kind) ||
                     IsTerminalElementsKind(elements_kind));
  HeapObject* result = nullptr;
  // JSObjects have maps with a mutable prototype_validity_cell, so they cannot
  // go in RO_SPACE.
  AllocationResult allocation =
      AllocateRaw(Map::kSize, is_js_object ? MAP_SPACE : RO_SPACE);
  if (!allocation.To(&result)) return allocation;

  result->set_map_after_allocation(ReadOnlyRoots(this).meta_map(),
                                   SKIP_WRITE_BARRIER);
  Map* map = isolate()->factory()->InitializeMap(
      Map::cast(result), instance_type, instance_size, elements_kind,
      inobject_properties);

  return map;
}

AllocationResult Heap::AllocatePartialMap(InstanceType instance_type,
                                          int instance_size) {
  Object* result = nullptr;
  AllocationResult allocation = AllocateRaw(Map::kSize, RO_SPACE);
  if (!allocation.To(&result)) return allocation;
  // Map::cast cannot be used due to uninitialized map field.
  Map* map = reinterpret_cast<Map*>(result);
  map->set_map_after_allocation(
      reinterpret_cast<Map*>(root(RootIndex::kMetaMap)), SKIP_WRITE_BARRIER);
  map->set_instance_type(instance_type);
  map->set_instance_size(instance_size);
  // Initialize to only containing tagged fields.
  if (FLAG_unbox_double_fields) {
    map->set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  // GetVisitorId requires a properly initialized LayoutDescriptor.
  map->set_visitor_id(Map::GetVisitorId(map));
  map->set_inobject_properties_start_or_constructor_function_index(0);
  DCHECK(!map->IsJSObjectMap());
  map->set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid));
  map->SetInObjectUnusedPropertyFields(0);
  map->set_bit_field(0);
  map->set_bit_field2(0);
  DCHECK(!map->is_in_retained_map_list());
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptorsBit::encode(true) |
                   Map::ConstructionCounterBits::encode(Map::kNoSlackTracking);
  map->set_bit_field3(bit_field3);
  map->set_elements_kind(TERMINAL_FAST_ELEMENTS_KIND);
  return map;
}

void Heap::FinalizePartialMap(Map* map) {
  ReadOnlyRoots roots(this);
  map->set_dependent_code(DependentCode::cast(roots.empty_weak_fixed_array()));
  map->set_raw_transitions(MaybeObject::FromSmi(Smi::kZero));
  map->set_instance_descriptors(roots.empty_descriptor_array());
  if (FLAG_unbox_double_fields) {
    map->set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  map->set_prototype(roots.null_value());
  map->set_constructor_or_backpointer(roots.null_value());
}

AllocationResult Heap::Allocate(Map* map, AllocationSpace space) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  HeapObject* result = nullptr;
  AllocationResult allocation = AllocateRaw(size, space);
  if (!allocation.To(&result)) return allocation;
  // New space objects are allocated white.
  WriteBarrierMode write_barrier_mode =
      space == NEW_SPACE ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  result->set_map_after_allocation(map, write_barrier_mode);
  return result;
}

AllocationResult Heap::AllocateEmptyFixedTypedArray(
    ExternalArrayType array_type) {
  int size = OBJECT_POINTER_ALIGN(FixedTypedArrayBase::kDataOffset);

  HeapObject* object = nullptr;
  AllocationResult allocation = AllocateRaw(
      size, RO_SPACE,
      array_type == kExternalFloat64Array ? kDoubleAligned : kWordAligned);
  if (!allocation.To(&object)) return allocation;

  object->set_map_after_allocation(
      ReadOnlyRoots(this).MapForFixedTypedArray(array_type),
      SKIP_WRITE_BARRIER);
  FixedTypedArrayBase* elements = FixedTypedArrayBase::cast(object);
  elements->set_base_pointer(elements, SKIP_WRITE_BARRIER);
  elements->set_external_pointer(
      reinterpret_cast<void*>(
          ExternalReference::fixed_typed_array_base_data_offset().address()),
      SKIP_WRITE_BARRIER);
  elements->set_length(0);
  return elements;
}

bool Heap::CreateInitialMaps() {
  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocatePartialMap(MAP_TYPE, Map::kSize);
    if (!allocation.To(&obj)) return false;
  }
  // Map::cast cannot be used due to uninitialized map field.
  Map* new_meta_map = reinterpret_cast<Map*>(obj);
  set_meta_map(new_meta_map);
  new_meta_map->set_map_after_allocation(new_meta_map);

  ReadOnlyRoots roots(this);
  {  // Partial map allocation
#define ALLOCATE_PARTIAL_MAP(instance_type, size, field_name)                \
  {                                                                          \
    Map* map;                                                                \
    if (!AllocatePartialMap((instance_type), (size)).To(&map)) return false; \
    set_##field_name##_map(map);                                             \
  }

    ALLOCATE_PARTIAL_MAP(FIXED_ARRAY_TYPE, kVariableSizeSentinel, fixed_array);
    ALLOCATE_PARTIAL_MAP(WEAK_FIXED_ARRAY_TYPE, kVariableSizeSentinel,
                         weak_fixed_array);
    ALLOCATE_PARTIAL_MAP(WEAK_ARRAY_LIST_TYPE, kVariableSizeSentinel,
                         weak_array_list);
    ALLOCATE_PARTIAL_MAP(FIXED_ARRAY_TYPE, kVariableSizeSentinel,
                         fixed_cow_array)
    DCHECK_NE(roots.fixed_array_map(), roots.fixed_cow_array_map());

    ALLOCATE_PARTIAL_MAP(DESCRIPTOR_ARRAY_TYPE, kVariableSizeSentinel,
                         descriptor_array)

    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, undefined);
    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, null);
    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, the_hole);

#undef ALLOCATE_PARTIAL_MAP
  }

  // Allocate the empty array.
  {
    AllocationResult alloc = AllocateRaw(FixedArray::SizeFor(0), RO_SPACE);
    if (!alloc.To(&obj)) return false;
    obj->set_map_after_allocation(roots.fixed_array_map(), SKIP_WRITE_BARRIER);
    FixedArray::cast(obj)->set_length(0);
  }
  set_empty_fixed_array(FixedArray::cast(obj));

  {
    AllocationResult alloc = AllocateRaw(WeakFixedArray::SizeFor(0), RO_SPACE);
    if (!alloc.To(&obj)) return false;
    obj->set_map_after_allocation(roots.weak_fixed_array_map(),
                                  SKIP_WRITE_BARRIER);
    WeakFixedArray::cast(obj)->set_length(0);
  }
  set_empty_weak_fixed_array(WeakFixedArray::cast(obj));

  {
    AllocationResult allocation =
        AllocateRaw(WeakArrayList::SizeForCapacity(0), RO_SPACE);
    if (!allocation.To(&obj)) return false;
    obj->set_map_after_allocation(roots.weak_array_list_map(),
                                  SKIP_WRITE_BARRIER);
    WeakArrayList::cast(obj)->set_capacity(0);
    WeakArrayList::cast(obj)->set_length(0);
  }
  set_empty_weak_array_list(WeakArrayList::cast(obj));

  {
    AllocationResult allocation = Allocate(roots.null_map(), RO_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_null_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kNull);

  {
    AllocationResult allocation = Allocate(roots.undefined_map(), RO_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_undefined_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kUndefined);
  DCHECK(!InNewSpace(roots.undefined_value()));
  {
    AllocationResult allocation = Allocate(roots.the_hole_map(), RO_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_the_hole_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kTheHole);

  // Set preliminary exception sentinel value before actually initializing it.
  set_exception(roots.null_value());

  // Setup the struct maps first (needed for the EnumCache).
  for (unsigned i = 0; i < arraysize(struct_table); i++) {
    const StructTable& entry = struct_table[i];
    Map* map;
    if (!AllocatePartialMap(entry.type, entry.size).To(&map)) return false;
    roots_[entry.index] = map;
  }

  // Allocate the empty enum cache.
  {
    AllocationResult allocation = Allocate(roots.tuple2_map(), RO_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_empty_enum_cache(EnumCache::cast(obj));
  EnumCache::cast(obj)->set_keys(roots.empty_fixed_array());
  EnumCache::cast(obj)->set_indices(roots.empty_fixed_array());

  // Allocate the empty descriptor array.
  {
    STATIC_ASSERT(DescriptorArray::kFirstIndex != 0);
    int length = DescriptorArray::kFirstIndex;
    int size = WeakFixedArray::SizeFor(length);
    if (!AllocateRaw(size, RO_SPACE).To(&obj)) return false;
    obj->set_map_after_allocation(roots.descriptor_array_map(),
                                  SKIP_WRITE_BARRIER);
    DescriptorArray::cast(obj)->set_length(length);
  }
  set_empty_descriptor_array(DescriptorArray::cast(obj));
  DescriptorArray::cast(obj)->SetNumberOfDescriptors(0);
  WeakFixedArray::cast(obj)->Set(
      DescriptorArray::kEnumCacheIndex,
      MaybeObject::FromObject(roots.empty_enum_cache()));

  // Fix the instance_descriptors for the existing maps.
  FinalizePartialMap(roots.meta_map());
  FinalizePartialMap(roots.fixed_array_map());
  FinalizePartialMap(roots.weak_fixed_array_map());
  FinalizePartialMap(roots.weak_array_list_map());
  FinalizePartialMap(roots.fixed_cow_array_map());
  FinalizePartialMap(roots.descriptor_array_map());
  FinalizePartialMap(roots.undefined_map());
  roots.undefined_map()->set_is_undetectable(true);
  FinalizePartialMap(roots.null_map());
  roots.null_map()->set_is_undetectable(true);
  FinalizePartialMap(roots.the_hole_map());
  for (unsigned i = 0; i < arraysize(struct_table); ++i) {
    const StructTable& entry = struct_table[i];
    FinalizePartialMap(Map::cast(roots_[entry.index]));
  }

  {  // Map allocation
#define ALLOCATE_MAP(instance_type, size, field_name)               \
    {                                                                 \
      Map* map;                                                       \
      if (!AllocateMap((instance_type), size).To(&map)) return false; \
      set_##field_name##_map(map);                                    \
    }

#define ALLOCATE_VARSIZE_MAP(instance_type, field_name) \
    ALLOCATE_MAP(instance_type, kVariableSizeSentinel, field_name)

#define ALLOCATE_PRIMITIVE_MAP(instance_type, size, field_name, \
                               constructor_function_index)      \
  {                                                             \
    ALLOCATE_MAP((instance_type), (size), field_name);          \
    roots.field_name##_map()->SetConstructorFunctionIndex(      \
        (constructor_function_index));                          \
  }

    ALLOCATE_VARSIZE_MAP(SCOPE_INFO_TYPE, scope_info)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, module_info)
    ALLOCATE_VARSIZE_MAP(FEEDBACK_VECTOR_TYPE, feedback_vector)
    ALLOCATE_PRIMITIVE_MAP(HEAP_NUMBER_TYPE, HeapNumber::kSize, heap_number,
                           Context::NUMBER_FUNCTION_INDEX)
    ALLOCATE_MAP(MUTABLE_HEAP_NUMBER_TYPE, MutableHeapNumber::kSize,
                 mutable_heap_number)
    ALLOCATE_PRIMITIVE_MAP(SYMBOL_TYPE, Symbol::kSize, symbol,
                           Context::SYMBOL_FUNCTION_INDEX)
    ALLOCATE_MAP(FOREIGN_TYPE, Foreign::kSize, foreign)

    ALLOCATE_PRIMITIVE_MAP(ODDBALL_TYPE, Oddball::kSize, boolean,
                           Context::BOOLEAN_FUNCTION_INDEX);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, uninitialized);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, arguments_marker);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, exception);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, termination_exception);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, optimized_out);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, stale_register);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, self_reference_marker);
    ALLOCATE_VARSIZE_MAP(BIGINT_TYPE, bigint);

    for (unsigned i = 0; i < arraysize(string_type_table); i++) {
      const StringTypeTable& entry = string_type_table[i];
      {
        AllocationResult allocation = AllocateMap(entry.type, entry.size);
        if (!allocation.To(&obj)) return false;
      }
      Map* map = Map::cast(obj);
      map->SetConstructorFunctionIndex(Context::STRING_FUNCTION_INDEX);
      // Mark cons string maps as unstable, because their objects can change
      // maps during GC.
      if (StringShape(entry.type).IsCons()) map->mark_unstable();
      roots_[entry.index] = map;
    }

    {  // Create a separate external one byte string map for native sources.
      AllocationResult allocation =
          AllocateMap(UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE,
                      ExternalOneByteString::kUncachedSize);
      if (!allocation.To(&obj)) return false;
      Map* map = Map::cast(obj);
      map->SetConstructorFunctionIndex(Context::STRING_FUNCTION_INDEX);
      set_native_source_string_map(map);
    }

    ALLOCATE_VARSIZE_MAP(FIXED_DOUBLE_ARRAY_TYPE, fixed_double_array)
    roots.fixed_double_array_map()->set_elements_kind(HOLEY_DOUBLE_ELEMENTS);
    ALLOCATE_VARSIZE_MAP(FEEDBACK_METADATA_TYPE, feedback_metadata)
    ALLOCATE_VARSIZE_MAP(BYTE_ARRAY_TYPE, byte_array)
    ALLOCATE_VARSIZE_MAP(BYTECODE_ARRAY_TYPE, bytecode_array)
    ALLOCATE_VARSIZE_MAP(FREE_SPACE_TYPE, free_space)
    ALLOCATE_VARSIZE_MAP(PROPERTY_ARRAY_TYPE, property_array)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_HASH_MAP_TYPE, small_ordered_hash_map)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_HASH_SET_TYPE, small_ordered_hash_set)

#define ALLOCATE_FIXED_TYPED_ARRAY_MAP(Type, type, TYPE, ctype) \
  ALLOCATE_VARSIZE_MAP(FIXED_##TYPE##_ARRAY_TYPE, fixed_##type##_array)

    TYPED_ARRAYS(ALLOCATE_FIXED_TYPED_ARRAY_MAP)
#undef ALLOCATE_FIXED_TYPED_ARRAY_MAP

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, sloppy_arguments_elements)

    ALLOCATE_VARSIZE_MAP(CODE_TYPE, code)

    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, cell);
    {
      // The invalid_prototype_validity_cell is needed for JSObject maps.
      Smi* value = Smi::FromInt(Map::kPrototypeChainInvalid);
      AllocationResult alloc = AllocateRaw(Cell::kSize, OLD_SPACE);
      if (!alloc.To(&obj)) return false;
      obj->set_map_after_allocation(roots.cell_map(), SKIP_WRITE_BARRIER);
      Cell::cast(obj)->set_value(value);
      set_invalid_prototype_validity_cell(Cell::cast(obj));
    }

    ALLOCATE_MAP(PROPERTY_CELL_TYPE, PropertyCell::kSize, global_property_cell)
    ALLOCATE_MAP(FILLER_TYPE, kPointerSize, one_pointer_filler)
    ALLOCATE_MAP(FILLER_TYPE, 2 * kPointerSize, two_pointer_filler)

    // The "no closures" and "one closure" FeedbackCell maps need
    // to be marked unstable because their objects can change maps.
    ALLOCATE_MAP(FEEDBACK_CELL_TYPE, FeedbackCell::kSize, no_closures_cell)
    roots.no_closures_cell_map()->mark_unstable();
    ALLOCATE_MAP(FEEDBACK_CELL_TYPE, FeedbackCell::kSize, one_closure_cell)
    roots.one_closure_cell_map()->mark_unstable();
    ALLOCATE_MAP(FEEDBACK_CELL_TYPE, FeedbackCell::kSize, many_closures_cell)

    ALLOCATE_VARSIZE_MAP(TRANSITION_ARRAY_TYPE, transition_array)

    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, hash_table)
    ALLOCATE_VARSIZE_MAP(ORDERED_HASH_MAP_TYPE, ordered_hash_map)
    ALLOCATE_VARSIZE_MAP(ORDERED_HASH_SET_TYPE, ordered_hash_set)
    ALLOCATE_VARSIZE_MAP(NAME_DICTIONARY_TYPE, name_dictionary)
    ALLOCATE_VARSIZE_MAP(GLOBAL_DICTIONARY_TYPE, global_dictionary)
    ALLOCATE_VARSIZE_MAP(NUMBER_DICTIONARY_TYPE, number_dictionary)
    ALLOCATE_VARSIZE_MAP(SIMPLE_NUMBER_DICTIONARY_TYPE,
                         simple_number_dictionary)
    ALLOCATE_VARSIZE_MAP(STRING_TABLE_TYPE, string_table)

    ALLOCATE_VARSIZE_MAP(EPHEMERON_HASH_TABLE_TYPE, ephemeron_hash_table)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, array_list)

    ALLOCATE_VARSIZE_MAP(FUNCTION_CONTEXT_TYPE, function_context)
    ALLOCATE_VARSIZE_MAP(CATCH_CONTEXT_TYPE, catch_context)
    ALLOCATE_VARSIZE_MAP(WITH_CONTEXT_TYPE, with_context)
    ALLOCATE_VARSIZE_MAP(DEBUG_EVALUATE_CONTEXT_TYPE, debug_evaluate_context)
    ALLOCATE_VARSIZE_MAP(AWAIT_CONTEXT_TYPE, await_context)
    ALLOCATE_VARSIZE_MAP(BLOCK_CONTEXT_TYPE, block_context)
    ALLOCATE_VARSIZE_MAP(MODULE_CONTEXT_TYPE, module_context)
    ALLOCATE_VARSIZE_MAP(EVAL_CONTEXT_TYPE, eval_context)
    ALLOCATE_VARSIZE_MAP(SCRIPT_CONTEXT_TYPE, script_context)
    ALLOCATE_VARSIZE_MAP(SCRIPT_CONTEXT_TABLE_TYPE, script_context_table)

    ALLOCATE_VARSIZE_MAP(OBJECT_BOILERPLATE_DESCRIPTION_TYPE,
                         object_boilerplate_description)

    ALLOCATE_VARSIZE_MAP(NATIVE_CONTEXT_TYPE, native_context)
    roots.native_context_map()->set_visitor_id(kVisitNativeContext);

    ALLOCATE_MAP(CALL_HANDLER_INFO_TYPE, CallHandlerInfo::kSize,
                 side_effect_call_handler_info)
    ALLOCATE_MAP(CALL_HANDLER_INFO_TYPE, CallHandlerInfo::kSize,
                 side_effect_free_call_handler_info)
    ALLOCATE_MAP(CALL_HANDLER_INFO_TYPE, CallHandlerInfo::kSize,
                 next_call_side_effect_free_call_handler_info)

    ALLOCATE_VARSIZE_MAP(PRE_PARSED_SCOPE_DATA_TYPE, pre_parsed_scope_data)
    ALLOCATE_MAP(UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE,
                 UncompiledDataWithoutPreParsedScope::kSize,
                 uncompiled_data_without_pre_parsed_scope)
    ALLOCATE_MAP(UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE,
                 UncompiledDataWithPreParsedScope::kSize,
                 uncompiled_data_with_pre_parsed_scope)
    ALLOCATE_MAP(SHARED_FUNCTION_INFO_TYPE, SharedFunctionInfo::kAlignedSize,
                 shared_function_info)

    ALLOCATE_MAP(CODE_DATA_CONTAINER_TYPE, CodeDataContainer::kSize,
                 code_data_container)

    ALLOCATE_MAP(JS_MESSAGE_OBJECT_TYPE, JSMessageObject::kSize, message_object)
    ALLOCATE_MAP(JS_OBJECT_TYPE, JSObject::kHeaderSize + kPointerSize, external)
    external_map()->set_is_extensible(false);
#undef ALLOCATE_PRIMITIVE_MAP
#undef ALLOCATE_VARSIZE_MAP
#undef ALLOCATE_MAP
  }

  {
    AllocationResult alloc = AllocateRaw(FixedArray::SizeFor(0), RO_SPACE);
    if (!alloc.To(&obj)) return false;
    obj->set_map_after_allocation(roots.scope_info_map(), SKIP_WRITE_BARRIER);
    FixedArray::cast(obj)->set_length(0);
  }
  set_empty_scope_info(ScopeInfo::cast(obj));

  {
    // Empty boilerplate needs a field for literal_flags
    AllocationResult alloc = AllocateRaw(FixedArray::SizeFor(1), RO_SPACE);
    if (!alloc.To(&obj)) return false;
    obj->set_map_after_allocation(roots.object_boilerplate_description_map(),
                                  SKIP_WRITE_BARRIER);

    FixedArray::cast(obj)->set_length(1);
    FixedArray::cast(obj)->set(ObjectBoilerplateDescription::kLiteralTypeOffset,
                               Smi::kZero);
  }
  set_empty_object_boilerplate_description(
      ObjectBoilerplateDescription::cast(obj));

  {
    // Empty array boilerplate description
    AllocationResult alloc =
        Allocate(roots.array_boilerplate_description_map(), RO_SPACE);
    if (!alloc.To(&obj)) return false;

    ArrayBoilerplateDescription::cast(obj)->set_constant_elements(
        roots.empty_fixed_array());
    ArrayBoilerplateDescription::cast(obj)->set_elements_kind(
        ElementsKind::PACKED_SMI_ELEMENTS);
  }
  set_empty_array_boilerplate_description(
      ArrayBoilerplateDescription::cast(obj));

  {
    AllocationResult allocation = Allocate(roots.boolean_map(), RO_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_true_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kTrue);

  {
    AllocationResult allocation = Allocate(roots.boolean_map(), RO_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_false_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kFalse);

  // Empty arrays.
  {
    if (!AllocateRaw(ByteArray::SizeFor(0), RO_SPACE).To(&obj)) return false;
    obj->set_map_after_allocation(roots.byte_array_map(), SKIP_WRITE_BARRIER);
    ByteArray::cast(obj)->set_length(0);
    set_empty_byte_array(ByteArray::cast(obj));
  }

  {
    if (!AllocateRaw(FixedArray::SizeFor(0), RO_SPACE).To(&obj)) {
      return false;
    }
    obj->set_map_after_allocation(roots.property_array_map(),
                                  SKIP_WRITE_BARRIER);
    PropertyArray::cast(obj)->initialize_length(0);
    set_empty_property_array(PropertyArray::cast(obj));
  }

#define ALLOCATE_EMPTY_FIXED_TYPED_ARRAY(Type, type, TYPE, ctype)       \
  {                                                                     \
    FixedTypedArrayBase* obj;                                           \
    if (!AllocateEmptyFixedTypedArray(kExternal##Type##Array).To(&obj)) \
      return false;                                                     \
    set_empty_fixed_##type##_array(obj);                                \
  }

  TYPED_ARRAYS(ALLOCATE_EMPTY_FIXED_TYPED_ARRAY)
#undef ALLOCATE_EMPTY_FIXED_TYPED_ARRAY

  DCHECK(!InNewSpace(roots.empty_fixed_array()));

  roots.bigint_map()->SetConstructorFunctionIndex(
      Context::BIGINT_FUNCTION_INDEX);

  return true;
}

void Heap::CreateApiObjects() {
  Isolate* isolate = this->isolate();
  HandleScope scope(isolate);

  set_message_listeners(*TemplateList::New(isolate, 2));

  Handle<InterceptorInfo> info = Handle<InterceptorInfo>::cast(
      isolate->factory()->NewStruct(INTERCEPTOR_INFO_TYPE, TENURED_READ_ONLY));
  info->set_flags(0);
  set_noop_interceptor_info(*info);
}

void Heap::CreateInitialObjects() {
  HandleScope scope(isolate());
  Factory* factory = isolate()->factory();
  ReadOnlyRoots roots(this);

  // The -0 value must be set before NewNumber works.
  set_minus_zero_value(*factory->NewHeapNumber(-0.0, TENURED_READ_ONLY));
  DCHECK(std::signbit(roots.minus_zero_value()->Number()));

  set_nan_value(*factory->NewHeapNumber(
      std::numeric_limits<double>::quiet_NaN(), TENURED_READ_ONLY));
  set_hole_nan_value(
      *factory->NewHeapNumberFromBits(kHoleNanInt64, TENURED_READ_ONLY));
  set_infinity_value(*factory->NewHeapNumber(V8_INFINITY, TENURED_READ_ONLY));
  set_minus_infinity_value(
      *factory->NewHeapNumber(-V8_INFINITY, TENURED_READ_ONLY));

  set_hash_seed(*factory->NewByteArray(kInt64Size, TENURED));
  InitializeHashSeed();

  // Allocate cache for single character one byte strings.
  set_single_character_string_cache(
      *factory->NewFixedArray(String::kMaxOneByteCharCode + 1, TENURED));

  // Allocate initial string table.
  set_string_table(*StringTable::New(isolate(), kInitialStringTableSize));

  for (unsigned i = 0; i < arraysize(constant_string_table); i++) {
    Handle<String> str =
        factory->InternalizeUtf8String(constant_string_table[i].contents);
    roots_[constant_string_table[i].index] = *str;
  }

  // Allocate

  // Finish initializing oddballs after creating the string table.
  Oddball::Initialize(isolate(), factory->undefined_value(), "undefined",
                      factory->nan_value(), "undefined", Oddball::kUndefined);

  // Initialize the null_value.
  Oddball::Initialize(isolate(), factory->null_value(), "null",
                      handle(Smi::kZero, isolate()), "object", Oddball::kNull);

  // Initialize the_hole_value.
  Oddball::Initialize(isolate(), factory->the_hole_value(), "hole",
                      factory->hole_nan_value(), "undefined",
                      Oddball::kTheHole);

  // Initialize the true_value.
  Oddball::Initialize(isolate(), factory->true_value(), "true",
                      handle(Smi::FromInt(1), isolate()), "boolean",
                      Oddball::kTrue);

  // Initialize the false_value.
  Oddball::Initialize(isolate(), factory->false_value(), "false",
                      handle(Smi::kZero, isolate()), "boolean",
                      Oddball::kFalse);

  set_uninitialized_value(
      *factory->NewOddball(factory->uninitialized_map(), "uninitialized",
                           handle(Smi::FromInt(-1), isolate()), "undefined",
                           Oddball::kUninitialized));

  set_arguments_marker(
      *factory->NewOddball(factory->arguments_marker_map(), "arguments_marker",
                           handle(Smi::FromInt(-4), isolate()), "undefined",
                           Oddball::kArgumentsMarker));

  set_termination_exception(*factory->NewOddball(
      factory->termination_exception_map(), "termination_exception",
      handle(Smi::FromInt(-3), isolate()), "undefined", Oddball::kOther));

  set_exception(*factory->NewOddball(factory->exception_map(), "exception",
                                     handle(Smi::FromInt(-5), isolate()),
                                     "undefined", Oddball::kException));

  set_optimized_out(*factory->NewOddball(factory->optimized_out_map(),
                                         "optimized_out",
                                         handle(Smi::FromInt(-6), isolate()),
                                         "undefined", Oddball::kOptimizedOut));

  set_stale_register(
      *factory->NewOddball(factory->stale_register_map(), "stale_register",
                           handle(Smi::FromInt(-7), isolate()), "undefined",
                           Oddball::kStaleRegister));

  // Initialize the self-reference marker.
  set_self_reference_marker(
      *factory->NewSelfReferenceMarker(TENURED_READ_ONLY));

  // Create the code_stubs dictionary. The initial size is set to avoid
  // expanding the dictionary during bootstrapping.
  set_code_stubs(*SimpleNumberDictionary::New(isolate(), 128));

  {
    HandleScope scope(isolate());
#define SYMBOL_INIT(_, name)                                        \
  {                                                                 \
    Handle<Symbol> symbol(                                          \
        isolate()->factory()->NewPrivateSymbol(TENURED_READ_ONLY)); \
    roots_[RootIndex::k##name] = *symbol;                           \
  }
    PRIVATE_SYMBOL_LIST_GENERATOR(SYMBOL_INIT, /* not used */)
#undef SYMBOL_INIT
  }

  {
    HandleScope scope(isolate());
#define SYMBOL_INIT(_, name, description)                                 \
  Handle<Symbol> name = factory->NewSymbol(TENURED_READ_ONLY);            \
  Handle<String> name##d =                                                \
      factory->NewStringFromStaticChars(#description, TENURED_READ_ONLY); \
  name->set_name(*name##d);                                               \
  roots_[RootIndex::k##name] = *name;
    PUBLIC_SYMBOL_LIST_GENERATOR(SYMBOL_INIT, /* not used */)
#undef SYMBOL_INIT

#define SYMBOL_INIT(_, name, description)                                 \
  Handle<Symbol> name = factory->NewSymbol(TENURED_READ_ONLY);            \
  Handle<String> name##d =                                                \
      factory->NewStringFromStaticChars(#description, TENURED_READ_ONLY); \
  name->set_is_well_known_symbol(true);                                   \
  name->set_name(*name##d);                                               \
  roots_[RootIndex::k##name] = *name;
    WELL_KNOWN_SYMBOL_LIST_GENERATOR(SYMBOL_INIT, /* not used */)
#undef SYMBOL_INIT

    // Mark "Interesting Symbols" appropriately.
    to_string_tag_symbol->set_is_interesting_symbol(true);
  }

  Handle<NameDictionary> empty_property_dictionary =
      NameDictionary::New(isolate(), 1, TENURED, USE_CUSTOM_MINIMUM_CAPACITY);
  DCHECK(!empty_property_dictionary->HasSufficientCapacityToAdd(1));
  set_empty_property_dictionary(*empty_property_dictionary);

  set_public_symbol_table(*empty_property_dictionary);
  set_api_symbol_table(*empty_property_dictionary);
  set_api_private_symbol_table(*empty_property_dictionary);

  set_number_string_cache(
      *factory->NewFixedArray(kInitialNumberStringCacheSize * 2, TENURED));

  // Allocate cache for string split and regexp-multiple.
  set_string_split_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, TENURED));
  set_regexp_multiple_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, TENURED));

  // Allocate FeedbackCell for builtins.
  Handle<FeedbackCell> many_closures_cell =
      factory->NewManyClosuresCell(factory->undefined_value());
  set_many_closures_cell(*many_closures_cell);

  set_default_microtask_queue(*factory->NewMicrotaskQueue());

  {
    Handle<FixedArray> empty_sloppy_arguments_elements =
        factory->NewFixedArray(2, TENURED_READ_ONLY);
    empty_sloppy_arguments_elements->set_map_after_allocation(
        roots.sloppy_arguments_elements_map(), SKIP_WRITE_BARRIER);
    set_empty_sloppy_arguments_elements(*empty_sloppy_arguments_elements);
  }

  set_detached_contexts(roots.empty_weak_array_list());
  set_retained_maps(roots.empty_weak_array_list());
  set_retaining_path_targets(roots.empty_weak_array_list());

  set_feedback_vectors_for_profiling_tools(roots.undefined_value());

  set_script_list(roots.empty_weak_array_list());

  Handle<NumberDictionary> slow_element_dictionary = NumberDictionary::New(
      isolate(), 1, TENURED_READ_ONLY, USE_CUSTOM_MINIMUM_CAPACITY);
  DCHECK(!slow_element_dictionary->HasSufficientCapacityToAdd(1));
  slow_element_dictionary->set_requires_slow_elements();
  set_empty_slow_element_dictionary(*slow_element_dictionary);

  set_materialized_objects(*factory->NewFixedArray(0, TENURED));

  // Handling of script id generation is in Heap::NextScriptId().
  set_last_script_id(Smi::FromInt(v8::UnboundScript::kNoScriptId));
  set_last_debugging_id(Smi::FromInt(DebugInfo::kNoDebuggingId));
  set_next_template_serial_number(Smi::kZero);

  // Allocate the empty OrderedHashMap.
  Handle<FixedArray> empty_ordered_hash_map = factory->NewFixedArray(
      OrderedHashMap::kHashTableStartIndex, TENURED_READ_ONLY);
  empty_ordered_hash_map->set_map_no_write_barrier(
      *factory->ordered_hash_map_map());
  for (int i = 0; i < empty_ordered_hash_map->length(); ++i) {
    empty_ordered_hash_map->set(i, Smi::kZero);
  }
  set_empty_ordered_hash_map(*empty_ordered_hash_map);

  // Allocate the empty OrderedHashSet.
  Handle<FixedArray> empty_ordered_hash_set = factory->NewFixedArray(
      OrderedHashSet::kHashTableStartIndex, TENURED_READ_ONLY);
  empty_ordered_hash_set->set_map_no_write_barrier(
      *factory->ordered_hash_set_map());
  for (int i = 0; i < empty_ordered_hash_set->length(); ++i) {
    empty_ordered_hash_set->set(i, Smi::kZero);
  }
  set_empty_ordered_hash_set(*empty_ordered_hash_set);

  // Allocate the empty FeedbackMetadata.
  Handle<FeedbackMetadata> empty_feedback_metadata =
      factory->NewFeedbackMetadata(0, TENURED_READ_ONLY);
  set_empty_feedback_metadata(*empty_feedback_metadata);

  // Allocate the empty script.
  Handle<Script> script = factory->NewScript(factory->empty_string());
  script->set_type(Script::TYPE_NATIVE);
  // This is used for exceptions thrown with no stack frames. Such exceptions
  // can be shared everywhere.
  script->set_origin_options(ScriptOriginOptions(true, false));
  set_empty_script(*script);

  Handle<Cell> array_constructor_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_array_constructor_protector(*array_constructor_cell);

  Handle<PropertyCell> cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_no_elements_protector(*cell);

  cell = factory->NewPropertyCell(factory->empty_string(), TENURED_READ_ONLY);
  cell->set_value(roots.the_hole_value());
  set_empty_property_cell(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_array_iterator_protector(*cell);

  Handle<Cell> is_concat_spreadable_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_is_concat_spreadable_protector(*is_concat_spreadable_cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_array_species_protector(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_typed_array_species_protector(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_promise_species_protector(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_string_iterator_protector(*cell);

  Handle<Cell> string_length_overflow_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_string_length_protector(*string_length_overflow_cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_array_buffer_neutering_protector(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_promise_hook_protector(*cell);

  Handle<Cell> promise_resolve_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_promise_resolve_protector(*promise_resolve_cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_promise_then_protector(*cell);

  set_serialized_objects(roots.empty_fixed_array());
  set_serialized_global_proxy_sizes(roots.empty_fixed_array());

  set_noscript_shared_function_infos(roots.empty_weak_array_list());

  // Evaluate the hash values which will then be cached in the strings.
  isolate()->factory()->zero_string()->Hash();
  isolate()->factory()->one_string()->Hash();

  // Initialize builtins constants table.
  set_builtins_constants_table(roots.empty_fixed_array());

  // Initialize context slot cache.
  isolate_->context_slot_cache()->Clear();

  // Initialize descriptor cache.
  isolate_->descriptor_lookup_cache()->Clear();

  // Initialize compilation cache.
  isolate_->compilation_cache()->Clear();
}

void Heap::CreateInternalAccessorInfoObjects() {
  Isolate* isolate = this->isolate();
  HandleScope scope(isolate);
  Handle<AccessorInfo> acessor_info;

#define INIT_ACCESSOR_INFO(_, accessor_name, AccessorName, ...) \
  acessor_info = Accessors::Make##AccessorName##Info(isolate);  \
  roots_[RootIndex::k##AccessorName##Accessor] = *acessor_info;
  ACCESSOR_INFO_LIST_GENERATOR(INIT_ACCESSOR_INFO, /* not used */)
#undef INIT_ACCESSOR_INFO

#define INIT_SIDE_EFFECT_FLAG(_, accessor_name, AccessorName, GetterType, \
                              SetterType)                                 \
  AccessorInfo::cast(roots_[RootIndex::k##AccessorName##Accessor])        \
      ->set_getter_side_effect_type(SideEffectType::GetterType);          \
  AccessorInfo::cast(roots_[RootIndex::k##AccessorName##Accessor])        \
      ->set_setter_side_effect_type(SideEffectType::SetterType);
  ACCESSOR_INFO_LIST_GENERATOR(INIT_SIDE_EFFECT_FLAG, /* not used */)
#undef INIT_SIDE_EFFECT_FLAG
}

}  // namespace internal
}  // namespace v8

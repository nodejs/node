// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/accessors.h"
#include "src/codegen/compilation-cache.h"
#include "src/execution/isolate.h"
#include "src/execution/protectors.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/ic/handler-configuration.h"
#include "src/init/heap-symbols.h"
#include "src/init/setup-isolate.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/arguments.h"
#include "src/objects/cell-inl.h"
#include "src/objects/contexts.h"
#include "src/objects/data-handler.h"
#include "src/objects/debug-objects.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/js-generator.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/lookup-cache.h"
#include "src/objects/map.h"
#include "src/objects/microtask.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/promise.h"
#include "src/objects/property-descriptor-object.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi.h"
#include "src/objects/source-text-module.h"
#include "src/objects/stack-frame-info.h"
#include "src/objects/string.h"
#include "src/objects/synthetic-module.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/torque-defined-classes-inl.h"
#include "src/regexp/regexp.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

namespace {

Handle<SharedFunctionInfo> CreateSharedFunctionInfo(
    Isolate* isolate, Builtins::Name builtin_id, int len,
    FunctionKind kind = FunctionKind::kNormalFunction) {
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), builtin_id, kind);
  shared->set_internal_formal_parameter_count(len);
  shared->set_length(len);
  return shared;
}

}  // namespace

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
  set_dirty_js_finalization_registries_list(
      ReadOnlyRoots(this).undefined_value());
  set_dirty_js_finalization_registries_list_tail(
      ReadOnlyRoots(this).undefined_value());

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
  bool is_wasm_object = false;
#if V8_ENABLE_WEBASSEMBLY
  is_wasm_object =
      instance_type == WASM_STRUCT_TYPE || instance_type == WASM_ARRAY_TYPE;
#endif  // V8_ENABLE_WEBASSEMBLY
  DCHECK_IMPLIES(is_js_object &&
                     !Map::CanHaveFastTransitionableElementsKind(instance_type),
                 IsDictionaryElementsKind(elements_kind) ||
                     IsTerminalElementsKind(elements_kind));
  HeapObject result;
  // JSObjects have maps with a mutable prototype_validity_cell, so they cannot
  // go in RO_SPACE. Maps for managed Wasm objects have mutable subtype lists.
  bool is_mutable = is_js_object || is_wasm_object;
  AllocationResult allocation =
      AllocateRaw(Map::kSize, is_mutable ? AllocationType::kMap
                                         : AllocationType::kReadOnly);
  if (!allocation.To(&result)) return allocation;

  result.set_map_after_allocation(ReadOnlyRoots(this).meta_map(),
                                  SKIP_WRITE_BARRIER);
  Map map = isolate()->factory()->InitializeMap(
      Map::cast(result), instance_type, instance_size, elements_kind,
      inobject_properties);

  return map;
}

AllocationResult Heap::AllocatePartialMap(InstanceType instance_type,
                                          int instance_size) {
  Object result;
  AllocationResult allocation =
      AllocateRaw(Map::kSize, AllocationType::kReadOnly);
  if (!allocation.To(&result)) return allocation;
  // Map::cast cannot be used due to uninitialized map field.
  Map map = Map::unchecked_cast(result);
  map.set_map_after_allocation(
      Map::unchecked_cast(isolate()->root(RootIndex::kMetaMap)),
      SKIP_WRITE_BARRIER);
  map.set_instance_type(instance_type);
  map.set_instance_size(instance_size);
  map.set_visitor_id(Map::GetVisitorId(map));
  map.set_inobject_properties_start_or_constructor_function_index(0);
  DCHECK(!map.IsJSObjectMap());
  map.set_prototype_validity_cell(Smi::FromInt(Map::kPrototypeChainValid));
  map.SetInObjectUnusedPropertyFields(0);
  map.set_bit_field(0);
  map.set_bit_field2(0);
  int bit_field3 =
      Map::Bits3::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
      Map::Bits3::OwnsDescriptorsBit::encode(true) |
      Map::Bits3::ConstructionCounterBits::encode(Map::kNoSlackTracking);
  map.set_bit_field3(bit_field3);
  DCHECK(!map.is_in_retained_map_list());
  map.clear_padding();
  map.set_elements_kind(TERMINAL_FAST_ELEMENTS_KIND);
  return map;
}

void Heap::FinalizePartialMap(Map map) {
  ReadOnlyRoots roots(this);
  map.set_dependent_code(DependentCode::cast(roots.empty_weak_fixed_array()));
  map.set_raw_transitions(MaybeObject::FromSmi(Smi::zero()));
  map.SetInstanceDescriptors(isolate(), roots.empty_descriptor_array(), 0);
  map.set_prototype(roots.null_value());
  map.set_constructor_or_back_pointer(roots.null_value());
}

AllocationResult Heap::Allocate(Handle<Map> map,
                                AllocationType allocation_type) {
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  HeapObject result;
  AllocationResult allocation = AllocateRaw(size, allocation_type);
  if (!allocation.To(&result)) return allocation;
  // New space objects are allocated white.
  WriteBarrierMode write_barrier_mode =
      allocation_type == AllocationType::kYoung ? SKIP_WRITE_BARRIER
                                                : UPDATE_WRITE_BARRIER;
  result.set_map_after_allocation(*map, write_barrier_mode);
  return result;
}

bool Heap::CreateInitialMaps() {
  HeapObject obj;
  {
    AllocationResult allocation = AllocatePartialMap(MAP_TYPE, Map::kSize);
    if (!allocation.To(&obj)) return false;
  }
  // Map::cast cannot be used due to uninitialized map field.
  Map new_meta_map = Map::unchecked_cast(obj);
  set_meta_map(new_meta_map);
  new_meta_map.set_map_after_allocation(new_meta_map);

  ReadOnlyRoots roots(this);
  {  // Partial map allocation
#define ALLOCATE_PARTIAL_MAP(instance_type, size, field_name)                \
  {                                                                          \
    Map map;                                                                 \
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
    AllocationResult alloc =
        AllocateRaw(FixedArray::SizeFor(0), AllocationType::kReadOnly);
    if (!alloc.To(&obj)) return false;
    obj.set_map_after_allocation(roots.fixed_array_map(), SKIP_WRITE_BARRIER);
    FixedArray::cast(obj).set_length(0);
  }
  set_empty_fixed_array(FixedArray::cast(obj));

  {
    AllocationResult alloc =
        AllocateRaw(WeakFixedArray::SizeFor(0), AllocationType::kReadOnly);
    if (!alloc.To(&obj)) return false;
    obj.set_map_after_allocation(roots.weak_fixed_array_map(),
                                 SKIP_WRITE_BARRIER);
    WeakFixedArray::cast(obj).set_length(0);
  }
  set_empty_weak_fixed_array(WeakFixedArray::cast(obj));

  {
    AllocationResult allocation = AllocateRaw(WeakArrayList::SizeForCapacity(0),
                                              AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
    obj.set_map_after_allocation(roots.weak_array_list_map(),
                                 SKIP_WRITE_BARRIER);
    WeakArrayList::cast(obj).set_capacity(0);
    WeakArrayList::cast(obj).set_length(0);
  }
  set_empty_weak_array_list(WeakArrayList::cast(obj));

  {
    AllocationResult allocation =
        Allocate(roots.null_map_handle(), AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
  }
  set_null_value(Oddball::cast(obj));
  Oddball::cast(obj).set_kind(Oddball::kNull);

  {
    AllocationResult allocation =
        Allocate(roots.undefined_map_handle(), AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
  }
  set_undefined_value(Oddball::cast(obj));
  Oddball::cast(obj).set_kind(Oddball::kUndefined);
  DCHECK(!InYoungGeneration(roots.undefined_value()));
  {
    AllocationResult allocation =
        Allocate(roots.the_hole_map_handle(), AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
  }
  set_the_hole_value(Oddball::cast(obj));
  Oddball::cast(obj).set_kind(Oddball::kTheHole);

  // Set preliminary exception sentinel value before actually initializing it.
  set_exception(roots.null_value());

  // Setup the struct maps first (needed for the EnumCache).
  for (unsigned i = 0; i < arraysize(struct_table); i++) {
    const StructTable& entry = struct_table[i];
    Map map;
    if (!AllocatePartialMap(entry.type, entry.size).To(&map)) return false;
    roots_table()[entry.index] = map.ptr();
  }

  // Allocate the empty enum cache.
  {
    AllocationResult allocation =
        Allocate(roots.enum_cache_map_handle(), AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
  }
  set_empty_enum_cache(EnumCache::cast(obj));
  EnumCache::cast(obj).set_keys(roots.empty_fixed_array());
  EnumCache::cast(obj).set_indices(roots.empty_fixed_array());

  // Allocate the empty descriptor array.
  {
    int size = DescriptorArray::SizeFor(0);
    if (!AllocateRaw(size, AllocationType::kReadOnly).To(&obj)) return false;
    obj.set_map_after_allocation(roots.descriptor_array_map(),
                                 SKIP_WRITE_BARRIER);
    DescriptorArray array = DescriptorArray::cast(obj);
    array.Initialize(roots.empty_enum_cache(), roots.undefined_value(), 0, 0);
  }
  set_empty_descriptor_array(DescriptorArray::cast(obj));

  // Fix the instance_descriptors for the existing maps.
  FinalizePartialMap(roots.meta_map());
  FinalizePartialMap(roots.fixed_array_map());
  FinalizePartialMap(roots.weak_fixed_array_map());
  FinalizePartialMap(roots.weak_array_list_map());
  FinalizePartialMap(roots.fixed_cow_array_map());
  FinalizePartialMap(roots.descriptor_array_map());
  FinalizePartialMap(roots.undefined_map());
  roots.undefined_map().set_is_undetectable(true);
  FinalizePartialMap(roots.null_map());
  roots.null_map().set_is_undetectable(true);
  FinalizePartialMap(roots.the_hole_map());
  for (unsigned i = 0; i < arraysize(struct_table); ++i) {
    const StructTable& entry = struct_table[i];
    FinalizePartialMap(Map::cast(Object(roots_table()[entry.index])));
  }

  {  // Map allocation
#define ALLOCATE_MAP(instance_type, size, field_name)               \
  {                                                                 \
    Map map;                                                        \
    if (!AllocateMap((instance_type), size).To(&map)) return false; \
    set_##field_name##_map(map);                                    \
  }

#define ALLOCATE_VARSIZE_MAP(instance_type, field_name) \
  ALLOCATE_MAP(instance_type, kVariableSizeSentinel, field_name)

#define ALLOCATE_PRIMITIVE_MAP(instance_type, size, field_name, \
                               constructor_function_index)      \
  {                                                             \
    ALLOCATE_MAP((instance_type), (size), field_name);          \
    roots.field_name##_map().SetConstructorFunctionIndex(       \
        (constructor_function_index));                          \
  }

    ALLOCATE_VARSIZE_MAP(SCOPE_INFO_TYPE, scope_info)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, module_info)
    ALLOCATE_VARSIZE_MAP(CLOSURE_FEEDBACK_CELL_ARRAY_TYPE,
                         closure_feedback_cell_array)
    ALLOCATE_VARSIZE_MAP(FEEDBACK_VECTOR_TYPE, feedback_vector)
    ALLOCATE_PRIMITIVE_MAP(HEAP_NUMBER_TYPE, HeapNumber::kSize, heap_number,
                           Context::NUMBER_FUNCTION_INDEX)
    ALLOCATE_PRIMITIVE_MAP(SYMBOL_TYPE, Symbol::kSize, symbol,
                           Context::SYMBOL_FUNCTION_INDEX)
    ALLOCATE_MAP(FOREIGN_TYPE, Foreign::kSize, foreign)
    ALLOCATE_MAP(MEGA_DOM_HANDLER_TYPE, MegaDomHandler::kSize, mega_dom_handler)

    ALLOCATE_PRIMITIVE_MAP(ODDBALL_TYPE, Oddball::kSize, boolean,
                           Context::BOOLEAN_FUNCTION_INDEX);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, uninitialized);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, arguments_marker);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, exception);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, termination_exception);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, optimized_out);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, stale_register);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, self_reference_marker);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, basic_block_counters_marker);
    ALLOCATE_VARSIZE_MAP(BIGINT_TYPE, bigint);

    for (unsigned i = 0; i < arraysize(string_type_table); i++) {
      const StringTypeTable& entry = string_type_table[i];
      Map map;
      if (!AllocateMap(entry.type, entry.size).To(&map)) return false;
      map.SetConstructorFunctionIndex(Context::STRING_FUNCTION_INDEX);
      // Mark cons string maps as unstable, because their objects can change
      // maps during GC.
      if (StringShape(entry.type).IsCons()) map.mark_unstable();
      roots_table()[entry.index] = map.ptr();
    }

    ALLOCATE_VARSIZE_MAP(FIXED_DOUBLE_ARRAY_TYPE, fixed_double_array)
    roots.fixed_double_array_map().set_elements_kind(HOLEY_DOUBLE_ELEMENTS);
    ALLOCATE_VARSIZE_MAP(FEEDBACK_METADATA_TYPE, feedback_metadata)
    ALLOCATE_VARSIZE_MAP(BYTE_ARRAY_TYPE, byte_array)
    ALLOCATE_VARSIZE_MAP(BYTECODE_ARRAY_TYPE, bytecode_array)
    ALLOCATE_VARSIZE_MAP(FREE_SPACE_TYPE, free_space)
    ALLOCATE_VARSIZE_MAP(PROPERTY_ARRAY_TYPE, property_array)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_HASH_MAP_TYPE, small_ordered_hash_map)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_HASH_SET_TYPE, small_ordered_hash_set)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_NAME_DICTIONARY_TYPE,
                         small_ordered_name_dictionary)

#define TORQUE_ALLOCATE_MAP(NAME, Name, name) \
  ALLOCATE_MAP(NAME, Name::SizeFor(), name)
    TORQUE_DEFINED_FIXED_INSTANCE_TYPE_LIST(TORQUE_ALLOCATE_MAP);
#undef TORQUE_ALLOCATE_MAP

#define TORQUE_ALLOCATE_VARSIZE_MAP(NAME, Name, name)                   \
  /* The DescriptorArray map is pre-allocated and initialized above. */ \
  if (NAME != DESCRIPTOR_ARRAY_TYPE) {                                  \
    ALLOCATE_VARSIZE_MAP(NAME, name)                                    \
  }
    TORQUE_DEFINED_VARSIZE_INSTANCE_TYPE_LIST(TORQUE_ALLOCATE_VARSIZE_MAP);
#undef TORQUE_ALLOCATE_VARSIZE_MAP

    ALLOCATE_VARSIZE_MAP(CODE_TYPE, code)

    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, cell);
    {
      // The invalid_prototype_validity_cell is needed for JSObject maps.
      Smi value = Smi::FromInt(Map::kPrototypeChainInvalid);
      AllocationResult alloc = AllocateRaw(Cell::kSize, AllocationType::kOld);
      if (!alloc.To(&obj)) return false;
      obj.set_map_after_allocation(roots.cell_map(), SKIP_WRITE_BARRIER);
      Cell::cast(obj).set_value(value);
      set_invalid_prototype_validity_cell(Cell::cast(obj));
    }

    ALLOCATE_MAP(PROPERTY_CELL_TYPE, PropertyCell::kSize, global_property_cell)
    ALLOCATE_MAP(FILLER_TYPE, kTaggedSize, one_pointer_filler)
    ALLOCATE_MAP(FILLER_TYPE, 2 * kTaggedSize, two_pointer_filler)

    // The "no closures" and "one closure" FeedbackCell maps need
    // to be marked unstable because their objects can change maps.
    ALLOCATE_MAP(FEEDBACK_CELL_TYPE, FeedbackCell::kAlignedSize,
                 no_closures_cell)
    roots.no_closures_cell_map().mark_unstable();
    ALLOCATE_MAP(FEEDBACK_CELL_TYPE, FeedbackCell::kAlignedSize,
                 one_closure_cell)
    roots.one_closure_cell_map().mark_unstable();
    ALLOCATE_MAP(FEEDBACK_CELL_TYPE, FeedbackCell::kAlignedSize,
                 many_closures_cell)

    ALLOCATE_VARSIZE_MAP(TRANSITION_ARRAY_TYPE, transition_array)

    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, hash_table)
    ALLOCATE_VARSIZE_MAP(ORDERED_HASH_MAP_TYPE, ordered_hash_map)
    ALLOCATE_VARSIZE_MAP(ORDERED_HASH_SET_TYPE, ordered_hash_set)
    ALLOCATE_VARSIZE_MAP(ORDERED_NAME_DICTIONARY_TYPE, ordered_name_dictionary)
    ALLOCATE_VARSIZE_MAP(NAME_DICTIONARY_TYPE, name_dictionary)
    ALLOCATE_VARSIZE_MAP(SWISS_NAME_DICTIONARY_TYPE, swiss_name_dictionary)
    ALLOCATE_VARSIZE_MAP(GLOBAL_DICTIONARY_TYPE, global_dictionary)
    ALLOCATE_VARSIZE_MAP(NUMBER_DICTIONARY_TYPE, number_dictionary)
    ALLOCATE_VARSIZE_MAP(SIMPLE_NUMBER_DICTIONARY_TYPE,
                         simple_number_dictionary)

    ALLOCATE_VARSIZE_MAP(EMBEDDER_DATA_ARRAY_TYPE, embedder_data_array)
    ALLOCATE_VARSIZE_MAP(EPHEMERON_HASH_TABLE_TYPE, ephemeron_hash_table)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, array_list)

    ALLOCATE_VARSIZE_MAP(SCRIPT_CONTEXT_TABLE_TYPE, script_context_table)

    ALLOCATE_VARSIZE_MAP(OBJECT_BOILERPLATE_DESCRIPTION_TYPE,
                         object_boilerplate_description)

    ALLOCATE_VARSIZE_MAP(COVERAGE_INFO_TYPE, coverage_info);

    ALLOCATE_MAP(CALL_HANDLER_INFO_TYPE, CallHandlerInfo::kSize,
                 side_effect_call_handler_info)
    ALLOCATE_MAP(CALL_HANDLER_INFO_TYPE, CallHandlerInfo::kSize,
                 side_effect_free_call_handler_info)
    ALLOCATE_MAP(CALL_HANDLER_INFO_TYPE, CallHandlerInfo::kSize,
                 next_call_side_effect_free_call_handler_info)

    ALLOCATE_VARSIZE_MAP(PREPARSE_DATA_TYPE, preparse_data)
    ALLOCATE_MAP(SHARED_FUNCTION_INFO_TYPE, SharedFunctionInfo::kAlignedSize,
                 shared_function_info)
    ALLOCATE_MAP(SOURCE_TEXT_MODULE_TYPE, SourceTextModule::kSize,
                 source_text_module)
    ALLOCATE_MAP(SYNTHETIC_MODULE_TYPE, SyntheticModule::kSize,
                 synthetic_module)
    ALLOCATE_MAP(CODE_DATA_CONTAINER_TYPE, CodeDataContainer::kSize,
                 code_data_container)

    IF_WASM(ALLOCATE_MAP, WASM_EXPORTED_FUNCTION_DATA_TYPE,
            WasmExportedFunctionData::kSize, wasm_exported_function_data)
    IF_WASM(ALLOCATE_MAP, WASM_JS_FUNCTION_DATA_TYPE, WasmJSFunctionData::kSize,
            wasm_js_function_data)
    IF_WASM(ALLOCATE_MAP, WASM_TYPE_INFO_TYPE, WasmTypeInfo::kSize,
            wasm_type_info)

    ALLOCATE_MAP(WEAK_CELL_TYPE, WeakCell::kSize, weak_cell)

    ALLOCATE_MAP(JS_MESSAGE_OBJECT_TYPE, JSMessageObject::kHeaderSize,
                 message_object)
    ALLOCATE_MAP(JS_OBJECT_TYPE, JSObject::kHeaderSize + kEmbedderDataSlotSize,
                 external)
    external_map().set_is_extensible(false);
#undef ALLOCATE_PRIMITIVE_MAP
#undef ALLOCATE_VARSIZE_MAP
#undef ALLOCATE_MAP
  }

  {
    AllocationResult alloc =
        AllocateRaw(ScopeInfo::SizeFor(ScopeInfo::kVariablePartIndex),
                    AllocationType::kReadOnly);
    if (!alloc.To(&obj)) return false;
    obj.set_map_after_allocation(roots.scope_info_map(), SKIP_WRITE_BARRIER);
    int flags = ScopeInfo::IsEmptyBit::encode(true);
    DCHECK_EQ(ScopeInfo::LanguageModeBit::decode(flags), LanguageMode::kSloppy);
    DCHECK_EQ(ScopeInfo::ReceiverVariableBits::decode(flags),
              VariableAllocationInfo::NONE);
    DCHECK_EQ(ScopeInfo::FunctionVariableBits::decode(flags),
              VariableAllocationInfo::NONE);
    ScopeInfo::cast(obj).set_flags(flags);
    ScopeInfo::cast(obj).set_context_local_count(0);
    ScopeInfo::cast(obj).set_parameter_count(0);
  }
  set_empty_scope_info(ScopeInfo::cast(obj));

  {
    // Empty boilerplate needs a field for literal_flags
    AllocationResult alloc =
        AllocateRaw(FixedArray::SizeFor(1), AllocationType::kReadOnly);
    if (!alloc.To(&obj)) return false;
    obj.set_map_after_allocation(roots.object_boilerplate_description_map(),
                                 SKIP_WRITE_BARRIER);

    FixedArray::cast(obj).set_length(1);
    FixedArray::cast(obj).set(ObjectBoilerplateDescription::kLiteralTypeOffset,
                              Smi::zero());
  }
  set_empty_object_boilerplate_description(
      ObjectBoilerplateDescription::cast(obj));

  {
    // Empty array boilerplate description
    AllocationResult alloc =
        Allocate(roots.array_boilerplate_description_map_handle(),
                 AllocationType::kReadOnly);
    if (!alloc.To(&obj)) return false;

    ArrayBoilerplateDescription::cast(obj).set_constant_elements(
        roots.empty_fixed_array());
    ArrayBoilerplateDescription::cast(obj).set_elements_kind(
        ElementsKind::PACKED_SMI_ELEMENTS);
  }
  set_empty_array_boilerplate_description(
      ArrayBoilerplateDescription::cast(obj));

  {
    AllocationResult allocation =
        Allocate(roots.boolean_map_handle(), AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
  }
  set_true_value(Oddball::cast(obj));
  Oddball::cast(obj).set_kind(Oddball::kTrue);

  {
    AllocationResult allocation =
        Allocate(roots.boolean_map_handle(), AllocationType::kReadOnly);
    if (!allocation.To(&obj)) return false;
  }
  set_false_value(Oddball::cast(obj));
  Oddball::cast(obj).set_kind(Oddball::kFalse);

  // Empty arrays.
  {
    if (!AllocateRaw(ByteArray::SizeFor(0), AllocationType::kReadOnly).To(&obj))
      return false;
    obj.set_map_after_allocation(roots.byte_array_map(), SKIP_WRITE_BARRIER);
    ByteArray::cast(obj).set_length(0);
    set_empty_byte_array(ByteArray::cast(obj));
  }

  {
    if (!AllocateRaw(FixedArray::SizeFor(0), AllocationType::kReadOnly)
             .To(&obj)) {
      return false;
    }
    obj.set_map_after_allocation(roots.property_array_map(),
                                 SKIP_WRITE_BARRIER);
    PropertyArray::cast(obj).initialize_length(0);
    set_empty_property_array(PropertyArray::cast(obj));
  }

  {
    if (!AllocateRaw(FixedArray::SizeFor(0), AllocationType::kReadOnly)
             .To(&obj)) {
      return false;
    }
    obj.set_map_after_allocation(roots.closure_feedback_cell_array_map(),
                                 SKIP_WRITE_BARRIER);
    FixedArray::cast(obj).set_length(0);
    set_empty_closure_feedback_cell_array(ClosureFeedbackCellArray::cast(obj));
  }

  DCHECK(!InYoungGeneration(roots.empty_fixed_array()));

  roots.bigint_map().SetConstructorFunctionIndex(
      Context::BIGINT_FUNCTION_INDEX);

  return true;
}

void Heap::CreateApiObjects() {
  Isolate* isolate = this->isolate();
  HandleScope scope(isolate);

  set_message_listeners(*TemplateList::New(isolate, 2));

  Handle<InterceptorInfo> info =
      Handle<InterceptorInfo>::cast(isolate->factory()->NewStruct(
          INTERCEPTOR_INFO_TYPE, AllocationType::kReadOnly));
  info->set_flags(0);
  set_noop_interceptor_info(*info);
}

void Heap::CreateInitialObjects() {
  HandleScope scope(isolate());
  Factory* factory = isolate()->factory();
  ReadOnlyRoots roots(this);

  // The -0 value must be set before NewNumber works.
  set_minus_zero_value(
      *factory->NewHeapNumber<AllocationType::kReadOnly>(-0.0));
  DCHECK(std::signbit(roots.minus_zero_value().Number()));

  set_nan_value(*factory->NewHeapNumber<AllocationType::kReadOnly>(
      std::numeric_limits<double>::quiet_NaN()));
  set_hole_nan_value(*factory->NewHeapNumberFromBits<AllocationType::kReadOnly>(
      kHoleNanInt64));
  set_infinity_value(
      *factory->NewHeapNumber<AllocationType::kReadOnly>(V8_INFINITY));
  set_minus_infinity_value(
      *factory->NewHeapNumber<AllocationType::kReadOnly>(-V8_INFINITY));

  set_hash_seed(*factory->NewByteArray(kInt64Size, AllocationType::kReadOnly));
  InitializeHashSeed();

  // There's no "current microtask" in the beginning.
  set_current_microtask(roots.undefined_value());

  set_weak_refs_keep_during_job(roots.undefined_value());

  // Allocate cache for single character one byte strings.
  set_single_character_string_cache(*factory->NewFixedArray(
      String::kMaxOneByteCharCode + 1, AllocationType::kOld));

  for (unsigned i = 0; i < arraysize(constant_string_table); i++) {
    Handle<String> str =
        factory->InternalizeUtf8String(constant_string_table[i].contents);
    roots_table()[constant_string_table[i].index] = str->ptr();
  }

  // Allocate

  // Finish initializing oddballs after creating the string table.
  Oddball::Initialize(isolate(), factory->undefined_value(), "undefined",
                      factory->nan_value(), "undefined", Oddball::kUndefined);

  // Initialize the null_value.
  Oddball::Initialize(isolate(), factory->null_value(), "null",
                      handle(Smi::zero(), isolate()), "object", Oddball::kNull);

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
                      handle(Smi::zero(), isolate()), "boolean",
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

  // Initialize marker objects used during compilation.
  set_self_reference_marker(*factory->NewSelfReferenceMarker());
  set_basic_block_counters_marker(*factory->NewBasicBlockCountersMarker());

  set_interpreter_entry_trampoline_for_profiling(roots.undefined_value());

  {
    HandleScope scope(isolate());
#define SYMBOL_INIT(_, name)                                                \
  {                                                                         \
    Handle<Symbol> symbol(                                                  \
        isolate()->factory()->NewPrivateSymbol(AllocationType::kReadOnly)); \
    roots_table()[RootIndex::k##name] = symbol->ptr();                      \
  }
    PRIVATE_SYMBOL_LIST_GENERATOR(SYMBOL_INIT, /* not used */)
#undef SYMBOL_INIT
  }

  {
    HandleScope scope(isolate());
#define SYMBOL_INIT(_, name, description)                                \
  Handle<Symbol> name = factory->NewSymbol(AllocationType::kReadOnly);   \
  Handle<String> name##d = factory->InternalizeUtf8String(#description); \
  name->set_description(*name##d);                                       \
  roots_table()[RootIndex::k##name] = name->ptr();
    PUBLIC_SYMBOL_LIST_GENERATOR(SYMBOL_INIT, /* not used */)
#undef SYMBOL_INIT

#define SYMBOL_INIT(_, name, description)                                \
  Handle<Symbol> name = factory->NewSymbol(AllocationType::kReadOnly);   \
  Handle<String> name##d = factory->InternalizeUtf8String(#description); \
  name->set_is_well_known_symbol(true);                                  \
  name->set_description(*name##d);                                       \
  roots_table()[RootIndex::k##name] = name->ptr();
    WELL_KNOWN_SYMBOL_LIST_GENERATOR(SYMBOL_INIT, /* not used */)
#undef SYMBOL_INIT

    // Mark "Interesting Symbols" appropriately.
    to_string_tag_symbol->set_is_interesting_symbol(true);
  }

  Handle<NameDictionary> empty_property_dictionary = NameDictionary::New(
      isolate(), 1, AllocationType::kReadOnly, USE_CUSTOM_MINIMUM_CAPACITY);
  DCHECK(!empty_property_dictionary->HasSufficientCapacityToAdd(1));
  set_empty_property_dictionary(*empty_property_dictionary);

  set_public_symbol_table(*empty_property_dictionary);
  set_api_symbol_table(*empty_property_dictionary);
  set_api_private_symbol_table(*empty_property_dictionary);

  set_number_string_cache(*factory->NewFixedArray(
      kInitialNumberStringCacheSize * 2, AllocationType::kOld));

  set_basic_block_profiling_data(ArrayList::cast(roots.empty_fixed_array()));

  // Allocate cache for string split and regexp-multiple.
  set_string_split_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, AllocationType::kOld));
  set_regexp_multiple_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, AllocationType::kOld));

  // Allocate FeedbackCell for builtins.
  Handle<FeedbackCell> many_closures_cell =
      factory->NewManyClosuresCell(factory->undefined_value());
  set_many_closures_cell(*many_closures_cell);

  set_detached_contexts(roots.empty_weak_array_list());
  set_retaining_path_targets(roots.empty_weak_array_list());

  set_feedback_vectors_for_profiling_tools(roots.undefined_value());
  set_pending_optimize_for_test_bytecode(roots.undefined_value());
  set_shared_wasm_memories(roots.empty_weak_array_list());

  set_script_list(roots.empty_weak_array_list());

  Handle<NumberDictionary> slow_element_dictionary = NumberDictionary::New(
      isolate(), 1, AllocationType::kReadOnly, USE_CUSTOM_MINIMUM_CAPACITY);
  DCHECK(!slow_element_dictionary->HasSufficientCapacityToAdd(1));
  set_empty_slow_element_dictionary(*slow_element_dictionary);

  set_materialized_objects(*factory->NewFixedArray(0, AllocationType::kOld));

  // Handling of script id generation is in Heap::NextScriptId().
  set_last_script_id(Smi::FromInt(v8::UnboundScript::kNoScriptId));
  set_last_debugging_id(Smi::FromInt(DebugInfo::kNoDebuggingId));
  set_next_template_serial_number(Smi::zero());

  // Allocate the empty OrderedHashMap.
  Handle<OrderedHashMap> empty_ordered_hash_map =
      OrderedHashMap::AllocateEmpty(isolate(), AllocationType::kReadOnly)
          .ToHandleChecked();
  set_empty_ordered_hash_map(*empty_ordered_hash_map);

  // Allocate the empty OrderedHashSet.
  Handle<OrderedHashSet> empty_ordered_hash_set =
      OrderedHashSet::AllocateEmpty(isolate(), AllocationType::kReadOnly)
          .ToHandleChecked();
  set_empty_ordered_hash_set(*empty_ordered_hash_set);

  // Allocate the empty OrderedNameDictionary
  Handle<OrderedNameDictionary> empty_ordered_property_dictionary =
      OrderedNameDictionary::AllocateEmpty(isolate(), AllocationType::kReadOnly)
          .ToHandleChecked();
  set_empty_ordered_property_dictionary(*empty_ordered_property_dictionary);

  // Allocate the empty SwissNameDictionary
  Handle<SwissNameDictionary> empty_swiss_property_dictionary =
      factory->CreateCanonicalEmptySwissNameDictionary();
  set_empty_swiss_property_dictionary(*empty_swiss_property_dictionary);

  // Allocate the empty FeedbackMetadata.
  Handle<FeedbackMetadata> empty_feedback_metadata =
      factory->NewFeedbackMetadata(0, 0, AllocationType::kReadOnly);
  set_empty_feedback_metadata(*empty_feedback_metadata);

  // Canonical scope arrays.
  Handle<ScopeInfo> global_this_binding =
      ScopeInfo::CreateGlobalThisBinding(isolate());
  set_global_this_binding_scope_info(*global_this_binding);

  Handle<ScopeInfo> empty_function =
      ScopeInfo::CreateForEmptyFunction(isolate());
  set_empty_function_scope_info(*empty_function);

  Handle<ScopeInfo> native_scope_info =
      ScopeInfo::CreateForNativeContext(isolate());
  set_native_scope_info(*native_scope_info);

  // Allocate the empty script.
  Handle<Script> script = factory->NewScript(factory->empty_string());
  script->set_type(Script::TYPE_NATIVE);
  // This is used for exceptions thrown with no stack frames. Such exceptions
  // can be shared everywhere.
  script->set_origin_options(ScriptOriginOptions(true, false));
  set_empty_script(*script);

  // Protectors
  set_array_buffer_detaching_protector(*factory->NewProtector());
  set_array_constructor_protector(*factory->NewProtector());
  set_array_iterator_protector(*factory->NewProtector());
  set_array_species_protector(*factory->NewProtector());
  set_is_concat_spreadable_protector(*factory->NewProtector());
  set_map_iterator_protector(*factory->NewProtector());
  set_no_elements_protector(*factory->NewProtector());
  set_mega_dom_protector(*factory->NewProtector());
  set_promise_hook_protector(*factory->NewProtector());
  set_promise_resolve_protector(*factory->NewProtector());
  set_promise_species_protector(*factory->NewProtector());
  set_promise_then_protector(*factory->NewProtector());
  set_regexp_species_protector(*factory->NewProtector());
  set_set_iterator_protector(*factory->NewProtector());
  set_string_iterator_protector(*factory->NewProtector());
  set_string_length_protector(*factory->NewProtector());
  set_typed_array_species_protector(*factory->NewProtector());

  set_serialized_objects(roots.empty_fixed_array());
  set_serialized_global_proxy_sizes(roots.empty_fixed_array());

  /* Canonical off-heap trampoline data */
  set_off_heap_trampoline_relocation_info(
      *Builtins::GenerateOffHeapTrampolineRelocInfo(isolate_));

  set_trampoline_trivial_code_data_container(
      *isolate()->factory()->NewCodeDataContainer(0,
                                                  AllocationType::kReadOnly));

  set_trampoline_promise_rejection_code_data_container(
      *isolate()->factory()->NewCodeDataContainer(
          Code::IsPromiseRejectionField::encode(true),
          AllocationType::kReadOnly));

  // Evaluate the hash values which will then be cached in the strings.
  isolate()->factory()->zero_string()->EnsureHash();
  isolate()->factory()->one_string()->EnsureHash();

  // Initialize builtins constants table.
  set_builtins_constants_table(roots.empty_fixed_array());

  // Initialize descriptor cache.
  isolate_->descriptor_lookup_cache()->Clear();

  // Initialize compilation cache.
  isolate_->compilation_cache()->Clear();

  // Create internal SharedFunctionInfos.

  // Async functions:
  {
    Handle<SharedFunctionInfo> info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncFunctionAwaitRejectClosure, 1);
    set_async_function_await_reject_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncFunctionAwaitResolveClosure, 1);
    set_async_function_await_resolve_shared_fun(*info);
  }

  // Async generators:
  {
    Handle<SharedFunctionInfo> info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncGeneratorAwaitResolveClosure, 1);
    set_async_generator_await_resolve_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncGeneratorAwaitRejectClosure, 1);
    set_async_generator_await_reject_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncGeneratorYieldResolveClosure, 1);
    set_async_generator_yield_resolve_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncGeneratorReturnResolveClosure, 1);
    set_async_generator_return_resolve_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncGeneratorReturnClosedResolveClosure, 1);
    set_async_generator_return_closed_resolve_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate(), Builtins::kAsyncGeneratorReturnClosedRejectClosure, 1);
    set_async_generator_return_closed_reject_shared_fun(*info);
  }

  // AsyncIterator:
  {
    Handle<SharedFunctionInfo> info = CreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncIteratorValueUnwrap, 1);
    set_async_iterator_value_unwrap_shared_fun(*info);
  }

  // Promises:
  {
    Handle<SharedFunctionInfo> info = CreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseCapabilityDefaultResolve, 1,
        FunctionKind::kConciseMethod);
    info->set_native(true);
    info->set_function_map_index(
        Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
    set_promise_capability_default_resolve_shared_fun(*info);

    info = CreateSharedFunctionInfo(isolate_,
                                    Builtins::kPromiseCapabilityDefaultReject,
                                    1, FunctionKind::kConciseMethod);
    info->set_native(true);
    info->set_function_map_index(
        Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
    set_promise_capability_default_reject_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseGetCapabilitiesExecutor, 2);
    set_promise_get_capabilities_executor_shared_fun(*info);
  }

  // Promises / finally:
  {
    Handle<SharedFunctionInfo> info =
        CreateSharedFunctionInfo(isolate(), Builtins::kPromiseThenFinally, 1);
    info->set_native(true);
    set_promise_then_finally_shared_fun(*info);

    info =
        CreateSharedFunctionInfo(isolate(), Builtins::kPromiseCatchFinally, 1);
    info->set_native(true);
    set_promise_catch_finally_shared_fun(*info);

    info = CreateSharedFunctionInfo(isolate(),
                                    Builtins::kPromiseValueThunkFinally, 0);
    set_promise_value_thunk_finally_shared_fun(*info);

    info = CreateSharedFunctionInfo(isolate(), Builtins::kPromiseThrowerFinally,
                                    0);
    set_promise_thrower_finally_shared_fun(*info);
  }

  // Promise combinators:
  {
    Handle<SharedFunctionInfo> info = CreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseAllResolveElementClosure, 1);
    set_promise_all_resolve_element_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseAllSettledResolveElementClosure, 1);
    set_promise_all_settled_resolve_element_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseAllSettledRejectElementClosure, 1);
    set_promise_all_settled_reject_element_shared_fun(*info);

    info = CreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseAnyRejectElementClosure, 1);
    set_promise_any_reject_element_shared_fun(*info);
  }

  // ProxyRevoke:
  {
    Handle<SharedFunctionInfo> info =
        CreateSharedFunctionInfo(isolate_, Builtins::kProxyRevoke, 0);
    set_proxy_revoke_shared_fun(*info);
  }
}

void Heap::CreateInternalAccessorInfoObjects() {
  Isolate* isolate = this->isolate();
  HandleScope scope(isolate);
  Handle<AccessorInfo> accessor_info;

#define INIT_ACCESSOR_INFO(_, accessor_name, AccessorName, ...) \
  accessor_info = Accessors::Make##AccessorName##Info(isolate); \
  roots_table()[RootIndex::k##AccessorName##Accessor] = accessor_info->ptr();
  ACCESSOR_INFO_LIST_GENERATOR(INIT_ACCESSOR_INFO, /* not used */)
#undef INIT_ACCESSOR_INFO

#define INIT_SIDE_EFFECT_FLAG(_, accessor_name, AccessorName, GetterType, \
                              SetterType)                                 \
  AccessorInfo::cast(                                                     \
      Object(roots_table()[RootIndex::k##AccessorName##Accessor]))        \
      .set_getter_side_effect_type(SideEffectType::GetterType);           \
  AccessorInfo::cast(                                                     \
      Object(roots_table()[RootIndex::k##AccessorName##Accessor]))        \
      .set_setter_side_effect_type(SideEffectType::SetterType);
  ACCESSOR_INFO_LIST_GENERATOR(INIT_SIDE_EFFECT_FLAG, /* not used */)
#undef INIT_SIDE_EFFECT_FLAG
}

}  // namespace internal
}  // namespace v8

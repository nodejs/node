// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/accessors.h"
#include "src/ast/context-slot-cache.h"
#include "src/compilation-cache.h"
#include "src/contexts.h"
#include "src/factory.h"
#include "src/heap-symbols.h"
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
#include "src/objects/map.h"
#include "src/objects/module.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"
#include "src/regexp/jsregexp.h"

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

  set_native_contexts_list(undefined_value());
  set_allocation_sites_list(undefined_value());

  return true;
}

const Heap::StringTypeTable Heap::string_type_table[] = {
#define STRING_TYPE_ELEMENT(type, size, name, camel_name) \
  {type, size, k##camel_name##MapRootIndex},
    STRING_TYPE_LIST(STRING_TYPE_ELEMENT)
#undef STRING_TYPE_ELEMENT
};

const Heap::ConstantStringTable Heap::constant_string_table[] = {
    {"", kempty_stringRootIndex},
#define CONSTANT_STRING_ELEMENT(name, contents) {contents, k##name##RootIndex},
    INTERNALIZED_STRING_LIST(CONSTANT_STRING_ELEMENT)
#undef CONSTANT_STRING_ELEMENT
};

const Heap::StructTable Heap::struct_table[] = {
#define STRUCT_TABLE_ELEMENT(NAME, Name, name) \
  {NAME##_TYPE, Name::kSize, k##Name##MapRootIndex},
    STRUCT_LIST(STRUCT_TABLE_ELEMENT)
#undef STRUCT_TABLE_ELEMENT

#define DATA_HANDLER_ELEMENT(NAME, Name, Size, name) \
  {NAME##_TYPE, Name::kSizeWithData##Size, k##Name##Size##MapRootIndex},
        DATA_HANDLER_LIST(DATA_HANDLER_ELEMENT)
#undef DATA_HANDLER_ELEMENT
};

namespace {

void FinalizePartialMap(Heap* heap, Map* map) {
  map->set_dependent_code(DependentCode::cast(heap->empty_fixed_array()));
  map->set_raw_transitions(Smi::kZero);
  map->set_instance_descriptors(heap->empty_descriptor_array());
  if (FLAG_unbox_double_fields) {
    map->set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  map->set_prototype(heap->null_value());
  map->set_constructor_or_backpointer(heap->null_value());
}

}  // namespace

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

  {  // Partial map allocation
#define ALLOCATE_PARTIAL_MAP(instance_type, size, field_name)                \
    {                                                                          \
      Map* map;                                                                \
      if (!AllocatePartialMap((instance_type), (size)).To(&map)) return false; \
      set_##field_name##_map(map);                                             \
    }

    ALLOCATE_PARTIAL_MAP(FIXED_ARRAY_TYPE, kVariableSizeSentinel, fixed_array);
    ALLOCATE_PARTIAL_MAP(FIXED_ARRAY_TYPE, kVariableSizeSentinel,
                         fixed_cow_array)
    DCHECK_NE(fixed_array_map(), fixed_cow_array_map());

    ALLOCATE_PARTIAL_MAP(DESCRIPTOR_ARRAY_TYPE, kVariableSizeSentinel,
                         descriptor_array)

    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, undefined);
    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, null);
    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, the_hole);

#undef ALLOCATE_PARTIAL_MAP
  }

  // Allocate the empty array.
  {
    AllocationResult allocation = AllocateEmptyFixedArray();
    if (!allocation.To(&obj)) return false;
  }
  set_empty_fixed_array(FixedArray::cast(obj));

  {
    AllocationResult allocation = Allocate(null_map(), OLD_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_null_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kNull);

  {
    AllocationResult allocation = Allocate(undefined_map(), OLD_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_undefined_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kUndefined);
  DCHECK(!InNewSpace(undefined_value()));
  {
    AllocationResult allocation = Allocate(the_hole_map(), OLD_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_the_hole_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kTheHole);

  // Set preliminary exception sentinel value before actually initializing it.
  set_exception(null_value());

  // Setup the struct maps first (needed for the EnumCache).
  for (unsigned i = 0; i < arraysize(struct_table); i++) {
    const StructTable& entry = struct_table[i];
    Map* map;
    if (!AllocatePartialMap(entry.type, entry.size).To(&map)) return false;
    roots_[entry.index] = map;
  }

  // Allocate the empty enum cache.
  {
    AllocationResult allocation = Allocate(tuple2_map(), OLD_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_empty_enum_cache(EnumCache::cast(obj));
  EnumCache::cast(obj)->set_keys(empty_fixed_array());
  EnumCache::cast(obj)->set_indices(empty_fixed_array());

  // Allocate the empty descriptor array.
  {
    STATIC_ASSERT(DescriptorArray::kFirstIndex != 0);
    AllocationResult allocation =
        AllocateUninitializedFixedArray(DescriptorArray::kFirstIndex, TENURED);
    if (!allocation.To(&obj)) return false;
  }
  obj->set_map_no_write_barrier(descriptor_array_map());
  set_empty_descriptor_array(DescriptorArray::cast(obj));
  DescriptorArray::cast(obj)->set(DescriptorArray::kDescriptorLengthIndex,
                                  Smi::kZero);
  DescriptorArray::cast(obj)->set(DescriptorArray::kEnumCacheIndex,
                                  empty_enum_cache());

  // Fix the instance_descriptors for the existing maps.
  FinalizePartialMap(this, meta_map());
  FinalizePartialMap(this, fixed_array_map());
  FinalizePartialMap(this, fixed_cow_array_map());
  FinalizePartialMap(this, descriptor_array_map());
  FinalizePartialMap(this, undefined_map());
  undefined_map()->set_is_undetectable(true);
  FinalizePartialMap(this, null_map());
  null_map()->set_is_undetectable(true);
  FinalizePartialMap(this, the_hole_map());
  for (unsigned i = 0; i < arraysize(struct_table); ++i) {
    const StructTable& entry = struct_table[i];
    FinalizePartialMap(this, Map::cast(roots_[entry.index]));
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
      field_name##_map()->SetConstructorFunctionIndex(            \
          (constructor_function_index));                          \
    }

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, scope_info)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, module_info)
    ALLOCATE_VARSIZE_MAP(FEEDBACK_VECTOR_TYPE, feedback_vector)
    ALLOCATE_PRIMITIVE_MAP(HEAP_NUMBER_TYPE, HeapNumber::kSize, heap_number,
                           Context::NUMBER_FUNCTION_INDEX)
    ALLOCATE_MAP(MUTABLE_HEAP_NUMBER_TYPE, HeapNumber::kSize,
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
          AllocateMap(SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE,
                      ExternalOneByteString::kShortSize);
      if (!allocation.To(&obj)) return false;
      Map* map = Map::cast(obj);
      map->SetConstructorFunctionIndex(Context::STRING_FUNCTION_INDEX);
      set_native_source_string_map(map);
    }

    ALLOCATE_VARSIZE_MAP(FIXED_DOUBLE_ARRAY_TYPE, fixed_double_array)
    fixed_double_array_map()->set_elements_kind(HOLEY_DOUBLE_ELEMENTS);
    ALLOCATE_VARSIZE_MAP(BYTE_ARRAY_TYPE, byte_array)
    ALLOCATE_VARSIZE_MAP(BYTECODE_ARRAY_TYPE, bytecode_array)
    ALLOCATE_VARSIZE_MAP(FREE_SPACE_TYPE, free_space)
    ALLOCATE_VARSIZE_MAP(PROPERTY_ARRAY_TYPE, property_array)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_HASH_MAP_TYPE, small_ordered_hash_map)
    ALLOCATE_VARSIZE_MAP(SMALL_ORDERED_HASH_SET_TYPE, small_ordered_hash_set)

#define ALLOCATE_FIXED_TYPED_ARRAY_MAP(Type, type, TYPE, ctype, size) \
    ALLOCATE_VARSIZE_MAP(FIXED_##TYPE##_ARRAY_TYPE, fixed_##type##_array)

    TYPED_ARRAYS(ALLOCATE_FIXED_TYPED_ARRAY_MAP)
#undef ALLOCATE_FIXED_TYPED_ARRAY_MAP

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, sloppy_arguments_elements)

    ALLOCATE_VARSIZE_MAP(CODE_TYPE, code)

    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, cell)
    ALLOCATE_MAP(PROPERTY_CELL_TYPE, PropertyCell::kSize, global_property_cell)
    ALLOCATE_MAP(WEAK_CELL_TYPE, WeakCell::kSize, weak_cell)
    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, no_closures_cell)
    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, one_closure_cell)
    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, many_closures_cell)
    ALLOCATE_MAP(FILLER_TYPE, kPointerSize, one_pointer_filler)
    ALLOCATE_MAP(FILLER_TYPE, 2 * kPointerSize, two_pointer_filler)

    ALLOCATE_VARSIZE_MAP(TRANSITION_ARRAY_TYPE, transition_array)

    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, hash_table)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, ordered_hash_map)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, ordered_hash_set)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, name_dictionary)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, global_dictionary)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, number_dictionary)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, string_table)
    ALLOCATE_VARSIZE_MAP(HASH_TABLE_TYPE, weak_hash_table)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, array_list)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, function_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, catch_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, with_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, debug_evaluate_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, block_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, module_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, eval_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, script_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, script_context_table)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, native_context)
    native_context_map()->set_visitor_id(kVisitNativeContext);

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
    AllocationResult allocation = AllocateEmptyScopeInfo();
    if (!allocation.To(&obj)) return false;
  }

  set_empty_scope_info(ScopeInfo::cast(obj));
  {
    AllocationResult allocation = Allocate(boolean_map(), OLD_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_true_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kTrue);

  {
    AllocationResult allocation = Allocate(boolean_map(), OLD_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_false_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kFalse);

  { // Empty arrays
    {
      ByteArray * byte_array;
      if (!AllocateByteArray(0, TENURED).To(&byte_array)) return false;
      set_empty_byte_array(byte_array);
    }

    {
      PropertyArray* property_array;
      if (!AllocatePropertyArray(0, TENURED).To(&property_array)) return false;
      set_empty_property_array(property_array);
    }

#define ALLOCATE_EMPTY_FIXED_TYPED_ARRAY(Type, type, TYPE, ctype, size)   \
    {                                                                     \
      FixedTypedArrayBase* obj;                                           \
      if (!AllocateEmptyFixedTypedArray(kExternal##Type##Array).To(&obj)) \
        return false;                                                     \
      set_empty_fixed_##type##_array(obj);                                \
    }

    TYPED_ARRAYS(ALLOCATE_EMPTY_FIXED_TYPED_ARRAY)
#undef ALLOCATE_EMPTY_FIXED_TYPED_ARRAY
  }
  DCHECK(!InNewSpace(empty_fixed_array()));
  return true;
}

void Heap::CreateApiObjects() {
  Isolate* isolate = this->isolate();
  HandleScope scope(isolate);

  set_message_listeners(*TemplateList::New(isolate, 2));

  Handle<InterceptorInfo> info = Handle<InterceptorInfo>::cast(
      isolate->factory()->NewStruct(INTERCEPTOR_INFO_TYPE, TENURED));
  info->set_flags(0);
  set_noop_interceptor_info(*info);
}

void Heap::CreateInitialObjects() {
  HandleScope scope(isolate());
  Factory* factory = isolate()->factory();

  // The -0 value must be set before NewNumber works.
  set_minus_zero_value(*factory->NewHeapNumber(-0.0, IMMUTABLE, TENURED));
  DCHECK(std::signbit(minus_zero_value()->Number()));

  set_nan_value(*factory->NewHeapNumber(
      std::numeric_limits<double>::quiet_NaN(), IMMUTABLE, TENURED));
  set_hole_nan_value(
      *factory->NewHeapNumberFromBits(kHoleNanInt64, IMMUTABLE, TENURED));
  set_infinity_value(*factory->NewHeapNumber(V8_INFINITY, IMMUTABLE, TENURED));
  set_minus_infinity_value(
      *factory->NewHeapNumber(-V8_INFINITY, IMMUTABLE, TENURED));

  // Allocate initial string table.
  set_string_table(*StringTable::New(isolate(), kInitialStringTableSize));

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

  for (unsigned i = 0; i < arraysize(constant_string_table); i++) {
    Handle<String> str =
        factory->InternalizeUtf8String(constant_string_table[i].contents);
    roots_[constant_string_table[i].index] = *str;
  }

  // Create the code_stubs dictionary. The initial size is set to avoid
  // expanding the dictionary during bootstrapping.
  set_code_stubs(*NumberDictionary::New(isolate(), 128));

  {
    HandleScope scope(isolate());
#define SYMBOL_INIT(name)                                              \
  {                                                                    \
    Handle<Symbol> symbol(isolate()->factory()->NewPrivateSymbol());   \
    roots_[k##name##RootIndex] = *symbol;                              \
  }
    PRIVATE_SYMBOL_LIST(SYMBOL_INIT)
#undef SYMBOL_INIT
  }

  {
    HandleScope scope(isolate());
#define SYMBOL_INIT(name, description)                                      \
  Handle<Symbol> name = factory->NewSymbol();                               \
  Handle<String> name##d = factory->NewStringFromStaticChars(#description); \
  name->set_name(*name##d);                                                 \
  roots_[k##name##RootIndex] = *name;
    PUBLIC_SYMBOL_LIST(SYMBOL_INIT)
#undef SYMBOL_INIT

#define SYMBOL_INIT(name, description)                                      \
  Handle<Symbol> name = factory->NewSymbol();                               \
  Handle<String> name##d = factory->NewStringFromStaticChars(#description); \
  name->set_is_well_known_symbol(true);                                     \
  name->set_name(*name##d);                                                 \
  roots_[k##name##RootIndex] = *name;
    WELL_KNOWN_SYMBOL_LIST(SYMBOL_INIT)
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

  // Allocate cache for single character one byte strings.
  set_single_character_string_cache(
      *factory->NewFixedArray(String::kMaxOneByteCharCode + 1, TENURED));

  // Allocate cache for string split and regexp-multiple.
  set_string_split_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, TENURED));
  set_regexp_multiple_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, TENURED));

  set_undefined_cell(*factory->NewCell(factory->undefined_value()));

  // Microtask queue uses the empty fixed array as a sentinel for "empty".
  // Number of queued microtasks stored in Isolate::pending_microtask_count().
  set_microtask_queue(empty_fixed_array());

  {
    Handle<FixedArray> empty_sloppy_arguments_elements =
        factory->NewFixedArray(2, TENURED);
    empty_sloppy_arguments_elements->set_map_after_allocation(
        sloppy_arguments_elements_map(), SKIP_WRITE_BARRIER);
    set_empty_sloppy_arguments_elements(*empty_sloppy_arguments_elements);
  }

  {
    Handle<WeakCell> cell = factory->NewWeakCell(factory->undefined_value());
    set_empty_weak_cell(*cell);
    cell->clear();
  }

  set_detached_contexts(empty_fixed_array());
  set_retained_maps(ArrayList::cast(empty_fixed_array()));
  set_retaining_path_targets(undefined_value());

  set_weak_object_to_code_table(*WeakHashTable::New(isolate(), 16, TENURED));

  set_weak_new_space_object_to_code_list(*ArrayList::New(isolate(), 16));

  set_feedback_vectors_for_profiling_tools(undefined_value());

  set_script_list(Smi::kZero);

  Handle<NumberDictionary> slow_element_dictionary =
      NumberDictionary::New(isolate(), 1, TENURED, USE_CUSTOM_MINIMUM_CAPACITY);
  DCHECK(!slow_element_dictionary->HasSufficientCapacityToAdd(1));
  slow_element_dictionary->set_requires_slow_elements();
  set_empty_slow_element_dictionary(*slow_element_dictionary);

  set_materialized_objects(*factory->NewFixedArray(0, TENURED));

  // Handling of script id generation is in Heap::NextScriptId().
  set_last_script_id(Smi::FromInt(v8::UnboundScript::kNoScriptId));
  set_next_template_serial_number(Smi::kZero);

  // Allocate the empty OrderedHashMap.
  Handle<FixedArray> empty_ordered_hash_map =
      factory->NewFixedArray(OrderedHashMap::kHashTableStartIndex, TENURED);
  empty_ordered_hash_map->set_map_no_write_barrier(
      *factory->ordered_hash_map_map());
  for (int i = 0; i < empty_ordered_hash_map->length(); ++i) {
    empty_ordered_hash_map->set(i, Smi::kZero);
  }
  set_empty_ordered_hash_map(*empty_ordered_hash_map);

  // Allocate the empty OrderedHashSet.
  Handle<FixedArray> empty_ordered_hash_set =
      factory->NewFixedArray(OrderedHashSet::kHashTableStartIndex, TENURED);
  empty_ordered_hash_set->set_map_no_write_barrier(
      *factory->ordered_hash_set_map());
  for (int i = 0; i < empty_ordered_hash_set->length(); ++i) {
    empty_ordered_hash_set->set(i, Smi::kZero);
  }
  set_empty_ordered_hash_set(*empty_ordered_hash_set);

  // Allocate the empty script.
  Handle<Script> script = factory->NewScript(factory->empty_string());
  script->set_type(Script::TYPE_NATIVE);
  set_empty_script(*script);

  Handle<Cell> array_constructor_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_array_constructor_protector(*array_constructor_cell);

  Handle<PropertyCell> cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_no_elements_protector(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(the_hole_value());
  set_empty_property_cell(*cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_array_iterator_protector(*cell);

  Handle<Cell> is_concat_spreadable_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_is_concat_spreadable_protector(*is_concat_spreadable_cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_species_protector(*cell);

  Handle<Cell> string_length_overflow_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_string_length_protector(*string_length_overflow_cell);

  Handle<Cell> fast_array_iteration_cell = factory->NewCell(
      handle(Smi::FromInt(Isolate::kProtectorValid), isolate()));
  set_fast_array_iteration_protector(*fast_array_iteration_cell);

  cell = factory->NewPropertyCell(factory->empty_string());
  cell->set_value(Smi::FromInt(Isolate::kProtectorValid));
  set_array_buffer_neutering_protector(*cell);

  set_serialized_objects(empty_fixed_array());
  set_serialized_global_proxy_sizes(empty_fixed_array());

  set_weak_stack_trace_list(Smi::kZero);

  set_noscript_shared_function_infos(Smi::kZero);

  STATIC_ASSERT(interpreter::BytecodeOperands::kOperandScaleCount == 3);
  set_deserialize_lazy_handler(Smi::kZero);
  set_deserialize_lazy_handler_wide(Smi::kZero);
  set_deserialize_lazy_handler_extra_wide(Smi::kZero);

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

#define INIT_ACCESSOR_INFO(accessor_name, AccessorName)        \
  acessor_info = Accessors::Make##AccessorName##Info(isolate); \
  roots_[k##AccessorName##AccessorRootIndex] = *acessor_info;
  ACCESSOR_INFO_LIST(INIT_ACCESSOR_INFO)
#undef INIT_ACCESSOR_INFO
}

}  // namespace internal
}  // namespace v8

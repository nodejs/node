// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "bootstrapper.h"
#include "execution.h"
#include "global-handles.h"
#include "ic-inl.h"
#include "natives.h"
#include "platform.h"
#include "runtime.h"
#include "serialize.h"
#include "stub-cache.h"
#include "v8threads.h"

namespace v8 {
namespace internal {


// -----------------------------------------------------------------------------
// Coding of external references.

// The encoding of an external reference. The type is in the high word.
// The id is in the low word.
static uint32_t EncodeExternal(TypeCode type, uint16_t id) {
  return static_cast<uint32_t>(type) << 16 | id;
}


static int* GetInternalPointer(StatsCounter* counter) {
  // All counters refer to dummy_counter, if deserializing happens without
  // setting up counters.
  static int dummy_counter = 0;
  return counter->Enabled() ? counter->GetInternalPointer() : &dummy_counter;
}


ExternalReferenceTable* ExternalReferenceTable::instance(Isolate* isolate) {
  ExternalReferenceTable* external_reference_table =
      isolate->external_reference_table();
  if (external_reference_table == NULL) {
    external_reference_table = new ExternalReferenceTable(isolate);
    isolate->set_external_reference_table(external_reference_table);
  }
  return external_reference_table;
}


void ExternalReferenceTable::AddFromId(TypeCode type,
                                       uint16_t id,
                                       const char* name,
                                       Isolate* isolate) {
  Address address;
  switch (type) {
    case C_BUILTIN: {
      ExternalReference ref(static_cast<Builtins::CFunctionId>(id), isolate);
      address = ref.address();
      break;
    }
    case BUILTIN: {
      ExternalReference ref(static_cast<Builtins::Name>(id), isolate);
      address = ref.address();
      break;
    }
    case RUNTIME_FUNCTION: {
      ExternalReference ref(static_cast<Runtime::FunctionId>(id), isolate);
      address = ref.address();
      break;
    }
    case IC_UTILITY: {
      ExternalReference ref(IC_Utility(static_cast<IC::UtilityId>(id)),
                            isolate);
      address = ref.address();
      break;
    }
    default:
      UNREACHABLE();
      return;
  }
  Add(address, type, id, name);
}


void ExternalReferenceTable::Add(Address address,
                                 TypeCode type,
                                 uint16_t id,
                                 const char* name) {
  ASSERT_NE(NULL, address);
  ExternalReferenceEntry entry;
  entry.address = address;
  entry.code = EncodeExternal(type, id);
  entry.name = name;
  ASSERT_NE(0, entry.code);
  refs_.Add(entry);
  if (id > max_id_[type]) max_id_[type] = id;
}


void ExternalReferenceTable::PopulateTable(Isolate* isolate) {
  for (int type_code = 0; type_code < kTypeCodeCount; type_code++) {
    max_id_[type_code] = 0;
  }

  // The following populates all of the different type of external references
  // into the ExternalReferenceTable.
  //
  // NOTE: This function was originally 100k of code.  It has since been
  // rewritten to be mostly table driven, as the callback macro style tends to
  // very easily cause code bloat.  Please be careful in the future when adding
  // new references.

  struct RefTableEntry {
    TypeCode type;
    uint16_t id;
    const char* name;
  };

  static const RefTableEntry ref_table[] = {
  // Builtins
#define DEF_ENTRY_C(name, ignored) \
  { C_BUILTIN, \
    Builtins::c_##name, \
    "Builtins::" #name },

  BUILTIN_LIST_C(DEF_ENTRY_C)
#undef DEF_ENTRY_C

#define DEF_ENTRY_C(name, ignored) \
  { BUILTIN, \
    Builtins::k##name, \
    "Builtins::" #name },
#define DEF_ENTRY_A(name, kind, state, extra) DEF_ENTRY_C(name, ignored)

  BUILTIN_LIST_C(DEF_ENTRY_C)
  BUILTIN_LIST_A(DEF_ENTRY_A)
  BUILTIN_LIST_DEBUG_A(DEF_ENTRY_A)
#undef DEF_ENTRY_C
#undef DEF_ENTRY_A

  // Runtime functions
#define RUNTIME_ENTRY(name, nargs, ressize) \
  { RUNTIME_FUNCTION, \
    Runtime::k##name, \
    "Runtime::" #name },

  RUNTIME_FUNCTION_LIST(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY

  // IC utilities
#define IC_ENTRY(name) \
  { IC_UTILITY, \
    IC::k##name, \
    "IC::" #name },

  IC_UTIL_LIST(IC_ENTRY)
#undef IC_ENTRY
  };  // end of ref_table[].

  for (size_t i = 0; i < ARRAY_SIZE(ref_table); ++i) {
    AddFromId(ref_table[i].type,
              ref_table[i].id,
              ref_table[i].name,
              isolate);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Debug addresses
  Add(Debug_Address(Debug::k_after_break_target_address).address(isolate),
      DEBUG_ADDRESS,
      Debug::k_after_break_target_address << kDebugIdShift,
      "Debug::after_break_target_address()");
  Add(Debug_Address(Debug::k_debug_break_slot_address).address(isolate),
      DEBUG_ADDRESS,
      Debug::k_debug_break_slot_address << kDebugIdShift,
      "Debug::debug_break_slot_address()");
  Add(Debug_Address(Debug::k_debug_break_return_address).address(isolate),
      DEBUG_ADDRESS,
      Debug::k_debug_break_return_address << kDebugIdShift,
      "Debug::debug_break_return_address()");
  Add(Debug_Address(Debug::k_restarter_frame_function_pointer).address(isolate),
      DEBUG_ADDRESS,
      Debug::k_restarter_frame_function_pointer << kDebugIdShift,
      "Debug::restarter_frame_function_pointer_address()");
#endif

  // Stat counters
  struct StatsRefTableEntry {
    StatsCounter* (Counters::*counter)();
    uint16_t id;
    const char* name;
  };

  const StatsRefTableEntry stats_ref_table[] = {
#define COUNTER_ENTRY(name, caption) \
  { &Counters::name,    \
    Counters::k_##name, \
    "Counters::" #name },

  STATS_COUNTER_LIST_1(COUNTER_ENTRY)
  STATS_COUNTER_LIST_2(COUNTER_ENTRY)
#undef COUNTER_ENTRY
  };  // end of stats_ref_table[].

  Counters* counters = isolate->counters();
  for (size_t i = 0; i < ARRAY_SIZE(stats_ref_table); ++i) {
    Add(reinterpret_cast<Address>(GetInternalPointer(
            (counters->*(stats_ref_table[i].counter))())),
        STATS_COUNTER,
        stats_ref_table[i].id,
        stats_ref_table[i].name);
  }

  // Top addresses

  const char* AddressNames[] = {
#define BUILD_NAME_LITERAL(CamelName, hacker_name)      \
    "Isolate::" #hacker_name "_address",
    FOR_EACH_ISOLATE_ADDRESS_NAME(BUILD_NAME_LITERAL)
    NULL
#undef C
  };

  for (uint16_t i = 0; i < Isolate::kIsolateAddressCount; ++i) {
    Add(isolate->get_address_from_id((Isolate::AddressId)i),
        TOP_ADDRESS, i, AddressNames[i]);
  }

  // Accessors
#define ACCESSOR_DESCRIPTOR_DECLARATION(name) \
  Add((Address)&Accessors::name, \
      ACCESSOR, \
      Accessors::k##name, \
      "Accessors::" #name);

  ACCESSOR_DESCRIPTOR_LIST(ACCESSOR_DESCRIPTOR_DECLARATION)
#undef ACCESSOR_DESCRIPTOR_DECLARATION

  StubCache* stub_cache = isolate->stub_cache();

  // Stub cache tables
  Add(stub_cache->key_reference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE,
      1,
      "StubCache::primary_->key");
  Add(stub_cache->value_reference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE,
      2,
      "StubCache::primary_->value");
  Add(stub_cache->map_reference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE,
      3,
      "StubCache::primary_->map");
  Add(stub_cache->key_reference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE,
      4,
      "StubCache::secondary_->key");
  Add(stub_cache->value_reference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE,
      5,
      "StubCache::secondary_->value");
  Add(stub_cache->map_reference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE,
      6,
      "StubCache::secondary_->map");

  // Runtime entries
  Add(ExternalReference::perform_gc_function(isolate).address(),
      RUNTIME_ENTRY,
      1,
      "Runtime::PerformGC");
  Add(ExternalReference::fill_heap_number_with_random_function(
          isolate).address(),
      RUNTIME_ENTRY,
      2,
      "V8::FillHeapNumberWithRandom");
  Add(ExternalReference::random_uint32_function(isolate).address(),
      RUNTIME_ENTRY,
      3,
      "V8::Random");
  Add(ExternalReference::delete_handle_scope_extensions(isolate).address(),
      RUNTIME_ENTRY,
      4,
      "HandleScope::DeleteExtensions");
  Add(ExternalReference::
          incremental_marking_record_write_function(isolate).address(),
      RUNTIME_ENTRY,
      5,
      "IncrementalMarking::RecordWrite");
  Add(ExternalReference::store_buffer_overflow_function(isolate).address(),
      RUNTIME_ENTRY,
      6,
      "StoreBuffer::StoreBufferOverflow");
  Add(ExternalReference::
          incremental_evacuation_record_write_function(isolate).address(),
      RUNTIME_ENTRY,
      7,
      "IncrementalMarking::RecordWrite");



  // Miscellaneous
  Add(ExternalReference::roots_array_start(isolate).address(),
      UNCLASSIFIED,
      3,
      "Heap::roots_array_start()");
  Add(ExternalReference::address_of_stack_limit(isolate).address(),
      UNCLASSIFIED,
      4,
      "StackGuard::address_of_jslimit()");
  Add(ExternalReference::address_of_real_stack_limit(isolate).address(),
      UNCLASSIFIED,
      5,
      "StackGuard::address_of_real_jslimit()");
#ifndef V8_INTERPRETED_REGEXP
  Add(ExternalReference::address_of_regexp_stack_limit(isolate).address(),
      UNCLASSIFIED,
      6,
      "RegExpStack::limit_address()");
  Add(ExternalReference::address_of_regexp_stack_memory_address(
          isolate).address(),
      UNCLASSIFIED,
      7,
      "RegExpStack::memory_address()");
  Add(ExternalReference::address_of_regexp_stack_memory_size(isolate).address(),
      UNCLASSIFIED,
      8,
      "RegExpStack::memory_size()");
  Add(ExternalReference::address_of_static_offsets_vector(isolate).address(),
      UNCLASSIFIED,
      9,
      "OffsetsVector::static_offsets_vector");
#endif  // V8_INTERPRETED_REGEXP
  Add(ExternalReference::new_space_start(isolate).address(),
      UNCLASSIFIED,
      10,
      "Heap::NewSpaceStart()");
  Add(ExternalReference::new_space_mask(isolate).address(),
      UNCLASSIFIED,
      11,
      "Heap::NewSpaceMask()");
  Add(ExternalReference::heap_always_allocate_scope_depth(isolate).address(),
      UNCLASSIFIED,
      12,
      "Heap::always_allocate_scope_depth()");
  Add(ExternalReference::new_space_allocation_limit_address(isolate).address(),
      UNCLASSIFIED,
      14,
      "Heap::NewSpaceAllocationLimitAddress()");
  Add(ExternalReference::new_space_allocation_top_address(isolate).address(),
      UNCLASSIFIED,
      15,
      "Heap::NewSpaceAllocationTopAddress()");
#ifdef ENABLE_DEBUGGER_SUPPORT
  Add(ExternalReference::debug_break(isolate).address(),
      UNCLASSIFIED,
      16,
      "Debug::Break()");
  Add(ExternalReference::debug_step_in_fp_address(isolate).address(),
      UNCLASSIFIED,
      17,
      "Debug::step_in_fp_addr()");
#endif
  Add(ExternalReference::double_fp_operation(Token::ADD, isolate).address(),
      UNCLASSIFIED,
      18,
      "add_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::SUB, isolate).address(),
      UNCLASSIFIED,
      19,
      "sub_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::MUL, isolate).address(),
      UNCLASSIFIED,
      20,
      "mul_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::DIV, isolate).address(),
      UNCLASSIFIED,
      21,
      "div_two_doubles");
  Add(ExternalReference::double_fp_operation(Token::MOD, isolate).address(),
      UNCLASSIFIED,
      22,
      "mod_two_doubles");
  Add(ExternalReference::compare_doubles(isolate).address(),
      UNCLASSIFIED,
      23,
      "compare_doubles");
#ifndef V8_INTERPRETED_REGEXP
  Add(ExternalReference::re_case_insensitive_compare_uc16(isolate).address(),
      UNCLASSIFIED,
      24,
      "NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()");
  Add(ExternalReference::re_check_stack_guard_state(isolate).address(),
      UNCLASSIFIED,
      25,
      "RegExpMacroAssembler*::CheckStackGuardState()");
  Add(ExternalReference::re_grow_stack(isolate).address(),
      UNCLASSIFIED,
      26,
      "NativeRegExpMacroAssembler::GrowStack()");
  Add(ExternalReference::re_word_character_map().address(),
      UNCLASSIFIED,
      27,
      "NativeRegExpMacroAssembler::word_character_map");
#endif  // V8_INTERPRETED_REGEXP
  // Keyed lookup cache.
  Add(ExternalReference::keyed_lookup_cache_keys(isolate).address(),
      UNCLASSIFIED,
      28,
      "KeyedLookupCache::keys()");
  Add(ExternalReference::keyed_lookup_cache_field_offsets(isolate).address(),
      UNCLASSIFIED,
      29,
      "KeyedLookupCache::field_offsets()");
  Add(ExternalReference::transcendental_cache_array_address(isolate).address(),
      UNCLASSIFIED,
      30,
      "TranscendentalCache::caches()");
  Add(ExternalReference::handle_scope_next_address().address(),
      UNCLASSIFIED,
      31,
      "HandleScope::next");
  Add(ExternalReference::handle_scope_limit_address().address(),
      UNCLASSIFIED,
      32,
      "HandleScope::limit");
  Add(ExternalReference::handle_scope_level_address().address(),
      UNCLASSIFIED,
      33,
      "HandleScope::level");
  Add(ExternalReference::new_deoptimizer_function(isolate).address(),
      UNCLASSIFIED,
      34,
      "Deoptimizer::New()");
  Add(ExternalReference::compute_output_frames_function(isolate).address(),
      UNCLASSIFIED,
      35,
      "Deoptimizer::ComputeOutputFrames()");
  Add(ExternalReference::address_of_min_int().address(),
      UNCLASSIFIED,
      36,
      "LDoubleConstant::min_int");
  Add(ExternalReference::address_of_one_half().address(),
      UNCLASSIFIED,
      37,
      "LDoubleConstant::one_half");
  Add(ExternalReference::isolate_address().address(),
      UNCLASSIFIED,
      38,
      "isolate");
  Add(ExternalReference::address_of_minus_zero().address(),
      UNCLASSIFIED,
      39,
      "LDoubleConstant::minus_zero");
  Add(ExternalReference::address_of_negative_infinity().address(),
      UNCLASSIFIED,
      40,
      "LDoubleConstant::negative_infinity");
  Add(ExternalReference::power_double_double_function(isolate).address(),
      UNCLASSIFIED,
      41,
      "power_double_double_function");
  Add(ExternalReference::power_double_int_function(isolate).address(),
      UNCLASSIFIED,
      42,
      "power_double_int_function");
  Add(ExternalReference::store_buffer_top(isolate).address(),
      UNCLASSIFIED,
      43,
      "store_buffer_top");
  Add(ExternalReference::address_of_canonical_non_hole_nan().address(),
      UNCLASSIFIED,
      44,
      "canonical_nan");
  Add(ExternalReference::address_of_the_hole_nan().address(),
      UNCLASSIFIED,
      45,
      "the_hole_nan");
  Add(ExternalReference::get_date_field_function(isolate).address(),
      UNCLASSIFIED,
      46,
      "JSDate::GetField");
  Add(ExternalReference::date_cache_stamp(isolate).address(),
      UNCLASSIFIED,
      47,
      "date_cache_stamp");
}


ExternalReferenceEncoder::ExternalReferenceEncoder()
    : encodings_(Match),
      isolate_(Isolate::Current()) {
  ExternalReferenceTable* external_references =
      ExternalReferenceTable::instance(isolate_);
  for (int i = 0; i < external_references->size(); ++i) {
    Put(external_references->address(i), i);
  }
}


uint32_t ExternalReferenceEncoder::Encode(Address key) const {
  int index = IndexOf(key);
  ASSERT(key == NULL || index >= 0);
  return index >=0 ?
         ExternalReferenceTable::instance(isolate_)->code(index) : 0;
}


const char* ExternalReferenceEncoder::NameOfAddress(Address key) const {
  int index = IndexOf(key);
  return index >= 0 ?
      ExternalReferenceTable::instance(isolate_)->name(index) : NULL;
}


int ExternalReferenceEncoder::IndexOf(Address key) const {
  if (key == NULL) return -1;
  HashMap::Entry* entry =
      const_cast<HashMap&>(encodings_).Lookup(key, Hash(key), false);
  return entry == NULL
      ? -1
      : static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
}


void ExternalReferenceEncoder::Put(Address key, int index) {
  HashMap::Entry* entry = encodings_.Lookup(key, Hash(key), true);
  entry->value = reinterpret_cast<void*>(index);
}


ExternalReferenceDecoder::ExternalReferenceDecoder()
    : encodings_(NewArray<Address*>(kTypeCodeCount)),
      isolate_(Isolate::Current()) {
  ExternalReferenceTable* external_references =
      ExternalReferenceTable::instance(isolate_);
  for (int type = kFirstTypeCode; type < kTypeCodeCount; ++type) {
    int max = external_references->max_id(type) + 1;
    encodings_[type] = NewArray<Address>(max + 1);
  }
  for (int i = 0; i < external_references->size(); ++i) {
    Put(external_references->code(i), external_references->address(i));
  }
}


ExternalReferenceDecoder::~ExternalReferenceDecoder() {
  for (int type = kFirstTypeCode; type < kTypeCodeCount; ++type) {
    DeleteArray(encodings_[type]);
  }
  DeleteArray(encodings_);
}


bool Serializer::serialization_enabled_ = false;
bool Serializer::too_late_to_enable_now_ = false;


Deserializer::Deserializer(SnapshotByteSource* source)
    : isolate_(NULL),
      source_(source),
      external_reference_decoder_(NULL) {
}


// This routine both allocates a new object, and also keeps
// track of where objects have been allocated so that we can
// fix back references when deserializing.
Address Deserializer::Allocate(int space_index, Space* space, int size) {
  Address address;
  if (!SpaceIsLarge(space_index)) {
    ASSERT(!SpaceIsPaged(space_index) ||
           size <= Page::kPageSize - Page::kObjectStartOffset);
    MaybeObject* maybe_new_allocation;
    if (space_index == NEW_SPACE) {
      maybe_new_allocation =
          reinterpret_cast<NewSpace*>(space)->AllocateRaw(size);
    } else {
      maybe_new_allocation =
          reinterpret_cast<PagedSpace*>(space)->AllocateRaw(size);
    }
    ASSERT(!maybe_new_allocation->IsFailure());
    Object* new_allocation = maybe_new_allocation->ToObjectUnchecked();
    HeapObject* new_object = HeapObject::cast(new_allocation);
    address = new_object->address();
    high_water_[space_index] = address + size;
  } else {
    ASSERT(SpaceIsLarge(space_index));
    LargeObjectSpace* lo_space = reinterpret_cast<LargeObjectSpace*>(space);
    Object* new_allocation;
    if (space_index == kLargeData || space_index == kLargeFixedArray) {
      new_allocation =
          lo_space->AllocateRaw(size, NOT_EXECUTABLE)->ToObjectUnchecked();
    } else {
      ASSERT_EQ(kLargeCode, space_index);
      new_allocation =
          lo_space->AllocateRaw(size, EXECUTABLE)->ToObjectUnchecked();
    }
    HeapObject* new_object = HeapObject::cast(new_allocation);
    // Record all large objects in the same space.
    address = new_object->address();
    pages_[LO_SPACE].Add(address);
  }
  last_object_address_ = address;
  return address;
}


// This returns the address of an object that has been described in the
// snapshot as being offset bytes back in a particular space.
HeapObject* Deserializer::GetAddressFromEnd(int space) {
  int offset = source_->GetInt();
  ASSERT(!SpaceIsLarge(space));
  offset <<= kObjectAlignmentBits;
  return HeapObject::FromAddress(high_water_[space] - offset);
}


// This returns the address of an object that has been described in the
// snapshot as being offset bytes into a particular space.
HeapObject* Deserializer::GetAddressFromStart(int space) {
  int offset = source_->GetInt();
  if (SpaceIsLarge(space)) {
    // Large spaces have one object per 'page'.
    return HeapObject::FromAddress(pages_[LO_SPACE][offset]);
  }
  offset <<= kObjectAlignmentBits;
  if (space == NEW_SPACE) {
    // New space has only one space - numbered 0.
    return HeapObject::FromAddress(pages_[space][0] + offset);
  }
  ASSERT(SpaceIsPaged(space));
  int page_of_pointee = offset >> kPageSizeBits;
  Address object_address = pages_[space][page_of_pointee] +
                           (offset & Page::kPageAlignmentMask);
  return HeapObject::FromAddress(object_address);
}


void Deserializer::Deserialize() {
  isolate_ = Isolate::Current();
  ASSERT(isolate_ != NULL);
  // Don't GC while deserializing - just expand the heap.
  AlwaysAllocateScope always_allocate;
  // Don't use the free lists while deserializing.
  LinearAllocationScope allocate_linearly;
  // No active threads.
  ASSERT_EQ(NULL, isolate_->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  ASSERT(isolate_->handle_scope_implementer()->blocks()->is_empty());
  // Make sure the entire partial snapshot cache is traversed, filling it with
  // valid object pointers.
  isolate_->set_serialize_partial_snapshot_cache_length(
      Isolate::kPartialSnapshotCacheCapacity);
  ASSERT_EQ(NULL, external_reference_decoder_);
  external_reference_decoder_ = new ExternalReferenceDecoder();
  isolate_->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG);
  isolate_->heap()->IterateWeakRoots(this, VISIT_ALL);

  isolate_->heap()->set_global_contexts_list(
      isolate_->heap()->undefined_value());

  // Update data pointers to the external strings containing natives sources.
  for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
    Object* source = isolate_->heap()->natives_source_cache()->get(i);
    if (!source->IsUndefined()) {
      ExternalAsciiString::cast(source)->update_data_cache();
    }
  }
}


void Deserializer::DeserializePartial(Object** root) {
  isolate_ = Isolate::Current();
  // Don't GC while deserializing - just expand the heap.
  AlwaysAllocateScope always_allocate;
  // Don't use the free lists while deserializing.
  LinearAllocationScope allocate_linearly;
  if (external_reference_decoder_ == NULL) {
    external_reference_decoder_ = new ExternalReferenceDecoder();
  }
  VisitPointer(root);
}


Deserializer::~Deserializer() {
  ASSERT(source_->AtEOF());
  if (external_reference_decoder_) {
    delete external_reference_decoder_;
    external_reference_decoder_ = NULL;
  }
}


// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
void Deserializer::VisitPointers(Object** start, Object** end) {
  // The space must be new space.  Any other space would cause ReadChunk to try
  // to update the remembered using NULL as the address.
  ReadChunk(start, end, NEW_SPACE, NULL);
}


// This routine writes the new object into the pointer provided and then
// returns true if the new object was in young space and false otherwise.
// The reason for this strange interface is that otherwise the object is
// written very late, which means the FreeSpace map is not set up by the
// time we need to use it to mark the space at the end of a page free.
void Deserializer::ReadObject(int space_number,
                              Space* space,
                              Object** write_back) {
  int size = source_->GetInt() << kObjectAlignmentBits;
  Address address = Allocate(space_number, space, size);
  *write_back = HeapObject::FromAddress(address);
  Object** current = reinterpret_cast<Object**>(address);
  Object** limit = current + (size >> kPointerSizeLog2);
  if (FLAG_log_snapshot_positions) {
    LOG(isolate_, SnapshotPositionEvent(address, source_->position()));
  }
  ReadChunk(current, limit, space_number, address);
#ifdef DEBUG
  bool is_codespace = (space == HEAP->code_space()) ||
      ((space == HEAP->lo_space()) && (space_number == kLargeCode));
  ASSERT(HeapObject::FromAddress(address)->IsCode() == is_codespace);
#endif
}


// This macro is always used with a constant argument so it should all fold
// away to almost nothing in the generated code.  It might be nicer to do this
// with the ternary operator but there are type issues with that.
#define ASSIGN_DEST_SPACE(space_number)                                        \
  Space* dest_space;                                                           \
  if (space_number == NEW_SPACE) {                                             \
    dest_space = isolate->heap()->new_space();                                \
  } else if (space_number == OLD_POINTER_SPACE) {                              \
    dest_space = isolate->heap()->old_pointer_space();                         \
  } else if (space_number == OLD_DATA_SPACE) {                                 \
    dest_space = isolate->heap()->old_data_space();                            \
  } else if (space_number == CODE_SPACE) {                                     \
    dest_space = isolate->heap()->code_space();                                \
  } else if (space_number == MAP_SPACE) {                                      \
    dest_space = isolate->heap()->map_space();                                 \
  } else if (space_number == CELL_SPACE) {                                     \
    dest_space = isolate->heap()->cell_space();                                \
  } else {                                                                     \
    ASSERT(space_number >= LO_SPACE);                                          \
    dest_space = isolate->heap()->lo_space();                                  \
  }


static const int kUnknownOffsetFromStart = -1;


void Deserializer::ReadChunk(Object** current,
                             Object** limit,
                             int source_space,
                             Address current_object_address) {
  Isolate* const isolate = isolate_;
  bool write_barrier_needed = (current_object_address != NULL &&
                               source_space != NEW_SPACE &&
                               source_space != CELL_SPACE &&
                               source_space != CODE_SPACE &&
                               source_space != OLD_DATA_SPACE);
  while (current < limit) {
    int data = source_->Get();
    switch (data) {
#define CASE_STATEMENT(where, how, within, space_number)                       \
      case where + how + within + space_number:                                \
      ASSERT((where & ~kPointedToMask) == 0);                                  \
      ASSERT((how & ~kHowToCodeMask) == 0);                                    \
      ASSERT((within & ~kWhereToPointMask) == 0);                              \
      ASSERT((space_number & ~kSpaceMask) == 0);

#define CASE_BODY(where, how, within, space_number_if_any, offset_from_start)  \
      {                                                                        \
        bool emit_write_barrier = false;                                       \
        bool current_was_incremented = false;                                  \
        int space_number =  space_number_if_any == kAnyOldSpace ?              \
                            (data & kSpaceMask) : space_number_if_any;         \
        if (where == kNewObject && how == kPlain && within == kStartOfObject) {\
          ASSIGN_DEST_SPACE(space_number)                                      \
          ReadObject(space_number, dest_space, current);                       \
          emit_write_barrier = (space_number == NEW_SPACE);                    \
        } else {                                                               \
          Object* new_object = NULL;  /* May not be a real Object pointer. */  \
          if (where == kNewObject) {                                           \
            ASSIGN_DEST_SPACE(space_number)                                    \
            ReadObject(space_number, dest_space, &new_object);                 \
          } else if (where == kRootArray) {                                    \
            int root_id = source_->GetInt();                                   \
            new_object = isolate->heap()->roots_array_start()[root_id];        \
            emit_write_barrier = isolate->heap()->InNewSpace(new_object);      \
          } else if (where == kPartialSnapshotCache) {                         \
            int cache_index = source_->GetInt();                               \
            new_object = isolate->serialize_partial_snapshot_cache()           \
                [cache_index];                                                 \
            emit_write_barrier = isolate->heap()->InNewSpace(new_object);      \
          } else if (where == kExternalReference) {                            \
            int reference_id = source_->GetInt();                              \
            Address address = external_reference_decoder_->                    \
                Decode(reference_id);                                          \
            new_object = reinterpret_cast<Object*>(address);                   \
          } else if (where == kBackref) {                                      \
            emit_write_barrier = (space_number == NEW_SPACE);                  \
            new_object = GetAddressFromEnd(data & kSpaceMask);                 \
          } else {                                                             \
            ASSERT(where == kFromStart);                                       \
            if (offset_from_start == kUnknownOffsetFromStart) {                \
              emit_write_barrier = (space_number == NEW_SPACE);                \
              new_object = GetAddressFromStart(data & kSpaceMask);             \
            } else {                                                           \
              Address object_address = pages_[space_number][0] +               \
                  (offset_from_start << kObjectAlignmentBits);                 \
              new_object = HeapObject::FromAddress(object_address);            \
            }                                                                  \
          }                                                                    \
          if (within == kFirstInstruction) {                                   \
            Code* new_code_object = reinterpret_cast<Code*>(new_object);       \
            new_object = reinterpret_cast<Object*>(                            \
                new_code_object->instruction_start());                         \
          }                                                                    \
          if (how == kFromCode) {                                              \
            Address location_of_branch_data =                                  \
                reinterpret_cast<Address>(current);                            \
            Assembler::deserialization_set_special_target_at(                  \
                location_of_branch_data,                                       \
                reinterpret_cast<Address>(new_object));                        \
            location_of_branch_data += Assembler::kSpecialTargetSize;          \
            current = reinterpret_cast<Object**>(location_of_branch_data);     \
            current_was_incremented = true;                                    \
          } else {                                                             \
            *current = new_object;                                             \
          }                                                                    \
        }                                                                      \
        if (emit_write_barrier && write_barrier_needed) {                      \
          Address current_address = reinterpret_cast<Address>(current);        \
          isolate->heap()->RecordWrite(                                        \
              current_object_address,                                          \
              static_cast<int>(current_address - current_object_address));     \
        }                                                                      \
        if (!current_was_incremented) {                                        \
          current++;                                                           \
        }                                                                      \
        break;                                                                 \
      }                                                                        \

// This generates a case and a body for each space.  The large object spaces are
// very rare in snapshots so they are grouped in one body.
#define ONE_PER_SPACE(where, how, within)                                      \
  CASE_STATEMENT(where, how, within, NEW_SPACE)                                \
  CASE_BODY(where, how, within, NEW_SPACE, kUnknownOffsetFromStart)            \
  CASE_STATEMENT(where, how, within, OLD_DATA_SPACE)                           \
  CASE_BODY(where, how, within, OLD_DATA_SPACE, kUnknownOffsetFromStart)       \
  CASE_STATEMENT(where, how, within, OLD_POINTER_SPACE)                        \
  CASE_BODY(where, how, within, OLD_POINTER_SPACE, kUnknownOffsetFromStart)    \
  CASE_STATEMENT(where, how, within, CODE_SPACE)                               \
  CASE_BODY(where, how, within, CODE_SPACE, kUnknownOffsetFromStart)           \
  CASE_STATEMENT(where, how, within, CELL_SPACE)                               \
  CASE_BODY(where, how, within, CELL_SPACE, kUnknownOffsetFromStart)           \
  CASE_STATEMENT(where, how, within, MAP_SPACE)                                \
  CASE_BODY(where, how, within, MAP_SPACE, kUnknownOffsetFromStart)            \
  CASE_STATEMENT(where, how, within, kLargeData)                               \
  CASE_STATEMENT(where, how, within, kLargeCode)                               \
  CASE_STATEMENT(where, how, within, kLargeFixedArray)                         \
  CASE_BODY(where, how, within, kAnyOldSpace, kUnknownOffsetFromStart)

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with 8 fall-through
// cases and one body.
#define ALL_SPACES(where, how, within)                                         \
  CASE_STATEMENT(where, how, within, NEW_SPACE)                                \
  CASE_BODY(where, how, within, NEW_SPACE, kUnknownOffsetFromStart)            \
  CASE_STATEMENT(where, how, within, OLD_DATA_SPACE)                           \
  CASE_STATEMENT(where, how, within, OLD_POINTER_SPACE)                        \
  CASE_STATEMENT(where, how, within, CODE_SPACE)                               \
  CASE_STATEMENT(where, how, within, CELL_SPACE)                               \
  CASE_STATEMENT(where, how, within, MAP_SPACE)                                \
  CASE_STATEMENT(where, how, within, kLargeData)                               \
  CASE_STATEMENT(where, how, within, kLargeCode)                               \
  CASE_STATEMENT(where, how, within, kLargeFixedArray)                         \
  CASE_BODY(where, how, within, kAnyOldSpace, kUnknownOffsetFromStart)

#define ONE_PER_CODE_SPACE(where, how, within)                                 \
  CASE_STATEMENT(where, how, within, CODE_SPACE)                               \
  CASE_BODY(where, how, within, CODE_SPACE, kUnknownOffsetFromStart)           \
  CASE_STATEMENT(where, how, within, kLargeCode)                               \
  CASE_BODY(where, how, within, kLargeCode, kUnknownOffsetFromStart)

#define FOUR_CASES(byte_code)             \
  case byte_code:                         \
  case byte_code + 1:                     \
  case byte_code + 2:                     \
  case byte_code + 3:

#define SIXTEEN_CASES(byte_code)          \
  FOUR_CASES(byte_code)                   \
  FOUR_CASES(byte_code + 4)               \
  FOUR_CASES(byte_code + 8)               \
  FOUR_CASES(byte_code + 12)

      // We generate 15 cases and bodies that process special tags that combine
      // the raw data tag and the length into one byte.
#define RAW_CASE(index, size)                                      \
      case kRawData + index: {                                     \
        byte* raw_data_out = reinterpret_cast<byte*>(current);     \
        source_->CopyRaw(raw_data_out, size);                      \
        current = reinterpret_cast<Object**>(raw_data_out + size); \
        break;                                                     \
      }
      COMMON_RAW_LENGTHS(RAW_CASE)
#undef RAW_CASE

      // Deserialize a chunk of raw data that doesn't have one of the popular
      // lengths.
      case kRawData: {
        int size = source_->GetInt();
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        source_->CopyRaw(raw_data_out, size);
        current = reinterpret_cast<Object**>(raw_data_out + size);
        break;
      }

      SIXTEEN_CASES(kRootArrayLowConstants)
      SIXTEEN_CASES(kRootArrayHighConstants) {
        int root_id = RootArrayConstantFromByteCode(data);
        Object* object = isolate->heap()->roots_array_start()[root_id];
        ASSERT(!isolate->heap()->InNewSpace(object));
        *current++ = object;
        break;
      }

      case kRepeat: {
        int repeats = source_->GetInt();
        Object* object = current[-1];
        ASSERT(!isolate->heap()->InNewSpace(object));
        for (int i = 0; i < repeats; i++) current[i] = object;
        current += repeats;
        break;
      }

      STATIC_ASSERT(kRootArrayNumberOfConstantEncodings ==
                    Heap::kOldSpaceRoots);
      STATIC_ASSERT(kMaxRepeats == 12);
      FOUR_CASES(kConstantRepeat)
      FOUR_CASES(kConstantRepeat + 4)
      FOUR_CASES(kConstantRepeat + 8) {
        int repeats = RepeatsForCode(data);
        Object* object = current[-1];
        ASSERT(!isolate->heap()->InNewSpace(object));
        for (int i = 0; i < repeats; i++) current[i] = object;
        current += repeats;
        break;
      }

      // Deserialize a new object and write a pointer to it to the current
      // object.
      ONE_PER_SPACE(kNewObject, kPlain, kStartOfObject)
      // Support for direct instruction pointers in functions
      ONE_PER_CODE_SPACE(kNewObject, kPlain, kFirstInstruction)
      // Deserialize a new code object and write a pointer to its first
      // instruction to the current code object.
      ONE_PER_SPACE(kNewObject, kFromCode, kFirstInstruction)
      // Find a recently deserialized object using its offset from the current
      // allocation point and write a pointer to it to the current object.
      ALL_SPACES(kBackref, kPlain, kStartOfObject)
#if V8_TARGET_ARCH_MIPS
      // Deserialize a new object from pointer found in code and write
      // a pointer to it to the current object. Required only for MIPS, and
      // omitted on the other architectures because it is fully unrolled and
      // would cause bloat.
      ONE_PER_SPACE(kNewObject, kFromCode, kStartOfObject)
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to it to the current
      // object. Required only for MIPS.
      ALL_SPACES(kBackref, kFromCode, kStartOfObject)
      // Find an already deserialized code object using its offset from
      // the start and write a pointer to it to the current object.
      // Required only for MIPS.
      ALL_SPACES(kFromStart, kFromCode, kStartOfObject)
#endif
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to its first instruction
      // to the current code object or the instruction pointer in a function
      // object.
      ALL_SPACES(kBackref, kFromCode, kFirstInstruction)
      ALL_SPACES(kBackref, kPlain, kFirstInstruction)
      // Find an already deserialized object using its offset from the start
      // and write a pointer to it to the current object.
      ALL_SPACES(kFromStart, kPlain, kStartOfObject)
      ALL_SPACES(kFromStart, kPlain, kFirstInstruction)
      // Find an already deserialized code object using its offset from the
      // start and write a pointer to its first instruction to the current code
      // object.
      ALL_SPACES(kFromStart, kFromCode, kFirstInstruction)
      // Find an object in the roots array and write a pointer to it to the
      // current object.
      CASE_STATEMENT(kRootArray, kPlain, kStartOfObject, 0)
      CASE_BODY(kRootArray, kPlain, kStartOfObject, 0, kUnknownOffsetFromStart)
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      CASE_STATEMENT(kPartialSnapshotCache, kPlain, kStartOfObject, 0)
      CASE_BODY(kPartialSnapshotCache,
                kPlain,
                kStartOfObject,
                0,
                kUnknownOffsetFromStart)
      // Find an code entry in the partial snapshots cache and
      // write a pointer to it to the current object.
      CASE_STATEMENT(kPartialSnapshotCache, kPlain, kFirstInstruction, 0)
      CASE_BODY(kPartialSnapshotCache,
                kPlain,
                kFirstInstruction,
                0,
                kUnknownOffsetFromStart)
      // Find an external reference and write a pointer to it to the current
      // object.
      CASE_STATEMENT(kExternalReference, kPlain, kStartOfObject, 0)
      CASE_BODY(kExternalReference,
                kPlain,
                kStartOfObject,
                0,
                kUnknownOffsetFromStart)
      // Find an external reference and write a pointer to it in the current
      // code object.
      CASE_STATEMENT(kExternalReference, kFromCode, kStartOfObject, 0)
      CASE_BODY(kExternalReference,
                kFromCode,
                kStartOfObject,
                0,
                kUnknownOffsetFromStart)

#undef CASE_STATEMENT
#undef CASE_BODY
#undef ONE_PER_SPACE
#undef ALL_SPACES
#undef ASSIGN_DEST_SPACE

      case kNewPage: {
        int space = source_->Get();
        pages_[space].Add(last_object_address_);
        if (space == CODE_SPACE) {
          CPU::FlushICache(last_object_address_, Page::kPageSize);
        }
        break;
      }

      case kSkip: {
        current++;
        break;
      }

      case kNativesStringResource: {
        int index = source_->Get();
        Vector<const char> source_vector = Natives::GetRawScriptSource(index);
        NativesExternalStringResource* resource =
            new NativesExternalStringResource(isolate->bootstrapper(),
                                              source_vector.start(),
                                              source_vector.length());
        *current++ = reinterpret_cast<Object*>(resource);
        break;
      }

      case kSynchronize: {
        // If we get here then that indicates that you have a mismatch between
        // the number of GC roots when serializing and deserializing.
        UNREACHABLE();
      }

      default:
        UNREACHABLE();
    }
  }
  ASSERT_EQ(current, limit);
}


void SnapshotByteSink::PutInt(uintptr_t integer, const char* description) {
  const int max_shift = ((kPointerSize * kBitsPerByte) / 7) * 7;
  for (int shift = max_shift; shift > 0; shift -= 7) {
    if (integer >= static_cast<uintptr_t>(1u) << shift) {
      Put((static_cast<int>((integer >> shift)) & 0x7f) | 0x80, "IntPart");
    }
  }
  PutSection(static_cast<int>(integer & 0x7f), "IntLastPart");
}


Serializer::Serializer(SnapshotByteSink* sink)
    : sink_(sink),
      current_root_index_(0),
      external_reference_encoder_(new ExternalReferenceEncoder),
      large_object_total_(0),
      root_index_wave_front_(0) {
  isolate_ = Isolate::Current();
  // The serializer is meant to be used only to generate initial heap images
  // from a context in which there is only one isolate.
  ASSERT(isolate_->IsDefaultIsolate());
  for (int i = 0; i <= LAST_SPACE; i++) {
    fullness_[i] = 0;
  }
}


Serializer::~Serializer() {
  delete external_reference_encoder_;
}


void StartupSerializer::SerializeStrongReferences() {
  Isolate* isolate = Isolate::Current();
  // No active threads.
  CHECK_EQ(NULL, Isolate::Current()->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK(isolate->handle_scope_implementer()->blocks()->is_empty());
  CHECK_EQ(0, isolate->global_handles()->NumberOfWeakHandles());
  // We don't support serializing installed extensions.
  CHECK(!isolate->has_installed_extensions());

  HEAP->IterateStrongRoots(this, VISIT_ONLY_STRONG);
}


void PartialSerializer::Serialize(Object** object) {
  this->VisitPointer(object);
  Isolate* isolate = Isolate::Current();

  // After we have done the partial serialization the partial snapshot cache
  // will contain some references needed to decode the partial snapshot.  We
  // fill it up with undefineds so it has a predictable length so the
  // deserialization code doesn't need to know the length.
  for (int index = isolate->serialize_partial_snapshot_cache_length();
       index < Isolate::kPartialSnapshotCacheCapacity;
       index++) {
    isolate->serialize_partial_snapshot_cache()[index] =
        isolate->heap()->undefined_value();
    startup_serializer_->VisitPointer(
        &isolate->serialize_partial_snapshot_cache()[index]);
  }
  isolate->set_serialize_partial_snapshot_cache_length(
      Isolate::kPartialSnapshotCacheCapacity);
}


void Serializer::VisitPointers(Object** start, Object** end) {
  Isolate* isolate = Isolate::Current();

  for (Object** current = start; current < end; current++) {
    if (start == isolate->heap()->roots_array_start()) {
      root_index_wave_front_ =
          Max(root_index_wave_front_, static_cast<intptr_t>(current - start));
    }
    if (reinterpret_cast<Address>(current) ==
        isolate->heap()->store_buffer()->TopAddress()) {
      sink_->Put(kSkip, "Skip");
    } else if ((*current)->IsSmi()) {
      sink_->Put(kRawData, "RawData");
      sink_->PutInt(kPointerSize, "length");
      for (int i = 0; i < kPointerSize; i++) {
        sink_->Put(reinterpret_cast<byte*>(current)[i], "Byte");
      }
    } else {
      SerializeObject(*current, kPlain, kStartOfObject);
    }
  }
}


// This ensures that the partial snapshot cache keeps things alive during GC and
// tracks their movement.  When it is called during serialization of the startup
// snapshot the partial snapshot is empty, so nothing happens.  When the partial
// (context) snapshot is created, this array is populated with the pointers that
// the partial snapshot will need. As that happens we emit serialized objects to
// the startup snapshot that correspond to the elements of this cache array.  On
// deserialization we therefore need to visit the cache array.  This fills it up
// with pointers to deserialized objects.
void SerializerDeserializer::Iterate(ObjectVisitor* visitor) {
  Isolate* isolate = Isolate::Current();
  visitor->VisitPointers(
      isolate->serialize_partial_snapshot_cache(),
      &isolate->serialize_partial_snapshot_cache()[
          isolate->serialize_partial_snapshot_cache_length()]);
}


// When deserializing we need to set the size of the snapshot cache.  This means
// the root iteration code (above) will iterate over array elements, writing the
// references to deserialized objects in them.
void SerializerDeserializer::SetSnapshotCacheSize(int size) {
  Isolate::Current()->set_serialize_partial_snapshot_cache_length(size);
}


int PartialSerializer::PartialSnapshotCacheIndex(HeapObject* heap_object) {
  Isolate* isolate = Isolate::Current();

  for (int i = 0;
       i < isolate->serialize_partial_snapshot_cache_length();
       i++) {
    Object* entry = isolate->serialize_partial_snapshot_cache()[i];
    if (entry == heap_object) return i;
  }

  // We didn't find the object in the cache.  So we add it to the cache and
  // then visit the pointer so that it becomes part of the startup snapshot
  // and we can refer to it from the partial snapshot.
  int length = isolate->serialize_partial_snapshot_cache_length();
  CHECK(length < Isolate::kPartialSnapshotCacheCapacity);
  isolate->serialize_partial_snapshot_cache()[length] = heap_object;
  startup_serializer_->VisitPointer(
      &isolate->serialize_partial_snapshot_cache()[length]);
  // We don't recurse from the startup snapshot generator into the partial
  // snapshot generator.
  ASSERT(length == isolate->serialize_partial_snapshot_cache_length());
  isolate->set_serialize_partial_snapshot_cache_length(length + 1);
  return length;
}


int Serializer::RootIndex(HeapObject* heap_object, HowToCode from) {
  Heap* heap = HEAP;
  if (heap->InNewSpace(heap_object)) return kInvalidRootIndex;
  for (int i = 0; i < root_index_wave_front_; i++) {
    Object* root = heap->roots_array_start()[i];
    if (!root->IsSmi() && root == heap_object) {
#if V8_TARGET_ARCH_MIPS
      if (from == kFromCode) {
        // In order to avoid code bloat in the deserializer we don't have
        // support for the encoding that specifies a particular root should
        // be written into the lui/ori instructions on MIPS.  Therefore we
        // should not generate such serialization data for MIPS.
        return kInvalidRootIndex;
      }
#endif
      return i;
    }
  }
  return kInvalidRootIndex;
}


// Encode the location of an already deserialized object in order to write its
// location into a later object.  We can encode the location as an offset from
// the start of the deserialized objects or as an offset backwards from the
// current allocation pointer.
void Serializer::SerializeReferenceToPreviousObject(
    int space,
    int address,
    HowToCode how_to_code,
    WhereToPoint where_to_point) {
  int offset = CurrentAllocationAddress(space) - address;
  bool from_start = true;
  if (SpaceIsPaged(space)) {
    // For paged space it is simple to encode back from current allocation if
    // the object is on the same page as the current allocation pointer.
    if ((CurrentAllocationAddress(space) >> kPageSizeBits) ==
        (address >> kPageSizeBits)) {
      from_start = false;
      address = offset;
    }
  } else if (space == NEW_SPACE) {
    // For new space it is always simple to encode back from current allocation.
    if (offset < address) {
      from_start = false;
      address = offset;
    }
  }
  // If we are actually dealing with real offsets (and not a numbering of
  // all objects) then we should shift out the bits that are always 0.
  if (!SpaceIsLarge(space)) address >>= kObjectAlignmentBits;
  if (from_start) {
    sink_->Put(kFromStart + how_to_code + where_to_point + space, "RefSer");
    sink_->PutInt(address, "address");
  } else {
    sink_->Put(kBackref + how_to_code + where_to_point + space, "BackRefSer");
    sink_->PutInt(address, "address");
  }
}


void StartupSerializer::SerializeObject(
    Object* o,
    HowToCode how_to_code,
    WhereToPoint where_to_point) {
  CHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);

  int root_index;
  if ((root_index = RootIndex(heap_object, how_to_code)) != kInvalidRootIndex) {
    PutRoot(root_index, heap_object, how_to_code, where_to_point);
    return;
  }

  if (address_mapper_.IsMapped(heap_object)) {
    int space = SpaceOfAlreadySerializedObject(heap_object);
    int address = address_mapper_.MappedTo(heap_object);
    SerializeReferenceToPreviousObject(space,
                                       address,
                                       how_to_code,
                                       where_to_point);
  } else {
    // Object has not yet been serialized.  Serialize it here.
    ObjectSerializer object_serializer(this,
                                       heap_object,
                                       sink_,
                                       how_to_code,
                                       where_to_point);
    object_serializer.Serialize();
  }
}


void StartupSerializer::SerializeWeakReferences() {
  for (int i = Isolate::Current()->serialize_partial_snapshot_cache_length();
       i < Isolate::kPartialSnapshotCacheCapacity;
       i++) {
    sink_->Put(kRootArray + kPlain + kStartOfObject, "RootSerialization");
    sink_->PutInt(Heap::kUndefinedValueRootIndex, "root_index");
  }
  HEAP->IterateWeakRoots(this, VISIT_ALL);
}


void Serializer::PutRoot(int root_index,
                         HeapObject* object,
                         SerializerDeserializer::HowToCode how_to_code,
                         SerializerDeserializer::WhereToPoint where_to_point) {
  if (how_to_code == kPlain &&
      where_to_point == kStartOfObject &&
      root_index < kRootArrayNumberOfConstantEncodings &&
      !HEAP->InNewSpace(object)) {
    if (root_index < kRootArrayNumberOfLowConstantEncodings) {
      sink_->Put(kRootArrayLowConstants + root_index, "RootLoConstant");
    } else {
      sink_->Put(kRootArrayHighConstants + root_index -
                     kRootArrayNumberOfLowConstantEncodings,
                 "RootHiConstant");
    }
  } else {
    sink_->Put(kRootArray + how_to_code + where_to_point, "RootSerialization");
    sink_->PutInt(root_index, "root_index");
  }
}


void PartialSerializer::SerializeObject(
    Object* o,
    HowToCode how_to_code,
    WhereToPoint where_to_point) {
  CHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);

  if (heap_object->IsMap()) {
    // The code-caches link to context-specific code objects, which
    // the startup and context serializes cannot currently handle.
    ASSERT(Map::cast(heap_object)->code_cache() ==
           heap_object->GetHeap()->raw_unchecked_empty_fixed_array());
  }

  int root_index;
  if ((root_index = RootIndex(heap_object, how_to_code)) != kInvalidRootIndex) {
    PutRoot(root_index, heap_object, how_to_code, where_to_point);
    return;
  }

  if (ShouldBeInThePartialSnapshotCache(heap_object)) {
    int cache_index = PartialSnapshotCacheIndex(heap_object);
    sink_->Put(kPartialSnapshotCache + how_to_code + where_to_point,
               "PartialSnapshotCache");
    sink_->PutInt(cache_index, "partial_snapshot_cache_index");
    return;
  }

  // Pointers from the partial snapshot to the objects in the startup snapshot
  // should go through the root array or through the partial snapshot cache.
  // If this is not the case you may have to add something to the root array.
  ASSERT(!startup_serializer_->address_mapper()->IsMapped(heap_object));
  // All the symbols that the partial snapshot needs should be either in the
  // root table or in the partial snapshot cache.
  ASSERT(!heap_object->IsSymbol());

  if (address_mapper_.IsMapped(heap_object)) {
    int space = SpaceOfAlreadySerializedObject(heap_object);
    int address = address_mapper_.MappedTo(heap_object);
    SerializeReferenceToPreviousObject(space,
                                       address,
                                       how_to_code,
                                       where_to_point);
  } else {
    // Object has not yet been serialized.  Serialize it here.
    ObjectSerializer serializer(this,
                                heap_object,
                                sink_,
                                how_to_code,
                                where_to_point);
    serializer.Serialize();
  }
}


void Serializer::ObjectSerializer::Serialize() {
  int space = Serializer::SpaceOfObject(object_);
  int size = object_->Size();

  sink_->Put(kNewObject + reference_representation_ + space,
             "ObjectSerialization");
  sink_->PutInt(size >> kObjectAlignmentBits, "Size in words");

  LOG(i::Isolate::Current(),
      SnapshotPositionEvent(object_->address(), sink_->Position()));

  // Mark this object as already serialized.
  bool start_new_page;
  int offset = serializer_->Allocate(space, size, &start_new_page);
  serializer_->address_mapper()->AddMapping(object_, offset);
  if (start_new_page) {
    sink_->Put(kNewPage, "NewPage");
    sink_->PutSection(space, "NewPageSpace");
  }

  // Serialize the map (first word of the object).
  serializer_->SerializeObject(object_->map(), kPlain, kStartOfObject);

  // Serialize the rest of the object.
  CHECK_EQ(0, bytes_processed_so_far_);
  bytes_processed_so_far_ = kPointerSize;
  object_->IterateBody(object_->map()->instance_type(), size, this);
  OutputRawData(object_->address() + size);
}


void Serializer::ObjectSerializer::VisitPointers(Object** start,
                                                 Object** end) {
  Object** current = start;
  while (current < end) {
    while (current < end && (*current)->IsSmi()) current++;
    if (current < end) OutputRawData(reinterpret_cast<Address>(current));

    while (current < end && !(*current)->IsSmi()) {
      HeapObject* current_contents = HeapObject::cast(*current);
      int root_index = serializer_->RootIndex(current_contents, kPlain);
      // Repeats are not subject to the write barrier so there are only some
      // objects that can be used in a repeat encoding.  These are the early
      // ones in the root array that are never in new space.
      if (current != start &&
          root_index != kInvalidRootIndex &&
          root_index < kRootArrayNumberOfConstantEncodings &&
          current_contents == current[-1]) {
        ASSERT(!HEAP->InNewSpace(current_contents));
        int repeat_count = 1;
        while (current < end - 1 && current[repeat_count] == current_contents) {
          repeat_count++;
        }
        current += repeat_count;
        bytes_processed_so_far_ += repeat_count * kPointerSize;
        if (repeat_count > kMaxRepeats) {
          sink_->Put(kRepeat, "SerializeRepeats");
          sink_->PutInt(repeat_count, "SerializeRepeats");
        } else {
          sink_->Put(CodeForRepeats(repeat_count), "SerializeRepeats");
        }
      } else {
        serializer_->SerializeObject(current_contents, kPlain, kStartOfObject);
        bytes_processed_so_far_ += kPointerSize;
        current++;
      }
    }
  }
}


void Serializer::ObjectSerializer::VisitEmbeddedPointer(RelocInfo* rinfo) {
  Object** current = rinfo->target_object_address();

  OutputRawData(rinfo->target_address_address());
  HowToCode representation = rinfo->IsCodedSpecially() ? kFromCode : kPlain;
  serializer_->SerializeObject(*current, representation, kStartOfObject);
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitExternalReferences(Address* start,
                                                           Address* end) {
  Address references_start = reinterpret_cast<Address>(start);
  OutputRawData(references_start);

  for (Address* current = start; current < end; current++) {
    sink_->Put(kExternalReference + kPlain + kStartOfObject, "ExternalRef");
    int reference_id = serializer_->EncodeExternalReference(*current);
    sink_->PutInt(reference_id, "reference id");
  }
  bytes_processed_so_far_ += static_cast<int>((end - start) * kPointerSize);
}


void Serializer::ObjectSerializer::VisitExternalReference(RelocInfo* rinfo) {
  Address references_start = rinfo->target_address_address();
  OutputRawData(references_start);

  Address* current = rinfo->target_reference_address();
  int representation = rinfo->IsCodedSpecially() ?
                       kFromCode + kStartOfObject : kPlain + kStartOfObject;
  sink_->Put(kExternalReference + representation, "ExternalRef");
  int reference_id = serializer_->EncodeExternalReference(*current);
  sink_->PutInt(reference_id, "reference id");
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitRuntimeEntry(RelocInfo* rinfo) {
  Address target_start = rinfo->target_address_address();
  OutputRawData(target_start);
  Address target = rinfo->target_address();
  uint32_t encoding = serializer_->EncodeExternalReference(target);
  CHECK(target == NULL ? encoding == 0 : encoding != 0);
  int representation;
  // Can't use a ternary operator because of gcc.
  if (rinfo->IsCodedSpecially()) {
    representation = kStartOfObject + kFromCode;
  } else {
    representation = kStartOfObject + kPlain;
  }
  sink_->Put(kExternalReference + representation, "ExternalReference");
  sink_->PutInt(encoding, "reference id");
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitCodeTarget(RelocInfo* rinfo) {
  CHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Address target_start = rinfo->target_address_address();
  OutputRawData(target_start);
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  serializer_->SerializeObject(target, kFromCode, kFirstInstruction);
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitCodeEntry(Address entry_address) {
  Code* target = Code::cast(Code::GetObjectFromEntryAddress(entry_address));
  OutputRawData(entry_address);
  serializer_->SerializeObject(target, kPlain, kFirstInstruction);
  bytes_processed_so_far_ += kPointerSize;
}


void Serializer::ObjectSerializer::VisitGlobalPropertyCell(RelocInfo* rinfo) {
  // We shouldn't have any global property cell references in code
  // objects in the snapshot.
  UNREACHABLE();
}


void Serializer::ObjectSerializer::VisitExternalAsciiString(
    v8::String::ExternalAsciiStringResource** resource_pointer) {
  Address references_start = reinterpret_cast<Address>(resource_pointer);
  OutputRawData(references_start);
  for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
    Object* source = HEAP->natives_source_cache()->get(i);
    if (!source->IsUndefined()) {
      ExternalAsciiString* string = ExternalAsciiString::cast(source);
      typedef v8::String::ExternalAsciiStringResource Resource;
      const Resource* resource = string->resource();
      if (resource == *resource_pointer) {
        sink_->Put(kNativesStringResource, "NativesStringResource");
        sink_->PutSection(i, "NativesStringResourceEnd");
        bytes_processed_so_far_ += sizeof(resource);
        return;
      }
    }
  }
  // One of the strings in the natives cache should match the resource.  We
  // can't serialize any other kinds of external strings.
  UNREACHABLE();
}


void Serializer::ObjectSerializer::OutputRawData(Address up_to) {
  Address object_start = object_->address();
  int up_to_offset = static_cast<int>(up_to - object_start);
  int skipped = up_to_offset - bytes_processed_so_far_;
  // This assert will fail if the reloc info gives us the target_address_address
  // locations in a non-ascending order.  Luckily that doesn't happen.
  ASSERT(skipped >= 0);
  if (skipped != 0) {
    Address base = object_start + bytes_processed_so_far_;
#define RAW_CASE(index, length)                                                \
    if (skipped == length) {                                                   \
      sink_->PutSection(kRawData + index, "RawDataFixed");                     \
    } else  /* NOLINT */
    COMMON_RAW_LENGTHS(RAW_CASE)
#undef RAW_CASE
    {  /* NOLINT */
      sink_->Put(kRawData, "RawData");
      sink_->PutInt(skipped, "length");
    }
    for (int i = 0; i < skipped; i++) {
      unsigned int data = base[i];
      sink_->PutSection(data, "Byte");
    }
    bytes_processed_so_far_ += skipped;
  }
}


int Serializer::SpaceOfObject(HeapObject* object) {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    AllocationSpace s = static_cast<AllocationSpace>(i);
    if (HEAP->InSpace(object, s)) {
      if (i == LO_SPACE) {
        if (object->IsCode()) {
          return kLargeCode;
        } else if (object->IsFixedArray()) {
          return kLargeFixedArray;
        } else {
          return kLargeData;
        }
      }
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer::SpaceOfAlreadySerializedObject(HeapObject* object) {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    AllocationSpace s = static_cast<AllocationSpace>(i);
    if (HEAP->InSpace(object, s)) {
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer::Allocate(int space, int size, bool* new_page) {
  CHECK(space >= 0 && space < kNumberOfSpaces);
  if (SpaceIsLarge(space)) {
    // In large object space we merely number the objects instead of trying to
    // determine some sort of address.
    *new_page = true;
    large_object_total_ += size;
    return fullness_[LO_SPACE]++;
  }
  *new_page = false;
  if (fullness_[space] == 0) {
    *new_page = true;
  }
  if (SpaceIsPaged(space)) {
    // Paged spaces are a little special.  We encode their addresses as if the
    // pages were all contiguous and each page were filled up in the range
    // 0 - Page::kObjectAreaSize.  In practice the pages may not be contiguous
    // and allocation does not start at offset 0 in the page, but this scheme
    // means the deserializer can get the page number quickly by shifting the
    // serialized address.
    CHECK(IsPowerOf2(Page::kPageSize));
    int used_in_this_page = (fullness_[space] & (Page::kPageSize - 1));
    CHECK(size <= SpaceAreaSize(space));
    if (used_in_this_page + size > SpaceAreaSize(space)) {
      *new_page = true;
      fullness_[space] = RoundUp(fullness_[space], Page::kPageSize);
    }
  }
  int allocation_address = fullness_[space];
  fullness_[space] = allocation_address + size;
  return allocation_address;
}


int Serializer::SpaceAreaSize(int space) {
  if (space == CODE_SPACE) {
    return isolate_->memory_allocator()->CodePageAreaSize();
  } else {
    return Page::kPageSize - Page::kObjectStartOffset;
  }
}


} }  // namespace v8::internal

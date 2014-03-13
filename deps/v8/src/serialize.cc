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
#include "deoptimizer.h"
#include "execution.h"
#include "global-handles.h"
#include "ic-inl.h"
#include "natives.h"
#include "platform.h"
#include "runtime.h"
#include "serialize.h"
#include "snapshot.h"
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
#undef BUILD_NAME_LITERAL
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
  Add(ExternalReference::mod_two_doubles_operation(isolate).address(),
      UNCLASSIFIED,
      22,
      "mod_two_doubles");
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
  Add(ExternalReference::handle_scope_next_address(isolate).address(),
      UNCLASSIFIED,
      31,
      "HandleScope::next");
  Add(ExternalReference::handle_scope_limit_address(isolate).address(),
      UNCLASSIFIED,
      32,
      "HandleScope::limit");
  Add(ExternalReference::handle_scope_level_address(isolate).address(),
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
  Add(ExternalReference::isolate_address(isolate).address(),
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
  Add(ExternalReference::address_of_pending_message_obj(isolate).address(),
      UNCLASSIFIED,
      48,
      "address_of_pending_message_obj");
  Add(ExternalReference::address_of_has_pending_message(isolate).address(),
      UNCLASSIFIED,
      49,
      "address_of_has_pending_message");
  Add(ExternalReference::address_of_pending_message_script(isolate).address(),
      UNCLASSIFIED,
      50,
      "pending_message_script");
  Add(ExternalReference::get_make_code_young_function(isolate).address(),
      UNCLASSIFIED,
      51,
      "Code::MakeCodeYoung");
  Add(ExternalReference::cpu_features().address(),
      UNCLASSIFIED,
      52,
      "cpu_features");
  Add(ExternalReference(Runtime::kAllocateInNewSpace, isolate).address(),
      UNCLASSIFIED,
      53,
      "Runtime::AllocateInNewSpace");
  Add(ExternalReference(Runtime::kAllocateInTargetSpace, isolate).address(),
      UNCLASSIFIED,
      54,
      "Runtime::AllocateInTargetSpace");
  Add(ExternalReference::old_pointer_space_allocation_top_address(
      isolate).address(),
      UNCLASSIFIED,
      55,
      "Heap::OldPointerSpaceAllocationTopAddress");
  Add(ExternalReference::old_pointer_space_allocation_limit_address(
      isolate).address(),
      UNCLASSIFIED,
      56,
      "Heap::OldPointerSpaceAllocationLimitAddress");
  Add(ExternalReference::old_data_space_allocation_top_address(
      isolate).address(),
      UNCLASSIFIED,
      57,
      "Heap::OldDataSpaceAllocationTopAddress");
  Add(ExternalReference::old_data_space_allocation_limit_address(
      isolate).address(),
      UNCLASSIFIED,
      58,
      "Heap::OldDataSpaceAllocationLimitAddress");
  Add(ExternalReference::new_space_high_promotion_mode_active_address(isolate).
      address(),
      UNCLASSIFIED,
      59,
      "Heap::NewSpaceAllocationLimitAddress");
  Add(ExternalReference::allocation_sites_list_address(isolate).address(),
      UNCLASSIFIED,
      60,
      "Heap::allocation_sites_list_address()");
  Add(ExternalReference::address_of_uint32_bias().address(),
      UNCLASSIFIED,
      61,
      "uint32_bias");
  Add(ExternalReference::get_mark_code_as_executed_function(isolate).address(),
      UNCLASSIFIED,
      62,
      "Code::MarkCodeAsExecuted");

  // Add a small set of deopt entry addresses to encoder without generating the
  // deopt table code, which isn't possible at deserialization time.
  HandleScope scope(isolate);
  for (int entry = 0; entry < kDeoptTableSerializeEntryCount; ++entry) {
    Address address = Deoptimizer::GetDeoptimizationEntry(
        isolate,
        entry,
        Deoptimizer::LAZY,
        Deoptimizer::CALCULATE_ENTRY_ADDRESS);
    Add(address, LAZY_DEOPTIMIZATION, entry, "lazy_deopt");
  }
}


ExternalReferenceEncoder::ExternalReferenceEncoder(Isolate* isolate)
    : encodings_(Match),
      isolate_(isolate) {
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


ExternalReferenceDecoder::ExternalReferenceDecoder(Isolate* isolate)
    : encodings_(NewArray<Address*>(kTypeCodeCount)),
      isolate_(isolate) {
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


class CodeAddressMap: public CodeEventLogger {
 public:
  explicit CodeAddressMap(Isolate* isolate)
      : isolate_(isolate) {
    isolate->logger()->addCodeEventListener(this);
  }

  virtual ~CodeAddressMap() {
    isolate_->logger()->removeCodeEventListener(this);
  }

  virtual void CodeMoveEvent(Address from, Address to) {
    address_to_name_map_.Move(from, to);
  }

  virtual void CodeDeleteEvent(Address from) {
    address_to_name_map_.Remove(from);
  }

  const char* Lookup(Address address) {
    return address_to_name_map_.Lookup(address);
  }

 private:
  class NameMap {
   public:
    NameMap() : impl_(&PointerEquals) {}

    ~NameMap() {
      for (HashMap::Entry* p = impl_.Start(); p != NULL; p = impl_.Next(p)) {
        DeleteArray(static_cast<const char*>(p->value));
      }
    }

    void Insert(Address code_address, const char* name, int name_size) {
      HashMap::Entry* entry = FindOrCreateEntry(code_address);
      if (entry->value == NULL) {
        entry->value = CopyName(name, name_size);
      }
    }

    const char* Lookup(Address code_address) {
      HashMap::Entry* entry = FindEntry(code_address);
      return (entry != NULL) ? static_cast<const char*>(entry->value) : NULL;
    }

    void Remove(Address code_address) {
      HashMap::Entry* entry = FindEntry(code_address);
      if (entry != NULL) {
        DeleteArray(static_cast<char*>(entry->value));
        RemoveEntry(entry);
      }
    }

    void Move(Address from, Address to) {
      if (from == to) return;
      HashMap::Entry* from_entry = FindEntry(from);
      ASSERT(from_entry != NULL);
      void* value = from_entry->value;
      RemoveEntry(from_entry);
      HashMap::Entry* to_entry = FindOrCreateEntry(to);
      ASSERT(to_entry->value == NULL);
      to_entry->value = value;
    }

   private:
    static bool PointerEquals(void* lhs, void* rhs) {
      return lhs == rhs;
    }

    static char* CopyName(const char* name, int name_size) {
      char* result = NewArray<char>(name_size + 1);
      for (int i = 0; i < name_size; ++i) {
        char c = name[i];
        if (c == '\0') c = ' ';
        result[i] = c;
      }
      result[name_size] = '\0';
      return result;
    }

    HashMap::Entry* FindOrCreateEntry(Address code_address) {
      return impl_.Lookup(code_address, ComputePointerHash(code_address), true);
    }

    HashMap::Entry* FindEntry(Address code_address) {
      return impl_.Lookup(code_address,
                          ComputePointerHash(code_address),
                          false);
    }

    void RemoveEntry(HashMap::Entry* entry) {
      impl_.Remove(entry->key, entry->hash);
    }

    HashMap impl_;

    DISALLOW_COPY_AND_ASSIGN(NameMap);
  };

  virtual void LogRecordedBuffer(Code* code,
                                 SharedFunctionInfo*,
                                 const char* name,
                                 int length) {
    address_to_name_map_.Insert(code->address(), name, length);
  }

  NameMap address_to_name_map_;
  Isolate* isolate_;
};


CodeAddressMap* Serializer::code_address_map_ = NULL;


void Serializer::Enable(Isolate* isolate) {
  if (!serialization_enabled_) {
    ASSERT(!too_late_to_enable_now_);
  }
  if (serialization_enabled_) return;
  serialization_enabled_ = true;
  isolate->InitializeLoggingAndCounters();
  code_address_map_ = new CodeAddressMap(isolate);
}


void Serializer::Disable() {
  if (!serialization_enabled_) return;
  serialization_enabled_ = false;
  delete code_address_map_;
  code_address_map_ = NULL;
}


Deserializer::Deserializer(SnapshotByteSource* source)
    : isolate_(NULL),
      source_(source),
      external_reference_decoder_(NULL) {
  for (int i = 0; i < LAST_SPACE + 1; i++) {
    reservations_[i] = kUninitializedReservation;
  }
}


void Deserializer::FlushICacheForNewCodeObjects() {
  PageIterator it(isolate_->heap()->code_space());
  while (it.has_next()) {
    Page* p = it.next();
    CPU::FlushICache(p->area_start(), p->area_end() - p->area_start());
  }
}


void Deserializer::Deserialize(Isolate* isolate) {
  isolate_ = isolate;
  ASSERT(isolate_ != NULL);
  isolate_->heap()->ReserveSpace(reservations_, &high_water_[0]);
  // No active threads.
  ASSERT_EQ(NULL, isolate_->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  ASSERT(isolate_->handle_scope_implementer()->blocks()->is_empty());
  ASSERT_EQ(NULL, external_reference_decoder_);
  external_reference_decoder_ = new ExternalReferenceDecoder(isolate);
  isolate_->heap()->IterateSmiRoots(this);
  isolate_->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG);
  isolate_->heap()->RepairFreeListsAfterBoot();
  isolate_->heap()->IterateWeakRoots(this, VISIT_ALL);

  isolate_->heap()->set_native_contexts_list(
      isolate_->heap()->undefined_value());
  isolate_->heap()->set_array_buffers_list(
      isolate_->heap()->undefined_value());

  // The allocation site list is build during root iteration, but if no sites
  // were encountered then it needs to be initialized to undefined.
  if (isolate_->heap()->allocation_sites_list() == Smi::FromInt(0)) {
    isolate_->heap()->set_allocation_sites_list(
        isolate_->heap()->undefined_value());
  }

  isolate_->heap()->InitializeWeakObjectToCodeTable();

  // Update data pointers to the external strings containing natives sources.
  for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
    Object* source = isolate_->heap()->natives_source_cache()->get(i);
    if (!source->IsUndefined()) {
      ExternalAsciiString::cast(source)->update_data_cache();
    }
  }

  FlushICacheForNewCodeObjects();

  // Issue code events for newly deserialized code objects.
  LOG_CODE_EVENT(isolate_, LogCodeObjects());
  LOG_CODE_EVENT(isolate_, LogCompiledFunctions());
}


void Deserializer::DeserializePartial(Isolate* isolate, Object** root) {
  isolate_ = isolate;
  for (int i = NEW_SPACE; i < kNumberOfSpaces; i++) {
    ASSERT(reservations_[i] != kUninitializedReservation);
  }
  isolate_->heap()->ReserveSpace(reservations_, &high_water_[0]);
  if (external_reference_decoder_ == NULL) {
    external_reference_decoder_ = new ExternalReferenceDecoder(isolate);
  }

  // Keep track of the code space start and end pointers in case new
  // code objects were unserialized
  OldSpace* code_space = isolate_->heap()->code_space();
  Address start_address = code_space->top();
  VisitPointer(root);

  // There's no code deserialized here. If this assert fires
  // then that's changed and logging should be added to notify
  // the profiler et al of the new code.
  CHECK_EQ(start_address, code_space->top());
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


void Deserializer::RelinkAllocationSite(AllocationSite* site) {
  if (isolate_->heap()->allocation_sites_list() == Smi::FromInt(0)) {
    site->set_weak_next(isolate_->heap()->undefined_value());
  } else {
    site->set_weak_next(isolate_->heap()->allocation_sites_list());
  }
  isolate_->heap()->set_allocation_sites_list(site);
}


// This routine writes the new object into the pointer provided and then
// returns true if the new object was in young space and false otherwise.
// The reason for this strange interface is that otherwise the object is
// written very late, which means the FreeSpace map is not set up by the
// time we need to use it to mark the space at the end of a page free.
void Deserializer::ReadObject(int space_number,
                              Object** write_back) {
  int size = source_->GetInt() << kObjectAlignmentBits;
  Address address = Allocate(space_number, size);
  HeapObject* obj = HeapObject::FromAddress(address);
  *write_back = obj;
  Object** current = reinterpret_cast<Object**>(address);
  Object** limit = current + (size >> kPointerSizeLog2);
  if (FLAG_log_snapshot_positions) {
    LOG(isolate_, SnapshotPositionEvent(address, source_->position()));
  }
  ReadChunk(current, limit, space_number, address);

  // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
  // as a (weak) root. If this root is relocated correctly,
  // RelinkAllocationSite() isn't necessary.
  if (obj->IsAllocationSite()) {
    RelinkAllocationSite(AllocationSite::cast(obj));
  }

#ifdef DEBUG
  bool is_codespace = (space_number == CODE_SPACE);
  ASSERT(obj->IsCode() == is_codespace);
#endif
}

void Deserializer::ReadChunk(Object** current,
                             Object** limit,
                             int source_space,
                             Address current_object_address) {
  Isolate* const isolate = isolate_;
  // Write barrier support costs around 1% in startup time.  In fact there
  // are no new space objects in current boot snapshots, so it's not needed,
  // but that may change.
  bool write_barrier_needed = (current_object_address != NULL &&
                               source_space != NEW_SPACE &&
                               source_space != CELL_SPACE &&
                               source_space != PROPERTY_CELL_SPACE &&
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

#define CASE_BODY(where, how, within, space_number_if_any)                     \
      {                                                                        \
        bool emit_write_barrier = false;                                       \
        bool current_was_incremented = false;                                  \
        int space_number =  space_number_if_any == kAnyOldSpace ?              \
                            (data & kSpaceMask) : space_number_if_any;         \
        if (where == kNewObject && how == kPlain && within == kStartOfObject) {\
          ReadObject(space_number, current);                                   \
          emit_write_barrier = (space_number == NEW_SPACE);                    \
        } else {                                                               \
          Object* new_object = NULL;  /* May not be a real Object pointer. */  \
          if (where == kNewObject) {                                           \
            ReadObject(space_number, &new_object);                             \
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
            int skip = source_->GetInt();                                      \
            current = reinterpret_cast<Object**>(reinterpret_cast<Address>(    \
                current) + skip);                                              \
            int reference_id = source_->GetInt();                              \
            Address address = external_reference_decoder_->                    \
                Decode(reference_id);                                          \
            new_object = reinterpret_cast<Object*>(address);                   \
          } else if (where == kBackref) {                                      \
            emit_write_barrier = (space_number == NEW_SPACE);                  \
            new_object = GetAddressFromEnd(data & kSpaceMask);                 \
          } else {                                                             \
            ASSERT(where == kBackrefWithSkip);                                 \
            int skip = source_->GetInt();                                      \
            current = reinterpret_cast<Object**>(                              \
                reinterpret_cast<Address>(current) + skip);                    \
            emit_write_barrier = (space_number == NEW_SPACE);                  \
            new_object = GetAddressFromEnd(data & kSpaceMask);                 \
          }                                                                    \
          if (within == kInnerPointer) {                                       \
            if (space_number != CODE_SPACE || new_object->IsCode()) {          \
              Code* new_code_object = reinterpret_cast<Code*>(new_object);     \
              new_object = reinterpret_cast<Object*>(                          \
                  new_code_object->instruction_start());                       \
            } else {                                                           \
              ASSERT(space_number == CODE_SPACE);                              \
              Cell* cell = Cell::cast(new_object);                             \
              new_object = reinterpret_cast<Object*>(                          \
                  cell->ValueAddress());                                       \
            }                                                                  \
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

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with 8 fall-through
// cases and one body.
#define ALL_SPACES(where, how, within)                                         \
  CASE_STATEMENT(where, how, within, NEW_SPACE)                                \
  CASE_BODY(where, how, within, NEW_SPACE)                                     \
  CASE_STATEMENT(where, how, within, OLD_DATA_SPACE)                           \
  CASE_STATEMENT(where, how, within, OLD_POINTER_SPACE)                        \
  CASE_STATEMENT(where, how, within, CODE_SPACE)                               \
  CASE_STATEMENT(where, how, within, CELL_SPACE)                               \
  CASE_STATEMENT(where, how, within, PROPERTY_CELL_SPACE)                      \
  CASE_STATEMENT(where, how, within, MAP_SPACE)                                \
  CASE_BODY(where, how, within, kAnyOldSpace)

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

#define COMMON_RAW_LENGTHS(f)        \
  f(1)  \
  f(2)  \
  f(3)  \
  f(4)  \
  f(5)  \
  f(6)  \
  f(7)  \
  f(8)  \
  f(9)  \
  f(10) \
  f(11) \
  f(12) \
  f(13) \
  f(14) \
  f(15) \
  f(16) \
  f(17) \
  f(18) \
  f(19) \
  f(20) \
  f(21) \
  f(22) \
  f(23) \
  f(24) \
  f(25) \
  f(26) \
  f(27) \
  f(28) \
  f(29) \
  f(30) \
  f(31)

      // We generate 15 cases and bodies that process special tags that combine
      // the raw data tag and the length into one byte.
#define RAW_CASE(index)                                                      \
      case kRawData + index: {                                               \
        byte* raw_data_out = reinterpret_cast<byte*>(current);               \
        source_->CopyRaw(raw_data_out, index * kPointerSize);                \
        current =                                                            \
            reinterpret_cast<Object**>(raw_data_out + index * kPointerSize); \
        break;                                                               \
      }
      COMMON_RAW_LENGTHS(RAW_CASE)
#undef RAW_CASE

      // Deserialize a chunk of raw data that doesn't have one of the popular
      // lengths.
      case kRawData: {
        int size = source_->GetInt();
        byte* raw_data_out = reinterpret_cast<byte*>(current);
        source_->CopyRaw(raw_data_out, size);
        break;
      }

      SIXTEEN_CASES(kRootArrayConstants + kNoSkipDistance)
      SIXTEEN_CASES(kRootArrayConstants + kNoSkipDistance + 16) {
        int root_id = RootArrayConstantFromByteCode(data);
        Object* object = isolate->heap()->roots_array_start()[root_id];
        ASSERT(!isolate->heap()->InNewSpace(object));
        *current++ = object;
        break;
      }

      SIXTEEN_CASES(kRootArrayConstants + kHasSkipDistance)
      SIXTEEN_CASES(kRootArrayConstants + kHasSkipDistance + 16) {
        int root_id = RootArrayConstantFromByteCode(data);
        int skip = source_->GetInt();
        current = reinterpret_cast<Object**>(
            reinterpret_cast<intptr_t>(current) + skip);
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
      STATIC_ASSERT(kMaxRepeats == 13);
      case kConstantRepeat:
      FOUR_CASES(kConstantRepeat + 1)
      FOUR_CASES(kConstantRepeat + 5)
      FOUR_CASES(kConstantRepeat + 9) {
        int repeats = RepeatsForCode(data);
        Object* object = current[-1];
        ASSERT(!isolate->heap()->InNewSpace(object));
        for (int i = 0; i < repeats; i++) current[i] = object;
        current += repeats;
        break;
      }

      // Deserialize a new object and write a pointer to it to the current
      // object.
      ALL_SPACES(kNewObject, kPlain, kStartOfObject)
      // Support for direct instruction pointers in functions.  It's an inner
      // pointer because it points at the entry point, not at the start of the
      // code object.
      CASE_STATEMENT(kNewObject, kPlain, kInnerPointer, CODE_SPACE)
      CASE_BODY(kNewObject, kPlain, kInnerPointer, CODE_SPACE)
      // Deserialize a new code object and write a pointer to its first
      // instruction to the current code object.
      ALL_SPACES(kNewObject, kFromCode, kInnerPointer)
      // Find a recently deserialized object using its offset from the current
      // allocation point and write a pointer to it to the current object.
      ALL_SPACES(kBackref, kPlain, kStartOfObject)
      ALL_SPACES(kBackrefWithSkip, kPlain, kStartOfObject)
#if V8_TARGET_ARCH_MIPS
      // Deserialize a new object from pointer found in code and write
      // a pointer to it to the current object. Required only for MIPS, and
      // omitted on the other architectures because it is fully unrolled and
      // would cause bloat.
      ALL_SPACES(kNewObject, kFromCode, kStartOfObject)
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to it to the current
      // object. Required only for MIPS.
      ALL_SPACES(kBackref, kFromCode, kStartOfObject)
      ALL_SPACES(kBackrefWithSkip, kFromCode, kStartOfObject)
#endif
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to its first instruction
      // to the current code object or the instruction pointer in a function
      // object.
      ALL_SPACES(kBackref, kFromCode, kInnerPointer)
      ALL_SPACES(kBackrefWithSkip, kFromCode, kInnerPointer)
      ALL_SPACES(kBackref, kPlain, kInnerPointer)
      ALL_SPACES(kBackrefWithSkip, kPlain, kInnerPointer)
      // Find an object in the roots array and write a pointer to it to the
      // current object.
      CASE_STATEMENT(kRootArray, kPlain, kStartOfObject, 0)
      CASE_BODY(kRootArray, kPlain, kStartOfObject, 0)
      // Find an object in the partial snapshots cache and write a pointer to it
      // to the current object.
      CASE_STATEMENT(kPartialSnapshotCache, kPlain, kStartOfObject, 0)
      CASE_BODY(kPartialSnapshotCache,
                kPlain,
                kStartOfObject,
                0)
      // Find an code entry in the partial snapshots cache and
      // write a pointer to it to the current object.
      CASE_STATEMENT(kPartialSnapshotCache, kPlain, kInnerPointer, 0)
      CASE_BODY(kPartialSnapshotCache,
                kPlain,
                kInnerPointer,
                0)
      // Find an external reference and write a pointer to it to the current
      // object.
      CASE_STATEMENT(kExternalReference, kPlain, kStartOfObject, 0)
      CASE_BODY(kExternalReference,
                kPlain,
                kStartOfObject,
                0)
      // Find an external reference and write a pointer to it in the current
      // code object.
      CASE_STATEMENT(kExternalReference, kFromCode, kStartOfObject, 0)
      CASE_BODY(kExternalReference,
                kFromCode,
                kStartOfObject,
                0)

#undef CASE_STATEMENT
#undef CASE_BODY
#undef ALL_SPACES

      case kSkip: {
        int size = source_->GetInt();
        current = reinterpret_cast<Object**>(
            reinterpret_cast<intptr_t>(current) + size);
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
  ASSERT_EQ(limit, current);
}


void SnapshotByteSink::PutInt(uintptr_t integer, const char* description) {
  ASSERT(integer < 1 << 22);
  integer <<= 2;
  int bytes = 1;
  if (integer > 0xff) bytes = 2;
  if (integer > 0xffff) bytes = 3;
  integer |= bytes;
  Put(static_cast<int>(integer & 0xff), "IntPart1");
  if (bytes > 1) Put(static_cast<int>((integer >> 8) & 0xff), "IntPart2");
  if (bytes > 2) Put(static_cast<int>((integer >> 16) & 0xff), "IntPart3");
}


Serializer::Serializer(Isolate* isolate, SnapshotByteSink* sink)
    : isolate_(isolate),
      sink_(sink),
      external_reference_encoder_(new ExternalReferenceEncoder(isolate)),
      root_index_wave_front_(0) {
  // The serializer is meant to be used only to generate initial heap images
  // from a context in which there is only one isolate.
  for (int i = 0; i <= LAST_SPACE; i++) {
    fullness_[i] = 0;
  }
}


Serializer::~Serializer() {
  delete external_reference_encoder_;
}


void StartupSerializer::SerializeStrongReferences() {
  Isolate* isolate = this->isolate();
  // No active threads.
  CHECK_EQ(NULL, isolate->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK(isolate->handle_scope_implementer()->blocks()->is_empty());
  CHECK_EQ(0, isolate->global_handles()->NumberOfWeakHandles());
  CHECK_EQ(0, isolate->eternal_handles()->NumberOfHandles());
  // We don't support serializing installed extensions.
  CHECK(!isolate->has_installed_extensions());
  isolate->heap()->IterateSmiRoots(this);
  isolate->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG);
}


void PartialSerializer::Serialize(Object** object) {
  this->VisitPointer(object);
  Pad();
}


bool Serializer::ShouldBeSkipped(Object** current) {
  Object** roots = isolate()->heap()->roots_array_start();
  return current == &roots[Heap::kStoreBufferTopRootIndex]
      || current == &roots[Heap::kStackLimitRootIndex]
      || current == &roots[Heap::kRealStackLimitRootIndex];
}


void Serializer::VisitPointers(Object** start, Object** end) {
  Isolate* isolate = this->isolate();;

  for (Object** current = start; current < end; current++) {
    if (start == isolate->heap()->roots_array_start()) {
      root_index_wave_front_ =
          Max(root_index_wave_front_, static_cast<intptr_t>(current - start));
    }
    if (ShouldBeSkipped(current)) {
      sink_->Put(kSkip, "Skip");
      sink_->PutInt(kPointerSize, "SkipOneWord");
    } else if ((*current)->IsSmi()) {
      sink_->Put(kRawData + 1, "Smi");
      for (int i = 0; i < kPointerSize; i++) {
        sink_->Put(reinterpret_cast<byte*>(current)[i], "Byte");
      }
    } else {
      SerializeObject(*current, kPlain, kStartOfObject, 0);
    }
  }
}


// This ensures that the partial snapshot cache keeps things alive during GC and
// tracks their movement.  When it is called during serialization of the startup
// snapshot nothing happens.  When the partial (context) snapshot is created,
// this array is populated with the pointers that the partial snapshot will
// need. As that happens we emit serialized objects to the startup snapshot
// that correspond to the elements of this cache array.  On deserialization we
// therefore need to visit the cache array.  This fills it up with pointers to
// deserialized objects.
void SerializerDeserializer::Iterate(Isolate* isolate,
                                     ObjectVisitor* visitor) {
  if (Serializer::enabled()) return;
  for (int i = 0; ; i++) {
    if (isolate->serialize_partial_snapshot_cache_length() <= i) {
      // Extend the array ready to get a value from the visitor when
      // deserializing.
      isolate->PushToPartialSnapshotCache(Smi::FromInt(0));
    }
    Object** cache = isolate->serialize_partial_snapshot_cache();
    visitor->VisitPointers(&cache[i], &cache[i + 1]);
    // Sentinel is the undefined object, which is a root so it will not normally
    // be found in the cache.
    if (cache[i] == isolate->heap()->undefined_value()) {
      break;
    }
  }
}


int PartialSerializer::PartialSnapshotCacheIndex(HeapObject* heap_object) {
  Isolate* isolate = this->isolate();

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
  isolate->PushToPartialSnapshotCache(heap_object);
  startup_serializer_->VisitPointer(reinterpret_cast<Object**>(&heap_object));
  // We don't recurse from the startup snapshot generator into the partial
  // snapshot generator.
  ASSERT(length == isolate->serialize_partial_snapshot_cache_length() - 1);
  return length;
}


int Serializer::RootIndex(HeapObject* heap_object, HowToCode from) {
  Heap* heap = isolate()->heap();
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
    WhereToPoint where_to_point,
    int skip) {
  int offset = CurrentAllocationAddress(space) - address;
  // Shift out the bits that are always 0.
  offset >>= kObjectAlignmentBits;
  if (skip == 0) {
    sink_->Put(kBackref + how_to_code + where_to_point + space, "BackRefSer");
  } else {
    sink_->Put(kBackrefWithSkip + how_to_code + where_to_point + space,
               "BackRefSerWithSkip");
    sink_->PutInt(skip, "BackRefSkipDistance");
  }
  sink_->PutInt(offset, "offset");
}


void StartupSerializer::SerializeObject(
    Object* o,
    HowToCode how_to_code,
    WhereToPoint where_to_point,
    int skip) {
  CHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);

  int root_index;
  if ((root_index = RootIndex(heap_object, how_to_code)) != kInvalidRootIndex) {
    PutRoot(root_index, heap_object, how_to_code, where_to_point, skip);
    return;
  }

  if (address_mapper_.IsMapped(heap_object)) {
    int space = SpaceOfObject(heap_object);
    int address = address_mapper_.MappedTo(heap_object);
    SerializeReferenceToPreviousObject(space,
                                       address,
                                       how_to_code,
                                       where_to_point,
                                       skip);
  } else {
    if (skip != 0) {
      sink_->Put(kSkip, "FlushPendingSkip");
      sink_->PutInt(skip, "SkipDistance");
    }

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
  // This phase comes right after the partial serialization (of the snapshot).
  // After we have done the partial serialization the partial snapshot cache
  // will contain some references needed to decode the partial snapshot.  We
  // add one entry with 'undefined' which is the sentinel that the deserializer
  // uses to know it is done deserializing the array.
  Object* undefined = isolate()->heap()->undefined_value();
  VisitPointer(&undefined);
  isolate()->heap()->IterateWeakRoots(this, VISIT_ALL);
  Pad();
}


void Serializer::PutRoot(int root_index,
                         HeapObject* object,
                         SerializerDeserializer::HowToCode how_to_code,
                         SerializerDeserializer::WhereToPoint where_to_point,
                         int skip) {
  if (how_to_code == kPlain &&
      where_to_point == kStartOfObject &&
      root_index < kRootArrayNumberOfConstantEncodings &&
      !isolate()->heap()->InNewSpace(object)) {
    if (skip == 0) {
      sink_->Put(kRootArrayConstants + kNoSkipDistance + root_index,
                 "RootConstant");
    } else {
      sink_->Put(kRootArrayConstants + kHasSkipDistance + root_index,
                 "RootConstant");
      sink_->PutInt(skip, "SkipInPutRoot");
    }
  } else {
    if (skip != 0) {
      sink_->Put(kSkip, "SkipFromPutRoot");
      sink_->PutInt(skip, "SkipFromPutRootDistance");
    }
    sink_->Put(kRootArray + how_to_code + where_to_point, "RootSerialization");
    sink_->PutInt(root_index, "root_index");
  }
}


void PartialSerializer::SerializeObject(
    Object* o,
    HowToCode how_to_code,
    WhereToPoint where_to_point,
    int skip) {
  CHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);

  if (heap_object->IsMap()) {
    // The code-caches link to context-specific code objects, which
    // the startup and context serializes cannot currently handle.
    ASSERT(Map::cast(heap_object)->code_cache() ==
           heap_object->GetHeap()->empty_fixed_array());
  }

  int root_index;
  if ((root_index = RootIndex(heap_object, how_to_code)) != kInvalidRootIndex) {
    PutRoot(root_index, heap_object, how_to_code, where_to_point, skip);
    return;
  }

  if (ShouldBeInThePartialSnapshotCache(heap_object)) {
    if (skip != 0) {
      sink_->Put(kSkip, "SkipFromSerializeObject");
      sink_->PutInt(skip, "SkipDistanceFromSerializeObject");
    }

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
  // All the internalized strings that the partial snapshot needs should be
  // either in the root table or in the partial snapshot cache.
  ASSERT(!heap_object->IsInternalizedString());

  if (address_mapper_.IsMapped(heap_object)) {
    int space = SpaceOfObject(heap_object);
    int address = address_mapper_.MappedTo(heap_object);
    SerializeReferenceToPreviousObject(space,
                                       address,
                                       how_to_code,
                                       where_to_point,
                                       skip);
  } else {
    if (skip != 0) {
      sink_->Put(kSkip, "SkipFromSerializeObject");
      sink_->PutInt(skip, "SkipDistanceFromSerializeObject");
    }
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

  ASSERT(code_address_map_);
  const char* code_name = code_address_map_->Lookup(object_->address());
  LOG(serializer_->isolate_,
      CodeNameEvent(object_->address(), sink_->Position(), code_name));
  LOG(serializer_->isolate_,
      SnapshotPositionEvent(object_->address(), sink_->Position()));

  // Mark this object as already serialized.
  int offset = serializer_->Allocate(space, size);
  serializer_->address_mapper()->AddMapping(object_, offset);

  // Serialize the map (first word of the object).
  serializer_->SerializeObject(object_->map(), kPlain, kStartOfObject, 0);

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
        ASSERT(!serializer_->isolate()->heap()->InNewSpace(current_contents));
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
        serializer_->SerializeObject(
                current_contents, kPlain, kStartOfObject, 0);
        bytes_processed_so_far_ += kPointerSize;
        current++;
      }
    }
  }
}


void Serializer::ObjectSerializer::VisitEmbeddedPointer(RelocInfo* rinfo) {
  int skip = OutputRawData(rinfo->target_address_address(),
                           kCanReturnSkipInsteadOfSkipping);
  HowToCode how_to_code = rinfo->IsCodedSpecially() ? kFromCode : kPlain;
  Object* object = rinfo->target_object();
  serializer_->SerializeObject(object, how_to_code, kStartOfObject, skip);
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitExternalReference(Address* p) {
  int skip = OutputRawData(reinterpret_cast<Address>(p),
                           kCanReturnSkipInsteadOfSkipping);
  sink_->Put(kExternalReference + kPlain + kStartOfObject, "ExternalRef");
  sink_->PutInt(skip, "SkipB4ExternalRef");
  Address target = *p;
  sink_->PutInt(serializer_->EncodeExternalReference(target), "reference id");
  bytes_processed_so_far_ += kPointerSize;
}


void Serializer::ObjectSerializer::VisitExternalReference(RelocInfo* rinfo) {
  int skip = OutputRawData(rinfo->target_address_address(),
                           kCanReturnSkipInsteadOfSkipping);
  HowToCode how_to_code = rinfo->IsCodedSpecially() ? kFromCode : kPlain;
  sink_->Put(kExternalReference + how_to_code + kStartOfObject, "ExternalRef");
  sink_->PutInt(skip, "SkipB4ExternalRef");
  Address target = rinfo->target_reference();
  sink_->PutInt(serializer_->EncodeExternalReference(target), "reference id");
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitRuntimeEntry(RelocInfo* rinfo) {
  int skip = OutputRawData(rinfo->target_address_address(),
                           kCanReturnSkipInsteadOfSkipping);
  HowToCode how_to_code = rinfo->IsCodedSpecially() ? kFromCode : kPlain;
  sink_->Put(kExternalReference + how_to_code + kStartOfObject, "ExternalRef");
  sink_->PutInt(skip, "SkipB4ExternalRef");
  Address target = rinfo->target_address();
  sink_->PutInt(serializer_->EncodeExternalReference(target), "reference id");
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitCodeTarget(RelocInfo* rinfo) {
  int skip = OutputRawData(rinfo->target_address_address(),
                           kCanReturnSkipInsteadOfSkipping);
  Code* object = Code::GetCodeFromTargetAddress(rinfo->target_address());
  serializer_->SerializeObject(object, kFromCode, kInnerPointer, skip);
  bytes_processed_so_far_ += rinfo->target_address_size();
}


void Serializer::ObjectSerializer::VisitCodeEntry(Address entry_address) {
  int skip = OutputRawData(entry_address, kCanReturnSkipInsteadOfSkipping);
  Code* object = Code::cast(Code::GetObjectFromEntryAddress(entry_address));
  serializer_->SerializeObject(object, kPlain, kInnerPointer, skip);
  bytes_processed_so_far_ += kPointerSize;
}


void Serializer::ObjectSerializer::VisitCell(RelocInfo* rinfo) {
  int skip = OutputRawData(rinfo->pc(), kCanReturnSkipInsteadOfSkipping);
  Cell* object = Cell::cast(rinfo->target_cell());
  serializer_->SerializeObject(object, kPlain, kInnerPointer, skip);
}


void Serializer::ObjectSerializer::VisitExternalAsciiString(
    v8::String::ExternalAsciiStringResource** resource_pointer) {
  Address references_start = reinterpret_cast<Address>(resource_pointer);
  OutputRawData(references_start);
  for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
    Object* source =
        serializer_->isolate()->heap()->natives_source_cache()->get(i);
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


static Code* CloneCodeObject(HeapObject* code) {
  Address copy = new byte[code->Size()];
  OS::MemCopy(copy, code->address(), code->Size());
  return Code::cast(HeapObject::FromAddress(copy));
}


static void WipeOutRelocations(Code* code) {
  int mode_mask =
      RelocInfo::kCodeTargetMask |
      RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
      RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
  for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
    it.rinfo()->WipeOut();
  }
}


int Serializer::ObjectSerializer::OutputRawData(
    Address up_to, Serializer::ObjectSerializer::ReturnSkip return_skip) {
  Address object_start = object_->address();
  int base = bytes_processed_so_far_;
  int up_to_offset = static_cast<int>(up_to - object_start);
  int to_skip = up_to_offset - bytes_processed_so_far_;
  int bytes_to_output = to_skip;
  bytes_processed_so_far_ +=  to_skip;
  // This assert will fail if the reloc info gives us the target_address_address
  // locations in a non-ascending order.  Luckily that doesn't happen.
  ASSERT(to_skip >= 0);
  bool outputting_code = false;
  if (to_skip != 0 && code_object_ && !code_has_been_output_) {
    // Output the code all at once and fix later.
    bytes_to_output = object_->Size() + to_skip - bytes_processed_so_far_;
    outputting_code = true;
    code_has_been_output_ = true;
  }
  if (bytes_to_output != 0 &&
      (!code_object_ || outputting_code)) {
#define RAW_CASE(index)                                                        \
    if (!outputting_code && bytes_to_output == index * kPointerSize &&         \
        index * kPointerSize == to_skip) {                                     \
      sink_->PutSection(kRawData + index, "RawDataFixed");                     \
      to_skip = 0;  /* This insn already skips. */                             \
    } else  /* NOLINT */
    COMMON_RAW_LENGTHS(RAW_CASE)
#undef RAW_CASE
    {  /* NOLINT */
      // We always end up here if we are outputting the code of a code object.
      sink_->Put(kRawData, "RawData");
      sink_->PutInt(bytes_to_output, "length");
    }

    // To make snapshots reproducible, we need to wipe out all pointers in code.
    if (code_object_) {
      Code* code = CloneCodeObject(object_);
      WipeOutRelocations(code);
      // We need to wipe out the header fields *after* wiping out the
      // relocations, because some of these fields are needed for the latter.
      code->WipeOutHeader();
      object_start = code->address();
    }

    const char* description = code_object_ ? "Code" : "Byte";
    for (int i = 0; i < bytes_to_output; i++) {
      sink_->PutSection(object_start[base + i], description);
    }
    if (code_object_) delete[] object_start;
  }
  if (to_skip != 0 && return_skip == kIgnoringReturn) {
    sink_->Put(kSkip, "Skip");
    sink_->PutInt(to_skip, "SkipDistance");
    to_skip = 0;
  }
  return to_skip;
}


int Serializer::SpaceOfObject(HeapObject* object) {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    AllocationSpace s = static_cast<AllocationSpace>(i);
    if (object->GetHeap()->InSpace(object, s)) {
      ASSERT(i < kNumberOfSpaces);
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer::Allocate(int space, int size) {
  CHECK(space >= 0 && space < kNumberOfSpaces);
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


void Serializer::Pad() {
  // The non-branching GetInt will read up to 3 bytes too far, so we need
  // to pad the snapshot to make sure we don't read over the end.
  for (unsigned i = 0; i < sizeof(int32_t) - 1; i++) {
    sink_->Put(kNop, "Padding");
  }
}


bool SnapshotByteSource::AtEOF() {
  if (0u + length_ - position_ > 2 * sizeof(uint32_t)) return false;
  for (int x = position_; x < length_; x++) {
    if (data_[x] != SerializerDeserializer::nop()) return false;
  }
  return true;
}

} }  // namespace v8::internal

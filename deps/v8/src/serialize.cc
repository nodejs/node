// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/global-handles.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/natives.h"
#include "src/objects.h"
#include "src/runtime/runtime.h"
#include "src/serialize.h"
#include "src/snapshot.h"
#include "src/snapshot-source-sink.h"
#include "src/v8threads.h"
#include "src/version.h"

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
  DCHECK_NE(NULL, address);
  ExternalReferenceEntry entry;
  entry.address = address;
  entry.code = EncodeExternal(type, id);
  entry.name = name;
  DCHECK_NE(0, entry.code);
  // Assert that the code is added in ascending order to rule out duplicates.
  DCHECK((size() == 0) || (code(size() - 1) < entry.code));
  refs_.Add(entry);
  if (id > max_id_[type]) max_id_[type] = id;
}


void ExternalReferenceTable::PopulateTable(Isolate* isolate) {
  for (int type_code = 0; type_code < kTypeCodeCount; type_code++) {
    max_id_[type_code] = 0;
  }

  // Miscellaneous
  Add(ExternalReference::roots_array_start(isolate).address(),
      "Heap::roots_array_start()");
  Add(ExternalReference::address_of_stack_limit(isolate).address(),
      "StackGuard::address_of_jslimit()");
  Add(ExternalReference::address_of_real_stack_limit(isolate).address(),
      "StackGuard::address_of_real_jslimit()");
  Add(ExternalReference::new_space_start(isolate).address(),
      "Heap::NewSpaceStart()");
  Add(ExternalReference::new_space_mask(isolate).address(),
      "Heap::NewSpaceMask()");
  Add(ExternalReference::new_space_allocation_limit_address(isolate).address(),
      "Heap::NewSpaceAllocationLimitAddress()");
  Add(ExternalReference::new_space_allocation_top_address(isolate).address(),
      "Heap::NewSpaceAllocationTopAddress()");
  Add(ExternalReference::debug_break(isolate).address(), "Debug::Break()");
  Add(ExternalReference::debug_step_in_fp_address(isolate).address(),
      "Debug::step_in_fp_addr()");
  Add(ExternalReference::mod_two_doubles_operation(isolate).address(),
      "mod_two_doubles");
  // Keyed lookup cache.
  Add(ExternalReference::keyed_lookup_cache_keys(isolate).address(),
      "KeyedLookupCache::keys()");
  Add(ExternalReference::keyed_lookup_cache_field_offsets(isolate).address(),
      "KeyedLookupCache::field_offsets()");
  Add(ExternalReference::handle_scope_next_address(isolate).address(),
      "HandleScope::next");
  Add(ExternalReference::handle_scope_limit_address(isolate).address(),
      "HandleScope::limit");
  Add(ExternalReference::handle_scope_level_address(isolate).address(),
      "HandleScope::level");
  Add(ExternalReference::new_deoptimizer_function(isolate).address(),
      "Deoptimizer::New()");
  Add(ExternalReference::compute_output_frames_function(isolate).address(),
      "Deoptimizer::ComputeOutputFrames()");
  Add(ExternalReference::address_of_min_int().address(),
      "LDoubleConstant::min_int");
  Add(ExternalReference::address_of_one_half().address(),
      "LDoubleConstant::one_half");
  Add(ExternalReference::isolate_address(isolate).address(), "isolate");
  Add(ExternalReference::address_of_negative_infinity().address(),
      "LDoubleConstant::negative_infinity");
  Add(ExternalReference::power_double_double_function(isolate).address(),
      "power_double_double_function");
  Add(ExternalReference::power_double_int_function(isolate).address(),
      "power_double_int_function");
  Add(ExternalReference::math_log_double_function(isolate).address(),
      "std::log");
  Add(ExternalReference::store_buffer_top(isolate).address(),
      "store_buffer_top");
  Add(ExternalReference::address_of_canonical_non_hole_nan().address(),
      "canonical_nan");
  Add(ExternalReference::address_of_the_hole_nan().address(), "the_hole_nan");
  Add(ExternalReference::get_date_field_function(isolate).address(),
      "JSDate::GetField");
  Add(ExternalReference::date_cache_stamp(isolate).address(),
      "date_cache_stamp");
  Add(ExternalReference::address_of_pending_message_obj(isolate).address(),
      "address_of_pending_message_obj");
  Add(ExternalReference::address_of_has_pending_message(isolate).address(),
      "address_of_has_pending_message");
  Add(ExternalReference::address_of_pending_message_script(isolate).address(),
      "pending_message_script");
  Add(ExternalReference::get_make_code_young_function(isolate).address(),
      "Code::MakeCodeYoung");
  Add(ExternalReference::cpu_features().address(), "cpu_features");
  Add(ExternalReference(Runtime::kAllocateInNewSpace, isolate).address(),
      "Runtime::AllocateInNewSpace");
  Add(ExternalReference(Runtime::kAllocateInTargetSpace, isolate).address(),
      "Runtime::AllocateInTargetSpace");
  Add(ExternalReference::old_pointer_space_allocation_top_address(isolate)
          .address(),
      "Heap::OldPointerSpaceAllocationTopAddress");
  Add(ExternalReference::old_pointer_space_allocation_limit_address(isolate)
          .address(),
      "Heap::OldPointerSpaceAllocationLimitAddress");
  Add(ExternalReference::old_data_space_allocation_top_address(isolate)
          .address(),
      "Heap::OldDataSpaceAllocationTopAddress");
  Add(ExternalReference::old_data_space_allocation_limit_address(isolate)
          .address(),
      "Heap::OldDataSpaceAllocationLimitAddress");
  Add(ExternalReference::allocation_sites_list_address(isolate).address(),
      "Heap::allocation_sites_list_address()");
  Add(ExternalReference::address_of_uint32_bias().address(), "uint32_bias");
  Add(ExternalReference::get_mark_code_as_executed_function(isolate).address(),
      "Code::MarkCodeAsExecuted");
  Add(ExternalReference::is_profiling_address(isolate).address(),
      "CpuProfiler::is_profiling");
  Add(ExternalReference::scheduled_exception_address(isolate).address(),
      "Isolate::scheduled_exception");
  Add(ExternalReference::invoke_function_callback(isolate).address(),
      "InvokeFunctionCallback");
  Add(ExternalReference::invoke_accessor_getter_callback(isolate).address(),
      "InvokeAccessorGetterCallback");
  Add(ExternalReference::flush_icache_function(isolate).address(),
      "CpuFeatures::FlushICache");
  Add(ExternalReference::log_enter_external_function(isolate).address(),
      "Logger::EnterExternal");
  Add(ExternalReference::log_leave_external_function(isolate).address(),
      "Logger::LeaveExternal");
  Add(ExternalReference::address_of_minus_one_half().address(),
      "double_constants.minus_one_half");
  Add(ExternalReference::stress_deopt_count(isolate).address(),
      "Isolate::stress_deopt_count_address()");
  Add(ExternalReference::incremental_marking_record_write_function(isolate)
          .address(),
      "IncrementalMarking::RecordWriteFromCode");

  // Debug addresses
  Add(ExternalReference::debug_after_break_target_address(isolate).address(),
      "Debug::after_break_target_address()");
  Add(ExternalReference::debug_restarter_frame_function_pointer_address(isolate)
          .address(),
      "Debug::restarter_frame_function_pointer_address()");
  Add(ExternalReference::debug_is_active_address(isolate).address(),
      "Debug::is_active_address()");

#ifndef V8_INTERPRETED_REGEXP
  Add(ExternalReference::re_case_insensitive_compare_uc16(isolate).address(),
      "NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()");
  Add(ExternalReference::re_check_stack_guard_state(isolate).address(),
      "RegExpMacroAssembler*::CheckStackGuardState()");
  Add(ExternalReference::re_grow_stack(isolate).address(),
      "NativeRegExpMacroAssembler::GrowStack()");
  Add(ExternalReference::re_word_character_map().address(),
      "NativeRegExpMacroAssembler::word_character_map");
  Add(ExternalReference::address_of_regexp_stack_limit(isolate).address(),
      "RegExpStack::limit_address()");
  Add(ExternalReference::address_of_regexp_stack_memory_address(isolate)
          .address(),
      "RegExpStack::memory_address()");
  Add(ExternalReference::address_of_regexp_stack_memory_size(isolate).address(),
      "RegExpStack::memory_size()");
  Add(ExternalReference::address_of_static_offsets_vector(isolate).address(),
      "OffsetsVector::static_offsets_vector");
#endif  // V8_INTERPRETED_REGEXP

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
  INLINE_OPTIMIZED_FUNCTION_LIST(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY

#define INLINE_OPTIMIZED_ENTRY(name, nargs, ressize) \
  { RUNTIME_FUNCTION, \
    Runtime::kInlineOptimized##name, \
    "Runtime::" #name },

  INLINE_OPTIMIZED_FUNCTION_LIST(INLINE_OPTIMIZED_ENTRY)
#undef INLINE_OPTIMIZED_ENTRY

  // IC utilities
#define IC_ENTRY(name) \
  { IC_UTILITY, \
    IC::k##name, \
    "IC::" #name },

  IC_UTIL_LIST(IC_ENTRY)
#undef IC_ENTRY
  };  // end of ref_table[].

  for (size_t i = 0; i < arraysize(ref_table); ++i) {
    AddFromId(ref_table[i].type,
              ref_table[i].id,
              ref_table[i].name,
              isolate);
  }

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
  for (size_t i = 0; i < arraysize(stats_ref_table); ++i) {
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
#define ACCESSOR_INFO_DECLARATION(name) \
  Add(FUNCTION_ADDR(&Accessors::name##Getter), \
      ACCESSOR, \
      Accessors::k##name##Getter, \
      "Accessors::" #name "Getter"); \
  Add(FUNCTION_ADDR(&Accessors::name##Setter), \
      ACCESSOR, \
      Accessors::k##name##Setter, \
      "Accessors::" #name "Setter");
  ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION

  StubCache* stub_cache = isolate->stub_cache();

  // Stub cache tables
  Add(stub_cache->key_reference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE, 1, "StubCache::primary_->key");
  Add(stub_cache->value_reference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE, 2, "StubCache::primary_->value");
  Add(stub_cache->map_reference(StubCache::kPrimary).address(),
      STUB_CACHE_TABLE, 3, "StubCache::primary_->map");
  Add(stub_cache->key_reference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE, 4, "StubCache::secondary_->key");
  Add(stub_cache->value_reference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE, 5, "StubCache::secondary_->value");
  Add(stub_cache->map_reference(StubCache::kSecondary).address(),
      STUB_CACHE_TABLE, 6, "StubCache::secondary_->map");

  // Runtime entries
  Add(ExternalReference::delete_handle_scope_extensions(isolate).address(),
      RUNTIME_ENTRY, 1, "HandleScope::DeleteExtensions");
  Add(ExternalReference::incremental_marking_record_write_function(isolate)
          .address(),
      RUNTIME_ENTRY, 2, "IncrementalMarking::RecordWrite");
  Add(ExternalReference::store_buffer_overflow_function(isolate).address(),
      RUNTIME_ENTRY, 3, "StoreBuffer::StoreBufferOverflow");

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
    : encodings_(HashMap::PointersMatch),
      isolate_(isolate) {
  ExternalReferenceTable* external_references =
      ExternalReferenceTable::instance(isolate_);
  for (int i = 0; i < external_references->size(); ++i) {
    Put(external_references->address(i), i);
  }
}


uint32_t ExternalReferenceEncoder::Encode(Address key) const {
  int index = IndexOf(key);
  DCHECK(key == NULL || index >= 0);
  return index >= 0 ?
         ExternalReferenceTable::instance(isolate_)->code(index) : 0;
}


const char* ExternalReferenceEncoder::NameOfAddress(Address key) const {
  int index = IndexOf(key);
  return index >= 0 ? ExternalReferenceTable::instance(isolate_)->name(index)
                    : "<unknown>";
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

  virtual void CodeDisableOptEvent(Code* code, SharedFunctionInfo* shared) {
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
    NameMap() : impl_(HashMap::PointersMatch) {}

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
      DCHECK(from_entry != NULL);
      void* value = from_entry->value;
      RemoveEntry(from_entry);
      HashMap::Entry* to_entry = FindOrCreateEntry(to);
      DCHECK(to_entry->value == NULL);
      to_entry->value = value;
    }

   private:
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


Deserializer::Deserializer(SnapshotByteSource* source)
    : isolate_(NULL),
      attached_objects_(NULL),
      source_(source),
      external_reference_decoder_(NULL),
      deserialized_large_objects_(0) {
  for (int i = 0; i < kNumberOfSpaces; i++) {
    reservations_[i] = kUninitializedReservation;
  }
}


void Deserializer::FlushICacheForNewCodeObjects() {
  PageIterator it(isolate_->heap()->code_space());
  while (it.has_next()) {
    Page* p = it.next();
    CpuFeatures::FlushICache(p->area_start(), p->area_end() - p->area_start());
  }
}


void Deserializer::Deserialize(Isolate* isolate) {
  isolate_ = isolate;
  DCHECK(isolate_ != NULL);
  isolate_->heap()->ReserveSpace(reservations_, high_water_);
  // No active threads.
  DCHECK_EQ(NULL, isolate_->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate_->handle_scope_implementer()->blocks()->is_empty());
  DCHECK_EQ(NULL, external_reference_decoder_);
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
      ExternalOneByteString::cast(source)->update_data_cache();
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
    DCHECK(reservations_[i] != kUninitializedReservation);
  }
  Heap* heap = isolate->heap();
  heap->ReserveSpace(reservations_, high_water_);
  if (external_reference_decoder_ == NULL) {
    external_reference_decoder_ = new ExternalReferenceDecoder(isolate);
  }

  DisallowHeapAllocation no_gc;

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
  // TODO(svenpanne) Re-enable this assertion when v8 initialization is fixed.
  // DCHECK(source_->AtEOF());
  if (external_reference_decoder_) {
    delete external_reference_decoder_;
    external_reference_decoder_ = NULL;
  }
  if (attached_objects_) attached_objects_->Dispose();
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


// Used to insert a deserialized internalized string into the string table.
class StringTableInsertionKey : public HashTableKey {
 public:
  explicit StringTableInsertionKey(String* string)
      : string_(string), hash_(HashForObject(string)) {
    DCHECK(string->IsInternalizedString());
  }

  virtual bool IsMatch(Object* string) {
    // We know that all entries in a hash table had their hash keys created.
    // Use that knowledge to have fast failure.
    if (hash_ != HashForObject(string)) return false;
    // We want to compare the content of two internalized strings here.
    return string_->SlowEquals(String::cast(string));
  }

  virtual uint32_t Hash() OVERRIDE { return hash_; }

  virtual uint32_t HashForObject(Object* key) OVERRIDE {
    return String::cast(key)->Hash();
  }

  MUST_USE_RESULT virtual Handle<Object> AsHandle(Isolate* isolate)
      OVERRIDE {
    return handle(string_, isolate);
  }

  String* string_;
  uint32_t hash_;
};


HeapObject* Deserializer::ProcessNewObjectFromSerializedCode(HeapObject* obj) {
  if (obj->IsString()) {
    String* string = String::cast(obj);
    // Uninitialize hash field as the hash seed may have changed.
    string->set_hash_field(String::kEmptyHashField);
    if (string->IsInternalizedString()) {
      DisallowHeapAllocation no_gc;
      HandleScope scope(isolate_);
      StringTableInsertionKey key(string);
      String* canonical = *StringTable::LookupKey(isolate_, &key);
      string->SetForwardedInternalizedString(canonical);
      return canonical;
    }
  }
  return obj;
}


Object* Deserializer::ProcessBackRefInSerializedCode(Object* obj) {
  if (obj->IsInternalizedString()) {
    return String::cast(obj)->GetForwardedInternalizedString();
  }
  return obj;
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
  isolate_->heap()->OnAllocationEvent(obj, size);
  Object** current = reinterpret_cast<Object**>(address);
  Object** limit = current + (size >> kPointerSizeLog2);
  if (FLAG_log_snapshot_positions) {
    LOG(isolate_, SnapshotPositionEvent(address, source_->position()));
  }
  ReadChunk(current, limit, space_number, address);

  // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
  // as a (weak) root. If this root is relocated correctly,
  // RelinkAllocationSite() isn't necessary.
  if (obj->IsAllocationSite()) RelinkAllocationSite(AllocationSite::cast(obj));

  // Fix up strings from serialized user code.
  if (deserializing_user_code()) obj = ProcessNewObjectFromSerializedCode(obj);

  *write_back = obj;
#ifdef DEBUG
  if (obj->IsCode()) {
    DCHECK(space_number == CODE_SPACE || space_number == LO_SPACE);
  } else {
    DCHECK(space_number != CODE_SPACE);
  }
#endif
}


// We know the space requirements before deserialization and can
// pre-allocate that reserved space. During deserialization, all we need
// to do is to bump up the pointer for each space in the reserved
// space. This is also used for fixing back references.
// Since multiple large objects cannot be folded into one large object
// space allocation, we have to do an actual allocation when deserializing
// each large object. Instead of tracking offset for back references, we
// reference large objects by index.
Address Deserializer::Allocate(int space_index, int size) {
  if (space_index == LO_SPACE) {
    AlwaysAllocateScope scope(isolate_);
    LargeObjectSpace* lo_space = isolate_->heap()->lo_space();
    Executability exec = static_cast<Executability>(source_->GetInt());
    AllocationResult result = lo_space->AllocateRaw(size, exec);
    HeapObject* obj = HeapObject::cast(result.ToObjectChecked());
    deserialized_large_objects_.Add(obj);
    return obj->address();
  } else {
    DCHECK(space_index < kNumberOfPreallocatedSpaces);
    Address address = high_water_[space_index];
    high_water_[space_index] = address + size;
    return address;
  }
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
#define CASE_STATEMENT(where, how, within, space_number) \
  case where + how + within + space_number:              \
    STATIC_ASSERT((where & ~kPointedToMask) == 0);       \
    STATIC_ASSERT((how & ~kHowToCodeMask) == 0);         \
    STATIC_ASSERT((within & ~kWhereToPointMask) == 0);   \
    STATIC_ASSERT((space_number & ~kSpaceMask) == 0);

#define CASE_BODY(where, how, within, space_number_if_any)                     \
  {                                                                            \
    bool emit_write_barrier = false;                                           \
    bool current_was_incremented = false;                                      \
    int space_number = space_number_if_any == kAnyOldSpace                     \
                           ? (data & kSpaceMask)                               \
                           : space_number_if_any;                              \
    if (where == kNewObject && how == kPlain && within == kStartOfObject) {    \
      ReadObject(space_number, current);                                       \
      emit_write_barrier = (space_number == NEW_SPACE);                        \
    } else {                                                                   \
      Object* new_object = NULL; /* May not be a real Object pointer. */       \
      if (where == kNewObject) {                                               \
        ReadObject(space_number, &new_object);                                 \
      } else if (where == kRootArray) {                                        \
        int root_id = source_->GetInt();                                       \
        new_object = isolate->heap()->roots_array_start()[root_id];            \
        emit_write_barrier = isolate->heap()->InNewSpace(new_object);          \
      } else if (where == kPartialSnapshotCache) {                             \
        int cache_index = source_->GetInt();                                   \
        new_object = isolate->serialize_partial_snapshot_cache()[cache_index]; \
        emit_write_barrier = isolate->heap()->InNewSpace(new_object);          \
      } else if (where == kExternalReference) {                                \
        int skip = source_->GetInt();                                          \
        current = reinterpret_cast<Object**>(                                  \
            reinterpret_cast<Address>(current) + skip);                        \
        int reference_id = source_->GetInt();                                  \
        Address address = external_reference_decoder_->Decode(reference_id);   \
        new_object = reinterpret_cast<Object*>(address);                       \
      } else if (where == kBackref) {                                          \
        emit_write_barrier = (space_number == NEW_SPACE);                      \
        new_object = GetAddressFromEnd(data & kSpaceMask);                     \
        if (deserializing_user_code()) {                                       \
          new_object = ProcessBackRefInSerializedCode(new_object);             \
        }                                                                      \
      } else if (where == kBuiltin) {                                          \
        DCHECK(deserializing_user_code());                                     \
        int builtin_id = source_->GetInt();                                    \
        DCHECK_LE(0, builtin_id);                                              \
        DCHECK_LT(builtin_id, Builtins::builtin_count);                        \
        Builtins::Name name = static_cast<Builtins::Name>(builtin_id);         \
        new_object = isolate->builtins()->builtin(name);                       \
        emit_write_barrier = false;                                            \
      } else if (where == kAttachedReference) {                                \
        DCHECK(deserializing_user_code());                                     \
        int index = source_->GetInt();                                         \
        new_object = *attached_objects_->at(index);                            \
        emit_write_barrier = isolate->heap()->InNewSpace(new_object);          \
      } else {                                                                 \
        DCHECK(where == kBackrefWithSkip);                                     \
        int skip = source_->GetInt();                                          \
        current = reinterpret_cast<Object**>(                                  \
            reinterpret_cast<Address>(current) + skip);                        \
        emit_write_barrier = (space_number == NEW_SPACE);                      \
        new_object = GetAddressFromEnd(data & kSpaceMask);                     \
        if (deserializing_user_code()) {                                       \
          new_object = ProcessBackRefInSerializedCode(new_object);             \
        }                                                                      \
      }                                                                        \
      if (within == kInnerPointer) {                                           \
        if (space_number != CODE_SPACE || new_object->IsCode()) {              \
          Code* new_code_object = reinterpret_cast<Code*>(new_object);         \
          new_object =                                                         \
              reinterpret_cast<Object*>(new_code_object->instruction_start()); \
        } else {                                                               \
          DCHECK(space_number == CODE_SPACE);                                  \
          Cell* cell = Cell::cast(new_object);                                 \
          new_object = reinterpret_cast<Object*>(cell->ValueAddress());        \
        }                                                                      \
      }                                                                        \
      if (how == kFromCode) {                                                  \
        Address location_of_branch_data = reinterpret_cast<Address>(current);  \
        Assembler::deserialization_set_special_target_at(                      \
            location_of_branch_data,                                           \
            Code::cast(HeapObject::FromAddress(current_object_address)),       \
            reinterpret_cast<Address>(new_object));                            \
        location_of_branch_data += Assembler::kSpecialTargetSize;              \
        current = reinterpret_cast<Object**>(location_of_branch_data);         \
        current_was_incremented = true;                                        \
      } else {                                                                 \
        *current = new_object;                                                 \
      }                                                                        \
    }                                                                          \
    if (emit_write_barrier && write_barrier_needed) {                          \
      Address current_address = reinterpret_cast<Address>(current);            \
      isolate->heap()->RecordWrite(                                            \
          current_object_address,                                              \
          static_cast<int>(current_address - current_object_address));         \
    }                                                                          \
    if (!current_was_incremented) {                                            \
      current++;                                                               \
    }                                                                          \
    break;                                                                     \
  }

// This generates a case and a body for the new space (which has to do extra
// write barrier handling) and handles the other spaces with 8 fall-through
// cases and one body.
#define ALL_SPACES(where, how, within)                    \
  CASE_STATEMENT(where, how, within, NEW_SPACE)           \
  CASE_BODY(where, how, within, NEW_SPACE)                \
  CASE_STATEMENT(where, how, within, OLD_DATA_SPACE)      \
  CASE_STATEMENT(where, how, within, OLD_POINTER_SPACE)   \
  CASE_STATEMENT(where, how, within, CODE_SPACE)          \
  CASE_STATEMENT(where, how, within, MAP_SPACE)           \
  CASE_STATEMENT(where, how, within, CELL_SPACE)          \
  CASE_STATEMENT(where, how, within, PROPERTY_CELL_SPACE) \
  CASE_STATEMENT(where, how, within, LO_SPACE)            \
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
        DCHECK(!isolate->heap()->InNewSpace(object));
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
        DCHECK(!isolate->heap()->InNewSpace(object));
        *current++ = object;
        break;
      }

      case kRepeat: {
        int repeats = source_->GetInt();
        Object* object = current[-1];
        DCHECK(!isolate->heap()->InNewSpace(object));
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
        DCHECK(!isolate->heap()->InNewSpace(object));
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
#if defined(V8_TARGET_ARCH_MIPS) || V8_OOL_CONSTANT_POOL || \
    defined(V8_TARGET_ARCH_MIPS64)
      // Deserialize a new object from pointer found in code and write
      // a pointer to it to the current object. Required only for MIPS or ARM
      // with ool constant pool, and omitted on the other architectures because
      // it is fully unrolled and would cause bloat.
      ALL_SPACES(kNewObject, kFromCode, kStartOfObject)
      // Find a recently deserialized code object using its offset from the
      // current allocation point and write a pointer to it to the current
      // object. Required only for MIPS or ARM with ool constant pool.
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
#if defined(V8_TARGET_ARCH_MIPS) || V8_OOL_CONSTANT_POOL || \
    defined(V8_TARGET_ARCH_MIPS64)
      // Find an object in the roots array and write a pointer to it to in code.
      CASE_STATEMENT(kRootArray, kFromCode, kStartOfObject, 0)
      CASE_BODY(kRootArray, kFromCode, kStartOfObject, 0)
#endif
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
      // Find a builtin and write a pointer to it to the current object.
      CASE_STATEMENT(kBuiltin, kPlain, kStartOfObject, 0)
      CASE_BODY(kBuiltin, kPlain, kStartOfObject, 0)
#if V8_OOL_CONSTANT_POOL
      // Find a builtin code entry and write a pointer to it to the current
      // object.
      CASE_STATEMENT(kBuiltin, kPlain, kInnerPointer, 0)
      CASE_BODY(kBuiltin, kPlain, kInnerPointer, 0)
#endif
      // Find a builtin and write a pointer to it in the current code object.
      CASE_STATEMENT(kBuiltin, kFromCode, kInnerPointer, 0)
      CASE_BODY(kBuiltin, kFromCode, kInnerPointer, 0)
      // Find an object in the attached references and write a pointer to it to
      // the current object.
      CASE_STATEMENT(kAttachedReference, kPlain, kStartOfObject, 0)
      CASE_BODY(kAttachedReference, kPlain, kStartOfObject, 0)
      CASE_STATEMENT(kAttachedReference, kPlain, kInnerPointer, 0)
      CASE_BODY(kAttachedReference, kPlain, kInnerPointer, 0)
      CASE_STATEMENT(kAttachedReference, kFromCode, kInnerPointer, 0)
      CASE_BODY(kAttachedReference, kFromCode, kInnerPointer, 0)

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
  DCHECK_EQ(limit, current);
}


Serializer::Serializer(Isolate* isolate, SnapshotByteSink* sink)
    : isolate_(isolate),
      sink_(sink),
      external_reference_encoder_(new ExternalReferenceEncoder(isolate)),
      root_index_wave_front_(0),
      code_address_map_(NULL),
      seen_large_objects_index_(0) {
  // The serializer is meant to be used only to generate initial heap images
  // from a context in which there is only one isolate.
  for (int i = 0; i < kNumberOfSpaces; i++) fullness_[i] = 0;
}


Serializer::~Serializer() {
  delete external_reference_encoder_;
  if (code_address_map_ != NULL) delete code_address_map_;
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
  if (isolate->serializer_enabled()) return;
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
  DCHECK(length == isolate->serialize_partial_snapshot_cache_length() - 1);
  return length;
}


int Serializer::RootIndex(HeapObject* heap_object, HowToCode from) {
  Heap* heap = isolate()->heap();
  if (heap->InNewSpace(heap_object)) return kInvalidRootIndex;
  for (int i = 0; i < root_index_wave_front_; i++) {
    Object* root = heap->roots_array_start()[i];
    if (!root->IsSmi() && root == heap_object) {
      return i;
    }
  }
  return kInvalidRootIndex;
}


// Encode the location of an already deserialized object in order to write its
// location into a later object.  We can encode the location as an offset from
// the start of the deserialized objects or as an offset backwards from the
// current allocation pointer.
void Serializer::SerializeReferenceToPreviousObject(HeapObject* heap_object,
                                                    HowToCode how_to_code,
                                                    WhereToPoint where_to_point,
                                                    int skip) {
  int space = SpaceOfObject(heap_object);

  if (skip == 0) {
    sink_->Put(kBackref + how_to_code + where_to_point + space, "BackRefSer");
  } else {
    sink_->Put(kBackrefWithSkip + how_to_code + where_to_point + space,
               "BackRefSerWithSkip");
    sink_->PutInt(skip, "BackRefSkipDistance");
  }

  if (space == LO_SPACE) {
    int index = address_mapper_.MappedTo(heap_object);
    sink_->PutInt(index, "large object index");
  } else {
    int address = address_mapper_.MappedTo(heap_object);
    int offset = CurrentAllocationAddress(space) - address;
    // Shift out the bits that are always 0.
    offset >>= kObjectAlignmentBits;
    sink_->PutInt(offset, "offset");
  }
}


void StartupSerializer::SerializeObject(
    Object* o,
    HowToCode how_to_code,
    WhereToPoint where_to_point,
    int skip) {
  CHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);
  DCHECK(!heap_object->IsJSFunction());

  int root_index;
  if ((root_index = RootIndex(heap_object, how_to_code)) != kInvalidRootIndex) {
    PutRoot(root_index, heap_object, how_to_code, where_to_point, skip);
    return;
  }

  if (address_mapper_.IsMapped(heap_object)) {
    SerializeReferenceToPreviousObject(heap_object, how_to_code, where_to_point,
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
    DCHECK(Map::cast(heap_object)->code_cache() ==
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
  DCHECK(!startup_serializer_->address_mapper()->IsMapped(heap_object));
  // All the internalized strings that the partial snapshot needs should be
  // either in the root table or in the partial snapshot cache.
  DCHECK(!heap_object->IsInternalizedString());

  if (address_mapper_.IsMapped(heap_object)) {
    SerializeReferenceToPreviousObject(heap_object, how_to_code, where_to_point,
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

  if (serializer_->code_address_map_) {
    const char* code_name =
        serializer_->code_address_map_->Lookup(object_->address());
    LOG(serializer_->isolate_,
        CodeNameEvent(object_->address(), sink_->Position(), code_name));
    LOG(serializer_->isolate_,
        SnapshotPositionEvent(object_->address(), sink_->Position()));
  }

  // Mark this object as already serialized.
  if (space == LO_SPACE) {
    if (object_->IsCode()) {
      sink_->PutInt(EXECUTABLE, "executable large object");
    } else {
      sink_->PutInt(NOT_EXECUTABLE, "not executable large object");
    }
    int index = serializer_->AllocateLargeObject(size);
    serializer_->address_mapper()->AddMapping(object_, index);
  } else {
    int offset = serializer_->Allocate(space, size);
    serializer_->address_mapper()->AddMapping(object_, offset);
  }

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
        DCHECK(!serializer_->isolate()->heap()->InNewSpace(current_contents));
        int repeat_count = 1;
        while (&current[repeat_count] < end - 1 &&
               current[repeat_count] == current_contents) {
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
  // Out-of-line constant pool entries will be visited by the ConstantPoolArray.
  if (FLAG_enable_ool_constant_pool && rinfo->IsInConstantPool()) return;

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
  // Out-of-line constant pool entries will be visited by the ConstantPoolArray.
  if (FLAG_enable_ool_constant_pool && rinfo->IsInConstantPool()) return;

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
  // Out-of-line constant pool entries will be visited by the ConstantPoolArray.
  if (FLAG_enable_ool_constant_pool && rinfo->IsInConstantPool()) return;

  int skip = OutputRawData(rinfo->pc(), kCanReturnSkipInsteadOfSkipping);
  Cell* object = Cell::cast(rinfo->target_cell());
  serializer_->SerializeObject(object, kPlain, kInnerPointer, skip);
  bytes_processed_so_far_ += kPointerSize;
}


void Serializer::ObjectSerializer::VisitExternalOneByteString(
    v8::String::ExternalOneByteStringResource** resource_pointer) {
  Address references_start = reinterpret_cast<Address>(resource_pointer);
  OutputRawData(references_start);
  for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
    Object* source =
        serializer_->isolate()->heap()->natives_source_cache()->get(i);
    if (!source->IsUndefined()) {
      ExternalOneByteString* string = ExternalOneByteString::cast(source);
      typedef v8::String::ExternalOneByteStringResource Resource;
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
  MemCopy(copy, code->address(), code->Size());
  return Code::cast(HeapObject::FromAddress(copy));
}


static void WipeOutRelocations(Code* code) {
  int mode_mask =
      RelocInfo::kCodeTargetMask |
      RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
      RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
  for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
    if (!(FLAG_enable_ool_constant_pool && it.rinfo()->IsInConstantPool())) {
      it.rinfo()->WipeOut();
    }
  }
}


int Serializer::ObjectSerializer::OutputRawData(
    Address up_to, Serializer::ObjectSerializer::ReturnSkip return_skip) {
  Address object_start = object_->address();
  int base = bytes_processed_so_far_;
  int up_to_offset = static_cast<int>(up_to - object_start);
  int to_skip = up_to_offset - bytes_processed_so_far_;
  int bytes_to_output = to_skip;
  bytes_processed_so_far_ += to_skip;
  // This assert will fail if the reloc info gives us the target_address_address
  // locations in a non-ascending order.  Luckily that doesn't happen.
  DCHECK(to_skip >= 0);
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
      DCHECK(i < kNumberOfSpaces);
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


int Serializer::AllocateLargeObject(int size) {
  fullness_[LO_SPACE] += size;
  return seen_large_objects_index_++;
}


int Serializer::Allocate(int space, int size) {
  CHECK(space >= 0 && space < kNumberOfPreallocatedSpaces);
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


void Serializer::InitializeCodeAddressMap() {
  isolate_->InitializeLoggingAndCounters();
  code_address_map_ = new CodeAddressMap(isolate_);
}


ScriptData* CodeSerializer::Serialize(Isolate* isolate,
                                      Handle<SharedFunctionInfo> info,
                                      Handle<String> source) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  // Serialize code object.
  List<byte> payload;
  ListSnapshotSink list_sink(&payload);
  DebugSnapshotSink debug_sink(&list_sink);
  SnapshotByteSink* sink = FLAG_trace_code_serializer
                               ? static_cast<SnapshotByteSink*>(&debug_sink)
                               : static_cast<SnapshotByteSink*>(&list_sink);
  CodeSerializer cs(isolate, sink, *source, info->code());
  DisallowHeapAllocation no_gc;
  Object** location = Handle<Object>::cast(info).location();
  cs.VisitPointer(location);
  cs.Pad();

  SerializedCodeData data(&payload, &cs);
  ScriptData* script_data = data.GetScriptData();

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = script_data->length();
    PrintF("[Serializing to %d bytes took %0.3f ms]\n", length, ms);
  }

  return script_data;
}


void CodeSerializer::SerializeObject(Object* o, HowToCode how_to_code,
                                     WhereToPoint where_to_point, int skip) {
  HeapObject* heap_object = HeapObject::cast(o);

  int root_index;
  if ((root_index = RootIndex(heap_object, how_to_code)) != kInvalidRootIndex) {
    PutRoot(root_index, heap_object, how_to_code, where_to_point, skip);
    return;
  }

  if (address_mapper_.IsMapped(heap_object)) {
    SerializeReferenceToPreviousObject(heap_object, how_to_code, where_to_point,
                                       skip);
    return;
  }

  if (skip != 0) {
    sink_->Put(kSkip, "SkipFromSerializeObject");
    sink_->PutInt(skip, "SkipDistanceFromSerializeObject");
  }

  if (heap_object->IsCode()) {
    Code* code_object = Code::cast(heap_object);
    switch (code_object->kind()) {
      case Code::OPTIMIZED_FUNCTION:  // No optimized code compiled yet.
      case Code::HANDLER:             // No handlers patched in yet.
      case Code::REGEXP:              // No regexp literals initialized yet.
      case Code::NUMBER_OF_KINDS:     // Pseudo enum value.
        CHECK(false);
      case Code::BUILTIN:
        SerializeBuiltin(code_object, how_to_code, where_to_point);
        return;
      case Code::STUB:
        SerializeCodeStub(code_object, how_to_code, where_to_point);
        return;
#define IC_KIND_CASE(KIND) case Code::KIND:
        IC_KIND_LIST(IC_KIND_CASE)
#undef IC_KIND_CASE
        SerializeHeapObject(code_object, how_to_code, where_to_point);
        return;
      // TODO(yangguo): add special handling to canonicalize ICs.
      case Code::FUNCTION:
        // Only serialize the code for the toplevel function. Replace code
        // of included function literals by the lazy compile builtin.
        // This is safe, as checked in Compiler::BuildFunctionInfo.
        if (code_object != main_code_) {
          Code* lazy = *isolate()->builtins()->CompileLazy();
          SerializeBuiltin(lazy, how_to_code, where_to_point);
        } else {
          SerializeHeapObject(code_object, how_to_code, where_to_point);
        }
        return;
    }
  }

  if (heap_object == source_) {
    SerializeSourceObject(how_to_code, where_to_point);
    return;
  }

  // Past this point we should not see any (context-specific) maps anymore.
  CHECK(!heap_object->IsMap());
  // There should be no references to the global object embedded.
  CHECK(!heap_object->IsJSGlobalProxy() && !heap_object->IsGlobalObject());
  // There should be no hash table embedded. They would require rehashing.
  CHECK(!heap_object->IsHashTable());

  SerializeHeapObject(heap_object, how_to_code, where_to_point);
}


void CodeSerializer::SerializeHeapObject(HeapObject* heap_object,
                                         HowToCode how_to_code,
                                         WhereToPoint where_to_point) {
  if (heap_object->IsScript()) {
    // The wrapper cache uses a Foreign object to point to a global handle.
    // However, the object visitor expects foreign objects to point to external
    // references.  Clear the cache to avoid this issue.
    Script::cast(heap_object)->ClearWrapperCache();
  }

  if (FLAG_trace_code_serializer) {
    PrintF("Encoding heap object: ");
    heap_object->ShortPrint();
    PrintF("\n");
  }

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, heap_object, sink_, how_to_code,
                              where_to_point);
  serializer.Serialize();
}


void CodeSerializer::SerializeBuiltin(Code* builtin, HowToCode how_to_code,
                                      WhereToPoint where_to_point) {
  DCHECK((how_to_code == kPlain && where_to_point == kStartOfObject) ||
         (how_to_code == kPlain && where_to_point == kInnerPointer) ||
         (how_to_code == kFromCode && where_to_point == kInnerPointer));
  int builtin_index = builtin->builtin_index();
  DCHECK_LT(builtin_index, Builtins::builtin_count);
  DCHECK_LE(0, builtin_index);

  if (FLAG_trace_code_serializer) {
    PrintF("Encoding builtin: %s\n",
           isolate()->builtins()->name(builtin_index));
  }

  sink_->Put(kBuiltin + how_to_code + where_to_point, "Builtin");
  sink_->PutInt(builtin_index, "builtin_index");
}


void CodeSerializer::SerializeCodeStub(Code* stub, HowToCode how_to_code,
                                       WhereToPoint where_to_point) {
  DCHECK((how_to_code == kPlain && where_to_point == kStartOfObject) ||
         (how_to_code == kPlain && where_to_point == kInnerPointer) ||
         (how_to_code == kFromCode && where_to_point == kInnerPointer));
  uint32_t stub_key = stub->stub_key();
  DCHECK(CodeStub::MajorKeyFromKey(stub_key) != CodeStub::NoCache);

  int index = AddCodeStubKey(stub_key) + kCodeStubsBaseIndex;

  if (FLAG_trace_code_serializer) {
    PrintF("Encoding code stub %s as %d\n",
           CodeStub::MajorName(CodeStub::MajorKeyFromKey(stub_key), false),
           index);
  }

  sink_->Put(kAttachedReference + how_to_code + where_to_point, "CodeStub");
  sink_->PutInt(index, "CodeStub key");
}


int CodeSerializer::AddCodeStubKey(uint32_t stub_key) {
  // TODO(yangguo) Maybe we need a hash table for a faster lookup than O(n^2).
  int index = 0;
  while (index < stub_keys_.length()) {
    if (stub_keys_[index] == stub_key) return index;
    index++;
  }
  stub_keys_.Add(stub_key);
  return index;
}


void CodeSerializer::SerializeSourceObject(HowToCode how_to_code,
                                           WhereToPoint where_to_point) {
  if (FLAG_trace_code_serializer) PrintF("Encoding source object\n");

  DCHECK(how_to_code == kPlain && where_to_point == kStartOfObject);
  sink_->Put(kAttachedReference + how_to_code + where_to_point, "Source");
  sink_->PutInt(kSourceObjectIndex, "kSourceObjectIndex");
}


Handle<SharedFunctionInfo> CodeSerializer::Deserialize(Isolate* isolate,
                                                       ScriptData* data,
                                                       Handle<String> source) {
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  Object* root;

  {
    HandleScope scope(isolate);

    SerializedCodeData scd(data, *source);
    SnapshotByteSource payload(scd.Payload(), scd.PayloadLength());
    Deserializer deserializer(&payload);
    STATIC_ASSERT(NEW_SPACE == 0);
    for (int i = NEW_SPACE; i < kNumberOfSpaces; i++) {
      deserializer.set_reservation(i, scd.GetReservation(i));
    }

    // Prepare and register list of attached objects.
    Vector<const uint32_t> code_stub_keys = scd.CodeStubKeys();
    Vector<Handle<Object> > attached_objects = Vector<Handle<Object> >::New(
        code_stub_keys.length() + kCodeStubsBaseIndex);
    attached_objects[kSourceObjectIndex] = source;
    for (int i = 0; i < code_stub_keys.length(); i++) {
      attached_objects[i + kCodeStubsBaseIndex] =
          CodeStub::GetCode(isolate, code_stub_keys[i]).ToHandleChecked();
    }
    deserializer.SetAttachedObjects(&attached_objects);

    // Deserialize.
    deserializer.DeserializePartial(isolate, &root);
    deserializer.FlushICacheForNewCodeObjects();
  }

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int length = data->length();
    PrintF("[Deserializing from %d bytes took %0.3f ms]\n", length, ms);
  }
  return Handle<SharedFunctionInfo>(SharedFunctionInfo::cast(root), isolate);
}


SerializedCodeData::SerializedCodeData(List<byte>* payload, CodeSerializer* cs)
    : owns_script_data_(true) {
  DisallowHeapAllocation no_gc;
  List<uint32_t>* stub_keys = cs->stub_keys();

  // Calculate sizes.
  int num_stub_keys = stub_keys->length();
  int stub_keys_size = stub_keys->length() * kInt32Size;
  int data_length = kHeaderSize + stub_keys_size + payload->length();

  // Allocate backing store and create result data.
  byte* data = NewArray<byte>(data_length);
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(data), kPointerAlignment));
  script_data_ = new ScriptData(data, data_length);
  script_data_->AcquireDataOwnership();

  // Set header values.
  SetHeaderValue(kCheckSumOffset, CheckSum(cs->source()));
  SetHeaderValue(kNumCodeStubKeysOffset, num_stub_keys);
  SetHeaderValue(kPayloadLengthOffset, payload->length());
  STATIC_ASSERT(NEW_SPACE == 0);
  for (int i = 0; i < SerializerDeserializer::kNumberOfSpaces; i++) {
    SetHeaderValue(kReservationsOffset + i, cs->CurrentAllocationAddress(i));
  }

  // Copy code stub keys.
  CopyBytes(data + kHeaderSize, reinterpret_cast<byte*>(stub_keys->begin()),
            stub_keys_size);

  // Copy serialized data.
  CopyBytes(data + kHeaderSize + stub_keys_size, payload->begin(),
            static_cast<size_t>(payload->length()));
}


bool SerializedCodeData::IsSane(String* source) {
  return GetHeaderValue(kCheckSumOffset) == CheckSum(source) &&
         PayloadLength() >= SharedFunctionInfo::kSize;
}


int SerializedCodeData::CheckSum(String* string) {
  int checksum = Version::Hash();
#ifdef DEBUG
  uint32_t seed = static_cast<uint32_t>(checksum);
  checksum = static_cast<int>(IteratingStringHasher::Hash(string, seed));
#endif  // DEBUG
  return checksum;
}
} }  // namespace v8::internal

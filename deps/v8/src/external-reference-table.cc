// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/external-reference-table.h"

#include "src/accessors.h"
#include "src/assembler.h"
#include "src/counters.h"
#include "src/deoptimizer.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

ExternalReferenceTable* ExternalReferenceTable::instance(Isolate* isolate) {
  ExternalReferenceTable* external_reference_table =
      isolate->external_reference_table();
  if (external_reference_table == NULL) {
    external_reference_table = new ExternalReferenceTable(isolate);
    isolate->set_external_reference_table(external_reference_table);
  }
  return external_reference_table;
}

ExternalReferenceTable::ExternalReferenceTable(Isolate* isolate) {
  // Miscellaneous
  Add(ExternalReference::roots_array_start(isolate).address(),
      "Heap::roots_array_start()");
  Add(ExternalReference::address_of_stack_limit(isolate).address(),
      "StackGuard::address_of_jslimit()");
  Add(ExternalReference::address_of_real_stack_limit(isolate).address(),
      "StackGuard::address_of_real_jslimit()");
  Add(ExternalReference::new_space_allocation_limit_address(isolate).address(),
      "Heap::NewSpaceAllocationLimitAddress()");
  Add(ExternalReference::new_space_allocation_top_address(isolate).address(),
      "Heap::NewSpaceAllocationTopAddress()");
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
  Add(ExternalReference::interpreter_dispatch_table_address(isolate).address(),
      "Interpreter::dispatch_table_address");
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
  Add(ExternalReference::address_of_the_hole_nan().address(), "the_hole_nan");
  Add(ExternalReference::get_date_field_function(isolate).address(),
      "JSDate::GetField");
  Add(ExternalReference::date_cache_stamp(isolate).address(),
      "date_cache_stamp");
  Add(ExternalReference::address_of_pending_message_obj(isolate).address(),
      "address_of_pending_message_obj");
  Add(ExternalReference::get_make_code_young_function(isolate).address(),
      "Code::MakeCodeYoung");
  Add(ExternalReference::cpu_features().address(), "cpu_features");
  Add(ExternalReference::old_space_allocation_top_address(isolate).address(),
      "Heap::OldSpaceAllocationTopAddress");
  Add(ExternalReference::old_space_allocation_limit_address(isolate).address(),
      "Heap::OldSpaceAllocationLimitAddress");
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
  Add(ExternalReference::wasm_f32_trunc(isolate).address(),
      "wasm::f32_trunc_wrapper");
  Add(ExternalReference::wasm_f32_floor(isolate).address(),
      "wasm::f32_floor_wrapper");
  Add(ExternalReference::wasm_f32_ceil(isolate).address(),
      "wasm::f32_ceil_wrapper");
  Add(ExternalReference::wasm_f32_nearest_int(isolate).address(),
      "wasm::f32_nearest_int_wrapper");
  Add(ExternalReference::wasm_f64_trunc(isolate).address(),
      "wasm::f64_trunc_wrapper");
  Add(ExternalReference::wasm_f64_floor(isolate).address(),
      "wasm::f64_floor_wrapper");
  Add(ExternalReference::wasm_f64_ceil(isolate).address(),
      "wasm::f64_ceil_wrapper");
  Add(ExternalReference::wasm_f64_nearest_int(isolate).address(),
      "wasm::f64_nearest_int_wrapper");
  Add(ExternalReference::wasm_int64_to_float32(isolate).address(),
      "wasm::int64_to_float32_wrapper");
  Add(ExternalReference::wasm_uint64_to_float32(isolate).address(),
      "wasm::uint64_to_float32_wrapper");
  Add(ExternalReference::wasm_int64_to_float64(isolate).address(),
      "wasm::int64_to_float64_wrapper");
  Add(ExternalReference::wasm_uint64_to_float64(isolate).address(),
      "wasm::uint64_to_float64_wrapper");
  Add(ExternalReference::wasm_float32_to_int64(isolate).address(),
      "wasm::float32_to_int64_wrapper");
  Add(ExternalReference::wasm_float32_to_uint64(isolate).address(),
      "wasm::float32_to_uint64_wrapper");
  Add(ExternalReference::wasm_float64_to_int64(isolate).address(),
      "wasm::float64_to_int64_wrapper");
  Add(ExternalReference::wasm_float64_to_uint64(isolate).address(),
      "wasm::float64_to_uint64_wrapper");
  Add(ExternalReference::wasm_int64_div(isolate).address(), "wasm::int64_div");
  Add(ExternalReference::wasm_int64_mod(isolate).address(), "wasm::int64_mod");
  Add(ExternalReference::wasm_uint64_div(isolate).address(),
      "wasm::uint64_div");
  Add(ExternalReference::wasm_uint64_mod(isolate).address(),
      "wasm::uint64_mod");
  Add(ExternalReference::f64_acos_wrapper_function(isolate).address(),
      "f64_acos_wrapper");
  Add(ExternalReference::f64_asin_wrapper_function(isolate).address(),
      "f64_asin_wrapper");
  Add(ExternalReference::f64_atan_wrapper_function(isolate).address(),
      "f64_atan_wrapper");
  Add(ExternalReference::f64_cos_wrapper_function(isolate).address(),
      "f64_cos_wrapper");
  Add(ExternalReference::f64_sin_wrapper_function(isolate).address(),
      "f64_sin_wrapper");
  Add(ExternalReference::f64_tan_wrapper_function(isolate).address(),
      "f64_tan_wrapper");
  Add(ExternalReference::f64_exp_wrapper_function(isolate).address(),
      "f64_exp_wrapper");
  Add(ExternalReference::f64_log_wrapper_function(isolate).address(),
      "f64_log_wrapper");
  Add(ExternalReference::f64_pow_wrapper_function(isolate).address(),
      "f64_pow_wrapper");
  Add(ExternalReference::f64_atan2_wrapper_function(isolate).address(),
      "f64_atan2_wrapper");
  Add(ExternalReference::f64_mod_wrapper_function(isolate).address(),
      "f64_mod_wrapper");
  Add(ExternalReference::log_enter_external_function(isolate).address(),
      "Logger::EnterExternal");
  Add(ExternalReference::log_leave_external_function(isolate).address(),
      "Logger::LeaveExternal");
  Add(ExternalReference::address_of_minus_one_half().address(),
      "double_constants.minus_one_half");
  Add(ExternalReference::stress_deopt_count(isolate).address(),
      "Isolate::stress_deopt_count_address()");
  Add(ExternalReference::virtual_handler_register(isolate).address(),
      "Isolate::virtual_handler_register()");
  Add(ExternalReference::virtual_slot_register(isolate).address(),
      "Isolate::virtual_slot_register()");
  Add(ExternalReference::runtime_function_table_address(isolate).address(),
      "Runtime::runtime_function_table_address()");
  Add(ExternalReference::is_tail_call_elimination_enabled_address(isolate)
          .address(),
      "Isolate::is_tail_call_elimination_enabled_address()");

  // Debug addresses
  Add(ExternalReference::debug_after_break_target_address(isolate).address(),
      "Debug::after_break_target_address()");
  Add(ExternalReference::debug_is_active_address(isolate).address(),
      "Debug::is_active_address()");
  Add(ExternalReference::debug_step_in_enabled_address(isolate).address(),
      "Debug::step_in_enabled_address()");

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
    uint16_t id;
    const char* name;
  };

  static const RefTableEntry c_builtins[] = {
#define DEF_ENTRY_C(name, ignored) {Builtins::c_##name, "Builtins::" #name},
      BUILTIN_LIST_C(DEF_ENTRY_C)
#undef DEF_ENTRY_C
  };

  for (unsigned i = 0; i < arraysize(c_builtins); ++i) {
    ExternalReference ref(static_cast<Builtins::CFunctionId>(c_builtins[i].id),
                          isolate);
    Add(ref.address(), c_builtins[i].name);
  }

  static const RefTableEntry builtins[] = {
#define DEF_ENTRY_C(name, ignored) {Builtins::k##name, "Builtins::" #name},
#define DEF_ENTRY_A(name, i1, i2, i3) {Builtins::k##name, "Builtins::" #name},
      BUILTIN_LIST_C(DEF_ENTRY_C) BUILTIN_LIST_A(DEF_ENTRY_A)
          BUILTIN_LIST_DEBUG_A(DEF_ENTRY_A)
#undef DEF_ENTRY_C
#undef DEF_ENTRY_A
  };

  for (unsigned i = 0; i < arraysize(builtins); ++i) {
    ExternalReference ref(static_cast<Builtins::Name>(builtins[i].id), isolate);
    Add(ref.address(), builtins[i].name);
  }

  static const RefTableEntry runtime_functions[] = {
#define RUNTIME_ENTRY(name, i1, i2) {Runtime::k##name, "Runtime::" #name},
      FOR_EACH_INTRINSIC(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY
  };

  for (unsigned i = 0; i < arraysize(runtime_functions); ++i) {
    ExternalReference ref(
        static_cast<Runtime::FunctionId>(runtime_functions[i].id), isolate);
    Add(ref.address(), runtime_functions[i].name);
  }

  // Stat counters
  struct StatsRefTableEntry {
    StatsCounter* (Counters::*counter)();
    const char* name;
  };

  static const StatsRefTableEntry stats_ref_table[] = {
#define COUNTER_ENTRY(name, caption) {&Counters::name, "Counters::" #name},
      STATS_COUNTER_LIST_1(COUNTER_ENTRY) STATS_COUNTER_LIST_2(COUNTER_ENTRY)
#undef COUNTER_ENTRY
  };

  Counters* counters = isolate->counters();
  for (unsigned i = 0; i < arraysize(stats_ref_table); ++i) {
    // To make sure the indices are not dependent on whether counters are
    // enabled, use a dummy address as filler.
    Address address = NotAvailable();
    StatsCounter* counter = (counters->*(stats_ref_table[i].counter))();
    if (counter->Enabled()) {
      address = reinterpret_cast<Address>(counter->GetInternalPointer());
    }
    Add(address, stats_ref_table[i].name);
  }

  // Top addresses
  static const char* address_names[] = {
#define BUILD_NAME_LITERAL(Name, name) "Isolate::" #name "_address",
      FOR_EACH_ISOLATE_ADDRESS_NAME(BUILD_NAME_LITERAL) NULL
#undef BUILD_NAME_LITERAL
  };

  for (int i = 0; i < Isolate::kIsolateAddressCount; ++i) {
    Add(isolate->get_address_from_id(static_cast<Isolate::AddressId>(i)),
        address_names[i]);
  }

  // Accessors
  struct AccessorRefTable {
    Address address;
    const char* name;
  };

  static const AccessorRefTable accessors[] = {
#define ACCESSOR_INFO_DECLARATION(name) \
  {FUNCTION_ADDR(&Accessors::name##Getter), "Accessors::" #name "Getter"},
      ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION
#define ACCESSOR_SETTER_DECLARATION(name) \
  {FUNCTION_ADDR(&Accessors::name), "Accessors::" #name},
          ACCESSOR_SETTER_LIST(ACCESSOR_SETTER_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION
  };

  for (unsigned i = 0; i < arraysize(accessors); ++i) {
    Add(accessors[i].address, accessors[i].name);
  }

  StubCache* stub_cache = isolate->stub_cache();

  // Stub cache tables
  Add(stub_cache->key_reference(StubCache::kPrimary).address(),
      "StubCache::primary_->key");
  Add(stub_cache->value_reference(StubCache::kPrimary).address(),
      "StubCache::primary_->value");
  Add(stub_cache->map_reference(StubCache::kPrimary).address(),
      "StubCache::primary_->map");
  Add(stub_cache->key_reference(StubCache::kSecondary).address(),
      "StubCache::secondary_->key");
  Add(stub_cache->value_reference(StubCache::kSecondary).address(),
      "StubCache::secondary_->value");
  Add(stub_cache->map_reference(StubCache::kSecondary).address(),
      "StubCache::secondary_->map");

  // Runtime entries
  Add(ExternalReference::delete_handle_scope_extensions(isolate).address(),
      "HandleScope::DeleteExtensions");
  Add(ExternalReference::incremental_marking_record_write_function(isolate)
          .address(),
      "IncrementalMarking::RecordWrite");
  Add(ExternalReference::incremental_marking_record_write_code_entry_function(
          isolate)
          .address(),
      "IncrementalMarking::RecordWriteOfCodeEntryFromCode");
  Add(ExternalReference::store_buffer_overflow_function(isolate).address(),
      "StoreBuffer::StoreBufferOverflow");

  // Add a small set of deopt entry addresses to encoder without generating the
  // deopt table code, which isn't possible at deserialization time.
  HandleScope scope(isolate);
  for (int entry = 0; entry < kDeoptTableSerializeEntryCount; ++entry) {
    Address address = Deoptimizer::GetDeoptimizationEntry(
        isolate, entry, Deoptimizer::LAZY,
        Deoptimizer::CALCULATE_ENTRY_ADDRESS);
    Add(address, "lazy_deopt");
  }
}

}  // namespace internal
}  // namespace v8

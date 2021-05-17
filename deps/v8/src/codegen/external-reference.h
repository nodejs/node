// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_EXTERNAL_REFERENCE_H_
#define V8_CODEGEN_EXTERNAL_REFERENCE_H_

#include "src/common/globals.h"
#include "src/runtime/runtime.h"

namespace v8 {

class ApiFunction;

namespace internal {

class Isolate;
class Page;
class SCTableReference;
class StatsCounter;

//------------------------------------------------------------------------------
// External references

#define EXTERNAL_REFERENCE_LIST_WITH_ISOLATE(V)                                \
  V(isolate_address, "isolate")                                                \
  V(builtins_address, "builtins")                                              \
  V(handle_scope_implementer_address,                                          \
    "Isolate::handle_scope_implementer_address")                               \
  V(address_of_interpreter_entry_trampoline_instruction_start,                 \
    "Address of the InterpreterEntryTrampoline instruction start")             \
  V(interpreter_dispatch_counters, "Interpreter::dispatch_counters")           \
  V(interpreter_dispatch_table_address, "Interpreter::dispatch_table_address") \
  V(date_cache_stamp, "date_cache_stamp")                                      \
  V(stress_deopt_count, "Isolate::stress_deopt_count_address()")               \
  V(force_slow_path, "Isolate::force_slow_path_address()")                     \
  V(isolate_root, "Isolate::isolate_root()")                                   \
  V(allocation_sites_list_address, "Heap::allocation_sites_list_address()")    \
  V(address_of_jslimit, "StackGuard::address_of_jslimit()")                    \
  V(address_of_real_jslimit, "StackGuard::address_of_real_jslimit()")          \
  V(heap_is_marking_flag_address, "heap_is_marking_flag_address")              \
  V(new_space_allocation_top_address, "Heap::NewSpaceAllocationTopAddress()")  \
  V(new_space_allocation_limit_address,                                        \
    "Heap::NewSpaceAllocationLimitAddress()")                                  \
  V(old_space_allocation_top_address, "Heap::OldSpaceAllocationTopAddress")    \
  V(old_space_allocation_limit_address,                                        \
    "Heap::OldSpaceAllocationLimitAddress")                                    \
  V(handle_scope_level_address, "HandleScope::level")                          \
  V(handle_scope_next_address, "HandleScope::next")                            \
  V(handle_scope_limit_address, "HandleScope::limit")                          \
  V(scheduled_exception_address, "Isolate::scheduled_exception")               \
  V(address_of_pending_message_obj, "address_of_pending_message_obj")          \
  V(promise_hook_flags_address, "Isolate::promise_hook_flags_address()")       \
  V(promise_hook_address, "Isolate::promise_hook_address()")                   \
  V(async_event_delegate_address, "Isolate::async_event_delegate_address()")   \
  V(debug_execution_mode_address, "Isolate::debug_execution_mode_address()")   \
  V(debug_is_active_address, "Debug::is_active_address()")                     \
  V(debug_hook_on_function_call_address,                                       \
    "Debug::hook_on_function_call_address()")                                  \
  V(runtime_function_table_address,                                            \
    "Runtime::runtime_function_table_address()")                               \
  V(is_profiling_address, "Isolate::is_profiling")                             \
  V(debug_suspended_generator_address,                                         \
    "Debug::step_suspended_generator_address()")                               \
  V(debug_restart_fp_address, "Debug::restart_fp_address()")                   \
  V(fast_c_call_caller_fp_address,                                             \
    "IsolateData::fast_c_call_caller_fp_address")                              \
  V(fast_c_call_caller_pc_address,                                             \
    "IsolateData::fast_c_call_caller_pc_address")                              \
  V(fast_api_call_target_address, "IsolateData::fast_api_call_target_address") \
  V(stack_is_iterable_address, "IsolateData::stack_is_iterable_address")       \
  V(address_of_regexp_stack_limit_address,                                     \
    "RegExpStack::limit_address_address()")                                    \
  V(address_of_regexp_stack_memory_top_address,                                \
    "RegExpStack::memory_top_address_address()")                               \
  V(address_of_static_offsets_vector, "OffsetsVector::static_offsets_vector")  \
  V(thread_in_wasm_flag_address_address,                                       \
    "Isolate::thread_in_wasm_flag_address_address")                            \
  V(re_case_insensitive_compare_unicode,                                       \
    "NativeRegExpMacroAssembler::CaseInsensitiveCompareUnicode()")             \
  V(re_case_insensitive_compare_non_unicode,                                   \
    "NativeRegExpMacroAssembler::CaseInsensitiveCompareNonUnicode()")          \
  V(re_check_stack_guard_state,                                                \
    "RegExpMacroAssembler*::CheckStackGuardState()")                           \
  V(re_grow_stack, "NativeRegExpMacroAssembler::GrowStack()")                  \
  V(re_word_character_map, "NativeRegExpMacroAssembler::word_character_map")   \
  EXTERNAL_REFERENCE_LIST_WITH_ISOLATE_HEAP_SANDBOX(V)

#ifdef V8_HEAP_SANDBOX
#define EXTERNAL_REFERENCE_LIST_WITH_ISOLATE_HEAP_SANDBOX(V) \
  V(external_pointer_table_address,                          \
    "Isolate::external_pointer_table_address("               \
    ")")
#else
#define EXTERNAL_REFERENCE_LIST_WITH_ISOLATE_HEAP_SANDBOX(V)
#endif  // V8_HEAP_SANDBOX

#define EXTERNAL_REFERENCE_LIST(V)                                             \
  V(abort_with_reason, "abort_with_reason")                                    \
  V(address_of_builtin_subclassing_flag, "FLAG_builtin_subclassing")           \
  V(address_of_double_abs_constant, "double_absolute_constant")                \
  V(address_of_double_neg_constant, "double_negate_constant")                  \
  V(address_of_enable_experimental_regexp_engine,                              \
    "address_of_enable_experimental_regexp_engine")                            \
  V(address_of_float_abs_constant, "float_absolute_constant")                  \
  V(address_of_float_neg_constant, "float_negate_constant")                    \
  V(address_of_harmony_regexp_match_indices_flag,                              \
    "FLAG_harmony_regexp_match_indices")                                       \
  V(address_of_min_int, "LDoubleConstant::min_int")                            \
  V(address_of_mock_arraybuffer_allocator_flag,                                \
    "FLAG_mock_arraybuffer_allocator")                                         \
  V(address_of_one_half, "LDoubleConstant::one_half")                          \
  V(address_of_runtime_stats_flag, "TracingFlags::runtime_stats")              \
  V(address_of_the_hole_nan, "the_hole_nan")                                   \
  V(address_of_uint32_bias, "uint32_bias")                                     \
  V(address_of_wasm_i8x16_swizzle_mask, "wasm_i8x16_swizzle_mask")             \
  V(address_of_wasm_i8x16_popcnt_mask, "wasm_i8x16_popcnt_mask")               \
  V(address_of_wasm_i8x16_splat_0x01, "wasm_i8x16_splat_0x01")                 \
  V(address_of_wasm_i8x16_splat_0x0f, "wasm_i8x16_splat_0x0f")                 \
  V(address_of_wasm_i8x16_splat_0x33, "wasm_i8x16_splat_0x33")                 \
  V(address_of_wasm_i8x16_splat_0x55, "wasm_i8x16_splat_0x55")                 \
  V(address_of_wasm_i16x8_splat_0x0001, "wasm_16x8_splat_0x0001")              \
  V(baseline_pc_for_bytecode_offset, "BaselinePCForBytecodeOffset")            \
  V(baseline_pc_for_next_executed_bytecode,                                    \
    "BaselinePCForNextExecutedBytecode")                                       \
  V(bytecode_size_table_address, "Bytecodes::bytecode_size_table_address")     \
  V(check_object_type, "check_object_type")                                    \
  V(compute_integer_hash, "ComputeSeededHash")                                 \
  V(compute_output_frames_function, "Deoptimizer::ComputeOutputFrames()")      \
  V(copy_fast_number_jsarray_elements_to_typed_array,                          \
    "copy_fast_number_jsarray_elements_to_typed_array")                        \
  V(copy_typed_array_elements_slice, "copy_typed_array_elements_slice")        \
  V(copy_typed_array_elements_to_typed_array,                                  \
    "copy_typed_array_elements_to_typed_array")                                \
  V(cpu_features, "cpu_features")                                              \
  V(delete_handle_scope_extensions, "HandleScope::DeleteExtensions")           \
  V(ephemeron_key_write_barrier_function,                                      \
    "Heap::EphemeronKeyWriteBarrierFromCode")                                  \
  V(f64_acos_wrapper_function, "f64_acos_wrapper")                             \
  V(f64_asin_wrapper_function, "f64_asin_wrapper")                             \
  V(f64_mod_wrapper_function, "f64_mod_wrapper")                               \
  V(get_date_field_function, "JSDate::GetField")                               \
  V(get_or_create_hash_raw, "get_or_create_hash_raw")                          \
  V(ieee754_acos_function, "base::ieee754::acos")                              \
  V(ieee754_acosh_function, "base::ieee754::acosh")                            \
  V(ieee754_asin_function, "base::ieee754::asin")                              \
  V(ieee754_asinh_function, "base::ieee754::asinh")                            \
  V(ieee754_atan_function, "base::ieee754::atan")                              \
  V(ieee754_atan2_function, "base::ieee754::atan2")                            \
  V(ieee754_atanh_function, "base::ieee754::atanh")                            \
  V(ieee754_cbrt_function, "base::ieee754::cbrt")                              \
  V(ieee754_cos_function, "base::ieee754::cos")                                \
  V(ieee754_cosh_function, "base::ieee754::cosh")                              \
  V(ieee754_exp_function, "base::ieee754::exp")                                \
  V(ieee754_expm1_function, "base::ieee754::expm1")                            \
  V(ieee754_log_function, "base::ieee754::log")                                \
  V(ieee754_log10_function, "base::ieee754::log10")                            \
  V(ieee754_log1p_function, "base::ieee754::log1p")                            \
  V(ieee754_log2_function, "base::ieee754::log2")                              \
  V(ieee754_pow_function, "base::ieee754::pow")                                \
  V(ieee754_sin_function, "base::ieee754::sin")                                \
  V(ieee754_sinh_function, "base::ieee754::sinh")                              \
  V(ieee754_tan_function, "base::ieee754::tan")                                \
  V(ieee754_tanh_function, "base::ieee754::tanh")                              \
  V(insert_remembered_set_function, "Heap::InsertIntoRememberedSetFromCode")   \
  V(invalidate_prototype_chains_function,                                      \
    "JSObject::InvalidatePrototypeChains()")                                   \
  V(invoke_accessor_getter_callback, "InvokeAccessorGetterCallback")           \
  V(invoke_function_callback, "InvokeFunctionCallback")                        \
  V(jsarray_array_join_concat_to_sequential_string,                            \
    "jsarray_array_join_concat_to_sequential_string")                          \
  V(jsreceiver_create_identity_hash, "jsreceiver_create_identity_hash")        \
  V(libc_memchr_function, "libc_memchr")                                       \
  V(libc_memcpy_function, "libc_memcpy")                                       \
  V(libc_memmove_function, "libc_memmove")                                     \
  V(libc_memset_function, "libc_memset")                                       \
  V(mod_two_doubles_operation, "mod_two_doubles")                              \
  V(mutable_big_int_absolute_add_and_canonicalize_function,                    \
    "MutableBigInt_AbsoluteAddAndCanonicalize")                                \
  V(mutable_big_int_absolute_compare_function,                                 \
    "MutableBigInt_AbsoluteCompare")                                           \
  V(mutable_big_int_absolute_sub_and_canonicalize_function,                    \
    "MutableBigInt_AbsoluteSubAndCanonicalize")                                \
  V(new_deoptimizer_function, "Deoptimizer::New()")                            \
  V(orderedhashmap_gethash_raw, "orderedhashmap_gethash_raw")                  \
  V(printf_function, "printf")                                                 \
  V(refill_math_random, "MathRandom::RefillCache")                             \
  V(search_string_raw_one_one, "search_string_raw_one_one")                    \
  V(search_string_raw_one_two, "search_string_raw_one_two")                    \
  V(search_string_raw_two_one, "search_string_raw_two_one")                    \
  V(search_string_raw_two_two, "search_string_raw_two_two")                    \
  V(string_write_to_flat_one_byte, "string_write_to_flat_one_byte")            \
  V(string_write_to_flat_two_byte, "string_write_to_flat_two_byte")            \
  V(external_one_byte_string_get_chars, "external_one_byte_string_get_chars")  \
  V(external_two_byte_string_get_chars, "external_two_byte_string_get_chars")  \
  V(smi_lexicographic_compare_function, "smi_lexicographic_compare_function")  \
  V(string_to_array_index_function, "String::ToArrayIndex")                    \
  V(try_string_to_index_or_lookup_existing,                                    \
    "try_string_to_index_or_lookup_existing")                                  \
  IF_WASM(V, wasm_call_trap_callback_for_testing,                              \
          "wasm::call_trap_callback_for_testing")                              \
  IF_WASM(V, wasm_f32_ceil, "wasm::f32_ceil_wrapper")                          \
  IF_WASM(V, wasm_f32_floor, "wasm::f32_floor_wrapper")                        \
  IF_WASM(V, wasm_f32_nearest_int, "wasm::f32_nearest_int_wrapper")            \
  IF_WASM(V, wasm_f32_trunc, "wasm::f32_trunc_wrapper")                        \
  IF_WASM(V, wasm_f64_ceil, "wasm::f64_ceil_wrapper")                          \
  IF_WASM(V, wasm_f64_floor, "wasm::f64_floor_wrapper")                        \
  IF_WASM(V, wasm_f64_nearest_int, "wasm::f64_nearest_int_wrapper")            \
  IF_WASM(V, wasm_f64_trunc, "wasm::f64_trunc_wrapper")                        \
  IF_WASM(V, wasm_float32_to_int64, "wasm::float32_to_int64_wrapper")          \
  IF_WASM(V, wasm_float32_to_uint64, "wasm::float32_to_uint64_wrapper")        \
  IF_WASM(V, wasm_float32_to_int64_sat, "wasm::float32_to_int64_sat_wrapper")  \
  IF_WASM(V, wasm_float32_to_uint64_sat,                                       \
          "wasm::float32_to_uint64_sat_wrapper")                               \
  IF_WASM(V, wasm_float64_pow, "wasm::float64_pow")                            \
  IF_WASM(V, wasm_float64_to_int64, "wasm::float64_to_int64_wrapper")          \
  IF_WASM(V, wasm_float64_to_uint64, "wasm::float64_to_uint64_wrapper")        \
  IF_WASM(V, wasm_float64_to_int64_sat, "wasm::float64_to_int64_sat_wrapper")  \
  IF_WASM(V, wasm_float64_to_uint64_sat,                                       \
          "wasm::float64_to_uint64_sat_wrapper")                               \
  IF_WASM(V, wasm_int64_div, "wasm::int64_div")                                \
  IF_WASM(V, wasm_int64_mod, "wasm::int64_mod")                                \
  IF_WASM(V, wasm_int64_to_float32, "wasm::int64_to_float32_wrapper")          \
  IF_WASM(V, wasm_int64_to_float64, "wasm::int64_to_float64_wrapper")          \
  IF_WASM(V, wasm_uint64_div, "wasm::uint64_div")                              \
  IF_WASM(V, wasm_uint64_mod, "wasm::uint64_mod")                              \
  IF_WASM(V, wasm_uint64_to_float32, "wasm::uint64_to_float32_wrapper")        \
  IF_WASM(V, wasm_uint64_to_float64, "wasm::uint64_to_float64_wrapper")        \
  IF_WASM(V, wasm_word32_ctz, "wasm::word32_ctz")                              \
  IF_WASM(V, wasm_word32_popcnt, "wasm::word32_popcnt")                        \
  IF_WASM(V, wasm_word32_rol, "wasm::word32_rol")                              \
  IF_WASM(V, wasm_word32_ror, "wasm::word32_ror")                              \
  IF_WASM(V, wasm_word64_rol, "wasm::word64_rol")                              \
  IF_WASM(V, wasm_word64_ror, "wasm::word64_ror")                              \
  IF_WASM(V, wasm_word64_ctz, "wasm::word64_ctz")                              \
  IF_WASM(V, wasm_word64_popcnt, "wasm::word64_popcnt")                        \
  IF_WASM(V, wasm_f64x2_ceil, "wasm::f64x2_ceil_wrapper")                      \
  IF_WASM(V, wasm_f64x2_floor, "wasm::f64x2_floor_wrapper")                    \
  IF_WASM(V, wasm_f64x2_trunc, "wasm::f64x2_trunc_wrapper")                    \
  IF_WASM(V, wasm_f64x2_nearest_int, "wasm::f64x2_nearest_int_wrapper")        \
  IF_WASM(V, wasm_f32x4_ceil, "wasm::f32x4_ceil_wrapper")                      \
  IF_WASM(V, wasm_f32x4_floor, "wasm::f32x4_floor_wrapper")                    \
  IF_WASM(V, wasm_f32x4_trunc, "wasm::f32x4_trunc_wrapper")                    \
  IF_WASM(V, wasm_f32x4_nearest_int, "wasm::f32x4_nearest_int_wrapper")        \
  IF_WASM(V, wasm_memory_init, "wasm::memory_init")                            \
  IF_WASM(V, wasm_memory_copy, "wasm::memory_copy")                            \
  IF_WASM(V, wasm_memory_fill, "wasm::memory_fill")                            \
  V(address_of_wasm_f64x2_convert_low_i32x4_u_int_mask,                        \
    "wasm_f64x2_convert_low_i32x4_u_int_mask")                                 \
  V(supports_wasm_simd_128_address, "wasm::supports_wasm_simd_128_address")    \
  V(address_of_wasm_double_2_power_52, "wasm_double_2_power_52")               \
  V(address_of_wasm_int32_max_as_double, "wasm_int32_max_as_double")           \
  V(address_of_wasm_uint32_max_as_double, "wasm_uint32_max_as_double")         \
  V(write_barrier_marking_from_code_function, "WriteBarrier::MarkingFromCode") \
  V(call_enqueue_microtask_function, "MicrotaskQueue::CallEnqueueMicrotask")   \
  V(call_enter_context_function, "call_enter_context_function")                \
  V(atomic_pair_load_function, "atomic_pair_load_function")                    \
  V(atomic_pair_store_function, "atomic_pair_store_function")                  \
  V(atomic_pair_add_function, "atomic_pair_add_function")                      \
  V(atomic_pair_sub_function, "atomic_pair_sub_function")                      \
  V(atomic_pair_and_function, "atomic_pair_and_function")                      \
  V(atomic_pair_or_function, "atomic_pair_or_function")                        \
  V(atomic_pair_xor_function, "atomic_pair_xor_function")                      \
  V(atomic_pair_exchange_function, "atomic_pair_exchange_function")            \
  V(atomic_pair_compare_exchange_function,                                     \
    "atomic_pair_compare_exchange_function")                                   \
  V(js_finalization_registry_remove_cell_from_unregister_token_map,            \
    "JSFinalizationRegistry::RemoveCellFromUnregisterTokenMap")                \
  V(re_match_for_call_from_js, "IrregexpInterpreter::MatchForCallFromJs")      \
  V(re_experimental_match_for_call_from_js,                                    \
    "ExperimentalRegExp::MatchForCallFromJs")                                  \
  EXTERNAL_REFERENCE_LIST_INTL(V)                                              \
  EXTERNAL_REFERENCE_LIST_HEAP_SANDBOX(V)
#ifdef V8_INTL_SUPPORT
#define EXTERNAL_REFERENCE_LIST_INTL(V)                               \
  V(intl_convert_one_byte_to_lower, "intl_convert_one_byte_to_lower") \
  V(intl_to_latin1_lower_table, "intl_to_latin1_lower_table")
#else
#define EXTERNAL_REFERENCE_LIST_INTL(V)
#endif  // V8_INTL_SUPPORT

#ifdef V8_HEAP_SANDBOX
#define EXTERNAL_REFERENCE_LIST_HEAP_SANDBOX(V) \
  V(external_pointer_table_grow_table_function, \
    "ExternalPointerTable::GrowTable")
#else
#define EXTERNAL_REFERENCE_LIST_HEAP_SANDBOX(V)
#endif  // V8_HEAP_SANDBOX

// An ExternalReference represents a C++ address used in the generated
// code. All references to C++ functions and variables must be encapsulated
// in an ExternalReference instance. This is done in order to track the
// origin of all external references in the code so that they can be bound
// to the correct addresses when deserializing a heap.
class ExternalReference {
 public:
  // Used in the simulator to support different native api calls.
  enum Type {
    // Builtin call.
    // Address f(v8::internal::Arguments).
    BUILTIN_CALL,  // default

    // Builtin call returning object pair.
    // ObjectPair f(v8::internal::Arguments).
    BUILTIN_CALL_PAIR,

    // Builtin that takes float arguments and returns an int.
    // int f(double, double).
    BUILTIN_COMPARE_CALL,

    // Builtin call that returns floating point.
    // double f(double, double).
    BUILTIN_FP_FP_CALL,

    // Builtin call that returns floating point.
    // double f(double).
    BUILTIN_FP_CALL,

    // Builtin call that returns floating point.
    // double f(double, int).
    BUILTIN_FP_INT_CALL,

    // Direct call to API function callback.
    // void f(v8::FunctionCallbackInfo&)
    DIRECT_API_CALL,

    // Call to function callback via InvokeFunctionCallback.
    // void f(v8::FunctionCallbackInfo&, v8::FunctionCallback)
    PROFILING_API_CALL,

    // Direct call to accessor getter callback.
    // void f(Local<Name> property, PropertyCallbackInfo& info)
    DIRECT_GETTER_CALL,

    // Call to accessor getter callback via InvokeAccessorGetterCallback.
    // void f(Local<Name> property, PropertyCallbackInfo& info,
    //     AccessorNameGetterCallback callback)
    PROFILING_GETTER_CALL
  };

  static constexpr int kExternalReferenceCount =
#define COUNT_EXTERNAL_REFERENCE(name, desc) +1
      EXTERNAL_REFERENCE_LIST(COUNT_EXTERNAL_REFERENCE)
          EXTERNAL_REFERENCE_LIST_WITH_ISOLATE(COUNT_EXTERNAL_REFERENCE);
#undef COUNT_EXTERNAL_REFERENCE

  ExternalReference() : address_(kNullAddress) {}
  static ExternalReference Create(const SCTableReference& table_ref);
  static ExternalReference Create(StatsCounter* counter);
  static V8_EXPORT_PRIVATE ExternalReference Create(ApiFunction* ptr,
                                                    Type type);
  static ExternalReference Create(const Runtime::Function* f);
  static ExternalReference Create(IsolateAddressId id, Isolate* isolate);
  static ExternalReference Create(Runtime::FunctionId id);
  static V8_EXPORT_PRIVATE ExternalReference Create(Address address);

  template <typename SubjectChar, typename PatternChar>
  static ExternalReference search_string_raw();

  V8_EXPORT_PRIVATE static ExternalReference FromRawAddress(Address address);

#define DECL_EXTERNAL_REFERENCE(name, desc) \
  V8_EXPORT_PRIVATE static ExternalReference name();
  EXTERNAL_REFERENCE_LIST(DECL_EXTERNAL_REFERENCE)
#undef DECL_EXTERNAL_REFERENCE

#define DECL_EXTERNAL_REFERENCE(name, desc) \
  static V8_EXPORT_PRIVATE ExternalReference name(Isolate* isolate);
  EXTERNAL_REFERENCE_LIST_WITH_ISOLATE(DECL_EXTERNAL_REFERENCE)
#undef DECL_EXTERNAL_REFERENCE

  V8_EXPORT_PRIVATE V8_NOINLINE static ExternalReference
  runtime_function_table_address_for_unittests(Isolate* isolate);

  static V8_EXPORT_PRIVATE ExternalReference
  address_of_load_from_stack_count(const char* function_name);
  static V8_EXPORT_PRIVATE ExternalReference
  address_of_store_to_stack_count(const char* function_name);

  Address address() const { return address_; }

 private:
  explicit ExternalReference(Address address) : address_(address) {}

  explicit ExternalReference(void* address)
      : address_(reinterpret_cast<Address>(address)) {}

  static Address Redirect(Address address_arg,
                          Type type = ExternalReference::BUILTIN_CALL);

  Address address_;
};
ASSERT_TRIVIALLY_COPYABLE(ExternalReference);

V8_EXPORT_PRIVATE bool operator==(ExternalReference, ExternalReference);
bool operator!=(ExternalReference, ExternalReference);

size_t hash_value(ExternalReference);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, ExternalReference);

void abort_with_reason(int reason);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_EXTERNAL_REFERENCE_H_

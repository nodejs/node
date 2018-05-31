// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTERNAL_REFERENCE_H_
#define V8_EXTERNAL_REFERENCE_H_

#include "src/globals.h"
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

#define EXTERNAL_REFERENCE_LIST(V)                                             \
  V(address_of_double_abs_constant, "double_absolute_constant")                \
  V(address_of_double_neg_constant, "double_negate_constant")                  \
  V(address_of_float_abs_constant, "float_absolute_constant")                  \
  V(address_of_float_neg_constant, "float_negate_constant")                    \
  V(address_of_min_int, "LDoubleConstant::min_int")                            \
  V(address_of_minus_one_half, "double_constants.minus_one_half")              \
  V(address_of_negative_infinity, "LDoubleConstant::negative_infinity")        \
  V(address_of_one_half, "LDoubleConstant::one_half")                          \
  V(address_of_pending_message_obj, "address_of_pending_message_obj")          \
  V(address_of_real_stack_limit, "StackGuard::address_of_real_jslimit()")      \
  V(address_of_stack_limit, "StackGuard::address_of_jslimit()")                \
  V(address_of_the_hole_nan, "the_hole_nan")                                   \
  V(address_of_uint32_bias, "uint32_bias")                                     \
  V(allocation_sites_list_address, "Heap::allocation_sites_list_address()")    \
  V(builtins_address, "builtins")                                              \
  V(bytecode_size_table_address, "Bytecodes::bytecode_size_table_address")     \
  V(check_object_type, "check_object_type")                                    \
  V(compute_output_frames_function, "Deoptimizer::ComputeOutputFrames()")      \
  V(copy_fast_number_jsarray_elements_to_typed_array,                          \
    "copy_fast_number_jsarray_elements_to_typed_array")                        \
  V(copy_typed_array_elements_slice, "copy_typed_array_elements_slice")        \
  V(copy_typed_array_elements_to_typed_array,                                  \
    "copy_typed_array_elements_to_typed_array")                                \
  V(cpu_features, "cpu_features")                                              \
  V(date_cache_stamp, "date_cache_stamp")                                      \
  V(debug_execution_mode_address, "Isolate::debug_execution_mode()")           \
  V(debug_hook_on_function_call_address,                                       \
    "Debug::hook_on_function_call_address()")                                  \
  V(debug_is_active_address, "Debug::is_active_address()")                     \
  V(debug_last_step_action_address, "Debug::step_in_enabled_address()")        \
  V(debug_restart_fp_address, "Debug::restart_fp_address()")                   \
  V(debug_suspended_generator_address,                                         \
    "Debug::step_suspended_generator_address()")                               \
  V(delete_handle_scope_extensions, "HandleScope::DeleteExtensions")           \
  V(f64_acos_wrapper_function, "f64_acos_wrapper")                             \
  V(f64_asin_wrapper_function, "f64_asin_wrapper")                             \
  V(f64_mod_wrapper_function, "f64_mod_wrapper")                               \
  V(fixed_typed_array_base_data_offset, "fixed_typed_array_base_data_offset")  \
  V(force_slow_path, "Isolate::force_slow_path_address()")                     \
  V(get_date_field_function, "JSDate::GetField")                               \
  V(get_or_create_hash_raw, "get_or_create_hash_raw")                          \
  V(handle_scope_implementer_address,                                          \
    "Isolate::handle_scope_implementer_address")                               \
  V(handle_scope_level_address, "HandleScope::level")                          \
  V(handle_scope_limit_address, "HandleScope::limit")                          \
  V(handle_scope_next_address, "HandleScope::next")                            \
  V(heap_is_marking_flag_address, "heap_is_marking_flag_address")              \
  V(ieee754_acos_function, "base::ieee754::acos")                              \
  V(ieee754_acosh_function, "base::ieee754::acosh")                            \
  V(ieee754_asin_function, "base::ieee754::asin")                              \
  V(ieee754_asinh_function, "base::ieee754::asinh")                            \
  V(ieee754_atan2_function, "base::ieee754::atan2")                            \
  V(ieee754_atan_function, "base::ieee754::atan")                              \
  V(ieee754_atanh_function, "base::ieee754::atanh")                            \
  V(ieee754_cbrt_function, "base::ieee754::cbrt")                              \
  V(ieee754_cos_function, "base::ieee754::cos")                                \
  V(ieee754_cosh_function, "base::ieee754::cosh")                              \
  V(ieee754_exp_function, "base::ieee754::exp")                                \
  V(ieee754_expm1_function, "base::ieee754::expm1")                            \
  V(ieee754_log10_function, "base::ieee754::log10")                            \
  V(ieee754_log1p_function, "base::ieee754::log1p")                            \
  V(ieee754_log2_function, "base::ieee754::log2")                              \
  V(ieee754_log_function, "base::ieee754::log")                                \
  V(ieee754_sin_function, "base::ieee754::sin")                                \
  V(ieee754_sinh_function, "base::ieee754::sinh")                              \
  V(ieee754_tan_function, "base::ieee754::tan")                                \
  V(ieee754_tanh_function, "base::ieee754::tanh")                              \
  V(incremental_marking_record_write_function,                                 \
    "IncrementalMarking::RecordWrite")                                         \
  V(interpreter_dispatch_counters, "Interpreter::dispatch_counters")           \
  V(interpreter_dispatch_table_address, "Interpreter::dispatch_table_address") \
  V(invalidate_prototype_chains_function,                                      \
    "JSObject::InvalidatePrototypeChains()")                                   \
  V(invoke_accessor_getter_callback, "InvokeAccessorGetterCallback")           \
  V(invoke_function_callback, "InvokeFunctionCallback")                        \
  V(isolate_address, "isolate")                                                \
  V(is_profiling_address, "Isolate::is_profiling")                             \
  V(jsreceiver_create_identity_hash, "jsreceiver_create_identity_hash")        \
  V(libc_memchr_function, "libc_memchr")                                       \
  V(libc_memcpy_function, "libc_memcpy")                                       \
  V(libc_memmove_function, "libc_memmove")                                     \
  V(libc_memset_function, "libc_memset")                                       \
  V(log_enter_external_function, "Logger::EnterExternal")                      \
  V(log_leave_external_function, "Logger::LeaveExternal")                      \
  V(mod_two_doubles_operation, "mod_two_doubles")                              \
  V(new_deoptimizer_function, "Deoptimizer::New()")                            \
  V(new_space_allocation_limit_address,                                        \
    "Heap::NewSpaceAllocationLimitAddress()")                                  \
  V(new_space_allocation_top_address, "Heap::NewSpaceAllocationTopAddress()")  \
  V(old_space_allocation_limit_address,                                        \
    "Heap::OldSpaceAllocationLimitAddress")                                    \
  V(old_space_allocation_top_address, "Heap::OldSpaceAllocationTopAddress")    \
  V(orderedhashmap_gethash_raw, "orderedhashmap_gethash_raw")                  \
  V(pending_microtask_count_address,                                           \
    "Isolate::pending_microtask_count_address()")                              \
  V(power_double_double_function, "power_double_double_function")              \
  V(printf_function, "printf")                                                 \
  V(promise_hook_or_debug_is_active_address,                                   \
    "Isolate::promise_hook_or_debug_is_active_address()")                      \
  V(roots_array_start, "Heap::roots_array_start()")                            \
  V(runtime_function_table_address,                                            \
    "Runtime::runtime_function_table_address()")                               \
  V(scheduled_exception_address, "Isolate::scheduled_exception")               \
  V(search_string_raw_one_one, "search_string_raw_one_one")                    \
  V(search_string_raw_one_two, "search_string_raw_one_two")                    \
  V(search_string_raw_two_one, "search_string_raw_two_one")                    \
  V(search_string_raw_two_two, "search_string_raw_two_two")                    \
  V(store_buffer_overflow_function, "StoreBuffer::StoreBufferOverflow")        \
  V(store_buffer_top, "store_buffer_top")                                      \
  V(stress_deopt_count, "Isolate::stress_deopt_count_address()")               \
  V(try_internalize_string_function, "try_internalize_string_function")        \
  V(wasm_call_trap_callback_for_testing,                                       \
    "wasm::call_trap_callback_for_testing")                                    \
  V(wasm_clear_thread_in_wasm_flag, "wasm::clear_thread_in_wasm_flag")         \
  V(wasm_f32_ceil, "wasm::f32_ceil_wrapper")                                   \
  V(wasm_f32_floor, "wasm::f32_floor_wrapper")                                 \
  V(wasm_f32_nearest_int, "wasm::f32_nearest_int_wrapper")                     \
  V(wasm_f32_trunc, "wasm::f32_trunc_wrapper")                                 \
  V(wasm_f64_ceil, "wasm::f64_ceil_wrapper")                                   \
  V(wasm_f64_floor, "wasm::f64_floor_wrapper")                                 \
  V(wasm_f64_nearest_int, "wasm::f64_nearest_int_wrapper")                     \
  V(wasm_f64_trunc, "wasm::f64_trunc_wrapper")                                 \
  V(wasm_float32_to_int64, "wasm::float32_to_int64_wrapper")                   \
  V(wasm_float32_to_uint64, "wasm::float32_to_uint64_wrapper")                 \
  V(wasm_float64_pow, "wasm::float64_pow")                                     \
  V(wasm_float64_to_int64, "wasm::float64_to_int64_wrapper")                   \
  V(wasm_float64_to_uint64, "wasm::float64_to_uint64_wrapper")                 \
  V(wasm_int64_div, "wasm::int64_div")                                         \
  V(wasm_int64_mod, "wasm::int64_mod")                                         \
  V(wasm_int64_to_float32, "wasm::int64_to_float32_wrapper")                   \
  V(wasm_int64_to_float64, "wasm::int64_to_float64_wrapper")                   \
  V(wasm_set_thread_in_wasm_flag, "wasm::set_thread_in_wasm_flag")             \
  V(wasm_uint64_div, "wasm::uint64_div")                                       \
  V(wasm_uint64_mod, "wasm::uint64_mod")                                       \
  V(wasm_uint64_to_float32, "wasm::uint64_to_float32_wrapper")                 \
  V(wasm_uint64_to_float64, "wasm::uint64_to_float64_wrapper")                 \
  V(wasm_word32_ctz, "wasm::word32_ctz")                                       \
  V(wasm_word32_popcnt, "wasm::word32_popcnt")                                 \
  V(wasm_word32_rol, "wasm::word32_rol")                                       \
  V(wasm_word32_ror, "wasm::word32_ror")                                       \
  V(wasm_word64_ctz, "wasm::word64_ctz")                                       \
  V(wasm_word64_popcnt, "wasm::word64_popcnt")                                 \
  EXTERNAL_REFERENCE_LIST_NON_INTERPRETED_REGEXP(V)                            \
  EXTERNAL_REFERENCE_LIST_INTL(V)

#ifndef V8_INTERPRETED_REGEXP
#define EXTERNAL_REFERENCE_LIST_NON_INTERPRETED_REGEXP(V)                     \
  V(address_of_regexp_stack_limit, "RegExpStack::limit_address()")            \
  V(address_of_regexp_stack_memory_address, "RegExpStack::memory_address()")  \
  V(address_of_regexp_stack_memory_size, "RegExpStack::memory_size()")        \
  V(address_of_static_offsets_vector, "OffsetsVector::static_offsets_vector") \
  V(re_case_insensitive_compare_uc16,                                         \
    "NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()")               \
  V(re_check_stack_guard_state,                                               \
    "RegExpMacroAssembler*::CheckStackGuardState()")                          \
  V(re_grow_stack, "NativeRegExpMacroAssembler::GrowStack()")                 \
  V(re_word_character_map, "NativeRegExpMacroAssembler::word_character_map")
#else
#define EXTERNAL_REFERENCE_LIST_NON_INTERPRETED_REGEXP(V)
#endif  // V8_INTERPRETED_REGEXP

#ifdef V8_INTL_SUPPORT
#define EXTERNAL_REFERENCE_LIST_INTL(V)                               \
  V(intl_convert_one_byte_to_lower, "intl_convert_one_byte_to_lower") \
  V(intl_to_latin1_lower_table, "intl_to_latin1_lower_table")
#else
#define EXTERNAL_REFERENCE_LIST_INTL(V)
#endif  // V8_INTL_SUPPORT

// An ExternalReference represents a C++ address used in the generated
// code. All references to C++ functions and variables must be encapsulated
// in an ExternalReference instance. This is done in order to track the
// origin of all external references in the code so that they can be bound
// to the correct addresses when deserializing a heap.
class ExternalReference BASE_EMBEDDED {
 public:
  // Used in the simulator to support different native api calls.
  enum Type {
    // Builtin call.
    // Object* f(v8::internal::Arguments).
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
      EXTERNAL_REFERENCE_LIST(COUNT_EXTERNAL_REFERENCE);
#undef COUNT_EXTERNAL_REFERENCE

  static void SetUp();

  typedef void* ExternalReferenceRedirector(void* original, Type type);

  ExternalReference() : address_(nullptr) {}
  explicit ExternalReference(const SCTableReference& table_ref);
  explicit ExternalReference(StatsCounter* counter);
  ExternalReference(Address address, Isolate* isolate);
  ExternalReference(ApiFunction* ptr, Type type, Isolate* isolate);
  ExternalReference(const Runtime::Function* f, Isolate* isolate);
  ExternalReference(IsolateAddressId id, Isolate* isolate);
  ExternalReference(Runtime::FunctionId id, Isolate* isolate);

  template <typename SubjectChar, typename PatternChar>
  static ExternalReference search_string_raw(Isolate* isolate);

  static ExternalReference page_flags(Page* page);

  static ExternalReference ForDeoptEntry(Address entry);

#define DECL_EXTERNAL_REFERENCE(name, desc) \
  static ExternalReference name(Isolate* isolate);
  EXTERNAL_REFERENCE_LIST(DECL_EXTERNAL_REFERENCE)
#undef DECL_EXTERNAL_REFERENCE

  V8_EXPORT_PRIVATE V8_NOINLINE static ExternalReference
  runtime_function_table_address_for_unittests(Isolate* isolate);

  Address address() const { return reinterpret_cast<Address>(address_); }

  // This lets you register a function that rewrites all external references.
  // Used by the ARM simulator to catch calls to external references.
  static void set_redirector(Isolate* isolate,
                             ExternalReferenceRedirector* redirector);

 private:
  explicit ExternalReference(void* address) : address_(address) {}

  static void* Redirect(Isolate* isolate, Address address_arg,
                        Type type = ExternalReference::BUILTIN_CALL);

  void* address_;
};

V8_EXPORT_PRIVATE bool operator==(ExternalReference, ExternalReference);
bool operator!=(ExternalReference, ExternalReference);

size_t hash_value(ExternalReference);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, ExternalReference);

}  // namespace internal
}  // namespace v8

#endif  // V8_EXTERNAL_REFERENCE_H_

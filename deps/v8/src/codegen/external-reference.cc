// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/external-reference.h"

#include "src/api/api.h"
#include "src/base/ieee754.h"
#include "src/codegen/cpu-features.h"
#include "src/compiler/code-assembler.h"
#include "src/date/date.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/isolate.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/simulator-base.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/ic/stub-cache.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/numbers/math-random.h"
#include "src/objects/elements.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table.h"
#include "src/regexp/experimental/experimental.h"
#include "src/regexp/regexp-interpreter.h"
#include "src/regexp/regexp-macro-assembler-arch.h"
#include "src/regexp/regexp-stack.h"
#include "src/strings/string-search.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-external-refs.h"
#endif  // V8_ENABLE_WEBASSEMBLY

#ifdef V8_INTL_SUPPORT
#include "src/base/platform/wrappers.h"
#include "src/objects/intl-objects.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Common double constants.

constexpr double double_min_int_constant = kMinInt;
constexpr double double_one_half_constant = 0.5;
constexpr uint64_t double_the_hole_nan_constant = kHoleNanInt64;
constexpr double double_uint32_bias_constant =
    static_cast<double>(kMaxUInt32) + 1;

constexpr struct alignas(16) {
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
} float_absolute_constant = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};

constexpr struct alignas(16) {
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
} float_negate_constant = {0x80000000, 0x80000000, 0x80000000, 0x80000000};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} double_absolute_constant = {uint64_t{0x7FFFFFFFFFFFFFFF},
                              uint64_t{0x7FFFFFFFFFFFFFFF}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} double_negate_constant = {uint64_t{0x8000000000000000},
                            uint64_t{0x8000000000000000}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i8x16_swizzle_mask = {uint64_t{0x70707070'70707070},
                             uint64_t{0x70707070'70707070}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i8x16_popcnt_mask = {uint64_t{0x03020201'02010100},
                            uint64_t{0x04030302'03020201}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i8x16_splat_0x01 = {uint64_t{0x01010101'01010101},
                           uint64_t{0x01010101'01010101}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i8x16_splat_0x0f = {uint64_t{0x0F0F0F0F'0F0F0F0F},
                           uint64_t{0x0F0F0F0F'0F0F0F0F}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i8x16_splat_0x33 = {uint64_t{0x33333333'33333333},
                           uint64_t{0x33333333'33333333}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i8x16_splat_0x55 = {uint64_t{0x55555555'55555555},
                           uint64_t{0x55555555'55555555}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_i16x8_splat_0x0001 = {uint64_t{0x00010001'00010001},
                             uint64_t{0x00010001'00010001}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_f64x2_convert_low_i32x4_u_int_mask = {uint64_t{0x4330000043300000},
                                             uint64_t{0x4330000043300000}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_double_2_power_52 = {uint64_t{0x4330000000000000},
                            uint64_t{0x4330000000000000}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_int32_max_as_double = {uint64_t{0x41dfffffffc00000},
                              uint64_t{0x41dfffffffc00000}};

constexpr struct alignas(16) {
  uint64_t a;
  uint64_t b;
} wasm_uint32_max_as_double = {uint64_t{0x41efffffffe00000},
                               uint64_t{0x41efffffffe00000}};

// Implementation of ExternalReference

static ExternalReference::Type BuiltinCallTypeForResultSize(int result_size) {
  switch (result_size) {
    case 1:
      return ExternalReference::BUILTIN_CALL;
    case 2:
      return ExternalReference::BUILTIN_CALL_PAIR;
  }
  UNREACHABLE();
}

// static
ExternalReference ExternalReference::Create(
    ApiFunction* fun, Type type = ExternalReference::BUILTIN_CALL) {
  return ExternalReference(Redirect(fun->address(), type));
}

// static
ExternalReference ExternalReference::Create(Runtime::FunctionId id) {
  return Create(Runtime::FunctionForId(id));
}

// static
ExternalReference ExternalReference::Create(const Runtime::Function* f) {
  return ExternalReference(
      Redirect(f->entry, BuiltinCallTypeForResultSize(f->result_size)));
}

// static
ExternalReference ExternalReference::Create(Address address) {
  return ExternalReference(Redirect(address));
}

ExternalReference ExternalReference::isolate_address(Isolate* isolate) {
  return ExternalReference(isolate);
}

ExternalReference ExternalReference::builtins_address(Isolate* isolate) {
  return ExternalReference(isolate->heap()->builtin_address(0));
}

ExternalReference ExternalReference::handle_scope_implementer_address(
    Isolate* isolate) {
  return ExternalReference(isolate->handle_scope_implementer_address());
}

#ifdef V8_HEAP_SANDBOX
ExternalReference ExternalReference::external_pointer_table_address(
    Isolate* isolate) {
  return ExternalReference(isolate->external_pointer_table_address());
}
#endif

ExternalReference ExternalReference::interpreter_dispatch_table_address(
    Isolate* isolate) {
  return ExternalReference(isolate->interpreter()->dispatch_table_address());
}

ExternalReference ExternalReference::interpreter_dispatch_counters(
    Isolate* isolate) {
  return ExternalReference(
      isolate->interpreter()->bytecode_dispatch_counters_table());
}

ExternalReference
ExternalReference::address_of_interpreter_entry_trampoline_instruction_start(
    Isolate* isolate) {
  return ExternalReference(
      isolate->interpreter()
          ->address_of_interpreter_entry_trampoline_instruction_start());
}

ExternalReference ExternalReference::bytecode_size_table_address() {
  return ExternalReference(
      interpreter::Bytecodes::bytecode_size_table_address());
}

// static
ExternalReference ExternalReference::Create(StatsCounter* counter) {
  return ExternalReference(
      reinterpret_cast<Address>(counter->GetInternalPointer()));
}

// static
ExternalReference ExternalReference::Create(IsolateAddressId id,
                                            Isolate* isolate) {
  return ExternalReference(isolate->get_address_from_id(id));
}

// static
ExternalReference ExternalReference::Create(const SCTableReference& table_ref) {
  return ExternalReference(table_ref.address());
}

namespace {

// Helper function to verify that all types in a list of types are scalar.
// This includes primitive types (int, Address) and pointer types. We also
// allow void.
template <typename T>
constexpr bool AllScalar() {
  return std::is_scalar<T>::value || std::is_void<T>::value;
}

template <typename T1, typename T2, typename... Rest>
constexpr bool AllScalar() {
  return AllScalar<T1>() && AllScalar<T2, Rest...>();
}

// Checks a function pointer's type for compatibility with the
// ExternalReference calling mechanism. Specifically, all arguments
// as well as the result type must pass the AllScalar check above,
// because we expect each item to fit into one register or stack slot.
template <typename T>
struct IsValidExternalReferenceType;

template <typename Result, typename... Args>
struct IsValidExternalReferenceType<Result (*)(Args...)> {
  static const bool value = AllScalar<Result, Args...>();
};

template <typename Result, typename Class, typename... Args>
struct IsValidExternalReferenceType<Result (Class::*)(Args...)> {
  static const bool value = AllScalar<Result, Args...>();
};

}  // namespace

#define FUNCTION_REFERENCE(Name, Target)                                   \
  ExternalReference ExternalReference::Name() {                            \
    STATIC_ASSERT(IsValidExternalReferenceType<decltype(&Target)>::value); \
    return ExternalReference(Redirect(FUNCTION_ADDR(Target)));             \
  }

#define FUNCTION_REFERENCE_WITH_ISOLATE(Name, Target)                      \
  ExternalReference ExternalReference::Name(Isolate* isolate) {            \
    STATIC_ASSERT(IsValidExternalReferenceType<decltype(&Target)>::value); \
    return ExternalReference(Redirect(FUNCTION_ADDR(Target)));             \
  }

#define FUNCTION_REFERENCE_WITH_TYPE(Name, Target, Type)                   \
  ExternalReference ExternalReference::Name() {                            \
    STATIC_ASSERT(IsValidExternalReferenceType<decltype(&Target)>::value); \
    return ExternalReference(Redirect(FUNCTION_ADDR(Target), Type));       \
  }

FUNCTION_REFERENCE(write_barrier_marking_from_code_function,
                   WriteBarrier::MarkingFromCode)

FUNCTION_REFERENCE(insert_remembered_set_function,
                   Heap::InsertIntoRememberedSetFromCode)

FUNCTION_REFERENCE(delete_handle_scope_extensions,
                   HandleScope::DeleteExtensions)

FUNCTION_REFERENCE(ephemeron_key_write_barrier_function,
                   Heap::EphemeronKeyWriteBarrierFromCode)

FUNCTION_REFERENCE(get_date_field_function, JSDate::GetField)

ExternalReference ExternalReference::date_cache_stamp(Isolate* isolate) {
  return ExternalReference(isolate->date_cache()->stamp_address());
}

// static
ExternalReference
ExternalReference::runtime_function_table_address_for_unittests(
    Isolate* isolate) {
  return runtime_function_table_address(isolate);
}

// static
Address ExternalReference::Redirect(Address address, Type type) {
#ifdef USE_SIMULATOR
  return SimulatorBase::RedirectExternalReference(address, type);
#else
  return address;
#endif
}

ExternalReference ExternalReference::stress_deopt_count(Isolate* isolate) {
  return ExternalReference(isolate->stress_deopt_count_address());
}

ExternalReference ExternalReference::force_slow_path(Isolate* isolate) {
  return ExternalReference(isolate->force_slow_path_address());
}

FUNCTION_REFERENCE(new_deoptimizer_function, Deoptimizer::New)

FUNCTION_REFERENCE(compute_output_frames_function,
                   Deoptimizer::ComputeOutputFrames)

IF_WASM(FUNCTION_REFERENCE, wasm_f32_trunc, wasm::f32_trunc_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32_floor, wasm::f32_floor_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32_ceil, wasm::f32_ceil_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32_nearest_int, wasm::f32_nearest_int_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64_trunc, wasm::f64_trunc_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64_floor, wasm::f64_floor_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64_ceil, wasm::f64_ceil_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64_nearest_int, wasm::f64_nearest_int_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_int64_to_float32,
        wasm::int64_to_float32_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_uint64_to_float32,
        wasm::uint64_to_float32_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_int64_to_float64,
        wasm::int64_to_float64_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_uint64_to_float64,
        wasm::uint64_to_float64_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float32_to_int64,
        wasm::float32_to_int64_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float32_to_uint64,
        wasm::float32_to_uint64_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float64_to_int64,
        wasm::float64_to_int64_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float64_to_uint64,
        wasm::float64_to_uint64_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float32_to_int64_sat,
        wasm::float32_to_int64_sat_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float32_to_uint64_sat,
        wasm::float32_to_uint64_sat_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float64_to_int64_sat,
        wasm::float64_to_int64_sat_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float64_to_uint64_sat,
        wasm::float64_to_uint64_sat_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_int64_div, wasm::int64_div_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_int64_mod, wasm::int64_mod_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_uint64_div, wasm::uint64_div_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_uint64_mod, wasm::uint64_mod_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word32_ctz, wasm::word32_ctz_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word64_ctz, wasm::word64_ctz_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word32_popcnt, wasm::word32_popcnt_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word64_popcnt, wasm::word64_popcnt_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word32_rol, wasm::word32_rol_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word32_ror, wasm::word32_ror_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word64_rol, wasm::word64_rol_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_word64_ror, wasm::word64_ror_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64x2_ceil, wasm::f64x2_ceil_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64x2_floor, wasm::f64x2_floor_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64x2_trunc, wasm::f64x2_trunc_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f64x2_nearest_int,
        wasm::f64x2_nearest_int_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32x4_ceil, wasm::f32x4_ceil_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32x4_floor, wasm::f32x4_floor_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32x4_trunc, wasm::f32x4_trunc_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_f32x4_nearest_int,
        wasm::f32x4_nearest_int_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_memory_init, wasm::memory_init_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_memory_copy, wasm::memory_copy_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_memory_fill, wasm::memory_fill_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_float64_pow, wasm::float64_pow_wrapper)
IF_WASM(FUNCTION_REFERENCE, wasm_call_trap_callback_for_testing,
        wasm::call_trap_callback_for_testing)

static void f64_acos_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  WriteUnalignedValue(data, base::ieee754::acos(input));
}

FUNCTION_REFERENCE(f64_acos_wrapper_function, f64_acos_wrapper)

static void f64_asin_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  WriteUnalignedValue<double>(data, base::ieee754::asin(input));
}

FUNCTION_REFERENCE(f64_asin_wrapper_function, f64_asin_wrapper)


static void f64_mod_wrapper(Address data) {
  double dividend = ReadUnalignedValue<double>(data);
  double divisor = ReadUnalignedValue<double>(data + sizeof(dividend));
  WriteUnalignedValue<double>(data, Modulo(dividend, divisor));
}

FUNCTION_REFERENCE(f64_mod_wrapper_function, f64_mod_wrapper)

ExternalReference ExternalReference::isolate_root(Isolate* isolate) {
  return ExternalReference(isolate->isolate_root());
}

ExternalReference ExternalReference::allocation_sites_list_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->allocation_sites_list_address());
}

ExternalReference ExternalReference::address_of_jslimit(Isolate* isolate) {
  Address address = isolate->stack_guard()->address_of_jslimit();
  // For efficient generated code, this should be root-register-addressable.
  DCHECK(isolate->root_register_addressable_region().contains(address));
  return ExternalReference(address);
}

ExternalReference ExternalReference::address_of_real_jslimit(Isolate* isolate) {
  Address address = isolate->stack_guard()->address_of_real_jslimit();
  // For efficient generated code, this should be root-register-addressable.
  DCHECK(isolate->root_register_addressable_region().contains(address));
  return ExternalReference(address);
}

ExternalReference ExternalReference::heap_is_marking_flag_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->IsMarkingFlagAddress());
}

ExternalReference ExternalReference::new_space_allocation_top_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->NewSpaceAllocationTopAddress());
}

ExternalReference ExternalReference::new_space_allocation_limit_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->NewSpaceAllocationLimitAddress());
}

ExternalReference ExternalReference::old_space_allocation_top_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->OldSpaceAllocationTopAddress());
}

ExternalReference ExternalReference::old_space_allocation_limit_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->OldSpaceAllocationLimitAddress());
}

ExternalReference ExternalReference::handle_scope_level_address(
    Isolate* isolate) {
  return ExternalReference(HandleScope::current_level_address(isolate));
}

ExternalReference ExternalReference::handle_scope_next_address(
    Isolate* isolate) {
  return ExternalReference(HandleScope::current_next_address(isolate));
}

ExternalReference ExternalReference::handle_scope_limit_address(
    Isolate* isolate) {
  return ExternalReference(HandleScope::current_limit_address(isolate));
}

ExternalReference ExternalReference::scheduled_exception_address(
    Isolate* isolate) {
  return ExternalReference(isolate->scheduled_exception_address());
}

ExternalReference ExternalReference::address_of_pending_message_obj(
    Isolate* isolate) {
  return ExternalReference(isolate->pending_message_obj_address());
}

FUNCTION_REFERENCE(abort_with_reason, i::abort_with_reason)

ExternalReference ExternalReference::address_of_min_int() {
  return ExternalReference(reinterpret_cast<Address>(&double_min_int_constant));
}

ExternalReference
ExternalReference::address_of_mock_arraybuffer_allocator_flag() {
  return ExternalReference(&FLAG_mock_arraybuffer_allocator);
}

ExternalReference ExternalReference::address_of_builtin_subclassing_flag() {
  return ExternalReference(&FLAG_builtin_subclassing);
}

ExternalReference
ExternalReference::address_of_harmony_regexp_match_indices_flag() {
  return ExternalReference(&FLAG_harmony_regexp_match_indices);
}

ExternalReference ExternalReference::address_of_runtime_stats_flag() {
  return ExternalReference(&TracingFlags::runtime_stats);
}

ExternalReference ExternalReference::address_of_load_from_stack_count(
    const char* function_name) {
  return ExternalReference(
      Isolate::load_from_stack_count_address(function_name));
}

ExternalReference ExternalReference::address_of_store_to_stack_count(
    const char* function_name) {
  return ExternalReference(
      Isolate::store_to_stack_count_address(function_name));
}

ExternalReference ExternalReference::address_of_one_half() {
  return ExternalReference(
      reinterpret_cast<Address>(&double_one_half_constant));
}

ExternalReference ExternalReference::address_of_the_hole_nan() {
  return ExternalReference(
      reinterpret_cast<Address>(&double_the_hole_nan_constant));
}

ExternalReference ExternalReference::address_of_uint32_bias() {
  return ExternalReference(
      reinterpret_cast<Address>(&double_uint32_bias_constant));
}

ExternalReference ExternalReference::address_of_float_abs_constant() {
  return ExternalReference(reinterpret_cast<Address>(&float_absolute_constant));
}

ExternalReference ExternalReference::address_of_float_neg_constant() {
  return ExternalReference(reinterpret_cast<Address>(&float_negate_constant));
}

ExternalReference ExternalReference::address_of_double_abs_constant() {
  return ExternalReference(
      reinterpret_cast<Address>(&double_absolute_constant));
}

ExternalReference ExternalReference::address_of_double_neg_constant() {
  return ExternalReference(reinterpret_cast<Address>(&double_negate_constant));
}

ExternalReference ExternalReference::address_of_wasm_i8x16_swizzle_mask() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i8x16_swizzle_mask));
}

ExternalReference ExternalReference::address_of_wasm_i8x16_popcnt_mask() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i8x16_popcnt_mask));
}

ExternalReference ExternalReference::address_of_wasm_i8x16_splat_0x01() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i8x16_splat_0x01));
}

ExternalReference ExternalReference::address_of_wasm_i8x16_splat_0x0f() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i8x16_splat_0x0f));
}

ExternalReference ExternalReference::address_of_wasm_i8x16_splat_0x33() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i8x16_splat_0x33));
}

ExternalReference ExternalReference::address_of_wasm_i8x16_splat_0x55() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i8x16_splat_0x55));
}

ExternalReference ExternalReference::address_of_wasm_i16x8_splat_0x0001() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_i16x8_splat_0x0001));
}

ExternalReference
ExternalReference::address_of_wasm_f64x2_convert_low_i32x4_u_int_mask() {
  return ExternalReference(
      reinterpret_cast<Address>(&wasm_f64x2_convert_low_i32x4_u_int_mask));
}

ExternalReference ExternalReference::supports_wasm_simd_128_address() {
  return ExternalReference(
      reinterpret_cast<Address>(&CpuFeatures::supports_wasm_simd_128_));
}

ExternalReference ExternalReference::address_of_wasm_double_2_power_52() {
  return ExternalReference(reinterpret_cast<Address>(&wasm_double_2_power_52));
}

ExternalReference ExternalReference::address_of_wasm_int32_max_as_double() {
  return ExternalReference(
      reinterpret_cast<Address>(&wasm_int32_max_as_double));
}

ExternalReference ExternalReference::address_of_wasm_uint32_max_as_double() {
  return ExternalReference(
      reinterpret_cast<Address>(&wasm_uint32_max_as_double));
}

ExternalReference
ExternalReference::address_of_enable_experimental_regexp_engine() {
  return ExternalReference(&FLAG_enable_experimental_regexp_engine);
}

namespace {

static uintptr_t BaselinePCForBytecodeOffset(Address raw_code_obj,
                                             int bytecode_offset,
                                             Address raw_bytecode_array) {
  Code code_obj = Code::cast(Object(raw_code_obj));
  BytecodeArray bytecode_array =
      BytecodeArray::cast(Object(raw_bytecode_array));
  return code_obj.GetBaselineStartPCForBytecodeOffset(bytecode_offset,
                                                      bytecode_array);
}

static uintptr_t BaselinePCForNextExecutedBytecode(Address raw_code_obj,
                                                   int bytecode_offset,
                                                   Address raw_bytecode_array) {
  Code code_obj = Code::cast(Object(raw_code_obj));
  BytecodeArray bytecode_array =
      BytecodeArray::cast(Object(raw_bytecode_array));
  return code_obj.GetBaselinePCForNextExecutedBytecode(bytecode_offset,
                                                       bytecode_array);
}

}  // namespace

FUNCTION_REFERENCE(baseline_pc_for_bytecode_offset, BaselinePCForBytecodeOffset)
FUNCTION_REFERENCE(baseline_pc_for_next_executed_bytecode,
                   BaselinePCForNextExecutedBytecode)

ExternalReference ExternalReference::thread_in_wasm_flag_address_address(
    Isolate* isolate) {
  return ExternalReference(isolate->thread_in_wasm_flag_address_address());
}

ExternalReference ExternalReference::is_profiling_address(Isolate* isolate) {
  return ExternalReference(isolate->is_profiling_address());
}

ExternalReference ExternalReference::invoke_function_callback() {
  Address thunk_address = FUNCTION_ADDR(&InvokeFunctionCallback);
  ExternalReference::Type thunk_type = ExternalReference::PROFILING_API_CALL;
  ApiFunction thunk_fun(thunk_address);
  return ExternalReference::Create(&thunk_fun, thunk_type);
}

ExternalReference ExternalReference::invoke_accessor_getter_callback() {
  Address thunk_address = FUNCTION_ADDR(&InvokeAccessorGetterCallback);
  ExternalReference::Type thunk_type = ExternalReference::PROFILING_GETTER_CALL;
  ApiFunction thunk_fun(thunk_address);
  return ExternalReference::Create(&thunk_fun, thunk_type);
}

#if V8_TARGET_ARCH_X64
#define re_stack_check_func RegExpMacroAssemblerX64::CheckStackGuardState
#elif V8_TARGET_ARCH_IA32
#define re_stack_check_func RegExpMacroAssemblerIA32::CheckStackGuardState
#elif V8_TARGET_ARCH_ARM64
#define re_stack_check_func RegExpMacroAssemblerARM64::CheckStackGuardState
#elif V8_TARGET_ARCH_ARM
#define re_stack_check_func RegExpMacroAssemblerARM::CheckStackGuardState
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#define re_stack_check_func RegExpMacroAssemblerPPC::CheckStackGuardState
#elif V8_TARGET_ARCH_MIPS
#define re_stack_check_func RegExpMacroAssemblerMIPS::CheckStackGuardState
#elif V8_TARGET_ARCH_MIPS64
#define re_stack_check_func RegExpMacroAssemblerMIPS::CheckStackGuardState
#elif V8_TARGET_ARCH_S390
#define re_stack_check_func RegExpMacroAssemblerS390::CheckStackGuardState
#elif V8_TARGET_ARCH_RISCV64
#define re_stack_check_func RegExpMacroAssemblerRISCV::CheckStackGuardState
#else
UNREACHABLE();
#endif

FUNCTION_REFERENCE_WITH_ISOLATE(re_check_stack_guard_state, re_stack_check_func)
#undef re_stack_check_func

FUNCTION_REFERENCE_WITH_ISOLATE(re_grow_stack,
                                NativeRegExpMacroAssembler::GrowStack)

FUNCTION_REFERENCE(re_match_for_call_from_js,
                   IrregexpInterpreter::MatchForCallFromJs)

FUNCTION_REFERENCE(re_experimental_match_for_call_from_js,
                   ExperimentalRegExp::MatchForCallFromJs)

FUNCTION_REFERENCE_WITH_ISOLATE(
    re_case_insensitive_compare_unicode,
    NativeRegExpMacroAssembler::CaseInsensitiveCompareUnicode)

FUNCTION_REFERENCE_WITH_ISOLATE(
    re_case_insensitive_compare_non_unicode,
    NativeRegExpMacroAssembler::CaseInsensitiveCompareNonUnicode)

ExternalReference ExternalReference::re_word_character_map(Isolate* isolate) {
  return ExternalReference(
      NativeRegExpMacroAssembler::word_character_map_address());
}

ExternalReference ExternalReference::address_of_static_offsets_vector(
    Isolate* isolate) {
  return ExternalReference(
      reinterpret_cast<Address>(isolate->jsregexp_static_offsets_vector()));
}

ExternalReference ExternalReference::address_of_regexp_stack_limit_address(
    Isolate* isolate) {
  return ExternalReference(isolate->regexp_stack()->limit_address_address());
}

ExternalReference ExternalReference::address_of_regexp_stack_memory_top_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->regexp_stack()->memory_top_address_address());
}

FUNCTION_REFERENCE_WITH_TYPE(ieee754_acos_function, base::ieee754::acos,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_acosh_function, base::ieee754::acosh,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_asin_function, base::ieee754::asin,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_asinh_function, base::ieee754::asinh,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_atan_function, base::ieee754::atan,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_atanh_function, base::ieee754::atanh,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_atan2_function, base::ieee754::atan2,
                             BUILTIN_FP_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_cbrt_function, base::ieee754::cbrt,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_cos_function, base::ieee754::cos,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_cosh_function, base::ieee754::cosh,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_exp_function, base::ieee754::exp,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_expm1_function, base::ieee754::expm1,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_log_function, base::ieee754::log,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_log1p_function, base::ieee754::log1p,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_log10_function, base::ieee754::log10,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_log2_function, base::ieee754::log2,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_sin_function, base::ieee754::sin,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_sinh_function, base::ieee754::sinh,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_tan_function, base::ieee754::tan,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_tanh_function, base::ieee754::tanh,
                             BUILTIN_FP_CALL)
FUNCTION_REFERENCE_WITH_TYPE(ieee754_pow_function, base::ieee754::pow,
                             BUILTIN_FP_FP_CALL)

void* libc_memchr(void* string, int character, size_t search_length) {
  return memchr(string, character, search_length);
}

FUNCTION_REFERENCE(libc_memchr_function, libc_memchr)

void* libc_memcpy(void* dest, const void* src, size_t n) {
  return base::Memcpy(dest, src, n);
}

FUNCTION_REFERENCE(libc_memcpy_function, libc_memcpy)

void* libc_memmove(void* dest, const void* src, size_t n) {
  return memmove(dest, src, n);
}

FUNCTION_REFERENCE(libc_memmove_function, libc_memmove)

void* libc_memset(void* dest, int value, size_t n) {
  DCHECK_EQ(static_cast<byte>(value), value);
  return memset(dest, value, n);
}

FUNCTION_REFERENCE(libc_memset_function, libc_memset)

ExternalReference ExternalReference::printf_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(std::printf)));
}

FUNCTION_REFERENCE(refill_math_random, MathRandom::RefillCache)

template <typename SubjectChar, typename PatternChar>
ExternalReference ExternalReference::search_string_raw() {
  auto f = SearchStringRaw<SubjectChar, PatternChar>;
  return ExternalReference(Redirect(FUNCTION_ADDR(f)));
}

FUNCTION_REFERENCE(jsarray_array_join_concat_to_sequential_string,
                   JSArray::ArrayJoinConcatToSequentialString)

ExternalReference ExternalReference::search_string_raw_one_one() {
  return search_string_raw<const uint8_t, const uint8_t>();
}

ExternalReference ExternalReference::search_string_raw_one_two() {
  return search_string_raw<const uint8_t, const uc16>();
}

ExternalReference ExternalReference::search_string_raw_two_one() {
  return search_string_raw<const uc16, const uint8_t>();
}

ExternalReference ExternalReference::search_string_raw_two_two() {
  return search_string_raw<const uc16, const uc16>();
}

namespace {

void StringWriteToFlatOneByte(Address source, uint8_t* sink, int32_t from,
                              int32_t to) {
  return String::WriteToFlat<uint8_t>(String::cast(Object(source)), sink, from,
                                      to);
}

void StringWriteToFlatTwoByte(Address source, uint16_t* sink, int32_t from,
                              int32_t to) {
  return String::WriteToFlat<uint16_t>(String::cast(Object(source)), sink, from,
                                       to);
}

const uint8_t* ExternalOneByteStringGetChars(Address string) {
  // The following CHECK is a workaround to prevent a CFI bug where
  // ExternalOneByteStringGetChars() and ExternalTwoByteStringGetChars() are
  // merged by the linker, resulting in one of the input type's vtable address
  // failing the address range check.
  // TODO(chromium:1160961): Consider removing the CHECK when CFI is fixed.
  CHECK(Object(string).IsExternalOneByteString());
  return ExternalOneByteString::cast(Object(string)).GetChars();
}
const uint16_t* ExternalTwoByteStringGetChars(Address string) {
  // The following CHECK is a workaround to prevent a CFI bug where
  // ExternalOneByteStringGetChars() and ExternalTwoByteStringGetChars() are
  // merged by the linker, resulting in one of the input type's vtable address
  // failing the address range check.
  // TODO(chromium:1160961): Consider removing the CHECK when CFI is fixed.
  CHECK(Object(string).IsExternalTwoByteString());
  return ExternalTwoByteString::cast(Object(string)).GetChars();
}

}  // namespace

FUNCTION_REFERENCE(string_write_to_flat_one_byte, StringWriteToFlatOneByte)
FUNCTION_REFERENCE(string_write_to_flat_two_byte, StringWriteToFlatTwoByte)

FUNCTION_REFERENCE(external_one_byte_string_get_chars,
                   ExternalOneByteStringGetChars)
FUNCTION_REFERENCE(external_two_byte_string_get_chars,
                   ExternalTwoByteStringGetChars)

FUNCTION_REFERENCE(orderedhashmap_gethash_raw, OrderedHashMap::GetHash)

Address GetOrCreateHash(Isolate* isolate, Address raw_key) {
  DisallowGarbageCollection no_gc;
  return Object(raw_key).GetOrCreateHash(isolate).ptr();
}

FUNCTION_REFERENCE(get_or_create_hash_raw, GetOrCreateHash)

static Address JSReceiverCreateIdentityHash(Isolate* isolate, Address raw_key) {
  JSReceiver key = JSReceiver::cast(Object(raw_key));
  return JSReceiver::CreateIdentityHash(isolate, key).ptr();
}

FUNCTION_REFERENCE(jsreceiver_create_identity_hash,
                   JSReceiverCreateIdentityHash)

static uint32_t ComputeSeededIntegerHash(Isolate* isolate, int32_t key) {
  DisallowGarbageCollection no_gc;
  return ComputeSeededHash(static_cast<uint32_t>(key), HashSeed(isolate));
}

FUNCTION_REFERENCE(compute_integer_hash, ComputeSeededIntegerHash)
FUNCTION_REFERENCE(copy_fast_number_jsarray_elements_to_typed_array,
                   CopyFastNumberJSArrayElementsToTypedArray)
FUNCTION_REFERENCE(copy_typed_array_elements_to_typed_array,
                   CopyTypedArrayElementsToTypedArray)
FUNCTION_REFERENCE(copy_typed_array_elements_slice, CopyTypedArrayElementsSlice)
FUNCTION_REFERENCE(try_string_to_index_or_lookup_existing,
                   StringTable::TryStringToIndexOrLookupExisting)
FUNCTION_REFERENCE(string_to_array_index_function, String::ToArrayIndex)

static Address LexicographicCompareWrapper(Isolate* isolate, Address smi_x,
                                           Address smi_y) {
  Smi x(smi_x);
  Smi y(smi_y);
  return Smi::LexicographicCompare(isolate, x, y);
}

FUNCTION_REFERENCE(smi_lexicographic_compare_function,
                   LexicographicCompareWrapper)

FUNCTION_REFERENCE(mutable_big_int_absolute_add_and_canonicalize_function,
                   MutableBigInt_AbsoluteAddAndCanonicalize)

FUNCTION_REFERENCE(mutable_big_int_absolute_compare_function,
                   MutableBigInt_AbsoluteCompare)

FUNCTION_REFERENCE(mutable_big_int_absolute_sub_and_canonicalize_function,
                   MutableBigInt_AbsoluteSubAndCanonicalize)

FUNCTION_REFERENCE(check_object_type, CheckObjectType)

#ifdef V8_INTL_SUPPORT

static Address ConvertOneByteToLower(Address raw_src, Address raw_dst) {
  String src = String::cast(Object(raw_src));
  String dst = String::cast(Object(raw_dst));
  return Intl::ConvertOneByteToLower(src, dst).ptr();
}
FUNCTION_REFERENCE(intl_convert_one_byte_to_lower, ConvertOneByteToLower)

ExternalReference ExternalReference::intl_to_latin1_lower_table() {
  uint8_t* ptr = const_cast<uint8_t*>(Intl::ToLatin1LowerTable());
  return ExternalReference(reinterpret_cast<Address>(ptr));
}
#endif  // V8_INTL_SUPPORT

// Explicit instantiations for all combinations of 1- and 2-byte strings.
template ExternalReference
ExternalReference::search_string_raw<const uint8_t, const uint8_t>();
template ExternalReference
ExternalReference::search_string_raw<const uint8_t, const uc16>();
template ExternalReference
ExternalReference::search_string_raw<const uc16, const uint8_t>();
template ExternalReference
ExternalReference::search_string_raw<const uc16, const uc16>();

ExternalReference ExternalReference::FromRawAddress(Address address) {
  return ExternalReference(address);
}

ExternalReference ExternalReference::cpu_features() {
  DCHECK(CpuFeatures::initialized_);
  return ExternalReference(&CpuFeatures::supported_);
}

ExternalReference ExternalReference::promise_hook_flags_address(
    Isolate* isolate) {
  return ExternalReference(isolate->promise_hook_flags_address());
}

ExternalReference ExternalReference::promise_hook_address(Isolate* isolate) {
  return ExternalReference(isolate->promise_hook_address());
}

ExternalReference ExternalReference::async_event_delegate_address(
    Isolate* isolate) {
  return ExternalReference(isolate->async_event_delegate_address());
}

ExternalReference ExternalReference::debug_execution_mode_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug_execution_mode_address());
}

ExternalReference ExternalReference::debug_is_active_address(Isolate* isolate) {
  return ExternalReference(isolate->debug()->is_active_address());
}

ExternalReference ExternalReference::debug_hook_on_function_call_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug()->hook_on_function_call_address());
}

ExternalReference ExternalReference::runtime_function_table_address(
    Isolate* isolate) {
  return ExternalReference(
      const_cast<Runtime::Function*>(Runtime::RuntimeFunctionTable(isolate)));
}

static Address InvalidatePrototypeChainsWrapper(Address raw_map) {
  Map map = Map::cast(Object(raw_map));
  return JSObject::InvalidatePrototypeChains(map).ptr();
}

FUNCTION_REFERENCE(invalidate_prototype_chains_function,
                   InvalidatePrototypeChainsWrapper)

double modulo_double_double(double x, double y) { return Modulo(x, y); }

FUNCTION_REFERENCE_WITH_TYPE(mod_two_doubles_operation, modulo_double_double,
                             BUILTIN_FP_FP_CALL)

ExternalReference ExternalReference::debug_suspended_generator_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug()->suspended_generator_address());
}

ExternalReference ExternalReference::debug_restart_fp_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug()->restart_fp_address());
}

ExternalReference ExternalReference::fast_c_call_caller_fp_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->isolate_data()->fast_c_call_caller_fp_address());
}

ExternalReference ExternalReference::fast_c_call_caller_pc_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->isolate_data()->fast_c_call_caller_pc_address());
}

ExternalReference ExternalReference::fast_api_call_target_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->isolate_data()->fast_api_call_target_address());
}

ExternalReference ExternalReference::stack_is_iterable_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->isolate_data()->stack_is_iterable_address());
}

FUNCTION_REFERENCE(call_enqueue_microtask_function,
                   MicrotaskQueue::CallEnqueueMicrotask)

static int64_t atomic_pair_load(intptr_t address) {
  return std::atomic_load(reinterpret_cast<std::atomic<int64_t>*>(address));
}

ExternalReference ExternalReference::atomic_pair_load_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_load)));
}

static void atomic_pair_store(intptr_t address, int value_low, int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  std::atomic_store(reinterpret_cast<std::atomic<int64_t>*>(address), value);
}

ExternalReference ExternalReference::atomic_pair_store_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_store)));
}

static int64_t atomic_pair_add(intptr_t address, int value_low,
                               int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  return std::atomic_fetch_add(reinterpret_cast<std::atomic<int64_t>*>(address),
                               value);
}

ExternalReference ExternalReference::atomic_pair_add_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_add)));
}

static int64_t atomic_pair_sub(intptr_t address, int value_low,
                               int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  return std::atomic_fetch_sub(reinterpret_cast<std::atomic<int64_t>*>(address),
                               value);
}

ExternalReference ExternalReference::atomic_pair_sub_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_sub)));
}

static int64_t atomic_pair_and(intptr_t address, int value_low,
                               int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  return std::atomic_fetch_and(reinterpret_cast<std::atomic<int64_t>*>(address),
                               value);
}

ExternalReference ExternalReference::atomic_pair_and_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_and)));
}

static int64_t atomic_pair_or(intptr_t address, int value_low, int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  return std::atomic_fetch_or(reinterpret_cast<std::atomic<int64_t>*>(address),
                              value);
}

ExternalReference ExternalReference::atomic_pair_or_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_or)));
}

static int64_t atomic_pair_xor(intptr_t address, int value_low,
                               int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  return std::atomic_fetch_xor(reinterpret_cast<std::atomic<int64_t>*>(address),
                               value);
}

ExternalReference ExternalReference::atomic_pair_xor_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_xor)));
}

static int64_t atomic_pair_exchange(intptr_t address, int value_low,
                                    int value_high) {
  int64_t value =
      static_cast<int64_t>(value_high) << 32 | (value_low & 0xFFFFFFFF);
  return std::atomic_exchange(reinterpret_cast<std::atomic<int64_t>*>(address),
                              value);
}

ExternalReference ExternalReference::atomic_pair_exchange_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(atomic_pair_exchange)));
}

static uint64_t atomic_pair_compare_exchange(intptr_t address,
                                             int old_value_low,
                                             int old_value_high,
                                             int new_value_low,
                                             int new_value_high) {
  uint64_t old_value = static_cast<uint64_t>(old_value_high) << 32 |
                       (old_value_low & 0xFFFFFFFF);
  uint64_t new_value = static_cast<uint64_t>(new_value_high) << 32 |
                       (new_value_low & 0xFFFFFFFF);
  std::atomic_compare_exchange_strong(
      reinterpret_cast<std::atomic<uint64_t>*>(address), &old_value, new_value);
  return old_value;
}

FUNCTION_REFERENCE(atomic_pair_compare_exchange_function,
                   atomic_pair_compare_exchange)

static int EnterMicrotaskContextWrapper(HandleScopeImplementer* hsi,
                                        Address raw_context) {
  Context context = Context::cast(Object(raw_context));
  hsi->EnterMicrotaskContext(context);
  return 0;
}

FUNCTION_REFERENCE(call_enter_context_function, EnterMicrotaskContextWrapper)

FUNCTION_REFERENCE(
    js_finalization_registry_remove_cell_from_unregister_token_map,
    JSFinalizationRegistry::RemoveCellFromUnregisterTokenMap)

#ifdef V8_HEAP_SANDBOX
FUNCTION_REFERENCE(external_pointer_table_grow_table_function,
                   ExternalPointerTable::GrowTable)
#endif

bool operator==(ExternalReference lhs, ExternalReference rhs) {
  return lhs.address() == rhs.address();
}

bool operator!=(ExternalReference lhs, ExternalReference rhs) {
  return !(lhs == rhs);
}

size_t hash_value(ExternalReference reference) {
  if (FLAG_predictable) {
    // Avoid ASLR non-determinism in predictable mode. For this, just take the
    // lowest 12 bit corresponding to a 4K page size.
    return base::hash<Address>()(reference.address() & 0xfff);
  }
  return base::hash<Address>()(reference.address());
}

std::ostream& operator<<(std::ostream& os, ExternalReference reference) {
  os << reinterpret_cast<const void*>(reference.address());
  const Runtime::Function* fn = Runtime::FunctionForEntry(reference.address());
  if (fn) os << "<" << fn->name << ".entry>";
  return os;
}

void abort_with_reason(int reason) {
  if (IsValidAbortReason(reason)) {
    const char* message = GetAbortReason(static_cast<AbortReason>(reason));
    base::OS::PrintError("abort: %s\n", message);
  } else {
    base::OS::PrintError("abort: <unknown reason: %d>\n", reason);
  }
  base::OS::Abort();
  UNREACHABLE();
}

#undef FUNCTION_REFERENCE
#undef FUNCTION_REFERENCE_WITH_ISOLATE
#undef FUNCTION_REFERENCE_WITH_TYPE

}  // namespace internal
}  // namespace v8

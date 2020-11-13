// Copyright 2014 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "src/codegen/external-reference.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-external-refs.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename InType, typename OutType, typename Iterable>
void TestExternalReference_ConvertOp(
    BufferedRawMachineAssemblerTester<int32_t>* m, ExternalReference ref,
    void (*wrapper)(Address), Iterable inputs) {
  constexpr size_t kBufferSize = Max(sizeof(InType), sizeof(OutType));
  uint8_t buffer[kBufferSize] = {0};
  Address buffer_addr = reinterpret_cast<Address>(buffer);

  Node* function = m->ExternalConstant(ref);
  m->CallCFunction(
      function, MachineType::Pointer(),
      std::make_pair(MachineType::Pointer(), m->PointerConstant(buffer)));
  m->Return(m->Int32Constant(4356));

  for (InType input : inputs) {
    WriteUnalignedValue<InType>(buffer_addr, input);

    CHECK_EQ(4356, m->Call());
    OutType output = ReadUnalignedValue<OutType>(buffer_addr);

    WriteUnalignedValue<InType>(buffer_addr, input);
    wrapper(buffer_addr);
    OutType expected_output = ReadUnalignedValue<OutType>(buffer_addr);

    CHECK_EQ(expected_output, output);
  }
}

template <typename InType, typename OutType, typename Iterable>
void TestExternalReference_ConvertOpWithOutputAndReturn(
    BufferedRawMachineAssemblerTester<int32_t>* m, ExternalReference ref,
    int32_t (*wrapper)(Address), Iterable inputs) {
  constexpr size_t kBufferSize = Max(sizeof(InType), sizeof(OutType));
  uint8_t buffer[kBufferSize] = {0};
  Address buffer_addr = reinterpret_cast<Address>(buffer);

  Node* function = m->ExternalConstant(ref);
  m->Return(m->CallCFunction(
      function, MachineType::Int32(),
      std::make_pair(MachineType::Pointer(), m->PointerConstant(buffer))));

  for (InType input : inputs) {
    WriteUnalignedValue<InType>(buffer_addr, input);

    int32_t ret = m->Call();
    OutType output = ReadUnalignedValue<OutType>(buffer_addr);

    WriteUnalignedValue<InType>(buffer_addr, input);
    int32_t expected_ret = wrapper(buffer_addr);
    OutType expected_output = ReadUnalignedValue<OutType>(buffer_addr);

    CHECK_EQ(expected_ret, ret);
    CHECK_EQ(expected_output, output);
  }
}

template <typename InType, typename OutType, typename Iterable>
void TestExternalReference_ConvertOpWithReturn(
    BufferedRawMachineAssemblerTester<OutType>* m, ExternalReference ref,
    OutType (*wrapper)(Address), Iterable inputs) {
  constexpr size_t kBufferSize = sizeof(InType);
  uint8_t buffer[kBufferSize] = {0};
  Address buffer_addr = reinterpret_cast<Address>(buffer);

  Node* function = m->ExternalConstant(ref);
  m->Return(m->CallCFunction(
      function, MachineType::Int32(),
      std::make_pair(MachineType::Pointer(), m->PointerConstant(buffer))));

  for (InType input : inputs) {
    WriteUnalignedValue<InType>(buffer_addr, input);

    OutType ret = m->Call();

    WriteUnalignedValue<InType>(buffer_addr, input);
    OutType expected_ret = wrapper(buffer_addr);

    CHECK_EQ(expected_ret, ret);
  }
}

template <typename Type>
bool isnan(Type value) {
  return false;
}
template <>
bool isnan<float>(float value) {
  return std::isnan(value);
}
template <>
bool isnan<double>(double value) {
  return std::isnan(value);
}

template <typename Type, typename Iterable>
void TestExternalReference_UnOp(BufferedRawMachineAssemblerTester<int32_t>* m,
                                ExternalReference ref, void (*wrapper)(Address),
                                Iterable inputs) {
  constexpr size_t kBufferSize = sizeof(Type);
  uint8_t buffer[kBufferSize] = {0};
  Address buffer_addr = reinterpret_cast<Address>(buffer);

  Node* function = m->ExternalConstant(ref);
  m->CallCFunction(
      function, MachineType::Int32(),
      std::make_pair(MachineType::Pointer(), m->PointerConstant(buffer)));
  m->Return(m->Int32Constant(4356));

  for (Type input : inputs) {
    WriteUnalignedValue<Type>(buffer_addr, input);
    CHECK_EQ(4356, m->Call());
    Type output = ReadUnalignedValue<Type>(buffer_addr);

    WriteUnalignedValue<Type>(buffer_addr, input);
    wrapper(buffer_addr);
    Type expected_output = ReadUnalignedValue<Type>(buffer_addr);

    if (isnan(expected_output) && isnan(output)) continue;
    CHECK_EQ(expected_output, output);
  }
}

template <typename Type, typename Iterable>
void TestExternalReference_BinOp(BufferedRawMachineAssemblerTester<int32_t>* m,
                                 ExternalReference ref,
                                 void (*wrapper)(Address), Iterable inputs) {
  constexpr size_t kBufferSize = 2 * sizeof(Type);
  uint8_t buffer[kBufferSize] = {0};
  Address buffer_addr = reinterpret_cast<Address>(buffer);

  Node* function = m->ExternalConstant(ref);
  m->CallCFunction(
      function, MachineType::Int32(),
      std::make_pair(MachineType::Pointer(), m->PointerConstant(buffer)));
  m->Return(m->Int32Constant(4356));

  for (Type input1 : inputs) {
    for (Type input2 : inputs) {
      WriteUnalignedValue<Type>(buffer_addr, input1);
      WriteUnalignedValue<Type>(buffer_addr + sizeof(Type), input2);
      CHECK_EQ(4356, m->Call());
      Type output = ReadUnalignedValue<Type>(buffer_addr);

      WriteUnalignedValue<Type>(buffer_addr, input1);
      WriteUnalignedValue<Type>(buffer_addr + sizeof(Type), input2);
      wrapper(buffer_addr);
      Type expected_output = ReadUnalignedValue<Type>(buffer_addr);

      if (isnan(expected_output) && isnan(output)) continue;
      CHECK_EQ(expected_output, output);
    }
  }
}

template <typename Type, typename Iterable>
void TestExternalReference_BinOpWithReturn(
    BufferedRawMachineAssemblerTester<int32_t>* m, ExternalReference ref,
    int32_t (*wrapper)(Address), Iterable inputs) {
  constexpr size_t kBufferSize = 2 * sizeof(Type);
  uint8_t buffer[kBufferSize] = {0};
  Address buffer_addr = reinterpret_cast<Address>(buffer);

  Node* function = m->ExternalConstant(ref);
  m->Return(m->CallCFunction(
      function, MachineType::Int32(),
      std::make_pair(MachineType::Pointer(), m->PointerConstant(buffer))));

  for (Type input1 : inputs) {
    for (Type input2 : inputs) {
      WriteUnalignedValue<Type>(buffer_addr, input1);
      WriteUnalignedValue<Type>(buffer_addr + sizeof(Type), input2);
      int32_t ret = m->Call();
      Type output = ReadUnalignedValue<Type>(buffer_addr);

      WriteUnalignedValue<Type>(buffer_addr, input1);
      WriteUnalignedValue<Type>(buffer_addr + sizeof(Type), input2);
      int32_t expected_ret = wrapper(buffer_addr);
      Type expected_output = ReadUnalignedValue<Type>(buffer_addr);

      CHECK_EQ(expected_ret, ret);
      if (isnan(expected_output) && isnan(output)) continue;
      CHECK_EQ(expected_output, output);
    }
  }
}

TEST(RunCallF32Trunc) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_trunc();
  TestExternalReference_UnOp<float>(&m, ref, wasm::f32_trunc_wrapper,
                                    ValueHelper::float32_vector());
}

TEST(RunCallF32Floor) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_floor();
  TestExternalReference_UnOp<float>(&m, ref, wasm::f32_floor_wrapper,
                                    ValueHelper::float32_vector());
}

TEST(RunCallF32Ceil) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_ceil();
  TestExternalReference_UnOp<float>(&m, ref, wasm::f32_ceil_wrapper,
                                    ValueHelper::float32_vector());
}

TEST(RunCallF32RoundTiesEven) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_nearest_int();
  TestExternalReference_UnOp<float>(&m, ref, wasm::f32_nearest_int_wrapper,
                                    ValueHelper::float32_vector());
}

TEST(RunCallF64Trunc) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_trunc();
  TestExternalReference_UnOp<double>(&m, ref, wasm::f64_trunc_wrapper,
                                     ValueHelper::float64_vector());
}

TEST(RunCallF64Floor) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_floor();
  TestExternalReference_UnOp<double>(&m, ref, wasm::f64_floor_wrapper,
                                     ValueHelper::float64_vector());
}

TEST(RunCallF64Ceil) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_ceil();
  TestExternalReference_UnOp<double>(&m, ref, wasm::f64_ceil_wrapper,
                                     ValueHelper::float64_vector());
}

TEST(RunCallF64RoundTiesEven) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_nearest_int();
  TestExternalReference_UnOp<double>(&m, ref, wasm::f64_nearest_int_wrapper,
                                     ValueHelper::float64_vector());
}

TEST(RunCallInt64ToFloat32) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_to_float32();
  TestExternalReference_ConvertOp<int64_t, float>(
      &m, ref, wasm::int64_to_float32_wrapper, ValueHelper::int64_vector());
}

TEST(RunCallUint64ToFloat32) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_to_float32();
  TestExternalReference_ConvertOp<uint64_t, float>(
      &m, ref, wasm::uint64_to_float32_wrapper, ValueHelper::uint64_vector());
}

TEST(RunCallInt64ToFloat64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_to_float64();
  TestExternalReference_ConvertOp<int64_t, double>(
      &m, ref, wasm::int64_to_float64_wrapper, ValueHelper::int64_vector());
}

TEST(RunCallUint64ToFloat64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_to_float64();
  TestExternalReference_ConvertOp<uint64_t, double>(
      &m, ref, wasm::uint64_to_float64_wrapper, ValueHelper::uint64_vector());
}

TEST(RunCallFloat32ToInt64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float32_to_int64();
  TestExternalReference_ConvertOpWithOutputAndReturn<float, int64_t>(
      &m, ref, wasm::float32_to_int64_wrapper, ValueHelper::float32_vector());
}

TEST(RunCallFloat32ToUint64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float32_to_uint64();
  TestExternalReference_ConvertOpWithOutputAndReturn<float, uint64_t>(
      &m, ref, wasm::float32_to_uint64_wrapper, ValueHelper::float32_vector());
}

TEST(RunCallFloat64ToInt64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float64_to_int64();
  TestExternalReference_ConvertOpWithOutputAndReturn<double, int64_t>(
      &m, ref, wasm::float64_to_int64_wrapper, ValueHelper::float64_vector());
}

TEST(RunCallFloat64ToUint64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float64_to_uint64();
  TestExternalReference_ConvertOpWithOutputAndReturn<double, uint64_t>(
      &m, ref, wasm::float64_to_uint64_wrapper, ValueHelper::float64_vector());
}

TEST(RunCallInt64Div) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_div();
  TestExternalReference_BinOpWithReturn<int64_t>(
      &m, ref, wasm::int64_div_wrapper, ValueHelper::int64_vector());
}

TEST(RunCallInt64Mod) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_mod();
  TestExternalReference_BinOpWithReturn<int64_t>(
      &m, ref, wasm::int64_mod_wrapper, ValueHelper::int64_vector());
}

TEST(RunCallUint64Div) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_div();
  TestExternalReference_BinOpWithReturn<uint64_t>(
      &m, ref, wasm::uint64_div_wrapper, ValueHelper::uint64_vector());
}

TEST(RunCallUint64Mod) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_mod();
  TestExternalReference_BinOpWithReturn<uint64_t>(
      &m, ref, wasm::uint64_mod_wrapper, ValueHelper::uint64_vector());
}

TEST(RunCallWord32Ctz) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word32_ctz();
  TestExternalReference_ConvertOpWithReturn<int32_t, uint32_t>(
      &m, ref, wasm::word32_ctz_wrapper, ValueHelper::int32_vector());
}

TEST(RunCallWord64Ctz) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word64_ctz();
  TestExternalReference_ConvertOpWithReturn<int64_t, uint32_t>(
      &m, ref, wasm::word64_ctz_wrapper, ValueHelper::int64_vector());
}

TEST(RunCallWord32Popcnt) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word32_popcnt();
  TestExternalReference_ConvertOpWithReturn<uint32_t, uint32_t>(
      &m, ref, wasm::word32_popcnt_wrapper, ValueHelper::int32_vector());
}

TEST(RunCallWord64Popcnt) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word64_popcnt();
  TestExternalReference_ConvertOpWithReturn<int64_t, uint32_t>(
      &m, ref, wasm::word64_popcnt_wrapper, ValueHelper::int64_vector());
}

TEST(RunCallFloat64Pow) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float64_pow();
  TestExternalReference_BinOp<double>(&m, ref, wasm::float64_pow_wrapper,
                                      ValueHelper::float64_vector());
}

#ifdef V8_ENABLE_FP_PARAMS_IN_C_LINKAGE

template <typename T>
MachineType MachineTypeForCType() {
  UNREACHABLE();
}

template <>
MachineType MachineTypeForCType<int32_t>() {
  return MachineType::Int32();
}

template <>
MachineType MachineTypeForCType<double>() {
  return MachineType::Float64();
}

template <>
MachineType MachineTypeForCType<float>() {
  return MachineType::Float32();
}

#define SIGNATURE_TYPES_END(TYPE, IDX, VALUE) MachineTypeForCType<TYPE>()
#define SIGNATURE_TYPES(TYPE, IDX, VALUE) SIGNATURE_TYPES_END(TYPE, IDX, VALUE),

#define PARAM_PAIRS_END(TYPE, IDX, VALUE) \
  std::make_pair(MachineTypeForCType<TYPE>(), m.Parameter(IDX))

#define PARAM_PAIRS(TYPE, IDX, VALUE) PARAM_PAIRS_END(TYPE, IDX, VALUE),

#define CALL_ARGS_END(TYPE, IDX, VALUE) static_cast<TYPE>(VALUE)
#define CALL_ARGS(TYPE, IDX, VALUE) CALL_ARGS_END(TYPE, IDX, VALUE),

#define COMPARE_ARG_I_END(TYPE, IDX, VALUE) (arg##IDX == VALUE)
#define COMPARE_ARG_I(TYPE, IDX, VALUE) COMPARE_ARG_I_END(TYPE, IDX, VALUE)&&

#define SIGNATURE_TEST(NAME, SIGNATURE, FUNC)                             \
  TEST(NAME) {                                                            \
    RawMachineAssemblerTester<int64_t> m(SIGNATURE(SIGNATURE_TYPES));     \
                                                                          \
    Address func_address = FUNCTION_ADDR(&FUNC);                          \
    ExternalReference::Type dummy_type = ExternalReference::BUILTIN_CALL; \
    ApiFunction func(func_address);                                       \
    ExternalReference ref = ExternalReference::Create(&func, dummy_type); \
                                                                          \
    Node* function = m.ExternalConstant(ref);                             \
    m.Return(m.CallCFunction(function, MachineType::Int64(),              \
                             SIGNATURE(PARAM_PAIRS)));                    \
                                                                          \
    int64_t c = m.Call(SIGNATURE(CALL_ARGS));                             \
    CHECK_EQ(c, 42);                                                      \
  }

#define MIXED_SIGNATURE_SIMPLE(V) \
  V(int32_t, 0, 0)                \
  V(double, 1, 1.5)               \
  V##_END(int32_t, 2, 2)

int64_t test_api_func_simple(int32_t arg0, double arg1, int32_t arg2) {
  CHECK(MIXED_SIGNATURE_SIMPLE(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureSimple, MIXED_SIGNATURE_SIMPLE,
               test_api_func_simple)

#define MIXED_SIGNATURE(V) \
  V(int32_t, 0, 0)         \
  V(double, 1, 1.5)        \
  V(int32_t, 2, 2)         \
  V(double, 3, 3.5)        \
  V(int32_t, 4, 4)         \
  V(double, 5, 5.5)        \
  V(int32_t, 6, 6)         \
  V(double, 7, 7.5)        \
  V(int32_t, 8, 8)         \
  V(double, 9, 9.5)        \
  V##_END(int32_t, 10, 10)

int64_t test_api_func(int32_t arg0, double arg1, int32_t arg2, double arg3,
                      int32_t arg4, double arg5, int32_t arg6, double arg7,
                      int32_t arg8, double arg9, int32_t arg10) {
  CHECK(MIXED_SIGNATURE(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignature, MIXED_SIGNATURE, test_api_func)

#define MIXED_SIGNATURE_DOUBLE_INT(V) \
  V(double, 0, 0.5)                   \
  V(double, 1, 1.5)                   \
  V(double, 2, 2.5)                   \
  V(double, 3, 3.5)                   \
  V(double, 4, 4.5)                   \
  V(double, 5, 5.5)                   \
  V(double, 6, 6.5)                   \
  V(double, 7, 7.5)                   \
  V(double, 8, 8.5)                   \
  V(double, 9, 9.5)                   \
  V(int32_t, 10, 10)                  \
  V(int32_t, 11, 11)                  \
  V(int32_t, 12, 12)                  \
  V(int32_t, 13, 13)                  \
  V(int32_t, 14, 14)                  \
  V(int32_t, 15, 15)                  \
  V(int32_t, 16, 16)                  \
  V(int32_t, 17, 17)                  \
  V(int32_t, 18, 18)                  \
  V##_END(int32_t, 19, 19)

int64_t func_mixed_double_int(double arg0, double arg1, double arg2,
                              double arg3, double arg4, double arg5,
                              double arg6, double arg7, double arg8,
                              double arg9, int32_t arg10, int32_t arg11,
                              int32_t arg12, int32_t arg13, int32_t arg14,
                              int32_t arg15, int32_t arg16, int32_t arg17,
                              int32_t arg18, int32_t arg19) {
  CHECK(MIXED_SIGNATURE_DOUBLE_INT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureDoubleInt, MIXED_SIGNATURE_DOUBLE_INT,
               func_mixed_double_int)

#define MIXED_SIGNATURE_INT_DOUBLE(V) \
  V(int32_t, 0, 0)                    \
  V(int32_t, 1, 1)                    \
  V(int32_t, 2, 2)                    \
  V(int32_t, 3, 3)                    \
  V(int32_t, 4, 4)                    \
  V(int32_t, 5, 5)                    \
  V(int32_t, 6, 6)                    \
  V(int32_t, 7, 7)                    \
  V(int32_t, 8, 8)                    \
  V(int32_t, 9, 9)                    \
  V(double, 10, 10.5)                 \
  V(double, 11, 11.5)                 \
  V(double, 12, 12.5)                 \
  V(double, 13, 13.5)                 \
  V(double, 14, 14.5)                 \
  V(double, 15, 15.5)                 \
  V(double, 16, 16.5)                 \
  V(double, 17, 17.5)                 \
  V(double, 18, 18.5)                 \
  V##_END(double, 19, 19.5)

int64_t func_mixed_int_double(int32_t arg0, int32_t arg1, int32_t arg2,
                              int32_t arg3, int32_t arg4, int32_t arg5,
                              int32_t arg6, int32_t arg7, int32_t arg8,
                              int32_t arg9, double arg10, double arg11,
                              double arg12, double arg13, double arg14,
                              double arg15, double arg16, double arg17,
                              double arg18, double arg19) {
  CHECK(MIXED_SIGNATURE_INT_DOUBLE(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureIntDouble, MIXED_SIGNATURE_INT_DOUBLE,
               func_mixed_int_double)

#define MIXED_SIGNATURE_INT_DOUBLE_ALT(V) \
  V(int32_t, 0, 0)                        \
  V(double, 1, 1.5)                       \
  V(int32_t, 2, 2)                        \
  V(double, 3, 3.5)                       \
  V(int32_t, 4, 4)                        \
  V(double, 5, 5.5)                       \
  V(int32_t, 6, 6)                        \
  V(double, 7, 7.5)                       \
  V(int32_t, 8, 8)                        \
  V(double, 9, 9.5)                       \
  V(int32_t, 10, 10)                      \
  V(double, 11, 11.5)                     \
  V(int32_t, 12, 12)                      \
  V(double, 13, 13.5)                     \
  V(int32_t, 14, 14)                      \
  V(double, 15, 15.5)                     \
  V(int32_t, 16, 16)                      \
  V(double, 17, 17.5)                     \
  V(int32_t, 18, 18)                      \
  V##_END(double, 19, 19.5)

int64_t func_mixed_int_double_alt(int32_t arg0, double arg1, int32_t arg2,
                                  double arg3, int32_t arg4, double arg5,
                                  int32_t arg6, double arg7, int32_t arg8,
                                  double arg9, int32_t arg10, double arg11,
                                  int32_t arg12, double arg13, int32_t arg14,
                                  double arg15, int32_t arg16, double arg17,
                                  int32_t arg18, double arg19) {
  CHECK(MIXED_SIGNATURE_INT_DOUBLE_ALT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureIntDoubleAlt,
               MIXED_SIGNATURE_INT_DOUBLE_ALT, func_mixed_int_double_alt)

#define SIGNATURE_ONLY_DOUBLE(V) \
  V(double, 0, 0.5)              \
  V(double, 1, 1.5)              \
  V(double, 2, 2.5)              \
  V(double, 3, 3.5)              \
  V(double, 4, 4.5)              \
  V(double, 5, 5.5)              \
  V(double, 6, 6.5)              \
  V(double, 7, 7.5)              \
  V(double, 8, 8.5)              \
  V##_END(double, 9, 9.5)

int64_t func_only_double(double arg0, double arg1, double arg2, double arg3,
                         double arg4, double arg5, double arg6, double arg7,
                         double arg8, double arg9) {
  CHECK(SIGNATURE_ONLY_DOUBLE(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithSignatureOnlyDouble, SIGNATURE_ONLY_DOUBLE,
               func_only_double)

#define SIGNATURE_ONLY_DOUBLE_20(V) \
  V(double, 0, 0.5)                 \
  V(double, 1, 1.5)                 \
  V(double, 2, 2.5)                 \
  V(double, 3, 3.5)                 \
  V(double, 4, 4.5)                 \
  V(double, 5, 5.5)                 \
  V(double, 6, 6.5)                 \
  V(double, 7, 7.5)                 \
  V(double, 8, 8.5)                 \
  V(double, 9, 9.5)                 \
  V(double, 10, 10.5)               \
  V(double, 11, 11.5)               \
  V(double, 12, 12.5)               \
  V(double, 13, 13.5)               \
  V(double, 14, 14.5)               \
  V(double, 15, 15.5)               \
  V(double, 16, 16.5)               \
  V(double, 17, 17.5)               \
  V(double, 18, 18.5)               \
  V##_END(double, 19, 19.5)

int64_t func_only_double_20(double arg0, double arg1, double arg2, double arg3,
                            double arg4, double arg5, double arg6, double arg7,
                            double arg8, double arg9, double arg10,
                            double arg11, double arg12, double arg13,
                            double arg14, double arg15, double arg16,
                            double arg17, double arg18, double arg19) {
  CHECK(SIGNATURE_ONLY_DOUBLE_20(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithSignatureOnlyDouble20, SIGNATURE_ONLY_DOUBLE_20,
               func_only_double_20)

#define SIGNATURE_ONLY_INT(V) \
  V(int32_t, 0, 0)            \
  V(int32_t, 1, 1)            \
  V(int32_t, 2, 2)            \
  V(int32_t, 3, 3)            \
  V(int32_t, 4, 4)            \
  V(int32_t, 5, 5)            \
  V(int32_t, 6, 6)            \
  V(int32_t, 7, 7)            \
  V(int32_t, 8, 8)            \
  V##_END(int32_t, 9, 9)

int64_t func_only_int(int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3,
                      int32_t arg4, int32_t arg5, int32_t arg6, int32_t arg7,
                      int32_t arg8, int32_t arg9) {
  CHECK(SIGNATURE_ONLY_INT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithSignatureOnlyInt, SIGNATURE_ONLY_INT, func_only_int)

#define SIGNATURE_ONLY_INT_20(V) \
  V(int32_t, 0, 0)               \
  V(int32_t, 1, 1)               \
  V(int32_t, 2, 2)               \
  V(int32_t, 3, 3)               \
  V(int32_t, 4, 4)               \
  V(int32_t, 5, 5)               \
  V(int32_t, 6, 6)               \
  V(int32_t, 7, 7)               \
  V(int32_t, 8, 8)               \
  V(int32_t, 9, 9)               \
  V(int32_t, 10, 10)             \
  V(int32_t, 11, 11)             \
  V(int32_t, 12, 12)             \
  V(int32_t, 13, 13)             \
  V(int32_t, 14, 14)             \
  V(int32_t, 15, 15)             \
  V(int32_t, 16, 16)             \
  V(int32_t, 17, 17)             \
  V(int32_t, 18, 18)             \
  V##_END(int32_t, 19, 19)

int64_t func_only_int_20(int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3,
                         int32_t arg4, int32_t arg5, int32_t arg6, int32_t arg7,
                         int32_t arg8, int32_t arg9, int32_t arg10,
                         int32_t arg11, int32_t arg12, int32_t arg13,
                         int32_t arg14, int32_t arg15, int32_t arg16,
                         int32_t arg17, int32_t arg18, int32_t arg19) {
  CHECK(SIGNATURE_ONLY_INT_20(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithSignatureOnlyInt20, SIGNATURE_ONLY_INT_20,
               func_only_int_20)

#define MIXED_SIGNATURE_SIMPLE_FLOAT(V) \
  V(int32_t, 0, 0)                      \
  V(float, 1, 1.5)                      \
  V##_END(int32_t, 2, 2)

int64_t test_api_func_simple_float(int32_t arg0, float arg1, int32_t arg2) {
  CHECK(MIXED_SIGNATURE_SIMPLE_FLOAT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureSimpleFloat,
               MIXED_SIGNATURE_SIMPLE_FLOAT, test_api_func_simple_float)

#define MIXED_SIGNATURE_FLOAT(V) \
  V(int32_t, 0, 0)               \
  V(float, 1, 1.5)               \
  V(int32_t, 2, 2)               \
  V(float, 3, 3.5)               \
  V(int32_t, 4, 4)               \
  V(float, 5, 5.5)               \
  V(int32_t, 6, 6)               \
  V(float, 7, 7.5)               \
  V(int32_t, 8, 8)               \
  V(float, 9, 9.5)               \
  V##_END(int32_t, 10, 10)

int64_t test_api_func_float(int32_t arg0, float arg1, int32_t arg2, float arg3,
                            int32_t arg4, float arg5, int32_t arg6, float arg7,
                            int32_t arg8, float arg9, int32_t arg10) {
  CHECK(MIXED_SIGNATURE_FLOAT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureFloat, MIXED_SIGNATURE_FLOAT,
               test_api_func_float)

#define MIXED_SIGNATURE_INT_FLOAT_ALT(V) \
  V(int32_t, 0, 0)                       \
  V(float, 1, 1.5)                       \
  V(int32_t, 2, 2)                       \
  V(float, 3, 3.5)                       \
  V(int32_t, 4, 4)                       \
  V(float, 5, 5.5)                       \
  V(int32_t, 6, 6)                       \
  V(float, 7, 7.5)                       \
  V(int32_t, 8, 8)                       \
  V(float, 9, 9.5)                       \
  V(int32_t, 10, 10)                     \
  V(float, 11, 11.5)                     \
  V(int32_t, 12, 12)                     \
  V(float, 13, 13.5)                     \
  V(int32_t, 14, 14)                     \
  V(float, 15, 15.5)                     \
  V(int32_t, 16, 16)                     \
  V(float, 17, 17.5)                     \
  V(int32_t, 18, 18)                     \
  V##_END(float, 19, 19.5)

int64_t func_mixed_int_float_alt(int32_t arg0, float arg1, int32_t arg2,
                                 float arg3, int32_t arg4, float arg5,
                                 int32_t arg6, float arg7, int32_t arg8,
                                 float arg9, int32_t arg10, float arg11,
                                 int32_t arg12, float arg13, int32_t arg14,
                                 float arg15, int32_t arg16, float arg17,
                                 int32_t arg18, float arg19) {
  CHECK(MIXED_SIGNATURE_INT_FLOAT_ALT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureIntFloatAlt,
               MIXED_SIGNATURE_INT_FLOAT_ALT, func_mixed_int_float_alt)

#define SIGNATURE_ONLY_FLOAT_20(V) \
  V(float, 0, 0.5)                 \
  V(float, 1, 1.5)                 \
  V(float, 2, 2.5)                 \
  V(float, 3, 3.5)                 \
  V(float, 4, 4.5)                 \
  V(float, 5, 5.5)                 \
  V(float, 6, 6.5)                 \
  V(float, 7, 7.5)                 \
  V(float, 8, 8.5)                 \
  V(float, 9, 9.5)                 \
  V(float, 10, 10.5)               \
  V(float, 11, 11.5)               \
  V(float, 12, 12.5)               \
  V(float, 13, 13.5)               \
  V(float, 14, 14.5)               \
  V(float, 15, 15.5)               \
  V(float, 16, 16.5)               \
  V(float, 17, 17.5)               \
  V(float, 18, 18.5)               \
  V##_END(float, 19, 19.5)

int64_t func_only_float_20(float arg0, float arg1, float arg2, float arg3,
                           float arg4, float arg5, float arg6, float arg7,
                           float arg8, float arg9, float arg10, float arg11,
                           float arg12, float arg13, float arg14, float arg15,
                           float arg16, float arg17, float arg18, float arg19) {
  CHECK(SIGNATURE_ONLY_FLOAT_20(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithSignatureOnlyFloat20, SIGNATURE_ONLY_FLOAT_20,
               func_only_float_20)

#define MIXED_SIGNATURE_FLOAT_INT(V) \
  V(float, 0, 0.5)                   \
  V(float, 1, 1.5)                   \
  V(float, 2, 2.5)                   \
  V(float, 3, 3.5)                   \
  V(float, 4, 4.5)                   \
  V(float, 5, 5.5)                   \
  V(float, 6, 6.5)                   \
  V(float, 7, 7.5)                   \
  V(float, 8, 8.5)                   \
  V(float, 9, 9.5)                   \
  V(int32_t, 10, 10)                 \
  V(int32_t, 11, 11)                 \
  V(int32_t, 12, 12)                 \
  V(int32_t, 13, 13)                 \
  V(int32_t, 14, 14)                 \
  V(int32_t, 15, 15)                 \
  V(int32_t, 16, 16)                 \
  V(int32_t, 17, 17)                 \
  V(int32_t, 18, 18)                 \
  V##_END(int32_t, 19, 19)

int64_t func_mixed_float_int(float arg0, float arg1, float arg2, float arg3,
                             float arg4, float arg5, float arg6, float arg7,
                             float arg8, float arg9, int32_t arg10,
                             int32_t arg11, int32_t arg12, int32_t arg13,
                             int32_t arg14, int32_t arg15, int32_t arg16,
                             int32_t arg17, int32_t arg18, int32_t arg19) {
  CHECK(MIXED_SIGNATURE_FLOAT_INT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureFloatInt, MIXED_SIGNATURE_FLOAT_INT,
               func_mixed_float_int)

#define MIXED_SIGNATURE_INT_FLOAT(V) \
  V(int32_t, 0, 0)                   \
  V(int32_t, 1, 1)                   \
  V(int32_t, 2, 2)                   \
  V(int32_t, 3, 3)                   \
  V(int32_t, 4, 4)                   \
  V(int32_t, 5, 5)                   \
  V(int32_t, 6, 6)                   \
  V(int32_t, 7, 7)                   \
  V(int32_t, 8, 8)                   \
  V(int32_t, 9, 9)                   \
  V(float, 10, 10.5)                 \
  V(float, 11, 11.5)                 \
  V(float, 12, 12.5)                 \
  V(float, 13, 13.5)                 \
  V(float, 14, 14.5)                 \
  V(float, 15, 15.5)                 \
  V(float, 16, 16.5)                 \
  V(float, 17, 17.5)                 \
  V(float, 18, 18.5)                 \
  V##_END(float, 19, 19.5)

int64_t func_mixed_int_float(int32_t arg0, int32_t arg1, int32_t arg2,
                             int32_t arg3, int32_t arg4, int32_t arg5,
                             int32_t arg6, int32_t arg7, int32_t arg8,
                             int32_t arg9, float arg10, float arg11,
                             float arg12, float arg13, float arg14, float arg15,
                             float arg16, float arg17, float arg18,
                             float arg19) {
  CHECK(MIXED_SIGNATURE_INT_FLOAT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureIntFloat, MIXED_SIGNATURE_INT_FLOAT,
               func_mixed_int_float)

#define MIXED_SIGNATURE_FLOAT_DOUBLE(V) \
  V(float, 0, 0.5)                      \
  V(float, 1, 1.5)                      \
  V(float, 2, 2.5)                      \
  V(float, 3, 3.5)                      \
  V(float, 4, 4.5)                      \
  V(float, 5, 5.5)                      \
  V(float, 6, 6.5)                      \
  V(float, 7, 7.5)                      \
  V(float, 8, 8.5)                      \
  V(float, 9, 9.5)                      \
  V(double, 10, 10.7)                   \
  V(double, 11, 11.7)                   \
  V(double, 12, 12.7)                   \
  V(double, 13, 13.7)                   \
  V(double, 14, 14.7)                   \
  V(double, 15, 15.7)                   \
  V(double, 16, 16.7)                   \
  V(double, 17, 17.7)                   \
  V(double, 18, 18.7)                   \
  V##_END(double, 19, 19.7)

int64_t func_mixed_float_double(float arg0, float arg1, float arg2, float arg3,
                                float arg4, float arg5, float arg6, float arg7,
                                float arg8, float arg9, double arg10,
                                double arg11, double arg12, double arg13,
                                double arg14, double arg15, double arg16,
                                double arg17, double arg18, double arg19) {
  CHECK(MIXED_SIGNATURE_FLOAT_DOUBLE(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureFloatDouble,
               MIXED_SIGNATURE_FLOAT_DOUBLE, func_mixed_float_double)

#define MIXED_SIGNATURE_DOUBLE_FLOAT(V) \
  V(double, 0, 0.7)                     \
  V(double, 1, 1.7)                     \
  V(double, 2, 2.7)                     \
  V(double, 3, 3.7)                     \
  V(double, 4, 4.7)                     \
  V(double, 5, 5.7)                     \
  V(double, 6, 6.7)                     \
  V(double, 7, 7.7)                     \
  V(double, 8, 8.7)                     \
  V(double, 9, 9.7)                     \
  V(float, 10, 10.5)                    \
  V(float, 11, 11.5)                    \
  V(float, 12, 12.5)                    \
  V(float, 13, 13.5)                    \
  V(float, 14, 14.5)                    \
  V(float, 15, 15.5)                    \
  V(float, 16, 16.5)                    \
  V(float, 17, 17.5)                    \
  V(float, 18, 18.5)                    \
  V##_END(float, 19, 19.5)

int64_t func_mixed_double_float(double arg0, double arg1, double arg2,
                                double arg3, double arg4, double arg5,
                                double arg6, double arg7, double arg8,
                                double arg9, float arg10, float arg11,
                                float arg12, float arg13, float arg14,
                                float arg15, float arg16, float arg17,
                                float arg18, float arg19) {
  CHECK(MIXED_SIGNATURE_DOUBLE_FLOAT(COMPARE_ARG_I));
  return 42;
}

SIGNATURE_TEST(RunCallWithMixedSignatureDoubleFloat,
               MIXED_SIGNATURE_DOUBLE_FLOAT, func_mixed_double_float)

#endif  // V8_ENABLE_FP_PARAMS_IN_C_LINKAGE

}  // namespace compiler
}  // namespace internal
}  // namespace v8

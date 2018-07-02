// Copyright 2014 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "src/objects-inl.h"
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
  m->CallCFunction1(MachineType::Pointer(), MachineType::Pointer(), function,
                    m->PointerConstant(buffer));
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
  m->Return(m->CallCFunction1(MachineType::Int32(), MachineType::Pointer(),
                              function, m->PointerConstant(buffer)));

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
  m->Return(m->CallCFunction1(MachineType::Int32(), MachineType::Pointer(),
                              function, m->PointerConstant(buffer)));

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
  m->CallCFunction1(MachineType::Int32(), MachineType::Pointer(), function,
                    m->PointerConstant(buffer));
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
  m->CallCFunction1(MachineType::Int32(), MachineType::Pointer(), function,
                    m->PointerConstant(buffer));
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
  m->Return(m->CallCFunction1(MachineType::Int32(), MachineType::Pointer(),
                              function, m->PointerConstant(buffer)));

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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "src/wasm/wasm-external-refs.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename P>
void TestExternalReference(BufferedRawMachineAssemblerTester<int32_t>* m,
                           ExternalReference ref, void (*comparison)(P*),
                           P param) {
  P comparison_param = param;

  Node* function = m->ExternalConstant(ref);
  m->CallCFunction1(MachineType::Pointer(), MachineType::Pointer(), function,
                    m->PointerConstant(&param));
  m->Return(m->Int32Constant(4356));

  m->Call();
  comparison(&comparison_param);

  CHECK_EQ(comparison_param, param);
}

template <typename P1, typename P2>
void TestExternalReference(BufferedRawMachineAssemblerTester<int32_t>* m,
                           ExternalReference ref, void (*comparison)(P1*, P2*),
                           P1 param1, P2 param2) {
  P1 comparison_param1 = param1;
  P2 comparison_param2 = param2;

  Node* function = m->ExternalConstant(ref);
  m->CallCFunction2(MachineType::Pointer(), MachineType::Pointer(),
                    MachineType::Pointer(), function,
                    m->PointerConstant(&param1), m->PointerConstant(&param2));
  m->Return(m->Int32Constant(4356));

  m->Call();
  comparison(&comparison_param1, &comparison_param2);

  CHECK_EQ(comparison_param1, param1);
  CHECK_EQ(comparison_param2, param2);
}

template <typename R, typename P>
void TestExternalReference(BufferedRawMachineAssemblerTester<R>* m,
                           ExternalReference ref, R (*comparison)(P*),
                           P param) {
  P comparison_param = param;

  Node* function = m->ExternalConstant(ref);
  m->Return(m->CallCFunction1(MachineType::Pointer(), MachineType::Pointer(),
                              function, m->PointerConstant(&param)));

  CHECK_EQ(comparison(&comparison_param), m->Call());

  CHECK_EQ(comparison_param, param);
}

template <typename R, typename P1, typename P2>
void TestExternalReference(BufferedRawMachineAssemblerTester<R>* m,
                           ExternalReference ref, R (*comparison)(P1*, P2*),
                           P1 param1, P2 param2) {
  P1 comparison_param1 = param1;
  P2 comparison_param2 = param2;

  Node* function = m->ExternalConstant(ref);
  m->Return(m->CallCFunction2(
      MachineType::Pointer(), MachineType::Pointer(), MachineType::Pointer(),
      function, m->PointerConstant(&param1), m->PointerConstant(&param2)));

  CHECK_EQ(comparison(&comparison_param1, &comparison_param2), m->Call());

  CHECK_EQ(comparison_param1, param1);
  CHECK_EQ(comparison_param2, param2);
}

TEST(RunCallF32Trunc) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_trunc(m.isolate());
  TestExternalReference(&m, ref, wasm::f32_trunc_wrapper, 1.25f);
}

TEST(RunCallF32Floor) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_floor(m.isolate());
  TestExternalReference(&m, ref, wasm::f32_floor_wrapper, 1.25f);
}

TEST(RunCallF32Ceil) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_ceil(m.isolate());
  TestExternalReference(&m, ref, wasm::f32_ceil_wrapper, 1.25f);
}

TEST(RunCallF32RoundTiesEven) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_nearest_int(m.isolate());
  TestExternalReference(&m, ref, wasm::f32_nearest_int_wrapper, 1.25f);
}

TEST(RunCallF64Trunc) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_trunc(m.isolate());
  TestExternalReference(&m, ref, wasm::f64_trunc_wrapper, 1.25);
}

TEST(RunCallF64Floor) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_floor(m.isolate());
  TestExternalReference(&m, ref, wasm::f64_floor_wrapper, 1.25);
}

TEST(RunCallF64Ceil) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_ceil(m.isolate());
  TestExternalReference(&m, ref, wasm::f64_ceil_wrapper, 1.25);
}

TEST(RunCallF64RoundTiesEven) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_nearest_int(m.isolate());
  TestExternalReference(&m, ref, wasm::f64_nearest_int_wrapper, 1.25);
}

TEST(RunCallInt64ToFloat32) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_to_float32(m.isolate());
  TestExternalReference(&m, ref, wasm::int64_to_float32_wrapper, int64_t(-2124),
                        1.25f);
}

TEST(RunCallUint64ToFloat32) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_uint64_to_float32(m.isolate());
  TestExternalReference(&m, ref, wasm::uint64_to_float32_wrapper,
                        uint64_t(2124), 1.25f);
}

TEST(RunCallInt64ToFloat64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_to_float64(m.isolate());
  TestExternalReference(&m, ref, wasm::int64_to_float64_wrapper, int64_t(2124),
                        1.25);
}

TEST(RunCallUint64ToFloat64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_uint64_to_float64(m.isolate());
  TestExternalReference(&m, ref, wasm::uint64_to_float64_wrapper,
                        uint64_t(2124), 1.25);
}

TEST(RunCallFloat32ToInt64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float32_to_int64(m.isolate());
  TestExternalReference(&m, ref, wasm::float32_to_int64_wrapper, 1.25f,
                        int64_t(2124));
}

TEST(RunCallFloat32ToUint64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_float32_to_uint64(m.isolate());
  TestExternalReference(&m, ref, wasm::float32_to_uint64_wrapper, 1.25f,
                        uint64_t(2124));
}

TEST(RunCallFloat64ToInt64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float64_to_int64(m.isolate());
  TestExternalReference(&m, ref, wasm::float64_to_int64_wrapper, 1.25,
                        int64_t(2124));
}

TEST(RunCallFloat64ToUint64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_float64_to_uint64(m.isolate());
  TestExternalReference(&m, ref, wasm::float64_to_uint64_wrapper, 1.25,
                        uint64_t(2124));
}

TEST(RunCallInt64Div) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_div(m.isolate());
  TestExternalReference(&m, ref, wasm::int64_div_wrapper, int64_t(1774),
                        int64_t(21));
}

TEST(RunCallInt64Mod) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_mod(m.isolate());
  TestExternalReference(&m, ref, wasm::int64_mod_wrapper, int64_t(1774),
                        int64_t(21));
}

TEST(RunCallUint64Div) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_div(m.isolate());
  TestExternalReference(&m, ref, wasm::uint64_div_wrapper, uint64_t(1774),
                        uint64_t(21));
}

TEST(RunCallUint64Mod) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_mod(m.isolate());
  TestExternalReference(&m, ref, wasm::uint64_mod_wrapper, uint64_t(1774),
                        uint64_t(21));
}

TEST(RunCallWord32Ctz) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word32_ctz(m.isolate());
  TestExternalReference(&m, ref, wasm::word32_ctz_wrapper, uint32_t(1774));
}

TEST(RunCallWord64Ctz) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word64_ctz(m.isolate());
  TestExternalReference(&m, ref, wasm::word64_ctz_wrapper, uint64_t(1774));
}

TEST(RunCallWord32Popcnt) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word32_popcnt(m.isolate());
  TestExternalReference(&m, ref, wasm::word32_popcnt_wrapper, uint32_t(1774));
}

TEST(RunCallWord64Popcnt) {
  BufferedRawMachineAssemblerTester<uint32_t> m;
  ExternalReference ref = ExternalReference::wasm_word64_popcnt(m.isolate());
  TestExternalReference(&m, ref, wasm::word64_popcnt_wrapper, uint64_t(1774));
}

TEST(RunCallFloat64Pow) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float64_pow(m.isolate());
  TestExternalReference(&m, ref, wasm::float64_pow_wrapper, 1.5, 1.5);
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8

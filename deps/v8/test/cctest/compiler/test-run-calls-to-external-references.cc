// Copyright 2014 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename T>
void TestExternalReferenceRoundingFunction(
    BufferedRawMachineAssemblerTester<int32_t>* m, ExternalReference ref,
    T (*comparison)(T)) {
  T parameter;

  Node* function = m->ExternalConstant(ref);
  m->CallCFunction1(MachineType::Pointer(), MachineType::Pointer(), function,
                    m->PointerConstant(&parameter));
  m->Return(m->Int32Constant(4356));
  FOR_FLOAT64_INPUTS(i) {
    parameter = *i;
    m->Call();
    CHECK_DOUBLE_EQ(comparison(*i), parameter);
  }
}

TEST(RunCallF32Trunc) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_trunc(m.isolate());
  TestExternalReferenceRoundingFunction<float>(&m, ref, truncf);
}

TEST(RunCallF32Floor) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_floor(m.isolate());
  TestExternalReferenceRoundingFunction<float>(&m, ref, floorf);
}

TEST(RunCallF32Ceil) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_ceil(m.isolate());
  TestExternalReferenceRoundingFunction<float>(&m, ref, ceilf);
}

TEST(RunCallF32RoundTiesEven) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f32_nearest_int(m.isolate());
  TestExternalReferenceRoundingFunction<float>(&m, ref, nearbyintf);
}

TEST(RunCallF64Trunc) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_trunc(m.isolate());
  TestExternalReferenceRoundingFunction<double>(&m, ref, trunc);
}

TEST(RunCallF64Floor) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_floor(m.isolate());
  TestExternalReferenceRoundingFunction<double>(&m, ref, floor);
}

TEST(RunCallF64Ceil) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_ceil(m.isolate());
  TestExternalReferenceRoundingFunction<double>(&m, ref, ceil);
}

TEST(RunCallF64RoundTiesEven) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_f64_nearest_int(m.isolate());
  TestExternalReferenceRoundingFunction<double>(&m, ref, nearbyint);
}

TEST(RunCallInt64ToFloat32) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_to_float32(m.isolate());

  int64_t input;
  float output;

  Node* function = m.ExternalConstant(ref);
  m.CallCFunction2(MachineType::Pointer(), MachineType::Pointer(),
                   MachineType::Pointer(), function, m.PointerConstant(&input),
                   m.PointerConstant(&output));
  m.Return(m.Int32Constant(4356));
  FOR_INT64_INPUTS(i) {
    input = *i;
    m.Call();
    CHECK_FLOAT_EQ(static_cast<float>(*i), output);
  }
}

TEST(RunCallUint64ToFloat32) {
  struct {
    uint64_t input;
    uint32_t expected;
  } values[] = {{0x0, 0x0},
                {0x1, 0x3f800000},
                {0xffffffff, 0x4f800000},
                {0x1b09788b, 0x4dd84bc4},
                {0x4c5fce8, 0x4c98bf9d},
                {0xcc0de5bf, 0x4f4c0de6},
                {0x2, 0x40000000},
                {0x3, 0x40400000},
                {0x4, 0x40800000},
                {0x5, 0x40a00000},
                {0x8, 0x41000000},
                {0x9, 0x41100000},
                {0xffffffffffffffff, 0x5f800000},
                {0xfffffffffffffffe, 0x5f800000},
                {0xfffffffffffffffd, 0x5f800000},
                {0x0, 0x0},
                {0x100000000, 0x4f800000},
                {0xffffffff00000000, 0x5f800000},
                {0x1b09788b00000000, 0x5dd84bc4},
                {0x4c5fce800000000, 0x5c98bf9d},
                {0xcc0de5bf00000000, 0x5f4c0de6},
                {0x200000000, 0x50000000},
                {0x300000000, 0x50400000},
                {0x400000000, 0x50800000},
                {0x500000000, 0x50a00000},
                {0x800000000, 0x51000000},
                {0x900000000, 0x51100000},
                {0x273a798e187937a3, 0x5e1ce9e6},
                {0xece3af835495a16b, 0x5f6ce3b0},
                {0xb668ecc11223344, 0x5d3668ed},
                {0x9e, 0x431e0000},
                {0x43, 0x42860000},
                {0xaf73, 0x472f7300},
                {0x116b, 0x458b5800},
                {0x658ecc, 0x4acb1d98},
                {0x2b3b4c, 0x4a2ced30},
                {0x88776655, 0x4f087766},
                {0x70000000, 0x4ee00000},
                {0x7200000, 0x4ce40000},
                {0x7fffffff, 0x4f000000},
                {0x56123761, 0x4eac246f},
                {0x7fffff00, 0x4efffffe},
                {0x761c4761eeeeeeee, 0x5eec388f},
                {0x80000000eeeeeeee, 0x5f000000},
                {0x88888888dddddddd, 0x5f088889},
                {0xa0000000dddddddd, 0x5f200000},
                {0xddddddddaaaaaaaa, 0x5f5dddde},
                {0xe0000000aaaaaaaa, 0x5f600000},
                {0xeeeeeeeeeeeeeeee, 0x5f6eeeef},
                {0xfffffffdeeeeeeee, 0x5f800000},
                {0xf0000000dddddddd, 0x5f700000},
                {0x7fffffdddddddd, 0x5b000000},
                {0x3fffffaaaaaaaa, 0x5a7fffff},
                {0x1fffffaaaaaaaa, 0x59fffffd},
                {0xfffff, 0x497ffff0},
                {0x7ffff, 0x48ffffe0},
                {0x3ffff, 0x487fffc0},
                {0x1ffff, 0x47ffff80},
                {0xffff, 0x477fff00},
                {0x7fff, 0x46fffe00},
                {0x3fff, 0x467ffc00},
                {0x1fff, 0x45fff800},
                {0xfff, 0x457ff000},
                {0x7ff, 0x44ffe000},
                {0x3ff, 0x447fc000},
                {0x1ff, 0x43ff8000},
                {0x3fffffffffff, 0x56800000},
                {0x1fffffffffff, 0x56000000},
                {0xfffffffffff, 0x55800000},
                {0x7ffffffffff, 0x55000000},
                {0x3ffffffffff, 0x54800000},
                {0x1ffffffffff, 0x54000000},
                {0x8000008000000000, 0x5f000000},
                {0x8000008000000001, 0x5f000001},
                {0x8000008000000002, 0x5f000001},
                {0x8000008000000004, 0x5f000001},
                {0x8000008000000008, 0x5f000001},
                {0x8000008000000010, 0x5f000001},
                {0x8000008000000020, 0x5f000001},
                {0x8000009000000000, 0x5f000001},
                {0x800000a000000000, 0x5f000001},
                {0x8000008000100000, 0x5f000001},
                {0x8000000000000400, 0x5f000000},
                {0x8000000000000401, 0x5f000000}};

  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_uint64_to_float32(m.isolate());

  uint64_t input;
  float output;

  Node* function = m.ExternalConstant(ref);
  m.CallCFunction2(MachineType::Pointer(), MachineType::Pointer(),
                   MachineType::Pointer(), function, m.PointerConstant(&input),
                   m.PointerConstant(&output));
  m.Return(m.Int32Constant(4356));

  for (size_t i = 0; i < arraysize(values); i++) {
    input = values[i].input;
    m.Call();
    CHECK_EQ(values[i].expected, bit_cast<uint32_t>(output));
  }
}

TEST(RunCallInt64ToFloat64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_to_float64(m.isolate());

  int64_t input;
  double output;

  Node* function = m.ExternalConstant(ref);
  m.CallCFunction2(MachineType::Pointer(), MachineType::Pointer(),
                   MachineType::Pointer(), function, m.PointerConstant(&input),
                   m.PointerConstant(&output));
  m.Return(m.Int32Constant(4356));
  FOR_INT64_INPUTS(i) {
    input = *i;
    m.Call();
    CHECK_DOUBLE_EQ(static_cast<double>(*i), output);
  }
}

TEST(RunCallUint64ToFloat64) {
  struct {
    uint64_t input;
    uint64_t expected;
  } values[] = {{0x0, 0x0},
                {0x1, 0x3ff0000000000000},
                {0xffffffff, 0x41efffffffe00000},
                {0x1b09788b, 0x41bb09788b000000},
                {0x4c5fce8, 0x419317f3a0000000},
                {0xcc0de5bf, 0x41e981bcb7e00000},
                {0x2, 0x4000000000000000},
                {0x3, 0x4008000000000000},
                {0x4, 0x4010000000000000},
                {0x5, 0x4014000000000000},
                {0x8, 0x4020000000000000},
                {0x9, 0x4022000000000000},
                {0xffffffffffffffff, 0x43f0000000000000},
                {0xfffffffffffffffe, 0x43f0000000000000},
                {0xfffffffffffffffd, 0x43f0000000000000},
                {0x100000000, 0x41f0000000000000},
                {0xffffffff00000000, 0x43efffffffe00000},
                {0x1b09788b00000000, 0x43bb09788b000000},
                {0x4c5fce800000000, 0x439317f3a0000000},
                {0xcc0de5bf00000000, 0x43e981bcb7e00000},
                {0x200000000, 0x4200000000000000},
                {0x300000000, 0x4208000000000000},
                {0x400000000, 0x4210000000000000},
                {0x500000000, 0x4214000000000000},
                {0x800000000, 0x4220000000000000},
                {0x900000000, 0x4222000000000000},
                {0x273a798e187937a3, 0x43c39d3cc70c3c9c},
                {0xece3af835495a16b, 0x43ed9c75f06a92b4},
                {0xb668ecc11223344, 0x43a6cd1d98224467},
                {0x9e, 0x4063c00000000000},
                {0x43, 0x4050c00000000000},
                {0xaf73, 0x40e5ee6000000000},
                {0x116b, 0x40b16b0000000000},
                {0x658ecc, 0x415963b300000000},
                {0x2b3b4c, 0x41459da600000000},
                {0x88776655, 0x41e10eeccaa00000},
                {0x70000000, 0x41dc000000000000},
                {0x7200000, 0x419c800000000000},
                {0x7fffffff, 0x41dfffffffc00000},
                {0x56123761, 0x41d5848dd8400000},
                {0x7fffff00, 0x41dfffffc0000000},
                {0x761c4761eeeeeeee, 0x43dd8711d87bbbbc},
                {0x80000000eeeeeeee, 0x43e00000001dddde},
                {0x88888888dddddddd, 0x43e11111111bbbbc},
                {0xa0000000dddddddd, 0x43e40000001bbbbc},
                {0xddddddddaaaaaaaa, 0x43ebbbbbbbb55555},
                {0xe0000000aaaaaaaa, 0x43ec000000155555},
                {0xeeeeeeeeeeeeeeee, 0x43edddddddddddde},
                {0xfffffffdeeeeeeee, 0x43efffffffbdddde},
                {0xf0000000dddddddd, 0x43ee0000001bbbbc},
                {0x7fffffdddddddd, 0x435ffffff7777777},
                {0x3fffffaaaaaaaa, 0x434fffffd5555555},
                {0x1fffffaaaaaaaa, 0x433fffffaaaaaaaa},
                {0xfffff, 0x412ffffe00000000},
                {0x7ffff, 0x411ffffc00000000},
                {0x3ffff, 0x410ffff800000000},
                {0x1ffff, 0x40fffff000000000},
                {0xffff, 0x40efffe000000000},
                {0x7fff, 0x40dfffc000000000},
                {0x3fff, 0x40cfff8000000000},
                {0x1fff, 0x40bfff0000000000},
                {0xfff, 0x40affe0000000000},
                {0x7ff, 0x409ffc0000000000},
                {0x3ff, 0x408ff80000000000},
                {0x1ff, 0x407ff00000000000},
                {0x3fffffffffff, 0x42cfffffffffff80},
                {0x1fffffffffff, 0x42bfffffffffff00},
                {0xfffffffffff, 0x42affffffffffe00},
                {0x7ffffffffff, 0x429ffffffffffc00},
                {0x3ffffffffff, 0x428ffffffffff800},
                {0x1ffffffffff, 0x427ffffffffff000},
                {0x8000008000000000, 0x43e0000010000000},
                {0x8000008000000001, 0x43e0000010000000},
                {0x8000000000000400, 0x43e0000000000000},
                {0x8000000000000401, 0x43e0000000000001},
                {0x8000000000000402, 0x43e0000000000001},
                {0x8000000000000404, 0x43e0000000000001},
                {0x8000000000000408, 0x43e0000000000001},
                {0x8000000000000410, 0x43e0000000000001},
                {0x8000000000000420, 0x43e0000000000001},
                {0x8000000000000440, 0x43e0000000000001},
                {0x8000000000000480, 0x43e0000000000001},
                {0x8000000000000500, 0x43e0000000000001},
                {0x8000000000000600, 0x43e0000000000001}};

  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_uint64_to_float64(m.isolate());

  uint64_t input;
  double output;

  Node* function = m.ExternalConstant(ref);
  m.CallCFunction2(MachineType::Pointer(), MachineType::Pointer(),
                   MachineType::Pointer(), function, m.PointerConstant(&input),
                   m.PointerConstant(&output));
  m.Return(m.Int32Constant(4356));

  for (size_t i = 0; i < arraysize(values); i++) {
    input = values[i].input;
    m.Call();
    CHECK_EQ(values[i].expected, bit_cast<uint64_t>(output));
  }
}

TEST(RunCallFloat32ToInt64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float32_to_int64(m.isolate());

  float input;
  int64_t output;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(
      MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
      function, m.PointerConstant(&input), m.PointerConstant(&output)));
  FOR_FLOAT32_INPUTS(i) {
    input = *i;
    if (*i >= static_cast<float>(std::numeric_limits<int64_t>::min()) &&
        *i < static_cast<float>(std::numeric_limits<int64_t>::max())) {
      CHECK_EQ(1, m.Call());
      CHECK_EQ(static_cast<int64_t>(*i), output);
    } else {
      CHECK_EQ(0, m.Call());
    }
  }
}

TEST(RunCallFloat32ToUint64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_float32_to_uint64(m.isolate());

  float input;
  uint64_t output;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(
      MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
      function, m.PointerConstant(&input), m.PointerConstant(&output)));
  FOR_FLOAT32_INPUTS(i) {
    input = *i;
    if (*i > -1.0 &&
        *i < static_cast<float>(std::numeric_limits<uint64_t>::max())) {
      CHECK_EQ(1, m.Call());
      CHECK_EQ(static_cast<uint64_t>(*i), output);
    } else {
      CHECK_EQ(0, m.Call());
    }
  }
}

TEST(RunCallFloat64ToInt64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_float64_to_int64(m.isolate());

  double input;
  int64_t output;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(
      MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
      function, m.PointerConstant(&input), m.PointerConstant(&output)));
  FOR_FLOAT64_INPUTS(i) {
    input = *i;
    if (*i >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
        *i < static_cast<double>(std::numeric_limits<int64_t>::max())) {
      CHECK_EQ(1, m.Call());
      CHECK_EQ(static_cast<int64_t>(*i), output);
    } else {
      CHECK_EQ(0, m.Call());
    }
  }
}

TEST(RunCallFloat64ToUint64) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref =
      ExternalReference::wasm_float64_to_uint64(m.isolate());

  double input;
  uint64_t output;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(
      MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
      function, m.PointerConstant(&input), m.PointerConstant(&output)));
  FOR_FLOAT64_INPUTS(i) {
    input = *i;
    if (*i > -1.0 &&
        *i < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
      CHECK_EQ(1, m.Call());
      CHECK_EQ(static_cast<uint64_t>(*i), output);
    } else {
      CHECK_EQ(0, m.Call());
    }
  }
}

TEST(RunCallInt64Div) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_div(m.isolate());

  int64_t dst;
  int64_t src;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(MachineType::Int32(), MachineType::Pointer(),
                            MachineType::Pointer(), function,
                            m.PointerConstant(&dst), m.PointerConstant(&src)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) {
      dst = *i;
      src = *j;
      if (src == 0) {
        CHECK_EQ(0, m.Call());
      } else if (src == -1 && dst == std::numeric_limits<int64_t>::min()) {
        CHECK_EQ(-1, m.Call());
      } else {
        CHECK_EQ(1, m.Call());
        CHECK_EQ(*i / *j, dst);
      }
    }
  }
}

TEST(RunCallInt64Mod) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_int64_mod(m.isolate());

  int64_t dst;
  int64_t src;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(MachineType::Int32(), MachineType::Pointer(),
                            MachineType::Pointer(), function,
                            m.PointerConstant(&dst), m.PointerConstant(&src)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) {
      dst = *i;
      src = *j;
      if (src == 0) {
        CHECK_EQ(0, m.Call());
      } else {
        CHECK_EQ(1, m.Call());
        CHECK_EQ(*i % *j, dst);
      }
    }
  }
}

TEST(RunCallUint64Div) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_div(m.isolate());

  uint64_t dst;
  uint64_t src;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(MachineType::Int32(), MachineType::Pointer(),
                            MachineType::Pointer(), function,
                            m.PointerConstant(&dst), m.PointerConstant(&src)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) {
      dst = *i;
      src = *j;
      if (src == 0) {
        CHECK_EQ(0, m.Call());
      } else {
        CHECK_EQ(1, m.Call());
        CHECK_EQ(*i / *j, dst);
      }
    }
  }
}

TEST(RunCallUint64Mod) {
  BufferedRawMachineAssemblerTester<int32_t> m;
  ExternalReference ref = ExternalReference::wasm_uint64_mod(m.isolate());

  uint64_t dst;
  uint64_t src;

  Node* function = m.ExternalConstant(ref);
  m.Return(m.CallCFunction2(MachineType::Int32(), MachineType::Pointer(),
                            MachineType::Pointer(), function,
                            m.PointerConstant(&dst), m.PointerConstant(&src)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) {
      dst = *i;
      src = *j;
      if (src == 0) {
        CHECK_EQ(0, m.Call());
      } else {
        CHECK_EQ(1, m.Call());
        CHECK_EQ(*i % *j, dst);
      }
    }
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8

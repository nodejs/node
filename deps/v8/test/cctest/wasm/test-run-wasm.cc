// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/test-signatures.h"
#include "test/cctest/wasm/wasm-run-utils.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

#define BUILD(r, ...)                      \
  do {                                     \
    byte code[] = {__VA_ARGS__};           \
    r.Build(code, code + arraysize(code)); \
  } while (false)


TEST(Run_WasmInt8Const) {
  WasmRunner<int8_t> r;
  const byte kExpectedValue = 121;
  // return(kExpectedValue)
  BUILD(r, WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt8Const_fallthru1) {
  WasmRunner<int8_t> r;
  const byte kExpectedValue = 122;
  // kExpectedValue
  BUILD(r, WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt8Const_fallthru2) {
  WasmRunner<int8_t> r;
  const byte kExpectedValue = 123;
  // -99 kExpectedValue
  BUILD(r, WASM_I8(-99), WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt8Const_all) {
  for (int value = -128; value <= 127; value++) {
    WasmRunner<int8_t> r;
    // return(value)
    BUILD(r, WASM_I8(value));
    int8_t result = r.Call();
    CHECK_EQ(value, result);
  }
}


TEST(Run_WasmInt32Const) {
  WasmRunner<int32_t> r;
  const int32_t kExpectedValue = 0x11223344;
  // return(kExpectedValue)
  BUILD(r, WASM_I32(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt32Const_many) {
  FOR_INT32_INPUTS(i) {
    WasmRunner<int32_t> r;
    const int32_t kExpectedValue = *i;
    // return(kExpectedValue)
    BUILD(r, WASM_I32(kExpectedValue));
    CHECK_EQ(kExpectedValue, r.Call());
  }
}


TEST(Run_WasmMemorySize) {
  WasmRunner<int32_t> r;
  TestingModule module;
  module.AddMemory(1024);
  r.env()->module = &module;
  BUILD(r, kExprMemorySize);
  CHECK_EQ(1024, r.Call());
}


#if WASM_64
TEST(Run_WasmInt64Const) {
  WasmRunner<int64_t> r;
  const int64_t kExpectedValue = 0x1122334455667788LL;
  // return(kExpectedValue)
  BUILD(r, WASM_I64(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt64Const_many) {
  int cntr = 0;
  FOR_INT32_INPUTS(i) {
    WasmRunner<int64_t> r;
    const int64_t kExpectedValue = (static_cast<int64_t>(*i) << 32) | cntr;
    // return(kExpectedValue)
    BUILD(r, WASM_I64(kExpectedValue));
    CHECK_EQ(kExpectedValue, r.Call());
    cntr++;
  }
}
#endif


TEST(Run_WasmInt32Param0) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // return(local[0])
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}


TEST(Run_WasmInt32Param0_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // local[0]
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}


TEST(Run_WasmInt32Param1) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  // local[1]
  BUILD(r, WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(-111, *i)); }
}


TEST(Run_WasmInt32Add) {
  WasmRunner<int32_t> r;
  // 11 + 44
  BUILD(r, WASM_I32_ADD(WASM_I8(11), WASM_I8(44)));
  CHECK_EQ(55, r.Call());
}


TEST(Run_WasmInt32Add_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 + 13
  BUILD(r, WASM_I32_ADD(WASM_I8(13), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i + 13, r.Call(*i)); }
}


TEST(Run_WasmInt32Add_P_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 + 13
  BUILD(r, WASM_I32_ADD(WASM_I8(13), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i + 13, r.Call(*i)); }
}


TEST(Run_WasmInt32Add_P2) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  //  p0 + p1
  BUILD(r, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*i) +
                                              static_cast<uint32_t>(*j));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}


// TODO(titzer): Fix for nosee4 and re-enable.
#if 0

TEST(Run_WasmFloat32Add) {
  WasmRunner<int32_t> r;
  // int(11.5f + 44.5f)
  BUILD(r,
        WASM_I32_SCONVERT_F32(WASM_F32_ADD(WASM_F32(11.5f), WASM_F32(44.5f))));
  CHECK_EQ(56, r.Call());
}


TEST(Run_WasmFloat64Add) {
  WasmRunner<int32_t> r;
  // return int(13.5d + 43.5d)
  BUILD(r, WASM_I32_SCONVERT_F64(WASM_F64_ADD(WASM_F64(13.5), WASM_F64(43.5))));
  CHECK_EQ(57, r.Call());
}

#endif


void TestInt32Binop(WasmOpcode opcode, int32_t expected, int32_t a, int32_t b) {
  {
    WasmRunner<int32_t> r;
    // K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I32(a), WASM_I32(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
    // a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


TEST(Run_WasmInt32Binops) {
  TestInt32Binop(kExprI32Add, 88888888, 33333333, 55555555);
  TestInt32Binop(kExprI32Sub, -1111111, 7777777, 8888888);
  TestInt32Binop(kExprI32Mul, 65130756, 88734, 734);
  TestInt32Binop(kExprI32DivS, -66, -4777344, 72384);
  TestInt32Binop(kExprI32DivU, 805306368, 0xF0000000, 5);
  TestInt32Binop(kExprI32RemS, -3, -3003, 1000);
  TestInt32Binop(kExprI32RemU, 4, 4004, 1000);
  TestInt32Binop(kExprI32And, 0xEE, 0xFFEE, 0xFF0000FF);
  TestInt32Binop(kExprI32Ior, 0xF0FF00FF, 0xF0F000EE, 0x000F0011);
  TestInt32Binop(kExprI32Xor, 0xABCDEF01, 0xABCDEFFF, 0xFE);
  TestInt32Binop(kExprI32Shl, 0xA0000000, 0xA, 28);
  TestInt32Binop(kExprI32ShrU, 0x07000010, 0x70000100, 4);
  TestInt32Binop(kExprI32ShrS, 0xFF000000, 0x80000000, 7);
  TestInt32Binop(kExprI32Eq, 1, -99, -99);
  TestInt32Binop(kExprI32Ne, 0, -97, -97);

  TestInt32Binop(kExprI32LtS, 1, -4, 4);
  TestInt32Binop(kExprI32LeS, 0, -2, -3);
  TestInt32Binop(kExprI32LtU, 1, 0, -6);
  TestInt32Binop(kExprI32LeU, 1, 98978, 0xF0000000);

  TestInt32Binop(kExprI32GtS, 1, 4, -4);
  TestInt32Binop(kExprI32GeS, 0, -3, -2);
  TestInt32Binop(kExprI32GtU, 1, -6, 0);
  TestInt32Binop(kExprI32GeU, 1, 0xF0000000, 98978);
}


void TestInt32Unop(WasmOpcode opcode, int32_t expected, int32_t a) {
  {
    WasmRunner<int32_t> r;
    // return op K
    BUILD(r, WASM_UNOP(opcode, WASM_I32(a)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Int32());
    // return op a
    BUILD(r, WASM_UNOP(opcode, WASM_GET_LOCAL(0)));
    CHECK_EQ(expected, r.Call(a));
  }
}


TEST(Run_WasmInt32Clz) {
  TestInt32Unop(kExprI32Clz, 0, 0x80001000);
  TestInt32Unop(kExprI32Clz, 1, 0x40000500);
  TestInt32Unop(kExprI32Clz, 2, 0x20000300);
  TestInt32Unop(kExprI32Clz, 3, 0x10000003);
  TestInt32Unop(kExprI32Clz, 4, 0x08050000);
  TestInt32Unop(kExprI32Clz, 5, 0x04006000);
  TestInt32Unop(kExprI32Clz, 6, 0x02000000);
  TestInt32Unop(kExprI32Clz, 7, 0x010000a0);
  TestInt32Unop(kExprI32Clz, 8, 0x00800c00);
  TestInt32Unop(kExprI32Clz, 9, 0x00400000);
  TestInt32Unop(kExprI32Clz, 10, 0x0020000d);
  TestInt32Unop(kExprI32Clz, 11, 0x00100f00);
  TestInt32Unop(kExprI32Clz, 12, 0x00080000);
  TestInt32Unop(kExprI32Clz, 13, 0x00041000);
  TestInt32Unop(kExprI32Clz, 14, 0x00020020);
  TestInt32Unop(kExprI32Clz, 15, 0x00010300);
  TestInt32Unop(kExprI32Clz, 16, 0x00008040);
  TestInt32Unop(kExprI32Clz, 17, 0x00004005);
  TestInt32Unop(kExprI32Clz, 18, 0x00002050);
  TestInt32Unop(kExprI32Clz, 19, 0x00001700);
  TestInt32Unop(kExprI32Clz, 20, 0x00000870);
  TestInt32Unop(kExprI32Clz, 21, 0x00000405);
  TestInt32Unop(kExprI32Clz, 22, 0x00000203);
  TestInt32Unop(kExprI32Clz, 23, 0x00000101);
  TestInt32Unop(kExprI32Clz, 24, 0x00000089);
  TestInt32Unop(kExprI32Clz, 25, 0x00000041);
  TestInt32Unop(kExprI32Clz, 26, 0x00000022);
  TestInt32Unop(kExprI32Clz, 27, 0x00000013);
  TestInt32Unop(kExprI32Clz, 28, 0x00000008);
  TestInt32Unop(kExprI32Clz, 29, 0x00000004);
  TestInt32Unop(kExprI32Clz, 30, 0x00000002);
  TestInt32Unop(kExprI32Clz, 31, 0x00000001);
  TestInt32Unop(kExprI32Clz, 32, 0x00000000);
}


TEST(Run_WasmInt32Ctz) {
  TestInt32Unop(kExprI32Ctz, 32, 0x00000000);
  TestInt32Unop(kExprI32Ctz, 31, 0x80000000);
  TestInt32Unop(kExprI32Ctz, 30, 0x40000000);
  TestInt32Unop(kExprI32Ctz, 29, 0x20000000);
  TestInt32Unop(kExprI32Ctz, 28, 0x10000000);
  TestInt32Unop(kExprI32Ctz, 27, 0xa8000000);
  TestInt32Unop(kExprI32Ctz, 26, 0xf4000000);
  TestInt32Unop(kExprI32Ctz, 25, 0x62000000);
  TestInt32Unop(kExprI32Ctz, 24, 0x91000000);
  TestInt32Unop(kExprI32Ctz, 23, 0xcd800000);
  TestInt32Unop(kExprI32Ctz, 22, 0x09400000);
  TestInt32Unop(kExprI32Ctz, 21, 0xaf200000);
  TestInt32Unop(kExprI32Ctz, 20, 0xac100000);
  TestInt32Unop(kExprI32Ctz, 19, 0xe0b80000);
  TestInt32Unop(kExprI32Ctz, 18, 0x9ce40000);
  TestInt32Unop(kExprI32Ctz, 17, 0xc7920000);
  TestInt32Unop(kExprI32Ctz, 16, 0xb8f10000);
  TestInt32Unop(kExprI32Ctz, 15, 0x3b9f8000);
  TestInt32Unop(kExprI32Ctz, 14, 0xdb4c4000);
  TestInt32Unop(kExprI32Ctz, 13, 0xe9a32000);
  TestInt32Unop(kExprI32Ctz, 12, 0xfca61000);
  TestInt32Unop(kExprI32Ctz, 11, 0x6c8a7800);
  TestInt32Unop(kExprI32Ctz, 10, 0x8ce5a400);
  TestInt32Unop(kExprI32Ctz, 9, 0xcb7d0200);
  TestInt32Unop(kExprI32Ctz, 8, 0xcb4dc100);
  TestInt32Unop(kExprI32Ctz, 7, 0xdfbec580);
  TestInt32Unop(kExprI32Ctz, 6, 0x27a9db40);
  TestInt32Unop(kExprI32Ctz, 5, 0xde3bcb20);
  TestInt32Unop(kExprI32Ctz, 4, 0xd7e8a610);
  TestInt32Unop(kExprI32Ctz, 3, 0x9afdbc88);
  TestInt32Unop(kExprI32Ctz, 2, 0x9afdbc84);
  TestInt32Unop(kExprI32Ctz, 1, 0x9afdbc82);
  TestInt32Unop(kExprI32Ctz, 0, 0x9afdbc81);
}


TEST(Run_WasmInt32Popcnt) {
  TestInt32Unop(kExprI32Popcnt, 32, 0xffffffff);
  TestInt32Unop(kExprI32Popcnt, 0, 0x00000000);
  TestInt32Unop(kExprI32Popcnt, 1, 0x00008000);
  TestInt32Unop(kExprI32Popcnt, 13, 0x12345678);
  TestInt32Unop(kExprI32Popcnt, 19, 0xfedcba09);
}


#if WASM_64
void TestInt64Binop(WasmOpcode opcode, int64_t expected, int64_t a, int64_t b) {
  if (!WasmOpcodes::IsSupported(opcode)) return;
  {
    WasmRunner<int64_t> r;
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I64(a), WASM_I64(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestInt64Cmp(WasmOpcode opcode, int64_t expected, int64_t a, int64_t b) {
  if (!WasmOpcodes::IsSupported(opcode)) return;
  {
    WasmRunner<int32_t> r;
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I64(a), WASM_I64(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Int64(), MachineType::Int64());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


TEST(Run_WasmInt64Binops) {
  // TODO(titzer): real 64-bit numbers
  TestInt64Binop(kExprI64Add, 8888888888888LL, 3333333333333LL,
                 5555555555555LL);
  TestInt64Binop(kExprI64Sub, -111111111111LL, 777777777777LL, 888888888888LL);
  TestInt64Binop(kExprI64Mul, 65130756, 88734, 734);
  TestInt64Binop(kExprI64DivS, -66, -4777344, 72384);
  TestInt64Binop(kExprI64DivU, 805306368, 0xF0000000, 5);
  TestInt64Binop(kExprI64RemS, -3, -3003, 1000);
  TestInt64Binop(kExprI64RemU, 4, 4004, 1000);
  TestInt64Binop(kExprI64And, 0xEE, 0xFFEE, 0xFF0000FF);
  TestInt64Binop(kExprI64Ior, 0xF0FF00FF, 0xF0F000EE, 0x000F0011);
  TestInt64Binop(kExprI64Xor, 0xABCDEF01, 0xABCDEFFF, 0xFE);
  TestInt64Binop(kExprI64Shl, 0xA0000000, 0xA, 28);
  TestInt64Binop(kExprI64ShrU, 0x0700001000123456LL, 0x7000010001234567LL, 4);
  TestInt64Binop(kExprI64ShrS, 0xFF00000000000000LL, 0x8000000000000000LL, 7);
  TestInt64Cmp(kExprI64Eq, 1, -9999, -9999);
  TestInt64Cmp(kExprI64Ne, 1, -9199, -9999);
  TestInt64Cmp(kExprI64LtS, 1, -4, 4);
  TestInt64Cmp(kExprI64LeS, 0, -2, -3);
  TestInt64Cmp(kExprI64LtU, 1, 0, -6);
  TestInt64Cmp(kExprI64LeU, 1, 98978, 0xF0000000);
}


TEST(Run_WasmInt64Clz) {
  struct {
    int64_t expected;
    uint64_t input;
  } values[] = {{0, 0x8000100000000000},  {1, 0x4000050000000000},
                {2, 0x2000030000000000},  {3, 0x1000000300000000},
                {4, 0x0805000000000000},  {5, 0x0400600000000000},
                {6, 0x0200000000000000},  {7, 0x010000a000000000},
                {8, 0x00800c0000000000},  {9, 0x0040000000000000},
                {10, 0x0020000d00000000}, {11, 0x00100f0000000000},
                {12, 0x0008000000000000}, {13, 0x0004100000000000},
                {14, 0x0002002000000000}, {15, 0x0001030000000000},
                {16, 0x0000804000000000}, {17, 0x0000400500000000},
                {18, 0x0000205000000000}, {19, 0x0000170000000000},
                {20, 0x0000087000000000}, {21, 0x0000040500000000},
                {22, 0x0000020300000000}, {23, 0x0000010100000000},
                {24, 0x0000008900000000}, {25, 0x0000004100000000},
                {26, 0x0000002200000000}, {27, 0x0000001300000000},
                {28, 0x0000000800000000}, {29, 0x0000000400000000},
                {30, 0x0000000200000000}, {31, 0x0000000100000000},
                {32, 0x0000000080001000}, {33, 0x0000000040000500},
                {34, 0x0000000020000300}, {35, 0x0000000010000003},
                {36, 0x0000000008050000}, {37, 0x0000000004006000},
                {38, 0x0000000002000000}, {39, 0x00000000010000a0},
                {40, 0x0000000000800c00}, {41, 0x0000000000400000},
                {42, 0x000000000020000d}, {43, 0x0000000000100f00},
                {44, 0x0000000000080000}, {45, 0x0000000000041000},
                {46, 0x0000000000020020}, {47, 0x0000000000010300},
                {48, 0x0000000000008040}, {49, 0x0000000000004005},
                {50, 0x0000000000002050}, {51, 0x0000000000001700},
                {52, 0x0000000000000870}, {53, 0x0000000000000405},
                {54, 0x0000000000000203}, {55, 0x0000000000000101},
                {56, 0x0000000000000089}, {57, 0x0000000000000041},
                {58, 0x0000000000000022}, {59, 0x0000000000000013},
                {60, 0x0000000000000008}, {61, 0x0000000000000004},
                {62, 0x0000000000000002}, {63, 0x0000000000000001},
                {64, 0x0000000000000000}};

  WasmRunner<int64_t> r(MachineType::Uint64());
  BUILD(r, WASM_I64_CLZ(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}


TEST(Run_WasmInt64Ctz) {
  struct {
    int64_t expected;
    uint64_t input;
  } values[] = {{64, 0x0000000000000000}, {63, 0x8000000000000000},
                {62, 0x4000000000000000}, {61, 0x2000000000000000},
                {60, 0x1000000000000000}, {59, 0xa800000000000000},
                {58, 0xf400000000000000}, {57, 0x6200000000000000},
                {56, 0x9100000000000000}, {55, 0xcd80000000000000},
                {54, 0x0940000000000000}, {53, 0xaf20000000000000},
                {52, 0xac10000000000000}, {51, 0xe0b8000000000000},
                {50, 0x9ce4000000000000}, {49, 0xc792000000000000},
                {48, 0xb8f1000000000000}, {47, 0x3b9f800000000000},
                {46, 0xdb4c400000000000}, {45, 0xe9a3200000000000},
                {44, 0xfca6100000000000}, {43, 0x6c8a780000000000},
                {42, 0x8ce5a40000000000}, {41, 0xcb7d020000000000},
                {40, 0xcb4dc10000000000}, {39, 0xdfbec58000000000},
                {38, 0x27a9db4000000000}, {37, 0xde3bcb2000000000},
                {36, 0xd7e8a61000000000}, {35, 0x9afdbc8800000000},
                {34, 0x9afdbc8400000000}, {33, 0x9afdbc8200000000},
                {32, 0x9afdbc8100000000}, {31, 0x0000000080000000},
                {30, 0x0000000040000000}, {29, 0x0000000020000000},
                {28, 0x0000000010000000}, {27, 0x00000000a8000000},
                {26, 0x00000000f4000000}, {25, 0x0000000062000000},
                {24, 0x0000000091000000}, {23, 0x00000000cd800000},
                {22, 0x0000000009400000}, {21, 0x00000000af200000},
                {20, 0x00000000ac100000}, {19, 0x00000000e0b80000},
                {18, 0x000000009ce40000}, {17, 0x00000000c7920000},
                {16, 0x00000000b8f10000}, {15, 0x000000003b9f8000},
                {14, 0x00000000db4c4000}, {13, 0x00000000e9a32000},
                {12, 0x00000000fca61000}, {11, 0x000000006c8a7800},
                {10, 0x000000008ce5a400}, {9, 0x00000000cb7d0200},
                {8, 0x00000000cb4dc100},  {7, 0x00000000dfbec580},
                {6, 0x0000000027a9db40},  {5, 0x00000000de3bcb20},
                {4, 0x00000000d7e8a610},  {3, 0x000000009afdbc88},
                {2, 0x000000009afdbc84},  {1, 0x000000009afdbc82},
                {0, 0x000000009afdbc81}};

  WasmRunner<int64_t> r(MachineType::Uint64());
  BUILD(r, WASM_I64_CTZ(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}


TEST(Run_WasmInt64Popcnt) {
  struct {
    int64_t expected;
    uint64_t input;
  } values[] = {{64, 0xffffffffffffffff},
                {0, 0x0000000000000000},
                {2, 0x0000080000008000},
                {26, 0x1123456782345678},
                {38, 0xffedcba09edcba09}};

  WasmRunner<int64_t> r(MachineType::Uint64());
  BUILD(r, WASM_I64_POPCNT(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}


#endif

TEST(Run_WASM_Int32DivS_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(std::numeric_limits<int32_t>::min(), -1));
  CHECK_TRAP(r.Call(std::numeric_limits<int32_t>::min(), 0));
}


TEST(Run_WASM_Int32RemS_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(33, r.Call(133, 100));
  CHECK_EQ(0, r.Call(std::numeric_limits<int32_t>::min(), -1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(std::numeric_limits<int32_t>::min(), 0));
}


TEST(Run_WASM_Int32DivU_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(std::numeric_limits<int32_t>::min(), -1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(std::numeric_limits<int32_t>::min(), 0));
}


TEST(Run_WASM_Int32RemU_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(17, r.Call(217, 100));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(std::numeric_limits<int32_t>::min(), 0));
  CHECK_EQ(std::numeric_limits<int32_t>::min(),
           r.Call(std::numeric_limits<int32_t>::min(), -1));
}


TEST(Run_WASM_Int32DivS_byzero_const) {
  for (int8_t denom = -2; denom < 8; denom++) {
    WasmRunner<int32_t> r(MachineType::Int32());
    BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I8(denom)));
    for (int32_t val = -7; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}


TEST(Run_WASM_Int32DivU_byzero_const) {
  for (uint32_t denom = 0xfffffffe; denom < 8; denom++) {
    WasmRunner<uint32_t> r(MachineType::Uint32());
    BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_I32(denom)));

    for (uint32_t val = 0xfffffff0; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}


TEST(Run_WASM_Int32DivS_trap_effect) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  TestingModule module;
  module.AddMemoryElems<int32_t>(8);
  r.env()->module = &module;

  BUILD(r,
        WASM_IF_ELSE(WASM_GET_LOCAL(0),
                     WASM_I32_DIVS(WASM_STORE_MEM(MachineType::Int8(),
                                                  WASM_ZERO, WASM_GET_LOCAL(0)),
                                   WASM_GET_LOCAL(1)),
                     WASM_I32_DIVS(WASM_STORE_MEM(MachineType::Int8(),
                                                  WASM_ZERO, WASM_GET_LOCAL(0)),
                                   WASM_GET_LOCAL(1))));
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_TRAP(r.Call(8, 0));
  CHECK_TRAP(r.Call(4, 0));
  CHECK_TRAP(r.Call(0, 0));
}


#if WASM_64
#define as64(x) static_cast<int64_t>(x)
TEST(Run_WASM_Int64DivS_trap) {
  WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
  BUILD(r, WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(0, r.Call(as64(0), as64(100)));
  CHECK_TRAP64(r.Call(as64(100), as64(0)));
  CHECK_TRAP64(r.Call(as64(-1001), as64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), as64(-1)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), as64(0)));
}


TEST(Run_WASM_Int64RemS_trap) {
  WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
  BUILD(r, WASM_I64_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(33, r.Call(as64(133), as64(100)));
  CHECK_EQ(0, r.Call(std::numeric_limits<int64_t>::min(), as64(-1)));
  CHECK_TRAP64(r.Call(as64(100), as64(0)));
  CHECK_TRAP64(r.Call(as64(-1001), as64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), as64(0)));
}


TEST(Run_WASM_Int64DivU_trap) {
  WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
  BUILD(r, WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(0, r.Call(as64(0), as64(100)));
  CHECK_EQ(0, r.Call(std::numeric_limits<int64_t>::min(), as64(-1)));
  CHECK_TRAP64(r.Call(as64(100), as64(0)));
  CHECK_TRAP64(r.Call(as64(-1001), as64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), as64(0)));
}


TEST(Run_WASM_Int64RemU_trap) {
  WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
  BUILD(r, WASM_I64_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(17, r.Call(as64(217), as64(100)));
  CHECK_TRAP64(r.Call(as64(100), as64(0)));
  CHECK_TRAP64(r.Call(as64(-1001), as64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), as64(0)));
  CHECK_EQ(std::numeric_limits<int64_t>::min(),
           r.Call(std::numeric_limits<int64_t>::min(), as64(-1)));
}


TEST(Run_WASM_Int64DivS_byzero_const) {
  for (int8_t denom = -2; denom < 8; denom++) {
    WasmRunner<int64_t> r(MachineType::Int64());
    BUILD(r, WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_I64(denom)));
    for (int64_t val = -7; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP64(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}


TEST(Run_WASM_Int64DivU_byzero_const) {
  for (uint64_t denom = 0xfffffffffffffffe; denom < 8; denom++) {
    WasmRunner<uint64_t> r(MachineType::Uint64());
    BUILD(r, WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_I64(denom)));

    for (uint64_t val = 0xfffffffffffffff0; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP64(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}
#endif


void TestFloat32Binop(WasmOpcode opcode, int32_t expected, float a, float b) {
  {
    WasmRunner<int32_t> r;
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_F32(a), WASM_F32(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float32(), MachineType::Float32());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat32BinopWithConvert(WasmOpcode opcode, int32_t expected, float a,
                                 float b) {
  {
    WasmRunner<int32_t> r;
    // return int(K op K)
    BUILD(r,
          WASM_I32_SCONVERT_F32(WASM_BINOP(opcode, WASM_F32(a), WASM_F32(b))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float32(), MachineType::Float32());
    // return int(a op b)
    BUILD(r, WASM_I32_SCONVERT_F32(
                 WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat32UnopWithConvert(WasmOpcode opcode, int32_t expected, float a) {
  {
    WasmRunner<int32_t> r;
    // return int(op(K))
    BUILD(r, WASM_I32_SCONVERT_F32(WASM_UNOP(opcode, WASM_F32(a))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float32());
    // return int(op(a))
    BUILD(r, WASM_I32_SCONVERT_F32(WASM_UNOP(opcode, WASM_GET_LOCAL(0))));
    CHECK_EQ(expected, r.Call(a));
  }
}


void TestFloat64Binop(WasmOpcode opcode, int32_t expected, double a, double b) {
  {
    WasmRunner<int32_t> r;
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_F64(a), WASM_F64(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float64(), MachineType::Float64());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat64BinopWithConvert(WasmOpcode opcode, int32_t expected, double a,
                                 double b) {
  {
    WasmRunner<int32_t> r;
    // return int(K op K)
    BUILD(r,
          WASM_I32_SCONVERT_F64(WASM_BINOP(opcode, WASM_F64(a), WASM_F64(b))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float64(), MachineType::Float64());
    BUILD(r, WASM_I32_SCONVERT_F64(
                 WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat64UnopWithConvert(WasmOpcode opcode, int32_t expected, double a) {
  {
    WasmRunner<int32_t> r;
    // return int(op(K))
    BUILD(r, WASM_I32_SCONVERT_F64(WASM_UNOP(opcode, WASM_F64(a))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float64());
    // return int(op(a))
    BUILD(r, WASM_I32_SCONVERT_F64(WASM_UNOP(opcode, WASM_GET_LOCAL(0))));
    CHECK_EQ(expected, r.Call(a));
  }
}


// TODO(titzer): Fix for nosee4 and re-enable.
#if 0

TEST(Run_WasmFloat32Binops) {
  TestFloat32Binop(kExprF32Eq, 1, 8.125f, 8.125f);
  TestFloat32Binop(kExprF32Ne, 1, 8.125f, 8.127f);
  TestFloat32Binop(kExprF32Lt, 1, -9.5f, -9.0f);
  TestFloat32Binop(kExprF32Le, 1, -1111.0f, -1111.0f);
  TestFloat32Binop(kExprF32Gt, 1, -9.0f, -9.5f);
  TestFloat32Binop(kExprF32Ge, 1, -1111.0f, -1111.0f);

  TestFloat32BinopWithConvert(kExprF32Add, 10, 3.5f, 6.5f);
  TestFloat32BinopWithConvert(kExprF32Sub, 2, 44.5f, 42.5f);
  TestFloat32BinopWithConvert(kExprF32Mul, -66, -132.1f, 0.5f);
  TestFloat32BinopWithConvert(kExprF32Div, 11, 22.1f, 2.0f);
}


TEST(Run_WasmFloat32Unops) {
  TestFloat32UnopWithConvert(kExprF32Abs, 8, 8.125f);
  TestFloat32UnopWithConvert(kExprF32Abs, 9, -9.125f);
  TestFloat32UnopWithConvert(kExprF32Neg, -213, 213.125f);
  TestFloat32UnopWithConvert(kExprF32Sqrt, 12, 144.4f);
}


TEST(Run_WasmFloat64Binops) {
  TestFloat64Binop(kExprF64Eq, 1, 16.25, 16.25);
  TestFloat64Binop(kExprF64Ne, 1, 16.25, 16.15);
  TestFloat64Binop(kExprF64Lt, 1, -32.4, 11.7);
  TestFloat64Binop(kExprF64Le, 1, -88.9, -88.9);
  TestFloat64Binop(kExprF64Gt, 1, 11.7, -32.4);
  TestFloat64Binop(kExprF64Ge, 1, -88.9, -88.9);

  TestFloat64BinopWithConvert(kExprF64Add, 100, 43.5, 56.5);
  TestFloat64BinopWithConvert(kExprF64Sub, 200, 12200.1, 12000.1);
  TestFloat64BinopWithConvert(kExprF64Mul, -33, 134, -0.25);
  TestFloat64BinopWithConvert(kExprF64Div, -1111, -2222.3, 2);
}


TEST(Run_WasmFloat64Unops) {
  TestFloat64UnopWithConvert(kExprF64Abs, 108, 108.125);
  TestFloat64UnopWithConvert(kExprF64Abs, 209, -209.125);
  TestFloat64UnopWithConvert(kExprF64Neg, -209, 209.125);
  TestFloat64UnopWithConvert(kExprF64Sqrt, 13, 169.4);
}

#endif


TEST(Run_WasmFloat32Neg) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_NEG(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    CHECK_EQ(0x80000000,
             bit_cast<uint32_t>(*i) ^ bit_cast<uint32_t>(r.Call(*i)));
  }
}


TEST(Run_WasmFloat64Neg) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_NEG(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    CHECK_EQ(0x8000000000000000,
             bit_cast<uint64_t>(*i) ^ bit_cast<uint64_t>(r.Call(*i)));
  }
}


TEST(Run_Wasm_IfElse_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // if (p0) return 11; else return 22;
  BUILD(r, WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                        WASM_I8(11),        // --
                        WASM_I8(22)));      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_IfElse_Unreachable1) {
  WasmRunner<int32_t> r;
  // if (0) unreachable; else return 22;
  BUILD(r, WASM_IF_ELSE(WASM_ZERO,         // --
                        WASM_UNREACHABLE,  // --
                        WASM_I8(27)));     // --
  CHECK_EQ(27, r.Call());
}


TEST(Run_Wasm_Return12) {
  WasmRunner<int32_t> r;

  BUILD(r, WASM_RETURN(WASM_I8(12)));
  CHECK_EQ(12, r.Call());
}


TEST(Run_Wasm_Return17) {
  WasmRunner<int32_t> r;

  BUILD(r, WASM_BLOCK(1, WASM_RETURN(WASM_I8(17))));
  CHECK_EQ(17, r.Call());
}


TEST(Run_Wasm_Return_I32) {
  WasmRunner<int32_t> r(MachineType::Int32());

  BUILD(r, WASM_RETURN(WASM_GET_LOCAL(0)));

  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}


#if WASM_64
TEST(Run_Wasm_Return_I64) {
  WasmRunner<int64_t> r(MachineType::Int64());

  BUILD(r, WASM_RETURN(WASM_GET_LOCAL(0)));

  FOR_INT64_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}
#endif


TEST(Run_Wasm_Return_F32) {
  WasmRunner<float> r(MachineType::Float32());

  BUILD(r, WASM_RETURN(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    float expect = *i;
    float result = r.Call(expect);
    if (std::isnan(expect)) {
      CHECK(std::isnan(result));
    } else {
      CHECK_EQ(expect, result);
    }
  }
}


TEST(Run_Wasm_Return_F64) {
  WasmRunner<double> r(MachineType::Float64());

  BUILD(r, WASM_RETURN(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    double expect = *i;
    double result = r.Call(expect);
    if (std::isnan(expect)) {
      CHECK(std::isnan(result));
    } else {
      CHECK_EQ(expect, result);
    }
  }
}


TEST(Run_Wasm_Select) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // return select(a, 11, 22);
  BUILD(r, WASM_SELECT(WASM_GET_LOCAL(0), WASM_I8(11), WASM_I8(22)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Select_strict1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // select(a, a = 11, 22); return a
  BUILD(r,
        WASM_BLOCK(2, WASM_SELECT(WASM_GET_LOCAL(0),
                                  WASM_SET_LOCAL(0, WASM_I8(11)), WASM_I8(22)),
                   WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(11, r.Call(*i)); }
}


TEST(Run_Wasm_Select_strict2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // select(a, 11, a = 22); return a;
  BUILD(r, WASM_BLOCK(2, WASM_SELECT(WASM_GET_LOCAL(0), WASM_I8(11),
                                     WASM_SET_LOCAL(0, WASM_I8(22))),
                      WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(22, r.Call(*i)); }
}


TEST(Run_Wasm_BrIf_strict) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(
               2, WASM_BLOCK(1, WASM_BRV_IF(0, WASM_GET_LOCAL(0),
                                            WASM_SET_LOCAL(0, WASM_I8(99)))),
               WASM_GET_LOCAL(0)));

  FOR_INT32_INPUTS(i) { CHECK_EQ(99, r.Call(*i)); }
}


TEST(Run_Wasm_TableSwitch1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_TABLESWITCH_OP(1, 1, WASM_CASE(0)),
        WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_RETURN(WASM_I8(93))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(93, r.Call(*i)); }
}


TEST(Run_Wasm_TableSwitch_br) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_TABLESWITCH_OP(1, 2, WASM_CASE_BR(0), WASM_CASE(0)),
        WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_RETURN(WASM_I8(91))),
        WASM_I8(99));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(91, r.Call(1));
  CHECK_EQ(91, r.Call(2));
  CHECK_EQ(91, r.Call(3));
}


TEST(Run_Wasm_TableSwitch_br2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(
               2, WASM_BLOCK(2, WASM_TABLESWITCH_OP(
                                    1, 4, WASM_CASE_BR(0), WASM_CASE_BR(1),
                                    WASM_CASE_BR(2), WASM_CASE(0)),
                             WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0),
                                                   WASM_RETURN(WASM_I8(85))),
                             WASM_RETURN(WASM_I8(86))),
               WASM_RETURN(WASM_I8(87))),
        WASM_I8(88));
  CHECK_EQ(86, r.Call(0));
  CHECK_EQ(87, r.Call(1));
  CHECK_EQ(88, r.Call(2));
  CHECK_EQ(85, r.Call(3));
  CHECK_EQ(85, r.Call(4));
  CHECK_EQ(85, r.Call(5));
}


TEST(Run_Wasm_TableSwitch2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_TABLESWITCH_OP(2, 2, WASM_CASE(0), WASM_CASE(1)),
        WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_RETURN(WASM_I8(91)),
                              WASM_RETURN(WASM_I8(92))));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i == 0 ? 91 : 92;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_TableSwitch2b) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_TABLESWITCH_OP(2, 2, WASM_CASE(1), WASM_CASE(0)),
        WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_RETURN(WASM_I8(81)),
                              WASM_RETURN(WASM_I8(82))));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i == 0 ? 82 : 81;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_TableSwitch4) {
  for (int i = 0; i < 4; i++) {
    const uint16_t br = 0x8000u;
    uint16_t c = 0;
    uint16_t cases[] = {i == 0 ? br : c++, i == 1 ? br : c++, i == 2 ? br : c++,
                        i == 3 ? br : c++};
    byte code[] = {
        WASM_BLOCK(1, WASM_TABLESWITCH_OP(
                          3, 4, WASM_CASE(cases[0]), WASM_CASE(cases[1]),
                          WASM_CASE(cases[2]), WASM_CASE(cases[3])),
                   WASM_TABLESWITCH_BODY(
                       WASM_GET_LOCAL(0), WASM_RETURN(WASM_I8(71)),
                       WASM_RETURN(WASM_I8(72)), WASM_RETURN(WASM_I8(73)))),
        WASM_RETURN(WASM_I8(74))};

    WasmRunner<int32_t> r(MachineType::Int32());
    r.Build(code, code + arraysize(code));

    FOR_INT32_INPUTS(i) {
      int index = (*i < 0 || *i > 3) ? 3 : *i;
      int32_t expected = 71 + cases[index];
      if (expected >= 0x8000) expected = 74;
      CHECK_EQ(expected, r.Call(*i));
    }
  }
}


TEST(Run_Wasm_TableSwitch4b) {
  for (int a = 0; a < 2; a++) {
    for (int b = 0; b < 2; b++) {
      for (int c = 0; c < 2; c++) {
        for (int d = 0; d < 2; d++) {
          if (a + b + c + d == 0) continue;
          if (a + b + c + d == 4) continue;

          byte code[] = {
              WASM_TABLESWITCH_OP(2, 4, WASM_CASE(a), WASM_CASE(b),
                                  WASM_CASE(c), WASM_CASE(d)),
              WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_RETURN(WASM_I8(61)),
                                    WASM_RETURN(WASM_I8(62)))};

          WasmRunner<int32_t> r(MachineType::Int32());
          r.Build(code, code + arraysize(code));

          CHECK_EQ(61 + a, r.Call(0));
          CHECK_EQ(61 + b, r.Call(1));
          CHECK_EQ(61 + c, r.Call(2));
          CHECK_EQ(61 + d, r.Call(3));
          CHECK_EQ(61 + d, r.Call(4));
        }
      }
    }
  }
}


TEST(Run_Wasm_TableSwitch4_fallthru) {
  byte code[] = {
      WASM_TABLESWITCH_OP(4, 4, WASM_CASE(0), WASM_CASE(1), WASM_CASE(2),
                          WASM_CASE(3)),
      WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_INC_LOCAL_BY(1, 1),
                            WASM_INC_LOCAL_BY(1, 2), WASM_INC_LOCAL_BY(1, 4),
                            WASM_INC_LOCAL_BY(1, 8)),
      WASM_GET_LOCAL(1)};

  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  r.Build(code, code + arraysize(code));

  CHECK_EQ(15, r.Call(0, 0));
  CHECK_EQ(14, r.Call(1, 0));
  CHECK_EQ(12, r.Call(2, 0));
  CHECK_EQ(8, r.Call(3, 0));
  CHECK_EQ(8, r.Call(4, 0));

  CHECK_EQ(115, r.Call(0, 100));
  CHECK_EQ(114, r.Call(1, 100));
  CHECK_EQ(112, r.Call(2, 100));
  CHECK_EQ(108, r.Call(3, 100));
  CHECK_EQ(108, r.Call(4, 100));
}


TEST(Run_Wasm_TableSwitch4_fallthru_br) {
  byte code[] = {
      WASM_TABLESWITCH_OP(4, 4, WASM_CASE(0), WASM_CASE(1), WASM_CASE(2),
                          WASM_CASE(3)),
      WASM_TABLESWITCH_BODY(WASM_GET_LOCAL(0), WASM_INC_LOCAL_BY(1, 1),
                            WASM_BRV(0, WASM_INC_LOCAL_BY(1, 2)),
                            WASM_INC_LOCAL_BY(1, 4),
                            WASM_BRV(0, WASM_INC_LOCAL_BY(1, 8))),
      WASM_GET_LOCAL(1)};

  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  r.Build(code, code + arraysize(code));

  CHECK_EQ(3, r.Call(0, 0));
  CHECK_EQ(2, r.Call(1, 0));
  CHECK_EQ(12, r.Call(2, 0));
  CHECK_EQ(8, r.Call(3, 0));
  CHECK_EQ(8, r.Call(4, 0));

  CHECK_EQ(203, r.Call(0, 200));
  CHECK_EQ(202, r.Call(1, 200));
  CHECK_EQ(212, r.Call(2, 200));
  CHECK_EQ(208, r.Call(3, 200));
  CHECK_EQ(208, r.Call(4, 200));
}


TEST(Run_Wasm_F32ReinterpretI32) {
  WasmRunner<int32_t> r;
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  r.env()->module = &module;

  BUILD(r, WASM_I32_REINTERPRET_F32(
               WASM_LOAD_MEM(MachineType::Float32(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    memory[0] = expected;
    CHECK_EQ(expected, r.Call());
  }
}


TEST(Run_Wasm_I32ReinterpretF32) {
  WasmRunner<int32_t> r(MachineType::Int32());
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  r.env()->module = &module;

  BUILD(r, WASM_BLOCK(
               2, WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO,
                                 WASM_F32_REINTERPRET_I32(WASM_GET_LOCAL(0))),
               WASM_I8(107)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    CHECK_EQ(107, r.Call(expected));
    CHECK_EQ(expected, memory[0]);
  }
}


TEST(Run_Wasm_ReturnStore) {
  WasmRunner<int32_t> r;
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  r.env()->module = &module;

  BUILD(r, WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                          WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    memory[0] = expected;
    CHECK_EQ(expected, r.Call());
  }
}


TEST(Run_Wasm_VoidReturn1) {
  WasmRunner<void> r;
  BUILD(r, kExprNop);
  r.Call();
}


TEST(Run_Wasm_VoidReturn2) {
  WasmRunner<void> r;
  BUILD(r, WASM_RETURN0);
  r.Call();
}


TEST(Run_Wasm_Block_If_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { if (p0) return 51; return 52; }
  BUILD(r, WASM_BLOCK(2,                                  // --
                      WASM_IF(WASM_GET_LOCAL(0),          // --
                              WASM_BRV(0, WASM_I8(51))),  // --
                      WASM_I8(52)));                      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 51 : 52;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_BrIf_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_I8(51)),
                      WASM_I8(52)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 51 : 52;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_IfElse_P_assign) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { if (p0) p0 = 71; else p0 = 72; return p0; }
  BUILD(r, WASM_BLOCK(2,                                             // --
                      WASM_IF_ELSE(WASM_GET_LOCAL(0),                // --
                                   WASM_SET_LOCAL(0, WASM_I8(71)),   // --
                                   WASM_SET_LOCAL(0, WASM_I8(72))),  // --
                      WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 71 : 72;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_IfElse_P_return) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // if (p0) return 81; else return 82;
  BUILD(r,                                        // --
        WASM_IF_ELSE(WASM_GET_LOCAL(0),           // --
                     WASM_RETURN(WASM_I8(81)),    // --
                     WASM_RETURN(WASM_I8(82))));  // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 81 : 82;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_If_P_assign) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { if (p0) p0 = 61; p0; }
  BUILD(r, WASM_BLOCK(
               2, WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_I8(61))),
               WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 61 : *i;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_DanglingAssign) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { return 0; p0 = 0; }
  BUILD(r,
        WASM_BLOCK(2, WASM_RETURN(WASM_I8(99)), WASM_SET_LOCAL(0, WASM_ZERO)));
  CHECK_EQ(99, r.Call(1));
}


TEST(Run_Wasm_ExprIf_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 ? 11 : 22;
  BUILD(r, WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                        WASM_I8(11),        // --
                        WASM_I8(22)));      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_ExprIf_P_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 ? 11 : 22;
  BUILD(r, WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                        WASM_I8(11),        // --
                        WASM_I8(22)));      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_CountDown) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(
            2, WASM_LOOP(
                   1, WASM_IF(WASM_GET_LOCAL(0),
                              WASM_BRV(0, WASM_SET_LOCAL(
                                              0, WASM_I32_SUB(WASM_GET_LOCAL(0),
                                                              WASM_I8(1)))))),
            WASM_GET_LOCAL(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_CountDown_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(
            2, WASM_LOOP(3, WASM_IF(WASM_NOT(WASM_GET_LOCAL(0)), WASM_BREAK(0)),
                         WASM_SET_LOCAL(
                             0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(1))),
                         WASM_CONTINUE(0)),
            WASM_GET_LOCAL(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_WhileCountDown) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(
               2, WASM_WHILE(WASM_GET_LOCAL(0),
                             WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0),
                                                            WASM_I8(1)))),
               WASM_GET_LOCAL(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_Loop_if_break1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_LOOP(2, WASM_IF(WASM_GET_LOCAL(0), WASM_BREAK(0)),
                                   WASM_SET_LOCAL(0, WASM_I8(99))),
                      WASM_GET_LOCAL(0)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10000, r.Call(10000));
  CHECK_EQ(-29, r.Call(-29));
}


TEST(Run_Wasm_Loop_if_break2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_LOOP(2, WASM_BR_IF(1, WASM_GET_LOCAL(0)),
                                   WASM_SET_LOCAL(0, WASM_I8(99))),
                      WASM_GET_LOCAL(0)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10000, r.Call(10000));
  CHECK_EQ(-29, r.Call(-29));
}


TEST(Run_Wasm_Loop_if_break_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(1, WASM_LOOP(2, WASM_IF(WASM_GET_LOCAL(0), WASM_BREAK(1)),
                                   WASM_SET_LOCAL(0, WASM_I8(93)))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(93, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10001, r.Call(10001));
  CHECK_EQ(-22, r.Call(-22));
}


TEST(Run_Wasm_LoadMemI32) {
  WasmRunner<int32_t> r(MachineType::Int32());
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  module.RandomizeMemory(1111);
  r.env()->module = &module;

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_I8(0)));

  memory[0] = 99999999;
  CHECK_EQ(99999999, r.Call(0));

  memory[0] = 88888888;
  CHECK_EQ(88888888, r.Call(0));

  memory[0] = 77777777;
  CHECK_EQ(77777777, r.Call(0));
}


TEST(Run_Wasm_LoadMemI32_oob) {
  WasmRunner<int32_t> r(MachineType::Uint32());
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  module.RandomizeMemory(1111);
  r.env()->module = &module;

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  memory[0] = 88888888;
  CHECK_EQ(88888888, r.Call(0u));
  for (uint32_t offset = 29; offset < 40; offset++) {
    CHECK_TRAP(r.Call(offset));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK_TRAP(r.Call(offset));
  }
}


TEST(Run_Wasm_LoadMemI32_oob_asm) {
  WasmRunner<int32_t> r(MachineType::Uint32());
  TestingModule module;
  module.asm_js = true;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  module.RandomizeMemory(1112);
  r.env()->module = &module;

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  memory[0] = 999999;
  CHECK_EQ(999999, r.Call(0u));
  // TODO(titzer): offset 29-31 should also be OOB.
  for (uint32_t offset = 32; offset < 40; offset++) {
    CHECK_EQ(0, r.Call(offset));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK_EQ(0, r.Call(offset));
  }
}


TEST(Run_Wasm_LoadMem_offset_oob) {
  TestingModule module;
  module.AddMemoryElems<int32_t>(8);

  static const MachineType machineTypes[] = {
      MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
      MachineType::Float64()};

  for (size_t m = 0; m < arraysize(machineTypes); m++) {
    module.RandomizeMemory(1116 + static_cast<int>(m));
    WasmRunner<int32_t> r(MachineType::Uint32());
    r.env()->module = &module;
    uint32_t boundary = 24 - WasmOpcodes::MemSize(machineTypes[m]);

    BUILD(r, WASM_LOAD_MEM_OFFSET(machineTypes[m], 8, WASM_GET_LOCAL(0)),
          WASM_ZERO);

    CHECK_EQ(0, r.Call(boundary));  // in bounds.

    for (uint32_t offset = boundary + 1; offset < boundary + 19; offset++) {
      CHECK_TRAP(r.Call(offset));  // out of bounds.
    }
  }
}


TEST(Run_Wasm_LoadMemI32_offset) {
  WasmRunner<int32_t> r(MachineType::Int32());
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(4);
  module.RandomizeMemory(1111);
  r.env()->module = &module;

  BUILD(r, WASM_LOAD_MEM_OFFSET(MachineType::Int32(), 4, WASM_GET_LOCAL(0)));

  memory[0] = 66666666;
  memory[1] = 77777777;
  memory[2] = 88888888;
  memory[3] = 99999999;
  CHECK_EQ(77777777, r.Call(0));
  CHECK_EQ(88888888, r.Call(4));
  CHECK_EQ(99999999, r.Call(8));

  memory[0] = 11111111;
  memory[1] = 22222222;
  memory[2] = 33333333;
  memory[3] = 44444444;
  CHECK_EQ(22222222, r.Call(0));
  CHECK_EQ(33333333, r.Call(4));
  CHECK_EQ(44444444, r.Call(8));
}


// TODO(titzer): Fix for mips and re-enable.
#if !V8_TARGET_ARCH_MIPS && !V8_TARGET_ARCH_MIPS64

TEST(Run_Wasm_LoadMemI32_const_oob) {
  TestingModule module;
  const int kMemSize = 12;
  module.AddMemoryElems<byte>(kMemSize);

  for (int offset = 0; offset < kMemSize + 5; offset++) {
    for (int index = 0; index < kMemSize + 5; index++) {
      WasmRunner<int32_t> r;
      r.env()->module = &module;
      module.RandomizeMemory();

      BUILD(r,
            WASM_LOAD_MEM_OFFSET(MachineType::Int32(), offset, WASM_I8(index)));

      if ((offset + index) <= (kMemSize - sizeof(int32_t))) {
        CHECK_EQ(module.raw_val_at<int32_t>(offset + index), r.Call());
      } else {
        CHECK_TRAP(r.Call());
      }
    }
  }
}

#endif


TEST(Run_Wasm_StoreMemI32_offset) {
  WasmRunner<int32_t> r(MachineType::Int32());
  const int32_t kWritten = 0xaabbccdd;
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(4);
  r.env()->module = &module;

  BUILD(r, WASM_STORE_MEM_OFFSET(MachineType::Int32(), 4, WASM_GET_LOCAL(0),
                                 WASM_I32(kWritten)));

  for (int i = 0; i < 2; i++) {
    module.RandomizeMemory(1111);
    memory[0] = 66666666;
    memory[1] = 77777777;
    memory[2] = 88888888;
    memory[3] = 99999999;
    CHECK_EQ(kWritten, r.Call(i * 4));
    CHECK_EQ(66666666, memory[0]);
    CHECK_EQ(i == 0 ? kWritten : 77777777, memory[1]);
    CHECK_EQ(i == 1 ? kWritten : 88888888, memory[2]);
    CHECK_EQ(i == 2 ? kWritten : 99999999, memory[3]);
  }
}


TEST(Run_Wasm_StoreMem_offset_oob) {
  TestingModule module;
  byte* memory = module.AddMemoryElems<byte>(32);

#if WASM_64
  static const MachineType machineTypes[] = {
      MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
      MachineType::Float64()};
#else
  static const MachineType machineTypes[] = {
      MachineType::Int8(),    MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(),  MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Float32(), MachineType::Float64()};
#endif

  for (size_t m = 0; m < arraysize(machineTypes); m++) {
    module.RandomizeMemory(1119 + static_cast<int>(m));
    WasmRunner<int32_t> r(MachineType::Uint32());
    r.env()->module = &module;

    BUILD(r, WASM_STORE_MEM_OFFSET(machineTypes[m], 8, WASM_GET_LOCAL(0),
                                   WASM_LOAD_MEM(machineTypes[m], WASM_ZERO)),
          WASM_ZERO);

    byte memsize = WasmOpcodes::MemSize(machineTypes[m]);
    uint32_t boundary = 24 - memsize;
    CHECK_EQ(0, r.Call(boundary));  // in bounds.
    CHECK_EQ(0, memcmp(&memory[0], &memory[8 + boundary], memsize));

    for (uint32_t offset = boundary + 1; offset < boundary + 19; offset++) {
      CHECK_TRAP(r.Call(offset));  // out of bounds.
    }
  }
}


#if WASM_64
TEST(Run_Wasm_F64ReinterpretI64) {
  WasmRunner<int64_t> r;
  TestingModule module;
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  r.env()->module = &module;

  BUILD(r, WASM_I64_REINTERPRET_F64(
               WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int64_t expected = static_cast<int64_t>(*i) * 0x300010001;
    memory[0] = expected;
    CHECK_EQ(expected, r.Call());
  }
}


TEST(Run_Wasm_I64ReinterpretF64) {
  WasmRunner<int64_t> r(MachineType::Int64());
  TestingModule module;
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  r.env()->module = &module;

  BUILD(r, WASM_BLOCK(
               2, WASM_STORE_MEM(MachineType::Float64(), WASM_ZERO,
                                 WASM_F64_REINTERPRET_I64(WASM_GET_LOCAL(0))),
               WASM_GET_LOCAL(0)));

  FOR_INT32_INPUTS(i) {
    int64_t expected = static_cast<int64_t>(*i) * 0x300010001;
    CHECK_EQ(expected, r.Call(expected));
    CHECK_EQ(expected, memory[0]);
  }
}


TEST(Run_Wasm_LoadMemI64) {
  WasmRunner<int64_t> r;
  TestingModule module;
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  module.RandomizeMemory(1111);
  r.env()->module = &module;

  BUILD(r, WASM_LOAD_MEM(MachineType::Int64(), WASM_I8(0)));

  memory[0] = 0xaabbccdd00112233LL;
  CHECK_EQ(0xaabbccdd00112233LL, r.Call());

  memory[0] = 0x33aabbccdd001122LL;
  CHECK_EQ(0x33aabbccdd001122LL, r.Call());

  memory[0] = 77777777;
  CHECK_EQ(77777777, r.Call());
}
#endif


TEST(Run_Wasm_LoadMemI32_P) {
  const int kNumElems = 8;
  WasmRunner<int32_t> r(MachineType::Int32());
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(kNumElems);
  module.RandomizeMemory(2222);
  r.env()->module = &module;

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumElems; i++) {
    CHECK_EQ(memory[i], r.Call(i * 4));
  }
}


TEST(Run_Wasm_MemI32_Sum) {
  WasmRunner<uint32_t> r(MachineType::Int32());
  const int kNumElems = 20;
  const byte kSum = r.AllocateLocal(kAstI32);
  TestingModule module;
  uint32_t* memory = module.AddMemoryElems<uint32_t>(kNumElems);
  r.env()->module = &module;

  BUILD(r, WASM_BLOCK(
               2, WASM_WHILE(
                      WASM_GET_LOCAL(0),
                      WASM_BLOCK(
                          2, WASM_SET_LOCAL(
                                 kSum, WASM_I32_ADD(
                                           WASM_GET_LOCAL(kSum),
                                           WASM_LOAD_MEM(MachineType::Int32(),
                                                         WASM_GET_LOCAL(0)))),
                          WASM_SET_LOCAL(
                              0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(4))))),
               WASM_GET_LOCAL(1)));

  // Run 4 trials.
  for (int i = 0; i < 3; i++) {
    module.RandomizeMemory(i * 33);
    uint32_t expected = 0;
    for (size_t j = kNumElems - 1; j > 0; j--) {
      expected += memory[j];
    }
    uint32_t result = r.Call(static_cast<int>(4 * (kNumElems - 1)));
    CHECK_EQ(expected, result);
  }
}


TEST(Run_Wasm_CheckMachIntsZero) {
  WasmRunner<uint32_t> r(MachineType::Int32());
  const int kNumElems = 55;
  TestingModule module;
  module.AddMemoryElems<uint32_t>(kNumElems);
  r.env()->module = &module;

  BUILD(r, kExprBlock, 2, kExprLoop, 1, kExprIf, kExprGetLocal, 0, kExprBr, 0,
        kExprIfElse, kExprI32LoadMem, 0, kExprGetLocal, 0, kExprBr, 2,
        kExprI8Const, 255, kExprSetLocal, 0, kExprI32Sub, kExprGetLocal, 0,
        kExprI8Const, 4, kExprI8Const, 0);

  module.BlankMemory();
  CHECK_EQ(0, r.Call((kNumElems - 1) * 4));
}


TEST(Run_Wasm_MemF32_Sum) {
  WasmRunner<int32_t> r(MachineType::Int32());
  const byte kSum = r.AllocateLocal(kAstF32);
  const int kSize = 5;
  TestingModule module;
  module.AddMemoryElems<float>(kSize);
  float* buffer = module.raw_mem_start<float>();
  buffer[0] = -99.25;
  buffer[1] = -888.25;
  buffer[2] = -77.25;
  buffer[3] = 66666.25;
  buffer[4] = 5555.25;
  r.env()->module = &module;

  BUILD(r, WASM_BLOCK(
               3, WASM_WHILE(
                      WASM_GET_LOCAL(0),
                      WASM_BLOCK(
                          2, WASM_SET_LOCAL(
                                 kSum, WASM_F32_ADD(
                                           WASM_GET_LOCAL(kSum),
                                           WASM_LOAD_MEM(MachineType::Float32(),
                                                         WASM_GET_LOCAL(0)))),
                          WASM_SET_LOCAL(
                              0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(4))))),
               WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO,
                              WASM_GET_LOCAL(kSum)),
               WASM_GET_LOCAL(0)));

  CHECK_EQ(0, r.Call(4 * (kSize - 1)));
  CHECK_NE(-99.25, buffer[0]);
  CHECK_EQ(71256.0f, buffer[0]);
}


#if WASM_64
TEST(Run_Wasm_MemI64_Sum) {
  WasmRunner<uint64_t> r(MachineType::Int32());
  const int kNumElems = 20;
  const byte kSum = r.AllocateLocal(kAstI64);
  TestingModule module;
  uint64_t* memory = module.AddMemoryElems<uint64_t>(kNumElems);
  r.env()->module = &module;

  BUILD(r, WASM_BLOCK(
               2, WASM_WHILE(
                      WASM_GET_LOCAL(0),
                      WASM_BLOCK(
                          2, WASM_SET_LOCAL(
                                 kSum, WASM_I64_ADD(
                                           WASM_GET_LOCAL(kSum),
                                           WASM_LOAD_MEM(MachineType::Int64(),
                                                         WASM_GET_LOCAL(0)))),
                          WASM_SET_LOCAL(
                              0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(8))))),
               WASM_GET_LOCAL(1)));

  // Run 4 trials.
  for (int i = 0; i < 3; i++) {
    module.RandomizeMemory(i * 33);
    uint64_t expected = 0;
    for (size_t j = kNumElems - 1; j > 0; j--) {
      expected += memory[j];
    }
    uint64_t result = r.Call(8 * (kNumElems - 1));
    CHECK_EQ(expected, result);
  }
}
#endif


template <typename T>
T GenerateAndRunFold(WasmOpcode binop, T* buffer, size_t size,
                     LocalType astType, MachineType memType) {
  WasmRunner<int32_t> r(MachineType::Int32());
  const byte kAccum = r.AllocateLocal(astType);
  TestingModule module;
  module.AddMemoryElems<T>(size);
  for (size_t i = 0; i < size; i++) {
    module.raw_mem_start<T>()[i] = buffer[i];
  }
  r.env()->module = &module;

  BUILD(
      r,
      WASM_BLOCK(
          4, WASM_SET_LOCAL(kAccum, WASM_LOAD_MEM(memType, WASM_ZERO)),
          WASM_WHILE(
              WASM_GET_LOCAL(0),
              WASM_BLOCK(
                  2, WASM_SET_LOCAL(
                         kAccum,
                         WASM_BINOP(binop, WASM_GET_LOCAL(kAccum),
                                    WASM_LOAD_MEM(memType, WASM_GET_LOCAL(0)))),
                  WASM_SET_LOCAL(
                      0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(sizeof(T)))))),
          WASM_STORE_MEM(memType, WASM_ZERO, WASM_GET_LOCAL(kAccum)),
          WASM_GET_LOCAL(0)));
  r.Call(static_cast<int>(sizeof(T) * (size - 1)));
  return module.raw_mem_at<double>(0);
}


TEST(Run_Wasm_MemF64_Mul) {
  const size_t kSize = 6;
  double buffer[kSize] = {1, 2, 2, 2, 2, 2};
  double result = GenerateAndRunFold<double>(kExprF64Mul, buffer, kSize,
                                             kAstF64, MachineType::Float64());
  CHECK_EQ(32, result);
}


TEST(Build_Wasm_Infinite_Loop) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // Only build the graph and compile, don't run.
  BUILD(r, WASM_INFINITE_LOOP);
}


TEST(Build_Wasm_Infinite_Loop_effect) {
  WasmRunner<int32_t> r(MachineType::Int32());
  TestingModule module;
  module.AddMemoryElems<int8_t>(16);
  r.env()->module = &module;

  // Only build the graph and compile, don't run.
  BUILD(r, WASM_LOOP(1, WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)));
}


TEST(Run_Wasm_Unreachable0a) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(2, WASM_BRV(0, WASM_I8(9)), WASM_RETURN(WASM_GET_LOCAL(0))));
  CHECK_EQ(9, r.Call(0));
  CHECK_EQ(9, r.Call(1));
}


TEST(Run_Wasm_Unreachable0b) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_BRV(0, WASM_I8(7)), WASM_UNREACHABLE));
  CHECK_EQ(7, r.Call(0));
  CHECK_EQ(7, r.Call(1));
}


TEST(Build_Wasm_Unreachable1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE);
}


TEST(Build_Wasm_Unreachable2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE, WASM_UNREACHABLE);
}


TEST(Build_Wasm_Unreachable3) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE, WASM_UNREACHABLE, WASM_UNREACHABLE);
}


TEST(Build_Wasm_UnreachableIf1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE, WASM_IF(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));
}


TEST(Build_Wasm_UnreachableIf2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE,
        WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_UNREACHABLE));
}


TEST(Run_Wasm_Unreachable_Load) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_BRV(0, WASM_GET_LOCAL(0)),
                      WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0))));
  CHECK_EQ(11, r.Call(11));
  CHECK_EQ(21, r.Call(21));
}


TEST(Run_Wasm_Infinite_Loop_not_taken1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_IF(WASM_GET_LOCAL(0), WASM_INFINITE_LOOP),
                      WASM_I8(45)));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(0));
}


TEST(Run_Wasm_Infinite_Loop_not_taken2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(1, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_BRV(0, WASM_I8(45)),
                                   WASM_INFINITE_LOOP)));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(1));
}


TEST(Run_Wasm_Infinite_Loop_not_taken2_brif) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_I8(45)),
                      WASM_INFINITE_LOOP));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(1));
}


static void TestBuildGraphForSimpleExpression(WasmOpcode opcode) {
  if (!WasmOpcodes::IsSupported(opcode)) return;

  Zone zone;
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  // Enable all optional operators.
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(&zone, MachineType::PointerRepresentation(),
                                 MachineOperatorBuilder::kAllOptionalOps);
  Graph graph(&zone);
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);
  FunctionEnv env;
  FunctionSig* sig = WasmOpcodes::Signature(opcode);
  init_env(&env, sig);

  if (sig->parameter_count() == 1) {
    byte code[] = {static_cast<byte>(opcode), kExprGetLocal, 0};
    TestBuildingGraph(&zone, &jsgraph, &env, code, code + arraysize(code));
  } else {
    CHECK_EQ(2, sig->parameter_count());
    byte code[] = {static_cast<byte>(opcode), kExprGetLocal, 0, kExprGetLocal,
                   1};
    TestBuildingGraph(&zone, &jsgraph, &env, code, code + arraysize(code));
  }
}


TEST(Build_Wasm_SimpleExprs) {
// Test that the decoder can build a graph for all supported simple expressions.
#define GRAPH_BUILD_TEST(name, opcode, sig) \
  TestBuildGraphForSimpleExpression(kExpr##name);

  FOREACH_SIMPLE_OPCODE(GRAPH_BUILD_TEST);

#undef GRAPH_BUILD_TEST
}


TEST(Run_Wasm_Int32LoadInt8_signext) {
  TestingModule module;
  const int kNumElems = 16;
  int8_t* memory = module.AddMemoryElems<int8_t>(kNumElems);
  module.RandomizeMemory();
  memory[0] = -1;
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  BUILD(r, WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumElems; i++) {
    CHECK_EQ(memory[i], r.Call(static_cast<int>(i)));
  }
}


TEST(Run_Wasm_Int32LoadInt8_zeroext) {
  TestingModule module;
  const int kNumElems = 16;
  byte* memory = module.AddMemory(kNumElems);
  module.RandomizeMemory(77);
  memory[0] = 255;
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  BUILD(r, WASM_LOAD_MEM(MachineType::Uint8(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumElems; i++) {
    CHECK_EQ(memory[i], r.Call(static_cast<int>(i)));
  }
}


TEST(Run_Wasm_Int32LoadInt16_signext) {
  TestingModule module;
  const int kNumBytes = 16;
  byte* memory = module.AddMemory(kNumBytes);
  module.RandomizeMemory(888);
  memory[1] = 200;
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  BUILD(r, WASM_LOAD_MEM(MachineType::Int16(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumBytes; i += 2) {
    int32_t expected = memory[i] | (static_cast<int8_t>(memory[i + 1]) << 8);
    CHECK_EQ(expected, r.Call(static_cast<int>(i)));
  }
}


TEST(Run_Wasm_Int32LoadInt16_zeroext) {
  TestingModule module;
  const int kNumBytes = 16;
  byte* memory = module.AddMemory(kNumBytes);
  module.RandomizeMemory(9999);
  memory[1] = 204;
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  BUILD(r, WASM_LOAD_MEM(MachineType::Uint16(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumBytes; i += 2) {
    int32_t expected = memory[i] | (memory[i + 1] << 8);
    CHECK_EQ(expected, r.Call(static_cast<int>(i)));
  }
}


TEST(Run_WasmInt32Global) {
  TestingModule module;
  int32_t* global = module.AddGlobal<int32_t>(MachineType::Int32());
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  // global = global + p0
  BUILD(r, WASM_STORE_GLOBAL(
               0, WASM_I32_ADD(WASM_LOAD_GLOBAL(0), WASM_GET_LOCAL(0))));

  *global = 116;
  for (int i = 9; i < 444444; i += 111111) {
    int32_t expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}


TEST(Run_WasmInt32Globals_DontAlias) {
  const int kNumGlobals = 3;
  TestingModule module;
  int32_t* globals[] = {module.AddGlobal<int32_t>(MachineType::Int32()),
                        module.AddGlobal<int32_t>(MachineType::Int32()),
                        module.AddGlobal<int32_t>(MachineType::Int32())};

  for (int g = 0; g < kNumGlobals; g++) {
    // global = global + p0
    WasmRunner<int32_t> r(MachineType::Int32());
    r.env()->module = &module;
    BUILD(r, WASM_STORE_GLOBAL(
                 g, WASM_I32_ADD(WASM_LOAD_GLOBAL(g), WASM_GET_LOCAL(0))));

    // Check that reading/writing global number {g} doesn't alter the others.
    *globals[g] = 116 * g;
    int32_t before[kNumGlobals];
    for (int i = 9; i < 444444; i += 111113) {
      int32_t sum = *globals[g] + i;
      for (int j = 0; j < kNumGlobals; j++) before[j] = *globals[j];
      r.Call(i);
      for (int j = 0; j < kNumGlobals; j++) {
        int32_t expected = j == g ? sum : before[j];
        CHECK_EQ(expected, *globals[j]);
      }
    }
  }
}


#if WASM_64
TEST(Run_WasmInt64Global) {
  TestingModule module;
  int64_t* global = module.AddGlobal<int64_t>(MachineType::Int64());
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  // global = global + p0
  BUILD(r, WASM_BLOCK(2, WASM_STORE_GLOBAL(
                             0, WASM_I64_ADD(
                                    WASM_LOAD_GLOBAL(0),
                                    WASM_I64_SCONVERT_I32(WASM_GET_LOCAL(0)))),
                      WASM_ZERO));

  *global = 0xFFFFFFFFFFFFFFFFLL;
  for (int i = 9; i < 444444; i += 111111) {
    int64_t expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}
#endif


TEST(Run_WasmFloat32Global) {
  TestingModule module;
  float* global = module.AddGlobal<float>(MachineType::Float32());
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  // global = global + p0
  BUILD(r, WASM_BLOCK(2, WASM_STORE_GLOBAL(
                             0, WASM_F32_ADD(
                                    WASM_LOAD_GLOBAL(0),
                                    WASM_F32_SCONVERT_I32(WASM_GET_LOCAL(0)))),
                      WASM_ZERO));

  *global = 1.25;
  for (int i = 9; i < 4444; i += 1111) {
    volatile float expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}


TEST(Run_WasmFloat64Global) {
  TestingModule module;
  double* global = module.AddGlobal<double>(MachineType::Float64());
  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;
  // global = global + p0
  BUILD(r, WASM_BLOCK(2, WASM_STORE_GLOBAL(
                             0, WASM_F64_ADD(
                                    WASM_LOAD_GLOBAL(0),
                                    WASM_F64_SCONVERT_I32(WASM_GET_LOCAL(0)))),
                      WASM_ZERO));

  *global = 1.25;
  for (int i = 9; i < 4444; i += 1111) {
    volatile double expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}


TEST(Run_WasmMixedGlobals) {
  TestingModule module;
  int32_t* unused = module.AddGlobal<int32_t>(MachineType::Int32());
  byte* memory = module.AddMemory(32);

  int8_t* var_int8 = module.AddGlobal<int8_t>(MachineType::Int8());
  uint8_t* var_uint8 = module.AddGlobal<uint8_t>(MachineType::Uint8());
  int16_t* var_int16 = module.AddGlobal<int16_t>(MachineType::Int16());
  uint16_t* var_uint16 = module.AddGlobal<uint16_t>(MachineType::Uint16());
  int32_t* var_int32 = module.AddGlobal<int32_t>(MachineType::Int32());
  uint32_t* var_uint32 = module.AddGlobal<uint32_t>(MachineType::Uint32());
  float* var_float = module.AddGlobal<float>(MachineType::Float32());
  double* var_double = module.AddGlobal<double>(MachineType::Float64());

  WasmRunner<int32_t> r(MachineType::Int32());
  r.env()->module = &module;

  BUILD(
      r,
      WASM_BLOCK(
          9,
          WASM_STORE_GLOBAL(1, WASM_LOAD_MEM(MachineType::Int8(), WASM_ZERO)),
          WASM_STORE_GLOBAL(2, WASM_LOAD_MEM(MachineType::Uint8(), WASM_ZERO)),
          WASM_STORE_GLOBAL(3, WASM_LOAD_MEM(MachineType::Int16(), WASM_ZERO)),
          WASM_STORE_GLOBAL(4, WASM_LOAD_MEM(MachineType::Uint16(), WASM_ZERO)),
          WASM_STORE_GLOBAL(5, WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
          WASM_STORE_GLOBAL(6, WASM_LOAD_MEM(MachineType::Uint32(), WASM_ZERO)),
          WASM_STORE_GLOBAL(7,
                            WASM_LOAD_MEM(MachineType::Float32(), WASM_ZERO)),
          WASM_STORE_GLOBAL(8,
                            WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO)),
          WASM_ZERO));

  memory[0] = 0xaa;
  memory[1] = 0xcc;
  memory[2] = 0x55;
  memory[3] = 0xee;
  memory[4] = 0x33;
  memory[5] = 0x22;
  memory[6] = 0x11;
  memory[7] = 0x99;
  r.Call(1);

  CHECK(static_cast<int8_t>(0xaa) == *var_int8);
  CHECK(static_cast<uint8_t>(0xaa) == *var_uint8);
  CHECK(static_cast<int16_t>(0xccaa) == *var_int16);
  CHECK(static_cast<uint16_t>(0xccaa) == *var_uint16);
  CHECK(static_cast<int32_t>(0xee55ccaa) == *var_int32);
  CHECK(static_cast<uint32_t>(0xee55ccaa) == *var_uint32);
  CHECK(bit_cast<float>(0xee55ccaa) == *var_float);
  CHECK(bit_cast<double>(0x99112233ee55ccaaULL) == *var_double);

  USE(unused);
}


#if WASM_64
// Test the WasmRunner with an Int64 return value and different numbers of
// Int64 parameters.
TEST(Run_TestI64WasmRunner) {
  {
    FOR_INT64_INPUTS(i) {
      WasmRunner<int64_t> r;
      BUILD(r, WASM_I64(*i));
      CHECK_EQ(*i, r.Call());
    }
  }
  {
    WasmRunner<int64_t> r(MachineType::Int64());
    BUILD(r, WASM_GET_LOCAL(0));
    FOR_INT64_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
    BUILD(r, WASM_I64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) { CHECK_EQ(*i + *j, r.Call(*i, *j)); }
    }
  }
  {
    WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64(),
                          MachineType::Int64());
    BUILD(r, WASM_I64_ADD(WASM_GET_LOCAL(0),
                          WASM_I64_ADD(WASM_GET_LOCAL(1), WASM_GET_LOCAL(2))));
    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) {
        CHECK_EQ(*i + *j + *j, r.Call(*i, *j, *j));
        CHECK_EQ(*j + *i + *j, r.Call(*j, *i, *j));
        CHECK_EQ(*j + *j + *i, r.Call(*j, *j, *i));
      }
    }
  }
  {
    WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64(),
                          MachineType::Int64(), MachineType::Int64());
    BUILD(r, WASM_I64_ADD(WASM_GET_LOCAL(0),
                          WASM_I64_ADD(WASM_GET_LOCAL(1),
                                       WASM_I64_ADD(WASM_GET_LOCAL(2),
                                                    WASM_GET_LOCAL(3)))));
    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) {
        CHECK_EQ(*i + *j + *j + *j, r.Call(*i, *j, *j, *j));
        CHECK_EQ(*j + *i + *j + *j, r.Call(*j, *i, *j, *j));
        CHECK_EQ(*j + *j + *i + *j, r.Call(*j, *j, *i, *j));
        CHECK_EQ(*j + *j + *j + *i, r.Call(*j, *j, *j, *i));
      }
    }
  }
}
#endif


TEST(Run_WasmCallEmpty) {
  const int32_t kExpected = -414444;
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_v());
  BUILD(t, WASM_I32(kExpected));
  uint32_t index = t.CompileAndAdd(&module);

  // Build the calling function.
  WasmRunner<int32_t> r;
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION0(index));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}


// TODO(tizer): Fix on arm and reenable.
#if !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_ARM64

TEST(Run_WasmCallF32StackParameter) {
  // Build the target function.
  LocalType param_types[20];
  for (int i = 0; i < 20; i++) param_types[i] = kAstF32;
  FunctionSig sig(1, 19, param_types);
  TestingModule module;
  WasmFunctionCompiler t(&sig);
  BUILD(t, WASM_GET_LOCAL(17));
  uint32_t index = t.CompileAndAdd(&module);

  // Build the calling function.
  WasmRunner<float> r;
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION(
               index, WASM_F32(1.0f), WASM_F32(2.0f), WASM_F32(4.0f),
               WASM_F32(8.0f), WASM_F32(16.0f), WASM_F32(32.0f),
               WASM_F32(64.0f), WASM_F32(128.0f), WASM_F32(256.0f),
               WASM_F32(1.5f), WASM_F32(2.5f), WASM_F32(4.5f), WASM_F32(8.5f),
               WASM_F32(16.5f), WASM_F32(32.5f), WASM_F32(64.5f),
               WASM_F32(128.5f), WASM_F32(256.5f), WASM_F32(512.5f)));

  float result = r.Call();
  CHECK_EQ(256.5f, result);
}


TEST(Run_WasmCallF64StackParameter) {
  // Build the target function.
  LocalType param_types[20];
  for (int i = 0; i < 20; i++) param_types[i] = kAstF64;
  FunctionSig sig(1, 19, param_types);
  TestingModule module;
  WasmFunctionCompiler t(&sig);
  BUILD(t, WASM_GET_LOCAL(17));
  uint32_t index = t.CompileAndAdd(&module);

  // Build the calling function.
  WasmRunner<double> r;
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION(index, WASM_F64(1.0), WASM_F64(2.0),
                              WASM_F64(4.0), WASM_F64(8.0), WASM_F64(16.0),
                              WASM_F64(32.0), WASM_F64(64.0), WASM_F64(128.0),
                              WASM_F64(256.0), WASM_F64(1.5), WASM_F64(2.5),
                              WASM_F64(4.5), WASM_F64(8.5), WASM_F64(16.5),
                              WASM_F64(32.5), WASM_F64(64.5), WASM_F64(128.5),
                              WASM_F64(256.5), WASM_F64(512.5)));

  float result = r.Call();
  CHECK_EQ(256.5, result);
}

#endif


TEST(Run_WasmCallVoid) {
  const byte kMemOffset = 8;
  const int32_t kElemNum = kMemOffset / sizeof(int32_t);
  const int32_t kExpected = -414444;
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  module.AddMemory(16);
  module.RandomizeMemory();
  WasmFunctionCompiler t(sigs.v_v());
  t.env.module = &module;
  BUILD(t, WASM_STORE_MEM(MachineType::Int32(), WASM_I8(kMemOffset),
                          WASM_I32(kExpected)));
  uint32_t index = t.CompileAndAdd(&module);

  // Build the calling function.
  WasmRunner<int32_t> r;
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION0(index),
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I8(kMemOffset)));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
  CHECK_EQ(kExpected, module.raw_mem_start<int32_t>()[kElemNum]);
}


TEST(Run_WasmCall_Int32Add) {
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_ii());
  BUILD(t, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  uint32_t index = t.CompileAndAdd(&module);

  // Build the caller function.
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION(index, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*i) +
                                              static_cast<uint32_t>(*j));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}


#if WASM_64
TEST(Run_WasmCall_Int64Sub) {
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.l_ll());
  BUILD(t, WASM_I64_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  uint32_t index = t.CompileAndAdd(&module);

  // Build the caller function.
  WasmRunner<int64_t> r(MachineType::Int64(), MachineType::Int64());
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION(index, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int64_t a = static_cast<int64_t>(*i) << 32 |
                  (static_cast<int64_t>(*j) | 0xFFFFFFFF);
      int64_t b = static_cast<int64_t>(*j) << 32 |
                  (static_cast<int64_t>(*i) | 0xFFFFFFFF);

      int64_t expected = static_cast<int64_t>(static_cast<uint64_t>(a) -
                                              static_cast<uint64_t>(b));
      CHECK_EQ(expected, r.Call(a, b));
    }
  }
}
#endif


TEST(Run_WasmCall_Float32Sub) {
  TestSignatures sigs;
  WasmFunctionCompiler t(sigs.f_ff());

  // Build the target function.
  TestingModule module;
  BUILD(t, WASM_F32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  uint32_t index = t.CompileAndAdd(&module);

  // Builder the caller function.
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  r.env()->module = &module;
  BUILD(r, WASM_CALL_FUNCTION(index, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) {
      volatile float expected = *i - *j;
      CheckFloatEq(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_WasmCall_Float64Sub) {
  WasmRunner<int32_t> r;
  TestingModule module;
  double* memory = module.AddMemoryElems<double>(16);
  r.env()->module = &module;

  // TODO(titzer): convert to a binop test.
  BUILD(r, WASM_BLOCK(
               2, WASM_STORE_MEM(
                      MachineType::Float64(), WASM_ZERO,
                      WASM_F64_SUB(
                          WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO),
                          WASM_LOAD_MEM(MachineType::Float64(), WASM_I8(8)))),
               WASM_I8(107)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      memory[0] = *i;
      memory[1] = *j;
      double expected = *i - *j;
      CHECK_EQ(107, r.Call());
      if (expected != expected) {
        CHECK(memory[0] != memory[0]);
      } else {
        CHECK_EQ(expected, memory[0]);
      }
    }
  }
}

#define ADD_CODE(vec, ...)                                              \
  do {                                                                  \
    byte __buf[] = {__VA_ARGS__};                                       \
    for (size_t i = 0; i < sizeof(__buf); i++) vec.push_back(__buf[i]); \
  } while (false)


static void Run_WasmMixedCall_N(int start) {
  const int kExpected = 6333;
  const int kElemSize = 8;
  TestSignatures sigs;

#if WASM_64
  static MachineType mixed[] = {
      MachineType::Int32(),   MachineType::Float32(), MachineType::Int64(),
      MachineType::Float64(), MachineType::Float32(), MachineType::Int64(),
      MachineType::Int32(),   MachineType::Float64(), MachineType::Float32(),
      MachineType::Float64(), MachineType::Int32(),   MachineType::Int64(),
      MachineType::Int32(),   MachineType::Int32()};
#else
  static MachineType mixed[] = {
      MachineType::Int32(),   MachineType::Float32(), MachineType::Float64(),
      MachineType::Float32(), MachineType::Int32(),   MachineType::Float64(),
      MachineType::Float32(), MachineType::Float64(), MachineType::Int32(),
      MachineType::Int32(),   MachineType::Int32()};
#endif

  int num_params = static_cast<int>(arraysize(mixed)) - start;
  for (int which = 0; which < num_params; which++) {
    Zone zone;
    TestingModule module;
    module.AddMemory(1024);
    MachineType* memtypes = &mixed[start];
    MachineType result = memtypes[which];

    // =========================================================================
    // Build the selector function.
    // =========================================================================
    uint32_t index;
    FunctionSig::Builder b(&zone, 1, num_params);
    b.AddReturn(WasmOpcodes::LocalTypeFor(result));
    for (int i = 0; i < num_params; i++) {
      b.AddParam(WasmOpcodes::LocalTypeFor(memtypes[i]));
    }
    WasmFunctionCompiler t(b.Build());
    t.env.module = &module;
    BUILD(t, WASM_GET_LOCAL(which));
    index = t.CompileAndAdd(&module);

    // =========================================================================
    // Build the calling function.
    // =========================================================================
    WasmRunner<int32_t> r;
    r.env()->module = &module;

    {
      std::vector<byte> code;
      ADD_CODE(code,
               static_cast<byte>(WasmOpcodes::LoadStoreOpcodeOf(result, true)),
               WasmOpcodes::LoadStoreAccessOf(false));
      ADD_CODE(code, WASM_ZERO);
      ADD_CODE(code, kExprCallFunction, static_cast<byte>(index));

      for (int i = 0; i < num_params; i++) {
        int offset = (i + 1) * kElemSize;
        ADD_CODE(code, WASM_LOAD_MEM(memtypes[i], WASM_I8(offset)));
      }

      ADD_CODE(code, WASM_I32(kExpected));
      size_t end = code.size();
      code.push_back(0);
      r.Build(&code[0], &code[end]);
    }

    // Run the code.
    for (int t = 0; t < 10; t++) {
      module.RandomizeMemory();
      CHECK_EQ(kExpected, r.Call());

      int size = WasmOpcodes::MemSize(result);
      for (int i = 0; i < size; i++) {
        int base = (which + 1) * kElemSize;
        byte expected = module.raw_mem_at<byte>(base + i);
        byte result = module.raw_mem_at<byte>(i);
        CHECK_EQ(expected, result);
      }
    }
  }
}


TEST(Run_WasmMixedCall_0) { Run_WasmMixedCall_N(0); }
TEST(Run_WasmMixedCall_1) { Run_WasmMixedCall_N(1); }
TEST(Run_WasmMixedCall_2) { Run_WasmMixedCall_N(2); }
TEST(Run_WasmMixedCall_3) { Run_WasmMixedCall_N(3); }


TEST(Run_Wasm_CountDown_expr) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_LOOP(
               3, WASM_IF(WASM_NOT(WASM_GET_LOCAL(0)),
                          WASM_BREAKV(0, WASM_GET_LOCAL(0))),
               WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(1))),
               WASM_CONTINUE(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_ExprBlock2a) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(0, WASM_I8(1))),
                      WASM_I8(1)));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock2b) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(0, WASM_I8(1))),
                      WASM_I8(2)));
  CHECK_EQ(2, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock2c) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_I8(1)),
                      WASM_I8(1)));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock2d) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(2, WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_I8(1)),
                      WASM_I8(2)));
  CHECK_EQ(2, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock_ManualSwitch) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(6, WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(1)),
                                 WASM_BRV(0, WASM_I8(11))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(2)),
                              WASM_BRV(0, WASM_I8(12))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(3)),
                              WASM_BRV(0, WASM_I8(13))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(4)),
                              WASM_BRV(0, WASM_I8(14))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(5)),
                              WASM_BRV(0, WASM_I8(15))),
                      WASM_I8(99)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(12, r.Call(2));
  CHECK_EQ(13, r.Call(3));
  CHECK_EQ(14, r.Call(4));
  CHECK_EQ(15, r.Call(5));
  CHECK_EQ(99, r.Call(6));
}


TEST(Run_Wasm_ExprBlock_ManualSwitch_brif) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(6, WASM_BRV_IF(0, WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(1)),
                                  WASM_I8(11)),
                   WASM_BRV_IF(0, WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(2)),
                               WASM_I8(12)),
                   WASM_BRV_IF(0, WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(3)),
                               WASM_I8(13)),
                   WASM_BRV_IF(0, WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(4)),
                               WASM_I8(14)),
                   WASM_BRV_IF(0, WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(5)),
                               WASM_I8(15)),
                   WASM_I8(99)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(12, r.Call(2));
  CHECK_EQ(13, r.Call(3));
  CHECK_EQ(14, r.Call(4));
  CHECK_EQ(15, r.Call(5));
  CHECK_EQ(99, r.Call(6));
}


TEST(Run_Wasm_nested_ifs) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());

  BUILD(r, WASM_IF_ELSE(
               WASM_GET_LOCAL(0),
               WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_I8(11), WASM_I8(12)),
               WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_I8(13), WASM_I8(14))));


  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}


TEST(Run_Wasm_ExprBlock_if) {
  WasmRunner<int32_t> r(MachineType::Int32());

  BUILD(r,
        WASM_BLOCK(1, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_BRV(0, WASM_I8(11)),
                                   WASM_BRV(0, WASM_I8(14)))));

  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(14, r.Call(0));
}


TEST(Run_Wasm_ExprBlock_nested_ifs) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());

  BUILD(r, WASM_BLOCK(
               1, WASM_IF_ELSE(
                      WASM_GET_LOCAL(0),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(0, WASM_I8(11)),
                                   WASM_BRV(0, WASM_I8(12))),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(0, WASM_I8(13)),
                                   WASM_BRV(0, WASM_I8(14))))));


  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}


TEST(Run_Wasm_ExprLoop_nested_ifs) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());

  BUILD(r, WASM_LOOP(
               1, WASM_IF_ELSE(
                      WASM_GET_LOCAL(0),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(1, WASM_I8(11)),
                                   WASM_BRV(1, WASM_I8(12))),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(1, WASM_I8(13)),
                                   WASM_BRV(1, WASM_I8(14))))));


  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}


#if WASM_64
TEST(Run_Wasm_LoadStoreI64_sx) {
  byte loads[] = {kExprI64LoadMem8S, kExprI64LoadMem16S, kExprI64LoadMem32S,
                  kExprI64LoadMem};

  for (size_t m = 0; m < arraysize(loads); m++) {
    WasmRunner<int64_t> r;
    TestingModule module;
    byte* memory = module.AddMemoryElems<byte>(16);
    r.env()->module = &module;

    byte code[] = {kExprI64StoreMem, 0, kExprI8Const, 8,
                   loads[m],         0, kExprI8Const, 0};

    r.Build(code, code + arraysize(code));

    // Try a bunch of different negative values.
    for (int i = -1; i >= -128; i -= 11) {
      int size = 1 << m;
      module.BlankMemory();
      memory[size - 1] = static_cast<byte>(i);  // set the high order byte.

      int64_t expected = static_cast<int64_t>(i) << ((size - 1) * 8);

      CHECK_EQ(expected, r.Call());
      CHECK_EQ(static_cast<byte>(i), memory[8 + size - 1]);
      for (int j = size; j < 8; j++) {
        CHECK_EQ(255, memory[8 + j]);
      }
    }
  }
}


#endif


TEST(Run_Wasm_SimpleCallIndirect) {
  Isolate* isolate = CcTest::InitIsolateOnce();

  WasmRunner<int32_t> r(MachineType::Int32());
  TestSignatures sigs;
  TestingModule module;
  r.env()->module = &module;
  WasmFunctionCompiler t1(sigs.i_ii());
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.CompileAndAdd(&module);

  WasmFunctionCompiler t2(sigs.i_ii());
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.CompileAndAdd(&module);

  // Signature table.
  module.AddSignature(sigs.f_ff());
  module.AddSignature(sigs.i_ii());
  module.AddSignature(sigs.d_dd());

  // Function table.
  int table_size = 2;
  module.module->function_table = new std::vector<uint16_t>;
  module.module->function_table->push_back(0);
  module.module->function_table->push_back(1);

  // Function table.
  Handle<FixedArray> fixed = isolate->factory()->NewFixedArray(2 * table_size);
  fixed->set(0, Smi::FromInt(1));
  fixed->set(1, Smi::FromInt(1));
  fixed->set(2, *module.function_code->at(0));
  fixed->set(3, *module.function_code->at(1));
  module.function_table = fixed;

  // Builder the caller function.
  BUILD(r, WASM_CALL_INDIRECT(1, WASM_GET_LOCAL(0), WASM_I8(66), WASM_I8(22)));

  CHECK_EQ(88, r.Call(0));
  CHECK_EQ(44, r.Call(1));
  CHECK_TRAP(r.Call(2));
}


TEST(Run_Wasm_MultipleCallIndirect) {
  Isolate* isolate = CcTest::InitIsolateOnce();

  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32(),
                        MachineType::Int32());
  TestSignatures sigs;
  TestingModule module;
  r.env()->module = &module;
  WasmFunctionCompiler t1(sigs.i_ii());
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.CompileAndAdd(&module);

  WasmFunctionCompiler t2(sigs.i_ii());
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.CompileAndAdd(&module);

  // Signature table.
  module.AddSignature(sigs.f_ff());
  module.AddSignature(sigs.i_ii());
  module.AddSignature(sigs.d_dd());

  // Function table.
  int table_size = 2;
  module.module->function_table = new std::vector<uint16_t>;
  module.module->function_table->push_back(0);
  module.module->function_table->push_back(1);

  // Function table.
  Handle<FixedArray> fixed = isolate->factory()->NewFixedArray(2 * table_size);
  fixed->set(0, Smi::FromInt(1));
  fixed->set(1, Smi::FromInt(1));
  fixed->set(2, *module.function_code->at(0));
  fixed->set(3, *module.function_code->at(1));
  module.function_table = fixed;

  // Builder the caller function.
  BUILD(r,
        WASM_I32_ADD(WASM_CALL_INDIRECT(1, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                        WASM_GET_LOCAL(2)),
                     WASM_CALL_INDIRECT(1, WASM_GET_LOCAL(1), WASM_GET_LOCAL(2),
                                        WASM_GET_LOCAL(0))));

  CHECK_EQ(5, r.Call(0, 1, 2));
  CHECK_EQ(19, r.Call(0, 1, 9));
  CHECK_EQ(1, r.Call(1, 0, 2));
  CHECK_EQ(1, r.Call(1, 0, 9));

  CHECK_TRAP(r.Call(0, 2, 1));
  CHECK_TRAP(r.Call(1, 2, 0));
  CHECK_TRAP(r.Call(2, 0, 1));
  CHECK_TRAP(r.Call(2, 1, 0));
}


// TODO(titzer): Fix for nosee4 and re-enable.
#if 0

TEST(Run_Wasm_F32Floor) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_FLOOR(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CheckFloatEq(floor(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F32Ceil) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_CEIL(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CheckFloatEq(ceil(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F32Trunc) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_TRUNC(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CheckFloatEq(trunc(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F32NearestInt) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_NEARESTINT(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CheckFloatEq(nearbyint(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F64Floor) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_FLOOR(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CheckDoubleEq(floor(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F64Ceil) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_CEIL(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CheckDoubleEq(ceil(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F64Trunc) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_TRUNC(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CheckDoubleEq(trunc(*i), r.Call(*i)); }
}


TEST(Run_Wasm_F64NearestInt) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_NEARESTINT(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CheckDoubleEq(nearbyint(*i), r.Call(*i)); }
}

#endif


TEST(Run_Wasm_F32Min) {
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_F32_MIN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) {
      float expected;
      if (*i < *j) {
        expected = *i;
      } else if (*j < *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CheckFloatEq(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_Wasm_F64Min) {
  WasmRunner<double> r(MachineType::Float64(), MachineType::Float64());
  BUILD(r, WASM_F64_MIN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      double expected;
      if (*i < *j) {
        expected = *i;
      } else if (*j < *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CheckDoubleEq(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_Wasm_F32Max) {
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_F32_MAX(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) {
      float expected;
      if (*i > *j) {
        expected = *i;
      } else if (*j > *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CheckFloatEq(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_Wasm_F64Max) {
  WasmRunner<double> r(MachineType::Float64(), MachineType::Float64());
  BUILD(r, WASM_F64_MAX(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      double expected;
      if (*i > *j) {
        expected = *i;
      } else if (*j > *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CheckDoubleEq(expected, r.Call(*i, *j));
    }
  }
}


#if WASM_64
TEST(Run_Wasm_F32SConvertI64) {
  WasmRunner<float> r(MachineType::Int64());
  BUILD(r, WASM_F32_SCONVERT_I64(WASM_GET_LOCAL(0)));
  FOR_INT64_INPUTS(i) { CHECK_EQ(static_cast<float>(*i), r.Call(*i)); }
}


#if !defined(_WIN64)
// TODO(ahaas): Fix this failure.
TEST(Run_Wasm_F32UConvertI64) {
  WasmRunner<float> r(MachineType::Uint64());
  BUILD(r, WASM_F32_UCONVERT_I64(WASM_GET_LOCAL(0)));
  FOR_UINT64_INPUTS(i) { CHECK_EQ(static_cast<float>(*i), r.Call(*i)); }
}
#endif


TEST(Run_Wasm_F64SConvertI64) {
  WasmRunner<double> r(MachineType::Int64());
  BUILD(r, WASM_F64_SCONVERT_I64(WASM_GET_LOCAL(0)));
  FOR_INT64_INPUTS(i) { CHECK_EQ(static_cast<double>(*i), r.Call(*i)); }
}


#if !defined(_WIN64)
// TODO(ahaas): Fix this failure.
TEST(Run_Wasm_F64UConvertI64) {
  WasmRunner<double> r(MachineType::Uint64());
  BUILD(r, WASM_F64_UCONVERT_I64(WASM_GET_LOCAL(0)));
  FOR_UINT64_INPUTS(i) { CHECK_EQ(static_cast<double>(*i), r.Call(*i)); }
}
#endif


TEST(Run_Wasm_I64SConvertF32) {
  WasmRunner<int64_t> r(MachineType::Float32());
  BUILD(r, WASM_I64_SCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(INT64_MAX) &&
        *i >= static_cast<float>(INT64_MIN)) {
      CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I64SConvertF64) {
  WasmRunner<int64_t> r(MachineType::Float64());
  BUILD(r, WASM_I64_SCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<double>(INT64_MAX) &&
        *i >= static_cast<double>(INT64_MIN)) {
      CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I64UConvertF32) {
  WasmRunner<uint64_t> r(MachineType::Float32());
  BUILD(r, WASM_I64_UCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(UINT64_MAX) && *i > -1) {
      CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I64UConvertF64) {
  WasmRunner<uint64_t> r(MachineType::Float64());
  BUILD(r, WASM_I64_UCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<float>(UINT64_MAX) && *i > -1) {
      CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}
#endif


// TODO(titzer): Fix and re-enable.
#if 0
TEST(Run_Wasm_I32SConvertF32) {
  WasmRunner<int32_t> r(MachineType::Float32());
  BUILD(r, WASM_I32_SCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(INT32_MAX) &&
        *i >= static_cast<float>(INT32_MIN)) {
      CHECK_EQ(static_cast<int32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I32SConvertF64) {
  WasmRunner<int32_t> r(MachineType::Float64());
  BUILD(r, WASM_I32_SCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<double>(INT32_MAX) &&
        *i >= static_cast<double>(INT32_MIN)) {
      CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I32UConvertF32) {
  WasmRunner<uint32_t> r(MachineType::Float32());
  BUILD(r, WASM_I32_UCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(UINT32_MAX) && *i > -1) {
      CHECK_EQ(static_cast<uint32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I32UConvertF64) {
  WasmRunner<uint32_t> r(MachineType::Float64());
  BUILD(r, WASM_I32_UCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<float>(UINT32_MAX) && *i > -1) {
      CHECK_EQ(static_cast<uint32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}
#endif


TEST(Run_Wasm_F64CopySign) {
  WasmRunner<double> r(MachineType::Float64(), MachineType::Float64());
  BUILD(r, WASM_F64_COPYSIGN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) { CheckDoubleEq(copysign(*i, *j), r.Call(*i, *j)); }
  }
}


// TODO(tizer): Fix on arm and reenable.
#if !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_ARM64

TEST(Run_Wasm_F32CopySign) {
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_F32_COPYSIGN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CheckFloatEq(copysign(*i, *j), r.Call(*i, *j)); }
  }
}

#endif

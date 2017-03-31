// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/platform/elapsed-timer.h"
#include "src/utils.h"
#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

// for even shorter tests.
#define B1(a) WASM_BLOCK(a)
#define B2(a, b) WASM_BLOCK(a, b)
#define B3(a, b, c) WASM_BLOCK(a, b, c)
#define RET(x) x, kExprReturn
#define RET_I8(x) WASM_I32V_2(x), kExprReturn

WASM_EXEC_TEST(Int32Const) {
  WasmRunner<int32_t> r(execution_mode);
  const int32_t kExpectedValue = 0x11223344;
  // return(kExpectedValue)
  BUILD(r, WASM_I32V_5(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}

WASM_EXEC_TEST(Int32Const_many) {
  FOR_INT32_INPUTS(i) {
    WasmRunner<int32_t> r(execution_mode);
    const int32_t kExpectedValue = *i;
    // return(kExpectedValue)
    BUILD(r, WASM_I32V(kExpectedValue));
    CHECK_EQ(kExpectedValue, r.Call());
  }
}

WASM_EXEC_TEST(GraphTrimming) {
  // This WebAssembly code requires graph trimming in the TurboFan compiler.
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, kExprGetLocal, 0, kExprGetLocal, 0, kExprGetLocal, 0, kExprI32RemS,
        kExprI32Eq, kExprGetLocal, 0, kExprI32DivS, kExprUnreachable);
  r.Call(1);
}

WASM_EXEC_TEST(Int32Param0) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // return(local[0])
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Int32Param0_fallthru) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // local[0]
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Int32Param1) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  // local[1]
  BUILD(r, WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(-111, *i)); }
}

WASM_EXEC_TEST(Int32Add) {
  WasmRunner<int32_t> r(execution_mode);
  // 11 + 44
  BUILD(r, WASM_I32_ADD(WASM_I32V_1(11), WASM_I32V_1(44)));
  CHECK_EQ(55, r.Call());
}

WASM_EXEC_TEST(Int32Add_P) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // p0 + 13
  BUILD(r, WASM_I32_ADD(WASM_I32V_1(13), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i + 13, r.Call(*i)); }
}

WASM_EXEC_TEST(Int32Add_P_fallthru) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // p0 + 13
  BUILD(r, WASM_I32_ADD(WASM_I32V_1(13), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i + 13, r.Call(*i)); }
}

static void RunInt32AddTest(WasmExecutionMode execution_mode, const byte* code,
                            size_t size) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  r.Build(code, code + size);
  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*i) +
                                              static_cast<uint32_t>(*j));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST(Int32Add_P2) {
  FLAG_wasm_mv_prototype = true;
  static const byte code[] = {
      WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
  RunInt32AddTest(execution_mode, code, sizeof(code));
}

WASM_EXEC_TEST(Int32Add_block1) {
  FLAG_wasm_mv_prototype = true;
  static const byte code[] = {
      WASM_BLOCK_TT(kWasmI32, kWasmI32, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
      kExprI32Add};
  RunInt32AddTest(execution_mode, code, sizeof(code));
}

WASM_EXEC_TEST(Int32Add_block2) {
  FLAG_wasm_mv_prototype = true;
  static const byte code[] = {
      WASM_BLOCK_TT(kWasmI32, kWasmI32, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                    kExprBr, DEPTH_0),
      kExprI32Add};
  RunInt32AddTest(execution_mode, code, sizeof(code));
}

WASM_EXEC_TEST(Int32Add_multi_if) {
  FLAG_wasm_mv_prototype = true;
  static const byte code[] = {
      WASM_IF_ELSE_TT(kWasmI32, kWasmI32, WASM_GET_LOCAL(0),
                      WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                      WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
      kExprI32Add};
  RunInt32AddTest(execution_mode, code, sizeof(code));
}

WASM_EXEC_TEST(Float32Add) {
  WasmRunner<int32_t> r(execution_mode);
  // int(11.5f + 44.5f)
  BUILD(r,
        WASM_I32_SCONVERT_F32(WASM_F32_ADD(WASM_F32(11.5f), WASM_F32(44.5f))));
  CHECK_EQ(56, r.Call());
}

WASM_EXEC_TEST(Float64Add) {
  WasmRunner<int32_t> r(execution_mode);
  // return int(13.5d + 43.5d)
  BUILD(r, WASM_I32_SCONVERT_F64(WASM_F64_ADD(WASM_F64(13.5), WASM_F64(43.5))));
  CHECK_EQ(57, r.Call());
}

void TestInt32Binop(WasmExecutionMode execution_mode, WasmOpcode opcode,
                    int32_t expected, int32_t a, int32_t b) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I32V(a), WASM_I32V(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
    // a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

WASM_EXEC_TEST(Int32Binops) {
  TestInt32Binop(execution_mode, kExprI32Add, 88888888, 33333333, 55555555);
  TestInt32Binop(execution_mode, kExprI32Sub, -1111111, 7777777, 8888888);
  TestInt32Binop(execution_mode, kExprI32Mul, 65130756, 88734, 734);
  TestInt32Binop(execution_mode, kExprI32DivS, -66, -4777344, 72384);
  TestInt32Binop(execution_mode, kExprI32DivU, 805306368, 0xF0000000, 5);
  TestInt32Binop(execution_mode, kExprI32RemS, -3, -3003, 1000);
  TestInt32Binop(execution_mode, kExprI32RemU, 4, 4004, 1000);
  TestInt32Binop(execution_mode, kExprI32And, 0xEE, 0xFFEE, 0xFF0000FF);
  TestInt32Binop(execution_mode, kExprI32Ior, 0xF0FF00FF, 0xF0F000EE,
                 0x000F0011);
  TestInt32Binop(execution_mode, kExprI32Xor, 0xABCDEF01, 0xABCDEFFF, 0xFE);
  TestInt32Binop(execution_mode, kExprI32Shl, 0xA0000000, 0xA, 28);
  TestInt32Binop(execution_mode, kExprI32ShrU, 0x07000010, 0x70000100, 4);
  TestInt32Binop(execution_mode, kExprI32ShrS, 0xFF000000, 0x80000000, 7);
  TestInt32Binop(execution_mode, kExprI32Ror, 0x01000000, 0x80000000, 7);
  TestInt32Binop(execution_mode, kExprI32Ror, 0x01000000, 0x80000000, 39);
  TestInt32Binop(execution_mode, kExprI32Rol, 0x00000040, 0x80000000, 7);
  TestInt32Binop(execution_mode, kExprI32Rol, 0x00000040, 0x80000000, 39);
  TestInt32Binop(execution_mode, kExprI32Eq, 1, -99, -99);
  TestInt32Binop(execution_mode, kExprI32Ne, 0, -97, -97);

  TestInt32Binop(execution_mode, kExprI32LtS, 1, -4, 4);
  TestInt32Binop(execution_mode, kExprI32LeS, 0, -2, -3);
  TestInt32Binop(execution_mode, kExprI32LtU, 1, 0, -6);
  TestInt32Binop(execution_mode, kExprI32LeU, 1, 98978, 0xF0000000);

  TestInt32Binop(execution_mode, kExprI32GtS, 1, 4, -4);
  TestInt32Binop(execution_mode, kExprI32GeS, 0, -3, -2);
  TestInt32Binop(execution_mode, kExprI32GtU, 1, -6, 0);
  TestInt32Binop(execution_mode, kExprI32GeU, 1, 0xF0000000, 98978);
}

void TestInt32Unop(WasmExecutionMode execution_mode, WasmOpcode opcode,
                   int32_t expected, int32_t a) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return op K
    BUILD(r, WASM_UNOP(opcode, WASM_I32V(a)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, int32_t> r(execution_mode);
    // return op a
    BUILD(r, WASM_UNOP(opcode, WASM_GET_LOCAL(0)));
    CHECK_EQ(expected, r.Call(a));
  }
}

WASM_EXEC_TEST(Int32Clz) {
  TestInt32Unop(execution_mode, kExprI32Clz, 0, 0x80001000);
  TestInt32Unop(execution_mode, kExprI32Clz, 1, 0x40000500);
  TestInt32Unop(execution_mode, kExprI32Clz, 2, 0x20000300);
  TestInt32Unop(execution_mode, kExprI32Clz, 3, 0x10000003);
  TestInt32Unop(execution_mode, kExprI32Clz, 4, 0x08050000);
  TestInt32Unop(execution_mode, kExprI32Clz, 5, 0x04006000);
  TestInt32Unop(execution_mode, kExprI32Clz, 6, 0x02000000);
  TestInt32Unop(execution_mode, kExprI32Clz, 7, 0x010000a0);
  TestInt32Unop(execution_mode, kExprI32Clz, 8, 0x00800c00);
  TestInt32Unop(execution_mode, kExprI32Clz, 9, 0x00400000);
  TestInt32Unop(execution_mode, kExprI32Clz, 10, 0x0020000d);
  TestInt32Unop(execution_mode, kExprI32Clz, 11, 0x00100f00);
  TestInt32Unop(execution_mode, kExprI32Clz, 12, 0x00080000);
  TestInt32Unop(execution_mode, kExprI32Clz, 13, 0x00041000);
  TestInt32Unop(execution_mode, kExprI32Clz, 14, 0x00020020);
  TestInt32Unop(execution_mode, kExprI32Clz, 15, 0x00010300);
  TestInt32Unop(execution_mode, kExprI32Clz, 16, 0x00008040);
  TestInt32Unop(execution_mode, kExprI32Clz, 17, 0x00004005);
  TestInt32Unop(execution_mode, kExprI32Clz, 18, 0x00002050);
  TestInt32Unop(execution_mode, kExprI32Clz, 19, 0x00001700);
  TestInt32Unop(execution_mode, kExprI32Clz, 20, 0x00000870);
  TestInt32Unop(execution_mode, kExprI32Clz, 21, 0x00000405);
  TestInt32Unop(execution_mode, kExprI32Clz, 22, 0x00000203);
  TestInt32Unop(execution_mode, kExprI32Clz, 23, 0x00000101);
  TestInt32Unop(execution_mode, kExprI32Clz, 24, 0x00000089);
  TestInt32Unop(execution_mode, kExprI32Clz, 25, 0x00000041);
  TestInt32Unop(execution_mode, kExprI32Clz, 26, 0x00000022);
  TestInt32Unop(execution_mode, kExprI32Clz, 27, 0x00000013);
  TestInt32Unop(execution_mode, kExprI32Clz, 28, 0x00000008);
  TestInt32Unop(execution_mode, kExprI32Clz, 29, 0x00000004);
  TestInt32Unop(execution_mode, kExprI32Clz, 30, 0x00000002);
  TestInt32Unop(execution_mode, kExprI32Clz, 31, 0x00000001);
  TestInt32Unop(execution_mode, kExprI32Clz, 32, 0x00000000);
}

WASM_EXEC_TEST(Int32Ctz) {
  TestInt32Unop(execution_mode, kExprI32Ctz, 32, 0x00000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 31, 0x80000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 30, 0x40000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 29, 0x20000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 28, 0x10000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 27, 0xa8000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 26, 0xf4000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 25, 0x62000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 24, 0x91000000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 23, 0xcd800000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 22, 0x09400000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 21, 0xaf200000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 20, 0xac100000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 19, 0xe0b80000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 18, 0x9ce40000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 17, 0xc7920000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 16, 0xb8f10000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 15, 0x3b9f8000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 14, 0xdb4c4000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 13, 0xe9a32000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 12, 0xfca61000);
  TestInt32Unop(execution_mode, kExprI32Ctz, 11, 0x6c8a7800);
  TestInt32Unop(execution_mode, kExprI32Ctz, 10, 0x8ce5a400);
  TestInt32Unop(execution_mode, kExprI32Ctz, 9, 0xcb7d0200);
  TestInt32Unop(execution_mode, kExprI32Ctz, 8, 0xcb4dc100);
  TestInt32Unop(execution_mode, kExprI32Ctz, 7, 0xdfbec580);
  TestInt32Unop(execution_mode, kExprI32Ctz, 6, 0x27a9db40);
  TestInt32Unop(execution_mode, kExprI32Ctz, 5, 0xde3bcb20);
  TestInt32Unop(execution_mode, kExprI32Ctz, 4, 0xd7e8a610);
  TestInt32Unop(execution_mode, kExprI32Ctz, 3, 0x9afdbc88);
  TestInt32Unop(execution_mode, kExprI32Ctz, 2, 0x9afdbc84);
  TestInt32Unop(execution_mode, kExprI32Ctz, 1, 0x9afdbc82);
  TestInt32Unop(execution_mode, kExprI32Ctz, 0, 0x9afdbc81);
}

WASM_EXEC_TEST(Int32Popcnt) {
  TestInt32Unop(execution_mode, kExprI32Popcnt, 32, 0xffffffff);
  TestInt32Unop(execution_mode, kExprI32Popcnt, 0, 0x00000000);
  TestInt32Unop(execution_mode, kExprI32Popcnt, 1, 0x00008000);
  TestInt32Unop(execution_mode, kExprI32Popcnt, 13, 0x12345678);
  TestInt32Unop(execution_mode, kExprI32Popcnt, 19, 0xfedcba09);
}

WASM_EXEC_TEST(I32Eqz) {
  TestInt32Unop(execution_mode, kExprI32Eqz, 0, 1);
  TestInt32Unop(execution_mode, kExprI32Eqz, 0, -1);
  TestInt32Unop(execution_mode, kExprI32Eqz, 0, -827343);
  TestInt32Unop(execution_mode, kExprI32Eqz, 0, 8888888);
  TestInt32Unop(execution_mode, kExprI32Eqz, 1, 0);
}

WASM_EXEC_TEST(I32Shl) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_I32_SHL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT32_INPUTS(i) {
    FOR_UINT32_INPUTS(j) {
      uint32_t expected = (*i) << (*j & 0x1f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST(I32Shr) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_I32_SHR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT32_INPUTS(i) {
    FOR_UINT32_INPUTS(j) {
      uint32_t expected = (*i) >> (*j & 0x1f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST(I32Sar) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_I32_SAR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = (*i) >> (*j & 0x1f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(Int32DivS_trap) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, -1));
  CHECK_TRAP(r.Call(kMin, 0));
}

WASM_EXEC_TEST_WITH_TRAP(Int32RemS_trap) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(33, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
}

WASM_EXEC_TEST_WITH_TRAP(Int32DivU_trap) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
}

WASM_EXEC_TEST_WITH_TRAP(Int32RemU_trap) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(17, r.Call(217, 100));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
}

WASM_EXEC_TEST_WITH_TRAP(Int32DivS_byzero_const) {
  for (int8_t denom = -2; denom < 8; ++denom) {
    WasmRunner<int32_t, int32_t> r(execution_mode);
    BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I32V_1(denom)));
    for (int32_t val = -7; val < 8; ++val) {
      if (denom == 0) {
        CHECK_TRAP(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}

WASM_EXEC_TEST(Int32AsmjsDivS_byzero_const) {
  for (int8_t denom = -2; denom < 8; ++denom) {
    WasmRunner<int32_t, int32_t> r(execution_mode);
    r.module().ChangeOriginToAsmjs();
    BUILD(r, WASM_I32_ASMJS_DIVS(WASM_GET_LOCAL(0), WASM_I32V_1(denom)));
    FOR_INT32_INPUTS(i) {
      if (denom == 0) {
        CHECK_EQ(0, r.Call(*i));
      } else if (denom == -1 && *i == std::numeric_limits<int32_t>::min()) {
        CHECK_EQ(std::numeric_limits<int32_t>::min(), r.Call(*i));
      } else {
        CHECK_EQ(*i / denom, r.Call(*i));
      }
    }
  }
}

WASM_EXEC_TEST(Int32AsmjsRemS_byzero_const) {
  for (int8_t denom = -2; denom < 8; ++denom) {
    WasmRunner<int32_t, int32_t> r(execution_mode);
    r.module().ChangeOriginToAsmjs();
    BUILD(r, WASM_I32_ASMJS_REMS(WASM_GET_LOCAL(0), WASM_I32V_1(denom)));
    FOR_INT32_INPUTS(i) {
      if (denom == 0) {
        CHECK_EQ(0, r.Call(*i));
      } else if (denom == -1 && *i == std::numeric_limits<int32_t>::min()) {
        CHECK_EQ(0, r.Call(*i));
      } else {
        CHECK_EQ(*i % denom, r.Call(*i));
      }
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(Int32DivU_byzero_const) {
  for (uint32_t denom = 0xfffffffe; denom < 8; ++denom) {
    WasmRunner<uint32_t, uint32_t> r(execution_mode);
    BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_I32V_1(denom)));

    for (uint32_t val = 0xfffffff0; val < 8; ++val) {
      if (denom == 0) {
        CHECK_TRAP(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(Int32DivS_trap_effect) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  r.module().AddMemoryElems<int32_t>(8);

  BUILD(r, WASM_IF_ELSE_I(
               WASM_GET_LOCAL(0),
               WASM_I32_DIVS(
                   WASM_BLOCK_I(WASM_STORE_MEM(MachineType::Int8(), WASM_ZERO,
                                               WASM_GET_LOCAL(0)),
                                WASM_GET_LOCAL(0)),
                   WASM_GET_LOCAL(1)),
               WASM_I32_DIVS(
                   WASM_BLOCK_I(WASM_STORE_MEM(MachineType::Int8(), WASM_ZERO,
                                               WASM_GET_LOCAL(0)),
                                WASM_GET_LOCAL(0)),
                   WASM_GET_LOCAL(1))));
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_TRAP(r.Call(8, 0));
  CHECK_TRAP(r.Call(4, 0));
  CHECK_TRAP(r.Call(0, 0));
}

void TestFloat32Binop(WasmExecutionMode execution_mode, WasmOpcode opcode,
                      int32_t expected, float a, float b) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_F32(a), WASM_F32(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, float, float> r(execution_mode);
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

void TestFloat32BinopWithConvert(WasmExecutionMode execution_mode,
                                 WasmOpcode opcode, int32_t expected, float a,
                                 float b) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return int(K op K)
    BUILD(r,
          WASM_I32_SCONVERT_F32(WASM_BINOP(opcode, WASM_F32(a), WASM_F32(b))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, float, float> r(execution_mode);
    // return int(a op b)
    BUILD(r, WASM_I32_SCONVERT_F32(
                 WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

void TestFloat32UnopWithConvert(WasmExecutionMode execution_mode,
                                WasmOpcode opcode, int32_t expected, float a) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return int(op(K))
    BUILD(r, WASM_I32_SCONVERT_F32(WASM_UNOP(opcode, WASM_F32(a))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, float> r(execution_mode);
    // return int(op(a))
    BUILD(r, WASM_I32_SCONVERT_F32(WASM_UNOP(opcode, WASM_GET_LOCAL(0))));
    CHECK_EQ(expected, r.Call(a));
  }
}

void TestFloat64Binop(WasmExecutionMode execution_mode, WasmOpcode opcode,
                      int32_t expected, double a, double b) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_F64(a), WASM_F64(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, double, double> r(execution_mode);
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

void TestFloat64BinopWithConvert(WasmExecutionMode execution_mode,
                                 WasmOpcode opcode, int32_t expected, double a,
                                 double b) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return int(K op K)
    BUILD(r,
          WASM_I32_SCONVERT_F64(WASM_BINOP(opcode, WASM_F64(a), WASM_F64(b))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, double, double> r(execution_mode);
    BUILD(r, WASM_I32_SCONVERT_F64(
                 WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

void TestFloat64UnopWithConvert(WasmExecutionMode execution_mode,
                                WasmOpcode opcode, int32_t expected, double a) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return int(op(K))
    BUILD(r, WASM_I32_SCONVERT_F64(WASM_UNOP(opcode, WASM_F64(a))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t, double> r(execution_mode);
    // return int(op(a))
    BUILD(r, WASM_I32_SCONVERT_F64(WASM_UNOP(opcode, WASM_GET_LOCAL(0))));
    CHECK_EQ(expected, r.Call(a));
  }
}

WASM_EXEC_TEST(Float32Binops) {
  TestFloat32Binop(execution_mode, kExprF32Eq, 1, 8.125f, 8.125f);
  TestFloat32Binop(execution_mode, kExprF32Ne, 1, 8.125f, 8.127f);
  TestFloat32Binop(execution_mode, kExprF32Lt, 1, -9.5f, -9.0f);
  TestFloat32Binop(execution_mode, kExprF32Le, 1, -1111.0f, -1111.0f);
  TestFloat32Binop(execution_mode, kExprF32Gt, 1, -9.0f, -9.5f);
  TestFloat32Binop(execution_mode, kExprF32Ge, 1, -1111.0f, -1111.0f);

  TestFloat32BinopWithConvert(execution_mode, kExprF32Add, 10, 3.5f, 6.5f);
  TestFloat32BinopWithConvert(execution_mode, kExprF32Sub, 2, 44.5f, 42.5f);
  TestFloat32BinopWithConvert(execution_mode, kExprF32Mul, -66, -132.1f, 0.5f);
  TestFloat32BinopWithConvert(execution_mode, kExprF32Div, 11, 22.1f, 2.0f);
}

WASM_EXEC_TEST(Float32Unops) {
  TestFloat32UnopWithConvert(execution_mode, kExprF32Abs, 8, 8.125f);
  TestFloat32UnopWithConvert(execution_mode, kExprF32Abs, 9, -9.125f);
  TestFloat32UnopWithConvert(execution_mode, kExprF32Neg, -213, 213.125f);
  TestFloat32UnopWithConvert(execution_mode, kExprF32Sqrt, 12, 144.4f);
}

WASM_EXEC_TEST(Float64Binops) {
  TestFloat64Binop(execution_mode, kExprF64Eq, 1, 16.25, 16.25);
  TestFloat64Binop(execution_mode, kExprF64Ne, 1, 16.25, 16.15);
  TestFloat64Binop(execution_mode, kExprF64Lt, 1, -32.4, 11.7);
  TestFloat64Binop(execution_mode, kExprF64Le, 1, -88.9, -88.9);
  TestFloat64Binop(execution_mode, kExprF64Gt, 1, 11.7, -32.4);
  TestFloat64Binop(execution_mode, kExprF64Ge, 1, -88.9, -88.9);

  TestFloat64BinopWithConvert(execution_mode, kExprF64Add, 100, 43.5, 56.5);
  TestFloat64BinopWithConvert(execution_mode, kExprF64Sub, 200, 12200.1,
                              12000.1);
  TestFloat64BinopWithConvert(execution_mode, kExprF64Mul, -33, 134, -0.25);
  TestFloat64BinopWithConvert(execution_mode, kExprF64Div, -1111, -2222.3, 2);
}

WASM_EXEC_TEST(Float64Unops) {
  TestFloat64UnopWithConvert(execution_mode, kExprF64Abs, 108, 108.125);
  TestFloat64UnopWithConvert(execution_mode, kExprF64Abs, 209, -209.125);
  TestFloat64UnopWithConvert(execution_mode, kExprF64Neg, -209, 209.125);
  TestFloat64UnopWithConvert(execution_mode, kExprF64Sqrt, 13, 169.4);
}

WASM_EXEC_TEST(Float32Neg) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_F32_NEG(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    CHECK_EQ(0x80000000,
             bit_cast<uint32_t>(*i) ^ bit_cast<uint32_t>(r.Call(*i)));
  }
}

WASM_EXEC_TEST(Float64Neg) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_F64_NEG(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    CHECK_EQ(0x8000000000000000,
             bit_cast<uint64_t>(*i) ^ bit_cast<uint64_t>(r.Call(*i)));
  }
}

WASM_EXEC_TEST(IfElse_P) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // if (p0) return 11; else return 22;
  BUILD(r, WASM_IF_ELSE_I(WASM_GET_LOCAL(0),  // --
                          WASM_I32V_1(11),    // --
                          WASM_I32V_1(22)));  // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}
#define EMPTY

WASM_EXEC_TEST(If_empty1) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_GET_LOCAL(0), kExprIf, kLocalVoid, kExprEnd, WASM_GET_LOCAL(1));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i - 9, *i)); }
}

WASM_EXEC_TEST(IfElse_empty1) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_GET_LOCAL(0), kExprIf, kLocalVoid, kExprElse, kExprEnd,
        WASM_GET_LOCAL(1));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i - 8, *i)); }
}

WASM_EXEC_TEST(IfElse_empty2) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_GET_LOCAL(0), kExprIf, kLocalVoid, WASM_NOP, kExprElse,
        kExprEnd, WASM_GET_LOCAL(1));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i - 7, *i)); }
}

WASM_EXEC_TEST(IfElse_empty3) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_GET_LOCAL(0), kExprIf, kLocalVoid, kExprElse, WASM_NOP,
        kExprEnd, WASM_GET_LOCAL(1));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i - 6, *i)); }
}

WASM_EXEC_TEST(If_chain1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // if (p0) 13; if (p0) 14; 15
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_NOP),
        WASM_IF(WASM_GET_LOCAL(0), WASM_NOP), WASM_I32V_1(15));
  FOR_INT32_INPUTS(i) { CHECK_EQ(15, r.Call(*i)); }
}

WASM_EXEC_TEST(If_chain_set) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  // if (p0) p1 = 73; if (p0) p1 = 74; p1
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(1, WASM_I32V_2(73))),
        WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(1, WASM_I32V_2(74))),
        WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 74 : *i;
    CHECK_EQ(expected, r.Call(*i, *i));
  }
}

WASM_EXEC_TEST(IfElse_Unreachable1) {
  WasmRunner<int32_t> r(execution_mode);
  // 0 ? unreachable : 27
  BUILD(r, WASM_IF_ELSE_I(WASM_ZERO,          // --
                          WASM_UNREACHABLE,   // --
                          WASM_I32V_1(27)));  // --
  CHECK_EQ(27, r.Call());
}

WASM_EXEC_TEST(IfElse_Unreachable2) {
  WasmRunner<int32_t> r(execution_mode);
  // 1 ? 28 : unreachable
  BUILD(r, WASM_IF_ELSE_I(WASM_I32V_1(1),      // --
                          WASM_I32V_1(28),     // --
                          WASM_UNREACHABLE));  // --
  CHECK_EQ(28, r.Call());
}

WASM_EXEC_TEST(Return12) {
  WasmRunner<int32_t> r(execution_mode);

  BUILD(r, RET_I8(12));
  CHECK_EQ(12, r.Call());
}

WASM_EXEC_TEST(Return17) {
  WasmRunner<int32_t> r(execution_mode);

  BUILD(r, WASM_BLOCK(RET_I8(17)), WASM_ZERO);
  CHECK_EQ(17, r.Call());
}

WASM_EXEC_TEST(Return_I32) {
  WasmRunner<int32_t, int32_t> r(execution_mode);

  BUILD(r, RET(WASM_GET_LOCAL(0)));

  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Return_F32) {
  WasmRunner<float, float> r(execution_mode);

  BUILD(r, RET(WASM_GET_LOCAL(0)));

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

WASM_EXEC_TEST(Return_F64) {
  WasmRunner<double, double> r(execution_mode);

  BUILD(r, RET(WASM_GET_LOCAL(0)));

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

WASM_EXEC_TEST(Select_float_parameters) {
  WasmRunner<float, float, float, int32_t> r(execution_mode);
  // return select(11, 22, a);
  BUILD(r,
        WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)));
  CHECK_FLOAT_EQ(2.0f, r.Call(2.0f, 1.0f, 1));
}

WASM_EXEC_TEST(Select) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // return select(11, 22, a);
  BUILD(r, WASM_SELECT(WASM_I32V_1(11), WASM_I32V_1(22), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(Select_strict1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // select(a=0, a=1, a=2); return a
  BUILD(r, WASM_SELECT(WASM_TEE_LOCAL(0, WASM_ZERO),
                       WASM_TEE_LOCAL(0, WASM_I32V_1(1)),
                       WASM_TEE_LOCAL(0, WASM_I32V_1(2))),
        WASM_DROP, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(2, r.Call(*i)); }
}

WASM_EXEC_TEST(Select_strict2) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmI32);
  // select(b=5, c=6, a)
  BUILD(r, WASM_SELECT(WASM_TEE_LOCAL(1, WASM_I32V_1(5)),
                       WASM_TEE_LOCAL(2, WASM_I32V_1(6)), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 5 : 6;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(Select_strict3) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmI32);
  // select(b=5, c=6, a=b)
  BUILD(r, WASM_SELECT(WASM_TEE_LOCAL(1, WASM_I32V_1(5)),
                       WASM_TEE_LOCAL(2, WASM_I32V_1(6)),
                       WASM_TEE_LOCAL(0, WASM_GET_LOCAL(1))));
  FOR_INT32_INPUTS(i) {
    int32_t expected = 5;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(BrIf_strict) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV_IF(0, WASM_GET_LOCAL(0),
                                    WASM_TEE_LOCAL(0, WASM_I32V_2(99)))));

  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Br_height) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(
               WASM_BLOCK(WASM_BRV_IFD(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)),
                          WASM_RETURN1(WASM_I32V_1(9))),
               WASM_BRV(0, WASM_I32V_1(8))));

  for (int32_t i = 0; i < 5; i++) {
    int32_t expected = i != 0 ? 8 : 9;
    CHECK_EQ(expected, r.Call(i));
  }
}

WASM_EXEC_TEST(Regression_660262) {
  WasmRunner<int32_t> r(execution_mode);
  r.module().AddMemoryElems<int32_t>(8);
  BUILD(r, kExprI32Const, 0x00, kExprI32Const, 0x00, kExprI32LoadMem, 0x00,
        0x0f, kExprBrTable, 0x00, 0x80, 0x00);  // entries=0
  r.Call();
}

WASM_EXEC_TEST(BrTable0a) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(0)))),
        WASM_I32V_2(91));
  FOR_INT32_INPUTS(i) { CHECK_EQ(91, r.Call(*i)); }
}

WASM_EXEC_TEST(BrTable0b) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r,
        B1(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 1, BR_TARGET(0), BR_TARGET(0)))),
        WASM_I32V_2(92));
  FOR_INT32_INPUTS(i) { CHECK_EQ(92, r.Call(*i)); }
}

WASM_EXEC_TEST(BrTable0c) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(
      r,
      B1(B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 1, BR_TARGET(0), BR_TARGET(1))),
            RET_I8(76))),
      WASM_I32V_2(77));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i == 0 ? 76 : 77;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(BrTable1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(0))), RET_I8(93));
  FOR_INT32_INPUTS(i) { CHECK_EQ(93, r.Call(*i)); }
}

WASM_EXEC_TEST(BrTable_loop) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r,
        B2(B1(WASM_LOOP(WASM_BR_TABLE(WASM_INC_LOCAL_BYV(0, 1), 2, BR_TARGET(2),
                                      BR_TARGET(1), BR_TARGET(0)))),
           RET_I8(99)),
        WASM_I32V_2(98));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(98, r.Call(-1));
  CHECK_EQ(98, r.Call(-2));
  CHECK_EQ(98, r.Call(-3));
  CHECK_EQ(98, r.Call(-100));
}

WASM_EXEC_TEST(BrTable_br) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r,
        B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 1, BR_TARGET(1), BR_TARGET(0))),
           RET_I8(91)),
        WASM_I32V_2(99));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(91, r.Call(1));
  CHECK_EQ(91, r.Call(2));
  CHECK_EQ(91, r.Call(3));
}

WASM_EXEC_TEST(BrTable_br2) {
  WasmRunner<int32_t, int32_t> r(execution_mode);

  BUILD(r, B2(B2(B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 3, BR_TARGET(1),
                                     BR_TARGET(2), BR_TARGET(3), BR_TARGET(0))),
                    RET_I8(85)),
                 RET_I8(86)),
              RET_I8(87)),
        WASM_I32V_2(88));
  CHECK_EQ(86, r.Call(0));
  CHECK_EQ(87, r.Call(1));
  CHECK_EQ(88, r.Call(2));
  CHECK_EQ(85, r.Call(3));
  CHECK_EQ(85, r.Call(4));
  CHECK_EQ(85, r.Call(5));
}

WASM_EXEC_TEST(BrTable4) {
  for (int i = 0; i < 4; ++i) {
    for (int t = 0; t < 4; ++t) {
      uint32_t cases[] = {0, 1, 2, 3};
      cases[i] = t;
      byte code[] = {B2(B2(B2(B2(B1(WASM_BR_TABLE(
                                     WASM_GET_LOCAL(0), 3, BR_TARGET(cases[0]),
                                     BR_TARGET(cases[1]), BR_TARGET(cases[2]),
                                     BR_TARGET(cases[3]))),
                                 RET_I8(70)),
                              RET_I8(71)),
                           RET_I8(72)),
                        RET_I8(73)),
                     WASM_I32V_2(75)};

      WasmRunner<int32_t, int32_t> r(execution_mode);
      r.Build(code, code + arraysize(code));

      for (int x = -3; x < 50; ++x) {
        int index = (x > 3 || x < 0) ? 3 : x;
        int32_t expected = 70 + cases[index];
        CHECK_EQ(expected, r.Call(x));
      }
    }
  }
}

WASM_EXEC_TEST(BrTable4x4) {
  for (byte a = 0; a < 4; ++a) {
    for (byte b = 0; b < 4; ++b) {
      for (byte c = 0; c < 4; ++c) {
        for (byte d = 0; d < 4; ++d) {
          for (int i = 0; i < 4; ++i) {
            uint32_t cases[] = {a, b, c, d};
            byte code[] = {
                B2(B2(B2(B2(B1(WASM_BR_TABLE(
                                WASM_GET_LOCAL(0), 3, BR_TARGET(cases[0]),
                                BR_TARGET(cases[1]), BR_TARGET(cases[2]),
                                BR_TARGET(cases[3]))),
                            RET_I8(50)),
                         RET_I8(51)),
                      RET_I8(52)),
                   RET_I8(53)),
                WASM_I32V_2(55)};

            WasmRunner<int32_t, int32_t> r(execution_mode);
            r.Build(code, code + arraysize(code));

            for (int x = -6; x < 47; ++x) {
              int index = (x > 3 || x < 0) ? 3 : x;
              int32_t expected = 50 + cases[index];
              CHECK_EQ(expected, r.Call(x));
            }
          }
        }
      }
    }
  }
}

WASM_EXEC_TEST(BrTable4_fallthru) {
  byte code[] = {
      B2(B2(B2(B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 3, BR_TARGET(0),
                                   BR_TARGET(1), BR_TARGET(2), BR_TARGET(3))),
                  WASM_INC_LOCAL_BY(1, 1)),
               WASM_INC_LOCAL_BY(1, 2)),
            WASM_INC_LOCAL_BY(1, 4)),
         WASM_INC_LOCAL_BY(1, 8)),
      WASM_GET_LOCAL(1)};

  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
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

WASM_EXEC_TEST(F32ReinterpretI32) {
  WasmRunner<int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);

  BUILD(r, WASM_I32_REINTERPRET_F32(
               WASM_LOAD_MEM(MachineType::Float32(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    r.module().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32ReinterpretF32) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);

  BUILD(r, WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO,
                          WASM_F32_REINTERPRET_I32(WASM_GET_LOCAL(0))),
        WASM_I32V_2(107));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    CHECK_EQ(107, r.Call(expected));
    CHECK_EQ(expected, r.module().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(SignallingNanSurvivesI32ReinterpretF32) {
  WasmRunner<int32_t> r(execution_mode);

  BUILD(r, WASM_I32_REINTERPRET_F32(
               WASM_SEQ(kExprF32Const, 0x00, 0x00, 0xa0, 0x7f)));

  // This is a signalling nan.
  CHECK_EQ(0x7fa00000, r.Call());
}

WASM_EXEC_TEST_WITH_TRAP(LoadMaxUint32Offset) {
  WasmRunner<int32_t> r(execution_mode);
  r.module().AddMemoryElems<int32_t>(8);

  BUILD(r, kExprI32Const, 0,  // index
        static_cast<byte>(v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(
            MachineType::Int32(), false)),  // --
        0,                                  // alignment
        U32V_5(0xffffffff));                // offset

  CHECK_TRAP32(r.Call());
}

WASM_EXEC_TEST(LoadStoreLoad) {
  WasmRunner<int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);

  BUILD(r, WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                          WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
        WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    r.module().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(VoidReturn1) {
  const int32_t kExpected = -414444;
  WasmRunner<int32_t> r(execution_mode);

  // Build the test function.
  WasmFunctionCompiler& test_func = r.NewFunction<void>();
  BUILD(test_func, kExprNop);

  // Build the calling function.
  BUILD(r, WASM_CALL_FUNCTION0(test_func.function_index()),
        WASM_I32V_3(kExpected));

  // Call and check.
  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}

WASM_EXEC_TEST(VoidReturn2) {
  const int32_t kExpected = -414444;
  WasmRunner<int32_t> r(execution_mode);

  // Build the test function.
  WasmFunctionCompiler& test_func = r.NewFunction<void>();
  BUILD(test_func, WASM_RETURN0);

  // Build the calling function.
  BUILD(r, WASM_CALL_FUNCTION0(test_func.function_index()),
        WASM_I32V_3(kExpected));

  // Call and check.
  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}

WASM_EXEC_TEST(BrEmpty) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BRV(0, WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(BrIfEmpty) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_empty) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, kExprBlock, kLocalVoid, kExprEnd, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_empty_br1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(WASM_BR(0)), WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_empty_brif1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK(WASM_BR_IF(0, WASM_ZERO)), WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_empty_brif2) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK(WASM_BR_IF(0, WASM_GET_LOCAL(1))), WASM_GET_LOCAL(0));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i, *i + 1)); }
}

WASM_EXEC_TEST(Block_i) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_f) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_BLOCK_F(WASM_GET_LOCAL(0)));
  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_d) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_BLOCK_D(WASM_GET_LOCAL(0)));
  FOR_FLOAT64_INPUTS(i) { CHECK_FLOAT_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Block_br2) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0))));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, static_cast<uint32_t>(r.Call(*i))); }
}

WASM_EXEC_TEST(Block_If_P) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // block { if (p0) break 51; 52; }
  BUILD(r, WASM_BLOCK_I(                               // --
               WASM_IF(WASM_GET_LOCAL(0),              // --
                       WASM_BRV(1, WASM_I32V_1(51))),  // --
               WASM_I32V_1(52)));                      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 51 : 52;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(Loop_empty) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, kExprLoop, kLocalVoid, kExprEnd, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Loop_i) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_LOOP_I(WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Loop_f) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_LOOP_F(WASM_GET_LOCAL(0)));
  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Loop_d) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_LOOP_D(WASM_GET_LOCAL(0)));
  FOR_FLOAT64_INPUTS(i) { CHECK_FLOAT_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Loop_empty_br1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(WASM_LOOP(WASM_BR(1))), WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Loop_empty_brif1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(WASM_LOOP(WASM_BR_IF(1, WASM_ZERO))), WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(Loop_empty_brif2) {
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_LOOP_I(WASM_BRV_IF(1, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i, *i + 1)); }
}

WASM_EXEC_TEST(Loop_empty_brif3) {
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_mode);
  BUILD(r, WASM_LOOP(WASM_BRV_IFD(1, WASM_GET_LOCAL(2), WASM_GET_LOCAL(0))),
        WASM_GET_LOCAL(1));
  FOR_UINT32_INPUTS(i) {
    FOR_UINT32_INPUTS(j) {
      CHECK_EQ(*i, r.Call(0, *i, *j));
      CHECK_EQ(*j, r.Call(1, *i, *j));
    }
  }
}

WASM_EXEC_TEST(Block_BrIf_P) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV_IFD(0, WASM_I32V_1(51), WASM_GET_LOCAL(0)),
                        WASM_I32V_1(52)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 51 : 52;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(Block_IfElse_P_assign) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // { if (p0) p0 = 71; else p0 = 72; return p0; }
  BUILD(r,                                                 // --
        WASM_IF_ELSE(WASM_GET_LOCAL(0),                    // --
                     WASM_SET_LOCAL(0, WASM_I32V_2(71)),   // --
                     WASM_SET_LOCAL(0, WASM_I32V_2(72))),  // --
        WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 71 : 72;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(Block_IfElse_P_return) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // if (p0) return 81; else return 82;
  BUILD(r,                               // --
        WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                     RET_I8(81),         // --
                     RET_I8(82)),        // --
        WASM_ZERO);                      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 81 : 82;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(Block_If_P_assign) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // { if (p0) p0 = 61; p0; }
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_I32V_1(61))),
        WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 61 : *i;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(DanglingAssign) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // { return 0; p0 = 0; }
  BUILD(r, WASM_BLOCK_I(RET_I8(99), WASM_TEE_LOCAL(0, WASM_ZERO)));
  CHECK_EQ(99, r.Call(1));
}

WASM_EXEC_TEST(ExprIf_P) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // p0 ? 11 : 22;
  BUILD(r, WASM_IF_ELSE_I(WASM_GET_LOCAL(0),  // --
                          WASM_I32V_1(11),    // --
                          WASM_I32V_1(22)));  // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(CountDown) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_LOOP(WASM_IFB(WASM_GET_LOCAL(0),
                              WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0),
                                                             WASM_I32V_1(1))),
                              WASM_BR(1))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}

WASM_EXEC_TEST(CountDown_fallthru) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(
      r,
      WASM_LOOP(
          WASM_IF(WASM_NOT(WASM_GET_LOCAL(0)), WASM_BRV(2, WASM_GET_LOCAL(0))),
          WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I32V_1(1))),
          WASM_CONTINUE(0)),
      WASM_GET_LOCAL(0));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}

WASM_EXEC_TEST(WhileCountDown) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_WHILE(WASM_GET_LOCAL(0),
                      WASM_SET_LOCAL(
                          0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I32V_1(1)))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}

WASM_EXEC_TEST(Loop_if_break1) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(2, WASM_GET_LOCAL(1))),
                     WASM_SET_LOCAL(0, WASM_I32V_2(99))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(99, r.Call(0, 11));
  CHECK_EQ(65, r.Call(3, 65));
  CHECK_EQ(10001, r.Call(10000, 10001));
  CHECK_EQ(-29, r.Call(-28, -29));
}

WASM_EXEC_TEST(Loop_if_break2) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_LOOP(WASM_BRV_IF(1, WASM_GET_LOCAL(1), WASM_GET_LOCAL(0)),
                     WASM_DROP, WASM_SET_LOCAL(0, WASM_I32V_2(99))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(99, r.Call(0, 33));
  CHECK_EQ(3, r.Call(1, 3));
  CHECK_EQ(10000, r.Call(99, 10000));
  CHECK_EQ(-29, r.Call(-11, -29));
}

WASM_EXEC_TEST(Loop_if_break_fallthru) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BR(2)),
                        WASM_SET_LOCAL(0, WASM_I32V_2(93)))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(93, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10001, r.Call(10001));
  CHECK_EQ(-22, r.Call(-22));
}

WASM_EXEC_TEST(Loop_if_break_fallthru2) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, B1(B1(WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BR(2)),
                           WASM_SET_LOCAL(0, WASM_I32V_2(93))))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(93, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10001, r.Call(10001));
  CHECK_EQ(-22, r.Call(-22));
}

WASM_EXEC_TEST(IfBreak1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SEQ(WASM_BR(0), WASM_UNREACHABLE)),
        WASM_I32V_2(91));
  CHECK_EQ(91, r.Call(0));
  CHECK_EQ(91, r.Call(1));
  CHECK_EQ(91, r.Call(-8734));
}

WASM_EXEC_TEST(IfBreak2) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SEQ(WASM_BR(0), RET_I8(77))),
        WASM_I32V_2(81));
  CHECK_EQ(81, r.Call(0));
  CHECK_EQ(81, r.Call(1));
  CHECK_EQ(81, r.Call(-8734));
}

WASM_EXEC_TEST(LoadMemI32) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);
  r.module().RandomizeMemory(1111);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO));

  r.module().WriteMemory(&memory[0], 99999999);
  CHECK_EQ(99999999, r.Call(0));

  r.module().WriteMemory(&memory[0], 88888888);
  CHECK_EQ(88888888, r.Call(0));

  r.module().WriteMemory(&memory[0], 77777777);
  CHECK_EQ(77777777, r.Call(0));
}

WASM_EXEC_TEST(LoadMemI32_alignment) {
  for (byte alignment = 0; alignment <= 2; ++alignment) {
    WasmRunner<int32_t, int32_t> r(execution_mode);
    int32_t* memory = r.module().AddMemoryElems<int32_t>(8);
    r.module().RandomizeMemory(1111);

    BUILD(r,
          WASM_LOAD_MEM_ALIGNMENT(MachineType::Int32(), WASM_ZERO, alignment));

    r.module().WriteMemory(&memory[0], 0x1a2b3c4d);
    CHECK_EQ(0x1a2b3c4d, r.Call(0));

    r.module().WriteMemory(&memory[0], 0x5e6f7a8b);
    CHECK_EQ(0x5e6f7a8b, r.Call(0));

    r.module().WriteMemory(&memory[0], 0x7ca0b1c2);
    CHECK_EQ(0x7ca0b1c2, r.Call(0));
  }
}

WASM_EXEC_TEST_WITH_TRAP(LoadMemI32_oob) {
  WasmRunner<int32_t, uint32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);
  r.module().RandomizeMemory(1111);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  r.module().WriteMemory(&memory[0], 88888888);
  CHECK_EQ(88888888, r.Call(0u));
  for (uint32_t offset = 29; offset < 40; ++offset) {
    CHECK_TRAP(r.Call(offset));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; ++offset) {
    CHECK_TRAP(r.Call(offset));
  }
}

WASM_EXEC_TEST_WITH_TRAP(LoadMem_offset_oob) {
  static const MachineType machineTypes[] = {
      MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
      MachineType::Float64()};

  for (size_t m = 0; m < arraysize(machineTypes); ++m) {
    WasmRunner<int32_t, uint32_t> r(execution_mode);
    r.module().AddMemoryElems<int32_t>(8);
    r.module().RandomizeMemory(1116 + static_cast<int>(m));

    uint32_t boundary = 24 - WasmOpcodes::MemSize(machineTypes[m]);

    BUILD(r, WASM_LOAD_MEM_OFFSET(machineTypes[m], 8, WASM_GET_LOCAL(0)),
          WASM_DROP, WASM_ZERO);

    CHECK_EQ(0, r.Call(boundary));  // in bounds.

    for (uint32_t offset = boundary + 1; offset < boundary + 19; ++offset) {
      CHECK_TRAP(r.Call(offset));  // out of bounds.
    }
  }
}

WASM_EXEC_TEST(LoadMemI32_offset) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(4);
  r.module().RandomizeMemory(1111);

  BUILD(r, WASM_LOAD_MEM_OFFSET(MachineType::Int32(), 4, WASM_GET_LOCAL(0)));

  r.module().WriteMemory(&memory[0], 66666666);
  r.module().WriteMemory(&memory[1], 77777777);
  r.module().WriteMemory(&memory[2], 88888888);
  r.module().WriteMemory(&memory[3], 99999999);
  CHECK_EQ(77777777, r.Call(0));
  CHECK_EQ(88888888, r.Call(4));
  CHECK_EQ(99999999, r.Call(8));

  r.module().WriteMemory(&memory[0], 11111111);
  r.module().WriteMemory(&memory[1], 22222222);
  r.module().WriteMemory(&memory[2], 33333333);
  r.module().WriteMemory(&memory[3], 44444444);
  CHECK_EQ(22222222, r.Call(0));
  CHECK_EQ(33333333, r.Call(4));
  CHECK_EQ(44444444, r.Call(8));
}

WASM_EXEC_TEST_WITH_TRAP(LoadMemI32_const_oob_misaligned) {
  const int kMemSize = 12;
  // TODO(titzer): Fix misaligned accesses on MIPS and re-enable.
  for (int offset = 0; offset < kMemSize + 5; ++offset) {
    for (int index = 0; index < kMemSize + 5; ++index) {
      WasmRunner<int32_t> r(execution_mode);
      r.module().AddMemoryElems<byte>(kMemSize);
      r.module().RandomizeMemory();

      BUILD(r, WASM_LOAD_MEM_OFFSET(MachineType::Int32(), offset,
                                    WASM_I32V_2(index)));

      if ((offset + index) <= static_cast<int>((kMemSize - sizeof(int32_t)))) {
        CHECK_EQ(r.module().raw_val_at<int32_t>(offset + index), r.Call());
      } else {
        CHECK_TRAP(r.Call());
      }
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(LoadMemI32_const_oob) {
  const int kMemSize = 24;
  for (int offset = 0; offset < kMemSize + 5; offset += 4) {
    for (int index = 0; index < kMemSize + 5; index += 4) {
      WasmRunner<int32_t> r(execution_mode);
      r.module().AddMemoryElems<byte>(kMemSize);
      r.module().RandomizeMemory();

      BUILD(r, WASM_LOAD_MEM_OFFSET(MachineType::Int32(), offset,
                                    WASM_I32V_2(index)));

      if ((offset + index) <= static_cast<int>((kMemSize - sizeof(int32_t)))) {
        CHECK_EQ(r.module().raw_val_at<int32_t>(offset + index), r.Call());
      } else {
        CHECK_TRAP(r.Call());
      }
    }
  }
}

WASM_EXEC_TEST(StoreMemI32_alignment) {
  const int32_t kWritten = 0x12345678;

  for (byte i = 0; i <= 2; ++i) {
    WasmRunner<int32_t, int32_t> r(execution_mode);
    int32_t* memory = r.module().AddMemoryElems<int32_t>(4);
    BUILD(r, WASM_STORE_MEM_ALIGNMENT(MachineType::Int32(), WASM_ZERO, i,
                                      WASM_GET_LOCAL(0)),
          WASM_GET_LOCAL(0));
    r.module().RandomizeMemory(1111);
    memory[0] = 0;

    CHECK_EQ(kWritten, r.Call(kWritten));
    CHECK_EQ(kWritten, r.module().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(StoreMemI32_offset) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(4);
  const int32_t kWritten = 0xaabbccdd;

  BUILD(r, WASM_STORE_MEM_OFFSET(MachineType::Int32(), 4, WASM_GET_LOCAL(0),
                                 WASM_I32V_5(kWritten)),
        WASM_I32V_5(kWritten));

  for (int i = 0; i < 2; ++i) {
    r.module().RandomizeMemory(1111);
    r.module().WriteMemory(&memory[0], 66666666);
    r.module().WriteMemory(&memory[1], 77777777);
    r.module().WriteMemory(&memory[2], 88888888);
    r.module().WriteMemory(&memory[3], 99999999);
    CHECK_EQ(kWritten, r.Call(i * 4));
    CHECK_EQ(66666666, r.module().ReadMemory(&memory[0]));
    CHECK_EQ(i == 0 ? kWritten : 77777777, r.module().ReadMemory(&memory[1]));
    CHECK_EQ(i == 1 ? kWritten : 88888888, r.module().ReadMemory(&memory[2]));
    CHECK_EQ(i == 2 ? kWritten : 99999999, r.module().ReadMemory(&memory[3]));
  }
}

WASM_EXEC_TEST_WITH_TRAP(StoreMem_offset_oob) {
  // 64-bit cases are handled in test-run-wasm-64.cc
  static const MachineType machineTypes[] = {
      MachineType::Int8(),    MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(),  MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Float32(), MachineType::Float64()};

  for (size_t m = 0; m < arraysize(machineTypes); ++m) {
    WasmRunner<int32_t, uint32_t> r(execution_mode);
    byte* memory = r.module().AddMemoryElems<byte>(32);

    r.module().RandomizeMemory(1119 + static_cast<int>(m));

    BUILD(r, WASM_STORE_MEM_OFFSET(machineTypes[m], 8, WASM_GET_LOCAL(0),
                                   WASM_LOAD_MEM(machineTypes[m], WASM_ZERO)),
          WASM_ZERO);

    byte memsize = WasmOpcodes::MemSize(machineTypes[m]);
    uint32_t boundary = 24 - memsize;
    CHECK_EQ(0, r.Call(boundary));  // in bounds.
    CHECK_EQ(0, memcmp(&memory[0], &memory[8 + boundary], memsize));

    for (uint32_t offset = boundary + 1; offset < boundary + 19; ++offset) {
      CHECK_TRAP(r.Call(offset));  // out of bounds.
    }
  }
}

WASM_EXEC_TEST(LoadMemI32_P) {
  const int kNumElems = 8;
  WasmRunner<int32_t, int32_t> r(execution_mode);
  int32_t* memory = r.module().AddMemoryElems<int32_t>(kNumElems);
  r.module().RandomizeMemory(2222);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumElems; ++i) {
    CHECK_EQ(r.module().ReadMemory(&memory[i]), r.Call(i * 4));
  }
}

WASM_EXEC_TEST(MemI32_Sum) {
  const int kNumElems = 20;
  WasmRunner<uint32_t, int32_t> r(execution_mode);
  uint32_t* memory = r.module().AddMemoryElems<uint32_t>(kNumElems);
  const byte kSum = r.AllocateLocal(kWasmI32);

  BUILD(r, WASM_WHILE(
               WASM_GET_LOCAL(0),
               WASM_BLOCK(
                   WASM_SET_LOCAL(
                       kSum, WASM_I32_ADD(WASM_GET_LOCAL(kSum),
                                          WASM_LOAD_MEM(MachineType::Int32(),
                                                        WASM_GET_LOCAL(0)))),
                   WASM_SET_LOCAL(
                       0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I32V_1(4))))),
        WASM_GET_LOCAL(1));

  // Run 4 trials.
  for (int i = 0; i < 3; ++i) {
    r.module().RandomizeMemory(i * 33);
    uint32_t expected = 0;
    for (size_t j = kNumElems - 1; j > 0; --j) {
      expected += r.module().ReadMemory(&memory[j]);
    }
    uint32_t result = r.Call(4 * (kNumElems - 1));
    CHECK_EQ(expected, result);
  }
}

WASM_EXEC_TEST(CheckMachIntsZero) {
  const int kNumElems = 55;
  WasmRunner<uint32_t, int32_t> r(execution_mode);
  r.module().AddMemoryElems<uint32_t>(kNumElems);

  BUILD(r,                               // --
        /**/ kExprLoop, kLocalVoid,      // --
        /*  */ kExprGetLocal, 0,         // --
        /*  */ kExprIf, kLocalVoid,      // --
        /*    */ kExprGetLocal, 0,       // --
        /*    */ kExprI32LoadMem, 0, 0,  // --
        /*    */ kExprIf, kLocalVoid,    // --
        /*      */ kExprI32Const, 127,   // --
        /*      */ kExprReturn,          // --
        /*    */ kExprEnd,               // --
        /*    */ kExprGetLocal, 0,       // --
        /*    */ kExprI32Const, 4,       // --
        /*    */ kExprI32Sub,            // --
        /*    */ kExprTeeLocal, 0,       // --
        /*    */ kExprBr, DEPTH_0,       // --
        /*  */ kExprEnd,                 // --
        /**/ kExprEnd,                   // --
        /**/ kExprI32Const, 0);          // --

  r.module().BlankMemory();
  CHECK_EQ(0, r.Call((kNumElems - 1) * 4));
}

WASM_EXEC_TEST(MemF32_Sum) {
  const int kSize = 5;
  WasmRunner<int32_t, int32_t> r(execution_mode);
  r.module().AddMemoryElems<float>(kSize);
  float* buffer = r.module().raw_mem_start<float>();
  r.module().WriteMemory(&buffer[0], -99.25f);
  r.module().WriteMemory(&buffer[1], -888.25f);
  r.module().WriteMemory(&buffer[2], -77.25f);
  r.module().WriteMemory(&buffer[3], 66666.25f);
  r.module().WriteMemory(&buffer[4], 5555.25f);
  const byte kSum = r.AllocateLocal(kWasmF32);

  BUILD(r, WASM_WHILE(
               WASM_GET_LOCAL(0),
               WASM_BLOCK(
                   WASM_SET_LOCAL(
                       kSum, WASM_F32_ADD(WASM_GET_LOCAL(kSum),
                                          WASM_LOAD_MEM(MachineType::Float32(),
                                                        WASM_GET_LOCAL(0)))),
                   WASM_SET_LOCAL(
                       0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I32V_1(4))))),
        WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO, WASM_GET_LOCAL(kSum)),
        WASM_GET_LOCAL(0));

  CHECK_EQ(0, r.Call(4 * (kSize - 1)));
  CHECK_NE(-99.25f, r.module().ReadMemory(&buffer[0]));
  CHECK_EQ(71256.0f, r.module().ReadMemory(&buffer[0]));
}

template <typename T>
T GenerateAndRunFold(WasmExecutionMode execution_mode, WasmOpcode binop,
                     T* buffer, uint32_t size, ValueType astType,
                     MachineType memType) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  T* memory = r.module().AddMemoryElems<T>(size);
  for (uint32_t i = 0; i < size; ++i) {
    r.module().WriteMemory(&memory[i], buffer[i]);
  }
  const byte kAccum = r.AllocateLocal(astType);

  BUILD(
      r, WASM_SET_LOCAL(kAccum, WASM_LOAD_MEM(memType, WASM_ZERO)),
      WASM_WHILE(
          WASM_GET_LOCAL(0),
          WASM_BLOCK(WASM_SET_LOCAL(
                         kAccum,
                         WASM_BINOP(binop, WASM_GET_LOCAL(kAccum),
                                    WASM_LOAD_MEM(memType, WASM_GET_LOCAL(0)))),
                     WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0),
                                                    WASM_I32V_1(sizeof(T)))))),
      WASM_STORE_MEM(memType, WASM_ZERO, WASM_GET_LOCAL(kAccum)),
      WASM_GET_LOCAL(0));
  r.Call(static_cast<int>(sizeof(T) * (size - 1)));
  return r.module().ReadMemory(&memory[0]);
}

WASM_EXEC_TEST(MemF64_Mul) {
  const size_t kSize = 6;
  double buffer[kSize] = {1, 2, 2, 2, 2, 2};
  double result =
      GenerateAndRunFold<double>(execution_mode, kExprF64Mul, buffer, kSize,
                                 kWasmF64, MachineType::Float64());
  CHECK_EQ(32, result);
}

WASM_EXEC_TEST(Build_Wasm_Infinite_Loop) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  // Only build the graph and compile, don't run.
  BUILD(r, WASM_INFINITE_LOOP, WASM_ZERO);
}

WASM_EXEC_TEST(Build_Wasm_Infinite_Loop_effect) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  r.module().AddMemoryElems<int8_t>(16);

  // Only build the graph and compile, don't run.
  BUILD(r, WASM_LOOP(WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO), WASM_DROP),
        WASM_ZERO);
}

WASM_EXEC_TEST(Unreachable0a) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV(0, WASM_I32V_1(9)), RET(WASM_GET_LOCAL(0))));
  CHECK_EQ(9, r.Call(0));
  CHECK_EQ(9, r.Call(1));
}

WASM_EXEC_TEST(Unreachable0b) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV(0, WASM_I32V_1(7)), WASM_UNREACHABLE));
  CHECK_EQ(7, r.Call(0));
  CHECK_EQ(7, r.Call(1));
}

TEST(Build_Wasm_Unreachable1) {
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_UNREACHABLE);
}

TEST(Build_Wasm_Unreachable2) {
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_UNREACHABLE, WASM_UNREACHABLE);
}

TEST(Build_Wasm_Unreachable3) {
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_UNREACHABLE, WASM_UNREACHABLE, WASM_UNREACHABLE);
}

TEST(Build_Wasm_UnreachableIf1) {
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_UNREACHABLE,
        WASM_IF(WASM_GET_LOCAL(0), WASM_SEQ(WASM_GET_LOCAL(0), WASM_DROP)),
        WASM_ZERO);
}

TEST(Build_Wasm_UnreachableIf2) {
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_UNREACHABLE,
        WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_UNREACHABLE));
}

WASM_EXEC_TEST(Unreachable_Load) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  r.module().AddMemory(8);
  BUILD(r, WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0)),
                        WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0))));
  CHECK_EQ(11, r.Call(11));
  CHECK_EQ(21, r.Call(21));
}

WASM_EXEC_TEST(BrV_Fallthrough) {
  WasmRunner<int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BLOCK(WASM_BRV(1, WASM_I32V_1(42))),
                        WASM_I32V_1(22)));
  CHECK_EQ(42, r.Call());
}

WASM_EXEC_TEST(Infinite_Loop_not_taken1) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_INFINITE_LOOP), WASM_I32V_1(45));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(0));
}

WASM_EXEC_TEST(Infinite_Loop_not_taken2) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(
               WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_I32V_1(45)),
                            WASM_INFINITE_LOOP),
               WASM_ZERO));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(1));
}

WASM_EXEC_TEST(Infinite_Loop_not_taken2_brif) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV_IF(0, WASM_I32V_1(45), WASM_GET_LOCAL(0)),
                        WASM_INFINITE_LOOP));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(1));
}

static void TestBuildGraphForSimpleExpression(WasmOpcode opcode) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  Zone zone(isolate->allocator(), ZONE_NAME);
  HandleScope scope(isolate);
  // Enable all optional operators.
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(&zone, MachineType::PointerRepresentation(),
                                 MachineOperatorBuilder::kAllOptionalOps);
  Graph graph(&zone);
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);
  FunctionSig* sig = WasmOpcodes::Signature(opcode);

  if (sig->parameter_count() == 1) {
    byte code[] = {WASM_NO_LOCALS, kExprGetLocal, 0, static_cast<byte>(opcode),
                   WASM_END};
    TestBuildingGraph(&zone, &jsgraph, nullptr, sig, nullptr, code,
                      code + arraysize(code));
  } else {
    CHECK_EQ(2, sig->parameter_count());
    byte code[] = {WASM_NO_LOCALS,
                   kExprGetLocal,
                   0,
                   kExprGetLocal,
                   1,
                   static_cast<byte>(opcode),
                   WASM_END};
    TestBuildingGraph(&zone, &jsgraph, nullptr, sig, nullptr, code,
                      code + arraysize(code));
  }
}

TEST(Build_Wasm_SimpleExprs) {
// Test that the decoder can build a graph for all supported simple expressions.
#define GRAPH_BUILD_TEST(name, opcode, sig) \
  TestBuildGraphForSimpleExpression(kExpr##name);

  FOREACH_SIMPLE_OPCODE(GRAPH_BUILD_TEST);

#undef GRAPH_BUILD_TEST
}

WASM_EXEC_TEST(Int32LoadInt8_signext) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  const int kNumElems = 16;
  int8_t* memory = r.module().AddMemoryElems<int8_t>(kNumElems);
  r.module().RandomizeMemory();
  memory[0] = -1;
  BUILD(r, WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumElems; ++i) {
    CHECK_EQ(memory[i], r.Call(i));
  }
}

WASM_EXEC_TEST(Int32LoadInt8_zeroext) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  const int kNumElems = 16;
  byte* memory = r.module().AddMemory(kNumElems);
  r.module().RandomizeMemory(77);
  memory[0] = 255;
  BUILD(r, WASM_LOAD_MEM(MachineType::Uint8(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumElems; ++i) {
    CHECK_EQ(memory[i], r.Call(i));
  }
}

WASM_EXEC_TEST(Int32LoadInt16_signext) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  const int kNumBytes = 16;
  byte* memory = r.module().AddMemory(kNumBytes);
  r.module().RandomizeMemory(888);
  memory[1] = 200;
  BUILD(r, WASM_LOAD_MEM(MachineType::Int16(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumBytes; i += 2) {
    int32_t expected = memory[i] | (static_cast<int8_t>(memory[i + 1]) << 8);
    CHECK_EQ(expected, r.Call(i));
  }
}

WASM_EXEC_TEST(Int32LoadInt16_zeroext) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  const int kNumBytes = 16;
  byte* memory = r.module().AddMemory(kNumBytes);
  r.module().RandomizeMemory(9999);
  memory[1] = 204;
  BUILD(r, WASM_LOAD_MEM(MachineType::Uint16(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumBytes; i += 2) {
    int32_t expected = memory[i] | (memory[i + 1] << 8);
    CHECK_EQ(expected, r.Call(i));
  }
}

WASM_EXEC_TEST(Int32Global) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  int32_t* global = r.module().AddGlobal<int32_t>();
  // global = global + p0
  BUILD(r,
        WASM_SET_GLOBAL(0, WASM_I32_ADD(WASM_GET_GLOBAL(0), WASM_GET_LOCAL(0))),
        WASM_ZERO);

  *global = 116;
  for (int i = 9; i < 444444; i += 111111) {
    int32_t expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}

WASM_EXEC_TEST(Int32Globals_DontAlias) {
  const int kNumGlobals = 3;
  for (int g = 0; g < kNumGlobals; ++g) {
    // global = global + p0
    WasmRunner<int32_t, int32_t> r(execution_mode);
    int32_t* globals[] = {r.module().AddGlobal<int32_t>(),
                          r.module().AddGlobal<int32_t>(),
                          r.module().AddGlobal<int32_t>()};

    BUILD(r, WASM_SET_GLOBAL(
                 g, WASM_I32_ADD(WASM_GET_GLOBAL(g), WASM_GET_LOCAL(0))),
          WASM_GET_GLOBAL(g));

    // Check that reading/writing global number {g} doesn't alter the others.
    *globals[g] = 116 * g;
    int32_t before[kNumGlobals];
    for (int i = 9; i < 444444; i += 111113) {
      int32_t sum = *globals[g] + i;
      for (int j = 0; j < kNumGlobals; ++j) before[j] = *globals[j];
      int32_t result = r.Call(i);
      CHECK_EQ(sum, result);
      for (int j = 0; j < kNumGlobals; ++j) {
        int32_t expected = j == g ? sum : before[j];
        CHECK_EQ(expected, *globals[j]);
      }
    }
  }
}

WASM_EXEC_TEST(Float32Global) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  float* global = r.module().AddGlobal<float>();
  // global = global + p0
  BUILD(r, WASM_SET_GLOBAL(
               0, WASM_F32_ADD(WASM_GET_GLOBAL(0),
                               WASM_F32_SCONVERT_I32(WASM_GET_LOCAL(0)))),
        WASM_ZERO);

  *global = 1.25;
  for (int i = 9; i < 4444; i += 1111) {
    volatile float expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}

WASM_EXEC_TEST(Float64Global) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  double* global = r.module().AddGlobal<double>();
  // global = global + p0
  BUILD(r, WASM_SET_GLOBAL(
               0, WASM_F64_ADD(WASM_GET_GLOBAL(0),
                               WASM_F64_SCONVERT_I32(WASM_GET_LOCAL(0)))),
        WASM_ZERO);

  *global = 1.25;
  for (int i = 9; i < 4444; i += 1111) {
    volatile double expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}

WASM_EXEC_TEST(MixedGlobals) {
  WasmRunner<int32_t, int32_t> r(execution_mode);

  int32_t* unused = r.module().AddGlobal<int32_t>();
  byte* memory = r.module().AddMemory(32);

  int32_t* var_int32 = r.module().AddGlobal<int32_t>();
  uint32_t* var_uint32 = r.module().AddGlobal<uint32_t>();
  float* var_float = r.module().AddGlobal<float>();
  double* var_double = r.module().AddGlobal<double>();

  BUILD(r, WASM_SET_GLOBAL(1, WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
        WASM_SET_GLOBAL(2, WASM_LOAD_MEM(MachineType::Uint32(), WASM_ZERO)),
        WASM_SET_GLOBAL(3, WASM_LOAD_MEM(MachineType::Float32(), WASM_ZERO)),
        WASM_SET_GLOBAL(4, WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO)),
        WASM_ZERO);

  memory[0] = 0xaa;
  memory[1] = 0xcc;
  memory[2] = 0x55;
  memory[3] = 0xee;
  memory[4] = 0x33;
  memory[5] = 0x22;
  memory[6] = 0x11;
  memory[7] = 0x99;
  r.Call(1);

  CHECK(static_cast<int32_t>(0xee55ccaa) == *var_int32);
  CHECK(static_cast<uint32_t>(0xee55ccaa) == *var_uint32);
  CHECK(bit_cast<float>(0xee55ccaa) == *var_float);
  CHECK(bit_cast<double>(0x99112233ee55ccaaULL) == *var_double);

  USE(unused);
}

WASM_EXEC_TEST(CallEmpty) {
  const int32_t kExpected = -414444;
  WasmRunner<int32_t> r(execution_mode);

  // Build the target function.
  WasmFunctionCompiler& target_func = r.NewFunction<int>();
  BUILD(target_func, WASM_I32V_3(kExpected));

  // Build the calling function.
  BUILD(r, WASM_CALL_FUNCTION0(target_func.function_index()));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}

WASM_EXEC_TEST(CallF32StackParameter) {
  WasmRunner<float> r(execution_mode);

  // Build the target function.
  ValueType param_types[20];
  for (int i = 0; i < 20; ++i) param_types[i] = kWasmF32;
  FunctionSig sig(1, 19, param_types);
  WasmFunctionCompiler& t = r.NewFunction(&sig);
  BUILD(t, WASM_GET_LOCAL(17));

  // Build the calling function.
  BUILD(r, WASM_CALL_FUNCTION(
               t.function_index(), WASM_F32(1.0f), WASM_F32(2.0f),
               WASM_F32(4.0f), WASM_F32(8.0f), WASM_F32(16.0f), WASM_F32(32.0f),
               WASM_F32(64.0f), WASM_F32(128.0f), WASM_F32(256.0f),
               WASM_F32(1.5f), WASM_F32(2.5f), WASM_F32(4.5f), WASM_F32(8.5f),
               WASM_F32(16.5f), WASM_F32(32.5f), WASM_F32(64.5f),
               WASM_F32(128.5f), WASM_F32(256.5f), WASM_F32(512.5f)));

  float result = r.Call();
  CHECK_EQ(256.5f, result);
}

WASM_EXEC_TEST(CallF64StackParameter) {
  WasmRunner<double> r(execution_mode);

  // Build the target function.
  ValueType param_types[20];
  for (int i = 0; i < 20; ++i) param_types[i] = kWasmF64;
  FunctionSig sig(1, 19, param_types);
  WasmFunctionCompiler& t = r.NewFunction(&sig);
  BUILD(t, WASM_GET_LOCAL(17));

  // Build the calling function.
  BUILD(r, WASM_CALL_FUNCTION(t.function_index(), WASM_F64(1.0), WASM_F64(2.0),
                              WASM_F64(4.0), WASM_F64(8.0), WASM_F64(16.0),
                              WASM_F64(32.0), WASM_F64(64.0), WASM_F64(128.0),
                              WASM_F64(256.0), WASM_F64(1.5), WASM_F64(2.5),
                              WASM_F64(4.5), WASM_F64(8.5), WASM_F64(16.5),
                              WASM_F64(32.5), WASM_F64(64.5), WASM_F64(128.5),
                              WASM_F64(256.5), WASM_F64(512.5)));

  float result = r.Call();
  CHECK_EQ(256.5, result);
}

WASM_EXEC_TEST(CallVoid) {
  WasmRunner<int32_t> r(execution_mode);

  const byte kMemOffset = 8;
  const int32_t kElemNum = kMemOffset / sizeof(int32_t);
  const int32_t kExpected = 414444;
  // Build the target function.
  TestSignatures sigs;
  int32_t* memory = r.module().AddMemoryElems<int32_t>(16 / sizeof(int32_t));
  r.module().RandomizeMemory();
  WasmFunctionCompiler& t = r.NewFunction(sigs.v_v());
  BUILD(t, WASM_STORE_MEM(MachineType::Int32(), WASM_I32V_1(kMemOffset),
                          WASM_I32V_3(kExpected)));

  // Build the calling function.
  BUILD(r, WASM_CALL_FUNCTION0(t.function_index()),
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(kMemOffset)));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
  CHECK_EQ(static_cast<int64_t>(kExpected),
           static_cast<int64_t>(r.module().ReadMemory(&memory[kElemNum])));
}

WASM_EXEC_TEST(Call_Int32Add) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);

  // Build the target function.
  WasmFunctionCompiler& t = r.NewFunction<int32_t, int32_t, int32_t>();
  BUILD(t, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  // Build the caller function.
  BUILD(r, WASM_CALL_FUNCTION(t.function_index(), WASM_GET_LOCAL(0),
                              WASM_GET_LOCAL(1)));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*i) +
                                              static_cast<uint32_t>(*j));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST(Call_Float32Sub) {
  WasmRunner<float, float, float> r(execution_mode);

  // Build the target function.
  WasmFunctionCompiler& target_func = r.NewFunction<float, float, float>();
  BUILD(target_func, WASM_F32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  // Build the caller function.
  BUILD(r, WASM_CALL_FUNCTION(target_func.function_index(), WASM_GET_LOCAL(0),
                              WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CHECK_FLOAT_EQ(*i - *j, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(Call_Float64Sub) {
  WasmRunner<int32_t> r(execution_mode);
  double* memory = r.module().AddMemoryElems<double>(16);

  BUILD(r, WASM_STORE_MEM(
               MachineType::Float64(), WASM_ZERO,
               WASM_F64_SUB(
                   WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO),
                   WASM_LOAD_MEM(MachineType::Float64(), WASM_I32V_1(8)))),
        WASM_I32V_2(107));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      r.module().WriteMemory(&memory[0], *i);
      r.module().WriteMemory(&memory[1], *j);
      double expected = *i - *j;
      CHECK_EQ(107, r.Call());

      if (expected != expected) {
        CHECK(r.module().ReadMemory(&memory[0]) !=
              r.module().ReadMemory(&memory[0]));
      } else {
        CHECK_EQ(expected, r.module().ReadMemory(&memory[0]));
      }
    }
  }
}

#define ADD_CODE(vec, ...)                                              \
  do {                                                                  \
    byte __buf[] = {__VA_ARGS__};                                       \
    for (size_t i = 0; i < sizeof(__buf); ++i) vec.push_back(__buf[i]); \
  } while (false)

static void Run_WasmMixedCall_N(WasmExecutionMode execution_mode, int start) {
  const int kExpected = 6333;
  const int kElemSize = 8;
  TestSignatures sigs;

  // 64-bit cases handled in test-run-wasm-64.cc.
  static MachineType mixed[] = {
      MachineType::Int32(),   MachineType::Float32(), MachineType::Float64(),
      MachineType::Float32(), MachineType::Int32(),   MachineType::Float64(),
      MachineType::Float32(), MachineType::Float64(), MachineType::Int32(),
      MachineType::Int32(),   MachineType::Int32()};

  int num_params = static_cast<int>(arraysize(mixed)) - start;
  for (int which = 0; which < num_params; ++which) {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    WasmRunner<int32_t> r(execution_mode);
    r.module().AddMemory(1024);
    MachineType* memtypes = &mixed[start];
    MachineType result = memtypes[which];

    // =========================================================================
    // Build the selector function.
    // =========================================================================
    FunctionSig::Builder b(&zone, 1, num_params);
    b.AddReturn(WasmOpcodes::ValueTypeFor(result));
    for (int i = 0; i < num_params; ++i) {
      b.AddParam(WasmOpcodes::ValueTypeFor(memtypes[i]));
    }
    WasmFunctionCompiler& t = r.NewFunction(b.Build());
    BUILD(t, WASM_GET_LOCAL(which));

    // =========================================================================
    // Build the calling function.
    // =========================================================================
    std::vector<byte> code;

    // Load the offset for the store.
    ADD_CODE(code, WASM_ZERO);

    // Load the arguments.
    for (int i = 0; i < num_params; ++i) {
      int offset = (i + 1) * kElemSize;
      ADD_CODE(code, WASM_LOAD_MEM(memtypes[i], WASM_I32V_2(offset)));
    }

    // Call the selector function.
    ADD_CODE(code, WASM_CALL_FUNCTION0(t.function_index()));

    // Store the result in memory.
    ADD_CODE(code,
             static_cast<byte>(WasmOpcodes::LoadStoreOpcodeOf(result, true)),
             ZERO_ALIGNMENT, ZERO_OFFSET);

    // Return the expected value.
    ADD_CODE(code, WASM_I32V_2(kExpected));

    r.Build(&code[0], &code[0] + code.size());

    // Run the code.
    for (int t = 0; t < 10; ++t) {
      r.module().RandomizeMemory();
      CHECK_EQ(kExpected, r.Call());

      int size = WasmOpcodes::MemSize(result);
      for (int i = 0; i < size; ++i) {
        int base = (which + 1) * kElemSize;
        byte expected = r.module().raw_mem_at<byte>(base + i);
        byte result = r.module().raw_mem_at<byte>(i);
        CHECK_EQ(expected, result);
      }
    }
  }
}

WASM_EXEC_TEST(MixedCall_0) { Run_WasmMixedCall_N(execution_mode, 0); }
WASM_EXEC_TEST(MixedCall_1) { Run_WasmMixedCall_N(execution_mode, 1); }
WASM_EXEC_TEST(MixedCall_2) { Run_WasmMixedCall_N(execution_mode, 2); }
WASM_EXEC_TEST(MixedCall_3) { Run_WasmMixedCall_N(execution_mode, 3); }

WASM_EXEC_TEST(AddCall) {
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  WasmFunctionCompiler& t1 = r.NewFunction<int32_t, int32_t, int32_t>();
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  byte local = r.AllocateLocal(kWasmI32);
  BUILD(r, WASM_SET_LOCAL(local, WASM_I32V_2(99)),
        WASM_I32_ADD(WASM_CALL_FUNCTION(t1.function_index(), WASM_GET_LOCAL(0),
                                        WASM_GET_LOCAL(0)),
                     WASM_CALL_FUNCTION(t1.function_index(), WASM_GET_LOCAL(1),
                                        WASM_GET_LOCAL(local))));

  CHECK_EQ(198, r.Call(0));
  CHECK_EQ(200, r.Call(1));
  CHECK_EQ(100, r.Call(-49));
}

WASM_EXEC_TEST(MultiReturnSub) {
  FLAG_wasm_mv_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);

  ValueType storage[] = {kWasmI32, kWasmI32, kWasmI32, kWasmI32};
  FunctionSig sig_ii_ii(2, 2, storage);
  WasmFunctionCompiler& t1 = r.NewFunction(&sig_ii_ii);
  BUILD(t1, WASM_GET_LOCAL(1), WASM_GET_LOCAL(0));

  BUILD(r, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
        WASM_CALL_FUNCTION0(t1.function_index()), kExprI32Sub);

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*j) -
                                              static_cast<uint32_t>(*i));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

template <typename T>
void RunMultiReturnSelect(WasmExecutionMode execution_mode, const T* inputs) {
  FLAG_wasm_mv_prototype = true;
  ValueType type = WasmOpcodes::ValueTypeFor(MachineTypeForC<T>());
  ValueType storage[] = {type, type, type, type, type, type};
  const size_t kNumReturns = 2;
  const size_t kNumParams = arraysize(storage) - kNumReturns;
  FunctionSig sig(kNumReturns, kNumParams, storage);

  for (size_t i = 0; i < kNumParams; i++) {
    for (size_t j = 0; j < kNumParams; j++) {
      for (int k = 0; k < 2; k++) {
        WasmRunner<T, T, T, T, T> r(execution_mode);
        WasmFunctionCompiler& r1 = r.NewFunction(&sig);

        BUILD(r1, WASM_GET_LOCAL(i), WASM_GET_LOCAL(j));

        if (k == 0) {
          BUILD(r, WASM_CALL_FUNCTION(r1.function_index(), WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(1), WASM_GET_LOCAL(2),
                                      WASM_GET_LOCAL(3)),
                WASM_DROP);
        } else {
          BUILD(r, WASM_CALL_FUNCTION(r1.function_index(), WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(1), WASM_GET_LOCAL(2),
                                      WASM_GET_LOCAL(3)),
                kExprSetLocal, 0, WASM_DROP, WASM_GET_LOCAL(0));
        }

        T expected = inputs[k == 0 ? i : j];
        CHECK_EQ(expected, r.Call(inputs[0], inputs[1], inputs[2], inputs[3]));
      }
    }
  }
}

WASM_EXEC_TEST(MultiReturnSelect_i32) {
  static const int32_t inputs[] = {3333333, 4444444, -55555555, -7777777};
  RunMultiReturnSelect<int32_t>(execution_mode, inputs);
}

WASM_EXEC_TEST(MultiReturnSelect_f32) {
  static const float inputs[] = {33.33333f, 444.4444f, -55555.555f, -77777.77f};
  RunMultiReturnSelect<float>(execution_mode, inputs);
}

WASM_EXEC_TEST(MultiReturnSelect_i64) {
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
  // TODO(titzer): implement int64-lowering for multiple return values
  static const int64_t inputs[] = {33333338888, 44444446666, -555555553333,
                                   -77777771111};
  RunMultiReturnSelect<int64_t>(execution_mode, inputs);
#endif
}

WASM_EXEC_TEST(MultiReturnSelect_f64) {
  static const double inputs[] = {3.333333, 44444.44, -55.555555, -7777.777};
  RunMultiReturnSelect<double>(execution_mode, inputs);
}

WASM_EXEC_TEST(ExprBlock2a) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_I32V_1(1))),
                        WASM_I32V_1(1)));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}

WASM_EXEC_TEST(ExprBlock2b) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_I32V_1(1))),
                        WASM_I32V_1(2)));
  CHECK_EQ(2, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}

WASM_EXEC_TEST(ExprBlock2c) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV_IFD(0, WASM_I32V_1(1), WASM_GET_LOCAL(0)),
                        WASM_I32V_1(1)));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}

WASM_EXEC_TEST(ExprBlock2d) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_BRV_IFD(0, WASM_I32V_1(1), WASM_GET_LOCAL(0)),
                        WASM_I32V_1(2)));
  CHECK_EQ(2, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}

WASM_EXEC_TEST(ExprBlock_ManualSwitch) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(1)),
                                WASM_BRV(1, WASM_I32V_1(11))),
                        WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(2)),
                                WASM_BRV(1, WASM_I32V_1(12))),
                        WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(3)),
                                WASM_BRV(1, WASM_I32V_1(13))),
                        WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(4)),
                                WASM_BRV(1, WASM_I32V_1(14))),
                        WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(5)),
                                WASM_BRV(1, WASM_I32V_1(15))),
                        WASM_I32V_2(99)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(12, r.Call(2));
  CHECK_EQ(13, r.Call(3));
  CHECK_EQ(14, r.Call(4));
  CHECK_EQ(15, r.Call(5));
  CHECK_EQ(99, r.Call(6));
}

WASM_EXEC_TEST(ExprBlock_ManualSwitch_brif) {
  WasmRunner<int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_BLOCK_I(
               WASM_BRV_IFD(0, WASM_I32V_1(11),
                            WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(1))),
               WASM_BRV_IFD(0, WASM_I32V_1(12),
                            WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(2))),
               WASM_BRV_IFD(0, WASM_I32V_1(13),
                            WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(3))),
               WASM_BRV_IFD(0, WASM_I32V_1(14),
                            WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(4))),
               WASM_BRV_IFD(0, WASM_I32V_1(15),
                            WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(5))),
               WASM_I32V_2(99)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(12, r.Call(2));
  CHECK_EQ(13, r.Call(3));
  CHECK_EQ(14, r.Call(4));
  CHECK_EQ(15, r.Call(5));
  CHECK_EQ(99, r.Call(6));
}

WASM_EXEC_TEST(If_nested) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);

  BUILD(
      r,
      WASM_IF_ELSE_I(
          WASM_GET_LOCAL(0),
          WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_I32V_1(11), WASM_I32V_1(12)),
          WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_I32V_1(13), WASM_I32V_1(14))));

  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}

WASM_EXEC_TEST(ExprBlock_if) {
  WasmRunner<int32_t, int32_t> r(execution_mode);

  BUILD(r, WASM_BLOCK_I(WASM_IF_ELSE_I(WASM_GET_LOCAL(0),
                                       WASM_BRV(0, WASM_I32V_1(11)),
                                       WASM_BRV(1, WASM_I32V_1(14)))));

  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(14, r.Call(0));
}

WASM_EXEC_TEST(ExprBlock_nested_ifs) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);

  BUILD(r, WASM_BLOCK_I(WASM_IF_ELSE_I(
               WASM_GET_LOCAL(0),
               WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_BRV(0, WASM_I32V_1(11)),
                              WASM_BRV(1, WASM_I32V_1(12))),
               WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_BRV(0, WASM_I32V_1(13)),
                              WASM_BRV(1, WASM_I32V_1(14))))));

  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}

WASM_EXEC_TEST_WITH_TRAP(SimpleCallIndirect) {
  TestSignatures sigs;
  WasmRunner<int32_t, int32_t> r(execution_mode);

  WasmFunctionCompiler& t1 = r.NewFunction(sigs.i_ii());
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.SetSigIndex(1);

  WasmFunctionCompiler& t2 = r.NewFunction(sigs.i_ii());
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.SetSigIndex(1);

  // Signature table.
  r.module().AddSignature(sigs.f_ff());
  r.module().AddSignature(sigs.i_ii());
  r.module().AddSignature(sigs.d_dd());

  // Function table.
  uint16_t indirect_function_table[] = {
      static_cast<uint16_t>(t1.function_index()),
      static_cast<uint16_t>(t2.function_index())};
  r.module().AddIndirectFunctionTable(indirect_function_table,
                                      arraysize(indirect_function_table));
  r.module().PopulateIndirectFunctionTable();

  // Build the caller function.
  BUILD(r, WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_I32V_2(66),
                               WASM_I32V_1(22)));

  CHECK_EQ(88, r.Call(0));
  CHECK_EQ(44, r.Call(1));
  CHECK_TRAP(r.Call(2));
}

WASM_EXEC_TEST_WITH_TRAP(MultipleCallIndirect) {
  TestSignatures sigs;
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_mode);

  WasmFunctionCompiler& t1 = r.NewFunction(sigs.i_ii());
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.SetSigIndex(1);

  WasmFunctionCompiler& t2 = r.NewFunction(sigs.i_ii());
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.SetSigIndex(1);

  // Signature table.
  r.module().AddSignature(sigs.f_ff());
  r.module().AddSignature(sigs.i_ii());
  r.module().AddSignature(sigs.d_dd());

  // Function table.
  uint16_t indirect_function_table[] = {
      static_cast<uint16_t>(t1.function_index()),
      static_cast<uint16_t>(t2.function_index())};
  r.module().AddIndirectFunctionTable(indirect_function_table,
                                      arraysize(indirect_function_table));
  r.module().PopulateIndirectFunctionTable();

  // Build the caller function.
  BUILD(r, WASM_I32_ADD(
               WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                   WASM_GET_LOCAL(2)),
               WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(1), WASM_GET_LOCAL(2),
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

WASM_EXEC_TEST_WITH_TRAP(CallIndirect_EmptyTable) {
  TestSignatures sigs;
  WasmRunner<int32_t, int32_t> r(execution_mode);

  // One function.
  WasmFunctionCompiler& t1 = r.NewFunction(sigs.i_ii());
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.SetSigIndex(1);

  // Signature table.
  r.module().AddSignature(sigs.f_ff());
  r.module().AddSignature(sigs.i_ii());
  r.module().AddIndirectFunctionTable(nullptr, 0);

  // Build the caller function.
  BUILD(r, WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_I32V_2(66),
                               WASM_I32V_1(22)));

  CHECK_TRAP(r.Call(0));
  CHECK_TRAP(r.Call(1));
  CHECK_TRAP(r.Call(2));
}

WASM_EXEC_TEST_WITH_TRAP(CallIndirect_canonical) {
  TestSignatures sigs;
  WasmRunner<int32_t, int32_t> r(execution_mode);

  WasmFunctionCompiler& t1 = r.NewFunction(sigs.i_ii());
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.SetSigIndex(0);

  WasmFunctionCompiler& t2 = r.NewFunction(sigs.i_ii());
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.SetSigIndex(1);

  WasmFunctionCompiler& t3 = r.NewFunction(sigs.f_ff());
  BUILD(t3, WASM_F32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t3.SetSigIndex(2);

  // Signature table.
  r.module().AddSignature(sigs.i_ii());
  r.module().AddSignature(sigs.i_ii());
  r.module().AddSignature(sigs.f_ff());

  // Function table.
  uint16_t i1 = static_cast<uint16_t>(t1.function_index());
  uint16_t i2 = static_cast<uint16_t>(t2.function_index());
  uint16_t i3 = static_cast<uint16_t>(t3.function_index());
  uint16_t indirect_function_table[] = {i1, i2, i3, i1, i2};

  r.module().AddIndirectFunctionTable(indirect_function_table,
                                      arraysize(indirect_function_table));
  r.module().PopulateIndirectFunctionTable();

  // Build the caller function.
  BUILD(r, WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_I32V_2(77),
                               WASM_I32V_1(11)));

  CHECK_EQ(88, r.Call(0));
  CHECK_EQ(66, r.Call(1));
  CHECK_TRAP(r.Call(2));
  CHECK_EQ(88, r.Call(3));
  CHECK_EQ(66, r.Call(4));
  CHECK_TRAP(r.Call(5));
}

WASM_EXEC_TEST(F32Floor) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_F32_FLOOR(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(floorf(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F32Ceil) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_F32_CEIL(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(ceilf(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F32Trunc) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_F32_TRUNC(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(truncf(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F32NearestInt) {
  WasmRunner<float, float> r(execution_mode);
  BUILD(r, WASM_F32_NEARESTINT(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(nearbyintf(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F64Floor) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_F64_FLOOR(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(floor(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F64Ceil) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_F64_CEIL(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(ceil(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F64Trunc) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_F64_TRUNC(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(trunc(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F64NearestInt) {
  WasmRunner<double, double> r(execution_mode);
  BUILD(r, WASM_F64_NEARESTINT(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(nearbyint(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F32Min) {
  WasmRunner<float, float, float> r(execution_mode);
  BUILD(r, WASM_F32_MIN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CHECK_DOUBLE_EQ(JSMin(*i, *j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(F64Min) {
  WasmRunner<double, double, double> r(execution_mode);
  BUILD(r, WASM_F64_MIN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) { CHECK_DOUBLE_EQ(JSMin(*i, *j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(F32Max) {
  WasmRunner<float, float, float> r(execution_mode);
  BUILD(r, WASM_F32_MAX(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CHECK_FLOAT_EQ(JSMax(*i, *j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(F64Max) {
  WasmRunner<double, double, double> r(execution_mode);
  BUILD(r, WASM_F64_MAX(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      double result = r.Call(*i, *j);
      CHECK_DOUBLE_EQ(JSMax(*i, *j), result);
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(I32SConvertF32) {
  WasmRunner<int32_t, float> r(execution_mode);
  BUILD(r, WASM_I32_SCONVERT_F32(WASM_GET_LOCAL(0)));

  // The upper bound is (INT32_MAX + 1), which is the lowest float-representable
  // number above INT32_MAX which cannot be represented as int32.
  float upper_bound = 2147483648.0f;
  // We use INT32_MIN as a lower bound because (INT32_MIN - 1) is not
  // representable as float, and no number between (INT32_MIN - 1) and INT32_MIN
  // is.
  float lower_bound = static_cast<float>(INT32_MIN);
  FOR_FLOAT32_INPUTS(i) {
    if (*i < upper_bound && *i >= lower_bound) {
      CHECK_EQ(static_cast<int32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(I32SConvertF64) {
  WasmRunner<int32_t, double> r(execution_mode);
  BUILD(r, WASM_I32_SCONVERT_F64(WASM_GET_LOCAL(0)));

  // The upper bound is (INT32_MAX + 1), which is the lowest double-
  // representable number above INT32_MAX which cannot be represented as int32.
  double upper_bound = 2147483648.0;
  // The lower bound is (INT32_MIN - 1), which is the greatest double-
  // representable number below INT32_MIN which cannot be represented as int32.
  double lower_bound = -2147483649.0;
  FOR_FLOAT64_INPUTS(i) {
    if (*i<upper_bound&& * i> lower_bound) {
      CHECK_EQ(static_cast<int32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(I32UConvertF32) {
  WasmRunner<uint32_t, float> r(execution_mode);
  BUILD(r, WASM_I32_UCONVERT_F32(WASM_GET_LOCAL(0)));
  // The upper bound is (UINT32_MAX + 1), which is the lowest
  // float-representable number above UINT32_MAX which cannot be represented as
  // uint32.
  double upper_bound = 4294967296.0f;
  double lower_bound = -1.0f;
  FOR_FLOAT32_INPUTS(i) {
    if (*i<upper_bound&& * i> lower_bound) {
      CHECK_EQ(static_cast<uint32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST_WITH_TRAP(I32UConvertF64) {
  WasmRunner<uint32_t, double> r(execution_mode);
  BUILD(r, WASM_I32_UCONVERT_F64(WASM_GET_LOCAL(0)));
  // The upper bound is (UINT32_MAX + 1), which is the lowest
  // double-representable number above UINT32_MAX which cannot be represented as
  // uint32.
  double upper_bound = 4294967296.0;
  double lower_bound = -1.0;
  FOR_FLOAT64_INPUTS(i) {
    if (*i<upper_bound&& * i> lower_bound) {
      CHECK_EQ(static_cast<uint32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(F64CopySign) {
  WasmRunner<double, double, double> r(execution_mode);
  BUILD(r, WASM_F64_COPYSIGN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) { CHECK_DOUBLE_EQ(copysign(*i, *j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(F32CopySign) {
  WasmRunner<float, float, float> r(execution_mode);
  BUILD(r, WASM_F32_COPYSIGN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CHECK_FLOAT_EQ(copysignf(*i, *j), r.Call(*i, *j)); }
  }
}

static void CompileCallIndirectMany(ValueType param) {
  // Make sure we don't run out of registers when compiling indirect calls
  // with many many parameters.
  TestSignatures sigs;
  for (byte num_params = 0; num_params < 40; ++num_params) {
    WasmRunner<void> r(kExecuteCompiled);
    FunctionSig* sig = sigs.many(r.zone(), kWasmStmt, param, num_params);

    r.module().AddSignature(sig);
    r.module().AddSignature(sig);
    r.module().AddIndirectFunctionTable(nullptr, 0);

    WasmFunctionCompiler& t = r.NewFunction(sig);

    std::vector<byte> code;
    for (byte p = 0; p < num_params; ++p) {
      ADD_CODE(code, kExprGetLocal, p);
    }
    ADD_CODE(code, kExprI32Const, 0);
    ADD_CODE(code, kExprCallIndirect, 1, TABLE_ZERO);

    t.Build(&code[0], &code[0] + code.size());
  }
}

TEST(Compile_Wasm_CallIndirect_Many_i32) { CompileCallIndirectMany(kWasmI32); }

TEST(Compile_Wasm_CallIndirect_Many_f32) { CompileCallIndirectMany(kWasmF32); }

TEST(Compile_Wasm_CallIndirect_Many_f64) { CompileCallIndirectMany(kWasmF64); }

WASM_EXEC_TEST_WITH_TRAP(Int32RemS_dead) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  BUILD(r, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), WASM_DROP,
        WASM_ZERO);
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(0, 1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
}

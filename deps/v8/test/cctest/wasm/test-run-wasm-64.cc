// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/bits.h"
#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

// If the target architecture is 64-bit, enable all tests.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif

#define CHECK_TRAP32(x) \
  CHECK_EQ(0xdeadbeef, (bit_cast<uint32_t>(x)) & 0xFFFFFFFF)
#define CHECK_TRAP64(x) \
  CHECK_EQ(0xdeadbeefdeadbeef, (bit_cast<uint64_t>(x)) & 0xFFFFFFFFFFFFFFFF)
#define CHECK_TRAP(x) CHECK_TRAP32(x)

#define asi64(x) static_cast<int64_t>(x)

#define asu64(x) static_cast<uint64_t>(x)

#define B2(a, b) kExprBlock, a, b, kExprEnd

// Can't bridge macro land with nested macros.
#if V8_TARGET_ARCH_MIPS
#define MIPS true
#else
#define MIPS false
#endif

#define FOREACH_I64_OPERATOR(V) \
  V(DepthFirst, true)           \
  V(I64Phi, true)               \
  V(I64Const, true)             \
  V(I64Return, true)            \
  V(I64Param, true)             \
  V(I64LoadStore, true)         \
  V(I64Add, true)               \
  V(I64Sub, true)               \
  V(I64Mul, !MIPS)              \
  V(I64DivS, true)              \
  V(I64DivU, true)              \
  V(I64RemS, true)              \
  V(I64RemU, true)              \
  V(I64And, true)               \
  V(I64Ior, true)               \
  V(I64Xor, true)               \
  V(I64Shl, true)               \
  V(I64ShrU, true)              \
  V(I64ShrS, true)              \
  V(I64Eq, true)                \
  V(I64Ne, true)                \
  V(I64LtS, true)               \
  V(I64LeS, true)               \
  V(I64LtU, true)               \
  V(I64LeU, true)               \
  V(I64GtS, true)               \
  V(I64GeS, true)               \
  V(I64GtU, true)               \
  V(I64GeU, true)               \
  V(I64Ctz, true)               \
  V(I64Clz, true)               \
  V(I64Popcnt, true)            \
  V(I32ConvertI64, true)        \
  V(I64SConvertF32, true)       \
  V(I64SConvertF64, true)       \
  V(I64UConvertF32, true)       \
  V(I64UConvertF64, true)       \
  V(I64SConvertI32, true)       \
  V(I64UConvertI32, true)       \
  V(F32SConvertI64, true)       \
  V(F32UConvertI64, true)       \
  V(F64SConvertI64, true)       \
  V(F64UConvertI64, true)       \
  V(F64ReinterpretI64, true)    \
  V(I64ReinterpretF64, true)    \
  V(I64Ror, true)               \
  V(I64Rol, true)

#define DECLARE_CONST(name, cond) static const bool kSupported_##name = cond;
FOREACH_I64_OPERATOR(DECLARE_CONST)
#undef DECLARE_CONST

#define REQUIRE(name) \
  if (!WASM_64 && !kSupported_##name) return

WASM_EXEC_TEST(I64Const) {
  REQUIRE(I64Const);
  WasmRunner<int64_t> r(execution_mode);
  const int64_t kExpectedValue = 0x1122334455667788LL;
  // return(kExpectedValue)
  BUILD(r, WASM_I64V_9(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}

WASM_EXEC_TEST(I64Const_many) {
  REQUIRE(I64Const);
  int cntr = 0;
  FOR_INT32_INPUTS(i) {
    WasmRunner<int64_t> r(execution_mode);
    const int64_t kExpectedValue = (static_cast<int64_t>(*i) << 32) | cntr;
    // return(kExpectedValue)
    BUILD(r, WASM_I64V(kExpectedValue));
    CHECK_EQ(kExpectedValue, r.Call());
    cntr++;
  }
}

WASM_EXEC_TEST(Return_I64) {
  REQUIRE(I64Return);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64());

  BUILD(r, WASM_RETURN1(WASM_GET_LOCAL(0)));

  FOR_INT64_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}

WASM_EXEC_TEST(I64Add) {
  REQUIRE(I64Add);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i + *j, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64Sub) {
  REQUIRE(I64Sub);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i - *j, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64DivS) {
  REQUIRE(I64DivS);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) {
      if (*j == 0) {
        CHECK_TRAP64(r.Call(*i, *j));
      } else if (*j == -1 && *i == std::numeric_limits<int64_t>::min()) {
        CHECK_TRAP64(r.Call(*i, *j));
      } else {
        CHECK_EQ(*i / *j, r.Call(*i, *j));
      }
    }
  }
}

WASM_EXEC_TEST(I64DivS_Trap) {
  REQUIRE(I64DivS);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(0, r.Call(asi64(0), asi64(100)));
  CHECK_TRAP64(r.Call(asi64(100), asi64(0)));
  CHECK_TRAP64(r.Call(asi64(-1001), asi64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), asi64(-1)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), asi64(0)));
}

WASM_EXEC_TEST(I64DivS_Byzero_Const) {
  REQUIRE(I64DivS);
  for (int8_t denom = -2; denom < 8; denom++) {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_I64V_1(denom)));
    for (int64_t val = -7; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP64(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}

WASM_EXEC_TEST(I64DivU) {
  REQUIRE(I64DivU);
  WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64(),
                         MachineType::Uint64());
  BUILD(r, WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) {
      if (*j == 0) {
        CHECK_TRAP64(r.Call(*i, *j));
      } else {
        CHECK_EQ(*i / *j, r.Call(*i, *j));
      }
    }
  }
}

WASM_EXEC_TEST(I64DivU_Trap) {
  REQUIRE(I64DivU);
  WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64(),
                         MachineType::Uint64());
  BUILD(r, WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(0, r.Call(asu64(0), asu64(100)));
  CHECK_TRAP64(r.Call(asu64(100), asu64(0)));
  CHECK_TRAP64(r.Call(asu64(1001), asu64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<uint64_t>::max(), asu64(0)));
}

WASM_EXEC_TEST(I64DivU_Byzero_Const) {
  REQUIRE(I64DivU);
  for (uint64_t denom = 0xfffffffffffffffe; denom < 8; denom++) {
    WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64());
    BUILD(r, WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_I64V_1(denom)));

    for (uint64_t val = 0xfffffffffffffff0; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP64(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}

WASM_EXEC_TEST(I64RemS) {
  REQUIRE(I64RemS);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) {
      if (*j == 0) {
        CHECK_TRAP64(r.Call(*i, *j));
      } else {
        CHECK_EQ(*i % *j, r.Call(*i, *j));
      }
    }
  }
}

WASM_EXEC_TEST(I64RemS_Trap) {
  REQUIRE(I64RemS);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(33, r.Call(asi64(133), asi64(100)));
  CHECK_EQ(0, r.Call(std::numeric_limits<int64_t>::min(), asi64(-1)));
  CHECK_TRAP64(r.Call(asi64(100), asi64(0)));
  CHECK_TRAP64(r.Call(asi64(-1001), asi64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<int64_t>::min(), asi64(0)));
}

WASM_EXEC_TEST(I64RemU) {
  REQUIRE(I64RemU);
  WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64(),
                         MachineType::Uint64());
  BUILD(r, WASM_I64_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) {
      if (*j == 0) {
        CHECK_TRAP64(r.Call(*i, *j));
      } else {
        CHECK_EQ(*i % *j, r.Call(*i, *j));
      }
    }
  }
}

WASM_EXEC_TEST(I64RemU_Trap) {
  REQUIRE(I64RemU);
  WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64(),
                         MachineType::Uint64());
  BUILD(r, WASM_I64_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(17, r.Call(asu64(217), asu64(100)));
  CHECK_TRAP64(r.Call(asu64(100), asu64(0)));
  CHECK_TRAP64(r.Call(asu64(1001), asu64(0)));
  CHECK_TRAP64(r.Call(std::numeric_limits<uint64_t>::max(), asu64(0)));
}

WASM_EXEC_TEST(I64And) {
  REQUIRE(I64And);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_AND(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ((*i) & (*j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64Ior) {
  REQUIRE(I64Ior);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_IOR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ((*i) | (*j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64Xor) {
  REQUIRE(I64Xor);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_XOR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ((*i) ^ (*j), r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64Shl) {
  REQUIRE(I64Shl);
  {
    WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64(),
                           MachineType::Uint64());
    BUILD(r, WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

    FOR_UINT64_INPUTS(i) {
      FOR_UINT64_INPUTS(j) {
        uint64_t expected = (*i) << (*j & 0x3f);
        CHECK_EQ(expected, r.Call(*i, *j));
      }
    }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(0)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i << 0, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(32)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i << 32, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(20)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i << 20, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(40)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i << 40, r.Call(*i)); }
  }
}

WASM_EXEC_TEST(I64ShrU) {
  REQUIRE(I64ShrU);
  {
    WasmRunner<uint64_t> r(execution_mode, MachineType::Uint64(),
                           MachineType::Uint64());
    BUILD(r, WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

    FOR_UINT64_INPUTS(i) {
      FOR_UINT64_INPUTS(j) {
        uint64_t expected = (*i) >> (*j & 0x3f);
        CHECK_EQ(expected, r.Call(*i, *j));
      }
    }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(0)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i >> 0, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(32)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i >> 32, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(20)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i >> 20, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(40)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(*i >> 40, r.Call(*i)); }
  }
}

WASM_EXEC_TEST(I64ShrS) {
  REQUIRE(I64ShrS);
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                          MachineType::Int64());
    BUILD(r, WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) {
        int64_t expected = (*i) >> (*j & 0x3f);
        CHECK_EQ(expected, r.Call(*i, *j));
      }
    }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(0)));
    FOR_INT64_INPUTS(i) { CHECK_EQ(*i >> 0, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(32)));
    FOR_INT64_INPUTS(i) { CHECK_EQ(*i >> 32, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(20)));
    FOR_INT64_INPUTS(i) { CHECK_EQ(*i >> 20, r.Call(*i)); }
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
    BUILD(r, WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(40)));
    FOR_INT64_INPUTS(i) { CHECK_EQ(*i >> 40, r.Call(*i)); }
  }
}

WASM_EXEC_TEST(I64Eq) {
  REQUIRE(I64Eq);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_EQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i == *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64Ne) {
  REQUIRE(I64Ne);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_NE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i != *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64LtS) {
  REQUIRE(I64LtS);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_LTS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i < *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64LeS) {
  REQUIRE(I64LeS);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_LES(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i <= *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64LtU) {
  REQUIRE(I64LtU);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_LTU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) { CHECK_EQ(*i < *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64LeU) {
  REQUIRE(I64LeU);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_LEU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) { CHECK_EQ(*i <= *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64GtS) {
  REQUIRE(I64GtS);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_GTS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i > *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64GeS) {
  REQUIRE(I64GeS);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_GES(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i >= *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64GtU) {
  REQUIRE(I64GtU);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_GTU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) { CHECK_EQ(*i > *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I64GeU) {
  REQUIRE(I64GeU);
  WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_GEU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) { CHECK_EQ(*i >= *j ? 1 : 0, r.Call(*i, *j)); }
  }
}

WASM_EXEC_TEST(I32ConvertI64) {
  REQUIRE(I32ConvertI64);
  FOR_INT64_INPUTS(i) {
    WasmRunner<int32_t> r(execution_mode);
    BUILD(r, WASM_I32_CONVERT_I64(WASM_I64V(*i)));
    CHECK_EQ(static_cast<int32_t>(*i), r.Call());
  }
}

WASM_EXEC_TEST(I64SConvertI32) {
  REQUIRE(I64SConvertI32);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int32());
  BUILD(r, WASM_I64_SCONVERT_I32(WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(I64UConvertI32) {
  REQUIRE(I64UConvertI32);
  WasmRunner<int64_t> r(execution_mode, MachineType::Uint32());
  BUILD(r, WASM_I64_UCONVERT_I32(WASM_GET_LOCAL(0)));
  FOR_UINT32_INPUTS(i) { CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(I64Popcnt) {
  struct {
    int64_t expected;
    uint64_t input;
  } values[] = {{64, 0xffffffffffffffff},
                {0, 0x0000000000000000},
                {2, 0x0000080000008000},
                {26, 0x1123456782345678},
                {38, 0xffedcba09edcba09}};

  WasmRunner<int64_t> r(execution_mode, MachineType::Uint64());
  BUILD(r, WASM_I64_POPCNT(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}

WASM_EXEC_TEST(F32SConvertI64) {
  REQUIRE(F32SConvertI64);
  WasmRunner<float> r(execution_mode, MachineType::Int64());
  BUILD(r, WASM_F32_SCONVERT_I64(WASM_GET_LOCAL(0)));
  FOR_INT64_INPUTS(i) { CHECK_FLOAT_EQ(static_cast<float>(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F32UConvertI64) {
  REQUIRE(F32UConvertI64);
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
                {0x8000000000000400, 0x5f000000},
                {0x8000000000000401, 0x5f000000}};
  WasmRunner<float> r(execution_mode, MachineType::Uint64());
  BUILD(r, WASM_F32_UCONVERT_I64(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(bit_cast<float>(values[i].expected), r.Call(values[i].input));
  }
}

WASM_EXEC_TEST(F64SConvertI64) {
  REQUIRE(F64SConvertI64);
  WasmRunner<double> r(execution_mode, MachineType::Int64());
  BUILD(r, WASM_F64_SCONVERT_I64(WASM_GET_LOCAL(0)));
  FOR_INT64_INPUTS(i) { CHECK_DOUBLE_EQ(static_cast<double>(*i), r.Call(*i)); }
}

WASM_EXEC_TEST(F64UConvertI64) {
  REQUIRE(F64UConvertI64);
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
                {0x8000000000000401, 0x43e0000000000001}};
  WasmRunner<double> r(execution_mode, MachineType::Uint64());
  BUILD(r, WASM_F64_UCONVERT_I64(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(bit_cast<double>(values[i].expected), r.Call(values[i].input));
  }
}

WASM_EXEC_TEST(I64SConvertF32a) {
  WasmRunner<int64_t> r(execution_mode, MachineType::Float32());
  BUILD(r, WASM_I64_SCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(std::numeric_limits<int64_t>::max()) &&
        *i >= static_cast<float>(std::numeric_limits<int64_t>::min())) {
      CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(I64SConvertF64a) {
  WasmRunner<int64_t> r(execution_mode, MachineType::Float64());
  BUILD(r, WASM_I64_SCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<double>(std::numeric_limits<int64_t>::max()) &&
        *i >= static_cast<double>(std::numeric_limits<int64_t>::min())) {
      CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(I64UConvertF32a) {
  WasmRunner<uint64_t> r(execution_mode, MachineType::Float32());
  BUILD(r, WASM_I64_UCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(std::numeric_limits<uint64_t>::max()) &&
        *i > -1) {
      CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(I64UConvertF64a) {
  WasmRunner<uint64_t> r(execution_mode, MachineType::Float64());
  BUILD(r, WASM_I64_UCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<float>(std::numeric_limits<uint64_t>::max()) &&
        *i > -1) {
      CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(CallI64Parameter) {
  // Build the target function.
  LocalType param_types[20];
  for (int i = 0; i < 20; i++) param_types[i] = kAstI64;
  param_types[3] = kAstI32;
  param_types[4] = kAstI32;
  FunctionSig sig(1, 19, param_types);
  for (int i = 0; i < 19; i++) {
    TestingModule module(execution_mode);
    WasmFunctionCompiler t(&sig, &module);
    if (i == 2 || i == 3) {
      continue;
    } else {
      BUILD(t, WASM_GET_LOCAL(i));
    }
    uint32_t index = t.CompileAndAdd();

    // Build the calling function.
    WasmRunner<int32_t> r(&module);
    BUILD(
        r,
        WASM_I32_CONVERT_I64(WASM_CALL_FUNCTION(
            index, WASM_I64V_9(0xbcd12340000000b),
            WASM_I64V_9(0xbcd12340000000c), WASM_I32V_1(0xd),
            WASM_I32_CONVERT_I64(WASM_I64V_9(0xbcd12340000000e)),
            WASM_I64V_9(0xbcd12340000000f), WASM_I64V_10(0xbcd1234000000010),
            WASM_I64V_10(0xbcd1234000000011), WASM_I64V_10(0xbcd1234000000012),
            WASM_I64V_10(0xbcd1234000000013), WASM_I64V_10(0xbcd1234000000014),
            WASM_I64V_10(0xbcd1234000000015), WASM_I64V_10(0xbcd1234000000016),
            WASM_I64V_10(0xbcd1234000000017), WASM_I64V_10(0xbcd1234000000018),
            WASM_I64V_10(0xbcd1234000000019), WASM_I64V_10(0xbcd123400000001a),
            WASM_I64V_10(0xbcd123400000001b), WASM_I64V_10(0xbcd123400000001c),
            WASM_I64V_10(0xbcd123400000001d))));

    CHECK_EQ(i + 0xb, r.Call());
  }
}

void TestI64Binop(WasmExecutionMode execution_mode, WasmOpcode opcode,
                  int64_t expected, int64_t a, int64_t b) {
  {
    WasmRunner<int64_t> r(execution_mode);
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I64V(a), WASM_I64V(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                          MachineType::Int64());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

void TestI64Cmp(WasmExecutionMode execution_mode, WasmOpcode opcode,
                int64_t expected, int64_t a, int64_t b) {
  {
    WasmRunner<int32_t> r(execution_mode);
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I64V(a), WASM_I64V(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(execution_mode, MachineType::Int64(),
                          MachineType::Int64());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

#define TEST_I64_BINOP(name, expected, a, b)                     \
  do {                                                           \
    if (WASM_64 || kSupported_##name)                            \
      TestI64Binop(execution_mode, kExpr##name, expected, a, b); \
  } while (false)

WASM_EXEC_TEST(I64Binops) {
  TEST_I64_BINOP(I64Add, -5586332274295447011, 0x501b72ebabc26847,
                 0x625de9793d8f79d6);
  TEST_I64_BINOP(I64Sub, 9001903251710731490, 0xf24fe6474640002e,
                 0x7562b6f711991b4c);
  TEST_I64_BINOP(I64Mul, -4569547818546064176, 0x231a263c2cbc6451,
                 0xead44de6bd3e23d0);
  TEST_I64_BINOP(I64Mul, -25963122347507043, 0x4da1fa47c9352b73,
                 0x91fe82317aa035af);
  TEST_I64_BINOP(I64Mul, 7640290486138131960, 0x185731abe8eea47c,
                 0x714ec59f1380d4c2);
  TEST_I64_BINOP(I64DivS, -91517, 0x93b1190a34de56a0, 0x00004d8f68863948);
  TEST_I64_BINOP(I64DivU, 149016, 0xe15b3727e8a2080a, 0x0000631bfa72db8b);
  TEST_I64_BINOP(I64RemS, -664128064149968, 0x9a78b4e4fe708692,
                 0x0003e0b6b3be7609);
  TEST_I64_BINOP(I64RemU, 1742040017332765, 0x0ce84708c6258c81,
                 0x000a6fde82016697);
  TEST_I64_BINOP(I64And, 2531040582801836054, 0xaf257d1602644a16,
                 0x33b290a91a10d997);
  TEST_I64_BINOP(I64Ior, 8556201506536114940, 0x169d9be7bd3f0a5c,
                 0x66bca28d77af40e8);
  TEST_I64_BINOP(I64Xor, -4605655183785456377, 0xb6ea20a5d48e85b8,
                 0x76ff4da6c80688bf);
  TEST_I64_BINOP(I64Shl, -7240704056088331264, 0xef4dc1ed030e8ffe, 9);
  TEST_I64_BINOP(I64ShrU, 12500673744059159, 0xb1a52fa7deec5d14, 10);
  TEST_I64_BINOP(I64ShrS, 1725103446999874, 0x3107c791461a112b, 11);
  TEST_I64_BINOP(I64Ror, -8960135652432576946, 0x73418d1717e4e83a, 12);
  TEST_I64_BINOP(I64Ror, 7617662827409989779, 0xebff67cf0c126d36, 13);
  TEST_I64_BINOP(I64Rol, -2097714064174346012, 0x43938b8db0b0f230, 14);
  TEST_I64_BINOP(I64Rol, 8728493013947314237, 0xe07af243ac4d219d, 15);
}

#define TEST_I64_CMP(name, expected, a, b)                     \
  do {                                                         \
    if (WASM_64 || kSupported_##name)                          \
      TestI64Cmp(execution_mode, kExpr##name, expected, a, b); \
  } while (false)

WASM_EXEC_TEST(I64Compare) {
  TEST_I64_CMP(I64Eq, 0, 0xB915D8FA494064F0, 0x04D700B2536019A3);
  TEST_I64_CMP(I64Ne, 1, 0xC2FAFAAAB0446CDC, 0x52A3328F780C97A3);
  TEST_I64_CMP(I64LtS, 0, 0x673636E6306B0578, 0x028EC9ECA78F7227);
  TEST_I64_CMP(I64LeS, 1, 0xAE5214114B86A0FA, 0x7C1D21DA3DFD0CCF);
  TEST_I64_CMP(I64LtU, 0, 0x7D52166381EC1CE0, 0x59F4A6A9E78CD3D8);
  TEST_I64_CMP(I64LeU, 1, 0xE4169A385C7EA0E0, 0xFBDBED2C8781E5BC);
  TEST_I64_CMP(I64GtS, 0, 0x9D08FF8FB5F42E81, 0xD4E5C9D7FE09F621);
  TEST_I64_CMP(I64GeS, 1, 0x78DA3B2F73264E0F, 0x6FE5E2A67C501CBE);
  TEST_I64_CMP(I64GtU, 0, 0x8F691284E44F7DA9, 0xD5EA9BC1EE149192);
  TEST_I64_CMP(I64GeU, 0, 0x0886A0C58C7AA224, 0x5DDBE5A81FD7EE47);
}

WASM_EXEC_TEST(I64Clz) {
  REQUIRE(I64Clz);
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

  WasmRunner<int64_t> r(execution_mode, MachineType::Uint64());
  BUILD(r, WASM_I64_CLZ(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}

WASM_EXEC_TEST(I64Ctz) {
  REQUIRE(I64Ctz);
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

  WasmRunner<int64_t> r(execution_mode, MachineType::Uint64());
  BUILD(r, WASM_I64_CTZ(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}

WASM_EXEC_TEST(I64Popcnt2) {
  REQUIRE(I64Popcnt);
  struct {
    int64_t expected;
    uint64_t input;
  } values[] = {{64, 0xffffffffffffffff},
                {0, 0x0000000000000000},
                {2, 0x0000080000008000},
                {26, 0x1123456782345678},
                {38, 0xffedcba09edcba09}};

  WasmRunner<int64_t> r(execution_mode, MachineType::Uint64());
  BUILD(r, WASM_I64_POPCNT(WASM_GET_LOCAL(0)));
  for (size_t i = 0; i < arraysize(values); i++) {
    CHECK_EQ(values[i].expected, r.Call(values[i].input));
  }
}

// Test the WasmRunner with an Int64 return value and different numbers of
// Int64 parameters.
WASM_EXEC_TEST(I64WasmRunner) {
  REQUIRE(I64Param);
  REQUIRE(I64Xor);
  {FOR_INT64_INPUTS(i){WasmRunner<int64_t> r(execution_mode);
  BUILD(r, WASM_I64V(*i));
  CHECK_EQ(*i, r.Call());
}
}
{
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64());
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT64_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}
{
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_XOR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) { CHECK_EQ(*i ^ *j, r.Call(*i, *j)); }
  }
}
{
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64(), MachineType::Int64());
  BUILD(r, WASM_I64_XOR(WASM_GET_LOCAL(0),
                        WASM_I64_XOR(WASM_GET_LOCAL(1), WASM_GET_LOCAL(2))));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) {
      CHECK_EQ(*i ^ *j ^ *j, r.Call(*i, *j, *j));
      CHECK_EQ(*j ^ *i ^ *j, r.Call(*j, *i, *j));
      CHECK_EQ(*j ^ *j ^ *i, r.Call(*j, *j, *i));
    }
  }
}
{
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64(), MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_XOR(WASM_GET_LOCAL(0),
                        WASM_I64_XOR(WASM_GET_LOCAL(1),
                                     WASM_I64_XOR(WASM_GET_LOCAL(2),
                                                  WASM_GET_LOCAL(3)))));
  FOR_INT64_INPUTS(i) {
    FOR_INT64_INPUTS(j) {
      CHECK_EQ(*i ^ *j ^ *j ^ *j, r.Call(*i, *j, *j, *j));
      CHECK_EQ(*j ^ *i ^ *j ^ *j, r.Call(*j, *i, *j, *j));
      CHECK_EQ(*j ^ *j ^ *i ^ *j, r.Call(*j, *j, *i, *j));
      CHECK_EQ(*j ^ *j ^ *j ^ *i, r.Call(*j, *j, *j, *i));
    }
  }
}
}

WASM_EXEC_TEST(Call_Int64Sub) {
  REQUIRE(I64Sub);
  // Build the target function.
  TestSignatures sigs;
  TestingModule module(execution_mode);
  WasmFunctionCompiler t(sigs.l_ll(), &module);
  BUILD(t, WASM_I64_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  uint32_t index = t.CompileAndAdd();

  // Build the caller function.
  WasmRunner<int64_t> r(&module, MachineType::Int64(), MachineType::Int64());
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

WASM_EXEC_TEST(LoadStoreI64_sx) {
  REQUIRE(I64LoadStore);
  REQUIRE(DepthFirst);
  byte loads[] = {kExprI64LoadMem8S, kExprI64LoadMem16S, kExprI64LoadMem32S,
                  kExprI64LoadMem};

  for (size_t m = 0; m < arraysize(loads); m++) {
    TestingModule module(execution_mode);
    byte* memory = module.AddMemoryElems<byte>(16);
    WasmRunner<int64_t> r(&module);

    byte code[] = {
        kExprI8Const,     8,  // --
        kExprI8Const,     0,  // --
        loads[m],             // --
        ZERO_ALIGNMENT,       // --
        ZERO_OFFSET,          // --
        kExprI64StoreMem,     // --
        ZERO_ALIGNMENT,       // --
        ZERO_OFFSET,          // --
        kExprI8Const,     0,  // --
        loads[m],             // --
        ZERO_ALIGNMENT,       // --
        ZERO_OFFSET,          // --
    };

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

WASM_EXEC_TEST(I64SConvertF32b) {
  REQUIRE(I64SConvertF32);
  WasmRunner<int64_t> r(execution_mode, MachineType::Float32());
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

WASM_EXEC_TEST(I64SConvertF64b) {
  REQUIRE(I64SConvertF64);
  WasmRunner<int64_t> r(execution_mode, MachineType::Float64());
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

WASM_EXEC_TEST(I64UConvertF32b) {
  REQUIRE(I64UConvertF32);
  WasmRunner<uint64_t> r(execution_mode, MachineType::Float32());
  BUILD(r, WASM_I64_UCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(UINT64_MAX) && *i > -1) {
      CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(I64UConvertF64b) {
  REQUIRE(I64UConvertF64);
  WasmRunner<uint64_t> r(execution_mode, MachineType::Float64());
  BUILD(r, WASM_I64_UCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < static_cast<float>(UINT64_MAX) && *i > -1) {
      CHECK_EQ(static_cast<uint64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP64(r.Call(*i));
    }
  }
}

WASM_EXEC_TEST(I64ReinterpretF64) {
  REQUIRE(I64ReinterpretF64);
  TestingModule module(execution_mode);
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  WasmRunner<int64_t> r(&module);

  BUILD(r, WASM_I64_REINTERPRET_F64(
               WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int64_t expected = static_cast<int64_t>(*i) * 0x300010001;
    module.WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(F64ReinterpretI64) {
  REQUIRE(F64ReinterpretI64);
  TestingModule module(execution_mode);
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  WasmRunner<int64_t> r(&module, MachineType::Int64());

  BUILD(r, WASM_STORE_MEM(MachineType::Float64(), WASM_ZERO,
                          WASM_F64_REINTERPRET_I64(WASM_GET_LOCAL(0))),
        WASM_GET_LOCAL(0));

  FOR_INT32_INPUTS(i) {
    int64_t expected = static_cast<int64_t>(*i) * 0x300010001;
    CHECK_EQ(expected, r.Call(expected));
    CHECK_EQ(expected, module.ReadMemory<int64_t>(&memory[0]));
  }
}

WASM_EXEC_TEST(LoadMemI64) {
  REQUIRE(I64LoadStore);
  TestingModule module(execution_mode);
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  module.RandomizeMemory(1111);
  WasmRunner<int64_t> r(&module);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int64(), WASM_I8(0)));

  module.WriteMemory<int64_t>(&memory[0], 0xaabbccdd00112233LL);
  CHECK_EQ(0xaabbccdd00112233LL, r.Call());

  module.WriteMemory<int64_t>(&memory[0], 0x33aabbccdd001122LL);
  CHECK_EQ(0x33aabbccdd001122LL, r.Call());

  module.WriteMemory<int64_t>(&memory[0], 77777777);
  CHECK_EQ(77777777, r.Call());
}

WASM_EXEC_TEST(LoadMemI64_alignment) {
  REQUIRE(I64LoadStore);
  TestingModule module(execution_mode);
  int64_t* memory = module.AddMemoryElems<int64_t>(8);
  for (byte alignment = 0; alignment <= 3; alignment++) {
    module.RandomizeMemory(1111);
    WasmRunner<int64_t> r(&module);

    BUILD(r,
          WASM_LOAD_MEM_ALIGNMENT(MachineType::Int64(), WASM_I8(0), alignment));

    module.WriteMemory<int64_t>(&memory[0], 0xaabbccdd00112233LL);
    CHECK_EQ(0xaabbccdd00112233LL, r.Call());

    module.WriteMemory<int64_t>(&memory[0], 0x33aabbccdd001122LL);
    CHECK_EQ(0x33aabbccdd001122LL, r.Call());

    module.WriteMemory<int64_t>(&memory[0], 77777777);
    CHECK_EQ(77777777, r.Call());
  }
}

WASM_EXEC_TEST(MemI64_Sum) {
  REQUIRE(I64LoadStore);
  REQUIRE(I64Add);
  REQUIRE(I64Sub);
  REQUIRE(I64Phi);
  const int kNumElems = 20;
  TestingModule module(execution_mode);
  uint64_t* memory = module.AddMemoryElems<uint64_t>(kNumElems);
  WasmRunner<uint64_t> r(&module, MachineType::Int32());
  const byte kSum = r.AllocateLocal(kAstI64);

  BUILD(
      r,
      WASM_WHILE(
          WASM_GET_LOCAL(0),
          WASM_BLOCK(
              WASM_SET_LOCAL(kSum,
                             WASM_I64_ADD(WASM_GET_LOCAL(kSum),
                                          WASM_LOAD_MEM(MachineType::Int64(),
                                                        WASM_GET_LOCAL(0)))),
              WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(8))))),
      WASM_GET_LOCAL(1));

  // Run 4 trials.
  for (int i = 0; i < 3; i++) {
    module.RandomizeMemory(i * 33);
    uint64_t expected = 0;
    for (size_t j = kNumElems - 1; j > 0; j--) {
      expected += module.ReadMemory(&memory[j]);
    }
    uint64_t result = r.Call(8 * (kNumElems - 1));
    CHECK_EQ(expected, result);
  }
}

WASM_EXEC_TEST(StoreMemI64_alignment) {
  TestingModule module(execution_mode);
  int64_t* memory = module.AddMemoryElems<int64_t>(4);
  const int64_t kWritten = 0x12345678abcd0011ll;

  for (byte i = 0; i <= 3; i++) {
    WasmRunner<int64_t> r(&module, MachineType::Int64());
    BUILD(r, WASM_STORE_MEM_ALIGNMENT(MachineType::Int64(), WASM_ZERO, i,
                                      WASM_GET_LOCAL(0)),
          WASM_GET_LOCAL(0));
    module.RandomizeMemory(1111);
    module.WriteMemory<int64_t>(&memory[0], 0);

    CHECK_EQ(kWritten, r.Call(kWritten));
    CHECK_EQ(kWritten, module.ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I64Global) {
  REQUIRE(I64LoadStore);
  REQUIRE(I64SConvertI32);
  REQUIRE(I64And);
  REQUIRE(DepthFirst);
  TestingModule module(execution_mode);
  int64_t* global = module.AddGlobal<int64_t>(kAstI64);
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  // global = global + p0
  BUILD(r, WASM_SET_GLOBAL(
               0, WASM_I64_AND(WASM_GET_GLOBAL(0),
                               WASM_I64_SCONVERT_I32(WASM_GET_LOCAL(0)))),
        WASM_ZERO);

  module.WriteMemory<int64_t>(global, 0xFFFFFFFFFFFFFFFFLL);
  for (int i = 9; i < 444444; i += 111111) {
    int64_t expected = *global & i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}

WASM_EXEC_TEST(I64Eqz) {
  REQUIRE(I64Eq);

  WasmRunner<int32_t> r(execution_mode, MachineType::Int64());
  BUILD(r, WASM_I64_EQZ(WASM_GET_LOCAL(0)));

  FOR_INT64_INPUTS(i) {
    int32_t result = *i == 0 ? 1 : 0;
    CHECK_EQ(result, r.Call(*i));
  }
}

WASM_EXEC_TEST(I64Ror) {
  REQUIRE(I64Ror);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_ROR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) {
      int64_t expected = bits::RotateRight64(*i, *j & 0x3f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST(I64Rol) {
  REQUIRE(I64Rol);
  WasmRunner<int64_t> r(execution_mode, MachineType::Int64(),
                        MachineType::Int64());
  BUILD(r, WASM_I64_ROL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT64_INPUTS(i) {
    FOR_UINT64_INPUTS(j) {
      int64_t expected = bits::RotateLeft64(*i, *j & 0x3f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

WASM_EXEC_TEST(StoreMem_offset_oob_i64) {
  TestingModule module(execution_mode);
  byte* memory = module.AddMemoryElems<byte>(32);

  static const MachineType machineTypes[] = {
      MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
      MachineType::Float64()};

  for (size_t m = 0; m < arraysize(machineTypes); m++) {
    module.RandomizeMemory(1119 + static_cast<int>(m));
    WasmRunner<int32_t> r(&module, MachineType::Uint32());

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

#define ADD_CODE(vec, ...)                                              \
  do {                                                                  \
    byte __buf[] = {__VA_ARGS__};                                       \
    for (size_t i = 0; i < sizeof(__buf); i++) vec.push_back(__buf[i]); \
  } while (false)

static void CompileCallIndirectMany(LocalType param) {
  // Make sure we don't run out of registers when compiling indirect calls
  // with many many parameters.
  TestSignatures sigs;
  for (byte num_params = 0; num_params < 40; num_params++) {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator);
    HandleScope scope(CcTest::InitIsolateOnce());
    TestingModule module(kExecuteCompiled);
    FunctionSig* sig = sigs.many(&zone, kAstStmt, param, num_params);

    module.AddSignature(sig);
    module.AddSignature(sig);
    module.AddIndirectFunctionTable(nullptr, 0);

    WasmFunctionCompiler t(sig, &module);

    std::vector<byte> code;
    for (byte p = 0; p < num_params; p++) {
      ADD_CODE(code, kExprGetLocal, p);
    }
    ADD_CODE(code, kExprI8Const, 0);
    ADD_CODE(code, kExprCallIndirect, 1);

    t.Build(&code[0], &code[0] + code.size());
    t.Compile();
  }
}

TEST(Compile_Wasm_CallIndirect_Many_i64) { CompileCallIndirectMany(kAstI64); }

static void Run_WasmMixedCall_N(WasmExecutionMode execution_mode, int start) {
  const int kExpected = 6333;
  const int kElemSize = 8;
  TestSignatures sigs;

  static MachineType mixed[] = {
      MachineType::Int32(),   MachineType::Float32(), MachineType::Int64(),
      MachineType::Float64(), MachineType::Float32(), MachineType::Int64(),
      MachineType::Int32(),   MachineType::Float64(), MachineType::Float32(),
      MachineType::Float64(), MachineType::Int32(),   MachineType::Int64(),
      MachineType::Int32(),   MachineType::Int32()};

  int num_params = static_cast<int>(arraysize(mixed)) - start;
  for (int which = 0; which < num_params; which++) {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator);
    TestingModule module(execution_mode);
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
    WasmFunctionCompiler t(b.Build(), &module);
    BUILD(t, WASM_GET_LOCAL(which));
    index = t.CompileAndAdd();

    // =========================================================================
    // Build the calling function.
    // =========================================================================
    WasmRunner<int32_t> r(&module);
    std::vector<byte> code;

    // Load the offset for the store.
    ADD_CODE(code, WASM_ZERO);

    // Load the arguments.
    for (int i = 0; i < num_params; i++) {
      int offset = (i + 1) * kElemSize;
      ADD_CODE(code, WASM_LOAD_MEM(memtypes[i], WASM_I8(offset)));
    }

    // Call the selector function.
    ADD_CODE(code, kExprCallFunction, static_cast<byte>(index));

    // Store the result in memory.
    ADD_CODE(code,
             static_cast<byte>(WasmOpcodes::LoadStoreOpcodeOf(result, true)),
             ZERO_ALIGNMENT, ZERO_OFFSET);

    // Return the expected value.
    ADD_CODE(code, WASM_I32V_2(kExpected));

    r.Build(&code[0], &code[0] + code.size());

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

WASM_EXEC_TEST(MixedCall_i64_0) { Run_WasmMixedCall_N(execution_mode, 0); }
WASM_EXEC_TEST(MixedCall_i64_1) { Run_WasmMixedCall_N(execution_mode, 1); }
WASM_EXEC_TEST(MixedCall_i64_2) { Run_WasmMixedCall_N(execution_mode, 2); }
WASM_EXEC_TEST(MixedCall_i64_3) { Run_WasmMixedCall_N(execution_mode, 3); }

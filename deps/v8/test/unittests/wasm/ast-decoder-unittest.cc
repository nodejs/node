// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/v8.h"

#include "test/cctest/wasm/test-signatures.h"

#include "src/objects.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

#define B1(a) kExprBlock, a, kExprEnd
#define B2(a, b) kExprBlock, a, b, kExprEnd
#define B3(a, b, c) kExprBlock, a, b, c, kExprEnd

static const byte kCodeGetLocal0[] = {kExprGetLocal, 0};
static const byte kCodeGetLocal1[] = {kExprGetLocal, 1};
static const byte kCodeSetLocal0[] = {WASM_SET_LOCAL(0, WASM_ZERO)};

static const LocalType kLocalTypes[] = {kAstI32, kAstI64, kAstF32, kAstF64};
static const MachineType machineTypes[] = {
    MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
    MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
    MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
    MachineType::Float64()};

static const WasmOpcode kInt32BinopOpcodes[] = {
    kExprI32Add,  kExprI32Sub,  kExprI32Mul,  kExprI32DivS, kExprI32DivU,
    kExprI32RemS, kExprI32RemU, kExprI32And,  kExprI32Ior,  kExprI32Xor,
    kExprI32Shl,  kExprI32ShrU, kExprI32ShrS, kExprI32Eq,   kExprI32LtS,
    kExprI32LeS,  kExprI32LtU,  kExprI32LeU};

#define WASM_BRV_IF_ZERO(depth, val) \
  val, WASM_ZERO, kExprBrIf, ARITY_1, static_cast<byte>(depth)

#define EXPECT_VERIFIES(env, x) Verify(kSuccess, env, x, x + arraysize(x))

#define EXPECT_FAILURE(env, x) Verify(kError, env, x, x + arraysize(x))

#define EXPECT_VERIFIES_INLINE(env, ...)                 \
  do {                                                   \
    static byte code[] = {__VA_ARGS__};                  \
    Verify(kSuccess, env, code, code + arraysize(code)); \
  } while (false)

#define EXPECT_FAILURE_INLINE(env, ...)                \
  do {                                                 \
    static byte code[] = {__VA_ARGS__};                \
    Verify(kError, env, code, code + arraysize(code)); \
  } while (false)

#define VERIFY(...)                                          \
  do {                                                       \
    static const byte code[] = {__VA_ARGS__};                \
    Verify(kSuccess, sigs.v_i(), code, code + sizeof(code)); \
  } while (false)

class AstDecoderTest : public TestWithZone {
 public:
  typedef std::pair<uint32_t, LocalType> LocalsDecl;

  AstDecoderTest() : module(nullptr), local_decls(zone()) {}

  TestSignatures sigs;
  ModuleEnv* module;
  LocalDeclEncoder local_decls;

  void AddLocals(LocalType type, uint32_t count) {
    local_decls.AddLocals(count, type);
  }

  // Prepends local variable declarations and renders nice error messages for
  // verification failures.
  void Verify(ErrorCode expected, FunctionSig* sig, const byte* start,
              const byte* end) {
    local_decls.Prepend(zone(), &start, &end);
    // Verify the code.
    DecodeResult result =
        VerifyWasmCode(zone()->allocator(), module, sig, start, end);

    if (result.error_code != expected) {
      ptrdiff_t pc = result.error_pc - result.start;
      ptrdiff_t pt = result.error_pt - result.start;
      std::ostringstream str;
      if (expected == kSuccess) {
        str << "Verification failed: " << result.error_code << " pc = +" << pc;
        if (result.error_pt) str << ", pt = +" << pt;
        str << ", msg = " << result.error_msg.get();
      } else {
        str << "Verification expected: " << expected << ", but got "
            << result.error_code;
        if (result.error_code != kSuccess) {
          str << " pc = +" << pc;
          if (result.error_pt) str << ", pt = +" << pt;
        }
      }
      FATAL(str.str().c_str());
    }
  }

  void TestBinop(WasmOpcode opcode, FunctionSig* success) {
    // op(local[0], local[1])
    byte code[] = {WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
    EXPECT_VERIFIES(success, code);

    // Try all combinations of return and parameter types.
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
        for (size_t k = 0; k < arraysize(kLocalTypes); k++) {
          LocalType types[] = {kLocalTypes[i], kLocalTypes[j], kLocalTypes[k]};
          if (types[0] != success->GetReturn(0) ||
              types[1] != success->GetParam(0) ||
              types[2] != success->GetParam(1)) {
            // Test signature mismatch.
            FunctionSig sig(1, 2, types);
            EXPECT_FAILURE(&sig, code);
          }
        }
      }
    }
  }

  void TestUnop(WasmOpcode opcode, FunctionSig* success) {
    TestUnop(opcode, success->GetReturn(), success->GetParam(0));
  }

  void TestUnop(WasmOpcode opcode, LocalType ret_type, LocalType param_type) {
    // Return(op(local[0]))
    byte code[] = {WASM_UNOP(opcode, WASM_GET_LOCAL(0))};
    {
      LocalType types[] = {ret_type, param_type};
      FunctionSig sig(1, 1, types);
      EXPECT_VERIFIES(&sig, code);
    }

    // Try all combinations of return and parameter types.
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
        LocalType types[] = {kLocalTypes[i], kLocalTypes[j]};
        if (types[0] != ret_type || types[1] != param_type) {
          // Test signature mismatch.
          FunctionSig sig(1, 1, types);
          EXPECT_FAILURE(&sig, code);
        }
      }
    }
  }
};

TEST_F(AstDecoderTest, Int8Const) {
  byte code[] = {kExprI8Const, 0};
  for (int i = -128; i < 128; i++) {
    code[1] = static_cast<byte>(i);
    EXPECT_VERIFIES(sigs.i_i(), code);
  }
}

TEST_F(AstDecoderTest, EmptyFunction) {
  byte code[] = {0};
  Verify(kSuccess, sigs.v_v(), code, code);
  Verify(kError, sigs.i_i(), code, code);
}

TEST_F(AstDecoderTest, IncompleteIf1) {
  byte code[] = {kExprIf};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Int8Const_fallthru) {
  byte code[] = {kExprI8Const, 0, kExprI8Const, 1};
  EXPECT_VERIFIES(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Int32Const) {
  const int kInc = 4498211;
  for (int32_t i = kMinInt; i < kMaxInt - kInc; i = i + kInc) {
    // TODO(binji): expand test for other sized int32s; 1 through 5 bytes.
    byte code[] = {WASM_I32V(i)};
    EXPECT_VERIFIES(sigs.i_i(), code);
  }
}

TEST_F(AstDecoderTest, Int8Const_fallthru2) {
  byte code[] = {WASM_I8(0), WASM_I32V_4(0x1122334)};
  EXPECT_VERIFIES(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Int64Const) {
  const int kInc = 4498211;
  for (int32_t i = kMinInt; i < kMaxInt - kInc; i = i + kInc) {
    byte code[] = {WASM_I64V((static_cast<int64_t>(i) << 32) | i)};
    EXPECT_VERIFIES(sigs.l_l(), code);
  }
}

TEST_F(AstDecoderTest, Float32Const) {
  byte code[] = {kExprF32Const, 0, 0, 0, 0};
  float* ptr = reinterpret_cast<float*>(code + 1);
  for (int i = 0; i < 30; i++) {
    *ptr = i * -7.75f;
    EXPECT_VERIFIES(sigs.f_ff(), code);
  }
}

TEST_F(AstDecoderTest, Float64Const) {
  byte code[] = {kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0};
  double* ptr = reinterpret_cast<double*>(code + 1);
  for (int i = 0; i < 30; i++) {
    *ptr = i * 33.45;
    EXPECT_VERIFIES(sigs.d_dd(), code);
  }
}

TEST_F(AstDecoderTest, Int32Const_off_end) {
  byte code[] = {kExprI32Const, 0xaa, 0xbb, 0xcc, 0x44};

  for (int size = 1; size <= 4; size++) {
    Verify(kError, sigs.i_i(), code, code + size);
  }
}

TEST_F(AstDecoderTest, GetLocal0_param) {
  EXPECT_VERIFIES(sigs.i_i(), kCodeGetLocal0);
}

TEST_F(AstDecoderTest, GetLocal0_local) {
  AddLocals(kAstI32, 1);
  EXPECT_VERIFIES(sigs.i_v(), kCodeGetLocal0);
}

TEST_F(AstDecoderTest, GetLocal0_param_n) {
  FunctionSig* array[] = {sigs.i_i(), sigs.i_ii(), sigs.i_iii()};

  for (size_t i = 0; i < arraysize(array); i++) {
    EXPECT_VERIFIES(array[i], kCodeGetLocal0);
  }
}

TEST_F(AstDecoderTest, GetLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kAstI32, 1);
    for (byte j = 0; j < i; j++) {
      byte code[] = {kExprGetLocal, j};
      EXPECT_VERIFIES(sigs.i_v(), code);
    }
  }
}

TEST_F(AstDecoderTest, GetLocal0_fail_no_params) {
  EXPECT_FAILURE(sigs.i_v(), kCodeGetLocal0);
}

TEST_F(AstDecoderTest, GetLocal1_fail_no_locals) {
  EXPECT_FAILURE(sigs.i_i(), kCodeGetLocal1);
}

TEST_F(AstDecoderTest, GetLocal_off_end) {
  static const byte code[] = {kExprGetLocal};
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, GetLocal_varint) {
  const int kMaxLocals = 8000000;
  AddLocals(kAstI32, kMaxLocals);

  for (int index = 0; index < kMaxLocals; index = index * 11 + 5) {
    EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_1(index));
    EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_2(index));
    EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_3(index));
    EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_4(index));
  }

  EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_5(kMaxLocals - 1));

  EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_4(kMaxLocals - 1));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), kExprGetLocal, U32V_4(kMaxLocals));
  EXPECT_FAILURE_INLINE(sigs.i_i(), kExprGetLocal, U32V_4(kMaxLocals + 1));

  EXPECT_FAILURE_INLINE(sigs.i_v(), kExprGetLocal, U32V_4(kMaxLocals));
  EXPECT_FAILURE_INLINE(sigs.i_v(), kExprGetLocal, U32V_4(kMaxLocals + 1));
}

TEST_F(AstDecoderTest, Binops_off_end) {
  byte code1[] = {0};  // [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code1[0] = kInt32BinopOpcodes[i];
    EXPECT_FAILURE(sigs.i_i(), code1);
  }

  byte code3[] = {kExprGetLocal, 0, 0};  // [expr] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code3[2] = kInt32BinopOpcodes[i];
    EXPECT_FAILURE(sigs.i_i(), code3);
  }

  byte code4[] = {kExprGetLocal, 0, 0, 0};  // [expr] [opcode] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code4[2] = kInt32BinopOpcodes[i];
    code4[3] = kInt32BinopOpcodes[i];
    EXPECT_FAILURE(sigs.i_i(), code4);
  }
}

TEST_F(AstDecoderTest, BinopsAcrossBlock1) {
  static const byte code[] = {WASM_ZERO, kExprBlock, WASM_ZERO, kExprI32Add,
                              kExprEnd};
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, BinopsAcrossBlock2) {
  static const byte code[] = {WASM_ZERO, WASM_ZERO, kExprBlock, kExprI32Add,
                              kExprEnd};
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, BinopsAcrossBlock3) {
  static const byte code[] = {WASM_ZERO, WASM_ZERO,   kExprIf, kExprI32Add,
                              kExprElse, kExprI32Add, kExprEnd};
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Nop) {
  static const byte code[] = {kExprNop};
  EXPECT_VERIFIES(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, SetLocal0_param) {
  EXPECT_VERIFIES(sigs.i_i(), kCodeSetLocal0);
  EXPECT_FAILURE(sigs.f_ff(), kCodeSetLocal0);
  EXPECT_FAILURE(sigs.d_dd(), kCodeSetLocal0);
}

TEST_F(AstDecoderTest, SetLocal0_local) {
  EXPECT_FAILURE(sigs.i_v(), kCodeSetLocal0);
  AddLocals(kAstI32, 1);
  EXPECT_VERIFIES(sigs.i_v(), kCodeSetLocal0);
}

TEST_F(AstDecoderTest, SetLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kAstI32, 1);
    for (byte j = 0; j < i; j++) {
      EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_SET_LOCAL(j, WASM_I8(i)));
    }
  }
}

TEST_F(AstDecoderTest, BlockN) {
  const int kMaxSize = 200;
  byte buffer[kMaxSize + 2];

  for (int i = 0; i <= kMaxSize; i++) {
    memset(buffer, kExprNop, sizeof(buffer));
    buffer[0] = kExprBlock;
    buffer[i + 1] = kExprEnd;
    Verify(kSuccess, sigs.v_i(), buffer, buffer + i + 2);
  }
}

TEST_F(AstDecoderTest, Block0) {
  static const byte code[] = {kExprBlock, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Block0_fallthru1) {
  static const byte code[] = {kExprBlock, kExprBlock, kExprEnd, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Block0Block0) {
  static const byte code[] = {kExprBlock, kExprEnd, kExprBlock, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, Block0_end_end) {
  static const byte code[] = {kExprBlock, kExprEnd, kExprEnd};
  EXPECT_FAILURE(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, Block1) {
  byte code[] = {B1(WASM_SET_LOCAL(0, WASM_ZERO))};
  EXPECT_VERIFIES(sigs.i_i(), code);
  EXPECT_VERIFIES(sigs.v_i(), code);
  EXPECT_FAILURE(sigs.d_dd(), code);
}

TEST_F(AstDecoderTest, Block1_i) {
  byte code[] = {B1(WASM_ZERO)};
  EXPECT_VERIFIES(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.f_ff(), code);
  EXPECT_FAILURE(sigs.d_dd(), code);
  EXPECT_FAILURE(sigs.l_ll(), code);
}

TEST_F(AstDecoderTest, Block1_f) {
  byte code[] = {B1(WASM_F32(0))};
  EXPECT_FAILURE(sigs.i_i(), code);
  EXPECT_VERIFIES(sigs.f_ff(), code);
  EXPECT_FAILURE(sigs.d_dd(), code);
  EXPECT_FAILURE(sigs.l_ll(), code);
}

TEST_F(AstDecoderTest, Block1_continue) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B1(WASM_BR(0)));
  EXPECT_FAILURE_INLINE(sigs.v_v(), B1(WASM_BR(1)));
  EXPECT_FAILURE_INLINE(sigs.v_v(), B1(WASM_BR(2)));
  EXPECT_FAILURE_INLINE(sigs.v_v(), B1(WASM_BR(3)));
}

TEST_F(AstDecoderTest, Block1_br) {
  EXPECT_FAILURE_INLINE(sigs.v_v(), kExprBlock, kExprBr, ARITY_1, DEPTH_0,
                        kExprEnd);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprBlock, kExprBr, ARITY_0, DEPTH_0,
                         kExprEnd);
}

TEST_F(AstDecoderTest, Block2_br) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B2(WASM_NOP, WASM_BR(0)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B2(WASM_BR(0), WASM_NOP));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B2(WASM_BR(0), WASM_BR(0)));
}

TEST_F(AstDecoderTest, Block2) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         B2(WASM_NOP, WASM_SET_LOCAL(0, WASM_ZERO)));
  EXPECT_FAILURE_INLINE(sigs.i_i(), B2(WASM_SET_LOCAL(0, WASM_ZERO), WASM_NOP));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                        WASM_SET_LOCAL(0, WASM_ZERO)));
}

TEST_F(AstDecoderTest, Block2b) {
  byte code[] = {B2(WASM_SET_LOCAL(0, WASM_ZERO), WASM_ZERO)};
  EXPECT_VERIFIES(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.f_ff(), code);
}

TEST_F(AstDecoderTest, Block2_fallthru) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                        WASM_SET_LOCAL(0, WASM_ZERO)),
                         WASM_I8(23));
}

TEST_F(AstDecoderTest, Block3) {
  EXPECT_VERIFIES_INLINE(
      sigs.i_i(), B3(WASM_SET_LOCAL(0, WASM_ZERO), WASM_SET_LOCAL(0, WASM_ZERO),
                     WASM_I8(11)));
}

TEST_F(AstDecoderTest, Block5) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(), B1(WASM_GET_LOCAL(0)));

  EXPECT_VERIFIES_INLINE(sigs.v_i(), B2(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));

  EXPECT_VERIFIES_INLINE(
      sigs.v_i(), B3(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));

  EXPECT_VERIFIES_INLINE(sigs.v_i(),
                         WASM_BLOCK(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                    WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));

  EXPECT_VERIFIES_INLINE(
      sigs.v_i(),
      WASM_BLOCK(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                 WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, BlockF32) {
  static const byte code[] = {kExprBlock, kExprF32Const, 0, 0, 0, 0, kExprEnd};
  EXPECT_VERIFIES(sigs.f_ff(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.d_dd(), code);
}

TEST_F(AstDecoderTest, BlockN_off_end) {
  byte code[] = {kExprBlock, kExprNop, kExprNop, kExprNop, kExprNop, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
  for (size_t i = 1; i < arraysize(code); i++) {
    Verify(kError, sigs.v_v(), code, code + i);
  }
}

TEST_F(AstDecoderTest, Block2_continue) {
  static const byte code[] = {kExprBlock, kExprBr,  ARITY_0,
                              DEPTH_1,    kExprNop, kExprEnd};
  EXPECT_FAILURE(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, NestedBlock_return) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), B1(B1(WASM_RETURN1(WASM_ZERO))));
}

TEST_F(AstDecoderTest, BlockBinop) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_I32_AND(B1(WASM_I8(1)), WASM_I8(2)));
}

TEST_F(AstDecoderTest, BlockBrBinop) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         WASM_I32_AND(B1(WASM_BRV(0, WASM_I8(1))), WASM_I8(2)));
}

TEST_F(AstDecoderTest, If_empty1) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_ZERO, kExprIf, kExprEnd);
}

TEST_F(AstDecoderTest, If_empty2) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_ZERO, kExprIf, kExprElse, kExprEnd);
}

TEST_F(AstDecoderTest, If_empty3) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_ZERO, kExprIf, WASM_ZERO, kExprElse,
                         kExprEnd);
}

TEST_F(AstDecoderTest, If_empty4) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_ZERO, kExprIf, kExprElse, WASM_ZERO,
                         kExprEnd);
}

TEST_F(AstDecoderTest, If_empty_stack) {
  byte code[] = {kExprIf};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, If_incomplete1) {
  byte code[] = {kExprI8Const, 0, kExprIf};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, If_incomplete2) {
  byte code[] = {kExprI8Const, 0, kExprIf, kExprNop};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, If_else_else) {
  byte code[] = {kExprI8Const, 0, kExprIf, kExprElse, kExprElse, kExprEnd};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, IfEmpty) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(), kExprGetLocal, 0, kExprIf, kExprEnd);
}

TEST_F(AstDecoderTest, IfSet) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(), WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(),
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO), WASM_NOP));
}

TEST_F(AstDecoderTest, IfElseEmpty) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(), WASM_GET_LOCAL(0), kExprIf, kExprElse,
                         kExprEnd);
  EXPECT_VERIFIES_INLINE(sigs.v_i(),
                         WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, IfElseUnreachable1) {
  EXPECT_VERIFIES_INLINE(
      sigs.i_i(),
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_UNREACHABLE, WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES_INLINE(
      sigs.i_i(),
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_UNREACHABLE));
}

TEST_F(AstDecoderTest, IfElseUnreachable2) {
  static const byte code[] = {
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_UNREACHABLE, WASM_GET_LOCAL(0))};

  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType types[] = {kAstI32, kLocalTypes[i]};
    FunctionSig sig(1, 1, types);

    if (kLocalTypes[i] == kAstI32) {
      EXPECT_VERIFIES(&sig, code);
    } else {
      EXPECT_FAILURE(&sig, code);
    }
  }
}

TEST_F(AstDecoderTest, IfBreak) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(), WASM_IF(WASM_GET_LOCAL(0), WASM_BR(0)));
  EXPECT_FAILURE_INLINE(sigs.v_i(), WASM_IF(WASM_GET_LOCAL(0), WASM_BR(1)));
}

TEST_F(AstDecoderTest, IfElseBreak) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(),
                         WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(0)));
  EXPECT_FAILURE_INLINE(sigs.v_i(),
                        WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(1)));
}

TEST_F(AstDecoderTest, Block_else) {
  byte code[] = {kExprI8Const, 0, kExprBlock, kExprElse, kExprEnd};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, IfNop) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(), WASM_IF(WASM_GET_LOCAL(0), WASM_NOP));
}

TEST_F(AstDecoderTest, IfNopElseNop) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(),
                         WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, If_end_end) {
  static const byte code[] = {kExprGetLocal, 0, kExprIf, kExprEnd, kExprEnd};
  EXPECT_FAILURE(sigs.v_i(), code);
}

TEST_F(AstDecoderTest, If_falloff) {
  static const byte code[] = {kExprGetLocal, 0, kExprIf};
  EXPECT_FAILURE(sigs.v_i(), code);
}

TEST_F(AstDecoderTest, IfElse_falloff) {
  static const byte code[] = {kExprGetLocal, 0, kExprIf, kExprNop, kExprElse};
  EXPECT_FAILURE(sigs.v_i(), code);
}

TEST_F(AstDecoderTest, IfElseNop) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(),
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO), WASM_NOP));
}

TEST_F(AstDecoderTest, IfBlock1) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(), WASM_IF_ELSE(WASM_GET_LOCAL(0),
                               B1(WASM_SET_LOCAL(0, WASM_ZERO)), WASM_NOP));
}

TEST_F(AstDecoderTest, IfBlock1b) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(), WASM_IF(WASM_GET_LOCAL(0), B1(WASM_SET_LOCAL(0, WASM_ZERO))));
}

TEST_F(AstDecoderTest, IfBlock2a) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(), WASM_IF(WASM_GET_LOCAL(0), B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                                WASM_SET_LOCAL(0, WASM_ZERO))));
}

TEST_F(AstDecoderTest, IfBlock2b) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(),
      WASM_IF_ELSE(WASM_GET_LOCAL(0), B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                         WASM_SET_LOCAL(0, WASM_ZERO)),
                   WASM_NOP));
}

TEST_F(AstDecoderTest, IfElseSet) {
  EXPECT_VERIFIES_INLINE(
      sigs.v_i(), WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO),
                               WASM_SET_LOCAL(0, WASM_I8(1))));
}

TEST_F(AstDecoderTest, Loop0) {
  static const byte code[] = {kExprLoop, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, Loop1) {
  static const byte code[] = {WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO))};
  EXPECT_VERIFIES(sigs.v_i(), code);
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.f_ff(), code);
}

TEST_F(AstDecoderTest, Loop2) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(), WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO),
                                               WASM_SET_LOCAL(0, WASM_ZERO)));
}

TEST_F(AstDecoderTest, Loop1_continue) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BR(0)));
}

TEST_F(AstDecoderTest, Loop1_break) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BR(1)));
}

TEST_F(AstDecoderTest, Loop2_continue) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(),
                         WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO), WASM_BR(0)));
}

TEST_F(AstDecoderTest, Loop2_break) {
  EXPECT_VERIFIES_INLINE(sigs.v_i(),
                         WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO), WASM_BR(1)));
}

TEST_F(AstDecoderTest, ExprLoop0) {
  static const byte code[] = {kExprLoop, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, ExprLoop1a) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_LOOP(WASM_BRV(0, WASM_ZERO)));
}

TEST_F(AstDecoderTest, ExprLoop1b) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_LOOP(WASM_BRV(1, WASM_ZERO)));
  EXPECT_FAILURE_INLINE(sigs.f_ff(), WASM_LOOP(WASM_BRV(1, WASM_ZERO)));
}

TEST_F(AstDecoderTest, ExprLoop2_unreachable) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_LOOP(WASM_BR(0), WASM_NOP));
}

TEST_F(AstDecoderTest, ReturnVoid1) {
  static const byte code[] = {kExprNop};
  EXPECT_VERIFIES(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.i_f(), code);
}

TEST_F(AstDecoderTest, ReturnVoid2) {
  static const byte code[] = {kExprBlock, kExprBr, ARITY_0, DEPTH_0, kExprEnd};
  EXPECT_VERIFIES(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.i_f(), code);
}

TEST_F(AstDecoderTest, ReturnVoid3) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprI8Const, 0);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprI32Const, 0, 0, 0, 0);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprI64Const, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprF32Const, 0, 0, 0, 0);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0);

  EXPECT_VERIFIES_INLINE(sigs.v_i(), kExprGetLocal, 0);
}

TEST_F(AstDecoderTest, Unreachable1) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprUnreachable);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprUnreachable, kExprUnreachable);
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B2(WASM_UNREACHABLE, WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B2(WASM_BR(0), WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_UNREACHABLE, WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BR(0), WASM_ZERO));
}

TEST_F(AstDecoderTest, Unreachable_binop) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_I32_AND(WASM_ZERO, WASM_UNREACHABLE));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_I32_AND(WASM_UNREACHABLE, WASM_ZERO));
}

TEST_F(AstDecoderTest, Unreachable_select) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         WASM_SELECT(WASM_UNREACHABLE, WASM_ZERO, WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         WASM_SELECT(WASM_ZERO, WASM_UNREACHABLE, WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         WASM_SELECT(WASM_ZERO, WASM_ZERO, WASM_UNREACHABLE));
}

TEST_F(AstDecoderTest, If1) {
  EXPECT_VERIFIES_INLINE(
      sigs.i_i(), WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_I8(9), WASM_I8(8)));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_I8(9),
                                                  WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES_INLINE(
      sigs.i_i(),
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_I8(8)));
}

TEST_F(AstDecoderTest, If_off_end) {
  static const byte kCode[] = {
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0))};
  for (size_t len = 3; len < arraysize(kCode); len++) {
    Verify(kError, sigs.i_i(), kCode, kCode + len);
  }
}

TEST_F(AstDecoderTest, If_type1) {
  // float|double ? 1 : 2
  static const byte kCode[] = {
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_I8(0), WASM_I8(2))};
  EXPECT_VERIFIES(sigs.i_i(), kCode);
  EXPECT_FAILURE(sigs.i_f(), kCode);
  EXPECT_FAILURE(sigs.i_d(), kCode);
}

TEST_F(AstDecoderTest, If_type2) {
  // 1 ? float|double : 2
  static const byte kCode[] = {
      WASM_IF_ELSE(WASM_I8(1), WASM_GET_LOCAL(0), WASM_I8(1))};
  EXPECT_VERIFIES(sigs.i_i(), kCode);
  EXPECT_FAILURE(sigs.i_f(), kCode);
  EXPECT_FAILURE(sigs.i_d(), kCode);
}

TEST_F(AstDecoderTest, If_type3) {
  // stmt ? 0 : 1
  static const byte kCode[] = {WASM_IF_ELSE(WASM_NOP, WASM_I8(0), WASM_I8(1))};
  EXPECT_FAILURE(sigs.i_i(), kCode);
  EXPECT_FAILURE(sigs.i_f(), kCode);
  EXPECT_FAILURE(sigs.i_d(), kCode);
}

TEST_F(AstDecoderTest, If_type4) {
  // 0 ? stmt : 1
  static const byte kCode[] = {
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_I8(1))};
  EXPECT_FAILURE(sigs.i_i(), kCode);
  EXPECT_FAILURE(sigs.i_f(), kCode);
  EXPECT_FAILURE(sigs.i_d(), kCode);
}

TEST_F(AstDecoderTest, If_type5) {
  // 0 ? 1 : stmt
  static const byte kCode[] = {WASM_IF_ELSE(WASM_ZERO, WASM_I8(1), WASM_NOP)};
  EXPECT_FAILURE(sigs.i_i(), kCode);
  EXPECT_FAILURE(sigs.i_f(), kCode);
  EXPECT_FAILURE(sigs.i_d(), kCode);
}

TEST_F(AstDecoderTest, Int64Local_param) {
  EXPECT_VERIFIES(sigs.l_l(), kCodeGetLocal0);
}

TEST_F(AstDecoderTest, Int64Locals) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kAstI64, 1);
    for (byte j = 0; j < i; j++) {
      EXPECT_VERIFIES_INLINE(sigs.l_v(), WASM_GET_LOCAL(j));
    }
  }
}

TEST_F(AstDecoderTest, Int32Binops) {
  TestBinop(kExprI32Add, sigs.i_ii());
  TestBinop(kExprI32Sub, sigs.i_ii());
  TestBinop(kExprI32Mul, sigs.i_ii());
  TestBinop(kExprI32DivS, sigs.i_ii());
  TestBinop(kExprI32DivU, sigs.i_ii());
  TestBinop(kExprI32RemS, sigs.i_ii());
  TestBinop(kExprI32RemU, sigs.i_ii());
  TestBinop(kExprI32And, sigs.i_ii());
  TestBinop(kExprI32Ior, sigs.i_ii());
  TestBinop(kExprI32Xor, sigs.i_ii());
  TestBinop(kExprI32Shl, sigs.i_ii());
  TestBinop(kExprI32ShrU, sigs.i_ii());
  TestBinop(kExprI32ShrS, sigs.i_ii());
  TestBinop(kExprI32Eq, sigs.i_ii());
  TestBinop(kExprI32LtS, sigs.i_ii());
  TestBinop(kExprI32LeS, sigs.i_ii());
  TestBinop(kExprI32LtU, sigs.i_ii());
  TestBinop(kExprI32LeU, sigs.i_ii());
}

TEST_F(AstDecoderTest, DoubleBinops) {
  TestBinop(kExprF64Add, sigs.d_dd());
  TestBinop(kExprF64Sub, sigs.d_dd());
  TestBinop(kExprF64Mul, sigs.d_dd());
  TestBinop(kExprF64Div, sigs.d_dd());

  TestBinop(kExprF64Eq, sigs.i_dd());
  TestBinop(kExprF64Lt, sigs.i_dd());
  TestBinop(kExprF64Le, sigs.i_dd());
}

TEST_F(AstDecoderTest, FloatBinops) {
  TestBinop(kExprF32Add, sigs.f_ff());
  TestBinop(kExprF32Sub, sigs.f_ff());
  TestBinop(kExprF32Mul, sigs.f_ff());
  TestBinop(kExprF32Div, sigs.f_ff());

  TestBinop(kExprF32Eq, sigs.i_ff());
  TestBinop(kExprF32Lt, sigs.i_ff());
  TestBinop(kExprF32Le, sigs.i_ff());
}

TEST_F(AstDecoderTest, TypeConversions) {
  TestUnop(kExprI32SConvertF32, kAstI32, kAstF32);
  TestUnop(kExprI32SConvertF64, kAstI32, kAstF64);
  TestUnop(kExprI32UConvertF32, kAstI32, kAstF32);
  TestUnop(kExprI32UConvertF64, kAstI32, kAstF64);
  TestUnop(kExprF64SConvertI32, kAstF64, kAstI32);
  TestUnop(kExprF64UConvertI32, kAstF64, kAstI32);
  TestUnop(kExprF64ConvertF32, kAstF64, kAstF32);
  TestUnop(kExprF32SConvertI32, kAstF32, kAstI32);
  TestUnop(kExprF32UConvertI32, kAstF32, kAstI32);
  TestUnop(kExprF32ConvertF64, kAstF32, kAstF64);
}

TEST_F(AstDecoderTest, MacrosStmt) {
  VERIFY(WASM_SET_LOCAL(0, WASM_I32V_3(87348)));
  VERIFY(WASM_STORE_MEM(MachineType::Int32(), WASM_I8(24), WASM_I8(40)));
  VERIFY(WASM_IF(WASM_GET_LOCAL(0), WASM_NOP));
  VERIFY(WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP));
  VERIFY(WASM_NOP);
  VERIFY(B1(WASM_NOP));
  VERIFY(WASM_LOOP(WASM_NOP));
  VERIFY(WASM_LOOP(WASM_BREAK(0)));
  VERIFY(WASM_LOOP(WASM_CONTINUE(0)));
}

TEST_F(AstDecoderTest, MacrosBreak) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BREAK(0)));

  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_LOOP(WASM_BREAKV(0, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.l_l(), WASM_LOOP(WASM_BREAKV(0, WASM_I64V_1(0))));
  EXPECT_VERIFIES_INLINE(sigs.f_ff(), WASM_LOOP(WASM_BREAKV(0, WASM_F32(0.0))));
  EXPECT_VERIFIES_INLINE(sigs.d_dd(), WASM_LOOP(WASM_BREAKV(0, WASM_F64(0.0))));
}

TEST_F(AstDecoderTest, MacrosContinue) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_CONTINUE(0)));
}

TEST_F(AstDecoderTest, MacrosVariadic) {
  VERIFY(B2(WASM_NOP, WASM_NOP));
  VERIFY(B3(WASM_NOP, WASM_NOP, WASM_NOP));
  VERIFY(WASM_LOOP(WASM_NOP, WASM_NOP));
  VERIFY(WASM_LOOP(WASM_NOP, WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, MacrosNestedBlocks) {
  VERIFY(B2(WASM_NOP, B2(WASM_NOP, WASM_NOP)));
  VERIFY(B3(WASM_NOP,                  // --
            B2(WASM_NOP, WASM_NOP),    // --
            B2(WASM_NOP, WASM_NOP)));  // --
  VERIFY(B1(B1(B2(WASM_NOP, WASM_NOP))));
}

TEST_F(AstDecoderTest, MultipleReturn) {
  static LocalType kIntTypes5[] = {kAstI32, kAstI32, kAstI32, kAstI32, kAstI32};
  FunctionSig sig_ii_v(2, 0, kIntTypes5);
  EXPECT_VERIFIES_INLINE(&sig_ii_v, WASM_RETURNN(2, WASM_ZERO, WASM_ONE));
  EXPECT_FAILURE_INLINE(&sig_ii_v, WASM_RETURNN(1, WASM_ZERO));

  FunctionSig sig_iii_v(3, 0, kIntTypes5);
  EXPECT_VERIFIES_INLINE(&sig_iii_v,
                         WASM_RETURNN(3, WASM_ZERO, WASM_ONE, WASM_I8(44)));
  EXPECT_FAILURE_INLINE(&sig_iii_v, WASM_RETURNN(2, WASM_ZERO, WASM_ONE));
}

TEST_F(AstDecoderTest, MultipleReturn_fallthru) {
  static LocalType kIntTypes5[] = {kAstI32, kAstI32, kAstI32, kAstI32, kAstI32};
  FunctionSig sig_ii_v(2, 0, kIntTypes5);

  EXPECT_VERIFIES_INLINE(&sig_ii_v, WASM_ZERO, WASM_ONE);
  EXPECT_FAILURE_INLINE(&sig_ii_v, WASM_ZERO);

  FunctionSig sig_iii_v(3, 0, kIntTypes5);
  EXPECT_VERIFIES_INLINE(&sig_iii_v, WASM_ZERO, WASM_ONE, WASM_I8(44));
  EXPECT_FAILURE_INLINE(&sig_iii_v, WASM_ZERO, WASM_ONE);
}

TEST_F(AstDecoderTest, MacrosInt32) {
  VERIFY(WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_I8(12)));
  VERIFY(WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(13)));
  VERIFY(WASM_I32_MUL(WASM_GET_LOCAL(0), WASM_I8(14)));
  VERIFY(WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I8(15)));
  VERIFY(WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_I8(16)));
  VERIFY(WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_I8(17)));
  VERIFY(WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_I8(18)));
  VERIFY(WASM_I32_AND(WASM_GET_LOCAL(0), WASM_I8(19)));
  VERIFY(WASM_I32_IOR(WASM_GET_LOCAL(0), WASM_I8(20)));
  VERIFY(WASM_I32_XOR(WASM_GET_LOCAL(0), WASM_I8(21)));
  VERIFY(WASM_I32_SHL(WASM_GET_LOCAL(0), WASM_I8(22)));
  VERIFY(WASM_I32_SHR(WASM_GET_LOCAL(0), WASM_I8(23)));
  VERIFY(WASM_I32_SAR(WASM_GET_LOCAL(0), WASM_I8(24)));
  VERIFY(WASM_I32_ROR(WASM_GET_LOCAL(0), WASM_I8(24)));
  VERIFY(WASM_I32_ROL(WASM_GET_LOCAL(0), WASM_I8(24)));
  VERIFY(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(25)));
  VERIFY(WASM_I32_NE(WASM_GET_LOCAL(0), WASM_I8(25)));

  VERIFY(WASM_I32_LTS(WASM_GET_LOCAL(0), WASM_I8(26)));
  VERIFY(WASM_I32_LES(WASM_GET_LOCAL(0), WASM_I8(27)));
  VERIFY(WASM_I32_LTU(WASM_GET_LOCAL(0), WASM_I8(28)));
  VERIFY(WASM_I32_LEU(WASM_GET_LOCAL(0), WASM_I8(29)));

  VERIFY(WASM_I32_GTS(WASM_GET_LOCAL(0), WASM_I8(26)));
  VERIFY(WASM_I32_GES(WASM_GET_LOCAL(0), WASM_I8(27)));
  VERIFY(WASM_I32_GTU(WASM_GET_LOCAL(0), WASM_I8(28)));
  VERIFY(WASM_I32_GEU(WASM_GET_LOCAL(0), WASM_I8(29)));
}

TEST_F(AstDecoderTest, MacrosInt64) {
#define VERIFY_L_LL(...) EXPECT_VERIFIES_INLINE(sigs.l_ll(), __VA_ARGS__)
#define VERIFY_I_LL(...) EXPECT_VERIFIES_INLINE(sigs.i_ll(), __VA_ARGS__)

  VERIFY_L_LL(WASM_I64_ADD(WASM_GET_LOCAL(0), WASM_I64V_1(12)));
  VERIFY_L_LL(WASM_I64_SUB(WASM_GET_LOCAL(0), WASM_I64V_1(13)));
  VERIFY_L_LL(WASM_I64_MUL(WASM_GET_LOCAL(0), WASM_I64V_1(14)));
  VERIFY_L_LL(WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_I64V_1(15)));
  VERIFY_L_LL(WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_I64V_1(16)));
  VERIFY_L_LL(WASM_I64_REMS(WASM_GET_LOCAL(0), WASM_I64V_1(17)));
  VERIFY_L_LL(WASM_I64_REMU(WASM_GET_LOCAL(0), WASM_I64V_1(18)));
  VERIFY_L_LL(WASM_I64_AND(WASM_GET_LOCAL(0), WASM_I64V_1(19)));
  VERIFY_L_LL(WASM_I64_IOR(WASM_GET_LOCAL(0), WASM_I64V_1(20)));
  VERIFY_L_LL(WASM_I64_XOR(WASM_GET_LOCAL(0), WASM_I64V_1(21)));

  VERIFY_L_LL(WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(22)));
  VERIFY_L_LL(WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(23)));
  VERIFY_L_LL(WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(24)));
  VERIFY_L_LL(WASM_I64_ROR(WASM_GET_LOCAL(0), WASM_I64V_1(24)));
  VERIFY_L_LL(WASM_I64_ROL(WASM_GET_LOCAL(0), WASM_I64V_1(24)));

  VERIFY_I_LL(WASM_I64_LTS(WASM_GET_LOCAL(0), WASM_I64V_1(26)));
  VERIFY_I_LL(WASM_I64_LES(WASM_GET_LOCAL(0), WASM_I64V_1(27)));
  VERIFY_I_LL(WASM_I64_LTU(WASM_GET_LOCAL(0), WASM_I64V_1(28)));
  VERIFY_I_LL(WASM_I64_LEU(WASM_GET_LOCAL(0), WASM_I64V_1(29)));

  VERIFY_I_LL(WASM_I64_GTS(WASM_GET_LOCAL(0), WASM_I64V_1(26)));
  VERIFY_I_LL(WASM_I64_GES(WASM_GET_LOCAL(0), WASM_I64V_1(27)));
  VERIFY_I_LL(WASM_I64_GTU(WASM_GET_LOCAL(0), WASM_I64V_1(28)));
  VERIFY_I_LL(WASM_I64_GEU(WASM_GET_LOCAL(0), WASM_I64V_1(29)));

  VERIFY_I_LL(WASM_I64_EQ(WASM_GET_LOCAL(0), WASM_I64V_1(25)));
  VERIFY_I_LL(WASM_I64_NE(WASM_GET_LOCAL(0), WASM_I64V_1(25)));
}

TEST_F(AstDecoderTest, AllSimpleExpressions) {
// Test all simple expressions which are described by a signature.
#define DECODE_TEST(name, opcode, sig)                      \
  {                                                         \
    FunctionSig* sig = WasmOpcodes::Signature(kExpr##name); \
    if (sig->parameter_count() == 1) {                      \
      TestUnop(kExpr##name, sig);                           \
    } else {                                                \
      TestBinop(kExpr##name, sig);                          \
    }                                                       \
  }

  FOREACH_SIMPLE_OPCODE(DECODE_TEST);

#undef DECODE_TEST
}

TEST_F(AstDecoderTest, MemorySize) {
  byte code[] = {kExprMemorySize};
  EXPECT_VERIFIES(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.f_ff(), code);
}

TEST_F(AstDecoderTest, GrowMemory) {
  byte code[] = {WASM_UNOP(kExprGrowMemory, WASM_GET_LOCAL(0))};
  EXPECT_VERIFIES(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.i_d(), code);
}

TEST_F(AstDecoderTest, LoadMemOffset) {
  for (int offset = 0; offset < 128; offset += 7) {
    byte code[] = {kExprI8Const, 0, kExprI32LoadMem, ZERO_ALIGNMENT,
                   static_cast<byte>(offset)};
    EXPECT_VERIFIES(sigs.i_i(), code);
  }
}

TEST_F(AstDecoderTest, StoreMemOffset) {
  for (int offset = 0; offset < 128; offset += 7) {
    byte code[] = {WASM_STORE_MEM_OFFSET(MachineType::Int32(), offset,
                                         WASM_ZERO, WASM_ZERO)};
    EXPECT_VERIFIES(sigs.i_i(), code);
  }
}

#define BYTE0(x) ((x)&0x7F)
#define BYTE1(x) ((x >> 7) & 0x7F)
#define BYTE2(x) ((x >> 14) & 0x7F)
#define BYTE3(x) ((x >> 21) & 0x7F)

#define VARINT1(x) BYTE0(x)
#define VARINT2(x) BYTE0(x) | 0x80, BYTE1(x)
#define VARINT3(x) BYTE0(x) | 0x80, BYTE1(x) | 0x80, BYTE2(x)
#define VARINT4(x) BYTE0(x) | 0x80, BYTE1(x) | 0x80, BYTE2(x) | 0x80, BYTE3(x)

TEST_F(AstDecoderTest, LoadMemOffset_varint) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                         VARINT1(0x45));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                         VARINT2(0x3999));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                         VARINT3(0x344445));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                         VARINT4(0x36666667));
}

TEST_F(AstDecoderTest, StoreMemOffset_varint) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                         ZERO_ALIGNMENT, VARINT1(0x33));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                         ZERO_ALIGNMENT, VARINT2(0x1111));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                         ZERO_ALIGNMENT, VARINT3(0x222222));
  EXPECT_VERIFIES_INLINE(sigs.i_i(), WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                         ZERO_ALIGNMENT, VARINT4(0x44444444));
}

TEST_F(AstDecoderTest, AllLoadMemCombinations) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType local_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_LOAD_MEM(mem_type, WASM_ZERO)};
      FunctionSig sig(1, 0, &local_type);
      if (local_type == WasmOpcodes::LocalTypeFor(mem_type)) {
        EXPECT_VERIFIES(&sig, code);
      } else {
        EXPECT_FAILURE(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, AllStoreMemCombinations) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType local_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_STORE_MEM(mem_type, WASM_ZERO, WASM_GET_LOCAL(0))};
      FunctionSig sig(0, 1, &local_type);
      if (local_type == WasmOpcodes::LocalTypeFor(mem_type)) {
        EXPECT_VERIFIES(&sig, code);
      } else {
        EXPECT_FAILURE(&sig, code);
      }
    }
  }
}

namespace {
// A helper for tests that require a module environment for functions and
// globals.
class TestModuleEnv : public ModuleEnv {
 public:
  TestModuleEnv() {
    instance = nullptr;
    module = &mod;
  }
  byte AddGlobal(LocalType type) {
    mod.globals.push_back({0, 0, type, 0, false});
    CHECK(mod.globals.size() <= 127);
    return static_cast<byte>(mod.globals.size() - 1);
  }
  byte AddSignature(FunctionSig* sig) {
    mod.signatures.push_back(sig);
    CHECK(mod.signatures.size() <= 127);
    return static_cast<byte>(mod.signatures.size() - 1);
  }
  byte AddFunction(FunctionSig* sig) {
    mod.functions.push_back({sig,  // sig
                             0,    // func_index
                             0,    // sig_index
                             0,    // name_offset
                             0,    // name_length
                             0,    // code_start_offset
                             0});  // code_end_offset
    CHECK(mod.functions.size() <= 127);
    return static_cast<byte>(mod.functions.size() - 1);
  }
  byte AddImport(FunctionSig* sig) {
    mod.import_table.push_back({sig,  // sig
                                0,    // sig_index
                                0,    // module_name_offset
                                0,    // module_name_length
                                0,    // function_name_offset
                                0});  // function_name_length
    CHECK(mod.import_table.size() <= 127);
    return static_cast<byte>(mod.import_table.size() - 1);
  }

 private:
  WasmModule mod;
};
}  // namespace

TEST_F(AstDecoderTest, SimpleCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_v());
  module_env.AddFunction(sigs.i_i());
  module_env.AddFunction(sigs.i_ii());

  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_FUNCTION0(0));
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_FUNCTION1(1, WASM_I8(27)));
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_FUNCTION2(2, WASM_I8(37), WASM_I8(77)));
}

TEST_F(AstDecoderTest, CallsWithTooFewArguments) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_i());
  module_env.AddFunction(sigs.i_ii());
  module_env.AddFunction(sigs.f_ff());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION0(0));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(1, WASM_ZERO));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(2, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, CallsWithMismatchedSigs2) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_i());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(0, WASM_I64V_1(17)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(0, WASM_F32(17.1)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(0, WASM_F64(17.1)));
}

TEST_F(AstDecoderTest, CallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_f());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(0, WASM_I8(17)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(0, WASM_I64V_1(27)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(0, WASM_F64(37.2)));

  module_env.AddFunction(sigs.i_d());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(1, WASM_I8(16)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(1, WASM_I64V_1(16)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_FUNCTION1(1, WASM_F32(17.6)));
}

TEST_F(AstDecoderTest, SimpleIndirectCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddSignature(sigs.i_v());
  byte f1 = module_env.AddSignature(sigs.i_i());
  byte f2 = module_env.AddSignature(sigs.i_ii());

  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_INDIRECT0(f0, WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I8(22)));
  EXPECT_VERIFIES_INLINE(
      sig, WASM_CALL_INDIRECT2(f2, WASM_ZERO, WASM_I8(32), WASM_I8(72)));
}

TEST_F(AstDecoderTest, IndirectCallsOutOfBounds) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT0(0, WASM_ZERO));
  module_env.AddSignature(sigs.i_v());
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_INDIRECT0(0, WASM_ZERO));

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT1(1, WASM_ZERO, WASM_I8(22)));
  module_env.AddSignature(sigs.i_i());
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_INDIRECT1(1, WASM_ZERO, WASM_I8(27)));

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT1(2, WASM_ZERO, WASM_I8(27)));
}

TEST_F(AstDecoderTest, IndirectCallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddFunction(sigs.i_f());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_I8(17)));
  EXPECT_FAILURE_INLINE(sig,
                        WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_I64V_1(27)));
  EXPECT_FAILURE_INLINE(sig,
                        WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_F64(37.2)));

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT0(f0, WASM_I8(17)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT0(f0, WASM_I64V_1(27)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT0(f0, WASM_F64(37.2)));

  byte f1 = module_env.AddFunction(sigs.i_d());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I8(16)));
  EXPECT_FAILURE_INLINE(sig,
                        WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I64V_1(16)));
  EXPECT_FAILURE_INLINE(sig,
                        WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_F32(17.6)));
}

TEST_F(AstDecoderTest, SimpleImportCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddImport(sigs.i_v());
  byte f1 = module_env.AddImport(sigs.i_i());
  byte f2 = module_env.AddImport(sigs.i_ii());

  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_IMPORT0(f0));
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_IMPORT1(f1, WASM_I8(22)));
  EXPECT_VERIFIES_INLINE(sig, WASM_CALL_IMPORT2(f2, WASM_I8(32), WASM_I8(72)));
}

TEST_F(AstDecoderTest, ImportCallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddImport(sigs.i_f());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT0(f0));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT1(f0, WASM_I8(17)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT1(f0, WASM_I64V_1(27)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT1(f0, WASM_F64(37.2)));

  byte f1 = module_env.AddImport(sigs.i_d());

  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT0(f1));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT1(f1, WASM_I8(16)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT1(f1, WASM_I64V_1(16)));
  EXPECT_FAILURE_INLINE(sig, WASM_CALL_IMPORT1(f1, WASM_F32(17.6)));
}

TEST_F(AstDecoderTest, Int32Globals) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstI32);

  EXPECT_VERIFIES_INLINE(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_INLINE(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, Int32Globals_fail) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstI64);
  module_env.AddGlobal(kAstI64);
  module_env.AddGlobal(kAstF32);
  module_env.AddGlobal(kAstF64);

  EXPECT_FAILURE_INLINE(sig, WASM_GET_GLOBAL(0));
  EXPECT_FAILURE_INLINE(sig, WASM_GET_GLOBAL(1));
  EXPECT_FAILURE_INLINE(sig, WASM_GET_GLOBAL(2));
  EXPECT_FAILURE_INLINE(sig, WASM_GET_GLOBAL(3));

  EXPECT_FAILURE_INLINE(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
  EXPECT_FAILURE_INLINE(sig, WASM_SET_GLOBAL(1, WASM_GET_LOCAL(0)));
  EXPECT_FAILURE_INLINE(sig, WASM_SET_GLOBAL(2, WASM_GET_LOCAL(0)));
  EXPECT_FAILURE_INLINE(sig, WASM_SET_GLOBAL(3, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, Int64Globals) {
  FunctionSig* sig = sigs.l_l();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstI64);
  module_env.AddGlobal(kAstI64);

  EXPECT_VERIFIES_INLINE(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_INLINE(sig, WASM_GET_GLOBAL(1));

  EXPECT_VERIFIES_INLINE(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES_INLINE(sig, WASM_SET_GLOBAL(1, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, Float32Globals) {
  FunctionSig* sig = sigs.f_ff();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstF32);

  EXPECT_VERIFIES_INLINE(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_INLINE(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, Float64Globals) {
  FunctionSig* sig = sigs.d_dd();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstF64);

  EXPECT_VERIFIES_INLINE(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_INLINE(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, AllGetGlobalCombinations) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType local_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType global_type = kLocalTypes[j];
      FunctionSig sig(1, 0, &local_type);
      TestModuleEnv module_env;
      module = &module_env;
      module_env.AddGlobal(global_type);
      if (local_type == global_type) {
        EXPECT_VERIFIES_INLINE(&sig, WASM_GET_GLOBAL(0));
      } else {
        EXPECT_FAILURE_INLINE(&sig, WASM_GET_GLOBAL(0));
      }
    }
  }
}

TEST_F(AstDecoderTest, AllSetGlobalCombinations) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType local_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType global_type = kLocalTypes[j];
      FunctionSig sig(0, 1, &local_type);
      TestModuleEnv module_env;
      module = &module_env;
      module_env.AddGlobal(global_type);
      if (local_type == global_type) {
        EXPECT_VERIFIES_INLINE(&sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
      } else {
        EXPECT_FAILURE_INLINE(&sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
      }
    }
  }
}

TEST_F(AstDecoderTest, BreakEnd) {
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         B1(WASM_I32_ADD(WASM_BRV(0, WASM_ZERO), WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.i_i(),
                         B1(WASM_I32_ADD(WASM_ZERO, WASM_BRV(0, WASM_ZERO))));
}

TEST_F(AstDecoderTest, BreakIfBinop) {
  EXPECT_FAILURE_INLINE(
      sigs.i_i(), WASM_BLOCK(WASM_I32_ADD(WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO),
                                          WASM_ZERO)));
  EXPECT_FAILURE_INLINE(sigs.i_i(),
                        WASM_BLOCK(WASM_I32_ADD(
                            WASM_ZERO, WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO))));
}

TEST_F(AstDecoderTest, BreakNesting1) {
  for (int i = 0; i < 5; i++) {
    // (block[2] (loop[2] (if (get p) break[N]) (set p 1)) p)
    byte code[] = {WASM_BLOCK(
        WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(i + 1, WASM_ZERO)),
                  WASM_SET_LOCAL(0, WASM_I8(1))),
        WASM_GET_LOCAL(0))};
    if (i < 3) {
      EXPECT_VERIFIES(sigs.i_i(), code);
    } else {
      EXPECT_FAILURE(sigs.i_i(), code);
    }
  }
}

TEST_F(AstDecoderTest, BreakNesting2) {
  AddLocals(kAstI32, 1);
  for (int i = 0; i < 5; i++) {
    // (block[2] (loop[2] (if 0 break[N]) (set p 1)) (return p)) (11)
    byte code[] = {B1(WASM_LOOP(WASM_IF(WASM_ZERO, WASM_BREAK(i + 1)),
                                WASM_SET_LOCAL(0, WASM_I8(1)))),
                   WASM_I8(11)};
    if (i < 2) {
      EXPECT_VERIFIES(sigs.v_v(), code);
    } else {
      EXPECT_FAILURE(sigs.v_v(), code);
    }
  }
}

TEST_F(AstDecoderTest, BreakNesting3) {
  for (int i = 0; i < 5; i++) {
    // (block[1] (loop[1] (block[1] (if 0 break[N])
    byte code[] = {
        WASM_BLOCK(WASM_LOOP(B1(WASM_IF(WASM_ZERO, WASM_BREAK(i + 1)))))};
    if (i < 3) {
      EXPECT_VERIFIES(sigs.v_v(), code);
    } else {
      EXPECT_FAILURE(sigs.v_v(), code);
    }
  }
}

TEST_F(AstDecoderTest, BreaksWithMultipleTypes) {
  EXPECT_FAILURE_INLINE(sigs.i_i(),
                        B2(WASM_BRV_IF_ZERO(0, WASM_I8(7)), WASM_F32(7.7)));

  EXPECT_FAILURE_INLINE(sigs.i_i(), B2(WASM_BRV_IF_ZERO(0, WASM_I8(7)),
                                       WASM_BRV_IF_ZERO(0, WASM_F32(7.7))));
  EXPECT_FAILURE_INLINE(sigs.i_i(), B3(WASM_BRV_IF_ZERO(0, WASM_I8(8)),
                                       WASM_BRV_IF_ZERO(0, WASM_I8(0)),
                                       WASM_BRV_IF_ZERO(0, WASM_F32(7.7))));
  EXPECT_FAILURE_INLINE(sigs.i_i(), B3(WASM_BRV_IF_ZERO(0, WASM_I8(9)),
                                       WASM_BRV_IF_ZERO(0, WASM_F32(7.7)),
                                       WASM_BRV_IF_ZERO(0, WASM_I8(11))));
}

TEST_F(AstDecoderTest, BreakNesting_6_levels) {
  for (int mask = 0; mask < 64; mask++) {
    for (int i = 0; i < 14; i++) {
      byte code[] = {
          kExprBlock,                                 // --
          kExprBlock,                                 // --
          kExprBlock,                                 // --
          kExprBlock,                                 // --
          kExprBlock,                                 // --
          kExprBlock,                                 // --
          kExprBr,    ARITY_0, static_cast<byte>(i),  // --
          kExprEnd,                                   // --
          kExprEnd,                                   // --
          kExprEnd,                                   // --
          kExprEnd,                                   // --
          kExprEnd,                                   // --
          kExprEnd                                    // --
      };

      int depth = 6;
      for (int l = 0; l < 6; l++) {
        if (mask & (1 << l)) {
          code[l] = kExprLoop;
          depth++;
        }
      }

      if (i < depth) {
        EXPECT_VERIFIES(sigs.v_v(), code);
      } else {
        EXPECT_FAILURE(sigs.v_v(), code);
      }
    }
  }
}

TEST_F(AstDecoderTest, ExprBreak_TypeCheck) {
  FunctionSig* sigarray[] = {sigs.i_i(), sigs.l_l(), sigs.f_ff(), sigs.d_dd()};
  for (size_t i = 0; i < arraysize(sigarray); i++) {
    FunctionSig* sig = sigarray[i];
    // unify X and X => OK
    EXPECT_VERIFIES_INLINE(
        sig, B2(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
                WASM_GET_LOCAL(0)));
  }

  // unify i32 and f32 => fail
  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      B2(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_ZERO)), WASM_F32(1.2)));

  // unify f64 and f64 => OK
  EXPECT_VERIFIES_INLINE(
      sigs.d_dd(),
      B2(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))), WASM_F64(1.2)));
}

TEST_F(AstDecoderTest, ExprBreak_TypeCheckAll) {
  byte code1[] = {WASM_BLOCK(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
                             WASM_GET_LOCAL(1))};
  byte code2[] = {B2(WASM_IF(WASM_ZERO, WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))),
                     WASM_GET_LOCAL(1))};

  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType storage[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j]};
      FunctionSig sig(1, 2, storage);

      if (i == j) {
        EXPECT_VERIFIES(&sig, code1);
        EXPECT_VERIFIES(&sig, code2);
      } else {
        EXPECT_FAILURE(&sig, code1);
        EXPECT_FAILURE(&sig, code2);
      }
    }
  }
}

TEST_F(AstDecoderTest, ExprBr_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      LocalType type = kLocalTypes[i];
      LocalType storage[] = {kAstI32, kAstI32, type};
      FunctionSig sig(1, 2, storage);

      byte code1[] = {B2(WASM_IF(WASM_ZERO, WASM_BRV(1, WASM_GET_LOCAL(which))),
                         WASM_GET_LOCAL(which ^ 1))};
      byte code2[] = {
          WASM_LOOP(WASM_IF(WASM_ZERO, WASM_BRV(2, WASM_GET_LOCAL(which))),
                    WASM_GET_LOCAL(which ^ 1))};

      if (type == kAstI32) {
        EXPECT_VERIFIES(&sig, code1);
        EXPECT_VERIFIES(&sig, code2);
      } else {
        EXPECT_FAILURE(&sig, code1);
        EXPECT_FAILURE(&sig, code2);
      }
    }
  }
}

TEST_F(AstDecoderTest, ExprBrIf_cond_type) {
  byte code[] = {B1(WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)))};
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType types[] = {kLocalTypes[i], kLocalTypes[j]};
      FunctionSig sig(0, 2, types);

      if (types[1] == kAstI32) {
        EXPECT_VERIFIES(&sig, code);
      } else {
        EXPECT_FAILURE(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, ExprBrIf_val_type) {
  byte code[] = {B2(WASM_BRV_IF(0, WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
                    WASM_GET_LOCAL(0))};
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType types[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j],
                           kAstI32};
      FunctionSig sig(1, 3, types);

      if (i == j) {
        EXPECT_VERIFIES(&sig, code);
      } else {
        EXPECT_FAILURE(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, ExprBrIf_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      LocalType type = kLocalTypes[i];
      LocalType storage[] = {kAstI32, kAstI32, type};
      FunctionSig sig(1, 2, storage);

      byte code1[] = {B2(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(which)),
                         WASM_GET_LOCAL(which ^ 1))};
      byte code2[] = {WASM_LOOP(WASM_BRV_IF_ZERO(1, WASM_GET_LOCAL(which)),
                                WASM_GET_LOCAL(which ^ 1))};

      if (type == kAstI32) {
        EXPECT_VERIFIES(&sig, code1);
        EXPECT_VERIFIES(&sig, code2);
      } else {
        EXPECT_FAILURE(&sig, code1);
        EXPECT_FAILURE(&sig, code2);
      }
    }
  }
}

TEST_F(AstDecoderTest, BrTable0) {
  static byte code[] = {kExprNop, kExprBrTable, 0, 0};
  EXPECT_FAILURE(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, BrTable0b) {
  static byte code[] = {kExprNop, kExprI32Const, 11, kExprBrTable, 0, 0};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, BrTable0c) {
  static byte code[] = {kExprNop, kExprI32Const, 11, kExprBrTable, 0, 1, 0, 0};
  EXPECT_FAILURE(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
}

TEST_F(AstDecoderTest, BrTable1a) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_I8(67), 0, BR_TARGET(0)))};
  EXPECT_VERIFIES(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, BrTable1b) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_ZERO, 0, BR_TARGET(0)))};
  EXPECT_VERIFIES(sigs.v_v(), code);
  EXPECT_FAILURE(sigs.i_i(), code);
  EXPECT_FAILURE(sigs.f_ff(), code);
  EXPECT_FAILURE(sigs.d_dd(), code);
}

TEST_F(AstDecoderTest, BrTable2a) {
  static byte code[] = {
      B1(WASM_BR_TABLE(WASM_I8(67), 1, BR_TARGET(0), BR_TARGET(0)))};
  EXPECT_VERIFIES(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, BrTable2b) {
  static byte code[] = {WASM_BLOCK(
      WASM_BLOCK(WASM_BR_TABLE(WASM_I8(67), 1, BR_TARGET(0), BR_TARGET(1))))};
  EXPECT_VERIFIES(sigs.v_v(), code);
}

TEST_F(AstDecoderTest, BrTable_off_end) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(0)))};
  for (size_t len = 1; len < sizeof(code); len++) {
    Verify(kError, sigs.i_i(), code, code + len);
  }
}

TEST_F(AstDecoderTest, BrTable_invalid_br1) {
  for (int depth = 0; depth < 4; depth++) {
    byte code[] = {B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(depth)))};
    if (depth == 0) {
      EXPECT_VERIFIES(sigs.v_i(), code);
    } else {
      EXPECT_FAILURE(sigs.v_i(), code);
    }
  }
}

TEST_F(AstDecoderTest, BrTable_invalid_br2) {
  for (int depth = 0; depth < 4; depth++) {
    byte code[] = {
        WASM_LOOP(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(depth)))};
    if (depth <= 1) {
      EXPECT_VERIFIES(sigs.v_i(), code);
    } else {
      EXPECT_FAILURE(sigs.v_i(), code);
    }
  }
}

TEST_F(AstDecoderTest, ExprBreakNesting1) {
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B1(WASM_BRV(0, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B1(WASM_BR(0)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B1(WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), B1(WASM_BR_IF(0, WASM_ZERO)));

  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BRV(0, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BR(0)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(),
                         WASM_LOOP(WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BR_IF(0, WASM_ZERO)));

  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BRV(1, WASM_ZERO)));
  EXPECT_VERIFIES_INLINE(sigs.v_v(), WASM_LOOP(WASM_BR(1)));
}

TEST_F(AstDecoderTest, Select) {
  EXPECT_VERIFIES_INLINE(
      sigs.i_i(), WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.f_ff(),
                         WASM_SELECT(WASM_F32(0.0), WASM_F32(0.0), WASM_ZERO));
  EXPECT_VERIFIES_INLINE(sigs.d_dd(),
                         WASM_SELECT(WASM_F64(0.0), WASM_F64(0.0), WASM_ZERO));
  EXPECT_VERIFIES_INLINE(
      sigs.l_l(), WASM_SELECT(WASM_I64V_1(0), WASM_I64V_1(0), WASM_ZERO));
}

TEST_F(AstDecoderTest, Select_fail1) {
  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      WASM_SELECT(WASM_F32(0.0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));
  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      WASM_SELECT(WASM_GET_LOCAL(0), WASM_F32(0.0), WASM_GET_LOCAL(0)));
  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_F32(0.0)));
}

TEST_F(AstDecoderTest, Select_fail2) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType type = kLocalTypes[i];
    if (type == kAstI32) continue;

    LocalType types[] = {type, kAstI32, type};
    FunctionSig sig(1, 2, types);

    EXPECT_VERIFIES_INLINE(
        &sig,
        WASM_SELECT(WASM_GET_LOCAL(1), WASM_GET_LOCAL(1), WASM_GET_LOCAL(0)));

    EXPECT_FAILURE_INLINE(
        &sig,
        WASM_SELECT(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));

    EXPECT_FAILURE_INLINE(
        &sig,
        WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(0)));

    EXPECT_FAILURE_INLINE(
        &sig,
        WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  }
}

TEST_F(AstDecoderTest, Select_TypeCheck) {
  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      WASM_SELECT(WASM_F32(9.9), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));

  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      WASM_SELECT(WASM_GET_LOCAL(0), WASM_F64(0.25), WASM_GET_LOCAL(0)));

  EXPECT_FAILURE_INLINE(
      sigs.i_i(),
      WASM_SELECT(WASM_F32(9.9), WASM_GET_LOCAL(0), WASM_I64V_1(0)));
}

TEST_F(AstDecoderTest, Throw) {
  FLAG_wasm_eh_prototype = true;
  EXPECT_VERIFIES_INLINE(sigs.v_i(), WASM_GET_LOCAL(0), kExprThrow);

  EXPECT_FAILURE_INLINE(sigs.i_d(), WASM_GET_LOCAL(0), kExprThrow,
                        WASM_I32V(0));
  EXPECT_FAILURE_INLINE(sigs.i_f(), WASM_GET_LOCAL(0), kExprThrow,
                        WASM_I32V(0));
  EXPECT_FAILURE_INLINE(sigs.l_l(), WASM_GET_LOCAL(0), kExprThrow,
                        WASM_I64V(0));
}

#define WASM_CATCH(local) kExprCatch, static_cast<byte>(local)
TEST_F(AstDecoderTest, TryCatch) {
  FLAG_wasm_eh_prototype = true;
  EXPECT_VERIFIES_INLINE(sigs.v_i(), kExprTryCatch, WASM_CATCH(0), kExprEnd);

  // Missing catch.
  EXPECT_FAILURE_INLINE(sigs.v_v(), kExprTryCatch, kExprEnd);

  // Missing end.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatch, WASM_CATCH(0));

  // Double catch.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatch, WASM_CATCH(0), WASM_CATCH(0),
                        kExprEnd);

  // Unexpected finally.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatch, WASM_CATCH(0), kExprFinally,
                        kExprEnd);
}

TEST_F(AstDecoderTest, TryFinally) {
  FLAG_wasm_eh_prototype = true;
  EXPECT_VERIFIES_INLINE(sigs.v_v(), kExprTryFinally, kExprFinally, kExprEnd);

  // Mising finally.
  EXPECT_FAILURE_INLINE(sigs.v_v(), kExprTryFinally, kExprEnd);

  // Missing end.
  EXPECT_FAILURE_INLINE(sigs.v_v(), kExprTryFinally, kExprFinally);

  // Double finally.
  EXPECT_FAILURE_INLINE(sigs.v_v(), kExprTryFinally, kExprFinally, kExprFinally,
                        kExprEnd);

  // Unexpected catch.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatch, WASM_CATCH(0), kExprFinally,
                        kExprEnd);
}

TEST_F(AstDecoderTest, TryCatchFinally) {
  FLAG_wasm_eh_prototype = true;
  EXPECT_VERIFIES_INLINE(sigs.v_i(), kExprTryCatchFinally, WASM_CATCH(0),
                         kExprFinally, kExprEnd);

  // Missing catch.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, kExprFinally,
                        kExprEnd);

  // Double catch.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, WASM_CATCH(0),
                        WASM_CATCH(0), kExprFinally, kExprEnd);

  // Missing finally.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, WASM_CATCH(0),
                        kExprEnd);

  // Double finally.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, WASM_CATCH(0),
                        kExprFinally, kExprFinally, kExprEnd);

  // Finally before catch.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, kExprFinally,
                        WASM_CATCH(0), kExprEnd);

  // Missing both try and finally.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, kExprEnd);

  // Missing end.
  EXPECT_FAILURE_INLINE(sigs.v_i(), kExprTryCatchFinally, WASM_CATCH(0),
                        kExprFinally);
}

class WasmOpcodeLengthTest : public TestWithZone {
 public:
  WasmOpcodeLengthTest() : TestWithZone() {}
};

#define EXPECT_LENGTH(expected, opcode)                           \
  {                                                               \
    static const byte code[] = {opcode, 0, 0, 0, 0, 0, 0, 0, 0};  \
    EXPECT_EQ(expected, OpcodeLength(code, code + sizeof(code))); \
  }

#define EXPECT_LENGTH_N(expected, ...)                            \
  {                                                               \
    static const byte code[] = {__VA_ARGS__};                     \
    EXPECT_EQ(expected, OpcodeLength(code, code + sizeof(code))); \
  }

TEST_F(WasmOpcodeLengthTest, Statements) {
  EXPECT_LENGTH(1, kExprNop);
  EXPECT_LENGTH(1, kExprBlock);
  EXPECT_LENGTH(1, kExprLoop);
  EXPECT_LENGTH(1, kExprIf);
  EXPECT_LENGTH(1, kExprElse);
  EXPECT_LENGTH(1, kExprEnd);
  EXPECT_LENGTH(1, kExprSelect);
  EXPECT_LENGTH(3, kExprBr);
  EXPECT_LENGTH(3, kExprBrIf);
  EXPECT_LENGTH(1, kExprThrow);
  EXPECT_LENGTH(1, kExprTryCatch);
  EXPECT_LENGTH(1, kExprTryFinally);
  EXPECT_LENGTH(1, kExprTryCatchFinally);
  EXPECT_LENGTH(2, kExprCatch);
  EXPECT_LENGTH(1, kExprFinally);
}

TEST_F(WasmOpcodeLengthTest, MiscExpressions) {
  EXPECT_LENGTH(2, kExprI8Const);
  EXPECT_LENGTH(5, kExprF32Const);
  EXPECT_LENGTH(9, kExprF64Const);
  EXPECT_LENGTH(2, kExprGetLocal);
  EXPECT_LENGTH(2, kExprSetLocal);
  EXPECT_LENGTH(2, kExprGetGlobal);
  EXPECT_LENGTH(2, kExprSetGlobal);
  EXPECT_LENGTH(3, kExprCallFunction);
  EXPECT_LENGTH(3, kExprCallImport);
  EXPECT_LENGTH(3, kExprCallIndirect);
  EXPECT_LENGTH(1, kExprIf);
  EXPECT_LENGTH(1, kExprBlock);
  EXPECT_LENGTH(1, kExprLoop);
  EXPECT_LENGTH(3, kExprBr);
  EXPECT_LENGTH(3, kExprBrIf);
}

TEST_F(WasmOpcodeLengthTest, I32Const) {
  EXPECT_LENGTH_N(2, kExprI32Const, U32V_1(1));
  EXPECT_LENGTH_N(3, kExprI32Const, U32V_2(999));
  EXPECT_LENGTH_N(4, kExprI32Const, U32V_3(9999));
  EXPECT_LENGTH_N(5, kExprI32Const, U32V_4(999999));
  EXPECT_LENGTH_N(6, kExprI32Const, U32V_5(99999999));
}

TEST_F(WasmOpcodeLengthTest, I64Const) {
  EXPECT_LENGTH_N(2, kExprI64Const, U32V_1(1));
  EXPECT_LENGTH_N(3, kExprI64Const, U32V_2(99));
  EXPECT_LENGTH_N(4, kExprI64Const, U32V_3(9999));
  EXPECT_LENGTH_N(5, kExprI64Const, U32V_4(99999));
  EXPECT_LENGTH_N(6, kExprI64Const, U32V_5(9999999));
  EXPECT_LENGTH_N(7, WASM_I64V_6(777777));
  EXPECT_LENGTH_N(8, WASM_I64V_7(7777777));
  EXPECT_LENGTH_N(9, WASM_I64V_8(77777777));
  EXPECT_LENGTH_N(10, WASM_I64V_9(777777777));
}

TEST_F(WasmOpcodeLengthTest, VariableLength) {
  EXPECT_LENGTH_N(2, kExprGetGlobal, U32V_1(1));
  EXPECT_LENGTH_N(3, kExprGetGlobal, U32V_2(33));
  EXPECT_LENGTH_N(4, kExprGetGlobal, U32V_3(44));
  EXPECT_LENGTH_N(5, kExprGetGlobal, U32V_4(66));
  EXPECT_LENGTH_N(6, kExprGetGlobal, U32V_5(77));
}

TEST_F(WasmOpcodeLengthTest, LoadsAndStores) {
  EXPECT_LENGTH(3, kExprI32LoadMem8S);
  EXPECT_LENGTH(3, kExprI32LoadMem8U);
  EXPECT_LENGTH(3, kExprI32LoadMem16S);
  EXPECT_LENGTH(3, kExprI32LoadMem16U);
  EXPECT_LENGTH(3, kExprI32LoadMem);
  EXPECT_LENGTH(3, kExprI64LoadMem8S);
  EXPECT_LENGTH(3, kExprI64LoadMem8U);
  EXPECT_LENGTH(3, kExprI64LoadMem16S);
  EXPECT_LENGTH(3, kExprI64LoadMem16U);
  EXPECT_LENGTH(3, kExprI64LoadMem32S);
  EXPECT_LENGTH(3, kExprI64LoadMem32U);
  EXPECT_LENGTH(3, kExprI64LoadMem);
  EXPECT_LENGTH(3, kExprF32LoadMem);
  EXPECT_LENGTH(3, kExprF64LoadMem);

  EXPECT_LENGTH(3, kExprI32StoreMem8);
  EXPECT_LENGTH(3, kExprI32StoreMem16);
  EXPECT_LENGTH(3, kExprI32StoreMem);
  EXPECT_LENGTH(3, kExprI64StoreMem8);
  EXPECT_LENGTH(3, kExprI64StoreMem16);
  EXPECT_LENGTH(3, kExprI64StoreMem32);
  EXPECT_LENGTH(3, kExprI64StoreMem);
  EXPECT_LENGTH(3, kExprF32StoreMem);
  EXPECT_LENGTH(3, kExprF64StoreMem);
}

TEST_F(WasmOpcodeLengthTest, MiscMemExpressions) {
  EXPECT_LENGTH(1, kExprMemorySize);
  EXPECT_LENGTH(1, kExprGrowMemory);
}

TEST_F(WasmOpcodeLengthTest, SimpleExpressions) {
  EXPECT_LENGTH(1, kExprI32Add);
  EXPECT_LENGTH(1, kExprI32Sub);
  EXPECT_LENGTH(1, kExprI32Mul);
  EXPECT_LENGTH(1, kExprI32DivS);
  EXPECT_LENGTH(1, kExprI32DivU);
  EXPECT_LENGTH(1, kExprI32RemS);
  EXPECT_LENGTH(1, kExprI32RemU);
  EXPECT_LENGTH(1, kExprI32And);
  EXPECT_LENGTH(1, kExprI32Ior);
  EXPECT_LENGTH(1, kExprI32Xor);
  EXPECT_LENGTH(1, kExprI32Shl);
  EXPECT_LENGTH(1, kExprI32ShrU);
  EXPECT_LENGTH(1, kExprI32ShrS);
  EXPECT_LENGTH(1, kExprI32Eq);
  EXPECT_LENGTH(1, kExprI32Ne);
  EXPECT_LENGTH(1, kExprI32LtS);
  EXPECT_LENGTH(1, kExprI32LeS);
  EXPECT_LENGTH(1, kExprI32LtU);
  EXPECT_LENGTH(1, kExprI32LeU);
  EXPECT_LENGTH(1, kExprI32GtS);
  EXPECT_LENGTH(1, kExprI32GeS);
  EXPECT_LENGTH(1, kExprI32GtU);
  EXPECT_LENGTH(1, kExprI32GeU);
  EXPECT_LENGTH(1, kExprI32Clz);
  EXPECT_LENGTH(1, kExprI32Ctz);
  EXPECT_LENGTH(1, kExprI32Popcnt);
  EXPECT_LENGTH(1, kExprI32Eqz);
  EXPECT_LENGTH(1, kExprI64Add);
  EXPECT_LENGTH(1, kExprI64Sub);
  EXPECT_LENGTH(1, kExprI64Mul);
  EXPECT_LENGTH(1, kExprI64DivS);
  EXPECT_LENGTH(1, kExprI64DivU);
  EXPECT_LENGTH(1, kExprI64RemS);
  EXPECT_LENGTH(1, kExprI64RemU);
  EXPECT_LENGTH(1, kExprI64And);
  EXPECT_LENGTH(1, kExprI64Ior);
  EXPECT_LENGTH(1, kExprI64Xor);
  EXPECT_LENGTH(1, kExprI64Shl);
  EXPECT_LENGTH(1, kExprI64ShrU);
  EXPECT_LENGTH(1, kExprI64ShrS);
  EXPECT_LENGTH(1, kExprI64Eq);
  EXPECT_LENGTH(1, kExprI64Ne);
  EXPECT_LENGTH(1, kExprI64LtS);
  EXPECT_LENGTH(1, kExprI64LeS);
  EXPECT_LENGTH(1, kExprI64LtU);
  EXPECT_LENGTH(1, kExprI64LeU);
  EXPECT_LENGTH(1, kExprI64GtS);
  EXPECT_LENGTH(1, kExprI64GeS);
  EXPECT_LENGTH(1, kExprI64GtU);
  EXPECT_LENGTH(1, kExprI64GeU);
  EXPECT_LENGTH(1, kExprI64Clz);
  EXPECT_LENGTH(1, kExprI64Ctz);
  EXPECT_LENGTH(1, kExprI64Popcnt);
  EXPECT_LENGTH(1, kExprF32Add);
  EXPECT_LENGTH(1, kExprF32Sub);
  EXPECT_LENGTH(1, kExprF32Mul);
  EXPECT_LENGTH(1, kExprF32Div);
  EXPECT_LENGTH(1, kExprF32Min);
  EXPECT_LENGTH(1, kExprF32Max);
  EXPECT_LENGTH(1, kExprF32Abs);
  EXPECT_LENGTH(1, kExprF32Neg);
  EXPECT_LENGTH(1, kExprF32CopySign);
  EXPECT_LENGTH(1, kExprF32Ceil);
  EXPECT_LENGTH(1, kExprF32Floor);
  EXPECT_LENGTH(1, kExprF32Trunc);
  EXPECT_LENGTH(1, kExprF32NearestInt);
  EXPECT_LENGTH(1, kExprF32Sqrt);
  EXPECT_LENGTH(1, kExprF32Eq);
  EXPECT_LENGTH(1, kExprF32Ne);
  EXPECT_LENGTH(1, kExprF32Lt);
  EXPECT_LENGTH(1, kExprF32Le);
  EXPECT_LENGTH(1, kExprF32Gt);
  EXPECT_LENGTH(1, kExprF32Ge);
  EXPECT_LENGTH(1, kExprF64Add);
  EXPECT_LENGTH(1, kExprF64Sub);
  EXPECT_LENGTH(1, kExprF64Mul);
  EXPECT_LENGTH(1, kExprF64Div);
  EXPECT_LENGTH(1, kExprF64Min);
  EXPECT_LENGTH(1, kExprF64Max);
  EXPECT_LENGTH(1, kExprF64Abs);
  EXPECT_LENGTH(1, kExprF64Neg);
  EXPECT_LENGTH(1, kExprF64CopySign);
  EXPECT_LENGTH(1, kExprF64Ceil);
  EXPECT_LENGTH(1, kExprF64Floor);
  EXPECT_LENGTH(1, kExprF64Trunc);
  EXPECT_LENGTH(1, kExprF64NearestInt);
  EXPECT_LENGTH(1, kExprF64Sqrt);
  EXPECT_LENGTH(1, kExprF64Eq);
  EXPECT_LENGTH(1, kExprF64Ne);
  EXPECT_LENGTH(1, kExprF64Lt);
  EXPECT_LENGTH(1, kExprF64Le);
  EXPECT_LENGTH(1, kExprF64Gt);
  EXPECT_LENGTH(1, kExprF64Ge);
  EXPECT_LENGTH(1, kExprI32SConvertF32);
  EXPECT_LENGTH(1, kExprI32SConvertF64);
  EXPECT_LENGTH(1, kExprI32UConvertF32);
  EXPECT_LENGTH(1, kExprI32UConvertF64);
  EXPECT_LENGTH(1, kExprI32ConvertI64);
  EXPECT_LENGTH(1, kExprI64SConvertF32);
  EXPECT_LENGTH(1, kExprI64SConvertF64);
  EXPECT_LENGTH(1, kExprI64UConvertF32);
  EXPECT_LENGTH(1, kExprI64UConvertF64);
  EXPECT_LENGTH(1, kExprI64SConvertI32);
  EXPECT_LENGTH(1, kExprI64UConvertI32);
  EXPECT_LENGTH(1, kExprF32SConvertI32);
  EXPECT_LENGTH(1, kExprF32UConvertI32);
  EXPECT_LENGTH(1, kExprF32SConvertI64);
  EXPECT_LENGTH(1, kExprF32UConvertI64);
  EXPECT_LENGTH(1, kExprF32ConvertF64);
  EXPECT_LENGTH(1, kExprF32ReinterpretI32);
  EXPECT_LENGTH(1, kExprF64SConvertI32);
  EXPECT_LENGTH(1, kExprF64UConvertI32);
  EXPECT_LENGTH(1, kExprF64SConvertI64);
  EXPECT_LENGTH(1, kExprF64UConvertI64);
  EXPECT_LENGTH(1, kExprF64ConvertF32);
  EXPECT_LENGTH(1, kExprF64ReinterpretI64);
  EXPECT_LENGTH(1, kExprI32ReinterpretF32);
  EXPECT_LENGTH(1, kExprI64ReinterpretF64);
}

class WasmOpcodeArityTest : public TestWithZone {
 public:
  WasmOpcodeArityTest() : TestWithZone() {}
};

#define EXPECT_ARITY(expected, ...)                              \
  {                                                              \
    static const byte code[] = {__VA_ARGS__};                    \
    EXPECT_EQ(expected, OpcodeArity(code, code + sizeof(code))); \
  }

TEST_F(WasmOpcodeArityTest, Control) {
  EXPECT_ARITY(0, kExprNop);

  EXPECT_ARITY(0, kExprBlock, 0);
  EXPECT_ARITY(0, kExprBlock, 1);
  EXPECT_ARITY(0, kExprBlock, 2);
  EXPECT_ARITY(0, kExprBlock, 5);
  EXPECT_ARITY(0, kExprBlock, 10);

  EXPECT_ARITY(0, kExprLoop, 0);
  EXPECT_ARITY(0, kExprLoop, 1);
  EXPECT_ARITY(0, kExprLoop, 2);
  EXPECT_ARITY(0, kExprLoop, 7);
  EXPECT_ARITY(0, kExprLoop, 11);

  EXPECT_ARITY(3, kExprSelect);

  EXPECT_ARITY(0, kExprBr);
  EXPECT_ARITY(1, kExprBrIf);
  EXPECT_ARITY(1, kExprBrTable);

  EXPECT_ARITY(1, kExprBr, ARITY_1);
  EXPECT_ARITY(2, kExprBrIf, ARITY_1);
  EXPECT_ARITY(2, kExprBrTable, ARITY_1);

  {
    EXPECT_ARITY(0, kExprReturn, ARITY_0);
    EXPECT_ARITY(1, kExprReturn, ARITY_1);
  }

  EXPECT_ARITY(0, kExprThrow);
  EXPECT_ARITY(0, kExprTryCatch);
  EXPECT_ARITY(0, kExprTryFinally);
  EXPECT_ARITY(0, kExprTryCatchFinally);
  EXPECT_ARITY(1, kExprCatch, 2);
  EXPECT_ARITY(0, kExprFinally);
}

TEST_F(WasmOpcodeArityTest, Misc) {
  EXPECT_ARITY(0, kExprI8Const);
  EXPECT_ARITY(0, kExprI32Const);
  EXPECT_ARITY(0, kExprF32Const);
  EXPECT_ARITY(0, kExprI64Const);
  EXPECT_ARITY(0, kExprF64Const);
  EXPECT_ARITY(0, kExprGetLocal);
  EXPECT_ARITY(1, kExprSetLocal);
  EXPECT_ARITY(0, kExprGetGlobal);
  EXPECT_ARITY(1, kExprSetGlobal);
}

TEST_F(WasmOpcodeArityTest, Calls) {
  {
    EXPECT_ARITY(2, kExprCallFunction, 2, 0);
    EXPECT_ARITY(2, kExprCallImport, 2, 0);
    EXPECT_ARITY(3, kExprCallIndirect, 2, 0);

    EXPECT_ARITY(1, kExprBr, ARITY_1);
    EXPECT_ARITY(2, kExprBrIf, ARITY_1);
    EXPECT_ARITY(2, kExprBrTable, ARITY_1);

    EXPECT_ARITY(0, kExprBr, ARITY_0);
    EXPECT_ARITY(1, kExprBrIf, ARITY_0);
    EXPECT_ARITY(1, kExprBrTable, ARITY_0);
  }

  {
    EXPECT_ARITY(1, kExprCallFunction, ARITY_1, 1);
    EXPECT_ARITY(1, kExprCallImport, ARITY_1, 1);
    EXPECT_ARITY(2, kExprCallIndirect, ARITY_1, 1);

    EXPECT_ARITY(1, kExprBr, ARITY_1);
    EXPECT_ARITY(2, kExprBrIf, ARITY_1);
    EXPECT_ARITY(2, kExprBrTable, ARITY_1);

    EXPECT_ARITY(0, kExprBr, ARITY_0);
    EXPECT_ARITY(1, kExprBrIf, ARITY_0);
    EXPECT_ARITY(1, kExprBrTable, ARITY_0);
  }
}

TEST_F(WasmOpcodeArityTest, LoadsAndStores) {
  EXPECT_ARITY(1, kExprI32LoadMem8S);
  EXPECT_ARITY(1, kExprI32LoadMem8U);
  EXPECT_ARITY(1, kExprI32LoadMem16S);
  EXPECT_ARITY(1, kExprI32LoadMem16U);
  EXPECT_ARITY(1, kExprI32LoadMem);

  EXPECT_ARITY(1, kExprI64LoadMem8S);
  EXPECT_ARITY(1, kExprI64LoadMem8U);
  EXPECT_ARITY(1, kExprI64LoadMem16S);
  EXPECT_ARITY(1, kExprI64LoadMem16U);
  EXPECT_ARITY(1, kExprI64LoadMem32S);
  EXPECT_ARITY(1, kExprI64LoadMem32U);
  EXPECT_ARITY(1, kExprI64LoadMem);
  EXPECT_ARITY(1, kExprF32LoadMem);
  EXPECT_ARITY(1, kExprF64LoadMem);

  EXPECT_ARITY(2, kExprI32StoreMem8);
  EXPECT_ARITY(2, kExprI32StoreMem16);
  EXPECT_ARITY(2, kExprI32StoreMem);
  EXPECT_ARITY(2, kExprI64StoreMem8);
  EXPECT_ARITY(2, kExprI64StoreMem16);
  EXPECT_ARITY(2, kExprI64StoreMem32);
  EXPECT_ARITY(2, kExprI64StoreMem);
  EXPECT_ARITY(2, kExprF32StoreMem);
  EXPECT_ARITY(2, kExprF64StoreMem);
}

TEST_F(WasmOpcodeArityTest, MiscMemExpressions) {
  EXPECT_ARITY(0, kExprMemorySize);
  EXPECT_ARITY(1, kExprGrowMemory);
}

TEST_F(WasmOpcodeArityTest, SimpleExpressions) {
  EXPECT_ARITY(2, kExprI32Add);
  EXPECT_ARITY(2, kExprI32Sub);
  EXPECT_ARITY(2, kExprI32Mul);
  EXPECT_ARITY(2, kExprI32DivS);
  EXPECT_ARITY(2, kExprI32DivU);
  EXPECT_ARITY(2, kExprI32RemS);
  EXPECT_ARITY(2, kExprI32RemU);
  EXPECT_ARITY(2, kExprI32And);
  EXPECT_ARITY(2, kExprI32Ior);
  EXPECT_ARITY(2, kExprI32Xor);
  EXPECT_ARITY(2, kExprI32Shl);
  EXPECT_ARITY(2, kExprI32ShrU);
  EXPECT_ARITY(2, kExprI32ShrS);
  EXPECT_ARITY(2, kExprI32Eq);
  EXPECT_ARITY(2, kExprI32Ne);
  EXPECT_ARITY(2, kExprI32LtS);
  EXPECT_ARITY(2, kExprI32LeS);
  EXPECT_ARITY(2, kExprI32LtU);
  EXPECT_ARITY(2, kExprI32LeU);
  EXPECT_ARITY(2, kExprI32GtS);
  EXPECT_ARITY(2, kExprI32GeS);
  EXPECT_ARITY(2, kExprI32GtU);
  EXPECT_ARITY(2, kExprI32GeU);
  EXPECT_ARITY(1, kExprI32Clz);
  EXPECT_ARITY(1, kExprI32Ctz);
  EXPECT_ARITY(1, kExprI32Popcnt);
  EXPECT_ARITY(1, kExprI32Eqz);
  EXPECT_ARITY(2, kExprI64Add);
  EXPECT_ARITY(2, kExprI64Sub);
  EXPECT_ARITY(2, kExprI64Mul);
  EXPECT_ARITY(2, kExprI64DivS);
  EXPECT_ARITY(2, kExprI64DivU);
  EXPECT_ARITY(2, kExprI64RemS);
  EXPECT_ARITY(2, kExprI64RemU);
  EXPECT_ARITY(2, kExprI64And);
  EXPECT_ARITY(2, kExprI64Ior);
  EXPECT_ARITY(2, kExprI64Xor);
  EXPECT_ARITY(2, kExprI64Shl);
  EXPECT_ARITY(2, kExprI64ShrU);
  EXPECT_ARITY(2, kExprI64ShrS);
  EXPECT_ARITY(2, kExprI64Eq);
  EXPECT_ARITY(2, kExprI64Ne);
  EXPECT_ARITY(2, kExprI64LtS);
  EXPECT_ARITY(2, kExprI64LeS);
  EXPECT_ARITY(2, kExprI64LtU);
  EXPECT_ARITY(2, kExprI64LeU);
  EXPECT_ARITY(2, kExprI64GtS);
  EXPECT_ARITY(2, kExprI64GeS);
  EXPECT_ARITY(2, kExprI64GtU);
  EXPECT_ARITY(2, kExprI64GeU);
  EXPECT_ARITY(1, kExprI64Clz);
  EXPECT_ARITY(1, kExprI64Ctz);
  EXPECT_ARITY(1, kExprI64Popcnt);
  EXPECT_ARITY(2, kExprF32Add);
  EXPECT_ARITY(2, kExprF32Sub);
  EXPECT_ARITY(2, kExprF32Mul);
  EXPECT_ARITY(2, kExprF32Div);
  EXPECT_ARITY(2, kExprF32Min);
  EXPECT_ARITY(2, kExprF32Max);
  EXPECT_ARITY(1, kExprF32Abs);
  EXPECT_ARITY(1, kExprF32Neg);
  EXPECT_ARITY(2, kExprF32CopySign);
  EXPECT_ARITY(1, kExprF32Ceil);
  EXPECT_ARITY(1, kExprF32Floor);
  EXPECT_ARITY(1, kExprF32Trunc);
  EXPECT_ARITY(1, kExprF32NearestInt);
  EXPECT_ARITY(1, kExprF32Sqrt);
  EXPECT_ARITY(2, kExprF32Eq);
  EXPECT_ARITY(2, kExprF32Ne);
  EXPECT_ARITY(2, kExprF32Lt);
  EXPECT_ARITY(2, kExprF32Le);
  EXPECT_ARITY(2, kExprF32Gt);
  EXPECT_ARITY(2, kExprF32Ge);
  EXPECT_ARITY(2, kExprF64Add);
  EXPECT_ARITY(2, kExprF64Sub);
  EXPECT_ARITY(2, kExprF64Mul);
  EXPECT_ARITY(2, kExprF64Div);
  EXPECT_ARITY(2, kExprF64Min);
  EXPECT_ARITY(2, kExprF64Max);
  EXPECT_ARITY(1, kExprF64Abs);
  EXPECT_ARITY(1, kExprF64Neg);
  EXPECT_ARITY(2, kExprF64CopySign);
  EXPECT_ARITY(1, kExprF64Ceil);
  EXPECT_ARITY(1, kExprF64Floor);
  EXPECT_ARITY(1, kExprF64Trunc);
  EXPECT_ARITY(1, kExprF64NearestInt);
  EXPECT_ARITY(1, kExprF64Sqrt);
  EXPECT_ARITY(2, kExprF64Eq);
  EXPECT_ARITY(2, kExprF64Ne);
  EXPECT_ARITY(2, kExprF64Lt);
  EXPECT_ARITY(2, kExprF64Le);
  EXPECT_ARITY(2, kExprF64Gt);
  EXPECT_ARITY(2, kExprF64Ge);
  EXPECT_ARITY(1, kExprI32SConvertF32);
  EXPECT_ARITY(1, kExprI32SConvertF64);
  EXPECT_ARITY(1, kExprI32UConvertF32);
  EXPECT_ARITY(1, kExprI32UConvertF64);
  EXPECT_ARITY(1, kExprI32ConvertI64);
  EXPECT_ARITY(1, kExprI64SConvertF32);
  EXPECT_ARITY(1, kExprI64SConvertF64);
  EXPECT_ARITY(1, kExprI64UConvertF32);
  EXPECT_ARITY(1, kExprI64UConvertF64);
  EXPECT_ARITY(1, kExprI64SConvertI32);
  EXPECT_ARITY(1, kExprI64UConvertI32);
  EXPECT_ARITY(1, kExprF32SConvertI32);
  EXPECT_ARITY(1, kExprF32UConvertI32);
  EXPECT_ARITY(1, kExprF32SConvertI64);
  EXPECT_ARITY(1, kExprF32UConvertI64);
  EXPECT_ARITY(1, kExprF32ConvertF64);
  EXPECT_ARITY(1, kExprF32ReinterpretI32);
  EXPECT_ARITY(1, kExprF64SConvertI32);
  EXPECT_ARITY(1, kExprF64UConvertI32);
  EXPECT_ARITY(1, kExprF64SConvertI64);
  EXPECT_ARITY(1, kExprF64UConvertI64);
  EXPECT_ARITY(1, kExprF64ConvertF32);
  EXPECT_ARITY(1, kExprF64ReinterpretI64);
  EXPECT_ARITY(1, kExprI32ReinterpretF32);
  EXPECT_ARITY(1, kExprI64ReinterpretF64);
}

typedef ZoneVector<LocalType> LocalTypeMap;

class LocalDeclDecoderTest : public TestWithZone {
 public:
  base::AccountingAllocator allocator;

  size_t ExpectRun(LocalTypeMap map, size_t pos, LocalType expected,
                   size_t count) {
    for (size_t i = 0; i < count; i++) {
      EXPECT_EQ(expected, map[pos++]);
    }
    return pos;
  }

  LocalTypeMap Expand(AstLocalDecls& decls) {
    ZoneVector<LocalType> map(zone());
    for (auto p : decls.local_types) {
      map.insert(map.end(), p.second, p.first);
    }
    return map;
  }
};

TEST_F(LocalDeclDecoderTest, EmptyLocals) {
  AstLocalDecls decls(zone());
  bool result = DecodeLocalDecls(decls, nullptr, nullptr);
  EXPECT_FALSE(result);
}

TEST_F(LocalDeclDecoderTest, NoLocals) {
  static const byte data[] = {0};
  AstLocalDecls decls(zone());
  bool result = DecodeLocalDecls(decls, data, data + sizeof(data));
  EXPECT_TRUE(result);
  EXPECT_EQ(0, decls.total_local_count);
}

TEST_F(LocalDeclDecoderTest, OneLocal) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType type = kLocalTypes[i];
    const byte data[] = {
        1, 1, static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(type))};
    AstLocalDecls decls(zone());
    bool result = DecodeLocalDecls(decls, data, data + sizeof(data));
    EXPECT_TRUE(result);
    EXPECT_EQ(1, decls.total_local_count);

    LocalTypeMap map = Expand(decls);
    EXPECT_EQ(1, map.size());
    EXPECT_EQ(type, map[0]);
  }
}

TEST_F(LocalDeclDecoderTest, FiveLocals) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType type = kLocalTypes[i];
    const byte data[] = {
        1, 5, static_cast<byte>(WasmOpcodes::LocalTypeCodeFor(type))};
    AstLocalDecls decls(zone());
    bool result = DecodeLocalDecls(decls, data, data + sizeof(data));
    EXPECT_TRUE(result);
    EXPECT_EQ(sizeof(data), decls.decls_encoded_size);
    EXPECT_EQ(5, decls.total_local_count);

    LocalTypeMap map = Expand(decls);
    EXPECT_EQ(5, map.size());
    ExpectRun(map, 0, type, 5);
  }
}

TEST_F(LocalDeclDecoderTest, MixedLocals) {
  for (byte a = 0; a < 3; a++) {
    for (byte b = 0; b < 3; b++) {
      for (byte c = 0; c < 3; c++) {
        for (byte d = 0; d < 3; d++) {
          const byte data[] = {4, a,         kLocalI32, b,        kLocalI64,
                               c, kLocalF32, d,         kLocalF64};
          AstLocalDecls decls(zone());
          bool result = DecodeLocalDecls(decls, data, data + sizeof(data));
          EXPECT_TRUE(result);
          EXPECT_EQ(sizeof(data), decls.decls_encoded_size);
          EXPECT_EQ(a + b + c + d, decls.total_local_count);

          LocalTypeMap map = Expand(decls);
          EXPECT_EQ(a + b + c + d, map.size());

          size_t pos = 0;
          pos = ExpectRun(map, pos, kAstI32, a);
          pos = ExpectRun(map, pos, kAstI64, b);
          pos = ExpectRun(map, pos, kAstF32, c);
          pos = ExpectRun(map, pos, kAstF64, d);
        }
      }
    }
  }
}

TEST_F(LocalDeclDecoderTest, UseEncoder) {
  const byte* data = nullptr;
  const byte* end = nullptr;
  LocalDeclEncoder local_decls(zone());

  local_decls.AddLocals(5, kAstF32);
  local_decls.AddLocals(1337, kAstI32);
  local_decls.AddLocals(212, kAstI64);
  local_decls.Prepend(zone(), &data, &end);

  AstLocalDecls decls(zone());
  bool result = DecodeLocalDecls(decls, data, end);
  EXPECT_TRUE(result);
  EXPECT_EQ(5 + 1337 + 212, decls.total_local_count);

  LocalTypeMap map = Expand(decls);
  size_t pos = 0;
  pos = ExpectRun(map, pos, kAstF32, 5);
  pos = ExpectRun(map, pos, kAstI32, 1337);
  pos = ExpectRun(map, pos, kAstI64, 212);
}

class BytecodeIteratorTest : public TestWithZone {};

TEST_F(BytecodeIteratorTest, SimpleForeach) {
  byte code[] = {WASM_IF_ELSE(WASM_ZERO, WASM_ZERO, WASM_ZERO)};
  BytecodeIterator iter(code, code + sizeof(code));
  WasmOpcode expected[] = {kExprI8Const, kExprIf,      kExprI8Const,
                           kExprElse,    kExprI8Const, kExprEnd};
  size_t pos = 0;
  for (WasmOpcode opcode : iter) {
    if (pos >= arraysize(expected)) {
      EXPECT_TRUE(false);
      break;
    }
    EXPECT_EQ(expected[pos++], opcode);
  }
  EXPECT_EQ(arraysize(expected), pos);
}

TEST_F(BytecodeIteratorTest, ForeachTwice) {
  byte code[] = {WASM_IF_ELSE(WASM_ZERO, WASM_ZERO, WASM_ZERO)};
  BytecodeIterator iter(code, code + sizeof(code));
  int count = 0;

  count = 0;
  for (WasmOpcode opcode : iter) {
    USE(opcode);
    count++;
  }
  EXPECT_EQ(6, count);

  count = 0;
  for (WasmOpcode opcode : iter) {
    USE(opcode);
    count++;
  }
  EXPECT_EQ(6, count);
}

TEST_F(BytecodeIteratorTest, WithAstDecls) {
  byte code[] = {1, 1, kLocalI32, WASM_I8(9), WASM_I8(11)};
  AstLocalDecls decls(zone());
  BytecodeIterator iter(code, code + sizeof(code), &decls);

  EXPECT_EQ(3, decls.decls_encoded_size);
  EXPECT_EQ(3, iter.pc_offset());
  EXPECT_TRUE(iter.has_next());
  EXPECT_EQ(kExprI8Const, iter.current());
  iter.next();
  EXPECT_TRUE(iter.has_next());
  EXPECT_EQ(kExprI8Const, iter.current());
  iter.next();
  EXPECT_FALSE(iter.has_next());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

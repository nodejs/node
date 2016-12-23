// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/v8.h"

#include "test/common/wasm/test-signatures.h"

#include "src/objects.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

#define B1(a) WASM_BLOCK(a)
#define B2(a, b) WASM_BLOCK(a, b)
#define B3(a, b, c) WASM_BLOCK(a, b, c)

#define WASM_IF_OP kExprIf, kLocalVoid
#define WASM_LOOP_OP kExprLoop, kLocalVoid

static const byte kCodeGetLocal0[] = {kExprGetLocal, 0};
static const byte kCodeGetLocal1[] = {kExprGetLocal, 1};
static const byte kCodeSetLocal0[] = {WASM_SET_LOCAL(0, WASM_ZERO)};
static const byte kCodeTeeLocal0[] = {WASM_TEE_LOCAL(0, WASM_ZERO)};

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
  val, WASM_ZERO, kExprBrIf, static_cast<byte>(depth)

#define EXPECT_VERIFIES_C(sig, x) \
  Verify(kSuccess, sigs.sig(), x, x + arraysize(x))

#define EXPECT_FAILURE_C(sig, x) Verify(kError, sigs.sig(), x, x + arraysize(x))

#define EXPECT_VERIFIES_SC(sig, x) Verify(kSuccess, sig, x, x + arraysize(x))

#define EXPECT_FAILURE_SC(sig, x) Verify(kError, sig, x, x + arraysize(x))

#define EXPECT_VERIFIES_S(env, ...)                      \
  do {                                                   \
    static byte code[] = {__VA_ARGS__};                  \
    Verify(kSuccess, env, code, code + arraysize(code)); \
  } while (false)

#define EXPECT_FAILURE_S(env, ...)                     \
  do {                                                 \
    static byte code[] = {__VA_ARGS__};                \
    Verify(kError, env, code, code + arraysize(code)); \
  } while (false)

#define EXPECT_VERIFIES(sig, ...)                            \
  do {                                                       \
    static const byte code[] = {__VA_ARGS__};                \
    Verify(kSuccess, sigs.sig(), code, code + sizeof(code)); \
  } while (false)

#define EXPECT_FAILURE(sig, ...)                           \
  do {                                                     \
    static const byte code[] = {__VA_ARGS__};              \
    Verify(kError, sigs.sig(), code, code + sizeof(code)); \
  } while (false)

static bool old_eh_flag;

class AstDecoderTest : public TestWithZone {
 public:
  typedef std::pair<uint32_t, LocalType> LocalsDecl;

  AstDecoderTest() : module(nullptr), local_decls(zone()) {}

  static void SetUpTestCase() { old_eh_flag = FLAG_wasm_eh_prototype; }

  static void TearDownTestCase() {
    // Reset the wasm_eh_prototype flag
    FLAG_wasm_eh_prototype = old_eh_flag;
  }

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
      EXPECT_TRUE(false) << str.str().c_str();
    }
  }

  void TestBinop(WasmOpcode opcode, FunctionSig* success) {
    // op(local[0], local[1])
    byte code[] = {WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
    EXPECT_VERIFIES_SC(success, code);

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
            EXPECT_FAILURE_SC(&sig, code);
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
      EXPECT_VERIFIES_SC(&sig, code);
    }

    // Try all combinations of return and parameter types.
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
        LocalType types[] = {kLocalTypes[i], kLocalTypes[j]};
        if (types[0] != ret_type || types[1] != param_type) {
          // Test signature mismatch.
          FunctionSig sig(1, 1, types);
          EXPECT_FAILURE_SC(&sig, code);
        }
      }
    }
  }
};

TEST_F(AstDecoderTest, Int8Const) {
  byte code[] = {kExprI8Const, 0};
  for (int i = -128; i < 128; i++) {
    code[1] = static_cast<byte>(i);
    EXPECT_VERIFIES_C(i_i, code);
  }
}

TEST_F(AstDecoderTest, EmptyFunction) {
  byte code[] = {0};
  Verify(kSuccess, sigs.v_v(), code, code);
  Verify(kError, sigs.i_i(), code, code);
}

TEST_F(AstDecoderTest, IncompleteIf1) {
  byte code[] = {kExprIf};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, Int8Const_fallthru) {
  EXPECT_VERIFIES(i_i, WASM_I32V_1(0));
}

TEST_F(AstDecoderTest, Int8Const_fallthru2) {
  EXPECT_FAILURE(i_i, WASM_I32V_1(0), WASM_I32V_1(1));
}

TEST_F(AstDecoderTest, Int32Const) {
  const int kInc = 4498211;
  for (int32_t i = kMinInt; i < kMaxInt - kInc; i = i + kInc) {
    // TODO(binji): expand test for other sized int32s; 1 through 5 bytes.
    byte code[] = {WASM_I32V(i)};
    EXPECT_VERIFIES_C(i_i, code);
  }
}

TEST_F(AstDecoderTest, Int64Const) {
  const int kInc = 4498211;
  for (int32_t i = kMinInt; i < kMaxInt - kInc; i = i + kInc) {
    byte code[] = {WASM_I64V((static_cast<int64_t>(i) << 32) | i)};
    EXPECT_VERIFIES_C(l_l, code);
  }
}

TEST_F(AstDecoderTest, Float32Const) {
  byte code[] = {kExprF32Const, 0, 0, 0, 0};
  float* ptr = reinterpret_cast<float*>(code + 1);
  for (int i = 0; i < 30; i++) {
    WriteLittleEndianValue<float>(ptr, i * -7.75f);
    EXPECT_VERIFIES_C(f_ff, code);
  }
}

TEST_F(AstDecoderTest, Float64Const) {
  byte code[] = {kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0};
  double* ptr = reinterpret_cast<double*>(code + 1);
  for (int i = 0; i < 30; i++) {
    WriteLittleEndianValue<double>(ptr, i * 33.45);
    EXPECT_VERIFIES_C(d_dd, code);
  }
}

TEST_F(AstDecoderTest, Int32Const_off_end) {
  byte code[] = {kExprI32Const, 0xaa, 0xbb, 0xcc, 0x44};

  for (int size = 1; size <= 4; size++) {
    Verify(kError, sigs.i_i(), code, code + size);
  }
}

TEST_F(AstDecoderTest, GetLocal0_param) {
  EXPECT_VERIFIES_C(i_i, kCodeGetLocal0);
}

TEST_F(AstDecoderTest, GetLocal0_local) {
  AddLocals(kAstI32, 1);
  EXPECT_VERIFIES_C(i_v, kCodeGetLocal0);
}

TEST_F(AstDecoderTest, TooManyLocals) {
  AddLocals(kAstI32, 4034986500);
  EXPECT_FAILURE_C(i_v, kCodeGetLocal0);
}

TEST_F(AstDecoderTest, GetLocal0_param_n) {
  FunctionSig* array[] = {sigs.i_i(), sigs.i_ii(), sigs.i_iii()};

  for (size_t i = 0; i < arraysize(array); i++) {
    EXPECT_VERIFIES_SC(array[i], kCodeGetLocal0);
  }
}

TEST_F(AstDecoderTest, GetLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kAstI32, 1);
    for (byte j = 0; j < i; j++) {
      byte code[] = {kExprGetLocal, j};
      EXPECT_VERIFIES_C(i_v, code);
    }
  }
}

TEST_F(AstDecoderTest, GetLocal0_fail_no_params) {
  EXPECT_FAILURE_C(i_v, kCodeGetLocal0);
}

TEST_F(AstDecoderTest, GetLocal1_fail_no_locals) {
  EXPECT_FAILURE_C(i_i, kCodeGetLocal1);
}

TEST_F(AstDecoderTest, GetLocal_off_end) {
  static const byte code[] = {kExprGetLocal};
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, NumLocalBelowLimit) {
  AddLocals(kAstI32, kMaxNumWasmLocals - 1);
  EXPECT_VERIFIES(v_v, WASM_NOP);
}

TEST_F(AstDecoderTest, NumLocalAtLimit) {
  AddLocals(kAstI32, kMaxNumWasmLocals);
  EXPECT_VERIFIES(v_v, WASM_NOP);
}

TEST_F(AstDecoderTest, NumLocalAboveLimit) {
  AddLocals(kAstI32, kMaxNumWasmLocals + 1);
  EXPECT_FAILURE(v_v, WASM_NOP);
}

TEST_F(AstDecoderTest, GetLocal_varint) {
  const int kMaxLocals = kMaxNumWasmLocals;
  AddLocals(kAstI32, kMaxLocals);

  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_1(66));
  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_2(7777));
  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_3(888888));
  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_4(3999999));

  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_5(kMaxLocals - 1));

  EXPECT_FAILURE(i_i, kExprGetLocal, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);

  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_4(kMaxLocals - 1));
  EXPECT_VERIFIES(i_i, kExprGetLocal, U32V_4(kMaxLocals));
  EXPECT_FAILURE(i_i, kExprGetLocal, U32V_4(kMaxLocals + 1));

  EXPECT_FAILURE(i_v, kExprGetLocal, U32V_4(kMaxLocals));
  EXPECT_FAILURE(i_v, kExprGetLocal, U32V_4(kMaxLocals + 1));
}

TEST_F(AstDecoderTest, Binops_off_end) {
  byte code1[] = {0};  // [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code1[0] = kInt32BinopOpcodes[i];
    EXPECT_FAILURE_C(i_i, code1);
  }

  byte code3[] = {kExprGetLocal, 0, 0};  // [expr] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code3[2] = kInt32BinopOpcodes[i];
    EXPECT_FAILURE_C(i_i, code3);
  }

  byte code4[] = {kExprGetLocal, 0, 0, 0};  // [expr] [opcode] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code4[2] = kInt32BinopOpcodes[i];
    code4[3] = kInt32BinopOpcodes[i];
    EXPECT_FAILURE_C(i_i, code4);
  }
}

TEST_F(AstDecoderTest, BinopsAcrossBlock1) {
  static const byte code[] = {WASM_ZERO, kExprBlock, WASM_ZERO, kExprI32Add,
                              kExprEnd};
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, BinopsAcrossBlock2) {
  static const byte code[] = {WASM_ZERO, WASM_ZERO, kExprBlock, kExprI32Add,
                              kExprEnd};
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, BinopsAcrossBlock3) {
  static const byte code[] = {WASM_ZERO, WASM_ZERO,   kExprIf, kExprI32Add,
                              kExprElse, kExprI32Add, kExprEnd};
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, Nop) {
  static const byte code[] = {kExprNop};
  EXPECT_VERIFIES_C(v_v, code);
}

TEST_F(AstDecoderTest, SetLocal0_void) {
  EXPECT_FAILURE(i_i, WASM_SET_LOCAL(0, WASM_ZERO));
}

TEST_F(AstDecoderTest, SetLocal0_param) {
  EXPECT_FAILURE_C(i_i, kCodeSetLocal0);
  EXPECT_FAILURE_C(f_ff, kCodeSetLocal0);
  EXPECT_FAILURE_C(d_dd, kCodeSetLocal0);
}

TEST_F(AstDecoderTest, TeeLocal0_param) {
  EXPECT_VERIFIES_C(i_i, kCodeTeeLocal0);
  EXPECT_FAILURE_C(f_ff, kCodeTeeLocal0);
  EXPECT_FAILURE_C(d_dd, kCodeTeeLocal0);
}

TEST_F(AstDecoderTest, SetLocal0_local) {
  EXPECT_FAILURE_C(i_v, kCodeSetLocal0);
  EXPECT_FAILURE_C(v_v, kCodeSetLocal0);
  AddLocals(kAstI32, 1);
  EXPECT_FAILURE_C(i_v, kCodeSetLocal0);
  EXPECT_VERIFIES_C(v_v, kCodeSetLocal0);
}

TEST_F(AstDecoderTest, TeeLocal0_local) {
  EXPECT_FAILURE_C(i_v, kCodeTeeLocal0);
  AddLocals(kAstI32, 1);
  EXPECT_VERIFIES_C(i_v, kCodeTeeLocal0);
}

TEST_F(AstDecoderTest, TeeLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kAstI32, 1);
    for (byte j = 0; j < i; j++) {
      EXPECT_FAILURE(v_v, WASM_TEE_LOCAL(j, WASM_I8(i)));
      EXPECT_VERIFIES(i_i, WASM_TEE_LOCAL(j, WASM_I8(i)));
    }
  }
}

TEST_F(AstDecoderTest, BlockN) {
  const int kMaxSize = 200;
  byte buffer[kMaxSize + 3];

  for (int i = 0; i <= kMaxSize; i++) {
    memset(buffer, kExprNop, sizeof(buffer));
    buffer[0] = kExprBlock;
    buffer[1] = kLocalVoid;
    buffer[i + 2] = kExprEnd;
    Verify(kSuccess, sigs.v_i(), buffer, buffer + i + 3);
  }
}

#define WASM_EMPTY_BLOCK kExprBlock, kLocalVoid, kExprEnd

TEST_F(AstDecoderTest, Block0) {
  static const byte code[] = {WASM_EMPTY_BLOCK};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, Block0_fallthru1) {
  static const byte code[] = {WASM_BLOCK(WASM_EMPTY_BLOCK)};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, Block0Block0) {
  static const byte code[] = {WASM_EMPTY_BLOCK, WASM_EMPTY_BLOCK};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, Block0_end) {
  EXPECT_VERIFIES(v_v, WASM_EMPTY_BLOCK, kExprEnd);
}

TEST_F(AstDecoderTest, Block0_end_end) {
  EXPECT_FAILURE(v_v, WASM_EMPTY_BLOCK, kExprEnd, kExprEnd);
}

TEST_F(AstDecoderTest, Block1) {
  byte code[] = {WASM_BLOCK_I(WASM_GET_LOCAL(0))};
  EXPECT_VERIFIES_C(i_i, code);
  EXPECT_FAILURE_C(v_i, code);
  EXPECT_FAILURE_C(d_dd, code);
  EXPECT_FAILURE_C(i_f, code);
  EXPECT_FAILURE_C(i_d, code);
}

TEST_F(AstDecoderTest, Block1_i) {
  byte code[] = {WASM_BLOCK_I(WASM_ZERO)};
  EXPECT_VERIFIES_C(i_i, code);
  EXPECT_FAILURE_C(f_ff, code);
  EXPECT_FAILURE_C(d_dd, code);
  EXPECT_FAILURE_C(l_ll, code);
}

TEST_F(AstDecoderTest, Block1_f) {
  byte code[] = {WASM_BLOCK_F(WASM_F32(0))};
  EXPECT_FAILURE_C(i_i, code);
  EXPECT_VERIFIES_C(f_ff, code);
  EXPECT_FAILURE_C(d_dd, code);
  EXPECT_FAILURE_C(l_ll, code);
}

TEST_F(AstDecoderTest, Block1_continue) {
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_BR(0)));
}

TEST_F(AstDecoderTest, Block1_br) {
  EXPECT_VERIFIES(v_v, B1(WASM_BR(0)));
  EXPECT_VERIFIES(v_v, B1(WASM_BR(1)));
  EXPECT_FAILURE(v_v, B1(WASM_BR(2)));
}

TEST_F(AstDecoderTest, Block2_br) {
  EXPECT_VERIFIES(v_v, B2(WASM_NOP, WASM_BR(0)));
  EXPECT_VERIFIES(v_v, B2(WASM_BR(0), WASM_NOP));
  EXPECT_VERIFIES(v_v, B2(WASM_BR(0), WASM_BR(0)));
}

TEST_F(AstDecoderTest, Block2) {
  EXPECT_FAILURE(i_i, WASM_BLOCK(WASM_NOP, WASM_NOP));
  EXPECT_FAILURE(i_i, WASM_BLOCK_I(WASM_NOP, WASM_NOP));
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_NOP, WASM_ZERO));
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_ZERO, WASM_NOP));
  EXPECT_FAILURE(i_i, WASM_BLOCK_I(WASM_ZERO, WASM_ZERO));
}

TEST_F(AstDecoderTest, Block2b) {
  byte code[] = {WASM_BLOCK_I(WASM_SET_LOCAL(0, WASM_ZERO), WASM_ZERO)};
  EXPECT_VERIFIES_C(i_i, code);
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(f_ff, code);
}

TEST_F(AstDecoderTest, Block2_fallthru) {
  EXPECT_VERIFIES(
      i_i, B2(WASM_SET_LOCAL(0, WASM_ZERO), WASM_SET_LOCAL(0, WASM_ZERO)),
      WASM_I8(23));
}

TEST_F(AstDecoderTest, Block3) {
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_SET_LOCAL(0, WASM_ZERO),
                                    WASM_SET_LOCAL(0, WASM_ZERO), WASM_I8(11)));
}

TEST_F(AstDecoderTest, Block5) {
  EXPECT_FAILURE(v_i, WASM_BLOCK(WASM_ZERO));

  EXPECT_FAILURE(v_i, WASM_BLOCK(WASM_ZERO, WASM_ZERO));

  EXPECT_FAILURE(v_i, WASM_BLOCK(WASM_ZERO, WASM_ZERO, WASM_ZERO));

  EXPECT_FAILURE(v_i, WASM_BLOCK(WASM_ZERO, WASM_ZERO, WASM_ZERO, WASM_ZERO));

  EXPECT_FAILURE(
      v_i, WASM_BLOCK(WASM_ZERO, WASM_ZERO, WASM_ZERO, WASM_ZERO, WASM_ZERO));
}

TEST_F(AstDecoderTest, BlockType) {
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(l_l, WASM_BLOCK_L(WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(f_f, WASM_BLOCK_F(WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(d_d, WASM_BLOCK_D(WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, BlockType_fail) {
  EXPECT_FAILURE(i_i, WASM_BLOCK_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(i_i, WASM_BLOCK_F(WASM_F32(0.0)));
  EXPECT_FAILURE(i_i, WASM_BLOCK_D(WASM_F64(1.1)));

  EXPECT_FAILURE(l_l, WASM_BLOCK_I(WASM_ZERO));
  EXPECT_FAILURE(l_l, WASM_BLOCK_F(WASM_F32(0.0)));
  EXPECT_FAILURE(l_l, WASM_BLOCK_D(WASM_F64(1.1)));

  EXPECT_FAILURE(f_ff, WASM_BLOCK_I(WASM_ZERO));
  EXPECT_FAILURE(f_ff, WASM_BLOCK_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(f_ff, WASM_BLOCK_D(WASM_F64(1.1)));

  EXPECT_FAILURE(d_dd, WASM_BLOCK_I(WASM_ZERO));
  EXPECT_FAILURE(d_dd, WASM_BLOCK_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(d_dd, WASM_BLOCK_F(WASM_F32(0.0)));
}

TEST_F(AstDecoderTest, BlockF32) {
  static const byte code[] = {WASM_BLOCK_F(kExprF32Const, 0, 0, 0, 0)};
  EXPECT_VERIFIES_C(f_ff, code);
  EXPECT_FAILURE_C(i_i, code);
  EXPECT_FAILURE_C(d_dd, code);
}

TEST_F(AstDecoderTest, BlockN_off_end) {
  byte code[] = {WASM_BLOCK(kExprNop, kExprNop, kExprNop, kExprNop)};
  EXPECT_VERIFIES_C(v_v, code);
  for (size_t i = 1; i < arraysize(code); i++) {
    Verify(kError, sigs.v_v(), code, code + i);
  }
}

TEST_F(AstDecoderTest, Block2_continue) {
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_NOP, WASM_BR(0)));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_NOP, WASM_BR(1)));
  EXPECT_FAILURE(v_v, WASM_LOOP(WASM_NOP, WASM_BR(2)));
}

TEST_F(AstDecoderTest, Block3_continue) {
  EXPECT_VERIFIES(v_v, B1(WASM_LOOP(WASM_NOP, WASM_BR(0))));
  EXPECT_VERIFIES(v_v, B1(WASM_LOOP(WASM_NOP, WASM_BR(1))));
  EXPECT_VERIFIES(v_v, B1(WASM_LOOP(WASM_NOP, WASM_BR(2))));
  EXPECT_FAILURE(v_v, B1(WASM_LOOP(WASM_NOP, WASM_BR(3))));
}

TEST_F(AstDecoderTest, NestedBlock_return) {
  EXPECT_VERIFIES(i_i, B1(B1(WASM_RETURN1(WASM_ZERO))));
}

TEST_F(AstDecoderTest, BlockBrBinop) {
  EXPECT_VERIFIES(
      i_i, WASM_I32_AND(WASM_BLOCK_I(WASM_BRV(0, WASM_I8(1))), WASM_I8(2)));
}

TEST_F(AstDecoderTest, If_empty1) {
  EXPECT_VERIFIES(v_v, WASM_ZERO, WASM_IF_OP, kExprEnd);
}

TEST_F(AstDecoderTest, If_empty2) {
  EXPECT_VERIFIES(v_v, WASM_ZERO, WASM_IF_OP, kExprElse, kExprEnd);
}

TEST_F(AstDecoderTest, If_empty3) {
  EXPECT_VERIFIES(v_v, WASM_ZERO, WASM_IF_OP, WASM_NOP, kExprElse, kExprEnd);
  EXPECT_FAILURE(v_v, WASM_ZERO, WASM_IF_OP, WASM_ZERO, kExprElse, kExprEnd);
}

TEST_F(AstDecoderTest, If_empty4) {
  EXPECT_VERIFIES(v_v, WASM_ZERO, WASM_IF_OP, kExprElse, WASM_NOP, kExprEnd);
  EXPECT_FAILURE(v_v, WASM_ZERO, WASM_IF_OP, kExprElse, WASM_ZERO, kExprEnd);
}

TEST_F(AstDecoderTest, If_empty_stack) {
  byte code[] = {kExprIf};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, If_incomplete1) {
  byte code[] = {kExprI8Const, 0, kExprIf};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, If_incomplete2) {
  byte code[] = {kExprI8Const, 0, kExprIf, kExprNop};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, If_else_else) {
  byte code[] = {kExprI8Const, 0, WASM_IF_OP, kExprElse, kExprElse, kExprEnd};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, IfEmpty) {
  EXPECT_VERIFIES(v_i, kExprGetLocal, 0, WASM_IF_OP, kExprEnd);
}

TEST_F(AstDecoderTest, IfSet) {
  EXPECT_VERIFIES(v_i,
                  WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO)));
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0),
                                    WASM_SET_LOCAL(0, WASM_ZERO), WASM_NOP));
}

TEST_F(AstDecoderTest, IfElseEmpty) {
  EXPECT_VERIFIES(v_i, WASM_GET_LOCAL(0), WASM_IF_OP, kExprElse, kExprEnd);
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, IfElseUnreachable1) {
  EXPECT_VERIFIES(i_i, WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_UNREACHABLE,
                                      WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(i_i, WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                      WASM_UNREACHABLE));
}

TEST_F(AstDecoderTest, IfElseUnreachable2) {
  static const byte code[] = {
      WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_UNREACHABLE, WASM_GET_LOCAL(0))};

  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType types[] = {kAstI32, kLocalTypes[i]};
    FunctionSig sig(1, 1, types);

    if (kLocalTypes[i] == kAstI32) {
      EXPECT_VERIFIES_SC(&sig, code);
    } else {
      EXPECT_FAILURE_SC(&sig, code);
    }
  }
}

TEST_F(AstDecoderTest, IfBreak) {
  EXPECT_VERIFIES(v_i, WASM_IF(WASM_GET_LOCAL(0), WASM_BR(0)));
  EXPECT_VERIFIES(v_i, WASM_IF(WASM_GET_LOCAL(0), WASM_BR(1)));
  EXPECT_FAILURE(v_i, WASM_IF(WASM_GET_LOCAL(0), WASM_BR(2)));
}

TEST_F(AstDecoderTest, IfElseBreak) {
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(0)));
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(1)));
  EXPECT_FAILURE(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(2)));
}

TEST_F(AstDecoderTest, Block_else) {
  byte code[] = {kExprI8Const, 0, kExprBlock, kExprElse, kExprEnd};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, IfNop) {
  EXPECT_VERIFIES(v_i, WASM_IF(WASM_GET_LOCAL(0), WASM_NOP));
}

TEST_F(AstDecoderTest, IfNopElseNop) {
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, If_end_end) {
  static const byte code[] = {kExprGetLocal, 0, WASM_IF_OP, kExprEnd, kExprEnd};
  EXPECT_VERIFIES_C(v_i, code);
}

TEST_F(AstDecoderTest, If_end_end_end) {
  static const byte code[] = {kExprGetLocal, 0,        WASM_IF_OP,
                              kExprEnd,      kExprEnd, kExprEnd};
  EXPECT_FAILURE_C(v_i, code);
}

TEST_F(AstDecoderTest, If_falloff1) {
  static const byte code[] = {kExprGetLocal, 0, kExprIf};
  EXPECT_FAILURE_C(v_i, code);
}

TEST_F(AstDecoderTest, If_falloff2) {
  static const byte code[] = {kExprGetLocal, 0, WASM_IF_OP};
  EXPECT_FAILURE_C(v_i, code);
}

TEST_F(AstDecoderTest, IfElse_falloff) {
  static const byte code[] = {kExprGetLocal, 0, WASM_IF_OP, kExprNop,
                              kExprElse};
  EXPECT_FAILURE_C(v_i, code);
}

TEST_F(AstDecoderTest, IfElseNop) {
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0),
                                    WASM_SET_LOCAL(0, WASM_ZERO), WASM_NOP));
}

TEST_F(AstDecoderTest, IfBlock1) {
  EXPECT_VERIFIES(
      v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), B1(WASM_SET_LOCAL(0, WASM_ZERO)),
                        WASM_NOP));
}

TEST_F(AstDecoderTest, IfBlock1b) {
  EXPECT_VERIFIES(v_i,
                  WASM_IF(WASM_GET_LOCAL(0), B1(WASM_SET_LOCAL(0, WASM_ZERO))));
}

TEST_F(AstDecoderTest, IfBlock2a) {
  EXPECT_VERIFIES(v_i,
                  WASM_IF(WASM_GET_LOCAL(0), B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                                WASM_SET_LOCAL(0, WASM_ZERO))));
}

TEST_F(AstDecoderTest, IfBlock2b) {
  EXPECT_VERIFIES(
      v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                              WASM_SET_LOCAL(0, WASM_ZERO)),
                        WASM_NOP));
}

TEST_F(AstDecoderTest, IfElseSet) {
  EXPECT_VERIFIES(v_i,
                  WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO),
                               WASM_SET_LOCAL(0, WASM_I8(1))));
}

TEST_F(AstDecoderTest, Loop0) { EXPECT_VERIFIES(v_v, WASM_LOOP_OP, kExprEnd); }

TEST_F(AstDecoderTest, Loop1) {
  static const byte code[] = {WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO))};
  EXPECT_VERIFIES_C(v_i, code);
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(f_ff, code);
}

TEST_F(AstDecoderTest, Loop2) {
  EXPECT_VERIFIES(v_i, WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO),
                                 WASM_SET_LOCAL(0, WASM_ZERO)));
}

TEST_F(AstDecoderTest, Loop1_continue) {
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_BR(0)));
}

TEST_F(AstDecoderTest, Loop1_break) {
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_BR(1)));
}

TEST_F(AstDecoderTest, Loop2_continue) {
  EXPECT_VERIFIES(v_i, WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO), WASM_BR(0)));
}

TEST_F(AstDecoderTest, Loop2_break) {
  EXPECT_VERIFIES(v_i, WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO), WASM_BR(1)));
}

TEST_F(AstDecoderTest, InfiniteLoop) {
  EXPECT_VERIFIES(i_i, WASM_LOOP(WASM_BR(0)));
  EXPECT_VERIFIES(i_i, WASM_LOOP(WASM_BRV(1, WASM_ZERO)));
}

TEST_F(AstDecoderTest, Loop2_unreachable) {
  EXPECT_VERIFIES(i_i, WASM_LOOP(WASM_BR(0), WASM_NOP));
}

TEST_F(AstDecoderTest, LoopType) {
  EXPECT_VERIFIES(i_i, WASM_LOOP_I(WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(l_l, WASM_LOOP_L(WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(f_f, WASM_LOOP_F(WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(d_d, WASM_LOOP_D(WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, LoopType_void) {
  EXPECT_FAILURE(v_v, WASM_LOOP_I(WASM_ZERO));
  EXPECT_FAILURE(v_v, WASM_LOOP_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(v_v, WASM_LOOP_F(WASM_F32(0.0)));
  EXPECT_FAILURE(v_v, WASM_LOOP_D(WASM_F64(1.1)));
}

TEST_F(AstDecoderTest, LoopType_fail) {
  EXPECT_FAILURE(i_i, WASM_LOOP_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(i_i, WASM_LOOP_F(WASM_F32(0.0)));
  EXPECT_FAILURE(i_i, WASM_LOOP_D(WASM_F64(1.1)));

  EXPECT_FAILURE(l_l, WASM_LOOP_I(WASM_ZERO));
  EXPECT_FAILURE(l_l, WASM_LOOP_F(WASM_F32(0.0)));
  EXPECT_FAILURE(l_l, WASM_LOOP_D(WASM_F64(1.1)));

  EXPECT_FAILURE(f_ff, WASM_LOOP_I(WASM_ZERO));
  EXPECT_FAILURE(f_ff, WASM_LOOP_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(f_ff, WASM_LOOP_D(WASM_F64(1.1)));

  EXPECT_FAILURE(d_dd, WASM_LOOP_I(WASM_ZERO));
  EXPECT_FAILURE(d_dd, WASM_LOOP_L(WASM_I64V_1(0)));
  EXPECT_FAILURE(d_dd, WASM_LOOP_F(WASM_F32(0.0)));
}

TEST_F(AstDecoderTest, ReturnVoid1) {
  static const byte code[] = {kExprNop};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
  EXPECT_FAILURE_C(i_f, code);
}

TEST_F(AstDecoderTest, ReturnVoid2) {
  static const byte code[] = {WASM_BLOCK(WASM_BR(0))};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
  EXPECT_FAILURE_C(i_f, code);
}

TEST_F(AstDecoderTest, ReturnVoid3) {
  EXPECT_FAILURE(v_v, kExprI8Const, 0);
  EXPECT_FAILURE(v_v, kExprI32Const, 0);
  EXPECT_FAILURE(v_v, kExprI64Const, 0);
  EXPECT_FAILURE(v_v, kExprF32Const, 0, 0, 0, 0);
  EXPECT_FAILURE(v_v, kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0);

  EXPECT_FAILURE(v_i, kExprGetLocal, 0);
}

TEST_F(AstDecoderTest, Unreachable1) {
  EXPECT_VERIFIES(v_v, kExprUnreachable);
  EXPECT_VERIFIES(v_v, kExprUnreachable, kExprUnreachable);
  EXPECT_VERIFIES(v_v, B2(WASM_UNREACHABLE, WASM_ZERO));
  EXPECT_VERIFIES(v_v, B2(WASM_BR(0), WASM_ZERO));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_UNREACHABLE, WASM_ZERO));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_BR(0), WASM_ZERO));
}

TEST_F(AstDecoderTest, Unreachable_binop) {
  EXPECT_VERIFIES(i_i, WASM_I32_AND(WASM_ZERO, WASM_UNREACHABLE));
  EXPECT_VERIFIES(i_i, WASM_I32_AND(WASM_UNREACHABLE, WASM_ZERO));
}

TEST_F(AstDecoderTest, Unreachable_select) {
  EXPECT_VERIFIES(i_i, WASM_SELECT(WASM_UNREACHABLE, WASM_ZERO, WASM_ZERO));
  EXPECT_VERIFIES(i_i, WASM_SELECT(WASM_ZERO, WASM_UNREACHABLE, WASM_ZERO));
  EXPECT_VERIFIES(i_i, WASM_SELECT(WASM_ZERO, WASM_ZERO, WASM_UNREACHABLE));
}

TEST_F(AstDecoderTest, If1) {
  EXPECT_VERIFIES(i_i,
                  WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I8(9), WASM_I8(8)));
  EXPECT_VERIFIES(
      i_i, WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I8(9), WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES(
      i_i, WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_I8(8)));
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
      WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I8(0), WASM_I8(2))};
  EXPECT_VERIFIES_C(i_i, kCode);
  EXPECT_FAILURE_C(i_f, kCode);
  EXPECT_FAILURE_C(i_d, kCode);
}

TEST_F(AstDecoderTest, If_type2) {
  // 1 ? float|double : 2
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_I8(1), WASM_GET_LOCAL(0), WASM_I8(1))};
  EXPECT_VERIFIES_C(i_i, kCode);
  EXPECT_FAILURE_C(i_f, kCode);
  EXPECT_FAILURE_C(i_d, kCode);
}

TEST_F(AstDecoderTest, If_type3) {
  // stmt ? 0 : 1
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_NOP, WASM_I8(0), WASM_I8(1))};
  EXPECT_FAILURE_C(i_i, kCode);
  EXPECT_FAILURE_C(i_f, kCode);
  EXPECT_FAILURE_C(i_d, kCode);
}

TEST_F(AstDecoderTest, If_type4) {
  // 0 ? stmt : 1
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_NOP, WASM_I8(1))};
  EXPECT_FAILURE_C(i_i, kCode);
  EXPECT_FAILURE_C(i_f, kCode);
  EXPECT_FAILURE_C(i_d, kCode);
}

TEST_F(AstDecoderTest, If_type5) {
  // 0 ? 1 : stmt
  static const byte kCode[] = {WASM_IF_ELSE_I(WASM_ZERO, WASM_I8(1), WASM_NOP)};
  EXPECT_FAILURE_C(i_i, kCode);
  EXPECT_FAILURE_C(i_f, kCode);
  EXPECT_FAILURE_C(i_d, kCode);
}

TEST_F(AstDecoderTest, Int64Local_param) {
  EXPECT_VERIFIES_C(l_l, kCodeGetLocal0);
}

TEST_F(AstDecoderTest, Int64Locals) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kAstI64, 1);
    for (byte j = 0; j < i; j++) {
      EXPECT_VERIFIES(l_v, WASM_GET_LOCAL(j));
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
  EXPECT_VERIFIES(v_i, WASM_SET_LOCAL(0, WASM_I32V_3(87348)));
  EXPECT_VERIFIES(
      v_i, WASM_STORE_MEM(MachineType::Int32(), WASM_I8(24), WASM_I8(40)));
  EXPECT_VERIFIES(v_i, WASM_IF(WASM_GET_LOCAL(0), WASM_NOP));
  EXPECT_VERIFIES(v_i, WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP));
  EXPECT_VERIFIES(v_v, WASM_NOP);
  EXPECT_VERIFIES(v_v, B1(WASM_NOP));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_NOP));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_BR(0)));
}

TEST_F(AstDecoderTest, MacrosContinue) {
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_CONTINUE(0)));
}

TEST_F(AstDecoderTest, MacrosVariadic) {
  EXPECT_VERIFIES(v_v, B2(WASM_NOP, WASM_NOP));
  EXPECT_VERIFIES(v_v, B3(WASM_NOP, WASM_NOP, WASM_NOP));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_NOP, WASM_NOP));
  EXPECT_VERIFIES(v_v, WASM_LOOP(WASM_NOP, WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, MacrosNestedBlocks) {
  EXPECT_VERIFIES(v_v, B2(WASM_NOP, B2(WASM_NOP, WASM_NOP)));
  EXPECT_VERIFIES(v_v, B3(WASM_NOP,                  // --
                          B2(WASM_NOP, WASM_NOP),    // --
                          B2(WASM_NOP, WASM_NOP)));  // --
  EXPECT_VERIFIES(v_v, B1(B1(B2(WASM_NOP, WASM_NOP))));
}

TEST_F(AstDecoderTest, MultipleReturn) {
  static LocalType kIntTypes5[] = {kAstI32, kAstI32, kAstI32, kAstI32, kAstI32};
  FunctionSig sig_ii_v(2, 0, kIntTypes5);
  EXPECT_VERIFIES_S(&sig_ii_v, WASM_RETURNN(2, WASM_ZERO, WASM_ONE));
  EXPECT_FAILURE_S(&sig_ii_v, WASM_RETURNN(1, WASM_ZERO));

  FunctionSig sig_iii_v(3, 0, kIntTypes5);
  EXPECT_VERIFIES_S(&sig_iii_v,
                    WASM_RETURNN(3, WASM_ZERO, WASM_ONE, WASM_I8(44)));
  EXPECT_FAILURE_S(&sig_iii_v, WASM_RETURNN(2, WASM_ZERO, WASM_ONE));
}

TEST_F(AstDecoderTest, MultipleReturn_fallthru) {
  static LocalType kIntTypes5[] = {kAstI32, kAstI32, kAstI32, kAstI32, kAstI32};
  FunctionSig sig_ii_v(2, 0, kIntTypes5);

  EXPECT_VERIFIES_S(&sig_ii_v, WASM_ZERO, WASM_ONE);
  EXPECT_FAILURE_S(&sig_ii_v, WASM_ZERO);

  FunctionSig sig_iii_v(3, 0, kIntTypes5);
  EXPECT_VERIFIES_S(&sig_iii_v, WASM_ZERO, WASM_ONE, WASM_I8(44));
  EXPECT_FAILURE_S(&sig_iii_v, WASM_ZERO, WASM_ONE);
}

TEST_F(AstDecoderTest, MacrosInt32) {
  EXPECT_VERIFIES(i_i, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_I8(12)));
  EXPECT_VERIFIES(i_i, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(13)));
  EXPECT_VERIFIES(i_i, WASM_I32_MUL(WASM_GET_LOCAL(0), WASM_I8(14)));
  EXPECT_VERIFIES(i_i, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I8(15)));
  EXPECT_VERIFIES(i_i, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_I8(16)));
  EXPECT_VERIFIES(i_i, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_I8(17)));
  EXPECT_VERIFIES(i_i, WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_I8(18)));
  EXPECT_VERIFIES(i_i, WASM_I32_AND(WASM_GET_LOCAL(0), WASM_I8(19)));
  EXPECT_VERIFIES(i_i, WASM_I32_IOR(WASM_GET_LOCAL(0), WASM_I8(20)));
  EXPECT_VERIFIES(i_i, WASM_I32_XOR(WASM_GET_LOCAL(0), WASM_I8(21)));
  EXPECT_VERIFIES(i_i, WASM_I32_SHL(WASM_GET_LOCAL(0), WASM_I8(22)));
  EXPECT_VERIFIES(i_i, WASM_I32_SHR(WASM_GET_LOCAL(0), WASM_I8(23)));
  EXPECT_VERIFIES(i_i, WASM_I32_SAR(WASM_GET_LOCAL(0), WASM_I8(24)));
  EXPECT_VERIFIES(i_i, WASM_I32_ROR(WASM_GET_LOCAL(0), WASM_I8(24)));
  EXPECT_VERIFIES(i_i, WASM_I32_ROL(WASM_GET_LOCAL(0), WASM_I8(24)));
  EXPECT_VERIFIES(i_i, WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(25)));
  EXPECT_VERIFIES(i_i, WASM_I32_NE(WASM_GET_LOCAL(0), WASM_I8(25)));

  EXPECT_VERIFIES(i_i, WASM_I32_LTS(WASM_GET_LOCAL(0), WASM_I8(26)));
  EXPECT_VERIFIES(i_i, WASM_I32_LES(WASM_GET_LOCAL(0), WASM_I8(27)));
  EXPECT_VERIFIES(i_i, WASM_I32_LTU(WASM_GET_LOCAL(0), WASM_I8(28)));
  EXPECT_VERIFIES(i_i, WASM_I32_LEU(WASM_GET_LOCAL(0), WASM_I8(29)));

  EXPECT_VERIFIES(i_i, WASM_I32_GTS(WASM_GET_LOCAL(0), WASM_I8(26)));
  EXPECT_VERIFIES(i_i, WASM_I32_GES(WASM_GET_LOCAL(0), WASM_I8(27)));
  EXPECT_VERIFIES(i_i, WASM_I32_GTU(WASM_GET_LOCAL(0), WASM_I8(28)));
  EXPECT_VERIFIES(i_i, WASM_I32_GEU(WASM_GET_LOCAL(0), WASM_I8(29)));
}

TEST_F(AstDecoderTest, MacrosInt64) {
  EXPECT_VERIFIES(l_ll, WASM_I64_ADD(WASM_GET_LOCAL(0), WASM_I64V_1(12)));
  EXPECT_VERIFIES(l_ll, WASM_I64_SUB(WASM_GET_LOCAL(0), WASM_I64V_1(13)));
  EXPECT_VERIFIES(l_ll, WASM_I64_MUL(WASM_GET_LOCAL(0), WASM_I64V_1(14)));
  EXPECT_VERIFIES(l_ll, WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_I64V_1(15)));
  EXPECT_VERIFIES(l_ll, WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_I64V_1(16)));
  EXPECT_VERIFIES(l_ll, WASM_I64_REMS(WASM_GET_LOCAL(0), WASM_I64V_1(17)));
  EXPECT_VERIFIES(l_ll, WASM_I64_REMU(WASM_GET_LOCAL(0), WASM_I64V_1(18)));
  EXPECT_VERIFIES(l_ll, WASM_I64_AND(WASM_GET_LOCAL(0), WASM_I64V_1(19)));
  EXPECT_VERIFIES(l_ll, WASM_I64_IOR(WASM_GET_LOCAL(0), WASM_I64V_1(20)));
  EXPECT_VERIFIES(l_ll, WASM_I64_XOR(WASM_GET_LOCAL(0), WASM_I64V_1(21)));

  EXPECT_VERIFIES(l_ll, WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(22)));
  EXPECT_VERIFIES(l_ll, WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(23)));
  EXPECT_VERIFIES(l_ll, WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(24)));
  EXPECT_VERIFIES(l_ll, WASM_I64_ROR(WASM_GET_LOCAL(0), WASM_I64V_1(24)));
  EXPECT_VERIFIES(l_ll, WASM_I64_ROL(WASM_GET_LOCAL(0), WASM_I64V_1(24)));

  EXPECT_VERIFIES(i_ll, WASM_I64_LTS(WASM_GET_LOCAL(0), WASM_I64V_1(26)));
  EXPECT_VERIFIES(i_ll, WASM_I64_LES(WASM_GET_LOCAL(0), WASM_I64V_1(27)));
  EXPECT_VERIFIES(i_ll, WASM_I64_LTU(WASM_GET_LOCAL(0), WASM_I64V_1(28)));
  EXPECT_VERIFIES(i_ll, WASM_I64_LEU(WASM_GET_LOCAL(0), WASM_I64V_1(29)));

  EXPECT_VERIFIES(i_ll, WASM_I64_GTS(WASM_GET_LOCAL(0), WASM_I64V_1(26)));
  EXPECT_VERIFIES(i_ll, WASM_I64_GES(WASM_GET_LOCAL(0), WASM_I64V_1(27)));
  EXPECT_VERIFIES(i_ll, WASM_I64_GTU(WASM_GET_LOCAL(0), WASM_I64V_1(28)));
  EXPECT_VERIFIES(i_ll, WASM_I64_GEU(WASM_GET_LOCAL(0), WASM_I64V_1(29)));

  EXPECT_VERIFIES(i_ll, WASM_I64_EQ(WASM_GET_LOCAL(0), WASM_I64V_1(25)));
  EXPECT_VERIFIES(i_ll, WASM_I64_NE(WASM_GET_LOCAL(0), WASM_I64V_1(25)));
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
  EXPECT_VERIFIES_C(i_i, code);
  EXPECT_FAILURE_C(f_ff, code);
}

TEST_F(AstDecoderTest, LoadMemOffset) {
  for (int offset = 0; offset < 128; offset += 7) {
    byte code[] = {kExprI8Const, 0, kExprI32LoadMem, ZERO_ALIGNMENT,
                   static_cast<byte>(offset)};
    EXPECT_VERIFIES_C(i_i, code);
  }
}

TEST_F(AstDecoderTest, LoadMemAlignment) {
  struct {
    WasmOpcode instruction;
    uint32_t maximum_aligment;
  } values[] = {
      {kExprI32LoadMem8U, 0},   // --
      {kExprI32LoadMem8S, 0},   // --
      {kExprI32LoadMem16U, 1},  // --
      {kExprI32LoadMem16S, 1},  // --
      {kExprI64LoadMem8U, 0},   // --
      {kExprI64LoadMem8S, 0},   // --
      {kExprI64LoadMem16U, 1},  // --
      {kExprI64LoadMem16S, 1},  // --
      {kExprI64LoadMem32U, 2},  // --
      {kExprI64LoadMem32S, 2},  // --
      {kExprI32LoadMem, 2},     // --
      {kExprI64LoadMem, 3},     // --
      {kExprF32LoadMem, 2},     // --
      {kExprF64LoadMem, 3},     // --
  };

  for (int i = 0; i < arraysize(values); i++) {
    for (byte alignment = 0; alignment <= 4; alignment++) {
      byte code[] = {WASM_ZERO, static_cast<byte>(values[i].instruction),
                     alignment, ZERO_OFFSET, WASM_DROP};
      if (static_cast<uint32_t>(alignment) <= values[i].maximum_aligment) {
        EXPECT_VERIFIES_C(v_i, code);
      } else {
        EXPECT_FAILURE_C(v_i, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, StoreMemOffset) {
  for (int offset = 0; offset < 128; offset += 7) {
    byte code[] = {WASM_STORE_MEM_OFFSET(MachineType::Int32(), offset,
                                         WASM_ZERO, WASM_ZERO)};
    EXPECT_VERIFIES_C(v_i, code);
  }
}

TEST_F(AstDecoderTest, StoreMemOffset_void) {
  EXPECT_FAILURE(i_i, WASM_STORE_MEM_OFFSET(MachineType::Int32(), 0, WASM_ZERO,
                                            WASM_ZERO));
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
  EXPECT_VERIFIES(i_i, WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                  VARINT1(0x45));
  EXPECT_VERIFIES(i_i, WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                  VARINT2(0x3999));
  EXPECT_VERIFIES(i_i, WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                  VARINT3(0x344445));
  EXPECT_VERIFIES(i_i, WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                  VARINT4(0x36666667));
}

TEST_F(AstDecoderTest, StoreMemOffset_varint) {
  EXPECT_VERIFIES(v_i, WASM_ZERO, WASM_ZERO, kExprI32StoreMem, ZERO_ALIGNMENT,
                  VARINT1(0x33));
  EXPECT_VERIFIES(v_i, WASM_ZERO, WASM_ZERO, kExprI32StoreMem, ZERO_ALIGNMENT,
                  VARINT2(0x1111));
  EXPECT_VERIFIES(v_i, WASM_ZERO, WASM_ZERO, kExprI32StoreMem, ZERO_ALIGNMENT,
                  VARINT3(0x222222));
  EXPECT_VERIFIES(v_i, WASM_ZERO, WASM_ZERO, kExprI32StoreMem, ZERO_ALIGNMENT,
                  VARINT4(0x44444444));
}

TEST_F(AstDecoderTest, AllLoadMemCombinations) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType local_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_LOAD_MEM(mem_type, WASM_ZERO)};
      FunctionSig sig(1, 0, &local_type);
      if (local_type == WasmOpcodes::LocalTypeFor(mem_type)) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
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
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
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
  byte AddGlobal(LocalType type, bool mutability = true) {
    mod.globals.push_back({type, mutability, NO_INIT, 0, false, false});
    CHECK(mod.globals.size() <= 127);
    return static_cast<byte>(mod.globals.size() - 1);
  }
  byte AddSignature(FunctionSig* sig) {
    mod.signatures.push_back(sig);
    CHECK(mod.signatures.size() <= 127);
    return static_cast<byte>(mod.signatures.size() - 1);
  }
  byte AddFunction(FunctionSig* sig) {
    mod.functions.push_back({sig,      // sig
                             0,        // func_index
                             0,        // sig_index
                             0,        // name_offset
                             0,        // name_length
                             0,        // code_start_offset
                             0,        // code_end_offset
                             false,    // import
                             false});  // export
    CHECK(mod.functions.size() <= 127);
    return static_cast<byte>(mod.functions.size() - 1);
  }
  byte AddImport(FunctionSig* sig) {
    byte result = AddFunction(sig);
    mod.functions[result].imported = true;
    return result;
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

  EXPECT_VERIFIES_S(sig, WASM_CALL_FUNCTION0(0));
  EXPECT_VERIFIES_S(sig, WASM_CALL_FUNCTION(1, WASM_I8(27)));
  EXPECT_VERIFIES_S(sig, WASM_CALL_FUNCTION(2, WASM_I8(37), WASM_I8(77)));
}

TEST_F(AstDecoderTest, CallsWithTooFewArguments) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_i());
  module_env.AddFunction(sigs.i_ii());
  module_env.AddFunction(sigs.f_ff());

  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION0(0));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(1, WASM_ZERO));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(2, WASM_GET_LOCAL(0)));
}

TEST_F(AstDecoderTest, CallsWithMismatchedSigs2) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_i());

  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(0, WASM_I64V_1(17)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(0, WASM_F32(17.1)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(0, WASM_F64(17.1)));
}

TEST_F(AstDecoderTest, CallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(sigs.i_f());

  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(0, WASM_I8(17)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(0, WASM_I64V_1(27)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(0, WASM_F64(37.2)));

  module_env.AddFunction(sigs.i_d());

  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(1, WASM_I8(16)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(1, WASM_I64V_1(16)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(1, WASM_F32(17.6)));
}

TEST_F(AstDecoderTest, MultiReturn) {
  FLAG_wasm_mv_prototype = true;
  LocalType storage[] = {kAstI32, kAstI32};
  FunctionSig sig_ii_v(2, 0, storage);
  FunctionSig sig_v_ii(0, 2, storage);
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddFunction(&sig_v_ii);
  module_env.AddFunction(&sig_ii_v);

  EXPECT_VERIFIES_S(&sig_ii_v, WASM_CALL_FUNCTION0(1));
  EXPECT_VERIFIES(v_v, WASM_CALL_FUNCTION0(1), WASM_DROP, WASM_DROP);
  EXPECT_VERIFIES(v_v, WASM_CALL_FUNCTION0(1), kExprCallFunction, 0);
}

TEST_F(AstDecoderTest, MultiReturnType) {
  FLAG_wasm_mv_prototype = true;
  for (size_t a = 0; a < arraysize(kLocalTypes); a++) {
    for (size_t b = 0; b < arraysize(kLocalTypes); b++) {
      for (size_t c = 0; c < arraysize(kLocalTypes); c++) {
        for (size_t d = 0; d < arraysize(kLocalTypes); d++) {
          LocalType storage_ab[] = {kLocalTypes[a], kLocalTypes[b]};
          FunctionSig sig_ab_v(2, 0, storage_ab);
          LocalType storage_cd[] = {kLocalTypes[c], kLocalTypes[d]};
          FunctionSig sig_cd_v(2, 0, storage_cd);

          TestModuleEnv module_env;
          module = &module_env;
          module_env.AddFunction(&sig_cd_v);

          EXPECT_VERIFIES_S(&sig_cd_v, WASM_CALL_FUNCTION0(0));

          if (a == c && b == d) {
            EXPECT_VERIFIES_S(&sig_ab_v, WASM_CALL_FUNCTION0(0));
          } else {
            EXPECT_FAILURE_S(&sig_ab_v, WASM_CALL_FUNCTION0(0));
          }
        }
      }
    }
  }
}

TEST_F(AstDecoderTest, SimpleIndirectCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddSignature(sigs.i_v());
  byte f1 = module_env.AddSignature(sigs.i_i());
  byte f2 = module_env.AddSignature(sigs.i_ii());

  EXPECT_VERIFIES_S(sig, WASM_CALL_INDIRECT0(f0, WASM_ZERO));
  EXPECT_VERIFIES_S(sig, WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I8(22)));
  EXPECT_VERIFIES_S(
      sig, WASM_CALL_INDIRECT2(f2, WASM_ZERO, WASM_I8(32), WASM_I8(72)));
}

TEST_F(AstDecoderTest, IndirectCallsOutOfBounds) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT0(0, WASM_ZERO));
  module_env.AddSignature(sigs.i_v());
  EXPECT_VERIFIES_S(sig, WASM_CALL_INDIRECT0(0, WASM_ZERO));

  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(1, WASM_ZERO, WASM_I8(22)));
  module_env.AddSignature(sigs.i_i());
  EXPECT_VERIFIES_S(sig, WASM_CALL_INDIRECT1(1, WASM_ZERO, WASM_I8(27)));

  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(2, WASM_ZERO, WASM_I8(27)));
}

TEST_F(AstDecoderTest, IndirectCallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddFunction(sigs.i_f());

  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_I8(17)));
  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_I64V_1(27)));
  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_F64(37.2)));

  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT0(f0, WASM_I8(17)));
  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT0(f0, WASM_I64V_1(27)));
  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT0(f0, WASM_F64(37.2)));

  byte f1 = module_env.AddFunction(sigs.i_d());

  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I8(16)));
  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I64V_1(16)));
  EXPECT_FAILURE_S(sig, WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_F32(17.6)));
}

TEST_F(AstDecoderTest, SimpleImportCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddImport(sigs.i_v());
  byte f1 = module_env.AddImport(sigs.i_i());
  byte f2 = module_env.AddImport(sigs.i_ii());

  EXPECT_VERIFIES_S(sig, WASM_CALL_FUNCTION0(f0));
  EXPECT_VERIFIES_S(sig, WASM_CALL_FUNCTION(f1, WASM_I8(22)));
  EXPECT_VERIFIES_S(sig, WASM_CALL_FUNCTION(f2, WASM_I8(32), WASM_I8(72)));
}

TEST_F(AstDecoderTest, ImportCallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  byte f0 = module_env.AddImport(sigs.i_f());

  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION0(f0));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(f0, WASM_I8(17)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(f0, WASM_I64V_1(27)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(f0, WASM_F64(37.2)));

  byte f1 = module_env.AddImport(sigs.i_d());

  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION0(f1));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(f1, WASM_I8(16)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(f1, WASM_I64V_1(16)));
  EXPECT_FAILURE_S(sig, WASM_CALL_FUNCTION(f1, WASM_F32(17.6)));
}

TEST_F(AstDecoderTest, Int32Globals) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstI32);

  EXPECT_VERIFIES_S(sig, WASM_GET_GLOBAL(0));
  EXPECT_FAILURE_S(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
  EXPECT_VERIFIES_S(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_ZERO);
}

TEST_F(AstDecoderTest, ImmutableGlobal) {
  FunctionSig* sig = sigs.v_v();
  TestModuleEnv module_env;
  module = &module_env;

  uint32_t g0 = module_env.AddGlobal(kAstI32, true);
  uint32_t g1 = module_env.AddGlobal(kAstI32, false);

  EXPECT_VERIFIES_S(sig, WASM_SET_GLOBAL(g0, WASM_ZERO));
  EXPECT_FAILURE_S(sig, WASM_SET_GLOBAL(g1, WASM_ZERO));
}

TEST_F(AstDecoderTest, Int32Globals_fail) {
  FunctionSig* sig = sigs.i_i();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstI64);
  module_env.AddGlobal(kAstI64);
  module_env.AddGlobal(kAstF32);
  module_env.AddGlobal(kAstF64);

  EXPECT_FAILURE_S(sig, WASM_GET_GLOBAL(0));
  EXPECT_FAILURE_S(sig, WASM_GET_GLOBAL(1));
  EXPECT_FAILURE_S(sig, WASM_GET_GLOBAL(2));
  EXPECT_FAILURE_S(sig, WASM_GET_GLOBAL(3));

  EXPECT_FAILURE_S(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_ZERO);
  EXPECT_FAILURE_S(sig, WASM_SET_GLOBAL(1, WASM_GET_LOCAL(0)), WASM_ZERO);
  EXPECT_FAILURE_S(sig, WASM_SET_GLOBAL(2, WASM_GET_LOCAL(0)), WASM_ZERO);
  EXPECT_FAILURE_S(sig, WASM_SET_GLOBAL(3, WASM_GET_LOCAL(0)), WASM_ZERO);
}

TEST_F(AstDecoderTest, Int64Globals) {
  FunctionSig* sig = sigs.l_l();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstI64);
  module_env.AddGlobal(kAstI64);

  EXPECT_VERIFIES_S(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_S(sig, WASM_GET_GLOBAL(1));

  EXPECT_VERIFIES_S(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0));
  EXPECT_VERIFIES_S(sig, WASM_SET_GLOBAL(1, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0));
}

TEST_F(AstDecoderTest, Float32Globals) {
  FunctionSig* sig = sigs.f_ff();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstF32);

  EXPECT_VERIFIES_S(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_S(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0));
}

TEST_F(AstDecoderTest, Float64Globals) {
  FunctionSig* sig = sigs.d_dd();
  TestModuleEnv module_env;
  module = &module_env;

  module_env.AddGlobal(kAstF64);

  EXPECT_VERIFIES_S(sig, WASM_GET_GLOBAL(0));
  EXPECT_VERIFIES_S(sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)),
                    WASM_GET_LOCAL(0));
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
        EXPECT_VERIFIES_S(&sig, WASM_GET_GLOBAL(0));
      } else {
        EXPECT_FAILURE_S(&sig, WASM_GET_GLOBAL(0));
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
        EXPECT_VERIFIES_S(&sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
      } else {
        EXPECT_FAILURE_S(&sig, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)));
      }
    }
  }
}

TEST_F(AstDecoderTest, WasmGrowMemory) {
  TestModuleEnv module_env;
  module = &module_env;
  module->origin = kWasmOrigin;

  byte code[] = {WASM_UNOP(kExprGrowMemory, WASM_GET_LOCAL(0))};
  EXPECT_VERIFIES_C(i_i, code);
  EXPECT_FAILURE_C(i_d, code);
}

TEST_F(AstDecoderTest, AsmJsGrowMemory) {
  TestModuleEnv module_env;
  module = &module_env;
  module->origin = kAsmJsOrigin;

  byte code[] = {WASM_UNOP(kExprGrowMemory, WASM_GET_LOCAL(0))};
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, AsmJsBinOpsCheckOrigin) {
  LocalType float32int32float32[] = {kAstF32, kAstI32, kAstF32};
  FunctionSig sig_f_if(1, 2, float32int32float32);
  LocalType float64int32float64[] = {kAstF64, kAstI32, kAstF64};
  FunctionSig sig_d_id(1, 2, float64int32float64);
  struct {
    WasmOpcode op;
    FunctionSig* sig;
  } AsmJsBinOps[] = {
      {kExprF64Atan2, sigs.d_dd()},
      {kExprF64Pow, sigs.d_dd()},
      {kExprF64Mod, sigs.d_dd()},
      {kExprI32AsmjsDivS, sigs.i_ii()},
      {kExprI32AsmjsDivU, sigs.i_ii()},
      {kExprI32AsmjsRemS, sigs.i_ii()},
      {kExprI32AsmjsRemU, sigs.i_ii()},
      {kExprI32AsmjsStoreMem8, sigs.i_ii()},
      {kExprI32AsmjsStoreMem16, sigs.i_ii()},
      {kExprI32AsmjsStoreMem, sigs.i_ii()},
      {kExprF32AsmjsStoreMem, &sig_f_if},
      {kExprF64AsmjsStoreMem, &sig_d_id},
  };

  {
    TestModuleEnv module_env;
    module = &module_env;
    module->origin = kAsmJsOrigin;
    for (int i = 0; i < arraysize(AsmJsBinOps); i++) {
      TestBinop(AsmJsBinOps[i].op, AsmJsBinOps[i].sig);
    }
  }

  {
    TestModuleEnv module_env;
    module = &module_env;
    module->origin = kWasmOrigin;
    for (int i = 0; i < arraysize(AsmJsBinOps); i++) {
      byte code[] = {
          WASM_BINOP(AsmJsBinOps[i].op, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
      EXPECT_FAILURE_SC(AsmJsBinOps[i].sig, code);
    }
  }
}

TEST_F(AstDecoderTest, AsmJsUnOpsCheckOrigin) {
  LocalType float32int32[] = {kAstF32, kAstI32};
  FunctionSig sig_f_i(1, 1, float32int32);
  LocalType float64int32[] = {kAstF64, kAstI32};
  FunctionSig sig_d_i(1, 1, float64int32);
  struct {
    WasmOpcode op;
    FunctionSig* sig;
  } AsmJsUnOps[] = {{kExprF64Acos, sigs.d_d()},
                    {kExprF64Asin, sigs.d_d()},
                    {kExprF64Atan, sigs.d_d()},
                    {kExprF64Cos, sigs.d_d()},
                    {kExprF64Sin, sigs.d_d()},
                    {kExprF64Tan, sigs.d_d()},
                    {kExprF64Exp, sigs.d_d()},
                    {kExprF64Log, sigs.d_d()},
                    {kExprI32AsmjsLoadMem8S, sigs.i_i()},
                    {kExprI32AsmjsLoadMem8U, sigs.i_i()},
                    {kExprI32AsmjsLoadMem16S, sigs.i_i()},
                    {kExprI32AsmjsLoadMem16U, sigs.i_i()},
                    {kExprI32AsmjsLoadMem, sigs.i_i()},
                    {kExprF32AsmjsLoadMem, &sig_f_i},
                    {kExprF64AsmjsLoadMem, &sig_d_i},
                    {kExprI32AsmjsSConvertF32, sigs.i_f()},
                    {kExprI32AsmjsUConvertF32, sigs.i_f()},
                    {kExprI32AsmjsSConvertF64, sigs.i_d()},
                    {kExprI32AsmjsUConvertF64, sigs.i_d()}};
  {
    TestModuleEnv module_env;
    module = &module_env;
    module->origin = kAsmJsOrigin;
    for (int i = 0; i < arraysize(AsmJsUnOps); i++) {
      TestUnop(AsmJsUnOps[i].op, AsmJsUnOps[i].sig);
    }
  }

  {
    TestModuleEnv module_env;
    module = &module_env;
    module->origin = kWasmOrigin;
    for (int i = 0; i < arraysize(AsmJsUnOps); i++) {
      byte code[] = {WASM_UNOP(AsmJsUnOps[i].op, WASM_GET_LOCAL(0))};
      EXPECT_FAILURE_SC(AsmJsUnOps[i].sig, code);
    }
  }
}

TEST_F(AstDecoderTest, BreakEnd) {
  EXPECT_VERIFIES(
      i_i, WASM_BLOCK_I(WASM_I32_ADD(WASM_BRV(0, WASM_ZERO), WASM_ZERO)));
  EXPECT_VERIFIES(
      i_i, WASM_BLOCK_I(WASM_I32_ADD(WASM_ZERO, WASM_BRV(0, WASM_ZERO))));
}

TEST_F(AstDecoderTest, BreakIfBinop) {
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_I32_ADD(
                           WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO), WASM_ZERO)));
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_I32_ADD(
                           WASM_ZERO, WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO))));
  EXPECT_VERIFIES_S(
      sigs.f_ff(),
      WASM_BLOCK_F(WASM_F32_ABS(WASM_BRV_IF(0, WASM_F32(0.0f), WASM_ZERO))));
}

TEST_F(AstDecoderTest, BreakIfBinop_fail) {
  EXPECT_FAILURE_S(
      sigs.f_ff(),
      WASM_BLOCK_F(WASM_F32_ABS(WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO))));
  EXPECT_FAILURE_S(
      sigs.i_i(),
      WASM_BLOCK_I(WASM_F32_ABS(WASM_BRV_IF(0, WASM_F32(0.0f), WASM_ZERO))));
}

TEST_F(AstDecoderTest, BreakNesting1) {
  for (int i = 0; i < 5; i++) {
    // (block[2] (loop[2] (if (get p) break[N]) (set p 1)) p)
    byte code[] = {WASM_BLOCK_I(
        WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(i + 1, WASM_ZERO)),
                  WASM_SET_LOCAL(0, WASM_I8(1))),
        WASM_ZERO)};
    if (i < 3) {
      EXPECT_VERIFIES_C(i_i, code);
    } else {
      EXPECT_FAILURE_C(i_i, code);
    }
  }
}

TEST_F(AstDecoderTest, BreakNesting2) {
  for (int i = 0; i < 7; i++) {
    byte code[] = {B1(WASM_LOOP(WASM_IF(WASM_ZERO, WASM_BR(i)), WASM_NOP))};
    if (i <= 3) {
      EXPECT_VERIFIES_C(v_v, code);
    } else {
      EXPECT_FAILURE_C(v_v, code);
    }
  }
}

TEST_F(AstDecoderTest, BreakNesting3) {
  for (int i = 0; i < 7; i++) {
    // (block[1] (loop[1] (block[1] (if 0 break[N])
    byte code[] = {
        WASM_BLOCK(WASM_LOOP(B1(WASM_IF(WASM_ZERO, WASM_BR(i + 1)))))};
    if (i < 4) {
      EXPECT_VERIFIES_C(v_v, code);
    } else {
      EXPECT_FAILURE_C(v_v, code);
    }
  }
}

TEST_F(AstDecoderTest, BreaksWithMultipleTypes) {
  EXPECT_FAILURE(i_i, B2(WASM_BRV_IF_ZERO(0, WASM_I8(7)), WASM_F32(7.7)));

  EXPECT_FAILURE(i_i, B2(WASM_BRV_IF_ZERO(0, WASM_I8(7)),
                         WASM_BRV_IF_ZERO(0, WASM_F32(7.7))));
  EXPECT_FAILURE(
      i_i, B3(WASM_BRV_IF_ZERO(0, WASM_I8(8)), WASM_BRV_IF_ZERO(0, WASM_I8(0)),
              WASM_BRV_IF_ZERO(0, WASM_F32(7.7))));
  EXPECT_FAILURE(i_i, B3(WASM_BRV_IF_ZERO(0, WASM_I8(9)),
                         WASM_BRV_IF_ZERO(0, WASM_F32(7.7)),
                         WASM_BRV_IF_ZERO(0, WASM_I8(11))));
}

TEST_F(AstDecoderTest, BreakNesting_6_levels) {
  for (int mask = 0; mask < 64; mask++) {
    for (int i = 0; i < 14; i++) {
      byte code[] = {WASM_BLOCK(WASM_BLOCK(
          WASM_BLOCK(WASM_BLOCK(WASM_BLOCK(WASM_BLOCK(WASM_BR(i)))))))};

      int depth = 6;
      int m = mask;
      for (size_t pos = 0; pos < sizeof(code) - 1; pos++) {
        if (code[pos] != kExprBlock) continue;
        if (m & 1) {
          code[pos] = kExprLoop;
          code[pos + 1] = kLocalVoid;
        }
        m >>= 1;
      }

      if (i <= depth) {
        EXPECT_VERIFIES_C(v_v, code);
      } else {
        EXPECT_FAILURE_C(v_v, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, Break_TypeCheck) {
  FunctionSig* sigarray[] = {sigs.i_i(), sigs.l_l(), sigs.f_ff(), sigs.d_dd()};
  for (size_t i = 0; i < arraysize(sigarray); i++) {
    FunctionSig* sig = sigarray[i];
    // unify X and X => OK
    byte code[] = {WASM_BLOCK_T(
        sig->GetReturn(), WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
        WASM_GET_LOCAL(0))};
    EXPECT_VERIFIES_SC(sig, code);
  }

  // unify i32 and f32 => fail
  EXPECT_FAILURE(i_i, WASM_BLOCK_I(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_ZERO)),
                                   WASM_F32(1.2)));

  // unify f64 and f64 => OK
  EXPECT_VERIFIES(
      d_dd, WASM_BLOCK_D(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
                         WASM_F64(1.2)));
}

TEST_F(AstDecoderTest, Break_TypeCheckAll1) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType storage[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_BLOCK_T(
          sig.GetReturn(), WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
          WASM_GET_LOCAL(1))};

      if (i == j) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, Break_TypeCheckAll2) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType storage[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_IF_ELSE_T(sig.GetReturn(0), WASM_ZERO,
                                    WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)),
                                    WASM_GET_LOCAL(1))};

      if (i == j) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, Break_TypeCheckAll3) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType storage[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_IF_ELSE_T(sig.GetReturn(), WASM_ZERO,
                                    WASM_GET_LOCAL(1),
                                    WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))};

      if (i == j) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, Break_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      LocalType type = kLocalTypes[i];
      LocalType storage[] = {kAstI32, kAstI32, type};
      FunctionSig sig(1, 2, storage);

      byte code1[] = {WASM_BLOCK_T(
          type, WASM_IF(WASM_ZERO, WASM_BRV(1, WASM_GET_LOCAL(which))),
          WASM_GET_LOCAL(which ^ 1))};

      if (type == kAstI32) {
        EXPECT_VERIFIES_SC(&sig, code1);
      } else {
        EXPECT_FAILURE_SC(&sig, code1);
      }
    }
  }
}

TEST_F(AstDecoderTest, BreakIf_cond_type) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType types[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j]};
      FunctionSig sig(1, 2, types);
      byte code[] = {WASM_BLOCK_T(
          types[0], WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)))};

      if (types[2] == kAstI32) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, BreakIf_val_type) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalType types[] = {kLocalTypes[i], kLocalTypes[i], kLocalTypes[j],
                           kAstI32};
      FunctionSig sig(1, 3, types);
      byte code[] = {WASM_BLOCK_T(
          types[1], WASM_BRV_IF(0, WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
          WASM_DROP, WASM_GET_LOCAL(0))};

      if (i == j) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, BreakIf_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
      LocalType type = kLocalTypes[i];
      LocalType storage[] = {kAstI32, kAstI32, type};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(which)),
                                  WASM_DROP, WASM_GET_LOCAL(which ^ 1))};

      if (type == kAstI32) {
        EXPECT_VERIFIES_SC(&sig, code);
      } else {
        EXPECT_FAILURE_SC(&sig, code);
      }
    }
  }
}

TEST_F(AstDecoderTest, BrTable0) {
  static byte code[] = {kExprBrTable, 0, BR_TARGET(0)};
  EXPECT_FAILURE_C(v_v, code);
}

TEST_F(AstDecoderTest, BrTable0b) {
  static byte code[] = {kExprI32Const, 11, kExprBrTable, 0, BR_TARGET(0)};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, BrTable0c) {
  static byte code[] = {kExprI32Const, 11, kExprBrTable, 0, BR_TARGET(1)};
  EXPECT_FAILURE_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
}

TEST_F(AstDecoderTest, BrTable1a) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_I8(67), 0, BR_TARGET(0)))};
  EXPECT_VERIFIES_C(v_v, code);
}

TEST_F(AstDecoderTest, BrTable1b) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_ZERO, 0, BR_TARGET(0)))};
  EXPECT_VERIFIES_C(v_v, code);
  EXPECT_FAILURE_C(i_i, code);
  EXPECT_FAILURE_C(f_ff, code);
  EXPECT_FAILURE_C(d_dd, code);
}

TEST_F(AstDecoderTest, BrTable2a) {
  static byte code[] = {
      B1(WASM_BR_TABLE(WASM_I8(67), 1, BR_TARGET(0), BR_TARGET(0)))};
  EXPECT_VERIFIES_C(v_v, code);
}

TEST_F(AstDecoderTest, BrTable2b) {
  static byte code[] = {WASM_BLOCK(
      WASM_BLOCK(WASM_BR_TABLE(WASM_I8(67), 1, BR_TARGET(0), BR_TARGET(1))))};
  EXPECT_VERIFIES_C(v_v, code);
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
    if (depth <= 1) {
      EXPECT_VERIFIES_C(v_i, code);
    } else {
      EXPECT_FAILURE_C(v_i, code);
    }
  }
}

TEST_F(AstDecoderTest, BrTable_invalid_br2) {
  for (int depth = 0; depth < 7; depth++) {
    byte code[] = {
        WASM_LOOP(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(depth)))};
    if (depth < 2) {
      EXPECT_VERIFIES_C(v_i, code);
    } else {
      EXPECT_FAILURE_C(v_i, code);
    }
  }
}

TEST_F(AstDecoderTest, Brv1) {
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_BRV(0, WASM_ZERO)));
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_LOOP(WASM_BRV(2, WASM_ZERO))));
}

TEST_F(AstDecoderTest, Brv1_type) {
  EXPECT_VERIFIES(i_ii, WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0))));
  EXPECT_VERIFIES(l_ll, WASM_BLOCK_L(WASM_BRV(0, WASM_GET_LOCAL(0))));
  EXPECT_VERIFIES(f_ff, WASM_BLOCK_F(WASM_BRV(0, WASM_GET_LOCAL(0))));
  EXPECT_VERIFIES(d_dd, WASM_BLOCK_D(WASM_BRV(0, WASM_GET_LOCAL(0))));
}

TEST_F(AstDecoderTest, Brv1_type_n) {
  EXPECT_FAILURE(i_f, WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0))));
  EXPECT_FAILURE(i_d, WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0))));
}

TEST_F(AstDecoderTest, BrvIf1) {
  EXPECT_VERIFIES(i_v, WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_ZERO)));
}

TEST_F(AstDecoderTest, BrvIf1_type) {
  EXPECT_VERIFIES(i_i, WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))));
  EXPECT_VERIFIES(l_l, WASM_BLOCK_L(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))));
  EXPECT_VERIFIES(f_ff, WASM_BLOCK_F(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))));
  EXPECT_VERIFIES(d_dd, WASM_BLOCK_D(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))));
}

TEST_F(AstDecoderTest, BrvIf1_type_n) {
  EXPECT_FAILURE(i_f, WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))));
  EXPECT_FAILURE(i_d, WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0))));
}

TEST_F(AstDecoderTest, Select) {
  EXPECT_VERIFIES(i_i,
                  WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_ZERO));
  EXPECT_VERIFIES(f_ff, WASM_SELECT(WASM_F32(0.0), WASM_F32(0.0), WASM_ZERO));
  EXPECT_VERIFIES(d_dd, WASM_SELECT(WASM_F64(0.0), WASM_F64(0.0), WASM_ZERO));
  EXPECT_VERIFIES(l_l, WASM_SELECT(WASM_I64V_1(0), WASM_I64V_1(0), WASM_ZERO));
}

TEST_F(AstDecoderTest, Select_fail1) {
  EXPECT_FAILURE(
      i_i, WASM_SELECT(WASM_F32(0.0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));
  EXPECT_FAILURE(
      i_i, WASM_SELECT(WASM_GET_LOCAL(0), WASM_F32(0.0), WASM_GET_LOCAL(0)));
  EXPECT_FAILURE(
      i_i, WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_F32(0.0)));
}

TEST_F(AstDecoderTest, Select_fail2) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalType type = kLocalTypes[i];
    if (type == kAstI32) continue;

    LocalType types[] = {type, kAstI32, type};
    FunctionSig sig(1, 2, types);

    EXPECT_VERIFIES_S(&sig, WASM_SELECT(WASM_GET_LOCAL(1), WASM_GET_LOCAL(1),
                                        WASM_GET_LOCAL(0)));

    EXPECT_FAILURE_S(&sig, WASM_SELECT(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0),
                                       WASM_GET_LOCAL(0)));

    EXPECT_FAILURE_S(&sig, WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                       WASM_GET_LOCAL(0)));

    EXPECT_FAILURE_S(&sig, WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                       WASM_GET_LOCAL(1)));
  }
}

TEST_F(AstDecoderTest, Select_TypeCheck) {
  EXPECT_FAILURE(
      i_i, WASM_SELECT(WASM_F32(9.9), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));

  EXPECT_FAILURE(
      i_i, WASM_SELECT(WASM_GET_LOCAL(0), WASM_F64(0.25), WASM_GET_LOCAL(0)));

  EXPECT_FAILURE(i_i,
                 WASM_SELECT(WASM_F32(9.9), WASM_GET_LOCAL(0), WASM_I64V_1(0)));
}

TEST_F(AstDecoderTest, Throw) {
  FLAG_wasm_eh_prototype = true;
  EXPECT_VERIFIES(v_i, WASM_GET_LOCAL(0), kExprThrow);

  EXPECT_FAILURE(i_d, WASM_GET_LOCAL(0), kExprThrow, WASM_I32V(0));
  EXPECT_FAILURE(i_f, WASM_GET_LOCAL(0), kExprThrow, WASM_I32V(0));
  EXPECT_FAILURE(l_l, WASM_GET_LOCAL(0), kExprThrow, WASM_I64V(0));
}

#define WASM_TRY_OP kExprTry, kLocalVoid

#define WASM_CATCH(local) kExprCatch, static_cast<byte>(local)

TEST_F(AstDecoderTest, TryCatch) {
  FLAG_wasm_eh_prototype = true;
  EXPECT_VERIFIES(v_i, WASM_TRY_OP, WASM_CATCH(0), kExprEnd);

  // Missing catch.
  EXPECT_FAILURE(v_v, WASM_TRY_OP, kExprEnd);

  // Missing end.
  EXPECT_FAILURE(v_i, WASM_TRY_OP, WASM_CATCH(0));

  // Double catch.
  EXPECT_FAILURE(v_i, WASM_TRY_OP, WASM_CATCH(0), WASM_CATCH(0), kExprEnd);
}

TEST_F(AstDecoderTest, MultiValBlock1) {
  FLAG_wasm_mv_prototype = true;
  EXPECT_VERIFIES(i_ii, WASM_BLOCK_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(1)),
                  kExprI32Add);
}

TEST_F(AstDecoderTest, MultiValBlock2) {
  FLAG_wasm_mv_prototype = true;
  EXPECT_VERIFIES(i_ii, WASM_BLOCK_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(1)),
                  WASM_I32_ADD(WASM_NOP, WASM_NOP));
}

TEST_F(AstDecoderTest, MultiValBlockBr1) {
  FLAG_wasm_mv_prototype = true;
  EXPECT_FAILURE(i_ii,
                 WASM_BLOCK_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0), WASM_BR(0)),
                 kExprI32Add);
  EXPECT_VERIFIES(i_ii, WASM_BLOCK_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(1), WASM_BR(0)),
                  kExprI32Add);
}

TEST_F(AstDecoderTest, MultiValIf1) {
  FLAG_wasm_mv_prototype = true;
  EXPECT_FAILURE(
      i_ii, WASM_IF_ELSE_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0),
                            WASM_SEQ(WASM_GET_LOCAL(0)),
                            WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
      kExprI32Add);
  EXPECT_FAILURE(i_ii,
                 WASM_IF_ELSE_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0),
                                 WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                                 WASM_SEQ(WASM_GET_LOCAL(1))),
                 kExprI32Add);
  EXPECT_VERIFIES(
      i_ii, WASM_IF_ELSE_TT(kAstI32, kAstI32, WASM_GET_LOCAL(0),
                            WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                            WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
      kExprI32Add);
}

class BranchTableIteratorTest : public TestWithZone {
 public:
  BranchTableIteratorTest() : TestWithZone() {}
  void CheckBrTableSize(const byte* start, const byte* end) {
    Decoder decoder(start, end);
    BranchTableOperand operand(&decoder, start);
    BranchTableIterator iterator(&decoder, operand);
    EXPECT_EQ(end - start - 1, iterator.length());
    EXPECT_TRUE(decoder.ok());
  }
  void CheckBrTableError(const byte* start, const byte* end) {
    Decoder decoder(start, end);
    BranchTableOperand operand(&decoder, start);
    BranchTableIterator iterator(&decoder, operand);
    iterator.length();
    EXPECT_FALSE(decoder.ok());
  }
};

#define CHECK_BR_TABLE_LENGTH(...)                    \
  {                                                   \
    static byte code[] = {kExprBrTable, __VA_ARGS__}; \
    CheckBrTableSize(code, code + sizeof(code));      \
  }

#define CHECK_BR_TABLE_ERROR(...)                     \
  {                                                   \
    static byte code[] = {kExprBrTable, __VA_ARGS__}; \
    CheckBrTableError(code, code + sizeof(code));     \
  }

TEST_F(BranchTableIteratorTest, count0) {
  CHECK_BR_TABLE_LENGTH(0, U32V_1(1));
  CHECK_BR_TABLE_LENGTH(0, U32V_2(200));
  CHECK_BR_TABLE_LENGTH(0, U32V_3(30000));
  CHECK_BR_TABLE_LENGTH(0, U32V_4(400000));

  CHECK_BR_TABLE_LENGTH(0, U32V_1(2));
  CHECK_BR_TABLE_LENGTH(0, U32V_2(300));
  CHECK_BR_TABLE_LENGTH(0, U32V_3(40000));
  CHECK_BR_TABLE_LENGTH(0, U32V_4(500000));
}

TEST_F(BranchTableIteratorTest, count1) {
  CHECK_BR_TABLE_LENGTH(1, U32V_1(1), U32V_1(6));
  CHECK_BR_TABLE_LENGTH(1, U32V_2(200), U32V_1(8));
  CHECK_BR_TABLE_LENGTH(1, U32V_3(30000), U32V_1(9));
  CHECK_BR_TABLE_LENGTH(1, U32V_4(400000), U32V_1(11));

  CHECK_BR_TABLE_LENGTH(1, U32V_1(2), U32V_2(6));
  CHECK_BR_TABLE_LENGTH(1, U32V_2(300), U32V_2(7));
  CHECK_BR_TABLE_LENGTH(1, U32V_3(40000), U32V_2(8));
  CHECK_BR_TABLE_LENGTH(1, U32V_4(500000), U32V_2(9));
}

TEST_F(BranchTableIteratorTest, error0) {
  CHECK_BR_TABLE_ERROR(0);
  CHECK_BR_TABLE_ERROR(1, U32V_1(33));
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
  EXPECT_LENGTH(2, kExprBlock);
  EXPECT_LENGTH(2, kExprLoop);
  EXPECT_LENGTH(2, kExprIf);
  EXPECT_LENGTH(1, kExprElse);
  EXPECT_LENGTH(1, kExprEnd);
  EXPECT_LENGTH(1, kExprSelect);
  EXPECT_LENGTH(2, kExprBr);
  EXPECT_LENGTH(2, kExprBrIf);
  EXPECT_LENGTH(1, kExprThrow);
  EXPECT_LENGTH(2, kExprTry);
  EXPECT_LENGTH(2, kExprCatch);
}

TEST_F(WasmOpcodeLengthTest, MiscExpressions) {
  EXPECT_LENGTH(2, kExprI8Const);
  EXPECT_LENGTH(5, kExprF32Const);
  EXPECT_LENGTH(9, kExprF64Const);
  EXPECT_LENGTH(2, kExprGetLocal);
  EXPECT_LENGTH(2, kExprSetLocal);
  EXPECT_LENGTH(2, kExprGetGlobal);
  EXPECT_LENGTH(2, kExprSetGlobal);
  EXPECT_LENGTH(2, kExprCallFunction);
  EXPECT_LENGTH(2, kExprCallIndirect);
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

typedef ZoneVector<LocalType> LocalTypeMap;

class LocalDeclDecoderTest : public TestWithZone {
 public:
  v8::internal::AccountingAllocator allocator;

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

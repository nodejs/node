// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/ostreams.h"
#include "src/v8.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/local-decl-encoder.h"
#include "src/wasm/signature-map.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace function_body_decoder_unittest {

#define B1(a) WASM_BLOCK(a)
#define B2(a, b) WASM_BLOCK(a, b)
#define B3(a, b, c) WASM_BLOCK(a, b, c)

#define WASM_IF_OP kExprIf, kLocalVoid
#define WASM_LOOP_OP kExprLoop, kLocalVoid

static const byte kCodeGetLocal0[] = {kExprGetLocal, 0};
static const byte kCodeGetLocal1[] = {kExprGetLocal, 1};
static const byte kCodeSetLocal0[] = {WASM_SET_LOCAL(0, WASM_ZERO)};
static const byte kCodeTeeLocal0[] = {WASM_TEE_LOCAL(0, WASM_ZERO)};

static const ValueType kValueTypes[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                                        kWasmAnyRef};
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

class FunctionBodyDecoderTest : public TestWithZone {
 public:
  using LocalsDecl = std::pair<uint32_t, ValueType>;
  // All features are disabled by default and must be activated with
  // a WASM_FEATURE_SCOPE in individual tests.
  WasmFeatures enabled_features_;

  FunctionBodyDecoderTest() : module(nullptr), local_decls(zone()) {}

  TestSignatures sigs;
  WasmModule* module;
  LocalDeclEncoder local_decls;

  void AddLocals(ValueType type, uint32_t count) {
    local_decls.AddLocals(count, type);
  }

  enum AppendEnd : bool { kAppendEnd, kOmitEnd };

  Vector<const byte> PrepareBytecode(Vector<const byte> code,
                                     AppendEnd append_end) {
    size_t locals_size = local_decls.Size();
    size_t total_size =
        code.size() + locals_size + (append_end == kAppendEnd ? 1 : 0);
    byte* buffer = static_cast<byte*>(zone()->New(total_size));
    // Prepend the local decls to the code.
    local_decls.Emit(buffer);
    // Emit the code.
    if (code.size() > 0) {
      memcpy(buffer + locals_size, code.start(), code.size());
    }
    if (append_end == kAppendEnd) {
      // Append an extra end opcode.
      buffer[total_size - 1] = kExprEnd;
    }

    return {buffer, total_size};
  }

  template <size_t N>
  Vector<const byte> CodeToVector(const byte (&code)[N]) {
    return ArrayVector(code);
  }

  Vector<const byte> CodeToVector(
      const std::initializer_list<const byte>& code) {
    return VectorOf(&*code.begin(), code.size());
  }

  Vector<const byte> CodeToVector(Vector<const byte> vec) { return vec; }

  // Prepends local variable declarations and renders nice error messages for
  // verification failures.
  template <typename Code = std::initializer_list<const byte>>
  void Validate(bool expected_success, FunctionSig* sig, Code&& raw_code,
                AppendEnd append_end = kAppendEnd,
                const char* message = nullptr) {
    Vector<const byte> code =
        PrepareBytecode(CodeToVector(std::forward<Code>(raw_code)), append_end);

    // Validate the code.
    FunctionBody body(sig, 0, code.start(), code.end());
    WasmFeatures unused_detected_features;
    DecodeResult result =
        VerifyWasmCode(zone()->allocator(), enabled_features_, module,
                       &unused_detected_features, body);

    std::ostringstream str;
    if (result.failed()) {
      str << "Verification failed: pc = +" << result.error().offset()
          << ", msg = " << result.error().message();
    } else {
      str << "Verification successed, expected failure";
    }
    EXPECT_EQ(result.ok(), expected_success) << str.str();
    if (result.failed() && message) {
      EXPECT_THAT(result.error().message(), ::testing::HasSubstr(message));
    }
  }

  template <typename Code = std::initializer_list<const byte>>
  void ExpectValidates(FunctionSig* sig, Code&& raw_code,
                       AppendEnd append_end = kAppendEnd,
                       const char* message = nullptr) {
    Validate(true, sig, std::forward<Code>(raw_code), append_end, message);
  }

  template <typename Code = std::initializer_list<const byte>>
  void ExpectFailure(FunctionSig* sig, Code&& raw_code,
                     AppendEnd append_end = kAppendEnd,
                     const char* message = nullptr) {
    Validate(false, sig, std::forward<Code>(raw_code), append_end, message);
  }

  void TestBinop(WasmOpcode opcode, FunctionSig* success) {
    // op(local[0], local[1])
    byte code[] = {WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
    ExpectValidates(success, code);

    // Try all combinations of return and parameter types.
    for (size_t i = 0; i < arraysize(kValueTypes); i++) {
      for (size_t j = 0; j < arraysize(kValueTypes); j++) {
        for (size_t k = 0; k < arraysize(kValueTypes); k++) {
          ValueType types[] = {kValueTypes[i], kValueTypes[j], kValueTypes[k]};
          if (types[0] != success->GetReturn(0) ||
              types[1] != success->GetParam(0) ||
              types[2] != success->GetParam(1)) {
            // Test signature mismatch.
            FunctionSig sig(1, 2, types);
            ExpectFailure(&sig, code);
          }
        }
      }
    }
  }

  void TestUnop(WasmOpcode opcode, FunctionSig* success) {
    TestUnop(opcode, success->GetReturn(), success->GetParam(0));
  }

  void TestUnop(WasmOpcode opcode, ValueType ret_type, ValueType param_type) {
    // Return(op(local[0]))
    byte code[] = {WASM_UNOP(opcode, WASM_GET_LOCAL(0))};
    {
      ValueType types[] = {ret_type, param_type};
      FunctionSig sig(1, 1, types);
      ExpectValidates(&sig, code);
    }

    // Try all combinations of return and parameter types.
    for (size_t i = 0; i < arraysize(kValueTypes); i++) {
      for (size_t j = 0; j < arraysize(kValueTypes); j++) {
        ValueType types[] = {kValueTypes[i], kValueTypes[j]};
        if (types[0] != ret_type || types[1] != param_type) {
          // Test signature mismatch.
          FunctionSig sig(1, 1, types);
          ExpectFailure(&sig, code);
        }
      }
    }
  }
};

namespace {

class EnableBoolScope {
 public:
  bool prev_;
  bool* ptr_;
  explicit EnableBoolScope(bool* ptr) : prev_(*ptr), ptr_(ptr) { *ptr = true; }
  ~EnableBoolScope() { *ptr_ = prev_; }
};

#define WASM_FEATURE_SCOPE(feat) \
  EnableBoolScope feat##_scope(&this->enabled_features_.feat);

constexpr size_t kMaxByteSizedLeb128 = 127;

// A helper for tests that require a module environment for functions,
// globals, or memories.
class TestModuleBuilder {
 public:
  explicit TestModuleBuilder(ModuleOrigin origin = kWasmOrigin) {
    mod.origin = origin;
  }
  byte AddGlobal(ValueType type, bool mutability = true) {
    mod.globals.push_back(
        {type, mutability, WasmInitExpr(), {0}, false, false});
    CHECK_LE(mod.globals.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.globals.size() - 1);
  }
  byte AddSignature(FunctionSig* sig) {
    mod.signatures.push_back(sig);
    CHECK_LE(mod.signatures.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.signatures.size() - 1);
  }
  byte AddFunction(FunctionSig* sig) {
    mod.functions.push_back({sig,      // sig
                             0,        // func_index
                             0,        // sig_index
                             {0, 0},   // code
                             false,    // import
                             false});  // export
    CHECK_LE(mod.functions.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.functions.size() - 1);
  }
  byte AddImport(FunctionSig* sig) {
    byte result = AddFunction(sig);
    mod.functions[result].imported = true;
    return result;
  }
  byte AddException(WasmExceptionSig* sig) {
    mod.exceptions.emplace_back(sig);
    CHECK_LE(mod.signatures.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.exceptions.size() - 1);
  }

  byte AddTable(ValueType type, uint32_t initial_size, bool has_maximum_size,
                uint32_t maximum_size) {
    CHECK(type == kWasmAnyRef || type == kWasmAnyFunc);
    mod.tables.emplace_back();
    WasmTable& table = mod.tables.back();
    table.type = type;
    table.initial_size = initial_size;
    table.has_maximum_size = has_maximum_size;
    table.maximum_size = maximum_size;
    return static_cast<byte>(mod.tables.size() - 1);
  }

  void InitializeMemory() {
    mod.has_memory = true;
    mod.initial_pages = 1;
    mod.maximum_pages = 100;
  }

  void InitializeTable() { mod.tables.emplace_back(); }

  byte AddPassiveElementSegment() {
    mod.elem_segments.emplace_back();
    return static_cast<byte>(mod.elem_segments.size() - 1);
  }

  // Set the number of data segments as declared by the DataCount section.
  void SetDataSegmentCount(uint32_t data_segment_count) {
    // The Data section occurs after the Code section, so we don't need to
    // update mod.data_segments, as it is always empty.
    mod.num_declared_data_segments = data_segment_count;
  }

  WasmModule* module() { return &mod; }

 private:
  WasmModule mod;
};
}  // namespace

TEST_F(FunctionBodyDecoderTest, Int32Const1) {
  byte code[] = {kExprI32Const, 0};
  for (int i = -64; i <= 63; i++) {
    code[1] = static_cast<byte>(i & 0x7F);
    ExpectValidates(sigs.i_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, RefNull) {
  WASM_FEATURE_SCOPE(anyref);
  ExpectValidates(sigs.r_v(), {kExprRefNull});
}

TEST_F(FunctionBodyDecoderTest, EmptyFunction) {
  ExpectValidates(sigs.v_v(), {});
  ExpectFailure(sigs.i_i(), {});
}

TEST_F(FunctionBodyDecoderTest, IncompleteIf1) {
  byte code[] = {kExprIf};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, Int32Const_fallthru) {
  ExpectValidates(sigs.i_i(), {WASM_I32V_1(0)});
}

TEST_F(FunctionBodyDecoderTest, Int32Const_fallthru2) {
  ExpectFailure(sigs.i_i(), {WASM_I32V_1(0), WASM_I32V_1(1)});
}

TEST_F(FunctionBodyDecoderTest, Int32Const) {
  const int kInc = 4498211;
  for (int32_t i = kMinInt; i < kMaxInt - kInc; i = i + kInc) {
    // TODO(binji): expand test for other sized int32s; 1 through 5 bytes.
    ExpectValidates(sigs.i_i(), {WASM_I32V(i)});
  }
}

TEST_F(FunctionBodyDecoderTest, Int64Const) {
  const int kInc = 4498211;
  for (int32_t i = kMinInt; i < kMaxInt - kInc; i = i + kInc) {
    ExpectValidates(sigs.l_l(),
                    {WASM_I64V((static_cast<uint64_t>(i) << 32) | i)});
  }
}

TEST_F(FunctionBodyDecoderTest, Float32Const) {
  byte code[] = {kExprF32Const, 0, 0, 0, 0};
  Address ptr = reinterpret_cast<Address>(code + 1);
  for (int i = 0; i < 30; i++) {
    WriteLittleEndianValue<float>(ptr, i * -7.75f);
    ExpectValidates(sigs.f_ff(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, Float64Const) {
  byte code[] = {kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0};
  Address ptr = reinterpret_cast<Address>(code + 1);
  for (int i = 0; i < 30; i++) {
    WriteLittleEndianValue<double>(ptr, i * 33.45);
    ExpectValidates(sigs.d_dd(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, Int32Const_off_end) {
  byte code[] = {kExprI32Const, 0xAA, 0xBB, 0xCC, 0x44};

  for (size_t size = 1; size <= 4; ++size) {
    ExpectFailure(sigs.i_i(), VectorOf(code, size), kAppendEnd);
    // Should also fail without the trailing 'end' opcode.
    ExpectFailure(sigs.i_i(), VectorOf(code, size), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, GetLocal0_param) {
  ExpectValidates(sigs.i_i(), kCodeGetLocal0);
}

TEST_F(FunctionBodyDecoderTest, GetLocal0_local) {
  AddLocals(kWasmI32, 1);
  ExpectValidates(sigs.i_v(), kCodeGetLocal0);
}

TEST_F(FunctionBodyDecoderTest, TooManyLocals) {
  AddLocals(kWasmI32, 4034986500);
  ExpectFailure(sigs.i_v(), kCodeGetLocal0);
}

TEST_F(FunctionBodyDecoderTest, GetLocal0_param_n) {
  FunctionSig* array[] = {sigs.i_i(), sigs.i_ii(), sigs.i_iii()};

  for (size_t i = 0; i < arraysize(array); i++) {
    ExpectValidates(array[i], kCodeGetLocal0);
  }
}

TEST_F(FunctionBodyDecoderTest, GetLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kWasmI32, 1);
    for (byte j = 0; j < i; j++) {
      ExpectValidates(sigs.i_v(), {kExprGetLocal, j});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, GetLocal0_fail_no_params) {
  ExpectFailure(sigs.i_v(), kCodeGetLocal0);
}

TEST_F(FunctionBodyDecoderTest, GetLocal1_fail_no_locals) {
  ExpectFailure(sigs.i_i(), kCodeGetLocal1);
}

TEST_F(FunctionBodyDecoderTest, GetLocal_off_end) {
  ExpectFailure(sigs.i_i(), {kExprGetLocal});
}

TEST_F(FunctionBodyDecoderTest, NumLocalBelowLimit) {
  AddLocals(kWasmI32, kV8MaxWasmFunctionLocals - 1);
  ExpectValidates(sigs.v_v(), {WASM_NOP});
}

TEST_F(FunctionBodyDecoderTest, NumLocalAtLimit) {
  AddLocals(kWasmI32, kV8MaxWasmFunctionLocals);
  ExpectValidates(sigs.v_v(), {WASM_NOP});
}

TEST_F(FunctionBodyDecoderTest, NumLocalAboveLimit) {
  AddLocals(kWasmI32, kV8MaxWasmFunctionLocals + 1);
  ExpectFailure(sigs.v_v(), {WASM_NOP});
}

TEST_F(FunctionBodyDecoderTest, GetLocal_varint) {
  const int kMaxLocals = kV8MaxWasmFunctionLocals - 1;
  AddLocals(kWasmI32, kMaxLocals);

  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_1(66)});
  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_2(7777)});
  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_3(8888)});
  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_4(9999)});

  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_5(kMaxLocals - 1)});

  ExpectFailure(sigs.i_i(), {kExprGetLocal, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_4(kMaxLocals - 1)});
  ExpectValidates(sigs.i_i(), {kExprGetLocal, U32V_4(kMaxLocals)});
  ExpectFailure(sigs.i_i(), {kExprGetLocal, U32V_4(kMaxLocals + 1)});

  ExpectFailure(sigs.i_v(), {kExprGetLocal, U32V_4(kMaxLocals)});
  ExpectFailure(sigs.i_v(), {kExprGetLocal, U32V_4(kMaxLocals + 1)});
}

TEST_F(FunctionBodyDecoderTest, GetLocal_toomany) {
  AddLocals(kWasmI32, kV8MaxWasmFunctionLocals - 100);
  AddLocals(kWasmI32, 100);

  ExpectValidates(sigs.i_v(), {kExprGetLocal, U32V_1(66)});
  ExpectFailure(sigs.i_i(), {kExprGetLocal, U32V_1(66)});
}

TEST_F(FunctionBodyDecoderTest, Binops_off_end) {
  byte code1[] = {0};  // [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code1[0] = kInt32BinopOpcodes[i];
    ExpectFailure(sigs.i_i(), code1);
  }

  byte code3[] = {kExprGetLocal, 0, 0};  // [expr] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code3[2] = kInt32BinopOpcodes[i];
    ExpectFailure(sigs.i_i(), code3);
  }

  byte code4[] = {kExprGetLocal, 0, 0, 0};  // [expr] [opcode] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code4[2] = kInt32BinopOpcodes[i];
    code4[3] = kInt32BinopOpcodes[i];
    ExpectFailure(sigs.i_i(), code4);
  }
}

TEST_F(FunctionBodyDecoderTest, BinopsAcrossBlock1) {
  ExpectFailure(sigs.i_i(), {WASM_ZERO, kExprBlock, kLocalI32, WASM_ZERO,
                             kExprI32Add, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, BinopsAcrossBlock2) {
  ExpectFailure(sigs.i_i(), {WASM_ZERO, WASM_ZERO, kExprBlock, kLocalI32,
                             kExprI32Add, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, BinopsAcrossBlock3) {
  ExpectFailure(sigs.i_i(), {WASM_ZERO, WASM_ZERO, kExprIf, kLocalI32,
                             kExprI32Add, kExprElse, kExprI32Add, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, Nop) {
  ExpectValidates(sigs.v_v(), {kExprNop});
}

TEST_F(FunctionBodyDecoderTest, SetLocal0_void) {
  ExpectFailure(sigs.i_i(), {WASM_SET_LOCAL(0, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, SetLocal0_param) {
  ExpectFailure(sigs.i_i(), kCodeSetLocal0);
  ExpectFailure(sigs.f_ff(), kCodeSetLocal0);
  ExpectFailure(sigs.d_dd(), kCodeSetLocal0);
}

TEST_F(FunctionBodyDecoderTest, TeeLocal0_param) {
  ExpectValidates(sigs.i_i(), kCodeTeeLocal0);
  ExpectFailure(sigs.f_ff(), kCodeTeeLocal0);
  ExpectFailure(sigs.d_dd(), kCodeTeeLocal0);
}

TEST_F(FunctionBodyDecoderTest, SetLocal0_local) {
  ExpectFailure(sigs.i_v(), kCodeSetLocal0);
  ExpectFailure(sigs.v_v(), kCodeSetLocal0);
  AddLocals(kWasmI32, 1);
  ExpectFailure(sigs.i_v(), kCodeSetLocal0);
  ExpectValidates(sigs.v_v(), kCodeSetLocal0);
}

TEST_F(FunctionBodyDecoderTest, TeeLocal0_local) {
  ExpectFailure(sigs.i_v(), kCodeTeeLocal0);
  AddLocals(kWasmI32, 1);
  ExpectValidates(sigs.i_v(), kCodeTeeLocal0);
}

TEST_F(FunctionBodyDecoderTest, TeeLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kWasmI32, 1);
    for (byte j = 0; j < i; j++) {
      ExpectFailure(sigs.v_v(), {WASM_TEE_LOCAL(j, WASM_I32V_1(i))});
      ExpectValidates(sigs.i_i(), {WASM_TEE_LOCAL(j, WASM_I32V_1(i))});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BlockN) {
  constexpr size_t kMaxSize = 200;
  byte buffer[kMaxSize + 3];

  for (size_t i = 0; i <= kMaxSize; i++) {
    memset(buffer, kExprNop, sizeof(buffer));
    buffer[0] = kExprBlock;
    buffer[1] = kLocalVoid;
    buffer[i + 2] = kExprEnd;
    ExpectValidates(sigs.v_i(), VectorOf(buffer, i + 3), kAppendEnd);
  }
}

#define WASM_EMPTY_BLOCK kExprBlock, kLocalVoid, kExprEnd

TEST_F(FunctionBodyDecoderTest, Block0) {
  ExpectValidates(sigs.v_v(), {WASM_EMPTY_BLOCK});
  ExpectFailure(sigs.i_i(), {WASM_EMPTY_BLOCK});
}

TEST_F(FunctionBodyDecoderTest, Block0_fallthru1) {
  ExpectValidates(sigs.v_v(), {WASM_BLOCK(WASM_EMPTY_BLOCK)});
  ExpectFailure(sigs.i_i(), {WASM_BLOCK(WASM_EMPTY_BLOCK)});
}

TEST_F(FunctionBodyDecoderTest, Block0Block0) {
  ExpectValidates(sigs.v_v(), {WASM_EMPTY_BLOCK, WASM_EMPTY_BLOCK});
  ExpectFailure(sigs.i_i(), {WASM_EMPTY_BLOCK, WASM_EMPTY_BLOCK});
}

TEST_F(FunctionBodyDecoderTest, Block0_end) {
  ExpectFailure(sigs.v_v(), {WASM_EMPTY_BLOCK, kExprEnd});
}

#undef WASM_EMPTY_BLOCK

TEST_F(FunctionBodyDecoderTest, Block1) {
  byte code[] = {WASM_BLOCK_I(WASM_GET_LOCAL(0))};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.v_i(), code);
  ExpectFailure(sigs.d_dd(), code);
  ExpectFailure(sigs.i_f(), code);
  ExpectFailure(sigs.i_d(), code);
}

TEST_F(FunctionBodyDecoderTest, Block1_i) {
  byte code[] = {WASM_BLOCK_I(WASM_ZERO)};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.f_ff(), code);
  ExpectFailure(sigs.d_dd(), code);
  ExpectFailure(sigs.l_ll(), code);
}

TEST_F(FunctionBodyDecoderTest, Block1_f) {
  byte code[] = {WASM_BLOCK_F(WASM_F32(0))};
  ExpectFailure(sigs.i_i(), code);
  ExpectValidates(sigs.f_ff(), code);
  ExpectFailure(sigs.d_dd(), code);
  ExpectFailure(sigs.l_ll(), code);
}

TEST_F(FunctionBodyDecoderTest, Block1_continue) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, Block1_br) {
  ExpectValidates(sigs.v_v(), {B1(WASM_BR(0))});
  ExpectValidates(sigs.v_v(), {B1(WASM_BR(1))});
  ExpectFailure(sigs.v_v(), {B1(WASM_BR(2))});
}

TEST_F(FunctionBodyDecoderTest, Block2_br) {
  ExpectValidates(sigs.v_v(), {B2(WASM_NOP, WASM_BR(0))});
  ExpectValidates(sigs.v_v(), {B2(WASM_BR(0), WASM_NOP)});
  ExpectValidates(sigs.v_v(), {B2(WASM_BR(0), WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, Block2) {
  ExpectFailure(sigs.i_i(), {WASM_BLOCK(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_i(), {WASM_BLOCK_I(WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_NOP, WASM_ZERO)});
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_ZERO, WASM_NOP)});
  ExpectFailure(sigs.i_i(), {WASM_BLOCK_I(WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Block2b) {
  byte code[] = {WASM_BLOCK_I(WASM_SET_LOCAL(0, WASM_ZERO), WASM_ZERO)};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.f_ff(), code);
}

TEST_F(FunctionBodyDecoderTest, Block2_fallthru) {
  ExpectValidates(sigs.i_i(), {B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                  WASM_SET_LOCAL(0, WASM_ZERO)),
                               WASM_I32V_1(23)});
}

TEST_F(FunctionBodyDecoderTest, Block3) {
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_SET_LOCAL(0, WASM_ZERO),
                                            WASM_SET_LOCAL(0, WASM_ZERO),
                                            WASM_I32V_1(11))});
}

TEST_F(FunctionBodyDecoderTest, Block5) {
  ExpectFailure(sigs.v_i(), {WASM_BLOCK(WASM_ZERO)});

  ExpectFailure(sigs.v_i(), {WASM_BLOCK(WASM_ZERO, WASM_ZERO)});

  ExpectFailure(sigs.v_i(), {WASM_BLOCK(WASM_ZERO, WASM_ZERO, WASM_ZERO)});

  ExpectFailure(sigs.v_i(),
                {WASM_BLOCK(WASM_ZERO, WASM_ZERO, WASM_ZERO, WASM_ZERO)});

  ExpectFailure(sigs.v_i(), {WASM_BLOCK(WASM_ZERO, WASM_ZERO, WASM_ZERO,
                                        WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, BlockType) {
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_GET_LOCAL(0))});
  ExpectValidates(sigs.l_l(), {WASM_BLOCK_L(WASM_GET_LOCAL(0))});
  ExpectValidates(sigs.f_f(), {WASM_BLOCK_F(WASM_GET_LOCAL(0))});
  ExpectValidates(sigs.d_d(), {WASM_BLOCK_D(WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, BlockType_fail) {
  ExpectFailure(sigs.i_i(), {WASM_BLOCK_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.i_i(), {WASM_BLOCK_F(WASM_F32(0.0))});
  ExpectFailure(sigs.i_i(), {WASM_BLOCK_D(WASM_F64(1.1))});

  ExpectFailure(sigs.l_l(), {WASM_BLOCK_I(WASM_ZERO)});
  ExpectFailure(sigs.l_l(), {WASM_BLOCK_F(WASM_F32(0.0))});
  ExpectFailure(sigs.l_l(), {WASM_BLOCK_D(WASM_F64(1.1))});

  ExpectFailure(sigs.f_ff(), {WASM_BLOCK_I(WASM_ZERO)});
  ExpectFailure(sigs.f_ff(), {WASM_BLOCK_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.f_ff(), {WASM_BLOCK_D(WASM_F64(1.1))});

  ExpectFailure(sigs.d_dd(), {WASM_BLOCK_I(WASM_ZERO)});
  ExpectFailure(sigs.d_dd(), {WASM_BLOCK_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.d_dd(), {WASM_BLOCK_F(WASM_F32(0.0))});
}

TEST_F(FunctionBodyDecoderTest, BlockF32) {
  static const byte code[] = {WASM_BLOCK_F(kExprF32Const, 0, 0, 0, 0)};
  ExpectValidates(sigs.f_ff(), code);
  ExpectFailure(sigs.i_i(), code);
  ExpectFailure(sigs.d_dd(), code);
}

TEST_F(FunctionBodyDecoderTest, BlockN_off_end) {
  byte code[] = {WASM_BLOCK(kExprNop, kExprNop, kExprNop, kExprNop)};
  ExpectValidates(sigs.v_v(), code);
  for (size_t i = 1; i < arraysize(code); i++) {
    ExpectFailure(sigs.v_v(), VectorOf(code, i), kAppendEnd);
    ExpectFailure(sigs.v_v(), VectorOf(code, i), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, Block2_continue) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_NOP, WASM_BR(0))});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_NOP, WASM_BR(1))});
  ExpectFailure(sigs.v_v(), {WASM_LOOP(WASM_NOP, WASM_BR(2))});
}

TEST_F(FunctionBodyDecoderTest, Block3_continue) {
  ExpectValidates(sigs.v_v(), {B1(WASM_LOOP(WASM_NOP, WASM_BR(0)))});
  ExpectValidates(sigs.v_v(), {B1(WASM_LOOP(WASM_NOP, WASM_BR(1)))});
  ExpectValidates(sigs.v_v(), {B1(WASM_LOOP(WASM_NOP, WASM_BR(2)))});
  ExpectFailure(sigs.v_v(), {B1(WASM_LOOP(WASM_NOP, WASM_BR(3)))});
}

TEST_F(FunctionBodyDecoderTest, NestedBlock_return) {
  ExpectValidates(sigs.i_i(), {B1(B1(WASM_RETURN1(WASM_ZERO))), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, BlockBrBinop) {
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_AND(WASM_BLOCK_I(WASM_BRV(0, WASM_I32V_1(1))),
                                WASM_I32V_1(2))});
}

TEST_F(FunctionBodyDecoderTest, If_empty1) {
  ExpectValidates(sigs.v_v(), {WASM_ZERO, WASM_IF_OP, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, If_empty2) {
  ExpectValidates(sigs.v_v(), {WASM_ZERO, WASM_IF_OP, kExprElse, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, If_empty3) {
  ExpectValidates(sigs.v_v(),
                  {WASM_ZERO, WASM_IF_OP, WASM_NOP, kExprElse, kExprEnd});
  ExpectFailure(sigs.v_v(),
                {WASM_ZERO, WASM_IF_OP, WASM_ZERO, kExprElse, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, If_empty4) {
  ExpectValidates(sigs.v_v(),
                  {WASM_ZERO, WASM_IF_OP, kExprElse, WASM_NOP, kExprEnd});
  ExpectFailure(sigs.v_v(),
                {WASM_ZERO, WASM_IF_OP, kExprElse, WASM_ZERO, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, If_empty_stack) {
  byte code[] = {kExprIf};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, If_incomplete1) {
  byte code[] = {kExprI32Const, 0, kExprIf};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, If_incomplete2) {
  byte code[] = {kExprI32Const, 0, kExprIf, kExprNop};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, If_else_else) {
  byte code[] = {kExprI32Const, 0, WASM_IF_OP, kExprElse, kExprElse, kExprEnd};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, IfEmpty) {
  ExpectValidates(sigs.v_i(), {kExprGetLocal, 0, WASM_IF_OP, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, IfSet) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO))});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO),
                                WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfElseEmpty) {
  ExpectValidates(sigs.v_i(),
                  {WASM_GET_LOCAL(0), WASM_IF_OP, kExprElse, kExprEnd});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfElseUnreachable1) {
  ExpectValidates(
      sigs.i_i(),
      {WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_UNREACHABLE, WASM_GET_LOCAL(0))});
  ExpectValidates(
      sigs.i_i(),
      {WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_UNREACHABLE)});
}

TEST_F(FunctionBodyDecoderTest, IfElseUnreachable2) {
  static const byte code[] = {
      WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_UNREACHABLE, WASM_GET_LOCAL(0))};

  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType types[] = {kWasmI32, kValueTypes[i]};
    FunctionSig sig(1, 1, types);

    Validate(kValueTypes[i] == kWasmI32, &sig, code);
  }
}

TEST_F(FunctionBodyDecoderTest, OneArmedIfWithArity) {
  static const byte code[] = {WASM_ZERO, kExprIf, kLocalI32, WASM_ONE,
                              kExprEnd};
  ExpectFailure(sigs.i_v(), code, kAppendEnd,
                "start-arity and end-arity of one-armed if must match");
}

TEST_F(FunctionBodyDecoderTest, IfBreak) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0), WASM_BR(0))});
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0), WASM_BR(1))});
  ExpectFailure(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0), WASM_BR(2))});
}

TEST_F(FunctionBodyDecoderTest, IfElseBreak) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(0))});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(1))});
  ExpectFailure(sigs.v_i(),
                {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_BR(2))});
}

TEST_F(FunctionBodyDecoderTest, Block_else) {
  byte code[] = {kExprI32Const, 0, kExprBlock, kExprElse, kExprEnd};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, IfNop) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0), WASM_NOP)});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, If_end) {
  ExpectValidates(sigs.v_i(), {kExprGetLocal, 0, WASM_IF_OP, kExprEnd});
  ExpectFailure(sigs.v_i(), {kExprGetLocal, 0, WASM_IF_OP, kExprEnd, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, If_falloff1) {
  ExpectFailure(sigs.v_i(), {kExprGetLocal, 0, kExprIf});
  ExpectFailure(sigs.v_i(), {kExprGetLocal, 0, WASM_IF_OP});
  ExpectFailure(sigs.v_i(),
                {kExprGetLocal, 0, WASM_IF_OP, kExprNop, kExprElse});
}

TEST_F(FunctionBodyDecoderTest, IfElseNop) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO),
                                WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfBlock1) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0),
                                B1(WASM_SET_LOCAL(0, WASM_ZERO)), WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfBlock1b) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0),
                                       B1(WASM_SET_LOCAL(0, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, IfBlock2a) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0),
                                       B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                          WASM_SET_LOCAL(0, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, IfBlock2b) {
  ExpectValidates(sigs.v_i(), {WASM_IF_ELSE(WASM_GET_LOCAL(0),
                                            B2(WASM_SET_LOCAL(0, WASM_ZERO),
                                               WASM_SET_LOCAL(0, WASM_ZERO)),
                                            WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfElseSet) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_ZERO),
                                WASM_SET_LOCAL(0, WASM_I32V_1(1)))});
}

TEST_F(FunctionBodyDecoderTest, Loop0) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP_OP, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, Loop1) {
  static const byte code[] = {WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO))};
  ExpectValidates(sigs.v_i(), code);
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.f_ff(), code);
}

TEST_F(FunctionBodyDecoderTest, Loop2) {
  ExpectValidates(sigs.v_i(), {WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO),
                                         WASM_SET_LOCAL(0, WASM_ZERO))});
}

TEST_F(FunctionBodyDecoderTest, Loop1_continue) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, Loop1_break) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(1))});
}

TEST_F(FunctionBodyDecoderTest, Loop2_continue) {
  ExpectValidates(sigs.v_i(),
                  {WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO), WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, Loop2_break) {
  ExpectValidates(sigs.v_i(),
                  {WASM_LOOP(WASM_SET_LOCAL(0, WASM_ZERO), WASM_BR(1))});
}

TEST_F(FunctionBodyDecoderTest, InfiniteLoop1) {
  ExpectValidates(sigs.i_i(), {WASM_LOOP(WASM_BR(0)), WASM_ZERO});
  ExpectValidates(sigs.i_i(), {WASM_LOOP(WASM_BR(0)), WASM_ZERO});
  ExpectValidates(sigs.i_i(), {WASM_LOOP_I(WASM_BRV(1, WASM_ZERO))});
}

TEST_F(FunctionBodyDecoderTest, InfiniteLoop2) {
  ExpectFailure(sigs.i_i(), {WASM_LOOP(WASM_BR(0), WASM_ZERO), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, Loop2_unreachable) {
  ExpectValidates(sigs.i_i(), {WASM_LOOP_I(WASM_BR(0), WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, LoopType) {
  ExpectValidates(sigs.i_i(), {WASM_LOOP_I(WASM_GET_LOCAL(0))});
  ExpectValidates(sigs.l_l(), {WASM_LOOP_L(WASM_GET_LOCAL(0))});
  ExpectValidates(sigs.f_f(), {WASM_LOOP_F(WASM_GET_LOCAL(0))});
  ExpectValidates(sigs.d_d(), {WASM_LOOP_D(WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, LoopType_void) {
  ExpectFailure(sigs.v_v(), {WASM_LOOP_I(WASM_ZERO)});
  ExpectFailure(sigs.v_v(), {WASM_LOOP_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.v_v(), {WASM_LOOP_F(WASM_F32(0.0))});
  ExpectFailure(sigs.v_v(), {WASM_LOOP_D(WASM_F64(1.1))});
}

TEST_F(FunctionBodyDecoderTest, LoopType_fail) {
  ExpectFailure(sigs.i_i(), {WASM_LOOP_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.i_i(), {WASM_LOOP_F(WASM_F32(0.0))});
  ExpectFailure(sigs.i_i(), {WASM_LOOP_D(WASM_F64(1.1))});

  ExpectFailure(sigs.l_l(), {WASM_LOOP_I(WASM_ZERO)});
  ExpectFailure(sigs.l_l(), {WASM_LOOP_F(WASM_F32(0.0))});
  ExpectFailure(sigs.l_l(), {WASM_LOOP_D(WASM_F64(1.1))});

  ExpectFailure(sigs.f_ff(), {WASM_LOOP_I(WASM_ZERO)});
  ExpectFailure(sigs.f_ff(), {WASM_LOOP_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.f_ff(), {WASM_LOOP_D(WASM_F64(1.1))});

  ExpectFailure(sigs.d_dd(), {WASM_LOOP_I(WASM_ZERO)});
  ExpectFailure(sigs.d_dd(), {WASM_LOOP_L(WASM_I64V_1(0))});
  ExpectFailure(sigs.d_dd(), {WASM_LOOP_F(WASM_F32(0.0))});
}

TEST_F(FunctionBodyDecoderTest, ReturnVoid1) {
  static const byte code[] = {kExprNop};
  ExpectValidates(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
  ExpectFailure(sigs.i_f(), code);
}

TEST_F(FunctionBodyDecoderTest, ReturnVoid2) {
  static const byte code[] = {WASM_BLOCK(WASM_BR(0))};
  ExpectValidates(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
  ExpectFailure(sigs.i_f(), code);
}

TEST_F(FunctionBodyDecoderTest, ReturnVoid3) {
  ExpectFailure(sigs.v_v(), {kExprI32Const, 0});
  ExpectFailure(sigs.v_v(), {kExprI64Const, 0});
  ExpectFailure(sigs.v_v(), {kExprF32Const, 0, 0, 0, 0});
  ExpectFailure(sigs.v_v(), {kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0});
  ExpectFailure(sigs.v_v(), {kExprRefNull});

  ExpectFailure(sigs.v_i(), {kExprGetLocal, 0});
}

TEST_F(FunctionBodyDecoderTest, Unreachable1) {
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, WASM_UNREACHABLE});
  ExpectValidates(sigs.i_i(), {WASM_UNREACHABLE, WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, Unreachable2) {
  ExpectFailure(sigs.v_v(), {B2(WASM_UNREACHABLE, WASM_ZERO)});
  ExpectFailure(sigs.v_v(), {B2(WASM_BR(0), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, UnreachableLoop1) {
  ExpectFailure(sigs.v_v(), {WASM_LOOP(WASM_UNREACHABLE, WASM_ZERO)});
  ExpectFailure(sigs.v_v(), {WASM_LOOP(WASM_BR(0), WASM_ZERO)});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_UNREACHABLE, WASM_NOP)});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(0), WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, Unreachable_binop1) {
  ExpectValidates(sigs.i_i(), {WASM_I32_AND(WASM_ZERO, WASM_UNREACHABLE)});
  ExpectValidates(sigs.i_i(), {WASM_I32_AND(WASM_UNREACHABLE, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Unreachable_binop2) {
  ExpectValidates(sigs.i_i(), {WASM_I32_AND(WASM_F32(0.0), WASM_UNREACHABLE)});
  ExpectFailure(sigs.i_i(), {WASM_I32_AND(WASM_UNREACHABLE, WASM_F32(0.0))});
}

TEST_F(FunctionBodyDecoderTest, Unreachable_select1) {
  ExpectValidates(sigs.i_i(),
                  {WASM_SELECT(WASM_UNREACHABLE, WASM_ZERO, WASM_ZERO)});
  ExpectValidates(sigs.i_i(),
                  {WASM_SELECT(WASM_ZERO, WASM_UNREACHABLE, WASM_ZERO)});
  ExpectValidates(sigs.i_i(),
                  {WASM_SELECT(WASM_ZERO, WASM_ZERO, WASM_UNREACHABLE)});
}

TEST_F(FunctionBodyDecoderTest, Unreachable_select2) {
  ExpectValidates(sigs.i_i(),
                  {WASM_SELECT(WASM_F32(0.0), WASM_UNREACHABLE, WASM_ZERO)});
  ExpectFailure(sigs.i_i(),
                {WASM_SELECT(WASM_UNREACHABLE, WASM_F32(0.0), WASM_ZERO)});
  ExpectFailure(sigs.i_i(),
                {WASM_SELECT(WASM_UNREACHABLE, WASM_ZERO, WASM_F32(0.0))});
}

TEST_F(FunctionBodyDecoderTest, If1) {
  ExpectValidates(sigs.i_i(), {WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I32V_1(9),
                                              WASM_I32V_1(8))});
  ExpectValidates(sigs.i_i(), {WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I32V_1(9),
                                              WASM_GET_LOCAL(0))});
  ExpectValidates(
      sigs.i_i(),
      {WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_I32V_1(8))});
}

TEST_F(FunctionBodyDecoderTest, If_off_end) {
  static const byte kCode[] = {
      WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0))};
  for (size_t len = 3; len < arraysize(kCode); len++) {
    ExpectFailure(sigs.i_i(), VectorOf(kCode, len), kAppendEnd);
    ExpectFailure(sigs.i_i(), VectorOf(kCode, len), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, If_type1) {
  // float|double ? 1 : 2
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I32V_1(0), WASM_I32V_1(2))};
  ExpectValidates(sigs.i_i(), kCode);
  ExpectFailure(sigs.i_f(), kCode);
  ExpectFailure(sigs.i_d(), kCode);
}

TEST_F(FunctionBodyDecoderTest, If_type2) {
  // 1 ? float|double : 2
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_I32V_1(1), WASM_GET_LOCAL(0), WASM_I32V_1(1))};
  ExpectValidates(sigs.i_i(), kCode);
  ExpectFailure(sigs.i_f(), kCode);
  ExpectFailure(sigs.i_d(), kCode);
}

TEST_F(FunctionBodyDecoderTest, If_type3) {
  // stmt ? 0 : 1
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_NOP, WASM_I32V_1(0), WASM_I32V_1(1))};
  ExpectFailure(sigs.i_i(), kCode);
  ExpectFailure(sigs.i_f(), kCode);
  ExpectFailure(sigs.i_d(), kCode);
}

TEST_F(FunctionBodyDecoderTest, If_type4) {
  // 0 ? stmt : 1
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_NOP, WASM_I32V_1(1))};
  ExpectFailure(sigs.i_i(), kCode);
  ExpectFailure(sigs.i_f(), kCode);
  ExpectFailure(sigs.i_d(), kCode);
}

TEST_F(FunctionBodyDecoderTest, If_type5) {
  // 0 ? 1 : stmt
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_ZERO, WASM_I32V_1(1), WASM_NOP)};
  ExpectFailure(sigs.i_i(), kCode);
  ExpectFailure(sigs.i_f(), kCode);
  ExpectFailure(sigs.i_d(), kCode);
}

TEST_F(FunctionBodyDecoderTest, Int64Local_param) {
  ExpectValidates(sigs.l_l(), kCodeGetLocal0);
}

TEST_F(FunctionBodyDecoderTest, Int64Locals) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kWasmI64, 1);
    for (byte j = 0; j < i; j++) {
      ExpectValidates(sigs.l_v(), {WASM_GET_LOCAL(j)});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Int32Binops) {
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

TEST_F(FunctionBodyDecoderTest, DoubleBinops) {
  TestBinop(kExprF64Add, sigs.d_dd());
  TestBinop(kExprF64Sub, sigs.d_dd());
  TestBinop(kExprF64Mul, sigs.d_dd());
  TestBinop(kExprF64Div, sigs.d_dd());

  TestBinop(kExprF64Eq, sigs.i_dd());
  TestBinop(kExprF64Lt, sigs.i_dd());
  TestBinop(kExprF64Le, sigs.i_dd());
}

TEST_F(FunctionBodyDecoderTest, FloatBinops) {
  TestBinop(kExprF32Add, sigs.f_ff());
  TestBinop(kExprF32Sub, sigs.f_ff());
  TestBinop(kExprF32Mul, sigs.f_ff());
  TestBinop(kExprF32Div, sigs.f_ff());

  TestBinop(kExprF32Eq, sigs.i_ff());
  TestBinop(kExprF32Lt, sigs.i_ff());
  TestBinop(kExprF32Le, sigs.i_ff());
}

TEST_F(FunctionBodyDecoderTest, TypeConversions) {
  TestUnop(kExprI32SConvertF32, kWasmI32, kWasmF32);
  TestUnop(kExprI32SConvertF64, kWasmI32, kWasmF64);
  TestUnop(kExprI32UConvertF32, kWasmI32, kWasmF32);
  TestUnop(kExprI32UConvertF64, kWasmI32, kWasmF64);
  TestUnop(kExprF64SConvertI32, kWasmF64, kWasmI32);
  TestUnop(kExprF64UConvertI32, kWasmF64, kWasmI32);
  TestUnop(kExprF64ConvertF32, kWasmF64, kWasmF32);
  TestUnop(kExprF32SConvertI32, kWasmF32, kWasmI32);
  TestUnop(kExprF32UConvertI32, kWasmF32, kWasmI32);
  TestUnop(kExprF32ConvertF64, kWasmF32, kWasmF64);
}

TEST_F(FunctionBodyDecoderTest, MacrosStmt) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  ExpectValidates(sigs.v_i(), {WASM_SET_LOCAL(0, WASM_I32V_3(87348))});
  ExpectValidates(
      sigs.v_i(),
      {WASM_STORE_MEM(MachineType::Int32(), WASM_I32V_1(24), WASM_I32V_1(40))});
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_GET_LOCAL(0), WASM_NOP)});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.v_v(), {WASM_NOP});
  ExpectValidates(sigs.v_v(), {B1(WASM_NOP)});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_NOP)});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, MacrosContinue) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_CONTINUE(0))});
}

TEST_F(FunctionBodyDecoderTest, MacrosVariadic) {
  ExpectValidates(sigs.v_v(), {B2(WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.v_v(), {B3(WASM_NOP, WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_NOP, WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, MacrosNestedBlocks) {
  ExpectValidates(sigs.v_v(), {B2(WASM_NOP, B2(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.v_v(), {B3(WASM_NOP,                   // --
                                  B2(WASM_NOP, WASM_NOP),     // --
                                  B2(WASM_NOP, WASM_NOP))});  // --
  ExpectValidates(sigs.v_v(), {B1(B1(B2(WASM_NOP, WASM_NOP)))});
}

TEST_F(FunctionBodyDecoderTest, MultipleReturn) {
  static ValueType kIntTypes5[] = {kWasmI32, kWasmI32, kWasmI32, kWasmI32,
                                   kWasmI32};
  FunctionSig sig_ii_v(2, 0, kIntTypes5);
  ExpectValidates(&sig_ii_v, {WASM_RETURNN(2, WASM_ZERO, WASM_ONE)});
  ExpectFailure(&sig_ii_v, {WASM_RETURNN(1, WASM_ZERO)});

  FunctionSig sig_iii_v(3, 0, kIntTypes5);
  ExpectValidates(&sig_iii_v,
                  {WASM_RETURNN(3, WASM_ZERO, WASM_ONE, WASM_I32V_1(44))});
  ExpectFailure(&sig_iii_v, {WASM_RETURNN(2, WASM_ZERO, WASM_ONE)});
}

TEST_F(FunctionBodyDecoderTest, MultipleReturn_fallthru) {
  static ValueType kIntTypes5[] = {kWasmI32, kWasmI32, kWasmI32, kWasmI32,
                                   kWasmI32};
  FunctionSig sig_ii_v(2, 0, kIntTypes5);

  ExpectValidates(&sig_ii_v, {WASM_ZERO, WASM_ONE});
  ExpectFailure(&sig_ii_v, {WASM_ZERO});

  FunctionSig sig_iii_v(3, 0, kIntTypes5);
  ExpectValidates(&sig_iii_v, {WASM_ZERO, WASM_ONE, WASM_I32V_1(44)});
  ExpectFailure(&sig_iii_v, {WASM_ZERO, WASM_ONE});
}

TEST_F(FunctionBodyDecoderTest, MacrosInt32) {
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_I32V_1(12))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I32V_1(13))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_MUL(WASM_GET_LOCAL(0), WASM_I32V_1(14))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I32V_1(15))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_I32V_1(16))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_I32V_1(17))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_I32V_1(18))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_AND(WASM_GET_LOCAL(0), WASM_I32V_1(19))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_IOR(WASM_GET_LOCAL(0), WASM_I32V_1(20))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_XOR(WASM_GET_LOCAL(0), WASM_I32V_1(21))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SHL(WASM_GET_LOCAL(0), WASM_I32V_1(22))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SHR(WASM_GET_LOCAL(0), WASM_I32V_1(23))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SAR(WASM_GET_LOCAL(0), WASM_I32V_1(24))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_ROR(WASM_GET_LOCAL(0), WASM_I32V_1(24))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_ROL(WASM_GET_LOCAL(0), WASM_I32V_1(24))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I32V_1(25))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_NE(WASM_GET_LOCAL(0), WASM_I32V_1(25))});

  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LTS(WASM_GET_LOCAL(0), WASM_I32V_1(26))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LES(WASM_GET_LOCAL(0), WASM_I32V_1(27))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LTU(WASM_GET_LOCAL(0), WASM_I32V_1(28))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LEU(WASM_GET_LOCAL(0), WASM_I32V_1(29))});

  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GTS(WASM_GET_LOCAL(0), WASM_I32V_1(26))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GES(WASM_GET_LOCAL(0), WASM_I32V_1(27))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GTU(WASM_GET_LOCAL(0), WASM_I32V_1(28))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GEU(WASM_GET_LOCAL(0), WASM_I32V_1(29))});
}

TEST_F(FunctionBodyDecoderTest, MacrosInt64) {
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_ADD(WASM_GET_LOCAL(0), WASM_I64V_1(12))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SUB(WASM_GET_LOCAL(0), WASM_I64V_1(13))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_MUL(WASM_GET_LOCAL(0), WASM_I64V_1(14))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_DIVS(WASM_GET_LOCAL(0), WASM_I64V_1(15))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_DIVU(WASM_GET_LOCAL(0), WASM_I64V_1(16))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_REMS(WASM_GET_LOCAL(0), WASM_I64V_1(17))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_REMU(WASM_GET_LOCAL(0), WASM_I64V_1(18))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_AND(WASM_GET_LOCAL(0), WASM_I64V_1(19))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_IOR(WASM_GET_LOCAL(0), WASM_I64V_1(20))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_XOR(WASM_GET_LOCAL(0), WASM_I64V_1(21))});

  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SHL(WASM_GET_LOCAL(0), WASM_I64V_1(22))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SHR(WASM_GET_LOCAL(0), WASM_I64V_1(23))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SAR(WASM_GET_LOCAL(0), WASM_I64V_1(24))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_ROR(WASM_GET_LOCAL(0), WASM_I64V_1(24))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_ROL(WASM_GET_LOCAL(0), WASM_I64V_1(24))});

  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LTS(WASM_GET_LOCAL(0), WASM_I64V_1(26))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LES(WASM_GET_LOCAL(0), WASM_I64V_1(27))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LTU(WASM_GET_LOCAL(0), WASM_I64V_1(28))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LEU(WASM_GET_LOCAL(0), WASM_I64V_1(29))});

  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GTS(WASM_GET_LOCAL(0), WASM_I64V_1(26))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GES(WASM_GET_LOCAL(0), WASM_I64V_1(27))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GTU(WASM_GET_LOCAL(0), WASM_I64V_1(28))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GEU(WASM_GET_LOCAL(0), WASM_I64V_1(29))});

  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_EQ(WASM_GET_LOCAL(0), WASM_I64V_1(25))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_NE(WASM_GET_LOCAL(0), WASM_I64V_1(25))});
}

TEST_F(FunctionBodyDecoderTest, AllSimpleExpressions) {
  WASM_FEATURE_SCOPE(se);
  WASM_FEATURE_SCOPE(anyref);
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

TEST_F(FunctionBodyDecoderTest, MemorySize) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  byte code[] = {kExprMemorySize, 0};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.f_ff(), code);
}

TEST_F(FunctionBodyDecoderTest, LoadMemOffset) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  for (int offset = 0; offset < 128; offset += 7) {
    byte code[] = {kExprI32Const, 0, kExprI32LoadMem, ZERO_ALIGNMENT,
                   static_cast<byte>(offset)};
    ExpectValidates(sigs.i_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, LoadMemAlignment) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
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

  for (size_t i = 0; i < arraysize(values); i++) {
    for (byte alignment = 0; alignment <= 4; alignment++) {
      byte code[] = {WASM_ZERO, static_cast<byte>(values[i].instruction),
                     alignment, ZERO_OFFSET, WASM_DROP};
      Validate(alignment <= values[i].maximum_aligment, sigs.v_i(), code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, StoreMemOffset) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  for (byte offset = 0; offset < 128; offset += 7) {
    byte code[] = {WASM_STORE_MEM_OFFSET(MachineType::Int32(), offset,
                                         WASM_ZERO, WASM_ZERO)};
    ExpectValidates(sigs.v_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, StoreMemOffset_void) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  ExpectFailure(sigs.i_i(), {WASM_STORE_MEM_OFFSET(MachineType::Int32(), 0,
                                                   WASM_ZERO, WASM_ZERO)});
}

#define BYTE0(x) ((x)&0x7F)
#define BYTE1(x) ((x >> 7) & 0x7F)
#define BYTE2(x) ((x >> 14) & 0x7F)
#define BYTE3(x) ((x >> 21) & 0x7F)

#define VARINT1(x) BYTE0(x)
#define VARINT2(x) BYTE0(x) | 0x80, BYTE1(x)
#define VARINT3(x) BYTE0(x) | 0x80, BYTE1(x) | 0x80, BYTE2(x)
#define VARINT4(x) BYTE0(x) | 0x80, BYTE1(x) | 0x80, BYTE2(x) | 0x80, BYTE3(x)

TEST_F(FunctionBodyDecoderTest, LoadMemOffset_varint) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  ExpectValidates(sigs.i_i(),
                  {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT, VARINT1(0x45)});
  ExpectValidates(sigs.i_i(), {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                               VARINT2(0x3999)});
  ExpectValidates(sigs.i_i(), {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                               VARINT3(0x344445)});
  ExpectValidates(sigs.i_i(), {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                               VARINT4(0x36666667)});
}

TEST_F(FunctionBodyDecoderTest, StoreMemOffset_varint) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, VARINT1(0x33)});
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, VARINT2(0x1111)});
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, VARINT3(0x222222)});
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, VARINT4(0x44444444)});
}

#undef BYTE0
#undef BYTE1
#undef BYTE2
#undef BYTE3

#undef VARINT1
#undef VARINT2
#undef VARINT3
#undef VARINT4

TEST_F(FunctionBodyDecoderTest, AllLoadMemCombinations) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType local_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_LOAD_MEM(mem_type, WASM_ZERO)};
      FunctionSig sig(1, 0, &local_type);
      Validate(local_type == ValueTypes::ValueTypeFor(mem_type), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, AllStoreMemCombinations) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType local_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_STORE_MEM(mem_type, WASM_ZERO, WASM_GET_LOCAL(0))};
      FunctionSig sig(0, 1, &local_type);
      Validate(local_type == ValueTypes::ValueTypeFor(mem_type), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, SimpleCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_v());
  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());

  ExpectValidates(sig, {WASM_CALL_FUNCTION0(0)});
  ExpectValidates(sig, {WASM_CALL_FUNCTION(1, WASM_I32V_1(27))});
  ExpectValidates(sig,
                  {WASM_CALL_FUNCTION(2, WASM_I32V_1(37), WASM_I32V_2(77))});
}

TEST_F(FunctionBodyDecoderTest, CallsWithTooFewArguments) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());
  builder.AddFunction(sigs.f_ff());

  ExpectFailure(sig, {WASM_CALL_FUNCTION0(0)});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(1, WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(2, WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, CallsWithMismatchedSigs2) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_i());

  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_I64V_1(17))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_F32(17.1))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_F64(17.1))});
}

TEST_F(FunctionBodyDecoderTest, CallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_f());

  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_F64(37.2))});

  builder.AddFunction(sigs.i_d());

  ExpectFailure(sig, {WASM_CALL_FUNCTION(1, WASM_I32V_1(16))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(1, WASM_I64V_1(16))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(1, WASM_F32(17.6))});
}

TEST_F(FunctionBodyDecoderTest, SimpleReturnCalls) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_v());
  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());

  ExpectValidates(sig, {WASM_RETURN_CALL_FUNCTION0(0)});
  ExpectValidates(sig, {WASM_RETURN_CALL_FUNCTION(1, WASM_I32V_1(27))});
  ExpectValidates(
      sig, {WASM_RETURN_CALL_FUNCTION(2, WASM_I32V_1(37), WASM_I32V_2(77))});
}

TEST_F(FunctionBodyDecoderTest, ReturnCallsWithTooFewArguments) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());
  builder.AddFunction(sigs.f_ff());

  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION0(0)});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(1, WASM_ZERO)});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(2, WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, ReturnCallsWithMismatchedSigs) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(sigs.i_f());
  builder.AddFunction(sigs.f_f());

  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(0, WASM_F64(37.2))});

  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(1, WASM_F64(37.2))});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(1, WASM_F32(37.2))});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(1, WASM_I32V_1(17))});
}

TEST_F(FunctionBodyDecoderTest, SimpleIndirectReturnCalls) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.AddTable(kWasmAnyFunc, 20, true, 30);
  module = builder.module();

  byte f0 = builder.AddSignature(sigs.i_v());
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());

  ExpectValidates(sig, {WASM_RETURN_CALL_INDIRECT0(f0, WASM_ZERO)});
  ExpectValidates(sig,
                  {WASM_RETURN_CALL_INDIRECT(f1, WASM_ZERO, WASM_I32V_1(22))});
  ExpectValidates(sig, {WASM_RETURN_CALL_INDIRECT(
                           f2, WASM_ZERO, WASM_I32V_1(32), WASM_I32V_2(72))});
}

TEST_F(FunctionBodyDecoderTest, IndirectReturnCallsOutOfBounds) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.AddTable(kWasmAnyFunc, 20, false, 20);
  module = builder.module();

  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT0(0, WASM_ZERO)});
  builder.AddSignature(sigs.i_v());
  ExpectValidates(sig, {WASM_RETURN_CALL_INDIRECT0(0, WASM_ZERO)});

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(1, WASM_ZERO, WASM_I32V_1(22))});
  builder.AddSignature(sigs.i_i());
  ExpectValidates(sig,
                  {WASM_RETURN_CALL_INDIRECT(1, WASM_ZERO, WASM_I32V_1(27))});

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(2, WASM_ZERO, WASM_I32V_1(27))});
}

TEST_F(FunctionBodyDecoderTest, IndirectReturnCallsWithMismatchedSigs3) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.InitializeTable();
  module = builder.module();

  byte f0 = builder.AddFunction(sigs.i_f());

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f0, WASM_ZERO, WASM_I32V_1(17))});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f0, WASM_ZERO, WASM_I64V_1(27))});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f0, WASM_ZERO, WASM_F64(37.2))});

  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT0(f0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT0(f0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT0(f0, WASM_F64(37.2))});

  byte f1 = builder.AddFunction(sigs.i_d());

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f1, WASM_ZERO, WASM_I32V_1(16))});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f1, WASM_ZERO, WASM_I64V_1(16))});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f1, WASM_ZERO, WASM_F32(17.6))});
}

TEST_F(FunctionBodyDecoderTest, IndirectReturnCallsWithoutTableCrash) {
  WASM_FEATURE_SCOPE(return_call);

  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  byte f0 = builder.AddSignature(sigs.i_v());
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());

  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT0(f0, WASM_ZERO)});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(f1, WASM_ZERO, WASM_I32V_1(22))});
  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(f2, WASM_ZERO, WASM_I32V_1(32),
                                                WASM_I32V_2(72))});
}

TEST_F(FunctionBodyDecoderTest, IncompleteIndirectReturnCall) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.InitializeTable();
  module = builder.module();

  static byte code[] = {kExprReturnCallIndirect};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, MultiReturn) {
  WASM_FEATURE_SCOPE(mv);
  ValueType storage[] = {kWasmI32, kWasmI32};
  FunctionSig sig_ii_v(2, 0, storage);
  FunctionSig sig_v_ii(0, 2, storage);
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddFunction(&sig_v_ii);
  builder.AddFunction(&sig_ii_v);

  ExpectValidates(&sig_ii_v, {WASM_CALL_FUNCTION0(1)});
  ExpectValidates(sigs.v_v(), {WASM_CALL_FUNCTION0(1), WASM_DROP, WASM_DROP});
  ExpectValidates(sigs.v_v(), {WASM_CALL_FUNCTION0(1), kExprCallFunction, 0});
}

TEST_F(FunctionBodyDecoderTest, MultiReturnType) {
  WASM_FEATURE_SCOPE(mv);
  for (size_t a = 0; a < arraysize(kValueTypes); a++) {
    for (size_t b = 0; b < arraysize(kValueTypes); b++) {
      for (size_t c = 0; c < arraysize(kValueTypes); c++) {
        for (size_t d = 0; d < arraysize(kValueTypes); d++) {
          ValueType storage_ab[] = {kValueTypes[a], kValueTypes[b]};
          FunctionSig sig_ab_v(2, 0, storage_ab);
          ValueType storage_cd[] = {kValueTypes[c], kValueTypes[d]};
          FunctionSig sig_cd_v(2, 0, storage_cd);

          TestModuleBuilder builder;
          module = builder.module();
          builder.AddFunction(&sig_cd_v);

          ExpectValidates(&sig_cd_v, {WASM_CALL_FUNCTION0(0)});

          if (a == c && b == d) {
            ExpectValidates(&sig_ab_v, {WASM_CALL_FUNCTION0(0)});
          } else {
            ExpectFailure(&sig_ab_v, {WASM_CALL_FUNCTION0(0)});
          }
        }
      }
    }
  }
}

TEST_F(FunctionBodyDecoderTest, SimpleIndirectCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.AddTable(kWasmAnyFunc, 20, false, 20);
  module = builder.module();

  byte f0 = builder.AddSignature(sigs.i_v());
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());

  ExpectValidates(sig, {WASM_CALL_INDIRECT0(f0, WASM_ZERO)});
  ExpectValidates(sig, {WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I32V_1(22))});
  ExpectValidates(sig, {WASM_CALL_INDIRECT2(f2, WASM_ZERO, WASM_I32V_1(32),
                                            WASM_I32V_2(72))});
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsOutOfBounds) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.AddTable(kWasmAnyFunc, 20, false, 20);
  module = builder.module();

  ExpectFailure(sig, {WASM_CALL_INDIRECT0(0, WASM_ZERO)});
  builder.AddSignature(sigs.i_v());
  ExpectValidates(sig, {WASM_CALL_INDIRECT0(0, WASM_ZERO)});

  ExpectFailure(sig, {WASM_CALL_INDIRECT1(1, WASM_ZERO, WASM_I32V_1(22))});
  builder.AddSignature(sigs.i_i());
  ExpectValidates(sig, {WASM_CALL_INDIRECT1(1, WASM_ZERO, WASM_I32V_1(27))});

  ExpectFailure(sig, {WASM_CALL_INDIRECT1(2, WASM_ZERO, WASM_I32V_1(27))});
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.InitializeTable();
  module = builder.module();

  byte f0 = builder.AddFunction(sigs.i_f());

  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f0, WASM_ZERO, WASM_F64(37.2))});

  ExpectFailure(sig, {WASM_CALL_INDIRECT0(f0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT0(f0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT0(f0, WASM_F64(37.2))});

  byte f1 = builder.AddFunction(sigs.i_d());

  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I32V_1(16))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I64V_1(16))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_F32(17.6))});
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsWithoutTableCrash) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  byte f0 = builder.AddSignature(sigs.i_v());
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());

  ExpectFailure(sig, {WASM_CALL_INDIRECT0(f0, WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT1(f1, WASM_ZERO, WASM_I32V_1(22))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT2(f2, WASM_ZERO, WASM_I32V_1(32),
                                          WASM_I32V_2(72))});
}

TEST_F(FunctionBodyDecoderTest, IncompleteIndirectCall) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.InitializeTable();
  module = builder.module();

  static byte code[] = {kExprCallIndirect};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, IncompleteStore) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.InitializeMemory();
  builder.InitializeTable();
  module = builder.module();

  static byte code[] = {kExprI32StoreMem};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, IncompleteS8x16Shuffle) {
  WASM_FEATURE_SCOPE(simd);
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  builder.InitializeMemory();
  builder.InitializeTable();
  module = builder.module();

  static byte code[] = {kSimdPrefix,
                        static_cast<byte>(kExprS8x16Shuffle & 0xff)};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, SimpleImportCalls) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  byte f0 = builder.AddImport(sigs.i_v());
  byte f1 = builder.AddImport(sigs.i_i());
  byte f2 = builder.AddImport(sigs.i_ii());

  ExpectValidates(sig, {WASM_CALL_FUNCTION0(f0)});
  ExpectValidates(sig, {WASM_CALL_FUNCTION(f1, WASM_I32V_1(22))});
  ExpectValidates(sig,
                  {WASM_CALL_FUNCTION(f2, WASM_I32V_1(32), WASM_I32V_2(72))});
}

TEST_F(FunctionBodyDecoderTest, ImportCallsWithMismatchedSigs3) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  byte f0 = builder.AddImport(sigs.i_f());

  ExpectFailure(sig, {WASM_CALL_FUNCTION0(f0)});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(f0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(f0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(f0, WASM_F64(37.2))});

  byte f1 = builder.AddImport(sigs.i_d());

  ExpectFailure(sig, {WASM_CALL_FUNCTION0(f1)});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(f1, WASM_I32V_1(16))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(f1, WASM_I64V_1(16))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(f1, WASM_F32(17.6))});
}

TEST_F(FunctionBodyDecoderTest, Int32Globals) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddGlobal(kWasmI32);

  ExpectValidates(sig, {WASM_GET_GLOBAL(0)});
  ExpectFailure(sig, {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0))});
  ExpectValidates(sig, {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, ImmutableGlobal) {
  FunctionSig* sig = sigs.v_v();
  TestModuleBuilder builder;
  module = builder.module();

  uint32_t g0 = builder.AddGlobal(kWasmI32, true);
  uint32_t g1 = builder.AddGlobal(kWasmI32, false);

  ExpectValidates(sig, {WASM_SET_GLOBAL(g0, WASM_ZERO)});
  ExpectFailure(sig, {WASM_SET_GLOBAL(g1, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Int32Globals_fail) {
  FunctionSig* sig = sigs.i_i();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddGlobal(kWasmI64);
  builder.AddGlobal(kWasmI64);
  builder.AddGlobal(kWasmF32);
  builder.AddGlobal(kWasmF64);

  ExpectFailure(sig, {WASM_GET_GLOBAL(0)});
  ExpectFailure(sig, {WASM_GET_GLOBAL(1)});
  ExpectFailure(sig, {WASM_GET_GLOBAL(2)});
  ExpectFailure(sig, {WASM_GET_GLOBAL(3)});

  ExpectFailure(sig, {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_ZERO});
  ExpectFailure(sig, {WASM_SET_GLOBAL(1, WASM_GET_LOCAL(0)), WASM_ZERO});
  ExpectFailure(sig, {WASM_SET_GLOBAL(2, WASM_GET_LOCAL(0)), WASM_ZERO});
  ExpectFailure(sig, {WASM_SET_GLOBAL(3, WASM_GET_LOCAL(0)), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, Int64Globals) {
  FunctionSig* sig = sigs.l_l();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddGlobal(kWasmI64);
  builder.AddGlobal(kWasmI64);

  ExpectValidates(sig, {WASM_GET_GLOBAL(0)});
  ExpectValidates(sig, {WASM_GET_GLOBAL(1)});

  ExpectValidates(sig,
                  {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_GET_LOCAL(0)});
  ExpectValidates(sig,
                  {WASM_SET_GLOBAL(1, WASM_GET_LOCAL(0)), WASM_GET_LOCAL(0)});
}

TEST_F(FunctionBodyDecoderTest, Float32Globals) {
  FunctionSig* sig = sigs.f_ff();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddGlobal(kWasmF32);

  ExpectValidates(sig, {WASM_GET_GLOBAL(0)});
  ExpectValidates(sig,
                  {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_GET_LOCAL(0)});
}

TEST_F(FunctionBodyDecoderTest, Float64Globals) {
  FunctionSig* sig = sigs.d_dd();
  TestModuleBuilder builder;
  module = builder.module();

  builder.AddGlobal(kWasmF64);

  ExpectValidates(sig, {WASM_GET_GLOBAL(0)});
  ExpectValidates(sig,
                  {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_GET_LOCAL(0)});
}

TEST_F(FunctionBodyDecoderTest, AllGetGlobalCombinations) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType local_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType global_type = kValueTypes[j];
      FunctionSig sig(1, 0, &local_type);
      TestModuleBuilder builder;
      module = builder.module();
      builder.AddGlobal(global_type);
      Validate(local_type == global_type, &sig, {WASM_GET_GLOBAL(0)});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, AllSetGlobalCombinations) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType local_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType global_type = kValueTypes[j];
      FunctionSig sig(0, 1, &local_type);
      TestModuleBuilder builder;
      module = builder.module();
      builder.AddGlobal(global_type);
      Validate(local_type == global_type, &sig,
               {WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0))});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, SetTable) {
  WASM_FEATURE_SCOPE(anyref);
  TestModuleBuilder builder;
  module = builder.module();
  byte tab_ref1 = builder.AddTable(kWasmAnyRef, 10, true, 20);
  byte tab_func1 = builder.AddTable(kWasmAnyFunc, 20, true, 30);
  byte tab_func2 = builder.AddTable(kWasmAnyFunc, 10, false, 20);
  byte tab_ref2 = builder.AddTable(kWasmAnyRef, 10, false, 20);
  ValueType sig_types[]{kWasmAnyRef, kWasmAnyFunc, kWasmI32};
  FunctionSig sig(0, 3, sig_types);
  byte local_ref = 0;
  byte local_func = 1;
  byte local_int = 2;
  ExpectValidates(&sig, {WASM_SET_TABLE(tab_ref1, WASM_I32V(6),
                                        WASM_GET_LOCAL(local_ref))});
  ExpectValidates(&sig, {WASM_SET_TABLE(tab_func1, WASM_I32V(5),
                                        WASM_GET_LOCAL(local_func))});
  ExpectValidates(&sig, {WASM_SET_TABLE(tab_func2, WASM_I32V(7),
                                        WASM_GET_LOCAL(local_func))});
  ExpectValidates(&sig, {WASM_SET_TABLE(tab_ref2, WASM_I32V(8),
                                        WASM_GET_LOCAL(local_ref))});

  // We can store anyfunc values as anyref, but not the other way around.
  ExpectValidates(&sig, {WASM_SET_TABLE(tab_ref1, WASM_I32V(4),
                                        WASM_GET_LOCAL(local_func))});
  ExpectFailure(&sig, {WASM_SET_TABLE(tab_func1, WASM_I32V(9),
                                      WASM_GET_LOCAL(local_ref))});
  ExpectFailure(&sig, {WASM_SET_TABLE(tab_func2, WASM_I32V(3),
                                      WASM_GET_LOCAL(local_ref))});
  ExpectValidates(&sig, {WASM_SET_TABLE(tab_ref2, WASM_I32V(2),
                                        WASM_GET_LOCAL(local_func))});
  ExpectFailure(&sig, {WASM_SET_TABLE(tab_ref1, WASM_I32V(9),
                                      WASM_GET_LOCAL(local_int))});
  ExpectFailure(&sig, {WASM_SET_TABLE(tab_func1, WASM_I32V(3),
                                      WASM_GET_LOCAL(local_int))});
  // Out-of-bounds table index should fail.
  byte oob_tab = 37;
  ExpectFailure(
      &sig, {WASM_SET_TABLE(oob_tab, WASM_I32V(9), WASM_GET_LOCAL(local_ref))});
  ExpectFailure(&sig, {WASM_SET_TABLE(oob_tab, WASM_I32V(3),
                                      WASM_GET_LOCAL(local_func))});
}

TEST_F(FunctionBodyDecoderTest, GetTable) {
  WASM_FEATURE_SCOPE(anyref);
  TestModuleBuilder builder;
  module = builder.module();
  byte tab_ref1 = builder.AddTable(kWasmAnyRef, 10, true, 20);
  byte tab_func1 = builder.AddTable(kWasmAnyFunc, 20, true, 30);
  byte tab_func2 = builder.AddTable(kWasmAnyFunc, 10, false, 20);
  byte tab_ref2 = builder.AddTable(kWasmAnyRef, 10, false, 20);
  ValueType sig_types[]{kWasmAnyRef, kWasmAnyFunc, kWasmI32};
  FunctionSig sig(0, 3, sig_types);
  byte local_ref = 0;
  byte local_func = 1;
  byte local_int = 2;
  ExpectValidates(
      &sig,
      {WASM_SET_LOCAL(local_ref, WASM_GET_TABLE(tab_ref1, WASM_I32V(6)))});
  ExpectValidates(
      &sig,
      {WASM_SET_LOCAL(local_ref, WASM_GET_TABLE(tab_ref2, WASM_I32V(8)))});
  ExpectValidates(
      &sig,
      {WASM_SET_LOCAL(local_func, WASM_GET_TABLE(tab_func1, WASM_I32V(5)))});
  ExpectValidates(
      &sig,
      {WASM_SET_LOCAL(local_func, WASM_GET_TABLE(tab_func2, WASM_I32V(7)))});

  // We can store anyfunc values as anyref, but not the other way around.
  ExpectFailure(&sig, {WASM_SET_LOCAL(local_func,
                                      WASM_GET_TABLE(tab_ref1, WASM_I32V(4)))});
  ExpectValidates(
      &sig,
      {WASM_SET_LOCAL(local_ref, WASM_GET_TABLE(tab_func1, WASM_I32V(9)))});
  ExpectValidates(
      &sig,
      {WASM_SET_LOCAL(local_ref, WASM_GET_TABLE(tab_func2, WASM_I32V(3)))});
  ExpectFailure(&sig, {WASM_SET_LOCAL(local_func,
                                      WASM_GET_TABLE(tab_ref2, WASM_I32V(2)))});

  ExpectFailure(&sig, {WASM_SET_LOCAL(local_int,
                                      WASM_GET_TABLE(tab_ref1, WASM_I32V(9)))});
  ExpectFailure(&sig, {WASM_SET_LOCAL(
                          local_int, WASM_GET_TABLE(tab_func1, WASM_I32V(3)))});
  // Out-of-bounds table index should fail.
  byte oob_tab = 37;
  ExpectFailure(
      &sig, {WASM_SET_LOCAL(local_ref, WASM_GET_TABLE(oob_tab, WASM_I32V(9)))});
  ExpectFailure(&sig, {WASM_SET_LOCAL(local_func,
                                      WASM_GET_TABLE(oob_tab, WASM_I32V(3)))});
}

TEST_F(FunctionBodyDecoderTest, MultiTableCallIndirect) {
  WASM_FEATURE_SCOPE(anyref);
  TestModuleBuilder builder;
  module = builder.module();
  byte tab_ref = builder.AddTable(kWasmAnyRef, 10, true, 20);
  byte tab_func = builder.AddTable(kWasmAnyFunc, 20, true, 30);

  ValueType sig_types[]{kWasmAnyRef, kWasmAnyFunc, kWasmI32};
  FunctionSig sig(0, 3, sig_types);
  byte sig_index = builder.AddSignature(sigs.i_v());

  // We can store anyfunc values as anyref, but not the other way around.
  ExpectValidates(sigs.i_v(),
                  {kExprI32Const, 0, kExprCallIndirect, sig_index, tab_func});

  ExpectFailure(sigs.i_v(),
                {kExprI32Const, 0, kExprCallIndirect, sig_index, tab_ref});
}

TEST_F(FunctionBodyDecoderTest, WasmMemoryGrow) {
  TestModuleBuilder builder;
  module = builder.module();
  builder.InitializeMemory();

  byte code[] = {WASM_GET_LOCAL(0), kExprMemoryGrow, 0};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.i_d(), code);
}

TEST_F(FunctionBodyDecoderTest, AsmJsMemoryGrow) {
  TestModuleBuilder builder(kAsmJsOrigin);
  module = builder.module();
  builder.InitializeMemory();

  byte code[] = {WASM_GET_LOCAL(0), kExprMemoryGrow, 0};
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, AsmJsBinOpsCheckOrigin) {
  ValueType float32int32float32[] = {kWasmF32, kWasmI32, kWasmF32};
  FunctionSig sig_f_if(1, 2, float32int32float32);
  ValueType float64int32float64[] = {kWasmF64, kWasmI32, kWasmF64};
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
    TestModuleBuilder builder(kAsmJsOrigin);
    module = builder.module();
    builder.InitializeMemory();
    for (size_t i = 0; i < arraysize(AsmJsBinOps); i++) {
      TestBinop(AsmJsBinOps[i].op, AsmJsBinOps[i].sig);
    }
  }

  {
    TestModuleBuilder builder;
    module = builder.module();
    builder.InitializeMemory();
    for (size_t i = 0; i < arraysize(AsmJsBinOps); i++) {
      ExpectFailure(AsmJsBinOps[i].sig,
                    {WASM_BINOP(AsmJsBinOps[i].op, WASM_GET_LOCAL(0),
                                WASM_GET_LOCAL(1))});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, AsmJsUnOpsCheckOrigin) {
  ValueType float32int32[] = {kWasmF32, kWasmI32};
  FunctionSig sig_f_i(1, 1, float32int32);
  ValueType float64int32[] = {kWasmF64, kWasmI32};
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
    TestModuleBuilder builder(kAsmJsOrigin);
    module = builder.module();
    builder.InitializeMemory();
    for (size_t i = 0; i < arraysize(AsmJsUnOps); i++) {
      TestUnop(AsmJsUnOps[i].op, AsmJsUnOps[i].sig);
    }
  }

  {
    TestModuleBuilder builder;
    module = builder.module();
    builder.InitializeMemory();
    for (size_t i = 0; i < arraysize(AsmJsUnOps); i++) {
      ExpectFailure(AsmJsUnOps[i].sig,
                    {WASM_UNOP(AsmJsUnOps[i].op, WASM_GET_LOCAL(0))});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakEnd) {
  ExpectValidates(
      sigs.i_i(),
      {WASM_BLOCK_I(WASM_I32_ADD(WASM_BRV(0, WASM_ZERO), WASM_ZERO))});
  ExpectValidates(
      sigs.i_i(),
      {WASM_BLOCK_I(WASM_I32_ADD(WASM_ZERO, WASM_BRV(0, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, BreakIfBinop) {
  ExpectValidates(sigs.i_i(),
                  {WASM_BLOCK_I(WASM_I32_ADD(
                      WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO), WASM_ZERO))});
  ExpectValidates(sigs.i_i(),
                  {WASM_BLOCK_I(WASM_I32_ADD(
                      WASM_ZERO, WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO)))});
  ExpectValidates(
      sigs.f_ff(),
      {WASM_BLOCK_F(WASM_F32_ABS(WASM_BRV_IF(0, WASM_F32(0.0f), WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, BreakIfBinop_fail) {
  ExpectFailure(
      sigs.f_ff(),
      {WASM_BLOCK_F(WASM_F32_ABS(WASM_BRV_IF(0, WASM_ZERO, WASM_ZERO)))});
  ExpectFailure(
      sigs.i_i(),
      {WASM_BLOCK_I(WASM_F32_ABS(WASM_BRV_IF(0, WASM_F32(0.0f), WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, BreakIfUnrNarrow) {
  ExpectFailure(
      sigs.f_ff(),
      {WASM_BLOCK_I(WASM_BRV_IF(0, WASM_UNREACHABLE, WASM_UNREACHABLE),
                    WASM_RETURN0),
       WASM_F32(0.0)});
}

TEST_F(FunctionBodyDecoderTest, BreakNesting1) {
  for (int i = 0; i < 5; i++) {
    // (block[2] (loop[2] (if (get p) break[N]) (set p 1)) p)
    byte code[] = {WASM_BLOCK_I(
        WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(i + 1, WASM_ZERO)),
                  WASM_SET_LOCAL(0, WASM_I32V_1(1))),
        WASM_ZERO)};
    Validate(i < 3, sigs.i_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, BreakNesting2) {
  for (int i = 0; i < 7; i++) {
    byte code[] = {B1(WASM_LOOP(WASM_IF(WASM_ZERO, WASM_BR(i)), WASM_NOP))};
    Validate(i <= 3, sigs.v_v(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, BreakNesting3) {
  for (int i = 0; i < 7; i++) {
    // (block[1] (loop[1] (block[1] (if 0 break[N])
    byte code[] = {
        WASM_BLOCK(WASM_LOOP(B1(WASM_IF(WASM_ZERO, WASM_BR(i + 1)))))};
    Validate(i < 4, sigs.v_v(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, BreaksWithMultipleTypes) {
  ExpectFailure(sigs.i_i(),
                {B2(WASM_BRV_IF_ZERO(0, WASM_I32V_1(7)), WASM_F32(7.7))});

  ExpectFailure(sigs.i_i(), {B2(WASM_BRV_IF_ZERO(0, WASM_I32V_1(7)),
                                WASM_BRV_IF_ZERO(0, WASM_F32(7.7)))});
  ExpectFailure(sigs.i_i(), {B3(WASM_BRV_IF_ZERO(0, WASM_I32V_1(8)),
                                WASM_BRV_IF_ZERO(0, WASM_I32V_1(0)),
                                WASM_BRV_IF_ZERO(0, WASM_F32(7.7)))});
  ExpectFailure(sigs.i_i(), {B3(WASM_BRV_IF_ZERO(0, WASM_I32V_1(9)),
                                WASM_BRV_IF_ZERO(0, WASM_F32(7.7)),
                                WASM_BRV_IF_ZERO(0, WASM_I32V_1(11)))});
}

TEST_F(FunctionBodyDecoderTest, BreakNesting_6_levels) {
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

      Validate(i <= depth, sigs.v_v(), code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheck) {
  FunctionSig* sigarray[] = {sigs.i_i(), sigs.l_l(), sigs.f_ff(), sigs.d_dd()};
  for (size_t i = 0; i < arraysize(sigarray); i++) {
    FunctionSig* sig = sigarray[i];
    // unify X and X => OK
    byte code[] = {WASM_BLOCK_T(
        sig->GetReturn(), WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
        WASM_GET_LOCAL(0))};
    ExpectValidates(sig, code);
  }

  // unify i32 and f32 => fail
  ExpectFailure(sigs.i_i(),
                {WASM_BLOCK_I(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_ZERO)),
                              WASM_F32(1.2))});

  // unify f64 and f64 => OK
  ExpectValidates(
      sigs.d_dd(),
      {WASM_BLOCK_D(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
                    WASM_F64(1.2))});
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheckAll1) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType storage[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_BLOCK_T(
          sig.GetReturn(), WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_GET_LOCAL(0))),
          WASM_GET_LOCAL(1))};

      Validate(i == j, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheckAll2) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType storage[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_IF_ELSE_T(sig.GetReturn(0), WASM_ZERO,
                                    WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)),
                                    WASM_GET_LOCAL(1))};

      Validate(i == j, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheckAll3) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType storage[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_IF_ELSE_T(sig.GetReturn(), WASM_ZERO,
                                    WASM_GET_LOCAL(1),
                                    WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))};

      Validate(i == j, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kValueTypes); i++) {
      ValueType type = kValueTypes[i];
      ValueType storage[] = {kWasmI32, kWasmI32, type};
      FunctionSig sig(1, 2, storage);

      byte code1[] = {WASM_BLOCK_T(
          type, WASM_IF(WASM_ZERO, WASM_BRV(1, WASM_GET_LOCAL(which))),
          WASM_GET_LOCAL(which ^ 1))};

      Validate(type == kWasmI32, &sig, code1);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakIf_cond_type) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType types[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, types);
      byte code[] = {WASM_BLOCK_T(
          types[0], WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)))};

      Validate(types[2] == kWasmI32, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakIf_val_type) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType types[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j],
                           kWasmI32};
      FunctionSig sig(1, 3, types);
      byte code[] = {WASM_BLOCK_T(
          types[1], WASM_BRV_IF(0, WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
          WASM_DROP, WASM_GET_LOCAL(0))};

      Validate(i == j, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakIf_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kValueTypes); i++) {
      ValueType type = kValueTypes[i];
      ValueType storage[] = {kWasmI32, kWasmI32, type};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(which)),
                                  WASM_DROP, WASM_GET_LOCAL(which ^ 1))};

      Validate(type == kWasmI32, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BrTable0) {
  ExpectFailure(sigs.v_v(), {kExprBrTable, 0, BR_TARGET(0)});
}

TEST_F(FunctionBodyDecoderTest, BrTable0b) {
  static byte code[] = {kExprI32Const, 11, kExprBrTable, 0, BR_TARGET(0)};
  ExpectValidates(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, BrTable0c) {
  static byte code[] = {kExprI32Const, 11, kExprBrTable, 0, BR_TARGET(1)};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, BrTable1a) {
  ExpectValidates(sigs.v_v(),
                  {B1(WASM_BR_TABLE(WASM_I32V_2(67), 0, BR_TARGET(0)))});
}

TEST_F(FunctionBodyDecoderTest, BrTable1b) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_ZERO, 0, BR_TARGET(0)))};
  ExpectValidates(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
  ExpectFailure(sigs.f_ff(), code);
  ExpectFailure(sigs.d_dd(), code);
}

TEST_F(FunctionBodyDecoderTest, BrTable2a) {
  ExpectValidates(
      sigs.v_v(),
      {B1(WASM_BR_TABLE(WASM_I32V_2(67), 1, BR_TARGET(0), BR_TARGET(0)))});
}

TEST_F(FunctionBodyDecoderTest, BrTable2b) {
  ExpectValidates(sigs.v_v(),
                  {WASM_BLOCK(WASM_BLOCK(WASM_BR_TABLE(
                      WASM_I32V_2(67), 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_off_end) {
  static byte code[] = {B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(0)))};
  for (size_t len = 1; len < sizeof(code); len++) {
    ExpectFailure(sigs.i_i(), VectorOf(code, len), kAppendEnd);
    ExpectFailure(sigs.i_i(), VectorOf(code, len), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, BrTable_invalid_br1) {
  for (int depth = 0; depth < 4; depth++) {
    byte code[] = {B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(depth)))};
    Validate(depth <= 1, sigs.v_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, BrTable_invalid_br2) {
  for (int depth = 0; depth < 7; depth++) {
    byte code[] = {
        WASM_LOOP(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(depth)))};
    Validate(depth < 2, sigs.v_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, BrTable_arity_mismatch1) {
  ExpectFailure(
      sigs.v_v(),
      {WASM_BLOCK(WASM_BLOCK_I(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_arity_mismatch2) {
  ExpectFailure(
      sigs.v_v(),
      {WASM_BLOCK_I(WASM_BLOCK(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_arity_mismatch_loop1) {
  ExpectFailure(
      sigs.v_v(),
      {WASM_LOOP(WASM_BLOCK_I(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_arity_mismatch_loop2) {
  ExpectFailure(
      sigs.v_v(),
      {WASM_BLOCK_I(WASM_LOOP(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_loop_block) {
  ExpectValidates(
      sigs.v_v(),
      {WASM_LOOP(WASM_BLOCK(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_block_loop) {
  ExpectValidates(
      sigs.v_v(),
      {WASM_LOOP(WASM_BLOCK(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_type_mismatch1) {
  ExpectFailure(
      sigs.v_v(),
      {WASM_BLOCK_I(WASM_BLOCK_F(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_type_mismatch2) {
  ExpectFailure(
      sigs.v_v(),
      {WASM_BLOCK_F(WASM_BLOCK_I(
          WASM_ONE, WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrTable_type_mismatch_unreachable) {
  ExpectFailure(sigs.v_v(),
                {WASM_BLOCK_F(WASM_BLOCK_I(
                    WASM_UNREACHABLE,
                    WASM_BR_TABLE(WASM_ONE, 1, BR_TARGET(0), BR_TARGET(1))))});
}

TEST_F(FunctionBodyDecoderTest, BrUnreachable1) {
  ExpectValidates(sigs.v_i(),
                  {WASM_GET_LOCAL(0), kExprBrTable, 0, BR_TARGET(0)});
}

TEST_F(FunctionBodyDecoderTest, BrUnreachable2) {
  ExpectValidates(sigs.v_i(),
                  {WASM_GET_LOCAL(0), kExprBrTable, 0, BR_TARGET(0), WASM_NOP});
  ExpectFailure(sigs.v_i(),
                {WASM_GET_LOCAL(0), kExprBrTable, 0, BR_TARGET(0), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, Brv1) {
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_BRV(0, WASM_ZERO))});
  ExpectValidates(sigs.i_i(),
                  {WASM_BLOCK_I(WASM_LOOP_I(WASM_BRV(2, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, Brv1_type) {
  ExpectValidates(sigs.i_ii(), {WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0)))});
  ExpectValidates(sigs.l_ll(), {WASM_BLOCK_L(WASM_BRV(0, WASM_GET_LOCAL(0)))});
  ExpectValidates(sigs.f_ff(), {WASM_BLOCK_F(WASM_BRV(0, WASM_GET_LOCAL(0)))});
  ExpectValidates(sigs.d_dd(), {WASM_BLOCK_D(WASM_BRV(0, WASM_GET_LOCAL(0)))});
}

TEST_F(FunctionBodyDecoderTest, Brv1_type_n) {
  ExpectFailure(sigs.i_f(), {WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0)))});
  ExpectFailure(sigs.i_d(), {WASM_BLOCK_I(WASM_BRV(0, WASM_GET_LOCAL(0)))});
}

TEST_F(FunctionBodyDecoderTest, BrvIf1) {
  ExpectValidates(sigs.i_v(), {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_ZERO))});
}

TEST_F(FunctionBodyDecoderTest, BrvIf1_type) {
  ExpectValidates(sigs.i_i(),
                  {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))});
  ExpectValidates(sigs.l_l(),
                  {WASM_BLOCK_L(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))});
  ExpectValidates(sigs.f_ff(),
                  {WASM_BLOCK_F(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))});
  ExpectValidates(sigs.d_dd(),
                  {WASM_BLOCK_D(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))});
}

TEST_F(FunctionBodyDecoderTest, BrvIf1_type_n) {
  ExpectFailure(sigs.i_f(),
                {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))});
  ExpectFailure(sigs.i_d(),
                {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_GET_LOCAL(0)))});
}

TEST_F(FunctionBodyDecoderTest, Select) {
  ExpectValidates(sigs.i_i(), {WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                           WASM_ZERO)});
  ExpectValidates(sigs.f_ff(),
                  {WASM_SELECT(WASM_F32(0.0), WASM_F32(0.0), WASM_ZERO)});
  ExpectValidates(sigs.d_dd(),
                  {WASM_SELECT(WASM_F64(0.0), WASM_F64(0.0), WASM_ZERO)});
  ExpectValidates(sigs.l_l(),
                  {WASM_SELECT(WASM_I64V_1(0), WASM_I64V_1(0), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Select_fail1) {
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_F32(0.0), WASM_GET_LOCAL(0),
                                         WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_GET_LOCAL(0), WASM_F32(0.0),
                                         WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                         WASM_F32(0.0))});
}

TEST_F(FunctionBodyDecoderTest, Select_fail2) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType type = kValueTypes[i];
    if (type == kWasmI32) continue;

    ValueType types[] = {type, kWasmI32, type};
    FunctionSig sig(1, 2, types);

    ExpectValidates(&sig, {WASM_SELECT(WASM_GET_LOCAL(1), WASM_GET_LOCAL(1),
                                       WASM_GET_LOCAL(0))});

    ExpectFailure(&sig, {WASM_SELECT(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0),
                                     WASM_GET_LOCAL(0))});

    ExpectFailure(&sig, {WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                     WASM_GET_LOCAL(0))});

    ExpectFailure(&sig, {WASM_SELECT(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                     WASM_GET_LOCAL(1))});
  }
}

TEST_F(FunctionBodyDecoderTest, Select_TypeCheck) {
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_F32(9.9), WASM_GET_LOCAL(0),
                                         WASM_GET_LOCAL(0))});

  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_GET_LOCAL(0), WASM_F64(0.25),
                                         WASM_GET_LOCAL(0))});

  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_F32(9.9), WASM_GET_LOCAL(0),
                                         WASM_I64V_1(0))});
}

TEST_F(FunctionBodyDecoderTest, Throw) {
  WASM_FEATURE_SCOPE(eh);
  TestModuleBuilder builder;
  module = builder.module();
  byte ex1 = builder.AddException(sigs.v_v());
  byte ex2 = builder.AddException(sigs.v_i());
  byte ex3 = builder.AddException(sigs.v_ii());
  ExpectValidates(sigs.v_v(), {kExprThrow, ex1});
  ExpectValidates(sigs.v_v(), {WASM_I32V(0), kExprThrow, ex2});
  ExpectFailure(sigs.v_v(), {WASM_F32(0.0), kExprThrow, ex2});
  ExpectValidates(sigs.v_v(), {WASM_I32V(0), WASM_I32V(0), kExprThrow, ex3});
  ExpectFailure(sigs.v_v(), {WASM_F32(0.0), WASM_I32V(0), kExprThrow, ex3});
  ExpectFailure(sigs.v_v(), {kExprThrow, 99});
}

TEST_F(FunctionBodyDecoderTest, ThrowUnreachable) {
  WASM_FEATURE_SCOPE(eh);
  TestModuleBuilder builder;
  module = builder.module();
  byte ex1 = builder.AddException(sigs.v_v());
  byte ex2 = builder.AddException(sigs.v_i());
  ExpectValidates(sigs.i_i(), {WASM_GET_LOCAL(0), kExprThrow, ex1, WASM_NOP});
  ExpectValidates(sigs.v_i(), {WASM_GET_LOCAL(0), kExprThrow, ex2, WASM_NOP});
  ExpectValidates(sigs.i_i(), {WASM_GET_LOCAL(0), kExprThrow, ex1, WASM_ZERO});
  ExpectFailure(sigs.v_i(), {WASM_GET_LOCAL(0), kExprThrow, ex2, WASM_ZERO});
  ExpectFailure(sigs.i_i(),
                {WASM_GET_LOCAL(0), kExprThrow, ex1, WASM_F32(0.0)});
  ExpectFailure(sigs.v_i(),
                {WASM_GET_LOCAL(0), kExprThrow, ex2, WASM_F32(0.0)});
}

#define WASM_TRY_OP kExprTry, kLocalVoid
#define WASM_BR_ON_EXN(depth, index) \
  kExprBrOnExn, static_cast<byte>(depth), static_cast<byte>(index)

TEST_F(FunctionBodyDecoderTest, TryCatch) {
  WASM_FEATURE_SCOPE(eh);
  TestModuleBuilder builder;
  module = builder.module();
  ExpectValidates(sigs.v_v(), {WASM_TRY_OP, kExprCatch, kExprDrop, kExprEnd});
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprCatch, kExprCatch, kExprEnd});
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprEnd});    // Missing catch.
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprCatch});  // Missing end.
  ExpectFailure(sigs.v_v(), {kExprCatch, kExprEnd});     // Missing try.
}

TEST_F(FunctionBodyDecoderTest, Rethrow) {
  WASM_FEATURE_SCOPE(eh);
  TestModuleBuilder builder;
  module = builder.module();
  ExpectValidates(sigs.v_v(),
                  {WASM_TRY_OP, kExprCatch, kExprRethrow, kExprEnd});
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprRethrow, kExprCatch, kExprEnd});
  ExpectFailure(sigs.v_v(), {WASM_BLOCK(kExprRethrow)});
  ExpectFailure(sigs.v_v(), {kExprRethrow});
}

TEST_F(FunctionBodyDecoderTest, BrOnExn) {
  WASM_FEATURE_SCOPE(eh);
  TestModuleBuilder builder;
  module = builder.module();
  byte ex1 = builder.AddException(sigs.v_v());
  byte ex2 = builder.AddException(sigs.v_i());
  ExpectValidates(sigs.v_v(), {WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(0, ex1),
                               kExprDrop, kExprEnd});
  ExpectValidates(sigs.v_v(), {WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(1, ex1),
                               kExprDrop, kExprEnd});
  ExpectValidates(sigs.v_v(), {WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(0, ex1),
                               WASM_BR_ON_EXN(0, ex1), kExprDrop, kExprEnd});
  ExpectValidates(sigs.v_v(),
                  {WASM_BLOCK(WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(1, ex1),
                              kExprDrop, kExprEnd)});
  ExpectValidates(sigs.i_v(),
                  {WASM_BLOCK_I(WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(1, ex2),
                                kExprDrop, kExprEnd, kExprI32Const, 0)});
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(2, ex1),
                             kExprDrop, kExprEnd});
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprCatch, kExprDrop,
                             WASM_BR_ON_EXN(0, ex1), kExprEnd});
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprCatch, WASM_BR_ON_EXN(0, ex1), kExprEnd});
}

#undef WASM_BR_ON_EXN
#undef WASM_TRY_OP

TEST_F(FunctionBodyDecoderTest, MultiValBlock1) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(f0, WASM_NOP), kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0)), kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                              WASM_GET_LOCAL(0)),
                 kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), kExprF32Add});
}

TEST_F(FunctionBodyDecoderTest, MultiValBlock2) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(sigs.i_ii(),
                  {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                   WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f0, WASM_NOP), WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0)),
                              WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                              WASM_GET_LOCAL(0)),
                 WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                 WASM_F32_ADD(WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, MultiValBlockBr) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f0 = builder.AddSignature(sigs.ii_v());
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0), WASM_BR(0)), kExprI32Add});
  ExpectValidates(sigs.i_ii(), {WASM_BLOCK_X(f0, WASM_GET_LOCAL(0),
                                             WASM_GET_LOCAL(1), WASM_BR(0)),
                                kExprI32Add});
}

TEST_F(FunctionBodyDecoderTest, MultiValLoop1) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_LOOP_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(f0, WASM_NOP), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(f0, WASM_GET_LOCAL(0)), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(f0, WASM_GET_LOCAL(0),
                                          WASM_GET_LOCAL(1), WASM_GET_LOCAL(0)),
                              kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_LOOP_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), kExprF32Add});
}

TEST_F(FunctionBodyDecoderTest, MultiValIf) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0),
                      WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                      WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
       kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0), WASM_NOP, WASM_NOP), kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0), WASM_NOP,
                                WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
                 kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0),
                      WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), WASM_NOP),
       kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                WASM_GET_LOCAL(1)),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0),
                                WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                                WASM_GET_LOCAL(1)),
                 kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(
           f0, WASM_GET_LOCAL(0),
           WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)),
           WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0), WASM_GET_LOCAL(0))),
       kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0),
                                WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0),
                                         WASM_GET_LOCAL(0)),
                                WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0),
                                WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                                WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(1),
                                         WASM_GET_LOCAL(1))),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(f0, WASM_GET_LOCAL(0),
                                WASM_SEQ(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                                WASM_SEQ(WASM_GET_LOCAL(1), WASM_GET_LOCAL(0))),
                 kExprF32Add});
}

TEST_F(FunctionBodyDecoderTest, BlockParam) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_GET_LOCAL(0),
       WASM_BLOCK_X(f1, WASM_GET_LOCAL(1), WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(),
                  {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                   WASM_BLOCK_X(f2, WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                WASM_BLOCK_X(f1, WASM_NOP),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(f1, WASM_NOP), WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(f1, WASM_GET_LOCAL(0)),
                              WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_GET_LOCAL(0), WASM_BLOCK_X(f2, WASM_I32_ADD(WASM_NOP, WASM_NOP)),
       WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_ii(),
                {WASM_GET_LOCAL(0), WASM_BLOCK_X(f1, WASM_F32_NEG(WASM_NOP)),
                 WASM_RETURN1(WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, LoopParam) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0),
                                WASM_LOOP_X(f1, WASM_GET_LOCAL(1),
                                            WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(),
                  {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                   WASM_LOOP_X(f2, WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                WASM_LOOP_X(f1, WASM_NOP),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_LOOP_X(f1, WASM_NOP), WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(f1, WASM_GET_LOCAL(0)),
                              WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_ii(), {WASM_GET_LOCAL(0),
                              WASM_LOOP_X(f2, WASM_I32_ADD(WASM_NOP, WASM_NOP)),
                              WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_ii(),
                {WASM_GET_LOCAL(0), WASM_LOOP_X(f1, WASM_F32_NEG(WASM_NOP)),
                 WASM_RETURN1(WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, LoopParamBr) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(sigs.i_ii(),
                  {WASM_GET_LOCAL(0), WASM_LOOP_X(f1, WASM_BR(0))});
  ExpectValidates(
      sigs.i_ii(),
      {WASM_GET_LOCAL(0), WASM_LOOP_X(f1, WASM_BRV(0, WASM_GET_LOCAL(1)))});
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                WASM_LOOP_X(f2, WASM_BR(0))});
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0),
                                WASM_LOOP_X(f1, WASM_BLOCK_X(f1, WASM_BR(1)))});
  ExpectFailure(sigs.i_ii(),
                {WASM_GET_LOCAL(0), WASM_LOOP_X(f1, WASM_BLOCK(WASM_BR(1))),
                 WASM_RETURN1(WASM_GET_LOCAL(0))});
  ExpectFailure(sigs.i_ii(), {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                              WASM_LOOP_X(f2, WASM_BLOCK_X(f1, WASM_BR(1))),
                              WASM_RETURN1(WASM_GET_LOCAL(0))});
}

TEST_F(FunctionBodyDecoderTest, IfParam) {
  WASM_FEATURE_SCOPE(mv);
  TestModuleBuilder builder;
  module = builder.module();
  byte f1 = builder.AddSignature(sigs.i_i());
  byte f2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(sigs.i_ii(),
                  {WASM_GET_LOCAL(0),
                   WASM_IF_X(f1, WASM_GET_LOCAL(0),
                             WASM_I32_ADD(WASM_NOP, WASM_GET_LOCAL(1)))});
  ExpectValidates(sigs.i_ii(),
                  {WASM_GET_LOCAL(0),
                   WASM_IF_ELSE_X(f1, WASM_GET_LOCAL(0),
                                  WASM_I32_ADD(WASM_NOP, WASM_GET_LOCAL(1)),
                                  WASM_I32_EQZ(WASM_NOP))});
  ExpectValidates(
      sigs.i_ii(),
      {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
       WASM_IF_ELSE_X(f2, WASM_GET_LOCAL(0), WASM_I32_ADD(WASM_NOP, WASM_NOP),
                      WASM_I32_MUL(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                WASM_IF_X(f1, WASM_GET_LOCAL(0), WASM_NOP),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.i_ii(), {WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                WASM_IF_ELSE_X(f1, WASM_GET_LOCAL(0), WASM_NOP,
                                               WASM_I32_EQZ(WASM_NOP)),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, Regression709741) {
  AddLocals(kWasmI32, kV8MaxWasmFunctionLocals - 1);
  ExpectValidates(sigs.v_v(), {WASM_NOP});
  byte code[] = {WASM_NOP, WASM_END};

  for (size_t i = 0; i < arraysize(code); ++i) {
    FunctionBody body(sigs.v_v(), 0, code, code + i);
    WasmFeatures unused_detected_features;
    DecodeResult result =
        VerifyWasmCode(zone()->allocator(), kAllWasmFeatures, nullptr,
                       &unused_detected_features, body);
    if (result.ok()) {
      std::ostringstream str;
      str << "Expected verification to fail";
    }
  }
}

TEST_F(FunctionBodyDecoderTest, MemoryInit) {
  TestModuleBuilder builder;
  builder.InitializeMemory();
  builder.SetDataSegmentCount(1);
  module = builder.module();

  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(1, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, MemoryInitInvalid) {
  TestModuleBuilder builder;
  builder.InitializeMemory();
  builder.SetDataSegmentCount(1);
  module = builder.module();

  WASM_FEATURE_SCOPE(bulk_memory);
  byte code[] = {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO),
                 WASM_END};
  for (size_t i = 0; i <= arraysize(code); ++i) {
    Validate(i == arraysize(code), sigs.v_v(), VectorOf(code, i), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, DataDrop) {
  TestModuleBuilder builder;
  builder.InitializeMemory();
  builder.SetDataSegmentCount(1);
  module = builder.module();

  ExpectFailure(sigs.v_v(), {WASM_DATA_DROP(0)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(), {WASM_DATA_DROP(0)});
  ExpectFailure(sigs.v_v(), {WASM_DATA_DROP(1)});
}

TEST_F(FunctionBodyDecoderTest, MemoryCopy) {
  TestModuleBuilder builder;
  builder.InitializeMemory();
  module = builder.module();

  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, MemoryFill) {
  TestModuleBuilder builder;
  builder.InitializeMemory();
  module = builder.module();

  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_FILL(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_FILL(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, BulkMemoryOpsWithoutMemory) {
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_FILL(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableInit) {
  TestModuleBuilder builder;
  builder.InitializeTable();
  builder.AddPassiveElementSegment();
  module = builder.module();

  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(),
                  {WASM_TABLE_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(1, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableInitInvalid) {
  TestModuleBuilder builder;
  builder.InitializeTable();
  builder.AddPassiveElementSegment();
  module = builder.module();

  WASM_FEATURE_SCOPE(bulk_memory);
  byte code[] = {WASM_TABLE_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO), WASM_END};
  for (size_t i = 0; i <= arraysize(code); ++i) {
    Validate(i == arraysize(code), sigs.v_v(), VectorOf(code, i), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, ElemDrop) {
  TestModuleBuilder builder;
  builder.InitializeTable();
  builder.AddPassiveElementSegment();
  module = builder.module();

  ExpectFailure(sigs.v_v(), {WASM_ELEM_DROP(0)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(), {WASM_ELEM_DROP(0)});
  ExpectFailure(sigs.v_v(), {WASM_ELEM_DROP(1)});
}

TEST_F(FunctionBodyDecoderTest, TableCopy) {
  TestModuleBuilder builder;
  builder.InitializeTable();
  module = builder.module();

  ExpectFailure(sigs.v_v(), {WASM_TABLE_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectValidates(sigs.v_v(),
                  {WASM_TABLE_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, BulkTableOpsWithoutTable) {
  TestModuleBuilder builder;
  builder.InitializeTable();
  builder.AddPassiveElementSegment();

  WASM_FEATURE_SCOPE(bulk_memory);
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(), {WASM_ELEM_DROP(0)});
  ExpectFailure(sigs.v_v(), {WASM_TABLE_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

class BranchTableIteratorTest : public TestWithZone {
 public:
  BranchTableIteratorTest() : TestWithZone() {}
  void CheckBrTableSize(const byte* start, const byte* end) {
    Decoder decoder(start, end);
    BranchTableImmediate<Decoder::kValidate> operand(&decoder, start);
    BranchTableIterator<Decoder::kValidate> iterator(&decoder, operand);
    EXPECT_EQ(end - start - 1u, iterator.length());
    EXPECT_TRUE(decoder.ok());
  }
  void CheckBrTableError(const byte* start, const byte* end) {
    Decoder decoder(start, end);
    BranchTableImmediate<Decoder::kValidate> operand(&decoder, start);
    BranchTableIterator<Decoder::kValidate> iterator(&decoder, operand);
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

#undef CHECK_BR_TABLE_LENGTH
#undef CHECK_BR_TABLE_ERROR

struct PrintOpcodes {
  const byte* start;
  const byte* end;
};
std::ostream& operator<<(std::ostream& out, const PrintOpcodes& range) {
  out << "First opcode: \""
      << WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(*range.start))
      << "\"\nall bytes: [";
  for (const byte* b = range.start; b < range.end; ++b) {
    out << (b == range.start ? "" : ", ") << uint32_t{*b} << "/"
        << AsHex(*b, 2, true);
  }
  return out << "]";
}

class WasmOpcodeLengthTest : public TestWithZone {
 public:
  WasmOpcodeLengthTest() : TestWithZone() {}

  template <typename... Bytes>
  void ExpectLength(unsigned expected, Bytes... bytes) {
    const byte code[] = {bytes..., 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(expected, OpcodeLength(code, code + sizeof(code)))
        << PrintOpcodes{code, code + sizeof...(bytes)};
  }
};

TEST_F(WasmOpcodeLengthTest, Statements) {
  ExpectLength(1, kExprNop);
  ExpectLength(1, kExprElse);
  ExpectLength(1, kExprEnd);
  ExpectLength(1, kExprSelect);
  ExpectLength(1, kExprCatch);
  ExpectLength(1, kExprRethrow);
  ExpectLength(2, kExprBr);
  ExpectLength(2, kExprBrIf);
  ExpectLength(2, kExprThrow);
  ExpectLength(3, kExprBrOnExn);
  ExpectLength(2, kExprBlock, kLocalI32);
  ExpectLength(2, kExprLoop, kLocalI32);
  ExpectLength(2, kExprIf, kLocalI32);
  ExpectLength(2, kExprTry, kLocalI32);
}

TEST_F(WasmOpcodeLengthTest, MiscExpressions) {
  ExpectLength(5, kExprF32Const);
  ExpectLength(9, kExprF64Const);
  ExpectLength(1, kExprRefNull);
  ExpectLength(2, kExprGetLocal);
  ExpectLength(2, kExprSetLocal);
  ExpectLength(2, kExprGetGlobal);
  ExpectLength(2, kExprSetGlobal);
  ExpectLength(2, kExprCallFunction);
  ExpectLength(3, kExprCallIndirect);
}

TEST_F(WasmOpcodeLengthTest, I32Const) {
  ExpectLength(2, kExprI32Const, U32V_1(1));
  ExpectLength(3, kExprI32Const, U32V_2(999));
  ExpectLength(4, kExprI32Const, U32V_3(9999));
  ExpectLength(5, kExprI32Const, U32V_4(999999));
  ExpectLength(6, kExprI32Const, U32V_5(99999999));
}

TEST_F(WasmOpcodeLengthTest, I64Const) {
  ExpectLength(2, kExprI64Const, U32V_1(1));
  ExpectLength(3, kExprI64Const, U32V_2(99));
  ExpectLength(4, kExprI64Const, U32V_3(9999));
  ExpectLength(5, kExprI64Const, U32V_4(99999));
  ExpectLength(6, kExprI64Const, U32V_5(9999999));
  ExpectLength(7, WASM_I64V_6(777777));
  ExpectLength(8, WASM_I64V_7(7777777));
  ExpectLength(9, WASM_I64V_8(77777777));
  ExpectLength(10, WASM_I64V_9(777777777));
}

TEST_F(WasmOpcodeLengthTest, VariableLength) {
  ExpectLength(2, kExprGetGlobal, U32V_1(1));
  ExpectLength(3, kExprGetGlobal, U32V_2(33));
  ExpectLength(4, kExprGetGlobal, U32V_3(44));
  ExpectLength(5, kExprGetGlobal, U32V_4(66));
  ExpectLength(6, kExprGetGlobal, U32V_5(77));
}

TEST_F(WasmOpcodeLengthTest, LoadsAndStores) {
  ExpectLength(3, kExprI32LoadMem8S);
  ExpectLength(3, kExprI32LoadMem8U);
  ExpectLength(3, kExprI32LoadMem16S);
  ExpectLength(3, kExprI32LoadMem16U);
  ExpectLength(3, kExprI32LoadMem);
  ExpectLength(3, kExprI64LoadMem8S);
  ExpectLength(3, kExprI64LoadMem8U);
  ExpectLength(3, kExprI64LoadMem16S);
  ExpectLength(3, kExprI64LoadMem16U);
  ExpectLength(3, kExprI64LoadMem32S);
  ExpectLength(3, kExprI64LoadMem32U);
  ExpectLength(3, kExprI64LoadMem);
  ExpectLength(3, kExprF32LoadMem);
  ExpectLength(3, kExprF64LoadMem);

  ExpectLength(3, kExprI32StoreMem8);
  ExpectLength(3, kExprI32StoreMem16);
  ExpectLength(3, kExprI32StoreMem);
  ExpectLength(3, kExprI64StoreMem8);
  ExpectLength(3, kExprI64StoreMem16);
  ExpectLength(3, kExprI64StoreMem32);
  ExpectLength(3, kExprI64StoreMem);
  ExpectLength(3, kExprF32StoreMem);
  ExpectLength(3, kExprF64StoreMem);
}

TEST_F(WasmOpcodeLengthTest, MiscMemExpressions) {
  ExpectLength(2, kExprMemorySize);
  ExpectLength(2, kExprMemoryGrow);
}

TEST_F(WasmOpcodeLengthTest, SimpleExpressions) {
#define SIMPLE_OPCODE(name, byte, sig) byte,
  static constexpr uint8_t kSimpleOpcodes[] = {
      FOREACH_SIMPLE_OPCODE(SIMPLE_OPCODE)};
#undef SIMPLE_OPCODE
  for (uint8_t simple_opcode : kSimpleOpcodes) {
    ExpectLength(1, simple_opcode);
  }
}

TEST_F(WasmOpcodeLengthTest, SimdExpressions) {
#define TEST_SIMD(name, opcode, sig) \
  ExpectLength(2, kSimdPrefix, static_cast<byte>(kExpr##name & 0xFF));
  FOREACH_SIMD_0_OPERAND_OPCODE(TEST_SIMD)
#undef TEST_SIMD
#define TEST_SIMD(name, opcode, sig) \
  ExpectLength(3, kSimdPrefix, static_cast<byte>(kExpr##name & 0xFF));
  FOREACH_SIMD_1_OPERAND_OPCODE(TEST_SIMD)
#undef TEST_SIMD
  ExpectLength(18, kSimdPrefix, static_cast<byte>(kExprS8x16Shuffle & 0xFF));
  // test for bad simd opcode
  ExpectLength(2, kSimdPrefix, 0xFF);
}

using TypesOfLocals = ZoneVector<ValueType>;

class LocalDeclDecoderTest : public TestWithZone {
 public:
  v8::internal::AccountingAllocator allocator;
  WasmFeatures enabled_features_;

  size_t ExpectRun(TypesOfLocals map, size_t pos, ValueType expected,
                   size_t count) {
    for (size_t i = 0; i < count; i++) {
      EXPECT_EQ(expected, map[pos++]);
    }
    return pos;
  }

  bool DecodeLocalDecls(BodyLocalDecls* decls, const byte* start,
                        const byte* end) {
    return i::wasm::DecodeLocalDecls(enabled_features_, decls, start, end);
  }
};

TEST_F(LocalDeclDecoderTest, EmptyLocals) {
  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, nullptr, nullptr);
  EXPECT_FALSE(result);
}

TEST_F(LocalDeclDecoderTest, NoLocals) {
  static const byte data[] = {0};
  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
  EXPECT_TRUE(result);
  EXPECT_TRUE(decls.type_list.empty());
}

TEST_F(LocalDeclDecoderTest, OneLocal) {
  WASM_FEATURE_SCOPE(anyref);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType type = kValueTypes[i];
    const byte data[] = {1, 1,
                         static_cast<byte>(ValueTypes::ValueTypeCodeFor(type))};
    BodyLocalDecls decls(zone());
    bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
    EXPECT_TRUE(result);
    EXPECT_EQ(1u, decls.type_list.size());

    TypesOfLocals map = decls.type_list;
    EXPECT_EQ(type, map[0]);
  }
}

TEST_F(LocalDeclDecoderTest, FiveLocals) {
  WASM_FEATURE_SCOPE(anyref);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType type = kValueTypes[i];
    const byte data[] = {1, 5,
                         static_cast<byte>(ValueTypes::ValueTypeCodeFor(type))};
    BodyLocalDecls decls(zone());
    bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
    EXPECT_TRUE(result);
    EXPECT_EQ(sizeof(data), decls.encoded_size);
    EXPECT_EQ(5u, decls.type_list.size());

    TypesOfLocals map = decls.type_list;
    EXPECT_EQ(5u, map.size());
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
          BodyLocalDecls decls(zone());
          bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
          EXPECT_TRUE(result);
          EXPECT_EQ(sizeof(data), decls.encoded_size);
          EXPECT_EQ(static_cast<uint32_t>(a + b + c + d),
                    decls.type_list.size());

          TypesOfLocals map = decls.type_list;

          size_t pos = 0;
          pos = ExpectRun(map, pos, kWasmI32, a);
          pos = ExpectRun(map, pos, kWasmI64, b);
          pos = ExpectRun(map, pos, kWasmF32, c);
          pos = ExpectRun(map, pos, kWasmF64, d);
        }
      }
    }
  }
}

TEST_F(LocalDeclDecoderTest, UseEncoder) {
  const byte* data = nullptr;
  const byte* end = nullptr;
  LocalDeclEncoder local_decls(zone());

  local_decls.AddLocals(5, kWasmF32);
  local_decls.AddLocals(1337, kWasmI32);
  local_decls.AddLocals(212, kWasmI64);
  local_decls.Prepend(zone(), &data, &end);

  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, data, end);
  EXPECT_TRUE(result);
  EXPECT_EQ(5u + 1337u + 212u, decls.type_list.size());

  TypesOfLocals map = decls.type_list;
  size_t pos = 0;
  pos = ExpectRun(map, pos, kWasmF32, 5);
  pos = ExpectRun(map, pos, kWasmI32, 1337);
  pos = ExpectRun(map, pos, kWasmI64, 212);
}

TEST_F(LocalDeclDecoderTest, ExceptRef) {
  WASM_FEATURE_SCOPE(eh);
  ValueType type = kWasmExceptRef;
  const byte data[] = {1, 1,
                       static_cast<byte>(ValueTypes::ValueTypeCodeFor(type))};
  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
  EXPECT_TRUE(result);
  EXPECT_EQ(1u, decls.type_list.size());

  TypesOfLocals map = decls.type_list;
  EXPECT_EQ(type, map[0]);
}

class BytecodeIteratorTest : public TestWithZone {};

TEST_F(BytecodeIteratorTest, SimpleForeach) {
  byte code[] = {WASM_IF_ELSE(WASM_ZERO, WASM_ZERO, WASM_ZERO)};
  BytecodeIterator iter(code, code + sizeof(code));
  WasmOpcode expected[] = {kExprI32Const, kExprIf,       kExprI32Const,
                           kExprElse,     kExprI32Const, kExprEnd};
  size_t pos = 0;
  for (WasmOpcode opcode : iter.opcodes()) {
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
  for (WasmOpcode opcode : iter.opcodes()) {
    USE(opcode);
    count++;
  }
  EXPECT_EQ(6, count);

  count = 0;
  for (WasmOpcode opcode : iter.opcodes()) {
    USE(opcode);
    count++;
  }
  EXPECT_EQ(6, count);
}

TEST_F(BytecodeIteratorTest, ForeachOffset) {
  byte code[] = {WASM_IF_ELSE(WASM_ZERO, WASM_ZERO, WASM_ZERO)};
  BytecodeIterator iter(code, code + sizeof(code));
  int count = 0;

  count = 0;
  for (auto offset : iter.offsets()) {
    USE(offset);
    count++;
  }
  EXPECT_EQ(6, count);

  count = 0;
  for (auto offset : iter.offsets()) {
    USE(offset);
    count++;
  }
  EXPECT_EQ(6, count);
}

TEST_F(BytecodeIteratorTest, WithLocalDecls) {
  byte code[] = {1, 1, kLocalI32, WASM_I32V_1(9), WASM_I32V_1(11)};
  BodyLocalDecls decls(zone());
  BytecodeIterator iter(code, code + sizeof(code), &decls);

  EXPECT_EQ(3u, decls.encoded_size);
  EXPECT_EQ(3u, iter.pc_offset());
  EXPECT_TRUE(iter.has_next());
  EXPECT_EQ(kExprI32Const, iter.current());
  iter.next();
  EXPECT_TRUE(iter.has_next());
  EXPECT_EQ(kExprI32Const, iter.current());
  iter.next();
  EXPECT_FALSE(iter.has_next());
}

#undef WASM_FEATURE_SCOPE
#undef B1
#undef B2
#undef B3
#undef WASM_IF_OP
#undef WASM_LOOP_OP
#undef WASM_BRV_IF_ZERO

}  // namespace function_body_decoder_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8

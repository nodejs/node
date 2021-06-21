// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-body-decoder.h"

#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/utils/ostreams.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/local-decl-encoder.h"
#include "src/wasm/signature-map.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/zone/zone.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace function_body_decoder_unittest {

#define B1(a) WASM_BLOCK(a)
#define B2(a, b) WASM_BLOCK(a, b)
#define B3(a, b, c) WASM_BLOCK(a, b, c)

#define WASM_IF_OP kExprIf, kVoidCode
#define WASM_LOOP_OP kExprLoop, kVoidCode

#define EXPECT_OK(result)                                        \
  do {                                                           \
    if (!result.ok()) {                                          \
      GTEST_NONFATAL_FAILURE_(result.error().message().c_str()); \
      return;                                                    \
    }                                                            \
  } while (false)

static const byte kCodeGetLocal0[] = {kExprLocalGet, 0};
static const byte kCodeGetLocal1[] = {kExprLocalGet, 1};
static const byte kCodeSetLocal0[] = {WASM_LOCAL_SET(0, WASM_ZERO)};
static const byte kCodeTeeLocal0[] = {WASM_LOCAL_TEE(0, WASM_ZERO)};

static const ValueType kValueTypes[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                                        kWasmExternRef};
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

constexpr size_t kMaxByteSizedLeb128 = 127;

using F = std::pair<ValueType, bool>;

// Used to construct fixed-size signatures: MakeSig::Returns(...).Params(...);
using MakeSig = FixedSizeSignature<ValueType>;

enum MemoryType { kMemory32, kMemory64 };

// A helper for tests that require a module environment for functions,
// globals, or memories.
class TestModuleBuilder {
 public:
  explicit TestModuleBuilder(ModuleOrigin origin = kWasmOrigin)
      : allocator(), mod(std::make_unique<Zone>(&allocator, ZONE_NAME)) {
    mod.origin = origin;
  }
  byte AddGlobal(ValueType type, bool mutability = true) {
    mod.globals.push_back(
        {type, mutability, WasmInitExpr(), {0}, false, false});
    CHECK_LE(mod.globals.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.globals.size() - 1);
  }
  byte AddSignature(const FunctionSig* sig) {
    mod.add_signature(sig);
    CHECK_LE(mod.types.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.types.size() - 1);
  }
  byte AddFunction(const FunctionSig* sig, bool declared = true) {
    byte sig_index = AddSignature(sig);
    mod.functions.push_back(
        {sig,                                          // sig
         static_cast<uint32_t>(mod.functions.size()),  // func_index
         sig_index,                                    // sig_index
         {0, 0},                                       // code
         false,                                        // import
         false,                                        // export
         declared});                                   // declared
    CHECK_LE(mod.functions.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.functions.size() - 1);
  }
  byte AddImport(const FunctionSig* sig) {
    byte result = AddFunction(sig);
    mod.functions[result].imported = true;
    return result;
  }
  byte AddException(WasmExceptionSig* sig) {
    mod.exceptions.emplace_back(sig);
    CHECK_LE(mod.types.size(), kMaxByteSizedLeb128);
    return static_cast<byte>(mod.exceptions.size() - 1);
  }

  byte AddTable(ValueType type, uint32_t initial_size, bool has_maximum_size,
                uint32_t maximum_size) {
    CHECK(WasmTable::IsValidTableType(type, &mod));
    mod.tables.emplace_back();
    WasmTable& table = mod.tables.back();
    table.type = type;
    table.initial_size = initial_size;
    table.has_maximum_size = has_maximum_size;
    table.maximum_size = maximum_size;
    return static_cast<byte>(mod.tables.size() - 1);
  }

  byte AddStruct(std::initializer_list<F> fields) {
    StructType::Builder type_builder(mod.signature_zone.get(),
                                     static_cast<uint32_t>(fields.size()));
    for (F field : fields) {
      type_builder.AddField(field.first, field.second);
    }
    mod.add_struct_type(type_builder.Build());
    return static_cast<byte>(mod.type_kinds.size() - 1);
  }

  byte AddArray(ValueType type, bool mutability) {
    ArrayType* array = mod.signature_zone->New<ArrayType>(type, mutability);
    mod.add_array_type(array);
    return static_cast<byte>(mod.type_kinds.size() - 1);
  }

  void InitializeMemory(MemoryType mem_type = kMemory32) {
    mod.has_memory = true;
    mod.is_memory64 = mem_type == kMemory64;
    mod.initial_pages = 1;
    mod.maximum_pages = 100;
  }

  byte InitializeTable(wasm::ValueType type) {
    mod.tables.emplace_back();
    mod.tables.back().type = type;
    return static_cast<byte>(mod.tables.size() - 1);
  }

  byte AddPassiveElementSegment(wasm::ValueType type) {
    mod.elem_segments.emplace_back(false);
    auto& init = mod.elem_segments.back();
    init.type = type;
    // Add 5 empty elements.
    for (uint32_t j = 0; j < 5; j++) {
      init.entries.push_back(WasmElemSegment::kNullIndex);
    }
    return static_cast<byte>(mod.elem_segments.size() - 1);
  }

  byte AddDeclarativeElementSegment() {
    mod.elem_segments.emplace_back(true);
    mod.elem_segments.back().entries.push_back(WasmElemSegment::kNullIndex);
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
  AccountingAllocator allocator;
  WasmModule mod;
};

template <class BaseTest>
class FunctionBodyDecoderTestBase : public WithZoneMixin<BaseTest> {
 public:
  using LocalsDecl = std::pair<uint32_t, ValueType>;
  // All features are disabled by default and must be activated with
  // a WASM_FEATURE_SCOPE in individual tests.
  WasmFeatures enabled_features_ = WasmFeatures::None();

  TestSignatures sigs;
  TestModuleBuilder builder;
  WasmModule* module = builder.module();
  LocalDeclEncoder local_decls{this->zone()};

  void AddLocals(ValueType type, uint32_t count) {
    local_decls.AddLocals(count, type);
  }

  enum AppendEnd : bool { kAppendEnd, kOmitEnd };

  Vector<const byte> PrepareBytecode(Vector<const byte> code,
                                     AppendEnd append_end) {
    size_t locals_size = local_decls.Size();
    size_t total_size =
        code.size() + locals_size + (append_end == kAppendEnd ? 1 : 0);
    byte* buffer = this->zone()->template NewArray<byte>(total_size);
    // Prepend the local decls to the code.
    local_decls.Emit(buffer);
    // Emit the code.
    if (code.size() > 0) {
      memcpy(buffer + locals_size, code.begin(), code.size());
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
  void Validate(bool expected_success, const FunctionSig* sig, Code&& raw_code,
                AppendEnd append_end = kAppendEnd,
                const char* message = nullptr) {
    Vector<const byte> code =
        PrepareBytecode(CodeToVector(std::forward<Code>(raw_code)), append_end);

    // Validate the code.
    FunctionBody body(sig, 0, code.begin(), code.end());
    WasmFeatures unused_detected_features = WasmFeatures::None();
    DecodeResult result =
        VerifyWasmCode(this->zone()->allocator(), enabled_features_, module,
                       &unused_detected_features, body);

    std::ostringstream str;
    if (result.failed()) {
      str << "Verification failed: pc = +" << result.error().offset()
          << ", msg = " << result.error().message();
    } else {
      str << "Verification succeeded, expected failure";
    }
    EXPECT_EQ(result.ok(), expected_success) << str.str();
    if (result.failed() && message) {
      EXPECT_THAT(result.error().message(), ::testing::HasSubstr(message));
    }
  }

  template <typename Code = std::initializer_list<const byte>>
  void ExpectValidates(const FunctionSig* sig, Code&& raw_code,
                       AppendEnd append_end = kAppendEnd,
                       const char* message = nullptr) {
    Validate(true, sig, std::forward<Code>(raw_code), append_end, message);
  }

  template <typename Code = std::initializer_list<const byte>>
  void ExpectFailure(const FunctionSig* sig, Code&& raw_code,
                     AppendEnd append_end = kAppendEnd,
                     const char* message = nullptr) {
    Validate(false, sig, std::forward<Code>(raw_code), append_end, message);
  }

  void TestBinop(WasmOpcode opcode, const FunctionSig* success) {
    // op(local[0], local[1])
    byte code[] = {WASM_BINOP(opcode, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))};
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

  void TestUnop(WasmOpcode opcode, const FunctionSig* success) {
    TestUnop(opcode, success->GetReturn(), success->GetParam(0));
  }

  void TestUnop(WasmOpcode opcode, ValueType ret_type, ValueType param_type) {
    // Return(op(local[0]))
    byte code[] = {WASM_UNOP(opcode, WASM_LOCAL_GET(0))};
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

using FunctionBodyDecoderTest = FunctionBodyDecoderTestBase<::testing::Test>;

TEST_F(FunctionBodyDecoderTest, Int32Const1) {
  byte code[] = {kExprI32Const, 0};
  for (int i = -64; i <= 63; i++) {
    code[1] = static_cast<byte>(i & 0x7F);
    ExpectValidates(sigs.i_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, RefFunc) {
  WASM_FEATURE_SCOPE(reftypes);

  builder.AddFunction(sigs.v_ii());
  builder.AddFunction(sigs.ii_v());
  ExpectValidates(sigs.a_v(), {kExprRefFunc, 1});
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
    base::WriteLittleEndianValue<float>(ptr, i * -7.75f);
    ExpectValidates(sigs.f_ff(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, Float64Const) {
  byte code[] = {kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0};
  Address ptr = reinterpret_cast<Address>(code + 1);
  for (int i = 0; i < 30; i++) {
    base::WriteLittleEndianValue<double>(ptr, i * 33.45);
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
  for (const FunctionSig* sig : {sigs.i_i(), sigs.i_ii(), sigs.i_iii()}) {
    ExpectValidates(sig, kCodeGetLocal0);
  }
}

TEST_F(FunctionBodyDecoderTest, GetLocalN_local) {
  for (byte i = 1; i < 8; i++) {
    AddLocals(kWasmI32, 1);
    for (byte j = 0; j < i; j++) {
      ExpectValidates(sigs.i_v(), {kExprLocalGet, j});
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
  ExpectFailure(sigs.i_i(), {kExprLocalGet});
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

  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_1(66)});
  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_2(7777)});
  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_3(8888)});
  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_4(9999)});

  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_5(kMaxLocals - 1)});

  ExpectFailure(sigs.i_i(), {kExprLocalGet, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_4(kMaxLocals - 1)});
  ExpectValidates(sigs.i_i(), {kExprLocalGet, U32V_4(kMaxLocals)});
  ExpectFailure(sigs.i_i(), {kExprLocalGet, U32V_4(kMaxLocals + 1)});

  ExpectFailure(sigs.i_v(), {kExprLocalGet, U32V_4(kMaxLocals)});
  ExpectFailure(sigs.i_v(), {kExprLocalGet, U32V_4(kMaxLocals + 1)});
}

TEST_F(FunctionBodyDecoderTest, GetLocal_toomany) {
  AddLocals(kWasmI32, kV8MaxWasmFunctionLocals - 100);
  AddLocals(kWasmI32, 100);

  ExpectValidates(sigs.i_v(), {kExprLocalGet, U32V_1(66)});
  ExpectFailure(sigs.i_i(), {kExprLocalGet, U32V_1(66)});
}

TEST_F(FunctionBodyDecoderTest, Binops_off_end) {
  byte code1[] = {0};  // [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code1[0] = kInt32BinopOpcodes[i];
    ExpectFailure(sigs.i_i(), code1);
  }

  byte code3[] = {kExprLocalGet, 0, 0};  // [expr] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code3[2] = kInt32BinopOpcodes[i];
    ExpectFailure(sigs.i_i(), code3);
  }

  byte code4[] = {kExprLocalGet, 0, 0, 0};  // [expr] [opcode] [opcode]
  for (size_t i = 0; i < arraysize(kInt32BinopOpcodes); i++) {
    code4[2] = kInt32BinopOpcodes[i];
    code4[3] = kInt32BinopOpcodes[i];
    ExpectFailure(sigs.i_i(), code4);
  }
}

TEST_F(FunctionBodyDecoderTest, BinopsAcrossBlock1) {
  ExpectFailure(sigs.i_i(), {WASM_ZERO, kExprBlock, kI32Code, WASM_ZERO,
                             kExprI32Add, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, BinopsAcrossBlock2) {
  ExpectFailure(sigs.i_i(), {WASM_ZERO, WASM_ZERO, kExprBlock, kI32Code,
                             kExprI32Add, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, BinopsAcrossBlock3) {
  ExpectFailure(sigs.i_i(), {WASM_ZERO, WASM_ZERO, kExprIf, kI32Code,
                             kExprI32Add, kExprElse, kExprI32Add, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, Nop) {
  ExpectValidates(sigs.v_v(), {kExprNop});
}

TEST_F(FunctionBodyDecoderTest, SetLocal0_void) {
  ExpectFailure(sigs.i_i(), {WASM_LOCAL_SET(0, WASM_ZERO)});
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
      ExpectFailure(sigs.v_v(), {WASM_LOCAL_TEE(j, WASM_I32V_1(i))});
      ExpectValidates(sigs.i_i(), {WASM_LOCAL_TEE(j, WASM_I32V_1(i))});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BlockN) {
  constexpr size_t kMaxSize = 200;
  byte buffer[kMaxSize + 3];

  for (size_t i = 0; i <= kMaxSize; i++) {
    memset(buffer, kExprNop, sizeof(buffer));
    buffer[0] = kExprBlock;
    buffer[1] = kVoidCode;
    buffer[i + 2] = kExprEnd;
    ExpectValidates(sigs.v_i(), VectorOf(buffer, i + 3), kAppendEnd);
  }
}

#define WASM_EMPTY_BLOCK kExprBlock, kVoidCode, kExprEnd

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
  byte code[] = {WASM_BLOCK_I(WASM_LOCAL_GET(0))};
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
  byte code[] = {WASM_BLOCK_I(WASM_LOCAL_SET(0, WASM_ZERO), WASM_ZERO)};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.f_ff(), code);
}

TEST_F(FunctionBodyDecoderTest, Block2_fallthru) {
  ExpectValidates(sigs.i_i(), {B2(WASM_LOCAL_SET(0, WASM_ZERO),
                                  WASM_LOCAL_SET(0, WASM_ZERO)),
                               WASM_I32V_1(23)});
}

TEST_F(FunctionBodyDecoderTest, Block3) {
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_LOCAL_SET(0, WASM_ZERO),
                                            WASM_LOCAL_SET(0, WASM_ZERO),
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
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_LOCAL_GET(0))});
  ExpectValidates(sigs.l_l(), {WASM_BLOCK_L(WASM_LOCAL_GET(0))});
  ExpectValidates(sigs.f_f(), {WASM_BLOCK_F(WASM_LOCAL_GET(0))});
  ExpectValidates(sigs.d_d(), {WASM_BLOCK_D(WASM_LOCAL_GET(0))});
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

TEST_F(FunctionBodyDecoderTest, VoidBlockTypeVariants) {
  // Valid kVoidCode encoded in 2 bytes.
  ExpectValidates(sigs.v_v(), {kExprBlock, kVoidCode | 0x80, 0x7F, kExprEnd});
  // Invalid code, whose last 7 bits coincide with kVoidCode.
  ExpectFailure(sigs.v_v(), {kExprBlock, kVoidCode | 0x80, 0x45, kExprEnd},
                kAppendEnd, "invalid block type");
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
  ExpectValidates(sigs.v_i(), {kExprLocalGet, 0, WASM_IF_OP, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, IfSet) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF(WASM_LOCAL_GET(0), WASM_LOCAL_SET(0, WASM_ZERO))});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_LOCAL_SET(0, WASM_ZERO),
                                WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfElseEmpty) {
  ExpectValidates(sigs.v_i(),
                  {WASM_LOCAL_GET(0), WASM_IF_OP, kExprElse, kExprEnd});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfElseUnreachable1) {
  ExpectValidates(
      sigs.i_i(),
      {WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_UNREACHABLE, WASM_LOCAL_GET(0))});
  ExpectValidates(
      sigs.i_i(),
      {WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0), WASM_UNREACHABLE)});
}

TEST_F(FunctionBodyDecoderTest, IfElseUnreachable2) {
  static const byte code[] = {
      WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_UNREACHABLE, WASM_LOCAL_GET(0))};

  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType types[] = {kWasmI32, kValueTypes[i]};
    FunctionSig sig(1, 1, types);

    Validate(kValueTypes[i] == kWasmI32, &sig, code);
  }
}

TEST_F(FunctionBodyDecoderTest, OneArmedIfWithArity) {
  static const byte code[] = {WASM_ZERO, kExprIf, kI32Code, WASM_ONE, kExprEnd};
  ExpectFailure(sigs.i_v(), code, kAppendEnd,
                "start-arity and end-arity of one-armed if must match");
}

TEST_F(FunctionBodyDecoderTest, IfBreak) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0), WASM_BR(0))});
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0), WASM_BR(1))});
  ExpectFailure(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0), WASM_BR(2))});
}

TEST_F(FunctionBodyDecoderTest, IfElseBreak) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_NOP, WASM_BR(0))});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_NOP, WASM_BR(1))});
  ExpectFailure(sigs.v_i(),
                {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_NOP, WASM_BR(2))});
}

TEST_F(FunctionBodyDecoderTest, Block_else) {
  byte code[] = {kExprI32Const, 0, kExprBlock, kExprElse, kExprEnd};
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.i_i(), code);
}

TEST_F(FunctionBodyDecoderTest, IfNop) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0), WASM_NOP)});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, If_end) {
  ExpectValidates(sigs.v_i(), {kExprLocalGet, 0, WASM_IF_OP, kExprEnd});
  ExpectFailure(sigs.v_i(), {kExprLocalGet, 0, WASM_IF_OP, kExprEnd, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, If_falloff1) {
  ExpectFailure(sigs.v_i(), {kExprLocalGet, 0, kExprIf});
  ExpectFailure(sigs.v_i(), {kExprLocalGet, 0, WASM_IF_OP});
  ExpectFailure(sigs.v_i(),
                {kExprLocalGet, 0, WASM_IF_OP, kExprNop, kExprElse});
}

TEST_F(FunctionBodyDecoderTest, IfElseNop) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_LOCAL_SET(0, WASM_ZERO),
                                WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfBlock1) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0),
                                B1(WASM_LOCAL_SET(0, WASM_ZERO)), WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfBlock1b) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0),
                                       B1(WASM_LOCAL_SET(0, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, IfBlock2a) {
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0),
                                       B2(WASM_LOCAL_SET(0, WASM_ZERO),
                                          WASM_LOCAL_SET(0, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, IfBlock2b) {
  ExpectValidates(sigs.v_i(), {WASM_IF_ELSE(WASM_LOCAL_GET(0),
                                            B2(WASM_LOCAL_SET(0, WASM_ZERO),
                                               WASM_LOCAL_SET(0, WASM_ZERO)),
                                            WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, IfElseSet) {
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_LOCAL_SET(0, WASM_ZERO),
                                WASM_LOCAL_SET(0, WASM_I32V_1(1)))});
}

TEST_F(FunctionBodyDecoderTest, Loop0) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP_OP, kExprEnd});
}

TEST_F(FunctionBodyDecoderTest, Loop1) {
  static const byte code[] = {WASM_LOOP(WASM_LOCAL_SET(0, WASM_ZERO))};
  ExpectValidates(sigs.v_i(), code);
  ExpectFailure(sigs.v_v(), code);
  ExpectFailure(sigs.f_ff(), code);
}

TEST_F(FunctionBodyDecoderTest, Loop2) {
  ExpectValidates(sigs.v_i(), {WASM_LOOP(WASM_LOCAL_SET(0, WASM_ZERO),
                                         WASM_LOCAL_SET(0, WASM_ZERO))});
}

TEST_F(FunctionBodyDecoderTest, Loop1_continue) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, Loop1_break) {
  ExpectValidates(sigs.v_v(), {WASM_LOOP(WASM_BR(1))});
}

TEST_F(FunctionBodyDecoderTest, Loop2_continue) {
  ExpectValidates(sigs.v_i(),
                  {WASM_LOOP(WASM_LOCAL_SET(0, WASM_ZERO), WASM_BR(0))});
}

TEST_F(FunctionBodyDecoderTest, Loop2_break) {
  ExpectValidates(sigs.v_i(),
                  {WASM_LOOP(WASM_LOCAL_SET(0, WASM_ZERO), WASM_BR(1))});
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
  ExpectValidates(sigs.i_i(), {WASM_LOOP_I(WASM_LOCAL_GET(0))});
  ExpectValidates(sigs.l_l(), {WASM_LOOP_L(WASM_LOCAL_GET(0))});
  ExpectValidates(sigs.f_f(), {WASM_LOOP_F(WASM_LOCAL_GET(0))});
  ExpectValidates(sigs.d_d(), {WASM_LOOP_D(WASM_LOCAL_GET(0))});
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
  ExpectFailure(sigs.v_v(), {kExprRefFunc, 0});

  ExpectFailure(sigs.v_i(), {kExprLocalGet, 0});
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

TEST_F(FunctionBodyDecoderTest, UnreachableRefTypes) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  WASM_FEATURE_SCOPE(return_call);

  byte function_index = builder.AddFunction(sigs.i_ii());
  byte struct_index = builder.AddStruct({F(kWasmI32, true), F(kWasmI64, true)});
  byte array_index = builder.AddArray(kWasmI32, true);

  ValueType struct_type = ValueType::Ref(struct_index, kNonNullable);
  FunctionSig sig_v_s(0, 1, &struct_type);
  byte struct_consumer = builder.AddFunction(&sig_v_s);

  ExpectValidates(sigs.i_v(), {WASM_UNREACHABLE, kExprRefIsNull});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, kExprRefAsNonNull, kExprDrop});

  ExpectValidates(sigs.i_v(), {WASM_UNREACHABLE, kExprCallRef, WASM_I32V(1)});
  ExpectValidates(sigs.i_v(), {WASM_UNREACHABLE, WASM_REF_FUNC(function_index),
                               kExprCallRef});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, kExprReturnCallRef});

  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_GC_OP(kExprStructNewWithRtt),
                   struct_index, kExprCallFunction, struct_consumer});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, WASM_RTT_CANON(struct_index),
                               WASM_GC_OP(kExprStructNewWithRtt), struct_index,
                               kExprCallFunction, struct_consumer});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, WASM_I64V(42),
                               WASM_RTT_CANON(struct_index),
                               WASM_GC_OP(kExprStructNewWithRtt), struct_index,
                               kExprCallFunction, struct_consumer});
  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_GC_OP(kExprStructNewDefault),
                   struct_index, kExprDrop});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, WASM_RTT_CANON(struct_index),
                               WASM_GC_OP(kExprStructNewDefault), struct_index,
                               kExprCallFunction, struct_consumer});

  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_GC_OP(kExprArrayNewWithRtt),
                   array_index, kExprDrop});
  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_RTT_CANON(array_index),
                   WASM_GC_OP(kExprArrayNewWithRtt), array_index, kExprDrop});
  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_I32V(42), WASM_RTT_CANON(array_index),
                   WASM_GC_OP(kExprArrayNewWithRtt), array_index, kExprDrop});
  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_GC_OP(kExprArrayNewDefault),
                   array_index, kExprDrop});
  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_RTT_CANON(array_index),
                   WASM_GC_OP(kExprArrayNewDefault), array_index, kExprDrop});

  ExpectValidates(sigs.i_v(), {WASM_UNREACHABLE, WASM_GC_OP(kExprRefTest),
                               struct_index, struct_index});
  ExpectValidates(sigs.i_v(),
                  {WASM_UNREACHABLE, WASM_RTT_CANON(struct_index),
                   WASM_GC_OP(kExprRefTest), struct_index, struct_index});

  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, WASM_GC_OP(kExprRefCast),
                               struct_index, struct_index, kExprDrop});
  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, WASM_RTT_CANON(struct_index),
                               WASM_GC_OP(kExprRefCast), struct_index,
                               struct_index, kExprDrop});

  ExpectValidates(sigs.v_v(),
                  {WASM_UNREACHABLE, WASM_GC_OP(kExprRttSub), array_index,
                   WASM_GC_OP(kExprRttSub), array_index, kExprDrop});

  ExpectValidates(sigs.v_v(), {WASM_UNREACHABLE, kExprBrOnNull, 0, WASM_DROP});

  ExpectValidates(&sig_v_s, {WASM_UNREACHABLE, WASM_LOCAL_GET(0), kExprBrOnNull,
                             0, kExprCallFunction, struct_consumer});

  ValueType opt_struct_type = ValueType::Ref(struct_index, kNullable);
  FunctionSig sig_v_os(0, 1, &opt_struct_type);
  ExpectValidates(&sig_v_os,
                  {WASM_UNREACHABLE, WASM_LOCAL_GET(0), kExprBrOnNull, 0,
                   kExprCallFunction, struct_consumer});

  ExpectFailure(
      sigs.v_v(), {WASM_UNREACHABLE, WASM_I32V(42), kExprBrOnNull, 0},
      kAppendEnd,
      "br_on_null[0] expected object reference, found i32.const of type i32");
}

TEST_F(FunctionBodyDecoderTest, If1) {
  ExpectValidates(sigs.i_i(), {WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_I32V_1(9),
                                              WASM_I32V_1(8))});
  ExpectValidates(sigs.i_i(), {WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_I32V_1(9),
                                              WASM_LOCAL_GET(0))});
  ExpectValidates(
      sigs.i_i(),
      {WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0), WASM_I32V_1(8))});
}

TEST_F(FunctionBodyDecoderTest, If_off_end) {
  static const byte kCode[] = {
      WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0), WASM_LOCAL_GET(0))};
  for (size_t len = 3; len < arraysize(kCode); len++) {
    ExpectFailure(sigs.i_i(), VectorOf(kCode, len), kAppendEnd);
    ExpectFailure(sigs.i_i(), VectorOf(kCode, len), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, If_type1) {
  // float|double ? 1 : 2
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_I32V_1(0), WASM_I32V_1(2))};
  ExpectValidates(sigs.i_i(), kCode);
  ExpectFailure(sigs.i_f(), kCode);
  ExpectFailure(sigs.i_d(), kCode);
}

TEST_F(FunctionBodyDecoderTest, If_type2) {
  // 1 ? float|double : 2
  static const byte kCode[] = {
      WASM_IF_ELSE_I(WASM_I32V_1(1), WASM_LOCAL_GET(0), WASM_I32V_1(1))};
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
      WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_NOP, WASM_I32V_1(1))};
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
      ExpectValidates(sigs.l_v(), {WASM_LOCAL_GET(j)});
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

TEST_F(FunctionBodyDecoderTest, MacrosVoid) {
  builder.InitializeMemory();
  ExpectValidates(sigs.v_i(), {WASM_LOCAL_SET(0, WASM_I32V_3(87348))});
  ExpectValidates(
      sigs.v_i(),
      {WASM_STORE_MEM(MachineType::Int32(), WASM_I32V_1(24), WASM_I32V_1(40))});
  ExpectValidates(sigs.v_i(), {WASM_IF(WASM_LOCAL_GET(0), WASM_NOP)});
  ExpectValidates(sigs.v_i(),
                  {WASM_IF_ELSE(WASM_LOCAL_GET(0), WASM_NOP, WASM_NOP)});
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
                  {WASM_I32_ADD(WASM_LOCAL_GET(0), WASM_I32V_1(12))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_I32V_1(13))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_MUL(WASM_LOCAL_GET(0), WASM_I32V_1(14))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_DIVS(WASM_LOCAL_GET(0), WASM_I32V_1(15))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_DIVU(WASM_LOCAL_GET(0), WASM_I32V_1(16))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_REMS(WASM_LOCAL_GET(0), WASM_I32V_1(17))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_REMU(WASM_LOCAL_GET(0), WASM_I32V_1(18))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_AND(WASM_LOCAL_GET(0), WASM_I32V_1(19))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_IOR(WASM_LOCAL_GET(0), WASM_I32V_1(20))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_XOR(WASM_LOCAL_GET(0), WASM_I32V_1(21))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SHL(WASM_LOCAL_GET(0), WASM_I32V_1(22))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SHR(WASM_LOCAL_GET(0), WASM_I32V_1(23))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_SAR(WASM_LOCAL_GET(0), WASM_I32V_1(24))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_ROR(WASM_LOCAL_GET(0), WASM_I32V_1(24))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_ROL(WASM_LOCAL_GET(0), WASM_I32V_1(24))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_I32V_1(25))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_NE(WASM_LOCAL_GET(0), WASM_I32V_1(25))});

  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LTS(WASM_LOCAL_GET(0), WASM_I32V_1(26))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LES(WASM_LOCAL_GET(0), WASM_I32V_1(27))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LTU(WASM_LOCAL_GET(0), WASM_I32V_1(28))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_LEU(WASM_LOCAL_GET(0), WASM_I32V_1(29))});

  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GTS(WASM_LOCAL_GET(0), WASM_I32V_1(26))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GES(WASM_LOCAL_GET(0), WASM_I32V_1(27))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GTU(WASM_LOCAL_GET(0), WASM_I32V_1(28))});
  ExpectValidates(sigs.i_i(),
                  {WASM_I32_GEU(WASM_LOCAL_GET(0), WASM_I32V_1(29))});
}

TEST_F(FunctionBodyDecoderTest, MacrosInt64) {
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_ADD(WASM_LOCAL_GET(0), WASM_I64V_1(12))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SUB(WASM_LOCAL_GET(0), WASM_I64V_1(13))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_MUL(WASM_LOCAL_GET(0), WASM_I64V_1(14))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_DIVS(WASM_LOCAL_GET(0), WASM_I64V_1(15))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_DIVU(WASM_LOCAL_GET(0), WASM_I64V_1(16))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_REMS(WASM_LOCAL_GET(0), WASM_I64V_1(17))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_REMU(WASM_LOCAL_GET(0), WASM_I64V_1(18))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_AND(WASM_LOCAL_GET(0), WASM_I64V_1(19))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_IOR(WASM_LOCAL_GET(0), WASM_I64V_1(20))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_XOR(WASM_LOCAL_GET(0), WASM_I64V_1(21))});

  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SHL(WASM_LOCAL_GET(0), WASM_I64V_1(22))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SHR(WASM_LOCAL_GET(0), WASM_I64V_1(23))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_SAR(WASM_LOCAL_GET(0), WASM_I64V_1(24))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_ROR(WASM_LOCAL_GET(0), WASM_I64V_1(24))});
  ExpectValidates(sigs.l_ll(),
                  {WASM_I64_ROL(WASM_LOCAL_GET(0), WASM_I64V_1(24))});

  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LTS(WASM_LOCAL_GET(0), WASM_I64V_1(26))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LES(WASM_LOCAL_GET(0), WASM_I64V_1(27))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LTU(WASM_LOCAL_GET(0), WASM_I64V_1(28))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_LEU(WASM_LOCAL_GET(0), WASM_I64V_1(29))});

  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GTS(WASM_LOCAL_GET(0), WASM_I64V_1(26))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GES(WASM_LOCAL_GET(0), WASM_I64V_1(27))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GTU(WASM_LOCAL_GET(0), WASM_I64V_1(28))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_GEU(WASM_LOCAL_GET(0), WASM_I64V_1(29))});

  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_EQ(WASM_LOCAL_GET(0), WASM_I64V_1(25))});
  ExpectValidates(sigs.i_ll(),
                  {WASM_I64_NE(WASM_LOCAL_GET(0), WASM_I64V_1(25))});
}

TEST_F(FunctionBodyDecoderTest, AllSimpleExpressions) {
  WASM_FEATURE_SCOPE(reftypes);
// Test all simple expressions which are described by a signature.
#define DECODE_TEST(name, opcode, sig)                            \
  {                                                               \
    const FunctionSig* sig = WasmOpcodes::Signature(kExpr##name); \
    if (sig->parameter_count() == 1) {                            \
      TestUnop(kExpr##name, sig);                                 \
    } else {                                                      \
      TestBinop(kExpr##name, sig);                                \
    }                                                             \
  }

  FOREACH_SIMPLE_OPCODE(DECODE_TEST);

#undef DECODE_TEST
}

TEST_F(FunctionBodyDecoderTest, MemorySize) {
  builder.InitializeMemory();
  byte code[] = {kExprMemorySize, 0};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.f_ff(), code);
}

TEST_F(FunctionBodyDecoderTest, LoadMemOffset) {
  builder.InitializeMemory();
  for (int offset = 0; offset < 128; offset += 7) {
    byte code[] = {kExprI32Const, 0, kExprI32LoadMem, ZERO_ALIGNMENT,
                   static_cast<byte>(offset)};
    ExpectValidates(sigs.i_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, LoadMemAlignment) {
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
  builder.InitializeMemory();
  for (byte offset = 0; offset < 128; offset += 7) {
    byte code[] = {WASM_STORE_MEM_OFFSET(MachineType::Int32(), offset,
                                         WASM_ZERO, WASM_ZERO)};
    ExpectValidates(sigs.v_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, StoreMemOffset_void) {
  builder.InitializeMemory();
  ExpectFailure(sigs.i_i(), {WASM_STORE_MEM_OFFSET(MachineType::Int32(), 0,
                                                   WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, LoadMemOffset_varint) {
  builder.InitializeMemory();
  ExpectValidates(sigs.i_i(),
                  {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT, U32V_1(0x45)});
  ExpectValidates(sigs.i_i(), {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                               U32V_2(0x3999)});
  ExpectValidates(sigs.i_i(), {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                               U32V_3(0x344445)});
  ExpectValidates(sigs.i_i(), {WASM_ZERO, kExprI32LoadMem, ZERO_ALIGNMENT,
                               U32V_4(0x36666667)});
}

TEST_F(FunctionBodyDecoderTest, StoreMemOffset_varint) {
  builder.InitializeMemory();
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, U32V_1(0x33)});
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, U32V_2(0x1111)});
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, U32V_3(0x222222)});
  ExpectValidates(sigs.v_i(), {WASM_ZERO, WASM_ZERO, kExprI32StoreMem,
                               ZERO_ALIGNMENT, U32V_4(0x44444444)});
}

TEST_F(FunctionBodyDecoderTest, AllLoadMemCombinations) {
  builder.InitializeMemory();
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType local_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_LOAD_MEM(mem_type, WASM_ZERO)};
      FunctionSig sig(1, 0, &local_type);
      Validate(local_type == ValueType::For(mem_type), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, AllStoreMemCombinations) {
  builder.InitializeMemory();
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType local_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(machineTypes); j++) {
      MachineType mem_type = machineTypes[j];
      byte code[] = {WASM_STORE_MEM(mem_type, WASM_ZERO, WASM_LOCAL_GET(0))};
      FunctionSig sig(0, 1, &local_type);
      Validate(local_type == ValueType::For(mem_type), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, SimpleCalls) {
  const FunctionSig* sig = sigs.i_i();

  builder.AddFunction(sigs.i_v());
  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());

  ExpectValidates(sig, {WASM_CALL_FUNCTION0(0)});
  ExpectValidates(sig, {WASM_CALL_FUNCTION(1, WASM_I32V_1(27))});
  ExpectValidates(sig,
                  {WASM_CALL_FUNCTION(2, WASM_I32V_1(37), WASM_I32V_2(77))});
}

TEST_F(FunctionBodyDecoderTest, CallsWithTooFewArguments) {
  const FunctionSig* sig = sigs.i_i();

  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());
  builder.AddFunction(sigs.f_ff());

  ExpectFailure(sig, {WASM_CALL_FUNCTION0(0)});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(1, WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(2, WASM_LOCAL_GET(0))});
}

TEST_F(FunctionBodyDecoderTest, CallsWithMismatchedSigs2) {
  const FunctionSig* sig = sigs.i_i();

  builder.AddFunction(sigs.i_i());

  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_I64V_1(17))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_F32(17.1))});
  ExpectFailure(sig, {WASM_CALL_FUNCTION(0, WASM_F64(17.1))});
}

TEST_F(FunctionBodyDecoderTest, CallsWithMismatchedSigs3) {
  const FunctionSig* sig = sigs.i_i();

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

  const FunctionSig* sig = sigs.i_i();

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

  const FunctionSig* sig = sigs.i_i();

  builder.AddFunction(sigs.i_i());
  builder.AddFunction(sigs.i_ii());
  builder.AddFunction(sigs.f_ff());

  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION0(0)});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(1, WASM_ZERO)});
  ExpectFailure(sig, {WASM_RETURN_CALL_FUNCTION(2, WASM_LOCAL_GET(0))});
}

TEST_F(FunctionBodyDecoderTest, ReturnCallsWithMismatchedSigs) {
  WASM_FEATURE_SCOPE(return_call);

  const FunctionSig* sig = sigs.i_i();

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

  const FunctionSig* sig = sigs.i_i();
  builder.AddTable(kWasmFuncRef, 20, true, 30);

  byte sig0 = builder.AddSignature(sigs.i_v());
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());

  ExpectValidates(sig, {WASM_RETURN_CALL_INDIRECT(sig0, WASM_ZERO)});
  ExpectValidates(
      sig, {WASM_RETURN_CALL_INDIRECT(sig1, WASM_I32V_1(22), WASM_ZERO)});
  ExpectValidates(sig, {WASM_RETURN_CALL_INDIRECT(sig2, WASM_I32V_1(32),
                                                  WASM_I32V_2(72), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IndirectReturnCallsOutOfBounds) {
  WASM_FEATURE_SCOPE(return_call);

  const FunctionSig* sig = sigs.i_i();
  builder.AddTable(kWasmFuncRef, 20, false, 20);

  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(0, WASM_ZERO)});
  builder.AddSignature(sigs.i_v());
  ExpectValidates(sig, {WASM_RETURN_CALL_INDIRECT(0, WASM_ZERO)});

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(1, WASM_I32V_1(22), WASM_ZERO)});
  builder.AddSignature(sigs.i_i());
  ExpectValidates(sig,
                  {WASM_RETURN_CALL_INDIRECT(1, WASM_I32V_1(27), WASM_ZERO)});

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(2, WASM_I32V_1(27), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IndirectReturnCallsWithMismatchedSigs3) {
  WASM_FEATURE_SCOPE(return_call);

  const FunctionSig* sig = sigs.i_i();
  builder.InitializeTable(wasm::kWasmVoid);

  byte sig0 = builder.AddSignature(sigs.i_f());

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig0, WASM_I32V_1(17), WASM_ZERO)});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig0, WASM_I64V_1(27), WASM_ZERO)});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig0, WASM_F64(37.2), WASM_ZERO)});

  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(sig0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(sig0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(sig0, WASM_F64(37.2))});

  byte sig1 = builder.AddFunction(sigs.i_d());

  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig1, WASM_I32V_1(16), WASM_ZERO)});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig1, WASM_I64V_1(16), WASM_ZERO)});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig1, WASM_F32(17.6), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IndirectReturnCallsWithoutTableCrash) {
  WASM_FEATURE_SCOPE(return_call);

  const FunctionSig* sig = sigs.i_i();

  byte sig0 = builder.AddSignature(sigs.i_v());
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());

  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(sig0, WASM_ZERO)});
  ExpectFailure(sig,
                {WASM_RETURN_CALL_INDIRECT(sig1, WASM_I32V_1(22), WASM_ZERO)});
  ExpectFailure(sig, {WASM_RETURN_CALL_INDIRECT(sig2, WASM_I32V_1(32),
                                                WASM_I32V_2(72), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IncompleteIndirectReturnCall) {
  const FunctionSig* sig = sigs.i_i();
  builder.InitializeTable(wasm::kWasmVoid);

  static byte code[] = {kExprReturnCallIndirect};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, MultiReturn) {
  WASM_FEATURE_SCOPE(mv);
  ValueType storage[] = {kWasmI32, kWasmI32};
  FunctionSig sig_ii_v(2, 0, storage);
  FunctionSig sig_v_ii(0, 2, storage);

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

          if (IsSubtypeOf(kValueTypes[c], kValueTypes[a], module) &&
              IsSubtypeOf(kValueTypes[d], kValueTypes[b], module)) {
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
  const FunctionSig* sig = sigs.i_i();
  builder.AddTable(kWasmFuncRef, 20, false, 20);

  byte sig0 = builder.AddSignature(sigs.i_v());
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());

  ExpectValidates(sig, {WASM_CALL_INDIRECT(sig0, WASM_ZERO)});
  ExpectValidates(sig, {WASM_CALL_INDIRECT(sig1, WASM_I32V_1(22), WASM_ZERO)});
  ExpectValidates(sig, {WASM_CALL_INDIRECT(sig2, WASM_I32V_1(32),
                                           WASM_I32V_2(72), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsOutOfBounds) {
  const FunctionSig* sig = sigs.i_i();
  builder.AddTable(kWasmFuncRef, 20, false, 20);

  ExpectFailure(sig, {WASM_CALL_INDIRECT(0, WASM_ZERO)});
  builder.AddSignature(sigs.i_v());
  ExpectValidates(sig, {WASM_CALL_INDIRECT(0, WASM_ZERO)});

  ExpectFailure(sig, {WASM_CALL_INDIRECT(1, WASM_I32V_1(22), WASM_ZERO)});
  builder.AddSignature(sigs.i_i());
  ExpectValidates(sig, {WASM_CALL_INDIRECT(1, WASM_I32V_1(27), WASM_ZERO)});

  ExpectFailure(sig, {WASM_CALL_INDIRECT(2, WASM_I32V_1(27), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsWithMismatchedSigs1) {
  const FunctionSig* sig = sigs.i_i();
  builder.InitializeTable(wasm::kWasmVoid);

  byte sig0 = builder.AddSignature(sigs.i_f());

  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_I32V_1(17), WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_I64V_1(27), WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_F64(37.2), WASM_ZERO)});

  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_I32V_1(17))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_I64V_1(27))});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_F64(37.2))});

  byte sig1 = builder.AddFunction(sigs.i_d());

  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig1, WASM_I32V_1(16), WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig1, WASM_I64V_1(16), WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig1, WASM_F32(17.6), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsWithMismatchedSigs2) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  byte table_type_index = builder.AddSignature(sigs.i_i());
  byte table_index =
      builder.InitializeTable(ValueType::Ref(table_type_index, kNullable));

  ExpectValidates(sigs.i_v(),
                  {WASM_CALL_INDIRECT_TABLE(table_index, table_type_index,
                                            WASM_I32V_1(42), WASM_ZERO)});

  byte wrong_type_index = builder.AddSignature(sigs.i_ii());
  ExpectFailure(sigs.i_v(),
                {WASM_CALL_INDIRECT_TABLE(table_index, wrong_type_index,
                                          WASM_I32V_1(42), WASM_ZERO)},
                kAppendEnd,
                "call_indirect: Immediate signature #1 is not a subtype of "
                "immediate table #0");

  byte non_function_table_index = builder.InitializeTable(kWasmExternRef);
  ExpectFailure(
      sigs.i_v(),
      {WASM_CALL_INDIRECT_TABLE(non_function_table_index, table_type_index,
                                WASM_I32V_1(42), WASM_ZERO)},
      kAppendEnd,
      "call_indirect: immediate table #1 is not of a function type");
}

TEST_F(FunctionBodyDecoderTest, IndirectCallsWithoutTableCrash) {
  const FunctionSig* sig = sigs.i_i();

  byte sig0 = builder.AddSignature(sigs.i_v());
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());

  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig0, WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig1, WASM_I32V_1(22), WASM_ZERO)});
  ExpectFailure(sig, {WASM_CALL_INDIRECT(sig2, WASM_I32V_1(32), WASM_I32V_2(72),
                                         WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, IncompleteIndirectCall) {
  const FunctionSig* sig = sigs.i_i();
  builder.InitializeTable(wasm::kWasmVoid);

  static byte code[] = {kExprCallIndirect};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, IncompleteStore) {
  const FunctionSig* sig = sigs.i_i();
  builder.InitializeMemory();
  builder.InitializeTable(wasm::kWasmVoid);

  static byte code[] = {kExprI32StoreMem};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, IncompleteI8x16Shuffle) {
  WASM_FEATURE_SCOPE(simd);
  const FunctionSig* sig = sigs.i_i();
  builder.InitializeMemory();
  builder.InitializeTable(wasm::kWasmVoid);

  static byte code[] = {kSimdPrefix,
                        static_cast<byte>(kExprI8x16Shuffle & 0xff)};
  ExpectFailure(sig, ArrayVector(code), kOmitEnd);
}

TEST_F(FunctionBodyDecoderTest, SimpleImportCalls) {
  const FunctionSig* sig = sigs.i_i();

  byte f0 = builder.AddImport(sigs.i_v());
  byte f1 = builder.AddImport(sigs.i_i());
  byte f2 = builder.AddImport(sigs.i_ii());

  ExpectValidates(sig, {WASM_CALL_FUNCTION0(f0)});
  ExpectValidates(sig, {WASM_CALL_FUNCTION(f1, WASM_I32V_1(22))});
  ExpectValidates(sig,
                  {WASM_CALL_FUNCTION(f2, WASM_I32V_1(32), WASM_I32V_2(72))});
}

TEST_F(FunctionBodyDecoderTest, ImportCallsWithMismatchedSigs3) {
  const FunctionSig* sig = sigs.i_i();

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
  const FunctionSig* sig = sigs.i_i();

  builder.AddGlobal(kWasmI32);

  ExpectValidates(sig, {WASM_GLOBAL_GET(0)});
  ExpectFailure(sig, {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0))});
  ExpectValidates(sig, {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0)), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, ImmutableGlobal) {
  const FunctionSig* sig = sigs.v_v();

  uint32_t g0 = builder.AddGlobal(kWasmI32, true);
  uint32_t g1 = builder.AddGlobal(kWasmI32, false);

  ExpectValidates(sig, {WASM_GLOBAL_SET(g0, WASM_ZERO)});
  ExpectFailure(sig, {WASM_GLOBAL_SET(g1, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Int32Globals_fail) {
  const FunctionSig* sig = sigs.i_i();

  builder.AddGlobal(kWasmI64);
  builder.AddGlobal(kWasmI64);
  builder.AddGlobal(kWasmF32);
  builder.AddGlobal(kWasmF64);

  ExpectFailure(sig, {WASM_GLOBAL_GET(0)});
  ExpectFailure(sig, {WASM_GLOBAL_GET(1)});
  ExpectFailure(sig, {WASM_GLOBAL_GET(2)});
  ExpectFailure(sig, {WASM_GLOBAL_GET(3)});

  ExpectFailure(sig, {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0)), WASM_ZERO});
  ExpectFailure(sig, {WASM_GLOBAL_SET(1, WASM_LOCAL_GET(0)), WASM_ZERO});
  ExpectFailure(sig, {WASM_GLOBAL_SET(2, WASM_LOCAL_GET(0)), WASM_ZERO});
  ExpectFailure(sig, {WASM_GLOBAL_SET(3, WASM_LOCAL_GET(0)), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, Int64Globals) {
  const FunctionSig* sig = sigs.l_l();

  builder.AddGlobal(kWasmI64);
  builder.AddGlobal(kWasmI64);

  ExpectValidates(sig, {WASM_GLOBAL_GET(0)});
  ExpectValidates(sig, {WASM_GLOBAL_GET(1)});

  ExpectValidates(sig,
                  {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0)), WASM_LOCAL_GET(0)});
  ExpectValidates(sig,
                  {WASM_GLOBAL_SET(1, WASM_LOCAL_GET(0)), WASM_LOCAL_GET(0)});
}

TEST_F(FunctionBodyDecoderTest, Float32Globals) {
  const FunctionSig* sig = sigs.f_ff();

  builder.AddGlobal(kWasmF32);

  ExpectValidates(sig, {WASM_GLOBAL_GET(0)});
  ExpectValidates(sig,
                  {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0)), WASM_LOCAL_GET(0)});
}

TEST_F(FunctionBodyDecoderTest, Float64Globals) {
  const FunctionSig* sig = sigs.d_dd();

  builder.AddGlobal(kWasmF64);

  ExpectValidates(sig, {WASM_GLOBAL_GET(0)});
  ExpectValidates(sig,
                  {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0)), WASM_LOCAL_GET(0)});
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
      Validate(IsSubtypeOf(global_type, local_type, module), &sig,
               {WASM_GLOBAL_GET(0)});
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
      Validate(IsSubtypeOf(local_type, global_type, module), &sig,
               {WASM_GLOBAL_SET(0, WASM_LOCAL_GET(0))});
    }
  }
}

TEST_F(FunctionBodyDecoderTest, TableSet) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);

  byte tab_type = builder.AddSignature(sigs.i_i());
  byte tab_ref1 = builder.AddTable(kWasmExternRef, 10, true, 20);
  byte tab_func1 = builder.AddTable(kWasmFuncRef, 20, true, 30);
  byte tab_func2 = builder.AddTable(kWasmFuncRef, 10, false, 20);
  byte tab_ref2 = builder.AddTable(kWasmExternRef, 10, false, 20);
  byte tab_typed_func =
      builder.AddTable(ValueType::Ref(tab_type, kNullable), 10, false, 20);

  ValueType sig_types[]{kWasmExternRef, kWasmFuncRef, kWasmI32,
                        ValueType::Ref(tab_type, kNonNullable)};
  FunctionSig sig(0, 4, sig_types);
  byte local_ref = 0;
  byte local_func = 1;
  byte local_int = 2;
  byte local_typed_func = 3;

  ExpectValidates(&sig, {WASM_TABLE_SET(tab_ref1, WASM_I32V(6),
                                        WASM_LOCAL_GET(local_ref))});
  ExpectValidates(&sig, {WASM_TABLE_SET(tab_func1, WASM_I32V(5),
                                        WASM_LOCAL_GET(local_func))});
  ExpectValidates(&sig, {WASM_TABLE_SET(tab_func2, WASM_I32V(7),
                                        WASM_LOCAL_GET(local_func))});
  ExpectValidates(&sig, {WASM_TABLE_SET(tab_ref2, WASM_I32V(8),
                                        WASM_LOCAL_GET(local_ref))});
  ExpectValidates(&sig, {WASM_TABLE_SET(tab_typed_func, WASM_I32V(8),
                                        WASM_LOCAL_GET(local_typed_func))});
  ExpectValidates(&sig, {WASM_TABLE_SET(tab_func1, WASM_I32V(8),
                                        WASM_LOCAL_GET(local_typed_func))});

  // Only values of the correct type can be set to a table.
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_ref1, WASM_I32V(4),
                                      WASM_LOCAL_GET(local_func))});
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_func1, WASM_I32V(9),
                                      WASM_LOCAL_GET(local_ref))});
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_func2, WASM_I32V(3),
                                      WASM_LOCAL_GET(local_ref))});
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_ref2, WASM_I32V(2),
                                      WASM_LOCAL_GET(local_func))});
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_ref1, WASM_I32V(9),
                                      WASM_LOCAL_GET(local_int))});
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_func1, WASM_I32V(3),
                                      WASM_LOCAL_GET(local_int))});
  ExpectFailure(&sig, {WASM_TABLE_SET(tab_typed_func, WASM_I32V(3),
                                      WASM_LOCAL_GET(local_func))});

  // Out-of-bounds table index should fail.
  byte oob_tab = 37;
  ExpectFailure(
      &sig, {WASM_TABLE_SET(oob_tab, WASM_I32V(9), WASM_LOCAL_GET(local_ref))});
  ExpectFailure(&sig, {WASM_TABLE_SET(oob_tab, WASM_I32V(3),
                                      WASM_LOCAL_GET(local_func))});
}

TEST_F(FunctionBodyDecoderTest, TableGet) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);

  byte tab_type = builder.AddSignature(sigs.i_i());
  byte tab_ref1 = builder.AddTable(kWasmExternRef, 10, true, 20);
  byte tab_func1 = builder.AddTable(kWasmFuncRef, 20, true, 30);
  byte tab_func2 = builder.AddTable(kWasmFuncRef, 10, false, 20);
  byte tab_ref2 = builder.AddTable(kWasmExternRef, 10, false, 20);
  byte tab_typed_func =
      builder.AddTable(ValueType::Ref(tab_type, kNullable), 10, false, 20);

  ValueType sig_types[]{kWasmExternRef, kWasmFuncRef, kWasmI32,
                        ValueType::Ref(tab_type, kNullable)};
  FunctionSig sig(0, 4, sig_types);
  byte local_ref = 0;
  byte local_func = 1;
  byte local_int = 2;
  byte local_typed_func = 3;

  ExpectValidates(
      &sig,
      {WASM_LOCAL_SET(local_ref, WASM_TABLE_GET(tab_ref1, WASM_I32V(6)))});
  ExpectValidates(
      &sig,
      {WASM_LOCAL_SET(local_ref, WASM_TABLE_GET(tab_ref2, WASM_I32V(8)))});
  ExpectValidates(
      &sig,
      {WASM_LOCAL_SET(local_func, WASM_TABLE_GET(tab_func1, WASM_I32V(5)))});
  ExpectValidates(
      &sig,
      {WASM_LOCAL_SET(local_func, WASM_TABLE_GET(tab_func2, WASM_I32V(7)))});
  ExpectValidates(
      &sig, {WASM_LOCAL_SET(local_ref, WASM_SEQ(WASM_I32V(6), kExprTableGet,
                                                U32V_2(tab_ref1)))});
  ExpectValidates(
      &sig, {WASM_LOCAL_SET(local_func,
                            WASM_TABLE_GET(tab_typed_func, WASM_I32V(7)))});
  ExpectValidates(
      &sig, {WASM_LOCAL_SET(local_typed_func,
                            WASM_TABLE_GET(tab_typed_func, WASM_I32V(7)))});

  // We cannot store references as any other type.
  ExpectFailure(&sig, {WASM_LOCAL_SET(local_func,
                                      WASM_TABLE_GET(tab_ref1, WASM_I32V(4)))});
  ExpectFailure(&sig, {WASM_LOCAL_SET(
                          local_ref, WASM_TABLE_GET(tab_func1, WASM_I32V(9)))});
  ExpectFailure(&sig, {WASM_LOCAL_SET(
                          local_ref, WASM_TABLE_GET(tab_func2, WASM_I32V(3)))});
  ExpectFailure(&sig, {WASM_LOCAL_SET(local_func,
                                      WASM_TABLE_GET(tab_ref2, WASM_I32V(2)))});

  ExpectFailure(&sig, {WASM_LOCAL_SET(local_int,
                                      WASM_TABLE_GET(tab_ref1, WASM_I32V(9)))});
  ExpectFailure(&sig, {WASM_LOCAL_SET(
                          local_int, WASM_TABLE_GET(tab_func1, WASM_I32V(3)))});
  ExpectFailure(&sig,
                {WASM_LOCAL_SET(local_typed_func,
                                WASM_TABLE_GET(tab_func1, WASM_I32V(3)))});

  // Out-of-bounds table index should fail.
  byte oob_tab = 37;
  ExpectFailure(
      &sig, {WASM_LOCAL_SET(local_ref, WASM_TABLE_GET(oob_tab, WASM_I32V(9)))});
  ExpectFailure(&sig, {WASM_LOCAL_SET(local_func,
                                      WASM_TABLE_GET(oob_tab, WASM_I32V(3)))});
}

TEST_F(FunctionBodyDecoderTest, MultiTableCallIndirect) {
  WASM_FEATURE_SCOPE(reftypes);
  byte tab_ref = builder.AddTable(kWasmExternRef, 10, true, 20);
  byte tab_func = builder.AddTable(kWasmFuncRef, 20, true, 30);

  ValueType sig_types[]{kWasmExternRef, kWasmFuncRef, kWasmI32};
  FunctionSig sig(0, 3, sig_types);
  byte sig_index = builder.AddSignature(sigs.i_v());

  // We can store funcref values as externref, but not the other way around.
  ExpectValidates(sigs.i_v(),
                  {kExprI32Const, 0, kExprCallIndirect, sig_index, tab_func});

  ExpectFailure(sigs.i_v(),
                {kExprI32Const, 0, kExprCallIndirect, sig_index, tab_ref});
}

TEST_F(FunctionBodyDecoderTest, WasmMemoryGrow) {
  builder.InitializeMemory();

  byte code[] = {WASM_LOCAL_GET(0), kExprMemoryGrow, 0};
  ExpectValidates(sigs.i_i(), code);
  ExpectFailure(sigs.i_d(), code);
}

TEST_F(FunctionBodyDecoderTest, AsmJsBinOpsCheckOrigin) {
  ValueType float32int32float32[] = {kWasmF32, kWasmI32, kWasmF32};
  FunctionSig sig_f_if(1, 2, float32int32float32);
  ValueType float64int32float64[] = {kWasmF64, kWasmI32, kWasmF64};
  FunctionSig sig_d_id(1, 2, float64int32float64);
  struct {
    WasmOpcode op;
    const FunctionSig* sig;
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
    TestModuleBuilder builder(kAsmJsSloppyOrigin);
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
                    {WASM_BINOP(AsmJsBinOps[i].op, WASM_LOCAL_GET(0),
                                WASM_LOCAL_GET(1))});
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
    const FunctionSig* sig;
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
    TestModuleBuilder builder(kAsmJsSloppyOrigin);
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
                    {WASM_UNOP(AsmJsUnOps[i].op, WASM_LOCAL_GET(0))});
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
        WASM_LOOP(WASM_IF(WASM_LOCAL_GET(0), WASM_BRV(i + 1, WASM_ZERO)),
                  WASM_LOCAL_SET(0, WASM_I32V_1(1))),
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
          code[pos + 1] = kVoidCode;
        }
        m >>= 1;
      }

      Validate(i <= depth, sigs.v_v(), code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheck) {
  for (const FunctionSig* sig :
       {sigs.i_i(), sigs.l_l(), sigs.f_ff(), sigs.d_dd()}) {
    // unify X and X => OK
    byte code[] = {WASM_BLOCK_T(
        sig->GetReturn(), WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_LOCAL_GET(0))),
        WASM_LOCAL_GET(0))};
    ExpectValidates(sig, code);
  }

  // unify i32 and f32 => fail
  ExpectFailure(sigs.i_i(),
                {WASM_BLOCK_I(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_ZERO)),
                              WASM_F32(1.2))});

  // unify f64 and f64 => OK
  ExpectValidates(
      sigs.d_dd(),
      {WASM_BLOCK_D(WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_LOCAL_GET(0))),
                    WASM_F64(1.2))});
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheckAll1) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType storage[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_BLOCK_T(
          sig.GetReturn(), WASM_IF(WASM_ZERO, WASM_BRV(0, WASM_LOCAL_GET(0))),
          WASM_LOCAL_GET(1))};

      Validate(IsSubtypeOf(kValueTypes[j], kValueTypes[i], module), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheckAll2) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType storage[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_IF_ELSE_T(sig.GetReturn(0), WASM_ZERO,
                                    WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)),
                                    WASM_LOCAL_GET(1))};

      Validate(IsSubtypeOf(kValueTypes[j], kValueTypes[i], module), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, Break_TypeCheckAll3) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType storage[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_IF_ELSE_T(sig.GetReturn(), WASM_ZERO,
                                    WASM_LOCAL_GET(1),
                                    WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))};

      Validate(IsSubtypeOf(kValueTypes[j], kValueTypes[i], module), &sig, code);
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
          type, WASM_IF(WASM_ZERO, WASM_BRV(1, WASM_LOCAL_GET(which))),
          WASM_LOCAL_GET(which ^ 1))};

      Validate(type == kWasmI32, &sig, code1);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakIf_cond_type) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType types[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j]};
      FunctionSig sig(1, 2, types);
      byte code[] = {WASM_BLOCK_T(
          types[0], WASM_BRV_IF(0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)))};

      Validate(types[2] == kWasmI32, &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakIf_val_type) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueType types[] = {kValueTypes[i], kValueTypes[i], kValueTypes[j],
                           kWasmI32};
      FunctionSig sig(1, 3, types);
      byte code[] = {WASM_BLOCK_T(
          types[1], WASM_BRV_IF(0, WASM_LOCAL_GET(1), WASM_LOCAL_GET(2)),
          WASM_DROP, WASM_LOCAL_GET(0))};

      Validate(IsSubtypeOf(kValueTypes[j], kValueTypes[i], module), &sig, code);
    }
  }
}

TEST_F(FunctionBodyDecoderTest, BreakIf_Unify) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < arraysize(kValueTypes); i++) {
      ValueType type = kValueTypes[i];
      ValueType storage[] = {kWasmI32, kWasmI32, type};
      FunctionSig sig(1, 2, storage);
      byte code[] = {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(which)),
                                  WASM_DROP, WASM_LOCAL_GET(which ^ 1))};

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
  static byte code[] = {B1(WASM_BR_TABLE(WASM_LOCAL_GET(0), 0, BR_TARGET(0)))};
  for (size_t len = 1; len < sizeof(code); len++) {
    ExpectFailure(sigs.i_i(), VectorOf(code, len), kAppendEnd);
    ExpectFailure(sigs.i_i(), VectorOf(code, len), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, BrTable_invalid_br1) {
  for (int depth = 0; depth < 4; depth++) {
    byte code[] = {B1(WASM_BR_TABLE(WASM_LOCAL_GET(0), 0, BR_TARGET(depth)))};
    Validate(depth <= 1, sigs.v_i(), code);
  }
}

TEST_F(FunctionBodyDecoderTest, BrTable_invalid_br2) {
  for (int depth = 0; depth < 7; depth++) {
    byte code[] = {
        WASM_LOOP(WASM_BR_TABLE(WASM_LOCAL_GET(0), 0, BR_TARGET(depth)))};
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
                  {WASM_LOCAL_GET(0), kExprBrTable, 0, BR_TARGET(0)});
}

TEST_F(FunctionBodyDecoderTest, BrUnreachable2) {
  ExpectValidates(sigs.v_i(),
                  {WASM_LOCAL_GET(0), kExprBrTable, 0, BR_TARGET(0), WASM_NOP});
  ExpectFailure(sigs.v_i(),
                {WASM_LOCAL_GET(0), kExprBrTable, 0, BR_TARGET(0), WASM_ZERO});
}

TEST_F(FunctionBodyDecoderTest, Brv1) {
  ExpectValidates(sigs.i_i(), {WASM_BLOCK_I(WASM_BRV(0, WASM_ZERO))});
  ExpectValidates(sigs.i_i(),
                  {WASM_BLOCK_I(WASM_LOOP_I(WASM_BRV(2, WASM_ZERO)))});
}

TEST_F(FunctionBodyDecoderTest, Brv1_type) {
  ExpectValidates(sigs.i_ii(), {WASM_BLOCK_I(WASM_BRV(0, WASM_LOCAL_GET(0)))});
  ExpectValidates(sigs.l_ll(), {WASM_BLOCK_L(WASM_BRV(0, WASM_LOCAL_GET(0)))});
  ExpectValidates(sigs.f_ff(), {WASM_BLOCK_F(WASM_BRV(0, WASM_LOCAL_GET(0)))});
  ExpectValidates(sigs.d_dd(), {WASM_BLOCK_D(WASM_BRV(0, WASM_LOCAL_GET(0)))});
}

TEST_F(FunctionBodyDecoderTest, Brv1_type_n) {
  ExpectFailure(sigs.i_f(), {WASM_BLOCK_I(WASM_BRV(0, WASM_LOCAL_GET(0)))});
  ExpectFailure(sigs.i_d(), {WASM_BLOCK_I(WASM_BRV(0, WASM_LOCAL_GET(0)))});
}

TEST_F(FunctionBodyDecoderTest, BrvIf1) {
  ExpectValidates(sigs.i_v(), {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_ZERO))});
}

TEST_F(FunctionBodyDecoderTest, BrvIf1_type) {
  ExpectValidates(sigs.i_i(),
                  {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))});
  ExpectValidates(sigs.l_l(),
                  {WASM_BLOCK_L(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))});
  ExpectValidates(sigs.f_ff(),
                  {WASM_BLOCK_F(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))});
  ExpectValidates(sigs.d_dd(),
                  {WASM_BLOCK_D(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))});
}

TEST_F(FunctionBodyDecoderTest, BrvIf1_type_n) {
  ExpectFailure(sigs.i_f(),
                {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))});
  ExpectFailure(sigs.i_d(),
                {WASM_BLOCK_I(WASM_BRV_IF_ZERO(0, WASM_LOCAL_GET(0)))});
}

TEST_F(FunctionBodyDecoderTest, Select) {
  ExpectValidates(sigs.i_i(), {WASM_SELECT(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                           WASM_ZERO)});
  ExpectValidates(sigs.f_ff(),
                  {WASM_SELECT(WASM_F32(0.0), WASM_F32(0.0), WASM_ZERO)});
  ExpectValidates(sigs.d_dd(),
                  {WASM_SELECT(WASM_F64(0.0), WASM_F64(0.0), WASM_ZERO)});
  ExpectValidates(sigs.l_l(),
                  {WASM_SELECT(WASM_I64V_1(0), WASM_I64V_1(0), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Select_needs_value_type) {
  WASM_FEATURE_SCOPE(reftypes);
  ExpectFailure(sigs.e_e(),
                {WASM_SELECT(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0), WASM_ZERO)});
  ExpectFailure(sigs.c_c(),
                {WASM_SELECT(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Select_fail1) {
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_F32(0.0), WASM_LOCAL_GET(0),
                                         WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_LOCAL_GET(0), WASM_F32(0.0),
                                         WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                         WASM_F32(0.0))});
}

TEST_F(FunctionBodyDecoderTest, Select_fail2) {
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType type = kValueTypes[i];
    if (type == kWasmI32) continue;
    // Select without specified type is only allowed for number types.
    if (type == kWasmExternRef) continue;

    ValueType types[] = {type, kWasmI32, type};
    FunctionSig sig(1, 2, types);

    ExpectValidates(&sig, {WASM_SELECT(WASM_LOCAL_GET(1), WASM_LOCAL_GET(1),
                                       WASM_LOCAL_GET(0))});

    ExpectFailure(&sig, {WASM_SELECT(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0),
                                     WASM_LOCAL_GET(0))});

    ExpectFailure(&sig, {WASM_SELECT(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                     WASM_LOCAL_GET(0))});

    ExpectFailure(&sig, {WASM_SELECT(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                     WASM_LOCAL_GET(1))});
  }
}

TEST_F(FunctionBodyDecoderTest, Select_TypeCheck) {
  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_F32(9.9), WASM_LOCAL_GET(0),
                                         WASM_LOCAL_GET(0))});

  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_LOCAL_GET(0), WASM_F64(0.25),
                                         WASM_LOCAL_GET(0))});

  ExpectFailure(sigs.i_i(), {WASM_SELECT(WASM_F32(9.9), WASM_LOCAL_GET(0),
                                         WASM_I64V_1(0))});
}

TEST_F(FunctionBodyDecoderTest, SelectWithType) {
  WASM_FEATURE_SCOPE(reftypes);
  ExpectValidates(sigs.i_i(), {WASM_SELECT_I(WASM_LOCAL_GET(0),
                                             WASM_LOCAL_GET(0), WASM_ZERO)});
  ExpectValidates(sigs.f_ff(),
                  {WASM_SELECT_F(WASM_F32(0.0), WASM_F32(0.0), WASM_ZERO)});
  ExpectValidates(sigs.d_dd(),
                  {WASM_SELECT_D(WASM_F64(0.0), WASM_F64(0.0), WASM_ZERO)});
  ExpectValidates(sigs.l_l(),
                  {WASM_SELECT_L(WASM_I64V_1(0), WASM_I64V_1(0), WASM_ZERO)});
  ExpectValidates(sigs.e_e(),
                  {WASM_SELECT_R(WASM_REF_NULL(kExternRefCode),
                                 WASM_REF_NULL(kExternRefCode), WASM_ZERO)});
  ExpectValidates(sigs.c_c(),
                  {WASM_SELECT_A(WASM_REF_NULL(kFuncRefCode),
                                 WASM_REF_NULL(kFuncRefCode), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, SelectWithType_fail) {
  WASM_FEATURE_SCOPE(reftypes);
  ExpectFailure(sigs.i_i(), {WASM_SELECT_F(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                           WASM_ZERO)});
  ExpectFailure(sigs.f_ff(),
                {WASM_SELECT_D(WASM_F32(0.0), WASM_F32(0.0), WASM_ZERO)});
  ExpectFailure(sigs.d_dd(),
                {WASM_SELECT_L(WASM_F64(0.0), WASM_F64(0.0), WASM_ZERO)});
  ExpectFailure(sigs.l_l(),
                {WASM_SELECT_I(WASM_I64V_1(0), WASM_I64V_1(0), WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, Throw) {
  WASM_FEATURE_SCOPE(eh);
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
  byte ex1 = builder.AddException(sigs.v_v());
  byte ex2 = builder.AddException(sigs.v_i());
  ExpectValidates(sigs.i_i(), {WASM_LOCAL_GET(0), kExprThrow, ex1, WASM_NOP});
  ExpectValidates(sigs.v_i(), {WASM_LOCAL_GET(0), kExprThrow, ex2, WASM_NOP});
  ExpectValidates(sigs.i_i(), {WASM_LOCAL_GET(0), kExprThrow, ex1, WASM_ZERO});
  ExpectFailure(sigs.v_i(), {WASM_LOCAL_GET(0), kExprThrow, ex2, WASM_ZERO});
  ExpectFailure(sigs.i_i(),
                {WASM_LOCAL_GET(0), kExprThrow, ex1, WASM_F32(0.0)});
  ExpectFailure(sigs.v_i(),
                {WASM_LOCAL_GET(0), kExprThrow, ex2, WASM_F32(0.0)});
}

#define WASM_TRY_OP kExprTry, kVoidCode

TEST_F(FunctionBodyDecoderTest, TryCatch) {
  WASM_FEATURE_SCOPE(eh);
  byte ex = builder.AddException(sigs.v_v());
  ExpectValidates(sigs.v_v(), {WASM_TRY_OP, kExprCatch, ex, kExprEnd});
  ExpectValidates(sigs.v_v(),
                  {WASM_TRY_OP, kExprCatch, ex, kExprCatchAll, kExprEnd});
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprCatchAll, kExprCatch, ex, kExprEnd},
                kAppendEnd, "catch after catch-all for try");
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprCatchAll, kExprCatchAll, kExprEnd},
                kAppendEnd, "catch-all already present for try");
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprEnd}, kAppendEnd,
                "missing catch or catch-all in try");
  ExpectFailure(sigs.v_v(), {kExprCatch, ex, kExprEnd}, kAppendEnd,
                "catch does not match a try");
}

TEST_F(FunctionBodyDecoderTest, TryUnwind) {
  WASM_FEATURE_SCOPE(eh);
  byte ex = builder.AddException(sigs.v_v());
  ExpectValidates(sigs.v_v(), {WASM_TRY_OP, kExprUnwind, kExprEnd});
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprUnwind, kExprCatch, ex, kExprEnd},
                kAppendEnd, "catch after unwind for try");
  ExpectFailure(sigs.v_v(), {WASM_TRY_OP, kExprCatchAll, kExprUnwind, kExprEnd},
                kAppendEnd,
                "catch, catch-all or unwind already present for try");
  ExpectFailure(
      sigs.v_v(), {WASM_TRY_OP, kExprCatch, ex, kExprUnwind, kExprEnd},
      kAppendEnd, "catch, catch-all or unwind already present for try");
}

TEST_F(FunctionBodyDecoderTest, Rethrow) {
  WASM_FEATURE_SCOPE(eh);
  ExpectValidates(sigs.v_v(),
                  {WASM_TRY_OP, kExprCatchAll, kExprRethrow, 0, kExprEnd});
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprRethrow, 0, kExprCatch, kExprEnd},
                kAppendEnd, "rethrow not targeting catch or catch-all");
  ExpectFailure(sigs.v_v(), {WASM_BLOCK(kExprRethrow, 0)}, kAppendEnd,
                "rethrow not targeting catch or catch-all");
  ExpectFailure(sigs.v_v(), {kExprRethrow, 0}, kAppendEnd,
                "rethrow not targeting catch or catch-all");
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprUnwind, kExprRethrow, 0, kExprEnd},
                kAppendEnd, "rethrow not targeting catch or catch-all");
}

TEST_F(FunctionBodyDecoderTest, TryDelegate) {
  WASM_FEATURE_SCOPE(eh);
  byte ex = builder.AddException(sigs.v_v());

  ExpectValidates(sigs.v_v(), {WASM_TRY_OP,
                               WASM_TRY_DELEGATE(WASM_STMTS(kExprThrow, ex), 0),
                               kExprCatch, ex, kExprEnd});
  ExpectValidates(
      sigs.v_v(),
      {WASM_BLOCK(WASM_TRY_OP, WASM_TRY_DELEGATE(WASM_STMTS(kExprThrow, ex), 2),
                  kExprCatch, ex, kExprEnd)});

  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP,
                 WASM_BLOCK(WASM_TRY_DELEGATE(WASM_STMTS(kExprThrow, ex), 0)),
                 kExprCatch, ex, kExprEnd},
                kAppendEnd,
                "delegate target must be a try block or the function block");
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprCatch, ex,
                 WASM_TRY_DELEGATE(WASM_STMTS(kExprThrow, ex), 0), kExprEnd},
                kAppendEnd,
                "cannot delegate inside the catch handler of the target");
  ExpectFailure(sigs.v_v(),
                {WASM_TRY_OP, kExprUnwind,
                 WASM_TRY_DELEGATE(WASM_STMTS(kExprThrow, ex), 0), kExprEnd},
                kAppendEnd,
                "cannot delegate inside the catch handler of the target");
  ExpectFailure(
      sigs.v_v(),
      {WASM_BLOCK(WASM_TRY_OP, WASM_TRY_DELEGATE(WASM_STMTS(kExprThrow, ex), 3),
                  kExprCatch, ex, kExprEnd)},
      kAppendEnd, "invalid branch depth: 3");
  ExpectFailure(
      sigs.v_v(),
      {WASM_TRY_OP, WASM_TRY_OP, kExprCatch, ex, kExprDelegate, 0, kExprEnd},
      kAppendEnd, "delegate does not match a try");
  ExpectFailure(
      sigs.v_v(),
      {WASM_TRY_OP, WASM_TRY_OP, kExprCatchAll, kExprDelegate, 1, kExprEnd},
      kAppendEnd, "delegate does not match a try");
}

#undef WASM_TRY_OP

TEST_F(FunctionBodyDecoderTest, MultiValBlock1) {
  WASM_FEATURE_SCOPE(mv);
  byte sig0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(sig0, WASM_NOP), kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0)), kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                              WASM_LOCAL_GET(0)),
                 kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), kExprF32Add});
}

TEST_F(FunctionBodyDecoderTest, MultiValBlock2) {
  WASM_FEATURE_SCOPE(mv);
  byte sig0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(sigs.i_ii(),
                  {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)),
                   WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(sig0, WASM_NOP),
                              WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0)),
                              WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                              WASM_LOCAL_GET(0)),
                 WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)),
                 WASM_F32_ADD(WASM_NOP, WASM_NOP)});
}

TEST_F(FunctionBodyDecoderTest, MultiValBlockBr) {
  WASM_FEATURE_SCOPE(mv);
  byte sig0 = builder.AddSignature(sigs.ii_v());
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0), WASM_BR(0)),
                              kExprI32Add});
  ExpectValidates(sigs.i_ii(), {WASM_BLOCK_X(sig0, WASM_LOCAL_GET(0),
                                             WASM_LOCAL_GET(1), WASM_BR(0)),
                                kExprI32Add});
}

TEST_F(FunctionBodyDecoderTest, MultiValLoop1) {
  WASM_FEATURE_SCOPE(mv);
  byte sig0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_LOOP_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(sig0, WASM_NOP), kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_LOOP_X(sig0, WASM_LOCAL_GET(0)), kExprI32Add});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(sig0, WASM_LOCAL_GET(0),
                                          WASM_LOCAL_GET(1), WASM_LOCAL_GET(0)),
                              kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_LOOP_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), kExprF32Add});
}

TEST_F(FunctionBodyDecoderTest, MultiValIf) {
  WASM_FEATURE_SCOPE(mv);
  byte sig0 = builder.AddSignature(sigs.ii_v());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0),
                      WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)),
                      WASM_SEQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))),
       kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0), WASM_NOP, WASM_NOP),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0), WASM_NOP,
                                WASM_SEQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))),
                 kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0),
                      WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), WASM_NOP),
       kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                WASM_LOCAL_GET(1)),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                WASM_SEQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0),
                                WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)),
                                WASM_LOCAL_GET(1)),
                 kExprI32Add});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_IF_ELSE_X(
           sig0, WASM_LOCAL_GET(0),
           WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)),
           WASM_SEQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0), WASM_LOCAL_GET(0))),
       kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0),
                                WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0),
                                         WASM_LOCAL_GET(0)),
                                WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0),
                                WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)),
                                WASM_SEQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(1),
                                         WASM_LOCAL_GET(1))),
                 kExprI32Add});
  ExpectFailure(sigs.i_ii(),
                {WASM_IF_ELSE_X(sig0, WASM_LOCAL_GET(0),
                                WASM_SEQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)),
                                WASM_SEQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))),
                 kExprF32Add});
}

TEST_F(FunctionBodyDecoderTest, BlockParam) {
  WASM_FEATURE_SCOPE(mv);
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(
      sigs.i_ii(),
      {WASM_LOCAL_GET(0), WASM_BLOCK_X(sig1, WASM_LOCAL_GET(1),
                                       WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(),
                  {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                   WASM_BLOCK_X(sig2, WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(), {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                WASM_BLOCK_X(sig1, WASM_NOP),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(sig1, WASM_NOP),
                              WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_ii(), {WASM_BLOCK_X(sig1, WASM_LOCAL_GET(0)),
                              WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_LOCAL_GET(0), WASM_BLOCK_X(sig2, WASM_I32_ADD(WASM_NOP, WASM_NOP)),
       WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_ii(),
                {WASM_LOCAL_GET(0), WASM_BLOCK_X(sig1, WASM_F32_NEG(WASM_NOP)),
                 WASM_RETURN1(WASM_LOCAL_GET(0))});
}

TEST_F(FunctionBodyDecoderTest, LoopParam) {
  WASM_FEATURE_SCOPE(mv);
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(sigs.i_ii(), {WASM_LOCAL_GET(0),
                                WASM_LOOP_X(sig1, WASM_LOCAL_GET(1),
                                            WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(),
                  {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                   WASM_LOOP_X(sig2, WASM_I32_ADD(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(), {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                WASM_LOOP_X(sig1, WASM_NOP),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectFailure(sigs.i_ii(),
                {WASM_LOOP_X(sig1, WASM_NOP), WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_ii(), {WASM_LOOP_X(sig1, WASM_LOCAL_GET(0)),
                              WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(
      sigs.i_ii(),
      {WASM_LOCAL_GET(0), WASM_LOOP_X(sig2, WASM_I32_ADD(WASM_NOP, WASM_NOP)),
       WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_ii(),
                {WASM_LOCAL_GET(0), WASM_LOOP_X(sig1, WASM_F32_NEG(WASM_NOP)),
                 WASM_RETURN1(WASM_LOCAL_GET(0))});
}

TEST_F(FunctionBodyDecoderTest, LoopParamBr) {
  WASM_FEATURE_SCOPE(mv);
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(sigs.i_ii(),
                  {WASM_LOCAL_GET(0), WASM_LOOP_X(sig1, WASM_BR(0))});
  ExpectValidates(
      sigs.i_ii(),
      {WASM_LOCAL_GET(0), WASM_LOOP_X(sig1, WASM_BRV(0, WASM_LOCAL_GET(1)))});
  ExpectValidates(sigs.i_ii(), {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                WASM_LOOP_X(sig2, WASM_BR(0))});
  ExpectValidates(
      sigs.i_ii(),
      {WASM_LOCAL_GET(0), WASM_LOOP_X(sig1, WASM_BLOCK_X(sig1, WASM_BR(1)))});
  ExpectFailure(sigs.i_ii(),
                {WASM_LOCAL_GET(0), WASM_LOOP_X(sig1, WASM_BLOCK(WASM_BR(1))),
                 WASM_RETURN1(WASM_LOCAL_GET(0))});
  ExpectFailure(sigs.i_ii(), {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                              WASM_LOOP_X(sig2, WASM_BLOCK_X(sig1, WASM_BR(1))),
                              WASM_RETURN1(WASM_LOCAL_GET(0))});
}

TEST_F(FunctionBodyDecoderTest, IfParam) {
  WASM_FEATURE_SCOPE(mv);
  byte sig1 = builder.AddSignature(sigs.i_i());
  byte sig2 = builder.AddSignature(sigs.i_ii());
  ExpectValidates(sigs.i_ii(),
                  {WASM_LOCAL_GET(0),
                   WASM_IF_X(sig1, WASM_LOCAL_GET(0),
                             WASM_I32_ADD(WASM_NOP, WASM_LOCAL_GET(1)))});
  ExpectValidates(sigs.i_ii(),
                  {WASM_LOCAL_GET(0),
                   WASM_IF_ELSE_X(sig1, WASM_LOCAL_GET(0),
                                  WASM_I32_ADD(WASM_NOP, WASM_LOCAL_GET(1)),
                                  WASM_I32_EQZ(WASM_NOP))});
  ExpectValidates(
      sigs.i_ii(),
      {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
       WASM_IF_ELSE_X(sig2, WASM_LOCAL_GET(0), WASM_I32_ADD(WASM_NOP, WASM_NOP),
                      WASM_I32_MUL(WASM_NOP, WASM_NOP))});
  ExpectValidates(sigs.i_ii(), {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                WASM_IF_X(sig1, WASM_LOCAL_GET(0), WASM_NOP),
                                WASM_I32_ADD(WASM_NOP, WASM_NOP)});
  ExpectValidates(sigs.i_ii(),
                  {WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                   WASM_IF_ELSE_X(sig1, WASM_LOCAL_GET(0), WASM_NOP,
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
        VerifyWasmCode(this->zone()->allocator(), WasmFeatures::All(), nullptr,
                       &unused_detected_features, body);
    if (result.ok()) {
      std::ostringstream str;
      str << "Expected verification to fail";
    }
  }
}

TEST_F(FunctionBodyDecoderTest, MemoryInit) {
  builder.InitializeMemory();
  builder.SetDataSegmentCount(1);

  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(0, 1, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, MemoryInitInvalid) {
  builder.InitializeMemory();
  builder.SetDataSegmentCount(1);

  byte code[] = {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO),
                 WASM_END};
  for (size_t i = 0; i <= arraysize(code); ++i) {
    Validate(i == arraysize(code), sigs.v_v(), VectorOf(code, i), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, DataDrop) {
  builder.InitializeMemory();
  builder.SetDataSegmentCount(1);

  ExpectValidates(sigs.v_v(), {WASM_DATA_DROP(0)});
  ExpectFailure(sigs.v_v(), {WASM_DATA_DROP(1)});
}

TEST_F(FunctionBodyDecoderTest, DataSegmentIndexUnsigned) {
  builder.InitializeMemory();
  builder.SetDataSegmentCount(65);

  // Make sure that the index is interpreted as an unsigned number; 64 is
  // interpreted as -64 when decoded as a signed LEB.
  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_INIT(64, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectValidates(sigs.v_v(), {WASM_DATA_DROP(64)});
}

TEST_F(FunctionBodyDecoderTest, MemoryCopy) {
  builder.InitializeMemory();

  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, MemoryFill) {
  builder.InitializeMemory();

  ExpectValidates(sigs.v_v(),
                  {WASM_MEMORY_FILL(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, BulkMemoryOpsWithoutMemory) {
  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_INIT(0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_COPY(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_MEMORY_FILL(WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableInit) {
  builder.InitializeTable(wasm::kWasmFuncRef);
  builder.AddPassiveElementSegment(wasm::kWasmFuncRef);

  ExpectValidates(sigs.v_v(),
                  {WASM_TABLE_INIT(0, 0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(0, 1, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableInitWrongType) {
  uint32_t table_index = builder.InitializeTable(wasm::kWasmFuncRef);
  uint32_t element_index =
      builder.AddPassiveElementSegment(wasm::kWasmExternRef);

  WASM_FEATURE_SCOPE(reftypes);
  ExpectFailure(sigs.v_v(), {WASM_TABLE_INIT(table_index, element_index,
                                             WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableInitInvalid) {
  builder.InitializeTable(wasm::kWasmFuncRef);
  builder.AddPassiveElementSegment(wasm::kWasmFuncRef);

  byte code[] = {WASM_TABLE_INIT(0, 0, WASM_ZERO, WASM_ZERO, WASM_ZERO),
                 WASM_END};
  for (size_t i = 0; i <= arraysize(code); ++i) {
    Validate(i == arraysize(code), sigs.v_v(), VectorOf(code, i), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, ElemDrop) {
  builder.InitializeTable(wasm::kWasmFuncRef);
  builder.AddPassiveElementSegment(wasm::kWasmFuncRef);

  ExpectValidates(sigs.v_v(), {WASM_ELEM_DROP(0)});
  ExpectFailure(sigs.v_v(), {WASM_ELEM_DROP(1)});
}

TEST_F(FunctionBodyDecoderTest, TableInitDeclarativeElem) {
  builder.InitializeTable(wasm::kWasmFuncRef);
  builder.AddDeclarativeElementSegment();

  WASM_FEATURE_SCOPE(reftypes);
  byte code[] = {WASM_TABLE_INIT(0, 0, WASM_ZERO, WASM_ZERO, WASM_ZERO),
                 WASM_END};
  for (size_t i = 0; i <= arraysize(code); ++i) {
    Validate(i == arraysize(code), sigs.v_v(), VectorOf(code, i), kOmitEnd);
  }
}

TEST_F(FunctionBodyDecoderTest, DeclarativeElemDrop) {
  builder.InitializeTable(wasm::kWasmFuncRef);
  builder.AddDeclarativeElementSegment();

  WASM_FEATURE_SCOPE(reftypes);
  ExpectValidates(sigs.v_v(), {WASM_ELEM_DROP(0)});
  ExpectFailure(sigs.v_v(), {WASM_ELEM_DROP(1)});
}

TEST_F(FunctionBodyDecoderTest, RefFuncDeclared) {
  builder.InitializeTable(wasm::kWasmVoid);
  byte function_index = builder.AddFunction(sigs.v_i());

  ExpectFailure(sigs.a_v(), {WASM_REF_FUNC(function_index)});
  WASM_FEATURE_SCOPE(reftypes);
  ExpectValidates(sigs.a_v(), {WASM_REF_FUNC(function_index)});
}

TEST_F(FunctionBodyDecoderTest, RefFuncUndeclared) {
  builder.InitializeTable(wasm::kWasmVoid);
  byte function_index = builder.AddFunction(sigs.v_i(), false);

  WASM_FEATURE_SCOPE(reftypes);
  ExpectFailure(sigs.a_v(), {WASM_REF_FUNC(function_index)});
}

TEST_F(FunctionBodyDecoderTest, ElemSegmentIndexUnsigned) {
  builder.InitializeTable(wasm::kWasmFuncRef);
  for (int i = 0; i < 65; ++i) {
    builder.AddPassiveElementSegment(wasm::kWasmFuncRef);
  }

  // Make sure that the index is interpreted as an unsigned number; 64 is
  // interpreted as -64 when decoded as a signed LEB.
  ExpectValidates(sigs.v_v(),
                  {WASM_TABLE_INIT(0, 64, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectValidates(sigs.v_v(), {WASM_ELEM_DROP(64)});
}

TEST_F(FunctionBodyDecoderTest, TableCopy) {
  builder.InitializeTable(wasm::kWasmVoid);

  ExpectValidates(sigs.v_v(),
                  {WASM_TABLE_COPY(0, 0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableCopyWrongType) {
  uint32_t dst_table_index = builder.InitializeTable(wasm::kWasmFuncRef);
  uint32_t src_table_index = builder.InitializeTable(wasm::kWasmExternRef);

  WASM_FEATURE_SCOPE(reftypes);
  ExpectFailure(sigs.v_v(), {WASM_TABLE_COPY(dst_table_index, src_table_index,
                                             WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableGrow) {
  byte tab_func = builder.AddTable(kWasmFuncRef, 10, true, 20);
  byte tab_ref = builder.AddTable(kWasmExternRef, 10, true, 20);

  ExpectFailure(
      sigs.i_c(),
      {WASM_TABLE_GROW(tab_func, WASM_REF_NULL(kFuncRefCode), WASM_ONE)});
  WASM_FEATURE_SCOPE(reftypes);
  ExpectValidates(
      sigs.i_c(),
      {WASM_TABLE_GROW(tab_func, WASM_REF_NULL(kFuncRefCode), WASM_ONE)});
  ExpectValidates(
      sigs.i_e(),
      {WASM_TABLE_GROW(tab_ref, WASM_REF_NULL(kExternRefCode), WASM_ONE)});
  // FuncRef table cannot be initialized with an ExternRef value.
  ExpectFailure(sigs.i_e(),
                {WASM_TABLE_GROW(tab_func, WASM_LOCAL_GET(0), WASM_ONE)});
  // ExternRef table cannot be initialized with a FuncRef value.
  ExpectFailure(sigs.i_c(),
                {WASM_TABLE_GROW(tab_ref, WASM_LOCAL_GET(0), WASM_ONE)});
  // Check that the table index gets verified.
  ExpectFailure(
      sigs.i_e(),
      {WASM_TABLE_GROW(tab_ref + 2, WASM_REF_NULL(kExternRefCode), WASM_ONE)});
}

TEST_F(FunctionBodyDecoderTest, TableSize) {
  int tab = builder.AddTable(kWasmFuncRef, 10, true, 20);

  ExpectFailure(sigs.i_v(), {WASM_TABLE_SIZE(tab)});
  WASM_FEATURE_SCOPE(reftypes);
  ExpectValidates(sigs.i_v(), {WASM_TABLE_SIZE(tab)});
  ExpectFailure(sigs.i_v(), {WASM_TABLE_SIZE(tab + 2)});
}

TEST_F(FunctionBodyDecoderTest, TableFill) {
  byte tab_func = builder.AddTable(kWasmFuncRef, 10, true, 20);
  byte tab_ref = builder.AddTable(kWasmExternRef, 10, true, 20);

  ExpectFailure(sigs.v_c(),
                {WASM_TABLE_FILL(tab_func, WASM_ONE,
                                 WASM_REF_NULL(kFuncRefCode), WASM_ONE)});
  WASM_FEATURE_SCOPE(reftypes);
  ExpectValidates(sigs.v_c(),
                  {WASM_TABLE_FILL(tab_func, WASM_ONE,
                                   WASM_REF_NULL(kFuncRefCode), WASM_ONE)});
  ExpectValidates(sigs.v_e(),
                  {WASM_TABLE_FILL(tab_ref, WASM_ONE,
                                   WASM_REF_NULL(kExternRefCode), WASM_ONE)});
  // FuncRef table cannot be initialized with an ExternRef value.
  ExpectFailure(sigs.v_e(), {WASM_TABLE_FILL(tab_func, WASM_ONE,
                                             WASM_LOCAL_GET(0), WASM_ONE)});
  // ExternRef table cannot be initialized with a FuncRef value.
  ExpectFailure(sigs.v_c(), {WASM_TABLE_FILL(tab_ref, WASM_ONE,
                                             WASM_LOCAL_GET(0), WASM_ONE)});
  // Check that the table index gets verified.
  ExpectFailure(sigs.v_e(),
                {WASM_TABLE_FILL(tab_ref + 2, WASM_ONE,
                                 WASM_REF_NULL(kExternRefCode), WASM_ONE)});
}

TEST_F(FunctionBodyDecoderTest, TableOpsWithoutTable) {
  {
    WASM_FEATURE_SCOPE(reftypes);
    ExpectFailure(sigs.i_v(), {WASM_TABLE_GROW(0, WASM_REF_NULL(kExternRefCode),
                                               WASM_ONE)});
    ExpectFailure(sigs.i_v(), {WASM_TABLE_SIZE(0)});
    ExpectFailure(sigs.i_e(),
                  {WASM_TABLE_FILL(0, WASM_ONE, WASM_REF_NULL(kExternRefCode),
                                   WASM_ONE)});
  }
  builder.AddPassiveElementSegment(wasm::kWasmFuncRef);
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_INIT(0, 0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
  ExpectFailure(sigs.v_v(),
                {WASM_TABLE_COPY(0, 0, WASM_ZERO, WASM_ZERO, WASM_ZERO)});
}

TEST_F(FunctionBodyDecoderTest, TableCopyMultiTable) {
  WASM_FEATURE_SCOPE(reftypes);
  {
    TestModuleBuilder builder;
    builder.AddTable(kWasmExternRef, 10, true, 20);
    builder.AddPassiveElementSegment(wasm::kWasmFuncRef);
    module = builder.module();
    // We added one table, therefore table.copy on table 0 should work.
    int table_src = 0;
    int table_dst = 0;
    ExpectValidates(sigs.v_v(),
                    {WASM_TABLE_COPY(table_dst, table_src, WASM_ZERO, WASM_ZERO,
                                     WASM_ZERO)});
    // There is only one table, so table.copy on table 1 should fail.
    table_src = 0;
    table_dst = 1;
    ExpectFailure(sigs.v_v(), {WASM_TABLE_COPY(table_dst, table_src, WASM_ZERO,
                                               WASM_ZERO, WASM_ZERO)});
    table_src = 1;
    table_dst = 0;
    ExpectFailure(sigs.v_v(), {WASM_TABLE_COPY(table_dst, table_src, WASM_ZERO,
                                               WASM_ZERO, WASM_ZERO)});
  }
  {
    TestModuleBuilder builder;
    builder.AddTable(kWasmExternRef, 10, true, 20);
    builder.AddTable(kWasmExternRef, 10, true, 20);
    builder.AddPassiveElementSegment(wasm::kWasmFuncRef);
    module = builder.module();
    // We added two tables, therefore table.copy on table 0 should work.
    int table_src = 0;
    int table_dst = 0;
    ExpectValidates(sigs.v_v(),
                    {WASM_TABLE_COPY(table_dst, table_src, WASM_ZERO, WASM_ZERO,
                                     WASM_ZERO)});
    // Also table.copy on table 1 should work now.
    table_src = 1;
    table_dst = 0;
    ExpectValidates(sigs.v_v(),
                    {WASM_TABLE_COPY(table_dst, table_src, WASM_ZERO, WASM_ZERO,
                                     WASM_ZERO)});
    table_src = 0;
    table_dst = 1;
    ExpectValidates(sigs.v_v(),
                    {WASM_TABLE_COPY(table_dst, table_src, WASM_ZERO, WASM_ZERO,
                                     WASM_ZERO)});
  }
}

TEST_F(FunctionBodyDecoderTest, TableInitMultiTable) {
  WASM_FEATURE_SCOPE(reftypes);
  {
    TestModuleBuilder builder;
    builder.AddTable(kWasmExternRef, 10, true, 20);
    builder.AddPassiveElementSegment(wasm::kWasmExternRef);
    module = builder.module();
    // We added one table, therefore table.init on table 0 should work.
    int table_index = 0;
    ExpectValidates(sigs.v_v(), {WASM_TABLE_INIT(table_index, 0, WASM_ZERO,
                                                 WASM_ZERO, WASM_ZERO)});
    // There is only one table, so table.init on table 1 should fail.
    table_index = 1;
    ExpectFailure(sigs.v_v(), {WASM_TABLE_INIT(table_index, 0, WASM_ZERO,
                                               WASM_ZERO, WASM_ZERO)});
  }
  {
    TestModuleBuilder builder;
    builder.AddTable(kWasmExternRef, 10, true, 20);
    builder.AddTable(kWasmExternRef, 10, true, 20);
    builder.AddPassiveElementSegment(wasm::kWasmExternRef);
    module = builder.module();
    // We added two tables, therefore table.init on table 0 should work.
    int table_index = 0;
    ExpectValidates(sigs.v_v(), {WASM_TABLE_INIT(table_index, 0, WASM_ZERO,
                                                 WASM_ZERO, WASM_ZERO)});
    // Also table.init on table 1 should work now.
    table_index = 1;
    ExpectValidates(sigs.v_v(), {WASM_TABLE_INIT(table_index, 0, WASM_ZERO,
                                                 WASM_ZERO, WASM_ZERO)});
  }
}

TEST_F(FunctionBodyDecoderTest, UnpackPackedTypes) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  {
    TestModuleBuilder builder;
    byte type_index = builder.AddStruct({F(kWasmI8, true), F(kWasmI16, false)});
    module = builder.module();
    ExpectValidates(
        sigs.v_v(),
        {WASM_STRUCT_SET(
            type_index, 0,
            WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(1), WASM_I32V(42),
                                     WASM_RTT_CANON(type_index)),
            WASM_I32V(-1))});
  }
  {
    TestModuleBuilder builder;
    byte type_index = builder.AddArray(kWasmI8, true);
    module = builder.module();
    ExpectValidates(sigs.v_v(),
                    {WASM_ARRAY_SET(type_index,
                                    WASM_ARRAY_NEW_WITH_RTT(
                                        type_index, WASM_I32V(10), WASM_I32V(5),
                                        WASM_RTT_CANON(type_index)),
                                    WASM_I32V(3), WASM_I32V(12345678))});
  }
}

ValueType ref(byte type_index) {
  return ValueType::Ref(type_index, kNonNullable);
}
ValueType optref(byte type_index) {
  return ValueType::Ref(type_index, kNullable);
}

TEST_F(FunctionBodyDecoderTest, StructNewDefault) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  {
    TestModuleBuilder builder;
    byte type_index = builder.AddStruct({F(kWasmI32, true)});
    byte bad_type_index = builder.AddStruct({F(ref(type_index), true)});
    module = builder.module();
    ExpectValidates(sigs.v_v(), {WASM_STRUCT_NEW_DEFAULT(
                                     type_index, WASM_RTT_CANON(type_index)),
                                 WASM_DROP});
    ExpectFailure(sigs.v_v(),
                  {WASM_STRUCT_NEW_DEFAULT(bad_type_index,
                                           WASM_RTT_CANON(bad_type_index)),
                   WASM_DROP});
  }
  {
    TestModuleBuilder builder;
    byte type_index = builder.AddArray(kWasmI32, true);
    byte bad_type_index = builder.AddArray(ref(type_index), true);
    module = builder.module();
    ExpectValidates(sigs.v_v(),
                    {WASM_ARRAY_NEW_DEFAULT(type_index, WASM_I32V(3),
                                            WASM_RTT_CANON(type_index)),
                     WASM_DROP});
    ExpectFailure(sigs.v_v(),
                  {WASM_ARRAY_NEW_DEFAULT(bad_type_index, WASM_I32V(3),
                                          WASM_RTT_CANON(bad_type_index)),
                   WASM_DROP});
  }
}

TEST_F(FunctionBodyDecoderTest, DefaultableLocal) {
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(reftypes);
  AddLocals(kWasmExternRef, 1);
  ExpectValidates(sigs.v_v(), {});
}

TEST_F(FunctionBodyDecoderTest, NonDefaultableLocal) {
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(reftypes);
  AddLocals(ValueType::Ref(HeapType::kExtern, kNonNullable), 1);
  ExpectFailure(sigs.v_v(), {}, kAppendEnd,
                "Cannot define function-level local of non-defaultable type");
}

TEST_F(FunctionBodyDecoderTest, RefEq) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(eh);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(simd);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte struct_type_index = builder.AddStruct({F(kWasmI32, true)});
  ValueType eqref_subtypes[] = {kWasmEqRef,
                                kWasmI31Ref,
                                ValueType::Ref(HeapType::kEq, kNonNullable),
                                ValueType::Ref(HeapType::kI31, kNullable),
                                ref(struct_type_index),
                                optref(struct_type_index)};
  ValueType non_eqref_subtypes[] = {
      kWasmI32,
      kWasmI64,
      kWasmF32,
      kWasmF64,
      kWasmS128,
      kWasmExternRef,
      kWasmFuncRef,
      kWasmAnyRef,
      ValueType::Ref(HeapType::kExtern, kNonNullable),
      ValueType::Ref(HeapType::kFunc, kNonNullable)};

  for (ValueType type1 : eqref_subtypes) {
    for (ValueType type2 : eqref_subtypes) {
      ValueType reps[] = {kWasmI32, type1, type2};
      FunctionSig sig(1, 2, reps);
      ExpectValidates(&sig,
                      {WASM_REF_EQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});
    }
  }

  for (ValueType type1 : eqref_subtypes) {
    for (ValueType type2 : non_eqref_subtypes) {
      ValueType reps[] = {kWasmI32, type1, type2};
      FunctionSig sig(1, 2, reps);
      ExpectFailure(&sig, {WASM_REF_EQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))},
                    kAppendEnd, "expected type eqref, found local.get of type");
      ExpectFailure(&sig, {WASM_REF_EQ(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))},
                    kAppendEnd, "expected type eqref, found local.get of type");
    }
  }
}

TEST_F(FunctionBodyDecoderTest, RefAsNonNull) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(eh);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(simd);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte struct_type_index = builder.AddStruct({F(kWasmI32, true)});
  byte array_type_index = builder.AddArray(kWasmI32, true);
  uint32_t heap_types[] = {
      struct_type_index, array_type_index, HeapType::kFunc, HeapType::kEq,
      HeapType::kExtern, HeapType::kI31,   HeapType::kAny};

  ValueType non_compatible_types[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                                      kWasmS128};

  // It works with nullable types.
  for (uint32_t heap_type : heap_types) {
    ValueType reprs[] = {ValueType::Ref(heap_type, kNonNullable),
                         ValueType::Ref(heap_type, kNullable)};
    FunctionSig sig(1, 1, reprs);
    ExpectValidates(&sig, {WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0))});
  }

  // It works with non-nullable types.
  for (uint32_t heap_type : heap_types) {
    ValueType reprs[] = {ValueType::Ref(heap_type, kNonNullable),
                         ValueType::Ref(heap_type, kNonNullable)};
    FunctionSig sig(1, 1, reprs);
    ExpectValidates(&sig, {WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0))});
  }

  // It fails with other types.
  for (ValueType type : non_compatible_types) {
    FunctionSig sig(0, 1, &type);
    ExpectFailure(
        &sig, {WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)), kExprDrop}, kAppendEnd,
        "ref.as_non_null[0] expected reference type, found local.get of type");
  }
}

TEST_F(FunctionBodyDecoderTest, RefNull) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(eh);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte struct_type_index = builder.AddStruct({F(kWasmI32, true)});
  byte array_type_index = builder.AddArray(kWasmI32, true);
  uint32_t type_reprs[] = {
      struct_type_index, array_type_index, HeapType::kFunc, HeapType::kEq,
      HeapType::kExtern, HeapType::kI31,   HeapType::kAny};
  // It works with heap types.
  for (uint32_t type_repr : type_reprs) {
    const ValueType type = ValueType::Ref(type_repr, kNullable);
    const FunctionSig sig(1, 0, &type);
    ExpectValidates(&sig, {WASM_REF_NULL(WASM_HEAP_TYPE(HeapType(type_repr)))});
  }
  // It fails for undeclared types.
  ExpectFailure(sigs.v_v(), {WASM_REF_NULL(42), kExprDrop}, kAppendEnd,
                "Type index 42 is out of bounds");
}

TEST_F(FunctionBodyDecoderTest, RefIsNull) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(eh);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  ExpectValidates(sigs.i_i(),
                  {WASM_REF_IS_NULL(WASM_REF_NULL(kExternRefCode))});
  ExpectFailure(
      sigs.i_i(), {WASM_REF_IS_NULL(WASM_LOCAL_GET(0))}, kAppendEnd,
      "ref.is_null[0] expected reference type, found local.get of type i32");

  TestModuleBuilder builder;
  module = builder.module();
  byte struct_type_index = builder.AddStruct({F(kWasmI32, true)});
  byte array_type_index = builder.AddArray(kWasmI32, true);
  uint32_t heap_types[] = {
      struct_type_index, array_type_index, HeapType::kFunc, HeapType::kEq,
      HeapType::kExtern, HeapType::kI31,   HeapType::kAny};

  for (uint32_t heap_type : heap_types) {
    const ValueType types[] = {kWasmI32, ValueType::Ref(heap_type, kNullable)};
    const FunctionSig sig(1, 1, types);
    // It works for nullable references.
    ExpectValidates(&sig, {WASM_REF_IS_NULL(WASM_LOCAL_GET(0))});
    // It works for non-nullable references.
    ExpectValidates(
        &sig, {WASM_REF_IS_NULL(WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)))});
  }

  // It fails if the argument type is not a reference type.
  ExpectFailure(
      sigs.v_v(), {WASM_REF_IS_NULL(WASM_I32V(0)), kExprDrop}, kAppendEnd,
      "ref.is_null[0] expected reference type, found i32.const of type i32");
}

TEST_F(FunctionBodyDecoderTest, BrOnNull) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  const ValueType reps[] = {ValueType::Ref(HeapType::kFunc, kNonNullable),
                            ValueType::Ref(HeapType::kFunc, kNullable)};
  const FunctionSig sig(1, 1, reps);
  ExpectValidates(
      &sig, {WASM_BLOCK_R(reps[0], WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)),
                          WASM_BR_ON_NULL(0, WASM_LOCAL_GET(0)), WASM_I32V(0),
                          kExprSelectWithType, 1, WASM_REF_TYPE(reps[0]))});

  // Should have block return value on stack before calling br_on_null.
  ExpectFailure(&sig,
                {WASM_BLOCK_R(reps[0], WASM_BR_ON_NULL(0, WASM_LOCAL_GET(0)),
                              WASM_I32V(0), kExprSelectWithType, 1,
                              WASM_REF_TYPE(reps[0]))},
                kAppendEnd,
                "expected 1 elements on the stack for br to @1, found 0");
}

TEST_F(FunctionBodyDecoderTest, GCStruct) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte struct_type_index = builder.AddStruct({F(kWasmI32, true)});
  byte array_type_index = builder.AddArray(kWasmI32, true);
  byte immutable_struct_type_index = builder.AddStruct({F(kWasmI32, false)});
  byte field_index = 0;

  ValueType struct_type = ValueType::Ref(struct_type_index, kNonNullable);
  ValueType reps_i_r[] = {kWasmI32, struct_type};
  ValueType reps_f_r[] = {kWasmF32, struct_type};
  const FunctionSig sig_i_r(1, 1, reps_i_r);
  const FunctionSig sig_v_r(0, 1, &struct_type);
  const FunctionSig sig_r_v(1, 0, &struct_type);
  const FunctionSig sig_f_r(1, 1, reps_f_r);

  /** struct.new_with_rtt **/
  ExpectValidates(
      &sig_r_v, {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, WASM_I32V(0),
                                          WASM_RTT_CANON(struct_type_index))});
  // Too few arguments.
  ExpectFailure(&sig_r_v,
                {WASM_STRUCT_NEW_WITH_RTT(struct_type_index,
                                          WASM_RTT_CANON(struct_type_index))},
                kAppendEnd,
                "not enough arguments on the stack for struct.new_with_rtt, "
                "expected 1 more");
  // Too many arguments.
  ExpectFailure(
      &sig_r_v,
      {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, WASM_I32V(0), WASM_I32V(1),
                                WASM_RTT_CANON(struct_type_index))},
      kAppendEnd,
      "expected 1 elements on the stack for fallthru to @1, found 2");
  // Mistyped arguments.
  ExpectFailure(&sig_v_r,
                {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, WASM_LOCAL_GET(0),
                                          WASM_RTT_CANON(struct_type_index))},
                kAppendEnd,
                "struct.new_with_rtt[0] expected type i32, found local.get of "
                "type (ref 0)");
  // Wrongly typed index.
  ExpectFailure(sigs.v_v(),
                {WASM_STRUCT_NEW_WITH_RTT(array_type_index, WASM_I32V(0),
                                          WASM_RTT_CANON(struct_type_index)),
                 kExprDrop},
                kAppendEnd, "invalid struct index: 1");
  // Wrongly typed rtt.
  ExpectFailure(sigs.v_v(),
                {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, WASM_I32V(0),
                                          WASM_RTT_CANON(array_type_index)),
                 kExprDrop},
                kAppendEnd,
                "struct.new_with_rtt[1] expected rtt for type 0, found "
                "rtt.canon of type (rtt 0 1)");
  // Out-of-bounds index.
  ExpectFailure(sigs.v_v(),
                {WASM_STRUCT_NEW_WITH_RTT(42, WASM_I32V(0),
                                          WASM_RTT_CANON(struct_type_index)),
                 kExprDrop},
                kAppendEnd, "invalid struct index: 42");

  /** struct.get **/
  ExpectValidates(&sig_i_r, {WASM_STRUCT_GET(struct_type_index, field_index,
                                             WASM_LOCAL_GET(0))});
  // With non-nullable struct.
  ExpectValidates(&sig_i_r,
                  {WASM_STRUCT_GET(struct_type_index, field_index,
                                   WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)))});
  // Wrong index.
  ExpectFailure(
      &sig_v_r,
      {WASM_STRUCT_GET(struct_type_index, field_index + 1, WASM_LOCAL_GET(0)),
       kExprDrop},
      kAppendEnd, "invalid field index: 1");
  // Mistyped expected type.
  ExpectFailure(
      &sig_f_r,
      {WASM_STRUCT_GET(struct_type_index, field_index, WASM_LOCAL_GET(0))},
      kAppendEnd, "type error in merge[0] (expected f32, got i32)");

  /** struct.set **/
  ExpectValidates(&sig_v_r, {WASM_STRUCT_SET(struct_type_index, field_index,
                                             WASM_LOCAL_GET(0), WASM_I32V(0))});
  // Non-nullable struct.
  ExpectValidates(
      &sig_v_r,
      {WASM_STRUCT_SET(struct_type_index, field_index,
                       WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)), WASM_I32V(0))});
  // Wrong index.
  ExpectFailure(&sig_v_r,
                {WASM_STRUCT_SET(struct_type_index, field_index + 1,
                                 WASM_LOCAL_GET(0), WASM_I32V(0))},
                kAppendEnd, "invalid field index: 1");
  // Mistyped input.
  ExpectFailure(&sig_v_r,
                {WASM_STRUCT_SET(struct_type_index, field_index,
                                 WASM_LOCAL_GET(0), WASM_I64V(0))},
                kAppendEnd,
                "struct.set[1] expected type i32, found i64.const of type i64");
  // Expecting output.
  ExpectFailure(&sig_i_r,
                {WASM_STRUCT_SET(struct_type_index, field_index,
                                 WASM_LOCAL_GET(0), WASM_I32V(0))},
                kAppendEnd,
                "expected 1 elements on the stack for fallthru to @1, found 0");
  // Setting immutable field.
  ExpectFailure(
      sigs.v_v(),
      {WASM_STRUCT_SET(
          immutable_struct_type_index, field_index,
          WASM_STRUCT_NEW_WITH_RTT(immutable_struct_type_index, WASM_I32V(42),
                                   WASM_RTT_CANON(immutable_struct_type_index)),
          WASM_I32V(0))},
      kAppendEnd, "struct.set: Field 0 of type 2 is immutable.");

  // struct.get_s/u fail
  ExpectFailure(
      &sig_i_r,
      {WASM_STRUCT_GET_S(struct_type_index, field_index, WASM_LOCAL_GET(0))},
      kAppendEnd,
      "struct.get_s: Immediate field 0 of type 0 has non-packed type i32. Use "
      "struct.get instead.");

  ExpectFailure(
      &sig_i_r,
      {WASM_STRUCT_GET_U(struct_type_index, field_index, WASM_LOCAL_GET(0))},
      kAppendEnd,
      "struct.get_u: Immediate field 0 of type 0 has non-packed type i32. Use "
      "struct.get instead.");
}

TEST_F(FunctionBodyDecoderTest, GCArray) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte array_type_index = builder.AddArray(kWasmFuncRef, true);
  byte struct_type_index = builder.AddStruct({F(kWasmI32, false)});

  ValueType array_type = ValueType::Ref(array_type_index, kNonNullable);
  ValueType reps_c_r[] = {kWasmFuncRef, array_type};
  ValueType reps_f_r[] = {kWasmF32, array_type};
  ValueType reps_i_r[] = {kWasmI32, array_type};
  const FunctionSig sig_c_r(1, 1, reps_c_r);
  const FunctionSig sig_v_r(0, 1, &array_type);
  const FunctionSig sig_r_v(1, 0, &array_type);
  const FunctionSig sig_f_r(1, 1, reps_f_r);
  const FunctionSig sig_v_cr(0, 2, reps_c_r);
  const FunctionSig sig_i_r(1, 1, reps_i_r);

  /** array.new_with_rtt **/
  ExpectValidates(&sig_r_v,
                  {WASM_ARRAY_NEW_WITH_RTT(
                      array_type_index, WASM_REF_NULL(kFuncRefCode),
                      WASM_I32V(10), WASM_RTT_CANON(array_type_index))});
  // Too few arguments.
  ExpectFailure(&sig_r_v,
                {WASM_I32V(10), WASM_RTT_CANON(array_type_index),
                 WASM_GC_OP(kExprArrayNewWithRtt), array_type_index},
                kAppendEnd,
                "not enough arguments on the stack for array.new_with_rtt, "
                "expected 1 more");
  // Mistyped initializer.
  ExpectFailure(&sig_r_v,
                {WASM_ARRAY_NEW_WITH_RTT(
                    array_type_index, WASM_REF_NULL(kExternRefCode),
                    WASM_I32V(10), WASM_RTT_CANON(array_type_index))},
                kAppendEnd,
                "array.new_with_rtt[0] expected type funcref, found ref.null "
                "of type externref");
  // Mistyped length.
  ExpectFailure(
      &sig_r_v,
      {WASM_ARRAY_NEW_WITH_RTT(array_type_index, WASM_REF_NULL(kFuncRefCode),
                               WASM_I64V(5), WASM_RTT_CANON(array_type_index))},
      kAppendEnd,
      "array.new_with_rtt[1] expected type i32, found i64.const of type i64");
  // Mistyped rtt.
  ExpectFailure(&sig_r_v,
                {WASM_ARRAY_NEW_WITH_RTT(
                    array_type_index, WASM_REF_NULL(kFuncRefCode), WASM_I32V(5),
                    WASM_RTT_CANON(struct_type_index))},
                kAppendEnd,
                "array.new_with_rtt[2] expected rtt for type 0, found "
                "rtt.canon of type (rtt 0 1)");
  // Wrong type index.
  ExpectFailure(
      sigs.v_v(),
      {WASM_ARRAY_NEW_WITH_RTT(struct_type_index, WASM_REF_NULL(kFuncRefCode),
                               WASM_I32V(10), WASM_RTT_CANON(array_type_index)),
       kExprDrop},
      kAppendEnd, "invalid array index: 1");

  /** array.get **/
  ExpectValidates(&sig_c_r, {WASM_ARRAY_GET(array_type_index, WASM_LOCAL_GET(0),
                                            WASM_I32V(5))});
  // With non-nullable array type.
  ExpectValidates(
      &sig_c_r,
      {WASM_ARRAY_GET(array_type_index, WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)),
                      WASM_I32V(5))});
  // Wrongly typed index.
  ExpectFailure(
      &sig_v_r,
      {WASM_ARRAY_GET(array_type_index, WASM_LOCAL_GET(0), WASM_I64V(5)),
       kExprDrop},
      kAppendEnd,
      "array.get[1] expected type i32, found i64.const of type i64");
  // Mistyped expected type.
  ExpectFailure(
      &sig_f_r,
      {WASM_ARRAY_GET(array_type_index, WASM_LOCAL_GET(0), WASM_I32V(5))},
      kAppendEnd, "type error in merge[0] (expected f32, got funcref)");

  // array.get_s/u fail.
  ExpectFailure(
      &sig_c_r,
      {WASM_ARRAY_GET_S(array_type_index, WASM_LOCAL_GET(0), WASM_I32V(5))},
      kAppendEnd,
      "array.get_s: Immediate array type 0 has non-packed type funcref. Use "
      "array.get instead.");
  ExpectFailure(
      &sig_c_r,
      {WASM_ARRAY_GET_U(array_type_index, WASM_LOCAL_GET(0), WASM_I32V(5))},
      kAppendEnd,
      "array.get_u: Immediate array type 0 has non-packed type funcref. Use "
      "array.get instead.");

  /** array.set **/
  ExpectValidates(&sig_v_r,
                  {WASM_ARRAY_SET(array_type_index, WASM_LOCAL_GET(0),
                                  WASM_I32V(42), WASM_REF_NULL(kFuncRefCode))});
  // With non-nullable array type.
  ExpectValidates(
      &sig_v_cr,
      {WASM_ARRAY_SET(array_type_index, WASM_LOCAL_GET(1), WASM_I32V(42),
                      WASM_REF_AS_NON_NULL(WASM_LOCAL_GET(0)))});
  // Non-array type index.
  ExpectFailure(&sig_v_cr,
                {WASM_ARRAY_SET(struct_type_index, WASM_LOCAL_GET(1),
                                WASM_I32V(42), WASM_LOCAL_GET(0))},
                kAppendEnd, "invalid array index: 1");
  // Wrongly typed index.
  ExpectFailure(&sig_v_cr,
                {WASM_ARRAY_SET(array_type_index, WASM_LOCAL_GET(1),
                                WASM_I64V(42), WASM_LOCAL_GET(0))},
                kAppendEnd,
                "array.set[1] expected type i32, found i64.const of type i64");
  // Wrongly typed value.
  ExpectFailure(
      &sig_v_cr,
      {WASM_ARRAY_SET(array_type_index, WASM_LOCAL_GET(1), WASM_I32V(42),
                      WASM_I64V(0))},
      kAppendEnd,
      "array.set[2] expected type funcref, found i64.const of type i64");

  /** array.len **/
  ExpectValidates(&sig_i_r,
                  {WASM_ARRAY_LEN(array_type_index, WASM_LOCAL_GET(0))});
  // Wrong return type.
  ExpectFailure(&sig_f_r, {WASM_ARRAY_LEN(array_type_index, WASM_LOCAL_GET(0))},
                kAppendEnd, "type error in merge[0] (expected f32, got i32)");
  // Non-array type index.
  ExpectFailure(&sig_i_r,
                {WASM_ARRAY_LEN(struct_type_index, WASM_LOCAL_GET(0))},
                kAppendEnd, "invalid array index: 1");
}

TEST_F(FunctionBodyDecoderTest, PackedFields) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte array_type_index = builder.AddArray(kWasmI8, true);
  byte struct_type_index = builder.AddStruct({F(kWasmI16, true)});
  byte field_index = 0;

  // *.new with packed fields works.
  ExpectValidates(sigs.v_v(), {WASM_ARRAY_NEW_WITH_RTT(
                                   array_type_index, WASM_I32V(0), WASM_I32V(5),
                                   WASM_RTT_CANON(array_type_index)),
                               kExprDrop});
  ExpectValidates(sigs.v_v(),
                  {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, WASM_I32V(42),
                                            WASM_RTT_CANON(struct_type_index)),
                   kExprDrop});
  // It can't unpack types other that i32.
  ExpectFailure(
      sigs.v_v(),
      {WASM_ARRAY_NEW_WITH_RTT(array_type_index, WASM_I64V(0), WASM_I32V(5),
                               WASM_RTT_CANON(array_type_index)),
       kExprDrop},
      kAppendEnd,
      "array.new_with_rtt[0] expected type i32, found i64.const of type i64");
  ExpectFailure(
      sigs.v_v(),
      {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, WASM_I64V(42),
                                WASM_RTT_CANON(struct_type_index)),
       kExprDrop},
      kAppendEnd,
      "struct.new_with_rtt[0] expected type i32, found i64.const of type i64");

  // *.set with packed fields works.
  ExpectValidates(sigs.v_v(), {WASM_ARRAY_SET(array_type_index,
                                              WASM_REF_NULL(array_type_index),
                                              WASM_I32V(0), WASM_I32V(5))});
  ExpectValidates(sigs.v_v(), {WASM_STRUCT_SET(struct_type_index, field_index,
                                               WASM_REF_NULL(struct_type_index),
                                               WASM_I32V(42))});
  // It can't unpack into types other that i32.
  ExpectFailure(
      sigs.v_v(),
      {WASM_ARRAY_SET(array_type_index, WASM_REF_NULL(array_type_index),
                      WASM_I32V(0), WASM_I64V(5))},
      kAppendEnd,
      "array.set[2] expected type i32, found i64.const of type i64");
  ExpectFailure(
      sigs.v_v(),
      {WASM_STRUCT_NEW_WITH_RTT(struct_type_index, field_index,
                                WASM_REF_NULL(struct_type_index), WASM_I64V(42),
                                WASM_RTT_CANON(struct_type_index))},
      kAppendEnd,
      "struct.new_with_rtt[0] expected type i32, found i64.const of type i64");

  // *.get_s/u works.
  ExpectValidates(sigs.i_v(), {WASM_ARRAY_GET_S(array_type_index,
                                                WASM_REF_NULL(array_type_index),
                                                WASM_I32V(0))});
  ExpectValidates(sigs.i_v(), {WASM_ARRAY_GET_U(array_type_index,
                                                WASM_REF_NULL(array_type_index),
                                                WASM_I32V(0))});
  ExpectValidates(sigs.i_v(),
                  {WASM_STRUCT_GET_S(struct_type_index, field_index,
                                     WASM_REF_NULL(struct_type_index))});
  ExpectValidates(sigs.i_v(),
                  {WASM_STRUCT_GET_U(struct_type_index, field_index,
                                     WASM_REF_NULL(struct_type_index))});

  // *.get fails.
  ExpectFailure(sigs.i_v(),
                {WASM_ARRAY_GET(array_type_index,
                                WASM_REF_NULL(array_type_index), WASM_I32V(0))},
                kAppendEnd,
                "array.get: Immediate array type 0 has packed type i8. Use "
                "array.get_s or array.get_u instead.");
  ExpectFailure(sigs.i_v(),
                {WASM_STRUCT_GET(struct_type_index, field_index,
                                 WASM_REF_NULL(struct_type_index))},
                kAppendEnd,
                "struct.get: Immediate field 0 of type 1 has packed type i16. "
                "Use struct.get_s or struct.get_u instead.");
}

TEST_F(FunctionBodyDecoderTest, PackedTypesAsLocals) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  AddLocals(kWasmI8, 1);
  ExpectFailure(sigs.v_v(), {}, kAppendEnd, "invalid value type");
}

TEST_F(FunctionBodyDecoderTest, RttCanon) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  WASM_FEATURE_SCOPE(eh);

  TestModuleBuilder builder;
  module = builder.module();
  uint8_t array_type_index = builder.AddArray(kWasmI32, true);
  uint8_t struct_type_index = builder.AddStruct({F(kWasmI64, true)});

  for (uint32_t type_index : {array_type_index, struct_type_index}) {
    ValueType rtt1 = ValueType::Rtt(type_index, 0);
    FunctionSig sig1(1, 0, &rtt1);
    ExpectValidates(&sig1, {WASM_RTT_CANON(type_index)});

    // rtt.canon should fail for incorrect depth.
    ValueType rtt2 = ValueType::Rtt(type_index, 1);
    FunctionSig sig2(1, 0, &rtt2);
    ExpectFailure(&sig2, {WASM_RTT_CANON(type_index)}, kAppendEnd,
                  "type error in merge[0]");
  }
}

TEST_F(FunctionBodyDecoderTest, RttSub) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  WASM_FEATURE_SCOPE(eh);

  TestModuleBuilder builder;
  module = builder.module();
  uint8_t array_type_index = builder.AddArray(kWasmI8, true);
  uint8_t super_struct_type_index = builder.AddStruct({F(kWasmI16, true)});
  uint8_t sub_struct_type_index =
      builder.AddStruct({F(kWasmI16, true), F(kWasmI32, false)});

  // Trivial type error.
  ExpectFailure(
      sigs.v_v(), {WASM_RTT_SUB(array_type_index, WASM_I32V(42)), kExprDrop},
      kAppendEnd, "rtt.sub[0] expected rtt for a supertype of type 0");

  {
    ValueType type = ValueType::Rtt(array_type_index, 1);
    FunctionSig sig(1, 0, &type);
    // Can build an rtt.sub with self type for an array type.
    ExpectValidates(&sig, {WASM_RTT_SUB(array_type_index,
                                        WASM_RTT_CANON(array_type_index))});
    // Fails when argument to rtt.sub is not a supertype.
    ExpectFailure(sigs.v_v(),
                  {WASM_RTT_SUB(super_struct_type_index,
                                WASM_RTT_CANON(array_type_index)),
                   kExprDrop},
                  kAppendEnd,
                  "rtt.sub[0] expected rtt for a supertype of type 1");
  }

  {
    ValueType type = ValueType::Rtt(super_struct_type_index, 1);
    FunctionSig sig(1, 0, &type);
    // Can build an rtt.sub with self type for a struct type.
    ExpectValidates(&sig,
                    {WASM_RTT_SUB(super_struct_type_index,
                                  WASM_RTT_CANON(super_struct_type_index))});
    // Fails when argument to rtt.sub is not a supertype.
    ExpectFailure(sigs.v_v(),
                  {WASM_RTT_SUB(super_struct_type_index,
                                WASM_RTT_CANON(array_type_index))},
                  kAppendEnd,
                  "rtt.sub[0] expected rtt for a supertype of type 1");
    ExpectFailure(sigs.v_v(),
                  {WASM_RTT_SUB(super_struct_type_index,
                                WASM_RTT_CANON(sub_struct_type_index))},
                  kAppendEnd,
                  "rtt.sub[0] expected rtt for a supertype of type 1");
  }

  {
    // Can build an rtt from a stuct supertype.
    ValueType type = ValueType::Rtt(sub_struct_type_index, 1);
    FunctionSig sig(1, 0, &type);
    ExpectValidates(&sig,
                    {WASM_RTT_SUB(sub_struct_type_index,
                                  WASM_RTT_CANON(super_struct_type_index))});
  }
}

TEST_F(FunctionBodyDecoderTest, RefTestCast) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);
  WASM_FEATURE_SCOPE(eh);

  TestModuleBuilder builder;
  module = builder.module();
  HeapType::Representation array_heap =
      static_cast<HeapType::Representation>(builder.AddArray(kWasmI8, true));
  HeapType::Representation super_struct_heap =
      static_cast<HeapType::Representation>(
          builder.AddStruct({F(kWasmI16, true)}));

  HeapType::Representation sub_struct_heap =
      static_cast<HeapType::Representation>(
          builder.AddStruct({F(kWasmI16, true), F(kWasmI32, false)}));

  HeapType::Representation func_heap_1 =
      static_cast<HeapType::Representation>(builder.AddSignature(sigs.i_i()));

  HeapType::Representation func_heap_2 =
      static_cast<HeapType::Representation>(builder.AddSignature(sigs.i_v()));

  std::tuple<HeapType::Representation, HeapType::Representation, bool> tests[] =
      {std::make_tuple(HeapType::kData, array_heap, true),
       std::make_tuple(HeapType::kData, super_struct_heap, true),
       std::make_tuple(HeapType::kFunc, func_heap_1, true),
       std::make_tuple(func_heap_1, func_heap_1, true),
       std::make_tuple(func_heap_1, func_heap_2, true),
       std::make_tuple(super_struct_heap, sub_struct_heap, true),
       std::make_tuple(array_heap, sub_struct_heap, true),
       std::make_tuple(super_struct_heap, func_heap_1, true),
       std::make_tuple(HeapType::kEq, super_struct_heap, false),
       std::make_tuple(HeapType::kAny, func_heap_1, false),
       std::make_tuple(HeapType::kI31, array_heap, false)};

  for (auto test : tests) {
    HeapType from_heap = HeapType(std::get<0>(test));
    HeapType to_heap = HeapType(std::get<1>(test));
    bool should_pass = std::get<2>(test);

    ValueType test_reps[] = {kWasmI32, ValueType::Ref(from_heap, kNullable)};
    FunctionSig test_sig(1, 1, test_reps);

    ValueType cast_reps_with_depth[] = {ValueType::Ref(to_heap, kNullable),
                                        ValueType::Ref(from_heap, kNullable)};
    FunctionSig cast_sig_with_depth(1, 1, cast_reps_with_depth);

    ValueType cast_reps[] = {ValueType::Ref(to_heap, kNullable),
                             ValueType::Ref(from_heap, kNullable),
                             ValueType::Rtt(to_heap.ref_index())};
    FunctionSig cast_sig(1, 2, cast_reps);

    if (should_pass) {
      ExpectValidates(&test_sig,
                      {WASM_REF_TEST(WASM_LOCAL_GET(0),
                                     WASM_RTT_CANON(WASM_HEAP_TYPE(to_heap)))});
      ExpectValidates(&cast_sig_with_depth,
                      {WASM_REF_CAST(WASM_LOCAL_GET(0),
                                     WASM_RTT_CANON(WASM_HEAP_TYPE(to_heap)))});
      ExpectValidates(&cast_sig,
                      {WASM_REF_CAST(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});
    } else {
      std::string error_message =
          "[0] expected subtype of (ref null func) or (ref null data), found "
          "local.get of type " +
          test_reps[1].name();
      ExpectFailure(&test_sig,
                    {WASM_REF_TEST(WASM_LOCAL_GET(0),
                                   WASM_RTT_CANON(WASM_HEAP_TYPE(to_heap)))},
                    kAppendEnd, ("ref.test" + error_message).c_str());
      ExpectFailure(&cast_sig_with_depth,
                    {WASM_REF_CAST(WASM_LOCAL_GET(0),
                                   WASM_RTT_CANON(WASM_HEAP_TYPE(to_heap)))},
                    kAppendEnd, ("ref.cast" + error_message).c_str());
      ExpectFailure(&cast_sig,
                    {WASM_REF_CAST(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))},
                    kAppendEnd, ("ref.cast" + error_message).c_str());
    }
  }

  // Trivial type error.
  ExpectFailure(
      sigs.v_v(),
      {WASM_REF_TEST(WASM_I32V(1), WASM_RTT_CANON(array_heap)), kExprDrop},
      kAppendEnd,
      "ref.test[0] expected subtype of (ref null func) or (ref null data), "
      "found i32.const of type i32");
  ExpectFailure(
      sigs.v_v(),
      {WASM_REF_CAST(WASM_I32V(1), WASM_RTT_CANON(array_heap)), kExprDrop},
      kAppendEnd,
      "ref.cast[0] expected subtype of (ref null func) or (ref null data), "
      "found i32.const of type i32");
}

TEST_F(FunctionBodyDecoderTest, LocalTeeTyping) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  WASM_FEATURE_SCOPE(gc);

  TestModuleBuilder builder;
  module = builder.module();
  byte array_type = builder.AddArray(kWasmI8, true);

  ValueType types[] = {ValueType::Ref(array_type, kNonNullable)};
  FunctionSig sig(1, 0, types);

  AddLocals(ValueType::Ref(array_type, kNullable), 1);

  ExpectFailure(
      &sig,
      {WASM_LOCAL_TEE(0, WASM_ARRAY_NEW_DEFAULT(array_type, WASM_I32V(5),
                                                WASM_RTT_CANON(array_type)))},
      kAppendEnd, "expected (ref 0), got (ref null 0)");
}

// This tests that num_locals_ in decoder remains consistent, even if we fail
// mid-DecodeLocals().
TEST_F(FunctionBodyDecoderTest, Regress_1154439) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);
  AddLocals(kWasmI32, 1);
  AddLocals(kWasmI64, 1000000);
  ExpectFailure(sigs.v_v(), {}, kAppendEnd, "local count too large");
}

TEST_F(FunctionBodyDecoderTest, DropOnEmptyStack) {
  // Valid code:
  ExpectValidates(sigs.v_v(), {kExprI32Const, 1, kExprDrop}, kAppendEnd);
  // Invalid code (dropping from empty stack):
  ExpectFailure(sigs.v_v(), {kExprDrop}, kAppendEnd,
                "not enough arguments on the stack for drop");
  // Valid code (dropping from empty stack in unreachable code):
  ExpectValidates(sigs.v_v(), {kExprUnreachable, kExprDrop}, kAppendEnd);
}

class BranchTableIteratorTest : public TestWithZone {
 public:
  BranchTableIteratorTest() : TestWithZone() {}
  void CheckBrTableSize(const byte* start, const byte* end) {
    Decoder decoder(start, end);
    BranchTableImmediate<Decoder::kFullValidation> operand(&decoder, start + 1);
    BranchTableIterator<Decoder::kFullValidation> iterator(&decoder, operand);
    EXPECT_EQ(end - start - 1u, iterator.length());
    EXPECT_OK(decoder);
  }
  void CheckBrTableError(const byte* start, const byte* end) {
    Decoder decoder(start, end);
    BranchTableImmediate<Decoder::kFullValidation> operand(&decoder, start + 1);
    BranchTableIterator<Decoder::kFullValidation> iterator(&decoder, operand);
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

  // Helper to check for prefixed opcodes, which can have multiple bytes.
  void ExpectLengthPrefixed(unsigned operands, WasmOpcode opcode) {
    uint8_t prefix = (opcode >> 8) & 0xff;
    DCHECK(WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(prefix)));
    uint8_t index = opcode & 0xff;
    uint8_t encoded[2] = {0, 0};
    uint8_t* p = encoded;
    unsigned len = static_cast<unsigned>(LEBHelper::sizeof_u32v(index));
    DCHECK_GE(2, len);
    LEBHelper::write_u32v(&p, index);
    // length of index, + number of operands + prefix bye
    ExpectLength(len + operands + 1, prefix, encoded[0], encoded[1]);
  }

  template <typename... Bytes>
  void ExpectFailure(Bytes... bytes) {
    const byte code[] = {bytes..., 0, 0, 0, 0, 0, 0, 0, 0};
    WasmFeatures no_features = WasmFeatures::None();
    WasmDecoder<Decoder::kFullValidation> decoder(
        this->zone(), nullptr, no_features, &no_features, nullptr, code,
        code + sizeof(code), 0);
    WasmDecoder<Decoder::kFullValidation>::OpcodeLength(&decoder, code);
    EXPECT_EQ(decoder.failed(), true);
  }
};

TEST_F(WasmOpcodeLengthTest, Statements) {
  ExpectLength(1, kExprNop);
  ExpectLength(1, kExprElse);
  ExpectLength(1, kExprEnd);
  ExpectLength(1, kExprSelect);
  ExpectLength(2, kExprCatch);
  ExpectLength(2, kExprDelegate);
  ExpectLength(2, kExprRethrow);
  ExpectLength(2, kExprBr);
  ExpectLength(2, kExprBrIf);
  ExpectLength(2, kExprThrow);
  ExpectLength(2, kExprBlock, kI32Code);
  ExpectLength(2, kExprLoop, kI32Code);
  ExpectLength(2, kExprIf, kI32Code);
  ExpectLength(2, kExprTry, kI32Code);
}

TEST_F(WasmOpcodeLengthTest, MiscExpressions) {
  ExpectLength(5, kExprF32Const);
  ExpectLength(9, kExprF64Const);
  ExpectLength(2, kExprRefNull);
  ExpectLength(2, kExprLocalGet);
  ExpectLength(2, kExprLocalSet);
  ExpectLength(2, kExprGlobalGet);
  ExpectLength(2, kExprGlobalSet);
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
  ExpectLength(2, kExprGlobalGet, U32V_1(1));
  ExpectLength(3, kExprGlobalGet, U32V_2(33));
  ExpectLength(4, kExprGlobalGet, U32V_3(44));
  ExpectLength(5, kExprGlobalGet, U32V_4(66));
  ExpectLength(6, kExprGlobalGet, U32V_5(77));

  ExpectLength(2, kExprRefFunc, U32V_1(1));
  ExpectLength(3, kExprRefFunc, U32V_2(33));
  ExpectLength(4, kExprRefFunc, U32V_3(44));
  ExpectLength(5, kExprRefFunc, U32V_4(66));
  ExpectLength(6, kExprRefFunc, U32V_5(77));

  ExpectLength(2, kExprTableGet, U32V_1(1));
  ExpectLength(3, kExprTableGet, U32V_2(33));
  ExpectLength(4, kExprTableGet, U32V_3(44));
  ExpectLength(5, kExprTableGet, U32V_4(66));
  ExpectLength(6, kExprTableGet, U32V_5(77));

  ExpectLength(2, kExprTableSet, U32V_1(1));
  ExpectLength(3, kExprTableSet, U32V_2(33));
  ExpectLength(4, kExprTableSet, U32V_3(44));
  ExpectLength(5, kExprTableSet, U32V_4(66));
  ExpectLength(6, kExprTableSet, U32V_5(77));

  ExpectLength(3, kExprCallIndirect, U32V_1(1), U32V_1(1));
  ExpectLength(4, kExprCallIndirect, U32V_1(1), U32V_2(33));
  ExpectLength(5, kExprCallIndirect, U32V_1(1), U32V_3(44));
  ExpectLength(6, kExprCallIndirect, U32V_1(1), U32V_4(66));
  ExpectLength(7, kExprCallIndirect, U32V_1(1), U32V_5(77));
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
#define TEST_SIMD(name, opcode, sig) ExpectLengthPrefixed(0, kExpr##name);
  FOREACH_SIMD_0_OPERAND_OPCODE(TEST_SIMD)
#undef TEST_SIMD
#define TEST_SIMD(name, opcode, sig) ExpectLengthPrefixed(1, kExpr##name);
  FOREACH_SIMD_1_OPERAND_OPCODE(TEST_SIMD)
#undef TEST_SIMD
  ExpectLengthPrefixed(16, kExprI8x16Shuffle);
  // test for bad simd opcode, 0xFF is encoded in two bytes.
  ExpectLength(3, kSimdPrefix, 0xFF, 0x1);
}

TEST_F(WasmOpcodeLengthTest, IllegalRefIndices) {
  ExpectFailure(kExprBlock, kOptRefCode, U32V_3(kV8MaxWasmTypes + 1));
  ExpectFailure(kExprBlock, kOptRefCode, U32V_4(0x01000000));
}

TEST_F(WasmOpcodeLengthTest, PrefixedOpcodesLEB) {
  // kExprI32New with a 4-byte LEB-encoded opcode.
  ExpectLength(5, 0xfb, 0xa0, 0x80, 0x80, 0x00);

  // kExprI8x16Splat with a 3-byte LEB-encoded opcode.
  ExpectLength(4, 0xfd, 0x8f, 0x80, 0x00);

  // kExprI32SConvertSatF32 with a 4-byte LEB-encoded opcode.
  ExpectLength(5, 0xfc, 0x80, 0x80, 0x80, 0x00);

  // kExprAtomicNotify with a 2-byte LEB-encoded opcode, and 2 i32 imm for
  // memarg.
  ExpectLength(5, 0xfe, 0x80, 0x00, 0x00, 0x00);
}

class TypeReaderTest : public TestWithZone {
 public:
  ValueType DecodeValueType(const byte* start, const byte* end,
                            const WasmModule* module) {
    Decoder decoder(start, end);
    uint32_t length;
    return value_type_reader::read_value_type<Decoder::kFullValidation>(
        &decoder, start, &length, module, enabled_features_);
  }

  HeapType DecodeHeapType(const byte* start, const byte* end,
                          const WasmModule* module) {
    Decoder decoder(start, end);
    uint32_t length;
    return value_type_reader::read_heap_type<Decoder::kFullValidation>(
        &decoder, start, &length, module, enabled_features_);
  }

  // This variable is modified by WASM_FEATURE_SCOPE.
  WasmFeatures enabled_features_;
};

TEST_F(TypeReaderTest, HeapTypeDecodingTest) {
  WASM_FEATURE_SCOPE(gc);
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);

  HeapType heap_func = HeapType(HeapType::kFunc);
  HeapType heap_bottom = HeapType(HeapType::kBottom);

  // 1- to 5-byte representation of kFuncRefCode.
  {
    const byte data[] = {kFuncRefCode};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_TRUE(result == heap_func);
  }
  {
    const byte data[] = {kFuncRefCode | 0x80, 0x7F};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_EQ(result, heap_func);
  }
  {
    const byte data[] = {kFuncRefCode | 0x80, 0xFF, 0x7F};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_EQ(result, heap_func);
  }
  {
    const byte data[] = {kFuncRefCode | 0x80, 0xFF, 0xFF, 0x7F};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_EQ(result, heap_func);
  }
  {
    const byte data[] = {kFuncRefCode | 0x80, 0xFF, 0xFF, 0xFF, 0x7F};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_EQ(result, heap_func);
  }

  {
    // Some negative number.
    const byte data[] = {0xB4, 0x7F};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_EQ(result, heap_bottom);
  }

  {
    // This differs from kFuncRefCode by one bit outside the 1-byte LEB128
    // range. This should therefore NOT be decoded as HeapType::kFunc and
    // instead fail.
    const byte data[] = {kFuncRefCode | 0x80, 0x6F};
    HeapType result = DecodeHeapType(data, data + sizeof(data), nullptr);
    EXPECT_EQ(result, heap_bottom);
  }
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
    WasmModule module;
    return i::wasm::DecodeLocalDecls(enabled_features_, decls, &module, start,
                                     end);
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

TEST_F(LocalDeclDecoderTest, WrongLocalDeclsCount1) {
  static const byte data[] = {1};
  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
  EXPECT_FALSE(result);
}

TEST_F(LocalDeclDecoderTest, WrongLocalDeclsCount2) {
  static const byte data[] = {2, 1,
                              static_cast<byte>(kWasmI32.value_type_code())};
  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
  EXPECT_FALSE(result);
}

TEST_F(LocalDeclDecoderTest, OneLocal) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType type = kValueTypes[i];
    const byte data[] = {1, 1, static_cast<byte>(type.value_type_code())};
    BodyLocalDecls decls(zone());
    bool result = DecodeLocalDecls(&decls, data, data + sizeof(data));
    EXPECT_TRUE(result);
    EXPECT_EQ(1u, decls.type_list.size());

    TypesOfLocals map = decls.type_list;
    EXPECT_EQ(type, map[0]);
  }
}

TEST_F(LocalDeclDecoderTest, FiveLocals) {
  WASM_FEATURE_SCOPE(reftypes);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueType type = kValueTypes[i];
    const byte data[] = {1, 5, static_cast<byte>(type.value_type_code())};
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
          const byte data[] = {4, a,        kI32Code, b,       kI64Code,
                               c, kF32Code, d,        kF64Code};
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

TEST_F(LocalDeclDecoderTest, InvalidTypeIndex) {
  WASM_FEATURE_SCOPE(reftypes);
  WASM_FEATURE_SCOPE(typed_funcref);

  const byte* data = nullptr;
  const byte* end = nullptr;
  LocalDeclEncoder local_decls(zone());

  local_decls.AddLocals(1, ValueType::Ref(0, kNullable));
  BodyLocalDecls decls(zone());
  bool result = DecodeLocalDecls(&decls, data, end);
  EXPECT_FALSE(result);
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
  byte code[] = {1, 1, kI32Code, WASM_I32V_1(9), WASM_I32V_1(11)};
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

/*******************************************************************************
 * Memory64 tests
 ******************************************************************************/

class FunctionBodyDecoderTestOnBothMemoryTypes
    : public FunctionBodyDecoderTestBase<::testing::TestWithParam<MemoryType>> {
 public:
  bool is_memory64() const { return GetParam() == kMemory64; }
};

std::string PrintMemoryType(::testing::TestParamInfo<MemoryType> info) {
  switch (info.param) {
    case kMemory32:
      return "kMemory32";
    case kMemory64:
      return "kMemory64";
  }
  UNREACHABLE();
}

INSTANTIATE_TEST_SUITE_P(MemoryTypes, FunctionBodyDecoderTestOnBothMemoryTypes,
                         ::testing::Values(kMemory32, kMemory64),
                         PrintMemoryType);

TEST_P(FunctionBodyDecoderTestOnBothMemoryTypes, IndexTypes) {
  builder.InitializeMemory(GetParam());
  Validate(!is_memory64(), sigs.i_v(),
           {WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)});
  Validate(is_memory64(), sigs.i_v(),
           {WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO64)});
  Validate(!is_memory64(), sigs.v_v(),
           {WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO, WASM_ZERO)});
  Validate(is_memory64(), sigs.v_v(),
           {WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO64, WASM_ZERO)});
}

TEST_P(FunctionBodyDecoderTestOnBothMemoryTypes, 64BitOffset) {
  builder.InitializeMemory(GetParam());
  // Macro for defining a zero constant of the right type. Explicitly use
  // {uint8_t} to make MSVC happy.
#define ZERO_FOR_TYPE \
  WASM_SEQ(is_memory64() ? uint8_t{kExprI64Const} : uint8_t{kExprI32Const}, 0)
  // Offset is zero encoded in 5 bytes (works always).
  Validate(
      true, sigs.i_v(),
      {WASM_LOAD_MEM_OFFSET(MachineType::Int32(), U64V_5(0), ZERO_FOR_TYPE)});
  // Offset is zero encoded in 6 bytes (works only in memory64).
  Validate(
      is_memory64(), sigs.i_v(),
      {WASM_LOAD_MEM_OFFSET(MachineType::Int32(), U64V_6(0), ZERO_FOR_TYPE)});
  // Same with store.
  Validate(true, sigs.v_v(),
           {WASM_STORE_MEM_OFFSET(MachineType::Int32(), U64V_5(0),
                                  ZERO_FOR_TYPE, WASM_ZERO)});
  Validate(is_memory64(), sigs.v_v(),
           {WASM_STORE_MEM_OFFSET(MachineType::Int32(), U64V_6(0),
                                  ZERO_FOR_TYPE, WASM_ZERO)});
#undef ZERO_FOR_TYPE
}

TEST_P(FunctionBodyDecoderTestOnBothMemoryTypes, MemorySize) {
  builder.InitializeMemory(GetParam());
  // memory.size returns i32 on memory32.
  Validate(!is_memory64(), sigs.v_v(),
           {WASM_MEMORY_SIZE, kExprI32Eqz, kExprDrop});
  // memory.size returns i64 on memory64.
  Validate(is_memory64(), sigs.v_v(),
           {WASM_MEMORY_SIZE, kExprI64Eqz, kExprDrop});
}

TEST_P(FunctionBodyDecoderTestOnBothMemoryTypes, MemoryGrow) {
  builder.InitializeMemory(GetParam());
  // memory.grow is i32->i32 memory32.
  Validate(!is_memory64(), sigs.i_i(), {WASM_MEMORY_GROW(WASM_LOCAL_GET(0))});
  // memory.grow is i64->i64 memory32.
  Validate(is_memory64(), sigs.l_l(), {WASM_MEMORY_GROW(WASM_LOCAL_GET(0))});
  // any other combination always fails.
  auto sig_l_i = MakeSig::Returns(kWasmI64).Params(kWasmI32);
  ExpectFailure(&sig_l_i, {WASM_MEMORY_GROW(WASM_LOCAL_GET(0))});
  auto sig_i_l = MakeSig::Returns(kWasmI32).Params(kWasmI64);
  ExpectFailure(&sig_i_l, {WASM_MEMORY_GROW(WASM_LOCAL_GET(0))});
}

#undef B1
#undef B2
#undef B3
#undef WASM_IF_OP
#undef WASM_LOOP_OP
#undef WASM_BRV_IF_ZERO
#undef EXPECT_OK

}  // namespace function_body_decoder_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8

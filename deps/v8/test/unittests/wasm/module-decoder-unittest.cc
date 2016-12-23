// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

#define WASM_INIT_EXPR_I32V_1(val) WASM_I32V_1(val), kExprEnd
#define WASM_INIT_EXPR_I32V_2(val) WASM_I32V_2(val), kExprEnd
#define WASM_INIT_EXPR_I32V_3(val) WASM_I32V_3(val), kExprEnd
#define WASM_INIT_EXPR_I32V_4(val) WASM_I32V_4(val), kExprEnd
#define WASM_INIT_EXPR_I32V_5(val) WASM_I32V_5(val), kExprEnd
#define WASM_INIT_EXPR_F32(val) WASM_F32(val), kExprEnd
#define WASM_INIT_EXPR_I64(val) WASM_I64(val), kExprEnd
#define WASM_INIT_EXPR_F64(val) WASM_F64(val), kExprEnd
#define WASM_INIT_EXPR_GLOBAL(index) WASM_GET_GLOBAL(index), kExprEnd

#define SIZEOF_EMPTY_FUNCTION ((size_t)5)
#define EMPTY_BODY 0
#define SIZEOF_EMPTY_BODY ((size_t)1)
#define NOP_BODY 2, 0, kExprNop
#define SIZEOF_NOP_BODY ((size_t)3)

#define SIG_ENTRY_i_i SIG_ENTRY_x_x(kLocalI32, kLocalI32)

#define UNKNOWN_SECTION(size) 0, U32V_1(size + 5), 4, 'l', 'u', 'l', 'z'

#define SECTION(name, size) k##name##SectionCode, U32V_1(size)

#define SIGNATURES_SECTION(count, ...) \
  SECTION(Type, 1 + 3 * (count)), U32V_1(count), __VA_ARGS__
#define FUNCTION_SIGNATURES_SECTION(count, ...) \
  SECTION(Function, 1 + (count)), U32V_1(count), __VA_ARGS__

#define FOO_STRING 3, 'f', 'o', 'o'
#define NO_LOCAL_NAMES 0

#define EMPTY_SIGNATURES_SECTION SECTION(Type, 1), 0
#define EMPTY_FUNCTION_SIGNATURES_SECTION SECTION(Function, 1), 0
#define EMPTY_FUNCTION_BODIES_SECTION SECTION(Code, 1), 0
#define SECTION_NAMES(size) \
  kUnknownSectionCode, U32V_1(size + 5), 4, 'n', 'a', 'm', 'e'
#define EMPTY_NAMES_SECTION SECTION_NAMES(1), 0

#define X1(...) __VA_ARGS__
#define X2(...) __VA_ARGS__, __VA_ARGS__
#define X3(...) __VA_ARGS__, __VA_ARGS__, __VA_ARGS__
#define X4(...) __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__

#define ONE_EMPTY_FUNCTION SECTION(Function, 1 + 1 * 1), 1, X1(0)

#define TWO_EMPTY_FUNCTIONS SECTION(Function, 1 + 2 * 1), 2, X2(0)

#define THREE_EMPTY_FUNCTIONS SECTION(Function, 1 + 3 * 1), 3, X3(0)

#define FOUR_EMPTY_FUNCTIONS SECTION(Function, 1 + 4 * 1), 4, X4(0)

#define ONE_EMPTY_BODY                           \
  SECTION(Code, 1 + 1 * (1 + SIZEOF_EMPTY_BODY)) \
  , 1, X1(SIZEOF_EMPTY_BODY, EMPTY_BODY)

#define TWO_EMPTY_BODIES                         \
  SECTION(Code, 1 + 2 * (1 + SIZEOF_EMPTY_BODY)) \
  , 2, X2(SIZEOF_EMPTY_BODY, EMPTY_BODY)

#define THREE_EMPTY_BODIES                       \
  SECTION(Code, 1 + 3 * (1 + SIZEOF_EMPTY_BODY)) \
  , 3, X3(SIZEOF_EMPTY_BODY, EMPTY_BODY)

#define FOUR_EMPTY_BODIES                        \
  SECTION(Code, 1 + 4 * (1 + SIZEOF_EMPTY_BODY)) \
  , 4, X4(SIZEOF_EMPTY_BODY, EMPTY_BODY)

#define SIGNATURES_SECTION_VOID_VOID \
  SECTION(Type, 1 + SIZEOF_SIG_ENTRY_v_v), 1, SIG_ENTRY_v_v

#define LINEAR_MEMORY_INDEX_0 0

#define EXPECT_VERIFIES(data)                                      \
  do {                                                             \
    ModuleResult result = DecodeModule(data, data + sizeof(data)); \
    EXPECT_TRUE(result.ok());                                      \
    if (result.val) delete result.val;                             \
  } while (false)

#define EXPECT_FAILURE_LEN(data, length)                     \
  do {                                                       \
    ModuleResult result = DecodeModule(data, data + length); \
    EXPECT_FALSE(result.ok());                               \
    if (result.val) delete result.val;                       \
  } while (false)

#define EXPECT_FAILURE(data) EXPECT_FAILURE_LEN(data, sizeof(data))

#define EXPECT_OFF_END_FAILURE(data, min, max)          \
  do {                                                  \
    for (size_t length = min; length < max; length++) { \
      EXPECT_FAILURE_LEN(data, length);                 \
    }                                                   \
  } while (false)

#define EXPECT_OK(result)                \
  do {                                   \
    EXPECT_TRUE(result.ok());            \
    if (!result.ok()) {                  \
      if (result.val) delete result.val; \
      return;                            \
    }                                    \
  } while (false)

static size_t SizeOfVarInt(size_t value) {
  size_t size = 0;
  do {
    size++;
    value = value >> 7;
  } while (value > 0);
  return size;
}

struct LocalTypePair {
  uint8_t code;
  LocalType type;
} kLocalTypes[] = {{kLocalI32, kAstI32},
                   {kLocalI64, kAstI64},
                   {kLocalF32, kAstF32},
                   {kLocalF64, kAstF64}};

class WasmModuleVerifyTest : public TestWithIsolateAndZone {
 public:
  ModuleResult DecodeModule(const byte* module_start, const byte* module_end) {
    // Add the WASM magic and version number automatically.
    size_t size = static_cast<size_t>(module_end - module_start);
    byte header[] = {WASM_MODULE_HEADER};
    size_t total = sizeof(header) + size;
    auto temp = new byte[total];
    memcpy(temp, header, sizeof(header));
    memcpy(temp + sizeof(header), module_start, size);
    ModuleResult result = DecodeWasmModule(isolate(), zone(), temp,
                                           temp + total, false, kWasmOrigin);
    delete[] temp;
    return result;
  }
  ModuleResult DecodeModuleNoHeader(const byte* module_start,
                                    const byte* module_end) {
    return DecodeWasmModule(isolate(), zone(), module_start, module_end, false,
                            kWasmOrigin);
  }
};

TEST_F(WasmModuleVerifyTest, WrongMagic) {
  for (uint32_t x = 1; x; x <<= 1) {
    const byte data[] = {U32_LE(kWasmMagic ^ x), U32_LE(kWasmVersion)};
    ModuleResult result = DecodeModuleNoHeader(data, data + sizeof(data));
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, WrongVersion) {
  for (uint32_t x = 1; x; x <<= 1) {
    const byte data[] = {U32_LE(kWasmMagic), U32_LE(kWasmVersion ^ x)};
    ModuleResult result = DecodeModuleNoHeader(data, data + sizeof(data));
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, DecodeEmpty) {
  ModuleResult result = DecodeModule(nullptr, 0);
  EXPECT_TRUE(result.ok());
  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, OneGlobal) {
  static const byte data[] = {
      SECTION(Global, 6),  // --
      1,
      kLocalI32,                 // local type
      0,                         // immutable
      WASM_INIT_EXPR_I32V_1(13)  // init
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(1, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(0, result.val->data_segments.size());

    const WasmGlobal* global = &result.val->globals.back();

    EXPECT_EQ(kAstI32, global->type);
    EXPECT_EQ(0, global->offset);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kI32Const, global->init.kind);
    EXPECT_EQ(13, global->init.val.i32_const);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, Global_invalid_type) {
  static const byte data[] = {
      SECTION(Global, 6),  // --
      1,
      64,                         // invalid memory type
      1,                          // mutable
      WASM_INIT_EXPR_I32V_1(33),  // init
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_FALSE(result.ok());
  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, Global_invalid_type2) {
  static const byte data[] = {
      SECTION(Global, 6),  // --
      1,
      kLocalVoid,                 // invalid memory type
      1,                          // mutable
      WASM_INIT_EXPR_I32V_1(33),  // init
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_FALSE(result.ok());
  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ZeroGlobals) {
  static const byte data[] = {
      SECTION(Global, 1),  // --
      0,                   // declare 0 globals
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.val) delete result.val;
}

static void AppendUint32v(std::vector<byte>& buffer, uint32_t val) {
  while (true) {
    uint32_t next = val >> 7;
    uint32_t out = val & 0x7f;
    if (next) {
      buffer.push_back(static_cast<byte>(0x80 | out));
      val = next;
    } else {
      buffer.push_back(static_cast<byte>(out));
      break;
    }
  }
}

TEST_F(WasmModuleVerifyTest, NGlobals) {
  static const byte data[] = {
      kLocalF32,                // memory type
      0,                        // immutable
      WASM_INIT_EXPR_F32(7.7),  // init
  };

  for (uint32_t i = 0; i < 1000000; i = i * 13 + 1) {
    std::vector<byte> buffer;
    size_t size = SizeOfVarInt(i) + i * sizeof(data);
    const byte globals[] = {kGlobalSectionCode, U32V_5(size)};
    for (size_t g = 0; g != sizeof(globals); ++g) {
      buffer.push_back(globals[g]);
    }
    AppendUint32v(buffer, i);  // Number of globals.
    for (uint32_t j = 0; j < i; j++) {
      buffer.insert(buffer.end(), data, data + sizeof(data));
    }

    ModuleResult result = DecodeModule(&buffer[0], &buffer[0] + buffer.size());
    EXPECT_OK(result);
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, GlobalWithInvalidMemoryType) {
  static const byte data[] = {SECTION(Global, 7),
                              33,  // memory type
                              0,   // exported
                              WASM_INIT_EXPR_I32V_1(1)};

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, TwoGlobals) {
  static const byte data[] = {
      SECTION(Global, 21),
      2,
      kLocalF32,  // type
      0,          // immutable
      WASM_INIT_EXPR_F32(22.0),
      kLocalF64,  // type
      1,          // mutable
      WASM_INIT_EXPR_F64(23.0),
  };

  {
    // Should decode to exactly two globals.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(2, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(0, result.val->data_segments.size());

    const WasmGlobal* g0 = &result.val->globals[0];

    EXPECT_EQ(kAstF32, g0->type);
    EXPECT_EQ(0, g0->offset);
    EXPECT_FALSE(g0->mutability);
    EXPECT_EQ(WasmInitExpr::kF32Const, g0->init.kind);

    const WasmGlobal* g1 = &result.val->globals[1];

    EXPECT_EQ(kAstF64, g1->type);
    EXPECT_EQ(8, g1->offset);
    EXPECT_TRUE(g1->mutability);
    EXPECT_EQ(WasmInitExpr::kF64Const, g1->init.kind);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, OneSignature) {
  {
    static const byte data[] = {SIGNATURES_SECTION_VOID_VOID};
    EXPECT_VERIFIES(data);
  }

  {
    static const byte data[] = {SECTION(Type, 1 + SIZEOF_SIG_ENTRY_x_x), 1,
                                SIG_ENTRY_i_i};
    EXPECT_VERIFIES(data);
  }
}

TEST_F(WasmModuleVerifyTest, MultipleSignatures) {
  static const byte data[] = {
      SECTION(Type, 1 + SIZEOF_SIG_ENTRY_v_v + SIZEOF_SIG_ENTRY_x_x +
                        SIZEOF_SIG_ENTRY_x_xx),         // --
      3,                                                // --
      SIG_ENTRY_v_v,                                    // void -> void
      SIG_ENTRY_x_x(kLocalI32, kLocalF32),              // f32 -> i32
      SIG_ENTRY_x_xx(kLocalI32, kLocalF64, kLocalF64),  // f64,f64 -> i32
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(3, result.val->signatures.size());
  if (result.val->signatures.size() == 3) {
    EXPECT_EQ(0, result.val->signatures[0]->return_count());
    EXPECT_EQ(1, result.val->signatures[1]->return_count());
    EXPECT_EQ(1, result.val->signatures[2]->return_count());

    EXPECT_EQ(0, result.val->signatures[0]->parameter_count());
    EXPECT_EQ(1, result.val->signatures[1]->parameter_count());
    EXPECT_EQ(2, result.val->signatures[2]->parameter_count());
  }
  if (result.val) delete result.val;

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, OneDataSegment) {
  const byte kDataSegmentSourceOffset = 24;
  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kResizableMaximumFlag,
      28,
      28,
      SECTION(Data, 11),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x9bbaa),  // dest addr
      U32V_1(3),                       // source size
      'a',
      'b',
      'c'  // data bytes
  };

  {
    EXPECT_VERIFIES(data);
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(0, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(1, result.val->data_segments.size());

    const WasmDataSegment* segment = &result.val->data_segments.back();

    EXPECT_EQ(WasmInitExpr::kI32Const, segment->dest_addr.kind);
    EXPECT_EQ(0x9bbaa, segment->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegmentSourceOffset, segment->source_offset);
    EXPECT_EQ(3, segment->source_size);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 14, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, TwoDataSegments) {
  const byte kDataSegment0SourceOffset = 24;
  const byte kDataSegment1SourceOffset = kDataSegment0SourceOffset + 11;

  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kResizableMaximumFlag,
      28,
      28,
      SECTION(Data, 29),
      ENTRY_COUNT(2),  // segment count
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x7ffee),  // #0: dest addr
      U32V_1(4),                       // source size
      1,
      2,
      3,
      4,  // data bytes
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x6ddcc),  // #1: dest addr
      U32V_1(10),                      // source size
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10  // data bytes
  };

  {
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(0, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(2, result.val->data_segments.size());

    const WasmDataSegment* s0 = &result.val->data_segments[0];
    const WasmDataSegment* s1 = &result.val->data_segments[1];

    EXPECT_EQ(WasmInitExpr::kI32Const, s0->dest_addr.kind);
    EXPECT_EQ(0x7ffee, s0->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegment0SourceOffset, s0->source_offset);
    EXPECT_EQ(4, s0->source_size);

    EXPECT_EQ(WasmInitExpr::kI32Const, s1->dest_addr.kind);
    EXPECT_EQ(0x6ddcc, s1->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegment1SourceOffset, s1->source_offset);
    EXPECT_EQ(10, s1->source_size);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 14, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, DataSegment_wrong_init_type) {
  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kResizableMaximumFlag,
      28,
      28,
      SECTION(Data, 11),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_F64(9.9),  // dest addr
      U32V_1(3),                // source size
      'a',
      'b',
      'c'  // data bytes
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, OneIndirectFunction) {
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), kWasmAnyFunctionTypeForm, 0, 1};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.ok()) {
    EXPECT_EQ(1, result.val->signatures.size());
    EXPECT_EQ(1, result.val->functions.size());
    EXPECT_EQ(1, result.val->function_tables.size());
    EXPECT_EQ(1, result.val->function_tables[0].values.size());
    EXPECT_EQ(-1, result.val->function_tables[0].values[0]);
  }
  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, OneIndirectFunction_one_entry) {
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), kWasmAnyFunctionTypeForm, 0, 1,
      // elements ------------------------------------------------------------
      SECTION(Element, 7),
      1,  // entry count
      TABLE_INDEX(0), WASM_INIT_EXPR_I32V_1(0),
      1,  // elements count
      FUNC_INDEX(0)};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.ok()) {
    EXPECT_EQ(1, result.val->signatures.size());
    EXPECT_EQ(1, result.val->functions.size());
    EXPECT_EQ(1, result.val->function_tables.size());
    EXPECT_EQ(1, result.val->function_tables[0].values.size());
    EXPECT_EQ(0, result.val->function_tables[0].values[0]);
  }
  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, MultipleIndirectFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SECTION(Type, 1 + SIZEOF_SIG_ENTRY_v_v + SIZEOF_SIG_ENTRY_v_x),
      2,                         // --
      SIG_ENTRY_v_v,             // void -> void
      SIG_ENTRY_v_x(kLocalI32),  // void -> i32
      // funcs ------------------------------------------------------
      FOUR_EMPTY_FUNCTIONS,
      // table declaration -------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), kWasmAnyFunctionTypeForm, 0, 8,
      // table elements ----------------------------------------------
      SECTION(Element, 14),
      1,  // entry count
      TABLE_INDEX(0), WASM_INIT_EXPR_I32V_1(0),
      8,              // elements count
      FUNC_INDEX(0),  // --
      FUNC_INDEX(1),  // --
      FUNC_INDEX(2),  // --
      FUNC_INDEX(3),  // --
      FUNC_INDEX(0),  // --
      FUNC_INDEX(1),  // --
      FUNC_INDEX(2),  // --
      FUNC_INDEX(3),  // --
      FOUR_EMPTY_BODIES};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.ok()) {
    EXPECT_EQ(2, result.val->signatures.size());
    EXPECT_EQ(4, result.val->functions.size());
    EXPECT_EQ(1, result.val->function_tables.size());
    EXPECT_EQ(8, result.val->function_tables[0].values.size());
    for (int i = 0; i < 8; i++) {
      EXPECT_EQ(i & 3, result.val->function_tables[0].values[i]);
    }
  }
  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, IndirectFunctionNoFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // indirect table ----------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), 1, 0, 0,
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, IndirectFunctionInvalidIndex) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // functions ---------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // indirect table ----------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), 1, 1, 0,
  };

  EXPECT_FAILURE(data);
}

class WasmSignatureDecodeTest : public TestWithZone {};

TEST_F(WasmSignatureDecodeTest, Ok_v_v) {
  static const byte data[] = {SIG_ENTRY_v_v};
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator);
  FunctionSig* sig =
      DecodeWasmSignatureForTesting(&zone, data, data + sizeof(data));

  EXPECT_TRUE(sig != nullptr);
  EXPECT_EQ(0, sig->parameter_count());
  EXPECT_EQ(0, sig->return_count());
}

TEST_F(WasmSignatureDecodeTest, Ok_t_v) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalTypePair ret_type = kLocalTypes[i];
    const byte data[] = {SIG_ENTRY_x(ret_type.code)};
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));

    EXPECT_TRUE(sig != nullptr);
    EXPECT_EQ(0, sig->parameter_count());
    EXPECT_EQ(1, sig->return_count());
    EXPECT_EQ(ret_type.type, sig->GetReturn());
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_v_t) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalTypePair param_type = kLocalTypes[i];
    const byte data[] = {SIG_ENTRY_v_x(param_type.code)};
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));

    EXPECT_TRUE(sig != nullptr);
    EXPECT_EQ(1, sig->parameter_count());
    EXPECT_EQ(0, sig->return_count());
    EXPECT_EQ(param_type.type, sig->GetParam(0));
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_t_t) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalTypePair ret_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalTypePair param_type = kLocalTypes[j];
      const byte data[] = {SIG_ENTRY_x_x(ret_type.code, param_type.code)};
      FunctionSig* sig =
          DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));

      EXPECT_TRUE(sig != nullptr);
      EXPECT_EQ(1, sig->parameter_count());
      EXPECT_EQ(1, sig->return_count());
      EXPECT_EQ(param_type.type, sig->GetParam(0));
      EXPECT_EQ(ret_type.type, sig->GetReturn());
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_i_tt) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalTypePair p0_type = kLocalTypes[i];
    for (size_t j = 0; j < arraysize(kLocalTypes); j++) {
      LocalTypePair p1_type = kLocalTypes[j];
      const byte data[] = {
          SIG_ENTRY_x_xx(kLocalI32, p0_type.code, p1_type.code)};
      FunctionSig* sig =
          DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));

      EXPECT_TRUE(sig != nullptr);
      EXPECT_EQ(2, sig->parameter_count());
      EXPECT_EQ(1, sig->return_count());
      EXPECT_EQ(p0_type.type, sig->GetParam(0));
      EXPECT_EQ(p1_type.type, sig->GetParam(1));
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_off_end) {
  byte data[256];
  for (int p = 0; p <= 255; p = p + 1 + p * 3) {
    for (int i = 0; i <= p; i++) data[i] = kLocalI32;
    data[0] = static_cast<byte>(p);

    for (int i = 0; i < p + 1; i++) {
      // Should fall off the end for all signatures.
      FunctionSig* sig = DecodeWasmSignatureForTesting(zone(), data, data + i);
      EXPECT_EQ(nullptr, sig);
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_type) {
  byte kInvalidType = 76;
  for (size_t i = 0; i < SIZEOF_SIG_ENTRY_x_xx; i++) {
    byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalI32)};
    data[i] = kInvalidType;
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));
    EXPECT_EQ(nullptr, sig);
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_ret_type1) {
  static const byte data[] = {SIG_ENTRY_x_x(kLocalVoid, kLocalI32)};
  FunctionSig* sig =
      DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type1) {
  static const byte data[] = {SIG_ENTRY_x_x(kLocalI32, kLocalVoid)};
  FunctionSig* sig =
      DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type2) {
  static const byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalVoid)};
  FunctionSig* sig =
      DecodeWasmSignatureForTesting(zone(), data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

class WasmFunctionVerifyTest : public TestWithIsolateAndZone {};

TEST_F(WasmFunctionVerifyTest, Ok_v_v_empty) {
  static const byte data[] = {
      SIG_ENTRY_v_v,  // signature entry
      4,              // locals
      3,
      kLocalI32,  // --
      4,
      kLocalI64,  // --
      5,
      kLocalF32,  // --
      6,
      kLocalF64,  // --
      kExprNop    // body
  };

  FunctionResult result =
      DecodeWasmFunction(isolate(), zone(), nullptr, data, data + sizeof(data));
  EXPECT_OK(result);

  if (result.val && result.ok()) {
    WasmFunction* function = result.val;
    EXPECT_EQ(0, function->sig->parameter_count());
    EXPECT_EQ(0, function->sig->return_count());
    EXPECT_EQ(0, function->name_offset);
    EXPECT_EQ(SIZEOF_SIG_ENTRY_v_v, function->code_start_offset);
    EXPECT_EQ(sizeof(data), function->code_end_offset);
    // TODO(titzer): verify encoding of local declarations
  }

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, SectionWithoutNameLength) {
  const byte data[] = {1};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, TheLoneliestOfValidModulesTheTrulyEmptyOne) {
  const byte data[] = {
      0,  // unknown section code.
      0,  // Empty section name.
          // No section name, no content, nothing but sadness.
      0,  // No section content.
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, OnlyUnknownSectionEmpty) {
  const byte data[] = {
      UNKNOWN_SECTION(0),
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, OnlyUnknownSectionNonEmpty) {
  const byte data[] = {
      UNKNOWN_SECTION(5),
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,  // section data
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, SignatureFollowedByEmptyUnknownSection) {
  const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      // -----------------------------------------------------------
      UNKNOWN_SECTION(0)};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, SignatureFollowedByUnknownSection) {
  const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      // -----------------------------------------------------------
      UNKNOWN_SECTION(5), 0xff, 0xff, 0xff, 0xff, 0xff,
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionOverflow) {
  static const byte data[] = {
      UNKNOWN_SECTION(9),
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,  // 10 byte section
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionUnderflow) {
  static const byte data[] = {
      UNKNOWN_SECTION(333),
      1,
      2,
      3,
      4,  // 4 byte section
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionSkipped) {
  static const byte data[] = {
      UNKNOWN_SECTION(1),
      0,  // one byte section
      SECTION(Global, 6),
      1,
      kLocalI32,                  // memory type
      0,                          // exported
      WASM_INIT_EXPR_I32V_1(33),  // init
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1, result.val->globals.size());
  EXPECT_EQ(0, result.val->functions.size());
  EXPECT_EQ(0, result.val->data_segments.size());

  const WasmGlobal* global = &result.val->globals.back();

  EXPECT_EQ(kAstI32, global->type);
  EXPECT_EQ(0, global->offset);

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ImportTable_empty) {
  static const byte data[] = {SECTION(Type, 1), 0, SECTION(Import, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs1) {
  static const byte data[] = {SECTION(Import, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs2) {
  static const byte data[] = {
      SECTION(Import, 6),  1,    // sig table
      NAME_LENGTH(1),      'm',  // module name
      NAME_LENGTH(1),      'f',  // function name
      kExternalFunction,         // import kind
      IMPORT_SIG_INDEX(0),       // sig index
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_sig) {
  static const byte data[] = {
      SECTION(Type, 1),    0,    // --
      SECTION(Import, 6),  1,    // --
      NAME_LENGTH(1),      'm',  // module name
      NAME_LENGTH(1),      'f',  // function name
      kExternalFunction,         // import kind
      IMPORT_SIG_INDEX(0),       // sig index
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_one_sig) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Import, 7),
      1,  // --
      NAME_LENGTH(1),
      'm',  // module name
      NAME_LENGTH(1),
      'f',                  // function name
      kExternalFunction,    // import kind
      IMPORT_SIG_INDEX(0),  // sig index
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_module) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,  // --
      SECTION(Import, 7),            // --
      1,                             // --
      NO_NAME,                       // module name
      NAME_LENGTH(1),                // --
      'f',                           // function name
      kExternalFunction,             // import kind
      IMPORT_SIG_INDEX(0),           // sig index
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_off_end) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Import, 6),
      1,
      NAME_LENGTH(1),
      'm',  // module name
      NAME_LENGTH(1),
      'f',                  // function name
      kExternalFunction,    // import kind
      IMPORT_SIG_INDEX(0),  // sig index
  };

  EXPECT_OFF_END_FAILURE(data, 16, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, ExportTable_empty1) {
  static const byte data[] = {                               // signatures
                              SIGNATURES_SECTION_VOID_VOID,  // --
                              ONE_EMPTY_FUNCTION, SECTION(Export, 1),  // --
                              0,                                       // --
                              ONE_EMPTY_BODY};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1, result.val->functions.size());
  EXPECT_EQ(0, result.val->export_table.size());

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ExportTable_empty2) {
  static const byte data[] = {
      SECTION(Type, 1), 0, SECTION(Export, 1), 0  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTable_NoFunctions2) {
  static const byte data[] = {SECTION(Export, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableOne) {
  static const byte data[] = {// signatures
                              SIGNATURES_SECTION_VOID_VOID,
                              ONE_EMPTY_FUNCTION,
                              SECTION(Export, 4),
                              1,                  // exports
                              NO_NAME,            // --
                              kExternalFunction,  // --
                              FUNC_INDEX(0),      // --
                              ONE_EMPTY_BODY};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1, result.val->functions.size());
  EXPECT_EQ(1, result.val->export_table.size());

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ExportNameWithInvalidStringLength) {
  static const byte data[] = {// signatures
                              SIGNATURES_SECTION_VOID_VOID,
                              ONE_EMPTY_FUNCTION,
                              SECTION(Export, 12),
                              1,                  // exports
                              NAME_LENGTH(84),    // invalid string length
                              'e',                // --
                              kExternalFunction,  // --
                              FUNC_INDEX(0)};

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableTwo) {
  static const byte data[] = {// signatures
                              SIGNATURES_SECTION_VOID_VOID,
                              ONE_EMPTY_FUNCTION,
                              SECTION(Export, 14),
                              2,  // exports
                              NAME_LENGTH(4),
                              'n',
                              'a',
                              'm',
                              'e',  // --
                              kExternalFunction,
                              FUNC_INDEX(0),  // --
                              NAME_LENGTH(3),
                              'n',
                              'o',
                              'm',                // --
                              kExternalFunction,  // --
                              FUNC_INDEX(0),      // --
                              ONE_EMPTY_BODY};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1, result.val->functions.size());
  EXPECT_EQ(2, result.val->export_table.size());

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ExportTableThree) {
  static const byte data[] = {// signatures
                              SIGNATURES_SECTION_VOID_VOID,
                              THREE_EMPTY_FUNCTIONS,
                              SECTION(Export, 13),
                              3,  // exports
                              NAME_LENGTH(1),
                              'a',  // --
                              kExternalFunction,
                              FUNC_INDEX(0),  // --
                              NAME_LENGTH(1),
                              'b',  // --
                              kExternalFunction,
                              FUNC_INDEX(1),  // --
                              NAME_LENGTH(1),
                              'c',  // --
                              kExternalFunction,
                              FUNC_INDEX(2),  // --
                              THREE_EMPTY_BODIES};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(3, result.val->functions.size());
  EXPECT_EQ(3, result.val->export_table.size());

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ExportTableThreeOne) {
  for (int i = 0; i < 6; i++) {
    const byte data[] = {// signatures
                         SIGNATURES_SECTION_VOID_VOID,
                         THREE_EMPTY_FUNCTIONS,
                         SECTION(Export, 6),
                         1,  // exports
                         NAME_LENGTH(2),
                         'e',
                         'x',  // --
                         kExternalFunction,
                         FUNC_INDEX(i),  // --
                         THREE_EMPTY_BODIES};

    if (i < 3) {
      EXPECT_VERIFIES(data);
    } else {
      EXPECT_FAILURE(data);
    }
  }
}

TEST_F(WasmModuleVerifyTest, ExportTableOne_off_end) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      ONE_EMPTY_FUNCTION,
      SECTION(Export, 1 + 6),
      1,        // exports
      NO_NAME,  // --
      kExternalFunction,
      FUNC_INDEX(0),  // --
  };

  for (int length = 33; length < sizeof(data); length++) {
    ModuleResult result = DecodeModule(data, data + length);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, FunctionSignatures_empty) {
  static const byte data[] = {
      SECTION(Type, 1), 0,     // --
      SECTION(Function, 1), 0  // --
  };                           // --
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionSignatures_one) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
      FUNCTION_SIGNATURES_SECTION(1, 0)      // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Regression_648070) {
  static const byte data[] = {
      SECTION(Type, 1), 0,   // --
      SECTION(Function, 5),  // --
      U32V_5(3500228624)     // function count = 3500228624
  };                         // --
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_empty) {
  static const byte data[] = {
      EMPTY_SIGNATURES_SECTION,           // --
      EMPTY_FUNCTION_SIGNATURES_SECTION,  // --
      EMPTY_FUNCTION_BODIES_SECTION       // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_one_empty) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),                // --
      FUNCTION_SIGNATURES_SECTION(1, 0),                   // --
      SECTION(Code, 1 + SIZEOF_EMPTY_BODY), 1, EMPTY_BODY  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_one_nop) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),            // --
      FUNCTION_SIGNATURES_SECTION(1, 0),               // --
      SECTION(Code, 1 + SIZEOF_NOP_BODY), 1, NOP_BODY  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_count_mismatch1) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),     // --
      FUNCTION_SIGNATURES_SECTION(2, 0, 0),     // --
      SECTION(Code, 1 + SIZEOF_EMPTY_BODY), 1,  // --
      EMPTY_BODY                                // --
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_count_mismatch2) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),    // --
      FUNCTION_SIGNATURES_SECTION(1, 0),       // --
      SECTION(Code, 1 + 2 * SIZEOF_NOP_BODY),  // --
      ENTRY_COUNT(2),                          // --
      NOP_BODY,                                // --
      NOP_BODY                                 // --
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, Names_empty) {
  static const byte data[] = {
      EMPTY_SIGNATURES_SECTION, EMPTY_FUNCTION_SIGNATURES_SECTION,
      EMPTY_FUNCTION_BODIES_SECTION, EMPTY_NAMES_SECTION};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Names_one_empty) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
      FUNCTION_SIGNATURES_SECTION(1, 0),     // --
      SECTION(Code, 1 + SIZEOF_EMPTY_BODY),
      ENTRY_COUNT(1),
      EMPTY_BODY,  // --
      SECTION_NAMES(1 + 5),
      ENTRY_COUNT(1),
      FOO_STRING,
      NO_LOCAL_NAMES  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Names_two_empty) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),      // --
      FUNCTION_SIGNATURES_SECTION(2, 0, 0),      // --
      SECTION(Code, 1 + 2 * SIZEOF_EMPTY_BODY),  // --
      ENTRY_COUNT(2),
      EMPTY_BODY,
      EMPTY_BODY,  // --
      SECTION_NAMES(1 + 10),
      ENTRY_COUNT(2),  // --
      FOO_STRING,
      NO_LOCAL_NAMES,  // --
      FOO_STRING,
      NO_LOCAL_NAMES,  // --
  };
  EXPECT_VERIFIES(data);
}

#define EXPECT_INIT_EXPR(Type, type, value, ...)                 \
  {                                                              \
    static const byte data[] = {__VA_ARGS__, kExprEnd};          \
    WasmInitExpr expr =                                          \
        DecodeWasmInitExprForTesting(data, data + sizeof(data)); \
    EXPECT_EQ(WasmInitExpr::k##Type##Const, expr.kind);          \
    EXPECT_EQ(value, expr.val.type##_const);                     \
  }

TEST_F(WasmModuleVerifyTest, InitExpr_i32) {
  EXPECT_INIT_EXPR(I32, i32, 33, WASM_I32V_1(33));
  EXPECT_INIT_EXPR(I32, i32, -21, WASM_I32V_1(-21));
  EXPECT_INIT_EXPR(I32, i32, 437, WASM_I32V_2(437));
  EXPECT_INIT_EXPR(I32, i32, 77777, WASM_I32V_3(77777));
}

TEST_F(WasmModuleVerifyTest, InitExpr_f32) {
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(13.1), WASM_F32(13.1));
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(-21.1), WASM_F32(-21.1));
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(437.2), WASM_F32(437.2));
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(77777.3), WASM_F32(77777.3));
}

TEST_F(WasmModuleVerifyTest, InitExpr_i64) {
  EXPECT_INIT_EXPR(I64, i64, 33, WASM_I64V_1(33));
  EXPECT_INIT_EXPR(I64, i64, -21, WASM_I64V_2(-21));
  EXPECT_INIT_EXPR(I64, i64, 437, WASM_I64V_5(437));
  EXPECT_INIT_EXPR(I64, i64, 77777, WASM_I64V_7(77777));
}

TEST_F(WasmModuleVerifyTest, InitExpr_f64) {
  EXPECT_INIT_EXPR(F64, f64, 83.22, WASM_F64(83.22));
  EXPECT_INIT_EXPR(F64, f64, -771.3, WASM_F64(-771.3));
  EXPECT_INIT_EXPR(F64, f64, 43703.0, WASM_F64(43703.0));
  EXPECT_INIT_EXPR(F64, f64, 77999.1, WASM_F64(77999.1));
}

#define EXPECT_INIT_EXPR_FAIL(...)                               \
  {                                                              \
    static const byte data[] = {__VA_ARGS__, kExprEnd};          \
    WasmInitExpr expr =                                          \
        DecodeWasmInitExprForTesting(data, data + sizeof(data)); \
    EXPECT_EQ(WasmInitExpr::kNone, expr.kind);                   \
  }

TEST_F(WasmModuleVerifyTest, InitExpr_illegal) {
  EXPECT_INIT_EXPR_FAIL(WASM_I32V_1(0), WASM_I32V_1(0));
  EXPECT_INIT_EXPR_FAIL(WASM_GET_LOCAL(0));
  EXPECT_INIT_EXPR_FAIL(WASM_SET_LOCAL(0, WASM_I32V_1(0)));
  EXPECT_INIT_EXPR_FAIL(WASM_I32_ADD(WASM_I32V_1(0), WASM_I32V_1(0)));
  EXPECT_INIT_EXPR_FAIL(WASM_IF_ELSE(WASM_ZERO, WASM_ZERO, WASM_ZERO));
}

TEST_F(WasmModuleVerifyTest, InitExpr_global) {
  static const byte data[] = {WASM_INIT_EXPR_GLOBAL(37)};
  WasmInitExpr expr = DecodeWasmInitExprForTesting(data, data + sizeof(data));
  EXPECT_EQ(WasmInitExpr::kGlobalIndex, expr.kind);
  EXPECT_EQ(37, expr.val.global_index);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

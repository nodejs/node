// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

#define EMPTY_FUNCTION(sig_index) 0, SIG_INDEX(sig_index), U16_LE(0)
#define EMPTY_FUNCTION_SIZE ((size_t)5)
#define EMPTY_BODY 0
#define EMPTY_BODY_SIZE ((size_t)1)
#define NOP_BODY 2, 0, kExprNop
#define NOP_BODY_SIZE ((size_t)3)
#define VOID_VOID_SIG 0, kLocalVoid
#define VOID_VOID_SIG_SIZE ((size_t)2)
#define INT_INT_SIG 1, kLocalI32, kLocalI32
#define INT_INT_SIG_SIZE ((size_t)3)

#define SECTION(NAME, EXTRA_SIZE) \
  U32V_1(WASM_SECTION_##NAME##_SIZE + (EXTRA_SIZE)), WASM_SECTION_##NAME

#define EXPECT_VERIFIES(data)                                         \
  do {                                                                \
    ModuleResult result = DecodeModule(data, data + arraysize(data)); \
    EXPECT_TRUE(result.ok());                                         \
    if (result.val) delete result.val;                                \
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

class WasmModuleVerifyTest : public TestWithZone {
 public:
  ModuleResult DecodeModule(const byte* module_start, const byte* module_end) {
    // Add the WASM magic and version number automatically.
    size_t size = static_cast<size_t>(module_end - module_start);
    byte header[] = {WASM_MODULE_HEADER};
    size_t total = sizeof(header) + size;
    auto temp = new byte[total];
    memcpy(temp, header, sizeof(header));
    memcpy(temp + sizeof(header), module_start, size);
    ModuleResult result = DecodeWasmModule(nullptr, zone(), temp, temp + total,
                                           false, kWasmOrigin);
    delete[] temp;
    return result;
  }
  ModuleResult DecodeModuleNoHeader(const byte* module_start,
                                    const byte* module_end) {
    return DecodeWasmModule(nullptr, zone(), module_start, module_end, false,
                            kWasmOrigin);
  }
};

TEST_F(WasmModuleVerifyTest, WrongMagic) {
  for (uint32_t x = 1; x; x <<= 1) {
    const byte data[] = {U32_LE(kWasmMagic ^ x), U32_LE(kWasmVersion),
                         SECTION(END, 0)};
    ModuleResult result = DecodeModuleNoHeader(data, data + sizeof(data));
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, WrongVersion) {
  for (uint32_t x = 1; x; x <<= 1) {
    const byte data[] = {U32_LE(kWasmMagic), U32_LE(kWasmVersion ^ x),
                         SECTION(END, 0)};
    ModuleResult result = DecodeModuleNoHeader(data, data + sizeof(data));
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, DecodeEmpty) {
  static const byte data[] = {SECTION(END, 0)};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, OneGlobal) {
  static const byte data[] = {
      SECTION(GLOBALS, 7),  // --
      1,
      NAME_LENGTH(1),
      'g',      // name
      kMemI32,  // memory type
      0,        // exported
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(1, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(0, result.val->data_segments.size());

    WasmGlobal* global = &result.val->globals.back();

    EXPECT_EQ(1, global->name_length);
    EXPECT_EQ(MachineType::Int32(), global->type);
    EXPECT_EQ(0, global->offset);
    EXPECT_FALSE(global->exported);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}


TEST_F(WasmModuleVerifyTest, ZeroGlobals) {
  static const byte data[] = {
      SECTION(GLOBALS, 1),  // --
      0,                    // declare 0 globals
  };
  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
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
      NO_NAME,  // name length
      kMemI32,  // memory type
      0,        // exported
  };

  for (uint32_t i = 0; i < 1000000; i = i * 13 + 1) {
    std::vector<byte> buffer;
    size_t size =
        WASM_SECTION_GLOBALS_SIZE + SizeOfVarInt(i) + i * sizeof(data);
    const byte globals[] = {U32V_5(size), WASM_SECTION_GLOBALS};
    for (size_t g = 0; g != sizeof(globals); ++g) {
      buffer.push_back(globals[g]);
    }
    AppendUint32v(buffer, i);  // Number of globals.
    for (uint32_t j = 0; j < i; j++) {
      buffer.insert(buffer.end(), data, data + sizeof(data));
    }

    ModuleResult result = DecodeModule(&buffer[0], &buffer[0] + buffer.size());
    EXPECT_TRUE(result.ok());
    if (result.val) delete result.val;
  }
}

TEST_F(WasmModuleVerifyTest, GlobalWithInvalidNameOffset) {
  static const byte data[] = {
      SECTION(GLOBALS, 7),
      1,        // declare one global
      NO_NAME,  // name offset
      33,       // memory type
      0,        // exported
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, GlobalWithInvalidMemoryType) {
  static const byte data[] = {
      SECTION(GLOBALS, 7),
      1,        // declare one global
      NO_NAME,  // name offset
      33,       // memory type
      0,        // exported
  };

  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, TwoGlobals) {
  static const byte data[] = {
      SECTION(GLOBALS, 13),
      2,
      NO_NAME,  // #0: name length
      kMemF32,  // memory type
      0,        // exported
      NO_NAME,  // #1: name length
      kMemF64,  // memory type
      1,        // exported
  };

  {
    // Should decode to exactly two globals.
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(2, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(0, result.val->data_segments.size());

    WasmGlobal* g0 = &result.val->globals[0];
    WasmGlobal* g1 = &result.val->globals[1];

    EXPECT_EQ(0, g0->name_length);
    EXPECT_EQ(MachineType::Float32(), g0->type);
    EXPECT_EQ(0, g0->offset);
    EXPECT_FALSE(g0->exported);

    EXPECT_EQ(0, g1->name_length);
    EXPECT_EQ(MachineType::Float64(), g1->type);
    EXPECT_EQ(0, g1->offset);
    EXPECT_TRUE(g1->exported);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}


TEST_F(WasmModuleVerifyTest, OneSignature) {
  {
    static const byte data[] = {SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE), 1,
                                VOID_VOID_SIG};
    EXPECT_VERIFIES(data);
  }

  {
    static const byte data[] = {SECTION(SIGNATURES, 1 + INT_INT_SIG_SIZE), 1,
                                INT_INT_SIG};
    EXPECT_VERIFIES(data);
  }
}


TEST_F(WasmModuleVerifyTest, MultipleSignatures) {
  static const byte data[] = {
      SECTION(SIGNATURES, 10),
      3,
      0,
      kLocalVoid,  // void -> void
      1,
      kLocalI32,
      kLocalF32,  // f32 -> i32
      2,
      kLocalI32,
      kLocalF64,
      kLocalF64,  // (f64,f64) -> i32
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
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


TEST_F(WasmModuleVerifyTest, FunctionWithoutSig) {
  static const byte data[] = {
      SECTION(FUNCTIONS, 25), 1,
      // func#0 ------------------------------------------------------
      SIG_INDEX(0),  // signature index
      NO_NAME,       // name length
      U32_LE(0),     // code start offset
      U32_LE(0),     // code end offset
      U16_LE(899),   // local int32 count
      U16_LE(799),   // local int64 count
      U16_LE(699),   // local float32 count
      U16_LE(599),   // local float64 count
      0,             // exported
      1              // external
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_FALSE(result.ok());
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, OneEmptyVoidVoidFunction) {
  const int kCodeStartOffset = 51;
  const int kCodeEndOffset = kCodeStartOffset + 1;

  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE), 1,
      // sig#0 -------------------------------------------------------
      VOID_VOID_SIG,
      // func#0 ------------------------------------------------------
      SECTION(FUNCTIONS, 19), 1,
      kDeclFunctionLocals | kDeclFunctionExport | kDeclFunctionName,
      SIG_INDEX(0),              // signature index
      NAME_LENGTH(2), 'h', 'i',  // name
      U16_LE(1466),              // local int32 count
      U16_LE(1355),              // local int64 count
      U16_LE(1244),              // local float32 count
      U16_LE(1133),              // local float64 count
      1, 0,                      // size
      kExprNop,
  };

  {
    // Should decode to exactly one function.
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(0, result.val->globals.size());
    EXPECT_EQ(1, result.val->signatures.size());
    EXPECT_EQ(1, result.val->functions.size());
    EXPECT_EQ(0, result.val->data_segments.size());
    EXPECT_EQ(0, result.val->function_table.size());

    WasmFunction* function = &result.val->functions.back();

    EXPECT_EQ(39, function->name_offset);
    EXPECT_EQ(2, function->name_length);
    EXPECT_EQ(kCodeStartOffset, function->code_start_offset);
    EXPECT_EQ(kCodeEndOffset, function->code_end_offset);

    EXPECT_EQ(1466, function->local_i32_count);
    EXPECT_EQ(1355, function->local_i64_count);
    EXPECT_EQ(1244, function->local_f32_count);
    EXPECT_EQ(1133, function->local_f64_count);

    EXPECT_TRUE(function->exported);
    EXPECT_FALSE(function->external);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 16, sizeof(data));
}


TEST_F(WasmModuleVerifyTest, OneFunctionImported) {
  static const byte data[] = {
      SECTION(SIGNATURES, VOID_VOID_SIG_SIZE), 1,
      // sig#0 -------------------------------------------------------
      VOID_VOID_SIG, SECTION(FUNCTIONS, 6), 1,
      // func#0 ------------------------------------------------------
      kDeclFunctionImport,  // no name, no locals, imported
      SIG_INDEX(0),
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1, result.val->functions.size());
  WasmFunction* function = &result.val->functions.back();

  EXPECT_EQ(0, function->name_length);
  EXPECT_EQ(0, function->code_start_offset);
  EXPECT_EQ(0, function->code_end_offset);

  EXPECT_EQ(0, function->local_i32_count);
  EXPECT_EQ(0, function->local_i64_count);
  EXPECT_EQ(0, function->local_f32_count);
  EXPECT_EQ(0, function->local_f64_count);

  EXPECT_FALSE(function->exported);
  EXPECT_TRUE(function->external);

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, OneFunctionWithNopBody) {
  static const byte kCodeStartOffset = 40;
  static const byte kCodeEndOffset = kCodeStartOffset + 1;

  static const byte data[] = {
      SECTION(SIGNATURES, 3), 1,
      // sig#0 -------------------------------------------------------
      0, 0,  // void -> void
      SECTION(FUNCTIONS, 7), 1,
      // func#0 ------------------------------------------------------
      0,        // no name, no locals
      0, 0,     // signature index
      1, 0,     // body size
      kExprNop  // body
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1, result.val->functions.size());
  WasmFunction* function = &result.val->functions.back();

  EXPECT_EQ(0, function->name_length);
  EXPECT_EQ(kCodeStartOffset, function->code_start_offset);
  EXPECT_EQ(kCodeEndOffset, function->code_end_offset);

  EXPECT_EQ(0, function->local_i32_count);
  EXPECT_EQ(0, function->local_i64_count);
  EXPECT_EQ(0, function->local_f32_count);
  EXPECT_EQ(0, function->local_f64_count);

  EXPECT_FALSE(function->exported);
  EXPECT_FALSE(function->external);

  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, OneFunctionWithNopBody_WithLocals) {
  static const byte kCodeStartOffset = 48;
  static const byte kCodeEndOffset = kCodeStartOffset + 1;

  static const byte data[] = {
      SECTION(SIGNATURES, 3), 1,
      // sig#0 -------------------------------------------------------
      0, 0,  // void -> void
      SECTION(FUNCTIONS, 15), 1,
      // func#0 ------------------------------------------------------
      kDeclFunctionLocals, 0, 0,  // signature index
      1, 2,                       // local int32 count
      3, 4,                       // local int64 count
      5, 6,                       // local float32 count
      7, 8,                       // local float64 count
      1, 0,                       // body size
      kExprNop                    // body
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1, result.val->functions.size());
  WasmFunction* function = &result.val->functions.back();

  EXPECT_EQ(0, function->name_length);
  EXPECT_EQ(kCodeStartOffset, function->code_start_offset);
  EXPECT_EQ(kCodeEndOffset, function->code_end_offset);

  EXPECT_EQ(513, function->local_i32_count);
  EXPECT_EQ(1027, function->local_i64_count);
  EXPECT_EQ(1541, function->local_f32_count);
  EXPECT_EQ(2055, function->local_f64_count);

  EXPECT_FALSE(function->exported);
  EXPECT_FALSE(function->external);

  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, OneGlobalOneFunctionWithNopBodyOneDataSegment) {
  static const byte kCodeStartOffset = 75;
  static const byte kCodeEndOffset = kCodeStartOffset + 3;
  static const byte kDataSegmentSourceOffset = kCodeEndOffset + 20;

  static const byte data[] = {
      SECTION(MEMORY, 3), 28, 28, 1,
      // global#0 --------------------------------------------------
      SECTION(GLOBALS, 7), 1,
      0,       // name length
      kMemU8,  // memory type
      0,       // exported
      // sig#0 -----------------------------------------------------
      SECTION(SIGNATURES, 3), 1, 0, 0,  // void -> void
      // func#0 ----------------------------------------------------
      SECTION(FUNCTIONS, 20), 1, kDeclFunctionLocals | kDeclFunctionName, 0,
      0,            // signature index
      2, 'h', 'i',  // name
      1, 2,         // local int32 count
      3, 4,         // local int64 count
      5, 6,         // local float32 count
      7, 8,         // local float64 count
      3, 0,         // body size
      kExprNop,     // func#0 body
      kExprNop,     // func#0 body
      kExprNop,     // func#0 body
      // segment#0 -------------------------------------------------
      SECTION(DATA_SEGMENTS, 14), 1,
      U32V_3(0x8b3ae),  // dest addr
      U32V_1(5),        // source size
      0, 1, 2, 3, 4,    // data bytes
      // rest ------------------------------------------------------
      SECTION(END, 0),
  };

  {
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(1, result.val->globals.size());
    EXPECT_EQ(1, result.val->functions.size());
    EXPECT_EQ(1, result.val->data_segments.size());

    WasmGlobal* global = &result.val->globals.back();

    EXPECT_EQ(0, global->name_length);
    EXPECT_EQ(MachineType::Uint8(), global->type);
    EXPECT_EQ(0, global->offset);
    EXPECT_FALSE(global->exported);

    WasmFunction* function = &result.val->functions.back();

    EXPECT_EQ(63, function->name_offset);
    EXPECT_EQ(2, function->name_length);
    EXPECT_EQ(kCodeStartOffset, function->code_start_offset);
    EXPECT_EQ(kCodeEndOffset, function->code_end_offset);

    EXPECT_FALSE(function->exported);
    EXPECT_FALSE(function->external);

    WasmDataSegment* segment = &result.val->data_segments.back();

    EXPECT_EQ(0x8b3ae, segment->dest_addr);
    EXPECT_EQ(kDataSegmentSourceOffset, segment->source_offset);
    EXPECT_EQ(5, segment->source_size);
    EXPECT_TRUE(segment->init);

    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, OneDataSegment) {
  const byte kDataSegmentSourceOffset = 39;
  const byte data[] = {
      SECTION(MEMORY, 3),
      28,
      28,
      1,
      SECTION(DATA_SEGMENTS, 8),
      1,
      U32V_3(0x9bbaa),  // dest addr
      U32V_1(3),        // source size
      'a',
      'b',
      'c'  // data bytes
  };

  {
    EXPECT_VERIFIES(data);
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(0, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(1, result.val->data_segments.size());

    WasmDataSegment* segment = &result.val->data_segments.back();

    EXPECT_EQ(0x9bbaa, segment->dest_addr);
    EXPECT_EQ(kDataSegmentSourceOffset, segment->source_offset);
    EXPECT_EQ(3, segment->source_size);
    EXPECT_TRUE(segment->init);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 13, sizeof(data));
}


TEST_F(WasmModuleVerifyTest, TwoDataSegments) {
  const byte kDataSegment0SourceOffset = 39;
  const byte kDataSegment1SourceOffset = 39 + 8;

  const byte data[] = {
      SECTION(MEMORY, 3),
      28,
      28,
      1,
      SECTION(DATA_SEGMENTS, 31),
      2,                // segment count
      U32V_3(0x7ffee),  // #0: dest addr
      U32V_1(4),        // source size
      1,
      2,
      3,
      4,                // data bytes
      U32V_3(0x6ddcc),  // #1: dest addr
      U32V_1(10),       // source size
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
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(0, result.val->globals.size());
    EXPECT_EQ(0, result.val->functions.size());
    EXPECT_EQ(2, result.val->data_segments.size());

    WasmDataSegment* s0 = &result.val->data_segments[0];
    WasmDataSegment* s1 = &result.val->data_segments[1];

    EXPECT_EQ(0x7ffee, s0->dest_addr);
    EXPECT_EQ(kDataSegment0SourceOffset, s0->source_offset);
    EXPECT_EQ(4, s0->source_size);
    EXPECT_TRUE(s0->init);

    EXPECT_EQ(0x6ddcc, s1->dest_addr);
    EXPECT_EQ(kDataSegment1SourceOffset, s1->source_offset);
    EXPECT_EQ(10, s1->source_size);
    EXPECT_TRUE(s1->init);

    if (result.val) delete result.val;
  }

  EXPECT_OFF_END_FAILURE(data, 13, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, DataSegmentWithInvalidDest) {
  const int source_size = 3;

  for (byte mem_pages = 1; mem_pages < 16; mem_pages++) {
    int mem_size = mem_pages * 0x10000;  // 64k pages.

    for (int dest_addr = mem_size - source_size;
         dest_addr < mem_size + source_size; dest_addr++) {
      byte data[] = {SECTION(MEMORY, 3),
                     mem_pages,
                     mem_pages,
                     1,
                     SECTION(DATA_SEGMENTS, 14),
                     1,
                     U32V_3(dest_addr),
                     U32V_1(source_size),
                     'a',
                     'b',
                     'c'};

      if (dest_addr <= (mem_size - source_size)) {
        EXPECT_VERIFIES(data);
      } else {
        EXPECT_FAILURE(data);
      }
    }
  }
}


// To make below tests for indirect calls much shorter.
#define FUNCTION(sig_index, external) kDeclFunctionImport, SIG_INDEX(sig_index)

TEST_F(WasmModuleVerifyTest, OneIndirectFunction) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SECTION(SIGNATURES, 3), 1, 0, 0,  // void -> void
      // func#0 ------------------------------------------------------
      SECTION(FUNCTIONS, 4), 1, FUNCTION(0, 0),
      // indirect table ----------------------------------------------
      SECTION(FUNCTION_TABLE, 2), 1, U32V_1(0)};

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.ok()) {
    EXPECT_EQ(1, result.val->signatures.size());
    EXPECT_EQ(1, result.val->functions.size());
    EXPECT_EQ(1, result.val->function_table.size());
    EXPECT_EQ(0, result.val->function_table[0]);
  }
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, MultipleIndirectFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SECTION(SIGNATURES, 5), 2, 0, 0,  // void -> void
      0, kLocalI32,                     // void -> i32
      // func#0 ------------------------------------------------------
      SECTION(FUNCTIONS, 13), 4, FUNCTION(0, 1),  // --
      FUNCTION(1, 1),                             // --
      FUNCTION(0, 1),                             // --
      FUNCTION(1, 1),                             // --
      // indirect table ----------------------------------------------
      SECTION(FUNCTION_TABLE, 9), 8,
      U32V_1(0),  // --
      U32V_1(1),  // --
      U32V_1(2),  // --
      U32V_1(3),  // --
      U32V_1(0),  // --
      U32V_1(1),  // --
      U32V_1(2),  // --
      U32V_1(3),  // --
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.ok()) {
    EXPECT_EQ(2, result.val->signatures.size());
    EXPECT_EQ(4, result.val->functions.size());
    EXPECT_EQ(8, result.val->function_table.size());
    for (int i = 0; i < 8; i++) {
      EXPECT_EQ(i & 3, result.val->function_table[i]);
    }
  }
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, IndirectFunctionNoFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SECTION(SIGNATURES, 3), 1, 0, 0,  // void -> void
      // indirect table ----------------------------------------------
      SECTION(FUNCTION_TABLE, 3), 1, 0, 0,
  };

  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, IndirectFunctionInvalidIndex) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SECTION(SIGNATURES, 3), 1, 0, 0,  // void -> void
      // functions ---------------------------------------------------
      SECTION(FUNCTIONS, 4), 1, FUNCTION(0, 1),
      // indirect table ----------------------------------------------
      SECTION(FUNCTION_TABLE, 3), 1, 1, 0,
  };

  EXPECT_FAILURE(data);
}


class WasmSignatureDecodeTest : public TestWithZone {};


TEST_F(WasmSignatureDecodeTest, Ok_v_v) {
  static const byte data[] = {0, 0};
  base::AccountingAllocator allocator;
  Zone zone(&allocator);
  FunctionSig* sig =
      DecodeWasmSignatureForTesting(&zone, data, data + arraysize(data));

  EXPECT_TRUE(sig != nullptr);
  EXPECT_EQ(0, sig->parameter_count());
  EXPECT_EQ(0, sig->return_count());
}


TEST_F(WasmSignatureDecodeTest, Ok_t_v) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalTypePair ret_type = kLocalTypes[i];
    const byte data[] = {0, ret_type.code};
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + arraysize(data));

    EXPECT_TRUE(sig != nullptr);
    EXPECT_EQ(0, sig->parameter_count());
    EXPECT_EQ(1, sig->return_count());
    EXPECT_EQ(ret_type.type, sig->GetReturn());
  }
}


TEST_F(WasmSignatureDecodeTest, Ok_v_t) {
  for (size_t i = 0; i < arraysize(kLocalTypes); i++) {
    LocalTypePair param_type = kLocalTypes[i];
    const byte data[] = {1, 0, param_type.code};
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + arraysize(data));

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
      const byte data[] = {1,                 // param count
                           ret_type.code,     // ret
                           param_type.code};  // param
      FunctionSig* sig =
          DecodeWasmSignatureForTesting(zone(), data, data + arraysize(data));

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
      const byte data[] = {2,              // param count
                           kLocalI32,      // ret
                           p0_type.code,   // p0
                           p1_type.code};  // p1
      FunctionSig* sig =
          DecodeWasmSignatureForTesting(zone(), data, data + arraysize(data));

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
  for (int i = 1; i < 3; i++) {
    byte data[] = {2, kLocalI32, kLocalI32, kLocalI32};
    data[i] = kInvalidType;
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + arraysize(data));
    EXPECT_EQ(nullptr, sig);
  }
}


TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type) {
  static const int kParamCount = 3;
  for (int i = 0; i < kParamCount; i++) {
    byte data[] = {kParamCount, kLocalI32, kLocalI32, kLocalI32, kLocalI32};
    data[i + 2] = kLocalVoid;
    FunctionSig* sig =
        DecodeWasmSignatureForTesting(zone(), data, data + arraysize(data));
    EXPECT_EQ(nullptr, sig);
  }
}


class WasmFunctionVerifyTest : public TestWithZone {};


TEST_F(WasmFunctionVerifyTest, Ok_v_v_empty) {
  static const byte data[] = {
      0,       kLocalVoid,  // signature
      4,                    // locals
      3,       kLocalI32,   // --
      4,       kLocalI64,   // --
      5,       kLocalF32,   // --
      6,       kLocalF64,   // --
      kExprNop              // body
  };

  FunctionResult result = DecodeWasmFunction(nullptr, zone(), nullptr, data,
                                             data + arraysize(data));
  EXPECT_TRUE(result.ok());

  if (result.val && result.ok()) {
    WasmFunction* function = result.val;
    EXPECT_EQ(0, function->sig->parameter_count());
    EXPECT_EQ(0, function->sig->return_count());
    EXPECT_EQ(0, function->name_offset);
    EXPECT_EQ(2, function->code_start_offset);
    EXPECT_EQ(arraysize(data), function->code_end_offset);
    // TODO(titzer): verify encoding of local declarations
    EXPECT_FALSE(function->external);
    EXPECT_FALSE(function->exported);
  }

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, SectionWithoutNameLength) {
  const byte data[] = {1};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, TheLoneliestOfValidModulesTheTrulyEmptyOne) {
  const byte data[] = {
      1,  // Section size.
      0,  // Empty section name.
          // No section name, no content, nothing but sadness.
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, OnlyUnknownSectionEmpty) {
  const byte data[] = {
      5,                      // Section size.
      4, 'l', 'u', 'l', 'z',  // unknown section.
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, OnlyUnknownSectionNonEmpty) {
  const byte data[] = {
      10,                     // Section size.
      4, 'l', 'u', 'l', 'z',  // unknown section.
      // Section content:
      0xff, 0xff, 0xff, 0xff, 0xff,
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, SignatureFollowedByEmptyUnknownSection) {
  const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE), 1, VOID_VOID_SIG,
      // -----------------------------------------------------------
      5,                      // Section size.
      4, 'l', 'u', 'l', 'z',  // unknown section.
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, SignatureFollowedByUnknownSection) {
  const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE), 1, VOID_VOID_SIG,
      // -----------------------------------------------------------
      10,                     // Section size.
      4, 'l', 'u', 'l', 'z',  // unknown section.
      0xff, 0xff, 0xff, 0xff, 0xff,
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, SignatureFollowedByUnknownSectionWithLongLEB) {
  const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE), 1, VOID_VOID_SIG,
      // -----------------------------------------------------------
      0x85, 0x80, 0x80, 0x80, 0x00,  // Section size: 1 but in a 5-byte LEB.
      4, 'l', 'u', 'l', 'z',         // unknown section.
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionOverflow) {
  static const byte data[] = {
      13,                                // Section size.
      1,                                 // Section name length.
      '\0',                              // Section name.
      1,    2, 3, 4, 5, 6, 7, 8, 9, 10,  // 10 byte section
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionUnderflow) {
  static const byte data[] = {
      0xff, 0xff, 0xff, 0xff, 0x0f,  // Section size LEB128 0xffffffff
      1,    '\0',                    // Section name and name length.
      1,    2,    3,    4,           // 4 byte section
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionLoop) {
  // Would infinite loop decoding if wrapping and allowed.
  static const byte data[] = {
      0xfa, 0xff, 0xff, 0xff, 0x0f,  // Section size LEB128 0xfffffffa
      1,    '\0',                    // Section name and name length.
      1,    2,    3,    4,           // 4 byte section
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, UnknownSectionSkipped) {
  static const byte data[] = {
      3,  // Section size.
      1,
      '\0',  // Section name: LEB128 1, string '\0'
      0,     // one byte section
      SECTION(GLOBALS, 7),
      1,
      0,        // name length
      kMemI32,  // memory type
      0,        // exported
  };
  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());

  EXPECT_EQ(1, result.val->globals.size());
  EXPECT_EQ(0, result.val->functions.size());
  EXPECT_EQ(0, result.val->data_segments.size());

  WasmGlobal* global = &result.val->globals.back();

  EXPECT_EQ(0, global->name_length);
  EXPECT_EQ(MachineType::Int32(), global->type);
  EXPECT_EQ(0, global->offset);
  EXPECT_FALSE(global->exported);

  if (result.val) delete result.val;
}

TEST_F(WasmModuleVerifyTest, ImportTable_empty) {
  static const byte data[] = {SECTION(SIGNATURES, 1), 0,
                              SECTION(IMPORT_TABLE, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs) {
  static const byte data[] = {SECTION(IMPORT_TABLE, 1), 0};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_sig) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1), 0,   SECTION(IMPORT_TABLE, 6), 1,
      IMPORT_SIG_INDEX(0),          // sig index
      NAME_LENGTH(1),         'm',  // module name
      NAME_LENGTH(1),         'f',  // function name
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_one_sig) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,
      VOID_VOID_SIG,
      SECTION(IMPORT_TABLE, 6),
      1,                    // --
      IMPORT_SIG_INDEX(0),  // sig index
      NAME_LENGTH(1),
      'm',  // module name
      NAME_LENGTH(1),
      'f',  // function name
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_module) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,
      VOID_VOID_SIG,
      SECTION(IMPORT_TABLE, 6),
      1,                    // --
      IMPORT_SIG_INDEX(0),  // sig index
      NO_NAME,              // module name
      NAME_LENGTH(1),
      'f'  // function name
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_off_end) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,
      VOID_VOID_SIG,
      SECTION(IMPORT_TABLE, 6),
      1,
      IMPORT_SIG_INDEX(0),  // sig index
      NAME_LENGTH(1),
      'm',  // module name
      NAME_LENGTH(1),
      'f',  // function name
  };

  EXPECT_OFF_END_FAILURE(data, 16, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, ExportTable_empty1) {
  static const byte data[] = {SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
                              1,
                              VOID_VOID_SIG,
                              SECTION(FUNCTIONS, 1 + EMPTY_FUNCTION_SIZE),
                              1,
                              EMPTY_FUNCTION(0),
                              SECTION(EXPORT_TABLE, 1),
                              0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTable_empty2) {
  static const byte data[] = {SECTION(SIGNATURES, 1),   0,
                              SECTION(FUNCTIONS, 1),    0,
                              SECTION(EXPORT_TABLE, 1), 0};
  // TODO(titzer): current behavior treats empty functions section as missing.
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ExportTable_NoFunctions1) {
  static const byte data[] = {SECTION(SIGNATURES, 1), 0,
                              SECTION(EXPORT_TABLE, 1), 0};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ExportTable_NoFunctions2) {
  static const byte data[] = {SECTION(EXPORT_TABLE, 1), 0};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableOne) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,              // sigs
      VOID_VOID_SIG,  // --
      SECTION(FUNCTIONS, 1 + EMPTY_FUNCTION_SIZE),
      1,                  // functions
      EMPTY_FUNCTION(0),  // --
      SECTION(EXPORT_TABLE, 7),
      1,              // exports
      FUNC_INDEX(0),  // --
      NO_NAME         // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableTwo) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,              // sigs
      VOID_VOID_SIG,  // --
      SECTION(FUNCTIONS, 1 + EMPTY_FUNCTION_SIZE),
      1,                  // functions
      EMPTY_FUNCTION(0),  // --
      SECTION(EXPORT_TABLE, 12),
      2,              // exports
      FUNC_INDEX(0),  // --
      NAME_LENGTH(4),
      'n',
      'a',
      'm',
      'e',            // --
      FUNC_INDEX(0),  // --
      NAME_LENGTH(3),
      'n',
      'o',
      'm'  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableThree) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,              // sigs
      VOID_VOID_SIG,  // --
      SECTION(FUNCTIONS, 1 + 3 * EMPTY_FUNCTION_SIZE),
      3,                  // functions
      EMPTY_FUNCTION(0),  // --
      EMPTY_FUNCTION(0),  // --
      EMPTY_FUNCTION(0),  // --
      SECTION(EXPORT_TABLE, 10),
      3,              // exports
      FUNC_INDEX(0),  // --
      NAME_LENGTH(1),
      'a',            // --
      FUNC_INDEX(1),  // --
      NAME_LENGTH(1),
      'b',            // --
      FUNC_INDEX(2),  // --
      NAME_LENGTH(1),
      'c'  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableThreeOne) {
  for (int i = 0; i < 6; i++) {
    const byte data[] = {
        SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
        1,              // sigs
        VOID_VOID_SIG,  // --
        SECTION(FUNCTIONS, 1 + 3 * EMPTY_FUNCTION_SIZE),
        3,                  // functions
        EMPTY_FUNCTION(0),  // --
        EMPTY_FUNCTION(0),  // --
        EMPTY_FUNCTION(0),  // --
        SECTION(EXPORT_TABLE, 5),
        1,              // exports
        FUNC_INDEX(i),  // --
        NAME_LENGTH(2),
        'e',
        'x',  // --
    };

    if (i < 3) {
      EXPECT_VERIFIES(data);
    } else {
      EXPECT_FAILURE(data);
    }
  }
}

TEST_F(WasmModuleVerifyTest, ExportTableOne_off_end) {
  static const byte data[] = {
      SECTION(SIGNATURES, 1 + VOID_VOID_SIG_SIZE),
      1,              // sigs
      VOID_VOID_SIG,  // --
      SECTION(FUNCTIONS, 1 + EMPTY_FUNCTION_SIZE),
      1,                  // functions
      EMPTY_FUNCTION(0),  // --
      SECTION(EXPORT_TABLE, 1 + 6),
      1,              // exports
      FUNC_INDEX(0),  // --
      NO_NAME         // --
  };

  for (int length = 33; length < sizeof(data); length++) {
    ModuleResult result = DecodeModule(data, data + length);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

#define SIGNATURES_SECTION(count, ...) \
  SECTION(SIGNATURES, 1 + 3 * (count)), U32V_1(count), __VA_ARGS__
#define FUNCTION_SIGNATURES_SECTION(count, ...) \
  SECTION(FUNCTION_SIGNATURES, 1 + (count)), U32V_1(count), __VA_ARGS__

#define FOO_STRING 3, 'f', 'o', 'o'
#define NO_LOCAL_NAMES 0

#define EMPTY_SIGNATURES_SECTION SECTION(SIGNATURES, 1), 0
#define EMPTY_FUNCTION_SIGNATURES_SECTION SECTION(FUNCTION_SIGNATURES, 1), 0
#define EMPTY_FUNCTION_BODIES_SECTION SECTION(FUNCTION_BODIES, 1), 0
#define EMPTY_NAMES_SECTION SECTION(NAMES, 1), 0

TEST_F(WasmModuleVerifyTest, FunctionSignatures_empty) {
  static const byte data[] = {SECTION(SIGNATURES, 1), 0,
                              SECTION(FUNCTION_SIGNATURES, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionSignatures_one) {
  static const byte data[] = {SIGNATURES_SECTION(1, VOID_VOID_SIG),
                              FUNCTION_SIGNATURES_SECTION(1, 0)};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_empty) {
  static const byte data[] = {EMPTY_SIGNATURES_SECTION,
                              EMPTY_FUNCTION_SIGNATURES_SECTION,
                              EMPTY_FUNCTION_BODIES_SECTION};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_one_empty) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, VOID_VOID_SIG), FUNCTION_SIGNATURES_SECTION(1, 0),
      SECTION(FUNCTION_BODIES, 1 + EMPTY_BODY_SIZE), 1, EMPTY_BODY};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_one_nop) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, VOID_VOID_SIG), FUNCTION_SIGNATURES_SECTION(1, 0),
      SECTION(FUNCTION_BODIES, 1 + NOP_BODY_SIZE), 1, NOP_BODY};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_count_mismatch1) {
  static const byte data[] = {SIGNATURES_SECTION(1, VOID_VOID_SIG),
                              FUNCTION_SIGNATURES_SECTION(2, 0, 0),
                              SECTION(FUNCTION_BODIES, 1 + EMPTY_BODY_SIZE), 1,
                              EMPTY_BODY};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_count_mismatch2) {
  static const byte data[] = {SIGNATURES_SECTION(1, VOID_VOID_SIG),
                              FUNCTION_SIGNATURES_SECTION(1, 0),
                              SECTION(FUNCTION_BODIES, 1 + 2 * NOP_BODY_SIZE),
                              2,
                              NOP_BODY,
                              NOP_BODY};
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
      SIGNATURES_SECTION(1, VOID_VOID_SIG),  // --
      FUNCTION_SIGNATURES_SECTION(1, 0),     // --
      SECTION(FUNCTION_BODIES, 1 + EMPTY_BODY_SIZE),
      1,
      EMPTY_BODY,  // --
      SECTION(NAMES, 1 + 5),
      1,
      FOO_STRING,
      NO_LOCAL_NAMES  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Names_two_empty) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, VOID_VOID_SIG),               // --
      FUNCTION_SIGNATURES_SECTION(2, 0, 0),               // --
      SECTION(FUNCTION_BODIES, 1 + 2 * EMPTY_BODY_SIZE),  // --
      2,
      EMPTY_BODY,
      EMPTY_BODY,  // --
      SECTION(NAMES, 1 + 10),
      2,  // --
      FOO_STRING,
      NO_LOCAL_NAMES,  // --
      FOO_STRING,
      NO_LOCAL_NAMES,  // --
  };
  EXPECT_VERIFIES(data);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

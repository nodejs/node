// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmModuleVerifyTest : public TestWithZone {
 public:
  ModuleResult DecodeModule(const byte* module_start, const byte* module_end) {
    return DecodeWasmModule(nullptr, zone(), module_start, module_end, false,
                            false);
  }
};


#define EXPECT_VERIFIES(data)                                         \
  do {                                                                \
    ModuleResult result = DecodeModule(data, data + arraysize(data)); \
    EXPECT_TRUE(result.ok());                                         \
    if (result.val) delete result.val;                                \
  } while (false)


#define EXPECT_FAILURE(data)                                          \
  do {                                                                \
    ModuleResult result = DecodeModule(data, data + arraysize(data)); \
    EXPECT_FALSE(result.ok());                                        \
    if (result.val) delete result.val;                                \
  } while (false)


struct LocalTypePair {
  uint8_t code;
  LocalType type;
} kLocalTypes[] = {{kLocalI32, kAstI32},
                   {kLocalI64, kAstI64},
                   {kLocalF32, kAstF32},
                   {kLocalF64, kAstF64}};


// TODO(titzer): use these macros everywhere below.
#define U32_LE(v)                                    \
  static_cast<byte>(v), static_cast<byte>((v) >> 8), \
      static_cast<byte>((v) >> 16), static_cast<byte>((v) >> 24)


#define U16_LE(v) static_cast<byte>(v), static_cast<byte>((v) >> 8)


TEST_F(WasmModuleVerifyTest, DecodeEmpty) {
  static const byte data[1]{kDeclEnd};
  {
    ModuleResult result = DecodeModule(data, data);
    EXPECT_TRUE(result.ok());
    if (result.val) delete result.val;
  }
  {
    ModuleResult result = DecodeModule(data, data + 1);
    EXPECT_TRUE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, OneGlobal) {
  static const byte data[] = {
      kDeclGlobals,
      1,
      0,
      0,
      0,
      0,        // name offset
      kMemI32,  // memory type
      0,        // exported
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(1, result.val->globals->size());
    EXPECT_EQ(0, result.val->functions->size());
    EXPECT_EQ(0, result.val->data_segments->size());

    WasmGlobal* global = &result.val->globals->back();

    EXPECT_EQ(0, global->name_offset);
    EXPECT_EQ(MachineType::Int32(), global->type);
    EXPECT_EQ(0, global->offset);
    EXPECT_FALSE(global->exported);

    if (result.val) delete result.val;
  }

  for (size_t size = 1; size < arraysize(data); size++) {
    // Should fall off end of module bytes.
    ModuleResult result = DecodeModule(data, data + size);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, ZeroGlobals) {
  static const byte data[] = {
      kDeclGlobals, 0,  // declare 0 globals
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
      0,       0, 0, 0,  // name offset
      kMemI32,           // memory type
      0,                 // exported
  };
  for (uint32_t i = 0; i < 1000000; i = i * 7 + 1) {
    std::vector<byte> buffer;
    buffer.push_back(kDeclGlobals);
    AppendUint32v(buffer, i);
    for (uint32_t j = 0; j < i; j++) {
      buffer.insert(buffer.end(), data, data + arraysize(data));
    }

    ModuleResult result = DecodeModule(&buffer[0], &buffer[0] + buffer.size());
    EXPECT_TRUE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, GlobalWithInvalidNameOffset) {
  static const byte data[] = {
      kDeclGlobals,
      1,  // declare one global
      0,
      3,
      0,
      0,        // name offset
      kMemI32,  // memory type
      0,        // exported
  };

  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, GlobalWithInvalidMemoryType) {
  static const byte data[] = {
      kDeclGlobals,
      1,  // declare one global
      0,
      0,
      0,
      0,   // name offset
      33,  // memory type
      0,   // exported
  };

  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, TwoGlobals) {
  static const byte data[] = {
      kDeclGlobals,
      2,
      0,
      0,
      0,
      0,        // #0: name offset
      kMemF32,  // memory type
      0,        // exported
      0,
      0,
      0,
      0,        // #1: name offset
      kMemF64,  // memory type
      1,        // exported
  };

  {
    // Should decode to exactly two globals.
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(2, result.val->globals->size());
    EXPECT_EQ(0, result.val->functions->size());
    EXPECT_EQ(0, result.val->data_segments->size());

    WasmGlobal* g0 = &result.val->globals->at(0);
    WasmGlobal* g1 = &result.val->globals->at(1);

    EXPECT_EQ(0, g0->name_offset);
    EXPECT_EQ(MachineType::Float32(), g0->type);
    EXPECT_EQ(0, g0->offset);
    EXPECT_FALSE(g0->exported);

    EXPECT_EQ(0, g1->name_offset);
    EXPECT_EQ(MachineType::Float64(), g1->type);
    EXPECT_EQ(0, g1->offset);
    EXPECT_TRUE(g1->exported);

    if (result.val) delete result.val;
  }

  for (size_t size = 1; size < arraysize(data); size++) {
    // Should fall off end of module bytes.
    ModuleResult result = DecodeModule(data, data + size);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, OneSignature) {
  static const byte data[] = {
      kDeclSignatures, 1, 0, kLocalVoid  // void -> void
  };
  EXPECT_VERIFIES(data);
}


TEST_F(WasmModuleVerifyTest, MultipleSignatures) {
  static const byte data[] = {
      kDeclSignatures,
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
  EXPECT_EQ(3, result.val->signatures->size());
  if (result.val->signatures->size() == 3) {
    EXPECT_EQ(0, result.val->signatures->at(0)->return_count());
    EXPECT_EQ(1, result.val->signatures->at(1)->return_count());
    EXPECT_EQ(1, result.val->signatures->at(2)->return_count());

    EXPECT_EQ(0, result.val->signatures->at(0)->parameter_count());
    EXPECT_EQ(1, result.val->signatures->at(1)->parameter_count());
    EXPECT_EQ(2, result.val->signatures->at(2)->parameter_count());
  }
  if (result.val) delete result.val;

  for (size_t size = 1; size < arraysize(data); size++) {
    ModuleResult result = DecodeModule(data, data + size);
    // Should fall off the end of module bytes.
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, FunctionWithoutSig) {
  static const byte data[] = {
      kDeclFunctions, 1,
      // func#0 ------------------------------------------------------
      0, 0,        // signature index
      0, 0, 0, 0,  // name offset
      0, 0, 0, 0,  // code start offset
      0, 0, 0, 0,  // code end offset
      1, 2,        // local int32 count
      3, 4,        // local int64 count
      5, 6,        // local float32 count
      7, 8,        // local float64 count
      0,           // exported
      1            // external
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_FALSE(result.ok());
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, OneEmptyVoidVoidFunction) {
  const int kCodeStartOffset = 23;
  const int kCodeEndOffset = kCodeStartOffset + 1;

  static const byte data[] = {
      kDeclSignatures, 1,
      // sig#0 -------------------------------------------------------
      0, 0,  // void -> void
      // func#0 ------------------------------------------------------
      kDeclFunctions, 1,
      kDeclFunctionLocals | kDeclFunctionExport | kDeclFunctionName, 0,
      0,           // signature index
      9, 0, 0, 0,  // name offset
      11, 2,       // local int32 count
      13, 4,       // local int64 count
      15, 6,       // local float32 count
      17, 8,       // local float64 count
      1, 0,        // size
      kExprNop,
  };

  {
    // Should decode to exactly one function.
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(0, result.val->globals->size());
    EXPECT_EQ(1, result.val->signatures->size());
    EXPECT_EQ(1, result.val->functions->size());
    EXPECT_EQ(0, result.val->data_segments->size());
    EXPECT_EQ(0, result.val->function_table->size());

    WasmFunction* function = &result.val->functions->back();

    EXPECT_EQ(9, function->name_offset);
    EXPECT_EQ(kCodeStartOffset, function->code_start_offset);
    EXPECT_EQ(kCodeEndOffset, function->code_end_offset);

    EXPECT_EQ(523, function->local_i32_count);
    EXPECT_EQ(1037, function->local_i64_count);
    EXPECT_EQ(1551, function->local_f32_count);
    EXPECT_EQ(2065, function->local_f64_count);

    EXPECT_TRUE(function->exported);
    EXPECT_FALSE(function->external);

    if (result.val) delete result.val;
  }

  for (size_t size = 5; size < arraysize(data); size++) {
    // Should fall off end of module bytes.
    ModuleResult result = DecodeModule(data, data + size);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, OneFunctionImported) {
  static const byte data[] = {
      kDeclSignatures, 1,
      // sig#0 -------------------------------------------------------
      0, 0,  // void -> void
      kDeclFunctions, 1,
      // func#0 ------------------------------------------------------
      kDeclFunctionImport,  // no name, no locals, imported
      0, 0,                 // signature index
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1, result.val->functions->size());
  WasmFunction* function = &result.val->functions->back();

  EXPECT_EQ(0, function->name_offset);
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
  static const byte kCodeStartOffset = 11;
  static const byte kCodeEndOffset = kCodeStartOffset + 1;

  static const byte data[] = {
      kDeclSignatures, 1,
      // sig#0 -------------------------------------------------------
      0, 0,  // void -> void
      kDeclFunctions, 1,
      // func#0 ------------------------------------------------------
      0,        // no name, no locals
      0, 0,     // signature index
      1, 0,     // body size
      kExprNop  // body
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1, result.val->functions->size());
  WasmFunction* function = &result.val->functions->back();

  EXPECT_EQ(0, function->name_offset);
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
  static const byte kCodeStartOffset = 19;
  static const byte kCodeEndOffset = kCodeStartOffset + 1;

  static const byte data[] = {
      kDeclSignatures, 1,
      // sig#0 -------------------------------------------------------
      0, 0,  // void -> void
      kDeclFunctions, 1,
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
  EXPECT_EQ(1, result.val->functions->size());
  WasmFunction* function = &result.val->functions->back();

  EXPECT_EQ(0, function->name_offset);
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
  static const byte kDeclMemorySize = 4;
  static const byte kCodeStartOffset =
      2 + kDeclMemorySize + kDeclGlobalSize + 4 + 2 + 17;
  static const byte kCodeEndOffset = kCodeStartOffset + 3;

  static const byte data[] = {
      kDeclMemory, 28, 28, 1,
      // global#0 --------------------------------------------------
      kDeclGlobals, 1, 0, 0, 0, 0,  // name offset
      kMemU8,                       // memory type
      0,                            // exported
      // sig#0 -----------------------------------------------------
      kDeclSignatures, 1, 0, 0,  // void -> void
      // func#0 ----------------------------------------------------
      kDeclFunctions, 1, kDeclFunctionLocals | kDeclFunctionName, 0,
      0,           // signature index
      9, 0, 0, 0,  // name offset
      1, 2,        // local int32 count
      3, 4,        // local int64 count
      5, 6,        // local float32 count
      7, 8,        // local float64 count
      3, 0,        // body size
      kExprNop,    // func#0 body
      kExprNop,    // func#0 body
      kExprNop,    // func#0 body
      // segment#0 -------------------------------------------------
      kDeclDataSegments, 1, 0xae, 0xb3, 0x08, 0,  // dest addr
      15, 0, 0, 0,                                // source offset
      5, 0, 0, 0,                                 // source size
      1,                                          // init
      // rest ------------------------------------------------------
      kDeclEnd,
  };

  {
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(1, result.val->globals->size());
    EXPECT_EQ(1, result.val->functions->size());
    EXPECT_EQ(1, result.val->data_segments->size());

    WasmGlobal* global = &result.val->globals->back();

    EXPECT_EQ(0, global->name_offset);
    EXPECT_EQ(MachineType::Uint8(), global->type);
    EXPECT_EQ(0, global->offset);
    EXPECT_FALSE(global->exported);

    WasmFunction* function = &result.val->functions->back();

    EXPECT_EQ(9, function->name_offset);
    EXPECT_EQ(kCodeStartOffset, function->code_start_offset);
    EXPECT_EQ(kCodeEndOffset, function->code_end_offset);

    EXPECT_FALSE(function->exported);
    EXPECT_FALSE(function->external);

    WasmDataSegment* segment = &result.val->data_segments->back();

    EXPECT_EQ(0x8b3ae, segment->dest_addr);
    EXPECT_EQ(15, segment->source_offset);
    EXPECT_EQ(5, segment->source_size);
    EXPECT_TRUE(segment->init);

    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, OneDataSegment) {
  const byte data[] = {
      kDeclMemory, 28, 28, 1, kDeclDataSegments, 1, 0xaa, 0xbb, 0x09,
      0,  // dest addr
      11,          0,  0,
      0,  // source offset
      3,           0,  0,
      0,  // source size
      1,  // init
  };

  {
    EXPECT_VERIFIES(data);
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(0, result.val->globals->size());
    EXPECT_EQ(0, result.val->functions->size());
    EXPECT_EQ(1, result.val->data_segments->size());

    WasmDataSegment* segment = &result.val->data_segments->back();

    EXPECT_EQ(0x9bbaa, segment->dest_addr);
    EXPECT_EQ(11, segment->source_offset);
    EXPECT_EQ(3, segment->source_size);
    EXPECT_TRUE(segment->init);

    if (result.val) delete result.val;
  }

  for (size_t size = 5; size < arraysize(data); size++) {
    // Should fall off end of module bytes.
    ModuleResult result = DecodeModule(data, data + size);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, TwoDataSegments) {
  const byte data[] = {
      kDeclMemory, 28,   28,   1, kDeclDataSegments, 2, 0xee, 0xff, 0x07,
      0,  // dest addr
      9,           0,    0,
      0,  // #0: source offset
      4,           0,    0,
      0,  // source size
      0,  // init
      0xcc,        0xdd, 0x06,
      0,  // #1: dest addr
      6,           0,    0,
      0,  // source offset
      10,          0,    0,
      0,  // source size
      1,  // init
  };

  {
    ModuleResult result = DecodeModule(data, data + arraysize(data));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(0, result.val->globals->size());
    EXPECT_EQ(0, result.val->functions->size());
    EXPECT_EQ(2, result.val->data_segments->size());

    WasmDataSegment* s0 = &result.val->data_segments->at(0);
    WasmDataSegment* s1 = &result.val->data_segments->at(1);

    EXPECT_EQ(0x7ffee, s0->dest_addr);
    EXPECT_EQ(9, s0->source_offset);
    EXPECT_EQ(4, s0->source_size);
    EXPECT_FALSE(s0->init);

    EXPECT_EQ(0x6ddcc, s1->dest_addr);
    EXPECT_EQ(6, s1->source_offset);
    EXPECT_EQ(10, s1->source_size);
    EXPECT_TRUE(s1->init);

    if (result.val) delete result.val;
  }

  for (size_t size = 5; size < arraysize(data); size++) {
    // Should fall off end of module bytes.
    ModuleResult result = DecodeModule(data, data + size);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}


TEST_F(WasmModuleVerifyTest, DataSegmentWithInvalidSource) {
  const int dest_addr = 0x100;
  const byte mem_size_log2 = 15;
  const int kDataSize = 19;

  for (int source_offset = 0; source_offset < 5 + kDataSize; source_offset++) {
    for (int source_size = -1; source_size < 5 + kDataSize; source_size += 3) {
      byte data[] = {
          kDeclMemory,
          mem_size_log2,
          mem_size_log2,
          1,
          kDeclDataSegments,
          1,
          U32_LE(dest_addr),
          U32_LE(source_offset),
          U32_LE(source_size),
          1,  // init
      };

      STATIC_ASSERT(kDataSize == arraysize(data));

      if (source_offset < kDataSize && source_size >= 0 &&
          (source_offset + source_size) <= kDataSize) {
        EXPECT_VERIFIES(data);
      } else {
        EXPECT_FAILURE(data);
      }
    }
  }
}


TEST_F(WasmModuleVerifyTest, DataSegmentWithInvalidDest) {
  const int source_size = 3;
  const int source_offset = 11;

  for (byte mem_size_log2 = 12; mem_size_log2 < 20; mem_size_log2++) {
    int mem_size = 1 << mem_size_log2;

    for (int dest_addr = mem_size - source_size;
         dest_addr < mem_size + source_size; dest_addr++) {
      byte data[] = {
          kDeclMemory,
          mem_size_log2,
          mem_size_log2,
          1,
          kDeclDataSegments,
          1,
          U32_LE(dest_addr),
          U32_LE(source_offset),
          U32_LE(source_size),
          1,  // init
      };

      if (dest_addr <= (mem_size - source_size)) {
        EXPECT_VERIFIES(data);
      } else {
        EXPECT_FAILURE(data);
      }
    }
  }
}


// To make below tests for indirect calls much shorter.
#define FUNCTION(sig_index, external)                \
  kDeclFunctionImport, static_cast<byte>(sig_index), \
      static_cast<byte>(sig_index >> 8)


TEST_F(WasmModuleVerifyTest, OneIndirectFunction) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      kDeclSignatures, 1, 0, 0,  // void -> void
      // func#0 ------------------------------------------------------
      kDeclFunctions, 1, FUNCTION(0, 0),
      // indirect table ----------------------------------------------
      kDeclFunctionTable, 1, 0, 0};

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.ok()) {
    EXPECT_EQ(1, result.val->signatures->size());
    EXPECT_EQ(1, result.val->functions->size());
    EXPECT_EQ(1, result.val->function_table->size());
    EXPECT_EQ(0, result.val->function_table->at(0));
  }
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, MultipleIndirectFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      kDeclSignatures, 2, 0, 0,  // void -> void
      0, kLocalI32,              // void -> i32
      // func#0 ------------------------------------------------------
      kDeclFunctions, 4, FUNCTION(0, 1), FUNCTION(1, 1), FUNCTION(0, 1),
      FUNCTION(1, 1),
      // indirect table ----------------------------------------------
      kDeclFunctionTable, 8, 0, 0, 1, 0, 2, 0, 3, 0, 0, 0, 1, 0, 2, 0, 3, 0,
  };

  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.ok()) {
    EXPECT_EQ(2, result.val->signatures->size());
    EXPECT_EQ(4, result.val->functions->size());
    EXPECT_EQ(8, result.val->function_table->size());
    for (int i = 0; i < 8; i++) {
      EXPECT_EQ(i & 3, result.val->function_table->at(i));
    }
  }
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, IndirectFunctionNoFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      kDeclSignatures, 1, 0, 0,  // void -> void
      // indirect table ----------------------------------------------
      kDeclFunctionTable, 1, 0, 0,
  };

  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, IndirectFunctionInvalidIndex) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      kDeclSignatures, 1, 0, 0,  // void -> void
      // functions ---------------------------------------------------
      kDeclFunctions, 1, FUNCTION(0, 1),
      // indirect table ----------------------------------------------
      kDeclFunctionTable, 1, 1, 0,
  };

  EXPECT_FAILURE(data);
}


class WasmSignatureDecodeTest : public TestWithZone {};


TEST_F(WasmSignatureDecodeTest, Ok_v_v) {
  static const byte data[] = {0, 0};
  Zone zone;
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
      3,       0,           // local int32 count
      4,       0,           // local int64 count
      5,       0,           // local float32 count
      6,       0,           // local float64 count
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
    EXPECT_EQ(arraysize(data) - 1, function->code_start_offset);
    EXPECT_EQ(arraysize(data), function->code_end_offset);
    EXPECT_EQ(3, function->local_i32_count);
    EXPECT_EQ(4, function->local_i64_count);
    EXPECT_EQ(5, function->local_f32_count);
    EXPECT_EQ(6, function->local_f64_count);
    EXPECT_FALSE(function->external);
    EXPECT_FALSE(function->exported);
  }

  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, WLLSectionNoLen) {
  const byte data[] = {
      kDeclWLL,  // section without length.
  };
  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, WLLSectionEmpty) {
  static const byte data[] = {
      kDeclWLL, 0,  // empty section
  };
  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, WLLSectionOne) {
  static const byte data[] = {
      kDeclWLL,
      1,  // LEB128 1
      0,  // one byte section
  };
  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, WLLSectionTen) {
  static const byte data[] = {
      kDeclWLL,
      10,                                    // LEB128 10
      1,        2, 3, 4, 5, 6, 7, 8, 9, 10,  // 10 byte section
  };
  ModuleResult result = DecodeModule(data, data + arraysize(data));
  EXPECT_TRUE(result.ok());
  if (result.val) delete result.val;
}


TEST_F(WasmModuleVerifyTest, WLLSectionOverflow) {
  static const byte data[] = {
      kDeclWLL,
      11,                                    // LEB128 11
      1,        2, 3, 4, 5, 6, 7, 8, 9, 10,  // 10 byte section
  };
  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, WLLSectionUnderflow) {
  static const byte data[] = {
      kDeclWLL, 0xff, 0xff, 0xff, 0xff, 0x0f,  // LEB128 0xffffffff
      1,        2,    3,    4,                 // 4 byte section
  };
  EXPECT_FAILURE(data);
}


TEST_F(WasmModuleVerifyTest, WLLSectionLoop) {
  // Would infinite loop decoding if wrapping and allowed.
  static const byte data[] = {
      kDeclWLL, 0xfa, 0xff, 0xff, 0xff, 0x0f,  // LEB128 0xfffffffa
      1,        2,    3,    4,                 // 4 byte section
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_empty) {
  static const byte data[] = {kDeclSignatures, 0, kDeclImportTable, 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs) {
  static const byte data[] = {kDeclImportTable, 0};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_sig) {
  static const byte data[] = {
      kDeclSignatures,
      0,
      kDeclImportTable,
      1,
      0,
      0,  // sig index
      1,
      0,
      0,
      0,  // module name
      1,
      0,
      0,
      0  // function name
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_one_sig) {
  static const byte data[] = {
      kDeclSignatures,
      1,
      0,
      static_cast<byte>(kAstStmt),
      kDeclImportTable,
      1,
      0,
      0,  // sig index
      1,
      0,
      0,
      0,  // module name
      1,
      0,
      0,
      0  // function name
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_off_end) {
  static const byte data[] = {
      kDeclSignatures,
      1,
      0,
      static_cast<byte>(kAstStmt),
      kDeclImportTable,
      1,
      0,
      0,  // sig index
      1,
      0,
      0,
      0,  // module name
      1,
      0,
      0,
      0  // function name
  };

  for (size_t length = 5; length < sizeof(data); length++) {
    ModuleResult result = DecodeModule(data, data + length);
    EXPECT_FALSE(result.ok());
    if (result.val) delete result.val;
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace module_decoder_unittest {

#define WASM_INIT_EXPR_I32V_1(val) WASM_I32V_1(val), kExprEnd
#define WASM_INIT_EXPR_I32V_2(val) WASM_I32V_2(val), kExprEnd
#define WASM_INIT_EXPR_I32V_3(val) WASM_I32V_3(val), kExprEnd
#define WASM_INIT_EXPR_I32V_4(val) WASM_I32V_4(val), kExprEnd
#define WASM_INIT_EXPR_I32V_5(val) WASM_I32V_5(val), kExprEnd
#define WASM_INIT_EXPR_F32(val) WASM_F32(val), kExprEnd
#define WASM_INIT_EXPR_I64(val) WASM_I64(val), kExprEnd
#define WASM_INIT_EXPR_F64(val) WASM_F64(val), kExprEnd
#define WASM_INIT_EXPR_ANYREF WASM_REF_NULL, kExprEnd
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
#define SECTION_NAMES(size) SECTION(Unknown, size + 5), 4, 'n', 'a', 'm', 'e'
#define SECTION_EXCEPTIONS(size) SECTION(Exception, size)
#define EMPTY_NAMES_SECTION SECTION_NAMES(1), 0

#define FAIL_IF_NO_EXPERIMENTAL_EH(data)                                 \
  do {                                                                   \
    ModuleResult result = DecodeModule((data), (data) + sizeof((data))); \
    EXPECT_FALSE(result.ok());                                           \
    EXPECT_EQ(0u, result.val->exceptions.size());                        \
  } while (false)

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
  } while (false)

#define EXPECT_FAILURE_LEN(data, length)                     \
  do {                                                       \
    ModuleResult result = DecodeModule(data, data + length); \
    EXPECT_FALSE(result.ok());                               \
  } while (false)

#define EXPECT_FAILURE(data) EXPECT_FAILURE_LEN(data, sizeof(data))

#define EXPECT_OFF_END_FAILURE(data, min, max)          \
  do {                                                  \
    for (size_t length = min; length < max; length++) { \
      EXPECT_FAILURE_LEN(data, length);                 \
    }                                                   \
  } while (false)

#define EXPECT_OK(result)     \
  do {                        \
    EXPECT_TRUE(result.ok()); \
    if (!result.ok()) return; \
  } while (false)

static size_t SizeOfVarInt(size_t value) {
  size_t size = 0;
  do {
    size++;
    value = value >> 7;
  } while (value > 0);
  return size;
}

struct ValueTypePair {
  uint8_t code;
  ValueType type;
} kValueTypes[] = {
    {kLocalI32, kWasmI32},          // --
    {kLocalI64, kWasmI64},          // --
    {kLocalF32, kWasmF32},          // --
    {kLocalF64, kWasmF64},          // --
    {kLocalAnyFunc, kWasmAnyFunc},  // --
    {kLocalAnyRef, kWasmAnyRef}     // --
};

class WasmModuleVerifyTest : public TestWithIsolateAndZone {
 public:
  WasmFeatures enabled_features_;

  ModuleResult DecodeModule(const byte* module_start, const byte* module_end) {
    // Add the wasm magic and version number automatically.
    size_t size = static_cast<size_t>(module_end - module_start);
    byte header[] = {WASM_MODULE_HEADER};
    size_t total = sizeof(header) + size;
    auto temp = new byte[total];
    memcpy(temp, header, sizeof(header));
    memcpy(temp + sizeof(header), module_start, size);
    ModuleResult result = DecodeWasmModule(
        enabled_features_, temp, temp + total, false, kWasmOrigin,
        isolate()->counters(), isolate()->allocator());
    delete[] temp;
    return result;
  }
  ModuleResult DecodeModuleNoHeader(const byte* module_start,
                                    const byte* module_end) {
    return DecodeWasmModule(enabled_features_, module_start, module_end, false,
                            kWasmOrigin, isolate()->counters(),
                            isolate()->allocator());
  }
};

namespace {
class EnableBoolScope {
 public:
  bool prev_;
  bool* ptr_;
  explicit EnableBoolScope(bool* ptr, bool val = true)
      : prev_(*ptr), ptr_(ptr) {
    *ptr = val;
  }
  ~EnableBoolScope() { *ptr_ = prev_; }
};

#define WASM_FEATURE_SCOPE(feat) \
  EnableBoolScope feat##_scope(&this->enabled_features_.feat)

#define WASM_FEATURE_SCOPE_VAL(feat, val) \
  EnableBoolScope feat##_scope(&this->enabled_features_.feat, val)
}  // namespace

TEST_F(WasmModuleVerifyTest, WrongMagic) {
  for (uint32_t x = 1; x; x <<= 1) {
    const byte data[] = {U32_LE(kWasmMagic ^ x), U32_LE(kWasmVersion)};
    ModuleResult result = DecodeModuleNoHeader(data, data + sizeof(data));
    EXPECT_FALSE(result.ok());
  }
}

TEST_F(WasmModuleVerifyTest, WrongVersion) {
  for (uint32_t x = 1; x; x <<= 1) {
    const byte data[] = {U32_LE(kWasmMagic), U32_LE(kWasmVersion ^ x)};
    ModuleResult result = DecodeModuleNoHeader(data, data + sizeof(data));
    EXPECT_FALSE(result.ok());
  }
}

TEST_F(WasmModuleVerifyTest, DecodeEmpty) {
  ModuleResult result = DecodeModule(nullptr, nullptr);
  EXPECT_TRUE(result.ok());
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
    EXPECT_EQ(1u, result.val->globals.size());
    EXPECT_EQ(0u, result.val->functions.size());
    EXPECT_EQ(0u, result.val->data_segments.size());

    const WasmGlobal* global = &result.val->globals.back();

    EXPECT_EQ(kWasmI32, global->type);
    EXPECT_EQ(0u, global->offset);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kI32Const, global->init.kind);
    EXPECT_EQ(13, global->init.val.i32_const);
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, AnyRefGlobal) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      SECTION(Global, 5),  // --
      1,
      kLocalAnyRef,          // local type
      0,                     // immutable
      WASM_INIT_EXPR_ANYREF  // init
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(1u, result.val->globals.size());
    EXPECT_EQ(0u, result.val->functions.size());
    EXPECT_EQ(0u, result.val->data_segments.size());

    const WasmGlobal* global = &result.val->globals.back();

    EXPECT_EQ(kWasmAnyRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kAnyRefConst, global->init.kind);
  }
}

TEST_F(WasmModuleVerifyTest, AnyRefGlobalWithGlobalInit) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      SECTION(Import, 8),  // section header
      1,                   // number of imports
      NAME_LENGTH(1),      // --
      'm',                 // module name
      NAME_LENGTH(1),      // --
      'f',                 // global name
      kExternalGlobal,     // import kind
      kLocalAnyRef,        // type
      0,                   // mutability
      SECTION(Global, 6),  // --
      1,
      kLocalAnyRef,  // local type
      0,             // immutable
      WASM_INIT_EXPR_GLOBAL(0),
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(2u, result.val->globals.size());
    EXPECT_EQ(0u, result.val->functions.size());
    EXPECT_EQ(0u, result.val->data_segments.size());

    const WasmGlobal* global = &result.val->globals.back();

    EXPECT_EQ(kWasmAnyRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kGlobalIndex, global->init.kind);
  }
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
}

TEST_F(WasmModuleVerifyTest, ZeroGlobals) {
  static const byte data[] = {
      SECTION(Global, 1),  // --
      0,                   // declare 0 globals
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
}

TEST_F(WasmModuleVerifyTest, ExportMutableGlobal) {
  WASM_FEATURE_SCOPE(mut_global);
  {
    static const byte data[] = {
        SECTION(Global, 6),  // --
        1,
        kLocalI32,                  // local type
        0,                          // immutable
        WASM_INIT_EXPR_I32V_1(13),  // init
        SECTION(Export, 8),         // --
        1,                          // Export count
        4,                          // name length
        'n',                        // --
        'a',                        // --
        'm',                        // --
        'e',                        // --
        kExternalGlobal,            // global
        0,                          // global index
    };
    EXPECT_VERIFIES(data);
  }
  {
    static const byte data[] = {
        SECTION(Global, 6),         // --
        1,                          // --
        kLocalI32,                  // local type
        1,                          // mutable
        WASM_INIT_EXPR_I32V_1(13),  // init
        SECTION(Export, 8),         // --
        1,                          // Export count
        4,                          // name length
        'n',                        // --
        'a',                        // --
        'm',                        // --
        'e',                        // --
        kExternalGlobal,            // global
        0,                          // global index
    };
    EXPECT_VERIFIES(data);
  }
}

static void AppendUint32v(std::vector<byte>& buffer, uint32_t val) {
  while (true) {
    uint32_t next = val >> 7;
    uint32_t out = val & 0x7F;
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

  for (uint32_t i = 0; i < kV8MaxWasmGlobals; i = i * 13 + 1) {
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
    EXPECT_EQ(2u, result.val->globals.size());
    EXPECT_EQ(0u, result.val->functions.size());
    EXPECT_EQ(0u, result.val->data_segments.size());

    const WasmGlobal* g0 = &result.val->globals[0];

    EXPECT_EQ(kWasmF32, g0->type);
    EXPECT_EQ(0u, g0->offset);
    EXPECT_FALSE(g0->mutability);
    EXPECT_EQ(WasmInitExpr::kF32Const, g0->init.kind);

    const WasmGlobal* g1 = &result.val->globals[1];

    EXPECT_EQ(kWasmF64, g1->type);
    EXPECT_EQ(8u, g1->offset);
    EXPECT_TRUE(g1->mutability);
    EXPECT_EQ(WasmInitExpr::kF64Const, g1->init.kind);
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, ZeroExceptions) {
  static const byte data[] = {
      SECTION_EXCEPTIONS(1), 0,
  };
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(0u, result.val->exceptions.size());
}

TEST_F(WasmModuleVerifyTest, OneI32Exception) {
  static const byte data[] = {SECTION_EXCEPTIONS(3), 1,
                              // except[0] (i32)
                              1, kLocalI32};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.val->exceptions.size());

  const WasmException& e0 = result.val->exceptions.front();
  EXPECT_EQ(1u, e0.sig->parameter_count());
  EXPECT_EQ(kWasmI32, e0.sig->GetParam(0));
}

TEST_F(WasmModuleVerifyTest, TwoExceptions) {
  static const byte data[] = {SECTION_EXCEPTIONS(6), 2,
                              // except[0] (f32, i64)
                              2, kLocalF32, kLocalI64,
                              // except[1] (i32)
                              1, kLocalI32};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(2u, result.val->exceptions.size());
  const WasmException& e0 = result.val->exceptions.front();
  EXPECT_EQ(2u, e0.sig->parameter_count());
  EXPECT_EQ(kWasmF32, e0.sig->GetParam(0));
  EXPECT_EQ(kWasmI64, e0.sig->GetParam(1));
  const WasmException& e1 = result.val->exceptions.back();
  EXPECT_EQ(kWasmI32, e1.sig->GetParam(0));
}

TEST_F(WasmModuleVerifyTest, Exception_invalid_type) {
  static const byte data[] = {SECTION_EXCEPTIONS(3), 1,
                              // except[0] (?)
                              1, 64};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  // Should fail decoding exception section.
  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_FALSE(result.ok());
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionCorrectPlacement) {
  static const byte data[] = {SECTION(Import, 1), 0, SECTION_EXCEPTIONS(1), 0,
                              SECTION(Export, 1), 0};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionAfterExport) {
  static const byte data[] = {SECTION(Export, 1), 0, SECTION_EXCEPTIONS(1), 0};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_FALSE(result.ok());
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionBeforeImport) {
  static const byte data[] = {SECTION_EXCEPTIONS(1), 0, SECTION(Import, 1), 0};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_FALSE(result.ok());
}

TEST_F(WasmModuleVerifyTest, ExceptionImport) {
  static const byte data[] = {SECTION(Import, 9),  // section header
                              1,                   // number of imports
                              NAME_LENGTH(1),      // --
                              'm',                 // module name
                              NAME_LENGTH(2),      // --
                              'e', 'x',            // exception name
                              kExternalException,  // import kind
                              // except[0] (i32)
                              1, kLocalI32};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.val->exceptions.size());
  EXPECT_EQ(1u, result.val->import_table.size());
}

TEST_F(WasmModuleVerifyTest, ExceptionExport) {
  static const byte data[] = {SECTION_EXCEPTIONS(3), 1,
                              // except[0] (i32)
                              1, kLocalI32, SECTION(Export, 4),
                              1,                   // exports
                              NO_NAME,             // --
                              kExternalException,  // --
                              EXCEPTION_INDEX(0)};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.val->exceptions.size());
  EXPECT_EQ(1u, result.val->export_table.size());
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
  EXPECT_EQ(3u, result.val->signatures.size());
  if (result.val->signatures.size() == 3) {
    EXPECT_EQ(0u, result.val->signatures[0]->return_count());
    EXPECT_EQ(1u, result.val->signatures[1]->return_count());
    EXPECT_EQ(1u, result.val->signatures[2]->return_count());

    EXPECT_EQ(0u, result.val->signatures[0]->parameter_count());
    EXPECT_EQ(1u, result.val->signatures[1]->parameter_count());
    EXPECT_EQ(2u, result.val->signatures[2]->parameter_count());
  }

  EXPECT_OFF_END_FAILURE(data, 1, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, DataSegmentWithImmutableImportedGlobal) {
  // Import 2 globals so that we can initialize data with a global index != 0.
  const byte data[] = {
      SECTION(Import, 15),  // section header
      2,                    // number of imports
      NAME_LENGTH(1),       // --
      'm',                  // module name
      NAME_LENGTH(1),       // --
      'f',                  // global name
      kExternalGlobal,      // import kind
      kLocalI32,            // type
      0,                    // mutability
      NAME_LENGTH(1),       // --
      'n',                  // module name
      NAME_LENGTH(1),       // --
      'g',                  // global name
      kExternalGlobal,      // import kind
      kLocalI32,            // type
      0,                    // mutability
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kHasMaximumFlag,
      28,
      28,
      SECTION(Data, 9),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_GLOBAL(1),  // dest addr
      U32V_1(3),                 // source size
      'a',
      'b',
      'c'  // data bytes
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  WasmInitExpr expr = result.val->data_segments.back().dest_addr;
  EXPECT_EQ(WasmInitExpr::kGlobalIndex, expr.kind);
  EXPECT_EQ(1u, expr.val.global_index);
}

TEST_F(WasmModuleVerifyTest, DataSegmentWithMutableImportedGlobal) {
  // Only an immutable imported global can be used as an init_expr.
  const byte data[] = {
      SECTION(Import, 8),  // section header
      1,                   // number of imports
      NAME_LENGTH(1),      // --
      'm',                 // module name
      NAME_LENGTH(1),      // --
      'f',                 // global name
      kExternalGlobal,     // import kind
      kLocalI32,           // type
      1,                   // mutability
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kHasMaximumFlag,
      28,
      28,
      SECTION(Data, 9),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_GLOBAL(0),  // dest addr
      U32V_1(3),                 // source size
      'a',
      'b',
      'c'  // data bytes
  };
  EXPECT_FAILURE(data);
}
TEST_F(WasmModuleVerifyTest, DataSegmentWithImmutableGlobal) {
  // Only an immutable imported global can be used as an init_expr.
  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kHasMaximumFlag,
      28,
      28,
      SECTION(Global, 8),  // --
      1,
      kLocalI32,                       // local type
      0,                               // immutable
      WASM_INIT_EXPR_I32V_3(0x9BBAA),  // init
      SECTION(Data, 9),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_GLOBAL(0),  // dest addr
      U32V_1(3),                 // source size
      'a',
      'b',
      'c'  // data bytes
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, OneDataSegment) {
  const byte kDataSegmentSourceOffset = 24;
  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kHasMaximumFlag,
      28,
      28,
      SECTION(Data, 11),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x9BBAA),  // dest addr
      U32V_1(3),                       // source size
      'a',
      'b',
      'c'  // data bytes
  };

  {
    EXPECT_VERIFIES(data);
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(0u, result.val->globals.size());
    EXPECT_EQ(0u, result.val->functions.size());
    EXPECT_EQ(1u, result.val->data_segments.size());

    const WasmDataSegment* segment = &result.val->data_segments.back();

    EXPECT_EQ(WasmInitExpr::kI32Const, segment->dest_addr.kind);
    EXPECT_EQ(0x9BBAA, segment->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegmentSourceOffset, segment->source.offset());
    EXPECT_EQ(3u, segment->source.length());
  }

  EXPECT_OFF_END_FAILURE(data, 14, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, TwoDataSegments) {
  const byte kDataSegment0SourceOffset = 24;
  const byte kDataSegment1SourceOffset = kDataSegment0SourceOffset + 11;

  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kHasMaximumFlag,
      28,
      28,
      SECTION(Data, 29),
      ENTRY_COUNT(2),  // segment count
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x7FFEE),  // #0: dest addr
      U32V_1(4),                       // source size
      1,
      2,
      3,
      4,  // data bytes
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x6DDCC),  // #1: dest addr
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
    EXPECT_EQ(0u, result.val->globals.size());
    EXPECT_EQ(0u, result.val->functions.size());
    EXPECT_EQ(2u, result.val->data_segments.size());

    const WasmDataSegment* s0 = &result.val->data_segments[0];
    const WasmDataSegment* s1 = &result.val->data_segments[1];

    EXPECT_EQ(WasmInitExpr::kI32Const, s0->dest_addr.kind);
    EXPECT_EQ(0x7FFEE, s0->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegment0SourceOffset, s0->source.offset());
    EXPECT_EQ(4u, s0->source.length());

    EXPECT_EQ(WasmInitExpr::kI32Const, s1->dest_addr.kind);
    EXPECT_EQ(0x6DDCC, s1->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegment1SourceOffset, s1->source.offset());
    EXPECT_EQ(10u, s1->source.length());
  }

  EXPECT_OFF_END_FAILURE(data, 14, sizeof(data));
}

TEST_F(WasmModuleVerifyTest, DataWithoutMemory) {
  const byte data[] = {
      SECTION(Data, 11),
      ENTRY_COUNT(1),
      LINEAR_MEMORY_INDEX_0,
      WASM_INIT_EXPR_I32V_3(0x9BBAA),  // dest addr
      U32V_1(3),                       // source size
      'a',
      'b',
      'c'  // data bytes
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, MaxMaximumMemorySize) {
  {
    const byte data[] = {
        SECTION(Memory, 6), ENTRY_COUNT(1), kHasMaximumFlag, 0, U32V_3(65536),
    };
    EXPECT_VERIFIES(data);
  }
  {
    const byte data[] = {
        SECTION(Memory, 6), ENTRY_COUNT(1), kHasMaximumFlag, 0, U32V_3(65537),
    };
    EXPECT_FAILURE(data);
  }
}

TEST_F(WasmModuleVerifyTest, DataSegment_wrong_init_type) {
  const byte data[] = {
      SECTION(Memory, 4),
      ENTRY_COUNT(1),
      kHasMaximumFlag,
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

TEST_F(WasmModuleVerifyTest, DataSegmentEndOverflow) {
  const byte data[] = {
      SECTION(Memory, 4),  // memory section
      ENTRY_COUNT(1),           kHasMaximumFlag, 28, 28,
      SECTION(Data, 10),         // data section
      ENTRY_COUNT(1),            // one entry
      LINEAR_MEMORY_INDEX_0,     // mem index
      WASM_INIT_EXPR_I32V_1(0),  // offset
      U32V_5(0xFFFFFFFF)         // size
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
      SECTION(Table, 4), ENTRY_COUNT(1), kLocalAnyFunc, 0, 1};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.ok()) {
    EXPECT_EQ(1u, result.val->signatures.size());
    EXPECT_EQ(1u, result.val->functions.size());
    EXPECT_EQ(1u, result.val->tables.size());
    EXPECT_EQ(1u, result.val->tables[0].initial_size);
  }
}

TEST_F(WasmModuleVerifyTest, ElementSectionWithInternalTable) {
  static const byte data[] = {
      // table ---------------------------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), kLocalAnyFunc, 0, 1,
      // elements ------------------------------------------------------------
      SECTION(Element, 1),
      0  // entry count
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionWithImportedTable) {
  static const byte data[] = {
      // imports -------------------------------------------------------------
      SECTION(Import, 9), ENTRY_COUNT(1),
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      't',             // table name
      kExternalTable,  // import kind
      kLocalAnyFunc,   // elem_type
      0,               // no maximum field
      1,               // initial size
      // elements ------------------------------------------------------------
      SECTION(Element, 1),
      0  // entry count
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionWithoutTable) {
  // Test that an element section without a table causes a validation error.
  static const byte data[] = {
      // elements ------------------------------------------------------------
      SECTION(Element, 4),
      1,  // entry count
      0,  // table index
      0,  // offset
      0   // number of elements
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, Regression_735887) {
  // Test with an invalid function index in the element section.
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), kLocalAnyFunc, 0, 1,
      // elements ------------------------------------------------------------
      SECTION(Element, 7),
      1,  // entry count
      TABLE_INDEX(0), WASM_INIT_EXPR_I32V_1(0),
      1,    // elements count
      0x9A  // invalid I32V as function index
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, OneIndirectFunction_one_entry) {
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 4), ENTRY_COUNT(1), kLocalAnyFunc, 0, 1,
      // elements ------------------------------------------------------------
      SECTION(Element, 7),
      1,  // entry count
      TABLE_INDEX(0), WASM_INIT_EXPR_I32V_1(0),
      1,  // elements count
      FUNC_INDEX(0)};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.ok()) {
    EXPECT_EQ(1u, result.val->signatures.size());
    EXPECT_EQ(1u, result.val->functions.size());
    EXPECT_EQ(1u, result.val->tables.size());
    EXPECT_EQ(1u, result.val->tables[0].initial_size);
  }
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
      SECTION(Table, 4), ENTRY_COUNT(1), kLocalAnyFunc, 0, 8,
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
    EXPECT_EQ(2u, result.val->signatures.size());
    EXPECT_EQ(4u, result.val->functions.size());
    EXPECT_EQ(1u, result.val->tables.size());
    EXPECT_EQ(8u, result.val->tables[0].initial_size);
  }
}

TEST_F(WasmModuleVerifyTest, ElementSectionMultipleTables) {
  // Test that if we have multiple tables, in the element section we can target
  // and initialize all tables.
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 7), ENTRY_COUNT(2),  // section header
      kLocalAnyFunc, 0, 5,                // table 0
      kLocalAnyFunc, 0, 9,                // table 1
      // elements ------------------------------------------------------------
      SECTION(Element, 14),
      2,                         // entry count
      TABLE_INDEX(0),            // element for table 0
      WASM_INIT_EXPR_I32V_1(0),  // index
      1,                         // elements count
      FUNC_INDEX(0),             // function
      TABLE_INDEX(1),            // element for table 1
      WASM_INIT_EXPR_I32V_1(7),  // index
      2,                         // elements count
      FUNC_INDEX(0),             // entry 0
      FUNC_INDEX(0),             // entry 1
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMixedTables) {
  // Test that if we have multiple tables, both imported and module-defined, in
  // the element section we can target and initialize all tables.
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // imports -------------------------------------------------------------
      SECTION(Import, 17), ENTRY_COUNT(2),
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      't',             // table name
      kExternalTable,  // import kind
      kLocalAnyFunc,   // elem_type
      0,               // no maximum field
      5,               // initial size
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      's',             // table name
      kExternalTable,  // import kind
      kLocalAnyFunc,   // elem_type
      0,               // no maximum field
      10,              // initial size
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 7), ENTRY_COUNT(2),  // section header
      kLocalAnyFunc, 0, 15,               // table 0
      kLocalAnyFunc, 0, 19,               // table 1
      // elements ------------------------------------------------------------
      SECTION(Element, 27),
      4,                          // entry count
      TABLE_INDEX(0),             // element for table 0
      WASM_INIT_EXPR_I32V_1(0),   // index
      1,                          // elements count
      FUNC_INDEX(0),              // function
      TABLE_INDEX(1),             // element for table 1
      WASM_INIT_EXPR_I32V_1(7),   // index
      2,                          // elements count
      FUNC_INDEX(0),              // entry 0
      FUNC_INDEX(0),              // entry 1
      TABLE_INDEX(2),             // element for table 2
      WASM_INIT_EXPR_I32V_1(12),  // index
      1,                          // elements count
      FUNC_INDEX(0),              // function
      TABLE_INDEX(3),             // element for table 1
      WASM_INIT_EXPR_I32V_1(17),  // index
      2,                          // elements count
      FUNC_INDEX(0),              // entry 0
      FUNC_INDEX(0),              // entry 1
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMultipleTablesArbitraryOrder) {
  // Test that the order in which tables are targeted in the element secion
  // can be arbitrary.
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 7), ENTRY_COUNT(2),  // section header
      kLocalAnyFunc, 0, 5,                // table 0
      kLocalAnyFunc, 0, 9,                // table 1
      // elements ------------------------------------------------------------
      SECTION(Element, 20),
      3,                         // entry count
      TABLE_INDEX(0),            // element for table 1
      WASM_INIT_EXPR_I32V_1(0),  // index
      1,                         // elements count
      FUNC_INDEX(0),             // function
      TABLE_INDEX(1),            // element for table 0
      WASM_INIT_EXPR_I32V_1(7),  // index
      2,                         // elements count
      FUNC_INDEX(0),             // entry 0
      FUNC_INDEX(0),             // entry 1
      TABLE_INDEX(0),            // element for table 1
      WASM_INIT_EXPR_I32V_1(3),  // index
      1,                         // elements count
      FUNC_INDEX(0),             // function
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMixedTablesArbitraryOrder) {
  // Test that the order in which tables are targeted in the element secion can
  // be arbitrary. In this test, tables can be both imported and module-defined.
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // imports -------------------------------------------------------------
      SECTION(Import, 17), ENTRY_COUNT(2),
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      't',             // table name
      kExternalTable,  // import kind
      kLocalAnyFunc,   // elem_type
      0,               // no maximum field
      5,               // initial size
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      's',             // table name
      kExternalTable,  // import kind
      kLocalAnyFunc,   // elem_type
      0,               // no maximum field
      10,              // initial size
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 7), ENTRY_COUNT(2),  // section header
      kLocalAnyFunc, 0, 15,               // table 0
      kLocalAnyFunc, 0, 19,               // table 1
      // elements ------------------------------------------------------------
      SECTION(Element, 27),
      4,                          // entry count
      TABLE_INDEX(2),             // element for table 0
      WASM_INIT_EXPR_I32V_1(10),  // index
      1,                          // elements count
      FUNC_INDEX(0),              // function
      TABLE_INDEX(3),             // element for table 1
      WASM_INIT_EXPR_I32V_1(17),  // index
      2,                          // elements count
      FUNC_INDEX(0),              // entry 0
      FUNC_INDEX(0),              // entry 1
      TABLE_INDEX(0),             // element for table 2
      WASM_INIT_EXPR_I32V_1(2),   // index
      1,                          // elements count
      FUNC_INDEX(0),              // function
      TABLE_INDEX(1),             // element for table 1
      WASM_INIT_EXPR_I32V_1(7),   // index
      2,                          // elements count
      FUNC_INDEX(0),              // entry 0
      FUNC_INDEX(0),              // entry 1
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionDontInitAnyRefTable) {
  // Test that tables of type 'AnyRef' cannot be initialized by the element
  // section.
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 7), ENTRY_COUNT(2),  // section header
      kLocalAnyRef, 0, 5,                 // table 0
      kLocalAnyFunc, 0, 9,                // table 1
      // elements ------------------------------------------------------------
      SECTION(Element, 14),
      2,                         // entry count
      TABLE_INDEX(0),            // element for table 0
      WASM_INIT_EXPR_I32V_1(0),  // index
      1,                         // elements count
      FUNC_INDEX(0),             // function
      TABLE_INDEX(1),            // element for table 1
      WASM_INIT_EXPR_I32V_1(7),  // index
      2,                         // elements count
      FUNC_INDEX(0),             // entry 0
      FUNC_INDEX(0),             // entry 1
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionDontInitAnyRefImportedTable) {
  // Test that imported tables of type AnyRef cannot be initialized in the
  // elements section.
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // imports -------------------------------------------------------------
      SECTION(Import, 17), ENTRY_COUNT(2),
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      't',             // table name
      kExternalTable,  // import kind
      kLocalAnyFunc,   // elem_type
      0,               // no maximum field
      5,               // initial size
      NAME_LENGTH(1),  // --
      'm',             // module name
      NAME_LENGTH(1),  // --
      's',             // table name
      kExternalTable,  // import kind
      kLocalAnyRef,    // elem_type
      0,               // no maximum field
      10,              // initial size
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION,
      // table declaration ---------------------------------------------------
      SECTION(Table, 7), ENTRY_COUNT(2),  // section header
      kLocalAnyFunc, 0, 15,               // table 0
      kLocalAnyFunc, 0, 19,               // table 1
      // elements ------------------------------------------------------------
      SECTION(Element, 14),
      4,                          // entry count
      TABLE_INDEX(0),             // element for table 0
      WASM_INIT_EXPR_I32V_1(10),  // index
      1,                          // elements count
      FUNC_INDEX(0),              // function
      TABLE_INDEX(1),             // element for table 1
      WASM_INIT_EXPR_I32V_1(17),  // index
      2,                          // elements count
      FUNC_INDEX(0),              // entry 0
      FUNC_INDEX(0),              // entry 1
  };

  EXPECT_FAILURE(data);
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

TEST_F(WasmModuleVerifyTest, MultipleTablesWithoutFlag) {
  static const byte data[] = {
      SECTION(Table, 7),  // table section
      ENTRY_COUNT(2),     // 2 tables
      kLocalAnyFunc,      // table 1: type
      0,                  // table 1: no maximum
      10,                 // table 1: minimum size
      kLocalAnyFunc,      // table 2: type
      0,                  // table 2: no maximum
      10,                 // table 2: minimum size
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, MultipleTablesWithFlag) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      SECTION(Table, 7),  // table section
      ENTRY_COUNT(2),     // 2 tables
      kLocalAnyFunc,      // table 1: type
      0,                  // table 1: no maximum
      10,                 // table 1: minimum size
      kLocalAnyRef,       // table 2: type
      0,                  // table 2: no maximum
      11,                 // table 2: minimum size
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(2u, result.val->tables.size());

  EXPECT_EQ(10u, result.val->tables[0].initial_size);
  EXPECT_EQ(kWasmAnyFunc, result.val->tables[0].type);

  EXPECT_EQ(11u, result.val->tables[1].initial_size);
  EXPECT_EQ(kWasmAnyRef, result.val->tables[1].type);
}

class WasmSignatureDecodeTest : public TestWithZone {
 public:
  WasmFeatures enabled_features_;

  FunctionSig* DecodeSig(const byte* start, const byte* end) {
    return DecodeWasmSignatureForTesting(enabled_features_, zone(), start, end);
  }
};

TEST_F(WasmSignatureDecodeTest, Ok_v_v) {
  static const byte data[] = {SIG_ENTRY_v_v};
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  FunctionSig* sig = DecodeSig(data, data + sizeof(data));

  EXPECT_TRUE(sig != nullptr);
  EXPECT_EQ(0u, sig->parameter_count());
  EXPECT_EQ(0u, sig->return_count());
}

TEST_F(WasmSignatureDecodeTest, Ok_t_v) {
  WASM_FEATURE_SCOPE(anyref);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueTypePair ret_type = kValueTypes[i];
    const byte data[] = {SIG_ENTRY_x(ret_type.code)};
    FunctionSig* sig = DecodeSig(data, data + sizeof(data));

    EXPECT_TRUE(sig != nullptr);
    EXPECT_EQ(0u, sig->parameter_count());
    EXPECT_EQ(1u, sig->return_count());
    EXPECT_EQ(ret_type.type, sig->GetReturn());
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_v_t) {
  WASM_FEATURE_SCOPE(anyref);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueTypePair param_type = kValueTypes[i];
    const byte data[] = {SIG_ENTRY_v_x(param_type.code)};
    FunctionSig* sig = DecodeSig(data, data + sizeof(data));

    EXPECT_TRUE(sig != nullptr);
    EXPECT_EQ(1u, sig->parameter_count());
    EXPECT_EQ(0u, sig->return_count());
    EXPECT_EQ(param_type.type, sig->GetParam(0));
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_t_t) {
  WASM_FEATURE_SCOPE(anyref);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueTypePair ret_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueTypePair param_type = kValueTypes[j];
      const byte data[] = {SIG_ENTRY_x_x(ret_type.code, param_type.code)};
      FunctionSig* sig = DecodeSig(data, data + sizeof(data));

      EXPECT_TRUE(sig != nullptr);
      EXPECT_EQ(1u, sig->parameter_count());
      EXPECT_EQ(1u, sig->return_count());
      EXPECT_EQ(param_type.type, sig->GetParam(0));
      EXPECT_EQ(ret_type.type, sig->GetReturn());
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_i_tt) {
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(mv);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueTypePair p0_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueTypePair p1_type = kValueTypes[j];
      const byte data[] = {
          SIG_ENTRY_x_xx(kLocalI32, p0_type.code, p1_type.code)};
      FunctionSig* sig = DecodeSig(data, data + sizeof(data));

      EXPECT_TRUE(sig != nullptr);
      EXPECT_EQ(2u, sig->parameter_count());
      EXPECT_EQ(1u, sig->return_count());
      EXPECT_EQ(p0_type.type, sig->GetParam(0));
      EXPECT_EQ(p1_type.type, sig->GetParam(1));
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Ok_tt_tt) {
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(mv);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueTypePair p0_type = kValueTypes[i];
    for (size_t j = 0; j < arraysize(kValueTypes); j++) {
      ValueTypePair p1_type = kValueTypes[j];
      const byte data[] = {SIG_ENTRY_xx_xx(p0_type.code, p1_type.code,
                                           p0_type.code, p1_type.code)};
      FunctionSig* sig = DecodeSig(data, data + sizeof(data));

      EXPECT_TRUE(sig != nullptr);
      EXPECT_EQ(2u, sig->parameter_count());
      EXPECT_EQ(2u, sig->return_count());
      EXPECT_EQ(p0_type.type, sig->GetParam(0));
      EXPECT_EQ(p1_type.type, sig->GetParam(1));
      EXPECT_EQ(p0_type.type, sig->GetReturn(0));
      EXPECT_EQ(p1_type.type, sig->GetReturn(1));
    }
  }
}

TEST_F(WasmSignatureDecodeTest, TooManyParams) {
  static const byte data[] = {kWasmFunctionTypeCode,
                              WASM_I32V_3(kV8MaxWasmFunctionParams + 1),
                              kLocalI32, 0};
  FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_FALSE(sig != nullptr);
}

TEST_F(WasmSignatureDecodeTest, TooManyReturns) {
  for (int i = 0; i < 2; i++) {
    bool enable_mv = i != 0;
    WASM_FEATURE_SCOPE_VAL(mv, enable_mv);
    const int max_return_count = static_cast<int>(
        enable_mv ? kV8MaxWasmFunctionMultiReturns : kV8MaxWasmFunctionReturns);
    byte data[] = {kWasmFunctionTypeCode, 0, WASM_I32V_3(max_return_count + 1),
                   kLocalI32};
    FunctionSig* sig = DecodeSig(data, data + sizeof(data));
    EXPECT_EQ(nullptr, sig);
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_off_end) {
  byte data[256];
  for (int p = 0; p <= 255; p = p + 1 + p * 3) {
    for (int i = 0; i <= p; i++) data[i] = kLocalI32;
    data[0] = static_cast<byte>(p);

    for (int i = 0; i < p + 1; i++) {
      // Should fall off the end for all signatures.
      FunctionSig* sig = DecodeSig(data, data + i);
      EXPECT_EQ(nullptr, sig);
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_anyref_without_flag) {
  // Disable AnyRef support and check that decoding fails.
  WASM_FEATURE_SCOPE_VAL(anyref, false);
  byte ref_types[] = {kLocalAnyFunc, kLocalAnyRef};
  for (byte invalid_type : ref_types) {
    for (size_t i = 0; i < SIZEOF_SIG_ENTRY_x_xx; i++) {
      byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalI32)};
      data[i] = invalid_type;
      FunctionSig* sig = DecodeSig(data, data + sizeof(data));
      EXPECT_EQ(nullptr, sig);
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_type) {
  byte kInvalidType = 76;
  for (size_t i = 0; i < SIZEOF_SIG_ENTRY_x_xx; i++) {
    byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalI32)};
    data[i] = kInvalidType;
    FunctionSig* sig = DecodeSig(data, data + sizeof(data));
    EXPECT_EQ(nullptr, sig);
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_ret_type1) {
  static const byte data[] = {SIG_ENTRY_x_x(kLocalVoid, kLocalI32)};
  FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type1) {
  static const byte data[] = {SIG_ENTRY_x_x(kLocalI32, kLocalVoid)};
  FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type2) {
  static const byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalVoid)};
  FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

class WasmFunctionVerifyTest : public TestWithIsolateAndZone {
 public:
  WasmFeatures enabled_features_;
  WasmModule module;
  Vector<const byte> bytes;

  FunctionResult DecodeWasmFunction(const ModuleWireBytes& wire_bytes,
                                    const WasmModule* module,
                                    const byte* function_start,
                                    const byte* function_end) {
    return DecodeWasmFunctionForTesting(enabled_features_, zone(), wire_bytes,
                                        module, function_start, function_end,
                                        isolate()->counters());
  }
};

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
      kExprEnd    // body
  };

  FunctionResult result =
      DecodeWasmFunction(bytes, &module, data, data + sizeof(data));
  EXPECT_OK(result);

  if (result.val && result.ok()) {
    WasmFunction* function = result.val.get();
    EXPECT_EQ(0u, function->sig->parameter_count());
    EXPECT_EQ(0u, function->sig->return_count());
    EXPECT_EQ(static_cast<uint32_t>(SIZEOF_SIG_ENTRY_v_v),
              function->code.offset());
    EXPECT_EQ(sizeof(data), function->code.end_offset());
    // TODO(titzer): verify encoding of local declarations
  }
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
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,  // section data
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
      UNKNOWN_SECTION(5), 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
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

  EXPECT_EQ(1u, result.val->globals.size());
  EXPECT_EQ(0u, result.val->functions.size());
  EXPECT_EQ(0u, result.val->data_segments.size());

  const WasmGlobal* global = &result.val->globals.back();

  EXPECT_EQ(kWasmI32, global->type);
  EXPECT_EQ(0u, global->offset);
}

TEST_F(WasmModuleVerifyTest, ImportTable_empty) {
  static const byte data[] = {SECTION(Type, 1), 0, SECTION(Import, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs1) {
  static const byte data[] = {SECTION(Import, 1), 0};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_mutable_global) {
  WASM_FEATURE_SCOPE(mut_global);
  {
    static const byte data[] = {
        SECTION(Import, 8),  // section header
        1,                   // number of imports
        NAME_LENGTH(1),      // --
        'm',                 // module name
        NAME_LENGTH(1),      // --
        'f',                 // global name
        kExternalGlobal,     // import kind
        kLocalI32,           // type
        0,                   // mutability
    };
    EXPECT_VERIFIES(data);
  }
  {
    static const byte data[] = {
        SECTION(Import, 8),  // section header
        1,                   // sig table
        NAME_LENGTH(1),      // --
        'm',                 // module name
        NAME_LENGTH(1),      // --
        'f',                 // global name
        kExternalGlobal,     // import kind
        kLocalI32,           // type
        1,                   // mutability
    };
    EXPECT_VERIFIES(data);
  }
}

TEST_F(WasmModuleVerifyTest, ImportTable_mutability_malformed) {
  WASM_FEATURE_SCOPE(mut_global);
  static const byte data[] = {
      SECTION(Import, 8),
      1,                   // --
      NAME_LENGTH(1),      // --
      'm',                 // module name
      NAME_LENGTH(1),      // --
      'g',                 // global name
      kExternalGlobal,     // import kind
      kLocalI32,           // type
      2,                   // invalid mutability
  };
  EXPECT_FAILURE(data);
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

  EXPECT_EQ(1u, result.val->functions.size());
  EXPECT_EQ(0u, result.val->export_table.size());
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

  EXPECT_EQ(1u, result.val->functions.size());
  EXPECT_EQ(1u, result.val->export_table.size());
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

  EXPECT_EQ(1u, result.val->functions.size());
  EXPECT_EQ(2u, result.val->export_table.size());
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

  EXPECT_EQ(3u, result.val->functions.size());
  EXPECT_EQ(3u, result.val->export_table.size());
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

  for (size_t length = 33; length < sizeof(data); length++) {
    ModuleResult result = DecodeModule(data, data + length);
    EXPECT_FALSE(result.ok());
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

TEST_F(WasmModuleVerifyTest, Regression_738097) {
  // The function body size caused an integer overflow in the module decoder.
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
      FUNCTION_SIGNATURES_SECTION(1, 0),     // --
      SECTION(Code, 1 + 5 + 1),              // --
      1,                                     // --
      U32V_5(0xFFFFFFFF),                    // function size,
      0                                      // No real body
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodySizeLimit) {
  const uint32_t delta = 3;
  for (uint32_t body_size = kV8MaxWasmFunctionSize - delta;
       body_size < kV8MaxWasmFunctionSize + delta; body_size++) {
    byte data[] = {
        SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
        FUNCTION_SIGNATURES_SECTION(1, 0),     // --
        kCodeSectionCode,                      // code section
        U32V_5(1 + body_size + 5),             // section size
        1,                                     // # functions
        U32V_5(body_size)                      // body size
    };
    size_t total = sizeof(data) + body_size;
    byte* buffer = reinterpret_cast<byte*>(calloc(1, total));
    memcpy(buffer, data, sizeof(data));
    ModuleResult result = DecodeModule(buffer, buffer + total);
    if (body_size <= kV8MaxWasmFunctionSize) {
      EXPECT_TRUE(result.ok());
    } else {
      EXPECT_FALSE(result.ok());
    }
    free(buffer);
  }
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

TEST_F(WasmModuleVerifyTest, Regression684855) {
  static const byte data[] = {
      SECTION_NAMES(12),
      0xFB,  // functions count
      0x27,  // |
      0x00,  // function name length
      0xFF,  // local names count
      0xFF,  // |
      0xFF,  // |
      0xFF,  // |
      0xFF,  // |
      0xFF,  // error: "varint too large"
      0xFF,  // |
      0x00,  // --
      0x00   // --
  };
  EXPECT_VERIFIES(data);
}

class WasmInitExprDecodeTest : public TestWithZone {
 public:
  WasmInitExprDecodeTest() = default;

  WasmFeatures enabled_features_;

  WasmInitExpr DecodeInitExpr(const byte* start, const byte* end) {
    return DecodeWasmInitExprForTesting(enabled_features_, start, end);
  }
};

#define EXPECT_INIT_EXPR(Type, type, value, ...)                   \
  {                                                                \
    static const byte data[] = {__VA_ARGS__, kExprEnd};            \
    WasmInitExpr expr = DecodeInitExpr(data, data + sizeof(data)); \
    EXPECT_EQ(WasmInitExpr::k##Type##Const, expr.kind);            \
    EXPECT_EQ(value, expr.val.type##_const);                       \
  }

#define EXPECT_INIT_EXPR_FAIL(...)                                 \
  {                                                                \
    static const byte data[] = {__VA_ARGS__, kExprEnd};            \
    WasmInitExpr expr = DecodeInitExpr(data, data + sizeof(data)); \
    EXPECT_EQ(WasmInitExpr::kNone, expr.kind);                     \
  }

TEST_F(WasmInitExprDecodeTest, InitExpr_i32) {
  EXPECT_INIT_EXPR(I32, i32, 33, WASM_I32V_1(33));
  EXPECT_INIT_EXPR(I32, i32, -21, WASM_I32V_1(-21));
  EXPECT_INIT_EXPR(I32, i32, 437, WASM_I32V_2(437));
  EXPECT_INIT_EXPR(I32, i32, 77777, WASM_I32V_3(77777));
}

TEST_F(WasmInitExprDecodeTest, InitExpr_f32) {
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(13.1), WASM_F32(13.1));
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(-21.1), WASM_F32(-21.1));
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(437.2), WASM_F32(437.2));
  EXPECT_INIT_EXPR(F32, f32, static_cast<float>(77777.3), WASM_F32(77777.3));
}

TEST_F(WasmInitExprDecodeTest, InitExpr_i64) {
  EXPECT_INIT_EXPR(I64, i64, 33, WASM_I64V_1(33));
  EXPECT_INIT_EXPR(I64, i64, -21, WASM_I64V_2(-21));
  EXPECT_INIT_EXPR(I64, i64, 437, WASM_I64V_5(437));
  EXPECT_INIT_EXPR(I64, i64, 77777, WASM_I64V_7(77777));
}

TEST_F(WasmInitExprDecodeTest, InitExpr_f64) {
  EXPECT_INIT_EXPR(F64, f64, 83.22, WASM_F64(83.22));
  EXPECT_INIT_EXPR(F64, f64, -771.3, WASM_F64(-771.3));
  EXPECT_INIT_EXPR(F64, f64, 43703.0, WASM_F64(43703.0));
  EXPECT_INIT_EXPR(F64, f64, 77999.1, WASM_F64(77999.1));
}

TEST_F(WasmInitExprDecodeTest, InitExpr_AnyRef) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {kExprRefNull, kExprEnd};
  WasmInitExpr expr = DecodeInitExpr(data, data + sizeof(data));
  EXPECT_EQ(WasmInitExpr::kAnyRefConst, expr.kind);
}

TEST_F(WasmInitExprDecodeTest, InitExpr_illegal) {
  EXPECT_INIT_EXPR_FAIL(WASM_I32V_1(0), WASM_I32V_1(0));
  EXPECT_INIT_EXPR_FAIL(WASM_GET_LOCAL(0));
  EXPECT_INIT_EXPR_FAIL(WASM_SET_LOCAL(0, WASM_I32V_1(0)));
  EXPECT_INIT_EXPR_FAIL(WASM_I32_ADD(WASM_I32V_1(0), WASM_I32V_1(0)));
  EXPECT_INIT_EXPR_FAIL(WASM_IF_ELSE(WASM_ZERO, WASM_ZERO, WASM_ZERO));
}

TEST_F(WasmModuleVerifyTest, Multiple_Named_Sections) {
  static const byte data[] = {
      SECTION(Unknown, 4), 1, 'X', 17,  18,                      // --
      SECTION(Unknown, 9), 3, 'f', 'o', 'o', 5,   6,   7, 8, 9,  // --
      SECTION(Unknown, 8), 5, 'o', 't', 'h', 'e', 'r', 7, 8,     // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Section_Name_No_UTF8) {
  static const byte data[] = {SECTION(Unknown, 4), 1, 0xFF, 17, 18};
  EXPECT_FAILURE(data);
}

class WasmModuleCustomSectionTest : public TestWithIsolateAndZone {
 public:
  void CheckSections(const byte* module_start, const byte* module_end,
                     const CustomSectionOffset* expected, size_t num_expected) {
    std::vector<CustomSectionOffset> custom_sections =
        DecodeCustomSections(module_start, module_end);

    CHECK_EQ(num_expected, custom_sections.size());

    for (size_t i = 0; i < num_expected; i++) {
      EXPECT_EQ(expected[i].section.offset(),
                custom_sections[i].section.offset());
      EXPECT_EQ(expected[i].section.length(),
                custom_sections[i].section.length());
      EXPECT_EQ(expected[i].name.offset(), custom_sections[i].name.offset());
      EXPECT_EQ(expected[i].name.length(), custom_sections[i].name.length());
      EXPECT_EQ(expected[i].payload.offset(),
                custom_sections[i].payload.offset());
      EXPECT_EQ(expected[i].payload.length(),
                custom_sections[i].payload.length());
    }
  }
};

TEST_F(WasmModuleCustomSectionTest, ThreeUnknownSections) {
  static constexpr byte data[] = {
      U32_LE(kWasmMagic),                                         // --
      U32_LE(kWasmVersion),                                       // --
      SECTION(Unknown, 4),  1, 'X', 17,  18,                      // --
      SECTION(Unknown, 9),  3, 'f', 'o', 'o', 5,   6,   7, 8, 9,  // --
      SECTION(Unknown, 8),  5, 'o', 't', 'h', 'e', 'r', 7, 8,     // --
  };

  static const CustomSectionOffset expected[] = {
      // section, name, payload
      {{10, 4}, {11, 1}, {12, 2}},  // --
      {{16, 9}, {17, 3}, {20, 5}},  // --
      {{27, 8}, {28, 5}, {33, 2}},  // --
  };

  CheckSections(data, data + sizeof(data), expected, arraysize(expected));
}

TEST_F(WasmModuleCustomSectionTest, TwoKnownTwoUnknownSections) {
  static const byte data[] = {
      U32_LE(kWasmMagic),                                   // --
      U32_LE(kWasmVersion),                                 // --
      SIGNATURES_SECTION(2, SIG_ENTRY_v_v, SIG_ENTRY_v_v),  // --
      SECTION(Unknown, 4),
      1,
      'X',
      17,
      18,  // --
      ONE_EMPTY_FUNCTION,
      SECTION(Unknown, 8),
      5,
      'o',
      't',
      'h',
      'e',
      'r',
      7,
      8,  // --
  };

  static const CustomSectionOffset expected[] = {
      // section, name, payload
      {{19, 4}, {20, 1}, {21, 2}},  // --
      {{29, 8}, {30, 5}, {35, 2}},  // --
  };

  CheckSections(data, data + sizeof(data), expected, arraysize(expected));
}

#define SRC_MAP                                                             \
  16, 's', 'o', 'u', 'r', 'c', 'e', 'M', 'a', 'p', 'p', 'i', 'n', 'g', 'U', \
      'R', 'L'
TEST_F(WasmModuleVerifyTest, SourceMappingURLSection) {
#define SRC 's', 'r', 'c', '/', 'x', 'y', 'z', '.', 'c'
  static const byte data[] = {SECTION(Unknown, 27), SRC_MAP, 9, SRC};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(9u, result.val->source_map_url.size());
  const char src[] = {SRC};
  EXPECT_EQ(
      0,
      strncmp(reinterpret_cast<const char*>(result.val->source_map_url.data()),
              src, 9));
#undef SRC
}

TEST_F(WasmModuleVerifyTest, BadSourceMappingURLSection) {
#define BAD_SRC 's', 'r', 'c', '/', 'x', 0xff, 'z', '.', 'c'
  static const byte data[] = {SECTION(Unknown, 27), SRC_MAP, 9, BAD_SRC};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(0u, result.val->source_map_url.size());
#undef BAD_SRC
}

TEST_F(WasmModuleVerifyTest, MultipleSourceMappingURLSections) {
#define SRC 'a', 'b', 'c'
  static const byte data[] = {SECTION(Unknown, 21),
                              SRC_MAP,
                              3,
                              SRC,
                              SECTION(Unknown, 21),
                              SRC_MAP,
                              3,
                              'p',
                              'q',
                              'r'};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(3u, result.val->source_map_url.size());
  const char src[] = {SRC};
  EXPECT_EQ(
      0,
      strncmp(reinterpret_cast<const char*>(result.val->source_map_url.data()),
              src, 3));
#undef SRC
}
#undef SRC_MAP

TEST_F(WasmModuleVerifyTest, MultipleNameSections) {
#define NAME_SECTION 4, 'n', 'a', 'm', 'e'
  static const byte data[] = {SECTION(Unknown, 11),
                              NAME_SECTION,
                              0,
                              4,
                              3,
                              'a',
                              'b',
                              'c',
                              SECTION(Unknown, 12),
                              NAME_SECTION,
                              0,
                              5,
                              4,
                              'p',
                              'q',
                              'r',
                              's'};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(3u, result.val->name.length());
#undef NAME_SECTION
}

#undef WASM_FEATURE_SCOPE
#undef WASM_FEATURE_SCOPE_VAL
#undef EXPECT_INIT_EXPR
#undef EXPECT_INIT_EXPR_FAIL
#undef WASM_INIT_EXPR_I32V_1
#undef WASM_INIT_EXPR_I32V_2
#undef WASM_INIT_EXPR_I32V_3
#undef WASM_INIT_EXPR_I32V_4
#undef WASM_INIT_EXPR_I32V_5
#undef WASM_INIT_EXPR_F32
#undef WASM_INIT_EXPR_I64
#undef WASM_INIT_EXPR_F64
#undef WASM_INIT_EXPR_ANYREF
#undef WASM_INIT_EXPR_GLOBAL
#undef SIZEOF_EMPTY_FUNCTION
#undef EMPTY_BODY
#undef SIZEOF_EMPTY_BODY
#undef NOP_BODY
#undef SIZEOF_NOP_BODY
#undef SIG_ENTRY_i_i
#undef UNKNOWN_SECTION
#undef SECTION
#undef SIGNATURES_SECTION
#undef FUNCTION_SIGNATURES_SECTION
#undef FOO_STRING
#undef NO_LOCAL_NAMES
#undef EMPTY_SIGNATURES_SECTION
#undef EMPTY_FUNCTION_SIGNATURES_SECTION
#undef EMPTY_FUNCTION_BODIES_SECTION
#undef SECTION_NAMES
#undef SECTION_EXCEPTIONS
#undef EMPTY_NAMES_SECTION
#undef FAIL_IF_NO_EXPERIMENTAL_EH
#undef X1
#undef X2
#undef X3
#undef X4
#undef ONE_EMPTY_FUNCTION
#undef TWO_EMPTY_FUNCTIONS
#undef THREE_EMPTY_FUNCTIONS
#undef FOUR_EMPTY_FUNCTIONS
#undef ONE_EMPTY_BODY
#undef TWO_EMPTY_BODIES
#undef THREE_EMPTY_BODIES
#undef FOUR_EMPTY_BODIES
#undef SIGNATURES_SECTION_VOID_VOID
#undef LINEAR_MEMORY_INDEX_0
#undef EXPECT_VERIFIES
#undef EXPECT_FAILURE_LEN
#undef EXPECT_FAILURE
#undef EXPECT_OFF_END_FAILURE
#undef EXPECT_OK

}  // namespace module_decoder_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8

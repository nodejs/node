// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/handles/handles.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "testing/gmock-support.h"

using testing::HasSubstr;

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
#define WASM_INIT_EXPR_REF_NULL WASM_REF_NULL, kExprEnd
#define WASM_INIT_EXPR_REF_FUNC(val) WASM_REF_FUNC(val), kExprEnd
#define WASM_INIT_EXPR_GLOBAL(index) WASM_GET_GLOBAL(index), kExprEnd

#define REF_NULL_ELEMENT kExprRefNull, kExprEnd
#define REF_FUNC_ELEMENT(v) kExprRefFunc, U32V_1(v), kExprEnd

#define EMPTY_BODY 0
#define NOP_BODY 2, 0, kExprNop

#define SIG_ENTRY_i_i SIG_ENTRY_x_x(kLocalI32, kLocalI32)

#define UNKNOWN_SECTION(size) 0, U32V_1(size + 5), ADD_COUNT('l', 'u', 'l', 'z')

template <typename... Args>
std::integral_constant<size_t, sizeof...(Args)> CountArgsHelper(Args...);
#define COUNT_ARGS(...) (decltype(CountArgsHelper(__VA_ARGS__))::value)

template <size_t num>
struct CheckLEB1 : std::integral_constant<size_t, num> {
  static_assert(num <= I32V_MAX(1), "LEB range check");
};
#define CHECK_LEB1(num) CheckLEB1<num>::value

#define ADD_COUNT(...) CHECK_LEB1(COUNT_ARGS(__VA_ARGS__)), __VA_ARGS__

#define SECTION(name, ...) k##name##SectionCode, ADD_COUNT(__VA_ARGS__)

#define SIGNATURES_SECTION(count, ...) SECTION(Type, U32V_1(count), __VA_ARGS__)
#define FUNCTION_SIGNATURES_SECTION(count, ...) \
  SECTION(Function, U32V_1(count), __VA_ARGS__)

#define FOO_STRING ADD_COUNT('f', 'o', 'o')
#define NO_LOCAL_NAMES 0

#define EMPTY_SIGNATURES_SECTION SECTION(Type, ENTRY_COUNT(0))
#define EMPTY_FUNCTION_SIGNATURES_SECTION SECTION(Function, ENTRY_COUNT(0))
#define EMPTY_FUNCTION_BODIES_SECTION SECTION(Code, ENTRY_COUNT(0))
#define SECTION_NAMES(...) \
  SECTION(Unknown, ADD_COUNT('n', 'a', 'm', 'e'), ##__VA_ARGS__)
#define EMPTY_NAMES_SECTION SECTION_NAMES()
#define SECTION_SRC_MAP(...)                                               \
  SECTION(Unknown,                                                         \
          ADD_COUNT('s', 'o', 'u', 'r', 'c', 'e', 'M', 'a', 'p', 'p', 'i', \
                    'n', 'g', 'U', 'R', 'L'),                              \
          ADD_COUNT(__VA_ARGS__))
#define SECTION_COMPILATION_HINTS(...)                                     \
  SECTION(Unknown,                                                         \
          ADD_COUNT('c', 'o', 'm', 'p', 'i', 'l', 'a', 't', 'i', 'o', 'n', \
                    'H', 'i', 'n', 't', 's'),                              \
          ADD_COUNT(__VA_ARGS__))

#define FAIL_IF_NO_EXPERIMENTAL_EH(data)                                 \
  do {                                                                   \
    ModuleResult result = DecodeModule((data), (data) + sizeof((data))); \
    EXPECT_FALSE(result.ok());                                           \
  } while (false)

#define X1(...) __VA_ARGS__
#define X2(...) __VA_ARGS__, __VA_ARGS__
#define X3(...) __VA_ARGS__, __VA_ARGS__, __VA_ARGS__
#define X4(...) __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__

#define ONE_EMPTY_FUNCTION(sig_index) \
  SECTION(Function, ENTRY_COUNT(1), X1(sig_index))

#define TWO_EMPTY_FUNCTIONS(sig_index) \
  SECTION(Function, ENTRY_COUNT(2), X2(sig_index))

#define THREE_EMPTY_FUNCTIONS(sig_index) \
  SECTION(Function, ENTRY_COUNT(3), X3(sig_index))

#define FOUR_EMPTY_FUNCTIONS(sig_index) \
  SECTION(Function, ENTRY_COUNT(4), X4(sig_index))

#define ONE_EMPTY_BODY SECTION(Code, ENTRY_COUNT(1), X1(EMPTY_BODY))
#define TWO_EMPTY_BODIES SECTION(Code, ENTRY_COUNT(2), X2(EMPTY_BODY))
#define THREE_EMPTY_BODIES SECTION(Code, ENTRY_COUNT(3), X3(EMPTY_BODY))
#define FOUR_EMPTY_BODIES SECTION(Code, ENTRY_COUNT(4), X4(EMPTY_BODY))

#define SIGNATURES_SECTION_VOID_VOID \
  SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v)

#define LINEAR_MEMORY_INDEX_0 0

#define EXCEPTION_ENTRY(sig_index) U32V_1(kExceptionAttribute), sig_index

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

#define EXPECT_FAILURE_WITH_MSG(data, msg)                         \
  do {                                                             \
    ModuleResult result = DecodeModule(data, data + sizeof(data)); \
    EXPECT_FALSE(result.ok());                                     \
    EXPECT_THAT(result.error().message(), HasSubstr(msg));         \
  } while (false)

#define EXPECT_OFF_END_FAILURE(data, min)                           \
  do {                                                              \
    STATIC_ASSERT(min < arraysize(data));                           \
    for (size_t length = min; length < arraysize(data); length++) { \
      EXPECT_FAILURE_LEN(data, length);                             \
    }                                                               \
  } while (false)

#define EXPECT_OK(result)     \
  do {                        \
    EXPECT_TRUE(result.ok()); \
    if (!result.ok()) return; \
  } while (false)

#define EXPECT_NOT_OK(result, msg)                         \
  do {                                                     \
    EXPECT_FALSE(result.ok());                             \
    EXPECT_THAT(result.error().message(), HasSubstr(msg)); \
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
    {kLocalFuncRef, kWasmFuncRef},  // --
    {kLocalAnyRef, kWasmAnyRef},    // --
    {kLocalNullRef, kWasmNullRef}   // --
};

class WasmModuleVerifyTest : public TestWithIsolateAndZone {
 public:
  WasmFeatures enabled_features_ = WasmFeatures::None();

  ModuleResult DecodeModule(const byte* module_start, const byte* module_end) {
    // Add the wasm magic and version number automatically.
    size_t size = static_cast<size_t>(module_end - module_start);
    byte header[] = {WASM_MODULE_HEADER};
    size_t total = sizeof(header) + size;
    auto temp = new byte[total];
    memcpy(temp, header, sizeof(header));
    if (size > 0) {
      memcpy(temp + sizeof(header), module_start, size);
    }
    ModuleResult result = DecodeWasmModule(
        enabled_features_, temp, temp + total, false, kWasmOrigin,
        isolate()->counters(), isolate()->wasm_engine()->allocator());
    delete[] temp;
    return result;
  }
  ModuleResult DecodeModuleNoHeader(const byte* module_start,
                                    const byte* module_end) {
    return DecodeWasmModule(enabled_features_, module_start, module_end, false,
                            kWasmOrigin, isolate()->counters(),
                            isolate()->wasm_engine()->allocator());
  }
};

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
      SECTION(Global,                     // --
              ENTRY_COUNT(1),             // --
              kLocalI32,                  // local type
              0,                          // immutable
              WASM_INIT_EXPR_I32V_1(13))  // init
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(1u, result.value()->globals.size());
    EXPECT_EQ(0u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* global = &result.value()->globals.back();

    EXPECT_EQ(kWasmI32, global->type);
    EXPECT_EQ(0u, global->offset);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kI32Const, global->init.kind);
    EXPECT_EQ(13, global->init.val.i32_const);
  }

  EXPECT_OFF_END_FAILURE(data, 1);
}

TEST_F(WasmModuleVerifyTest, AnyRefGlobal) {
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      TWO_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      SECTION(Global,                       // --
              ENTRY_COUNT(2),               // --
              kLocalAnyRef,                 // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_NULL,      // init
              kLocalAnyRef,                 // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_FUNC(1)),  // init
      SECTION(Element,                      // section name
              ENTRY_COUNT(2),               // entry count
              DECLARATIVE,                  // flags 0
              kExternalFunction,            // type
              ENTRY_COUNT(1),               // func entry count
              FUNC_INDEX(0),                // func index
              DECLARATIVE_WITH_ELEMENTS,    // flags 1
              kLocalFuncRef,                // local type
              ENTRY_COUNT(1),               // func ref count
              REF_FUNC_ELEMENT(1)),         // func ref
      TWO_EMPTY_BODIES};

  {
    // Should decode to two globals.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(2u, result.value()->globals.size());
    EXPECT_EQ(2u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* global = &result.value()->globals[0];
    EXPECT_EQ(kWasmAnyRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kRefNullConst, global->init.kind);

    global = &result.value()->globals[1];
    EXPECT_EQ(kWasmAnyRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kRefFuncConst, global->init.kind);
    EXPECT_EQ(uint32_t{1}, global->init.val.function_index);
  }
}

TEST_F(WasmModuleVerifyTest, FuncRefGlobal) {
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      TWO_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      SECTION(Global,                       // --
              ENTRY_COUNT(2),               // --
              kLocalFuncRef,                // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_NULL,      // init
              kLocalFuncRef,                // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_FUNC(1)),  // init
      SECTION(Element,                      // section name
              ENTRY_COUNT(2),               // entry count
              DECLARATIVE,                  // flags 0
              kExternalFunction,            // type
              ENTRY_COUNT(1),               // func entry count
              FUNC_INDEX(0),                // func index
              DECLARATIVE_WITH_ELEMENTS,    // flags 1
              kLocalFuncRef,                // local type
              ENTRY_COUNT(1),               // func ref count
              REF_FUNC_ELEMENT(1)),         // func ref
      TWO_EMPTY_BODIES};
  {
    // Should decode to two globals.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(2u, result.value()->globals.size());
    EXPECT_EQ(2u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* global = &result.value()->globals[0];
    EXPECT_EQ(kWasmFuncRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kRefNullConst, global->init.kind);

    global = &result.value()->globals[1];
    EXPECT_EQ(kWasmFuncRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kRefFuncConst, global->init.kind);
    EXPECT_EQ(uint32_t{1}, global->init.val.function_index);
  }
}

TEST_F(WasmModuleVerifyTest, NullRefGlobal) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      TWO_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      SECTION(Global,                    // --
              ENTRY_COUNT(1),            // --
              kLocalNullRef,             // local type
              0,                         // immutable
              WASM_INIT_EXPR_REF_NULL),  // init
      TWO_EMPTY_BODIES};
  {
    // Should decode to one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(1u, result.value()->globals.size());
    EXPECT_EQ(2u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* global = &result.value()->globals[0];
    EXPECT_EQ(kWasmNullRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kRefNullConst, global->init.kind);
  }
}

TEST_F(WasmModuleVerifyTest, InvalidFuncRefGlobal) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      TWO_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      SECTION(Global,                       // --
              ENTRY_COUNT(1),               // --
              kLocalFuncRef,                // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_FUNC(7)),  // invalid function index
      TWO_EMPTY_BODIES};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, InvalidNullRefGlobal) {
  WASM_FEATURE_SCOPE(anyref);
  // Initialize nullref from funcref
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      TWO_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      SECTION(Global,                       // --
              ENTRY_COUNT(1),               // --
              kLocalNullRef,                // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_FUNC(1)),  // init
      TWO_EMPTY_BODIES};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(
      result,
      "type error in global initialization, expected nullref, got funcref");
}

TEST_F(WasmModuleVerifyTest, AnyRefGlobalWithGlobalInit) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      SECTION(Import,           // --
              ENTRY_COUNT(1),   // number of imports
              ADD_COUNT('m'),   // module name
              ADD_COUNT('f'),   // global name
              kExternalGlobal,  // import kind
              kLocalAnyRef,     // type
              0),               // mutability
      SECTION(Global,           // --
              ENTRY_COUNT(1),
              kLocalAnyRef,  // local type
              0,             // immutable
              WASM_INIT_EXPR_GLOBAL(0)),
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(2u, result.value()->globals.size());
    EXPECT_EQ(0u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* global = &result.value()->globals.back();

    EXPECT_EQ(kWasmAnyRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kGlobalIndex, global->init.kind);
  }
}

TEST_F(WasmModuleVerifyTest, NullRefGlobalWithGlobalInit) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      SECTION(Import,           // --
              ENTRY_COUNT(1),   // number of imports
              ADD_COUNT('m'),   // module name
              ADD_COUNT('n'),   // global name
              kExternalGlobal,  // import kind
              kLocalNullRef,    // type
              0),               // mutability
      SECTION(Global,           // --
              ENTRY_COUNT(1),
              kLocalNullRef,  // local type
              0,              // immutable
              WASM_INIT_EXPR_GLOBAL(0)),
  };

  {
    // Should decode to exactly one global.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    std::cout << result.error().message() << std::endl;
    EXPECT_OK(result);
    EXPECT_EQ(2u, result.value()->globals.size());
    EXPECT_EQ(0u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* global = &result.value()->globals.back();

    EXPECT_EQ(kWasmNullRef, global->type);
    EXPECT_FALSE(global->mutability);
    EXPECT_EQ(WasmInitExpr::kGlobalIndex, global->init.kind);
  }
}

TEST_F(WasmModuleVerifyTest, Global_invalid_type) {
  static const byte data[] = {
      SECTION(Global,                      // --
              ENTRY_COUNT(1),              // --
              64,                          // invalid memory type
              1,                           // mutable
              WASM_INIT_EXPR_I32V_1(33)),  // init
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, Global_invalid_type2) {
  static const byte data[] = {
      SECTION(Global,                      // --
              ENTRY_COUNT(1),              // --
              kLocalVoid,                  // invalid memory type
              1,                           // mutable
              WASM_INIT_EXPR_I32V_1(33)),  // init
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ZeroGlobals) {
  static const byte data[] = {SECTION(Global, ENTRY_COUNT(0))};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
}

TEST_F(WasmModuleVerifyTest, ExportMutableGlobal) {
  {
    static const byte data[] = {
        SECTION(Global,                         // --
                ENTRY_COUNT(1),                 // --
                kLocalI32,                      // local type
                0,                              // immutable
                WASM_INIT_EXPR_I32V_1(13)),     // init
        SECTION(Export,                         // --
                ENTRY_COUNT(1),                 // export count
                ADD_COUNT('n', 'a', 'm', 'e'),  // name
                kExternalGlobal,                // global
                0),                             // global index
    };
    EXPECT_VERIFIES(data);
  }
  {
    static const byte data[] = {
        SECTION(Global,                         // --
                ENTRY_COUNT(1),                 // --
                kLocalI32,                      // local type
                1,                              // mutable
                WASM_INIT_EXPR_I32V_1(13)),     // init
        SECTION(Export,                         // --
                ENTRY_COUNT(1),                 // export count
                ADD_COUNT('n', 'a', 'm', 'e'),  // name
                kExternalGlobal,                // global
                0),                             // global index
    };
    EXPECT_VERIFIES(data);
  }
}

static void AppendUint32v(std::vector<byte>* buffer, uint32_t val) {
  while (true) {
    uint32_t next = val >> 7;
    uint32_t out = val & 0x7F;
    if (next) {
      buffer->push_back(static_cast<byte>(0x80 | out));
      val = next;
    } else {
      buffer->push_back(static_cast<byte>(out));
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
    AppendUint32v(&buffer, i);  // Number of globals.
    for (uint32_t j = 0; j < i; j++) {
      buffer.insert(buffer.end(), data, data + sizeof(data));
    }

    ModuleResult result = DecodeModule(&buffer[0], &buffer[0] + buffer.size());
    EXPECT_OK(result);
  }
}

TEST_F(WasmModuleVerifyTest, GlobalWithInvalidMemoryType) {
  static const byte data[] = {SECTION(Global,          // --
                                      ENTRY_COUNT(1),  // --
                                      33,              // memory type
                                      0,               // exported
                                      WASM_INIT_EXPR_I32V_1(1))};

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, TwoGlobals) {
  static const byte data[] = {SECTION(Global,                      // --
                                      ENTRY_COUNT(2),              // --
                                      kLocalF32,                   // type
                                      0,                           // immutable
                                      WASM_INIT_EXPR_F32(22.0),    // --
                                      kLocalF64,                   // type
                                      1,                           // mutable
                                      WASM_INIT_EXPR_F64(23.0))};  // --

  {
    // Should decode to exactly two globals.
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(2u, result.value()->globals.size());
    EXPECT_EQ(0u, result.value()->functions.size());
    EXPECT_EQ(0u, result.value()->data_segments.size());

    const WasmGlobal* g0 = &result.value()->globals[0];

    EXPECT_EQ(kWasmF32, g0->type);
    EXPECT_EQ(0u, g0->offset);
    EXPECT_FALSE(g0->mutability);
    EXPECT_EQ(WasmInitExpr::kF32Const, g0->init.kind);

    const WasmGlobal* g1 = &result.value()->globals[1];

    EXPECT_EQ(kWasmF64, g1->type);
    EXPECT_EQ(8u, g1->offset);
    EXPECT_TRUE(g1->mutability);
    EXPECT_EQ(WasmInitExpr::kF64Const, g1->init.kind);
  }

  EXPECT_OFF_END_FAILURE(data, 1);
}

TEST_F(WasmModuleVerifyTest, ZeroExceptions) {
  static const byte data[] = {SECTION(Exception, ENTRY_COUNT(0))};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(0u, result.value()->exceptions.size());
}

TEST_F(WasmModuleVerifyTest, OneI32Exception) {
  static const byte data[] = {
      SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_x(kLocalI32)),  // sig#0 (i32)
      SECTION(Exception, ENTRY_COUNT(1),
              EXCEPTION_ENTRY(SIG_INDEX(0)))};  // except[0] (sig#0)
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.value()->exceptions.size());

  const WasmException& e0 = result.value()->exceptions.front();
  EXPECT_EQ(1u, e0.sig->parameter_count());
  EXPECT_EQ(kWasmI32, e0.sig->GetParam(0));
}

TEST_F(WasmModuleVerifyTest, TwoExceptions) {
  static const byte data[] = {
      SECTION(Type, ENTRY_COUNT(2),
              SIG_ENTRY_v_x(kLocalI32),               // sig#0 (i32)
              SIG_ENTRY_v_xx(kLocalF32, kLocalI64)),  // sig#1 (f32, i64)
      SECTION(Exception, ENTRY_COUNT(2),
              EXCEPTION_ENTRY(SIG_INDEX(1)),    // except[0] (sig#1)
              EXCEPTION_ENTRY(SIG_INDEX(0)))};  // except[1] (sig#0)
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(2u, result.value()->exceptions.size());
  const WasmException& e0 = result.value()->exceptions.front();
  EXPECT_EQ(2u, e0.sig->parameter_count());
  EXPECT_EQ(kWasmF32, e0.sig->GetParam(0));
  EXPECT_EQ(kWasmI64, e0.sig->GetParam(1));
  const WasmException& e1 = result.value()->exceptions.back();
  EXPECT_EQ(kWasmI32, e1.sig->GetParam(0));
}

TEST_F(WasmModuleVerifyTest, Exception_invalid_sig_index) {
  static const byte data[] = {
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Exception, ENTRY_COUNT(1),
              EXCEPTION_ENTRY(
                  SIG_INDEX(23)))};  // except[0] (sig#23 [out-of-bounds])
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  // Should fail decoding exception section.
  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "signature index 23 out of bounds");
}

TEST_F(WasmModuleVerifyTest, Exception_invalid_sig_return) {
  static const byte data[] = {
      SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_i_i),
      SECTION(Exception, ENTRY_COUNT(1),
              EXCEPTION_ENTRY(
                  SIG_INDEX(0)))};  // except[0] (sig#0 [invalid-return-type])
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  // Should fail decoding exception section.
  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "exception signature 0 has non-void return");
}

TEST_F(WasmModuleVerifyTest, Exception_invalid_attribute) {
  static const byte data[] = {
      SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_i_i),
      SECTION(Exception, ENTRY_COUNT(1), 23,
              SIG_INDEX(0))};  // except[0] (sig#0) [invalid-attribute]
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  // Should fail decoding exception section.
  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "exception attribute 23 not supported");
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionCorrectPlacement) {
  static const byte data[] = {SECTION(Memory, ENTRY_COUNT(0)),
                              SECTION(Exception, ENTRY_COUNT(0)),
                              SECTION(Global, ENTRY_COUNT(0))};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionAfterGlobal) {
  static const byte data[] = {SECTION(Global, ENTRY_COUNT(0)),
                              SECTION(Exception, ENTRY_COUNT(0))};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result,
                "The Exception section must appear before the Global section");
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionBeforeMemory) {
  static const byte data[] = {SECTION(Exception, ENTRY_COUNT(0)),
                              SECTION(Memory, ENTRY_COUNT(0))};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "unexpected section <Memory>");
}

TEST_F(WasmModuleVerifyTest, ExceptionSectionAfterTableBeforeMemory) {
  STATIC_ASSERT(kMemorySectionCode + 1 == kGlobalSectionCode);
  static const byte data[] = {SECTION(Table, ENTRY_COUNT(0)),
                              SECTION(Exception, ENTRY_COUNT(0)),
                              SECTION(Memory, ENTRY_COUNT(0))};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "unexpected section <Memory>");
}

TEST_F(WasmModuleVerifyTest, ExceptionImport) {
  static const byte data[] = {
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Import,                           // section header
              ENTRY_COUNT(1),                   // number of imports
              ADD_COUNT('m'),                   // module name
              ADD_COUNT('e', 'x'),              // exception name
              kExternalException,               // import kind
              EXCEPTION_ENTRY(SIG_INDEX(0)))};  // except[0] (sig#0)
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.value()->exceptions.size());
  EXPECT_EQ(1u, result.value()->import_table.size());
}

TEST_F(WasmModuleVerifyTest, ExceptionExport) {
  static const byte data[] = {
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Exception, ENTRY_COUNT(1),
              EXCEPTION_ENTRY(SIG_INDEX(0))),  // except[0] (sig#0)
      SECTION(Export, ENTRY_COUNT(1),          // --
              NO_NAME,                         // --
              kExternalException,              // --
              EXCEPTION_INDEX(0))};
  FAIL_IF_NO_EXPERIMENTAL_EH(data);

  WASM_FEATURE_SCOPE(eh);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.value()->exceptions.size());
  EXPECT_EQ(1u, result.value()->export_table.size());
}

TEST_F(WasmModuleVerifyTest, OneSignature) {
  {
    static const byte data[] = {SIGNATURES_SECTION_VOID_VOID};
    EXPECT_VERIFIES(data);
  }

  {
    static const byte data[] = {SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_i_i)};
    EXPECT_VERIFIES(data);
  }
}

TEST_F(WasmModuleVerifyTest, MultipleSignatures) {
  static const byte data[] = {
      SECTION(
          Type,                                              // --
          ENTRY_COUNT(3),                                    // --
          SIG_ENTRY_v_v,                                     // void -> void
          SIG_ENTRY_x_x(kLocalI32, kLocalF32),               // f32 -> i32
          SIG_ENTRY_x_xx(kLocalI32, kLocalF64, kLocalF64)),  // f64,f64 -> i32
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(3u, result.value()->signatures.size());
  if (result.value()->signatures.size() == 3) {
    EXPECT_EQ(0u, result.value()->signatures[0]->return_count());
    EXPECT_EQ(1u, result.value()->signatures[1]->return_count());
    EXPECT_EQ(1u, result.value()->signatures[2]->return_count());

    EXPECT_EQ(0u, result.value()->signatures[0]->parameter_count());
    EXPECT_EQ(1u, result.value()->signatures[1]->parameter_count());
    EXPECT_EQ(2u, result.value()->signatures[2]->parameter_count());
  }

  EXPECT_OFF_END_FAILURE(data, 1);
}

TEST_F(WasmModuleVerifyTest, DataSegmentWithImmutableImportedGlobal) {
  // Import 2 globals so that we can initialize data with a global index != 0.
  const byte data[] = {
      SECTION(Import,           // section header
              ENTRY_COUNT(2),   // number of imports
              ADD_COUNT('m'),   // module name
              ADD_COUNT('f'),   // global name
              kExternalGlobal,  // import kind
              kLocalI32,        // type
              0,                // mutability
              ADD_COUNT('n'),   // module name
              ADD_COUNT('g'),   // global name
              kExternalGlobal,  // import kind
              kLocalI32,        // type
              0),               // mutability
      SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_GLOBAL(1),  // dest addr
              U32V_1(3),                 // source size
              'a', 'b', 'c')             // data bytes
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  WasmInitExpr expr = result.value()->data_segments.back().dest_addr;
  EXPECT_EQ(WasmInitExpr::kGlobalIndex, expr.kind);
  EXPECT_EQ(1u, expr.val.global_index);
}

TEST_F(WasmModuleVerifyTest, DataSegmentWithMutableImportedGlobal) {
  // Only an immutable imported global can be used as an init_expr.
  const byte data[] = {
      SECTION(Import,           // section header
              ENTRY_COUNT(1),   // number of imports
              ADD_COUNT('m'),   // module name
              ADD_COUNT('f'),   // global name
              kExternalGlobal,  // import kind
              kLocalI32,        // type
              1),               // mutability
      SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_GLOBAL(0),  // dest addr
              U32V_1(3),                 // source size
              'a', 'b', 'c')             // data bytes
  };
  EXPECT_FAILURE(data);
}
TEST_F(WasmModuleVerifyTest, DataSegmentWithImmutableGlobal) {
  // Only an immutable imported global can be used as an init_expr.
  const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Global, ENTRY_COUNT(1),
              kLocalI32,                        // local type
              0,                                // immutable
              WASM_INIT_EXPR_I32V_3(0x9BBAA)),  // init
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_GLOBAL(0),  // dest addr
              U32V_1(3),                 // source size
              'a', 'b', 'c')             // data bytes
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, OneDataSegment) {
  const byte kDataSegmentSourceOffset = 24;
  const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_I32V_3(0x9BBAA),  // dest addr
              U32V_1(3),                       // source size
              'a', 'b', 'c')                   // data bytes
  };

  {
    EXPECT_VERIFIES(data);
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(0u, result.value()->globals.size());
    EXPECT_EQ(0u, result.value()->functions.size());
    EXPECT_EQ(1u, result.value()->data_segments.size());

    const WasmDataSegment* segment = &result.value()->data_segments.back();

    EXPECT_EQ(WasmInitExpr::kI32Const, segment->dest_addr.kind);
    EXPECT_EQ(0x9BBAA, segment->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegmentSourceOffset, segment->source.offset());
    EXPECT_EQ(3u, segment->source.length());
  }

  EXPECT_OFF_END_FAILURE(data, 14);
}

TEST_F(WasmModuleVerifyTest, TwoDataSegments) {
  const byte kDataSegment0SourceOffset = 24;
  const byte kDataSegment1SourceOffset = kDataSegment0SourceOffset + 11;

  const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Data,
              ENTRY_COUNT(2),  // segment count
              LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_I32V_3(0x7FFEE),  // #0: dest addr
              U32V_1(4),                       // source size
              1, 2, 3, 4,                      // data bytes
              LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_I32V_3(0x6DDCC),  // #1: dest addr
              U32V_1(10),                      // source size
              1, 2, 3, 4, 5, 6, 7, 8, 9, 10)   // data bytes
  };

  {
    ModuleResult result = DecodeModule(data, data + sizeof(data));
    EXPECT_OK(result);
    EXPECT_EQ(0u, result.value()->globals.size());
    EXPECT_EQ(0u, result.value()->functions.size());
    EXPECT_EQ(2u, result.value()->data_segments.size());

    const WasmDataSegment* s0 = &result.value()->data_segments[0];
    const WasmDataSegment* s1 = &result.value()->data_segments[1];

    EXPECT_EQ(WasmInitExpr::kI32Const, s0->dest_addr.kind);
    EXPECT_EQ(0x7FFEE, s0->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegment0SourceOffset, s0->source.offset());
    EXPECT_EQ(4u, s0->source.length());

    EXPECT_EQ(WasmInitExpr::kI32Const, s1->dest_addr.kind);
    EXPECT_EQ(0x6DDCC, s1->dest_addr.val.i32_const);
    EXPECT_EQ(kDataSegment1SourceOffset, s1->source.offset());
    EXPECT_EQ(10u, s1->source.length());
  }

  EXPECT_OFF_END_FAILURE(data, 14);
}

TEST_F(WasmModuleVerifyTest, DataWithoutMemory) {
  const byte data[] = {
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_I32V_3(0x9BBAA),  // dest addr
              U32V_1(3),                       // source size
              'a', 'b', 'c')                   // data bytes
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, MaxMaximumMemorySize) {
  {
    const byte data[] = {
        SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 0, U32V_3(65536))};
    EXPECT_VERIFIES(data);
  }
  {
    const byte data[] = {
        SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 0, U32V_3(65537))};
    EXPECT_FAILURE(data);
  }
}

TEST_F(WasmModuleVerifyTest, DataSegment_wrong_init_type) {
  const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,
              WASM_INIT_EXPR_F64(9.9),  // dest addr
              U32V_1(3),                // source size
              'a', 'b', 'c')            // data bytes
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, DataSegmentEndOverflow) {
  const byte data[] = {
      SECTION(Memory,  // memory section
              ENTRY_COUNT(1), kHasMaximumFlag, 28, 28),
      SECTION(Data,                      // data section
              ENTRY_COUNT(1),            // one entry
              LINEAR_MEMORY_INDEX_0,     // mem index
              WASM_INIT_EXPR_I32V_1(0),  // offset
              U32V_5(0xFFFFFFFF))        // size
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, OneIndirectFunction) {
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  if (result.ok()) {
    EXPECT_EQ(1u, result.value()->signatures.size());
    EXPECT_EQ(1u, result.value()->functions.size());
    EXPECT_EQ(1u, result.value()->tables.size());
    EXPECT_EQ(1u, result.value()->tables[0].initial_size);
  }
}

TEST_F(WasmModuleVerifyTest, ElementSectionWithInternalTable) {
  static const byte data[] = {
      // table ---------------------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // elements ------------------------------------------------------------
      SECTION(Element, ENTRY_COUNT(0))};

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionWithImportedTable) {
  static const byte data[] = {
      // imports -------------------------------------------------------------
      SECTION(Import, ENTRY_COUNT(1),
              ADD_COUNT('m'),  // module name
              ADD_COUNT('t'),  // table name
              kExternalTable,  // import kind
              kLocalFuncRef,   // elem_type
              0,               // no maximum field
              1),              // initial size
      // elements ------------------------------------------------------------
      SECTION(Element, ENTRY_COUNT(0))};

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionWithoutTable) {
  // Test that an element section without a table causes a validation error.
  static const byte data[] = {
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(1),  // entry count
              0,               // table index
              0,               // offset
              0)               // number of elements
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, Regression_735887) {
  // Test with an invalid function index in the element section.
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(1),  // entry count
              TABLE_INDEX0, WASM_INIT_EXPR_I32V_1(0),
              1,     // elements count
              0x9A)  // invalid I32V as function index
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, OneIndirectFunction_one_entry) {
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(1),  // entry count
              TABLE_INDEX0, WASM_INIT_EXPR_I32V_1(0),
              1,  // elements count
              FUNC_INDEX(0)),
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(1u, result.value()->signatures.size());
  EXPECT_EQ(1u, result.value()->functions.size());
  EXPECT_EQ(1u, result.value()->tables.size());
  EXPECT_EQ(1u, result.value()->tables[0].initial_size);
}

TEST_F(WasmModuleVerifyTest, MultipleIndirectFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SECTION(Type,
              ENTRY_COUNT(2),             // --
              SIG_ENTRY_v_v,              // void -> void
              SIG_ENTRY_v_x(kLocalI32)),  // void -> i32
      // funcs ------------------------------------------------------
      FOUR_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      // table declaration -------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 8),
      // table elements ----------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(1),  // entry count
              TABLE_INDEX0, WASM_INIT_EXPR_I32V_1(0),
              ADD_COUNT(FUNC_INDEX(0), FUNC_INDEX(1), FUNC_INDEX(2),
                        FUNC_INDEX(3), FUNC_INDEX(0), FUNC_INDEX(1),
                        FUNC_INDEX(2), FUNC_INDEX(3))),
      FOUR_EMPTY_BODIES};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);
  EXPECT_EQ(2u, result.value()->signatures.size());
  EXPECT_EQ(4u, result.value()->functions.size());
  EXPECT_EQ(1u, result.value()->tables.size());
  EXPECT_EQ(8u, result.value()->tables[0].initial_size);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMultipleTables) {
  // Test that if we have multiple tables, in the element section we can target
  // and initialize all tables.
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(2),  // section header
              kLocalFuncRef, 0, 5,    // table 0
              kLocalFuncRef, 0, 9),   // table 1
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(2),            // entry count
              TABLE_INDEX0,              // element for table 0
              WASM_INIT_EXPR_I32V_1(0),  // index
              1,                         // elements count
              FUNC_INDEX(0),             // function
              TABLE_INDEX(1),            // element for table 1
              WASM_INIT_EXPR_I32V_1(7),  // index
              kExternalFunction,         // type
              2,                         // elements count
              FUNC_INDEX(0),             // entry 0
              FUNC_INDEX(0)),            // entry 1
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY};

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMixedTables) {
  // Test that if we have multiple tables, both imported and module-defined, in
  // the element section we can target and initialize all tables.
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // imports -------------------------------------------------------------
      SECTION(Import, ENTRY_COUNT(2),
              ADD_COUNT('m'),  // module name
              ADD_COUNT('t'),  // table name
              kExternalTable,  // import kind
              kLocalFuncRef,   // elem_type
              0,               // no maximum field
              5,               // initial size
              ADD_COUNT('m'),  // module name
              ADD_COUNT('s'),  // table name
              kExternalTable,  // import kind
              kLocalFuncRef,   // elem_type
              0,               // no maximum field
              10),             // initial size
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(2),  // section header
              kLocalFuncRef, 0, 15,   // table 0
              kLocalFuncRef, 0, 19),  // table 1
      // elements ------------------------------------------------------------
      SECTION(Element,
              4,                          // entry count
              TABLE_INDEX0,               // element for table 0
              WASM_INIT_EXPR_I32V_1(0),   // index
              1,                          // elements count
              FUNC_INDEX(0),              // function
              TABLE_INDEX(1),             // element for table 1
              WASM_INIT_EXPR_I32V_1(7),   // index
              kExternalFunction,          // type
              2,                          // elements count
              FUNC_INDEX(0),              // entry 0
              FUNC_INDEX(0),              // entry 1
              TABLE_INDEX(2),             // element for table 2
              WASM_INIT_EXPR_I32V_1(12),  // index
              kExternalFunction,          // type
              1,                          // elements count
              FUNC_INDEX(0),              // function
              TABLE_INDEX(3),             // element for table 1
              WASM_INIT_EXPR_I32V_1(17),  // index
              kExternalFunction,          // type
              2,                          // elements count
              FUNC_INDEX(0),              // entry 0
              FUNC_INDEX(0)),             // entry 1
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY};

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMultipleTablesArbitraryOrder) {
  // Test that the order in which tables are targeted in the element secion
  // can be arbitrary.
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(2),  // section header
              kLocalFuncRef, 0, 5,    // table 0
              kLocalFuncRef, 0, 9),   // table 1
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(3),            // entry count
              TABLE_INDEX0,              // element for table 1
              WASM_INIT_EXPR_I32V_1(0),  // index
              1,                         // elements count
              FUNC_INDEX(0),             // function
              TABLE_INDEX(1),            // element for table 0
              WASM_INIT_EXPR_I32V_1(7),  // index
              kExternalFunction,         // type
              2,                         // elements count
              FUNC_INDEX(0),             // entry 0
              FUNC_INDEX(0),             // entry 1
              TABLE_INDEX0,              // element for table 1
              WASM_INIT_EXPR_I32V_1(3),  // index
              1,                         // elements count
              FUNC_INDEX(0)),            // function
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY};

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionMixedTablesArbitraryOrder) {
  // Test that the order in which tables are targeted in the element secion can
  // be arbitrary. In this test, tables can be both imported and module-defined.
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // imports -------------------------------------------------------------
      SECTION(Import, ENTRY_COUNT(2),
              ADD_COUNT('m'),  // module name
              ADD_COUNT('t'),  // table name
              kExternalTable,  // import kind
              kLocalFuncRef,   // elem_type
              0,               // no maximum field
              5,               // initial size
              ADD_COUNT('m'),  // module name
              ADD_COUNT('s'),  // table name
              kExternalTable,  // import kind
              kLocalFuncRef,   // elem_type
              0,               // no maximum field
              10),             // initial size
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(2),  // section header
              kLocalFuncRef, 0, 15,   // table 0
              kLocalFuncRef, 0, 19),  // table 1
      // elements ------------------------------------------------------------
      SECTION(Element,
              4,                          // entry count
              TABLE_INDEX(2),             // element for table 0
              WASM_INIT_EXPR_I32V_1(10),  // index
              kExternalFunction,          // type
              1,                          // elements count
              FUNC_INDEX(0),              // function
              TABLE_INDEX(3),             // element for table 1
              WASM_INIT_EXPR_I32V_1(17),  // index
              kExternalFunction,          // type
              2,                          // elements count
              FUNC_INDEX(0),              // entry 0
              FUNC_INDEX(0),              // entry 1
              TABLE_INDEX0,               // element for table 2
              WASM_INIT_EXPR_I32V_1(2),   // index
              1,                          // elements count
              FUNC_INDEX(0),              // function
              TABLE_INDEX(1),             // element for table 1
              WASM_INIT_EXPR_I32V_1(7),   // index
              kExternalFunction,          // type
              2,                          // elements count
              FUNC_INDEX(0),              // entry 0
              FUNC_INDEX(0)),             // entry 1
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY};

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionInitAnyRefTableWithFuncRef) {
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(2),  // section header
              kLocalAnyRef, 0, 5,     // table 0
              kLocalFuncRef, 0, 9),   // table 1
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(2),            // entry count
              TABLE_INDEX0,              // element for table 0
              WASM_INIT_EXPR_I32V_1(0),  // index
              1,                         // elements count
              FUNC_INDEX(0),             // function
              TABLE_INDEX(1),            // element for table 1
              WASM_INIT_EXPR_I32V_1(7),  // index
              kExternalFunction,         // type
              2,                         // elements count
              FUNC_INDEX(0),             // entry 0
              FUNC_INDEX(0)),            // entry 1
      // code ----------------------------------------------------------------
      ONE_EMPTY_BODY,
  };

  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ElementSectionDontInitAnyRefImportedTable) {
  // Test that imported tables of type AnyRef cannot be initialized in the
  // elements section.
  WASM_FEATURE_SCOPE(anyref);
  WASM_FEATURE_SCOPE(bulk_memory);
  static const byte data[] = {
      // sig#0 ---------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // imports -------------------------------------------------------------
      SECTION(Import, ENTRY_COUNT(2),
              ADD_COUNT('m'),  // module name
              ADD_COUNT('t'),  // table name
              kExternalTable,  // import kind
              kLocalFuncRef,   // elem_type
              0,               // no maximum field
              5,               // initial size
              ADD_COUNT('m'),  // module name
              ADD_COUNT('s'),  // table name
              kExternalTable,  // import kind
              kLocalAnyRef,    // elem_type
              0,               // no maximum field
              10),             // initial size
      // funcs ---------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration ---------------------------------------------------
      SECTION(Table, ENTRY_COUNT(2),  // section header
              kLocalFuncRef, 0, 15,   // table 0
              kLocalFuncRef, 0, 19),  // table 1
      // elements ------------------------------------------------------------
      SECTION(Element,
              ENTRY_COUNT(4),             // entry count
              TABLE_INDEX0,               // element for table 0
              WASM_INIT_EXPR_I32V_1(10),  // index
              1,                          // elements count
              FUNC_INDEX(0),              // function
              TABLE_INDEX(1),             // element for table 1
              WASM_INIT_EXPR_I32V_1(17),  // index
              kExternalFunction,          // type
              2,                          // elements count
              FUNC_INDEX(0),              // entry 0
              FUNC_INDEX(0)),             // entry 1
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, IndirectFunctionNoFunctions) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // indirect table ----------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), 1, 0, 0)};

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, IndirectFunctionInvalidIndex) {
  static const byte data[] = {
      // sig#0 -------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // functions ---------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // indirect table ----------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), 1, 1, 0)};

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, MultipleTablesWithoutFlag) {
  static const byte data[] = {
      SECTION(Table,           // table section
              ENTRY_COUNT(2),  // 2 tables
              kLocalFuncRef,   // table 1: type
              0,               // table 1: no maximum
              10,              // table 1: minimum size
              kLocalFuncRef,   // table 2: type
              0,               // table 2: no maximum
              10),             // table 2: minimum size
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, MultipleTablesWithFlag) {
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      SECTION(Table,           // table section
              ENTRY_COUNT(2),  // 2 tables
              kLocalFuncRef,   // table 1: type
              0,               // table 1: no maximum
              10,              // table 1: minimum size
              kLocalAnyRef,    // table 2: type
              0,               // table 2: no maximum
              11),             // table 2: minimum size
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(2u, result.value()->tables.size());

  EXPECT_EQ(10u, result.value()->tables[0].initial_size);
  EXPECT_EQ(kWasmFuncRef, result.value()->tables[0].type);

  EXPECT_EQ(11u, result.value()->tables[1].initial_size);
  EXPECT_EQ(kWasmAnyRef, result.value()->tables[1].type);
}

TEST_F(WasmModuleVerifyTest, TieringCompilationHints) {
  WASM_FEATURE_SCOPE(compilation_hints);
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),
      FUNCTION_SIGNATURES_SECTION(3, 0, 0, 0),
      SECTION_COMPILATION_HINTS(
          BASELINE_TIER_INTERPRETER | TOP_TIER_BASELINE,
          BASELINE_TIER_BASELINE | TOP_TIER_OPTIMIZED,
          BASELINE_TIER_INTERPRETER | TOP_TIER_INTERPRETER),
      SECTION(Code, ENTRY_COUNT(3), NOP_BODY, NOP_BODY, NOP_BODY),
  };

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(3u, result.value()->compilation_hints.size());
  EXPECT_EQ(WasmCompilationHintStrategy::kDefault,
            result.value()->compilation_hints[0].strategy);
  EXPECT_EQ(WasmCompilationHintTier::kInterpreter,
            result.value()->compilation_hints[0].baseline_tier);
  EXPECT_EQ(WasmCompilationHintTier::kBaseline,
            result.value()->compilation_hints[0].top_tier);
  EXPECT_EQ(WasmCompilationHintStrategy::kDefault,
            result.value()->compilation_hints[1].strategy);
  EXPECT_EQ(WasmCompilationHintTier::kBaseline,
            result.value()->compilation_hints[1].baseline_tier);
  EXPECT_EQ(WasmCompilationHintTier::kOptimized,
            result.value()->compilation_hints[1].top_tier);
  EXPECT_EQ(WasmCompilationHintStrategy::kDefault,
            result.value()->compilation_hints[2].strategy);
  EXPECT_EQ(WasmCompilationHintTier::kInterpreter,
            result.value()->compilation_hints[2].baseline_tier);
  EXPECT_EQ(WasmCompilationHintTier::kInterpreter,
            result.value()->compilation_hints[2].top_tier);
}

class WasmSignatureDecodeTest : public TestWithZone {
 public:
  WasmFeatures enabled_features_ = WasmFeatures::None();

  const FunctionSig* DecodeSig(const byte* start, const byte* end) {
    return DecodeWasmSignatureForTesting(enabled_features_, zone(), start, end);
  }
};

TEST_F(WasmSignatureDecodeTest, Ok_v_v) {
  static const byte data[] = {SIG_ENTRY_v_v};
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  const FunctionSig* sig = DecodeSig(data, data + sizeof(data));

  EXPECT_TRUE(sig != nullptr);
  EXPECT_EQ(0u, sig->parameter_count());
  EXPECT_EQ(0u, sig->return_count());
}

TEST_F(WasmSignatureDecodeTest, Ok_t_v) {
  WASM_FEATURE_SCOPE(anyref);
  for (size_t i = 0; i < arraysize(kValueTypes); i++) {
    ValueTypePair ret_type = kValueTypes[i];
    const byte data[] = {SIG_ENTRY_x(ret_type.code)};
    const FunctionSig* sig = DecodeSig(data, data + sizeof(data));

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
    const FunctionSig* sig = DecodeSig(data, data + sizeof(data));

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
      const FunctionSig* sig = DecodeSig(data, data + sizeof(data));

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
      const FunctionSig* sig = DecodeSig(data, data + sizeof(data));

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
      const FunctionSig* sig = DecodeSig(data, data + sizeof(data));

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
  const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
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
    const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
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
      const FunctionSig* sig = DecodeSig(data, data + i);
      EXPECT_EQ(nullptr, sig);
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_anyref_without_flag) {
  // Disable AnyRef support and check that decoding fails.
  WASM_FEATURE_SCOPE_VAL(anyref, false);
  byte ref_types[] = {kLocalFuncRef, kLocalAnyRef, kLocalNullRef};
  for (byte invalid_type : ref_types) {
    for (size_t i = 0;; i++) {
      byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalI32)};
      if (i >= arraysize(data)) break;
      data[i] = invalid_type;
      const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
      EXPECT_EQ(nullptr, sig);
    }
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_type) {
  byte kInvalidType = 76;
  for (size_t i = 0;; i++) {
    byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalI32)};
    if (i >= arraysize(data)) break;
    data[i] = kInvalidType;
    const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
    EXPECT_EQ(nullptr, sig);
  }
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_ret_type1) {
  static const byte data[] = {SIG_ENTRY_x_x(kLocalVoid, kLocalI32)};
  const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type1) {
  static const byte data[] = {SIG_ENTRY_x_x(kLocalI32, kLocalVoid)};
  const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

TEST_F(WasmSignatureDecodeTest, Fail_invalid_param_type2) {
  static const byte data[] = {SIG_ENTRY_x_xx(kLocalI32, kLocalI32, kLocalVoid)};
  const FunctionSig* sig = DecodeSig(data, data + sizeof(data));
  EXPECT_EQ(nullptr, sig);
}

class WasmFunctionVerifyTest : public TestWithIsolateAndZone {
 public:
  FunctionResult DecodeWasmFunction(const ModuleWireBytes& wire_bytes,
                                    const WasmModule* module,
                                    const byte* function_start,
                                    const byte* function_end) {
    WasmFeatures enabled_features;
    return DecodeWasmFunctionForTesting(enabled_features, zone(), wire_bytes,
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

  WasmModule module;
  FunctionResult result = DecodeWasmFunction(ModuleWireBytes({}), &module, data,
                                             data + sizeof(data));
  EXPECT_OK(result);

  if (result.value() && result.ok()) {
    WasmFunction* function = result.value().get();
    EXPECT_EQ(0u, function->sig->parameter_count());
    EXPECT_EQ(0u, function->sig->return_count());
    EXPECT_EQ(COUNT_ARGS(SIG_ENTRY_v_v), function->code.offset());
    EXPECT_EQ(sizeof(data), function->code.end_offset());
    // TODO(titzer): verify encoding of local declarations
  }
}

TEST_F(WasmModuleVerifyTest, SectionWithoutNameLength) {
  const byte data[] = {1};
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, EmptyCustomSectionIsInvalid) {
  // An empty custom section is invalid, because at least one byte for the
  // length of the custom section name is required.
  const byte data[] = {
      0,  // unknown section code.
      0   // section length.
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, TheLoneliestOfValidModulesTheTrulyEmptyOne) {
  const byte data[] = {
      0,  // unknown section code.
      1,  // section length, only one byte for the name length.
      0,  // string length of 0.
          // Empty section name, no content, nothing but sadness.
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
      SECTION(Global, ENTRY_COUNT(1),
              kLocalI32,                   // memory type
              0,                           // exported
              WASM_INIT_EXPR_I32V_1(33)),  // init
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1u, result.value()->globals.size());
  EXPECT_EQ(0u, result.value()->functions.size());
  EXPECT_EQ(0u, result.value()->data_segments.size());

  const WasmGlobal* global = &result.value()->globals.back();

  EXPECT_EQ(kWasmI32, global->type);
  EXPECT_EQ(0u, global->offset);
}

TEST_F(WasmModuleVerifyTest, ImportTable_empty) {
  static const byte data[] = {SECTION(Type, ENTRY_COUNT(0)),
                              SECTION(Import, ENTRY_COUNT(0))};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs1) {
  static const byte data[] = {SECTION(Import, ENTRY_COUNT(0))};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_mutable_global) {
  {
    static const byte data[] = {
        SECTION(Import,           // section header
                ENTRY_COUNT(1),   // number of imports
                ADD_COUNT('m'),   // module name
                ADD_COUNT('f'),   // global name
                kExternalGlobal,  // import kind
                kLocalI32,        // type
                0),               // mutability
    };
    EXPECT_VERIFIES(data);
  }
  {
    static const byte data[] = {
        SECTION(Import,           // section header
                ENTRY_COUNT(1),   // sig table
                ADD_COUNT('m'),   // module name
                ADD_COUNT('f'),   // global name
                kExternalGlobal,  // import kind
                kLocalI32,        // type
                1),               // mutability
    };
    EXPECT_VERIFIES(data);
  }
}

TEST_F(WasmModuleVerifyTest, ImportTable_mutability_malformed) {
  static const byte data[] = {
      SECTION(Import,
              ENTRY_COUNT(1),   // --
              ADD_COUNT('m'),   // module name
              ADD_COUNT('g'),   // global name
              kExternalGlobal,  // import kind
              kLocalI32,        // type
              2),               // invalid mutability
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_nosigs2) {
  static const byte data[] = {
      SECTION(Import, ENTRY_COUNT(1),  // sig table
              ADD_COUNT('m'),          // module name
              ADD_COUNT('f'),          // function name
              kExternalFunction,       // import kind
              SIG_INDEX(0)),           // sig index
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_sig) {
  static const byte data[] = {
      SECTION(Type, ENTRY_COUNT(0)),   // --
      SECTION(Import, ENTRY_COUNT(1),  // --
              ADD_COUNT('m'),          // module name
              ADD_COUNT('f'),          // function name
              kExternalFunction,       // import kind
              SIG_INDEX(0)),           // sig index
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_one_sig) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Import,
              ENTRY_COUNT(1),     // --
              ADD_COUNT('m'),     // module name
              ADD_COUNT('f'),     // function name
              kExternalFunction,  // import kind
              SIG_INDEX(0)),      // sig index
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_invalid_module) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,  // --
      SECTION(Import,                // --
              ENTRY_COUNT(1),        // --
              NO_NAME,               // module name
              ADD_COUNT('f'),        // function name
              kExternalFunction,     // import kind
              SIG_INDEX(0),          // sig index
              0),                    // auxiliary data
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ImportTable_off_end) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID,
      SECTION(Import, ENTRY_COUNT(1),
              ADD_COUNT('m'),      // module name
              ADD_COUNT('f'),      // function name
              kExternalFunction),  // import kind
      SIG_INDEX(0),                // sig index (outside import section!)
  };

  EXPECT_OFF_END_FAILURE(data, arraysize(data) - 3);
}

TEST_F(WasmModuleVerifyTest, ExportTable_empty1) {
  static const byte data[] = {                               // signatures
                              SIGNATURES_SECTION_VOID_VOID,  // --
                              ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
                              SECTION(Export, ENTRY_COUNT(0)),  // --
                              ONE_EMPTY_BODY};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1u, result.value()->functions.size());
  EXPECT_EQ(0u, result.value()->export_table.size());
}

TEST_F(WasmModuleVerifyTest, ExportTable_empty2) {
  static const byte data[] = {SECTION(Type, ENTRY_COUNT(0)),
                              SECTION(Export, ENTRY_COUNT(0))};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTable_NoFunctions2) {
  static const byte data[] = {SECTION(Export, ENTRY_COUNT(0))};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableOne) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID, ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      SECTION(Export,
              ENTRY_COUNT(1),     // exports
              NO_NAME,            // --
              kExternalFunction,  // --
              FUNC_INDEX(0)),     // --
      ONE_EMPTY_BODY};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1u, result.value()->functions.size());
  EXPECT_EQ(1u, result.value()->export_table.size());
}

TEST_F(WasmModuleVerifyTest, ExportNameWithInvalidStringLength) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID, ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      SECTION(Export,
              ENTRY_COUNT(1),     // exports
              U32V_1(84),         // invalid string length
              'e',                // --
              kExternalFunction,  // --
              FUNC_INDEX(0),      // --
              0, 0, 0)            // auxiliary data
  };

  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, ExportTableTwo) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID, ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      SECTION(Export,
              ENTRY_COUNT(2),                 // exports
              ADD_COUNT('n', 'a', 'm', 'e'),  // --
              kExternalFunction,              // --
              FUNC_INDEX(0),                  // --
              ADD_COUNT('n', 'o', 'm'),       // --
              kExternalFunction,              // --
              FUNC_INDEX(0)),                 // --
      ONE_EMPTY_BODY};

  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(1u, result.value()->functions.size());
  EXPECT_EQ(2u, result.value()->export_table.size());
}

TEST_F(WasmModuleVerifyTest, ExportTableThree) {
  static const byte data[] = {
      // signatures
      SIGNATURES_SECTION_VOID_VOID, THREE_EMPTY_FUNCTIONS(SIG_INDEX(0)),
      SECTION(Export,
              ENTRY_COUNT(3),  // exports
              ADD_COUNT('a'),  // --
              kExternalFunction,
              FUNC_INDEX(0),   // --
              ADD_COUNT('b'),  // --
              kExternalFunction,
              FUNC_INDEX(1),   // --
              ADD_COUNT('c'),  // --
              kExternalFunction,
              FUNC_INDEX(2)),  // --
      THREE_EMPTY_BODIES};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_OK(result);

  EXPECT_EQ(3u, result.value()->functions.size());
  EXPECT_EQ(3u, result.value()->export_table.size());
}

TEST_F(WasmModuleVerifyTest, ExportTableThreeOne) {
  for (int i = 0; i < 6; i++) {
    const byte data[] = {
        // signatures
        SIGNATURES_SECTION_VOID_VOID, THREE_EMPTY_FUNCTIONS(SIG_INDEX(0)),
        SECTION(Export,
                ENTRY_COUNT(1),       // exports
                ADD_COUNT('e', 'x'),  // --
                kExternalFunction,
                FUNC_INDEX(i)),  // --
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
      SIGNATURES_SECTION_VOID_VOID, ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      SECTION(Export,
              ENTRY_COUNT(1),  // exports
              NO_NAME,         // --
              kExternalFunction,
              FUNC_INDEX(0),  // --
              0, 0, 0)        // auxiliary data
  };

  EXPECT_OFF_END_FAILURE(data, arraysize(data) - 3);
}

TEST_F(WasmModuleVerifyTest, Regression_648070) {
  static const byte data[] = {
      SECTION(Type, ENTRY_COUNT(0)),         // --
      SECTION(Function, U32V_5(3500228624))  // function count = 3500228624
  };                                         // --
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, Regression_738097) {
  // The function body size caused an integer overflow in the module decoder.
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
      FUNCTION_SIGNATURES_SECTION(1, 0),     // --
      SECTION(Code,                          // --
              ENTRY_COUNT(1),                // --
              U32V_5(0xFFFFFFFF),            // function size,
              0)                             // No real body
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
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
      FUNCTION_SIGNATURES_SECTION(1, 0),     // --
      ONE_EMPTY_BODY                         // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_one_nop) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),    // --
      FUNCTION_SIGNATURES_SECTION(1, 0),       // --
      SECTION(Code, ENTRY_COUNT(1), NOP_BODY)  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_count_mismatch1) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // --
      FUNCTION_SIGNATURES_SECTION(2, 0, 0),  // --
      ONE_EMPTY_BODY                         // --
  };
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, FunctionBodies_count_mismatch2) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),              // --
      FUNCTION_SIGNATURES_SECTION(1, 0),                 // --
      SECTION(Code, ENTRY_COUNT(2), NOP_BODY, NOP_BODY)  // --
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
  // TODO(wasm): This test does not test anything (corrupt name section does not
  // fail validation).
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),                      // --
      FUNCTION_SIGNATURES_SECTION(1, 0),                         // --
      ONE_EMPTY_BODY,                                            // --
      SECTION_NAMES(ENTRY_COUNT(1), FOO_STRING, NO_LOCAL_NAMES)  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Names_two_empty) {
  // TODO(wasm): This test does not test anything (corrupt name section does not
  // fail validation).
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),       // --
      FUNCTION_SIGNATURES_SECTION(2, 0, 0),       // --
      TWO_EMPTY_BODIES,                           // --
      SECTION_NAMES(ENTRY_COUNT(2),               // --
                    FOO_STRING, NO_LOCAL_NAMES,   // --
                    FOO_STRING, NO_LOCAL_NAMES),  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Regression684855) {
  static const byte data[] = {
      SECTION_NAMES(0xFB,  // functions count
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
                    0x00)  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, FunctionSectionWithoutCodeSection) {
  static const byte data[] = {
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // Type section.
      FUNCTION_SIGNATURES_SECTION(1, 0),     // Function section.
  };
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "function count is 1, but code section is absent");
}

TEST_F(WasmModuleVerifyTest, CodeSectionWithoutFunctionSection) {
  static const byte data[] = {ONE_EMPTY_BODY};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "function body count 1 mismatch (0 expected)");
}

TEST_F(WasmModuleVerifyTest, EmptyFunctionSectionWithoutCodeSection) {
  static const byte data[] = {SECTION(Function, ENTRY_COUNT(0))};
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, EmptyCodeSectionWithoutFunctionSection) {
  static const byte data[] = {SECTION(Code, ENTRY_COUNT(0))};
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
  EXPECT_EQ(WasmInitExpr::kRefNullConst, expr.kind);
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
      SECTION(Unknown, ADD_COUNT('X'), 17, 18),                    // --
      SECTION(Unknown, ADD_COUNT('f', 'o', 'o'), 5, 6, 7, 8, 9),   // --
      SECTION(Unknown, ADD_COUNT('o', 't', 'h', 'e', 'r'), 7, 8),  // --
  };
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, Section_Name_No_UTF8) {
  static const byte data[] = {SECTION(Unknown, 1, 0xFF, 17, 18)};
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
      U32_LE(kWasmMagic),                                  // --
      U32_LE(kWasmVersion),                                // --
      SECTION(Unknown, 1, 'X', 17, 18),                    // --
      SECTION(Unknown, 3, 'f', 'o', 'o', 5, 6, 7, 8, 9),   // --
      SECTION(Unknown, 5, 'o', 't', 'h', 'e', 'r', 7, 8),  // --
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
      U32_LE(kWasmMagic),                                          // --
      U32_LE(kWasmVersion),                                        // --
      SIGNATURES_SECTION(2, SIG_ENTRY_v_v, SIG_ENTRY_v_v),         // --
      SECTION(Unknown, ADD_COUNT('X'), 17, 18),                    // --
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),                            // --
      SECTION(Unknown, ADD_COUNT('o', 't', 'h', 'e', 'r'), 7, 8),  // --
  };

  static const CustomSectionOffset expected[] = {
      // section, name, payload
      {{19, 4}, {20, 1}, {21, 2}},  // --
      {{29, 8}, {30, 5}, {35, 2}},  // --
  };

  CheckSections(data, data + sizeof(data), expected, arraysize(expected));
}

TEST_F(WasmModuleVerifyTest, SourceMappingURLSection) {
  static const byte data[] = {
      SECTION_SRC_MAP('s', 'r', 'c', '/', 'x', 'y', 'z', '.', 'c')};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("src/xyz.c", result.value()->source_map_url);
}

TEST_F(WasmModuleVerifyTest, BadSourceMappingURLSection) {
  static const byte data[] = {
      SECTION_SRC_MAP('s', 'r', 'c', '/', 'x', 0xff, 'z', '.', 'c')};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(0u, result.value()->source_map_url.size());
}

TEST_F(WasmModuleVerifyTest, MultipleSourceMappingURLSections) {
  static const byte data[] = {SECTION_SRC_MAP('a', 'b', 'c'),
                              SECTION_SRC_MAP('p', 'q', 'r')};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("abc", result.value()->source_map_url);
}

TEST_F(WasmModuleVerifyTest, MultipleNameSections) {
  static const byte data[] = {
      SECTION_NAMES(0, ADD_COUNT(ADD_COUNT('a', 'b', 'c'))),
      SECTION_NAMES(0, ADD_COUNT(ADD_COUNT('p', 'q', 'r', 's')))};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(3u, result.value()->name.length());
}

TEST_F(WasmModuleVerifyTest, BadNameSection) {
  static const byte data[] = {SECTION_NAMES(
      0, ADD_COUNT(ADD_COUNT('s', 'r', 'c', '/', 'x', 0xff, 'z', '.', 'c')))};
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(0u, result.value()->name.length());
}

TEST_F(WasmModuleVerifyTest, PassiveDataSegment) {
  static const byte data[] = {
      // memory declaration ----------------------------------------------------
      SECTION(Memory, ENTRY_COUNT(1), 0, 1),
      // data segments  --------------------------------------------------------
      SECTION(Data, ENTRY_COUNT(1), PASSIVE, ADD_COUNT('h', 'i')),
  };
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_VERIFIES(data);
  EXPECT_OFF_END_FAILURE(data, arraysize(data) - 5);
}

TEST_F(WasmModuleVerifyTest, ActiveElementSegmentWithElements) {
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration -----------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // element segments  -----------------------------------------------------
      SECTION(Element, ENTRY_COUNT(1), ACTIVE_WITH_ELEMENTS, TABLE_INDEX0,
              WASM_INIT_EXPR_I32V_1(0), kLocalFuncRef, U32V_1(3),
              REF_FUNC_ELEMENT(0), REF_FUNC_ELEMENT(0), REF_NULL_ELEMENT),
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_VERIFIES(data);
  EXPECT_OFF_END_FAILURE(data, arraysize(data) - 5);
}

TEST_F(WasmModuleVerifyTest, PassiveElementSegment) {
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration -----------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // element segments  -----------------------------------------------------
      SECTION(Element, ENTRY_COUNT(1), PASSIVE_WITH_ELEMENTS, kLocalFuncRef,
              U32V_1(3), REF_FUNC_ELEMENT(0), REF_FUNC_ELEMENT(0),
              REF_NULL_ELEMENT),
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_VERIFIES(data);
  EXPECT_OFF_END_FAILURE(data, arraysize(data) - 5);
}

TEST_F(WasmModuleVerifyTest, PassiveElementSegmentAnyRef) {
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration -----------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // element segments  -----------------------------------------------------
      SECTION(Element, ENTRY_COUNT(1), PASSIVE_WITH_ELEMENTS, kLocalAnyRef,
              U32V_1(0)),
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_FAILURE(data);
}

TEST_F(WasmModuleVerifyTest, PassiveElementSegmentWithIndices) {
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // table declaration -----------------------------------------------------
      SECTION(Table, ENTRY_COUNT(1), kLocalFuncRef, 0, 1),
      // element segments ------------------------------------------------------
      SECTION(Element, ENTRY_COUNT(1), PASSIVE, kExternalFunction,
              ENTRY_COUNT(3), U32V_1(0), U32V_1(0), U32V_1(0)),
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_VERIFIES(data);
  EXPECT_OFF_END_FAILURE(data, arraysize(data) - 5);
}

TEST_F(WasmModuleVerifyTest, DeclarativeElementSegmentFuncRef) {
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // element segments  -----------------------------------------------------
      SECTION(Element,                    // section name
              ENTRY_COUNT(1),             // entry count
              DECLARATIVE_WITH_ELEMENTS,  // flags
              kLocalFuncRef,              // local type
              U32V_1(0)),                 // func ref count
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(anyref);
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, DeclarativeElementSegmentWithInvalidIndex) {
  WASM_FEATURE_SCOPE(bulk_memory);
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // element segments  -----------------------------------------------------
      SECTION(Element,            // section name
              ENTRY_COUNT(1),     // entry count
              DECLARATIVE,        // flags
              kExternalFunction,  // type
              ENTRY_COUNT(2),     // func index count
              U32V_1(0),          // func index
              U32V_1(1)),         // func index
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  EXPECT_FAILURE_WITH_MSG(data, "element function index 1 out of bounds");
}

TEST_F(WasmModuleVerifyTest, DeclarativeElementSegmentMissingForGlobal) {
  WASM_FEATURE_SCOPE(bulk_memory);
  WASM_FEATURE_SCOPE(anyref);
  static const byte data[] = {
      // sig#0 -----------------------------------------------------------------
      SIGNATURES_SECTION_VOID_VOID,
      // funcs -----------------------------------------------------------------
      ONE_EMPTY_FUNCTION(SIG_INDEX(0)),
      // global definitions ----------------------------------------------------
      SECTION(Global,                       // section name
              ENTRY_COUNT(1),               // entry count
              kLocalAnyRef,                 // local type
              0,                            // immutable
              WASM_INIT_EXPR_REF_FUNC(0)),  // init
      // code ------------------------------------------------------------------
      ONE_EMPTY_BODY};
  EXPECT_FAILURE_WITH_MSG(data, "undeclared reference to function");
}

TEST_F(WasmModuleVerifyTest, DataCountSectionCorrectPlacement) {
  static const byte data[] = {SECTION(Element, ENTRY_COUNT(0)),
                              SECTION(DataCount, ENTRY_COUNT(0)),
                              SECTION(Code, ENTRY_COUNT(0))};
  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, DataCountSectionAfterCode) {
  static const byte data[] = {SECTION(Code, ENTRY_COUNT(0)),
                              SECTION(DataCount, ENTRY_COUNT(0))};
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result,
                "The DataCount section must appear before the Code section");
}

TEST_F(WasmModuleVerifyTest, DataCountSectionBeforeElement) {
  static const byte data[] = {SECTION(DataCount, ENTRY_COUNT(0)),
                              SECTION(Element, ENTRY_COUNT(0))};
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "unexpected section <Element>");
}

TEST_F(WasmModuleVerifyTest, DataCountSectionAfterStartBeforeElement) {
  STATIC_ASSERT(kStartSectionCode + 1 == kElementSectionCode);
  static const byte data[] = {
      // We need the start section for this test, but the start section must
      // reference a valid function, which requires the type and function
      // sections too.
      SIGNATURES_SECTION(1, SIG_ENTRY_v_v),  // Type section.
      FUNCTION_SIGNATURES_SECTION(1, 0),     // Function section.

      SECTION(Start, U32V_1(0)),           // Start section.
      SECTION(DataCount, ENTRY_COUNT(0)),  // DataCount section.
      SECTION(Element, ENTRY_COUNT(0))     // Element section.
  };
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "unexpected section <Element>");
}

TEST_F(WasmModuleVerifyTest, MultipleDataCountSections) {
  static const byte data[] = {SECTION(DataCount, ENTRY_COUNT(0)),
                              SECTION(DataCount, ENTRY_COUNT(0))};
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "Multiple DataCount sections not allowed");
}

TEST_F(WasmModuleVerifyTest, DataCountSegmentCountMatch) {
  static const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), 0, 1),  // Memory section.
      SECTION(DataCount, ENTRY_COUNT(1)),     // DataCount section.
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,  // Data section.
              WASM_INIT_EXPR_I32V_1(12), ADD_COUNT('h', 'i'))};

  EXPECT_FAILURE(data);
  WASM_FEATURE_SCOPE(bulk_memory);
  EXPECT_VERIFIES(data);
}

TEST_F(WasmModuleVerifyTest, DataCountSegmentCount_greater) {
  static const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), 0, 1),  // Memory section.
      SECTION(DataCount, ENTRY_COUNT(3)),     // DataCount section.
      SECTION(Data, ENTRY_COUNT(0))};         // Data section.
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "data segments count 0 mismatch (3 expected)");
}

TEST_F(WasmModuleVerifyTest, DataCountSegmentCount_less) {
  static const byte data[] = {
      SECTION(Memory, ENTRY_COUNT(1), 0, 1),  // Memory section.
      SECTION(DataCount, ENTRY_COUNT(0)),     // DataCount section.
      SECTION(Data, ENTRY_COUNT(1), LINEAR_MEMORY_INDEX_0,  // Data section.
              WASM_INIT_EXPR_I32V_1(12), ADD_COUNT('a', 'b', 'c'))};
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "data segments count 1 mismatch (0 expected)");
}

TEST_F(WasmModuleVerifyTest, DataCountSegmentCount_omitted) {
  static const byte data[] = {SECTION(Memory, ENTRY_COUNT(1), 0, 1),
                              SECTION(DataCount, ENTRY_COUNT(1))};
  WASM_FEATURE_SCOPE(bulk_memory);
  ModuleResult result = DecodeModule(data, data + sizeof(data));
  EXPECT_NOT_OK(result, "data segments count 0 mismatch (1 expected)");
}

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
#undef WASM_INIT_EXPR_REF_NULL
#undef WASM_INIT_EXPR_REF_FUNC
#undef WASM_INIT_EXPR_GLOBAL
#undef REF_NULL_ELEMENT
#undef REF_FUNC_ELEMENT
#undef EMPTY_BODY
#undef NOP_BODY
#undef SIG_ENTRY_i_i
#undef UNKNOWN_SECTION
#undef COUNT_ARGS
#undef CHECK_LEB1
#undef ADD_COUNT
#undef SECTION
#undef SIGNATURES_SECTION
#undef FUNCTION_SIGNATURES_SECTION
#undef FOO_STRING
#undef NO_LOCAL_NAMES
#undef EMPTY_SIGNATURES_SECTION
#undef EMPTY_FUNCTION_SIGNATURES_SECTION
#undef EMPTY_FUNCTION_BODIES_SECTION
#undef SECTION_NAMES
#undef EMPTY_NAMES_SECTION
#undef SECTION_SRC_MAP
#undef SECTION_COMPILATION_HINTS
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
#undef EXCEPTION_ENTRY
#undef EXPECT_VERIFIES
#undef EXPECT_FAILURE_LEN
#undef EXPECT_FAILURE
#undef EXPECT_OFF_END_FAILURE
#undef EXPECT_OK
#undef EXPECT_NOT_OK

}  // namespace module_decoder_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8

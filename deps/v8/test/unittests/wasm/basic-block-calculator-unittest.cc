// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/basic-block-calculator.h"

#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/pgo.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm {

namespace {
class TestWireBytesStorage final : public WireBytesStorage {
 public:
  explicit TestWireBytesStorage(base::OwnedVector<const uint8_t>& wire_bytes)
      : wire_bytes_(base::OwnedCopyOf(wire_bytes)) {}

  base::Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
    return wire_bytes_.as_vector().SubVector(ref.offset(), ref.end_offset());
  }

  std::optional<ModuleWireBytes> GetModuleBytes() const final {
    return std::optional<ModuleWireBytes>(wire_bytes_.as_vector());
  }

 private:
  const base::OwnedVector<const uint8_t> wire_bytes_;
};

}  // namespace

class WasmBasicBlockCalculatorTest : public TestWithIsolateAndZone {
 public:
  WasmBasicBlockCalculatorTest() : builder_(zone()) {}

 protected:
  std::shared_ptr<NativeModule> CompileNativeModule(
      base::OwnedVector<const uint8_t>& module_bytes,
      WasmDetectedFeatures* detected_features) {
    ModuleResult result =
        DecodeWasmModule(WasmEnabledFeatures::All(), module_bytes.as_vector(),
                         false, kWasmOrigin, detected_features);
    CHECK(result.ok());

    ErrorThrower thrower(isolate(), "");
    constexpr int kNoCompilationId = 0;
    constexpr ProfileInformation* kNoProfileInformation = nullptr;
    std::shared_ptr<NativeModule> native_module = CompileToNativeModule(
        isolate(), WasmEnabledFeatures::All(), *detected_features,
        CompileTimeImports{}, &thrower, std::move(result).value(),
        std::move(module_bytes), kNoCompilationId,
        v8::metrics::Recorder::ContextId::Empty(), kNoProfileInformation);
    CHECK(!thrower.error());
    CHECK_NOT_NULL(native_module);

    return native_module;
  }

  bool DoCoverageResultsMatchExpectations(
      const base::Vector<const WasmCodeRange> result,
      const std::vector<WasmCodeRange>& expectation) {
    if (result.size() != expectation.size()) return false;
    for (size_t i = 0; i < result.size(); i++) {
      if ((result[i].start != expectation[i].start) ||
          (result[i].end != expectation[i].end)) {
        return false;
      }
    }
    return true;
  }

  void TestFunction(const FunctionSig* sig,
                    const std::vector<uint8_t>& code_bytes,
                    const std::vector<WasmCodeRange>& expectations,
                    const std::vector<ValueType> locals = {}) {
    int func_index = builder_.NumDeclaredFunctions();
    WasmFunctionBuilder* func_builder = builder_.AddFunction(sig);
    for (ValueType local_type : locals) {
      func_builder->AddLocal(local_type);
    }
    func_builder->EmitCode(code_bytes.data(),
                           static_cast<uint32_t>(code_bytes.size()));
    func_builder->Emit(kExprEnd);

    ZoneBuffer buffer{zone()};
    builder_.WriteTo(&buffer);
    base::OwnedVector<const uint8_t> bytes = base::OwnedCopyOf(buffer);
    TestWireBytesStorage wire_bytes_storage(bytes);

    WasmDetectedFeatures detected_features;
    std::shared_ptr<NativeModule> native_module =
        CompileNativeModule(bytes, &detected_features);

    CompilationEnv env = CompilationEnv::ForModule(native_module.get());
    const WasmFunction* func = &env.module->functions[func_index];
    base::Vector<const uint8_t> code = wire_bytes_storage.GetCode(func->code);
    BasicBlockCalculator basic_block_calculator(zone(), code);
    basic_block_calculator.ComputeBasicBlocks();
    base::Vector<const WasmCodeRange> code_ranges =
        basic_block_calculator.GetCodeRanges();

    EXPECT_TRUE(DoCoverageResultsMatchExpectations(code_ranges, expectations));
  }

  WasmModuleBuilder builder_;
};

TEST_F(WasmBasicBlockCalculatorTest, TestBasicBlockCalculator) {
  v8_flags.wasm_allow_mixed_eh_for_testing = true;
  TestSignatures sigs;

  // Test with simple Wasm code.
  //-----------------------------
  //  func([]->[i32])
  //    ...wasmI32Const(42), // 1
  //    ...wasmI32Const(11), // 3
  //    kExprI32Add,         // 5
  //  kExprEnd               // 6
  //-----------------------------
  TestFunction(sigs.i_v(), {WASM_I32_ADD(WASM_I32V_1(42), WASM_I32V_1(11))},
               {{0, 6}});

  // Test with unreachable code.
  //-----------------------------
  //  func([]->[i32])
  //    kExprUnreachable,    // 1
  //-----------------------------
  //    ...wasmI32Const(42), // 2
  //    ...wasmI32Const(11), // 4
  //    kExprI32Add,         // 6
  //  kExprEnd               // 7
  //-----------------------------
  TestFunction(
      sigs.i_v(),
      {kExprUnreachable, WASM_I32_ADD(WASM_I32V_1(42), WASM_I32V_1(11))},
      {{0, 1}, {2, 7}});

  // Test with br_if and return.
  //-----------------------------------
  //  func([i32]->[i32])
  //    kExprBlock, kWasmVoid,    //  1
  //      kExprLocalGet, 0,       //  3
  //      kExprBrIf, 0,           //  5
  //-----------------------------------
  //      ...wasmI32Const(1),     //  7
  //      kExprReturn,            //  9
  //-----------------------------------
  //    kExprEnd,                 // 10
  //-----------------------------------
  //    ...wasmI32Const(0),       // 11
  //  kExprEnd                    // 13
  //-----------------------------------
  TestFunction(sigs.i_i(),
               {
                   WASM_BLOCK(WASM_LOCAL_GET(0), kExprBrIf, 0, WASM_I32V_1(1),
                              kExprReturn),
                   WASM_I32V_1(0),
               },
               {{0, 6}, {7, 9}, {10, 10}, {11, 13}});

  // Test with if/end.
  //-----------------------------------
  //  func([i32]->[i32])
  //    kExprLocalGet, 0,         //  1
  //    kExprIf, kWasmVoid,       //  3
  //-----------------------------------
  //      ...wasmI32Const(0),     //  5
  //      kExprReturn,            //  7
  //-----------------------------------
  //    kExprEnd,                 //  8
  //-----------------------------------
  //    ...wasmI32Const(1),       //  9
  //  kExprEnd                    // 11
  //-----------------------------------
  TestFunction(sigs.i_i(),
               {
                   WASM_IF(WASM_LOCAL_GET(0), WASM_I32V_1(0), kExprReturn),
                   WASM_I32V_1(1),
               },
               {{0, 4}, {5, 7}, {8, 8}, {9, 11}});

  // Test with if/else/end.
  //-----------------------------------
  //  func([i32]->[i32])
  //    kExprLocalGet, 0,         //  1
  //    kExprIf, kWasmVoid,       //  3
  //-----------------------------------
  //      ...wasmI32Const(0),     //  5
  //      kExprReturn,            //  7
  //-----------------------------------
  //    kExprElse,                //  8
  //-----------------------------------
  //    ...wasmI32Const(1),       //  9
  //      kExprReturn,            // 11
  //-----------------------------------
  //    kExprEnd,                 // 12
  //-----------------------------------
  //    ...wasmI32Const(-1),      // 13
  //  kExprEnd                    // 19
  //-----------------------------------
  TestFunction(
      sigs.i_i(),
      {WASM_LOCAL_GET(0), kExprIf, kVoidCode, WASM_I32V_1(0), kExprReturn,
       kExprElse, WASM_I32V_1(1), kExprReturn, kExprEnd, WASM_I32V(-1)},
      {{0, 4}, {5, 7}, {8, 8}, {9, 11}, {12, 12}, {13, 19}});

  // Test with if/else/end with branch.
  //-----------------------------------
  //  func([i32]->[i32])
  //    kExprLocalGet, 0,         //  1
  //    kExprIf, kWasmVoid,       //  3
  //-----------------------------------
  //      kExprBr, 0,             //  5
  //-----------------------------------
  //    kExprElse,                //  7
  //-----------------------------------
  //    ...wasmI32Const(1),       //  8
  //      kExprReturn,            // 10
  //-----------------------------------
  //    kExprEnd,                 // 11
  //-----------------------------------
  //    ...wasmI32Const(0),       // 12
  //  kExprEnd                    // 14
  //-----------------------------------
  TestFunction(sigs.i_i(),
               {WASM_LOCAL_GET(0), kExprIf, kVoidCode, kExprBr, 0, kExprElse,
                WASM_I32V_1(1), kExprReturn, kExprEnd, WASM_I32V_1(0)},
               {{0, 4}, {5, 6}, {7, 7}, {8, 10}, {11, 11}, {12, 14}});

  // Test with if/else/end without branches.
  //-----------------------------------
  //  func([i32]->[i32])
  //    kExprLocalGet, 0,         //  1
  //    kExprIf, kWasmVoid,       //  3
  //-----------------------------------
  //      ...wasmI32Const(0),     //  5
  //    kExprElse,                //  7
  //-----------------------------------
  //      ...wasmI32Const(1),     //  8
  //    kExprEnd,                 // 10
  //-----------------------------------
  //  kExprEnd                    // 11
  //-----------------------------------
  TestFunction(sigs.i_i(),
               {WASM_LOCAL_GET(0), kExprIf, kI32Code, WASM_I32V_1(0), kExprElse,
                WASM_I32V_1(1), kExprEnd},
               {{0, 4}, {5, 7}, {8, 10}, {11, 11}});

  // Test with br_table.
  //---------------------------------------------------
  //  func([i32]->[i32])
  //    kExprBlock, kWasmVoid,                    //  1
  //      kExprBlock, kWasmVoid,                  //  3
  //        kExprBlock, kWasmVoid,                //  5
  //          kExprBlock, kWasmVoid,              //  7
  //            kExprBlock, kWasmVoid,            //  9
  //              kExprLocalGet, 0,               // 11
  //              kExprBrTable, 4, 0, 1, 2, 3, 4, // 13
  //---------------------------------------------------
  //            kExprEnd,                         // 20
  //---------------------------------------------------
  //            kExprLocalGet, 1,                 // 21
  //            kExprLocalGet, 2,                 // 23
  //            kExprF64Add,                      // 25
  //            kExprReturn,                      // 26
  //---------------------------------------------------
  //          kExprEnd,                           // 27
  //---------------------------------------------------
  //          kExprLocalGet, 1,                   // 28
  //          kExprLocalGet, 2,                   // 30
  //          kExprF64Sub,                        // 32
  //          kExprReturn,                        // 33
  //---------------------------------------------------
  //       kExprEnd,                              // 34
  //---------------------------------------------------
  //        kExprLocalGet, 1,                     // 35
  //        kExprLocalGet, 2,                     // 37
  //        kExprF64Mul,                          // 39
  //        kExprReturn,                          // 40
  //---------------------------------------------------
  //      kExprEnd,                               // 41
  //---------------------------------------------------
  //      kExprLocalGet, 1,                       // 42
  //      kExprLocalGet, 2,                       // 44
  //      kExprF64Div,                            // 46
  //      kExprReturn,                            // 47
  //---------------------------------------------------
  //    kExprEnd,                                 // 48
  //---------------------------------------------------
  //    ...wasmF64Const(.0),                      // 49
  //  kExprEnd                                    // 58
  //---------------------------------------------------
  static auto d_idd_sig =
      FixedSizeSignature<ValueType>::Returns(kWasmF64).Params(
          kWasmI32, kWasmF64, kWasmF64);
  TestFunction(
      &d_idd_sig,
      {WASM_BLOCK(
           WASM_BLOCK(
               WASM_BLOCK(WASM_BLOCK(WASM_BLOCK(WASM_LOCAL_GET(0), kExprBrTable,
                                                4, 0, 1, 2, 3, 4),
                                     WASM_LOCAL_GET(1), WASM_LOCAL_GET(2),
                                     kExprF64Add, kExprReturn),
                          WASM_LOCAL_GET(1), WASM_LOCAL_GET(2), kExprF64Sub,
                          kExprReturn),
               WASM_LOCAL_GET(1), WASM_LOCAL_GET(2), kExprF64Mul, kExprReturn),
           WASM_LOCAL_GET(1), WASM_LOCAL_GET(2), kExprF64Div, kExprReturn),
       WASM_F64(.0)},
      {{0, 19},
       {20, 20},
       {21, 26},
       {27, 27},
       {28, 33},
       {34, 34},
       {35, 40},
       {41, 41},
       {42, 47},
       {48, 48},
       {49, 58}});

  // Test with loop.
  //-----------------------------------
  //  func([i32]->[i32])
  //    ...wasmI32Const(1),         //  3
  //    kExprLocalSet, 1,           //  5
  //    kExprBlock, kWasmVoid,      //  7
  //-------------------------------------
  //      kExprLoop, kWasmVoid,     //  9
  //        kExprLocalGet, 0,       // 11
  //        ...wasmI32Const(0),     // 13
  //        kExprI32Eq,             // 15
  //        kExprBrIf, 1,           // 16
  //-------------------------------------
  //        kExprLocalGet, 1,       // 18
  //        kExprLocalGet, 0,       // 20
  //        kExprI32Mul,            // 22
  //        kExprLocalSet, 1,       // 23
  //        kExprLocalGet, 0,       // 25
  //        ...wasmI32Const(1),     // 27
  //        kExprI32Sub,            // 29
  //        kExprLocalSet, 0,       // 30
  //        kExprBr, 0,             // 32
  //-------------------------------------
  //     kExprEnd,                  // 34
  //-------------------------------------
  //   kExprEnd,                    // 35
  //-------------------------------------
  //   kExprLocalGet, 1,            // 36
  //  kExprEnd                      // 38
  //-------------------------------------
  TestFunction(sigs.i_i(),
               {WASM_LOCAL_SET(1, WASM_I32V_1(1)),
                WASM_BLOCK(WASM_LOOP(
                    WASM_LOCAL_GET(0), WASM_I32V_1(0), kExprI32Eq, kExprBrIf, 1,
                    WASM_LOCAL_GET(1), WASM_LOCAL_GET(0), kExprI32Mul,
                    kExprLocalSet, 1, WASM_LOCAL_GET(0), WASM_I32V_1(1),
                    kExprI32Sub, kExprLocalSet, 0, kExprBr, 0)),
                WASM_LOCAL_GET(1)},
               {{0, 8}, {9, 17}, {18, 33}, {34, 34}, {35, 35}, {36, 38}},
               {kWasmI32});

  // Test with br.
  //------------------------------------
  //  func([i32]->[i32])
  //    kExprBlock, kWasmVoid,     //  1
  //      kExprBlock, kWasmVoid,   //  3
  //        kExprBr, 0,            //  5
  //------------------------------------
  //        ...wasmI32Const(0),    //  7
  //        kExprReturn,           //  9
  //------------------------------------
  //      kExprEnd,                // 10
  //------------------------------------
  //      ...wasmI32Const(1),      // 11
  //      kExprReturn,             // 13
  //------------------------------------
  //    kExprEnd,                  // 14
  //------------------------------------
  //    ...wasmI32Const(-1),       // 15
  //  kExprEnd                     // 21
  //------------------------------------
  TestFunction(sigs.i_i(),
               {WASM_BLOCK(WASM_BLOCK(kExprBr, 0, WASM_I32V_1(0), kExprReturn),
                           WASM_I32V_1(1), kExprReturn),
                WASM_I32V(-1)},
               {{0, 6}, {7, 9}, {10, 10}, {11, 13}, {14, 14}, {15, 21}});

  // Test with Ref branch.
  //-------------------------------------------
  //  func([i32]->[i32])
  //    kExprBlock, kWasmVoid,            //  1
  //      kExprRefNull, array_type_index, //  3
  //      kExprBrOnNull, 0,               //  5
  //-------------------------------------------
  //      ...wasmI32Const(1),             //  7
  //      kExprReturn,                    //  9
  //-------------------------------------------
  //    kExprEnd,                         // 10
  //-------------------------------------------
  //    ...wasmI32Const(0),               // 11
  //  kExprEnd                            // 13
  //-------------------------------------------
  ModuleTypeIndex array_type_index = builder_.AddArrayType(
      zone()->New<ArrayType>(kWasmI32, false /*mutability*/),
      false /*is_final*/, kNoSuperType);

  TestFunction(sigs.i_v(),
               {WASM_BLOCK(kExprRefNull, ToByte(array_type_index),
                           kExprBrOnNull, 0, WASM_I32V_1(1), kExprReturn),
                WASM_I32V_1(0)},
               {{0, 6}, {7, 9}, {10, 10}, {11, 13}});

  // Test with nested legacy try/catch.
  //-----------------------------------------
  //  func([i32]->[i32])
  //    kExprTry, kWasmI32,             //  3
  //      ...wasmF32Const(3.14),        //  5
  //      ...wasmF32Const(3.14),        // 10
  //      kExprTry, kWasmI32,           // 15
  //        ...wasmI32Const(1),         // 17
  //        kExprThrow, except,         // 19
  //-----------------------------------------
  //      kExprCatch, except,           // 21
  //-----------------------------------------
  //        kExprTry, kWasmI32,         // 23
  //          ...wasmI32Const(2),       // 25
  //        kExprEnd,                   // 27
  //-----------------------------------------
  //      kExprCatchAll,                // 28
  //-----------------------------------------
  //        ...wasmI32Const(3),         // 29
  //      kExprEnd,                     // 31
  //-----------------------------------------
  //      kExprLocalSet, 0,             // 32
  //      kExprF32Ne,                   // 34
  //      kExprDrop,                    // 35
  //      kExprLocalGet, 0,             // 36
  //    kExprEnd,                       // 38
  //  kExprEnd                          // 39
  //-----------------------------------------

  uint8_t tag = builder_.AddTag(sigs.v_v());

  TestFunction(
      sigs.i_v(),
      {kExprTry,
       kI32Code,
       WASM_F32(3.14),
       WASM_F32(3.14),
       kExprTry,
       kI32Code,
       WASM_I32V_1(1),
       kExprThrow,
       tag,
       kExprCatch,
       tag,
       kExprTry,
       kI32Code,
       WASM_I32V_1(2),
       kExprEnd,
       kExprCatchAll,
       WASM_I32V_1(3),
       kExprEnd,
       kExprLocalSet,
       0,
       kExprF32Ne,
       kExprDrop,
       WASM_LOCAL_GET(0),
       kExprEnd},
      {{0, 20}, {21, 22}, {23, 27}, {28, 28}, {29, 31}, {32, 38}, {39, 39}},
      {kWasmI32});

  // Test with nested legacy try/catch with no throw.
  //-----------------------------------------
  //  func([i32]->[i32])
  //    kExprTry, kWasmI32,             //  1
  //      ...wasmF32Const(3.14),        //  3
  //      ...wasmF32Const(3.14),        //  8
  //      kExprTry, kWasmI32,           // 13
  //        ...wasmI32Const(1),         // 15
  //      kExprCatch, except,           // 17
  //-----------------------------------------
  //        kExprTry, kWasmI32,         // 19
  //          ...wasmI32Const(2),       // 21
  //        kExprEnd,                   // 23
  //-----------------------------------------
  //      kExprCatchAll,                // 24
  //-----------------------------------------
  //        ...wasmI32Const(3),         // 25
  //      kExprEnd,                     // 27
  //-----------------------------------------
  //      kExprDrop,                    // 28
  //      kExprF32Ne,                   // 29
  //    kExprEnd,                       // 30
  //-----------------------------------------
  //  kExprEnd                          // 31
  //-----------------------------------------
  TestFunction(sigs.i_v(),
               {kExprTry, kI32Code, WASM_F32(3.14), WASM_F32(3.14), kExprTry,
                kI32Code, WASM_I32V_1(1), kExprCatch, tag, kExprTry, kI32Code,
                WASM_I32V_1(2), kExprEnd, kExprCatchAll, WASM_I32V_1(3),
                kExprEnd, kExprDrop, kExprF32Ne, kExprEnd},
               {{0, 18}, {19, 23}, {24, 24}, {25, 27}, {28, 30}, {31, 31}});

  // Test with new exception handling with no throw.
  //-----------------------------------------
  //  func([i32]->[i32])
  //      kExprBlock, kWasmVoid,        //  1
  //        kExprTryTable, kWasmI32, 1, //  3
  //        kCatchNoRef, except, 0,
  //          kExprLocalGet, 0,         //  9
  //          kExprI32Eqz,              // 11
  //          kExprIf, kWasmVoid,       // 12
  //-----------------------------------------
  //            kExprI32Const, 23,      // 14
  //            kExprBr, 1,             // 16
  //-----------------------------------------
  //          kExprEnd,                 // 18
  //-----------------------------------------
  //          kExprI32Const, 42,        // 19
  //        kExprEnd,                   // 21
  //-----------------------------------------
  //        kExprReturn,                // 22
  //-----------------------------------------
  //      kExprEnd,                     // 23
  //-----------------------------------------
  //      kExprI32Const, 23             // 24
  //  kExprEnd                          // 26
  //-----------------------------------------
  TestFunction(
      sigs.i_i(),
      {WASM_BLOCK(kExprTryTable, kI32Code, U32V_1(1), CatchKind::kCatch, tag,
                  U32V_1(0), WASM_LOCAL_GET(0), kExprI32Eqz, kExprIf, kVoidCode,
                  WASM_I32V_1(23), kExprBr, 1, kExprEnd, WASM_I32V_1(42),
                  kExprEnd, kExprReturn),
       WASM_I32V_1(23)},
      {{0, 13}, {14, 17}, {18, 18}, {19, 21}, {22, 22}, {23, 23}, {24, 26}});

  // Test with new exception handling.
  //------------------------------------------------------------
  //  func([i32]->[i32])
  //    kExprBlock, kWasmVoid,                            //  1
  //      kExprTryTable, kWasmI32, 1, kCatch, except, 0,  //  3
  //        kExprLocalGet, 0,                             //  9
  //        kExprI32Eqz,                                  // 11
  //        kExprIf, kWasmVoid,                           // 12
  //-----------------------------------------------------------
  //          kExprThrow, except,                         // 14
  //-----------------------------------------------------------
  //        kExprEnd,                                     // 16
  //-----------------------------------------------------------
  //        kExprI32Const, 42,                            // 17
  //      kExprEnd,                                       // 19
  //------------------------------------------------------------
  //      kExprBr, 1,                                     // 20
  //-----------------------------------------------------------
  //    kExprEnd,                                         // 22
  //-----------------------------------------------------------
  //    kExprI32Const, 23                                 // 23
  //  kExprEnd                                            // 25
  //-----------------------------------------------------------
  TestFunction(
      sigs.i_i(),
      {WASM_BLOCK(kExprTryTable, kI32Code, U32V_1(1), CatchKind::kCatch, tag,
                  U32V_1(0), WASM_LOCAL_GET(0), kExprI32Eqz, kExprIf, kVoidCode,
                  kExprThrow, tag, kExprEnd, WASM_I32V_1(42), kExprEnd, kExprBr,
                  1),
       WASM_I32V_1(23)},
      {{0, 13}, {14, 15}, {16, 16}, {17, 19}, {20, 21}, {22, 22}, {23, 25}});

  // Test with complex code.
  //------------------------------------------
  //  func([i32]->[i32])
  //    kExprLocalGet, 0,               //  3
  //    kExprIf, kWasmVoid,             //  5
  //-----------------------------------------
  //      kExprI32Const, 5,             //  7
  //      kExprLocalSet, 1,             //  9
  //      kExprLocalGet, 0,             // 11
  //      kExprIf, kWasmVoid,           // 13
  //-----------------------------------------
  //        kExprLoop, kWasmVoid,       // 15
  //          kExprBlock, 0,            // 17
  //            kExprLocalGet, 0,       // 19
  //            kExprIf, kWasmVoid,     // 21
  //-----------------------------------------
  //              ...wasmI32Const(1),   // 23
  //              kExprDrop,            // 25
  //              kExprBr, 1,           // 26
  //-----------------------------------------
  //            kExprEnd,               // 28
  //-----------------------------------------
  //            kExprLocalGet, 1,       // 29
  //            kExprBrIf, 0,           // 31
  //-----------------------------------------
  //            ...wasmI32Const(1),     // 33
  //            kExprDrop,              // 35
  //          kExprEnd,                 // 36
  //-----------------------------------------
  //          kExprLocalGet, 1,         // 37
  //          ...wasmI32Const(1),       // 39
  //          kExprI32Sub,              // 41
  //          kExprLocalTee, 1,         // 42
  //          ...wasmI32Const(0),       // 44
  //          kExprI32Ne,               // 46
  //          kExprBrIf, 0,             // 47
  //-----------------------------------------
  //          ...wasmI32Const(1),       // 49
  //          kExprDrop,                // 51
  //        kExprEnd,                   // 52
  //-----------------------------------------
  //        ...wasmI32Const(1),         // 53
  //        kExprDrop,                  // 55
  //      kExprEnd,                     // 56
  //-----------------------------------------
  //      ...wasmI32Const(1),           // 57
  //      kExprDrop,                    // 59
  //    kExprElse,                      // 60
  //-----------------------------------------
  //      ...wasmI32Const(1),           // 61
  //      ...wasmI32Const(2),           // 63
  //      kExprI32Add,                  // 65
  //      kExprDrop,                    // 66
  //    kExprEnd,                       // 67
  //  kExprEnd                          // 68
  //-----------------------------------------
  TestFunction(
      sigs.v_i(),
      {WASM_LOCAL_GET(0),
       kExprIf,
       kVoidCode,
       WASM_I32V_1(5),
       kExprLocalSet,
       1,
       WASM_LOCAL_GET(0),
       kExprIf,
       kVoidCode,
       kExprLoop,
       kVoidCode,
       WASM_BLOCK(WASM_LOCAL_GET(0), kExprIf, kVoidCode, WASM_I32V_1(1),
                  kExprDrop, kExprBr, 1, kExprEnd, WASM_LOCAL_GET(1), kExprBrIf,
                  0, WASM_I32V_1(1), kExprDrop),
       WASM_LOCAL_GET(1),
       WASM_I32V_1(1),
       kExprI32Sub,
       kExprLocalTee,
       1,
       WASM_I32V_1(0),
       kExprI32Ne,
       kExprBrIf,
       0,
       WASM_I32V_1(1),
       kExprDrop,
       kExprEnd,
       WASM_I32V_1(1),
       kExprDrop,
       kExprEnd,
       WASM_I32V_1(1),
       kExprDrop,
       kExprElse,
       WASM_I32V_1(1),
       WASM_I32V_1(2),
       kExprI32Add,
       kExprDrop,
       kExprEnd},
      {{0, 6},
       {7, 14},
       {15, 22},
       {23, 27},
       {28, 28},
       {29, 32},
       {33, 36},
       {37, 48},
       {49, 52},
       {53, 56},
       {57, 60},
       {61, 67},
       {68, 68}},
      {kWasmI32});
}

}  // namespace v8::internal::wasm

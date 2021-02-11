// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-arguments.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_gc {

using F = std::pair<ValueType, bool>;

class WasmGCTester {
 public:
  WasmGCTester()
      : flag_gc(&v8::internal::FLAG_experimental_wasm_gc, true),
        flag_reftypes(&v8::internal::FLAG_experimental_wasm_reftypes, true),
        flag_typedfuns(&v8::internal::FLAG_experimental_wasm_typed_funcref,
                       true),
        zone(&allocator, ZONE_NAME),
        builder_(&zone),
        isolate_(CcTest::InitIsolateOnce()),
        scope(isolate_),
        thrower(isolate_, "Test wasm GC") {
    testing::SetupIsolateForWasmModule(isolate_);
  }

  byte AddGlobal(ValueType type, bool mutability, WasmInitExpr init) {
    return builder_.AddGlobal(type, mutability, std::move(init));
  }

  byte DefineFunction(FunctionSig* sig, std::initializer_list<ValueType> locals,
                      std::initializer_list<byte> code) {
    WasmFunctionBuilder* fun = builder_.AddFunction(sig);
    for (ValueType local : locals) {
      fun->AddLocal(local);
    }
    fun->EmitCode(code.begin(), static_cast<uint32_t>(code.size()));
    return fun->func_index();
  }

  void DefineExportedFunction(const char* name, FunctionSig* sig,
                              std::initializer_list<byte> code) {
    WasmFunctionBuilder* fun = builder_.AddFunction(sig);
    fun->EmitCode(code.begin(), static_cast<uint32_t>(code.size()));
    builder_.AddExport(CStrVector(name), fun);
  }

  MaybeHandle<Object> CallExportedFunction(const char* name, int argc,
                                           Handle<Object> args[]) {
    Handle<WasmExportedFunction> func =
        testing::GetExportedFunction(isolate_, instance_, name)
            .ToHandleChecked();
    return Execution::Call(isolate_, func,
                           isolate_->factory()->undefined_value(), argc, args);
  }

  byte DefineStruct(std::initializer_list<F> fields) {
    StructType::Builder type_builder(&zone,
                                     static_cast<uint32_t>(fields.size()));
    for (F field : fields) {
      type_builder.AddField(field.first, field.second);
    }
    return builder_.AddStructType(type_builder.Build());
  }

  byte DefineArray(ValueType element_type, bool mutability) {
    return builder_.AddArrayType(zone.New<ArrayType>(element_type, mutability));
  }

  byte DefineSignature(FunctionSig* sig) { return builder_.AddSignature(sig); }

  byte DefineTable(ValueType type, uint32_t min_size, uint32_t max_size) {
    return builder_.AddTable(type, min_size, max_size);
  }

  void CompileModule() {
    ZoneBuffer buffer(&zone);
    builder_.WriteTo(&buffer);
    MaybeHandle<WasmInstanceObject> maybe_instance =
        testing::CompileAndInstantiateForTesting(
            isolate_, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));
    if (thrower.error()) FATAL("%s", thrower.error_msg());
    instance_ = maybe_instance.ToHandleChecked();
  }

  void CallFunctionImpl(uint32_t function_index, const FunctionSig* sig,
                        CWasmArgumentsPacker* packer) {
    WasmCodeRefScope scope;
    NativeModule* native_module = instance_->module_object().native_module();
    WasmCode* code = native_module->GetCode(function_index);
    Address wasm_call_target = code->instruction_start();
    Handle<Object> object_ref = instance_;
    Handle<Code> c_wasm_entry =
        compiler::CompileCWasmEntry(isolate_, sig, native_module->module());
    Execution::CallWasm(isolate_, c_wasm_entry, wasm_call_target, object_ref,
                        packer->argv());
  }

  void CheckResult(uint32_t function_index, int32_t expected) {
    FunctionSig* sig = sigs.i_v();
    DCHECK(*sig == *instance_->module()->functions[function_index].sig);
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    CallFunctionImpl(function_index, sig, &packer);
    packer.Reset();
    CHECK_EQ(expected, packer.Pop<int32_t>());
  }

  void CheckResult(uint32_t function_index, int32_t expected, int32_t arg) {
    FunctionSig* sig = sigs.i_i();
    DCHECK(*sig == *instance_->module()->functions[function_index].sig);
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    packer.Push(arg);
    CallFunctionImpl(function_index, sig, &packer);
    packer.Reset();
    CHECK_EQ(expected, packer.Pop<int32_t>());
  }

  MaybeHandle<Object> GetResultObject(uint32_t function_index) {
    const FunctionSig* sig = instance_->module()->functions[function_index].sig;
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    CallFunctionImpl(function_index, sig, &packer);
    packer.Reset();
    return Handle<Object>(Object(packer.Pop<Address>()), isolate_);
  }

  void CheckHasThrown(uint32_t function_index, int32_t arg) {
    FunctionSig* sig = sigs.i_i();
    DCHECK(*sig == *instance_->module()->functions[function_index].sig);
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    packer.Push(arg);
    CallFunctionImpl(function_index, sig, &packer);
    CHECK(isolate_->has_pending_exception());
    isolate_->clear_pending_exception();
  }

  Handle<WasmInstanceObject> instance() { return instance_; }
  Isolate* isolate() { return isolate_; }
  WasmModuleBuilder* builder() { return &builder_; }

  TestSignatures sigs;

 private:
  const FlagScope<bool> flag_gc;
  const FlagScope<bool> flag_reftypes;
  const FlagScope<bool> flag_typedfuns;

  v8::internal::AccountingAllocator allocator;
  Zone zone;
  WasmModuleBuilder builder_;

  Isolate* const isolate_;
  const HandleScope scope;
  Handle<WasmInstanceObject> instance_;
  ErrorThrower thrower;
};

ValueType ref(uint32_t type_index) {
  return ValueType::Ref(type_index, kNonNullable);
}
ValueType optref(uint32_t type_index) {
  return ValueType::Ref(type_index, kNullable);
}

// TODO(7748): Use WASM_EXEC_TEST once interpreter and liftoff are supported.
TEST(WasmBasicStruct) {
  WasmGCTester tester;
  const byte type_index =
      tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});
  const byte empty_struct_index = tester.DefineStruct({});
  ValueType kRefType = ref(type_index);
  ValueType kEmptyStructType = ref(empty_struct_index);
  ValueType kOptRefType = optref(type_index);
  FunctionSig sig_q_v(1, 0, &kRefType);
  FunctionSig sig_qe_v(1, 0, &kEmptyStructType);

  // Test struct.new and struct.get.
  const byte kGet1 = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_STRUCT_GET(
           type_index, 0,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(64),
                                    WASM_RTT_CANON(type_index))),
       kExprEnd});

  // Test struct.new and struct.get.
  const byte kGet2 = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_STRUCT_GET(
           type_index, 1,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(64),
                                    WASM_RTT_CANON(type_index))),
       kExprEnd});

  // Test struct.new, returning struct reference.
  const byte kGetStruct = tester.DefineFunction(
      &sig_q_v, {},
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(64),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});

  // Test struct.new, returning reference to an empty struct.
  const byte kGetEmptyStruct = tester.DefineFunction(
      &sig_qe_v, {},
      {WASM_STRUCT_NEW_WITH_RTT(empty_struct_index,
                                WASM_RTT_CANON(empty_struct_index)),
       kExprEnd});

  // Test struct.set, struct refs types in locals.
  const byte j_local_index = 0;
  const byte j_field_index = 0;
  const byte kSet = tester.DefineFunction(
      tester.sigs.i_v(), {kOptRefType},
      {WASM_SET_LOCAL(
           j_local_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(64),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_SET(type_index, j_field_index, WASM_GET_LOCAL(j_local_index),
                       WASM_I32V(-99)),
       WASM_STRUCT_GET(type_index, j_field_index,
                       WASM_GET_LOCAL(j_local_index)),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(kGet1, 42);
  tester.CheckResult(kGet2, 64);
  CHECK(tester.GetResultObject(kGetStruct).ToHandleChecked()->IsWasmStruct());
  CHECK(tester.GetResultObject(kGetEmptyStruct)
            .ToHandleChecked()
            ->IsWasmStruct());
  tester.CheckResult(kSet, -99);
}

// Test struct.set, ref.as_non_null,
// struct refs types in globals and if-results.
TEST(WasmRefAsNonNull) {
  WasmGCTester tester;
  const byte type_index =
      tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});
  ValueType kRefTypes[] = {ref(type_index)};
  ValueType kOptRefType = optref(type_index);
  FunctionSig sig_q_v(1, 0, kRefTypes);

  const byte global_index =
      tester.AddGlobal(kOptRefType, true,
                       WasmInitExpr::RefNullConst(
                           static_cast<HeapType::Representation>(type_index)));
  const byte field_index = 0;
  const byte kFunc = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_SET_GLOBAL(
           global_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(55), WASM_I32V(66),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET(
           type_index, field_index,
           WASM_REF_AS_NON_NULL(WASM_IF_ELSE_R(kOptRefType, WASM_I32V(1),
                                               WASM_GET_GLOBAL(global_index),
                                               WASM_REF_NULL(type_index)))),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kFunc, 55);
}

TEST(WasmBrOnNull) {
  WasmGCTester tester;
  const byte type_index =
      tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});
  ValueType kRefTypes[] = {ref(type_index)};
  ValueType kOptRefType = optref(type_index);
  FunctionSig sig_q_v(1, 0, kRefTypes);
  const byte l_local_index = 0;
  const byte kTaken = tester.DefineFunction(
      tester.sigs.i_v(), {kOptRefType},
      {WASM_BLOCK_I(WASM_I32V(42),
                    // Branch will be taken.
                    // 42 left on stack outside the block (not 52).
                    WASM_BR_ON_NULL(0, WASM_GET_LOCAL(l_local_index)),
                    WASM_I32V(52), WASM_BR(0)),
       kExprEnd});

  const byte m_field_index = 0;
  const byte kNotTaken = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_BLOCK_I(
           WASM_I32V(42),
           WASM_STRUCT_GET(
               type_index, m_field_index,
               // Branch will not be taken.
               // 52 left on stack outside the block (not 42).
               WASM_BR_ON_NULL(0, WASM_STRUCT_NEW_WITH_RTT(
                                      type_index, WASM_I32V(52), WASM_I32V(62),
                                      WASM_RTT_CANON(type_index)))),
           WASM_BR(0)),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kTaken, 42);
  tester.CheckResult(kNotTaken, 52);
}

TEST(BrOnCast) {
  WasmGCTester tester;
  const byte type_index = tester.DefineStruct({F(kWasmI32, true)});
  const byte rtt_index =
      tester.AddGlobal(ValueType::Rtt(type_index, 1), false,
                       WasmInitExpr::RttCanon(
                           static_cast<HeapType::Representation>(type_index)));
  const byte kTestStruct = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kWasmEqRef},
      {WASM_BLOCK(WASM_SET_LOCAL(0, WASM_I32V(111)),
                  // Pipe a struct through a local so it's statically typed
                  // as eqref.
                  WASM_SET_LOCAL(
                      1, WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(1),
                                                  WASM_GET_GLOBAL(rtt_index))),
                  WASM_GET_LOCAL(1),
                  // The struct is not an i31, so this branch isn't taken.
                  WASM_BR_ON_CAST(0, WASM_RTT_CANON(kI31RefCode)),
                  WASM_SET_LOCAL(0, WASM_I32V(222)),  // Final result.
                  // This branch is taken.
                  WASM_BR_ON_CAST(0, WASM_GET_GLOBAL(rtt_index)),
                  // Not executed due to the branch.
                  WASM_DROP, WASM_SET_LOCAL(0, WASM_I32V(333))),
       WASM_GET_LOCAL(0), kExprEnd});

  const byte kTestI31 = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kWasmEqRef},
      {WASM_BLOCK(WASM_SET_LOCAL(0, WASM_I32V(111)),
                  // Pipe an i31ref through a local so it's statically typed
                  // as eqref.
                  WASM_SET_LOCAL(1, WASM_I31_NEW(WASM_I32V(42))),
                  WASM_GET_LOCAL(1),
                  // The i31 is not a struct, so this branch isn't taken.
                  WASM_BR_ON_CAST(0, WASM_GET_GLOBAL(rtt_index)),
                  WASM_SET_LOCAL(0, WASM_I32V(222)),  // Final result.
                  // This branch is taken.
                  WASM_BR_ON_CAST(0, WASM_RTT_CANON(kI31RefCode)),
                  // Not executed due to the branch.
                  WASM_DROP, WASM_SET_LOCAL(0, WASM_I32V(333))),
       WASM_GET_LOCAL(0), kExprEnd});

  const byte kTestNull = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kWasmEqRef},
      {WASM_BLOCK(WASM_SET_LOCAL(0, WASM_I32V(111)),
                  WASM_GET_LOCAL(1),  // Put a nullref onto the value stack.
                  // Neither of these branches is taken for nullref.
                  WASM_BR_ON_CAST(0, WASM_RTT_CANON(kI31RefCode)),
                  WASM_SET_LOCAL(0, WASM_I32V(222)),
                  WASM_BR_ON_CAST(0, WASM_GET_GLOBAL(rtt_index)), WASM_DROP,
                  WASM_SET_LOCAL(0, WASM_I32V(333))),  // Final result.
       WASM_GET_LOCAL(0), kExprEnd});

  const byte kTypedAfterBranch = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kWasmEqRef},
      {WASM_SET_LOCAL(1, WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                                  WASM_GET_GLOBAL(rtt_index))),
       WASM_BLOCK(WASM_SET_LOCAL(
           // The outer block catches the struct left behind by the inner block
           // and reads its field.
           0,
           WASM_STRUCT_GET(
               type_index, 0,
               // The inner block should take the early branch with a struct
               // on the stack.
               WASM_BLOCK_R(ValueType::Ref(type_index, kNonNullable),
                            WASM_GET_LOCAL(1),
                            WASM_BR_ON_CAST(0, WASM_GET_GLOBAL(rtt_index)),
                            // Returning 123 is the unreachable failure case.
                            WASM_SET_LOCAL(0, WASM_I32V(123)), WASM_BR(1))))),
       WASM_GET_LOCAL(0), kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kTestStruct, 222);
  tester.CheckResult(kTestI31, 222);
  tester.CheckResult(kTestNull, 333);
  tester.CheckResult(kTypedAfterBranch, 42);
}

TEST(WasmRefEq) {
  WasmGCTester tester;
  byte type_index = tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});
  ValueType kRefTypes[] = {ref(type_index)};
  ValueType kOptRefType = optref(type_index);
  FunctionSig sig_q_v(1, 0, kRefTypes);

  byte local_index = 0;
  const byte kFunc = tester.DefineFunction(
      tester.sigs.i_v(), {kOptRefType},
      {WASM_SET_LOCAL(local_index, WASM_STRUCT_NEW_WITH_RTT(
                                       type_index, WASM_I32V(55), WASM_I32V(66),
                                       WASM_RTT_CANON(type_index))),
       WASM_I32_ADD(
           WASM_I32_SHL(
               WASM_REF_EQ(  // true
                   WASM_GET_LOCAL(local_index), WASM_GET_LOCAL(local_index)),
               WASM_I32V(0)),
           WASM_I32_ADD(
               WASM_I32_SHL(WASM_REF_EQ(  // false
                                WASM_GET_LOCAL(local_index),
                                WASM_STRUCT_NEW_WITH_RTT(
                                    type_index, WASM_I32V(55), WASM_I32V(66),
                                    WASM_RTT_CANON(type_index))),
                            WASM_I32V(1)),
               WASM_I32_ADD(WASM_I32_SHL(  // false
                                WASM_REF_EQ(WASM_GET_LOCAL(local_index),
                                            WASM_REF_NULL(type_index)),
                                WASM_I32V(2)),
                            WASM_I32_SHL(WASM_REF_EQ(  // true
                                             WASM_REF_NULL(type_index),
                                             WASM_REF_NULL(type_index)),
                                         WASM_I32V(3))))),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kFunc, 0b1001);
}

TEST(WasmPackedStructU) {
  WasmGCTester tester;

  const byte type_index = tester.DefineStruct(
      {F(kWasmI8, true), F(kWasmI16, true), F(kWasmI32, true)});
  ValueType struct_type = optref(type_index);

  const byte local_index = 0;

  int32_t expected_output_0 = 0x1234;
  int32_t expected_output_1 = -1;

  const byte kF0 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_SET_LOCAL(local_index,
                      WASM_STRUCT_NEW_WITH_RTT(
                          type_index, WASM_I32V(expected_output_0),
                          WASM_I32V(expected_output_1), WASM_I32V(0x12345678),
                          WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_U(type_index, 0, WASM_GET_LOCAL(local_index)),
       kExprEnd});

  const byte kF1 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_SET_LOCAL(local_index,
                      WASM_STRUCT_NEW_WITH_RTT(
                          type_index, WASM_I32V(expected_output_0),
                          WASM_I32V(expected_output_1), WASM_I32V(0x12345678),
                          WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_U(type_index, 1, WASM_GET_LOCAL(local_index)),
       kExprEnd});
  tester.CompileModule();

  tester.CheckResult(kF0, static_cast<uint8_t>(expected_output_0));
  tester.CheckResult(kF1, static_cast<uint16_t>(expected_output_1));
}

TEST(WasmPackedStructS) {
  WasmGCTester tester;

  const byte type_index = tester.DefineStruct(
      {F(kWasmI8, true), F(kWasmI16, true), F(kWasmI32, true)});
  ValueType struct_type = optref(type_index);

  const byte local_index = 0;

  int32_t expected_output_0 = 0x80;
  int32_t expected_output_1 = 42;

  const byte kF0 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_SET_LOCAL(
           local_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(expected_output_0),
                                    WASM_I32V(expected_output_1), WASM_I32V(0),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_S(type_index, 0, WASM_GET_LOCAL(local_index)),
       kExprEnd});

  const byte kF1 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_SET_LOCAL(
           local_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(0x80),
                                    WASM_I32V(expected_output_1), WASM_I32V(0),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_S(type_index, 1, WASM_GET_LOCAL(local_index)),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(kF0, static_cast<int8_t>(expected_output_0));
  tester.CheckResult(kF1, static_cast<int16_t>(expected_output_1));
}

TEST(WasmLetInstruction) {
  WasmGCTester tester;
  const byte type_index =
      tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});

  const byte let_local_index = 0;
  const byte let_field_index = 0;
  const byte kLetTest1 = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_LET_1_I(
           WASM_SEQ(kRefCode, type_index),
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(52),
                                    WASM_RTT_CANON(type_index)),
           WASM_STRUCT_GET(type_index, let_field_index,
                           WASM_GET_LOCAL(let_local_index))),
       kExprEnd});

  const byte let_2_field_index = 0;
  const byte kLetTest2 = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_LET_2_I(
           kI32Code, WASM_I32_ADD(WASM_I32V(42), WASM_I32V(-32)),
           WASM_SEQ(kRefCode, type_index),
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(52),
                                    WASM_RTT_CANON(type_index)),
           WASM_I32_MUL(WASM_STRUCT_GET(type_index, let_2_field_index,
                                        WASM_GET_LOCAL(1)),
                        WASM_GET_LOCAL(0))),
       kExprEnd});

  const byte kLetTestLocals = tester.DefineFunction(
      tester.sigs.i_i(), {kWasmI32},
      {WASM_SET_LOCAL(1, WASM_I32V(100)),
       WASM_LET_2_I(
           kI32Code, WASM_I32V(1), kI32Code, WASM_I32V(10),
           WASM_I32_SUB(WASM_I32_ADD(WASM_GET_LOCAL(0),     // 1st let-local
                                     WASM_GET_LOCAL(2)),    // Parameter
                        WASM_I32_ADD(WASM_GET_LOCAL(1),     // 2nd let-local
                                     WASM_GET_LOCAL(3)))),  // Function local
       kExprEnd});
  // Result: (1 + 1000) - (10 + 100) = 891

  const byte let_erase_local_index = 0;
  const byte kLetTestErase = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32},
      {WASM_SET_LOCAL(let_erase_local_index, WASM_I32V(0)),
       WASM_LET_1_V(kI32Code, WASM_I32V(1), WASM_NOP),
       WASM_GET_LOCAL(let_erase_local_index), kExprEnd});
  // The result should be 0 and not 1, as local_get(0) refers to the original
  // local.

  tester.CompileModule();

  tester.CheckResult(kLetTest1, 42);
  tester.CheckResult(kLetTest2, 420);
  tester.CheckResult(kLetTestLocals, 891, 1000);
  tester.CheckResult(kLetTestErase, 0);
}

TEST(WasmBasicArray) {
  WasmGCTester tester;
  const byte type_index = tester.DefineArray(wasm::kWasmI32, true);
  ValueType kRefTypes[] = {ref(type_index)};
  FunctionSig sig_q_v(1, 0, kRefTypes);
  ValueType kOptRefType = optref(type_index);

  // f: a = [12, 12, 12]; a[1] = 42; return a[arg0]
  const byte local_index = 1;
  const byte kGetElem = tester.DefineFunction(
      tester.sigs.i_i(), {kOptRefType},
      {WASM_SET_LOCAL(local_index, WASM_ARRAY_NEW_WITH_RTT(
                                       type_index, WASM_I32V(12), WASM_I32V(3),
                                       WASM_RTT_CANON(type_index))),
       WASM_ARRAY_SET(type_index, WASM_GET_LOCAL(local_index), WASM_I32V(1),
                      WASM_I32V(42)),
       WASM_ARRAY_GET(type_index, WASM_GET_LOCAL(local_index),
                      WASM_GET_LOCAL(0)),
       kExprEnd});

  // Reads and returns an array's length.
  const byte kGetLength = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_ARRAY_LEN(type_index, WASM_ARRAY_NEW_WITH_RTT(
                                      type_index, WASM_I32V(0), WASM_I32V(42),
                                      WASM_RTT_CANON(type_index))),
       kExprEnd});

  // Create an array of length 2, initialized to [42, 42].
  const byte kAllocate = tester.DefineFunction(
      &sig_q_v, {},
      {WASM_ARRAY_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(2),
                               WASM_RTT_CANON(type_index)),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(kGetElem, 12, 0);
  tester.CheckResult(kGetElem, 42, 1);
  tester.CheckResult(kGetElem, 12, 2);
  tester.CheckHasThrown(kGetElem, 3);
  tester.CheckHasThrown(kGetElem, -1);
  tester.CheckResult(kGetLength, 42);

  MaybeHandle<Object> h_result = tester.GetResultObject(kAllocate);
  CHECK(h_result.ToHandleChecked()->IsWasmArray());
#if OBJECT_PRINT
  h_result.ToHandleChecked()->Print();
#endif
}

TEST(WasmPackedArrayU) {
  WasmGCTester tester;
  const byte array_index = tester.DefineArray(kWasmI8, true);
  ValueType array_type = optref(array_index);

  const byte param_index = 0;
  const byte local_index = 1;

  int32_t expected_output_3 = 258;

  const byte kF = tester.DefineFunction(
      tester.sigs.i_i(), {array_type},
      {WASM_SET_LOCAL(local_index, WASM_ARRAY_NEW_WITH_RTT(
                                       array_index, WASM_I32V(0), WASM_I32V(4),
                                       WASM_RTT_CANON(array_index))),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(0),
                      WASM_I32V(1)),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(1),
                      WASM_I32V(10)),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(2),
                      WASM_I32V(200)),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(3),
                      WASM_I32V(expected_output_3)),
       WASM_ARRAY_GET_U(array_index, WASM_GET_LOCAL(local_index),
                        WASM_GET_LOCAL(param_index)),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kF, 1, 0);
  tester.CheckResult(kF, 10, 1);
  tester.CheckResult(kF, 200, 2);
  // Only the 2 lsb's of 258 should be stored in the array.
  tester.CheckResult(kF, static_cast<uint8_t>(expected_output_3), 3);
}

TEST(WasmPackedArrayS) {
  WasmGCTester tester;
  const byte array_index = tester.DefineArray(kWasmI16, true);
  ValueType array_type = optref(array_index);

  int32_t expected_outputs[] = {0x12345678, 10, 0xFEDC, 0xFF1234};

  const byte param_index = 0;
  const byte local_index = 1;
  const byte kF = tester.DefineFunction(
      tester.sigs.i_i(), {array_type},
      {WASM_SET_LOCAL(
           local_index,
           WASM_ARRAY_NEW_WITH_RTT(array_index, WASM_I32V(0x12345678),
                                   WASM_I32V(4), WASM_RTT_CANON(array_index))),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(1),
                      WASM_I32V(10)),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(2),
                      WASM_I32V(0xFEDC)),
       WASM_ARRAY_SET(array_index, WASM_GET_LOCAL(local_index), WASM_I32V(3),
                      WASM_I32V(0xFF1234)),
       WASM_ARRAY_GET_S(array_index, WASM_GET_LOCAL(local_index),
                        WASM_GET_LOCAL(param_index)),
       kExprEnd});

  tester.CompileModule();
  // Exactly the 2 lsb's should be stored by array.new.
  tester.CheckResult(kF, static_cast<int16_t>(expected_outputs[0]), 0);
  tester.CheckResult(kF, static_cast<int16_t>(expected_outputs[1]), 1);
  // Sign should be extended.
  tester.CheckResult(kF, static_cast<int16_t>(expected_outputs[2]), 2);
  // Exactly the 2 lsb's should be stored by array.set.
  tester.CheckResult(kF, static_cast<int16_t>(expected_outputs[3]), 3);
}

TEST(NewDefault) {
  WasmGCTester tester;
  const byte struct_type = tester.DefineStruct(
      {F(wasm::kWasmI32, true), F(wasm::kWasmF64, true), F(optref(0), true)});
  const byte array_type = tester.DefineArray(wasm::kWasmI32, true);
  // Returns: struct[0] + f64_to_i32(struct[1]) + (struct[2].is_null ^ 1) == 0.
  const byte allocate_struct = tester.DefineFunction(
      tester.sigs.i_v(), {optref(struct_type)},
      {WASM_SET_LOCAL(0, WASM_STRUCT_NEW_DEFAULT(struct_type,
                                                 WASM_RTT_CANON(struct_type))),
       WASM_I32_ADD(
           WASM_I32_ADD(WASM_STRUCT_GET(struct_type, 0, WASM_GET_LOCAL(0)),
                        WASM_I32_SCONVERT_F64(WASM_STRUCT_GET(
                            struct_type, 1, WASM_GET_LOCAL(0)))),
           WASM_I32_XOR(WASM_REF_IS_NULL(
                            WASM_STRUCT_GET(struct_type, 2, WASM_GET_LOCAL(0))),
                        WASM_I32V(1))),
       kExprEnd});
  const byte allocate_array = tester.DefineFunction(
      tester.sigs.i_v(), {optref(array_type)},
      {WASM_SET_LOCAL(0, WASM_ARRAY_NEW_DEFAULT(array_type, WASM_I32V(2),
                                                WASM_RTT_CANON(array_type))),
       WASM_I32_ADD(
           WASM_ARRAY_GET(array_type, WASM_GET_LOCAL(0), WASM_I32V(0)),
           WASM_ARRAY_GET(array_type, WASM_GET_LOCAL(0), WASM_I32V(1))),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(allocate_struct, 0);
  tester.CheckResult(allocate_array, 0);
}

TEST(BasicRTT) {
  WasmGCTester tester;
  const byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  const byte subtype_index =
      tester.DefineStruct({F(wasm::kWasmI32, true), F(wasm::kWasmI32, true)});
  ValueType kRttTypes[] = {ValueType::Rtt(type_index, 1)};
  FunctionSig sig_t_v(1, 0, kRttTypes);
  ValueType kRttSubtypes[] = {
      ValueType::Rtt(static_cast<HeapType>(subtype_index), 2)};
  FunctionSig sig_t2_v(1, 0, kRttSubtypes);
  ValueType kRttTypesDeeper[] = {ValueType::Rtt(type_index, 2)};
  FunctionSig sig_t3_v(1, 0, kRttTypesDeeper);
  ValueType kRefTypes[] = {ref(type_index)};
  FunctionSig sig_q_v(1, 0, kRefTypes);

  const byte kRttCanon = tester.DefineFunction(
      &sig_t_v, {}, {WASM_RTT_CANON(type_index), kExprEnd});
  const byte kRttSub = tester.DefineFunction(
      &sig_t2_v, {},
      {WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index)), kExprEnd});
  const byte kRttSubGeneric = tester.DefineFunction(
      &sig_t3_v, {},
      {WASM_RTT_SUB(type_index, WASM_RTT_CANON(kEqRefCode)), kExprEnd});
  const byte kStructWithRtt = tester.DefineFunction(
      &sig_q_v, {},
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});
  const int kFieldIndex = 1;
  const int kStructIndexCode = 1;  // Shifted in 'let' block.
  const int kRttIndexCode = 0;     // Let-bound, hence first local.
  // This implements the following function:
  //   var local_struct: type0;
  //   let (local_rtt = rtt.sub(rtt.canon(type0), type1) in {
  //     local_struct = new type1 with rtt 'local_rtt';
  //     return (ref.test local_struct local_rtt) +
  //            ((ref.cast local_struct local_rtt)[field0]);
  //   }
  // The expected return value is 1+42 = 43.
  const byte kRefCast = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LET_1_I(
          WASM_RTT(2, subtype_index),
          WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index)),
          WASM_SET_LOCAL(kStructIndexCode,
                         WASM_STRUCT_NEW_WITH_RTT(
                             subtype_index, WASM_I32V(11), WASM_I32V(42),
                             WASM_GET_LOCAL(kRttIndexCode))),
          WASM_I32_ADD(
              WASM_REF_TEST(type_index, subtype_index,
                            WASM_GET_LOCAL(kStructIndexCode),
                            WASM_GET_LOCAL(kRttIndexCode)),
              WASM_STRUCT_GET(subtype_index, kFieldIndex,
                              WASM_REF_CAST(type_index, subtype_index,
                                            WASM_GET_LOCAL(kStructIndexCode),
                                            WASM_GET_LOCAL(kRttIndexCode)))),
          kExprEnd)});

  tester.CompileModule();

  Handle<Object> ref_result =
      tester.GetResultObject(kRttCanon).ToHandleChecked();

  CHECK(ref_result->IsMap());
  Handle<Map> map = Handle<Map>::cast(ref_result);
  CHECK(map->IsWasmStructMap());
  CHECK_EQ(reinterpret_cast<Address>(
               tester.instance()->module()->struct_type(type_index)),
           map->wasm_type_info().foreign_address());

  Handle<Object> subref_result =
      tester.GetResultObject(kRttSub).ToHandleChecked();
  CHECK(subref_result->IsMap());
  Handle<Map> submap = Handle<Map>::cast(subref_result);
  CHECK_EQ(*map, submap->wasm_type_info().parent());
  CHECK_EQ(reinterpret_cast<Address>(
               tester.instance()->module()->struct_type(subtype_index)),
           submap->wasm_type_info().foreign_address());
  Handle<Object> subref_result_canonicalized =
      tester.GetResultObject(kRttSub).ToHandleChecked();
  CHECK(subref_result.is_identical_to(subref_result_canonicalized));

  Handle<Object> sub_generic_1 =
      tester.GetResultObject(kRttSubGeneric).ToHandleChecked();
  Handle<Object> sub_generic_2 =
      tester.GetResultObject(kRttSubGeneric).ToHandleChecked();
  CHECK(sub_generic_1.is_identical_to(sub_generic_2));

  Handle<Object> s = tester.GetResultObject(kStructWithRtt).ToHandleChecked();
  CHECK(s->IsWasmStruct());
  CHECK_EQ(Handle<WasmStruct>::cast(s)->map(), *map);

  tester.CheckResult(kRefCast, 43);
}

TEST(ArrayNewMap) {
  WasmGCTester tester;
  const byte type_index = tester.DefineArray(kWasmI32, true);

  ValueType array_type = ValueType::Ref(type_index, kNonNullable);
  FunctionSig sig(1, 0, &array_type);
  const byte array_new_with_rtt = tester.DefineFunction(
      &sig, {},
      {WASM_ARRAY_NEW_WITH_RTT(type_index, WASM_I32V(10), WASM_I32V(42),
                               WASM_RTT_CANON(type_index)),
       kExprEnd});

  ValueType rtt_type = ValueType::Rtt(type_index, 1);
  FunctionSig rtt_canon_sig(1, 0, &rtt_type);
  const byte kRttCanon = tester.DefineFunction(
      &rtt_canon_sig, {}, {WASM_RTT_CANON(type_index), kExprEnd});

  tester.CompileModule();

  Handle<Object> map = tester.GetResultObject(kRttCanon).ToHandleChecked();

  Handle<Object> result =
      tester.GetResultObject(array_new_with_rtt).ToHandleChecked();
  CHECK(result->IsWasmArray());
  CHECK_EQ(Handle<WasmArray>::cast(result)->map(), *map);
}

TEST(FunctionRefs) {
  WasmGCTester tester;
  const byte func_index =
      tester.DefineFunction(tester.sigs.i_v(), {}, {WASM_I32V(42), kExprEnd});
  const byte sig_index = 0;

  const byte other_sig_index = tester.DefineSignature(tester.sigs.d_d());

  // This is just so func_index counts as "declared".
  tester.AddGlobal(ValueType::Ref(sig_index, kNullable), false,
                   WasmInitExpr::RefFuncConst(func_index));

  ValueType func_type = ValueType::Ref(sig_index, kNonNullable);
  FunctionSig sig_func(1, 0, &func_type);

  ValueType rtt1 = ValueType::Rtt(sig_index, 1);
  FunctionSig sig_rtt1(1, 0, &rtt1);
  const byte rtt_canon = tester.DefineFunction(
      &sig_rtt1, {}, {WASM_RTT_CANON(sig_index), kExprEnd});

  ValueType rtt2 = ValueType::Rtt(sig_index, 2);
  FunctionSig sig_rtt2(1, 0, &rtt2);
  const byte rtt_sub = tester.DefineFunction(
      &sig_rtt2, {},
      {WASM_RTT_SUB(sig_index, WASM_RTT_CANON(kFuncRefCode)), kExprEnd});

  const byte cast = tester.DefineFunction(
      &sig_func, {kWasmFuncRef},
      {WASM_SET_LOCAL(0, WASM_REF_FUNC(func_index)),
       WASM_REF_CAST(kFuncRefCode, sig_index, WASM_GET_LOCAL(0),
                     WASM_RTT_CANON(sig_index)),
       kExprEnd});

  const byte cast_reference = tester.DefineFunction(
      &sig_func, {}, {WASM_REF_FUNC(sig_index), kExprEnd});

  const byte test = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmFuncRef},
      {WASM_SET_LOCAL(0, WASM_REF_FUNC(func_index)),
       WASM_REF_TEST(kFuncRefCode, other_sig_index, WASM_GET_LOCAL(0),
                     WASM_RTT_CANON(other_sig_index)),
       kExprEnd});

  tester.CompileModule();

  Handle<Object> result_canon =
      tester.GetResultObject(rtt_canon).ToHandleChecked();
  CHECK(result_canon->IsMap());
  Handle<Map> map_canon = Handle<Map>::cast(result_canon);
  CHECK(map_canon->IsJSFunctionMap());

  Handle<Object> result_sub = tester.GetResultObject(rtt_sub).ToHandleChecked();
  CHECK(result_sub->IsMap());
  Handle<Map> map_sub = Handle<Map>::cast(result_sub);
  CHECK(map_sub->IsJSFunctionMap());

  Handle<Object> result_cast = tester.GetResultObject(cast).ToHandleChecked();
  CHECK(result_cast->IsJSFunction());
  Handle<JSFunction> cast_function = Handle<JSFunction>::cast(result_cast);

  Handle<Object> result_cast_reference =
      tester.GetResultObject(cast_reference).ToHandleChecked();
  CHECK(result_cast_reference->IsJSFunction());
  Handle<JSFunction> cast_function_reference =
      Handle<JSFunction>::cast(result_cast_reference);

  CHECK_EQ(cast_function->code().raw_instruction_start(),
           cast_function_reference->code().raw_instruction_start());

  tester.CheckResult(test, 0);
}

TEST(CallRef) {
  WasmGCTester tester;
  byte callee = tester.DefineFunction(
      tester.sigs.i_ii(), {},
      {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), kExprEnd});
  byte caller = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_CALL_REF(WASM_REF_FUNC(callee), WASM_I32V(42), WASM_GET_LOCAL(0)),
       kExprEnd});

  // This is just so func_index counts as "declared".
  tester.AddGlobal(ValueType::Ref(0, kNullable), false,
                   WasmInitExpr::RefFuncConst(callee));

  tester.CompileModule();

  tester.CheckResult(caller, 47, 5);
}

TEST(RefTestCastNull) {
  WasmGCTester tester;
  byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});

  const byte kRefTestNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(type_index, type_index, WASM_REF_NULL(type_index),
                     WASM_RTT_CANON(type_index)),
       kExprEnd});

  const byte kRefCastNull = tester.DefineFunction(
      tester.sigs.i_i(),  // Argument and return value ignored
      {},
      {WASM_REF_CAST(type_index, type_index, WASM_REF_NULL(type_index),
                     WASM_RTT_CANON(type_index)),
       kExprDrop, WASM_I32V(0), kExprEnd});
  tester.CompileModule();
  tester.CheckResult(kRefTestNull, 0);
  tester.CheckHasThrown(kRefCastNull, 0);
}

TEST(BasicI31) {
  WasmGCTester tester;
  const byte kSigned = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_I31_GET_S(WASM_I31_NEW(WASM_GET_LOCAL(0))), kExprEnd});
  const byte kUnsigned = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_I31_GET_U(WASM_I31_NEW(WASM_GET_LOCAL(0))), kExprEnd});
  // TODO(7748): Support (rtt.canon i31), and add a test like:
  // (ref.test (i31.new ...) (rtt.canon i31)).
  tester.CompileModule();
  tester.CheckResult(kSigned, 123, 123);
  tester.CheckResult(kUnsigned, 123, 123);
  // Truncation:
  tester.CheckResult(kSigned, 0x1234, static_cast<int32_t>(0x80001234));
  tester.CheckResult(kUnsigned, 0x1234, static_cast<int32_t>(0x80001234));
  // Sign/zero extension:
  tester.CheckResult(kSigned, -1, 0x7FFFFFFF);
  tester.CheckResult(kUnsigned, 0x7FFFFFFF, 0x7FFFFFFF);
}

TEST(I31Casts) {
  WasmGCTester tester;
  const byte struct_type = tester.DefineStruct({F(wasm::kWasmI32, true)});
  const byte i31_rtt =
      tester.AddGlobal(ValueType::Rtt(HeapType::kI31, 1), false,
                       WasmInitExpr::RttCanon(HeapType::kI31));
  const byte struct_rtt =
      tester.AddGlobal(ValueType::Rtt(struct_type, 1), false,
                       WasmInitExpr::RttCanon(
                           static_cast<HeapType::Representation>(struct_type)));
  // Adds the result of a successful typecheck to the untagged value, i.e.
  // should return 1 + 42 = 43.
  const byte kTestAndCastSuccess = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmEqRef},
      {WASM_SET_LOCAL(0, WASM_I31_NEW(WASM_I32V(42))),
       WASM_I32_ADD(WASM_REF_TEST(kEqRefCode, kI31RefCode, WASM_GET_LOCAL(0),
                                  WASM_GET_GLOBAL(i31_rtt)),
                    WASM_I31_GET_S(WASM_REF_CAST(kEqRefCode, kI31RefCode,
                                                 WASM_GET_LOCAL(0),
                                                 WASM_GET_GLOBAL(i31_rtt)))),
       kExprEnd});
  // Adds the results of two unsuccessful type checks (an i31ref is not a
  // struct, nor the other way round).
  const byte kTestFalse = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_I32_ADD(
           WASM_REF_TEST(kEqRefCode, kI31RefCode,
                         WASM_STRUCT_NEW_WITH_RTT(struct_type, WASM_I32V(42),
                                                  WASM_GET_GLOBAL(struct_rtt)),
                         WASM_GET_GLOBAL(i31_rtt)),
           WASM_REF_TEST(kEqRefCode, struct_type, WASM_I31_NEW(WASM_I32V(23)),
                         WASM_GET_GLOBAL(struct_rtt))),
       kExprEnd});
  // Tries to cast an i31ref to a struct, which should trap.
  const byte kCastI31ToStruct = tester.DefineFunction(
      tester.sigs.i_i(),  // Argument and return value ignored
      {},
      {WASM_STRUCT_GET(
           struct_type, 0,
           WASM_REF_CAST(kEqRefCode, struct_type, WASM_I31_NEW(WASM_I32V(42)),
                         WASM_GET_GLOBAL(struct_rtt))),
       kExprEnd});
  // Tries to cast a struct to i31ref, which should trap.
  const byte kCastStructToI31 = tester.DefineFunction(
      tester.sigs.i_i(),  // Argument and return value ignored
      {},
      {WASM_I31_GET_S(
           WASM_REF_CAST(kEqRefCode, kI31RefCode,
                         WASM_STRUCT_NEW_WITH_RTT(struct_type, WASM_I32V(42),
                                                  WASM_GET_GLOBAL(struct_rtt)),
                         WASM_GET_GLOBAL(i31_rtt))),
       kExprEnd});
  tester.CompileModule();
  tester.CheckResult(kTestAndCastSuccess, 43);
  tester.CheckResult(kTestFalse, 0);
  tester.CheckHasThrown(kCastI31ToStruct, 0);
  tester.CheckHasThrown(kCastStructToI31, 0);
}

TEST(GlobalInitReferencingGlobal) {
  WasmGCTester tester;
  const byte from = tester.AddGlobal(kWasmI32, false, WasmInitExpr(42));
  const byte to =
      tester.AddGlobal(kWasmI32, false, WasmInitExpr::GlobalGet(from));

  const byte func = tester.DefineFunction(tester.sigs.i_v(), {},
                                          {WASM_GET_GLOBAL(to), kExprEnd});

  tester.CompileModule();

  tester.CheckResult(func, 42);
}

TEST(IndirectNullSetManually) {
  WasmGCTester tester;
  byte sig_index = tester.DefineSignature(tester.sigs.i_i());
  tester.DefineTable(ValueType::Ref(sig_index, kNullable), 1, 1);
  byte func_index = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_TABLE_SET(0, WASM_I32V(0), WASM_REF_NULL(sig_index)),
       WASM_CALL_INDIRECT(sig_index, WASM_I32V(0), WASM_GET_LOCAL(0)),
       kExprEnd});

  tester.CompileModule();

  tester.CheckHasThrown(func_index, 42);
}

TEST(JsAccess) {
  WasmGCTester tester;
  const byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  ValueType kRefTypes[] = {ref(type_index)};
  ValueType kEqRefTypes[] = {kWasmEqRef};
  ValueType kEqToI[] = {kWasmI32, kWasmEqRef};
  FunctionSig sig_t_v(1, 0, kRefTypes);
  FunctionSig sig_q_v(1, 0, kEqRefTypes);
  FunctionSig sig_i_q(1, 1, kEqToI);

  tester.DefineExportedFunction(
      "disallowed", &sig_t_v,
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});
  // Same code, different signature.
  tester.DefineExportedFunction(
      "producer", &sig_q_v,
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});
  tester.DefineExportedFunction(
      "consumer", &sig_i_q,
      {WASM_STRUCT_GET(type_index, 0,
                       WASM_REF_CAST(kEqRefCode, type_index, WASM_GET_LOCAL(0),
                                     WASM_RTT_CANON(type_index))),
       kExprEnd});

  tester.CompileModule();
  Isolate* isolate = tester.isolate();
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  MaybeHandle<Object> maybe_result =
      tester.CallExportedFunction("disallowed", 0, nullptr);
  CHECK(maybe_result.is_null());
  CHECK(try_catch.HasCaught());
  try_catch.Reset();
  isolate->clear_pending_exception();

  maybe_result = tester.CallExportedFunction("producer", 0, nullptr);
  if (maybe_result.is_null()) {
    FATAL("Calling 'producer' failed: %s",
          *v8::String::Utf8Value(reinterpret_cast<v8::Isolate*>(isolate),
                                 try_catch.Message()->Get()));
  }
  {
    Handle<Object> args[] = {maybe_result.ToHandleChecked()};
    maybe_result = tester.CallExportedFunction("consumer", 1, args);
  }
  if (maybe_result.is_null()) {
    FATAL("Calling 'consumer' failed: %s",
          *v8::String::Utf8Value(reinterpret_cast<v8::Isolate*>(isolate),
                                 try_catch.Message()->Get()));
  }
  Handle<Object> result = maybe_result.ToHandleChecked();
  CHECK(result->IsSmi());
  CHECK_EQ(42, Smi::cast(*result).value());
  // Calling {consumer} with any other object (e.g. the Smi we just got as
  // {result}) should trap.
  {
    Handle<Object> args[] = {result};
    maybe_result = tester.CallExportedFunction("consumer", 1, args);
  }
  CHECK(maybe_result.is_null());
  CHECK(try_catch.HasCaught());
  try_catch.Reset();
  isolate->clear_pending_exception();
}

}  // namespace test_gc
}  // namespace wasm
}  // namespace internal
}  // namespace v8

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
  explicit WasmGCTester(
      TestExecutionTier execution_tier = TestExecutionTier::kTurbofan)
      : flag_gc(&v8::internal::FLAG_experimental_wasm_gc, true),
        flag_reftypes(&v8::internal::FLAG_experimental_wasm_reftypes, true),
        flag_typedfuns(&v8::internal::FLAG_experimental_wasm_typed_funcref,
                       true),
        flag_liftoff(
            &v8::internal::FLAG_liftoff,
            execution_tier == TestExecutionTier::kTurbofan ? false : true),
        flag_liftoff_only(
            &v8::internal::FLAG_liftoff_only,
            execution_tier == TestExecutionTier::kLiftoff ? true : false),
        flag_tierup(&v8::internal::FLAG_wasm_tier_up, false),
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
    CHECK(!isolate_->has_pending_exception());
    packer.Reset();
    CHECK_EQ(expected, packer.Pop<int32_t>());
  }

  void CheckResult(uint32_t function_index, int32_t expected, int32_t arg) {
    FunctionSig* sig = sigs.i_i();
    DCHECK(*sig == *instance_->module()->functions[function_index].sig);
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    packer.Push(arg);
    CallFunctionImpl(function_index, sig, &packer);
    CHECK(!isolate_->has_pending_exception());
    packer.Reset();
    CHECK_EQ(expected, packer.Pop<int32_t>());
  }

  MaybeHandle<Object> GetResultObject(uint32_t function_index) {
    const FunctionSig* sig = instance_->module()->functions[function_index].sig;
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    CallFunctionImpl(function_index, sig, &packer);
    CHECK(!isolate_->has_pending_exception());
    packer.Reset();
    return Handle<Object>(Object(packer.Pop<Address>()), isolate_);
  }

  void CheckHasThrown(uint32_t function_index) {
    const FunctionSig* sig = instance_->module()->functions[function_index].sig;
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    CallFunctionImpl(function_index, sig, &packer);
    CHECK(isolate_->has_pending_exception());
    isolate_->clear_pending_exception();
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
  const FlagScope<bool> flag_liftoff;
  const FlagScope<bool> flag_liftoff_only;
  const FlagScope<bool> flag_tierup;

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

WASM_COMPILED_EXEC_TEST(WasmBasicStruct) {
  WasmGCTester tester(execution_tier);

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
      {WASM_LOCAL_SET(
           j_local_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42), WASM_I32V(64),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_SET(type_index, j_field_index, WASM_LOCAL_GET(j_local_index),
                       WASM_I32V(-99)),
       WASM_STRUCT_GET(type_index, j_field_index,
                       WASM_LOCAL_GET(j_local_index)),
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
WASM_COMPILED_EXEC_TEST(WasmRefAsNonNull) {
  WasmGCTester tester(execution_tier);
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
      {WASM_GLOBAL_SET(
           global_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(55), WASM_I32V(66),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET(
           type_index, field_index,
           WASM_REF_AS_NON_NULL(WASM_IF_ELSE_R(kOptRefType, WASM_I32V(1),
                                               WASM_GLOBAL_GET(global_index),
                                               WASM_REF_NULL(type_index)))),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kFunc, 55);
}

WASM_COMPILED_EXEC_TEST(WasmBrOnNull) {
  WasmGCTester tester(execution_tier);
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
                    WASM_BR_ON_NULL(0, WASM_LOCAL_GET(l_local_index)),
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

WASM_COMPILED_EXEC_TEST(BrOnCast) {
  WasmGCTester tester(execution_tier);
  ValueType kDataRefNull = ValueType::Ref(HeapType::kData, kNullable);
  const byte type_index = tester.DefineStruct({F(kWasmI32, true)});
  const byte other_type_index = tester.DefineStruct({F(kWasmF32, true)});
  const byte rtt_index =
      tester.AddGlobal(ValueType::Rtt(type_index, 0), false,
                       WasmInitExpr::RttCanon(
                           static_cast<HeapType::Representation>(type_index)));
  const byte kTestStruct = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kDataRefNull},
      {WASM_BLOCK_R(ValueType::Ref(type_index, kNullable),
                    WASM_LOCAL_SET(0, WASM_I32V(111)),
                    // Pipe a struct through a local so it's statically typed
                    // as dataref.
                    WASM_LOCAL_SET(1, WASM_STRUCT_NEW_WITH_RTT(
                                          other_type_index, WASM_F32(1.0),
                                          WASM_RTT_CANON(other_type_index))),
                    WASM_LOCAL_GET(1),
                    // The type check fails, so this branch isn't taken.
                    WASM_BR_ON_CAST(0, WASM_GLOBAL_GET(rtt_index)), WASM_DROP,

                    WASM_LOCAL_SET(0, WASM_I32V(221)),  // (Final result) - 1
                    WASM_LOCAL_SET(1, WASM_STRUCT_NEW_WITH_RTT(
                                          type_index, WASM_I32V(1),
                                          WASM_GLOBAL_GET(rtt_index))),
                    WASM_LOCAL_GET(1),
                    // This branch is taken.
                    WASM_BR_ON_CAST(0, WASM_GLOBAL_GET(rtt_index)),
                    WASM_GLOBAL_GET(rtt_index), WASM_GC_OP(kExprRefCast),

                    // Not executed due to the branch.
                    WASM_LOCAL_SET(0, WASM_I32V(333))),
       WASM_GC_OP(kExprStructGet), type_index, 0, WASM_LOCAL_GET(0),
       kExprI32Add, kExprEnd});

  const byte kTestNull = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kDataRefNull},
      {WASM_BLOCK_R(ValueType::Ref(type_index, kNullable),
                    WASM_LOCAL_SET(0, WASM_I32V(111)),
                    WASM_LOCAL_GET(1),  // Put a nullref onto the value stack.
                    // Not taken for nullref.
                    WASM_BR_ON_CAST(0, WASM_GLOBAL_GET(rtt_index)),
                    WASM_RTT_CANON(type_index), WASM_GC_OP(kExprRefCast),

                    WASM_LOCAL_SET(0, WASM_I32V(222))),  // Final result.
       WASM_DROP, WASM_LOCAL_GET(0), kExprEnd});

  const byte kTypedAfterBranch = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32, kDataRefNull},
      {WASM_LOCAL_SET(1, WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                                  WASM_GLOBAL_GET(rtt_index))),
       WASM_BLOCK_I(
           // The inner block should take the early branch with a struct
           // on the stack.
           WASM_BLOCK_R(ValueType::Ref(type_index, kNonNullable),
                        WASM_LOCAL_GET(1),
                        WASM_BR_ON_CAST(0, WASM_GLOBAL_GET(rtt_index)),
                        // Returning 123 is the unreachable failure case.
                        WASM_I32V(123), WASM_BR(1)),
           // The outer block catches the struct left behind by the inner block
           // and reads its field.
           WASM_GC_OP(kExprStructGet), type_index, 0),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kTestStruct, 222);
  tester.CheckResult(kTestNull, 222);
  tester.CheckResult(kTypedAfterBranch, 42);
}

WASM_COMPILED_EXEC_TEST(WasmRefEq) {
  WasmGCTester tester(execution_tier);
  byte type_index = tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});
  ValueType kRefTypes[] = {ref(type_index)};
  ValueType kOptRefType = optref(type_index);
  FunctionSig sig_q_v(1, 0, kRefTypes);

  byte local_index = 0;
  const byte kFunc = tester.DefineFunction(
      tester.sigs.i_v(), {kOptRefType},
      {WASM_LOCAL_SET(local_index, WASM_STRUCT_NEW_WITH_RTT(
                                       type_index, WASM_I32V(55), WASM_I32V(66),
                                       WASM_RTT_CANON(type_index))),
       WASM_I32_ADD(
           WASM_I32_SHL(
               WASM_REF_EQ(  // true
                   WASM_LOCAL_GET(local_index), WASM_LOCAL_GET(local_index)),
               WASM_I32V(0)),
           WASM_I32_ADD(
               WASM_I32_SHL(WASM_REF_EQ(  // false
                                WASM_LOCAL_GET(local_index),
                                WASM_STRUCT_NEW_WITH_RTT(
                                    type_index, WASM_I32V(55), WASM_I32V(66),
                                    WASM_RTT_CANON(type_index))),
                            WASM_I32V(1)),
               WASM_I32_ADD(WASM_I32_SHL(  // false
                                WASM_REF_EQ(WASM_LOCAL_GET(local_index),
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

WASM_COMPILED_EXEC_TEST(WasmPackedStructU) {
  WasmGCTester tester(execution_tier);

  const byte type_index = tester.DefineStruct(
      {F(kWasmI8, true), F(kWasmI16, true), F(kWasmI32, true)});
  ValueType struct_type = optref(type_index);

  const byte local_index = 0;

  int32_t expected_output_0 = 0x1234;
  int32_t expected_output_1 = -1;

  const byte kF0 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_LOCAL_SET(local_index,
                      WASM_STRUCT_NEW_WITH_RTT(
                          type_index, WASM_I32V(expected_output_0),
                          WASM_I32V(expected_output_1), WASM_I32V(0x12345678),
                          WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_U(type_index, 0, WASM_LOCAL_GET(local_index)),
       kExprEnd});

  const byte kF1 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_LOCAL_SET(local_index,
                      WASM_STRUCT_NEW_WITH_RTT(
                          type_index, WASM_I32V(expected_output_0),
                          WASM_I32V(expected_output_1), WASM_I32V(0x12345678),
                          WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_U(type_index, 1, WASM_LOCAL_GET(local_index)),
       kExprEnd});
  tester.CompileModule();

  tester.CheckResult(kF0, static_cast<uint8_t>(expected_output_0));
  tester.CheckResult(kF1, static_cast<uint16_t>(expected_output_1));
}

WASM_COMPILED_EXEC_TEST(WasmPackedStructS) {
  WasmGCTester tester(execution_tier);

  const byte type_index = tester.DefineStruct(
      {F(kWasmI8, true), F(kWasmI16, true), F(kWasmI32, true)});
  ValueType struct_type = optref(type_index);

  const byte local_index = 0;

  int32_t expected_output_0 = 0x80;
  int32_t expected_output_1 = 42;

  const byte kF0 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_LOCAL_SET(
           local_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(expected_output_0),
                                    WASM_I32V(expected_output_1), WASM_I32V(0),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_S(type_index, 0, WASM_LOCAL_GET(local_index)),
       kExprEnd});

  const byte kF1 = tester.DefineFunction(
      tester.sigs.i_v(), {struct_type},
      {WASM_LOCAL_SET(
           local_index,
           WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(0x80),
                                    WASM_I32V(expected_output_1), WASM_I32V(0),
                                    WASM_RTT_CANON(type_index))),
       WASM_STRUCT_GET_S(type_index, 1, WASM_LOCAL_GET(local_index)),
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
                           WASM_LOCAL_GET(let_local_index))),
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
                                        WASM_LOCAL_GET(1)),
                        WASM_LOCAL_GET(0))),
       kExprEnd});

  const byte kLetTestLocals = tester.DefineFunction(
      tester.sigs.i_i(), {kWasmI32},
      {WASM_LOCAL_SET(1, WASM_I32V(100)),
       WASM_LET_2_I(
           kI32Code, WASM_I32V(1), kI32Code, WASM_I32V(10),
           WASM_I32_SUB(WASM_I32_ADD(WASM_LOCAL_GET(0),     // 1st let-local
                                     WASM_LOCAL_GET(2)),    // Parameter
                        WASM_I32_ADD(WASM_LOCAL_GET(1),     // 2nd let-local
                                     WASM_LOCAL_GET(3)))),  // Function local
       kExprEnd});
  // Result: (1 + 1000) - (10 + 100) = 891

  const byte let_erase_local_index = 0;
  const byte kLetTestErase = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmI32},
      {WASM_LOCAL_SET(let_erase_local_index, WASM_I32V(0)),
       WASM_LET_1_V(kI32Code, WASM_I32V(1), WASM_NOP),
       WASM_LOCAL_GET(let_erase_local_index), kExprEnd});
  // The result should be 0 and not 1, as local_get(0) refers to the original
  // local.

  const byte kLetInLoop = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_LOOP(WASM_LET_1_V(
           kI32Code, WASM_I32V(10),  // --
           WASM_LOCAL_SET(1, WASM_I32_SUB(WASM_LOCAL_GET(1), WASM_I32V(10))),
           WASM_BR_IF(1, WASM_I32_GES(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))))),
       WASM_LOCAL_GET(0), WASM_END});

  const byte kLetInBlock = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_BLOCK(WASM_LET_1_V(
           kI32Code, WASM_I32V(10),  // --
           WASM_BR_IF(1, WASM_I32_GES(WASM_LOCAL_GET(1), WASM_LOCAL_GET(0))),
           WASM_LOCAL_SET(1, WASM_I32V(30)))),
       WASM_LOCAL_GET(0), WASM_END});

  tester.CompileModule();

  tester.CheckResult(kLetTest1, 42);
  tester.CheckResult(kLetTest2, 420);
  tester.CheckResult(kLetTestLocals, 891, 1000);
  tester.CheckResult(kLetTestErase, 0);
  tester.CheckResult(kLetInLoop, 2, 52);
  tester.CheckResult(kLetInLoop, -11, -1);
  tester.CheckResult(kLetInBlock, 15, 15);
  tester.CheckResult(kLetInBlock, 30, 5);
}

WASM_COMPILED_EXEC_TEST(WasmBasicArray) {
  WasmGCTester tester(execution_tier);

  const byte type_index = tester.DefineArray(wasm::kWasmI32, true);
  const byte fp_type_index = tester.DefineArray(wasm::kWasmF64, true);
  ValueType kRefTypes[] = {ref(type_index)};
  FunctionSig sig_q_v(1, 0, kRefTypes);
  ValueType kOptRefType = optref(type_index);

  // f: a = [12, 12, 12]; a[1] = 42; return a[arg0]
  const byte local_index = 1;
  const byte kGetElem = tester.DefineFunction(
      tester.sigs.i_i(), {kOptRefType},
      {WASM_LOCAL_SET(local_index, WASM_ARRAY_NEW_WITH_RTT(
                                       type_index, WASM_I32V(12), WASM_I32V(3),
                                       WASM_RTT_CANON(type_index))),
       WASM_ARRAY_SET(type_index, WASM_LOCAL_GET(local_index), WASM_I32V(1),
                      WASM_I32V(42)),
       WASM_ARRAY_GET(type_index, WASM_LOCAL_GET(local_index),
                      WASM_LOCAL_GET(0)),
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

  const uint32_t kLongLength = 1u << 16;
  const byte kAllocateLarge = tester.DefineFunction(
      &sig_q_v, {},
      {WASM_ARRAY_NEW_DEFAULT(type_index, WASM_I32V(kLongLength),
                              WASM_RTT_CANON(type_index)),
       kExprEnd});

  const uint32_t kTooLong = kV8MaxWasmArrayLength + 1;
  const byte kAllocateTooLarge = tester.DefineFunction(
      &sig_q_v, {},
      {WASM_ARRAY_NEW_DEFAULT(type_index, WASM_I32V(kTooLong),
                              WASM_RTT_CANON(type_index)),
       kExprEnd});

  // Tests that fp arrays work properly.
  // f: a = [10.0, 10.0, 10.0]; a[1] = 42.42; return static_cast<int64>(a[1]);
  double result_value = 42.42;
  const byte kTestFpArray = tester.DefineFunction(
      tester.sigs.i_v(), {optref(fp_type_index)},
      {WASM_LOCAL_SET(0, WASM_ARRAY_NEW_WITH_RTT(
                             fp_type_index, WASM_F64(10.0), WASM_I32V(3),
                             WASM_RTT_CANON(fp_type_index))),
       WASM_ARRAY_SET(fp_type_index, WASM_LOCAL_GET(0), WASM_I32V(1),
                      WASM_F64(result_value)),
       WASM_I32_SCONVERT_F64(
           WASM_ARRAY_GET(fp_type_index, WASM_LOCAL_GET(0), WASM_I32V(1))),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(kGetElem, 12, 0);
  tester.CheckResult(kGetElem, 42, 1);
  tester.CheckResult(kGetElem, 12, 2);
  tester.CheckHasThrown(kGetElem, 3);
  tester.CheckHasThrown(kGetElem, -1);
  tester.CheckResult(kGetLength, 42);
  tester.CheckResult(kTestFpArray, static_cast<int32_t>(result_value));

  MaybeHandle<Object> h_result = tester.GetResultObject(kAllocate);
  CHECK(h_result.ToHandleChecked()->IsWasmArray());
#if OBJECT_PRINT
  h_result.ToHandleChecked()->Print();
#endif

  MaybeHandle<Object> maybe_large_result =
      tester.GetResultObject(kAllocateLarge);
  Handle<Object> large_result = maybe_large_result.ToHandleChecked();
  CHECK(large_result->IsWasmArray());
  CHECK(Handle<WasmArray>::cast(large_result)->Size() >
        kMaxRegularHeapObjectSize);

  tester.CheckHasThrown(kAllocateTooLarge);
}

WASM_COMPILED_EXEC_TEST(WasmPackedArrayU) {
  WasmGCTester tester(execution_tier);
  const byte array_index = tester.DefineArray(kWasmI8, true);
  ValueType array_type = optref(array_index);

  const byte param_index = 0;
  const byte local_index = 1;

  int32_t expected_output_3 = 258;

  const byte kF = tester.DefineFunction(
      tester.sigs.i_i(), {array_type},
      {WASM_LOCAL_SET(local_index, WASM_ARRAY_NEW_WITH_RTT(
                                       array_index, WASM_I32V(0), WASM_I32V(4),
                                       WASM_RTT_CANON(array_index))),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(0),
                      WASM_I32V(1)),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(1),
                      WASM_I32V(10)),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(2),
                      WASM_I32V(200)),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(3),
                      WASM_I32V(expected_output_3)),
       WASM_ARRAY_GET_U(array_index, WASM_LOCAL_GET(local_index),
                        WASM_LOCAL_GET(param_index)),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(kF, 1, 0);
  tester.CheckResult(kF, 10, 1);
  tester.CheckResult(kF, 200, 2);
  // Only the 2 lsb's of 258 should be stored in the array.
  tester.CheckResult(kF, static_cast<uint8_t>(expected_output_3), 3);
}

WASM_COMPILED_EXEC_TEST(WasmPackedArrayS) {
  WasmGCTester tester(execution_tier);
  const byte array_index = tester.DefineArray(kWasmI16, true);
  ValueType array_type = optref(array_index);

  int32_t expected_outputs[] = {0x12345678, 10, 0xFEDC, 0xFF1234};

  const byte param_index = 0;
  const byte local_index = 1;
  const byte kF = tester.DefineFunction(
      tester.sigs.i_i(), {array_type},
      {WASM_LOCAL_SET(
           local_index,
           WASM_ARRAY_NEW_WITH_RTT(array_index, WASM_I32V(0x12345678),
                                   WASM_I32V(4), WASM_RTT_CANON(array_index))),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(1),
                      WASM_I32V(10)),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(2),
                      WASM_I32V(0xFEDC)),
       WASM_ARRAY_SET(array_index, WASM_LOCAL_GET(local_index), WASM_I32V(3),
                      WASM_I32V(0xFF1234)),
       WASM_ARRAY_GET_S(array_index, WASM_LOCAL_GET(local_index),
                        WASM_LOCAL_GET(param_index)),
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

WASM_COMPILED_EXEC_TEST(NewDefault) {
  WasmGCTester tester(execution_tier);
  const byte struct_type = tester.DefineStruct(
      {F(wasm::kWasmI32, true), F(wasm::kWasmF64, true), F(optref(0), true)});
  const byte array_type = tester.DefineArray(wasm::kWasmI32, true);
  // Returns: struct[0] + f64_to_i32(struct[1]) + (struct[2].is_null ^ 1) == 0.
  const byte allocate_struct = tester.DefineFunction(
      tester.sigs.i_v(), {optref(struct_type)},
      {WASM_LOCAL_SET(0, WASM_STRUCT_NEW_DEFAULT(struct_type,
                                                 WASM_RTT_CANON(struct_type))),
       WASM_I32_ADD(
           WASM_I32_ADD(WASM_STRUCT_GET(struct_type, 0, WASM_LOCAL_GET(0)),
                        WASM_I32_SCONVERT_F64(WASM_STRUCT_GET(
                            struct_type, 1, WASM_LOCAL_GET(0)))),
           WASM_I32_XOR(WASM_REF_IS_NULL(
                            WASM_STRUCT_GET(struct_type, 2, WASM_LOCAL_GET(0))),
                        WASM_I32V(1))),
       kExprEnd});
  const byte allocate_array = tester.DefineFunction(
      tester.sigs.i_v(), {optref(array_type)},
      {WASM_LOCAL_SET(0, WASM_ARRAY_NEW_DEFAULT(array_type, WASM_I32V(2),
                                                WASM_RTT_CANON(array_type))),
       WASM_I32_ADD(
           WASM_ARRAY_GET(array_type, WASM_LOCAL_GET(0), WASM_I32V(0)),
           WASM_ARRAY_GET(array_type, WASM_LOCAL_GET(0), WASM_I32V(1))),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(allocate_struct, 0);
  tester.CheckResult(allocate_array, 0);
}

WASM_COMPILED_EXEC_TEST(BasicRtt) {
  WasmGCTester tester(execution_tier);

  const byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  const byte subtype_index =
      tester.DefineStruct({F(wasm::kWasmI32, true), F(wasm::kWasmI32, true)});

  ValueType kRttTypes[] = {ValueType::Rtt(type_index, 0)};
  FunctionSig sig_t_v(1, 0, kRttTypes);
  ValueType kRttSubtypes[] = {ValueType::Rtt(subtype_index, 1)};
  FunctionSig sig_t2_v(1, 0, kRttSubtypes);
  ValueType kRttTypesDeeper[] = {ValueType::Rtt(type_index, 1)};
  FunctionSig sig_t3_v(1, 0, kRttTypesDeeper);
  ValueType kRefTypes[] = {ref(type_index)};
  FunctionSig sig_q_v(1, 0, kRefTypes);

  const byte kRttCanon = tester.DefineFunction(
      &sig_t_v, {}, {WASM_RTT_CANON(type_index), kExprEnd});
  const byte kRttSub = tester.DefineFunction(
      &sig_t2_v, {},
      {WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index)), kExprEnd});
  const byte kStructWithRtt = tester.DefineFunction(
      &sig_q_v, {},
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});

  const int kFieldIndex = 1;
  const int kStructIndexCode = 0;
  // This implements the following function:
  //   var local_struct: type0;
  //   local_struct = new type1 with rtt 'kRttSub()';
  //   return (ref.test local_struct kRttSub()) +
  //          ((ref.cast local_struct kRttSub())[field0]);
  //   }
  // The expected return value is 1+42 = 43.
  const byte kRefCast = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(
           kStructIndexCode,
           WASM_STRUCT_NEW_WITH_RTT(subtype_index, WASM_I32V(11), WASM_I32V(42),
                                    WASM_CALL_FUNCTION0(kRttSub))),
       WASM_I32_ADD(
           WASM_REF_TEST(WASM_LOCAL_GET(kStructIndexCode),
                         WASM_CALL_FUNCTION0(kRttSub)),
           WASM_STRUCT_GET(subtype_index, kFieldIndex,
                           WASM_REF_CAST(WASM_LOCAL_GET(kStructIndexCode),
                                         WASM_CALL_FUNCTION0(kRttSub)))),
       kExprEnd});

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
  CHECK_EQ(reinterpret_cast<Address>(
               tester.instance()->module()->struct_type(subtype_index)),
           submap->wasm_type_info().foreign_address());
  Handle<Object> subref_result_canonicalized =
      tester.GetResultObject(kRttSub).ToHandleChecked();
  CHECK(subref_result.is_identical_to(subref_result_canonicalized));

  Handle<Object> s = tester.GetResultObject(kStructWithRtt).ToHandleChecked();
  CHECK(s->IsWasmStruct());
  CHECK_EQ(Handle<WasmStruct>::cast(s)->map(), *map);

  tester.CheckResult(kRefCast, 43);
}

WASM_COMPILED_EXEC_TEST(RefTrivialCasts) {
  WasmGCTester tester(execution_tier);
  byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  byte subtype_index =
      tester.DefineStruct({F(wasm::kWasmI32, true), F(wasm::kWasmS128, false)});
  ValueType sig_types[] = {kWasmS128, kWasmI32, kWasmF64};
  FunctionSig sig(1, 2, sig_types);
  byte sig_index = tester.DefineSignature(&sig);

  const byte kRefTestNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(WASM_REF_NULL(type_index), WASM_RTT_CANON(subtype_index)),
       kExprEnd});
  const byte kRefTestUpcast = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(
           WASM_STRUCT_NEW_DEFAULT(
               subtype_index,
               WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index))),
           WASM_RTT_CANON(type_index)),
       kExprEnd});
  const byte kRefTestUpcastNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(WASM_REF_NULL(subtype_index), WASM_RTT_CANON(type_index)),
       kExprEnd});
  const byte kRefTestUnrelated = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(
           WASM_STRUCT_NEW_DEFAULT(
               subtype_index,
               WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index))),
           WASM_RTT_CANON(sig_index)),
       kExprEnd});
  const byte kRefTestUnrelatedNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(WASM_REF_NULL(subtype_index), WASM_RTT_CANON(sig_index)),
       kExprEnd});
  const byte kRefTestUnrelatedNonNullable = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_TEST(
           WASM_STRUCT_NEW_DEFAULT(type_index, WASM_RTT_CANON(type_index)),
           WASM_RTT_CANON(sig_index)),
       kExprEnd});

  const byte kRefCastNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_NULL(WASM_REF_CAST(WASM_REF_NULL(type_index),
                                      WASM_RTT_CANON(subtype_index))),
       kExprEnd});
  const byte kRefCastUpcast = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_NULL(WASM_REF_CAST(
           WASM_STRUCT_NEW_DEFAULT(
               subtype_index,
               WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index))),
           WASM_RTT_CANON(type_index))),
       kExprEnd});
  const byte kRefCastUpcastNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_NULL(WASM_REF_CAST(WASM_REF_NULL(subtype_index),
                                      WASM_RTT_CANON(type_index))),
       kExprEnd});
  const byte kRefCastUnrelated = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_NULL(WASM_REF_CAST(
           WASM_STRUCT_NEW_DEFAULT(
               subtype_index,
               WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index))),
           WASM_RTT_CANON(sig_index))),
       kExprEnd});
  const byte kRefCastUnrelatedNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_NULL(WASM_REF_CAST(WASM_REF_NULL(subtype_index),
                                      WASM_RTT_CANON(sig_index))),
       kExprEnd});
  const byte kRefCastUnrelatedNonNullable = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_NULL(WASM_REF_CAST(
           WASM_STRUCT_NEW_DEFAULT(type_index, WASM_RTT_CANON(type_index)),
           WASM_RTT_CANON(sig_index))),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(kRefTestNull, 0);
  tester.CheckResult(kRefTestUpcast, 1);
  tester.CheckResult(kRefTestUpcastNull, 0);
  tester.CheckResult(kRefTestUnrelated, 0);
  tester.CheckResult(kRefTestUnrelatedNull, 0);
  tester.CheckResult(kRefTestUnrelatedNonNullable, 0);

  tester.CheckResult(kRefCastNull, 1);
  tester.CheckResult(kRefCastUpcast, 0);
  tester.CheckResult(kRefCastUpcastNull, 1);
  tester.CheckHasThrown(kRefCastUnrelated);
  tester.CheckResult(kRefCastUnrelatedNull, 1);
  tester.CheckHasThrown(kRefCastUnrelatedNonNullable);
}

WASM_EXEC_TEST(NoDepthRtt) {
  WasmGCTester tester(execution_tier);

  const byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  const byte subtype_index =
      tester.DefineStruct({F(wasm::kWasmI32, true), F(wasm::kWasmI32, true)});
  const byte empty_struct_index = tester.DefineStruct({});

  ValueType kRttTypeNoDepth = ValueType::Rtt(type_index);
  FunctionSig sig_t1_v_nd(1, 0, &kRttTypeNoDepth);
  ValueType kRttSubtypeNoDepth = ValueType::Rtt(subtype_index);
  FunctionSig sig_t2_v_nd(1, 0, &kRttSubtypeNoDepth);

  const byte kRttTypeCanon = tester.DefineFunction(
      &sig_t1_v_nd, {}, {WASM_RTT_CANON(type_index), kExprEnd});
  const byte kRttSubtypeCanon = tester.DefineFunction(
      &sig_t2_v_nd, {}, {WASM_RTT_CANON(subtype_index), kExprEnd});
  const byte kRttSubtypeSub = tester.DefineFunction(
      &sig_t2_v_nd, {},
      {WASM_RTT_SUB(subtype_index, WASM_CALL_FUNCTION0(kRttTypeCanon)),
       kExprEnd});

  const byte kTestCanon = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(0, WASM_STRUCT_NEW_WITH_RTT(
                             subtype_index, WASM_I32V(11), WASM_I32V(42),
                             WASM_RTT_CANON(subtype_index))),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_CALL_FUNCTION0(kRttSubtypeCanon)),
       kExprEnd});

  const byte kTestSub = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(
           0, WASM_STRUCT_NEW_WITH_RTT(
                  subtype_index, WASM_I32V(11), WASM_I32V(42),
                  WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index)))),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_CALL_FUNCTION0(kRttSubtypeSub)),
       kExprEnd});

  const byte kTestSubVsEmpty = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(0, WASM_STRUCT_NEW_WITH_RTT(
                             subtype_index, WASM_I32V(11), WASM_I32V(42),
                             WASM_RTT_SUB(subtype_index,
                                          WASM_RTT_CANON(empty_struct_index)))),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_CALL_FUNCTION0(kRttSubtypeSub)),
       kExprEnd});

  const byte kTestSubVsCanon = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(0, WASM_STRUCT_NEW_WITH_RTT(
                             subtype_index, WASM_I32V(11), WASM_I32V(42),
                             WASM_RTT_CANON(subtype_index))),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_CALL_FUNCTION0(kRttSubtypeSub)),
       kExprEnd});

  const byte kTestCanonVsSub = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(
           0, WASM_STRUCT_NEW_WITH_RTT(
                  subtype_index, WASM_I32V(11), WASM_I32V(42),
                  WASM_RTT_SUB(subtype_index, WASM_RTT_CANON(type_index)))),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_CALL_FUNCTION0(kRttSubtypeCanon)),
       kExprEnd});

  const byte kTestSuperVsSub = tester.DefineFunction(
      tester.sigs.i_v(), {optref(type_index)},
      {WASM_LOCAL_SET(0, WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                                  WASM_RTT_CANON(type_index))),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_CALL_FUNCTION0(kRttSubtypeCanon)),
       kExprEnd});

  tester.CompileModule();

  tester.CheckResult(kTestCanon, 1);
  tester.CheckResult(kTestSub, 1);
  tester.CheckResult(kTestSubVsEmpty, 0);
  tester.CheckResult(kTestSubVsCanon, 0);
  tester.CheckResult(kTestCanonVsSub, 0);
  tester.CheckResult(kTestSuperVsSub, 0);
}

WASM_COMPILED_EXEC_TEST(ArrayNewMap) {
  WasmGCTester tester(execution_tier);

  const byte type_index = tester.DefineArray(kWasmI32, true);

  ValueType array_type = ValueType::Ref(type_index, kNonNullable);
  FunctionSig sig(1, 0, &array_type);
  const byte array_new_with_rtt = tester.DefineFunction(
      &sig, {},
      {WASM_ARRAY_NEW_WITH_RTT(type_index, WASM_I32V(10), WASM_I32V(42),
                               WASM_RTT_CANON(type_index)),
       kExprEnd});

  ValueType rtt_type = ValueType::Rtt(type_index, 0);
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

WASM_COMPILED_EXEC_TEST(FunctionRefs) {
  WasmGCTester tester(execution_tier);
  const byte func_index =
      tester.DefineFunction(tester.sigs.i_v(), {}, {WASM_I32V(42), kExprEnd});
  const byte sig_index = 0;

  const byte other_sig_index = tester.DefineSignature(tester.sigs.d_d());

  // This is just so func_index counts as "declared".
  tester.AddGlobal(ValueType::Ref(sig_index, kNullable), false,
                   WasmInitExpr::RefFuncConst(func_index));

  ValueType func_type = ValueType::Ref(sig_index, kNullable);
  FunctionSig sig_func(1, 0, &func_type);

  ValueType rtt0 = ValueType::Rtt(sig_index, 0);
  FunctionSig sig_rtt0(1, 0, &rtt0);
  const byte rtt_canon = tester.DefineFunction(
      &sig_rtt0, {}, {WASM_RTT_CANON(sig_index), kExprEnd});

  const byte cast = tester.DefineFunction(
      &sig_func, {kWasmFuncRef},
      {WASM_LOCAL_SET(0, WASM_REF_FUNC(func_index)),
       WASM_REF_CAST(WASM_LOCAL_GET(0), WASM_RTT_CANON(sig_index)), kExprEnd});

  const byte cast_reference = tester.DefineFunction(
      &sig_func, {}, {WASM_REF_FUNC(sig_index), kExprEnd});

  const byte test = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmFuncRef},
      {WASM_LOCAL_SET(0, WASM_REF_FUNC(func_index)),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_RTT_CANON(sig_index)), kExprEnd});

  const byte test_fail_1 = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmFuncRef},
      {WASM_LOCAL_SET(0, WASM_REF_FUNC(func_index)),
       WASM_REF_TEST(WASM_LOCAL_GET(0), WASM_RTT_CANON(other_sig_index)),
       kExprEnd});

  const byte test_fail_2 = tester.DefineFunction(
      tester.sigs.i_v(), {kWasmFuncRef},
      {WASM_LOCAL_SET(0, WASM_REF_FUNC(func_index)),
       WASM_REF_TEST(WASM_LOCAL_GET(0),
                     WASM_RTT_SUB(sig_index, WASM_RTT_CANON(sig_index))),
       kExprEnd});

  tester.CompileModule();

  Handle<Object> result_canon =
      tester.GetResultObject(rtt_canon).ToHandleChecked();
  CHECK(result_canon->IsMap());
  Handle<Map> map_canon = Handle<Map>::cast(result_canon);
  CHECK(map_canon->IsJSFunctionMap());

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

  tester.CheckResult(test, 1);
  tester.CheckResult(test_fail_1, 0);
  tester.CheckResult(test_fail_2, 0);
}

WASM_COMPILED_EXEC_TEST(CallRef) {
  WasmGCTester tester(execution_tier);
  byte callee = tester.DefineFunction(
      tester.sigs.i_ii(), {},
      {WASM_I32_ADD(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), kExprEnd});
  byte caller = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_CALL_REF(WASM_REF_FUNC(callee), WASM_I32V(42), WASM_LOCAL_GET(0)),
       kExprEnd});

  // This is just so func_index counts as "declared".
  tester.AddGlobal(ValueType::Ref(0, kNullable), false,
                   WasmInitExpr::RefFuncConst(callee));

  tester.CompileModule();

  tester.CheckResult(caller, 47, 5);
}

WASM_COMPILED_EXEC_TEST(CallReftypeParameters) {
  WasmGCTester tester(execution_tier);
  byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  ValueType kRefType{optref(type_index)};
  ValueType sig_types[] = {kWasmI32, kRefType, kRefType, kRefType, kRefType,
                           kWasmI32, kWasmI32, kWasmI32, kWasmI32};
  FunctionSig sig(1, 8, sig_types);
  byte adder = tester.DefineFunction(
      &sig, {},
      {WASM_I32_ADD(
           WASM_STRUCT_GET(type_index, 0, WASM_LOCAL_GET(0)),
           WASM_I32_ADD(
               WASM_STRUCT_GET(type_index, 0, WASM_LOCAL_GET(1)),
               WASM_I32_ADD(
                   WASM_STRUCT_GET(type_index, 0, WASM_LOCAL_GET(2)),
                   WASM_I32_ADD(
                       WASM_STRUCT_GET(type_index, 0, WASM_LOCAL_GET(3)),
                       WASM_I32_ADD(
                           WASM_LOCAL_GET(4),
                           WASM_I32_ADD(WASM_LOCAL_GET(5),
                                        WASM_I32_ADD(WASM_LOCAL_GET(6),
                                                     WASM_LOCAL_GET(7)))))))),
       kExprEnd});
  byte caller = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_CALL_FUNCTION(adder,
                          WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(2),
                                                   WASM_RTT_CANON(type_index)),
                          WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(4),
                                                   WASM_RTT_CANON(type_index)),
                          WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(8),
                                                   WASM_RTT_CANON(type_index)),
                          WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(16),
                                                   WASM_RTT_CANON(type_index)),
                          WASM_I32V(32), WASM_I32V(64), WASM_I32V(128),
                          WASM_I32V(256)),
       kExprEnd});

  tester.CompileModule();
  tester.CheckResult(caller, 510);
}

WASM_COMPILED_EXEC_TEST(AbstractTypeChecks) {
  WasmGCTester tester(execution_tier);

  byte array_index = tester.DefineArray(kWasmI32, true);
  byte function_index =
      tester.DefineFunction(tester.sigs.v_v(), {}, {kExprEnd});
  byte sig_index = 1;

  // This is just so func_index counts as "declared".
  tester.AddGlobal(ValueType::Ref(sig_index, kNullable), false,
                   WasmInitExpr::RefFuncConst(function_index));

  byte kDataCheckNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_DATA(WASM_REF_NULL(kAnyRefCode)), kExprEnd});
  byte kFuncCheckNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_FUNC(WASM_REF_NULL(kAnyRefCode)), kExprEnd});
  byte kI31CheckNull = tester.DefineFunction(
      tester.sigs.i_v(), {},
      {WASM_REF_IS_I31(WASM_REF_NULL(kAnyRefCode)), kExprEnd});

  byte kDataCastNull =
      tester.DefineFunction(tester.sigs.i_v(), {},
                            {WASM_REF_AS_DATA(WASM_REF_NULL(kAnyRefCode)),
                             WASM_DROP, WASM_I32V(1), kExprEnd});
  byte kFuncCastNull =
      tester.DefineFunction(tester.sigs.i_v(), {},
                            {WASM_REF_AS_FUNC(WASM_REF_NULL(kAnyRefCode)),
                             WASM_DROP, WASM_I32V(1), kExprEnd});
  byte kI31CastNull =
      tester.DefineFunction(tester.sigs.i_v(), {},
                            {WASM_REF_AS_I31(WASM_REF_NULL(kAnyRefCode)),
                             WASM_DROP, WASM_I32V(1), kExprEnd});

#define TYPE_CHECK(type, value)                              \
  tester.DefineFunction(tester.sigs.i_v(), {kWasmAnyRef},    \
                        {WASM_LOCAL_SET(0, WASM_SEQ(value)), \
                         WASM_REF_IS_##type(WASM_LOCAL_GET(0)), kExprEnd})

  byte kDataCheckSuccess =
      TYPE_CHECK(DATA, WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                              WASM_RTT_CANON(array_index)));
  byte kDataCheckFailure = TYPE_CHECK(DATA, WASM_I31_NEW(WASM_I32V(42)));
  byte kFuncCheckSuccess = TYPE_CHECK(FUNC, WASM_REF_FUNC(function_index));
  byte kFuncCheckFailure =
      TYPE_CHECK(FUNC, WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                              WASM_RTT_CANON(array_index)));
  byte kI31CheckSuccess = TYPE_CHECK(I31, WASM_I31_NEW(WASM_I32V(42)));
  byte kI31CheckFailure =
      TYPE_CHECK(I31, WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                             WASM_RTT_CANON(array_index)));
#undef TYPE_CHECK

#define TYPE_CAST(type, value)                                             \
  tester.DefineFunction(tester.sigs.i_v(), {kWasmAnyRef},                  \
                        {WASM_LOCAL_SET(0, WASM_SEQ(value)),               \
                         WASM_REF_AS_##type(WASM_LOCAL_GET(0)), WASM_DROP, \
                         WASM_I32V(1), kExprEnd})

  byte kDataCastSuccess =
      TYPE_CAST(DATA, WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                             WASM_RTT_CANON(array_index)));
  byte kDataCastFailure = TYPE_CAST(DATA, WASM_I31_NEW(WASM_I32V(42)));
  byte kFuncCastSuccess = TYPE_CAST(FUNC, WASM_REF_FUNC(function_index));
  byte kFuncCastFailure =
      TYPE_CAST(FUNC, WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                             WASM_RTT_CANON(array_index)));
  byte kI31CastSuccess = TYPE_CAST(I31, WASM_I31_NEW(WASM_I32V(42)));
  byte kI31CastFailure =
      TYPE_CAST(I31, WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                            WASM_RTT_CANON(array_index)));
#undef TYPE_CAST

// If the branch is not taken, we return 0. If it is taken, then the respective
// type check should succeed, and we return 1.
#define BR_ON(TYPE, type, value)                                      \
  tester.DefineFunction(                                              \
      tester.sigs.i_v(), {kWasmAnyRef},                               \
      {WASM_LOCAL_SET(0, WASM_SEQ(value)),                            \
       WASM_REF_IS_##TYPE(WASM_BLOCK_R(                               \
           kWasm##type##Ref, WASM_BR_ON_##TYPE(0, WASM_LOCAL_GET(0)), \
           WASM_RETURN(WASM_I32V(0)))),                               \
       kExprEnd})

  byte kBrOnDataTaken =
      BR_ON(DATA, Data,
            WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                   WASM_RTT_CANON(array_index)));
  byte kBrOnDataNotTaken = BR_ON(DATA, Data, WASM_REF_FUNC(function_index));
  byte kBrOnFuncTaken = BR_ON(FUNC, Func, WASM_REF_FUNC(function_index));
  byte kBrOnFuncNotTaken = BR_ON(FUNC, Func, WASM_I31_NEW(WASM_I32V(42)));
  byte kBrOnI31Taken = BR_ON(I31, I31, WASM_I31_NEW(WASM_I32V(42)));
  byte kBrOnI31NotTaken =
      BR_ON(I31, I31,
            WASM_ARRAY_NEW_DEFAULT(array_index, WASM_I32V(10),
                                   WASM_RTT_CANON(array_index)));
#undef BR_ON

  tester.CompileModule();

  tester.CheckResult(kDataCheckNull, 0);
  tester.CheckHasThrown(kDataCastNull);
  tester.CheckResult(kDataCheckSuccess, 1);
  tester.CheckResult(kDataCheckFailure, 0);
  tester.CheckResult(kDataCastSuccess, 1);
  tester.CheckHasThrown(kDataCastFailure);
  tester.CheckResult(kBrOnDataTaken, 1);
  tester.CheckResult(kBrOnDataNotTaken, 0);

  tester.CheckResult(kFuncCheckNull, 0);
  tester.CheckHasThrown(kFuncCastNull);
  tester.CheckResult(kFuncCheckSuccess, 1);
  tester.CheckResult(kFuncCheckFailure, 0);
  tester.CheckResult(kFuncCastSuccess, 1);
  tester.CheckHasThrown(kFuncCastFailure);
  tester.CheckResult(kBrOnFuncTaken, 1);
  tester.CheckResult(kBrOnFuncNotTaken, 0);

  tester.CheckResult(kI31CheckNull, 0);
  tester.CheckHasThrown(kI31CastNull);
  tester.CheckResult(kI31CheckSuccess, 1);
  tester.CheckResult(kI31CheckFailure, 0);
  tester.CheckResult(kI31CastSuccess, 1);
  tester.CheckHasThrown(kI31CastFailure);
  tester.CheckResult(kBrOnI31Taken, 1);
  tester.CheckResult(kBrOnI31NotTaken, 0);
}

WASM_COMPILED_EXEC_TEST(BasicI31) {
  WasmGCTester tester(execution_tier);
  const byte kSigned = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_I31_GET_S(WASM_I31_NEW(WASM_LOCAL_GET(0))), kExprEnd});
  const byte kUnsigned = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_I31_GET_U(WASM_I31_NEW(WASM_LOCAL_GET(0))), kExprEnd});
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

// This flushed out a few bugs, so it serves as a regression test. It can also
// be modified (made to run longer) to measure performance of casts.
WASM_COMPILED_EXEC_TEST(CastsBenchmark) {
  WasmGCTester tester(execution_tier);
  const byte SuperType = tester.DefineStruct({F(wasm::kWasmI32, true)});
  const byte SubType =
      tester.DefineStruct({F(wasm::kWasmI32, true), F(wasm::kWasmI32, true)});

  ValueType kDataRefNull = ValueType::Ref(HeapType::kData, kNullable);
  const byte ListType = tester.DefineArray(kDataRefNull, true);

  const byte List =
      tester.AddGlobal(ValueType::Ref(ListType, kNullable), true,
                       WasmInitExpr::RefNullConst(
                           static_cast<HeapType::Representation>(ListType)));
  const byte RttSuper = tester.AddGlobal(
      ValueType::Rtt(SuperType, 0), false,
      WasmInitExpr::RttCanon(static_cast<HeapType::Representation>(SuperType)));
  const byte RttSub = tester.AddGlobal(
      ValueType::Rtt(SubType, 1), false,
      WasmInitExpr::RttSub(static_cast<HeapType::Representation>(SubType),
                           WasmInitExpr::GlobalGet(RttSuper)));
  const byte RttList = tester.AddGlobal(
      ValueType::Rtt(ListType, 0), false,
      WasmInitExpr::RttCanon(static_cast<HeapType::Representation>(ListType)));

  const uint32_t kListLength = 1024;
  const uint32_t i = 0;
  const byte Prepare = tester.DefineFunction(
      tester.sigs.i_v(), {wasm::kWasmI32},
      {// List = new eqref[kListLength];
       WASM_GLOBAL_SET(List,
                       WASM_ARRAY_NEW_DEFAULT(ListType, WASM_I32V(kListLength),
                                              WASM_GLOBAL_GET(RttList))),
       // for (int i = 0; i < kListLength; ) {
       //   List[i] = new Super(i);
       //   i++;
       //   List[i] = new Sub(i, 0);
       //   i++;
       // }
       WASM_LOCAL_SET(i, WASM_I32V_1(0)),
       WASM_LOOP(
           WASM_ARRAY_SET(ListType, WASM_GLOBAL_GET(List), WASM_LOCAL_GET(i),
                          WASM_STRUCT_NEW_WITH_RTT(SuperType, WASM_LOCAL_GET(i),
                                                   WASM_GLOBAL_GET(RttSuper))),
           WASM_LOCAL_SET(i, WASM_I32_ADD(WASM_LOCAL_GET(i), WASM_I32V_1(1))),
           WASM_ARRAY_SET(ListType, WASM_GLOBAL_GET(List), WASM_LOCAL_GET(i),
                          WASM_STRUCT_NEW_WITH_RTT(SubType, WASM_LOCAL_GET(i),
                                                   WASM_I32V_1(0),
                                                   WASM_GLOBAL_GET(RttSub))),
           WASM_LOCAL_SET(i, WASM_I32_ADD(WASM_LOCAL_GET(i), WASM_I32V_1(1))),
           WASM_BR_IF(0,
                      WASM_I32_NE(WASM_LOCAL_GET(i), WASM_I32V(kListLength)))),
       // return 42;  // Dummy value, due to test framework.
       WASM_I32V_1(42), kExprEnd});

  const uint32_t sum = 1;  // Index of the local.
  const uint32_t list = 2;
  const uint32_t kLoops = 2;
  const uint32_t kIterations = kLoops * kListLength;
  const byte Main = tester.DefineFunction(
      tester.sigs.i_v(),
      {
          wasm::kWasmI32,
          wasm::kWasmI32,
          ValueType::Ref(ListType, kNullable),
      },
      {WASM_LOCAL_SET(list, WASM_GLOBAL_GET(List)),
       // sum = 0;
       WASM_LOCAL_SET(sum, WASM_I32V_1(0)),
       // for (int i = 0; i < kIterations; i++) {
       //   sum += ref.cast<super>(List[i & kListLength]).x
       // }
       WASM_LOCAL_SET(i, WASM_I32V_1(0)),
       WASM_LOOP(
           WASM_LOCAL_SET(
               sum, WASM_I32_ADD(
                        WASM_LOCAL_GET(sum),
                        WASM_STRUCT_GET(
                            SuperType, 0,
                            WASM_REF_CAST(
                                WASM_ARRAY_GET(
                                    ListType, WASM_LOCAL_GET(list),
                                    WASM_I32_AND(WASM_LOCAL_GET(i),
                                                 WASM_I32V(kListLength - 1))),
                                WASM_GLOBAL_GET(RttSuper))))),
           WASM_LOCAL_SET(i, WASM_I32_ADD(WASM_LOCAL_GET(i), WASM_I32V_1(1))),
           WASM_BR_IF(0,
                      WASM_I32_LTS(WASM_LOCAL_GET(i), WASM_I32V(kIterations)))),
       // return sum;
       WASM_LOCAL_GET(sum), kExprEnd});

  tester.CompileModule();
  tester.CheckResult(Prepare, 42);

  // Time this section to get a benchmark for subtyping checks.
  // Note: if you bump kIterations or kListLength, you may have to take i32
  // overflow into account.
  tester.CheckResult(Main, (kListLength * (kListLength - 1) / 2) * kLoops);
}

WASM_COMPILED_EXEC_TEST(GlobalInitReferencingGlobal) {
  WasmGCTester tester(execution_tier);
  const byte from = tester.AddGlobal(kWasmI32, false, WasmInitExpr(42));
  const byte to =
      tester.AddGlobal(kWasmI32, false, WasmInitExpr::GlobalGet(from));

  const byte func = tester.DefineFunction(tester.sigs.i_v(), {},
                                          {WASM_GLOBAL_GET(to), kExprEnd});

  tester.CompileModule();

  tester.CheckResult(func, 42);
}

WASM_COMPILED_EXEC_TEST(IndirectNullSetManually) {
  WasmGCTester tester(execution_tier);
  byte sig_index = tester.DefineSignature(tester.sigs.i_i());
  tester.DefineTable(ValueType::Ref(sig_index, kNullable), 1, 1);
  byte func_index = tester.DefineFunction(
      tester.sigs.i_i(), {},
      {WASM_TABLE_SET(0, WASM_I32V(0), WASM_REF_NULL(sig_index)),
       WASM_CALL_INDIRECT(sig_index, WASM_I32V(0), WASM_LOCAL_GET(0)),
       kExprEnd});

  tester.CompileModule();

  tester.CheckHasThrown(func_index, 42);
}

WASM_COMPILED_EXEC_TEST(JsAccess) {
  WasmGCTester tester(execution_tier);
  const byte type_index = tester.DefineStruct({F(wasm::kWasmI32, true)});
  ValueType kRefType = ref(type_index);
  ValueType kSupertypeToI[] = {kWasmI32, kWasmDataRef};
  FunctionSig sig_t_v(1, 0, &kRefType);
  FunctionSig sig_super_v(1, 0, &kWasmDataRef);
  FunctionSig sig_i_super(1, 1, kSupertypeToI);

  tester.DefineExportedFunction(
      "disallowed", &sig_t_v,
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});
  // Same code, different signature.
  tester.DefineExportedFunction(
      "producer", &sig_super_v,
      {WASM_STRUCT_NEW_WITH_RTT(type_index, WASM_I32V(42),
                                WASM_RTT_CANON(type_index)),
       kExprEnd});
  tester.DefineExportedFunction(
      "consumer", &sig_i_super,
      {WASM_STRUCT_GET(
           type_index, 0,
           WASM_REF_CAST(WASM_LOCAL_GET(0), WASM_RTT_CANON(type_index))),
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

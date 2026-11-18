// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/heap/heap-allocator.h"
#include "src/heap/heap.h"
#include "src/wasm/c-api.h"
#include "test/wasm-api-tests/wasm-api-test.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

own<Trap> Stage2(void* env, const vec<Val>& args, vec<Val>& results) {
  printf("Stage2...\n");
  WasmCapiTest* self = reinterpret_cast<WasmCapiTest*>(env);
  Func* stage3 = self->GetExportedFunction(1);
  own<Trap> trap = stage3->call(args, results);
  if (trap) {
    printf("Stage2: got exception: %s\n", trap->message().get());
  } else {
    printf("Stage2: call successful\n");
  }
  return trap;
}

own<Trap> Stage4_GC(void* env, const vec<Val>& args, vec<Val>& results) {
  printf("Stage4...\n");
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env);
  isolate->heap()->PreciseCollectAllGarbage(GCFlag::kForced,
                                            GarbageCollectionReason::kTesting);
  results[0] = Val::i32(args[0].i32() + 1);
  return nullptr;
}

class WasmCapiCallbacksTest : public WasmCapiTest {
 public:
  WasmCapiCallbacksTest() : WasmCapiTest() {
    // Build the following function:
    // int32 stage1(int32 arg0) { return stage2(arg0); }
    uint32_t stage2_index = builder()->AddImport("stage2", wasm_i_i_sig());
    uint8_t code[] = {WASM_CALL_FUNCTION(stage2_index, WASM_LOCAL_GET(0))};
    AddExportedFunction("stage1", code, sizeof(code));

    stage2_ = Func::make(store(), cpp_i_i_sig(), Stage2, this);
  }

  Func* stage2() { return stage2_.get(); }
  void AddExportedFunction(std::string_view name, uint8_t code[],
                           size_t code_size) {
    WasmCapiTest::AddExportedFunction(name, code, code_size, wasm_i_i_sig());
  }

 private:
  own<Func> stage2_;
};

}  // namespace

TEST_F(WasmCapiCallbacksTest, Trap) {
  // Build the following function:
  // int32 stage3_trap(int32 arg0) { unreachable(); }
  uint8_t code[] = {WASM_UNREACHABLE};
  AddExportedFunction("stage3_trap", code, sizeof(code));

  vec<Extern*> imports = vec<Extern*>::make(stage2());
  Instantiate(imports);
  vec<Val> args = vec<Val>::make(Val::i32(42));
  vec<Val> results = vec<Val>::make_uninitialized(1);
  own<Trap> trap = GetExportedFunction(0)->call(args, results);
  EXPECT_NE(trap, nullptr);
  printf("Stage0: Got trap as expected: %s\n", trap->message().get());
}

TEST_F(WasmCapiCallbacksTest, GC) {
  // Build the following function:
  // int32 stage3_to4(int32 arg0) { return stage4(arg0); }
  uint32_t stage4_index = builder()->AddImport("stage4", wasm_i_i_sig());
  uint8_t code[] = {WASM_CALL_FUNCTION(stage4_index, WASM_LOCAL_GET(0))};
  AddExportedFunction("stage3_to4", code, sizeof(code));

  i::Isolate* isolate =
      reinterpret_cast<::wasm::StoreImpl*>(store())->i_isolate();
  own<Func> stage4 = Func::make(store(), cpp_i_i_sig(), Stage4_GC, isolate);
  EXPECT_EQ(cpp_i_i_sig()->params().size(), stage4->type()->params().size());
  EXPECT_EQ(cpp_i_i_sig()->results().size(), stage4->type()->results().size());
  vec<Extern*> imports = vec<Extern*>::make(stage2(), stage4.get());
  Instantiate(imports);
  vec<Val> args = vec<Val>::make(Val::i32(42));
  vec<Val> results = vec<Val>::make_uninitialized(1);
  own<Trap> trap = GetExportedFunction(0)->call(args, results);
  EXPECT_EQ(trap, nullptr);
  EXPECT_EQ(43, results[0].i32());
}

namespace {

own<Trap> FibonacciC(void* env, const vec<Val>& args, vec<Val>& results) {
  int32_t x = args[0].i32();
  if (x == 0 || x == 1) {
    results[0] = Val::i32(x);
    return nullptr;
  }
  WasmCapiTest* self = reinterpret_cast<WasmCapiTest*>(env);
  Func* fibo_wasm = self->GetExportedFunction(0);
  // Aggressively reuse existing arrays. That's maybe not great coding
  // style, but this test intentionally ensures that it works if someone
  // insists on doing it.
  vec<Val> recursive_args = vec<Val>::make(Val::i32(x - 1));
  own<Trap> trap = fibo_wasm->call(recursive_args, results);
  DCHECK_NULL(trap);
  int32_t x1 = results[0].i32();
  recursive_args[0] = Val::i32(x - 2);
  trap = fibo_wasm->call(recursive_args, results);
  DCHECK_NULL(trap);
  int32_t x2 = results[0].i32();
  results[0] = Val::i32(x1 + x2);
  return nullptr;
}

}  // namespace

TEST_F(WasmCapiTest, Recursion) {
  // Build the following function:
  // int32 fibonacci_wasm(int32 arg0) {
  //   if (arg0 == 0) return 0;
  //   if (arg0 == 1) return 1;
  //   return fibonacci_c(arg0 - 1) + fibonacci_c(arg0 - 2);
  // }
  uint32_t fibo_c_index = builder()->AddImport("fibonacci_c", wasm_i_i_sig());
  uint8_t code_fibo[] = {
      WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_ZERO),
              WASM_RETURN(WASM_ZERO)),
      WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_ONE), WASM_RETURN(WASM_ONE)),
      // Muck with the parameter to ensure callers don't depend on its value.
      WASM_LOCAL_SET(0, WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_ONE)),
      WASM_RETURN(WASM_I32_ADD(
          WASM_CALL_FUNCTION(fibo_c_index, WASM_LOCAL_GET(0)),
          WASM_CALL_FUNCTION(fibo_c_index,
                             WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_ONE))))};
  AddExportedFunction("fibonacci_wasm", code_fibo, sizeof(code_fibo),
                      wasm_i_i_sig());

  own<Func> fibonacci = Func::make(store(), cpp_i_i_sig(), FibonacciC, this);
  vec<Extern*> imports = vec<Extern*>::make(fibonacci.get());
  Instantiate(imports);
  // Enough iterations to make it interesting, few enough to keep it fast.
  vec<Val> args = vec<Val>::make(Val::i32(15));
  vec<Val> results = vec<Val>::make_uninitialized(1);
  own<Trap> result = GetExportedFunction(0)->call(args, results);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(610, results[0].i32());
}

namespace {

own<Trap> PlusOne(const vec<Val>& args, vec<Val>& results) {
  int32_t a0 = args[0].i32();
  results[0] = Val::i32(a0 + 1);
  int64_t a1 = args[1].i64();
  results[1] = Val::i64(a1 + 1);
  float a2 = args[2].f32();
  results[2] = Val::f32(a2 + 1);
  double a3 = args[3].f64();
  results[3] = Val::f64(a3 + 1);
  results[4] = Val::ref(args[4].ref()->copy());  // No +1 for Refs.
  return nullptr;
}

own<Trap> PlusOneWithManyArgs(const vec<Val>& args, vec<Val>& results) {
  int32_t a0 = args[0].i32();
  results[0] = Val::i32(a0 + 1);
  int64_t a1 = args[1].i64();
  results[1] = Val::i64(a1 + 1);
  float a2 = args[2].f32();
  results[2] = Val::f32(a2 + 1);
  double a3 = args[3].f64();
  results[3] = Val::f64(a3 + 1);
  results[4] = Val::ref(args[4].ref()->copy());  // No +1 for Refs.
  int32_t a5 = args[5].i32();
  results[5] = Val::i32(a5 + 1);
  int64_t a6 = args[6].i64();
  results[6] = Val::i64(a6 + 1);
  float a7 = args[7].f32();
  results[7] = Val::f32(a7 + 1);
  double a8 = args[8].f64();
  results[8] = Val::f64(a8 + 1);
  int32_t a9 = args[9].i32();
  results[9] = Val::i32(a9 + 1);
  int64_t a10 = args[10].i64();
  results[10] = Val::i64(a10 + 1);
  float a11 = args[11].f32();
  results[11] = Val::f32(a11 + 1);
  double a12 = args[12].f64();
  results[12] = Val::f64(a12 + 1);
  int32_t a13 = args[13].i32();
  results[13] = Val::i32(a13 + 1);
  return nullptr;
}
}  // namespace

TEST_F(WasmCapiTest, DirectCallCapiFunction) {
  own<FuncType> cpp_sig = FuncType::make(
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::EXTERNREF)),
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::EXTERNREF)));
  own<Func> func = Func::make(store(), cpp_sig.get(), PlusOne);
  vec<Extern*> imports = vec<Extern*>::make(func.get());
  ValueType wasm_types[] = {kWasmI32,       kWasmI64,      kWasmF32, kWasmF64,
                            kWasmExternRef, kWasmI32,      kWasmI64, kWasmF32,
                            kWasmF64,       kWasmExternRef};
  FunctionSig wasm_sig(5, 5, wasm_types);
  int func_index = builder()->AddImport("func", &wasm_sig);
  builder()->ExportImportedFunction("func", func_index);
  Instantiate(imports);
  int32_t a0 = 42;
  int64_t a1 = 0x1234c0ffee;
  float a2 = 1234.5;
  double a3 = 123.45;
  vec<Val> args = vec<Val>::make(Val::i32(a0), Val::i64(a1), Val::f32(a2),
                                 Val::f64(a3), Val::ref(func->copy()));
  vec<Val> results = vec<Val>::make_uninitialized(5);
  // Test that {func} can be called directly.
  own<Trap> trap = func->call(args, results);
  EXPECT_EQ(nullptr, trap);
  EXPECT_EQ(a0 + 1, results[0].i32());
  EXPECT_EQ(a1 + 1, results[1].i64());
  EXPECT_EQ(a2 + 1, results[2].f32());
  EXPECT_EQ(a3 + 1, results[3].f64());
  EXPECT_TRUE(func->same(results[4].ref()));

  // Test that {func} can be called after import/export round-tripping.
  trap = GetExportedFunction(0)->call(args, results);
  EXPECT_EQ(nullptr, trap);
  EXPECT_EQ(a0 + 1, results[0].i32());
  EXPECT_EQ(a1 + 1, results[1].i64());
  EXPECT_EQ(a2 + 1, results[2].f32());
  EXPECT_EQ(a3 + 1, results[3].f64());
  EXPECT_TRUE(func->same(results[4].ref()));
}

TEST_F(WasmCapiTest, DirectCallCapiFunctionWithManyArgs) {
  // Test with many arguments to make sure that CWasmArgumentsPacker won't use
  // its buffer-on-stack optimization.
  own<FuncType> cpp_sig = FuncType::make(
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::EXTERNREF),
                            ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::I32)),
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::EXTERNREF),
                            ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::I64),
                            ValType::make(::wasm::ValKind::F32),
                            ValType::make(::wasm::ValKind::F64),
                            ValType::make(::wasm::ValKind::I32)));
  own<Func> func = Func::make(store(), cpp_sig.get(), PlusOneWithManyArgs);
  vec<Extern*> imports = vec<Extern*>::make(func.get());
  ValueType wasm_types[] = {
      kWasmI32,       kWasmI64, kWasmF32, kWasmF64, kWasmExternRef, kWasmI32,
      kWasmI64,       kWasmF32, kWasmF64, kWasmI32, kWasmI64,       kWasmF32,
      kWasmF64,       kWasmI32, kWasmI32, kWasmI64, kWasmF32,       kWasmF64,
      kWasmExternRef, kWasmI32, kWasmI64, kWasmF32, kWasmF64,       kWasmI32,
      kWasmI64,       kWasmF32, kWasmF64, kWasmI32};
  FunctionSig wasm_sig(14, 14, wasm_types);
  int func_index = builder()->AddImport("func", &wasm_sig);
  builder()->ExportImportedFunction("func", func_index);
  Instantiate(imports);
  int32_t a0 = 42;
  int64_t a1 = 0x1234c0ffee;
  float a2 = 1234.5;
  double a3 = 123.45;
  vec<Val> args =
      vec<Val>::make(Val::i32(a0), Val::i64(a1), Val::f32(a2), Val::f64(a3),
                     Val::ref(func->copy()), Val::i32(a0), Val::i64(a1),
                     Val::f32(a2), Val::f64(a3), Val::i32(a0), Val::i64(a1),
                     Val::f32(a2), Val::f64(a3), Val::i32(a0));
  vec<Val> results = vec<Val>::make_uninitialized(14);
  // Test that {func} can be called directly.
  own<Trap> trap = func->call(args, results);
  EXPECT_EQ(nullptr, trap);
  EXPECT_EQ(a0 + 1, results[0].i32());
  EXPECT_EQ(a1 + 1, results[1].i64());
  EXPECT_EQ(a2 + 1, results[2].f32());
  EXPECT_EQ(a3 + 1, results[3].f64());
  EXPECT_TRUE(func->same(results[4].ref()));
  EXPECT_EQ(a0 + 1, results[5].i32());
  EXPECT_EQ(a1 + 1, results[6].i64());
  EXPECT_EQ(a2 + 1, results[7].f32());
  EXPECT_EQ(a3 + 1, results[8].f64());
  EXPECT_EQ(a0 + 1, results[9].i32());
  EXPECT_EQ(a1 + 1, results[10].i64());
  EXPECT_EQ(a2 + 1, results[11].f32());
  EXPECT_EQ(a3 + 1, results[12].f64());
  EXPECT_EQ(a0 + 1, results[13].i32());

  // Test that {func} can be called after import/export round-tripping.
  trap = GetExportedFunction(0)->call(args, results);
  EXPECT_EQ(nullptr, trap);
  EXPECT_EQ(a0 + 1, results[0].i32());
  EXPECT_EQ(a1 + 1, results[1].i64());
  EXPECT_EQ(a2 + 1, results[2].f32());
  EXPECT_EQ(a3 + 1, results[3].f64());
  EXPECT_TRUE(func->same(results[4].ref()));
  EXPECT_EQ(a0 + 1, results[5].i32());
  EXPECT_EQ(a1 + 1, results[6].i64());
  EXPECT_EQ(a2 + 1, results[7].f32());
  EXPECT_EQ(a3 + 1, results[8].f64());
  EXPECT_EQ(a0 + 1, results[9].i32());
  EXPECT_EQ(a1 + 1, results[10].i64());
  EXPECT_EQ(a2 + 1, results[11].f32());
  EXPECT_EQ(a3 + 1, results[12].f64());
  EXPECT_EQ(a0 + 1, results[13].i32());
}

namespace {

own<Trap> EchoAndCheckFuncRef(const vec<Val>& args, vec<Val>& results) {
  results[0] = Val::i32(args[0].ref() == nullptr ? 1 : 0);
  results[1] = args[0].copy();
  return nullptr;
}

own<Trap> EchoTwoRefs(const vec<Val>& args, vec<Val>& results) {
  EXPECT_NE(nullptr, args[0].ref());
  results[0] = args[0].copy();
  results[1] = args[1].copy();
  return nullptr;
}

}  // namespace

TEST_F(WasmCapiTest, FuncRefCallbackAndReturn) {
  own<FuncType> cb_cpp_sig = FuncType::make(
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::FUNCREF)),
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::I32),
                            ValType::make(::wasm::ValKind::FUNCREF)));
  own<Func> cb = Func::make(store(), cb_cpp_sig.get(), EchoAndCheckFuncRef);

  own<FuncType> cb_two_refs_cpp_sig = FuncType::make(
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::FUNCREF),
                            ValType::make(::wasm::ValKind::EXTERNREF)),
      ownvec<ValType>::make(ValType::make(::wasm::ValKind::FUNCREF),
                            ValType::make(::wasm::ValKind::EXTERNREF)));
  own<Func> cb_two_refs =
      Func::make(store(), cb_two_refs_cpp_sig.get(), EchoTwoRefs);

  ValueType cb_wasm_types[] = {kWasmI32, kWasmFuncRef, kWasmFuncRef};
  FunctionSig cb_wasm_sig(2, 1, cb_wasm_types);
  uint32_t cb_index = builder()->AddImport("cb", &cb_wasm_sig);

  ValueType cb_two_refs_wasm_types[] = {kWasmFuncRef, kWasmExternRef,
                                        kWasmFuncRef, kWasmExternRef};
  FunctionSig cb_two_refs_wasm_sig(2, 2, cb_two_refs_wasm_types);
  uint32_t cb_two_refs_index =
      builder()->AddImport("cb_two_refs", &cb_two_refs_wasm_sig);

  uint8_t roundtrip_code[] = {WASM_CALL_FUNCTION(cb_index, WASM_LOCAL_GET(0))};
  AddExportedFunction("roundtrip", roundtrip_code, sizeof(roundtrip_code),
                      &cb_wasm_sig);

  uint8_t call_cb_null_code[] = {
      WASM_CALL_FUNCTION(cb_index, WASM_REF_NULL(kFuncRefCode)), WASM_DROP};
  ValueType i32_ret[] = {kWasmI32};
  FunctionSig sig_i_v(1, 0, i32_ret);
  AddExportedFunction("call_cb_null", call_cb_null_code,
                      sizeof(call_cb_null_code), &sig_i_v);

  // Declare function 3 (call_cb_null) in a global so WASM_REF_FUNC(3) is valid.
  builder()->AddGlobal(kWasmFuncRef, false, WasmInitExpr::RefFuncConst(3));

  uint8_t call_cb_nonnull_code[] = {
      WASM_CALL_FUNCTION(cb_index, WASM_REF_FUNC(3)), WASM_DROP};
  AddExportedFunction("call_cb_nonnull", call_cb_nonnull_code,
                      sizeof(call_cb_nonnull_code), &sig_i_v);

  ValueType funcref_ret[] = {kWasmFuncRef};
  FunctionSig sig_r_v(1, 0, funcref_ret);
  uint8_t ret_null_code[] = {WASM_REF_NULL(kFuncRefCode)};
  AddExportedFunction("ret_null", ret_null_code, sizeof(ret_null_code),
                      &sig_r_v);

  uint8_t ret_nonnull_code[] = {WASM_REF_FUNC(3)};
  AddExportedFunction("ret_nonnull", ret_nonnull_code, sizeof(ret_nonnull_code),
                      &sig_r_v);

  // Unexported functions (indices 7 and 8) have no cached external JSFunction
  // until V8RefValueToWasm calls WasmInternalFunction::GetOrCreateExternal.
  uint8_t unexported_1_code[] = {WASM_I32V_1(11)};
  AddFunction(unexported_1_code, sizeof(unexported_1_code), &sig_i_v);
  builder()->AddGlobal(kWasmFuncRef, false, WasmInitExpr::RefFuncConst(7));

  uint8_t unexported_2_code[] = {WASM_I32V_1(22)};
  AddFunction(unexported_2_code, sizeof(unexported_2_code), &sig_i_v);
  builder()->AddGlobal(kWasmFuncRef, false, WasmInitExpr::RefFuncConst(8));

  uint8_t ret_two_refs_code[] = {WASM_REF_FUNC(7), WASM_LOCAL_GET(1)};
  AddExportedFunction("ret_two_refs", ret_two_refs_code,
                      sizeof(ret_two_refs_code), &cb_two_refs_wasm_sig);

  ValueType two_refs_ret_one_param_types[] = {kWasmFuncRef, kWasmExternRef,
                                              kWasmExternRef};
  FunctionSig sig_rr_r(2, 1, two_refs_ret_one_param_types);
  uint8_t call_cb_two_refs_code[] = {WASM_CALL_FUNCTION(
      cb_two_refs_index, WASM_REF_FUNC(8), WASM_LOCAL_GET(0))};
  AddExportedFunction("call_cb_two_refs", call_cb_two_refs_code,
                      sizeof(call_cb_two_refs_code), &sig_rr_r);

  vec<Extern*> imports = vec<Extern*>::make(cb.get(), cb_two_refs.get());
  Instantiate(imports);

  Func* roundtrip = GetExportedFunction(0);
  Func* call_cb_null = GetExportedFunction(1);
  Func* call_cb_nonnull = GetExportedFunction(2);
  Func* ret_null = GetExportedFunction(3);
  Func* ret_nonnull = GetExportedFunction(4);
  Func* ret_two_refs = GetExportedFunction(5);
  Func* call_cb_two_refs = GetExportedFunction(6);

  // Test PushArgs + v8_callback params/results + PopArgs with null and non-null
  // funcref.
  vec<Val> rt_null_args = vec<Val>::make(Val::ref(own<Ref>()));
  vec<Val> rt_results = vec<Val>::make_uninitialized(2);
  EXPECT_EQ(nullptr, roundtrip->call(rt_null_args, rt_results));
  EXPECT_EQ(1, rt_results[0].i32());
  EXPECT_EQ(nullptr, rt_results[1].ref());

  vec<Val> rt_func_args = vec<Val>::make(Val::ref(call_cb_null->copy()));
  EXPECT_EQ(nullptr, roundtrip->call(rt_func_args, rt_results));
  EXPECT_EQ(0, rt_results[0].i32());
  ASSERT_NE(nullptr, rt_results[1].ref());
  EXPECT_TRUE(call_cb_null->same(rt_results[1].ref()));

  vec<Val> empty_args = vec<Val>::make();
  vec<Val> results = vec<Val>::make_uninitialized(1);

  EXPECT_EQ(nullptr, call_cb_null->call(empty_args, results));
  EXPECT_EQ(1, results[0].i32());

  EXPECT_EQ(nullptr, call_cb_nonnull->call(empty_args, results));
  EXPECT_EQ(0, results[0].i32());

  EXPECT_EQ(nullptr, ret_null->call(empty_args, results));
  EXPECT_EQ(nullptr, results[0].ref());

  EXPECT_EQ(nullptr, ret_nonnull->call(empty_args, results));
  ASSERT_NE(nullptr, results[0].ref());
  EXPECT_TRUE(call_cb_null->same(results[0].ref()));

  // Verify GC safety when GetOrCreateExternal allocates for an unexported
  // funcref while a second reference slot is also being unpacked in PopArgs /
  // FuncData::v8_callback.
  {
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
    i::Isolate* isolate =
        reinterpret_cast<::wasm::StoreImpl*>(store())->i_isolate();
#endif
    own<Foreign> foreign1 = Foreign::make(store());
    vec<Val> args1 = vec<Val>::make(Val::ref(call_cb_null->copy()),
                                    Val::ref(foreign1->copy()));
    vec<Val> two_results = vec<Val>::make_uninitialized(2);
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
    i::HeapAllocator::SetAllocationGcInterval(1);
    isolate->heap()->set_allocation_timeout(1);
#endif
    EXPECT_EQ(nullptr, ret_two_refs->call(args1, two_results));
    EXPECT_NE(nullptr, two_results[0].ref());
    ASSERT_NE(nullptr, two_results[1].ref());
    EXPECT_TRUE(foreign1->same(two_results[1].ref()));

    own<Foreign> foreign2 = Foreign::make(store());
    vec<Val> args2 = vec<Val>::make(Val::ref(foreign2->copy()));
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
    isolate->heap()->set_allocation_timeout(1);
#endif
    EXPECT_EQ(nullptr, call_cb_two_refs->call(args2, two_results));
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
    i::HeapAllocator::SetAllocationGcInterval(-1);
    isolate->heap()->set_allocation_timeout(0);
#endif
    EXPECT_NE(nullptr, two_results[0].ref());
    ASSERT_NE(nullptr, two_results[1].ref());
    EXPECT_TRUE(foreign2->same(two_results[1].ref()));
  }

  // Also test standalone mutable FUNCREF Global::make / get / set.
  own<::wasm::GlobalType> global_type = ::wasm::GlobalType::make(
      ValType::make(::wasm::ValKind::FUNCREF), ::wasm::Mutability::VAR);
  own<::wasm::Global> global =
      ::wasm::Global::make(store(), global_type.get(), Val::ref(own<Ref>()));
  EXPECT_EQ(nullptr, global->get().ref());
  global->set(Val::ref(call_cb_null->copy()));
  ASSERT_NE(nullptr, global->get().ref());
  EXPECT_TRUE(call_cb_null->same(global->get().ref()));
  global->set(Val::ref(own<Ref>()));
  EXPECT_EQ(nullptr, global->get().ref());
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8

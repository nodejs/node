// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

#include "src/execution/isolate.h"
#include "src/heap/heap.h"
#include "src/wasm/c-api.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

own<Trap> Stage2(void* env, const Val args[], Val results[]) {
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

own<Trap> Stage4_GC(void* env, const Val args[], Val results[]) {
  printf("Stage4...\n");
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env);
  isolate->heap()->PreciseCollectAllGarbage(
      i::Heap::kForcedGC, i::GarbageCollectionReason::kTesting,
      v8::kNoGCCallbackFlags);
  results[0] = Val::i32(args[0].i32() + 1);
  return nullptr;
}

class WasmCapiCallbacksTest : public WasmCapiTest {
 public:
  WasmCapiCallbacksTest() : WasmCapiTest() {
    // Build the following function:
    // int32 stage1(int32 arg0) { return stage2(arg0); }
    uint32_t stage2_index =
        builder()->AddImport(CStrVector("stage2"), wasm_i_i_sig());
    byte code[] = {WASM_CALL_FUNCTION(stage2_index, WASM_GET_LOCAL(0))};
    AddExportedFunction(CStrVector("stage1"), code, sizeof(code));

    stage2_ = Func::make(store(), cpp_i_i_sig(), Stage2, this);
  }

  Func* stage2() { return stage2_.get(); }
  void AddExportedFunction(Vector<const char> name, byte code[],
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
  byte code[] = {WASM_UNREACHABLE};
  AddExportedFunction(CStrVector("stage3_trap"), code, sizeof(code));

  Extern* imports[] = {stage2()};
  Instantiate(imports);
  Val args[] = {Val::i32(42)};
  Val results[1];
  own<Trap> trap = GetExportedFunction(0)->call(args, results);
  EXPECT_NE(trap, nullptr);
  printf("Stage0: Got trap as expected: %s\n", trap->message().get());
}

TEST_F(WasmCapiCallbacksTest, GC) {
  // Build the following function:
  // int32 stage3_to4(int32 arg0) { return stage4(arg0); }
  uint32_t stage4_index =
      builder()->AddImport(CStrVector("stage4"), wasm_i_i_sig());
  byte code[] = {WASM_CALL_FUNCTION(stage4_index, WASM_GET_LOCAL(0))};
  AddExportedFunction(CStrVector("stage3_to4"), code, sizeof(code));

  i::Isolate* isolate =
      reinterpret_cast<::wasm::StoreImpl*>(store())->i_isolate();
  own<Func> stage4 = Func::make(store(), cpp_i_i_sig(), Stage4_GC, isolate);
  EXPECT_EQ(cpp_i_i_sig()->params().size(), stage4->type()->params().size());
  EXPECT_EQ(cpp_i_i_sig()->results().size(), stage4->type()->results().size());
  Extern* imports[] = {stage2(), stage4.get()};
  Instantiate(imports);
  Val args[] = {Val::i32(42)};
  Val results[1];
  own<Trap> trap = GetExportedFunction(0)->call(args, results);
  EXPECT_EQ(trap, nullptr);
  EXPECT_EQ(43, results[0].i32());
}

namespace {

own<Trap> FibonacciC(void* env, const Val args[], Val results[]) {
  int32_t x = args[0].i32();
  if (x == 0 || x == 1) {
    results[0] = Val::i32(x);
    return nullptr;
  }
  WasmCapiTest* self = reinterpret_cast<WasmCapiTest*>(env);
  Func* fibo_wasm = self->GetExportedFunction(0);
  // Aggressively re-use existing arrays. That's maybe not great coding
  // style, but this test intentionally ensures that it works if someone
  // insists on doing it.
  Val recursive_args[] = {Val::i32(x - 1)};
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
  uint32_t fibo_c_index =
      builder()->AddImport(CStrVector("fibonacci_c"), wasm_i_i_sig());
  byte code_fibo[] = {
      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_ZERO),
              WASM_RETURN1(WASM_ZERO)),
      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_ONE), WASM_RETURN1(WASM_ONE)),
      // Muck with the parameter to ensure callers don't depend on its value.
      WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_ONE)),
      WASM_RETURN1(WASM_I32_ADD(
          WASM_CALL_FUNCTION(fibo_c_index, WASM_GET_LOCAL(0)),
          WASM_CALL_FUNCTION(fibo_c_index,
                             WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_ONE))))};
  AddExportedFunction(CStrVector("fibonacci_wasm"), code_fibo,
                      sizeof(code_fibo), wasm_i_i_sig());

  own<Func> fibonacci = Func::make(store(), cpp_i_i_sig(), FibonacciC, this);
  Extern* imports[] = {fibonacci.get()};
  Instantiate(imports);
  // Enough iterations to make it interesting, few enough to keep it fast.
  Val args[] = {Val::i32(15)};
  Val results[1];
  own<Trap> result = GetExportedFunction(0)->call(args, results);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(610, results[0].i32());
}

namespace {

own<Trap> PlusOne(const Val args[], Val results[]) {
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

}  // namespace

TEST_F(WasmCapiTest, DirectCallCapiFunction) {
  own<FuncType> cpp_sig =
      FuncType::make(ownvec<ValType>::make(
                         ValType::make(::wasm::I32), ValType::make(::wasm::I64),
                         ValType::make(::wasm::F32), ValType::make(::wasm::F64),
                         ValType::make(::wasm::ANYREF)),
                     ownvec<ValType>::make(
                         ValType::make(::wasm::I32), ValType::make(::wasm::I64),
                         ValType::make(::wasm::F32), ValType::make(::wasm::F64),
                         ValType::make(::wasm::ANYREF)));
  own<Func> func = Func::make(store(), cpp_sig.get(), PlusOne);
  Extern* imports[] = {func.get()};
  ValueType wasm_types[] = {kWasmI32,    kWasmI64,   kWasmF32, kWasmF64,
                            kWasmAnyRef, kWasmI32,   kWasmI64, kWasmF32,
                            kWasmF64,    kWasmAnyRef};
  FunctionSig wasm_sig(5, 5, wasm_types);
  int func_index = builder()->AddImport(CStrVector("func"), &wasm_sig);
  builder()->ExportImportedFunction(CStrVector("func"), func_index);
  Instantiate(imports);
  int32_t a0 = 42;
  int64_t a1 = 0x1234c0ffee;
  float a2 = 1234.5;
  double a3 = 123.45;
  Val args[] = {Val::i32(a0), Val::i64(a1), Val::f32(a2), Val::f64(a3),
                Val::ref(func->copy())};
  Val results[5];
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

}  // namespace wasm
}  // namespace internal
}  // namespace v8

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/heap/heap.h"
#include "src/wasm/c-api.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/wasm-api/wasm.hh"

namespace wasm {

// TODO(jkummerow): Drop these from the API.
#ifdef DEBUG
template <class T>
void vec<T>::make_data() {}

template <class T>
void vec<T>::free_data() {}
#endif

}  // namespace wasm

namespace v8 {
namespace internal {
namespace wasm {

namespace {

using ::wasm::Engine;
using ::wasm::Extern;
using ::wasm::Func;
using ::wasm::FuncType;
using ::wasm::Instance;
using ::wasm::Module;
using ::wasm::own;
using ::wasm::Store;
using ::wasm::Trap;
using ::wasm::Val;
using ::wasm::ValType;
using ::wasm::vec;

own<Trap*> Stage2(void* env, const Val args[], Val results[]);

class WasmCapiTest : public ::testing::Test {
 public:
  WasmCapiTest()
      : Test(),
        zone_(&allocator_, ZONE_NAME),
        builder_(&zone_),
        exports_(vec<Extern*>::make()),
        wasm_sig_(1, 1, wasm_sig_types_) {
    engine_ = Engine::make();
    store_ = Store::make(engine_.get());

    // Build the following function:
    // int32 stage1(int32 arg0) { return stage2(arg0); }
    uint32_t stage2_index =
        builder_.AddImport(ArrayVector("stage2"), wasm_sig());
    byte code[] = {WASM_CALL_FUNCTION(stage2_index, WASM_GET_LOCAL(0))};
    AddExportedFunction(CStrVector("stage1"), code, sizeof(code));

    cpp_sig_ = FuncType::make(vec<ValType*>::make(ValType::make(::wasm::I32)),
                              vec<ValType*>::make(ValType::make(::wasm::I32)));
    stage2_ = Func::make(store(), cpp_sig_.get(), Stage2, this);
  }

  void Compile() {
    ZoneBuffer buffer(&zone_);
    builder_.WriteTo(buffer);
    size_t size = buffer.end() - buffer.begin();
    vec<byte_t> binary = vec<byte_t>::make(
        size, reinterpret_cast<byte_t*>(const_cast<byte*>(buffer.begin())));

    module_ = Module::make(store_.get(), binary);
    DCHECK_NE(module_.get(), nullptr);
  }

  own<Trap*> Run(Extern* imports[], Val args[], Val results[]) {
    instance_ = Instance::make(store_.get(), module_.get(), imports);
    DCHECK_NE(instance_.get(), nullptr);
    exports_ = instance_->exports();
    Func* entry = GetExportedFunction(0);
    return entry->call(args, results);
  }

  void AddExportedFunction(Vector<const char> name, byte code[],
                           size_t code_size) {
    WasmFunctionBuilder* fun = builder()->AddFunction(wasm_sig());
    fun->EmitCode(code, static_cast<uint32_t>(code_size));
    fun->Emit(kExprEnd);
    builder()->AddExport(name, fun);
  }

  Func* GetExportedFunction(size_t index) {
    DCHECK_GT(exports_.size(), index);
    Extern* exported = exports_[index];
    DCHECK_EQ(exported->kind(), ::wasm::EXTERN_FUNC);
    Func* func = exported->func();
    DCHECK_NE(func, nullptr);
    return func;
  }

  WasmModuleBuilder* builder() { return &builder_; }
  Store* store() { return store_.get(); }
  Func* stage2() { return stage2_.get(); }

  FunctionSig* wasm_sig() { return &wasm_sig_; }
  FuncType* cpp_sig() { return cpp_sig_.get(); }

 private:
  AccountingAllocator allocator_;
  Zone zone_;
  WasmModuleBuilder builder_;
  own<Engine*> engine_;
  own<Store*> store_;
  own<Module*> module_;
  own<Instance*> instance_;
  vec<Extern*> exports_;
  own<Func*> stage2_;
  own<FuncType*> cpp_sig_;
  ValueType wasm_sig_types_[2] = {kWasmI32, kWasmI32};
  FunctionSig wasm_sig_;
};

own<Trap*> Stage2(void* env, const Val args[], Val results[]) {
  printf("Stage2...\n");
  WasmCapiTest* self = reinterpret_cast<WasmCapiTest*>(env);
  Func* stage3 = self->GetExportedFunction(1);
  own<Trap*> result = stage3->call(args, results);
  if (result) {
    printf("Stage2: got exception: %s\n", result->message().get());
  } else {
    printf("Stage2: call successful\n");
  }
  return result;
}

own<Trap*> Stage4_GC(void* env, const Val args[], Val results[]) {
  printf("Stage4...\n");
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env);
  isolate->heap()->PreciseCollectAllGarbage(
      i::Heap::kNoGCFlags, i::GarbageCollectionReason::kTesting,
      v8::kGCCallbackFlagForced);
  results[0] = Val::i32(args[0].i32() + 1);
  return nullptr;
}

}  // namespace

TEST_F(WasmCapiTest, Trap) {
  // Build the following function:
  // int32 stage3_trap(int32 arg0) { unreachable(); }
  byte code[] = {WASM_UNREACHABLE};
  AddExportedFunction(CStrVector("stage3_trap"), code, sizeof(code));
  Compile();

  Extern* imports[] = {stage2()};
  Val args[] = {Val::i32(42)};
  Val results[1];
  own<Trap*> result = Run(imports, args, results);
  EXPECT_NE(result, nullptr);
  printf("Stage0: Got trap as expected: %s\n", result->message().get());
}

TEST_F(WasmCapiTest, GC) {
  // Build the following function:
  // int32 stage3_to4(int32 arg0) { return stage4(arg0); }
  uint32_t stage4_index =
      builder()->AddImport(ArrayVector("stage4"), wasm_sig());
  byte code[] = {WASM_CALL_FUNCTION(stage4_index, WASM_GET_LOCAL(0))};
  AddExportedFunction(CStrVector("stage3_to4"), code, sizeof(code));
  Compile();

  i::Isolate* isolate =
      reinterpret_cast<::wasm::StoreImpl*>(store())->i_isolate();
  own<Func*> stage4 = Func::make(store(), cpp_sig(), Stage4_GC, isolate);
  EXPECT_EQ(cpp_sig()->params().size(), stage4->type()->params().size());
  EXPECT_EQ(cpp_sig()->results().size(), stage4->type()->results().size());
  Extern* imports[] = {stage2(), stage4.get()};
  Val args[] = {Val::i32(42)};
  Val results[1];
  own<Trap*> result = Run(imports, args, results);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(43, results[0].i32());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

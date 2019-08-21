// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_WASM_API_TESTS_WASM_API_TEST_H_
#define TEST_WASM_API_TESTS_WASM_API_TEST_H_

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

using ::wasm::Engine;
using ::wasm::Extern;
using ::wasm::Foreign;
using ::wasm::Func;
using ::wasm::FuncType;
using ::wasm::Global;
using ::wasm::Instance;
using ::wasm::Memory;
using ::wasm::Module;
using ::wasm::own;
using ::wasm::Ref;
using ::wasm::Store;
using ::wasm::Table;
using ::wasm::Trap;
using ::wasm::Val;
using ::wasm::ValType;
using ::wasm::vec;

class WasmCapiTest : public ::testing::Test {
 public:
  WasmCapiTest()
      : Test(),
        zone_(&allocator_, ZONE_NAME),
        builder_(&zone_),
        exports_(vec<Extern*>::make()),
        wasm_i_i_sig_(1, 1, wasm_i_i_sig_types_) {
    engine_ = Engine::make();
    store_ = Store::make(engine_.get());
    cpp_i_i_sig_ =
        FuncType::make(vec<ValType*>::make(ValType::make(::wasm::I32)),
                       vec<ValType*>::make(ValType::make(::wasm::I32)));
  }

  void Compile() {
    ZoneBuffer buffer(&zone_);
    builder_.WriteTo(&buffer);
    size_t size = buffer.end() - buffer.begin();
    vec<byte_t> binary = vec<byte_t>::make(
        size, reinterpret_cast<byte_t*>(const_cast<byte*>(buffer.begin())));

    module_ = Module::make(store_.get(), binary);
    DCHECK_NE(module_.get(), nullptr);
  }

  void Instantiate(Extern* imports[]) {
    Compile();
    instance_ = Instance::make(store_.get(), module_.get(), imports);
    DCHECK_NE(instance_.get(), nullptr);
    exports_ = instance_->exports();
  }

  void AddExportedFunction(Vector<const char> name, byte code[],
                           size_t code_size, FunctionSig* sig) {
    WasmFunctionBuilder* fun = builder()->AddFunction(sig);
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

  Global* GetExportedGlobal(size_t index) {
    DCHECK_GT(exports_.size(), index);
    Extern* exported = exports_[index];
    DCHECK_EQ(exported->kind(), ::wasm::EXTERN_GLOBAL);
    Global* global = exported->global();
    DCHECK_NE(global, nullptr);
    return global;
  }

  Memory* GetExportedMemory(size_t index) {
    DCHECK_GT(exports_.size(), index);
    Extern* exported = exports_[index];
    DCHECK_EQ(exported->kind(), ::wasm::EXTERN_MEMORY);
    Memory* memory = exported->memory();
    DCHECK_NE(memory, nullptr);
    return memory;
  }

  Table* GetExportedTable(size_t index) {
    DCHECK_GT(exports_.size(), index);
    Extern* exported = exports_[index];
    DCHECK_EQ(exported->kind(), ::wasm::EXTERN_TABLE);
    Table* table = exported->table();
    DCHECK_NE(table, nullptr);
    return table;
  }

  void Shutdown() {
    instance_.reset();
    module_.reset();
    store_.reset();
    engine_.reset();
  }

  WasmModuleBuilder* builder() { return &builder_; }
  Engine* engine() { return engine_.get(); }
  Store* store() { return store_.get(); }
  Module* module() { return module_.get(); }
  const vec<Extern*>& exports() { return exports_; }

  FunctionSig* wasm_i_i_sig() { return &wasm_i_i_sig_; }
  FuncType* cpp_i_i_sig() { return cpp_i_i_sig_.get(); }

 private:
  AccountingAllocator allocator_;
  Zone zone_;
  WasmModuleBuilder builder_;
  own<Engine*> engine_;
  own<Store*> store_;
  own<Module*> module_;
  own<Instance*> instance_;
  vec<Extern*> exports_;
  own<FuncType*> cpp_i_i_sig_;
  ValueType wasm_i_i_sig_types_[2] = {kWasmI32, kWasmI32};
  FunctionSig wasm_i_i_sig_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // TEST_WASM_API_TESTS_WASM_API_TEST_H_

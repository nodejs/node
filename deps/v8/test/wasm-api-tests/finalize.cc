// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

int g_instances_finalized = 0;
int g_functions_finalized = 0;
int g_foreigns_finalized = 0;
int g_modules_finalized = 0;

void FinalizeInstance(void* data) {
  int iteration = static_cast<int>(reinterpret_cast<intptr_t>(data));
  g_instances_finalized += iteration;
}

void FinalizeFunction(void* data) {
  int iteration = static_cast<int>(reinterpret_cast<intptr_t>(data));
  g_functions_finalized += iteration;
}

void FinalizeForeign(void* data) {
  int iteration = static_cast<int>(reinterpret_cast<intptr_t>(data));
  g_foreigns_finalized += iteration;
}

void FinalizeModule(void* data) {
  g_modules_finalized += static_cast<int>(reinterpret_cast<intptr_t>(data));
}

}  // namespace

TEST_F(WasmCapiTest, InstanceFinalization) {
  // Add a dummy function: f(x) { return x; }
  byte code[] = {WASM_RETURN1(WASM_GET_LOCAL(0))};
  AddExportedFunction(CStrVector("f"), code, sizeof(code), wasm_i_i_sig());
  Compile();
  g_instances_finalized = 0;
  g_functions_finalized = 0;
  g_foreigns_finalized = 0;
  g_modules_finalized = 0;
  module()->set_host_info(reinterpret_cast<void*>(42), &FinalizeModule);
  static const int kIterations = 10;
  for (int iteration = 0; iteration < kIterations; iteration++) {
    void* finalizer_data = reinterpret_cast<void*>(iteration);
    own<Instance*> instance = Instance::make(store(), module(), nullptr);
    EXPECT_NE(nullptr, instance.get());
    instance->set_host_info(finalizer_data, &FinalizeInstance);

    own<Func*> func = instance->exports()[0]->func()->copy();
    ASSERT_NE(func, nullptr);
    func->set_host_info(finalizer_data, &FinalizeFunction);

    own<Foreign*> foreign = Foreign::make(store());
    foreign->set_host_info(finalizer_data, &FinalizeForeign);
  }
  Shutdown();
  // Verify that (1) all finalizers were called, and (2) they passed the
  // correct host data: the loop above sets {i} as data, and the finalizer
  // callbacks add them all up, so the expected value is
  // sum([0, 1, ..., kIterations - 1]), which per Gauss's formula is:
  static const int kExpected = (kIterations * (kIterations - 1)) / 2;
  EXPECT_EQ(g_instances_finalized, kExpected);
  EXPECT_EQ(g_functions_finalized, kExpected);
  EXPECT_EQ(g_foreigns_finalized, kExpected);
  EXPECT_EQ(g_modules_finalized, 42);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

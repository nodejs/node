// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/wasm/wasm-run-utils.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

#define FOREACH_TYPE(TEST_BODY)         \
  TEST_BODY(int32_t, I32, WASM_I32_ADD) \
  TEST_BODY(int64_t, I64, WASM_I64_ADD) \
  TEST_BODY(float, F32, WASM_F32_ADD)   \
  TEST_BODY(double, F64, WASM_F64_ADD)

#define LOAD_SET_GLOBAL_TEST_BODY(C_TYPE, MACHINE_TYPE, ADD)                 \
  TEST(WasmRelocateGlobal##MACHINE_TYPE) {                                   \
    TestingModule module(kExecuteCompiled);                                  \
    module.AddGlobal<C_TYPE>(kAst##MACHINE_TYPE);                            \
    module.AddGlobal<C_TYPE>(kAst##MACHINE_TYPE);                            \
                                                                             \
    WasmRunner<C_TYPE> r(&module,                                            \
                         WasmOpcodes::MachineTypeFor(kAst##MACHINE_TYPE));   \
                                                                             \
    /* global = global + p0 */                                               \
    BUILD(r, WASM_SET_GLOBAL(1, ADD(WASM_GET_GLOBAL(0), WASM_GET_LOCAL(0))), \
          WASM_GET_GLOBAL(0));                                               \
    CHECK_EQ(1, module.instance->function_code.size());                      \
                                                                             \
    int filter = 1 << RelocInfo::WASM_GLOBAL_REFERENCE;                      \
                                                                             \
    Handle<Code> code = module.instance->function_code[0];                   \
                                                                             \
    Address old_start = module.instance->globals_start;                      \
    Address new_start = old_start + 1;                                       \
                                                                             \
    Address old_addresses[4];                                                \
    uint32_t address_index = 0U;                                             \
    for (RelocIterator it(*code, filter); !it.done(); it.next()) {           \
      old_addresses[address_index] = it.rinfo()->wasm_global_reference();    \
      it.rinfo()->update_wasm_global_reference(old_start, new_start);        \
      ++address_index;                                                       \
    }                                                                        \
    CHECK_LE(address_index, 4U);                                             \
                                                                             \
    address_index = 0U;                                                      \
    for (RelocIterator it(*code, filter); !it.done(); it.next()) {           \
      CHECK_EQ(old_addresses[address_index] + 1,                             \
               it.rinfo()->wasm_global_reference());                         \
      ++address_index;                                                       \
    }                                                                        \
    CHECK_LE(address_index, 4U);                                             \
  }

FOREACH_TYPE(LOAD_SET_GLOBAL_TEST_BODY)

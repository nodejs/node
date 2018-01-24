// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/assembler-inl.h"
#include "src/objects-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_relocation {

WASM_COMPILED_EXEC_TEST(RunPatchWasmContext) {
  WasmRunner<uint32_t, uint32_t> r(execution_mode);
  Isolate* isolate = CcTest::i_isolate();

  r.builder().AddGlobal<uint32_t>();
  r.builder().AddGlobal<uint32_t>();

  BUILD(r, WASM_SET_GLOBAL(0, WASM_GET_LOCAL(0)), WASM_GET_GLOBAL(0));
  CHECK_EQ(1, r.builder().CodeTableLength());

  // Run with the old global data.
  CHECK_EQ(113, r.Call(113));

  WasmContext* old_wasm_context =
      r.builder().instance_object()->wasm_context()->get();
  Address old_wasm_context_address =
      reinterpret_cast<Address>(old_wasm_context);

  uint32_t new_global_data[3] = {0, 0, 0};
  WasmContext new_wasm_context;
  new_wasm_context.globals_start = reinterpret_cast<byte*>(new_global_data);

  {
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());

    // Patch in a new WasmContext that points to the new global data.
    int filter = 1 << RelocInfo::WASM_CONTEXT_REFERENCE;
    bool patched = false;
    Handle<Code> code = r.GetWrapperCode();
    for (RelocIterator it(*code, filter); !it.done(); it.next()) {
      CHECK_EQ(old_wasm_context_address, it.rinfo()->wasm_context_reference());
      it.rinfo()->set_wasm_context_reference(
          isolate, reinterpret_cast<Address>(&new_wasm_context));
      patched = true;
    }
    CHECK(patched);
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
  }

  // Run with the new global data.
  CHECK_EQ(115, r.Call(115));
  CHECK_EQ(115, new_global_data[0]);
}

}  // namespace test_run_wasm_relocation
}  // namespace wasm
}  // namespace internal
}  // namespace v8

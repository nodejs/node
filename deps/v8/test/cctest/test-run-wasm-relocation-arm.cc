// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>  // NOLINT(readability/streams)

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/arm/assembler-arm-inl.h"
#include "src/arm/simulator-arm.h"
#include "src/disassembler.h"
#include "src/factory.h"
#include "src/ostreams.h"
#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace wasm {

#define __ assm.

static int32_t DummyStaticFunction(Object* result) { return 1; }

TEST(WasmRelocationArmContextReference) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  DummyStaticFunction(nullptr);
  int32_t imm = 1234567;

  Assembler assm(isolate, buffer, sizeof buffer);

  __ mov(r0, Operand(imm, RelocInfo::WASM_CONTEXT_REFERENCE));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  compiler::CSignature0<int32_t> csig;
  compiler::CodeRunner<int32_t> runnable(isolate, code, &csig);
  int32_t ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm);

#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
  ::printf("f() = %d\n\n", ret_value);
#endif
  int offset = 1234;

  // Relocating references by offset
  int mode_mask = (1 << RelocInfo::WASM_CONTEXT_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    DCHECK(RelocInfo::IsWasmContextReference(it.rinfo()->rmode()));
    it.rinfo()->set_wasm_context_reference(
        isolate, it.rinfo()->wasm_context_reference() + offset,
        SKIP_ICACHE_FLUSH);
  }

  // Call into relocated code object
  ret_value = runnable.Call();
  CHECK_EQ((imm + offset), ret_value);

#ifdef DEBUG
  code->Print(os);
  ::printf("f() = %d\n\n", ret_value);
#endif
}

#undef __

}  // namespace wasm
}  // namespace internal
}  // namespace v8

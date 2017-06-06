// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>  // NOLINT(readability/streams)

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/arm64/simulator-arm64.h"
#include "src/arm64/utils-arm64.h"
#include "src/disassembler.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/compiler/call-tester.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;

#define __ masm.

static int64_t DummyStaticFunction(Object* result) { return 1; }

TEST(WasmRelocationArm64MemoryReference) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  DummyStaticFunction(NULL);
  int64_t imm = 1234567;

  MacroAssembler masm(isolate, buffer, sizeof buffer,
                      v8::internal::CodeObjectRequired::kYes);

  __ Mov(x0, Immediate(imm, RelocInfo::WASM_MEMORY_REFERENCE));
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  CSignature0<int64_t> csig;
  CodeRunner<int64_t> runnable(isolate, code, &csig);
  int64_t ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm);

#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
  ::printf("f() = %" PRIx64 "\n\n", ret_value);
#endif
  int offset = 1234;

  // Relocating reference by offset
  int mode_mask = (1 << RelocInfo::WASM_MEMORY_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    DCHECK(RelocInfo::IsWasmMemoryReference(it.rinfo()->rmode()));
    it.rinfo()->update_wasm_memory_reference(
        isolate, it.rinfo()->wasm_memory_reference(),
        it.rinfo()->wasm_memory_reference() + offset, SKIP_ICACHE_FLUSH);
  }

  // Call into relocated code object
  ret_value = runnable.Call();
  CHECK_EQ((imm + offset), ret_value);

#ifdef DEBUG
  code->Print(os);
  ::printf("f() = %" PRIx64 "\n\n", ret_value);
#endif
}

TEST(WasmRelocationArm64MemorySizeReference) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  DummyStaticFunction(NULL);
  Immediate size = Immediate(512, RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
  Label fail;

  MacroAssembler masm(isolate, buffer, sizeof buffer,
                      v8::internal::CodeObjectRequired::kYes);

  __ Mov(x0, size);
  __ Cmp(x0, size);
  __ B(ne, &fail);
  __ Ret();
  __ Bind(&fail);
  __ Mov(x0, Immediate(0xdeadbeef));
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  CSignature0<int64_t> csig;
  CodeRunner<int64_t> runnable(isolate, code, &csig);
  int64_t ret_value = runnable.Call();
  CHECK_NE(ret_value, 0xdeadbeef);

#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
  ::printf("f() = %" PRIx64 "\n\n", ret_value);
#endif
  int32_t diff = 512;

  int mode_mask = (1 << RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    DCHECK(RelocInfo::IsWasmMemorySizeReference(it.rinfo()->rmode()));
    it.rinfo()->update_wasm_memory_size(
        isolate, it.rinfo()->wasm_memory_size_reference(),
        it.rinfo()->wasm_memory_size_reference() + diff, SKIP_ICACHE_FLUSH);
  }

  ret_value = runnable.Call();
  CHECK_NE(ret_value, 0xdeadbeef);

#ifdef DEBUG
  code->Print(os);
  ::printf("f() = %" PRIx64 "\n\n", ret_value);
#endif
}

#undef __

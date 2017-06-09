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

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;

#define __ assm.

static int32_t DummyStaticFunction(Object* result) { return 1; }

TEST(WasmRelocationArmMemoryReference) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  DummyStaticFunction(NULL);
  int32_t imm = 1234567;

  Assembler assm(isolate, buffer, sizeof buffer);

  __ mov(r0, Operand(imm, RelocInfo::WASM_MEMORY_REFERENCE));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  CSignature0<int32_t> csig;
  CodeRunner<int32_t> runnable(isolate, code, &csig);
  int32_t ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm);

#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
  ::printf("f() = %d\n\n", ret_value);
#endif
  int offset = 1234;

  // Relocating references by offset
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
  ::printf("f() = %d\n\n", ret_value);
#endif
}

TEST(WasmRelocationArmMemorySizeReference) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  DummyStaticFunction(NULL);
  int32_t size = 512;
  Label fail;

  Assembler assm(isolate, buffer, sizeof buffer);

  __ mov(r0, Operand(size, RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
  __ cmp(r0, Operand(size, RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
  __ b(ne, &fail);
  __ mov(pc, Operand(lr));
  __ bind(&fail);
  __ mov(r0, Operand(0xdeadbeef));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  CSignature0<int32_t> csig;
  CodeRunner<int32_t> runnable(isolate, code, &csig);
  int32_t ret_value = runnable.Call();
  CHECK_NE(ret_value, bit_cast<int32_t>(0xdeadbeef));

#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
  ::printf("f() = %d\n\n", ret_value);
#endif
  size_t diff = 512;

  int mode_mask = (1 << RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    DCHECK(RelocInfo::IsWasmMemorySizeReference(it.rinfo()->rmode()));
    it.rinfo()->update_wasm_memory_size(
        isolate, it.rinfo()->wasm_memory_size_reference(),
        it.rinfo()->wasm_memory_size_reference() + diff, SKIP_ICACHE_FLUSH);
  }

  ret_value = runnable.Call();
  CHECK_NE(ret_value, bit_cast<int32_t>(0xdeadbeef));

#ifdef DEBUG
  code->Print(os);
  ::printf("f() = %d\n\n", ret_value);
#endif
}
#undef __

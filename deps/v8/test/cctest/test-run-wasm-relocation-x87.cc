// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/debug/debug.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/x87/frames-x87.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/compiler/call-tester.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

#define __ assm.

static int32_t DummyStaticFunction(Object* result) { return 1; }

TEST(WasmRelocationX87MemoryReference) {
  Isolate* isolate = CcTest::i_isolate();
  Zone zone(isolate->allocator());
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  Assembler assm(isolate, buffer, sizeof buffer);
  DummyStaticFunction(NULL);
  int32_t imm = 1234567;

  __ mov(eax, Immediate(reinterpret_cast<Address>(imm),
                        RelocInfo::WASM_MEMORY_REFERENCE));
  __ nop();
  __ ret(0);

  CSignature0<int32_t> csig;
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  USE(code);

  CodeRunner<int32_t> runnable(isolate, code, &csig);
  int32_t ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm);

#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
  byte* begin = code->instruction_start();
  byte* end = begin + code->instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif

  size_t offset = 1234;

  // Relocating references by offset
  int mode_mask = (1 << RelocInfo::WASM_MEMORY_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmMemoryReference(mode)) {
      // Dummy values of size used here as the objective of the test is to
      // verify that the immediate is patched correctly
      it.rinfo()->update_wasm_memory_reference(
          it.rinfo()->wasm_memory_reference(),
          it.rinfo()->wasm_memory_reference() + offset, 1, 2,
          SKIP_ICACHE_FLUSH);
    }
  }

  // Check if immediate is updated correctly
  ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm + offset);

#ifdef OBJECT_PRINT
  code->Print(os);
  begin = code->instruction_start();
  end = begin + code->instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif
}

TEST(WasmRelocationX87MemorySizeReference) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Zone zone(isolate->allocator());
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  Assembler assm(isolate, buffer, sizeof buffer);
  DummyStaticFunction(NULL);
  int32_t size = 80;
  Label fail;

  __ mov(eax, Immediate(reinterpret_cast<Address>(size),
                        RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
  __ cmp(eax, Immediate(reinterpret_cast<Address>(size),
                        RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
  __ j(not_equal, &fail);
  __ ret(0);
  __ bind(&fail);
  __ mov(eax, 0xdeadbeef);
  __ ret(0);

  CSignature0<int32_t> csig;
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  USE(code);

  CodeRunner<int32_t> runnable(isolate, code, &csig);
  int32_t ret_value = runnable.Call();
  CHECK_NE(ret_value, 0xdeadbeef);

#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
  byte* begin = code->instruction_start();
  byte* end = begin + code->instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif

  size_t offset = 10;

  int mode_mask = (1 << RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmMemorySizeReference(mode)) {
      it.rinfo()->update_wasm_memory_reference(
          reinterpret_cast<Address>(1234), reinterpret_cast<Address>(1234),
          it.rinfo()->wasm_memory_size_reference(),
          it.rinfo()->wasm_memory_size_reference() + offset, SKIP_ICACHE_FLUSH);
    }
  }

  ret_value = runnable.Call();
  CHECK_NE(ret_value, 0xdeadbeef);

#ifdef OBJECT_PRINT
  code->Print(os);
  begin = code->instruction_start();
  end = begin + code->instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif
}
#undef __

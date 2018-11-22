// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/handles-inl.h"
#include "src/macro-assembler-inl.h"
#include "src/simulator.h"
#include "test/cctest/cctest.h"
#include "test/common/assembler-tester.h"

namespace v8 {
namespace internal {

using F0 = int(int);

#define __ masm.

static constexpr int kNumInstr = 100;

static void FloodWithInc(Isolate* isolate, byte* buffer, size_t allocated) {
  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kYes);
#if V8_TARGET_ARCH_IA32
  __ mov(eax, Operand(esp, kPointerSize));
  for (int i = 0; i < kNumInstr; ++i) {
    __ add(eax, Immediate(1));
  }
#elif V8_TARGET_ARCH_X64
  __ movl(rax, arg_reg_1);
  for (int i = 0; i < kNumInstr; ++i) {
    __ addl(rax, Immediate(1));
  }
#elif V8_TARGET_ARCH_ARM64
  for (int i = 0; i < kNumInstr; ++i) {
    __ Add(x0, x0, Operand(1));
  }
#elif V8_TARGET_ARCH_ARM
  for (int i = 0; i < kNumInstr; ++i) {
    __ add(r0, r0, Operand(1));
  }
#elif V8_TARGET_ARCH_MIPS
  __ mov(v0, a0);
  for (int i = 0; i < kNumInstr; ++i) {
    __ Addu(v0, v0, Operand(1));
  }
#elif V8_TARGET_ARCH_MIPS64
  __ mov(v0, a0);
  for (int i = 0; i < kNumInstr; ++i) {
    __ Addu(v0, v0, Operand(1));
  }
#else
#error Unsupported architecture
#endif
  __ Ret();
}

static void FloodWithNop(Isolate* isolate, byte* buffer, size_t allocated) {
  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kYes);
#if V8_TARGET_ARCH_IA32
  __ mov(eax, Operand(esp, kPointerSize));
#elif V8_TARGET_ARCH_X64
  __ movl(rax, arg_reg_1);
#elif V8_TARGET_ARCH_MIPS
  __ mov(v0, a0);
#elif V8_TARGET_ARCH_MIPS64
  __ mov(v0, a0);
#endif
  for (int i = 0; i < kNumInstr; ++i) {
    __ nop();
  }
  __ Ret();
}

// Order of operation for this test case:
//   exec -> perm(RW) -> patch -> flush -> perm(RX) -> exec
TEST(TestFlushICacheOfWritable) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  FloodWithInc(isolate, buffer, allocated);
  MakeAssemblerBufferExecutable(buffer, allocated);

  // Allow calling the function from C++.
  auto f = GeneratedCode<F0>::FromBuffer(isolate, buffer);

  CHECK_EQ(23 + kNumInstr, f.Call(23));  // Call into generated code.
  CHECK(SetPermissions(GetPlatformPageAllocator(), buffer, allocated,
                       v8::PageAllocator::kReadWrite));
  FloodWithNop(isolate, buffer, allocated);
  Assembler::FlushICache(buffer, allocated);
  CHECK(SetPermissions(GetPlatformPageAllocator(), buffer, allocated,
                       v8::PageAllocator::kReadExecute));
  CHECK_EQ(23, f.Call(23));  // Call into generated code.
}

// Order of operation for this test case:
//   exec -> perm(RW) -> patch -> perm(RX) -> flush -> exec
TEST(TestFlushICacheOfExecutable) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  FloodWithInc(isolate, buffer, allocated);
  MakeAssemblerBufferExecutable(buffer, allocated);

  // Allow calling the function from C++.
  auto f = GeneratedCode<F0>::FromBuffer(isolate, buffer);

  CHECK_EQ(23 + kNumInstr, f.Call(23));  // Call into generated code.
  CHECK(SetPermissions(GetPlatformPageAllocator(), buffer, allocated,
                       v8::PageAllocator::kReadWrite));
  FloodWithNop(isolate, buffer, allocated);
  CHECK(SetPermissions(GetPlatformPageAllocator(), buffer, allocated,
                       v8::PageAllocator::kReadExecute));
  Assembler::FlushICache(buffer, allocated);
  CHECK_EQ(23, f.Call(23));  // Call into generated code.
}

// Order of operation for this test case:
//   exec -> perm(RWX) -> patch -> flush -> exec
TEST(TestFlushICacheOfWritableAndExecutable) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  FloodWithInc(isolate, buffer, allocated);
  MakeAssemblerBufferExecutable(buffer, allocated);

  // Allow calling the function from C++.
  auto f = GeneratedCode<F0>::FromBuffer(isolate, buffer);

  CHECK_EQ(23 + kNumInstr, f.Call(23));  // Call into generated code.
  CHECK(SetPermissions(GetPlatformPageAllocator(), buffer, allocated,
                       v8::PageAllocator::kReadWriteExecute));
  FloodWithNop(isolate, buffer, allocated);
  Assembler::FlushICache(buffer, allocated);
  CHECK_EQ(23, f.Call(23));  // Call into generated code.
}

#undef __

}  // namespace internal
}  // namespace v8

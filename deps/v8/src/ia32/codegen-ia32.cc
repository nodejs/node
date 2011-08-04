// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if defined(V8_TARGET_ARCH_IA32)

#include "codegen.h"

namespace v8 {
namespace internal {


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterInternalFrame();
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveInternalFrame();
}


#define __ masm.

static void MemCopyWrapper(void* dest, const void* src, size_t size) {
  memcpy(dest, src, size);
}


OS::MemCopyFunction CreateMemCopyFunction() {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB,
                                                 &actual_size,
                                                 true));
  if (buffer == NULL) return &MemCopyWrapper;
  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  // Generated code is put into a fixed, unmovable, buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // 32-bit C declaration function calls pass arguments on stack.

  // Stack layout:
  // esp[12]: Third argument, size.
  // esp[8]: Second argument, source pointer.
  // esp[4]: First argument, destination pointer.
  // esp[0]: return address

  const int kDestinationOffset = 1 * kPointerSize;
  const int kSourceOffset = 2 * kPointerSize;
  const int kSizeOffset = 3 * kPointerSize;

  int stack_offset = 0;  // Update if we change the stack height.

  if (FLAG_debug_code) {
    __ cmp(Operand(esp, kSizeOffset + stack_offset),
           Immediate(OS::kMinComplexMemCopy));
    Label ok;
    __ j(greater_equal, &ok);
    __ int3();
    __ bind(&ok);
  }
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope enable(SSE2);
    __ push(edi);
    __ push(esi);
    stack_offset += 2 * kPointerSize;
    Register dst = edi;
    Register src = esi;
    Register count = ecx;
    __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
    __ mov(src, Operand(esp, stack_offset + kSourceOffset));
    __ mov(count, Operand(esp, stack_offset + kSizeOffset));


    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(Operand(dst, 0), xmm0);
    __ mov(edx, dst);
    __ and_(edx, 0xF);
    __ neg(edx);
    __ add(Operand(edx), Immediate(16));
    __ add(dst, Operand(edx));
    __ add(src, Operand(edx));
    __ sub(Operand(count), edx);

    // edi is now aligned. Check if esi is also aligned.
    Label unaligned_source;
    __ test(Operand(src), Immediate(0x0F));
    __ j(not_zero, &unaligned_source);
    {
      // Copy loop for aligned source and destination.
      __ mov(edx, count);
      Register loop_count = ecx;
      Register count = edx;
      __ shr(loop_count, 5);
      {
        // Main copy loop.
        Label loop;
        __ bind(&loop);
        __ prefetch(Operand(src, 0x20), 1);
        __ movdqa(xmm0, Operand(src, 0x00));
        __ movdqa(xmm1, Operand(src, 0x10));
        __ add(Operand(src), Immediate(0x20));

        __ movdqa(Operand(dst, 0x00), xmm0);
        __ movdqa(Operand(dst, 0x10), xmm1);
        __ add(Operand(dst), Immediate(0x20));

        __ dec(loop_count);
        __ j(not_zero, &loop);
      }

      // At most 31 bytes to copy.
      Label move_less_16;
      __ test(Operand(count), Immediate(0x10));
      __ j(zero, &move_less_16);
      __ movdqa(xmm0, Operand(src, 0));
      __ add(Operand(src), Immediate(0x10));
      __ movdqa(Operand(dst, 0), xmm0);
      __ add(Operand(dst), Immediate(0x10));
      __ bind(&move_less_16);

      // At most 15 bytes to copy. Copy 16 bytes at end of string.
      __ and_(count, 0xF);
      __ movdqu(xmm0, Operand(src, count, times_1, -0x10));
      __ movdqu(Operand(dst, count, times_1, -0x10), xmm0);

      __ mov(eax, Operand(esp, stack_offset + kDestinationOffset));
      __ pop(esi);
      __ pop(edi);
      __ ret(0);
    }
    __ Align(16);
    {
      // Copy loop for unaligned source and aligned destination.
      // If source is not aligned, we can't read it as efficiently.
      __ bind(&unaligned_source);
      __ mov(edx, ecx);
      Register loop_count = ecx;
      Register count = edx;
      __ shr(loop_count, 5);
      {
        // Main copy loop
        Label loop;
        __ bind(&loop);
        __ prefetch(Operand(src, 0x20), 1);
        __ movdqu(xmm0, Operand(src, 0x00));
        __ movdqu(xmm1, Operand(src, 0x10));
        __ add(Operand(src), Immediate(0x20));

        __ movdqa(Operand(dst, 0x00), xmm0);
        __ movdqa(Operand(dst, 0x10), xmm1);
        __ add(Operand(dst), Immediate(0x20));

        __ dec(loop_count);
        __ j(not_zero, &loop);
      }

      // At most 31 bytes to copy.
      Label move_less_16;
      __ test(Operand(count), Immediate(0x10));
      __ j(zero, &move_less_16);
      __ movdqu(xmm0, Operand(src, 0));
      __ add(Operand(src), Immediate(0x10));
      __ movdqa(Operand(dst, 0), xmm0);
      __ add(Operand(dst), Immediate(0x10));
      __ bind(&move_less_16);

      // At most 15 bytes to copy. Copy 16 bytes at end of string.
      __ and_(count, 0x0F);
      __ movdqu(xmm0, Operand(src, count, times_1, -0x10));
      __ movdqu(Operand(dst, count, times_1, -0x10), xmm0);

      __ mov(eax, Operand(esp, stack_offset + kDestinationOffset));
      __ pop(esi);
      __ pop(edi);
      __ ret(0);
    }

  } else {
    // SSE2 not supported. Unlikely to happen in practice.
    __ push(edi);
    __ push(esi);
    stack_offset += 2 * kPointerSize;
    __ cld();
    Register dst = edi;
    Register src = esi;
    Register count = ecx;
    __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
    __ mov(src, Operand(esp, stack_offset + kSourceOffset));
    __ mov(count, Operand(esp, stack_offset + kSizeOffset));

    // Copy the first word.
    __ mov(eax, Operand(src, 0));
    __ mov(Operand(dst, 0), eax);

    // Increment src,dstso that dst is aligned.
    __ mov(edx, dst);
    __ and_(edx, 0x03);
    __ neg(edx);
    __ add(Operand(edx), Immediate(4));  // edx = 4 - (dst & 3)
    __ add(dst, Operand(edx));
    __ add(src, Operand(edx));
    __ sub(Operand(count), edx);
    // edi is now aligned, ecx holds number of remaning bytes to copy.

    __ mov(edx, count);
    count = edx;
    __ shr(ecx, 2);  // Make word count instead of byte count.
    __ rep_movs();

    // At most 3 bytes left to copy. Copy 4 bytes at end of string.
    __ and_(count, 3);
    __ mov(eax, Operand(src, count, times_1, -4));
    __ mov(Operand(dst, count, times_1, -4), eax);

    __ mov(eax, Operand(esp, stack_offset + kDestinationOffset));
    __ pop(esi);
    __ pop(edi);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(desc.reloc_size == 0);

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<OS::MemCopyFunction>(buffer);
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32

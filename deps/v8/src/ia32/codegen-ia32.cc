// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/codegen.h"
#include "src/factory-inl.h"
#include "src/heap/heap.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

#define __ masm.

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);
  // esp[1 * kPointerSize]: raw double input
  // esp[0 * kPointerSize]: return address
  // Move double input into registers.
  {
    __ movsd(xmm0, Operand(esp, 1 * kPointerSize));
    __ sqrtsd(xmm0, xmm0);
    __ movsd(Operand(esp, 1 * kPointerSize), xmm0);
    // Load result into floating point register as return value.
    __ fld_d(Operand(esp, 1 * kPointerSize));
    __ Ret();
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
}


// Helper functions for CreateMemMoveFunction.
#undef __
#define __ ACCESS_MASM(masm)

enum Direction { FORWARD, BACKWARD };
enum Alignment { MOVE_ALIGNED, MOVE_UNALIGNED };

// Expects registers:
// esi - source, aligned if alignment == ALIGNED
// edi - destination, always aligned
// ecx - count (copy size in bytes)
// edx - loop count (number of 64 byte chunks)
void MemMoveEmitMainLoop(MacroAssembler* masm,
                         Label* move_last_15,
                         Direction direction,
                         Alignment alignment) {
  Register src = esi;
  Register dst = edi;
  Register count = ecx;
  Register loop_count = edx;
  Label loop, move_last_31, move_last_63;
  __ cmp(loop_count, 0);
  __ j(equal, &move_last_63);
  __ bind(&loop);
  // Main loop. Copy in 64 byte chunks.
  if (direction == BACKWARD) __ sub(src, Immediate(0x40));
  __ movdq(alignment == MOVE_ALIGNED, xmm0, Operand(src, 0x00));
  __ movdq(alignment == MOVE_ALIGNED, xmm1, Operand(src, 0x10));
  __ movdq(alignment == MOVE_ALIGNED, xmm2, Operand(src, 0x20));
  __ movdq(alignment == MOVE_ALIGNED, xmm3, Operand(src, 0x30));
  if (direction == FORWARD) __ add(src, Immediate(0x40));
  if (direction == BACKWARD) __ sub(dst, Immediate(0x40));
  __ movdqa(Operand(dst, 0x00), xmm0);
  __ movdqa(Operand(dst, 0x10), xmm1);
  __ movdqa(Operand(dst, 0x20), xmm2);
  __ movdqa(Operand(dst, 0x30), xmm3);
  if (direction == FORWARD) __ add(dst, Immediate(0x40));
  __ dec(loop_count);
  __ j(not_zero, &loop);
  // At most 63 bytes left to copy.
  __ bind(&move_last_63);
  __ test(count, Immediate(0x20));
  __ j(zero, &move_last_31);
  if (direction == BACKWARD) __ sub(src, Immediate(0x20));
  __ movdq(alignment == MOVE_ALIGNED, xmm0, Operand(src, 0x00));
  __ movdq(alignment == MOVE_ALIGNED, xmm1, Operand(src, 0x10));
  if (direction == FORWARD) __ add(src, Immediate(0x20));
  if (direction == BACKWARD) __ sub(dst, Immediate(0x20));
  __ movdqa(Operand(dst, 0x00), xmm0);
  __ movdqa(Operand(dst, 0x10), xmm1);
  if (direction == FORWARD) __ add(dst, Immediate(0x20));
  // At most 31 bytes left to copy.
  __ bind(&move_last_31);
  __ test(count, Immediate(0x10));
  __ j(zero, move_last_15);
  if (direction == BACKWARD) __ sub(src, Immediate(0x10));
  __ movdq(alignment == MOVE_ALIGNED, xmm0, Operand(src, 0));
  if (direction == FORWARD) __ add(src, Immediate(0x10));
  if (direction == BACKWARD) __ sub(dst, Immediate(0x10));
  __ movdqa(Operand(dst, 0), xmm0);
  if (direction == FORWARD) __ add(dst, Immediate(0x10));
}


void MemMoveEmitPopAndReturn(MacroAssembler* masm) {
  __ pop(esi);
  __ pop(edi);
  __ ret(0);
}


#undef __
#define __ masm.


class LabelConverter {
 public:
  explicit LabelConverter(byte* buffer) : buffer_(buffer) {}
  int32_t address(Label* l) const {
    return reinterpret_cast<int32_t>(buffer_) + l->pos();
  }
 private:
  byte* buffer_;
};


MemMoveFunction CreateMemMoveFunction(Isolate* isolate) {
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);
  LabelConverter conv(buffer);

  // Generated code is put into a fixed, unmovable buffer, and not into
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

  // When copying up to this many bytes, use special "small" handlers.
  const size_t kSmallCopySize = 8;
  // When copying up to this many bytes, use special "medium" handlers.
  const size_t kMediumCopySize = 63;
  // When non-overlapping region of src and dst is less than this,
  // use a more careful implementation (slightly slower).
  const size_t kMinMoveDistance = 16;
  // Note that these values are dictated by the implementation below,
  // do not just change them and hope things will work!

  int stack_offset = 0;  // Update if we change the stack height.

  Label backward, backward_much_overlap;
  Label forward_much_overlap, small_size, medium_size, pop_and_return;
  __ push(edi);
  __ push(esi);
  stack_offset += 2 * kPointerSize;
  Register dst = edi;
  Register src = esi;
  Register count = ecx;
  Register loop_count = edx;
  __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
  __ mov(src, Operand(esp, stack_offset + kSourceOffset));
  __ mov(count, Operand(esp, stack_offset + kSizeOffset));

  __ cmp(dst, src);
  __ j(equal, &pop_and_return);

  __ prefetch(Operand(src, 0), 1);
  __ cmp(count, kSmallCopySize);
  __ j(below_equal, &small_size);
  __ cmp(count, kMediumCopySize);
  __ j(below_equal, &medium_size);
  __ cmp(dst, src);
  __ j(above, &backward);

  {
    // |dst| is a lower address than |src|. Copy front-to-back.
    Label unaligned_source, move_last_15, skip_last_move;
    __ mov(eax, src);
    __ sub(eax, dst);
    __ cmp(eax, kMinMoveDistance);
    __ j(below, &forward_much_overlap);
    // Copy first 16 bytes.
    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(Operand(dst, 0), xmm0);
    // Determine distance to alignment: 16 - (dst & 0xF).
    __ mov(edx, dst);
    __ and_(edx, 0xF);
    __ neg(edx);
    __ add(edx, Immediate(16));
    __ add(dst, edx);
    __ add(src, edx);
    __ sub(count, edx);
    // dst is now aligned. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    // Check if src is also aligned.
    __ test(src, Immediate(0xF));
    __ j(not_zero, &unaligned_source);
    // Copy loop for aligned source and destination.
    MemMoveEmitMainLoop(&masm, &move_last_15, FORWARD, MOVE_ALIGNED);
    // At most 15 bytes to copy. Copy 16 bytes at end of string.
    __ bind(&move_last_15);
    __ and_(count, 0xF);
    __ j(zero, &skip_last_move, Label::kNear);
    __ movdqu(xmm0, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm0);
    __ bind(&skip_last_move);
    MemMoveEmitPopAndReturn(&masm);

    // Copy loop for unaligned source and aligned destination.
    __ bind(&unaligned_source);
    MemMoveEmitMainLoop(&masm, &move_last_15, FORWARD, MOVE_UNALIGNED);
    __ jmp(&move_last_15);

    // Less than kMinMoveDistance offset between dst and src.
    Label loop_until_aligned, last_15_much_overlap;
    __ bind(&loop_until_aligned);
    __ mov_b(eax, Operand(src, 0));
    __ inc(src);
    __ mov_b(Operand(dst, 0), eax);
    __ inc(dst);
    __ dec(count);
    __ bind(&forward_much_overlap);  // Entry point into this block.
    __ test(dst, Immediate(0xF));
    __ j(not_zero, &loop_until_aligned);
    // dst is now aligned, src can't be. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    MemMoveEmitMainLoop(&masm, &last_15_much_overlap,
                        FORWARD, MOVE_UNALIGNED);
    __ bind(&last_15_much_overlap);
    __ and_(count, 0xF);
    __ j(zero, &pop_and_return);
    __ cmp(count, kSmallCopySize);
    __ j(below_equal, &small_size);
    __ jmp(&medium_size);
  }

  {
    // |dst| is a higher address than |src|. Copy backwards.
    Label unaligned_source, move_first_15, skip_last_move;
    __ bind(&backward);
    // |dst| and |src| always point to the end of what's left to copy.
    __ add(dst, count);
    __ add(src, count);
    __ mov(eax, dst);
    __ sub(eax, src);
    __ cmp(eax, kMinMoveDistance);
    __ j(below, &backward_much_overlap);
    // Copy last 16 bytes.
    __ movdqu(xmm0, Operand(src, -0x10));
    __ movdqu(Operand(dst, -0x10), xmm0);
    // Find distance to alignment: dst & 0xF
    __ mov(edx, dst);
    __ and_(edx, 0xF);
    __ sub(dst, edx);
    __ sub(src, edx);
    __ sub(count, edx);
    // dst is now aligned. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    // Check if src is also aligned.
    __ test(src, Immediate(0xF));
    __ j(not_zero, &unaligned_source);
    // Copy loop for aligned source and destination.
    MemMoveEmitMainLoop(&masm, &move_first_15, BACKWARD, MOVE_ALIGNED);
    // At most 15 bytes to copy. Copy 16 bytes at beginning of string.
    __ bind(&move_first_15);
    __ and_(count, 0xF);
    __ j(zero, &skip_last_move, Label::kNear);
    __ sub(src, count);
    __ sub(dst, count);
    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(Operand(dst, 0), xmm0);
    __ bind(&skip_last_move);
    MemMoveEmitPopAndReturn(&masm);

    // Copy loop for unaligned source and aligned destination.
    __ bind(&unaligned_source);
    MemMoveEmitMainLoop(&masm, &move_first_15, BACKWARD, MOVE_UNALIGNED);
    __ jmp(&move_first_15);

    // Less than kMinMoveDistance offset between dst and src.
    Label loop_until_aligned, first_15_much_overlap;
    __ bind(&loop_until_aligned);
    __ dec(src);
    __ dec(dst);
    __ mov_b(eax, Operand(src, 0));
    __ mov_b(Operand(dst, 0), eax);
    __ dec(count);
    __ bind(&backward_much_overlap);  // Entry point into this block.
    __ test(dst, Immediate(0xF));
    __ j(not_zero, &loop_until_aligned);
    // dst is now aligned, src can't be. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    MemMoveEmitMainLoop(&masm, &first_15_much_overlap,
                        BACKWARD, MOVE_UNALIGNED);
    __ bind(&first_15_much_overlap);
    __ and_(count, 0xF);
    __ j(zero, &pop_and_return);
    // Small/medium handlers expect dst/src to point to the beginning.
    __ sub(dst, count);
    __ sub(src, count);
    __ cmp(count, kSmallCopySize);
    __ j(below_equal, &small_size);
    __ jmp(&medium_size);
  }
  {
    // Special handlers for 9 <= copy_size < 64. No assumptions about
    // alignment or move distance, so all reads must be unaligned and
    // must happen before any writes.
    Label medium_handlers, f9_16, f17_32, f33_48, f49_63;

    __ bind(&f9_16);
    __ movsd(xmm0, Operand(src, 0));
    __ movsd(xmm1, Operand(src, count, times_1, -8));
    __ movsd(Operand(dst, 0), xmm0);
    __ movsd(Operand(dst, count, times_1, -8), xmm1);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f17_32);
    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(xmm1, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, 0x00), xmm0);
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm1);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f33_48);
    __ movdqu(xmm0, Operand(src, 0x00));
    __ movdqu(xmm1, Operand(src, 0x10));
    __ movdqu(xmm2, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, 0x00), xmm0);
    __ movdqu(Operand(dst, 0x10), xmm1);
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm2);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f49_63);
    __ movdqu(xmm0, Operand(src, 0x00));
    __ movdqu(xmm1, Operand(src, 0x10));
    __ movdqu(xmm2, Operand(src, 0x20));
    __ movdqu(xmm3, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, 0x00), xmm0);
    __ movdqu(Operand(dst, 0x10), xmm1);
    __ movdqu(Operand(dst, 0x20), xmm2);
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm3);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&medium_handlers);
    __ dd(conv.address(&f9_16));
    __ dd(conv.address(&f17_32));
    __ dd(conv.address(&f33_48));
    __ dd(conv.address(&f49_63));

    __ bind(&medium_size);  // Entry point into this block.
    __ mov(eax, count);
    __ dec(eax);
    __ shr(eax, 4);
    if (FLAG_debug_code) {
      Label ok;
      __ cmp(eax, 3);
      __ j(below_equal, &ok);
      __ int3();
      __ bind(&ok);
    }
    __ mov(eax, Operand(eax, times_4, conv.address(&medium_handlers)));
    __ jmp(eax);
  }
  {
    // Specialized copiers for copy_size <= 8 bytes.
    Label small_handlers, f0, f1, f2, f3, f4, f5_8;
    __ bind(&f0);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f1);
    __ mov_b(eax, Operand(src, 0));
    __ mov_b(Operand(dst, 0), eax);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f2);
    __ mov_w(eax, Operand(src, 0));
    __ mov_w(Operand(dst, 0), eax);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f3);
    __ mov_w(eax, Operand(src, 0));
    __ mov_b(edx, Operand(src, 2));
    __ mov_w(Operand(dst, 0), eax);
    __ mov_b(Operand(dst, 2), edx);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f4);
    __ mov(eax, Operand(src, 0));
    __ mov(Operand(dst, 0), eax);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&f5_8);
    __ mov(eax, Operand(src, 0));
    __ mov(edx, Operand(src, count, times_1, -4));
    __ mov(Operand(dst, 0), eax);
    __ mov(Operand(dst, count, times_1, -4), edx);
    MemMoveEmitPopAndReturn(&masm);

    __ bind(&small_handlers);
    __ dd(conv.address(&f0));
    __ dd(conv.address(&f1));
    __ dd(conv.address(&f2));
    __ dd(conv.address(&f3));
    __ dd(conv.address(&f4));
    __ dd(conv.address(&f5_8));
    __ dd(conv.address(&f5_8));
    __ dd(conv.address(&f5_8));
    __ dd(conv.address(&f5_8));

    __ bind(&small_size);  // Entry point into this block.
    if (FLAG_debug_code) {
      Label ok;
      __ cmp(count, 8);
      __ j(below_equal, &ok);
      __ int3();
      __ bind(&ok);
    }
    __ mov(eax, Operand(count, times_4, conv.address(&small_handlers)));
    __ jmp(eax);
  }

  __ bind(&pop_and_return);
  MemMoveEmitPopAndReturn(&masm);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));
  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  // TODO(jkummerow): It would be nice to register this code creation event
  // with the PROFILE / GDBJIT system.
  return FUNCTION_CAST<MemMoveFunction>(buffer);
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

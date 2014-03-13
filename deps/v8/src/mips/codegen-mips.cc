// Copyright 2012 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_MIPS

#include "codegen.h"
#include "macro-assembler.h"
#include "simulator-mips.h"

namespace v8 {
namespace internal {


#define __ masm.


#if defined(USE_SIMULATOR)
byte* fast_exp_mips_machine_code = NULL;
double fast_exp_simulator(double x) {
  return Simulator::current(Isolate::Current())->CallFP(
      fast_exp_mips_machine_code, x, 0);
}
#endif


UnaryMathFunction CreateExpFunction() {
  if (!FLAG_fast_math) return &std::exp;
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return &std::exp;
  ExternalReference::InitializeMathExpData();

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  {
    DoubleRegister input = f12;
    DoubleRegister result = f0;
    DoubleRegister double_scratch1 = f4;
    DoubleRegister double_scratch2 = f6;
    Register temp1 = t0;
    Register temp2 = t1;
    Register temp3 = t2;

    if (!IsMipsSoftFloatABI) {
      // Input value is in f12 anyway, nothing to do.
    } else {
      __ Move(input, a0, a1);
    }
    __ Push(temp3, temp2, temp1);
    MathExpGenerator::EmitMathExp(
        &masm, input, result, double_scratch1, double_scratch2,
        temp1, temp2, temp3);
    __ Pop(temp3, temp2, temp1);
    if (!IsMipsSoftFloatABI) {
      // Result is already in f0, nothing to do.
    } else {
      __ Move(v0, v1, result);
    }
    __ Ret();
  }

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);

#if !defined(USE_SIMULATOR)
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
#else
  fast_exp_mips_machine_code = buffer;
  return &fast_exp_simulator;
#endif
}


#if defined(V8_HOST_ARCH_MIPS)
OS::MemCopyUint8Function CreateMemCopyUint8Function(
      OS::MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  if (Serializer::enabled()) {
     return stub;
  }

  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(3 * KB, &actual_size, true));
  if (buffer == NULL) return stub;

  // This code assumes that cache lines are 32 bytes and if the cache line is
  // larger it will not work correctly.
  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  {
    Label lastb, unaligned, aligned, chkw,
          loop16w, chk1w, wordCopy_loop, skip_pref, lastbloop,
          leave, ua_chk16w, ua_loop16w, ua_skip_pref, ua_chkw,
          ua_chk1w, ua_wordCopy_loop, ua_smallCopy, ua_smallCopy_loop;

    // The size of each prefetch.
    uint32_t pref_chunk = 32;
    // The maximum size of a prefetch, it must not be less then pref_chunk.
    // If the real size of a prefetch is greater then max_pref_size and
    // the kPrefHintPrepareForStore hint is used, the code will not work
    // correctly.
    uint32_t max_pref_size = 128;
    ASSERT(pref_chunk < max_pref_size);

    // pref_limit is set based on the fact that we never use an offset
    // greater then 5 on a store pref and that a single pref can
    // never be larger then max_pref_size.
    uint32_t pref_limit = (5 * pref_chunk) + max_pref_size;
    int32_t pref_hint_load = kPrefHintLoadStreamed;
    int32_t pref_hint_store = kPrefHintPrepareForStore;
    uint32_t loadstore_chunk = 4;

    // The initial prefetches may fetch bytes that are before the buffer being
    // copied. Start copies with an offset of 4 so avoid this situation when
    // using kPrefHintPrepareForStore.
    ASSERT(pref_hint_store != kPrefHintPrepareForStore ||
           pref_chunk * 4 >= max_pref_size);

    // If the size is less than 8, go to lastb. Regardless of size,
    // copy dst pointer to v0 for the retuen value.
    __ slti(t2, a2, 2 * loadstore_chunk);
    __ bne(t2, zero_reg, &lastb);
    __ mov(v0, a0);  // In delay slot.

    // If src and dst have different alignments, go to unaligned, if they
    // have the same alignment (but are not actually aligned) do a partial
    // load/store to make them aligned. If they are both already aligned
    // we can start copying at aligned.
    __ xor_(t8, a1, a0);
    __ andi(t8, t8, loadstore_chunk - 1);  // t8 is a0/a1 word-displacement.
    __ bne(t8, zero_reg, &unaligned);
    __ subu(a3, zero_reg, a0);  // In delay slot.

    __ andi(a3, a3, loadstore_chunk - 1);  // Copy a3 bytes to align a0/a1.
    __ beq(a3, zero_reg, &aligned);  // Already aligned.
    __ subu(a2, a2, a3);  // In delay slot. a2 is the remining bytes count.

    __ lwr(t8, MemOperand(a1));
    __ addu(a1, a1, a3);
    __ swr(t8, MemOperand(a0));
    __ addu(a0, a0, a3);

    // Now dst/src are both aligned to (word) aligned addresses. Set a2 to
    // count how many bytes we have to copy after all the 64 byte chunks are
    // copied and a3 to the dst pointer after all the 64 byte chunks have been
    // copied. We will loop, incrementing a0 and a1 until a0 equals a3.
    __ bind(&aligned);
    __ andi(t8, a2, 0x3f);
    __ beq(a2, t8, &chkw);  // Less than 64?
    __ subu(a3, a2, t8);  // In delay slot.
    __ addu(a3, a0, a3);  // Now a3 is the final dst after loop.

    // When in the loop we prefetch with kPrefHintPrepareForStore hint,
    // in this case the a0+x should be past the "t0-32" address. This means:
    // for x=128 the last "safe" a0 address is "t0-160". Alternatively, for
    // x=64 the last "safe" a0 address is "t0-96". In the current version we
    // will use "pref hint, 128(a0)", so "t0-160" is the limit.
    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ addu(t0, a0, a2);  // t0 is the "past the end" address.
      __ Subu(t9, t0, pref_limit);  // t9 is the "last safe pref" address.
    }

    __ Pref(pref_hint_load, MemOperand(a1, 0 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 1 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 2 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 3 * pref_chunk));

    if (pref_hint_store != kPrefHintPrepareForStore) {
      __ Pref(pref_hint_store, MemOperand(a0, 1 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 2 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 3 * pref_chunk));
    }
    __ bind(&loop16w);
    __ lw(t0, MemOperand(a1));

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ sltu(v1, t9, a0);  // If a0 > t9, don't use next prefetch.
      __ Branch(USE_DELAY_SLOT, &skip_pref, gt, v1, Operand(zero_reg));
    }
    __ lw(t1, MemOperand(a1, 1, loadstore_chunk));  // Maybe in delay slot.

    __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
    __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

    __ bind(&skip_pref);
    __ lw(t2, MemOperand(a1, 2, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 3, loadstore_chunk));
    __ lw(t4, MemOperand(a1, 4, loadstore_chunk));
    __ lw(t5, MemOperand(a1, 5, loadstore_chunk));
    __ lw(t6, MemOperand(a1, 6, loadstore_chunk));
    __ lw(t7, MemOperand(a1, 7, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));

    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));

    __ lw(t0, MemOperand(a1, 8, loadstore_chunk));
    __ lw(t1, MemOperand(a1, 9, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 10, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 11, loadstore_chunk));
    __ lw(t4, MemOperand(a1, 12, loadstore_chunk));
    __ lw(t5, MemOperand(a1, 13, loadstore_chunk));
    __ lw(t6, MemOperand(a1, 14, loadstore_chunk));
    __ lw(t7, MemOperand(a1, 15, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));

    __ sw(t0, MemOperand(a0, 8, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 9, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 10, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 11, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 12, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 13, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 14, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 15, loadstore_chunk));
    __ addiu(a0, a0, 16 * loadstore_chunk);
    __ bne(a0, a3, &loop16w);
    __ addiu(a1, a1, 16 * loadstore_chunk);  // In delay slot.
    __ mov(a2, t8);

    // Here we have src and dest word-aligned but less than 64-bytes to go.
    // Check for a 32 bytes chunk and copy if there is one. Otherwise jump
    // down to chk1w to handle the tail end of the copy.
    __ bind(&chkw);
    __ Pref(pref_hint_load, MemOperand(a1, 0 * pref_chunk));
    __ andi(t8, a2, 0x1f);
    __ beq(a2, t8, &chk1w);  // Less than 32?
    __ nop();  // In delay slot.
    __ lw(t0, MemOperand(a1));
    __ lw(t1, MemOperand(a1, 1, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 2, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 3, loadstore_chunk));
    __ lw(t4, MemOperand(a1, 4, loadstore_chunk));
    __ lw(t5, MemOperand(a1, 5, loadstore_chunk));
    __ lw(t6, MemOperand(a1, 6, loadstore_chunk));
    __ lw(t7, MemOperand(a1, 7, loadstore_chunk));
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    __ addiu(a0, a0, 8 * loadstore_chunk);

    // Here we have less than 32 bytes to copy. Set up for a loop to copy
    // one word at a time. Set a2 to count how many bytes we have to copy
    // after all the word chunks are copied and a3 to the dst pointer after
    // all the word chunks have been copied. We will loop, incrementing a0
    // and a1 untill a0 equals a3.
    __ bind(&chk1w);
    __ andi(a2, t8, loadstore_chunk - 1);
    __ beq(a2, t8, &lastb);
    __ subu(a3, t8, a2);  // In delay slot.
    __ addu(a3, a0, a3);

    __ bind(&wordCopy_loop);
    __ lw(t3, MemOperand(a1));
    __ addiu(a0, a0, loadstore_chunk);
    __ addiu(a1, a1, loadstore_chunk);
    __ bne(a0, a3, &wordCopy_loop);
    __ sw(t3, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

    __ bind(&lastb);
    __ Branch(&leave, le, a2, Operand(zero_reg));
    __ addu(a3, a0, a2);

    __ bind(&lastbloop);
    __ lb(v1, MemOperand(a1));
    __ addiu(a0, a0, 1);
    __ addiu(a1, a1, 1);
    __ bne(a0, a3, &lastbloop);
    __ sb(v1, MemOperand(a0, -1));  // In delay slot.

    __ bind(&leave);
    __ jr(ra);
    __ nop();

    // Unaligned case. Only the dst gets aligned so we need to do partial
    // loads of the source followed by normal stores to the dst (once we
    // have aligned the destination).
    __ bind(&unaligned);
    __ andi(a3, a3, loadstore_chunk - 1);  // Copy a3 bytes to align a0/a1.
    __ beq(a3, zero_reg, &ua_chk16w);
    __ subu(a2, a2, a3);  // In delay slot.

    __ lwr(v1, MemOperand(a1));
    __ lwl(v1,
           MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    __ addu(a1, a1, a3);
    __ swr(v1, MemOperand(a0));
    __ addu(a0, a0, a3);

    // Now the dst (but not the source) is aligned. Set a2 to count how many
    // bytes we have to copy after all the 64 byte chunks are copied and a3 to
    // the dst pointer after all the 64 byte chunks have been copied. We will
    // loop, incrementing a0 and a1 until a0 equals a3.
    __ bind(&ua_chk16w);
    __ andi(t8, a2, 0x3f);
    __ beq(a2, t8, &ua_chkw);
    __ subu(a3, a2, t8);  // In delay slot.
    __ addu(a3, a0, a3);

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ addu(t0, a0, a2);
      __ Subu(t9, t0, pref_limit);
    }

    __ Pref(pref_hint_load, MemOperand(a1, 0 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 1 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 2 * pref_chunk));

    if (pref_hint_store != kPrefHintPrepareForStore) {
      __ Pref(pref_hint_store, MemOperand(a0, 1 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 2 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 3 * pref_chunk));
    }

    __ bind(&ua_loop16w);
    __ Pref(pref_hint_load, MemOperand(a1, 3 * pref_chunk));
    __ lwr(t0, MemOperand(a1));
    __ lwr(t1, MemOperand(a1, 1, loadstore_chunk));
    __ lwr(t2, MemOperand(a1, 2, loadstore_chunk));

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ sltu(v1, t9, a0);
      __ Branch(USE_DELAY_SLOT, &ua_skip_pref, gt, v1, Operand(zero_reg));
    }
    __ lwr(t3, MemOperand(a1, 3, loadstore_chunk));  // Maybe in delay slot.

    __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
    __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

    __ bind(&ua_skip_pref);
    __ lwr(t4, MemOperand(a1, 4, loadstore_chunk));
    __ lwr(t5, MemOperand(a1, 5, loadstore_chunk));
    __ lwr(t6, MemOperand(a1, 6, loadstore_chunk));
    __ lwr(t7, MemOperand(a1, 7, loadstore_chunk));
    __ lwl(t0,
           MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t1,
           MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t2,
           MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t3,
           MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t4,
           MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t5,
           MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t6,
           MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t7,
           MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    __ lwr(t0, MemOperand(a1, 8, loadstore_chunk));
    __ lwr(t1, MemOperand(a1, 9, loadstore_chunk));
    __ lwr(t2, MemOperand(a1, 10, loadstore_chunk));
    __ lwr(t3, MemOperand(a1, 11, loadstore_chunk));
    __ lwr(t4, MemOperand(a1, 12, loadstore_chunk));
    __ lwr(t5, MemOperand(a1, 13, loadstore_chunk));
    __ lwr(t6, MemOperand(a1, 14, loadstore_chunk));
    __ lwr(t7, MemOperand(a1, 15, loadstore_chunk));
    __ lwl(t0,
           MemOperand(a1, 9, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t1,
           MemOperand(a1, 10, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t2,
           MemOperand(a1, 11, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t3,
           MemOperand(a1, 12, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t4,
           MemOperand(a1, 13, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t5,
           MemOperand(a1, 14, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t6,
           MemOperand(a1, 15, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t7,
           MemOperand(a1, 16, loadstore_chunk, MemOperand::offset_minus_one));
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));
    __ sw(t0, MemOperand(a0, 8, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 9, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 10, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 11, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 12, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 13, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 14, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 15, loadstore_chunk));
    __ addiu(a0, a0, 16 * loadstore_chunk);
    __ bne(a0, a3, &ua_loop16w);
    __ addiu(a1, a1, 16 * loadstore_chunk);  // In delay slot.
    __ mov(a2, t8);

    // Here less than 64-bytes. Check for
    // a 32 byte chunk and copy if there is one. Otherwise jump down to
    // ua_chk1w to handle the tail end of the copy.
    __ bind(&ua_chkw);
    __ Pref(pref_hint_load, MemOperand(a1));
    __ andi(t8, a2, 0x1f);

    __ beq(a2, t8, &ua_chk1w);
    __ nop();  // In delay slot.
    __ lwr(t0, MemOperand(a1));
    __ lwr(t1, MemOperand(a1, 1, loadstore_chunk));
    __ lwr(t2, MemOperand(a1, 2, loadstore_chunk));
    __ lwr(t3, MemOperand(a1, 3, loadstore_chunk));
    __ lwr(t4, MemOperand(a1, 4, loadstore_chunk));
    __ lwr(t5, MemOperand(a1, 5, loadstore_chunk));
    __ lwr(t6, MemOperand(a1, 6, loadstore_chunk));
    __ lwr(t7, MemOperand(a1, 7, loadstore_chunk));
    __ lwl(t0,
           MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t1,
           MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t2,
           MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t3,
           MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t4,
           MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t5,
           MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t6,
           MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t7,
           MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    __ addiu(a0, a0, 8 * loadstore_chunk);

    // Less than 32 bytes to copy. Set up for a loop to
    // copy one word at a time.
    __ bind(&ua_chk1w);
    __ andi(a2, t8, loadstore_chunk - 1);
    __ beq(a2, t8, &ua_smallCopy);
    __ subu(a3, t8, a2);  // In delay slot.
    __ addu(a3, a0, a3);

    __ bind(&ua_wordCopy_loop);
    __ lwr(v1, MemOperand(a1));
    __ lwl(v1,
           MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    __ addiu(a0, a0, loadstore_chunk);
    __ addiu(a1, a1, loadstore_chunk);
    __ bne(a0, a3, &ua_wordCopy_loop);
    __ sw(v1, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

    // Copy the last 8 bytes.
    __ bind(&ua_smallCopy);
    __ beq(a2, zero_reg, &leave);
    __ addu(a3, a0, a2);  // In delay slot.

    __ bind(&ua_smallCopy_loop);
    __ lb(v1, MemOperand(a1));
    __ addiu(a0, a0, 1);
    __ addiu(a1, a1, 1);
    __ bne(a0, a3, &ua_smallCopy_loop);
    __ sb(v1, MemOperand(a0, -1));  // In delay slot.

    __ jr(ra);
    __ nop();
  }
  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<OS::MemCopyUint8Function>(buffer);
#endif
}
#endif

UnaryMathFunction CreateSqrtFunction() {
#if defined(USE_SIMULATOR)
  return &std::sqrt;
#else
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return &std::sqrt;

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  __ MovFromFloatParameter(f12);
  __ sqrt_d(f0, f12);
  __ MovToFloatResult(f0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
#endif
}

#undef __


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  ASSERT(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  ASSERT(masm->has_frame());
  masm->set_has_frame(false);
}


// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm, AllocationSiteMode mode,
    Label* allocation_memento_found) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  if (mode == TRACK_ALLOCATION_SITE) {
    ASSERT(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(a2, t0, allocation_memento_found);
  }

  // Set transitioned map.
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map, done;

  Register scratch = t6;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(a2, t0, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ lw(t0, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ Branch(&only_change_map, eq, at, Operand(t0));

  __ push(ra);
  __ lw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: source FixedArray
  // t1: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  __ sll(scratch, t1, 2);
  __ Addu(scratch, scratch, FixedDoubleArray::kHeaderSize);
  __ Allocate(scratch, t2, t3, t5, &gc_required, DOUBLE_ALIGNMENT);
  // t2: destination FixedDoubleArray, not tagged as heap object

  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(t5, Heap::kFixedDoubleArrayMapRootIndex);
  __ sw(t1, MemOperand(t2, FixedDoubleArray::kLengthOffset));
  __ sw(t5, MemOperand(t2, HeapObject::kMapOffset));
  // Update receiver's map.

  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ Addu(a3, t2, Operand(kHeapObjectTag));
  __ sw(a3, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ RecordWriteField(a2,
                      JSObject::kElementsOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);


  // Prepare for conversion loop.
  __ Addu(a3, t0, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Addu(t3, t2, Operand(FixedDoubleArray::kHeaderSize));
  __ sll(t2, t1, 2);
  __ Addu(t2, t2, t3);
  __ li(t0, Operand(kHoleNanLower32));
  __ li(t1, Operand(kHoleNanUpper32));
  // t0: kHoleNanLower32
  // t1: kHoleNanUpper32
  // t2: end of destination FixedDoubleArray, not tagged
  // t3: begin of FixedDoubleArray element fields, not tagged

  __ Branch(&entry);

  __ bind(&only_change_map);
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Branch(&done);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ pop(ra);
  __ Branch(fail);

  // Convert and copy elements.
  __ bind(&loop);
  __ lw(t5, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);
  // t5: current element
  __ UntagAndJumpIfNotSmi(t5, t5, &convert_hole);

  // Normal smi, convert to double and store.
  __ mtc1(t5, f0);
  __ cvt_d_w(f0, f0);
  __ sdc1(f0, MemOperand(t3));
  __ Addu(t3, t3, kDoubleSize);

  __ Branch(&entry);

  // Hole found, store the-hole NaN.
  __ bind(&convert_hole);
  if (FLAG_debug_code) {
    // Restore a "smi-untagged" heap object.
    __ SmiTag(t5);
    __ Or(t5, t5, Operand(1));
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Assert(eq, kObjectFoundInSmiOnlyArray, at, Operand(t5));
  }
  __ sw(t0, MemOperand(t3));  // mantissa
  __ sw(t1, MemOperand(t3, kIntSize));  // exponent
  __ Addu(t3, t3, kDoubleSize);

  __ bind(&entry);
  __ Branch(&loop, lt, t3, Operand(t2));

  __ pop(ra);
  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label entry, loop, convert_hole, gc_required, only_change_map;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(a2, t0, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ lw(t0, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ Branch(&only_change_map, eq, at, Operand(t0));

  __ MultiPush(a0.bit() | a1.bit() | a2.bit() | a3.bit() | ra.bit());

  __ lw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: source FixedArray
  // t1: number of elements (smi-tagged)

  // Allocate new FixedArray.
  __ sll(a0, t1, 1);
  __ Addu(a0, a0, FixedDoubleArray::kHeaderSize);
  __ Allocate(a0, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
  // t2: destination FixedArray, not tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(t5, Heap::kFixedArrayMapRootIndex);
  __ sw(t1, MemOperand(t2, FixedDoubleArray::kLengthOffset));
  __ sw(t5, MemOperand(t2, HeapObject::kMapOffset));

  // Prepare for conversion loop.
  __ Addu(t0, t0, Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag + 4));
  __ Addu(a3, t2, Operand(FixedArray::kHeaderSize));
  __ Addu(t2, t2, Operand(kHeapObjectTag));
  __ sll(t1, t1, 1);
  __ Addu(t1, a3, t1);
  __ LoadRoot(t3, Heap::kTheHoleValueRootIndex);
  __ LoadRoot(t5, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses.
  // a3: begin of destination FixedArray element fields, not tagged
  // t0: begin of source FixedDoubleArray element fields, not tagged, +4
  // t1: end of destination FixedArray, not tagged
  // t2: destination FixedArray
  // t3: the-hole pointer
  // t5: heap number map
  __ Branch(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ MultiPop(a0.bit() | a1.bit() | a2.bit() | a3.bit() | ra.bit());

  __ Branch(fail);

  __ bind(&loop);
  __ lw(a1, MemOperand(t0));
  __ Addu(t0, t0, kDoubleSize);
  // a1: current element's upper 32 bit
  // t0: address of next element's upper 32 bit
  __ Branch(&convert_hole, eq, a1, Operand(kHoleNanUpper32));

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(a2, a0, t6, t5, &gc_required);
  // a2: new heap number
  __ lw(a0, MemOperand(t0, -12));
  __ sw(a0, FieldMemOperand(a2, HeapNumber::kMantissaOffset));
  __ sw(a1, FieldMemOperand(a2, HeapNumber::kExponentOffset));
  __ mov(a0, a3);
  __ sw(a2, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);
  __ RecordWrite(t2,
                 a0,
                 a2,
                 kRAHasBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ Branch(&entry);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ sw(t3, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);

  __ bind(&entry);
  __ Branch(&loop, lt, a3, Operand(t1));

  __ MultiPop(a2.bit() | a3.bit() | a0.bit() | a1.bit());
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ sw(t2, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ RecordWriteField(a2,
                      JSObject::kElementsOffset,
                      t2,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(ra);

  __ bind(&only_change_map);
  // Update receiver's map.
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  // Fetch the instance type of the receiver into result register.
  __ lw(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbu(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ And(at, result, Operand(kIsIndirectStringMask));
  __ Branch(&check_sequential, eq, at, Operand(zero_reg));

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ And(at, result, Operand(kSlicedNotConsMask));
  __ Branch(&cons_string, eq, at, Operand(zero_reg));

  // Handle slices.
  Label indirect_string_loaded;
  __ lw(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ lw(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ sra(at, result, kSmiTagSize);
  __ Addu(index, index, at);
  __ jmp(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ lw(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ LoadRoot(at, Heap::kempty_stringRootIndex);
  __ Branch(call_runtime, ne, result, Operand(at));
  // Get the first of the two strings and load its instance type.
  __ lw(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ lw(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbu(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label external_string, check_encoding;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ And(at, result, Operand(kStringRepresentationMask));
  __ Branch(&external_string, ne, at, Operand(zero_reg));

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Addu(string,
          string,
          SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ jmp(&check_encoding);

  // Handle external strings.
  __ bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ And(at, result, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound,
        at, Operand(zero_reg));
  }
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ And(at, result, Operand(kShortExternalStringMask));
  __ Branch(call_runtime, ne, at, Operand(zero_reg));
  __ lw(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label ascii, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ And(at, result, Operand(kStringEncodingMask));
  __ Branch(&ascii, ne, at, Operand(zero_reg));
  // Two-byte string.
  __ sll(at, index, 1);
  __ Addu(at, string, at);
  __ lhu(result, MemOperand(at));
  __ jmp(&done);
  __ bind(&ascii);
  // Ascii string.
  __ Addu(at, string, index);
  __ lbu(result, MemOperand(at));
  __ bind(&done);
}


static MemOperand ExpConstant(int index, Register base) {
  return MemOperand(base, index * kDoubleSize);
}


void MathExpGenerator::EmitMathExp(MacroAssembler* masm,
                                   DoubleRegister input,
                                   DoubleRegister result,
                                   DoubleRegister double_scratch1,
                                   DoubleRegister double_scratch2,
                                   Register temp1,
                                   Register temp2,
                                   Register temp3) {
  ASSERT(!input.is(result));
  ASSERT(!input.is(double_scratch1));
  ASSERT(!input.is(double_scratch2));
  ASSERT(!result.is(double_scratch1));
  ASSERT(!result.is(double_scratch2));
  ASSERT(!double_scratch1.is(double_scratch2));
  ASSERT(!temp1.is(temp2));
  ASSERT(!temp1.is(temp3));
  ASSERT(!temp2.is(temp3));
  ASSERT(ExternalReference::math_exp_constants(0).address() != NULL);

  Label zero, infinity, done;

  __ li(temp3, Operand(ExternalReference::math_exp_constants(0)));

  __ ldc1(double_scratch1, ExpConstant(0, temp3));
  __ BranchF(&zero, NULL, ge, double_scratch1, input);

  __ ldc1(double_scratch2, ExpConstant(1, temp3));
  __ BranchF(&infinity, NULL, ge, input, double_scratch2);

  __ ldc1(double_scratch1, ExpConstant(3, temp3));
  __ ldc1(result, ExpConstant(4, temp3));
  __ mul_d(double_scratch1, double_scratch1, input);
  __ add_d(double_scratch1, double_scratch1, result);
  __ FmoveLow(temp2, double_scratch1);
  __ sub_d(double_scratch1, double_scratch1, result);
  __ ldc1(result, ExpConstant(6, temp3));
  __ ldc1(double_scratch2, ExpConstant(5, temp3));
  __ mul_d(double_scratch1, double_scratch1, double_scratch2);
  __ sub_d(double_scratch1, double_scratch1, input);
  __ sub_d(result, result, double_scratch1);
  __ mul_d(double_scratch2, double_scratch1, double_scratch1);
  __ mul_d(result, result, double_scratch2);
  __ ldc1(double_scratch2, ExpConstant(7, temp3));
  __ mul_d(result, result, double_scratch2);
  __ sub_d(result, result, double_scratch1);
  // Mov 1 in double_scratch2 as math_exp_constants_array[8] == 1.
  ASSERT(*reinterpret_cast<double*>
         (ExternalReference::math_exp_constants(8).address()) == 1);
  __ Move(double_scratch2, 1);
  __ add_d(result, result, double_scratch2);
  __ srl(temp1, temp2, 11);
  __ Ext(temp2, temp2, 0, 11);
  __ Addu(temp1, temp1, Operand(0x3ff));

  // Must not call ExpConstant() after overwriting temp3!
  __ li(temp3, Operand(ExternalReference::math_exp_log_table()));
  __ sll(at, temp2, 3);
  __ Addu(temp3, temp3, Operand(at));
  __ lw(temp2, MemOperand(temp3, 0));
  __ lw(temp3, MemOperand(temp3, kPointerSize));
  // The first word is loaded is the lower number register.
  if (temp2.code() < temp3.code()) {
    __ sll(at, temp1, 20);
    __ Or(temp1, temp3, at);
    __ Move(double_scratch1, temp2, temp1);
  } else {
    __ sll(at, temp1, 20);
    __ Or(temp1, temp2, at);
    __ Move(double_scratch1, temp3, temp1);
  }
  __ mul_d(result, result, double_scratch1);
  __ BranchShort(&done);

  __ bind(&zero);
  __ Move(result, kDoubleRegZero);
  __ BranchShort(&done);

  __ bind(&infinity);
  __ ldc1(result, ExpConstant(2, temp3));

  __ bind(&done);
}

#ifdef DEBUG
// nop(CODE_AGE_MARKER_NOP)
static const uint32_t kCodeAgePatchFirstInstruction = 0x00010180;
#endif

static byte* GetNoCodeAgeSequence(uint32_t* length) {
  // The sequence of instructions that is patched out for aging code is the
  // following boilerplate stack-building prologue that is found in FUNCTIONS
  static bool initialized = false;
  static uint32_t sequence[kNoCodeAgeSequenceLength];
  byte* byte_sequence = reinterpret_cast<byte*>(sequence);
  *length = kNoCodeAgeSequenceLength * Assembler::kInstrSize;
  if (!initialized) {
    // Since patcher is a large object, allocate it dynamically when needed,
    // to avoid overloading the stack in stress conditions.
    SmartPointer<CodePatcher>
        patcher(new CodePatcher(byte_sequence, kNoCodeAgeSequenceLength));
    PredictableCodeSizeScope scope(patcher->masm(), *length);
    patcher->masm()->Push(ra, fp, cp, a1);
    patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
    patcher->masm()->Addu(
        fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
    initialized = true;
  }
  return byte_sequence;
}


bool Code::IsYoungSequence(byte* sequence) {
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  bool result = !memcmp(sequence, young_sequence, young_length);
  ASSERT(result ||
         Memory::uint32_at(sequence) == kCodeAgePatchFirstInstruction);
  return result;
}


void Code::GetCodeAgeAndParity(byte* sequence, Age* age,
                               MarkingParity* parity) {
  if (IsYoungSequence(sequence)) {
    *age = kNoAgeCodeAge;
    *parity = NO_MARKING_PARITY;
  } else {
    Address target_address = Assembler::target_address_at(
        sequence + Assembler::kInstrSize);
    Code* stub = GetCodeFromTargetAddress(target_address);
    GetCodeAgeAndParity(stub, age, parity);
  }
}


void Code::PatchPlatformCodeAge(Isolate* isolate,
                                byte* sequence,
                                Code::Age age,
                                MarkingParity parity) {
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  if (age == kNoAgeCodeAge) {
    CopyBytes(sequence, young_sequence, young_length);
    CPU::FlushICache(sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(sequence, young_length / Assembler::kInstrSize);
    // Mark this code sequence for FindPlatformCodeAgeSequence().
    patcher.masm()->nop(Assembler::CODE_AGE_MARKER_NOP);
    // Load the stub address to t9 and call it,
    // GetCodeAgeAndParity() extracts the stub address from this instruction.
    patcher.masm()->li(
        t9,
        Operand(reinterpret_cast<uint32_t>(stub->instruction_start())),
        CONSTANT_SIZE);
    patcher.masm()->nop();  // Prevent jalr to jal optimization.
    patcher.masm()->jalr(t9, a0);
    patcher.masm()->nop();  // Branch delay slot nop.
    patcher.masm()->nop();  // Pad the empty space.
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS

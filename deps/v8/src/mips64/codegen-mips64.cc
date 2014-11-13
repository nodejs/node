// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/mips64/simulator-mips64.h"

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
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return &std::exp;
  ExternalReference::InitializeMathExpData();

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  {
    DoubleRegister input = f12;
    DoubleRegister result = f0;
    DoubleRegister double_scratch1 = f4;
    DoubleRegister double_scratch2 = f6;
    Register temp1 = a4;
    Register temp2 = a5;
    Register temp3 = a6;

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
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  CpuFeatures::FlushICache(buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);

#if !defined(USE_SIMULATOR)
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
#else
  fast_exp_mips_machine_code = buffer;
  return &fast_exp_simulator;
#endif
}


#if defined(V8_HOST_ARCH_MIPS)
MemCopyUint8Function CreateMemCopyUint8Function(MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else

  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(3 * KB, &actual_size, true));
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
    DCHECK(pref_chunk < max_pref_size);

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
    DCHECK(pref_hint_store != kPrefHintPrepareForStore ||
           pref_chunk * 4 >= max_pref_size);
    // If the size is less than 8, go to lastb. Regardless of size,
    // copy dst pointer to v0 for the retuen value.
    __ slti(a6, a2, 2 * loadstore_chunk);
    __ bne(a6, zero_reg, &lastb);
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
    // in this case the a0+x should be past the "a4-32" address. This means:
    // for x=128 the last "safe" a0 address is "a4-160". Alternatively, for
    // x=64 the last "safe" a0 address is "a4-96". In the current version we
    // will use "pref hint, 128(a0)", so "a4-160" is the limit.
    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ addu(a4, a0, a2);  // a4 is the "past the end" address.
      __ Subu(t9, a4, pref_limit);  // t9 is the "last safe pref" address.
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
    __ lw(a4, MemOperand(a1));

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ sltu(v1, t9, a0);  // If a0 > t9, don't use next prefetch.
      __ Branch(USE_DELAY_SLOT, &skip_pref, gt, v1, Operand(zero_reg));
    }
    __ lw(a5, MemOperand(a1, 1, loadstore_chunk));  // Maybe in delay slot.

    __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
    __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

    __ bind(&skip_pref);
    __ lw(a6, MemOperand(a1, 2, loadstore_chunk));
    __ lw(a7, MemOperand(a1, 3, loadstore_chunk));
    __ lw(t0, MemOperand(a1, 4, loadstore_chunk));
    __ lw(t1, MemOperand(a1, 5, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 6, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 7, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));

    __ sw(a4, MemOperand(a0));
    __ sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 7, loadstore_chunk));

    __ lw(a4, MemOperand(a1, 8, loadstore_chunk));
    __ lw(a5, MemOperand(a1, 9, loadstore_chunk));
    __ lw(a6, MemOperand(a1, 10, loadstore_chunk));
    __ lw(a7, MemOperand(a1, 11, loadstore_chunk));
    __ lw(t0, MemOperand(a1, 12, loadstore_chunk));
    __ lw(t1, MemOperand(a1, 13, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 14, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 15, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));

    __ sw(a4, MemOperand(a0, 8, loadstore_chunk));
    __ sw(a5, MemOperand(a0, 9, loadstore_chunk));
    __ sw(a6, MemOperand(a0, 10, loadstore_chunk));
    __ sw(a7, MemOperand(a0, 11, loadstore_chunk));
    __ sw(t0, MemOperand(a0, 12, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 13, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 14, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 15, loadstore_chunk));
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
    __ lw(a4, MemOperand(a1));
    __ lw(a5, MemOperand(a1, 1, loadstore_chunk));
    __ lw(a6, MemOperand(a1, 2, loadstore_chunk));
    __ lw(a7, MemOperand(a1, 3, loadstore_chunk));
    __ lw(t0, MemOperand(a1, 4, loadstore_chunk));
    __ lw(t1, MemOperand(a1, 5, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 6, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 7, loadstore_chunk));
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ sw(a4, MemOperand(a0));
    __ sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 7, loadstore_chunk));
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
    __ lw(a7, MemOperand(a1));
    __ addiu(a0, a0, loadstore_chunk);
    __ addiu(a1, a1, loadstore_chunk);
    __ bne(a0, a3, &wordCopy_loop);
    __ sw(a7, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

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
      __ addu(a4, a0, a2);
      __ Subu(t9, a4, pref_limit);
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
    __ lwr(a4, MemOperand(a1));
    __ lwr(a5, MemOperand(a1, 1, loadstore_chunk));
    __ lwr(a6, MemOperand(a1, 2, loadstore_chunk));

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ sltu(v1, t9, a0);
      __ Branch(USE_DELAY_SLOT, &ua_skip_pref, gt, v1, Operand(zero_reg));
    }
    __ lwr(a7, MemOperand(a1, 3, loadstore_chunk));  // Maybe in delay slot.

    __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
    __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

    __ bind(&ua_skip_pref);
    __ lwr(t0, MemOperand(a1, 4, loadstore_chunk));
    __ lwr(t1, MemOperand(a1, 5, loadstore_chunk));
    __ lwr(t2, MemOperand(a1, 6, loadstore_chunk));
    __ lwr(t3, MemOperand(a1, 7, loadstore_chunk));
    __ lwl(a4,
           MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a5,
           MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a6,
           MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a7,
           MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t0,
           MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t1,
           MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t2,
           MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t3,
           MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));
    __ sw(a4, MemOperand(a0));
    __ sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 7, loadstore_chunk));
    __ lwr(a4, MemOperand(a1, 8, loadstore_chunk));
    __ lwr(a5, MemOperand(a1, 9, loadstore_chunk));
    __ lwr(a6, MemOperand(a1, 10, loadstore_chunk));
    __ lwr(a7, MemOperand(a1, 11, loadstore_chunk));
    __ lwr(t0, MemOperand(a1, 12, loadstore_chunk));
    __ lwr(t1, MemOperand(a1, 13, loadstore_chunk));
    __ lwr(t2, MemOperand(a1, 14, loadstore_chunk));
    __ lwr(t3, MemOperand(a1, 15, loadstore_chunk));
    __ lwl(a4,
           MemOperand(a1, 9, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a5,
           MemOperand(a1, 10, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a6,
           MemOperand(a1, 11, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a7,
           MemOperand(a1, 12, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t0,
           MemOperand(a1, 13, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t1,
           MemOperand(a1, 14, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t2,
           MemOperand(a1, 15, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t3,
           MemOperand(a1, 16, loadstore_chunk, MemOperand::offset_minus_one));
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));
    __ sw(a4, MemOperand(a0, 8, loadstore_chunk));
    __ sw(a5, MemOperand(a0, 9, loadstore_chunk));
    __ sw(a6, MemOperand(a0, 10, loadstore_chunk));
    __ sw(a7, MemOperand(a0, 11, loadstore_chunk));
    __ sw(t0, MemOperand(a0, 12, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 13, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 14, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 15, loadstore_chunk));
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
    __ lwr(a4, MemOperand(a1));
    __ lwr(a5, MemOperand(a1, 1, loadstore_chunk));
    __ lwr(a6, MemOperand(a1, 2, loadstore_chunk));
    __ lwr(a7, MemOperand(a1, 3, loadstore_chunk));
    __ lwr(t0, MemOperand(a1, 4, loadstore_chunk));
    __ lwr(t1, MemOperand(a1, 5, loadstore_chunk));
    __ lwr(t2, MemOperand(a1, 6, loadstore_chunk));
    __ lwr(t3, MemOperand(a1, 7, loadstore_chunk));
    __ lwl(a4,
           MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a5,
           MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a6,
           MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(a7,
           MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t0,
           MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t1,
           MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t2,
           MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
    __ lwl(t3,
           MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ sw(a4, MemOperand(a0));
    __ sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 7, loadstore_chunk));
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
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  CpuFeatures::FlushICache(buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<MemCopyUint8Function>(buffer);
#endif
}
#endif

UnaryMathFunction CreateSqrtFunction() {
#if defined(USE_SIMULATOR)
  return &std::sqrt;
#else
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return &std::sqrt;

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  __ MovFromFloatParameter(f12);
  __ sqrt_d(f0, f12);
  __ MovToFloatResult(f0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  CpuFeatures::FlushICache(buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
#endif
}

#undef __


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  DCHECK(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  DCHECK(masm->has_frame());
  masm->set_has_frame(false);
}


// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* allocation_memento_found) {
  Register scratch_elements = a4;
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     scratch_elements));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(
        receiver, scratch_elements, allocation_memento_found);
  }

  // Set transitioned map.
  __ sd(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      t1,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* fail) {
  // Register ra contains the return address.
  Label loop, entry, convert_hole, gc_required, only_change_map, done;
  Register elements = a4;
  Register length = a5;
  Register array = a6;
  Register array_end = array;

  // target_map parameter can be clobbered.
  Register scratch1 = target_map;
  Register scratch2 = t1;
  Register scratch3 = a7;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     elements, length, array, scratch2));

  Register scratch = t2;
  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, elements, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ld(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ Branch(&only_change_map, eq, at, Operand(elements));

  __ push(ra);
  __ ld(length, FieldMemOperand(elements, FixedArray::kLengthOffset));
  // elements: source FixedArray
  // length: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  __ SmiScale(scratch, length, kDoubleSizeLog2);
  __ Daddu(scratch, scratch, FixedDoubleArray::kHeaderSize);
  __ Allocate(scratch, array, t3, scratch2, &gc_required, DOUBLE_ALIGNMENT);
  // array: destination FixedDoubleArray, not tagged as heap object

  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(scratch2, Heap::kFixedDoubleArrayMapRootIndex);
  __ sd(length, MemOperand(array, FixedDoubleArray::kLengthOffset));
  // Update receiver's map.
  __ sd(scratch2, MemOperand(array, HeapObject::kMapOffset));

  __ sd(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch2,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ Daddu(scratch1, array, Operand(kHeapObjectTag));
  __ sd(scratch1, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ RecordWriteField(receiver,
                      JSObject::kElementsOffset,
                      scratch1,
                      scratch2,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);


  // Prepare for conversion loop.
  __ Daddu(scratch1, elements,
      Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Daddu(scratch3, array, Operand(FixedDoubleArray::kHeaderSize));
  __ SmiScale(array_end, length, kDoubleSizeLog2);
  __ Daddu(array_end, array_end, scratch3);

  // Repurpose registers no longer in use.
  Register hole_lower = elements;
  Register hole_upper = length;
  __ li(hole_lower, Operand(kHoleNanLower32));
  // scratch1: begin of source FixedArray element fields, not tagged
  // hole_lower: kHoleNanLower32
  // hole_upper: kHoleNanUpper32
  // array_end: end of destination FixedDoubleArray, not tagged
  // scratch3: begin of FixedDoubleArray element fields, not tagged
  __ Branch(USE_DELAY_SLOT, &entry);
  __ li(hole_upper, Operand(kHoleNanUpper32));  // In delay slot.

  __ bind(&only_change_map);
  __ sd(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch2,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Branch(&done);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ ld(ra, MemOperand(sp, 0));
  __ Branch(USE_DELAY_SLOT, fail);
  __ daddiu(sp, sp, kPointerSize);  // In delay slot.

  // Convert and copy elements.
  __ bind(&loop);
  __ ld(scratch2, MemOperand(scratch1));
  __ Daddu(scratch1, scratch1, kIntSize);
  // scratch2: current element
  __ JumpIfNotSmi(scratch2, &convert_hole);
  __ SmiUntag(scratch2);

  // Normal smi, convert to double and store.
  __ mtc1(scratch2, f0);
  __ cvt_d_w(f0, f0);
  __ sdc1(f0, MemOperand(scratch3));
  __ Branch(USE_DELAY_SLOT, &entry);
  __ daddiu(scratch3, scratch3, kDoubleSize);  // In delay slot.

  // Hole found, store the-hole NaN.
  __ bind(&convert_hole);
  if (FLAG_debug_code) {
    // Restore a "smi-untagged" heap object.
    __ Or(scratch2, scratch2, Operand(1));
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Assert(eq, kObjectFoundInSmiOnlyArray, at, Operand(scratch2));
  }
  // mantissa
  __ sw(hole_lower, MemOperand(scratch3));
  // exponent
  __ sw(hole_upper, MemOperand(scratch3, kIntSize));
  __ Daddu(scratch3, scratch3, kDoubleSize);

  __ bind(&entry);
  __ Branch(&loop, lt, scratch3, Operand(array_end));

  __ bind(&done);
  __ pop(ra);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* fail) {
  // Register ra contains the return address.
  Label entry, loop, convert_hole, gc_required, only_change_map;
  Register elements = a4;
  Register array = a6;
  Register length = a5;
  Register scratch = t1;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     elements, array, length, scratch));
  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, elements, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ld(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ Branch(&only_change_map, eq, at, Operand(elements));

  __ MultiPush(
      value.bit() | key.bit() | receiver.bit() | target_map.bit() | ra.bit());

  __ ld(length, FieldMemOperand(elements, FixedArray::kLengthOffset));
  // elements: source FixedArray
  // length: number of elements (smi-tagged)

  // Allocate new FixedArray.
  // Re-use value and target_map registers, as they have been saved on the
  // stack.
  Register array_size = value;
  Register allocate_scratch = target_map;
  __ SmiScale(array_size, length, kPointerSizeLog2);
  __ Daddu(array_size, array_size, FixedDoubleArray::kHeaderSize);
  __ Allocate(array_size, array, allocate_scratch, scratch, &gc_required,
              NO_ALLOCATION_FLAGS);
  // array: destination FixedArray, not tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(scratch, Heap::kFixedArrayMapRootIndex);
  __ sd(length, MemOperand(array, FixedDoubleArray::kLengthOffset));
  __ sd(scratch, MemOperand(array, HeapObject::kMapOffset));

  // Prepare for conversion loop.
  Register src_elements = elements;
  Register dst_elements = target_map;
  Register dst_end = length;
  Register heap_number_map = scratch;
  __ Daddu(src_elements, src_elements,
      Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag + 4));
  __ Daddu(dst_elements, array, Operand(FixedArray::kHeaderSize));
  __ SmiScale(dst_end, dst_end, kPointerSizeLog2);
  __ Daddu(dst_end, dst_elements, dst_end);

  // Allocating heap numbers in the loop below can fail and cause a jump to
  // gc_required. We can't leave a partly initialized FixedArray behind,
  // so pessimistically fill it with holes now.
  Label initialization_loop, initialization_loop_entry;
  __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
  __ Branch(&initialization_loop_entry);
  __ bind(&initialization_loop);
  __ sd(scratch, MemOperand(dst_elements));
  __ Daddu(dst_elements, dst_elements, Operand(kPointerSize));
  __ bind(&initialization_loop_entry);
  __ Branch(&initialization_loop, lt, dst_elements, Operand(dst_end));

  __ Daddu(dst_elements, array, Operand(FixedArray::kHeaderSize));
  __ Daddu(array, array, Operand(kHeapObjectTag));
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses.
  // dst_elements: begin of destination FixedArray element fields, not tagged
  // src_elements: begin of source FixedDoubleArray element fields, not tagged,
  //               points to the exponent
  // dst_end: end of destination FixedArray, not tagged
  // array: destination FixedArray
  // heap_number_map: heap number map
  __ Branch(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ MultiPop(
      value.bit() | key.bit() | receiver.bit() | target_map.bit() | ra.bit());

  __ Branch(fail);

  __ bind(&loop);
  Register upper_bits = key;
  __ lw(upper_bits, MemOperand(src_elements));
  __ Daddu(src_elements, src_elements, kDoubleSize);
  // upper_bits: current element's upper 32 bit
  // src_elements: address of next element's upper 32 bit
  __ Branch(&convert_hole, eq, a1, Operand(kHoleNanUpper32));

  // Non-hole double, copy value into a heap number.
  Register heap_number = receiver;
  Register scratch2 = value;
  Register scratch3 = t2;
  __ AllocateHeapNumber(heap_number, scratch2, scratch3, heap_number_map,
                        &gc_required);
  // heap_number: new heap number
  // Load mantissa of current element, src_elements
  // point to exponent of next element.
  __ lw(scratch2, MemOperand(heap_number, -12));
  __ sw(scratch2, FieldMemOperand(heap_number, HeapNumber::kMantissaOffset));
  __ sw(upper_bits, FieldMemOperand(heap_number, HeapNumber::kExponentOffset));
  __ mov(scratch2, dst_elements);
  __ sd(heap_number, MemOperand(dst_elements));
  __ Daddu(dst_elements, dst_elements, kPointerSize);
  __ RecordWrite(array,
                 scratch2,
                 heap_number,
                 kRAHasBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ Branch(&entry);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ LoadRoot(scratch2, Heap::kTheHoleValueRootIndex);
  __ sd(scratch2, MemOperand(dst_elements));
  __ Daddu(dst_elements, dst_elements, kPointerSize);

  __ bind(&entry);
  __ Branch(&loop, lt, dst_elements, Operand(dst_end));

  __ MultiPop(receiver.bit() | target_map.bit() | value.bit() | key.bit());
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ sd(array, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ RecordWriteField(receiver,
                      JSObject::kElementsOffset,
                      array,
                      scratch,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(ra);

  __ bind(&only_change_map);
  // Update receiver's map.
  __ sd(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch,
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
  __ ld(result, FieldMemOperand(string, HeapObject::kMapOffset));
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
  __ ld(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ ld(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ dsra32(at, result, 0);
  __ Daddu(index, index, at);
  __ jmp(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ ld(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ LoadRoot(at, Heap::kempty_stringRootIndex);
  __ Branch(call_runtime, ne, result, Operand(at));
  // Get the first of the two strings and load its instance type.
  __ ld(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ ld(result, FieldMemOperand(string, HeapObject::kMapOffset));
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
  __ Daddu(string,
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
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ And(at, result, Operand(kShortExternalStringMask));
  __ Branch(call_runtime, ne, at, Operand(zero_reg));
  __ ld(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ And(at, result, Operand(kStringEncodingMask));
  __ Branch(&one_byte, ne, at, Operand(zero_reg));
  // Two-byte string.
  __ dsll(at, index, 1);
  __ Daddu(at, string, at);
  __ lhu(result, MemOperand(at));
  __ jmp(&done);
  __ bind(&one_byte);
  // One_byte string.
  __ Daddu(at, string, index);
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
  DCHECK(!input.is(result));
  DCHECK(!input.is(double_scratch1));
  DCHECK(!input.is(double_scratch2));
  DCHECK(!result.is(double_scratch1));
  DCHECK(!result.is(double_scratch2));
  DCHECK(!double_scratch1.is(double_scratch2));
  DCHECK(!temp1.is(temp2));
  DCHECK(!temp1.is(temp3));
  DCHECK(!temp2.is(temp3));
  DCHECK(ExternalReference::math_exp_constants(0).address() != NULL);
  DCHECK(!masm->serializer_enabled());  // External references not serializable.

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
  DCHECK(*reinterpret_cast<double*>
         (ExternalReference::math_exp_constants(8).address()) == 1);
  __ Move(double_scratch2, 1);
  __ add_d(result, result, double_scratch2);
  __ dsrl(temp1, temp2, 11);
  __ Ext(temp2, temp2, 0, 11);
  __ Daddu(temp1, temp1, Operand(0x3ff));

  // Must not call ExpConstant() after overwriting temp3!
  __ li(temp3, Operand(ExternalReference::math_exp_log_table()));
  __ dsll(at, temp2, 3);
  __ Daddu(temp3, temp3, Operand(at));
  __ lwu(temp2, MemOperand(temp3, 0));
  __ lwu(temp3, MemOperand(temp3, kIntSize));
  // The first word is loaded is the lower number register.
  if (temp2.code() < temp3.code()) {
    __ dsll(at, temp1, 20);
    __ Or(temp1, temp3, at);
    __ Move(double_scratch1, temp2, temp1);
  } else {
    __ dsll(at, temp1, 20);
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


CodeAgingHelper::CodeAgingHelper() {
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // Since patcher is a large object, allocate it dynamically when needed,
  // to avoid overloading the stack in stress conditions.
  // DONT_FLUSH is used because the CodeAgingHelper is initialized early in
  // the process, before MIPS simulator ICache is setup.
  SmartPointer<CodePatcher> patcher(
      new CodePatcher(young_sequence_.start(),
                      young_sequence_.length() / Assembler::kInstrSize,
                      CodePatcher::DONT_FLUSH));
  PredictableCodeSizeScope scope(patcher->masm(), young_sequence_.length());
  patcher->masm()->Push(ra, fp, cp, a1);
  patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
  patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
  patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
  patcher->masm()->Daddu(
      fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
}


#ifdef DEBUG
bool CodeAgingHelper::IsOld(byte* candidate) const {
  return Memory::uint32_at(candidate) == kCodeAgePatchFirstInstruction;
}
#endif


bool Code::IsYoungSequence(Isolate* isolate, byte* sequence) {
  bool result = isolate->code_aging_helper()->IsYoung(sequence);
  DCHECK(result || isolate->code_aging_helper()->IsOld(sequence));
  return result;
}


void Code::GetCodeAgeAndParity(Isolate* isolate, byte* sequence, Age* age,
                               MarkingParity* parity) {
  if (IsYoungSequence(isolate, sequence)) {
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
  uint32_t young_length = isolate->code_aging_helper()->young_sequence_length();
  if (age == kNoAgeCodeAge) {
    isolate->code_aging_helper()->CopyYoungSequenceTo(sequence);
    CpuFeatures::FlushICache(sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(sequence, young_length / Assembler::kInstrSize);
    // Mark this code sequence for FindPlatformCodeAgeSequence().
    patcher.masm()->nop(Assembler::CODE_AGE_MARKER_NOP);
    // Load the stub address to t9 and call it,
    // GetCodeAgeAndParity() extracts the stub address from this instruction.
    patcher.masm()->li(
        t9,
        Operand(reinterpret_cast<uint64_t>(stub->instruction_start())),
        ADDRESS_LOAD);
    patcher.masm()->nop();  // Prevent jalr to jal optimization.
    patcher.masm()->jalr(t9, a0);
    patcher.masm()->nop();  // Branch delay slot nop.
    patcher.masm()->nop();  // Pad the empty space.
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS64

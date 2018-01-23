// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

#include <memory>

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/mips/simulator-mips.h"

namespace v8 {
namespace internal {

#define __ masm.

#if defined(V8_HOST_ARCH_MIPS)

MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR) || defined(_MIPS_ARCH_MIPS32R6) || \
    defined(_MIPS_ARCH_MIPS32RX)
  return stub;
#else
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);

  // This code assumes that cache lines are 32 bytes and if the cache line is
  // larger it will not work correctly.
  {
    Label lastb, unaligned, aligned, chkw,
          loop16w, chk1w, wordCopy_loop, skip_pref, lastbloop,
          leave, ua_chk16w, ua_loop16w, ua_skip_pref, ua_chkw,
          ua_chk1w, ua_wordCopy_loop, ua_smallCopy, ua_smallCopy_loop;

    // The size of each prefetch.
    uint32_t pref_chunk = 32;
    // The maximum size of a prefetch, it must not be less than pref_chunk.
    // If the real size of a prefetch is greater than max_pref_size and
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

    if (kArchEndian == kLittle) {
      __ lwr(t8, MemOperand(a1));
      __ addu(a1, a1, a3);
      __ swr(t8, MemOperand(a0));
      __ addu(a0, a0, a3);
    } else {
      __ lwl(t8, MemOperand(a1));
      __ addu(a1, a1, a3);
      __ swl(t8, MemOperand(a0));
      __ addu(a0, a0, a3);
    }
    // Now dst/src are both aligned to (word) aligned addresses. Set a2 to
    // count how many bytes we have to copy after all the 64 byte chunks are
    // copied and a3 to the dst pointer after all the 64 byte chunks have been
    // copied. We will loop, incrementing a0 and a1 until a0 equals a3.
    __ bind(&aligned);
    __ andi(t8, a2, 0x3F);
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
    __ andi(t8, a2, 0x1F);
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
    // and a1 until a0 equals a3.
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

    if (kArchEndian == kLittle) {
      __ lwr(v1, MemOperand(a1));
      __ lwl(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ addu(a1, a1, a3);
      __ swr(v1, MemOperand(a0));
      __ addu(a0, a0, a3);
    } else {
      __ lwl(v1, MemOperand(a1));
      __ lwr(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ addu(a1, a1, a3);
      __ swl(v1, MemOperand(a0));
      __ addu(a0, a0, a3);
    }

    // Now the dst (but not the source) is aligned. Set a2 to count how many
    // bytes we have to copy after all the 64 byte chunks are copied and a3 to
    // the dst pointer after all the 64 byte chunks have been copied. We will
    // loop, incrementing a0 and a1 until a0 equals a3.
    __ bind(&ua_chk16w);
    __ andi(t8, a2, 0x3F);
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
    if (kArchEndian == kLittle) {
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
    } else {
      __ lwl(t0, MemOperand(a1));
      __ lwl(t1, MemOperand(a1, 1, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 2, loadstore_chunk));

      if (pref_hint_store == kPrefHintPrepareForStore) {
        __ sltu(v1, t9, a0);
        __ Branch(USE_DELAY_SLOT, &ua_skip_pref, gt, v1, Operand(zero_reg));
      }
      __ lwl(t3, MemOperand(a1, 3, loadstore_chunk));  // Maybe in delay slot.

      __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

      __ bind(&ua_skip_pref);
      __ lwl(t4, MemOperand(a1, 4, loadstore_chunk));
      __ lwl(t5, MemOperand(a1, 5, loadstore_chunk));
      __ lwl(t6, MemOperand(a1, 6, loadstore_chunk));
      __ lwl(t7, MemOperand(a1, 7, loadstore_chunk));
      __ lwr(t0,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t4,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t5,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t6,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t7,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    if (kArchEndian == kLittle) {
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
    } else {
      __ lwl(t0, MemOperand(a1, 8, loadstore_chunk));
      __ lwl(t1, MemOperand(a1, 9, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 10, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 11, loadstore_chunk));
      __ lwl(t4, MemOperand(a1, 12, loadstore_chunk));
      __ lwl(t5, MemOperand(a1, 13, loadstore_chunk));
      __ lwl(t6, MemOperand(a1, 14, loadstore_chunk));
      __ lwl(t7, MemOperand(a1, 15, loadstore_chunk));
      __ lwr(t0,
             MemOperand(a1, 9, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 10, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 11, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 12, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t4,
             MemOperand(a1, 13, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t5,
             MemOperand(a1, 14, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t6,
             MemOperand(a1, 15, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t7,
             MemOperand(a1, 16, loadstore_chunk, MemOperand::offset_minus_one));
    }
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
    __ andi(t8, a2, 0x1F);

    __ beq(a2, t8, &ua_chk1w);
    __ nop();  // In delay slot.
    if (kArchEndian == kLittle) {
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
    } else {
      __ lwl(t0, MemOperand(a1));
      __ lwl(t1, MemOperand(a1, 1, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 2, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 3, loadstore_chunk));
      __ lwl(t4, MemOperand(a1, 4, loadstore_chunk));
      __ lwl(t5, MemOperand(a1, 5, loadstore_chunk));
      __ lwl(t6, MemOperand(a1, 6, loadstore_chunk));
      __ lwl(t7, MemOperand(a1, 7, loadstore_chunk));
      __ lwr(t0,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t4,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t5,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t6,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t7,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    }
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
    if (kArchEndian == kLittle) {
      __ lwr(v1, MemOperand(a1));
      __ lwl(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    } else {
      __ lwl(v1, MemOperand(a1));
      __ lwr(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    }
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
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  return FUNCTION_CAST<MemCopyUint8Function>(buffer);
#endif
}
#endif

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);

  __ MovFromFloatParameter(f12);
  __ sqrt_d(f0, f12);
  __ MovToFloatResult(f0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#endif
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS

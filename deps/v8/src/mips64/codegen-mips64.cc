// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/mips64/codegen-mips64.h"

#if V8_TARGET_ARCH_MIPS64

#include <memory>

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/mips64/simulator-mips64.h"

namespace v8 {
namespace internal {


#define __ masm.


#if defined(V8_HOST_ARCH_MIPS)
MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else

  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(3 * KB, &actual_size, true));
  if (buffer == nullptr) return stub;

  // This code assumes that cache lines are 32 bytes and if the cache line is
  // larger it will not work correctly.
  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

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
    __ Lw(a4, MemOperand(a1));

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ sltu(v1, t9, a0);  // If a0 > t9, don't use next prefetch.
      __ Branch(USE_DELAY_SLOT, &skip_pref, gt, v1, Operand(zero_reg));
    }
    __ Lw(a5, MemOperand(a1, 1, loadstore_chunk));  // Maybe in delay slot.

    __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
    __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

    __ bind(&skip_pref);
    __ Lw(a6, MemOperand(a1, 2, loadstore_chunk));
    __ Lw(a7, MemOperand(a1, 3, loadstore_chunk));
    __ Lw(t0, MemOperand(a1, 4, loadstore_chunk));
    __ Lw(t1, MemOperand(a1, 5, loadstore_chunk));
    __ Lw(t2, MemOperand(a1, 6, loadstore_chunk));
    __ Lw(t3, MemOperand(a1, 7, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));

    __ Sw(a4, MemOperand(a0));
    __ Sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ Sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ Sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ Sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ Sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ Sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ Sw(t3, MemOperand(a0, 7, loadstore_chunk));

    __ Lw(a4, MemOperand(a1, 8, loadstore_chunk));
    __ Lw(a5, MemOperand(a1, 9, loadstore_chunk));
    __ Lw(a6, MemOperand(a1, 10, loadstore_chunk));
    __ Lw(a7, MemOperand(a1, 11, loadstore_chunk));
    __ Lw(t0, MemOperand(a1, 12, loadstore_chunk));
    __ Lw(t1, MemOperand(a1, 13, loadstore_chunk));
    __ Lw(t2, MemOperand(a1, 14, loadstore_chunk));
    __ Lw(t3, MemOperand(a1, 15, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));

    __ Sw(a4, MemOperand(a0, 8, loadstore_chunk));
    __ Sw(a5, MemOperand(a0, 9, loadstore_chunk));
    __ Sw(a6, MemOperand(a0, 10, loadstore_chunk));
    __ Sw(a7, MemOperand(a0, 11, loadstore_chunk));
    __ Sw(t0, MemOperand(a0, 12, loadstore_chunk));
    __ Sw(t1, MemOperand(a0, 13, loadstore_chunk));
    __ Sw(t2, MemOperand(a0, 14, loadstore_chunk));
    __ Sw(t3, MemOperand(a0, 15, loadstore_chunk));
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
    __ Lw(a4, MemOperand(a1));
    __ Lw(a5, MemOperand(a1, 1, loadstore_chunk));
    __ Lw(a6, MemOperand(a1, 2, loadstore_chunk));
    __ Lw(a7, MemOperand(a1, 3, loadstore_chunk));
    __ Lw(t0, MemOperand(a1, 4, loadstore_chunk));
    __ Lw(t1, MemOperand(a1, 5, loadstore_chunk));
    __ Lw(t2, MemOperand(a1, 6, loadstore_chunk));
    __ Lw(t3, MemOperand(a1, 7, loadstore_chunk));
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ Sw(a4, MemOperand(a0));
    __ Sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ Sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ Sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ Sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ Sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ Sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ Sw(t3, MemOperand(a0, 7, loadstore_chunk));
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
    __ Lw(a7, MemOperand(a1));
    __ addiu(a0, a0, loadstore_chunk);
    __ addiu(a1, a1, loadstore_chunk);
    __ bne(a0, a3, &wordCopy_loop);
    __ Sw(a7, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

    __ bind(&lastb);
    __ Branch(&leave, le, a2, Operand(zero_reg));
    __ addu(a3, a0, a2);

    __ bind(&lastbloop);
    __ Lb(v1, MemOperand(a1));
    __ addiu(a0, a0, 1);
    __ addiu(a1, a1, 1);
    __ bne(a0, a3, &lastbloop);
    __ Sb(v1, MemOperand(a0, -1));  // In delay slot.

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
    if (kArchEndian == kLittle) {
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
    } else {
      __ Pref(pref_hint_load, MemOperand(a1, 3 * pref_chunk));
      __ lwl(a4, MemOperand(a1));
      __ lwl(a5, MemOperand(a1, 1, loadstore_chunk));
      __ lwl(a6, MemOperand(a1, 2, loadstore_chunk));

      if (pref_hint_store == kPrefHintPrepareForStore) {
        __ sltu(v1, t9, a0);
        __ Branch(USE_DELAY_SLOT, &ua_skip_pref, gt, v1, Operand(zero_reg));
      }
      __ lwl(a7, MemOperand(a1, 3, loadstore_chunk));  // Maybe in delay slot.

      __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

      __ bind(&ua_skip_pref);
      __ lwl(t0, MemOperand(a1, 4, loadstore_chunk));
      __ lwl(t1, MemOperand(a1, 5, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 6, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 7, loadstore_chunk));
      __ lwr(a4,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a5,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a6,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a7,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t0,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));
    __ Sw(a4, MemOperand(a0));
    __ Sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ Sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ Sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ Sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ Sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ Sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ Sw(t3, MemOperand(a0, 7, loadstore_chunk));
    if (kArchEndian == kLittle) {
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
    } else {
      __ lwl(a4, MemOperand(a1, 8, loadstore_chunk));
      __ lwl(a5, MemOperand(a1, 9, loadstore_chunk));
      __ lwl(a6, MemOperand(a1, 10, loadstore_chunk));
      __ lwl(a7, MemOperand(a1, 11, loadstore_chunk));
      __ lwl(t0, MemOperand(a1, 12, loadstore_chunk));
      __ lwl(t1, MemOperand(a1, 13, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 14, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 15, loadstore_chunk));
      __ lwr(a4,
             MemOperand(a1, 9, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a5,
             MemOperand(a1, 10, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a6,
             MemOperand(a1, 11, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a7,
             MemOperand(a1, 12, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t0,
             MemOperand(a1, 13, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 14, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 15, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 16, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));
    __ Sw(a4, MemOperand(a0, 8, loadstore_chunk));
    __ Sw(a5, MemOperand(a0, 9, loadstore_chunk));
    __ Sw(a6, MemOperand(a0, 10, loadstore_chunk));
    __ Sw(a7, MemOperand(a0, 11, loadstore_chunk));
    __ Sw(t0, MemOperand(a0, 12, loadstore_chunk));
    __ Sw(t1, MemOperand(a0, 13, loadstore_chunk));
    __ Sw(t2, MemOperand(a0, 14, loadstore_chunk));
    __ Sw(t3, MemOperand(a0, 15, loadstore_chunk));
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
    if (kArchEndian == kLittle) {
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
    } else {
      __ lwl(a4, MemOperand(a1));
      __ lwl(a5, MemOperand(a1, 1, loadstore_chunk));
      __ lwl(a6, MemOperand(a1, 2, loadstore_chunk));
      __ lwl(a7, MemOperand(a1, 3, loadstore_chunk));
      __ lwl(t0, MemOperand(a1, 4, loadstore_chunk));
      __ lwl(t1, MemOperand(a1, 5, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 6, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 7, loadstore_chunk));
      __ lwr(a4,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a5,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a6,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(a7,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t0,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ Sw(a4, MemOperand(a0));
    __ Sw(a5, MemOperand(a0, 1, loadstore_chunk));
    __ Sw(a6, MemOperand(a0, 2, loadstore_chunk));
    __ Sw(a7, MemOperand(a0, 3, loadstore_chunk));
    __ Sw(t0, MemOperand(a0, 4, loadstore_chunk));
    __ Sw(t1, MemOperand(a0, 5, loadstore_chunk));
    __ Sw(t2, MemOperand(a0, 6, loadstore_chunk));
    __ Sw(t3, MemOperand(a0, 7, loadstore_chunk));
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
    __ Sw(v1, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

    // Copy the last 8 bytes.
    __ bind(&ua_smallCopy);
    __ beq(a2, zero_reg, &leave);
    __ addu(a3, a0, a2);  // In delay slot.

    __ bind(&ua_smallCopy_loop);
    __ Lb(v1, MemOperand(a1));
    __ addiu(a0, a0, 1);
    __ addiu(a1, a1, 1);
    __ bne(a0, a3, &ua_smallCopy_loop);
    __ Sb(v1, MemOperand(a0, -1));  // In delay slot.

    __ jr(ra);
    __ nop();
  }
  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<MemCopyUint8Function>(buffer);
#endif
}
#endif

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

  __ MovFromFloatParameter(f12);
  __ sqrt_d(f0, f12);
  __ MovToFloatResult(f0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
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

void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  Label indirect_string_loaded;
  __ bind(&indirect_string_loaded);

  // Fetch the instance type of the receiver into result register.
  __ Ld(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ Lbu(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ And(at, result, Operand(kIsIndirectStringMask));
  __ Branch(&check_sequential, eq, at, Operand(zero_reg));

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string, thin_string;
  __ And(at, result, Operand(kStringRepresentationMask));
  __ Branch(&cons_string, eq, at, Operand(kConsStringTag));
  __ Branch(&thin_string, eq, at, Operand(kThinStringTag));

  // Handle slices.
  __ Ld(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ Ld(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ dsra32(at, result, 0);
  __ Daddu(index, index, at);
  __ jmp(&indirect_string_loaded);

  // Handle thin strings.
  __ bind(&thin_string);
  __ Ld(string, FieldMemOperand(string, ThinString::kActualOffset));
  __ jmp(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ Ld(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ LoadRoot(at, Heap::kempty_stringRootIndex);
  __ Branch(call_runtime, ne, result, Operand(at));
  // Get the first of the two strings and load its instance type.
  __ Ld(string, FieldMemOperand(string, ConsString::kFirstOffset));
  __ jmp(&indirect_string_loaded);

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
  __ Ld(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ And(at, result, Operand(kStringEncodingMask));
  __ Branch(&one_byte, ne, at, Operand(zero_reg));
  // Two-byte string.
  __ Dlsa(at, string, index, 1);
  __ Lhu(result, MemOperand(at));
  __ jmp(&done);
  __ bind(&one_byte);
  // One_byte string.
  __ Daddu(at, string, index);
  __ Lbu(result, MemOperand(at));
  __ bind(&done);
}

#ifdef DEBUG
// nop(CODE_AGE_MARKER_NOP)
static const uint32_t kCodeAgePatchFirstInstruction = 0x00010180;
#endif


CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // Since patcher is a large object, allocate it dynamically when needed,
  // to avoid overloading the stack in stress conditions.
  // DONT_FLUSH is used because the CodeAgingHelper is initialized early in
  // the process, before MIPS simulator ICache is setup.
  std::unique_ptr<CodePatcher> patcher(
      new CodePatcher(isolate, young_sequence_.start(),
                      young_sequence_.length() / Assembler::kInstrSize,
                      CodePatcher::DONT_FLUSH));
  PredictableCodeSizeScope scope(patcher->masm(), young_sequence_.length());
  patcher->masm()->PushStandardFrame(a1);
  patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
  patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
  patcher->masm()->nop(Assembler::CODE_AGE_SEQUENCE_NOP);
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

Code::Age Code::GetCodeAge(Isolate* isolate, byte* sequence) {
  if (IsYoungSequence(isolate, sequence)) return kNoAgeCodeAge;

  Address target_address =
      Assembler::target_address_at(sequence + Assembler::kInstrSize);
  Code* stub = GetCodeFromTargetAddress(target_address);
  return GetAgeOfCodeAgeStub(stub);
}

void Code::PatchPlatformCodeAge(Isolate* isolate, byte* sequence,
                                Code::Age age) {
  uint32_t young_length = isolate->code_aging_helper()->young_sequence_length();
  if (age == kNoAgeCodeAge) {
    isolate->code_aging_helper()->CopyYoungSequenceTo(sequence);
    Assembler::FlushICache(isolate, sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age);
    CodePatcher patcher(isolate, sequence,
                        young_length / Assembler::kInstrSize);
    // Mark this code sequence for FindPlatformCodeAgeSequence().
    patcher.masm()->nop(Assembler::CODE_AGE_MARKER_NOP);
    // Load the stub address to t9 and call it,
    // GetCodeAge() extracts the stub address from this instruction.
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

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64

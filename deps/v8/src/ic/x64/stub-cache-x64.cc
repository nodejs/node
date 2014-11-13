// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/codegen.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(Isolate* isolate, MacroAssembler* masm,
                       Code::Flags flags, bool leave_frame,
                       StubCache::Table table, Register receiver, Register name,
                       // The offset is scaled by 4, based on
                       // kCacheIndexShift, which is two bits
                       Register offset) {
  // We need to scale up the pointer by 2 when the offset is scaled by less
  // than the pointer size.
  DCHECK(kPointerSize == kInt64Size
             ? kPointerSizeLog2 == StubCache::kCacheIndexShift + 1
             : kPointerSizeLog2 == StubCache::kCacheIndexShift);
  ScaleFactor scale_factor = kPointerSize == kInt64Size ? times_2 : times_1;

  DCHECK_EQ(3 * kPointerSize, sizeof(StubCache::Entry));
  // The offset register holds the entry offset times four (due to masking
  // and shifting optimizations).
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  Label miss;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ leap(offset, Operand(offset, offset, times_2, 0));

  __ LoadAddress(kScratchRegister, key_offset);

  // Check that the key in the entry matches the name.
  __ cmpp(name, Operand(kScratchRegister, offset, scale_factor, 0));
  __ j(not_equal, &miss);

  // Get the map entry from the cache.
  // Use key_offset + kPointerSize * 2, rather than loading map_offset.
  DCHECK(isolate->stub_cache()->map_reference(table).address() -
             isolate->stub_cache()->key_reference(table).address() ==
         kPointerSize * 2);
  __ movp(kScratchRegister,
          Operand(kScratchRegister, offset, scale_factor, kPointerSize * 2));
  __ cmpp(kScratchRegister, FieldOperand(receiver, HeapObject::kMapOffset));
  __ j(not_equal, &miss);

  // Get the code entry from the cache.
  __ LoadAddress(kScratchRegister, value_offset);
  __ movp(kScratchRegister, Operand(kScratchRegister, offset, scale_factor, 0));

  // Check that the flags match what we're looking for.
  __ movl(offset, FieldOperand(kScratchRegister, Code::kFlagsOffset));
  __ andp(offset, Immediate(~Code::kFlagsNotUsedInLookup));
  __ cmpl(offset, Immediate(flags));
  __ j(not_equal, &miss);

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ jmp(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ jmp(&miss);
  }
#endif

  if (leave_frame) __ leave();

  // Jump to the first instruction in the code stub.
  __ addp(kScratchRegister, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(kScratchRegister);

  __ bind(&miss);
}


void StubCache::GenerateProbe(MacroAssembler* masm, Code::Flags flags,
                              bool leave_frame, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Isolate* isolate = masm->isolate();
  Label miss;
  USE(extra);   // The register extra is not used on the X64 platform.
  USE(extra2);  // The register extra2 is not used on the X64 platform.
  USE(extra3);  // The register extra2 is not used on the X64 platform.
  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 3 * kPointerSize.
  DCHECK(sizeof(Entry) == 3 * kPointerSize);

  // Make sure the flags do not name a specific type.
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  DCHECK(!scratch.is(receiver));
  DCHECK(!scratch.is(name));

  // Check scratch register is valid, extra and extra2 are unused.
  DCHECK(!scratch.is(no_reg));
  DCHECK(extra2.is(no_reg));
  DCHECK(extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ movl(scratch, FieldOperand(name, Name::kHashFieldOffset));
  // Use only the low 32 bits of the map pointer.
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xorp(scratch, Immediate(flags));
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ andp(scratch, Immediate((kPrimaryTableSize - 1) << kCacheIndexShift));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, leave_frame, kPrimary, receiver, name,
             scratch);

  // Primary miss: Compute hash for secondary probe.
  __ movl(scratch, FieldOperand(name, Name::kHashFieldOffset));
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xorp(scratch, Immediate(flags));
  __ andp(scratch, Immediate((kPrimaryTableSize - 1) << kCacheIndexShift));
  __ subl(scratch, name);
  __ addl(scratch, Immediate(flags));
  __ andp(scratch, Immediate((kSecondaryTableSize - 1) << kCacheIndexShift));

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, leave_frame, kSecondary, receiver, name,
             scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64

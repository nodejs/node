// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

static void ProbeTable(StubCache* stub_cache, MacroAssembler* masm,
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

  DCHECK_EQ(3u * kPointerSize, sizeof(StubCache::Entry));
  // The offset register holds the entry offset times four (due to masking
  // and shifting optimizations).
  ExternalReference key_offset(stub_cache->key_reference(table));
  ExternalReference value_offset(stub_cache->value_reference(table));
  Label miss;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ leap(offset, Operand(offset, offset, times_2, 0));

  __ LoadAddress(kScratchRegister, key_offset);

  // Check that the key in the entry matches the name.
  __ cmpp(name, Operand(kScratchRegister, offset, scale_factor, 0));
  __ j(not_equal, &miss);

  // Get the map entry from the cache.
  // Use key_offset + kPointerSize * 2, rather than loading map_offset.
  DCHECK(stub_cache->map_reference(table).address() -
             stub_cache->key_reference(table).address() ==
         kPointerSize * 2);
  __ movp(kScratchRegister,
          Operand(kScratchRegister, offset, scale_factor, kPointerSize * 2));
  __ cmpp(kScratchRegister, FieldOperand(receiver, HeapObject::kMapOffset));
  __ j(not_equal, &miss);

  // Get the code entry from the cache.
  __ LoadAddress(kScratchRegister, value_offset);
  __ movp(kScratchRegister, Operand(kScratchRegister, offset, scale_factor, 0));

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ jmp(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ jmp(&miss);
  }
#endif

  // Jump to the first instruction in the code stub.
  __ addp(kScratchRegister, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(kScratchRegister);

  __ bind(&miss);
}

void StubCache::GenerateProbe(MacroAssembler* masm, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Label miss;
  USE(extra);   // The register extra is not used on the X64 platform.
  USE(extra2);  // The register extra2 is not used on the X64 platform.
  USE(extra3);  // The register extra2 is not used on the X64 platform.
  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 3 * kPointerSize.
  DCHECK(sizeof(Entry) == 3 * kPointerSize);

  // Make sure that there are no register conflicts.
  DCHECK(!scratch.is(receiver));
  DCHECK(!scratch.is(name));

  // Check scratch register is valid, extra and extra2 are unused.
  DCHECK(!scratch.is(no_reg));
  DCHECK(extra2.is(no_reg));
  DCHECK(extra3.is(no_reg));

#ifdef DEBUG
  // If vector-based ics are in use, ensure that scratch doesn't conflict with
  // the vector and slot registers, which need to be preserved for a handler
  // call or miss.
  if (IC::ICUseVector(ic_kind_)) {
    if (ic_kind_ == Code::LOAD_IC || ic_kind_ == Code::KEYED_LOAD_IC) {
      Register vector = LoadWithVectorDescriptor::VectorRegister();
      Register slot = LoadDescriptor::SlotRegister();
      DCHECK(!AreAliased(vector, slot, scratch));
    } else {
      DCHECK(ic_kind_ == Code::STORE_IC || ic_kind_ == Code::KEYED_STORE_IC);
      Register vector = StoreWithVectorDescriptor::VectorRegister();
      Register slot = StoreWithVectorDescriptor::SlotRegister();
      DCHECK(!AreAliased(vector, slot, scratch));
    }
  }
#endif

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ movl(scratch, FieldOperand(name, Name::kHashFieldOffset));
  // Use only the low 32 bits of the map pointer.
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xorp(scratch, Immediate(kPrimaryMagic));
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ andp(scratch, Immediate((kPrimaryTableSize - 1) << kCacheIndexShift));

  // Probe the primary table.
  ProbeTable(this, masm, kPrimary, receiver, name, scratch);

  // Primary miss: Compute hash for secondary probe.
  __ movl(scratch, FieldOperand(name, Name::kHashFieldOffset));
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xorp(scratch, Immediate(kPrimaryMagic));
  __ andp(scratch, Immediate((kPrimaryTableSize - 1) << kCacheIndexShift));
  __ subl(scratch, name);
  __ addl(scratch, Immediate(kSecondaryMagic));
  __ andp(scratch, Immediate((kSecondaryTableSize - 1) << kCacheIndexShift));

  // Probe the secondary table.
  ProbeTable(this, masm, kSecondary, receiver, name, scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

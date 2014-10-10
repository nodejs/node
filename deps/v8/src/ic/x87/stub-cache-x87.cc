// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X87

#include "src/codegen.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(Isolate* isolate, MacroAssembler* masm,
                       Code::Flags flags, bool leave_frame,
                       StubCache::Table table, Register name, Register receiver,
                       // Number of the cache entry pointer-size scaled.
                       Register offset, Register extra) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  Label miss;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ lea(offset, Operand(offset, offset, times_2, 0));

  if (extra.is_valid()) {
    // Get the code entry from the cache.
    __ mov(extra, Operand::StaticArray(offset, times_1, value_offset));

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_1, key_offset));
    __ j(not_equal, &miss);

    // Check the map matches.
    __ mov(offset, Operand::StaticArray(offset, times_1, map_offset));
    __ cmp(offset, FieldOperand(receiver, HeapObject::kMapOffset));
    __ j(not_equal, &miss);

    // Check that the flags match what we're looking for.
    __ mov(offset, FieldOperand(extra, Code::kFlagsOffset));
    __ and_(offset, ~Code::kFlagsNotUsedInLookup);
    __ cmp(offset, flags);
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
    __ add(extra, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(extra);

    __ bind(&miss);
  } else {
    // Save the offset on the stack.
    __ push(offset);

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_1, key_offset));
    __ j(not_equal, &miss);

    // Check the map matches.
    __ mov(offset, Operand::StaticArray(offset, times_1, map_offset));
    __ cmp(offset, FieldOperand(receiver, HeapObject::kMapOffset));
    __ j(not_equal, &miss);

    // Restore offset register.
    __ mov(offset, Operand(esp, 0));

    // Get the code entry from the cache.
    __ mov(offset, Operand::StaticArray(offset, times_1, value_offset));

    // Check that the flags match what we're looking for.
    __ mov(offset, FieldOperand(offset, Code::kFlagsOffset));
    __ and_(offset, ~Code::kFlagsNotUsedInLookup);
    __ cmp(offset, flags);
    __ j(not_equal, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

    // Restore offset and re-load code entry from cache.
    __ pop(offset);
    __ mov(offset, Operand::StaticArray(offset, times_1, value_offset));

    if (leave_frame) __ leave();

    // Jump to the first instruction in the code stub.
    __ add(offset, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(offset);

    // Pop at miss.
    __ bind(&miss);
    __ pop(offset);
  }
}


void StubCache::GenerateProbe(MacroAssembler* masm, Code::Flags flags,
                              bool leave_frame, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Label miss;

  // Assert that code is valid.  The multiplying code relies on the entry size
  // being 12.
  DCHECK(sizeof(Entry) == 12);

  // Assert the flags do not name a specific type.
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Assert that there are no register conflicts.
  DCHECK(!scratch.is(receiver));
  DCHECK(!scratch.is(name));
  DCHECK(!extra.is(receiver));
  DCHECK(!extra.is(name));
  DCHECK(!extra.is(scratch));

  // Assert scratch and extra registers are valid, and extra2/3 are unused.
  DCHECK(!scratch.is(no_reg));
  DCHECK(extra2.is(no_reg));
  DCHECK(extra3.is(no_reg));

  Register offset = scratch;
  scratch = no_reg;

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ mov(offset, FieldOperand(name, Name::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, flags);
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ and_(offset, (kPrimaryTableSize - 1) << kCacheIndexShift);
  // ProbeTable expects the offset to be pointer scaled, which it is, because
  // the heap object tag size is 2 and the pointer size log 2 is also 2.
  DCHECK(kCacheIndexShift == kPointerSizeLog2);

  // Probe the primary table.
  ProbeTable(isolate(), masm, flags, leave_frame, kPrimary, name, receiver,
             offset, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(offset, FieldOperand(name, Name::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, flags);
  __ and_(offset, (kPrimaryTableSize - 1) << kCacheIndexShift);
  __ sub(offset, name);
  __ add(offset, Immediate(flags));
  __ and_(offset, (kSecondaryTableSize - 1) << kCacheIndexShift);

  // Probe the secondary table.
  ProbeTable(isolate(), masm, flags, leave_frame, kSecondary, name, receiver,
             offset, extra);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X87

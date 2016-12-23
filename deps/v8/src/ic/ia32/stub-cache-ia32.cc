// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

static void ProbeTable(StubCache* stub_cache, MacroAssembler* masm,
                       StubCache::Table table, Register name, Register receiver,
                       // The offset is scaled by 4, based on
                       // kCacheIndexShift, which is two bits
                       Register offset, Register extra) {
  ExternalReference key_offset(stub_cache->key_reference(table));
  ExternalReference value_offset(stub_cache->value_reference(table));
  ExternalReference map_offset(stub_cache->map_reference(table));

  Label miss;
  Code::Kind ic_kind = stub_cache->ic_kind();
  bool is_vector_store =
      IC::ICUseVector(ic_kind) &&
      (ic_kind == Code::STORE_IC || ic_kind == Code::KEYED_STORE_IC);

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

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

    if (is_vector_store) {
      // The value, vector and slot were passed to the IC on the stack and
      // they are still there. So we can just jump to the handler.
      DCHECK(extra.is(StoreWithVectorDescriptor::SlotRegister()));
      __ add(extra, Immediate(Code::kHeaderSize - kHeapObjectTag));
      __ jmp(extra);
    } else {
      // The vector and slot were pushed onto the stack before starting the
      // probe, and need to be dropped before calling the handler.
      __ pop(LoadWithVectorDescriptor::VectorRegister());
      __ pop(LoadDescriptor::SlotRegister());
      __ add(extra, Immediate(Code::kHeaderSize - kHeapObjectTag));
      __ jmp(extra);
    }

    __ bind(&miss);
  } else {
    DCHECK(ic_kind == Code::STORE_IC || ic_kind == Code::KEYED_STORE_IC);

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

    // Jump to the first instruction in the code stub.
    if (is_vector_store) {
      DCHECK(offset.is(StoreWithVectorDescriptor::SlotRegister()));
    }
    __ add(offset, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(offset);

    // Pop at miss.
    __ bind(&miss);
    __ pop(offset);
  }
}

void StubCache::GenerateProbe(MacroAssembler* masm, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Label miss;

  // Assert that code is valid.  The multiplying code relies on the entry size
  // being 12.
  DCHECK(sizeof(Entry) == 12);

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
  __ xor_(offset, kPrimaryMagic);
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ and_(offset, (kPrimaryTableSize - 1) << kCacheIndexShift);
  // ProbeTable expects the offset to be pointer scaled, which it is, because
  // the heap object tag size is 2 and the pointer size log 2 is also 2.
  DCHECK(kCacheIndexShift == kPointerSizeLog2);

  // Probe the primary table.
  ProbeTable(this, masm, kPrimary, name, receiver, offset, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(offset, FieldOperand(name, Name::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, kPrimaryMagic);
  __ and_(offset, (kPrimaryTableSize - 1) << kCacheIndexShift);
  __ sub(offset, name);
  __ add(offset, Immediate(kSecondaryMagic));
  __ and_(offset, (kSecondaryTableSize - 1) << kCacheIndexShift);

  // Probe the secondary table.
  ProbeTable(this, masm, kSecondary, name, receiver, offset, extra);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

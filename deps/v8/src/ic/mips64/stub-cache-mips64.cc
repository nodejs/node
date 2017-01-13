// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

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
                       Register offset, Register scratch, Register scratch2,
                       Register offset_scratch) {
  ExternalReference key_offset(stub_cache->key_reference(table));
  ExternalReference value_offset(stub_cache->value_reference(table));
  ExternalReference map_offset(stub_cache->map_reference(table));

  uint64_t key_off_addr = reinterpret_cast<uint64_t>(key_offset.address());
  uint64_t value_off_addr = reinterpret_cast<uint64_t>(value_offset.address());
  uint64_t map_off_addr = reinterpret_cast<uint64_t>(map_offset.address());

  // Check the relative positions of the address fields.
  DCHECK(value_off_addr > key_off_addr);
  DCHECK((value_off_addr - key_off_addr) % 4 == 0);
  DCHECK((value_off_addr - key_off_addr) < (256 * 4));
  DCHECK(map_off_addr > key_off_addr);
  DCHECK((map_off_addr - key_off_addr) % 4 == 0);
  DCHECK((map_off_addr - key_off_addr) < (256 * 4));

  Label miss;
  Register base_addr = scratch;
  scratch = no_reg;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ Dlsa(offset_scratch, offset, offset, 1);

  // Calculate the base address of the entry.
  __ li(base_addr, Operand(key_offset));
  __ Dlsa(base_addr, base_addr, offset_scratch,
          kPointerSizeLog2 - StubCache::kCacheIndexShift);

  // Check that the key in the entry matches the name.
  __ ld(at, MemOperand(base_addr, 0));
  __ Branch(&miss, ne, name, Operand(at));

  // Check the map matches.
  __ ld(at, MemOperand(base_addr,
                       static_cast<int32_t>(map_off_addr - key_off_addr)));
  __ ld(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Branch(&miss, ne, at, Operand(scratch2));

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ ld(code, MemOperand(base_addr,
                         static_cast<int32_t>(value_off_addr - key_off_addr)));

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ jmp(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ jmp(&miss);
  }
#endif

  // Jump to the first instruction in the code stub.
  __ Daddu(at, code, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);

  // Miss: fall through.
  __ bind(&miss);
}

void StubCache::GenerateProbe(MacroAssembler* masm, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Label miss;

  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 12.
  // DCHECK(sizeof(Entry) == 12);
  // DCHECK(sizeof(Entry) == 3 * kPointerSize);

  // Make sure that there are no register conflicts.
  DCHECK(!AreAliased(receiver, name, scratch, extra, extra2, extra3));

  // Check register validity.
  DCHECK(!scratch.is(no_reg));
  DCHECK(!extra.is(no_reg));
  DCHECK(!extra2.is(no_reg));
  DCHECK(!extra3.is(no_reg));

#ifdef DEBUG
  // If vector-based ics are in use, ensure that scratch, extra, extra2 and
  // extra3 don't conflict with the vector and slot registers, which need
  // to be preserved for a handler call or miss.
  if (IC::ICUseVector(ic_kind_)) {
    Register vector, slot;
    if (ic_kind_ == Code::STORE_IC || ic_kind_ == Code::KEYED_STORE_IC) {
      vector = StoreWithVectorDescriptor::VectorRegister();
      slot = StoreWithVectorDescriptor::SlotRegister();
    } else {
      DCHECK(ic_kind_ == Code::LOAD_IC || ic_kind_ == Code::KEYED_LOAD_IC);
      vector = LoadWithVectorDescriptor::VectorRegister();
      slot = LoadWithVectorDescriptor::SlotRegister();
    }
    DCHECK(!AreAliased(vector, slot, scratch, extra, extra2, extra3));
  }
#endif

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1, extra2,
                      extra3);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ lwu(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ ld(at, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Addu(scratch, scratch, at);
  __ Xor(scratch, scratch, Operand(kPrimaryMagic));
  __ And(scratch, scratch,
         Operand((kPrimaryTableSize - 1) << kCacheIndexShift));

  // Probe the primary table.
  ProbeTable(this, masm, kPrimary, receiver, name, scratch, extra, extra2,
             extra3);

  // Primary miss: Compute hash for secondary probe.
  __ Subu(scratch, scratch, name);
  __ Addu(scratch, scratch, kSecondaryMagic);
  __ And(scratch, scratch,
         Operand((kSecondaryTableSize - 1) << kCacheIndexShift));

  // Probe the secondary table.
  ProbeTable(this, masm, kSecondary, receiver, name, scratch, extra, extra2,
             extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1, extra2,
                      extra3);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64

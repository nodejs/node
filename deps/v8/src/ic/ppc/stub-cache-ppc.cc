// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(Isolate* isolate, MacroAssembler* masm,
                       Code::Kind ic_kind, Code::Flags flags,
                       StubCache::Table table, Register receiver, Register name,
                       // Number of the cache entry, not scaled.
                       Register offset, Register scratch, Register scratch2,
                       Register offset_scratch) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  uintptr_t key_off_addr = reinterpret_cast<uintptr_t>(key_offset.address());
  uintptr_t value_off_addr =
      reinterpret_cast<uintptr_t>(value_offset.address());
  uintptr_t map_off_addr = reinterpret_cast<uintptr_t>(map_offset.address());

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
  __ ShiftLeftImm(offset_scratch, offset, Operand(1));
  __ add(offset_scratch, offset, offset_scratch);

  // Calculate the base address of the entry.
  __ mov(base_addr, Operand(key_offset));
#if V8_TARGET_ARCH_PPC64
  DCHECK(kPointerSizeLog2 > StubCache::kCacheIndexShift);
  __ ShiftLeftImm(offset_scratch, offset_scratch,
                  Operand(kPointerSizeLog2 - StubCache::kCacheIndexShift));
#else
  DCHECK(kPointerSizeLog2 == StubCache::kCacheIndexShift);
#endif
  __ add(base_addr, base_addr, offset_scratch);

  // Check that the key in the entry matches the name.
  __ LoadP(ip, MemOperand(base_addr, 0));
  __ cmp(name, ip);
  __ bne(&miss);

  // Check the map matches.
  __ LoadP(ip, MemOperand(base_addr, map_off_addr - key_off_addr));
  __ LoadP(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ cmp(ip, scratch2);
  __ bne(&miss);

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ LoadP(code, MemOperand(base_addr, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  Register flags_reg = base_addr;
  base_addr = no_reg;
  __ lwz(flags_reg, FieldMemOperand(code, Code::kFlagsOffset));

  DCHECK(!r0.is(flags_reg));
  __ li(r0, Operand(Code::kFlagsNotUsedInLookup));
  __ andc(flags_reg, flags_reg, r0);
  __ mov(r0, Operand(flags));
  __ cmpl(flags_reg, r0);
  __ bne(&miss);

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ b(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ b(&miss);
  }
#endif

  // Jump to the first instruction in the code stub.
  __ addi(r0, code, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ mtctr(r0);
  __ bctr();

  // Miss: fall through.
  __ bind(&miss);
}


void StubCache::GenerateProbe(MacroAssembler* masm, Code::Kind ic_kind,
                              Code::Flags flags, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Isolate* isolate = masm->isolate();
  Label miss;

#if V8_TARGET_ARCH_PPC64
  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 24.
  DCHECK(sizeof(Entry) == 24);
#else
  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 12.
  DCHECK(sizeof(Entry) == 12);
#endif

  // Make sure the flags does not name a specific type.
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  DCHECK(!AreAliased(receiver, name, scratch, extra, extra2, extra3));

  // Check scratch, extra and extra2 registers are valid.
  DCHECK(!scratch.is(no_reg));
  DCHECK(!extra.is(no_reg));
  DCHECK(!extra2.is(no_reg));
  DCHECK(!extra3.is(no_reg));

#ifdef DEBUG
  // If vector-based ics are in use, ensure that scratch, extra, extra2 and
  // extra3 don't conflict with the vector and slot registers, which need
  // to be preserved for a handler call or miss.
  if (IC::ICUseVector(ic_kind)) {
    Register vector, slot;
    if (ic_kind == Code::STORE_IC || ic_kind == Code::KEYED_STORE_IC) {
      vector = VectorStoreICDescriptor::VectorRegister();
      slot = VectorStoreICDescriptor::SlotRegister();
    } else {
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
  __ lwz(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ LoadP(ip, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ add(scratch, scratch, ip);
  __ xori(scratch, scratch, Operand(flags));
  // The mask omits the last two bits because they are not part of the hash.
  __ andi(scratch, scratch,
          Operand((kPrimaryTableSize - 1) << kCacheIndexShift));

  // Probe the primary table.
  ProbeTable(isolate, masm, ic_kind, flags, kPrimary, receiver, name, scratch,
             extra, extra2, extra3);

  // Primary miss: Compute hash for secondary probe.
  __ sub(scratch, scratch, name);
  __ addi(scratch, scratch, Operand(flags));
  __ andi(scratch, scratch,
          Operand((kSecondaryTableSize - 1) << kCacheIndexShift));

  // Probe the secondary table.
  ProbeTable(isolate, masm, ic_kind, flags, kSecondary, receiver, name, scratch,
             extra, extra2, extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1, extra2,
                      extra3);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

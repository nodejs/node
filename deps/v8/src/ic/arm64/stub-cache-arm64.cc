// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


// Probe primary or secondary table.
// If the entry is found in the cache, the generated code jump to the first
// instruction of the stub in the cache.
// If there is a miss the code fall trough.
//
// 'receiver', 'name' and 'offset' registers are preserved on miss.
static void ProbeTable(Isolate* isolate, MacroAssembler* masm,
                       Code::Kind ic_kind, Code::Flags flags,
                       StubCache::Table table, Register receiver, Register name,
                       Register offset, Register scratch, Register scratch2,
                       Register scratch3) {
  // Some code below relies on the fact that the Entry struct contains
  // 3 pointers (name, code, map).
  STATIC_ASSERT(sizeof(StubCache::Entry) == (3 * kPointerSize));

  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  uintptr_t key_off_addr = reinterpret_cast<uintptr_t>(key_offset.address());
  uintptr_t value_off_addr =
      reinterpret_cast<uintptr_t>(value_offset.address());
  uintptr_t map_off_addr = reinterpret_cast<uintptr_t>(map_offset.address());

  Label miss;

  DCHECK(!AreAliased(name, offset, scratch, scratch2, scratch3));

  // Multiply by 3 because there are 3 fields per entry.
  __ Add(scratch3, offset, Operand(offset, LSL, 1));

  // Calculate the base address of the entry.
  __ Mov(scratch, key_offset);
  __ Add(scratch, scratch, Operand(scratch3, LSL, kPointerSizeLog2));

  // Check that the key in the entry matches the name.
  __ Ldr(scratch2, MemOperand(scratch));
  __ Cmp(name, scratch2);
  __ B(ne, &miss);

  // Check the map matches.
  __ Ldr(scratch2, MemOperand(scratch, map_off_addr - key_off_addr));
  __ Ldr(scratch3, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Cmp(scratch2, scratch3);
  __ B(ne, &miss);

  // Get the code entry from the cache.
  __ Ldr(scratch, MemOperand(scratch, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  __ Ldr(scratch2.W(), FieldMemOperand(scratch, Code::kFlagsOffset));
  __ Bic(scratch2.W(), scratch2.W(), Code::kFlagsNotUsedInLookup);
  __ Cmp(scratch2.W(), flags);
  __ B(ne, &miss);

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ B(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ B(&miss);
  }
#endif

  // Jump to the first instruction in the code stub.
  __ Add(scratch, scratch, Code::kHeaderSize - kHeapObjectTag);
  __ Br(scratch);

  // Miss: fall through.
  __ Bind(&miss);
}


void StubCache::GenerateProbe(MacroAssembler* masm, Code::Kind ic_kind,
                              Code::Flags flags, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Isolate* isolate = masm->isolate();
  Label miss;

  // Make sure the flags does not name a specific type.
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  DCHECK(!AreAliased(receiver, name, scratch, extra, extra2, extra3));

  // Make sure extra and extra2 registers are valid.
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

  // Compute the hash for primary table.
  __ Ldr(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ Ldr(extra, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Add(scratch, scratch, extra);
  __ Eor(scratch, scratch, flags);
  // We shift out the last two bits because they are not part of the hash.
  __ Ubfx(scratch, scratch, kCacheIndexShift,
          CountTrailingZeros(kPrimaryTableSize, 64));

  // Probe the primary table.
  ProbeTable(isolate, masm, ic_kind, flags, kPrimary, receiver, name, scratch,
             extra, extra2, extra3);

  // Primary miss: Compute hash for secondary table.
  __ Sub(scratch, scratch, Operand(name, LSR, kCacheIndexShift));
  __ Add(scratch, scratch, flags >> kCacheIndexShift);
  __ And(scratch, scratch, kSecondaryTableSize - 1);

  // Probe the secondary table.
  ProbeTable(isolate, masm, ic_kind, flags, kSecondary, receiver, name, scratch,
             extra, extra2, extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ Bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1, extra2,
                      extra3);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

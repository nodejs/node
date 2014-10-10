// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM

#include "src/codegen.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(Isolate* isolate, MacroAssembler* masm,
                       Code::Flags flags, bool leave_frame,
                       StubCache::Table table, Register receiver, Register name,
                       // Number of the cache entry, not scaled.
                       Register offset, Register scratch, Register scratch2,
                       Register offset_scratch) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  uint32_t key_off_addr = reinterpret_cast<uint32_t>(key_offset.address());
  uint32_t value_off_addr = reinterpret_cast<uint32_t>(value_offset.address());
  uint32_t map_off_addr = reinterpret_cast<uint32_t>(map_offset.address());

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
  __ add(offset_scratch, offset, Operand(offset, LSL, 1));

  // Calculate the base address of the entry.
  __ mov(base_addr, Operand(key_offset));
  __ add(base_addr, base_addr, Operand(offset_scratch, LSL, kPointerSizeLog2));

  // Check that the key in the entry matches the name.
  __ ldr(ip, MemOperand(base_addr, 0));
  __ cmp(name, ip);
  __ b(ne, &miss);

  // Check the map matches.
  __ ldr(ip, MemOperand(base_addr, map_off_addr - key_off_addr));
  __ ldr(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ cmp(ip, scratch2);
  __ b(ne, &miss);

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ ldr(code, MemOperand(base_addr, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  Register flags_reg = base_addr;
  base_addr = no_reg;
  __ ldr(flags_reg, FieldMemOperand(code, Code::kFlagsOffset));
  // It's a nice optimization if this constant is encodable in the bic insn.

  uint32_t mask = Code::kFlagsNotUsedInLookup;
  DCHECK(__ ImmediateFitsAddrMode1Instruction(mask));
  __ bic(flags_reg, flags_reg, Operand(mask));
  __ cmp(flags_reg, Operand(flags));
  __ b(ne, &miss);

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ jmp(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ jmp(&miss);
  }
#endif

  if (leave_frame) __ LeaveFrame(StackFrame::INTERNAL);

  // Jump to the first instruction in the code stub.
  __ add(pc, code, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Miss: fall through.
  __ bind(&miss);
}


void StubCache::GenerateProbe(MacroAssembler* masm, Code::Flags flags,
                              bool leave_frame, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Isolate* isolate = masm->isolate();
  Label miss;

  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 12.
  DCHECK(sizeof(Entry) == 12);

  // Make sure the flags does not name a specific type.
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  DCHECK(!scratch.is(receiver));
  DCHECK(!scratch.is(name));
  DCHECK(!extra.is(receiver));
  DCHECK(!extra.is(name));
  DCHECK(!extra.is(scratch));
  DCHECK(!extra2.is(receiver));
  DCHECK(!extra2.is(name));
  DCHECK(!extra2.is(scratch));
  DCHECK(!extra2.is(extra));

  // Check scratch, extra and extra2 registers are valid.
  DCHECK(!scratch.is(no_reg));
  DCHECK(!extra.is(no_reg));
  DCHECK(!extra2.is(no_reg));
  DCHECK(!extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1, extra2,
                      extra3);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ ldr(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ ldr(ip, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ add(scratch, scratch, Operand(ip));
  uint32_t mask = kPrimaryTableSize - 1;
  // We shift out the last two bits because they are not part of the hash and
  // they are always 01 for maps.
  __ mov(scratch, Operand(scratch, LSR, kCacheIndexShift));
  // Mask down the eor argument to the minimum to keep the immediate
  // ARM-encodable.
  __ eor(scratch, scratch, Operand((flags >> kCacheIndexShift) & mask));
  // Prefer and_ to ubfx here because ubfx takes 2 cycles.
  __ and_(scratch, scratch, Operand(mask));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, leave_frame, kPrimary, receiver, name,
             scratch, extra, extra2, extra3);

  // Primary miss: Compute hash for secondary probe.
  __ sub(scratch, scratch, Operand(name, LSR, kCacheIndexShift));
  uint32_t mask2 = kSecondaryTableSize - 1;
  __ add(scratch, scratch, Operand((flags >> kCacheIndexShift) & mask2));
  __ and_(scratch, scratch, Operand(mask2));

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, leave_frame, kSecondary, receiver, name,
             scratch, extra, extra2, extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1, extra2,
                      extra3);
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

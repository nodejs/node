// Copyright 2013 the V8 project authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if V8_TARGET_ARCH_ARM64

#define ARM64_DEFINE_REG_STATICS
#include "src/arm64/assembler-arm64.h"

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/frames-arm64.h"
#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/register-configuration.h"

namespace v8 {
namespace internal {


// -----------------------------------------------------------------------------
// CpuFeatures implementation.

void CpuFeatures::ProbeImpl(bool cross_compile) {
  // AArch64 has no configuration options, no further probing is required.
  supported_ = 0;

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

  // Probe for runtime features
  base::CPU cpu;
  if (cpu.implementer() == base::CPU::NVIDIA &&
      cpu.variant() == base::CPU::NVIDIA_DENVER &&
      cpu.part() <= base::CPU::NVIDIA_DENVER_V10) {
    supported_ |= 1u << COHERENT_CACHE;
  }
}


void CpuFeatures::PrintTarget() { }


void CpuFeatures::PrintFeatures() {
  printf("COHERENT_CACHE=%d\n", CpuFeatures::IsSupported(COHERENT_CACHE));
}


// -----------------------------------------------------------------------------
// CPURegList utilities.

CPURegister CPURegList::PopLowestIndex() {
  DCHECK(IsValid());
  if (IsEmpty()) {
    return NoCPUReg;
  }
  int index = CountTrailingZeros(list_, kRegListSizeInBits);
  DCHECK((1 << index) & list_);
  Remove(index);
  return CPURegister::Create(index, size_, type_);
}


CPURegister CPURegList::PopHighestIndex() {
  DCHECK(IsValid());
  if (IsEmpty()) {
    return NoCPUReg;
  }
  int index = CountLeadingZeros(list_, kRegListSizeInBits);
  index = kRegListSizeInBits - 1 - index;
  DCHECK((1 << index) & list_);
  Remove(index);
  return CPURegister::Create(index, size_, type_);
}


void CPURegList::RemoveCalleeSaved() {
  if (type() == CPURegister::kRegister) {
    Remove(GetCalleeSaved(RegisterSizeInBits()));
  } else if (type() == CPURegister::kFPRegister) {
    Remove(GetCalleeSavedFP(RegisterSizeInBits()));
  } else {
    DCHECK(type() == CPURegister::kNoRegister);
    DCHECK(IsEmpty());
    // The list must already be empty, so do nothing.
  }
}


CPURegList CPURegList::GetCalleeSaved(int size) {
  return CPURegList(CPURegister::kRegister, size, 19, 29);
}


CPURegList CPURegList::GetCalleeSavedFP(int size) {
  return CPURegList(CPURegister::kFPRegister, size, 8, 15);
}


CPURegList CPURegList::GetCallerSaved(int size) {
  // Registers x0-x18 and lr (x30) are caller-saved.
  CPURegList list = CPURegList(CPURegister::kRegister, size, 0, 18);
  list.Combine(lr);
  return list;
}


CPURegList CPURegList::GetCallerSavedFP(int size) {
  // Registers d0-d7 and d16-d31 are caller-saved.
  CPURegList list = CPURegList(CPURegister::kFPRegister, size, 0, 7);
  list.Combine(CPURegList(CPURegister::kFPRegister, size, 16, 31));
  return list;
}


// This function defines the list of registers which are associated with a
// safepoint slot. Safepoint register slots are saved contiguously on the stack.
// MacroAssembler::SafepointRegisterStackIndex handles mapping from register
// code to index in the safepoint register slots. Any change here can affect
// this mapping.
CPURegList CPURegList::GetSafepointSavedRegisters() {
  CPURegList list = CPURegList::GetCalleeSaved();
  list.Combine(
      CPURegList(CPURegister::kRegister, kXRegSizeInBits, kJSCallerSaved));

  // Note that unfortunately we can't use symbolic names for registers and have
  // to directly use register codes. This is because this function is used to
  // initialize some static variables and we can't rely on register variables
  // to be initialized due to static initialization order issues in C++.

  // Drop ip0 and ip1 (i.e. x16 and x17), as they should not be expected to be
  // preserved outside of the macro assembler.
  list.Remove(16);
  list.Remove(17);

  // Add x18 to the safepoint list, as although it's not in kJSCallerSaved, it
  // is a caller-saved register according to the procedure call standard.
  list.Combine(18);

  // Drop jssp as the stack pointer doesn't need to be included.
  list.Remove(28);

  // Add the link register (x30) to the safepoint list.
  list.Combine(30);

  return list;
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

const int RelocInfo::kApplyMask = 1 << RelocInfo::INTERNAL_REFERENCE;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded. Being
  // specially coded on ARM64 means that it is a movz/movk sequence. We don't
  // generate those for relocatable pointers.
  return false;
}


bool RelocInfo::IsInConstantPool() {
  Instruction* instr = reinterpret_cast<Instruction*>(pc_);
  return instr->IsLdrLiteralX();
}


Register GetAllocatableRegisterThatIsNotOneOf(Register reg1, Register reg2,
                                              Register reg3, Register reg4) {
  CPURegList regs(reg1, reg2, reg3, reg4);
  const RegisterConfiguration* config =
      RegisterConfiguration::ArchDefault(RegisterConfiguration::CRANKSHAFT);
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    Register candidate = Register::from_code(code);
    if (regs.IncludesAliasOf(candidate)) continue;
    return candidate;
  }
  UNREACHABLE();
  return NoReg;
}


bool AreAliased(const CPURegister& reg1, const CPURegister& reg2,
                const CPURegister& reg3, const CPURegister& reg4,
                const CPURegister& reg5, const CPURegister& reg6,
                const CPURegister& reg7, const CPURegister& reg8) {
  int number_of_valid_regs = 0;
  int number_of_valid_fpregs = 0;

  RegList unique_regs = 0;
  RegList unique_fpregs = 0;

  const CPURegister regs[] = {reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8};

  for (unsigned i = 0; i < arraysize(regs); i++) {
    if (regs[i].IsRegister()) {
      number_of_valid_regs++;
      unique_regs |= regs[i].Bit();
    } else if (regs[i].IsFPRegister()) {
      number_of_valid_fpregs++;
      unique_fpregs |= regs[i].Bit();
    } else {
      DCHECK(!regs[i].IsValid());
    }
  }

  int number_of_unique_regs =
    CountSetBits(unique_regs, sizeof(unique_regs) * kBitsPerByte);
  int number_of_unique_fpregs =
    CountSetBits(unique_fpregs, sizeof(unique_fpregs) * kBitsPerByte);

  DCHECK(number_of_valid_regs >= number_of_unique_regs);
  DCHECK(number_of_valid_fpregs >= number_of_unique_fpregs);

  return (number_of_valid_regs != number_of_unique_regs) ||
         (number_of_valid_fpregs != number_of_unique_fpregs);
}


bool AreSameSizeAndType(const CPURegister& reg1, const CPURegister& reg2,
                        const CPURegister& reg3, const CPURegister& reg4,
                        const CPURegister& reg5, const CPURegister& reg6,
                        const CPURegister& reg7, const CPURegister& reg8) {
  DCHECK(reg1.IsValid());
  bool match = true;
  match &= !reg2.IsValid() || reg2.IsSameSizeAndType(reg1);
  match &= !reg3.IsValid() || reg3.IsSameSizeAndType(reg1);
  match &= !reg4.IsValid() || reg4.IsSameSizeAndType(reg1);
  match &= !reg5.IsValid() || reg5.IsSameSizeAndType(reg1);
  match &= !reg6.IsValid() || reg6.IsSameSizeAndType(reg1);
  match &= !reg7.IsValid() || reg7.IsSameSizeAndType(reg1);
  match &= !reg8.IsValid() || reg8.IsSameSizeAndType(reg1);
  return match;
}


void Immediate::InitializeHandle(Handle<Object> handle) {
  AllowDeferredHandleDereference using_raw_address;

  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  if (obj->IsHeapObject()) {
    DCHECK(!HeapObject::cast(obj)->GetHeap()->InNewSpace(obj));
    value_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    STATIC_ASSERT(sizeof(intptr_t) == sizeof(int64_t));
    value_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE64;
  }
}


bool Operand::NeedsRelocation(const Assembler* assembler) const {
  RelocInfo::Mode rmode = immediate_.rmode();

  if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
    return assembler->serializer_enabled();
  }

  return !RelocInfo::IsNone(rmode);
}


// Constant Pool.
void ConstPool::RecordEntry(intptr_t data,
                            RelocInfo::Mode mode) {
  DCHECK(mode != RelocInfo::COMMENT &&
         mode != RelocInfo::POSITION &&
         mode != RelocInfo::STATEMENT_POSITION &&
         mode != RelocInfo::CONST_POOL &&
         mode != RelocInfo::VENEER_POOL &&
         mode != RelocInfo::CODE_AGE_SEQUENCE &&
         mode != RelocInfo::DEOPT_REASON);
  uint64_t raw_data = static_cast<uint64_t>(data);
  int offset = assm_->pc_offset();
  if (IsEmpty()) {
    first_use_ = offset;
  }

  std::pair<uint64_t, int> entry = std::make_pair(raw_data, offset);
  if (CanBeShared(mode)) {
    shared_entries_.insert(entry);
    if (shared_entries_.count(entry.first) == 1) {
      shared_entries_count++;
    }
  } else {
    unique_entries_.push_back(entry);
  }

  if (EntryCount() > Assembler::kApproxMaxPoolEntryCount) {
    // Request constant pool emission after the next instruction.
    assm_->SetNextConstPoolCheckIn(1);
  }
}


int ConstPool::DistanceToFirstUse() {
  DCHECK(first_use_ >= 0);
  return assm_->pc_offset() - first_use_;
}


int ConstPool::MaxPcOffset() {
  // There are no pending entries in the pool so we can never get out of
  // range.
  if (IsEmpty()) return kMaxInt;

  // Entries are not necessarily emitted in the order they are added so in the
  // worst case the first constant pool use will be accessing the last entry.
  return first_use_ + kMaxLoadLiteralRange - WorstCaseSize();
}


int ConstPool::WorstCaseSize() {
  if (IsEmpty()) return 0;

  // Max size prologue:
  //   b   over
  //   ldr xzr, #pool_size
  //   blr xzr
  //   nop
  // All entries are 64-bit for now.
  return 4 * kInstructionSize + EntryCount() * kPointerSize;
}


int ConstPool::SizeIfEmittedAtCurrentPc(bool require_jump) {
  if (IsEmpty()) return 0;

  // Prologue is:
  //   b   over  ;; if require_jump
  //   ldr xzr, #pool_size
  //   blr xzr
  //   nop       ;; if not 64-bit aligned
  int prologue_size = require_jump ? kInstructionSize : 0;
  prologue_size += 2 * kInstructionSize;
  prologue_size += IsAligned(assm_->pc_offset() + prologue_size, 8) ?
                   0 : kInstructionSize;

  // All entries are 64-bit for now.
  return prologue_size + EntryCount() * kPointerSize;
}


void ConstPool::Emit(bool require_jump) {
  DCHECK(!assm_->is_const_pool_blocked());
  // Prevent recursive pool emission and protect from veneer pools.
  Assembler::BlockPoolsScope block_pools(assm_);

  int size = SizeIfEmittedAtCurrentPc(require_jump);
  Label size_check;
  assm_->bind(&size_check);

  assm_->RecordConstPool(size);
  // Emit the constant pool. It is preceded by an optional branch if
  // require_jump and a header which will:
  //  1) Encode the size of the constant pool, for use by the disassembler.
  //  2) Terminate the program, to try to prevent execution from accidentally
  //     flowing into the constant pool.
  //  3) align the pool entries to 64-bit.
  // The header is therefore made of up to three arm64 instructions:
  //   ldr xzr, #<size of the constant pool in 32-bit words>
  //   blr xzr
  //   nop
  //
  // If executed, the header will likely segfault and lr will point to the
  // instruction following the offending blr.
  // TODO(all): Make the alignment part less fragile. Currently code is
  // allocated as a byte array so there are no guarantees the alignment will
  // be preserved on compaction. Currently it works as allocation seems to be
  // 64-bit aligned.

  // Emit branch if required
  Label after_pool;
  if (require_jump) {
    assm_->b(&after_pool);
  }

  // Emit the header.
  assm_->RecordComment("[ Constant Pool");
  EmitMarker();
  EmitGuard();
  assm_->Align(8);

  // Emit constant pool entries.
  // TODO(all): currently each relocated constant is 64 bits, consider adding
  // support for 32-bit entries.
  EmitEntries();
  assm_->RecordComment("]");

  if (after_pool.is_linked()) {
    assm_->bind(&after_pool);
  }

  DCHECK(assm_->SizeOfCodeGeneratedSince(&size_check) ==
         static_cast<unsigned>(size));
}


void ConstPool::Clear() {
  shared_entries_.clear();
  shared_entries_count = 0;
  unique_entries_.clear();
  first_use_ = -1;
}


bool ConstPool::CanBeShared(RelocInfo::Mode mode) {
  // Constant pool currently does not support 32-bit entries.
  DCHECK(mode != RelocInfo::NONE32);

  return RelocInfo::IsNone(mode) ||
         (!assm_->serializer_enabled() && (mode >= RelocInfo::CELL));
}


void ConstPool::EmitMarker() {
  // A constant pool size is expressed in number of 32-bits words.
  // Currently all entries are 64-bit.
  // + 1 is for the crash guard.
  // + 0/1 for alignment.
  int word_count = EntryCount() * 2 + 1 +
                   (IsAligned(assm_->pc_offset(), 8) ? 0 : 1);
  assm_->Emit(LDR_x_lit                          |
              Assembler::ImmLLiteral(word_count) |
              Assembler::Rt(xzr));
}


MemOperand::PairResult MemOperand::AreConsistentForPair(
    const MemOperand& operandA,
    const MemOperand& operandB,
    int access_size_log2) {
  DCHECK(access_size_log2 >= 0);
  DCHECK(access_size_log2 <= 3);
  // Step one: check that they share the same base, that the mode is Offset
  // and that the offset is a multiple of access size.
  if (!operandA.base().Is(operandB.base()) ||
      (operandA.addrmode() != Offset) ||
      (operandB.addrmode() != Offset) ||
      ((operandA.offset() & ((1 << access_size_log2) - 1)) != 0)) {
    return kNotPair;
  }
  // Step two: check that the offsets are contiguous and that the range
  // is OK for ldp/stp.
  if ((operandB.offset() == operandA.offset() + (1 << access_size_log2)) &&
      is_int7(operandA.offset() >> access_size_log2)) {
    return kPairAB;
  }
  if ((operandA.offset() == operandB.offset() + (1 << access_size_log2)) &&
      is_int7(operandB.offset() >> access_size_log2)) {
    return kPairBA;
  }
  return kNotPair;
}


void ConstPool::EmitGuard() {
#ifdef DEBUG
  Instruction* instr = reinterpret_cast<Instruction*>(assm_->pc());
  DCHECK(instr->preceding()->IsLdrLiteralX() &&
         instr->preceding()->Rt() == xzr.code());
#endif
  assm_->EmitPoolGuard();
}


void ConstPool::EmitEntries() {
  DCHECK(IsAligned(assm_->pc_offset(), 8));

  typedef std::multimap<uint64_t, int>::const_iterator SharedEntriesIterator;
  SharedEntriesIterator value_it;
  // Iterate through the keys (constant pool values).
  for (value_it = shared_entries_.begin();
       value_it != shared_entries_.end();
       value_it = shared_entries_.upper_bound(value_it->first)) {
    std::pair<SharedEntriesIterator, SharedEntriesIterator> range;
    uint64_t data = value_it->first;
    range = shared_entries_.equal_range(data);
    SharedEntriesIterator offset_it;
    // Iterate through the offsets of a given key.
    for (offset_it = range.first; offset_it != range.second; offset_it++) {
      Instruction* instr = assm_->InstructionAt(offset_it->second);

      // Instruction to patch must be 'ldr rd, [pc, #offset]' with offset == 0.
      DCHECK(instr->IsLdrLiteral() && instr->ImmLLiteral() == 0);
      instr->SetImmPCOffsetTarget(assm_->isolate(), assm_->pc());
    }
    assm_->dc64(data);
  }
  shared_entries_.clear();
  shared_entries_count = 0;

  // Emit unique entries.
  std::vector<std::pair<uint64_t, int> >::const_iterator unique_it;
  for (unique_it = unique_entries_.begin();
       unique_it != unique_entries_.end();
       unique_it++) {
    Instruction* instr = assm_->InstructionAt(unique_it->second);

    // Instruction to patch must be 'ldr rd, [pc, #offset]' with offset == 0.
    DCHECK(instr->IsLdrLiteral() && instr->ImmLLiteral() == 0);
    instr->SetImmPCOffsetTarget(assm_->isolate(), assm_->pc());
    assm_->dc64(unique_it->first);
  }
  unique_entries_.clear();
  first_use_ = -1;
}


// Assembler
Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size),
      constpool_(this),
      recorded_ast_id_(TypeFeedbackId::None()),
      unresolved_branches_(),
      positions_recorder_(this) {
  const_pool_blocked_nesting_ = 0;
  veneer_pool_blocked_nesting_ = 0;
  Reset();
}


Assembler::~Assembler() {
  DCHECK(constpool_.IsEmpty());
  DCHECK(const_pool_blocked_nesting_ == 0);
  DCHECK(veneer_pool_blocked_nesting_ == 0);
}


void Assembler::Reset() {
#ifdef DEBUG
  DCHECK((pc_ >= buffer_) && (pc_ < buffer_ + buffer_size_));
  DCHECK(const_pool_blocked_nesting_ == 0);
  DCHECK(veneer_pool_blocked_nesting_ == 0);
  DCHECK(unresolved_branches_.empty());
  memset(buffer_, 0, pc_ - buffer_);
#endif
  pc_ = buffer_;
  reloc_info_writer.Reposition(reinterpret_cast<byte*>(buffer_ + buffer_size_),
                               reinterpret_cast<byte*>(pc_));
  constpool_.Clear();
  next_constant_pool_check_ = 0;
  next_veneer_pool_check_ = kMaxInt;
  no_const_pool_before_ = 0;
  ClearRecordedAstId();
}


void Assembler::GetCode(CodeDesc* desc) {
  reloc_info_writer.Finish();
  // Emit constant pool if necessary.
  CheckConstPool(true, false);
  DCHECK(constpool_.IsEmpty());

  // Set up code descriptor.
  if (desc) {
    desc->buffer = reinterpret_cast<byte*>(buffer_);
    desc->buffer_size = buffer_size_;
    desc->instr_size = pc_offset();
    desc->reloc_size =
        static_cast<int>((reinterpret_cast<byte*>(buffer_) + buffer_size_) -
                         reloc_info_writer.pos());
    desc->origin = this;
    desc->constant_pool_size = 0;
  }
}


void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo32(m));
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::CheckLabelLinkChain(Label const * label) {
#ifdef DEBUG
  if (label->is_linked()) {
    static const int kMaxLinksToCheck = 64;  // Avoid O(n2) behaviour.
    int links_checked = 0;
    int64_t linkoffset = label->pos();
    bool end_of_chain = false;
    while (!end_of_chain) {
      if (++links_checked > kMaxLinksToCheck) break;
      Instruction * link = InstructionAt(linkoffset);
      int64_t linkpcoffset = link->ImmPCOffset();
      int64_t prevlinkoffset = linkoffset + linkpcoffset;

      end_of_chain = (linkoffset == prevlinkoffset);
      linkoffset = linkoffset + linkpcoffset;
    }
  }
#endif
}


void Assembler::RemoveBranchFromLabelLinkChain(Instruction* branch,
                                               Label* label,
                                               Instruction* label_veneer) {
  DCHECK(label->is_linked());

  CheckLabelLinkChain(label);

  Instruction* link = InstructionAt(label->pos());
  Instruction* prev_link = link;
  Instruction* next_link;
  bool end_of_chain = false;

  while (link != branch && !end_of_chain) {
    next_link = link->ImmPCOffsetTarget();
    end_of_chain = (link == next_link);
    prev_link = link;
    link = next_link;
  }

  DCHECK(branch == link);
  next_link = branch->ImmPCOffsetTarget();

  if (branch == prev_link) {
    // The branch is the first instruction in the chain.
    if (branch == next_link) {
      // It is also the last instruction in the chain, so it is the only branch
      // currently referring to this label.
      label->Unuse();
    } else {
      label->link_to(
          static_cast<int>(reinterpret_cast<byte*>(next_link) - buffer_));
    }

  } else if (branch == next_link) {
    // The branch is the last (but not also the first) instruction in the chain.
    prev_link->SetImmPCOffsetTarget(isolate(), prev_link);

  } else {
    // The branch is in the middle of the chain.
    if (prev_link->IsTargetInImmPCOffsetRange(next_link)) {
      prev_link->SetImmPCOffsetTarget(isolate(), next_link);
    } else if (label_veneer != NULL) {
      // Use the veneer for all previous links in the chain.
      prev_link->SetImmPCOffsetTarget(isolate(), prev_link);

      end_of_chain = false;
      link = next_link;
      while (!end_of_chain) {
        next_link = link->ImmPCOffsetTarget();
        end_of_chain = (link == next_link);
        link->SetImmPCOffsetTarget(isolate(), label_veneer);
        link = next_link;
      }
    } else {
      // The assert below will fire.
      // Some other work could be attempted to fix up the chain, but it would be
      // rather complicated. If we crash here, we may want to consider using an
      // other mechanism than a chain of branches.
      //
      // Note that this situation currently should not happen, as we always call
      // this function with a veneer to the target label.
      // However this could happen with a MacroAssembler in the following state:
      //    [previous code]
      //    B(label);
      //    [20KB code]
      //    Tbz(label);   // First tbz. Pointing to unconditional branch.
      //    [20KB code]
      //    Tbz(label);   // Second tbz. Pointing to the first tbz.
      //    [more code]
      // and this function is called to remove the first tbz from the label link
      // chain. Since tbz has a range of +-32KB, the second tbz cannot point to
      // the unconditional branch.
      CHECK(prev_link->IsTargetInImmPCOffsetRange(next_link));
      UNREACHABLE();
    }
  }

  CheckLabelLinkChain(label);
}


void Assembler::bind(Label* label) {
  // Bind label to the address at pc_. All instructions (most likely branches)
  // that are linked to this label will be updated to point to the newly-bound
  // label.

  DCHECK(!label->is_near_linked());
  DCHECK(!label->is_bound());

  DeleteUnresolvedBranchInfoForLabel(label);

  // If the label is linked, the link chain looks something like this:
  //
  // |--I----I-------I-------L
  // |---------------------->| pc_offset
  // |-------------->|         linkoffset = label->pos()
  //         |<------|         link->ImmPCOffset()
  // |------>|                 prevlinkoffset = linkoffset + link->ImmPCOffset()
  //
  // On each iteration, the last link is updated and then removed from the
  // chain until only one remains. At that point, the label is bound.
  //
  // If the label is not linked, no preparation is required before binding.
  while (label->is_linked()) {
    int linkoffset = label->pos();
    Instruction* link = InstructionAt(linkoffset);
    int prevlinkoffset = linkoffset + static_cast<int>(link->ImmPCOffset());

    CheckLabelLinkChain(label);

    DCHECK(linkoffset >= 0);
    DCHECK(linkoffset < pc_offset());
    DCHECK((linkoffset > prevlinkoffset) ||
           (linkoffset - prevlinkoffset == kStartOfLabelLinkChain));
    DCHECK(prevlinkoffset >= 0);

    // Update the link to point to the label.
    if (link->IsUnresolvedInternalReference()) {
      // Internal references do not get patched to an instruction but directly
      // to an address.
      internal_reference_positions_.push_back(linkoffset);
      PatchingAssembler patcher(isolate(), link, 2);
      patcher.dc64(reinterpret_cast<uintptr_t>(pc_));
    } else {
      link->SetImmPCOffsetTarget(isolate(),
                                 reinterpret_cast<Instruction*>(pc_));
    }

    // Link the label to the previous link in the chain.
    if (linkoffset - prevlinkoffset == kStartOfLabelLinkChain) {
      // We hit kStartOfLabelLinkChain, so the chain is fully processed.
      label->Unuse();
    } else {
      // Update the label for the next iteration.
      label->link_to(prevlinkoffset);
    }
  }
  label->bind_to(pc_offset());

  DCHECK(label->is_bound());
  DCHECK(!label->is_linked());
}


int Assembler::LinkAndGetByteOffsetTo(Label* label) {
  DCHECK(sizeof(*pc_) == 1);
  CheckLabelLinkChain(label);

  int offset;
  if (label->is_bound()) {
    // The label is bound, so it does not need to be updated. Referring
    // instructions must link directly to the label as they will not be
    // updated.
    //
    // In this case, label->pos() returns the offset of the label from the
    // start of the buffer.
    //
    // Note that offset can be zero for self-referential instructions. (This
    // could be useful for ADR, for example.)
    offset = label->pos() - pc_offset();
    DCHECK(offset <= 0);
  } else {
    if (label->is_linked()) {
      // The label is linked, so the referring instruction should be added onto
      // the end of the label's link chain.
      //
      // In this case, label->pos() returns the offset of the last linked
      // instruction from the start of the buffer.
      offset = label->pos() - pc_offset();
      DCHECK(offset != kStartOfLabelLinkChain);
      // Note that the offset here needs to be PC-relative only so that the
      // first instruction in a buffer can link to an unbound label. Otherwise,
      // the offset would be 0 for this case, and 0 is reserved for
      // kStartOfLabelLinkChain.
    } else {
      // The label is unused, so it now becomes linked and the referring
      // instruction is at the start of the new link chain.
      offset = kStartOfLabelLinkChain;
    }
    // The instruction at pc is now the last link in the label's chain.
    label->link_to(pc_offset());
  }

  return offset;
}


void Assembler::DeleteUnresolvedBranchInfoForLabelTraverse(Label* label) {
  DCHECK(label->is_linked());
  CheckLabelLinkChain(label);

  int link_offset = label->pos();
  int link_pcoffset;
  bool end_of_chain = false;

  while (!end_of_chain) {
    Instruction * link = InstructionAt(link_offset);
    link_pcoffset = static_cast<int>(link->ImmPCOffset());

    // ADR instructions are not handled by veneers.
    if (link->IsImmBranch()) {
      int max_reachable_pc =
          static_cast<int>(InstructionOffset(link) +
                           Instruction::ImmBranchRange(link->BranchType()));
      typedef std::multimap<int, FarBranchInfo>::iterator unresolved_info_it;
      std::pair<unresolved_info_it, unresolved_info_it> range;
      range = unresolved_branches_.equal_range(max_reachable_pc);
      unresolved_info_it it;
      for (it = range.first; it != range.second; ++it) {
        if (it->second.pc_offset_ == link_offset) {
          unresolved_branches_.erase(it);
          break;
        }
      }
    }

    end_of_chain = (link_pcoffset == 0);
    link_offset = link_offset + link_pcoffset;
  }
}


void Assembler::DeleteUnresolvedBranchInfoForLabel(Label* label) {
  if (unresolved_branches_.empty()) {
    DCHECK(next_veneer_pool_check_ == kMaxInt);
    return;
  }

  if (label->is_linked()) {
    // Branches to this label will be resolved when the label is bound, normally
    // just after all the associated info has been deleted.
    DeleteUnresolvedBranchInfoForLabelTraverse(label);
  }
  if (unresolved_branches_.empty()) {
    next_veneer_pool_check_ = kMaxInt;
  } else {
    next_veneer_pool_check_ =
      unresolved_branches_first_limit() - kVeneerDistanceCheckMargin;
  }
}


void Assembler::StartBlockConstPool() {
  if (const_pool_blocked_nesting_++ == 0) {
    // Prevent constant pool checks happening by setting the next check to
    // the biggest possible offset.
    next_constant_pool_check_ = kMaxInt;
  }
}


void Assembler::EndBlockConstPool() {
  if (--const_pool_blocked_nesting_ == 0) {
    // Check the constant pool hasn't been blocked for too long.
    DCHECK(pc_offset() < constpool_.MaxPcOffset());
    // Two cases:
    //  * no_const_pool_before_ >= next_constant_pool_check_ and the emission is
    //    still blocked
    //  * no_const_pool_before_ < next_constant_pool_check_ and the next emit
    //    will trigger a check.
    next_constant_pool_check_ = no_const_pool_before_;
  }
}


bool Assembler::is_const_pool_blocked() const {
  return (const_pool_blocked_nesting_ > 0) ||
         (pc_offset() < no_const_pool_before_);
}


bool Assembler::IsConstantPoolAt(Instruction* instr) {
  // The constant pool marker is made of two instructions. These instructions
  // will never be emitted by the JIT, so checking for the first one is enough:
  // 0: ldr xzr, #<size of pool>
  bool result = instr->IsLdrLiteralX() && (instr->Rt() == kZeroRegCode);

  // It is still worth asserting the marker is complete.
  // 4: blr xzr
  DCHECK(!result || (instr->following()->IsBranchAndLinkToRegister() &&
                     instr->following()->Rn() == kZeroRegCode));

  return result;
}


int Assembler::ConstantPoolSizeAt(Instruction* instr) {
#ifdef USE_SIMULATOR
  // Assembler::debug() embeds constants directly into the instruction stream.
  // Although this is not a genuine constant pool, treat it like one to avoid
  // disassembling the constants.
  if ((instr->Mask(ExceptionMask) == HLT) &&
      (instr->ImmException() == kImmExceptionIsDebug)) {
    const char* message =
        reinterpret_cast<const char*>(
            instr->InstructionAtOffset(kDebugMessageOffset));
    int size = static_cast<int>(kDebugMessageOffset + strlen(message) + 1);
    return RoundUp(size, kInstructionSize) / kInstructionSize;
  }
  // Same for printf support, see MacroAssembler::CallPrintf().
  if ((instr->Mask(ExceptionMask) == HLT) &&
      (instr->ImmException() == kImmExceptionIsPrintf)) {
    return kPrintfLength / kInstructionSize;
  }
#endif
  if (IsConstantPoolAt(instr)) {
    return instr->ImmLLiteral();
  } else {
    return -1;
  }
}


void Assembler::EmitPoolGuard() {
  // We must generate only one instruction as this is used in scopes that
  // control the size of the code generated.
  Emit(BLR | Rn(xzr));
}


void Assembler::StartBlockVeneerPool() {
  ++veneer_pool_blocked_nesting_;
}


void Assembler::EndBlockVeneerPool() {
  if (--veneer_pool_blocked_nesting_ == 0) {
    // Check the veneer pool hasn't been blocked for too long.
    DCHECK(unresolved_branches_.empty() ||
           (pc_offset() < unresolved_branches_first_limit()));
  }
}


void Assembler::br(const Register& xn) {
  positions_recorder()->WriteRecordedPositions();
  DCHECK(xn.Is64Bits());
  Emit(BR | Rn(xn));
}


void Assembler::blr(const Register& xn) {
  positions_recorder()->WriteRecordedPositions();
  DCHECK(xn.Is64Bits());
  // The pattern 'blr xzr' is used as a guard to detect when execution falls
  // through the constant pool. It should not be emitted.
  DCHECK(!xn.Is(xzr));
  Emit(BLR | Rn(xn));
}


void Assembler::ret(const Register& xn) {
  positions_recorder()->WriteRecordedPositions();
  DCHECK(xn.Is64Bits());
  Emit(RET | Rn(xn));
}


void Assembler::b(int imm26) {
  Emit(B | ImmUncondBranch(imm26));
}


void Assembler::b(Label* label) {
  positions_recorder()->WriteRecordedPositions();
  b(LinkAndGetInstructionOffsetTo(label));
}


void Assembler::b(int imm19, Condition cond) {
  Emit(B_cond | ImmCondBranch(imm19) | cond);
}


void Assembler::b(Label* label, Condition cond) {
  positions_recorder()->WriteRecordedPositions();
  b(LinkAndGetInstructionOffsetTo(label), cond);
}


void Assembler::bl(int imm26) {
  positions_recorder()->WriteRecordedPositions();
  Emit(BL | ImmUncondBranch(imm26));
}


void Assembler::bl(Label* label) {
  positions_recorder()->WriteRecordedPositions();
  bl(LinkAndGetInstructionOffsetTo(label));
}


void Assembler::cbz(const Register& rt,
                    int imm19) {
  positions_recorder()->WriteRecordedPositions();
  Emit(SF(rt) | CBZ | ImmCmpBranch(imm19) | Rt(rt));
}


void Assembler::cbz(const Register& rt,
                    Label* label) {
  positions_recorder()->WriteRecordedPositions();
  cbz(rt, LinkAndGetInstructionOffsetTo(label));
}


void Assembler::cbnz(const Register& rt,
                     int imm19) {
  positions_recorder()->WriteRecordedPositions();
  Emit(SF(rt) | CBNZ | ImmCmpBranch(imm19) | Rt(rt));
}


void Assembler::cbnz(const Register& rt,
                     Label* label) {
  positions_recorder()->WriteRecordedPositions();
  cbnz(rt, LinkAndGetInstructionOffsetTo(label));
}


void Assembler::tbz(const Register& rt,
                    unsigned bit_pos,
                    int imm14) {
  positions_recorder()->WriteRecordedPositions();
  DCHECK(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSizeInBits)));
  Emit(TBZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbz(const Register& rt,
                    unsigned bit_pos,
                    Label* label) {
  positions_recorder()->WriteRecordedPositions();
  tbz(rt, bit_pos, LinkAndGetInstructionOffsetTo(label));
}


void Assembler::tbnz(const Register& rt,
                     unsigned bit_pos,
                     int imm14) {
  positions_recorder()->WriteRecordedPositions();
  DCHECK(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSizeInBits)));
  Emit(TBNZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbnz(const Register& rt,
                     unsigned bit_pos,
                     Label* label) {
  positions_recorder()->WriteRecordedPositions();
  tbnz(rt, bit_pos, LinkAndGetInstructionOffsetTo(label));
}


void Assembler::adr(const Register& rd, int imm21) {
  DCHECK(rd.Is64Bits());
  Emit(ADR | ImmPCRelAddress(imm21) | Rd(rd));
}


void Assembler::adr(const Register& rd, Label* label) {
  adr(rd, LinkAndGetByteOffsetTo(label));
}


void Assembler::add(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  AddSub(rd, rn, operand, LeaveFlags, ADD);
}


void Assembler::adds(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  AddSub(rd, rn, operand, SetFlags, ADD);
}


void Assembler::cmn(const Register& rn,
                    const Operand& operand) {
  Register zr = AppropriateZeroRegFor(rn);
  adds(zr, rn, operand);
}


void Assembler::sub(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  AddSub(rd, rn, operand, LeaveFlags, SUB);
}


void Assembler::subs(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  AddSub(rd, rn, operand, SetFlags, SUB);
}


void Assembler::cmp(const Register& rn, const Operand& operand) {
  Register zr = AppropriateZeroRegFor(rn);
  subs(zr, rn, operand);
}


void Assembler::neg(const Register& rd, const Operand& operand) {
  Register zr = AppropriateZeroRegFor(rd);
  sub(rd, zr, operand);
}


void Assembler::negs(const Register& rd, const Operand& operand) {
  Register zr = AppropriateZeroRegFor(rd);
  subs(rd, zr, operand);
}


void Assembler::adc(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  AddSubWithCarry(rd, rn, operand, LeaveFlags, ADC);
}


void Assembler::adcs(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  AddSubWithCarry(rd, rn, operand, SetFlags, ADC);
}


void Assembler::sbc(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  AddSubWithCarry(rd, rn, operand, LeaveFlags, SBC);
}


void Assembler::sbcs(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  AddSubWithCarry(rd, rn, operand, SetFlags, SBC);
}


void Assembler::ngc(const Register& rd, const Operand& operand) {
  Register zr = AppropriateZeroRegFor(rd);
  sbc(rd, zr, operand);
}


void Assembler::ngcs(const Register& rd, const Operand& operand) {
  Register zr = AppropriateZeroRegFor(rd);
  sbcs(rd, zr, operand);
}


// Logical instructions.
void Assembler::and_(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  Logical(rd, rn, operand, AND);
}


void Assembler::ands(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  Logical(rd, rn, operand, ANDS);
}


void Assembler::tst(const Register& rn,
                    const Operand& operand) {
  ands(AppropriateZeroRegFor(rn), rn, operand);
}


void Assembler::bic(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  Logical(rd, rn, operand, BIC);
}


void Assembler::bics(const Register& rd,
                     const Register& rn,
                     const Operand& operand) {
  Logical(rd, rn, operand, BICS);
}


void Assembler::orr(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  Logical(rd, rn, operand, ORR);
}


void Assembler::orn(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  Logical(rd, rn, operand, ORN);
}


void Assembler::eor(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  Logical(rd, rn, operand, EOR);
}


void Assembler::eon(const Register& rd,
                    const Register& rn,
                    const Operand& operand) {
  Logical(rd, rn, operand, EON);
}


void Assembler::lslv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | LSLV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::lsrv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | LSRV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::asrv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | ASRV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::rorv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | RORV | Rm(rm) | Rn(rn) | Rd(rd));
}


// Bitfield operations.
void Assembler::bfm(const Register& rd, const Register& rn, int immr,
                    int imms) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | BFM | N |
       ImmR(immr, rd.SizeInBits()) |
       ImmS(imms, rn.SizeInBits()) |
       Rn(rn) | Rd(rd));
}


void Assembler::sbfm(const Register& rd, const Register& rn, int immr,
                     int imms) {
  DCHECK(rd.Is64Bits() || rn.Is32Bits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | SBFM | N |
       ImmR(immr, rd.SizeInBits()) |
       ImmS(imms, rn.SizeInBits()) |
       Rn(rn) | Rd(rd));
}


void Assembler::ubfm(const Register& rd, const Register& rn, int immr,
                     int imms) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | UBFM | N |
       ImmR(immr, rd.SizeInBits()) |
       ImmS(imms, rn.SizeInBits()) |
       Rn(rn) | Rd(rd));
}


void Assembler::extr(const Register& rd, const Register& rn, const Register& rm,
                     int lsb) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | EXTR | N | Rm(rm) |
       ImmS(lsb, rn.SizeInBits()) | Rn(rn) | Rd(rd));
}


void Assembler::csel(const Register& rd,
                     const Register& rn,
                     const Register& rm,
                     Condition cond) {
  ConditionalSelect(rd, rn, rm, cond, CSEL);
}


void Assembler::csinc(const Register& rd,
                      const Register& rn,
                      const Register& rm,
                      Condition cond) {
  ConditionalSelect(rd, rn, rm, cond, CSINC);
}


void Assembler::csinv(const Register& rd,
                      const Register& rn,
                      const Register& rm,
                      Condition cond) {
  ConditionalSelect(rd, rn, rm, cond, CSINV);
}


void Assembler::csneg(const Register& rd,
                      const Register& rn,
                      const Register& rm,
                      Condition cond) {
  ConditionalSelect(rd, rn, rm, cond, CSNEG);
}


void Assembler::cset(const Register &rd, Condition cond) {
  DCHECK((cond != al) && (cond != nv));
  Register zr = AppropriateZeroRegFor(rd);
  csinc(rd, zr, zr, NegateCondition(cond));
}


void Assembler::csetm(const Register &rd, Condition cond) {
  DCHECK((cond != al) && (cond != nv));
  Register zr = AppropriateZeroRegFor(rd);
  csinv(rd, zr, zr, NegateCondition(cond));
}


void Assembler::cinc(const Register &rd, const Register &rn, Condition cond) {
  DCHECK((cond != al) && (cond != nv));
  csinc(rd, rn, rn, NegateCondition(cond));
}


void Assembler::cinv(const Register &rd, const Register &rn, Condition cond) {
  DCHECK((cond != al) && (cond != nv));
  csinv(rd, rn, rn, NegateCondition(cond));
}


void Assembler::cneg(const Register &rd, const Register &rn, Condition cond) {
  DCHECK((cond != al) && (cond != nv));
  csneg(rd, rn, rn, NegateCondition(cond));
}


void Assembler::ConditionalSelect(const Register& rd,
                                  const Register& rn,
                                  const Register& rm,
                                  Condition cond,
                                  ConditionalSelectOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | op | Rm(rm) | Cond(cond) | Rn(rn) | Rd(rd));
}


void Assembler::ccmn(const Register& rn,
                     const Operand& operand,
                     StatusFlags nzcv,
                     Condition cond) {
  ConditionalCompare(rn, operand, nzcv, cond, CCMN);
}


void Assembler::ccmp(const Register& rn,
                     const Operand& operand,
                     StatusFlags nzcv,
                     Condition cond) {
  ConditionalCompare(rn, operand, nzcv, cond, CCMP);
}


void Assembler::DataProcessing3Source(const Register& rd,
                                      const Register& rn,
                                      const Register& rm,
                                      const Register& ra,
                                      DataProcessing3SourceOp op) {
  Emit(SF(rd) | op | Rm(rm) | Ra(ra) | Rn(rn) | Rd(rd));
}


void Assembler::mul(const Register& rd,
                    const Register& rn,
                    const Register& rm) {
  DCHECK(AreSameSizeAndType(rd, rn, rm));
  Register zr = AppropriateZeroRegFor(rn);
  DataProcessing3Source(rd, rn, rm, zr, MADD);
}


void Assembler::madd(const Register& rd,
                     const Register& rn,
                     const Register& rm,
                     const Register& ra) {
  DCHECK(AreSameSizeAndType(rd, rn, rm, ra));
  DataProcessing3Source(rd, rn, rm, ra, MADD);
}


void Assembler::mneg(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(AreSameSizeAndType(rd, rn, rm));
  Register zr = AppropriateZeroRegFor(rn);
  DataProcessing3Source(rd, rn, rm, zr, MSUB);
}


void Assembler::msub(const Register& rd,
                     const Register& rn,
                     const Register& rm,
                     const Register& ra) {
  DCHECK(AreSameSizeAndType(rd, rn, rm, ra));
  DataProcessing3Source(rd, rn, rm, ra, MSUB);
}


void Assembler::smaddl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  DCHECK(rd.Is64Bits() && ra.Is64Bits());
  DCHECK(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, SMADDL_x);
}


void Assembler::smsubl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  DCHECK(rd.Is64Bits() && ra.Is64Bits());
  DCHECK(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, SMSUBL_x);
}


void Assembler::umaddl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  DCHECK(rd.Is64Bits() && ra.Is64Bits());
  DCHECK(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, UMADDL_x);
}


void Assembler::umsubl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  DCHECK(rd.Is64Bits() && ra.Is64Bits());
  DCHECK(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, UMSUBL_x);
}


void Assembler::smull(const Register& rd,
                      const Register& rn,
                      const Register& rm) {
  DCHECK(rd.Is64Bits());
  DCHECK(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, xzr, SMADDL_x);
}


void Assembler::smulh(const Register& rd,
                      const Register& rn,
                      const Register& rm) {
  DCHECK(AreSameSizeAndType(rd, rn, rm));
  DataProcessing3Source(rd, rn, rm, xzr, SMULH_x);
}


void Assembler::sdiv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | SDIV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::udiv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | UDIV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::rbit(const Register& rd,
                     const Register& rn) {
  DataProcessing1Source(rd, rn, RBIT);
}


void Assembler::rev16(const Register& rd,
                      const Register& rn) {
  DataProcessing1Source(rd, rn, REV16);
}


void Assembler::rev32(const Register& rd,
                      const Register& rn) {
  DCHECK(rd.Is64Bits());
  DataProcessing1Source(rd, rn, REV);
}


void Assembler::rev(const Register& rd,
                    const Register& rn) {
  DataProcessing1Source(rd, rn, rd.Is64Bits() ? REV_x : REV_w);
}


void Assembler::clz(const Register& rd,
                    const Register& rn) {
  DataProcessing1Source(rd, rn, CLZ);
}


void Assembler::cls(const Register& rd,
                    const Register& rn) {
  DataProcessing1Source(rd, rn, CLS);
}


void Assembler::ldp(const CPURegister& rt,
                    const CPURegister& rt2,
                    const MemOperand& src) {
  LoadStorePair(rt, rt2, src, LoadPairOpFor(rt, rt2));
}


void Assembler::stp(const CPURegister& rt,
                    const CPURegister& rt2,
                    const MemOperand& dst) {
  LoadStorePair(rt, rt2, dst, StorePairOpFor(rt, rt2));
}


void Assembler::ldpsw(const Register& rt,
                      const Register& rt2,
                      const MemOperand& src) {
  DCHECK(rt.Is64Bits());
  LoadStorePair(rt, rt2, src, LDPSW_x);
}


void Assembler::LoadStorePair(const CPURegister& rt,
                              const CPURegister& rt2,
                              const MemOperand& addr,
                              LoadStorePairOp op) {
  // 'rt' and 'rt2' can only be aliased for stores.
  DCHECK(((op & LoadStorePairLBit) == 0) || !rt.Is(rt2));
  DCHECK(AreSameSizeAndType(rt, rt2));
  DCHECK(IsImmLSPair(addr.offset(), CalcLSPairDataSize(op)));
  int offset = static_cast<int>(addr.offset());

  Instr memop = op | Rt(rt) | Rt2(rt2) | RnSP(addr.base()) |
                ImmLSPair(offset, CalcLSPairDataSize(op));

  Instr addrmodeop;
  if (addr.IsImmediateOffset()) {
    addrmodeop = LoadStorePairOffsetFixed;
  } else {
    // Pre-index and post-index modes.
    DCHECK(!rt.Is(addr.base()));
    DCHECK(!rt2.Is(addr.base()));
    DCHECK(addr.offset() != 0);
    if (addr.IsPreIndex()) {
      addrmodeop = LoadStorePairPreIndexFixed;
    } else {
      DCHECK(addr.IsPostIndex());
      addrmodeop = LoadStorePairPostIndexFixed;
    }
  }
  Emit(addrmodeop | memop);
}


// Memory instructions.
void Assembler::ldrb(const Register& rt, const MemOperand& src) {
  LoadStore(rt, src, LDRB_w);
}


void Assembler::strb(const Register& rt, const MemOperand& dst) {
  LoadStore(rt, dst, STRB_w);
}


void Assembler::ldrsb(const Register& rt, const MemOperand& src) {
  LoadStore(rt, src, rt.Is64Bits() ? LDRSB_x : LDRSB_w);
}


void Assembler::ldrh(const Register& rt, const MemOperand& src) {
  LoadStore(rt, src, LDRH_w);
}


void Assembler::strh(const Register& rt, const MemOperand& dst) {
  LoadStore(rt, dst, STRH_w);
}


void Assembler::ldrsh(const Register& rt, const MemOperand& src) {
  LoadStore(rt, src, rt.Is64Bits() ? LDRSH_x : LDRSH_w);
}


void Assembler::ldr(const CPURegister& rt, const MemOperand& src) {
  LoadStore(rt, src, LoadOpFor(rt));
}


void Assembler::str(const CPURegister& rt, const MemOperand& src) {
  LoadStore(rt, src, StoreOpFor(rt));
}


void Assembler::ldrsw(const Register& rt, const MemOperand& src) {
  DCHECK(rt.Is64Bits());
  LoadStore(rt, src, LDRSW_x);
}


void Assembler::ldr_pcrel(const CPURegister& rt, int imm19) {
  // The pattern 'ldr xzr, #offset' is used to indicate the beginning of a
  // constant pool. It should not be emitted.
  DCHECK(!rt.IsZero());
  Emit(LoadLiteralOpFor(rt) | ImmLLiteral(imm19) | Rt(rt));
}


void Assembler::ldr(const CPURegister& rt, const Immediate& imm) {
  // Currently we only support 64-bit literals.
  DCHECK(rt.Is64Bits());

  RecordRelocInfo(imm.rmode(), imm.value());
  BlockConstPoolFor(1);
  // The load will be patched when the constpool is emitted, patching code
  // expect a load literal with offset 0.
  ldr_pcrel(rt, 0);
}


void Assembler::mov(const Register& rd, const Register& rm) {
  // Moves involving the stack pointer are encoded as add immediate with
  // second operand of zero. Otherwise, orr with first operand zr is
  // used.
  if (rd.IsSP() || rm.IsSP()) {
    add(rd, rm, 0);
  } else {
    orr(rd, AppropriateZeroRegFor(rd), rm);
  }
}


void Assembler::mvn(const Register& rd, const Operand& operand) {
  orn(rd, AppropriateZeroRegFor(rd), operand);
}


void Assembler::mrs(const Register& rt, SystemRegister sysreg) {
  DCHECK(rt.Is64Bits());
  Emit(MRS | ImmSystemRegister(sysreg) | Rt(rt));
}


void Assembler::msr(SystemRegister sysreg, const Register& rt) {
  DCHECK(rt.Is64Bits());
  Emit(MSR | Rt(rt) | ImmSystemRegister(sysreg));
}


void Assembler::hint(SystemHint code) {
  Emit(HINT | ImmHint(code) | Rt(xzr));
}


void Assembler::dmb(BarrierDomain domain, BarrierType type) {
  Emit(DMB | ImmBarrierDomain(domain) | ImmBarrierType(type));
}


void Assembler::dsb(BarrierDomain domain, BarrierType type) {
  Emit(DSB | ImmBarrierDomain(domain) | ImmBarrierType(type));
}


void Assembler::isb() {
  Emit(ISB | ImmBarrierDomain(FullSystem) | ImmBarrierType(BarrierAll));
}


void Assembler::fmov(FPRegister fd, double imm) {
  DCHECK(fd.Is64Bits());
  DCHECK(IsImmFP64(imm));
  Emit(FMOV_d_imm | Rd(fd) | ImmFP64(imm));
}


void Assembler::fmov(FPRegister fd, float imm) {
  DCHECK(fd.Is32Bits());
  DCHECK(IsImmFP32(imm));
  Emit(FMOV_s_imm | Rd(fd) | ImmFP32(imm));
}


void Assembler::fmov(Register rd, FPRegister fn) {
  DCHECK(rd.SizeInBits() == fn.SizeInBits());
  FPIntegerConvertOp op = rd.Is32Bits() ? FMOV_ws : FMOV_xd;
  Emit(op | Rd(rd) | Rn(fn));
}


void Assembler::fmov(FPRegister fd, Register rn) {
  DCHECK(fd.SizeInBits() == rn.SizeInBits());
  FPIntegerConvertOp op = fd.Is32Bits() ? FMOV_sw : FMOV_dx;
  Emit(op | Rd(fd) | Rn(rn));
}


void Assembler::fmov(FPRegister fd, FPRegister fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  Emit(FPType(fd) | FMOV | Rd(fd) | Rn(fn));
}


void Assembler::fadd(const FPRegister& fd,
                     const FPRegister& fn,
                     const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FADD);
}


void Assembler::fsub(const FPRegister& fd,
                     const FPRegister& fn,
                     const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FSUB);
}


void Assembler::fmul(const FPRegister& fd,
                     const FPRegister& fn,
                     const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FMUL);
}


void Assembler::fmadd(const FPRegister& fd,
                      const FPRegister& fn,
                      const FPRegister& fm,
                      const FPRegister& fa) {
  FPDataProcessing3Source(fd, fn, fm, fa, fd.Is32Bits() ? FMADD_s : FMADD_d);
}


void Assembler::fmsub(const FPRegister& fd,
                      const FPRegister& fn,
                      const FPRegister& fm,
                      const FPRegister& fa) {
  FPDataProcessing3Source(fd, fn, fm, fa, fd.Is32Bits() ? FMSUB_s : FMSUB_d);
}


void Assembler::fnmadd(const FPRegister& fd,
                       const FPRegister& fn,
                       const FPRegister& fm,
                       const FPRegister& fa) {
  FPDataProcessing3Source(fd, fn, fm, fa, fd.Is32Bits() ? FNMADD_s : FNMADD_d);
}


void Assembler::fnmsub(const FPRegister& fd,
                       const FPRegister& fn,
                       const FPRegister& fm,
                       const FPRegister& fa) {
  FPDataProcessing3Source(fd, fn, fm, fa, fd.Is32Bits() ? FNMSUB_s : FNMSUB_d);
}


void Assembler::fdiv(const FPRegister& fd,
                     const FPRegister& fn,
                     const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FDIV);
}


void Assembler::fmax(const FPRegister& fd,
                     const FPRegister& fn,
                     const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FMAX);
}


void Assembler::fmaxnm(const FPRegister& fd,
                       const FPRegister& fn,
                       const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FMAXNM);
}


void Assembler::fmin(const FPRegister& fd,
                     const FPRegister& fn,
                     const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FMIN);
}


void Assembler::fminnm(const FPRegister& fd,
                       const FPRegister& fn,
                       const FPRegister& fm) {
  FPDataProcessing2Source(fd, fn, fm, FMINNM);
}


void Assembler::fabs(const FPRegister& fd,
                     const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FABS);
}


void Assembler::fneg(const FPRegister& fd,
                     const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FNEG);
}


void Assembler::fsqrt(const FPRegister& fd,
                      const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FSQRT);
}


void Assembler::frinta(const FPRegister& fd,
                       const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTA);
}


void Assembler::frintm(const FPRegister& fd,
                       const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTM);
}


void Assembler::frintn(const FPRegister& fd,
                       const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTN);
}


void Assembler::frintp(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTP);
}


void Assembler::frintz(const FPRegister& fd,
                       const FPRegister& fn) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTZ);
}


void Assembler::fcmp(const FPRegister& fn,
                     const FPRegister& fm) {
  DCHECK(fn.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fn) | FCMP | Rm(fm) | Rn(fn));
}


void Assembler::fcmp(const FPRegister& fn,
                     double value) {
  USE(value);
  // Although the fcmp instruction can strictly only take an immediate value of
  // +0.0, we don't need to check for -0.0 because the sign of 0.0 doesn't
  // affect the result of the comparison.
  DCHECK(value == 0.0);
  Emit(FPType(fn) | FCMP_zero | Rn(fn));
}


void Assembler::fccmp(const FPRegister& fn,
                      const FPRegister& fm,
                      StatusFlags nzcv,
                      Condition cond) {
  DCHECK(fn.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fn) | FCCMP | Rm(fm) | Cond(cond) | Rn(fn) | Nzcv(nzcv));
}


void Assembler::fcsel(const FPRegister& fd,
                      const FPRegister& fn,
                      const FPRegister& fm,
                      Condition cond) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  DCHECK(fd.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fd) | FCSEL | Rm(fm) | Cond(cond) | Rn(fn) | Rd(fd));
}


void Assembler::FPConvertToInt(const Register& rd,
                               const FPRegister& fn,
                               FPIntegerConvertOp op) {
  Emit(SF(rd) | FPType(fn) | op | Rn(fn) | Rd(rd));
}


void Assembler::fcvt(const FPRegister& fd,
                     const FPRegister& fn) {
  if (fd.Is64Bits()) {
    // Convert float to double.
    DCHECK(fn.Is32Bits());
    FPDataProcessing1Source(fd, fn, FCVT_ds);
  } else {
    // Convert double to float.
    DCHECK(fn.Is64Bits());
    FPDataProcessing1Source(fd, fn, FCVT_sd);
  }
}


void Assembler::fcvtau(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTAU);
}


void Assembler::fcvtas(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTAS);
}


void Assembler::fcvtmu(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTMU);
}


void Assembler::fcvtms(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTMS);
}


void Assembler::fcvtnu(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTNU);
}


void Assembler::fcvtns(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTNS);
}


void Assembler::fcvtzu(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTZU);
}


void Assembler::fcvtzs(const Register& rd, const FPRegister& fn) {
  FPConvertToInt(rd, fn, FCVTZS);
}


void Assembler::scvtf(const FPRegister& fd,
                      const Register& rn,
                      unsigned fbits) {
  if (fbits == 0) {
    Emit(SF(rn) | FPType(fd) | SCVTF | Rn(rn) | Rd(fd));
  } else {
    Emit(SF(rn) | FPType(fd) | SCVTF_fixed | FPScale(64 - fbits) | Rn(rn) |
         Rd(fd));
  }
}


void Assembler::ucvtf(const FPRegister& fd,
                      const Register& rn,
                      unsigned fbits) {
  if (fbits == 0) {
    Emit(SF(rn) | FPType(fd) | UCVTF | Rn(rn) | Rd(fd));
  } else {
    Emit(SF(rn) | FPType(fd) | UCVTF_fixed | FPScale(64 - fbits) | Rn(rn) |
         Rd(fd));
  }
}


void Assembler::dcptr(Label* label) {
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  if (label->is_bound()) {
    // The label is bound, so it does not need to be updated and the internal
    // reference should be emitted.
    //
    // In this case, label->pos() returns the offset of the label from the
    // start of the buffer.
    internal_reference_positions_.push_back(pc_offset());
    dc64(reinterpret_cast<uintptr_t>(buffer_ + label->pos()));
  } else {
    int32_t offset;
    if (label->is_linked()) {
      // The label is linked, so the internal reference should be added
      // onto the end of the label's link chain.
      //
      // In this case, label->pos() returns the offset of the last linked
      // instruction from the start of the buffer.
      offset = label->pos() - pc_offset();
      DCHECK(offset != kStartOfLabelLinkChain);
    } else {
      // The label is unused, so it now becomes linked and the internal
      // reference is at the start of the new link chain.
      offset = kStartOfLabelLinkChain;
    }
    // The instruction at pc is now the last link in the label's chain.
    label->link_to(pc_offset());

    // Traditionally the offset to the previous instruction in the chain is
    // encoded in the instruction payload (e.g. branch range) but internal
    // references are not instructions so while unbound they are encoded as
    // two consecutive brk instructions. The two 16-bit immediates are used
    // to encode the offset.
    offset >>= kInstructionSizeLog2;
    DCHECK(is_int32(offset));
    uint32_t high16 = unsigned_bitextract_32(31, 16, offset);
    uint32_t low16 = unsigned_bitextract_32(15, 0, offset);

    brk(high16);
    brk(low16);
  }
}


// Note:
// Below, a difference in case for the same letter indicates a
// negated bit.
// If b is 1, then B is 0.
Instr Assembler::ImmFP32(float imm) {
  DCHECK(IsImmFP32(imm));
  // bits: aBbb.bbbc.defg.h000.0000.0000.0000.0000
  uint32_t bits = float_to_rawbits(imm);
  // bit7: a000.0000
  uint32_t bit7 = ((bits >> 31) & 0x1) << 7;
  // bit6: 0b00.0000
  uint32_t bit6 = ((bits >> 29) & 0x1) << 6;
  // bit5_to_0: 00cd.efgh
  uint32_t bit5_to_0 = (bits >> 19) & 0x3f;

  return (bit7 | bit6 | bit5_to_0) << ImmFP_offset;
}


Instr Assembler::ImmFP64(double imm) {
  DCHECK(IsImmFP64(imm));
  // bits: aBbb.bbbb.bbcd.efgh.0000.0000.0000.0000
  //       0000.0000.0000.0000.0000.0000.0000.0000
  uint64_t bits = double_to_rawbits(imm);
  // bit7: a000.0000
  uint64_t bit7 = ((bits >> 63) & 0x1) << 7;
  // bit6: 0b00.0000
  uint64_t bit6 = ((bits >> 61) & 0x1) << 6;
  // bit5_to_0: 00cd.efgh
  uint64_t bit5_to_0 = (bits >> 48) & 0x3f;

  return static_cast<Instr>((bit7 | bit6 | bit5_to_0) << ImmFP_offset);
}


// Code generation helpers.
void Assembler::MoveWide(const Register& rd,
                         uint64_t imm,
                         int shift,
                         MoveWideImmediateOp mov_op) {
  // Ignore the top 32 bits of an immediate if we're moving to a W register.
  if (rd.Is32Bits()) {
    // Check that the top 32 bits are zero (a positive 32-bit number) or top
    // 33 bits are one (a negative 32-bit number, sign extended to 64 bits).
    DCHECK(((imm >> kWRegSizeInBits) == 0) ||
           ((imm >> (kWRegSizeInBits - 1)) == 0x1ffffffff));
    imm &= kWRegMask;
  }

  if (shift >= 0) {
    // Explicit shift specified.
    DCHECK((shift == 0) || (shift == 16) || (shift == 32) || (shift == 48));
    DCHECK(rd.Is64Bits() || (shift == 0) || (shift == 16));
    shift /= 16;
  } else {
    // Calculate a new immediate and shift combination to encode the immediate
    // argument.
    shift = 0;
    if ((imm & ~0xffffUL) == 0) {
      // Nothing to do.
    } else if ((imm & ~(0xffffUL << 16)) == 0) {
      imm >>= 16;
      shift = 1;
    } else if ((imm & ~(0xffffUL << 32)) == 0) {
      DCHECK(rd.Is64Bits());
      imm >>= 32;
      shift = 2;
    } else if ((imm & ~(0xffffUL << 48)) == 0) {
      DCHECK(rd.Is64Bits());
      imm >>= 48;
      shift = 3;
    }
  }

  DCHECK(is_uint16(imm));

  Emit(SF(rd) | MoveWideImmediateFixed | mov_op | Rd(rd) |
       ImmMoveWide(static_cast<int>(imm)) | ShiftMoveWide(shift));
}


void Assembler::AddSub(const Register& rd,
                       const Register& rn,
                       const Operand& operand,
                       FlagsUpdate S,
                       AddSubOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(!operand.NeedsRelocation(this));
  if (operand.IsImmediate()) {
    int64_t immediate = operand.ImmediateValue();
    DCHECK(IsImmAddSub(immediate));
    Instr dest_reg = (S == SetFlags) ? Rd(rd) : RdSP(rd);
    Emit(SF(rd) | AddSubImmediateFixed | op | Flags(S) |
         ImmAddSub(static_cast<int>(immediate)) | dest_reg | RnSP(rn));
  } else if (operand.IsShiftedRegister()) {
    DCHECK(operand.reg().SizeInBits() == rd.SizeInBits());
    DCHECK(operand.shift() != ROR);

    // For instructions of the form:
    //   add/sub   wsp, <Wn>, <Wm> [, LSL #0-3 ]
    //   add/sub   <Wd>, wsp, <Wm> [, LSL #0-3 ]
    //   add/sub   wsp, wsp, <Wm> [, LSL #0-3 ]
    //   adds/subs <Wd>, wsp, <Wm> [, LSL #0-3 ]
    // or their 64-bit register equivalents, convert the operand from shifted to
    // extended register mode, and emit an add/sub extended instruction.
    if (rn.IsSP() || rd.IsSP()) {
      DCHECK(!(rd.IsSP() && (S == SetFlags)));
      DataProcExtendedRegister(rd, rn, operand.ToExtendedRegister(), S,
                               AddSubExtendedFixed | op);
    } else {
      DataProcShiftedRegister(rd, rn, operand, S, AddSubShiftedFixed | op);
    }
  } else {
    DCHECK(operand.IsExtendedRegister());
    DataProcExtendedRegister(rd, rn, operand, S, AddSubExtendedFixed | op);
  }
}


void Assembler::AddSubWithCarry(const Register& rd,
                                const Register& rn,
                                const Operand& operand,
                                FlagsUpdate S,
                                AddSubWithCarryOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(rd.SizeInBits() == operand.reg().SizeInBits());
  DCHECK(operand.IsShiftedRegister() && (operand.shift_amount() == 0));
  DCHECK(!operand.NeedsRelocation(this));
  Emit(SF(rd) | op | Flags(S) | Rm(operand.reg()) | Rn(rn) | Rd(rd));
}


void Assembler::hlt(int code) {
  DCHECK(is_uint16(code));
  Emit(HLT | ImmException(code));
}


void Assembler::brk(int code) {
  DCHECK(is_uint16(code));
  Emit(BRK | ImmException(code));
}


void Assembler::EmitStringData(const char* string) {
  size_t len = strlen(string) + 1;
  DCHECK(RoundUp(len, kInstructionSize) <= static_cast<size_t>(kGap));
  EmitData(string, static_cast<int>(len));
  // Pad with NULL characters until pc_ is aligned.
  const char pad[] = {'\0', '\0', '\0', '\0'};
  STATIC_ASSERT(sizeof(pad) == kInstructionSize);
  EmitData(pad, RoundUp(pc_offset(), kInstructionSize) - pc_offset());
}


void Assembler::debug(const char* message, uint32_t code, Instr params) {
#ifdef USE_SIMULATOR
  // Don't generate simulator specific code if we are building a snapshot, which
  // might be run on real hardware.
  if (!serializer_enabled()) {
    // The arguments to the debug marker need to be contiguous in memory, so
    // make sure we don't try to emit pools.
    BlockPoolsScope scope(this);

    Label start;
    bind(&start);

    // Refer to instructions-arm64.h for a description of the marker and its
    // arguments.
    hlt(kImmExceptionIsDebug);
    DCHECK(SizeOfCodeGeneratedSince(&start) == kDebugCodeOffset);
    dc32(code);
    DCHECK(SizeOfCodeGeneratedSince(&start) == kDebugParamsOffset);
    dc32(params);
    DCHECK(SizeOfCodeGeneratedSince(&start) == kDebugMessageOffset);
    EmitStringData(message);
    hlt(kImmExceptionIsUnreachable);

    return;
  }
  // Fall through if Serializer is enabled.
#endif

  if (params & BREAK) {
    hlt(kImmExceptionIsDebug);
  }
}


void Assembler::Logical(const Register& rd,
                        const Register& rn,
                        const Operand& operand,
                        LogicalOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  DCHECK(!operand.NeedsRelocation(this));
  if (operand.IsImmediate()) {
    int64_t immediate = operand.ImmediateValue();
    unsigned reg_size = rd.SizeInBits();

    DCHECK(immediate != 0);
    DCHECK(immediate != -1);
    DCHECK(rd.Is64Bits() || is_uint32(immediate));

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = rd.Is64Bits() ? ~immediate : (~immediate & kWRegMask);
    }

    unsigned n, imm_s, imm_r;
    if (IsImmLogical(immediate, reg_size, &n, &imm_s, &imm_r)) {
      // Immediate can be encoded in the instruction.
      LogicalImmediate(rd, rn, n, imm_s, imm_r, op);
    } else {
      // This case is handled in the macro assembler.
      UNREACHABLE();
    }
  } else {
    DCHECK(operand.IsShiftedRegister());
    DCHECK(operand.reg().SizeInBits() == rd.SizeInBits());
    Instr dp_op = static_cast<Instr>(op | LogicalShiftedFixed);
    DataProcShiftedRegister(rd, rn, operand, LeaveFlags, dp_op);
  }
}


void Assembler::LogicalImmediate(const Register& rd,
                                 const Register& rn,
                                 unsigned n,
                                 unsigned imm_s,
                                 unsigned imm_r,
                                 LogicalOp op) {
  unsigned reg_size = rd.SizeInBits();
  Instr dest_reg = (op == ANDS) ? Rd(rd) : RdSP(rd);
  Emit(SF(rd) | LogicalImmediateFixed | op | BitN(n, reg_size) |
       ImmSetBits(imm_s, reg_size) | ImmRotate(imm_r, reg_size) | dest_reg |
       Rn(rn));
}


void Assembler::ConditionalCompare(const Register& rn,
                                   const Operand& operand,
                                   StatusFlags nzcv,
                                   Condition cond,
                                   ConditionalCompareOp op) {
  Instr ccmpop;
  DCHECK(!operand.NeedsRelocation(this));
  if (operand.IsImmediate()) {
    int64_t immediate = operand.ImmediateValue();
    DCHECK(IsImmConditionalCompare(immediate));
    ccmpop = ConditionalCompareImmediateFixed | op |
             ImmCondCmp(static_cast<unsigned>(immediate));
  } else {
    DCHECK(operand.IsShiftedRegister() && (operand.shift_amount() == 0));
    ccmpop = ConditionalCompareRegisterFixed | op | Rm(operand.reg());
  }
  Emit(SF(rn) | ccmpop | Cond(cond) | Rn(rn) | Nzcv(nzcv));
}


void Assembler::DataProcessing1Source(const Register& rd,
                                      const Register& rn,
                                      DataProcessing1SourceOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  Emit(SF(rn) | op | Rn(rn) | Rd(rd));
}


void Assembler::FPDataProcessing1Source(const FPRegister& fd,
                                        const FPRegister& fn,
                                        FPDataProcessing1SourceOp op) {
  Emit(FPType(fn) | op | Rn(fn) | Rd(fd));
}


void Assembler::FPDataProcessing2Source(const FPRegister& fd,
                                        const FPRegister& fn,
                                        const FPRegister& fm,
                                        FPDataProcessing2SourceOp op) {
  DCHECK(fd.SizeInBits() == fn.SizeInBits());
  DCHECK(fd.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fd) | op | Rm(fm) | Rn(fn) | Rd(fd));
}


void Assembler::FPDataProcessing3Source(const FPRegister& fd,
                                        const FPRegister& fn,
                                        const FPRegister& fm,
                                        const FPRegister& fa,
                                        FPDataProcessing3SourceOp op) {
  DCHECK(AreSameSizeAndType(fd, fn, fm, fa));
  Emit(FPType(fd) | op | Rm(fm) | Rn(fn) | Rd(fd) | Ra(fa));
}


void Assembler::EmitShift(const Register& rd,
                          const Register& rn,
                          Shift shift,
                          unsigned shift_amount) {
  switch (shift) {
    case LSL:
      lsl(rd, rn, shift_amount);
      break;
    case LSR:
      lsr(rd, rn, shift_amount);
      break;
    case ASR:
      asr(rd, rn, shift_amount);
      break;
    case ROR:
      ror(rd, rn, shift_amount);
      break;
    default:
      UNREACHABLE();
  }
}


void Assembler::EmitExtendShift(const Register& rd,
                                const Register& rn,
                                Extend extend,
                                unsigned left_shift) {
  DCHECK(rd.SizeInBits() >= rn.SizeInBits());
  unsigned reg_size = rd.SizeInBits();
  // Use the correct size of register.
  Register rn_ = Register::Create(rn.code(), rd.SizeInBits());
  // Bits extracted are high_bit:0.
  unsigned high_bit = (8 << (extend & 0x3)) - 1;
  // Number of bits left in the result that are not introduced by the shift.
  unsigned non_shift_bits = (reg_size - left_shift) & (reg_size - 1);

  if ((non_shift_bits > high_bit) || (non_shift_bits == 0)) {
    switch (extend) {
      case UXTB:
      case UXTH:
      case UXTW: ubfm(rd, rn_, non_shift_bits, high_bit); break;
      case SXTB:
      case SXTH:
      case SXTW: sbfm(rd, rn_, non_shift_bits, high_bit); break;
      case UXTX:
      case SXTX: {
        DCHECK(rn.SizeInBits() == kXRegSizeInBits);
        // Nothing to extend. Just shift.
        lsl(rd, rn_, left_shift);
        break;
      }
      default: UNREACHABLE();
    }
  } else {
    // No need to extend as the extended bits would be shifted away.
    lsl(rd, rn_, left_shift);
  }
}


void Assembler::DataProcShiftedRegister(const Register& rd,
                                        const Register& rn,
                                        const Operand& operand,
                                        FlagsUpdate S,
                                        Instr op) {
  DCHECK(operand.IsShiftedRegister());
  DCHECK(rn.Is64Bits() || (rn.Is32Bits() && is_uint5(operand.shift_amount())));
  DCHECK(!operand.NeedsRelocation(this));
  Emit(SF(rd) | op | Flags(S) |
       ShiftDP(operand.shift()) | ImmDPShift(operand.shift_amount()) |
       Rm(operand.reg()) | Rn(rn) | Rd(rd));
}


void Assembler::DataProcExtendedRegister(const Register& rd,
                                         const Register& rn,
                                         const Operand& operand,
                                         FlagsUpdate S,
                                         Instr op) {
  DCHECK(!operand.NeedsRelocation(this));
  Instr dest_reg = (S == SetFlags) ? Rd(rd) : RdSP(rd);
  Emit(SF(rd) | op | Flags(S) | Rm(operand.reg()) |
       ExtendMode(operand.extend()) | ImmExtendShift(operand.shift_amount()) |
       dest_reg | RnSP(rn));
}


bool Assembler::IsImmAddSub(int64_t immediate) {
  return is_uint12(immediate) ||
         (is_uint12(immediate >> 12) && ((immediate & 0xfff) == 0));
}

void Assembler::LoadStore(const CPURegister& rt,
                          const MemOperand& addr,
                          LoadStoreOp op) {
  Instr memop = op | Rt(rt) | RnSP(addr.base());

  if (addr.IsImmediateOffset()) {
    LSDataSize size = CalcLSDataSize(op);
    if (IsImmLSScaled(addr.offset(), size)) {
      int offset = static_cast<int>(addr.offset());
      // Use the scaled addressing mode.
      Emit(LoadStoreUnsignedOffsetFixed | memop |
           ImmLSUnsigned(offset >> size));
    } else if (IsImmLSUnscaled(addr.offset())) {
      int offset = static_cast<int>(addr.offset());
      // Use the unscaled addressing mode.
      Emit(LoadStoreUnscaledOffsetFixed | memop | ImmLS(offset));
    } else {
      // This case is handled in the macro assembler.
      UNREACHABLE();
    }
  } else if (addr.IsRegisterOffset()) {
    Extend ext = addr.extend();
    Shift shift = addr.shift();
    unsigned shift_amount = addr.shift_amount();

    // LSL is encoded in the option field as UXTX.
    if (shift == LSL) {
      ext = UXTX;
    }

    // Shifts are encoded in one bit, indicating a left shift by the memory
    // access size.
    DCHECK((shift_amount == 0) ||
           (shift_amount == static_cast<unsigned>(CalcLSDataSize(op))));
    Emit(LoadStoreRegisterOffsetFixed | memop | Rm(addr.regoffset()) |
         ExtendMode(ext) | ImmShiftLS((shift_amount > 0) ? 1 : 0));
  } else {
    // Pre-index and post-index modes.
    DCHECK(!rt.Is(addr.base()));
    if (IsImmLSUnscaled(addr.offset())) {
      int offset = static_cast<int>(addr.offset());
      if (addr.IsPreIndex()) {
        Emit(LoadStorePreIndexFixed | memop | ImmLS(offset));
      } else {
        DCHECK(addr.IsPostIndex());
        Emit(LoadStorePostIndexFixed | memop | ImmLS(offset));
      }
    } else {
      // This case is handled in the macro assembler.
      UNREACHABLE();
    }
  }
}


bool Assembler::IsImmLSUnscaled(int64_t offset) {
  return is_int9(offset);
}


bool Assembler::IsImmLSScaled(int64_t offset, LSDataSize size) {
  bool offset_is_size_multiple = (((offset >> size) << size) == offset);
  return offset_is_size_multiple && is_uint12(offset >> size);
}


bool Assembler::IsImmLSPair(int64_t offset, LSDataSize size) {
  bool offset_is_size_multiple = (((offset >> size) << size) == offset);
  return offset_is_size_multiple && is_int7(offset >> size);
}


bool Assembler::IsImmLLiteral(int64_t offset) {
  int inst_size = static_cast<int>(kInstructionSizeLog2);
  bool offset_is_inst_multiple =
      (((offset >> inst_size) << inst_size) == offset);
  return offset_is_inst_multiple && is_intn(offset, ImmLLiteral_width);
}


// Test if a given value can be encoded in the immediate field of a logical
// instruction.
// If it can be encoded, the function returns true, and values pointed to by n,
// imm_s and imm_r are updated with immediates encoded in the format required
// by the corresponding fields in the logical instruction.
// If it can not be encoded, the function returns false, and the values pointed
// to by n, imm_s and imm_r are undefined.
bool Assembler::IsImmLogical(uint64_t value,
                             unsigned width,
                             unsigned* n,
                             unsigned* imm_s,
                             unsigned* imm_r) {
  DCHECK((n != NULL) && (imm_s != NULL) && (imm_r != NULL));
  DCHECK((width == kWRegSizeInBits) || (width == kXRegSizeInBits));

  bool negate = false;

  // Logical immediates are encoded using parameters n, imm_s and imm_r using
  // the following table:
  //
  //    N   imms    immr    size        S             R
  //    1  ssssss  rrrrrr    64    UInt(ssssss)  UInt(rrrrrr)
  //    0  0sssss  xrrrrr    32    UInt(sssss)   UInt(rrrrr)
  //    0  10ssss  xxrrrr    16    UInt(ssss)    UInt(rrrr)
  //    0  110sss  xxxrrr     8    UInt(sss)     UInt(rrr)
  //    0  1110ss  xxxxrr     4    UInt(ss)      UInt(rr)
  //    0  11110s  xxxxxr     2    UInt(s)       UInt(r)
  // (s bits must not be all set)
  //
  // A pattern is constructed of size bits, where the least significant S+1 bits
  // are set. The pattern is rotated right by R, and repeated across a 32 or
  // 64-bit value, depending on destination register width.
  //
  // Put another way: the basic format of a logical immediate is a single
  // contiguous stretch of 1 bits, repeated across the whole word at intervals
  // given by a power of 2. To identify them quickly, we first locate the
  // lowest stretch of 1 bits, then the next 1 bit above that; that combination
  // is different for every logical immediate, so it gives us all the
  // information we need to identify the only logical immediate that our input
  // could be, and then we simply check if that's the value we actually have.
  //
  // (The rotation parameter does give the possibility of the stretch of 1 bits
  // going 'round the end' of the word. To deal with that, we observe that in
  // any situation where that happens the bitwise NOT of the value is also a
  // valid logical immediate. So we simply invert the input whenever its low bit
  // is set, and then we know that the rotated case can't arise.)

  if (value & 1) {
    // If the low bit is 1, negate the value, and set a flag to remember that we
    // did (so that we can adjust the return values appropriately).
    negate = true;
    value = ~value;
  }

  if (width == kWRegSizeInBits) {
    // To handle 32-bit logical immediates, the very easiest thing is to repeat
    // the input value twice to make a 64-bit word. The correct encoding of that
    // as a logical immediate will also be the correct encoding of the 32-bit
    // value.

    // The most-significant 32 bits may not be zero (ie. negate is true) so
    // shift the value left before duplicating it.
    value <<= kWRegSizeInBits;
    value |= value >> kWRegSizeInBits;
  }

  // The basic analysis idea: imagine our input word looks like this.
  //
  //    0011111000111110001111100011111000111110001111100011111000111110
  //                                                          c  b    a
  //                                                          |<--d-->|
  //
  // We find the lowest set bit (as an actual power-of-2 value, not its index)
  // and call it a. Then we add a to our original number, which wipes out the
  // bottommost stretch of set bits and replaces it with a 1 carried into the
  // next zero bit. Then we look for the new lowest set bit, which is in
  // position b, and subtract it, so now our number is just like the original
  // but with the lowest stretch of set bits completely gone. Now we find the
  // lowest set bit again, which is position c in the diagram above. Then we'll
  // measure the distance d between bit positions a and c (using CLZ), and that
  // tells us that the only valid logical immediate that could possibly be equal
  // to this number is the one in which a stretch of bits running from a to just
  // below b is replicated every d bits.
  uint64_t a = LargestPowerOf2Divisor(value);
  uint64_t value_plus_a = value + a;
  uint64_t b = LargestPowerOf2Divisor(value_plus_a);
  uint64_t value_plus_a_minus_b = value_plus_a - b;
  uint64_t c = LargestPowerOf2Divisor(value_plus_a_minus_b);

  int d, clz_a, out_n;
  uint64_t mask;

  if (c != 0) {
    // The general case, in which there is more than one stretch of set bits.
    // Compute the repeat distance d, and set up a bitmask covering the basic
    // unit of repetition (i.e. a word with the bottom d bits set). Also, in all
    // of these cases the N bit of the output will be zero.
    clz_a = CountLeadingZeros(a, kXRegSizeInBits);
    int clz_c = CountLeadingZeros(c, kXRegSizeInBits);
    d = clz_a - clz_c;
    mask = ((V8_UINT64_C(1) << d) - 1);
    out_n = 0;
  } else {
    // Handle degenerate cases.
    //
    // If any of those 'find lowest set bit' operations didn't find a set bit at
    // all, then the word will have been zero thereafter, so in particular the
    // last lowest_set_bit operation will have returned zero. So we can test for
    // all the special case conditions in one go by seeing if c is zero.
    if (a == 0) {
      // The input was zero (or all 1 bits, which will come to here too after we
      // inverted it at the start of the function), for which we just return
      // false.
      return false;
    } else {
      // Otherwise, if c was zero but a was not, then there's just one stretch
      // of set bits in our word, meaning that we have the trivial case of
      // d == 64 and only one 'repetition'. Set up all the same variables as in
      // the general case above, and set the N bit in the output.
      clz_a = CountLeadingZeros(a, kXRegSizeInBits);
      d = 64;
      mask = ~V8_UINT64_C(0);
      out_n = 1;
    }
  }

  // If the repeat period d is not a power of two, it can't be encoded.
  if (!IS_POWER_OF_TWO(d)) {
    return false;
  }

  if (((b - a) & ~mask) != 0) {
    // If the bit stretch (b - a) does not fit within the mask derived from the
    // repeat period, then fail.
    return false;
  }

  // The only possible option is b - a repeated every d bits. Now we're going to
  // actually construct the valid logical immediate derived from that
  // specification, and see if it equals our original input.
  //
  // To repeat a value every d bits, we multiply it by a number of the form
  // (1 + 2^d + 2^(2d) + ...), i.e. 0x0001000100010001 or similar. These can
  // be derived using a table lookup on CLZ(d).
  static const uint64_t multipliers[] = {
    0x0000000000000001UL,
    0x0000000100000001UL,
    0x0001000100010001UL,
    0x0101010101010101UL,
    0x1111111111111111UL,
    0x5555555555555555UL,
  };
  int multiplier_idx = CountLeadingZeros(d, kXRegSizeInBits) - 57;
  // Ensure that the index to the multipliers array is within bounds.
  DCHECK((multiplier_idx >= 0) &&
         (static_cast<size_t>(multiplier_idx) < arraysize(multipliers)));
  uint64_t multiplier = multipliers[multiplier_idx];
  uint64_t candidate = (b - a) * multiplier;

  if (value != candidate) {
    // The candidate pattern doesn't match our input value, so fail.
    return false;
  }

  // We have a match! This is a valid logical immediate, so now we have to
  // construct the bits and pieces of the instruction encoding that generates
  // it.

  // Count the set bits in our basic stretch. The special case of clz(0) == -1
  // makes the answer come out right for stretches that reach the very top of
  // the word (e.g. numbers like 0xffffc00000000000).
  int clz_b = (b == 0) ? -1 : CountLeadingZeros(b, kXRegSizeInBits);
  int s = clz_a - clz_b;

  // Decide how many bits to rotate right by, to put the low bit of that basic
  // stretch in position a.
  int r;
  if (negate) {
    // If we inverted the input right at the start of this function, here's
    // where we compensate: the number of set bits becomes the number of clear
    // bits, and the rotation count is based on position b rather than position
    // a (since b is the location of the 'lowest' 1 bit after inversion).
    s = d - s;
    r = (clz_b + 1) & (d - 1);
  } else {
    r = (clz_a + 1) & (d - 1);
  }

  // Now we're done, except for having to encode the S output in such a way that
  // it gives both the number of set bits and the length of the repeated
  // segment. The s field is encoded like this:
  //
  //     imms    size        S
  //    ssssss    64    UInt(ssssss)
  //    0sssss    32    UInt(sssss)
  //    10ssss    16    UInt(ssss)
  //    110sss     8    UInt(sss)
  //    1110ss     4    UInt(ss)
  //    11110s     2    UInt(s)
  //
  // So we 'or' (-d << 1) with our computed s to form imms.
  *n = out_n;
  *imm_s = ((-d << 1) | (s - 1)) & 0x3f;
  *imm_r = r;

  return true;
}


bool Assembler::IsImmConditionalCompare(int64_t immediate) {
  return is_uint5(immediate);
}


bool Assembler::IsImmFP32(float imm) {
  // Valid values will have the form:
  // aBbb.bbbc.defg.h000.0000.0000.0000.0000
  uint32_t bits = float_to_rawbits(imm);
  // bits[19..0] are cleared.
  if ((bits & 0x7ffff) != 0) {
    return false;
  }

  // bits[29..25] are all set or all cleared.
  uint32_t b_pattern = (bits >> 16) & 0x3e00;
  if (b_pattern != 0 && b_pattern != 0x3e00) {
    return false;
  }

  // bit[30] and bit[29] are opposite.
  if (((bits ^ (bits << 1)) & 0x40000000) == 0) {
    return false;
  }

  return true;
}


bool Assembler::IsImmFP64(double imm) {
  // Valid values will have the form:
  // aBbb.bbbb.bbcd.efgh.0000.0000.0000.0000
  // 0000.0000.0000.0000.0000.0000.0000.0000
  uint64_t bits = double_to_rawbits(imm);
  // bits[47..0] are cleared.
  if ((bits & 0xffffffffffffL) != 0) {
    return false;
  }

  // bits[61..54] are all set or all cleared.
  uint32_t b_pattern = (bits >> 48) & 0x3fc0;
  if (b_pattern != 0 && b_pattern != 0x3fc0) {
    return false;
  }

  // bit[62] and bit[61] are opposite.
  if (((bits ^ (bits << 1)) & 0x4000000000000000L) == 0) {
    return false;
  }

  return true;
}


void Assembler::GrowBuffer() {
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // the new buffer
  if (buffer_size_ < 1 * MB) {
    desc.buffer_size = 2 * buffer_size_;
  } else {
    desc.buffer_size = buffer_size_ + 1 * MB;
  }
  CHECK_GT(desc.buffer_size, 0);  // No overflow.

  byte* buffer = reinterpret_cast<byte*>(buffer_);

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;

  desc.instr_size = pc_offset();
  desc.reloc_size =
      static_cast<int>((buffer + buffer_size_) - reloc_info_writer.pos());

  // Copy the data.
  intptr_t pc_delta = desc.buffer - buffer;
  intptr_t rc_delta = (desc.buffer + desc.buffer_size) -
                      (buffer + buffer_size_);
  memmove(desc.buffer, buffer, desc.instr_size);
  memmove(reloc_info_writer.pos() + rc_delta,
          reloc_info_writer.pos(), desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ = reinterpret_cast<byte*>(pc_) + pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // None of our relocation types are pc relative pointing outside the code
  // buffer nor pc absolute pointing inside the code buffer, so there is no need
  // to relocate any emitted relocation entries.

  // Relocate internal references.
  for (auto pos : internal_reference_positions_) {
    intptr_t* p = reinterpret_cast<intptr_t*>(buffer_ + pos);
    *p += pc_delta;
  }

  // Pending relocation entries are also relative, no need to relocate.
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(isolate(), reinterpret_cast<byte*>(pc_), rmode, data, NULL);
  if (((rmode >= RelocInfo::COMMENT) &&
       (rmode <= RelocInfo::DEBUG_BREAK_SLOT_AT_CALL)) ||
      (rmode == RelocInfo::INTERNAL_REFERENCE) ||
      (rmode == RelocInfo::CONST_POOL) || (rmode == RelocInfo::VENEER_POOL) ||
      (rmode == RelocInfo::DEOPT_REASON) ||
      (rmode == RelocInfo::GENERATOR_CONTINUATION)) {
    // Adjust code for new modes.
    DCHECK(RelocInfo::IsDebugBreakSlot(rmode) || RelocInfo::IsComment(rmode) ||
           RelocInfo::IsDeoptReason(rmode) || RelocInfo::IsPosition(rmode) ||
           RelocInfo::IsInternalReference(rmode) ||
           RelocInfo::IsConstPool(rmode) || RelocInfo::IsVeneerPool(rmode) ||
           RelocInfo::IsGeneratorContinuation(rmode));
    // These modes do not need an entry in the constant pool.
  } else {
    constpool_.RecordEntry(data, rmode);
    // Make sure the constant pool is not emitted in place of the next
    // instruction for which we just recorded relocation info.
    BlockConstPoolFor(1);
  }

  if (!RelocInfo::IsNone(rmode)) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
        !serializer_enabled() && !emit_debug_code()) {
      return;
    }
    DCHECK(buffer_space() >= kMaxRelocSize);  // too late to grow buffer here
    if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
      RelocInfo reloc_info_with_ast_id(isolate(), reinterpret_cast<byte*>(pc_),
                                       rmode, RecordedAstId().ToInt(), NULL);
      ClearRecordedAstId();
      reloc_info_writer.Write(&reloc_info_with_ast_id);
    } else {
      reloc_info_writer.Write(&rinfo);
    }
  }
}


void Assembler::BlockConstPoolFor(int instructions) {
  int pc_limit = pc_offset() + instructions * kInstructionSize;
  if (no_const_pool_before_ < pc_limit) {
    no_const_pool_before_ = pc_limit;
    // Make sure the pool won't be blocked for too long.
    DCHECK(pc_limit < constpool_.MaxPcOffset());
  }

  if (next_constant_pool_check_ < no_const_pool_before_) {
    next_constant_pool_check_ = no_const_pool_before_;
  }
}


void Assembler::CheckConstPool(bool force_emit, bool require_jump) {
  // Some short sequence of instruction mustn't be broken up by constant pool
  // emission, such sequences are protected by calls to BlockConstPoolFor and
  // BlockConstPoolScope.
  if (is_const_pool_blocked()) {
    // Something is wrong if emission is forced and blocked at the same time.
    DCHECK(!force_emit);
    return;
  }

  // There is nothing to do if there are no pending constant pool entries.
  if (constpool_.IsEmpty())  {
    // Calculate the offset of the next check.
    SetNextConstPoolCheckIn(kCheckConstPoolInterval);
    return;
  }

  // We emit a constant pool when:
  //  * requested to do so by parameter force_emit (e.g. after each function).
  //  * the distance to the first instruction accessing the constant pool is
  //    kApproxMaxDistToConstPool or more.
  //  * the number of entries in the pool is kApproxMaxPoolEntryCount or more.
  int dist = constpool_.DistanceToFirstUse();
  int count = constpool_.EntryCount();
  if (!force_emit &&
      (dist < kApproxMaxDistToConstPool) &&
      (count < kApproxMaxPoolEntryCount)) {
    return;
  }


  // Emit veneers for branches that would go out of range during emission of the
  // constant pool.
  int worst_case_size = constpool_.WorstCaseSize();
  CheckVeneerPool(false, require_jump,
                  kVeneerDistanceMargin + worst_case_size);

  // Check that the code buffer is large enough before emitting the constant
  // pool (this includes the gap to the relocation information).
  int needed_space = worst_case_size + kGap + 1 * kInstructionSize;
  while (buffer_space() <= needed_space) {
    GrowBuffer();
  }

  Label size_check;
  bind(&size_check);
  constpool_.Emit(require_jump);
  DCHECK(SizeOfCodeGeneratedSince(&size_check) <=
         static_cast<unsigned>(worst_case_size));

  // Since a constant pool was just emitted, move the check offset forward by
  // the standard interval.
  SetNextConstPoolCheckIn(kCheckConstPoolInterval);
}


bool Assembler::ShouldEmitVeneer(int max_reachable_pc, int margin) {
  // Account for the branch around the veneers and the guard.
  int protection_offset = 2 * kInstructionSize;
  return pc_offset() > max_reachable_pc - margin - protection_offset -
    static_cast<int>(unresolved_branches_.size() * kMaxVeneerCodeSize);
}


void Assembler::RecordVeneerPool(int location_offset, int size) {
  RelocInfo rinfo(isolate(), buffer_ + location_offset, RelocInfo::VENEER_POOL,
                  static_cast<intptr_t>(size), NULL);
  reloc_info_writer.Write(&rinfo);
}


void Assembler::EmitVeneers(bool force_emit, bool need_protection, int margin) {
  BlockPoolsScope scope(this);
  RecordComment("[ Veneers");

  // The exact size of the veneer pool must be recorded (see the comment at the
  // declaration site of RecordConstPool()), but computing the number of
  // veneers that will be generated is not obvious. So instead we remember the
  // current position and will record the size after the pool has been
  // generated.
  Label size_check;
  bind(&size_check);
  int veneer_pool_relocinfo_loc = pc_offset();

  Label end;
  if (need_protection) {
    b(&end);
  }

  EmitVeneersGuard();

  Label veneer_size_check;

  std::multimap<int, FarBranchInfo>::iterator it, it_to_delete;

  it = unresolved_branches_.begin();
  while (it != unresolved_branches_.end()) {
    if (force_emit || ShouldEmitVeneer(it->first, margin)) {
      Instruction* branch = InstructionAt(it->second.pc_offset_);
      Label* label = it->second.label_;

#ifdef DEBUG
      bind(&veneer_size_check);
#endif
      // Patch the branch to point to the current position, and emit a branch
      // to the label.
      Instruction* veneer = reinterpret_cast<Instruction*>(pc_);
      RemoveBranchFromLabelLinkChain(branch, label, veneer);
      branch->SetImmPCOffsetTarget(isolate(), veneer);
      b(label);
#ifdef DEBUG
      DCHECK(SizeOfCodeGeneratedSince(&veneer_size_check) <=
             static_cast<uint64_t>(kMaxVeneerCodeSize));
      veneer_size_check.Unuse();
#endif

      it_to_delete = it++;
      unresolved_branches_.erase(it_to_delete);
    } else {
      ++it;
    }
  }

  // Record the veneer pool size.
  int pool_size = static_cast<int>(SizeOfCodeGeneratedSince(&size_check));
  RecordVeneerPool(veneer_pool_relocinfo_loc, pool_size);

  if (unresolved_branches_.empty()) {
    next_veneer_pool_check_ = kMaxInt;
  } else {
    next_veneer_pool_check_ =
      unresolved_branches_first_limit() - kVeneerDistanceCheckMargin;
  }

  bind(&end);

  RecordComment("]");
}


void Assembler::CheckVeneerPool(bool force_emit, bool require_jump,
                                int margin) {
  // There is nothing to do if there are no pending veneer pool entries.
  if (unresolved_branches_.empty())  {
    DCHECK(next_veneer_pool_check_ == kMaxInt);
    return;
  }

  DCHECK(pc_offset() < unresolved_branches_first_limit());

  // Some short sequence of instruction mustn't be broken up by veneer pool
  // emission, such sequences are protected by calls to BlockVeneerPoolFor and
  // BlockVeneerPoolScope.
  if (is_veneer_pool_blocked()) {
    DCHECK(!force_emit);
    return;
  }

  if (!require_jump) {
    // Prefer emitting veneers protected by an existing instruction.
    margin *= kVeneerNoProtectionFactor;
  }
  if (force_emit || ShouldEmitVeneers(margin)) {
    EmitVeneers(force_emit, require_jump, margin);
  } else {
    next_veneer_pool_check_ =
      unresolved_branches_first_limit() - kVeneerDistanceCheckMargin;
  }
}


int Assembler::buffer_space() const {
  return static_cast<int>(reloc_info_writer.pos() -
                          reinterpret_cast<byte*>(pc_));
}


void Assembler::RecordConstPool(int size) {
  // We only need this for debugger support, to correctly compute offsets in the
  // code.
  RecordRelocInfo(RelocInfo::CONST_POOL, static_cast<intptr_t>(size));
}


void PatchingAssembler::PatchAdrFar(int64_t target_offset) {
  // The code at the current instruction should be:
  //   adr  rd, 0
  //   nop  (adr_far)
  //   nop  (adr_far)
  //   movz scratch, 0

  // Verify the expected code.
  Instruction* expected_adr = InstructionAt(0);
  CHECK(expected_adr->IsAdr() && (expected_adr->ImmPCRel() == 0));
  int rd_code = expected_adr->Rd();
  for (int i = 0; i < kAdrFarPatchableNNops; ++i) {
    CHECK(InstructionAt((i + 1) * kInstructionSize)->IsNop(ADR_FAR_NOP));
  }
  Instruction* expected_movz =
      InstructionAt((kAdrFarPatchableNInstrs - 1) * kInstructionSize);
  CHECK(expected_movz->IsMovz() &&
        (expected_movz->ImmMoveWide() == 0) &&
        (expected_movz->ShiftMoveWide() == 0));
  int scratch_code = expected_movz->Rd();

  // Patch to load the correct address.
  Register rd = Register::XRegFromCode(rd_code);
  Register scratch = Register::XRegFromCode(scratch_code);
  // Addresses are only 48 bits.
  adr(rd, target_offset & 0xFFFF);
  movz(scratch, (target_offset >> 16) & 0xFFFF, 16);
  movk(scratch, (target_offset >> 32) & 0xFFFF, 32);
  DCHECK((target_offset >> 48) == 0);
  add(rd, rd, scratch);
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

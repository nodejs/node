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

#include "v8.h"

#if V8_TARGET_ARCH_ARM64

#define ARM64_DEFINE_REG_STATICS

#include "arm64/assembler-arm64-inl.h"

namespace v8 {
namespace internal {


// -----------------------------------------------------------------------------
// CpuFeatures utilities (for V8 compatibility).

ExternalReference ExternalReference::cpu_features() {
  return ExternalReference(&CpuFeatures::supported_);
}


// -----------------------------------------------------------------------------
// CPURegList utilities.

CPURegister CPURegList::PopLowestIndex() {
  ASSERT(IsValid());
  if (IsEmpty()) {
    return NoCPUReg;
  }
  int index = CountTrailingZeros(list_, kRegListSizeInBits);
  ASSERT((1 << index) & list_);
  Remove(index);
  return CPURegister::Create(index, size_, type_);
}


CPURegister CPURegList::PopHighestIndex() {
  ASSERT(IsValid());
  if (IsEmpty()) {
    return NoCPUReg;
  }
  int index = CountLeadingZeros(list_, kRegListSizeInBits);
  index = kRegListSizeInBits - 1 - index;
  ASSERT((1 << index) & list_);
  Remove(index);
  return CPURegister::Create(index, size_, type_);
}


void CPURegList::RemoveCalleeSaved() {
  if (type() == CPURegister::kRegister) {
    Remove(GetCalleeSaved(RegisterSizeInBits()));
  } else if (type() == CPURegister::kFPRegister) {
    Remove(GetCalleeSavedFP(RegisterSizeInBits()));
  } else {
    ASSERT(type() == CPURegister::kNoRegister);
    ASSERT(IsEmpty());
    // The list must already be empty, so do nothing.
  }
}


CPURegList CPURegList::GetCalleeSaved(unsigned size) {
  return CPURegList(CPURegister::kRegister, size, 19, 29);
}


CPURegList CPURegList::GetCalleeSavedFP(unsigned size) {
  return CPURegList(CPURegister::kFPRegister, size, 8, 15);
}


CPURegList CPURegList::GetCallerSaved(unsigned size) {
  // Registers x0-x18 and lr (x30) are caller-saved.
  CPURegList list = CPURegList(CPURegister::kRegister, size, 0, 18);
  list.Combine(lr);
  return list;
}


CPURegList CPURegList::GetCallerSavedFP(unsigned size) {
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

const int RelocInfo::kApplyMask = 0;


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


void RelocInfo::PatchCode(byte* instructions, int instruction_count) {
  // Patch the code at the current address with the supplied instructions.
  Instr* pc = reinterpret_cast<Instr*>(pc_);
  Instr* instr = reinterpret_cast<Instr*>(instructions);
  for (int i = 0; i < instruction_count; i++) {
    *(pc + i) = *(instr + i);
  }

  // Indicate that code has changed.
  CPU::FlushICache(pc_, instruction_count * kInstructionSize);
}


// Patch the code at the current PC with a call to the target address.
// Additional guard instructions can be added if required.
void RelocInfo::PatchCodeWithCall(Address target, int guard_bytes) {
  UNIMPLEMENTED();
}


Register GetAllocatableRegisterThatIsNotOneOf(Register reg1, Register reg2,
                                              Register reg3, Register reg4) {
  CPURegList regs(reg1, reg2, reg3, reg4);
  for (int i = 0; i < Register::NumAllocatableRegisters(); i++) {
    Register candidate = Register::FromAllocationIndex(i);
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

  for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
    if (regs[i].IsRegister()) {
      number_of_valid_regs++;
      unique_regs |= regs[i].Bit();
    } else if (regs[i].IsFPRegister()) {
      number_of_valid_fpregs++;
      unique_fpregs |= regs[i].Bit();
    } else {
      ASSERT(!regs[i].IsValid());
    }
  }

  int number_of_unique_regs =
    CountSetBits(unique_regs, sizeof(unique_regs) * kBitsPerByte);
  int number_of_unique_fpregs =
    CountSetBits(unique_fpregs, sizeof(unique_fpregs) * kBitsPerByte);

  ASSERT(number_of_valid_regs >= number_of_unique_regs);
  ASSERT(number_of_valid_fpregs >= number_of_unique_fpregs);

  return (number_of_valid_regs != number_of_unique_regs) ||
         (number_of_valid_fpregs != number_of_unique_fpregs);
}


bool AreSameSizeAndType(const CPURegister& reg1, const CPURegister& reg2,
                        const CPURegister& reg3, const CPURegister& reg4,
                        const CPURegister& reg5, const CPURegister& reg6,
                        const CPURegister& reg7, const CPURegister& reg8) {
  ASSERT(reg1.IsValid());
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


void Operand::initialize_handle(Handle<Object> handle) {
  AllowDeferredHandleDereference using_raw_address;

  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  if (obj->IsHeapObject()) {
    ASSERT(!HeapObject::cast(obj)->GetHeap()->InNewSpace(obj));
    immediate_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    STATIC_ASSERT(sizeof(intptr_t) == sizeof(int64_t));
    immediate_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE64;
  }
}


bool Operand::NeedsRelocation() const {
  if (rmode_ == RelocInfo::EXTERNAL_REFERENCE) {
#ifdef DEBUG
    if (!Serializer::enabled()) {
      Serializer::TooLateToEnableNow();
    }
#endif
    return Serializer::enabled();
  }

  return !RelocInfo::IsNone(rmode_);
}


// Assembler

Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size),
      recorded_ast_id_(TypeFeedbackId::None()),
      unresolved_branches_(),
      positions_recorder_(this) {
  const_pool_blocked_nesting_ = 0;
  veneer_pool_blocked_nesting_ = 0;
  Reset();
}


Assembler::~Assembler() {
  ASSERT(num_pending_reloc_info_ == 0);
  ASSERT(const_pool_blocked_nesting_ == 0);
  ASSERT(veneer_pool_blocked_nesting_ == 0);
}


void Assembler::Reset() {
#ifdef DEBUG
  ASSERT((pc_ >= buffer_) && (pc_ < buffer_ + buffer_size_));
  ASSERT(const_pool_blocked_nesting_ == 0);
  ASSERT(veneer_pool_blocked_nesting_ == 0);
  ASSERT(unresolved_branches_.empty());
  memset(buffer_, 0, pc_ - buffer_);
#endif
  pc_ = buffer_;
  reloc_info_writer.Reposition(reinterpret_cast<byte*>(buffer_ + buffer_size_),
                               reinterpret_cast<byte*>(pc_));
  num_pending_reloc_info_ = 0;
  next_constant_pool_check_ = 0;
  next_veneer_pool_check_ = kMaxInt;
  no_const_pool_before_ = 0;
  first_const_pool_use_ = -1;
  ClearRecordedAstId();
}


void Assembler::GetCode(CodeDesc* desc) {
  // Emit constant pool if necessary.
  CheckConstPool(true, false);
  ASSERT(num_pending_reloc_info_ == 0);

  // Set up code descriptor.
  if (desc) {
    desc->buffer = reinterpret_cast<byte*>(buffer_);
    desc->buffer_size = buffer_size_;
    desc->instr_size = pc_offset();
    desc->reloc_size = (reinterpret_cast<byte*>(buffer_) + buffer_size_) -
                       reloc_info_writer.pos();
    desc->origin = this;
  }
}


void Assembler::Align(int m) {
  ASSERT(m >= 4 && IsPowerOf2(m));
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::CheckLabelLinkChain(Label const * label) {
#ifdef DEBUG
  if (label->is_linked()) {
    int linkoffset = label->pos();
    bool end_of_chain = false;
    while (!end_of_chain) {
      Instruction * link = InstructionAt(linkoffset);
      int linkpcoffset = link->ImmPCOffset();
      int prevlinkoffset = linkoffset + linkpcoffset;

      end_of_chain = (linkoffset == prevlinkoffset);
      linkoffset = linkoffset + linkpcoffset;
    }
  }
#endif
}


void Assembler::RemoveBranchFromLabelLinkChain(Instruction* branch,
                                               Label* label,
                                               Instruction* label_veneer) {
  ASSERT(label->is_linked());

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

  ASSERT(branch == link);
  next_link = branch->ImmPCOffsetTarget();

  if (branch == prev_link) {
    // The branch is the first instruction in the chain.
    if (branch == next_link) {
      // It is also the last instruction in the chain, so it is the only branch
      // currently referring to this label.
      label->Unuse();
    } else {
      label->link_to(reinterpret_cast<byte*>(next_link) - buffer_);
    }

  } else if (branch == next_link) {
    // The branch is the last (but not also the first) instruction in the chain.
    prev_link->SetImmPCOffsetTarget(prev_link);

  } else {
    // The branch is in the middle of the chain.
    if (prev_link->IsTargetInImmPCOffsetRange(next_link)) {
      prev_link->SetImmPCOffsetTarget(next_link);
    } else if (label_veneer != NULL) {
      // Use the veneer for all previous links in the chain.
      prev_link->SetImmPCOffsetTarget(prev_link);

      end_of_chain = false;
      link = next_link;
      while (!end_of_chain) {
        next_link = link->ImmPCOffsetTarget();
        end_of_chain = (link == next_link);
        link->SetImmPCOffsetTarget(label_veneer);
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

  ASSERT(!label->is_near_linked());
  ASSERT(!label->is_bound());

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
    int prevlinkoffset = linkoffset + link->ImmPCOffset();

    CheckLabelLinkChain(label);

    ASSERT(linkoffset >= 0);
    ASSERT(linkoffset < pc_offset());
    ASSERT((linkoffset > prevlinkoffset) ||
           (linkoffset - prevlinkoffset == kStartOfLabelLinkChain));
    ASSERT(prevlinkoffset >= 0);

    // Update the link to point to the label.
    link->SetImmPCOffsetTarget(reinterpret_cast<Instruction*>(pc_));

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

  ASSERT(label->is_bound());
  ASSERT(!label->is_linked());

  DeleteUnresolvedBranchInfoForLabel(label);
}


int Assembler::LinkAndGetByteOffsetTo(Label* label) {
  ASSERT(sizeof(*pc_) == 1);
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
    ASSERT(offset <= 0);
  } else {
    if (label->is_linked()) {
      // The label is linked, so the referring instruction should be added onto
      // the end of the label's link chain.
      //
      // In this case, label->pos() returns the offset of the last linked
      // instruction from the start of the buffer.
      offset = label->pos() - pc_offset();
      ASSERT(offset != kStartOfLabelLinkChain);
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


void Assembler::DeleteUnresolvedBranchInfoForLabel(Label* label) {
  if (unresolved_branches_.empty()) {
    ASSERT(next_veneer_pool_check_ == kMaxInt);
    return;
  }

  // Branches to this label will be resolved when the label is bound below.
  std::multimap<int, FarBranchInfo>::iterator it_tmp, it;
  it = unresolved_branches_.begin();
  while (it != unresolved_branches_.end()) {
    it_tmp = it++;
    if (it_tmp->second.label_ == label) {
      CHECK(it_tmp->first >= pc_offset());
      unresolved_branches_.erase(it_tmp);
    }
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
    ASSERT((num_pending_reloc_info_ == 0) ||
           (pc_offset() < (first_const_pool_use_ + kMaxDistToConstPool)));
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
  bool result = instr->IsLdrLiteralX() && (instr->Rt() == xzr.code());

  // It is still worth asserting the marker is complete.
  // 4: blr xzr
  ASSERT(!result || (instr->following()->IsBranchAndLinkToRegister() &&
                     instr->following()->Rn() == xzr.code()));

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
    int size = kDebugMessageOffset + strlen(message) + 1;
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


void Assembler::ConstantPoolMarker(uint32_t size) {
  ASSERT(is_const_pool_blocked());
  // + 1 is for the crash guard.
  Emit(LDR_x_lit | ImmLLiteral(2 * size + 1) | Rt(xzr));
}


void Assembler::EmitPoolGuard() {
  // We must generate only one instruction as this is used in scopes that
  // control the size of the code generated.
  Emit(BLR | Rn(xzr));
}


void Assembler::ConstantPoolGuard() {
#ifdef DEBUG
  // Currently this is only used after a constant pool marker.
  ASSERT(is_const_pool_blocked());
  Instruction* instr = reinterpret_cast<Instruction*>(pc_);
  ASSERT(instr->preceding()->IsLdrLiteralX() &&
         instr->preceding()->Rt() == xzr.code());
#endif
  EmitPoolGuard();
}


void Assembler::StartBlockVeneerPool() {
  ++veneer_pool_blocked_nesting_;
}


void Assembler::EndBlockVeneerPool() {
  if (--veneer_pool_blocked_nesting_ == 0) {
    // Check the veneer pool hasn't been blocked for too long.
    ASSERT(unresolved_branches_.empty() ||
           (pc_offset() < unresolved_branches_first_limit()));
  }
}


void Assembler::br(const Register& xn) {
  positions_recorder()->WriteRecordedPositions();
  ASSERT(xn.Is64Bits());
  Emit(BR | Rn(xn));
}


void Assembler::blr(const Register& xn) {
  positions_recorder()->WriteRecordedPositions();
  ASSERT(xn.Is64Bits());
  // The pattern 'blr xzr' is used as a guard to detect when execution falls
  // through the constant pool. It should not be emitted.
  ASSERT(!xn.Is(xzr));
  Emit(BLR | Rn(xn));
}


void Assembler::ret(const Register& xn) {
  positions_recorder()->WriteRecordedPositions();
  ASSERT(xn.Is64Bits());
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
  ASSERT(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSizeInBits)));
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
  ASSERT(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSizeInBits)));
  Emit(TBNZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbnz(const Register& rt,
                     unsigned bit_pos,
                     Label* label) {
  positions_recorder()->WriteRecordedPositions();
  tbnz(rt, bit_pos, LinkAndGetInstructionOffsetTo(label));
}


void Assembler::adr(const Register& rd, int imm21) {
  ASSERT(rd.Is64Bits());
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
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | LSLV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::lsrv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | LSRV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::asrv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | ASRV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::rorv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | RORV | Rm(rm) | Rn(rn) | Rd(rd));
}


// Bitfield operations.
void Assembler::bfm(const Register& rd,
                     const Register& rn,
                     unsigned immr,
                     unsigned imms) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | BFM | N |
       ImmR(immr, rd.SizeInBits()) |
       ImmS(imms, rn.SizeInBits()) |
       Rn(rn) | Rd(rd));
}


void Assembler::sbfm(const Register& rd,
                     const Register& rn,
                     unsigned immr,
                     unsigned imms) {
  ASSERT(rd.Is64Bits() || rn.Is32Bits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | SBFM | N |
       ImmR(immr, rd.SizeInBits()) |
       ImmS(imms, rn.SizeInBits()) |
       Rn(rn) | Rd(rd));
}


void Assembler::ubfm(const Register& rd,
                     const Register& rn,
                     unsigned immr,
                     unsigned imms) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  Instr N = SF(rd) >> (kSFOffset - kBitfieldNOffset);
  Emit(SF(rd) | UBFM | N |
       ImmR(immr, rd.SizeInBits()) |
       ImmS(imms, rn.SizeInBits()) |
       Rn(rn) | Rd(rd));
}


void Assembler::extr(const Register& rd,
                     const Register& rn,
                     const Register& rm,
                     unsigned lsb) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
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
  ASSERT((cond != al) && (cond != nv));
  Register zr = AppropriateZeroRegFor(rd);
  csinc(rd, zr, zr, InvertCondition(cond));
}


void Assembler::csetm(const Register &rd, Condition cond) {
  ASSERT((cond != al) && (cond != nv));
  Register zr = AppropriateZeroRegFor(rd);
  csinv(rd, zr, zr, InvertCondition(cond));
}


void Assembler::cinc(const Register &rd, const Register &rn, Condition cond) {
  ASSERT((cond != al) && (cond != nv));
  csinc(rd, rn, rn, InvertCondition(cond));
}


void Assembler::cinv(const Register &rd, const Register &rn, Condition cond) {
  ASSERT((cond != al) && (cond != nv));
  csinv(rd, rn, rn, InvertCondition(cond));
}


void Assembler::cneg(const Register &rd, const Register &rn, Condition cond) {
  ASSERT((cond != al) && (cond != nv));
  csneg(rd, rn, rn, InvertCondition(cond));
}


void Assembler::ConditionalSelect(const Register& rd,
                                  const Register& rn,
                                  const Register& rm,
                                  Condition cond,
                                  ConditionalSelectOp op) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
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
  ASSERT(AreSameSizeAndType(rd, rn, rm));
  Register zr = AppropriateZeroRegFor(rn);
  DataProcessing3Source(rd, rn, rm, zr, MADD);
}


void Assembler::madd(const Register& rd,
                     const Register& rn,
                     const Register& rm,
                     const Register& ra) {
  ASSERT(AreSameSizeAndType(rd, rn, rm, ra));
  DataProcessing3Source(rd, rn, rm, ra, MADD);
}


void Assembler::mneg(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  ASSERT(AreSameSizeAndType(rd, rn, rm));
  Register zr = AppropriateZeroRegFor(rn);
  DataProcessing3Source(rd, rn, rm, zr, MSUB);
}


void Assembler::msub(const Register& rd,
                     const Register& rn,
                     const Register& rm,
                     const Register& ra) {
  ASSERT(AreSameSizeAndType(rd, rn, rm, ra));
  DataProcessing3Source(rd, rn, rm, ra, MSUB);
}


void Assembler::smaddl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  ASSERT(rd.Is64Bits() && ra.Is64Bits());
  ASSERT(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, SMADDL_x);
}


void Assembler::smsubl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  ASSERT(rd.Is64Bits() && ra.Is64Bits());
  ASSERT(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, SMSUBL_x);
}


void Assembler::umaddl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  ASSERT(rd.Is64Bits() && ra.Is64Bits());
  ASSERT(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, UMADDL_x);
}


void Assembler::umsubl(const Register& rd,
                       const Register& rn,
                       const Register& rm,
                       const Register& ra) {
  ASSERT(rd.Is64Bits() && ra.Is64Bits());
  ASSERT(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, ra, UMSUBL_x);
}


void Assembler::smull(const Register& rd,
                      const Register& rn,
                      const Register& rm) {
  ASSERT(rd.Is64Bits());
  ASSERT(rn.Is32Bits() && rm.Is32Bits());
  DataProcessing3Source(rd, rn, rm, xzr, SMADDL_x);
}


void Assembler::smulh(const Register& rd,
                      const Register& rn,
                      const Register& rm) {
  ASSERT(AreSameSizeAndType(rd, rn, rm));
  DataProcessing3Source(rd, rn, rm, xzr, SMULH_x);
}


void Assembler::sdiv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
  Emit(SF(rd) | SDIV | Rm(rm) | Rn(rn) | Rd(rd));
}


void Assembler::udiv(const Register& rd,
                     const Register& rn,
                     const Register& rm) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == rm.SizeInBits());
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
  ASSERT(rd.Is64Bits());
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
  ASSERT(rt.Is64Bits());
  LoadStorePair(rt, rt2, src, LDPSW_x);
}


void Assembler::LoadStorePair(const CPURegister& rt,
                              const CPURegister& rt2,
                              const MemOperand& addr,
                              LoadStorePairOp op) {
  // 'rt' and 'rt2' can only be aliased for stores.
  ASSERT(((op & LoadStorePairLBit) == 0) || !rt.Is(rt2));
  ASSERT(AreSameSizeAndType(rt, rt2));

  Instr memop = op | Rt(rt) | Rt2(rt2) | RnSP(addr.base()) |
                ImmLSPair(addr.offset(), CalcLSPairDataSize(op));

  Instr addrmodeop;
  if (addr.IsImmediateOffset()) {
    addrmodeop = LoadStorePairOffsetFixed;
  } else {
    // Pre-index and post-index modes.
    ASSERT(!rt.Is(addr.base()));
    ASSERT(!rt2.Is(addr.base()));
    ASSERT(addr.offset() != 0);
    if (addr.IsPreIndex()) {
      addrmodeop = LoadStorePairPreIndexFixed;
    } else {
      ASSERT(addr.IsPostIndex());
      addrmodeop = LoadStorePairPostIndexFixed;
    }
  }
  Emit(addrmodeop | memop);
}


void Assembler::ldnp(const CPURegister& rt,
                     const CPURegister& rt2,
                     const MemOperand& src) {
  LoadStorePairNonTemporal(rt, rt2, src,
                           LoadPairNonTemporalOpFor(rt, rt2));
}


void Assembler::stnp(const CPURegister& rt,
                     const CPURegister& rt2,
                     const MemOperand& dst) {
  LoadStorePairNonTemporal(rt, rt2, dst,
                           StorePairNonTemporalOpFor(rt, rt2));
}


void Assembler::LoadStorePairNonTemporal(const CPURegister& rt,
                                         const CPURegister& rt2,
                                         const MemOperand& addr,
                                         LoadStorePairNonTemporalOp op) {
  ASSERT(!rt.Is(rt2));
  ASSERT(AreSameSizeAndType(rt, rt2));
  ASSERT(addr.IsImmediateOffset());

  LSDataSize size = CalcLSPairDataSize(
    static_cast<LoadStorePairOp>(op & LoadStorePairMask));
  Emit(op | Rt(rt) | Rt2(rt2) | RnSP(addr.base()) |
       ImmLSPair(addr.offset(), size));
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
  ASSERT(rt.Is64Bits());
  LoadStore(rt, src, LDRSW_x);
}


void Assembler::ldr(const Register& rt, uint64_t imm) {
  // TODO(all): Constant pool may be garbage collected. Hence we cannot store
  // arbitrary values in them. Manually move it for now. Fix
  // MacroAssembler::Fmov when this is implemented.
  UNIMPLEMENTED();
}


void Assembler::ldr(const FPRegister& ft, double imm) {
  // TODO(all): Constant pool may be garbage collected. Hence we cannot store
  // arbitrary values in them. Manually move it for now. Fix
  // MacroAssembler::Fmov when this is implemented.
  UNIMPLEMENTED();
}


void Assembler::ldr(const FPRegister& ft, float imm) {
  // TODO(all): Constant pool may be garbage collected. Hence we cannot store
  // arbitrary values in them. Manually move it for now. Fix
  // MacroAssembler::Fmov when this is implemented.
  UNIMPLEMENTED();
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
  ASSERT(rt.Is64Bits());
  Emit(MRS | ImmSystemRegister(sysreg) | Rt(rt));
}


void Assembler::msr(SystemRegister sysreg, const Register& rt) {
  ASSERT(rt.Is64Bits());
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
  ASSERT(fd.Is64Bits());
  ASSERT(IsImmFP64(imm));
  Emit(FMOV_d_imm | Rd(fd) | ImmFP64(imm));
}


void Assembler::fmov(FPRegister fd, float imm) {
  ASSERT(fd.Is32Bits());
  ASSERT(IsImmFP32(imm));
  Emit(FMOV_s_imm | Rd(fd) | ImmFP32(imm));
}


void Assembler::fmov(Register rd, FPRegister fn) {
  ASSERT(rd.SizeInBits() == fn.SizeInBits());
  FPIntegerConvertOp op = rd.Is32Bits() ? FMOV_ws : FMOV_xd;
  Emit(op | Rd(rd) | Rn(fn));
}


void Assembler::fmov(FPRegister fd, Register rn) {
  ASSERT(fd.SizeInBits() == rn.SizeInBits());
  FPIntegerConvertOp op = fd.Is32Bits() ? FMOV_sw : FMOV_dx;
  Emit(op | Rd(fd) | Rn(rn));
}


void Assembler::fmov(FPRegister fd, FPRegister fn) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
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
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FABS);
}


void Assembler::fneg(const FPRegister& fd,
                     const FPRegister& fn) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FNEG);
}


void Assembler::fsqrt(const FPRegister& fd,
                      const FPRegister& fn) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FSQRT);
}


void Assembler::frinta(const FPRegister& fd,
                       const FPRegister& fn) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTA);
}


void Assembler::frintn(const FPRegister& fd,
                       const FPRegister& fn) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTN);
}


void Assembler::frintz(const FPRegister& fd,
                       const FPRegister& fn) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  FPDataProcessing1Source(fd, fn, FRINTZ);
}


void Assembler::fcmp(const FPRegister& fn,
                     const FPRegister& fm) {
  ASSERT(fn.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fn) | FCMP | Rm(fm) | Rn(fn));
}


void Assembler::fcmp(const FPRegister& fn,
                     double value) {
  USE(value);
  // Although the fcmp instruction can strictly only take an immediate value of
  // +0.0, we don't need to check for -0.0 because the sign of 0.0 doesn't
  // affect the result of the comparison.
  ASSERT(value == 0.0);
  Emit(FPType(fn) | FCMP_zero | Rn(fn));
}


void Assembler::fccmp(const FPRegister& fn,
                      const FPRegister& fm,
                      StatusFlags nzcv,
                      Condition cond) {
  ASSERT(fn.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fn) | FCCMP | Rm(fm) | Cond(cond) | Rn(fn) | Nzcv(nzcv));
}


void Assembler::fcsel(const FPRegister& fd,
                      const FPRegister& fn,
                      const FPRegister& fm,
                      Condition cond) {
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  ASSERT(fd.SizeInBits() == fm.SizeInBits());
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
    ASSERT(fn.Is32Bits());
    FPDataProcessing1Source(fd, fn, FCVT_ds);
  } else {
    // Convert double to float.
    ASSERT(fn.Is64Bits());
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


// Note:
// Below, a difference in case for the same letter indicates a
// negated bit.
// If b is 1, then B is 0.
Instr Assembler::ImmFP32(float imm) {
  ASSERT(IsImmFP32(imm));
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
  ASSERT(IsImmFP64(imm));
  // bits: aBbb.bbbb.bbcd.efgh.0000.0000.0000.0000
  //       0000.0000.0000.0000.0000.0000.0000.0000
  uint64_t bits = double_to_rawbits(imm);
  // bit7: a000.0000
  uint32_t bit7 = ((bits >> 63) & 0x1) << 7;
  // bit6: 0b00.0000
  uint32_t bit6 = ((bits >> 61) & 0x1) << 6;
  // bit5_to_0: 00cd.efgh
  uint32_t bit5_to_0 = (bits >> 48) & 0x3f;

  return (bit7 | bit6 | bit5_to_0) << ImmFP_offset;
}


// Code generation helpers.
void Assembler::MoveWide(const Register& rd,
                         uint64_t imm,
                         int shift,
                         MoveWideImmediateOp mov_op) {
  if (shift >= 0) {
    // Explicit shift specified.
    ASSERT((shift == 0) || (shift == 16) || (shift == 32) || (shift == 48));
    ASSERT(rd.Is64Bits() || (shift == 0) || (shift == 16));
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
      ASSERT(rd.Is64Bits());
      imm >>= 32;
      shift = 2;
    } else if ((imm & ~(0xffffUL << 48)) == 0) {
      ASSERT(rd.Is64Bits());
      imm >>= 48;
      shift = 3;
    }
  }

  ASSERT(is_uint16(imm));

  Emit(SF(rd) | MoveWideImmediateFixed | mov_op |
       Rd(rd) | ImmMoveWide(imm) | ShiftMoveWide(shift));
}


void Assembler::AddSub(const Register& rd,
                       const Register& rn,
                       const Operand& operand,
                       FlagsUpdate S,
                       AddSubOp op) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(!operand.NeedsRelocation());
  if (operand.IsImmediate()) {
    int64_t immediate = operand.immediate();
    ASSERT(IsImmAddSub(immediate));
    Instr dest_reg = (S == SetFlags) ? Rd(rd) : RdSP(rd);
    Emit(SF(rd) | AddSubImmediateFixed | op | Flags(S) |
         ImmAddSub(immediate) | dest_reg | RnSP(rn));
  } else if (operand.IsShiftedRegister()) {
    ASSERT(operand.reg().SizeInBits() == rd.SizeInBits());
    ASSERT(operand.shift() != ROR);

    // For instructions of the form:
    //   add/sub   wsp, <Wn>, <Wm> [, LSL #0-3 ]
    //   add/sub   <Wd>, wsp, <Wm> [, LSL #0-3 ]
    //   add/sub   wsp, wsp, <Wm> [, LSL #0-3 ]
    //   adds/subs <Wd>, wsp, <Wm> [, LSL #0-3 ]
    // or their 64-bit register equivalents, convert the operand from shifted to
    // extended register mode, and emit an add/sub extended instruction.
    if (rn.IsSP() || rd.IsSP()) {
      ASSERT(!(rd.IsSP() && (S == SetFlags)));
      DataProcExtendedRegister(rd, rn, operand.ToExtendedRegister(), S,
                               AddSubExtendedFixed | op);
    } else {
      DataProcShiftedRegister(rd, rn, operand, S, AddSubShiftedFixed | op);
    }
  } else {
    ASSERT(operand.IsExtendedRegister());
    DataProcExtendedRegister(rd, rn, operand, S, AddSubExtendedFixed | op);
  }
}


void Assembler::AddSubWithCarry(const Register& rd,
                                const Register& rn,
                                const Operand& operand,
                                FlagsUpdate S,
                                AddSubWithCarryOp op) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(rd.SizeInBits() == operand.reg().SizeInBits());
  ASSERT(operand.IsShiftedRegister() && (operand.shift_amount() == 0));
  ASSERT(!operand.NeedsRelocation());
  Emit(SF(rd) | op | Flags(S) | Rm(operand.reg()) | Rn(rn) | Rd(rd));
}


void Assembler::hlt(int code) {
  ASSERT(is_uint16(code));
  Emit(HLT | ImmException(code));
}


void Assembler::brk(int code) {
  ASSERT(is_uint16(code));
  Emit(BRK | ImmException(code));
}


void Assembler::debug(const char* message, uint32_t code, Instr params) {
#ifdef USE_SIMULATOR
  // Don't generate simulator specific code if we are building a snapshot, which
  // might be run on real hardware.
  if (!Serializer::enabled()) {
#ifdef DEBUG
    Serializer::TooLateToEnableNow();
#endif
    // The arguments to the debug marker need to be contiguous in memory, so
    // make sure we don't try to emit pools.
    BlockPoolsScope scope(this);

    Label start;
    bind(&start);

    // Refer to instructions-arm64.h for a description of the marker and its
    // arguments.
    hlt(kImmExceptionIsDebug);
    ASSERT(SizeOfCodeGeneratedSince(&start) == kDebugCodeOffset);
    dc32(code);
    ASSERT(SizeOfCodeGeneratedSince(&start) == kDebugParamsOffset);
    dc32(params);
    ASSERT(SizeOfCodeGeneratedSince(&start) == kDebugMessageOffset);
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
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
  ASSERT(!operand.NeedsRelocation());
  if (operand.IsImmediate()) {
    int64_t immediate = operand.immediate();
    unsigned reg_size = rd.SizeInBits();

    ASSERT(immediate != 0);
    ASSERT(immediate != -1);
    ASSERT(rd.Is64Bits() || is_uint32(immediate));

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
    ASSERT(operand.IsShiftedRegister());
    ASSERT(operand.reg().SizeInBits() == rd.SizeInBits());
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
  ASSERT(!operand.NeedsRelocation());
  if (operand.IsImmediate()) {
    int64_t immediate = operand.immediate();
    ASSERT(IsImmConditionalCompare(immediate));
    ccmpop = ConditionalCompareImmediateFixed | op | ImmCondCmp(immediate);
  } else {
    ASSERT(operand.IsShiftedRegister() && (operand.shift_amount() == 0));
    ccmpop = ConditionalCompareRegisterFixed | op | Rm(operand.reg());
  }
  Emit(SF(rn) | ccmpop | Cond(cond) | Rn(rn) | Nzcv(nzcv));
}


void Assembler::DataProcessing1Source(const Register& rd,
                                      const Register& rn,
                                      DataProcessing1SourceOp op) {
  ASSERT(rd.SizeInBits() == rn.SizeInBits());
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
  ASSERT(fd.SizeInBits() == fn.SizeInBits());
  ASSERT(fd.SizeInBits() == fm.SizeInBits());
  Emit(FPType(fd) | op | Rm(fm) | Rn(fn) | Rd(fd));
}


void Assembler::FPDataProcessing3Source(const FPRegister& fd,
                                        const FPRegister& fn,
                                        const FPRegister& fm,
                                        const FPRegister& fa,
                                        FPDataProcessing3SourceOp op) {
  ASSERT(AreSameSizeAndType(fd, fn, fm, fa));
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
  ASSERT(rd.SizeInBits() >= rn.SizeInBits());
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
        ASSERT(rn.SizeInBits() == kXRegSizeInBits);
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
  ASSERT(operand.IsShiftedRegister());
  ASSERT(rn.Is64Bits() || (rn.Is32Bits() && is_uint5(operand.shift_amount())));
  ASSERT(!operand.NeedsRelocation());
  Emit(SF(rd) | op | Flags(S) |
       ShiftDP(operand.shift()) | ImmDPShift(operand.shift_amount()) |
       Rm(operand.reg()) | Rn(rn) | Rd(rd));
}


void Assembler::DataProcExtendedRegister(const Register& rd,
                                         const Register& rn,
                                         const Operand& operand,
                                         FlagsUpdate S,
                                         Instr op) {
  ASSERT(!operand.NeedsRelocation());
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
  ptrdiff_t offset = addr.offset();

  if (addr.IsImmediateOffset()) {
    LSDataSize size = CalcLSDataSize(op);
    if (IsImmLSScaled(offset, size)) {
      // Use the scaled addressing mode.
      Emit(LoadStoreUnsignedOffsetFixed | memop |
           ImmLSUnsigned(offset >> size));
    } else if (IsImmLSUnscaled(offset)) {
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
    ASSERT((shift_amount == 0) ||
           (shift_amount == static_cast<unsigned>(CalcLSDataSize(op))));
    Emit(LoadStoreRegisterOffsetFixed | memop | Rm(addr.regoffset()) |
         ExtendMode(ext) | ImmShiftLS((shift_amount > 0) ? 1 : 0));
  } else {
    // Pre-index and post-index modes.
    ASSERT(!rt.Is(addr.base()));
    if (IsImmLSUnscaled(offset)) {
      if (addr.IsPreIndex()) {
        Emit(LoadStorePreIndexFixed | memop | ImmLS(offset));
      } else {
        ASSERT(addr.IsPostIndex());
        Emit(LoadStorePostIndexFixed | memop | ImmLS(offset));
      }
    } else {
      // This case is handled in the macro assembler.
      UNREACHABLE();
    }
  }
}


bool Assembler::IsImmLSUnscaled(ptrdiff_t offset) {
  return is_int9(offset);
}


bool Assembler::IsImmLSScaled(ptrdiff_t offset, LSDataSize size) {
  bool offset_is_size_multiple = (((offset >> size) << size) == offset);
  return offset_is_size_multiple && is_uint12(offset >> size);
}


void Assembler::LoadLiteral(const CPURegister& rt, int offset_from_pc) {
  ASSERT((offset_from_pc & ((1 << kLiteralEntrySizeLog2) - 1)) == 0);
  // The pattern 'ldr xzr, #offset' is used to indicate the beginning of a
  // constant pool. It should not be emitted.
  ASSERT(!rt.Is(xzr));
  Emit(LDR_x_lit |
       ImmLLiteral(offset_from_pc >> kLiteralEntrySizeLog2) |
       Rt(rt));
}


void Assembler::LoadRelocatedValue(const CPURegister& rt,
                                   const Operand& operand,
                                   LoadLiteralOp op) {
  int64_t imm = operand.immediate();
  ASSERT(is_int32(imm) || is_uint32(imm) || (rt.Is64Bits()));
  RecordRelocInfo(operand.rmode(), imm);
  BlockConstPoolFor(1);
  Emit(op | ImmLLiteral(0) | Rt(rt));
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
  ASSERT((n != NULL) && (imm_s != NULL) && (imm_r != NULL));
  ASSERT((width == kWRegSizeInBits) || (width == kXRegSizeInBits));

  // Logical immediates are encoded using parameters n, imm_s and imm_r using
  // the following table:
  //
  //  N   imms    immr    size        S             R
  //  1  ssssss  rrrrrr    64    UInt(ssssss)  UInt(rrrrrr)
  //  0  0sssss  xrrrrr    32    UInt(sssss)   UInt(rrrrr)
  //  0  10ssss  xxrrrr    16    UInt(ssss)    UInt(rrrr)
  //  0  110sss  xxxrrr     8    UInt(sss)     UInt(rrr)
  //  0  1110ss  xxxxrr     4    UInt(ss)      UInt(rr)
  //  0  11110s  xxxxxr     2    UInt(s)       UInt(r)
  // (s bits must not be all set)
  //
  // A pattern is constructed of size bits, where the least significant S+1
  // bits are set. The pattern is rotated right by R, and repeated across a
  // 32 or 64-bit value, depending on destination register width.
  //
  // To test if an arbitary immediate can be encoded using this scheme, an
  // iterative algorithm is used.
  //
  // TODO(mcapewel) This code does not consider using X/W register overlap to
  // support 64-bit immediates where the top 32-bits are zero, and the bottom
  // 32-bits are an encodable logical immediate.

  // 1. If the value has all set or all clear bits, it can't be encoded.
  if ((value == 0) || (value == 0xffffffffffffffffUL) ||
      ((width == kWRegSizeInBits) && (value == 0xffffffff))) {
    return false;
  }

  unsigned lead_zero = CountLeadingZeros(value, width);
  unsigned lead_one = CountLeadingZeros(~value, width);
  unsigned trail_zero = CountTrailingZeros(value, width);
  unsigned trail_one = CountTrailingZeros(~value, width);
  unsigned set_bits = CountSetBits(value, width);

  // The fixed bits in the immediate s field.
  // If width == 64 (X reg), start at 0xFFFFFF80.
  // If width == 32 (W reg), start at 0xFFFFFFC0, as the iteration for 64-bit
  // widths won't be executed.
  int imm_s_fixed = (width == kXRegSizeInBits) ? -128 : -64;
  int imm_s_mask = 0x3F;

  for (;;) {
    // 2. If the value is two bits wide, it can be encoded.
    if (width == 2) {
      *n = 0;
      *imm_s = 0x3C;
      *imm_r = (value & 3) - 1;
      return true;
    }

    *n = (width == 64) ? 1 : 0;
    *imm_s = ((imm_s_fixed | (set_bits - 1)) & imm_s_mask);
    if ((lead_zero + set_bits) == width) {
      *imm_r = 0;
    } else {
      *imm_r = (lead_zero > 0) ? (width - trail_zero) : lead_one;
    }

    // 3. If the sum of leading zeros, trailing zeros and set bits is equal to
    //    the bit width of the value, it can be encoded.
    if (lead_zero + trail_zero + set_bits == width) {
      return true;
    }

    // 4. If the sum of leading ones, trailing ones and unset bits in the
    //    value is equal to the bit width of the value, it can be encoded.
    if (lead_one + trail_one + (width - set_bits) == width) {
      return true;
    }

    // 5. If the most-significant half of the bitwise value is equal to the
    //    least-significant half, return to step 2 using the least-significant
    //    half of the value.
    uint64_t mask = (1UL << (width >> 1)) - 1;
    if ((value & mask) == ((value >> (width >> 1)) & mask)) {
      width >>= 1;
      set_bits >>= 1;
      imm_s_fixed >>= 1;
      continue;
    }

    // 6. Otherwise, the value can't be encoded.
    return false;
  }
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
  if (buffer_size_ < 4 * KB) {
    desc.buffer_size = 4 * KB;
  } else if (buffer_size_ < 1 * MB) {
    desc.buffer_size = 2 * buffer_size_;
  } else {
    desc.buffer_size = buffer_size_ + 1 * MB;
  }
  CHECK_GT(desc.buffer_size, 0);  // No overflow.

  byte* buffer = reinterpret_cast<byte*>(buffer_);

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);

  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer + buffer_size_) - reloc_info_writer.pos();

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

  // Relocate pending relocation entries.
  for (int i = 0; i < num_pending_reloc_info_; i++) {
    RelocInfo& rinfo = pending_reloc_info_[i];
    ASSERT(rinfo.rmode() != RelocInfo::COMMENT &&
           rinfo.rmode() != RelocInfo::POSITION);
    if (rinfo.rmode() != RelocInfo::JS_RETURN) {
      rinfo.set_pc(rinfo.pc() + pc_delta);
    }
  }
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(reinterpret_cast<byte*>(pc_), rmode, data, NULL);
  if (((rmode >= RelocInfo::JS_RETURN) &&
       (rmode <= RelocInfo::DEBUG_BREAK_SLOT)) ||
      (rmode == RelocInfo::CONST_POOL) ||
      (rmode == RelocInfo::VENEER_POOL)) {
    // Adjust code for new modes.
    ASSERT(RelocInfo::IsDebugBreakSlot(rmode)
           || RelocInfo::IsJSReturn(rmode)
           || RelocInfo::IsComment(rmode)
           || RelocInfo::IsPosition(rmode)
           || RelocInfo::IsConstPool(rmode)
           || RelocInfo::IsVeneerPool(rmode));
    // These modes do not need an entry in the constant pool.
  } else {
    ASSERT(num_pending_reloc_info_ < kMaxNumPendingRelocInfo);
    if (num_pending_reloc_info_ == 0) {
      first_const_pool_use_ = pc_offset();
    }
    pending_reloc_info_[num_pending_reloc_info_++] = rinfo;
    // Make sure the constant pool is not emitted in place of the next
    // instruction for which we just recorded relocation info.
    BlockConstPoolFor(1);
  }

  if (!RelocInfo::IsNone(rmode)) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
#ifdef DEBUG
      if (!Serializer::enabled()) {
        Serializer::TooLateToEnableNow();
      }
#endif
      if (!Serializer::enabled() && !emit_debug_code()) {
        return;
      }
    }
    ASSERT(buffer_space() >= kMaxRelocSize);  // too late to grow buffer here
    if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
      RelocInfo reloc_info_with_ast_id(
          reinterpret_cast<byte*>(pc_), rmode, RecordedAstId().ToInt(), NULL);
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
    // If there are some pending entries, the constant pool cannot be blocked
    // further than first_const_pool_use_ + kMaxDistToConstPool
    ASSERT((num_pending_reloc_info_ == 0) ||
           (pc_limit < (first_const_pool_use_ + kMaxDistToConstPool)));
    no_const_pool_before_ = pc_limit;
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
    ASSERT(!force_emit);
    return;
  }

  // There is nothing to do if there are no pending constant pool entries.
  if (num_pending_reloc_info_ == 0)  {
    // Calculate the offset of the next check.
    next_constant_pool_check_ = pc_offset() + kCheckConstPoolInterval;
    return;
  }

  // We emit a constant pool when:
  //  * requested to do so by parameter force_emit (e.g. after each function).
  //  * the distance to the first instruction accessing the constant pool is
  //    kAvgDistToConstPool or more.
  //  * no jump is required and the distance to the first instruction accessing
  //    the constant pool is at least kMaxDistToPConstool / 2.
  ASSERT(first_const_pool_use_ >= 0);
  int dist = pc_offset() - first_const_pool_use_;
  if (!force_emit && dist < kAvgDistToConstPool &&
      (require_jump || (dist < (kMaxDistToConstPool / 2)))) {
    return;
  }

  int jump_instr = require_jump ? kInstructionSize : 0;
  int size_pool_marker = kInstructionSize;
  int size_pool_guard = kInstructionSize;
  int pool_size = jump_instr + size_pool_marker + size_pool_guard +
    num_pending_reloc_info_ * kPointerSize;
  int needed_space = pool_size + kGap;

  // Emit veneers for branches that would go out of range during emission of the
  // constant pool.
  CheckVeneerPool(false, require_jump, kVeneerDistanceMargin + pool_size);

  Label size_check;
  bind(&size_check);

  // Check that the code buffer is large enough before emitting the constant
  // pool (include the jump over the pool, the constant pool marker, the
  // constant pool guard, and the gap to the relocation information).
  while (buffer_space() <= needed_space) {
    GrowBuffer();
  }

  {
    // Block recursive calls to CheckConstPool and protect from veneer pools.
    BlockPoolsScope block_pools(this);
    RecordComment("[ Constant Pool");
    RecordConstPool(pool_size);

    // Emit jump over constant pool if necessary.
    Label after_pool;
    if (require_jump) {
      b(&after_pool);
    }

    // Emit a constant pool header. The header has two goals:
    //  1) Encode the size of the constant pool, for use by the disassembler.
    //  2) Terminate the program, to try to prevent execution from accidentally
    //     flowing into the constant pool.
    // The header is therefore made of two arm64 instructions:
    //   ldr xzr, #<size of the constant pool in 32-bit words>
    //   blr xzr
    // If executed the code will likely segfault and lr will point to the
    // beginning of the constant pool.
    // TODO(all): currently each relocated constant is 64 bits, consider adding
    // support for 32-bit entries.
    ConstantPoolMarker(2 * num_pending_reloc_info_);
    ConstantPoolGuard();

    // Emit constant pool entries.
    for (int i = 0; i < num_pending_reloc_info_; i++) {
      RelocInfo& rinfo = pending_reloc_info_[i];
      ASSERT(rinfo.rmode() != RelocInfo::COMMENT &&
             rinfo.rmode() != RelocInfo::POSITION &&
             rinfo.rmode() != RelocInfo::STATEMENT_POSITION &&
             rinfo.rmode() != RelocInfo::CONST_POOL &&
             rinfo.rmode() != RelocInfo::VENEER_POOL);

      Instruction* instr = reinterpret_cast<Instruction*>(rinfo.pc());
      // Instruction to patch must be 'ldr rd, [pc, #offset]' with offset == 0.
      ASSERT(instr->IsLdrLiteral() &&
             instr->ImmLLiteral() == 0);

      instr->SetImmPCOffsetTarget(reinterpret_cast<Instruction*>(pc_));
      dc64(rinfo.data());
    }

    num_pending_reloc_info_ = 0;
    first_const_pool_use_ = -1;

    RecordComment("]");

    if (after_pool.is_linked()) {
      bind(&after_pool);
    }
  }

  // Since a constant pool was just emitted, move the check offset forward by
  // the standard interval.
  next_constant_pool_check_ = pc_offset() + kCheckConstPoolInterval;

  ASSERT(SizeOfCodeGeneratedSince(&size_check) ==
         static_cast<unsigned>(pool_size));
}


bool Assembler::ShouldEmitVeneer(int max_reachable_pc, int margin) {
  // Account for the branch around the veneers and the guard.
  int protection_offset = 2 * kInstructionSize;
  return pc_offset() > max_reachable_pc - margin - protection_offset -
    static_cast<int>(unresolved_branches_.size() * kMaxVeneerCodeSize);
}


void Assembler::RecordVeneerPool(int location_offset, int size) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  RelocInfo rinfo(buffer_ + location_offset,
                  RelocInfo::VENEER_POOL, static_cast<intptr_t>(size),
                  NULL);
  reloc_info_writer.Write(&rinfo);
#endif
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
      branch->SetImmPCOffsetTarget(veneer);
      b(label);
#ifdef DEBUG
      ASSERT(SizeOfCodeGeneratedSince(&veneer_size_check) <=
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
  int pool_size = SizeOfCodeGeneratedSince(&size_check);
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
    ASSERT(next_veneer_pool_check_ == kMaxInt);
    return;
  }

  ASSERT(pc_offset() < unresolved_branches_first_limit());

  // Some short sequence of instruction mustn't be broken up by veneer pool
  // emission, such sequences are protected by calls to BlockVeneerPoolFor and
  // BlockVeneerPoolScope.
  if (is_veneer_pool_blocked()) {
    ASSERT(!force_emit);
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


void Assembler::RecordComment(const char* msg) {
  if (FLAG_code_comments) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::COMMENT, reinterpret_cast<intptr_t>(msg));
  }
}


int Assembler::buffer_space() const {
  return reloc_info_writer.pos() - reinterpret_cast<byte*>(pc_);
}


void Assembler::RecordJSReturn() {
  positions_recorder()->WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::JS_RETURN);
}


void Assembler::RecordDebugBreakSlot() {
  positions_recorder()->WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::DEBUG_BREAK_SLOT);
}


void Assembler::RecordConstPool(int size) {
  // We only need this for debugger support, to correctly compute offsets in the
  // code.
#ifdef ENABLE_DEBUGGER_SUPPORT
  RecordRelocInfo(RelocInfo::CONST_POOL, static_cast<intptr_t>(size));
#endif
}


MaybeObject* Assembler::AllocateConstantPool(Heap* heap) {
  // No out-of-line constant pool support.
  UNREACHABLE();
  return NULL;
}


void Assembler::PopulateConstantPool(ConstantPoolArray* constant_pool) {
  // No out-of-line constant pool support.
  UNREACHABLE();
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64

// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2014 the V8 project authors. All rights reserved.

#include "src/ppc/assembler-ppc.h"

#if V8_TARGET_ARCH_PPC

#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/macro-assembler.h"
#include "src/ppc/assembler-ppc-inl.h"

namespace v8 {
namespace internal {

// Get the CPU features enabled by the build.
static unsigned CpuFeaturesImpliedByCompiler() {
  unsigned answer = 0;
  return answer;
}


void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();
  icache_line_size_ = 128;

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

// Detect whether frim instruction is supported (POWER5+)
// For now we will just check for processors we know do not
// support it
#ifndef USE_SIMULATOR
  // Probe for additional features at runtime.
  base::CPU cpu;
#if V8_TARGET_ARCH_PPC64
  if (cpu.part() == base::CPU::PPC_POWER8) {
    supported_ |= (1u << FPR_GPR_MOV);
  }
#endif
  if (cpu.part() == base::CPU::PPC_POWER6 ||
      cpu.part() == base::CPU::PPC_POWER7 ||
      cpu.part() == base::CPU::PPC_POWER8) {
    supported_ |= (1u << LWSYNC);
  }
  if (cpu.part() == base::CPU::PPC_POWER7 ||
      cpu.part() == base::CPU::PPC_POWER8) {
    supported_ |= (1u << ISELECT);
  }
#if V8_OS_LINUX
  if (!(cpu.part() == base::CPU::PPC_G5 || cpu.part() == base::CPU::PPC_G4)) {
    // Assume support
    supported_ |= (1u << FPU);
  }
  if (cpu.icache_line_size() != base::CPU::UNKNOWN_CACHE_LINE_SIZE) {
    icache_line_size_ = cpu.icache_line_size();
  }
#elif V8_OS_AIX
  // Assume support FP support and default cache line size
  supported_ |= (1u << FPU);
#endif
#else  // Simulator
  supported_ |= (1u << FPU);
  supported_ |= (1u << LWSYNC);
  supported_ |= (1u << ISELECT);
#if V8_TARGET_ARCH_PPC64
  supported_ |= (1u << FPR_GPR_MOV);
#endif
#endif
}


void CpuFeatures::PrintTarget() {
  const char* ppc_arch = NULL;

#if V8_TARGET_ARCH_PPC64
  ppc_arch = "ppc64";
#else
  ppc_arch = "ppc";
#endif

  printf("target %s\n", ppc_arch);
}


void CpuFeatures::PrintFeatures() {
  printf("FPU=%d\n", CpuFeatures::IsSupported(FPU));
}


Register ToRegister(int num) {
  DCHECK(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] = {r0,  sp,  r2,  r3,  r4,  r5,  r6,  r7,
                                 r8,  r9,  r10, r11, ip,  r13, r14, r15,
                                 r16, r17, r18, r19, r20, r21, r22, r23,
                                 r24, r25, r26, r27, r28, r29, r30, fp};
  return kRegisters[num];
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

const int RelocInfo::kApplyMask = 1 << RelocInfo::INTERNAL_REFERENCE |
                                  1 << RelocInfo::INTERNAL_REFERENCE_ENCODED;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially
  // coded.  Being specially coded on PPC means that it is a lis/ori
  // instruction sequence or is a constant pool entry, and these are
  // always the case inside code objects.
  return true;
}


bool RelocInfo::IsInConstantPool() {
  if (FLAG_enable_embedded_constant_pool) {
    Address constant_pool = host_->constant_pool();
    return (constant_pool && Assembler::IsConstantPoolLoadStart(pc_));
  }
  return false;
}

Address RelocInfo::wasm_memory_reference() {
  DCHECK(IsWasmMemoryReference(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

uint32_t RelocInfo::wasm_memory_size_reference() {
  DCHECK(IsWasmMemorySizeReference(rmode_));
  return static_cast<uint32_t>(
     reinterpret_cast<intptr_t>(Assembler::target_address_at(pc_, host_)));
}

Address RelocInfo::wasm_global_reference() {
  DCHECK(IsWasmGlobalReference(rmode_));
  return Assembler::target_address_at(pc_, host_);
}


void RelocInfo::unchecked_update_wasm_memory_reference(
    Address address, ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate_, pc_, host_, address, flush_mode);
}

void RelocInfo::unchecked_update_wasm_memory_size(uint32_t size,
                                                  ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate_, pc_, host_,
                                   reinterpret_cast<Address>(size), flush_mode);
}

// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand
// See assembler-ppc-inl.h for inlined constructors

Operand::Operand(Handle<Object> handle) {
  AllowDeferredHandleDereference using_raw_address;
  rm_ = no_reg;
  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  if (obj->IsHeapObject()) {
    imm_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // no relocation needed
    imm_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = kRelocInfo_NONEPTR;
  }
}


MemOperand::MemOperand(Register rn, int32_t offset) {
  ra_ = rn;
  rb_ = no_reg;
  offset_ = offset;
}


MemOperand::MemOperand(Register ra, Register rb) {
  ra_ = ra;
  rb_ = rb;
  offset_ = 0;
}


// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size),
      recorded_ast_id_(TypeFeedbackId::None()),
      constant_pool_builder_(kLoadPtrMaxReachBits, kLoadDoubleMaxReachBits) {
  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);

  no_trampoline_pool_before_ = 0;
  trampoline_pool_blocked_nesting_ = 0;
  constant_pool_entry_sharing_blocked_nesting_ = 0;
  next_trampoline_check_ = kMaxInt;
  internal_trampoline_exception_ = false;
  last_bound_pos_ = 0;
  optimizable_cmpi_pos_ = -1;
  trampoline_emitted_ = FLAG_force_long_branches;
  tracked_branch_count_ = 0;
  ClearRecordedAstId();
  relocations_.reserve(128);
}


void Assembler::GetCode(CodeDesc* desc) {
  // Emit constant pool if necessary.
  int constant_pool_offset = EmitConstantPool();

  EmitRelocations();

  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc->constant_pool_size =
      (constant_pool_offset ? desc->instr_size - constant_pool_offset : 0);
  desc->origin = this;
  desc->unwinding_info_size = 0;
  desc->unwinding_info = nullptr;
}


void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo32(m));
  DCHECK((pc_offset() & (kInstrSize - 1)) == 0);
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::CodeTargetAlign() { Align(8); }


Condition Assembler::GetCondition(Instr instr) {
  switch (instr & kCondMask) {
    case BT:
      return eq;
    case BF:
      return ne;
    default:
      UNIMPLEMENTED();
  }
  return al;
}


bool Assembler::IsLis(Instr instr) {
  return ((instr & kOpcodeMask) == ADDIS) && GetRA(instr).is(r0);
}


bool Assembler::IsLi(Instr instr) {
  return ((instr & kOpcodeMask) == ADDI) && GetRA(instr).is(r0);
}


bool Assembler::IsAddic(Instr instr) { return (instr & kOpcodeMask) == ADDIC; }


bool Assembler::IsOri(Instr instr) { return (instr & kOpcodeMask) == ORI; }


bool Assembler::IsBranch(Instr instr) { return ((instr & kOpcodeMask) == BCX); }


Register Assembler::GetRA(Instr instr) {
  Register reg;
  reg.reg_code = Instruction::RAValue(instr);
  return reg;
}


Register Assembler::GetRB(Instr instr) {
  Register reg;
  reg.reg_code = Instruction::RBValue(instr);
  return reg;
}


#if V8_TARGET_ARCH_PPC64
// This code assumes a FIXED_SEQUENCE for 64bit loads (lis/ori)
bool Assembler::Is64BitLoadIntoR12(Instr instr1, Instr instr2, Instr instr3,
                                   Instr instr4, Instr instr5) {
  // Check the instructions are indeed a five part load (into r12)
  // 3d800000       lis     r12, 0
  // 618c0000       ori     r12, r12, 0
  // 798c07c6       rldicr  r12, r12, 32, 31
  // 658c00c3       oris    r12, r12, 195
  // 618ccd40       ori     r12, r12, 52544
  return (((instr1 >> 16) == 0x3d80) && ((instr2 >> 16) == 0x618c) &&
          (instr3 == 0x798c07c6) && ((instr4 >> 16) == 0x658c) &&
          ((instr5 >> 16) == 0x618c));
}
#else
// This code assumes a FIXED_SEQUENCE for 32bit loads (lis/ori)
bool Assembler::Is32BitLoadIntoR12(Instr instr1, Instr instr2) {
  // Check the instruction is indeed a two part load (into r12)
  // 3d802553       lis     r12, 9555
  // 618c5000       ori   r12, r12, 20480
  return (((instr1 >> 16) == 0x3d80) && ((instr2 >> 16) == 0x618c));
}
#endif


bool Assembler::IsCmpRegister(Instr instr) {
  return (((instr & kOpcodeMask) == EXT2) &&
          ((instr & kExt2OpcodeMask) == CMP));
}


bool Assembler::IsRlwinm(Instr instr) {
  return ((instr & kOpcodeMask) == RLWINMX);
}


bool Assembler::IsAndi(Instr instr) { return ((instr & kOpcodeMask) == ANDIx); }


#if V8_TARGET_ARCH_PPC64
bool Assembler::IsRldicl(Instr instr) {
  return (((instr & kOpcodeMask) == EXT5) &&
          ((instr & kExt5OpcodeMask) == RLDICL));
}
#endif


bool Assembler::IsCmpImmediate(Instr instr) {
  return ((instr & kOpcodeMask) == CMPI);
}


bool Assembler::IsCrSet(Instr instr) {
  return (((instr & kOpcodeMask) == EXT1) &&
          ((instr & kExt1OpcodeMask) == CREQV));
}


Register Assembler::GetCmpImmediateRegister(Instr instr) {
  DCHECK(IsCmpImmediate(instr));
  return GetRA(instr);
}


int Assembler::GetCmpImmediateRawImmediate(Instr instr) {
  DCHECK(IsCmpImmediate(instr));
  return instr & kOff16Mask;
}


// Labels refer to positions in the (to be) generated code.
// There are bound, linked, and unused labels.
//
// Bound labels refer to known positions in the already
// generated code. pos() is the position the label refers to.
//
// Linked labels refer to unknown positions in the code
// to be generated; pos() is the position of the last
// instruction using the label.


// The link chain is terminated by a negative code position (must be aligned)
const int kEndOfChain = -4;


// Dummy opcodes for unbound label mov instructions or jump table entries.
enum {
  kUnboundMovLabelOffsetOpcode = 0 << 26,
  kUnboundAddLabelOffsetOpcode = 1 << 26,
  kUnboundMovLabelAddrOpcode = 2 << 26,
  kUnboundJumpTableEntryOpcode = 3 << 26
};


int Assembler::target_at(int pos) {
  Instr instr = instr_at(pos);
  // check which type of branch this is 16 or 26 bit offset
  int opcode = instr & kOpcodeMask;
  int link;
  switch (opcode) {
    case BX:
      link = SIGN_EXT_IMM26(instr & kImm26Mask);
      link &= ~(kAAMask | kLKMask);  // discard AA|LK bits if present
      break;
    case BCX:
      link = SIGN_EXT_IMM16((instr & kImm16Mask));
      link &= ~(kAAMask | kLKMask);  // discard AA|LK bits if present
      break;
    case kUnboundMovLabelOffsetOpcode:
    case kUnboundAddLabelOffsetOpcode:
    case kUnboundMovLabelAddrOpcode:
    case kUnboundJumpTableEntryOpcode:
      link = SIGN_EXT_IMM26(instr & kImm26Mask);
      link <<= 2;
      break;
    default:
      DCHECK(false);
      return -1;
  }

  if (link == 0) return kEndOfChain;
  return pos + link;
}


void Assembler::target_at_put(int pos, int target_pos, bool* is_branch) {
  Instr instr = instr_at(pos);
  int opcode = instr & kOpcodeMask;

  if (is_branch != nullptr) {
    *is_branch = (opcode == BX || opcode == BCX);
  }

  switch (opcode) {
    case BX: {
      int imm26 = target_pos - pos;
      CHECK(is_int26(imm26) && (imm26 & (kAAMask | kLKMask)) == 0);
      if (imm26 == kInstrSize && !(instr & kLKMask)) {
        // Branch to next instr without link.
        instr = ORI;  // nop: ori, 0,0,0
      } else {
        instr &= ((~kImm26Mask) | kAAMask | kLKMask);
        instr |= (imm26 & kImm26Mask);
      }
      instr_at_put(pos, instr);
      break;
    }
    case BCX: {
      int imm16 = target_pos - pos;
      CHECK(is_int16(imm16) && (imm16 & (kAAMask | kLKMask)) == 0);
      if (imm16 == kInstrSize && !(instr & kLKMask)) {
        // Branch to next instr without link.
        instr = ORI;  // nop: ori, 0,0,0
      } else {
        instr &= ((~kImm16Mask) | kAAMask | kLKMask);
        instr |= (imm16 & kImm16Mask);
      }
      instr_at_put(pos, instr);
      break;
    }
    case kUnboundMovLabelOffsetOpcode: {
      // Load the position of the label relative to the generated code object
      // pointer in a register.
      Register dst = Register::from_code(instr_at(pos + kInstrSize));
      int32_t offset = target_pos + (Code::kHeaderSize - kHeapObjectTag);
      CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos), 2,
                          CodePatcher::DONT_FLUSH);
      patcher.masm()->bitwise_mov32(dst, offset);
      break;
    }
    case kUnboundAddLabelOffsetOpcode: {
      // dst = base + position + immediate
      Instr operands = instr_at(pos + kInstrSize);
      Register dst = Register::from_code((operands >> 21) & 0x1f);
      Register base = Register::from_code((operands >> 16) & 0x1f);
      int32_t offset = target_pos + SIGN_EXT_IMM16(operands & kImm16Mask);
      CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos), 2,
                          CodePatcher::DONT_FLUSH);
      patcher.masm()->bitwise_add32(dst, base, offset);
      break;
    }
    case kUnboundMovLabelAddrOpcode: {
      // Load the address of the label in a register.
      Register dst = Register::from_code(instr_at(pos + kInstrSize));
      CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos),
                          kMovInstructionsNoConstantPool,
                          CodePatcher::DONT_FLUSH);
      // Keep internal references relative until EmitRelocations.
      patcher.masm()->bitwise_mov(dst, target_pos);
      break;
    }
    case kUnboundJumpTableEntryOpcode: {
      CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos),
                          kPointerSize / kInstrSize, CodePatcher::DONT_FLUSH);
      // Keep internal references relative until EmitRelocations.
      patcher.masm()->dp(target_pos);
      break;
    }
    default:
      DCHECK(false);
      break;
  }
}


int Assembler::max_reach_from(int pos) {
  Instr instr = instr_at(pos);
  int opcode = instr & kOpcodeMask;

  // check which type of branch this is 16 or 26 bit offset
  switch (opcode) {
    case BX:
      return 26;
    case BCX:
      return 16;
    case kUnboundMovLabelOffsetOpcode:
    case kUnboundAddLabelOffsetOpcode:
    case kUnboundMovLabelAddrOpcode:
    case kUnboundJumpTableEntryOpcode:
      return 0;  // no limit on reach
  }

  DCHECK(false);
  return 0;
}


void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // must have a valid binding position
  int32_t trampoline_pos = kInvalidSlotPos;
  bool is_branch = false;
  while (L->is_linked()) {
    int fixup_pos = L->pos();
    int32_t offset = pos - fixup_pos;
    int maxReach = max_reach_from(fixup_pos);
    next(L);  // call next before overwriting link with target at fixup_pos
    if (maxReach && is_intn(offset, maxReach) == false) {
      if (trampoline_pos == kInvalidSlotPos) {
        trampoline_pos = get_trampoline_entry();
        CHECK(trampoline_pos != kInvalidSlotPos);
        target_at_put(trampoline_pos, pos);
      }
      target_at_put(fixup_pos, trampoline_pos);
    } else {
      target_at_put(fixup_pos, pos, &is_branch);
    }
  }
  L->bind_to(pos);

  if (!trampoline_emitted_ && is_branch) {
    UntrackBranch();
  }

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_) last_bound_pos_ = pos;
}


void Assembler::bind(Label* L) {
  DCHECK(!L->is_bound());  // label can only be bound once
  bind_to(L, pc_offset());
}


void Assembler::next(Label* L) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos());
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    DCHECK(link >= 0);
    L->link_to(link);
  }
}


bool Assembler::is_near(Label* L, Condition cond) {
  DCHECK(L->is_bound());
  if (L->is_bound() == false) return false;

  int maxReach = ((cond == al) ? 26 : 16);
  int offset = L->pos() - pc_offset();

  return is_intn(offset, maxReach);
}


void Assembler::a_form(Instr instr, DoubleRegister frt, DoubleRegister fra,
                       DoubleRegister frb, RCBit r) {
  emit(instr | frt.code() * B21 | fra.code() * B16 | frb.code() * B11 | r);
}


void Assembler::d_form(Instr instr, Register rt, Register ra,
                       const intptr_t val, bool signed_disp) {
  if (signed_disp) {
    if (!is_int16(val)) {
      PrintF("val = %" V8PRIdPTR ", 0x%" V8PRIxPTR "\n", val, val);
    }
    CHECK(is_int16(val));
  } else {
    if (!is_uint16(val)) {
      PrintF("val = %" V8PRIdPTR ", 0x%" V8PRIxPTR
             ", is_unsigned_imm16(val)=%d, kImm16Mask=0x%x\n",
             val, val, is_uint16(val), kImm16Mask);
    }
    CHECK(is_uint16(val));
  }
  emit(instr | rt.code() * B21 | ra.code() * B16 | (kImm16Mask & val));
}


void Assembler::x_form(Instr instr, Register ra, Register rs, Register rb,
                       RCBit r) {
  emit(instr | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 | r);
}


void Assembler::xo_form(Instr instr, Register rt, Register ra, Register rb,
                        OEBit o, RCBit r) {
  emit(instr | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 | o | r);
}


void Assembler::md_form(Instr instr, Register ra, Register rs, int shift,
                        int maskbit, RCBit r) {
  int sh0_4 = shift & 0x1f;
  int sh5 = (shift >> 5) & 0x1;
  int m0_4 = maskbit & 0x1f;
  int m5 = (maskbit >> 5) & 0x1;

  emit(instr | rs.code() * B21 | ra.code() * B16 | sh0_4 * B11 | m0_4 * B6 |
       m5 * B5 | sh5 * B1 | r);
}


void Assembler::mds_form(Instr instr, Register ra, Register rs, Register rb,
                         int maskbit, RCBit r) {
  int m0_4 = maskbit & 0x1f;
  int m5 = (maskbit >> 5) & 0x1;

  emit(instr | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 | m0_4 * B6 |
       m5 * B5 | r);
}


// Returns the next free trampoline entry.
int32_t Assembler::get_trampoline_entry() {
  int32_t trampoline_entry = kInvalidSlotPos;

  if (!internal_trampoline_exception_) {
    trampoline_entry = trampoline_.take_slot();

    if (kInvalidSlotPos == trampoline_entry) {
      internal_trampoline_exception_ = true;
    }
  }
  return trampoline_entry;
}


int Assembler::link(Label* L) {
  int position;
  if (L->is_bound()) {
    position = L->pos();
  } else {
    if (L->is_linked()) {
      position = L->pos();  // L's link
    } else {
      // was: target_pos = kEndOfChain;
      // However, using self to mark the first reference
      // should avoid most instances of branch offset overflow.  See
      // target_at() for where this is converted back to kEndOfChain.
      position = pc_offset();
    }
    L->link_to(pc_offset());
  }

  return position;
}


// Branch instructions.


void Assembler::bclr(BOfield bo, int condition_bit, LKBit lk) {
  emit(EXT1 | bo | condition_bit * B16 | BCLRX | lk);
}


void Assembler::bcctr(BOfield bo, int condition_bit, LKBit lk) {
  emit(EXT1 | bo | condition_bit * B16 | BCCTRX | lk);
}


// Pseudo op - branch to link register
void Assembler::blr() { bclr(BA, 0, LeaveLK); }


// Pseudo op - branch to count register -- used for "jump"
void Assembler::bctr() { bcctr(BA, 0, LeaveLK); }


void Assembler::bctrl() { bcctr(BA, 0, SetLK); }


void Assembler::bc(int branch_offset, BOfield bo, int condition_bit, LKBit lk) {
  int imm16 = branch_offset;
  CHECK(is_int16(imm16) && (imm16 & (kAAMask | kLKMask)) == 0);
  emit(BCX | bo | condition_bit * B16 | (imm16 & kImm16Mask) | lk);
}


void Assembler::b(int branch_offset, LKBit lk) {
  int imm26 = branch_offset;
  CHECK(is_int26(imm26) && (imm26 & (kAAMask | kLKMask)) == 0);
  emit(BX | (imm26 & kImm26Mask) | lk);
}


void Assembler::xori(Register dst, Register src, const Operand& imm) {
  d_form(XORI, src, dst, imm.imm_, false);
}


void Assembler::xoris(Register ra, Register rs, const Operand& imm) {
  d_form(XORIS, rs, ra, imm.imm_, false);
}


void Assembler::xor_(Register dst, Register src1, Register src2, RCBit rc) {
  x_form(EXT2 | XORX, dst, src1, src2, rc);
}


void Assembler::cntlzw_(Register ra, Register rs, RCBit rc) {
  x_form(EXT2 | CNTLZWX, ra, rs, r0, rc);
}


void Assembler::popcntw(Register ra, Register rs) {
  emit(EXT2 | POPCNTW | rs.code() * B21 | ra.code() * B16);
}


void Assembler::and_(Register ra, Register rs, Register rb, RCBit rc) {
  x_form(EXT2 | ANDX, ra, rs, rb, rc);
}


void Assembler::rlwinm(Register ra, Register rs, int sh, int mb, int me,
                       RCBit rc) {
  sh &= 0x1f;
  mb &= 0x1f;
  me &= 0x1f;
  emit(RLWINMX | rs.code() * B21 | ra.code() * B16 | sh * B11 | mb * B6 |
       me << 1 | rc);
}


void Assembler::rlwnm(Register ra, Register rs, Register rb, int mb, int me,
                      RCBit rc) {
  mb &= 0x1f;
  me &= 0x1f;
  emit(RLWNMX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 | mb * B6 |
       me << 1 | rc);
}


void Assembler::rlwimi(Register ra, Register rs, int sh, int mb, int me,
                       RCBit rc) {
  sh &= 0x1f;
  mb &= 0x1f;
  me &= 0x1f;
  emit(RLWIMIX | rs.code() * B21 | ra.code() * B16 | sh * B11 | mb * B6 |
       me << 1 | rc);
}


void Assembler::slwi(Register dst, Register src, const Operand& val, RCBit rc) {
  DCHECK((32 > val.imm_) && (val.imm_ >= 0));
  rlwinm(dst, src, val.imm_, 0, 31 - val.imm_, rc);
}


void Assembler::srwi(Register dst, Register src, const Operand& val, RCBit rc) {
  DCHECK((32 > val.imm_) && (val.imm_ >= 0));
  rlwinm(dst, src, 32 - val.imm_, val.imm_, 31, rc);
}


void Assembler::clrrwi(Register dst, Register src, const Operand& val,
                       RCBit rc) {
  DCHECK((32 > val.imm_) && (val.imm_ >= 0));
  rlwinm(dst, src, 0, 0, 31 - val.imm_, rc);
}


void Assembler::clrlwi(Register dst, Register src, const Operand& val,
                       RCBit rc) {
  DCHECK((32 > val.imm_) && (val.imm_ >= 0));
  rlwinm(dst, src, 0, val.imm_, 31, rc);
}


void Assembler::srawi(Register ra, Register rs, int sh, RCBit r) {
  emit(EXT2 | SRAWIX | rs.code() * B21 | ra.code() * B16 | sh * B11 | r);
}


void Assembler::srw(Register dst, Register src1, Register src2, RCBit r) {
  x_form(EXT2 | SRWX, dst, src1, src2, r);
}


void Assembler::slw(Register dst, Register src1, Register src2, RCBit r) {
  x_form(EXT2 | SLWX, dst, src1, src2, r);
}


void Assembler::sraw(Register ra, Register rs, Register rb, RCBit r) {
  x_form(EXT2 | SRAW, ra, rs, rb, r);
}


void Assembler::rotlw(Register ra, Register rs, Register rb, RCBit r) {
  rlwnm(ra, rs, rb, 0, 31, r);
}


void Assembler::rotlwi(Register ra, Register rs, int sh, RCBit r) {
  rlwinm(ra, rs, sh, 0, 31, r);
}


void Assembler::rotrwi(Register ra, Register rs, int sh, RCBit r) {
  rlwinm(ra, rs, 32 - sh, 0, 31, r);
}


void Assembler::subi(Register dst, Register src, const Operand& imm) {
  addi(dst, src, Operand(-(imm.imm_)));
}

void Assembler::addc(Register dst, Register src1, Register src2, OEBit o,
                     RCBit r) {
  xo_form(EXT2 | ADDCX, dst, src1, src2, o, r);
}

void Assembler::adde(Register dst, Register src1, Register src2, OEBit o,
                     RCBit r) {
  xo_form(EXT2 | ADDEX, dst, src1, src2, o, r);
}

void Assembler::addze(Register dst, Register src1, OEBit o, RCBit r) {
  // a special xo_form
  emit(EXT2 | ADDZEX | dst.code() * B21 | src1.code() * B16 | o | r);
}


void Assembler::sub(Register dst, Register src1, Register src2, OEBit o,
                    RCBit r) {
  xo_form(EXT2 | SUBFX, dst, src2, src1, o, r);
}

void Assembler::subc(Register dst, Register src1, Register src2, OEBit o,
                     RCBit r) {
  xo_form(EXT2 | SUBFCX, dst, src2, src1, o, r);
}

void Assembler::sube(Register dst, Register src1, Register src2, OEBit o,
                     RCBit r) {
  xo_form(EXT2 | SUBFEX, dst, src2, src1, o, r);
}

void Assembler::subfic(Register dst, Register src, const Operand& imm) {
  d_form(SUBFIC, dst, src, imm.imm_, true);
}


void Assembler::add(Register dst, Register src1, Register src2, OEBit o,
                    RCBit r) {
  xo_form(EXT2 | ADDX, dst, src1, src2, o, r);
}


// Multiply low word
void Assembler::mullw(Register dst, Register src1, Register src2, OEBit o,
                      RCBit r) {
  xo_form(EXT2 | MULLW, dst, src1, src2, o, r);
}


// Multiply hi word
void Assembler::mulhw(Register dst, Register src1, Register src2, RCBit r) {
  xo_form(EXT2 | MULHWX, dst, src1, src2, LeaveOE, r);
}


// Multiply hi word unsigned
void Assembler::mulhwu(Register dst, Register src1, Register src2, RCBit r) {
  xo_form(EXT2 | MULHWUX, dst, src1, src2, LeaveOE, r);
}


// Divide word
void Assembler::divw(Register dst, Register src1, Register src2, OEBit o,
                     RCBit r) {
  xo_form(EXT2 | DIVW, dst, src1, src2, o, r);
}


// Divide word unsigned
void Assembler::divwu(Register dst, Register src1, Register src2, OEBit o,
                      RCBit r) {
  xo_form(EXT2 | DIVWU, dst, src1, src2, o, r);
}


void Assembler::addi(Register dst, Register src, const Operand& imm) {
  DCHECK(!src.is(r0));  // use li instead to show intent
  d_form(ADDI, dst, src, imm.imm_, true);
}


void Assembler::addis(Register dst, Register src, const Operand& imm) {
  DCHECK(!src.is(r0));  // use lis instead to show intent
  d_form(ADDIS, dst, src, imm.imm_, true);
}


void Assembler::addic(Register dst, Register src, const Operand& imm) {
  d_form(ADDIC, dst, src, imm.imm_, true);
}


void Assembler::andi(Register ra, Register rs, const Operand& imm) {
  d_form(ANDIx, rs, ra, imm.imm_, false);
}


void Assembler::andis(Register ra, Register rs, const Operand& imm) {
  d_form(ANDISx, rs, ra, imm.imm_, false);
}


void Assembler::nor(Register dst, Register src1, Register src2, RCBit r) {
  x_form(EXT2 | NORX, dst, src1, src2, r);
}


void Assembler::notx(Register dst, Register src, RCBit r) {
  x_form(EXT2 | NORX, dst, src, src, r);
}


void Assembler::ori(Register ra, Register rs, const Operand& imm) {
  d_form(ORI, rs, ra, imm.imm_, false);
}


void Assembler::oris(Register dst, Register src, const Operand& imm) {
  d_form(ORIS, src, dst, imm.imm_, false);
}


void Assembler::orx(Register dst, Register src1, Register src2, RCBit rc) {
  x_form(EXT2 | ORX, dst, src1, src2, rc);
}


void Assembler::orc(Register dst, Register src1, Register src2, RCBit rc) {
  x_form(EXT2 | ORC, dst, src1, src2, rc);
}


void Assembler::cmpi(Register src1, const Operand& src2, CRegister cr) {
  intptr_t imm16 = src2.imm_;
#if V8_TARGET_ARCH_PPC64
  int L = 1;
#else
  int L = 0;
#endif
  DCHECK(is_int16(imm16));
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  imm16 &= kImm16Mask;
  emit(CMPI | cr.code() * B23 | L * B21 | src1.code() * B16 | imm16);
}


void Assembler::cmpli(Register src1, const Operand& src2, CRegister cr) {
  uintptr_t uimm16 = src2.imm_;
#if V8_TARGET_ARCH_PPC64
  int L = 1;
#else
  int L = 0;
#endif
  DCHECK(is_uint16(uimm16));
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  uimm16 &= kImm16Mask;
  emit(CMPLI | cr.code() * B23 | L * B21 | src1.code() * B16 | uimm16);
}


void Assembler::cmp(Register src1, Register src2, CRegister cr) {
#if V8_TARGET_ARCH_PPC64
  int L = 1;
#else
  int L = 0;
#endif
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  emit(EXT2 | CMP | cr.code() * B23 | L * B21 | src1.code() * B16 |
       src2.code() * B11);
}


void Assembler::cmpl(Register src1, Register src2, CRegister cr) {
#if V8_TARGET_ARCH_PPC64
  int L = 1;
#else
  int L = 0;
#endif
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  emit(EXT2 | CMPL | cr.code() * B23 | L * B21 | src1.code() * B16 |
       src2.code() * B11);
}


void Assembler::cmpwi(Register src1, const Operand& src2, CRegister cr) {
  intptr_t imm16 = src2.imm_;
  int L = 0;
  int pos = pc_offset();
  DCHECK(is_int16(imm16));
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  imm16 &= kImm16Mask;

  // For cmpwi against 0, save postition and cr for later examination
  // of potential optimization.
  if (imm16 == 0 && pos > 0 && last_bound_pos_ != pos) {
    optimizable_cmpi_pos_ = pos;
    cmpi_cr_ = cr;
  }
  emit(CMPI | cr.code() * B23 | L * B21 | src1.code() * B16 | imm16);
}


void Assembler::cmplwi(Register src1, const Operand& src2, CRegister cr) {
  uintptr_t uimm16 = src2.imm_;
  int L = 0;
  DCHECK(is_uint16(uimm16));
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  uimm16 &= kImm16Mask;
  emit(CMPLI | cr.code() * B23 | L * B21 | src1.code() * B16 | uimm16);
}


void Assembler::cmpw(Register src1, Register src2, CRegister cr) {
  int L = 0;
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  emit(EXT2 | CMP | cr.code() * B23 | L * B21 | src1.code() * B16 |
       src2.code() * B11);
}


void Assembler::cmplw(Register src1, Register src2, CRegister cr) {
  int L = 0;
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  emit(EXT2 | CMPL | cr.code() * B23 | L * B21 | src1.code() * B16 |
       src2.code() * B11);
}


void Assembler::isel(Register rt, Register ra, Register rb, int cb) {
  emit(EXT2 | ISEL | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       cb * B6);
}


// Pseudo op - load immediate
void Assembler::li(Register dst, const Operand& imm) {
  d_form(ADDI, dst, r0, imm.imm_, true);
}


void Assembler::lis(Register dst, const Operand& imm) {
  d_form(ADDIS, dst, r0, imm.imm_, true);
}


// Pseudo op - move register
void Assembler::mr(Register dst, Register src) {
  // actually or(dst, src, src)
  orx(dst, src, src);
}


void Assembler::lbz(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(LBZ, dst, src.ra(), src.offset(), true);
}


void Assembler::lbzx(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LBZX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lbzux(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LBZUX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lhz(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(LHZ, dst, src.ra(), src.offset(), true);
}


void Assembler::lhzx(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LHZX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lhzux(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LHZUX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lhax(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LHAX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11);
}


void Assembler::lwz(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(LWZ, dst, src.ra(), src.offset(), true);
}


void Assembler::lwzu(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(LWZU, dst, src.ra(), src.offset(), true);
}


void Assembler::lwzx(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LWZX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lwzux(Register rt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LWZUX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lha(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(LHA, dst, src.ra(), src.offset(), true);
}


void Assembler::lwa(Register dst, const MemOperand& src) {
#if V8_TARGET_ARCH_PPC64
  int offset = src.offset();
  DCHECK(!src.ra_.is(r0));
  CHECK(!(offset & 3) && is_int16(offset));
  offset = kImm16Mask & offset;
  emit(LD | dst.code() * B21 | src.ra().code() * B16 | offset | 2);
#else
  lwz(dst, src);
#endif
}


void Assembler::lwax(Register rt, const MemOperand& src) {
#if V8_TARGET_ARCH_PPC64
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LWAX | rt.code() * B21 | ra.code() * B16 | rb.code() * B11);
#else
  lwzx(rt, src);
#endif
}


void Assembler::ldbrx(Register dst, const MemOperand& src) {
  x_form(EXT2 | LDBRX, src.ra(), dst, src.rb(), LeaveRC);
}


void Assembler::lwbrx(Register dst, const MemOperand& src) {
  x_form(EXT2 | LWBRX, src.ra(), dst, src.rb(), LeaveRC);
}


void Assembler::lhbrx(Register dst, const MemOperand& src) {
  x_form(EXT2 | LHBRX, src.ra(), dst, src.rb(), LeaveRC);
}


void Assembler::stb(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(STB, dst, src.ra(), src.offset(), true);
}


void Assembler::stbx(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STBX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stbux(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STBUX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::sth(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(STH, dst, src.ra(), src.offset(), true);
}


void Assembler::sthx(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STHX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::sthux(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STHUX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stw(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(STW, dst, src.ra(), src.offset(), true);
}


void Assembler::stwu(Register dst, const MemOperand& src) {
  DCHECK(!src.ra_.is(r0));
  d_form(STWU, dst, src.ra(), src.offset(), true);
}


void Assembler::stwx(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STWX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stwux(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STWUX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::extsb(Register rs, Register ra, RCBit rc) {
  emit(EXT2 | EXTSB | ra.code() * B21 | rs.code() * B16 | rc);
}


void Assembler::extsh(Register rs, Register ra, RCBit rc) {
  emit(EXT2 | EXTSH | ra.code() * B21 | rs.code() * B16 | rc);
}


void Assembler::extsw(Register rs, Register ra, RCBit rc) {
#if V8_TARGET_ARCH_PPC64
  emit(EXT2 | EXTSW | ra.code() * B21 | rs.code() * B16 | rc);
#else
  // nop on 32-bit
  DCHECK(rs.is(ra) && rc == LeaveRC);
#endif
}


void Assembler::neg(Register rt, Register ra, OEBit o, RCBit r) {
  emit(EXT2 | NEGX | rt.code() * B21 | ra.code() * B16 | o | r);
}


void Assembler::andc(Register dst, Register src1, Register src2, RCBit rc) {
  x_form(EXT2 | ANDCX, dst, src1, src2, rc);
}


#if V8_TARGET_ARCH_PPC64
// 64bit specific instructions
void Assembler::ld(Register rd, const MemOperand& src) {
  int offset = src.offset();
  DCHECK(!src.ra_.is(r0));
  CHECK(!(offset & 3) && is_int16(offset));
  offset = kImm16Mask & offset;
  emit(LD | rd.code() * B21 | src.ra().code() * B16 | offset);
}


void Assembler::ldx(Register rd, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LDX | rd.code() * B21 | ra.code() * B16 | rb.code() * B11);
}


void Assembler::ldu(Register rd, const MemOperand& src) {
  int offset = src.offset();
  DCHECK(!src.ra_.is(r0));
  CHECK(!(offset & 3) && is_int16(offset));
  offset = kImm16Mask & offset;
  emit(LD | rd.code() * B21 | src.ra().code() * B16 | offset | 1);
}


void Assembler::ldux(Register rd, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LDUX | rd.code() * B21 | ra.code() * B16 | rb.code() * B11);
}


void Assembler::std(Register rs, const MemOperand& src) {
  int offset = src.offset();
  DCHECK(!src.ra_.is(r0));
  CHECK(!(offset & 3) && is_int16(offset));
  offset = kImm16Mask & offset;
  emit(STD | rs.code() * B21 | src.ra().code() * B16 | offset);
}


void Assembler::stdx(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STDX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11);
}


void Assembler::stdu(Register rs, const MemOperand& src) {
  int offset = src.offset();
  DCHECK(!src.ra_.is(r0));
  CHECK(!(offset & 3) && is_int16(offset));
  offset = kImm16Mask & offset;
  emit(STD | rs.code() * B21 | src.ra().code() * B16 | offset | 1);
}


void Assembler::stdux(Register rs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STDUX | rs.code() * B21 | ra.code() * B16 | rb.code() * B11);
}


void Assembler::rldic(Register ra, Register rs, int sh, int mb, RCBit r) {
  md_form(EXT5 | RLDIC, ra, rs, sh, mb, r);
}


void Assembler::rldicl(Register ra, Register rs, int sh, int mb, RCBit r) {
  md_form(EXT5 | RLDICL, ra, rs, sh, mb, r);
}


void Assembler::rldcl(Register ra, Register rs, Register rb, int mb, RCBit r) {
  mds_form(EXT5 | RLDCL, ra, rs, rb, mb, r);
}


void Assembler::rldicr(Register ra, Register rs, int sh, int me, RCBit r) {
  md_form(EXT5 | RLDICR, ra, rs, sh, me, r);
}


void Assembler::sldi(Register dst, Register src, const Operand& val, RCBit rc) {
  DCHECK((64 > val.imm_) && (val.imm_ >= 0));
  rldicr(dst, src, val.imm_, 63 - val.imm_, rc);
}


void Assembler::srdi(Register dst, Register src, const Operand& val, RCBit rc) {
  DCHECK((64 > val.imm_) && (val.imm_ >= 0));
  rldicl(dst, src, 64 - val.imm_, val.imm_, rc);
}


void Assembler::clrrdi(Register dst, Register src, const Operand& val,
                       RCBit rc) {
  DCHECK((64 > val.imm_) && (val.imm_ >= 0));
  rldicr(dst, src, 0, 63 - val.imm_, rc);
}


void Assembler::clrldi(Register dst, Register src, const Operand& val,
                       RCBit rc) {
  DCHECK((64 > val.imm_) && (val.imm_ >= 0));
  rldicl(dst, src, 0, val.imm_, rc);
}


void Assembler::rldimi(Register ra, Register rs, int sh, int mb, RCBit r) {
  md_form(EXT5 | RLDIMI, ra, rs, sh, mb, r);
}


void Assembler::sradi(Register ra, Register rs, int sh, RCBit r) {
  int sh0_4 = sh & 0x1f;
  int sh5 = (sh >> 5) & 0x1;

  emit(EXT2 | SRADIX | rs.code() * B21 | ra.code() * B16 | sh0_4 * B11 |
       sh5 * B1 | r);
}


void Assembler::srd(Register dst, Register src1, Register src2, RCBit r) {
  x_form(EXT2 | SRDX, dst, src1, src2, r);
}


void Assembler::sld(Register dst, Register src1, Register src2, RCBit r) {
  x_form(EXT2 | SLDX, dst, src1, src2, r);
}


void Assembler::srad(Register ra, Register rs, Register rb, RCBit r) {
  x_form(EXT2 | SRAD, ra, rs, rb, r);
}


void Assembler::rotld(Register ra, Register rs, Register rb, RCBit r) {
  rldcl(ra, rs, rb, 0, r);
}


void Assembler::rotldi(Register ra, Register rs, int sh, RCBit r) {
  rldicl(ra, rs, sh, 0, r);
}


void Assembler::rotrdi(Register ra, Register rs, int sh, RCBit r) {
  rldicl(ra, rs, 64 - sh, 0, r);
}


void Assembler::cntlzd_(Register ra, Register rs, RCBit rc) {
  x_form(EXT2 | CNTLZDX, ra, rs, r0, rc);
}


void Assembler::popcntd(Register ra, Register rs) {
  emit(EXT2 | POPCNTD | rs.code() * B21 | ra.code() * B16);
}


void Assembler::mulld(Register dst, Register src1, Register src2, OEBit o,
                      RCBit r) {
  xo_form(EXT2 | MULLD, dst, src1, src2, o, r);
}


void Assembler::divd(Register dst, Register src1, Register src2, OEBit o,
                     RCBit r) {
  xo_form(EXT2 | DIVD, dst, src1, src2, o, r);
}


void Assembler::divdu(Register dst, Register src1, Register src2, OEBit o,
                      RCBit r) {
  xo_form(EXT2 | DIVDU, dst, src1, src2, o, r);
}
#endif


// Function descriptor for AIX.
// Code address skips the function descriptor "header".
// TOC and static chain are ignored and set to 0.
void Assembler::function_descriptor() {
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    Label instructions;
    DCHECK(pc_offset() == 0);
    emit_label_addr(&instructions);
    dp(0);
    dp(0);
    bind(&instructions);
  }
}


int Assembler::instructions_required_for_mov(Register dst,
                                             const Operand& src) const {
  bool canOptimize =
      !(src.must_output_reloc_info(this) || is_trampoline_pool_blocked());
  if (use_constant_pool_for_mov(dst, src, canOptimize)) {
    if (ConstantPoolAccessIsInOverflow()) {
      return kMovInstructionsConstantPool + 1;
    }
    return kMovInstructionsConstantPool;
  }
  DCHECK(!canOptimize);
  return kMovInstructionsNoConstantPool;
}


bool Assembler::use_constant_pool_for_mov(Register dst, const Operand& src,
                                          bool canOptimize) const {
  if (!FLAG_enable_embedded_constant_pool || !is_constant_pool_available()) {
    // If there is no constant pool available, we must use a mov
    // immediate sequence.
    return false;
  }

  intptr_t value = src.immediate();
#if V8_TARGET_ARCH_PPC64
  bool allowOverflow = !((canOptimize && is_int32(value)) || dst.is(r0));
#else
  bool allowOverflow = !(canOptimize || dst.is(r0));
#endif
  if (canOptimize && is_int16(value)) {
    // Prefer a single-instruction load-immediate.
    return false;
  }
  if (!allowOverflow && ConstantPoolAccessIsInOverflow()) {
    // Prefer non-relocatable two-instruction bitwise-mov32 over
    // overflow sequence.
    return false;
  }

  return true;
}


void Assembler::EnsureSpaceFor(int space_needed) {
  if (buffer_space() <= (kGap + space_needed)) {
    GrowBuffer(space_needed);
  }
}


bool Operand::must_output_reloc_info(const Assembler* assembler) const {
  if (rmode_ == RelocInfo::EXTERNAL_REFERENCE) {
    if (assembler != NULL && assembler->predictable_code_size()) return true;
    return assembler->serializer_enabled();
  } else if (RelocInfo::IsNone(rmode_)) {
    return false;
  }
  return true;
}


// Primarily used for loading constants
// This should really move to be in macro-assembler as it
// is really a pseudo instruction
// Some usages of this intend for a FIXED_SEQUENCE to be used
// Todo - break this dependency so we can optimize mov() in general
// and only use the generic version when we require a fixed sequence
void Assembler::mov(Register dst, const Operand& src) {
  intptr_t value = src.immediate();
  bool relocatable = src.must_output_reloc_info(this);
  bool canOptimize;

  canOptimize =
      !(relocatable || (is_trampoline_pool_blocked() && !is_int16(value)));

  if (use_constant_pool_for_mov(dst, src, canOptimize)) {
    DCHECK(is_constant_pool_available());
    if (relocatable) {
      RecordRelocInfo(src.rmode_);
    }
    ConstantPoolEntry::Access access = ConstantPoolAddEntry(src.rmode_, value);
#if V8_TARGET_ARCH_PPC64
    if (access == ConstantPoolEntry::OVERFLOWED) {
      addis(dst, kConstantPoolRegister, Operand::Zero());
      ld(dst, MemOperand(dst, 0));
    } else {
      ld(dst, MemOperand(kConstantPoolRegister, 0));
    }
#else
    if (access == ConstantPoolEntry::OVERFLOWED) {
      addis(dst, kConstantPoolRegister, Operand::Zero());
      lwz(dst, MemOperand(dst, 0));
    } else {
      lwz(dst, MemOperand(kConstantPoolRegister, 0));
    }
#endif
    return;
  }

  if (canOptimize) {
    if (is_int16(value)) {
      li(dst, Operand(value));
    } else {
      uint16_t u16;
#if V8_TARGET_ARCH_PPC64
      if (is_int32(value)) {
#endif
        lis(dst, Operand(value >> 16));
#if V8_TARGET_ARCH_PPC64
      } else {
        if (is_int48(value)) {
          li(dst, Operand(value >> 32));
        } else {
          lis(dst, Operand(value >> 48));
          u16 = ((value >> 32) & 0xffff);
          if (u16) {
            ori(dst, dst, Operand(u16));
          }
        }
        sldi(dst, dst, Operand(32));
        u16 = ((value >> 16) & 0xffff);
        if (u16) {
          oris(dst, dst, Operand(u16));
        }
      }
#endif
      u16 = (value & 0xffff);
      if (u16) {
        ori(dst, dst, Operand(u16));
      }
    }
    return;
  }

  DCHECK(!canOptimize);
  if (relocatable) {
    RecordRelocInfo(src.rmode_);
  }
  bitwise_mov(dst, value);
}


void Assembler::bitwise_mov(Register dst, intptr_t value) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
#if V8_TARGET_ARCH_PPC64
    int32_t hi_32 = static_cast<int32_t>(value >> 32);
    int32_t lo_32 = static_cast<int32_t>(value);
    int hi_word = static_cast<int>(hi_32 >> 16);
    int lo_word = static_cast<int>(hi_32 & 0xffff);
    lis(dst, Operand(SIGN_EXT_IMM16(hi_word)));
    ori(dst, dst, Operand(lo_word));
    sldi(dst, dst, Operand(32));
    hi_word = static_cast<int>(((lo_32 >> 16) & 0xffff));
    lo_word = static_cast<int>(lo_32 & 0xffff);
    oris(dst, dst, Operand(hi_word));
    ori(dst, dst, Operand(lo_word));
#else
    int hi_word = static_cast<int>(value >> 16);
    int lo_word = static_cast<int>(value & 0xffff);
    lis(dst, Operand(SIGN_EXT_IMM16(hi_word)));
    ori(dst, dst, Operand(lo_word));
#endif
}


void Assembler::bitwise_mov32(Register dst, int32_t value) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  int hi_word = static_cast<int>(value >> 16);
  int lo_word = static_cast<int>(value & 0xffff);
  lis(dst, Operand(SIGN_EXT_IMM16(hi_word)));
  ori(dst, dst, Operand(lo_word));
}


void Assembler::bitwise_add32(Register dst, Register src, int32_t value) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (is_int16(value)) {
    addi(dst, src, Operand(value));
    nop();
  } else {
    int hi_word = static_cast<int>(value >> 16);
    int lo_word = static_cast<int>(value & 0xffff);
    if (lo_word & 0x8000) hi_word++;
    addis(dst, src, Operand(SIGN_EXT_IMM16(hi_word)));
    addic(dst, dst, Operand(SIGN_EXT_IMM16(lo_word)));
  }
}


void Assembler::mov_label_offset(Register dst, Label* label) {
  int position = link(label);
  if (label->is_bound()) {
    // Load the position of the label relative to the generated code object.
    mov(dst, Operand(position + Code::kHeaderSize - kHeapObjectTag));
  } else {
    // Encode internal reference to unbound label. We use a dummy opcode
    // such that it won't collide with any opcode that might appear in the
    // label's chain.  Encode the destination register in the 2nd instruction.
    int link = position - pc_offset();
    DCHECK_EQ(0, link & 3);
    link >>= 2;
    DCHECK(is_int26(link));

    // When the label is bound, these instructions will be patched
    // with a 2 instruction mov sequence that will load the
    // destination register with the position of the label from the
    // beginning of the code.
    //
    // target_at extracts the link and target_at_put patches the instructions.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    emit(kUnboundMovLabelOffsetOpcode | (link & kImm26Mask));
    emit(dst.code());
  }
}


void Assembler::add_label_offset(Register dst, Register base, Label* label,
                                 int delta) {
  int position = link(label);
  if (label->is_bound()) {
    // dst = base + position + delta
    position += delta;
    bitwise_add32(dst, base, position);
  } else {
    // Encode internal reference to unbound label. We use a dummy opcode
    // such that it won't collide with any opcode that might appear in the
    // label's chain.  Encode the operands in the 2nd instruction.
    int link = position - pc_offset();
    DCHECK_EQ(0, link & 3);
    link >>= 2;
    DCHECK(is_int26(link));
    DCHECK(is_int16(delta));

    BlockTrampolinePoolScope block_trampoline_pool(this);
    emit(kUnboundAddLabelOffsetOpcode | (link & kImm26Mask));
    emit(dst.code() * B21 | base.code() * B16 | (delta & kImm16Mask));
  }
}


void Assembler::mov_label_addr(Register dst, Label* label) {
  CheckBuffer();
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE_ENCODED);
  int position = link(label);
  if (label->is_bound()) {
    // Keep internal references relative until EmitRelocations.
    bitwise_mov(dst, position);
  } else {
    // Encode internal reference to unbound label. We use a dummy opcode
    // such that it won't collide with any opcode that might appear in the
    // label's chain.  Encode the destination register in the 2nd instruction.
    int link = position - pc_offset();
    DCHECK_EQ(0, link & 3);
    link >>= 2;
    DCHECK(is_int26(link));

    // When the label is bound, these instructions will be patched
    // with a multi-instruction mov sequence that will load the
    // destination register with the address of the label.
    //
    // target_at extracts the link and target_at_put patches the instructions.
    BlockTrampolinePoolScope block_trampoline_pool(this);
    emit(kUnboundMovLabelAddrOpcode | (link & kImm26Mask));
    emit(dst.code());
    DCHECK(kMovInstructionsNoConstantPool >= 2);
    for (int i = 0; i < kMovInstructionsNoConstantPool - 2; i++) nop();
  }
}


void Assembler::emit_label_addr(Label* label) {
  CheckBuffer();
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  int position = link(label);
  if (label->is_bound()) {
    // Keep internal references relative until EmitRelocations.
    dp(position);
  } else {
    // Encode internal reference to unbound label. We use a dummy opcode
    // such that it won't collide with any opcode that might appear in the
    // label's chain.
    int link = position - pc_offset();
    DCHECK_EQ(0, link & 3);
    link >>= 2;
    DCHECK(is_int26(link));

    // When the label is bound, the instruction(s) will be patched
    // as a jump table entry containing the label address.  target_at extracts
    // the link and target_at_put patches the instruction(s).
    BlockTrampolinePoolScope block_trampoline_pool(this);
    emit(kUnboundJumpTableEntryOpcode | (link & kImm26Mask));
#if V8_TARGET_ARCH_PPC64
    nop();
#endif
  }
}


// Special register instructions
void Assembler::crxor(int bt, int ba, int bb) {
  emit(EXT1 | CRXOR | bt * B21 | ba * B16 | bb * B11);
}


void Assembler::creqv(int bt, int ba, int bb) {
  emit(EXT1 | CREQV | bt * B21 | ba * B16 | bb * B11);
}


void Assembler::mflr(Register dst) {
  emit(EXT2 | MFSPR | dst.code() * B21 | 256 << 11);  // Ignore RC bit
}


void Assembler::mtlr(Register src) {
  emit(EXT2 | MTSPR | src.code() * B21 | 256 << 11);  // Ignore RC bit
}


void Assembler::mtctr(Register src) {
  emit(EXT2 | MTSPR | src.code() * B21 | 288 << 11);  // Ignore RC bit
}


void Assembler::mtxer(Register src) {
  emit(EXT2 | MTSPR | src.code() * B21 | 32 << 11);
}


void Assembler::mcrfs(CRegister cr, FPSCRBit bit) {
  DCHECK(static_cast<int>(bit) < 32);
  int bf = cr.code();
  int bfa = bit / CRWIDTH;
  emit(EXT4 | MCRFS | bf * B23 | bfa * B18);
}


void Assembler::mfcr(Register dst) { emit(EXT2 | MFCR | dst.code() * B21); }


#if V8_TARGET_ARCH_PPC64
void Assembler::mffprd(Register dst, DoubleRegister src) {
  emit(EXT2 | MFVSRD | src.code() * B21 | dst.code() * B16);
}


void Assembler::mffprwz(Register dst, DoubleRegister src) {
  emit(EXT2 | MFVSRWZ | src.code() * B21 | dst.code() * B16);
}


void Assembler::mtfprd(DoubleRegister dst, Register src) {
  emit(EXT2 | MTVSRD | dst.code() * B21 | src.code() * B16);
}


void Assembler::mtfprwz(DoubleRegister dst, Register src) {
  emit(EXT2 | MTVSRWZ | dst.code() * B21 | src.code() * B16);
}


void Assembler::mtfprwa(DoubleRegister dst, Register src) {
  emit(EXT2 | MTVSRWA | dst.code() * B21 | src.code() * B16);
}
#endif


// Exception-generating instructions and debugging support.
// Stops with a non-negative code less than kNumOfWatchedStops support
// enabling/disabling and a counter feature. See simulator-ppc.h .
void Assembler::stop(const char* msg, Condition cond, int32_t code,
                     CRegister cr) {
  if (cond != al) {
    Label skip;
    b(NegateCondition(cond), &skip, cr);
    bkpt(0);
    bind(&skip);
  } else {
    bkpt(0);
  }
}


void Assembler::bkpt(uint32_t imm16) { emit(0x7d821008); }


void Assembler::dcbf(Register ra, Register rb) {
  emit(EXT2 | DCBF | ra.code() * B16 | rb.code() * B11);
}


void Assembler::sync() { emit(EXT2 | SYNC); }


void Assembler::lwsync() { emit(EXT2 | SYNC | 1 * B21); }


void Assembler::icbi(Register ra, Register rb) {
  emit(EXT2 | ICBI | ra.code() * B16 | rb.code() * B11);
}


void Assembler::isync() { emit(EXT1 | ISYNC); }


// Floating point support

void Assembler::lfd(const DoubleRegister frt, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  DCHECK(!ra.is(r0));
  CHECK(is_int16(offset));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(LFD | frt.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::lfdu(const DoubleRegister frt, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  DCHECK(!ra.is(r0));
  CHECK(is_int16(offset));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(LFDU | frt.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::lfdx(const DoubleRegister frt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LFDX | frt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lfdux(const DoubleRegister frt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LFDUX | frt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lfs(const DoubleRegister frt, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  CHECK(is_int16(offset));
  DCHECK(!ra.is(r0));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(LFS | frt.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::lfsu(const DoubleRegister frt, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  CHECK(is_int16(offset));
  DCHECK(!ra.is(r0));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(LFSU | frt.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::lfsx(const DoubleRegister frt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LFSX | frt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::lfsux(const DoubleRegister frt, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | LFSUX | frt.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stfd(const DoubleRegister frs, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  CHECK(is_int16(offset));
  DCHECK(!ra.is(r0));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(STFD | frs.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::stfdu(const DoubleRegister frs, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  CHECK(is_int16(offset));
  DCHECK(!ra.is(r0));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(STFDU | frs.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::stfdx(const DoubleRegister frs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STFDX | frs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stfdux(const DoubleRegister frs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STFDUX | frs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stfs(const DoubleRegister frs, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  CHECK(is_int16(offset));
  DCHECK(!ra.is(r0));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(STFS | frs.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::stfsu(const DoubleRegister frs, const MemOperand& src) {
  int offset = src.offset();
  Register ra = src.ra();
  CHECK(is_int16(offset));
  DCHECK(!ra.is(r0));
  int imm16 = offset & kImm16Mask;
  // could be x_form instruction with some casting magic
  emit(STFSU | frs.code() * B21 | ra.code() * B16 | imm16);
}


void Assembler::stfsx(const DoubleRegister frs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STFSX | frs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::stfsux(const DoubleRegister frs, const MemOperand& src) {
  Register ra = src.ra();
  Register rb = src.rb();
  DCHECK(!ra.is(r0));
  emit(EXT2 | STFSUX | frs.code() * B21 | ra.code() * B16 | rb.code() * B11 |
       LeaveRC);
}


void Assembler::fsub(const DoubleRegister frt, const DoubleRegister fra,
                     const DoubleRegister frb, RCBit rc) {
  a_form(EXT4 | FSUB, frt, fra, frb, rc);
}


void Assembler::fadd(const DoubleRegister frt, const DoubleRegister fra,
                     const DoubleRegister frb, RCBit rc) {
  a_form(EXT4 | FADD, frt, fra, frb, rc);
}


void Assembler::fmul(const DoubleRegister frt, const DoubleRegister fra,
                     const DoubleRegister frc, RCBit rc) {
  emit(EXT4 | FMUL | frt.code() * B21 | fra.code() * B16 | frc.code() * B6 |
       rc);
}


void Assembler::fdiv(const DoubleRegister frt, const DoubleRegister fra,
                     const DoubleRegister frb, RCBit rc) {
  a_form(EXT4 | FDIV, frt, fra, frb, rc);
}


void Assembler::fcmpu(const DoubleRegister fra, const DoubleRegister frb,
                      CRegister cr) {
  DCHECK(cr.code() >= 0 && cr.code() <= 7);
  emit(EXT4 | FCMPU | cr.code() * B23 | fra.code() * B16 | frb.code() * B11);
}


void Assembler::fmr(const DoubleRegister frt, const DoubleRegister frb,
                    RCBit rc) {
  emit(EXT4 | FMR | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fctiwz(const DoubleRegister frt, const DoubleRegister frb) {
  emit(EXT4 | FCTIWZ | frt.code() * B21 | frb.code() * B11);
}


void Assembler::fctiw(const DoubleRegister frt, const DoubleRegister frb) {
  emit(EXT4 | FCTIW | frt.code() * B21 | frb.code() * B11);
}


void Assembler::frin(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FRIN | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::friz(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FRIZ | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::frip(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FRIP | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::frim(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FRIM | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::frsp(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FRSP | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fcfid(const DoubleRegister frt, const DoubleRegister frb,
                      RCBit rc) {
  emit(EXT4 | FCFID | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fcfidu(const DoubleRegister frt, const DoubleRegister frb,
                       RCBit rc) {
  emit(EXT4 | FCFIDU | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fcfidus(const DoubleRegister frt, const DoubleRegister frb,
                        RCBit rc) {
  emit(EXT3 | FCFIDU | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fcfids(const DoubleRegister frt, const DoubleRegister frb,
                       RCBit rc) {
  emit(EXT3 | FCFID | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fctid(const DoubleRegister frt, const DoubleRegister frb,
                      RCBit rc) {
  emit(EXT4 | FCTID | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fctidz(const DoubleRegister frt, const DoubleRegister frb,
                       RCBit rc) {
  emit(EXT4 | FCTIDZ | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fctidu(const DoubleRegister frt, const DoubleRegister frb,
                       RCBit rc) {
  emit(EXT4 | FCTIDU | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fctiduz(const DoubleRegister frt, const DoubleRegister frb,
                        RCBit rc) {
  emit(EXT4 | FCTIDUZ | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fsel(const DoubleRegister frt, const DoubleRegister fra,
                     const DoubleRegister frc, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FSEL | frt.code() * B21 | fra.code() * B16 | frb.code() * B11 |
       frc.code() * B6 | rc);
}


void Assembler::fneg(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FNEG | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::mtfsb0(FPSCRBit bit, RCBit rc) {
  DCHECK(static_cast<int>(bit) < 32);
  int bt = bit;
  emit(EXT4 | MTFSB0 | bt * B21 | rc);
}


void Assembler::mtfsb1(FPSCRBit bit, RCBit rc) {
  DCHECK(static_cast<int>(bit) < 32);
  int bt = bit;
  emit(EXT4 | MTFSB1 | bt * B21 | rc);
}


void Assembler::mtfsfi(int bf, int immediate, RCBit rc) {
  emit(EXT4 | MTFSFI | bf * B23 | immediate * B12 | rc);
}


void Assembler::mffs(const DoubleRegister frt, RCBit rc) {
  emit(EXT4 | MFFS | frt.code() * B21 | rc);
}


void Assembler::mtfsf(const DoubleRegister frb, bool L, int FLM, bool W,
                      RCBit rc) {
  emit(EXT4 | MTFSF | frb.code() * B11 | W * B16 | FLM * B17 | L * B25 | rc);
}


void Assembler::fsqrt(const DoubleRegister frt, const DoubleRegister frb,
                      RCBit rc) {
  emit(EXT4 | FSQRT | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fabs(const DoubleRegister frt, const DoubleRegister frb,
                     RCBit rc) {
  emit(EXT4 | FABS | frt.code() * B21 | frb.code() * B11 | rc);
}


void Assembler::fmadd(const DoubleRegister frt, const DoubleRegister fra,
                      const DoubleRegister frc, const DoubleRegister frb,
                      RCBit rc) {
  emit(EXT4 | FMADD | frt.code() * B21 | fra.code() * B16 | frb.code() * B11 |
       frc.code() * B6 | rc);
}


void Assembler::fmsub(const DoubleRegister frt, const DoubleRegister fra,
                      const DoubleRegister frc, const DoubleRegister frb,
                      RCBit rc) {
  emit(EXT4 | FMSUB | frt.code() * B21 | fra.code() * B16 | frb.code() * B11 |
       frc.code() * B6 | rc);
}


// Pseudo instructions.
void Assembler::nop(int type) {
  Register reg = r0;
  switch (type) {
    case NON_MARKING_NOP:
      reg = r0;
      break;
    case GROUP_ENDING_NOP:
      reg = r2;
      break;
    case DEBUG_BREAK_NOP:
      reg = r3;
      break;
    default:
      UNIMPLEMENTED();
  }

  ori(reg, reg, Operand::Zero());
}


bool Assembler::IsNop(Instr instr, int type) {
  int reg = 0;
  switch (type) {
    case NON_MARKING_NOP:
      reg = 0;
      break;
    case GROUP_ENDING_NOP:
      reg = 2;
      break;
    case DEBUG_BREAK_NOP:
      reg = 3;
      break;
    default:
      UNIMPLEMENTED();
  }
  return instr == (ORI | reg * B21 | reg * B16);
}


void Assembler::GrowBuffer(int needed) {
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
  int space = buffer_space() + (desc.buffer_size - buffer_size_);
  if (space < needed) {
    desc.buffer_size += needed - space;
  }
  CHECK_GT(desc.buffer_size, 0);  // no overflow

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;

  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();

  // Copy the data.
  intptr_t pc_delta = desc.buffer - buffer_;
  intptr_t rc_delta =
      (desc.buffer + desc.buffer_size) - (buffer_ + buffer_size_);
  memmove(desc.buffer, buffer_, desc.instr_size);
  memmove(reloc_info_writer.pos() + rc_delta, reloc_info_writer.pos(),
          desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Nothing else to do here since we keep all internal references and
  // deferred relocation entries relative to the buffer (until
  // EmitRelocations).
}


void Assembler::db(uint8_t data) {
  CheckBuffer();
  *reinterpret_cast<uint8_t*>(pc_) = data;
  pc_ += sizeof(uint8_t);
}


void Assembler::dd(uint32_t data) {
  CheckBuffer();
  *reinterpret_cast<uint32_t*>(pc_) = data;
  pc_ += sizeof(uint32_t);
}


void Assembler::dq(uint64_t value) {
  CheckBuffer();
  *reinterpret_cast<uint64_t*>(pc_) = value;
  pc_ += sizeof(uint64_t);
}


void Assembler::dp(uintptr_t data) {
  CheckBuffer();
  *reinterpret_cast<uintptr_t*>(pc_) = data;
  pc_ += sizeof(uintptr_t);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  if (RelocInfo::IsNone(rmode) ||
      // Don't record external references unless the heap will be serialized.
      (rmode == RelocInfo::EXTERNAL_REFERENCE && !serializer_enabled() &&
       !emit_debug_code())) {
    return;
  }
  if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
    data = RecordedAstId().ToInt();
    ClearRecordedAstId();
  }
  DeferredRelocInfo rinfo(pc_offset(), rmode, data);
  relocations_.push_back(rinfo);
}


void Assembler::EmitRelocations() {
  EnsureSpaceFor(relocations_.size() * kMaxRelocSize);

  for (std::vector<DeferredRelocInfo>::iterator it = relocations_.begin();
       it != relocations_.end(); it++) {
    RelocInfo::Mode rmode = it->rmode();
    Address pc = buffer_ + it->position();
    Code* code = NULL;
    RelocInfo rinfo(isolate(), pc, rmode, it->data(), code);

    // Fix up internal references now that they are guaranteed to be bound.
    if (RelocInfo::IsInternalReference(rmode)) {
      // Jump table entry
      intptr_t pos = reinterpret_cast<intptr_t>(Memory::Address_at(pc));
      Memory::Address_at(pc) = buffer_ + pos;
    } else if (RelocInfo::IsInternalReferenceEncoded(rmode)) {
      // mov sequence
      intptr_t pos = reinterpret_cast<intptr_t>(target_address_at(pc, code));
      set_target_address_at(isolate(), pc, code, buffer_ + pos,
                            SKIP_ICACHE_FLUSH);
    }

    reloc_info_writer.Write(&rinfo);
  }
}


void Assembler::BlockTrampolinePoolFor(int instructions) {
  BlockTrampolinePoolBefore(pc_offset() + instructions * kInstrSize);
}


void Assembler::CheckTrampolinePool() {
  // Some small sequences of instructions must not be broken up by the
  // insertion of a trampoline pool; such sequences are protected by setting
  // either trampoline_pool_blocked_nesting_ or no_trampoline_pool_before_,
  // which are both checked here. Also, recursive calls to CheckTrampolinePool
  // are blocked by trampoline_pool_blocked_nesting_.
  if (trampoline_pool_blocked_nesting_ > 0) return;
  if (pc_offset() < no_trampoline_pool_before_) {
    next_trampoline_check_ = no_trampoline_pool_before_;
    return;
  }

  DCHECK(!trampoline_emitted_);
  if (tracked_branch_count_ > 0) {
    int size = tracked_branch_count_ * kInstrSize;

    // As we are only going to emit trampoline once, we need to prevent any
    // further emission.
    trampoline_emitted_ = true;
    next_trampoline_check_ = kMaxInt;

    // First we emit jump, then we emit trampoline pool.
    b(size + kInstrSize, LeaveLK);
    for (int i = size; i > 0; i -= kInstrSize) {
      b(i, LeaveLK);
    }

    trampoline_ = Trampoline(pc_offset() - size, tracked_branch_count_);
  }
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2021 the V8 project authors. All rights reserved.

#if V8_TARGET_ARCH_RISCV64

#include "src/codegen/riscv64/assembler-riscv64.h"

#include "src/base/cpu.h"
#include "src/codegen/riscv64/assembler-riscv64-inl.h"
#include "src/codegen/safepoint-table.h"
#include "src/codegen/string-constants.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/objects/heap-number-inl.h"

namespace v8 {
namespace internal {
// Get the CPU features enabled by the build. For cross compilation the
// preprocessor symbols CAN_USE_FPU_INSTRUCTIONS
// can be defined to enable FPU instructions when building the
// snapshot.
static unsigned CpuFeaturesImpliedByCompiler() {
  unsigned answer = 0;
#ifdef CAN_USE_FPU_INSTRUCTIONS
  answer |= 1u << FPU;
#endif  // def CAN_USE_FPU_INSTRUCTIONS

  return answer;
}

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(RISCV_SIMD); }

void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

  // Probe for additional features at runtime.
  base::CPU cpu;
  if (cpu.has_fpu()) supported_ |= 1u << FPU;
}

void CpuFeatures::PrintTarget() {}
void CpuFeatures::PrintFeatures() {}

int ToNumber(Register reg) {
  DCHECK(reg.is_valid());
  const int kNumbers[] = {
      0,   // zero_reg
      1,   // ra
      2,   // sp
      3,   // gp
      4,   // tp
      5,   // t0
      6,   // t1
      7,   // t2
      8,   // s0/fp
      9,   // s1
      10,  // a0
      11,  // a1
      12,  // a2
      13,  // a3
      14,  // a4
      15,  // a5
      16,  // a6
      17,  // a7
      18,  // s2
      19,  // s3
      20,  // s4
      21,  // s5
      22,  // s6
      23,  // s7
      24,  // s8
      25,  // s9
      26,  // s10
      27,  // s11
      28,  // t3
      29,  // t4
      30,  // t5
      31,  // t6
  };
  return kNumbers[reg.code()];
}

Register ToRegister(int num) {
  DCHECK(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] = {
      zero_reg, ra, sp, gp, tp, t0, t1, t2, fp, s1, a0,  a1,  a2, a3, a4, a5,
      a6,       a7, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3, t4, t5, t6};
  return kRegisters[num];
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo.

const int RelocInfo::kApplyMask =
    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
    RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET);

bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on RISC-V means that it is a lui/addi instruction, and that
  // is always the case inside code objects.
  return true;
}

bool RelocInfo::IsInConstantPool() { return false; }

uint32_t RelocInfo::wasm_call_tag() const {
  DCHECK(rmode_ == WASM_CALL || rmode_ == WASM_STUB_CALL);
  return static_cast<uint32_t>(
      Assembler::target_address_at(pc_, constant_pool_));
}

// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand.
// See assembler-riscv64-inl.h for inlined constructors.

Operand::Operand(Handle<HeapObject> handle)
    : rm_(no_reg), rmode_(RelocInfo::FULL_EMBEDDED_OBJECT) {
  value_.immediate = static_cast<intptr_t>(handle.address());
}

Operand Operand::EmbeddedNumber(double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) return Operand(Smi::FromInt(smi));
  Operand result(0, RelocInfo::FULL_EMBEDDED_OBJECT);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(value);
  return result;
}

Operand Operand::EmbeddedStringConstant(const StringConstantBase* str) {
  Operand result(0, RelocInfo::FULL_EMBEDDED_OBJECT);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(str);
  return result;
}

MemOperand::MemOperand(Register rm, int32_t offset) : Operand(rm) {
  offset_ = offset;
}

MemOperand::MemOperand(Register rm, int32_t unit, int32_t multiplier,
                       OffsetAddend offset_addend)
    : Operand(rm) {
  offset_ = unit * multiplier + offset_addend;
}

void Assembler::AllocateAndInstallRequestedHeapObjects(Isolate* isolate) {
  DCHECK_IMPLIES(isolate == nullptr, heap_object_requests_.empty());
  for (auto& request : heap_object_requests_) {
    Handle<HeapObject> object;
    switch (request.kind()) {
      case HeapObjectRequest::kHeapNumber:
        object = isolate->factory()->NewHeapNumber<AllocationType::kOld>(
            request.heap_number());
        break;
      case HeapObjectRequest::kStringConstant:
        const StringConstantBase* str = request.string();
        CHECK_NOT_NULL(str);
        object = str->AllocateStringConstant(isolate);
        break;
    }
    Address pc = reinterpret_cast<Address>(buffer_start_) + request.offset();
    set_target_value_at(pc, reinterpret_cast<uint64_t>(object.location()));
  }
}

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

Assembler::Assembler(const AssemblerOptions& options,
                     std::unique_ptr<AssemblerBuffer> buffer)
    : AssemblerBase(options, std::move(buffer)),
      scratch_register_list_(t3.bit() | t5.bit()),
      constpool_(this) {
  reloc_info_writer.Reposition(buffer_start_ + buffer_->size(), pc_);

  last_trampoline_pool_end_ = 0;
  no_trampoline_pool_before_ = 0;
  trampoline_pool_blocked_nesting_ = 0;
  // We leave space (16 * kTrampolineSlotsSize)
  // for BlockTrampolinePoolScope buffer.
  next_buffer_check_ = FLAG_force_long_branches
                           ? kMaxInt
                           : kMaxBranchOffset - kTrampolineSlotsSize * 16;
  internal_trampoline_exception_ = false;
  last_bound_pos_ = 0;

  trampoline_emitted_ = FLAG_force_long_branches;
  unbound_labels_count_ = 0;
  block_buffer_growth_ = false;
}

void Assembler::GetCode(Isolate* isolate, CodeDesc* desc,
                        SafepointTableBuilder* safepoint_table_builder,
                        int handler_table_offset) {
  // As a crutch to avoid having to add manual Align calls wherever we use a
  // raw workflow to create Code objects (mostly in tests), add another Align
  // call here. It does no harm - the end of the Code object is aligned to the
  // (larger) kCodeAlignment anyways.
  // TODO(jgruber): Consider moving responsibility for proper alignment to
  // metadata table builders (safepoint, handler, constant pool, code
  // comments).
  DataAlign(Code::kMetadataAlignment);

  ForceConstantPoolEmissionWithoutJump();

  int code_comments_size = WriteCodeComments();

  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.

  AllocateAndInstallRequestedHeapObjects(isolate);

  // Set up code descriptor.
  // TODO(jgruber): Reconsider how these offsets and sizes are maintained up to
  // this point to make CodeDesc initialization less fiddly.

  static constexpr int kConstantPoolSize = 0;
  const int instruction_size = pc_offset();
  const int code_comments_offset = instruction_size - code_comments_size;
  const int constant_pool_offset = code_comments_offset - kConstantPoolSize;
  const int handler_table_offset2 = (handler_table_offset == kNoHandlerTable)
                                        ? constant_pool_offset
                                        : handler_table_offset;
  const int safepoint_table_offset =
      (safepoint_table_builder == kNoSafepointTable)
          ? handler_table_offset2
          : safepoint_table_builder->GetCodeOffset();
  const int reloc_info_offset =
      static_cast<int>(reloc_info_writer.pos() - buffer_->start());
  CodeDesc::Initialize(desc, this, safepoint_table_offset,
                       handler_table_offset2, constant_pool_offset,
                       code_comments_offset, reloc_info_offset);
}

void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo(m));
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}

void Assembler::CodeTargetAlign() {
  // No advantage to aligning branch/call targets to more than
  // single instruction, that I am aware of.
  Align(4);
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

// The link chain is terminated by a value in the instruction of 0,
// which is an otherwise illegal value (branch 0 is inf loop). When this case
// is detected, return an position of -1, an otherwise illegal position.
const int kEndOfChain = -1;
const int kEndOfJumpChain = 0;

bool Assembler::IsBranch(Instr instr) {
  return (instr & kBaseOpcodeMask) == BRANCH;
}

bool Assembler::IsJump(Instr instr) {
  int Op = instr & kBaseOpcodeMask;
  return Op == JAL || Op == JALR;
}

bool Assembler::IsJal(Instr instr) { return (instr & kBaseOpcodeMask) == JAL; }

bool Assembler::IsJalr(Instr instr) {
  return (instr & kBaseOpcodeMask) == JALR;
}

bool Assembler::IsCJal(Instr instr) {
  return (instr & kRvcOpcodeMask) == RO_C_J;
}

bool Assembler::IsLui(Instr instr) { return (instr & kBaseOpcodeMask) == LUI; }
bool Assembler::IsAuipc(Instr instr) {
  return (instr & kBaseOpcodeMask) == AUIPC;
}
bool Assembler::IsAddiw(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_ADDIW;
}
bool Assembler::IsAddi(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_ADDI;
}
bool Assembler::IsOri(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_ORI;
}
bool Assembler::IsSlli(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_SLLI;
}

bool Assembler::IsLd(Instr instr) {
  return (instr & (kBaseOpcodeMask | kFunct3Mask)) == RO_LD;
}

int Assembler::target_at(int pos, bool is_internal) {
  if (is_internal) {
    int64_t* p = reinterpret_cast<int64_t*>(buffer_start_ + pos);
    int64_t address = *p;
    if (address == kEndOfJumpChain) {
      return kEndOfChain;
    } else {
      int64_t instr_address = reinterpret_cast<int64_t>(p);
      DCHECK(instr_address - address < INT_MAX);
      int delta = static_cast<int>(instr_address - address);
      DCHECK(pos > delta);
      return pos - delta;
    }
  }
  Instruction* instruction = Instruction::At(buffer_start_ + pos);
  DEBUG_PRINTF("target_at: %p (%d)\n\t",
               reinterpret_cast<Instr*>(buffer_start_ + pos), pos);
  Instr instr = instruction->InstructionBits();
  disassembleInstr(instruction->InstructionBits());

  switch (instruction->InstructionOpcodeType()) {
    case BRANCH: {
      int32_t imm13 = BranchOffset(instr);
      if (imm13 == kEndOfJumpChain) {
        // EndOfChain sentinel is returned directly, not relative to pc or pos.
        return kEndOfChain;
      } else {
        return pos + imm13;
      }
    } break;
    case JAL: {
      int32_t imm21 = JumpOffset(instr);
      if (imm21 == kEndOfJumpChain) {
        // EndOfChain sentinel is returned directly, not relative to pc or pos.
        return kEndOfChain;
      } else {
        return pos + imm21;
      }
    } break;
    case JALR: {
      int32_t imm12 = instr >> 20;
      if (imm12 == kEndOfJumpChain) {
        // EndOfChain sentinel is returned directly, not relative to pc or pos.
        return kEndOfChain;
      } else {
        return pos + imm12;
      }
    } break;
    case LUI: {
      Address pc = reinterpret_cast<Address>(buffer_start_ + pos);
      pc = target_address_at(pc);
      uint64_t instr_address = reinterpret_cast<uint64_t>(buffer_start_ + pos);
      uint64_t imm = reinterpret_cast<uint64_t>(pc);
      if (imm == kEndOfJumpChain) {
        return kEndOfChain;
      } else {
        DCHECK(instr_address - imm < INT_MAX);
        int32_t delta = static_cast<int32_t>(instr_address - imm);
        DCHECK(pos > delta);
        return pos - delta;
      }
    } break;
    case AUIPC: {
      Instr instr_auipc = instr;
      Instr instr_I = instr_at(pos + 4);
      DCHECK(IsJalr(instr_I) || IsAddi(instr_I));
      int32_t offset = BrachlongOffset(instr_auipc, instr_I);
      if (offset == kEndOfJumpChain) return kEndOfChain;
      return offset + pos;
    } break;
    case RO_C_J: {
      int32_t offset = instruction->RvcImm11CJValue();
      if (offset == kEndOfJumpChain) return kEndOfChain;
      return offset + pos;
    } break;
    default: {
      if (instr == kEndOfJumpChain) {
        return kEndOfChain;
      } else {
        int32_t imm18 =
            ((instr & static_cast<int32_t>(kImm16Mask)) << 16) >> 14;
        return (imm18 + pos);
      }
    } break;
  }
}

static inline Instr SetBranchOffset(int32_t pos, int32_t target_pos,
                                    Instr instr) {
  int32_t imm = target_pos - pos;
  DCHECK_EQ(imm & 1, 0);
  DCHECK(is_intn(imm, Assembler::kBranchOffsetBits));

  instr &= ~kBImm12Mask;
  int32_t imm12 = ((imm & 0x800) >> 4) |   // bit  11
                  ((imm & 0x1e) << 7) |    // bits 4-1
                  ((imm & 0x7e0) << 20) |  // bits 10-5
                  ((imm & 0x1000) << 19);  // bit 12

  return instr | (imm12 & kBImm12Mask);
}

static inline Instr SetLdOffset(int32_t offset, Instr instr) {
  DCHECK(Assembler::IsLd(instr));
  DCHECK(is_int12(offset));
  instr &= ~kImm12Mask;
  int32_t imm12 = offset << kImm12Shift;
  return instr | (imm12 & kImm12Mask);
}

static inline Instr SetAuipcOffset(int32_t offset, Instr instr) {
  DCHECK(Assembler::IsAuipc(instr));
  DCHECK(is_int20(offset));
  instr = (instr & ~kImm31_12Mask) | ((offset & kImm19_0Mask) << 12);
  return instr;
}

static inline Instr SetJalrOffset(int32_t offset, Instr instr) {
  DCHECK(Assembler::IsJalr(instr));
  DCHECK(is_int12(offset));
  instr &= ~kImm12Mask;
  int32_t imm12 = offset << kImm12Shift;
  DCHECK(Assembler::IsJalr(instr | (imm12 & kImm12Mask)));
  DCHECK(Assembler::JalrOffset(instr | (imm12 & kImm12Mask)) == offset);
  return instr | (imm12 & kImm12Mask);
}

static inline Instr SetJalOffset(int32_t pos, int32_t target_pos, Instr instr) {
  DCHECK(Assembler::IsJal(instr));
  int32_t imm = target_pos - pos;
  DCHECK_EQ(imm & 1, 0);
  DCHECK(is_intn(imm, Assembler::kJumpOffsetBits));

  instr &= ~kImm20Mask;
  int32_t imm20 = (imm & 0xff000) |          // bits 19-12
                  ((imm & 0x800) << 9) |     // bit  11
                  ((imm & 0x7fe) << 20) |    // bits 10-1
                  ((imm & 0x100000) << 11);  // bit  20

  return instr | (imm20 & kImm20Mask);
}

static inline ShortInstr SetCJalOffset(int32_t pos, int32_t target_pos,
                                       Instr instr) {
  DCHECK(Assembler::IsCJal(instr));
  int32_t imm = target_pos - pos;
  DCHECK_EQ(imm & 1, 0);
  DCHECK(is_intn(imm, Assembler::kCJalOffsetBits));
  instr &= ~kImm11Mask;
  int16_t imm11 = ((imm & 0x800) >> 1) | ((imm & 0x400) >> 4) |
                  ((imm & 0x300) >> 1) | ((imm & 0x80) >> 3) |
                  ((imm & 0x40) >> 1) | ((imm & 0x20) >> 5) |
                  ((imm & 0x10) << 5) | (imm & 0xe);
  imm11 = imm11 << kImm11Shift;
  DCHECK(Assembler::IsCJal(instr | (imm11 & kImm11Mask)));
  return instr | (imm11 & kImm11Mask);
}

void Assembler::target_at_put(int pos, int target_pos, bool is_internal) {
  if (is_internal) {
    uint64_t imm = reinterpret_cast<uint64_t>(buffer_start_) + target_pos;
    *reinterpret_cast<uint64_t*>(buffer_start_ + pos) = imm;
    return;
  }
  DEBUG_PRINTF("target_at_put: %p (%d) to %p (%d)\n",
               reinterpret_cast<Instr*>(buffer_start_ + pos), pos,
               reinterpret_cast<Instr*>(buffer_start_ + target_pos),
               target_pos);
  Instruction* instruction = Instruction::At(buffer_start_ + pos);
  Instr instr = instruction->InstructionBits();

  switch (instruction->InstructionOpcodeType()) {
    case BRANCH: {
      instr = SetBranchOffset(pos, target_pos, instr);
      instr_at_put(pos, instr);
    } break;
    case JAL: {
      instr = SetJalOffset(pos, target_pos, instr);
      instr_at_put(pos, instr);
    } break;
    case LUI: {
      Address pc = reinterpret_cast<Address>(buffer_start_ + pos);
      set_target_value_at(
          pc, reinterpret_cast<uint64_t>(buffer_start_ + target_pos));
    } break;
    case AUIPC: {
      Instr instr_auipc = instr;
      Instr instr_I = instr_at(pos + 4);
      DCHECK(IsJalr(instr_I) || IsAddi(instr_I));

      int64_t offset = target_pos - pos;
      DCHECK(is_int32(offset));

      int32_t Hi20 = (((int32_t)offset + 0x800) >> 12);
      int32_t Lo12 = (int32_t)offset << 20 >> 20;

      instr_auipc =
          (instr_auipc & ~kImm31_12Mask) | ((Hi20 & kImm19_0Mask) << 12);
      instr_at_put(pos, instr_auipc);

      const int kImm31_20Mask = ((1 << 12) - 1) << 20;
      const int kImm11_0Mask = ((1 << 12) - 1);
      instr_I = (instr_I & ~kImm31_20Mask) | ((Lo12 & kImm11_0Mask) << 20);
      instr_at_put(pos + 4, instr_I);
    } break;
    case RO_C_J: {
      ShortInstr short_instr = SetCJalOffset(pos, target_pos, instr);
      instr_at_put(pos, short_instr);
    } break;
    default: {
      // Emitted label constant, not part of a branch.
      // Make label relative to Code pointer of generated Code object.
      instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    } break;
  }
  disassembleInstr(instr);
}

void Assembler::print(const Label* L) {
  if (L->is_unused()) {
    PrintF("unused label\n");
  } else if (L->is_bound()) {
    PrintF("bound label to %d\n", L->pos());
  } else if (L->is_linked()) {
    Label l;
    l.link_to(L->pos());
    PrintF("unbound label");
    while (l.is_linked()) {
      PrintF("@ %d ", l.pos());
      Instr instr = instr_at(l.pos());
      if ((instr & ~kImm16Mask) == 0) {
        PrintF("value\n");
      } else {
        PrintF("%d\n", instr);
      }
      next(&l, is_internal_reference(&l));
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}

void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // Must have valid binding position.
  DEBUG_PRINTF("binding %d to label %p\n", pos, L);
  int trampoline_pos = kInvalidSlotPos;
  bool is_internal = false;
  if (L->is_linked() && !trampoline_emitted_) {
    unbound_labels_count_--;
    if (!is_internal_reference(L)) {
      next_buffer_check_ += kTrampolineSlotsSize;
    }
  }

  while (L->is_linked()) {
    int fixup_pos = L->pos();
    int dist = pos - fixup_pos;
    is_internal = is_internal_reference(L);
    next(L, is_internal);  // Call next before overwriting link with target
                           // at fixup_pos.
    Instr instr = instr_at(fixup_pos);
    DEBUG_PRINTF("\tfixup: %d to %d\n", fixup_pos, dist);
    if (is_internal) {
      target_at_put(fixup_pos, pos, is_internal);
    } else {
      if (IsBranch(instr)) {
        if (dist > kMaxBranchOffset) {
          if (trampoline_pos == kInvalidSlotPos) {
            trampoline_pos = get_trampoline_entry(fixup_pos);
            CHECK_NE(trampoline_pos, kInvalidSlotPos);
          }
          CHECK((trampoline_pos - fixup_pos) <= kMaxBranchOffset);
          DEBUG_PRINTF("\t\ttrampolining: %d\n", trampoline_pos);
          target_at_put(fixup_pos, trampoline_pos, false);
          fixup_pos = trampoline_pos;
        }
        target_at_put(fixup_pos, pos, false);
      } else if (IsJal(instr)) {
        if (dist > kMaxJumpOffset) {
          if (trampoline_pos == kInvalidSlotPos) {
            trampoline_pos = get_trampoline_entry(fixup_pos);
            CHECK_NE(trampoline_pos, kInvalidSlotPos);
          }
          CHECK((trampoline_pos - fixup_pos) <= kMaxJumpOffset);
          DEBUG_PRINTF("\t\ttrampolining: %d\n", trampoline_pos);
          target_at_put(fixup_pos, trampoline_pos, false);
          fixup_pos = trampoline_pos;
        }
        target_at_put(fixup_pos, pos, false);
      } else {
        target_at_put(fixup_pos, pos, false);
      }
    }
  }
  L->bind_to(pos);

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_) last_bound_pos_ = pos;
}

void Assembler::bind(Label* L) {
  DCHECK(!L->is_bound());  // Label can only be bound once.
  bind_to(L, pc_offset());
}

void Assembler::next(Label* L, bool is_internal) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos(), is_internal);
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    DCHECK_GT(link, 0);
    DEBUG_PRINTF("next: %p to %p (%d)\n", L,
                 reinterpret_cast<Instr*>(buffer_start_ + link), link);
    L->link_to(link);
  }
}

bool Assembler::is_near(Label* L) {
  DCHECK(L->is_bound());
  return is_intn((pc_offset() - L->pos()), kJumpOffsetBits);
}

bool Assembler::is_near(Label* L, OffsetSize bits) {
  if (L == nullptr || !L->is_bound()) return true;
  return is_intn((pc_offset() - L->pos()), bits);
}

bool Assembler::is_near_branch(Label* L) {
  DCHECK(L->is_bound());
  return is_intn((pc_offset() - L->pos()), kBranchOffsetBits);
}

int Assembler::BranchOffset(Instr instr) {
  // | imm[12] | imm[10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode |
  //  31          25                      11          7
  int32_t imm13 = ((instr & 0xf00) >> 7) | ((instr & 0x7e000000) >> 20) |
                  ((instr & 0x80) << 4) | ((instr & 0x80000000) >> 19);
  imm13 = imm13 << 19 >> 19;
  return imm13;
}

int Assembler::JumpOffset(Instr instr) {
  int32_t imm21 = ((instr & 0x7fe00000) >> 20) | ((instr & 0x100000) >> 9) |
                  (instr & 0xff000) | ((instr & 0x80000000) >> 11);
  imm21 = imm21 << 11 >> 11;
  return imm21;
}

int Assembler::CJumpOffset(Instr instr) {
  int32_t imm12 = ((instr & 0x4) << 3) | ((instr & 0x38) >> 2) |
                  ((instr & 0x40) << 1) | ((instr & 0x80) >> 1) |
                  ((instr & 0x100) << 2) | ((instr & 0x600) >> 1) |
                  ((instr & 0x800) >> 7) | ((instr & 0x1000) >> 1);
  imm12 = imm12 << 20 >> 20;
  return imm12;
}

int Assembler::BrachlongOffset(Instr auipc, Instr instr_I) {
  DCHECK(reinterpret_cast<Instruction*>(&instr_I)->InstructionType() ==
         InstructionBase::kIType);
  DCHECK(IsAuipc(auipc));
  int32_t imm_auipc = AuipcOffset(auipc);
  int32_t imm12 = (instr_I & kImm12Mask) >> 20;
  int32_t offset = imm12 + imm_auipc;
  return offset;
}

int Assembler::PatchBranchlongOffset(Address pc, Instr instr_auipc,
                                     Instr instr_jalr, int32_t offset) {
  DCHECK(IsAuipc(instr_auipc));
  DCHECK(IsJalr(instr_jalr));
  int32_t Hi20 = (((int32_t)offset + 0x800) >> 12);
  int32_t Lo12 = (int32_t)offset << 20 >> 20;
  CHECK(is_int32(offset));
  instr_at_put(pc, SetAuipcOffset(Hi20, instr_auipc));
  instr_at_put(pc + 4, SetJalrOffset(Lo12, instr_jalr));
  DCHECK(offset ==
         BrachlongOffset(Assembler::instr_at(pc), Assembler::instr_at(pc + 4)));
  return 2;
}

int Assembler::LdOffset(Instr instr) {
  DCHECK(IsLd(instr));
  int32_t imm12 = (instr & kImm12Mask) >> 20;
  return imm12;
}

int Assembler::JalrOffset(Instr instr) {
  DCHECK(IsJalr(instr));
  int32_t imm12 = (instr & kImm12Mask) >> 20;
  return imm12;
}

int Assembler::AuipcOffset(Instr instr) {
  DCHECK(IsAuipc(instr));
  int32_t imm20 = instr & kImm20Mask;
  return imm20;
}
// We have to use a temporary register for things that can be relocated even
// if they can be encoded in RISC-V's 12 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Assembler::MustUseReg(RelocInfo::Mode rmode) {
  return !RelocInfo::IsNone(rmode);
}

void Assembler::disassembleInstr(Instr instr) {
  if (!FLAG_debug_riscv) return;
  disasm::NameConverter converter;
  disasm::Disassembler disasm(converter);
  EmbeddedVector<char, 128> disasm_buffer;

  disasm.InstructionDecode(disasm_buffer, reinterpret_cast<byte*>(&instr));
  DEBUG_PRINTF("%s\n", disasm_buffer.begin());
}

// ----- Top-level instruction formats match those in the ISA manual
// (R, I, S, B, U, J). These match the formats defined in the compiler
void Assembler::GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode,
                          Register rd, Register rs1, Register rs2) {
  DCHECK(is_uint7(funct7) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode,
                          FPURegister rd, FPURegister rs1, FPURegister rs2) {
  DCHECK(is_uint7(funct7) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode,
                          Register rd, FPURegister rs1, Register rs2) {
  DCHECK(is_uint7(funct7) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode,
                          FPURegister rd, Register rs1, Register rs2) {
  DCHECK(is_uint7(funct7) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode,
                          FPURegister rd, FPURegister rs1, Register rs2) {
  DCHECK(is_uint7(funct7) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrR(uint8_t funct7, uint8_t funct3, Opcode opcode,
                          Register rd, FPURegister rs1, FPURegister rs2) {
  DCHECK(is_uint7(funct7) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrR4(uint8_t funct2, Opcode opcode, Register rd,
                           Register rs1, Register rs2, Register rs3,
                           RoundingMode frm) {
  DCHECK(is_uint2(funct2) && rd.is_valid() && rs1.is_valid() &&
         rs2.is_valid() && rs3.is_valid() && is_uint3(frm));
  Instr instr = opcode | (rd.code() << kRdShift) | (frm << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct2 << kFunct2Shift) | (rs3.code() << kRs3Shift);
  emit(instr);
}

void Assembler::GenInstrR4(uint8_t funct2, Opcode opcode, FPURegister rd,
                           FPURegister rs1, FPURegister rs2, FPURegister rs3,
                           RoundingMode frm) {
  DCHECK(is_uint2(funct2) && rd.is_valid() && rs1.is_valid() &&
         rs2.is_valid() && rs3.is_valid() && is_uint3(frm));
  Instr instr = opcode | (rd.code() << kRdShift) | (frm << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct2 << kFunct2Shift) | (rs3.code() << kRs3Shift);
  emit(instr);
}

void Assembler::GenInstrRAtomic(uint8_t funct5, bool aq, bool rl,
                                uint8_t funct3, Register rd, Register rs1,
                                Register rs2) {
  DCHECK(is_uint5(funct5) && is_uint3(funct3) && rd.is_valid() &&
         rs1.is_valid() && rs2.is_valid());
  Instr instr = AMO | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (rl << kRlShift) | (aq << kAqShift) | (funct5 << kFunct5Shift);
  emit(instr);
}

void Assembler::GenInstrRFrm(uint8_t funct7, Opcode opcode, Register rd,
                             Register rs1, Register rs2, RoundingMode frm) {
  DCHECK(rd.is_valid() && rs1.is_valid() && rs2.is_valid() && is_uint3(frm));
  Instr instr = opcode | (rd.code() << kRdShift) | (frm << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (rs2.code() << kRs2Shift) |
                (funct7 << kFunct7Shift);
  emit(instr);
}

void Assembler::GenInstrI(uint8_t funct3, Opcode opcode, Register rd,
                          Register rs1, int16_t imm12) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && rs1.is_valid() &&
         (is_uint12(imm12) || is_int12(imm12)));
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (imm12 << kImm12Shift);
  emit(instr);
}

void Assembler::GenInstrI(uint8_t funct3, Opcode opcode, FPURegister rd,
                          Register rs1, int16_t imm12) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && rs1.is_valid() &&
         (is_uint12(imm12) || is_int12(imm12)));
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (imm12 << kImm12Shift);
  emit(instr);
}

void Assembler::GenInstrIShift(bool arithshift, uint8_t funct3, Opcode opcode,
                               Register rd, Register rs1, uint8_t shamt) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && rs1.is_valid() &&
         is_uint6(shamt));
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (shamt << kShamtShift) |
                (arithshift << kArithShiftShift);
  emit(instr);
}

void Assembler::GenInstrIShiftW(bool arithshift, uint8_t funct3, Opcode opcode,
                                Register rd, Register rs1, uint8_t shamt) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && rs1.is_valid() &&
         is_uint5(shamt));
  Instr instr = opcode | (rd.code() << kRdShift) | (funct3 << kFunct3Shift) |
                (rs1.code() << kRs1Shift) | (shamt << kShamtWShift) |
                (arithshift << kArithShiftShift);
  emit(instr);
}

void Assembler::GenInstrS(uint8_t funct3, Opcode opcode, Register rs1,
                          Register rs2, int16_t imm12) {
  DCHECK(is_uint3(funct3) && rs1.is_valid() && rs2.is_valid() &&
         is_int12(imm12));
  Instr instr = opcode | ((imm12 & 0x1f) << 7) |  // bits  4-0
                (funct3 << kFunct3Shift) | (rs1.code() << kRs1Shift) |
                (rs2.code() << kRs2Shift) |
                ((imm12 & 0xfe0) << 20);  // bits 11-5
  emit(instr);
}

void Assembler::GenInstrS(uint8_t funct3, Opcode opcode, Register rs1,
                          FPURegister rs2, int16_t imm12) {
  DCHECK(is_uint3(funct3) && rs1.is_valid() && rs2.is_valid() &&
         is_int12(imm12));
  Instr instr = opcode | ((imm12 & 0x1f) << 7) |  // bits  4-0
                (funct3 << kFunct3Shift) | (rs1.code() << kRs1Shift) |
                (rs2.code() << kRs2Shift) |
                ((imm12 & 0xfe0) << 20);  // bits 11-5
  emit(instr);
}

void Assembler::GenInstrB(uint8_t funct3, Opcode opcode, Register rs1,
                          Register rs2, int16_t imm13) {
  DCHECK(is_uint3(funct3) && rs1.is_valid() && rs2.is_valid() &&
         is_int13(imm13) && ((imm13 & 1) == 0));
  Instr instr = opcode | ((imm13 & 0x800) >> 4) |  // bit  11
                ((imm13 & 0x1e) << 7) |            // bits 4-1
                (funct3 << kFunct3Shift) | (rs1.code() << kRs1Shift) |
                (rs2.code() << kRs2Shift) |
                ((imm13 & 0x7e0) << 20) |  // bits 10-5
                ((imm13 & 0x1000) << 19);  // bit 12
  emit(instr);
}

void Assembler::GenInstrU(Opcode opcode, Register rd, int32_t imm20) {
  DCHECK(rd.is_valid() && (is_int20(imm20) || is_uint20(imm20)));
  Instr instr = opcode | (rd.code() << kRdShift) | (imm20 << kImm20Shift);
  emit(instr);
}

void Assembler::GenInstrJ(Opcode opcode, Register rd, int32_t imm21) {
  DCHECK(rd.is_valid() && is_int21(imm21) && ((imm21 & 1) == 0));
  Instr instr = opcode | (rd.code() << kRdShift) |
                (imm21 & 0xff000) |          // bits 19-12
                ((imm21 & 0x800) << 9) |     // bit  11
                ((imm21 & 0x7fe) << 20) |    // bits 10-1
                ((imm21 & 0x100000) << 11);  // bit  20
  emit(instr);
}

void Assembler::GenInstrCR(uint8_t funct4, Opcode opcode, Register rd,
                           Register rs2) {
  DCHECK(is_uint4(funct4) && rd.is_valid() && rs2.is_valid());
  ShortInstr instr = opcode | (rs2.code() << kRvcRs2Shift) |
                     (rd.code() << kRvcRdShift) | (funct4 << kRvcFunct4Shift);
  emit(instr);
}

void Assembler::GenInstrCA(uint8_t funct6, Opcode opcode, Register rd,
                           uint8_t funct, Register rs2) {
  DCHECK(is_uint6(funct6) && rd.is_valid() && rs2.is_valid() &&
         is_uint2(funct));
  ShortInstr instr = opcode | ((rs2.code() & 0x7) << kRvcRs2sShift) |
                     ((rd.code() & 0x7) << kRvcRs1sShift) |
                     (funct6 << kRvcFunct6Shift) | (funct << kRvcFunct2Shift);
  emit(instr);
}

void Assembler::GenInstrCI(uint8_t funct3, Opcode opcode, Register rd,
                           int8_t imm6) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && is_int6(imm6));
  ShortInstr instr = opcode | ((imm6 & 0x1f) << 2) |
                     (rd.code() << kRvcRdShift) | ((imm6 & 0x20) << 7) |
                     (funct3 << kRvcFunct3Shift);
  emit(instr);
}

void Assembler::GenInstrCIU(uint8_t funct3, Opcode opcode, Register rd,
                            uint8_t uimm6) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && is_uint6(uimm6));
  ShortInstr instr = opcode | ((uimm6 & 0x1f) << 2) |
                     (rd.code() << kRvcRdShift) | ((uimm6 & 0x20) << 7) |
                     (funct3 << kRvcFunct3Shift);
  emit(instr);
}

void Assembler::GenInstrCIU(uint8_t funct3, Opcode opcode, FPURegister rd,
                            uint8_t uimm6) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && is_uint6(uimm6));
  ShortInstr instr = opcode | ((uimm6 & 0x1f) << 2) |
                     (rd.code() << kRvcRdShift) | ((uimm6 & 0x20) << 7) |
                     (funct3 << kRvcFunct3Shift);
  emit(instr);
}

void Assembler::GenInstrCIW(uint8_t funct3, Opcode opcode, Register rd,
                            uint8_t uimm8) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && is_uint8(uimm8));
  ShortInstr instr = opcode | ((uimm8) << 5) |
                     ((rd.code() & 0x7) << kRvcRs2sShift) |
                     (funct3 << kRvcFunct3Shift);
  emit(instr);
}

void Assembler::GenInstrCSS(uint8_t funct3, Opcode opcode, Register rs2,
                            uint8_t uimm6) {
  DCHECK(is_uint3(funct3) && rs2.is_valid() && is_uint6(uimm6));
  ShortInstr instr = opcode | (uimm6 << 7) | (rs2.code() << kRvcRs2Shift) |
                     (funct3 << kRvcFunct3Shift);
  emit(instr);
}

void Assembler::GenInstrCSS(uint8_t funct3, Opcode opcode, FPURegister rs2,
                            uint8_t uimm6) {
  DCHECK(is_uint3(funct3) && rs2.is_valid() && is_uint6(uimm6));
  ShortInstr instr = opcode | (uimm6 << 7) | (rs2.code() << kRvcRs2Shift) |
                     (funct3 << kRvcFunct3Shift);
  emit(instr);
}

void Assembler::GenInstrCL(uint8_t funct3, Opcode opcode, Register rd,
                           Register rs1, uint8_t uimm5) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && rs1.is_valid() &&
         is_uint5(uimm5));
  ShortInstr instr = opcode | ((uimm5 & 0x3) << 5) |
                     ((rd.code() & 0x7) << kRvcRs2sShift) |
                     ((uimm5 & 0x1c) << 8) | (funct3 << kRvcFunct3Shift) |
                     ((rs1.code() & 0x7) << kRvcRs1sShift);
  emit(instr);
}

void Assembler::GenInstrCL(uint8_t funct3, Opcode opcode, FPURegister rd,
                           Register rs1, uint8_t uimm5) {
  DCHECK(is_uint3(funct3) && rd.is_valid() && rs1.is_valid() &&
         is_uint5(uimm5));
  ShortInstr instr = opcode | ((uimm5 & 0x3) << 5) |
                     ((rd.code() & 0x7) << kRvcRs2sShift) |
                     ((uimm5 & 0x1c) << 8) | (funct3 << kRvcFunct3Shift) |
                     ((rs1.code() & 0x7) << kRvcRs1sShift);
  emit(instr);
}
void Assembler::GenInstrCJ(uint8_t funct3, Opcode opcode, uint16_t uint11) {
  DCHECK(is_uint11(uint11));
  ShortInstr instr = opcode | (funct3 << kRvcFunct3Shift) | (uint11 << 2);
  emit(instr);
}

void Assembler::GenInstrCS(uint8_t funct3, Opcode opcode, Register rs2,
                           Register rs1, uint8_t uimm5) {
  DCHECK(is_uint3(funct3) && rs2.is_valid() && rs1.is_valid() &&
         is_uint5(uimm5));
  ShortInstr instr = opcode | ((uimm5 & 0x3) << 5) |
                     ((rs2.code() & 0x7) << kRvcRs2sShift) |
                     ((uimm5 & 0x1c) << 8) | (funct3 << kRvcFunct3Shift) |
                     ((rs1.code() & 0x7) << kRvcRs1sShift);
  emit(instr);
}

void Assembler::GenInstrCS(uint8_t funct3, Opcode opcode, FPURegister rs2,
                           Register rs1, uint8_t uimm5) {
  DCHECK(is_uint3(funct3) && rs2.is_valid() && rs1.is_valid() &&
         is_uint5(uimm5));
  ShortInstr instr = opcode | ((uimm5 & 0x3) << 5) |
                     ((rs2.code() & 0x7) << kRvcRs2sShift) |
                     ((uimm5 & 0x1c) << 8) | (funct3 << kRvcFunct3Shift) |
                     ((rs1.code() & 0x7) << kRvcRs1sShift);
  emit(instr);
}

// ----- Instruction class templates match those in the compiler

void Assembler::GenInstrBranchCC_rri(uint8_t funct3, Register rs1, Register rs2,
                                     int16_t imm13) {
  GenInstrB(funct3, BRANCH, rs1, rs2, imm13);
}

void Assembler::GenInstrLoad_ri(uint8_t funct3, Register rd, Register rs1,
                                int16_t imm12) {
  GenInstrI(funct3, LOAD, rd, rs1, imm12);
}

void Assembler::GenInstrStore_rri(uint8_t funct3, Register rs1, Register rs2,
                                  int16_t imm12) {
  GenInstrS(funct3, STORE, rs1, rs2, imm12);
}

void Assembler::GenInstrALU_ri(uint8_t funct3, Register rd, Register rs1,
                               int16_t imm12) {
  GenInstrI(funct3, OP_IMM, rd, rs1, imm12);
}

void Assembler::GenInstrShift_ri(bool arithshift, uint8_t funct3, Register rd,
                                 Register rs1, uint8_t shamt) {
  DCHECK(is_uint6(shamt));
  GenInstrI(funct3, OP_IMM, rd, rs1, (arithshift << 10) | shamt);
}

void Assembler::GenInstrALU_rr(uint8_t funct7, uint8_t funct3, Register rd,
                               Register rs1, Register rs2) {
  GenInstrR(funct7, funct3, OP, rd, rs1, rs2);
}

void Assembler::GenInstrCSR_ir(uint8_t funct3, Register rd,
                               ControlStatusReg csr, Register rs1) {
  GenInstrI(funct3, SYSTEM, rd, rs1, csr);
}

void Assembler::GenInstrCSR_ii(uint8_t funct3, Register rd,
                               ControlStatusReg csr, uint8_t imm5) {
  GenInstrI(funct3, SYSTEM, rd, ToRegister(imm5), csr);
}

void Assembler::GenInstrShiftW_ri(bool arithshift, uint8_t funct3, Register rd,
                                  Register rs1, uint8_t shamt) {
  GenInstrIShiftW(arithshift, funct3, OP_IMM_32, rd, rs1, shamt);
}

void Assembler::GenInstrALUW_rr(uint8_t funct7, uint8_t funct3, Register rd,
                                Register rs1, Register rs2) {
  GenInstrR(funct7, funct3, OP_32, rd, rs1, rs2);
}

void Assembler::GenInstrPriv(uint8_t funct7, Register rs1, Register rs2) {
  GenInstrR(funct7, 0b000, SYSTEM, ToRegister(0), rs1, rs2);
}

void Assembler::GenInstrLoadFP_ri(uint8_t funct3, FPURegister rd, Register rs1,
                                  int16_t imm12) {
  GenInstrI(funct3, LOAD_FP, rd, rs1, imm12);
}

void Assembler::GenInstrStoreFP_rri(uint8_t funct3, Register rs1,
                                    FPURegister rs2, int16_t imm12) {
  GenInstrS(funct3, STORE_FP, rs1, rs2, imm12);
}

void Assembler::GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, FPURegister rd,
                                 FPURegister rs1, FPURegister rs2) {
  GenInstrR(funct7, funct3, OP_FP, rd, rs1, rs2);
}

void Assembler::GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, FPURegister rd,
                                 Register rs1, Register rs2) {
  GenInstrR(funct7, funct3, OP_FP, rd, rs1, rs2);
}

void Assembler::GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, FPURegister rd,
                                 FPURegister rs1, Register rs2) {
  GenInstrR(funct7, funct3, OP_FP, rd, rs1, rs2);
}

void Assembler::GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, Register rd,
                                 FPURegister rs1, Register rs2) {
  GenInstrR(funct7, funct3, OP_FP, rd, rs1, rs2);
}

void Assembler::GenInstrALUFP_rr(uint8_t funct7, uint8_t funct3, Register rd,
                                 FPURegister rs1, FPURegister rs2) {
  GenInstrR(funct7, funct3, OP_FP, rd, rs1, rs2);
}

// Returns the next free trampoline entry.
int32_t Assembler::get_trampoline_entry(int32_t pos) {
  int32_t trampoline_entry = kInvalidSlotPos;
  if (!internal_trampoline_exception_) {
    DEBUG_PRINTF("\tstart: %d,pos: %d\n", trampoline_.start(), pos);
    if (trampoline_.start() > pos) {
      trampoline_entry = trampoline_.take_slot();
    }

    if (kInvalidSlotPos == trampoline_entry) {
      internal_trampoline_exception_ = true;
    }
  }
  return trampoline_entry;
}

uint64_t Assembler::jump_address(Label* L) {
  int64_t target_pos;
  DEBUG_PRINTF("jump_address: %p to %p (%d)\n", L,
               reinterpret_cast<Instr*>(buffer_start_ + pc_offset()),
               pc_offset());
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      DEBUG_PRINTF("\tstarted link\n");
      return kEndOfJumpChain;
    }
  }
  uint64_t imm = reinterpret_cast<uint64_t>(buffer_start_) + target_pos;
  DCHECK_EQ(imm & 3, 0);

  return imm;
}

uint64_t Assembler::branch_long_offset(Label* L) {
  int64_t target_pos;

  DEBUG_PRINTF("branch_long_offset: %p to %p (%d)\n", L,
               reinterpret_cast<Instr*>(buffer_start_ + pc_offset()),
               pc_offset());
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      DEBUG_PRINTF("\tstarted link\n");
      return kEndOfJumpChain;
    }
  }
  int64_t offset = target_pos - pc_offset();
  DCHECK_EQ(offset & 3, 0);

  return static_cast<uint64_t>(offset);
}

int32_t Assembler::branch_offset_helper(Label* L, OffsetSize bits) {
  int32_t target_pos;

  DEBUG_PRINTF("branch_offset_helper: %p to %p (%d)\n", L,
               reinterpret_cast<Instr*>(buffer_start_ + pc_offset()),
               pc_offset());
  if (L->is_bound()) {
    target_pos = L->pos();
    DEBUG_PRINTF("\tbound: %d", target_pos);
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset());
      DEBUG_PRINTF("\tadded to link: %d\n", target_pos);
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      DEBUG_PRINTF("\tstarted link\n");
      return kEndOfJumpChain;
    }
  }

  int32_t offset = target_pos - pc_offset();
  DCHECK(is_intn(offset, bits));
  DCHECK_EQ(offset & 1, 0);
  DEBUG_PRINTF("\toffset = %d\n", offset);
  return offset;
}

void Assembler::label_at_put(Label* L, int at_offset) {
  int target_pos;
  DEBUG_PRINTF("label_at_put: %p @ %p (%d)\n", L,
               reinterpret_cast<Instr*>(buffer_start_ + at_offset), at_offset);
  if (L->is_bound()) {
    target_pos = L->pos();
    instr_at_put(at_offset, target_pos + (Code::kHeaderSize - kHeapObjectTag));
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      int32_t imm18 = target_pos - at_offset;
      DCHECK_EQ(imm18 & 3, 0);
      int32_t imm16 = imm18 >> 2;
      DCHECK(is_int16(imm16));
      instr_at_put(at_offset, (imm16 & kImm16Mask));
    } else {
      target_pos = kEndOfJumpChain;
      instr_at_put(at_offset, target_pos);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
    }
    L->link_to(at_offset);
  }
}

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

void Assembler::lui(Register rd, int32_t imm20) { GenInstrU(LUI, rd, imm20); }

void Assembler::auipc(Register rd, int32_t imm20) {
  GenInstrU(AUIPC, rd, imm20);
}

// Jumps

void Assembler::jal(Register rd, int32_t imm21) {
  GenInstrJ(JAL, rd, imm21);
  BlockTrampolinePoolFor(1);
}

void Assembler::jalr(Register rd, Register rs1, int16_t imm12) {
  GenInstrI(0b000, JALR, rd, rs1, imm12);
  BlockTrampolinePoolFor(1);
}

// Branches

void Assembler::beq(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b000, rs1, rs2, imm13);
}

void Assembler::bne(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b001, rs1, rs2, imm13);
}

void Assembler::blt(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b100, rs1, rs2, imm13);
}

void Assembler::bge(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b101, rs1, rs2, imm13);
}

void Assembler::bltu(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b110, rs1, rs2, imm13);
}

void Assembler::bgeu(Register rs1, Register rs2, int16_t imm13) {
  GenInstrBranchCC_rri(0b111, rs1, rs2, imm13);
}

// Loads

void Assembler::lb(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b000, rd, rs1, imm12);
}

void Assembler::lh(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b001, rd, rs1, imm12);
}

void Assembler::lw(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b010, rd, rs1, imm12);
}

void Assembler::lbu(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b100, rd, rs1, imm12);
}

void Assembler::lhu(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b101, rd, rs1, imm12);
}

// Stores

void Assembler::sb(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b000, base, source, imm12);
}

void Assembler::sh(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b001, base, source, imm12);
}

void Assembler::sw(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b010, base, source, imm12);
}

// Arithmetic with immediate

void Assembler::addi(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b000, rd, rs1, imm12);
}

void Assembler::slti(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b010, rd, rs1, imm12);
}

void Assembler::sltiu(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b011, rd, rs1, imm12);
}

void Assembler::xori(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b100, rd, rs1, imm12);
}

void Assembler::ori(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b110, rd, rs1, imm12);
}

void Assembler::andi(Register rd, Register rs1, int16_t imm12) {
  GenInstrALU_ri(0b111, rd, rs1, imm12);
}

void Assembler::slli(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShift_ri(0, 0b001, rd, rs1, shamt & 0x3f);
}

void Assembler::srli(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShift_ri(0, 0b101, rd, rs1, shamt & 0x3f);
}

void Assembler::srai(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShift_ri(1, 0b101, rd, rs1, shamt & 0x3f);
}

// Arithmetic

void Assembler::add(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b000, rd, rs1, rs2);
}

void Assembler::sub(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0100000, 0b000, rd, rs1, rs2);
}

void Assembler::sll(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b001, rd, rs1, rs2);
}

void Assembler::slt(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b010, rd, rs1, rs2);
}

void Assembler::sltu(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b011, rd, rs1, rs2);
}

void Assembler::xor_(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b100, rd, rs1, rs2);
}

void Assembler::srl(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b101, rd, rs1, rs2);
}

void Assembler::sra(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0100000, 0b101, rd, rs1, rs2);
}

void Assembler::or_(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b110, rd, rs1, rs2);
}

void Assembler::and_(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000000, 0b111, rd, rs1, rs2);
}

// Memory fences

void Assembler::fence(uint8_t pred, uint8_t succ) {
  DCHECK(is_uint4(pred) && is_uint4(succ));
  uint16_t imm12 = succ | (pred << 4) | (0b0000 << 8);
  GenInstrI(0b000, MISC_MEM, ToRegister(0), ToRegister(0), imm12);
}

void Assembler::fence_tso() {
  uint16_t imm12 = (0b0011) | (0b0011 << 4) | (0b1000 << 8);
  GenInstrI(0b000, MISC_MEM, ToRegister(0), ToRegister(0), imm12);
}

// Environment call / break

void Assembler::ecall() {
  GenInstrI(0b000, SYSTEM, ToRegister(0), ToRegister(0), 0);
}

void Assembler::ebreak() {
  GenInstrI(0b000, SYSTEM, ToRegister(0), ToRegister(0), 1);
}

// This is a de facto standard (as set by GNU binutils) 32-bit unimplemented
// instruction (i.e., it should always trap, if your implementation has invalid
// instruction traps).
void Assembler::unimp() {
  GenInstrI(0b001, SYSTEM, ToRegister(0), ToRegister(0), 0b110000000000);
}

// CSR

void Assembler::csrrw(Register rd, ControlStatusReg csr, Register rs1) {
  GenInstrCSR_ir(0b001, rd, csr, rs1);
}

void Assembler::csrrs(Register rd, ControlStatusReg csr, Register rs1) {
  GenInstrCSR_ir(0b010, rd, csr, rs1);
}

void Assembler::csrrc(Register rd, ControlStatusReg csr, Register rs1) {
  GenInstrCSR_ir(0b011, rd, csr, rs1);
}

void Assembler::csrrwi(Register rd, ControlStatusReg csr, uint8_t imm5) {
  GenInstrCSR_ii(0b101, rd, csr, imm5);
}

void Assembler::csrrsi(Register rd, ControlStatusReg csr, uint8_t imm5) {
  GenInstrCSR_ii(0b110, rd, csr, imm5);
}

void Assembler::csrrci(Register rd, ControlStatusReg csr, uint8_t imm5) {
  GenInstrCSR_ii(0b111, rd, csr, imm5);
}

// RV64I

void Assembler::lwu(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b110, rd, rs1, imm12);
}

void Assembler::ld(Register rd, Register rs1, int16_t imm12) {
  GenInstrLoad_ri(0b011, rd, rs1, imm12);
}

void Assembler::sd(Register source, Register base, int16_t imm12) {
  GenInstrStore_rri(0b011, base, source, imm12);
}

void Assembler::addiw(Register rd, Register rs1, int16_t imm12) {
  GenInstrI(0b000, OP_IMM_32, rd, rs1, imm12);
}

void Assembler::slliw(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShiftW_ri(0, 0b001, rd, rs1, shamt & 0x1f);
}

void Assembler::srliw(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShiftW_ri(0, 0b101, rd, rs1, shamt & 0x1f);
}

void Assembler::sraiw(Register rd, Register rs1, uint8_t shamt) {
  GenInstrShiftW_ri(1, 0b101, rd, rs1, shamt & 0x1f);
}

void Assembler::addw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000000, 0b000, rd, rs1, rs2);
}

void Assembler::subw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0100000, 0b000, rd, rs1, rs2);
}

void Assembler::sllw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000000, 0b001, rd, rs1, rs2);
}

void Assembler::srlw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000000, 0b101, rd, rs1, rs2);
}

void Assembler::sraw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0100000, 0b101, rd, rs1, rs2);
}

// RV32M Standard Extension

void Assembler::mul(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b000, rd, rs1, rs2);
}

void Assembler::mulh(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b001, rd, rs1, rs2);
}

void Assembler::mulhsu(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b010, rd, rs1, rs2);
}

void Assembler::mulhu(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b011, rd, rs1, rs2);
}

void Assembler::div(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b100, rd, rs1, rs2);
}

void Assembler::divu(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b101, rd, rs1, rs2);
}

void Assembler::rem(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b110, rd, rs1, rs2);
}

void Assembler::remu(Register rd, Register rs1, Register rs2) {
  GenInstrALU_rr(0b0000001, 0b111, rd, rs1, rs2);
}

// RV64M Standard Extension (in addition to RV32M)

void Assembler::mulw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000001, 0b000, rd, rs1, rs2);
}

void Assembler::divw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000001, 0b100, rd, rs1, rs2);
}

void Assembler::divuw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000001, 0b101, rd, rs1, rs2);
}

void Assembler::remw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000001, 0b110, rd, rs1, rs2);
}

void Assembler::remuw(Register rd, Register rs1, Register rs2) {
  GenInstrALUW_rr(0b0000001, 0b111, rd, rs1, rs2);
}

// RV32A Standard Extension

void Assembler::lr_w(bool aq, bool rl, Register rd, Register rs1) {
  GenInstrRAtomic(0b00010, aq, rl, 0b010, rd, rs1, zero_reg);
}

void Assembler::sc_w(bool aq, bool rl, Register rd, Register rs1,
                     Register rs2) {
  GenInstrRAtomic(0b00011, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amoswap_w(bool aq, bool rl, Register rd, Register rs1,
                          Register rs2) {
  GenInstrRAtomic(0b00001, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amoadd_w(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b00000, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amoxor_w(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b00100, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amoand_w(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b01100, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amoor_w(bool aq, bool rl, Register rd, Register rs1,
                        Register rs2) {
  GenInstrRAtomic(0b01000, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amomin_w(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b10000, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amomax_w(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b10100, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amominu_w(bool aq, bool rl, Register rd, Register rs1,
                          Register rs2) {
  GenInstrRAtomic(0b11000, aq, rl, 0b010, rd, rs1, rs2);
}

void Assembler::amomaxu_w(bool aq, bool rl, Register rd, Register rs1,
                          Register rs2) {
  GenInstrRAtomic(0b11100, aq, rl, 0b010, rd, rs1, rs2);
}

// RV64A Standard Extension (in addition to RV32A)

void Assembler::lr_d(bool aq, bool rl, Register rd, Register rs1) {
  GenInstrRAtomic(0b00010, aq, rl, 0b011, rd, rs1, zero_reg);
}

void Assembler::sc_d(bool aq, bool rl, Register rd, Register rs1,
                     Register rs2) {
  GenInstrRAtomic(0b00011, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amoswap_d(bool aq, bool rl, Register rd, Register rs1,
                          Register rs2) {
  GenInstrRAtomic(0b00001, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amoadd_d(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b00000, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amoxor_d(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b00100, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amoand_d(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b01100, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amoor_d(bool aq, bool rl, Register rd, Register rs1,
                        Register rs2) {
  GenInstrRAtomic(0b01000, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amomin_d(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b10000, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amomax_d(bool aq, bool rl, Register rd, Register rs1,
                         Register rs2) {
  GenInstrRAtomic(0b10100, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amominu_d(bool aq, bool rl, Register rd, Register rs1,
                          Register rs2) {
  GenInstrRAtomic(0b11000, aq, rl, 0b011, rd, rs1, rs2);
}

void Assembler::amomaxu_d(bool aq, bool rl, Register rd, Register rs1,
                          Register rs2) {
  GenInstrRAtomic(0b11100, aq, rl, 0b011, rd, rs1, rs2);
}

// RV32F Standard Extension

void Assembler::flw(FPURegister rd, Register rs1, int16_t imm12) {
  GenInstrLoadFP_ri(0b010, rd, rs1, imm12);
}

void Assembler::fsw(FPURegister source, Register base, int16_t imm12) {
  GenInstrStoreFP_rri(0b010, base, source, imm12);
}

void Assembler::fmadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                        FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b00, MADD, rd, rs1, rs2, rs3, frm);
}

void Assembler::fmsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                        FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b00, MSUB, rd, rs1, rs2, rs3, frm);
}

void Assembler::fnmsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                         FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b00, NMSUB, rd, rs1, rs2, rs3, frm);
}

void Assembler::fnmadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                         FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b00, NMADD, rd, rs1, rs2, rs3, frm);
}

void Assembler::fadd_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0000000, frm, rd, rs1, rs2);
}

void Assembler::fsub_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0000100, frm, rd, rs1, rs2);
}

void Assembler::fmul_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0001000, frm, rd, rs1, rs2);
}

void Assembler::fdiv_s(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0001100, frm, rd, rs1, rs2);
}

void Assembler::fsqrt_s(FPURegister rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b0101100, frm, rd, rs1, zero_reg);
}

void Assembler::fsgnj_s(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010000, 0b000, rd, rs1, rs2);
}

void Assembler::fsgnjn_s(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010000, 0b001, rd, rs1, rs2);
}

void Assembler::fsgnjx_s(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010000, 0b010, rd, rs1, rs2);
}

void Assembler::fmin_s(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010100, 0b000, rd, rs1, rs2);
}

void Assembler::fmax_s(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010100, 0b001, rd, rs1, rs2);
}

void Assembler::fcvt_w_s(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100000, frm, rd, rs1, zero_reg);
}

void Assembler::fcvt_wu_s(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100000, frm, rd, rs1, ToRegister(1));
}

void Assembler::fmv_x_w(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110000, 0b000, rd, rs1, zero_reg);
}

void Assembler::feq_s(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010000, 0b010, rd, rs1, rs2);
}

void Assembler::flt_s(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010000, 0b001, rd, rs1, rs2);
}

void Assembler::fle_s(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010000, 0b000, rd, rs1, rs2);
}

void Assembler::fclass_s(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110000, 0b001, rd, rs1, zero_reg);
}

void Assembler::fcvt_s_w(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101000, frm, rd, rs1, zero_reg);
}

void Assembler::fcvt_s_wu(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101000, frm, rd, rs1, ToRegister(1));
}

void Assembler::fmv_w_x(FPURegister rd, Register rs1) {
  GenInstrALUFP_rr(0b1111000, 0b000, rd, rs1, zero_reg);
}

// RV64F Standard Extension (in addition to RV32F)

void Assembler::fcvt_l_s(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100000, frm, rd, rs1, ToRegister(2));
}

void Assembler::fcvt_lu_s(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100000, frm, rd, rs1, ToRegister(3));
}

void Assembler::fcvt_s_l(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101000, frm, rd, rs1, ToRegister(2));
}

void Assembler::fcvt_s_lu(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101000, frm, rd, rs1, ToRegister(3));
}

// RV32D Standard Extension

void Assembler::fld(FPURegister rd, Register rs1, int16_t imm12) {
  GenInstrLoadFP_ri(0b011, rd, rs1, imm12);
}

void Assembler::fsd(FPURegister source, Register base, int16_t imm12) {
  GenInstrStoreFP_rri(0b011, base, source, imm12);
}

void Assembler::fmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                        FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b01, MADD, rd, rs1, rs2, rs3, frm);
}

void Assembler::fmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                        FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b01, MSUB, rd, rs1, rs2, rs3, frm);
}

void Assembler::fnmsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                         FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b01, NMSUB, rd, rs1, rs2, rs3, frm);
}

void Assembler::fnmadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                         FPURegister rs3, RoundingMode frm) {
  GenInstrR4(0b01, NMADD, rd, rs1, rs2, rs3, frm);
}

void Assembler::fadd_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0000001, frm, rd, rs1, rs2);
}

void Assembler::fsub_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0000101, frm, rd, rs1, rs2);
}

void Assembler::fmul_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0001001, frm, rd, rs1, rs2);
}

void Assembler::fdiv_d(FPURegister rd, FPURegister rs1, FPURegister rs2,
                       RoundingMode frm) {
  GenInstrALUFP_rr(0b0001101, frm, rd, rs1, rs2);
}

void Assembler::fsqrt_d(FPURegister rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b0101101, frm, rd, rs1, zero_reg);
}

void Assembler::fsgnj_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010001, 0b000, rd, rs1, rs2);
}

void Assembler::fsgnjn_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010001, 0b001, rd, rs1, rs2);
}

void Assembler::fsgnjx_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010001, 0b010, rd, rs1, rs2);
}

void Assembler::fmin_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010101, 0b000, rd, rs1, rs2);
}

void Assembler::fmax_d(FPURegister rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b0010101, 0b001, rd, rs1, rs2);
}

void Assembler::fcvt_s_d(FPURegister rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b0100000, frm, rd, rs1, ToRegister(1));
}

void Assembler::fcvt_d_s(FPURegister rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b0100001, frm, rd, rs1, zero_reg);
}

void Assembler::feq_d(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010001, 0b010, rd, rs1, rs2);
}

void Assembler::flt_d(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010001, 0b001, rd, rs1, rs2);
}

void Assembler::fle_d(Register rd, FPURegister rs1, FPURegister rs2) {
  GenInstrALUFP_rr(0b1010001, 0b000, rd, rs1, rs2);
}

void Assembler::fclass_d(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110001, 0b001, rd, rs1, zero_reg);
}

void Assembler::fcvt_w_d(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, zero_reg);
}

void Assembler::fcvt_wu_d(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, ToRegister(1));
}

void Assembler::fcvt_d_w(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, zero_reg);
}

void Assembler::fcvt_d_wu(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, ToRegister(1));
}

// RV64D Standard Extension (in addition to RV32D)

void Assembler::fcvt_l_d(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, ToRegister(2));
}

void Assembler::fcvt_lu_d(Register rd, FPURegister rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1100001, frm, rd, rs1, ToRegister(3));
}

void Assembler::fmv_x_d(Register rd, FPURegister rs1) {
  GenInstrALUFP_rr(0b1110001, 0b000, rd, rs1, zero_reg);
}

void Assembler::fcvt_d_l(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, ToRegister(2));
}

void Assembler::fcvt_d_lu(FPURegister rd, Register rs1, RoundingMode frm) {
  GenInstrALUFP_rr(0b1101001, frm, rd, rs1, ToRegister(3));
}

void Assembler::fmv_d_x(FPURegister rd, Register rs1) {
  GenInstrALUFP_rr(0b1111001, 0b000, rd, rs1, zero_reg);
}

// RV64C Standard Extension
void Assembler::c_nop() { GenInstrCI(0b000, C1, zero_reg, 0); }

void Assembler::c_addi(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg && imm6 != 0);
  GenInstrCI(0b000, C1, rd, imm6);
}

void Assembler::c_addiw(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg);
  GenInstrCI(0b001, C1, rd, imm6);
}

void Assembler::c_addi16sp(int16_t imm10) {
  DCHECK(is_int10(imm10) && (imm10 & 0xf) == 0);
  uint8_t uimm6 = ((imm10 & 0x200) >> 4) | (imm10 & 0x10) |
                  ((imm10 & 0x40) >> 3) | ((imm10 & 0x180) >> 6) |
                  ((imm10 & 0x20) >> 5);
  GenInstrCIU(0b011, C1, sp, uimm6);
}

void Assembler::c_addi4spn(Register rd, int16_t uimm10) {
  DCHECK(is_uint10(uimm10) && (uimm10 != 0));
  uint8_t uimm8 = ((uimm10 & 0x4) >> 1) | ((uimm10 & 0x8) >> 3) |
                  ((uimm10 & 0x30) << 2) | ((uimm10 & 0x3c0) >> 4);
  GenInstrCIW(0b000, C0, rd, uimm8);
}

void Assembler::c_li(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg);
  GenInstrCI(0b010, C1, rd, imm6);
}

void Assembler::c_lui(Register rd, int8_t imm6) {
  DCHECK(rd != zero_reg && rd != sp && imm6 != 0);
  GenInstrCI(0b011, C1, rd, imm6);
}

void Assembler::c_slli(Register rd, uint8_t uimm6) {
  DCHECK(rd != zero_reg && uimm6 != 0);
  GenInstrCIU(0b000, C2, rd, uimm6);
}

void Assembler::c_fldsp(FPURegister rd, uint16_t uimm9) {
  DCHECK(is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCIU(0b001, C2, rd, uimm6);
}

void Assembler::c_lwsp(Register rd, uint16_t uimm8) {
  DCHECK(rd != zero_reg && is_uint8(uimm8) && (uimm8 & 0x3) == 0);
  uint8_t uimm6 = (uimm8 & 0x3c) | ((uimm8 & 0xc0) >> 6);
  GenInstrCIU(0b010, C2, rd, uimm6);
}

void Assembler::c_ldsp(Register rd, uint16_t uimm9) {
  DCHECK(rd != zero_reg && is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCIU(0b011, C2, rd, uimm6);
}

void Assembler::c_jr(Register rs1) {
  DCHECK(rs1 != zero_reg);
  GenInstrCR(0b1000, C2, rs1, zero_reg);
  BlockTrampolinePoolFor(1);
}

void Assembler::c_mv(Register rd, Register rs2) {
  DCHECK(rd != zero_reg && rs2 != zero_reg);
  GenInstrCR(0b1000, C2, rd, rs2);
}

void Assembler::c_ebreak() { GenInstrCR(0b1001, C2, zero_reg, zero_reg); }

void Assembler::c_jalr(Register rs1) {
  DCHECK(rs1 != zero_reg);
  GenInstrCR(0b1001, C2, rs1, zero_reg);
  BlockTrampolinePoolFor(1);
}

void Assembler::c_add(Register rd, Register rs2) {
  DCHECK(rd != zero_reg && rs2 != zero_reg);
  GenInstrCR(0b1001, C2, rd, rs2);
}

// CA Instructions
void Assembler::c_sub(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b00, rs2);
}

void Assembler::c_xor(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b01, rs2);
}

void Assembler::c_or(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b10, rs2);
}

void Assembler::c_and(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100011, C1, rd, 0b11, rs2);
}

void Assembler::c_subw(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100111, C1, rd, 0b00, rs2);
}

void Assembler::c_addw(Register rd, Register rs2) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs2.code() & 0b11000) == 0b01000));
  GenInstrCA(0b100111, C1, rd, 0b01, rs2);
}

void Assembler::c_swsp(Register rs2, uint16_t uimm8) {
  DCHECK(is_uint8(uimm8) && (uimm8 & 0x3) == 0);
  uint8_t uimm6 = (uimm8 & 0x3c) | ((uimm8 & 0xc0) >> 6);
  GenInstrCSS(0b110, C2, rs2, uimm6);
}

void Assembler::c_sdsp(Register rs2, uint16_t uimm9) {
  DCHECK(is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCSS(0b111, C2, rs2, uimm6);
}

void Assembler::c_fsdsp(FPURegister rs2, uint16_t uimm9) {
  DCHECK(is_uint9(uimm9) && (uimm9 & 0x7) == 0);
  uint8_t uimm6 = (uimm9 & 0x38) | ((uimm9 & 0x1c0) >> 6);
  GenInstrCSS(0b101, C2, rs2, uimm6);
}

// CL Instructions

void Assembler::c_lw(Register rd, Register rs1, uint16_t uimm7) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint7(uimm7));
  uint8_t uimm5 =
      ((uimm7 & 0x4) >> 1) | ((uimm7 & 0x40) >> 6) | ((uimm7 & 0x38) >> 1);
  GenInstrCL(0b010, C0, rd, rs1, uimm5);
}

void Assembler::c_ld(Register rd, Register rs1, uint16_t uimm8) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCL(0b011, C0, rd, rs1, uimm5);
}

void Assembler::c_fld(FPURegister rd, Register rs1, uint16_t uimm8) {
  DCHECK(((rd.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCL(0b001, C0, rd, rs1, uimm5);
}

// CS Instructions

void Assembler::c_sw(Register rs2, Register rs1, uint16_t uimm7) {
  DCHECK(((rs2.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint7(uimm7));
  uint8_t uimm5 =
      ((uimm7 & 0x4) >> 1) | ((uimm7 & 0x40) >> 6) | ((uimm7 & 0x38) >> 1);
  GenInstrCS(0b110, C0, rs2, rs1, uimm5);
}

void Assembler::c_sd(Register rs2, Register rs1, uint16_t uimm8) {
  DCHECK(((rs2.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCS(0b111, C0, rs2, rs1, uimm5);
}

void Assembler::c_fsd(FPURegister rs2, Register rs1, uint16_t uimm8) {
  DCHECK(((rs2.code() & 0b11000) == 0b01000) &&
         ((rs1.code() & 0b11000) == 0b01000) && is_uint8(uimm8));
  uint8_t uimm5 = ((uimm8 & 0x38) >> 1) | ((uimm8 & 0xc0) >> 6);
  GenInstrCS(0b101, C0, rs2, rs1, uimm5);
}

// CJ Instructions

void Assembler::c_j(int16_t imm12) {
  DCHECK(is_int12(imm12));
  int16_t uimm11 = ((imm12 & 0x800) >> 1) | ((imm12 & 0x400) >> 4) |
                   ((imm12 & 0x300) >> 1) | ((imm12 & 0x80) >> 3) |
                   ((imm12 & 0x40) >> 1) | ((imm12 & 0x20) >> 5) |
                   ((imm12 & 0x10) << 5) | (imm12 & 0xe);
  GenInstrCJ(0b101, C1, uimm11);
  BlockTrampolinePoolFor(1);
}

// Privileged

void Assembler::uret() {
  GenInstrPriv(0b0000000, ToRegister(0), ToRegister(0b00010));
}

void Assembler::sret() {
  GenInstrPriv(0b0001000, ToRegister(0), ToRegister(0b00010));
}

void Assembler::mret() {
  GenInstrPriv(0b0011000, ToRegister(0), ToRegister(0b00010));
}

void Assembler::wfi() {
  GenInstrPriv(0b0001000, ToRegister(0), ToRegister(0b00101));
}

void Assembler::sfence_vma(Register rs1, Register rs2) {
  GenInstrR(0b0001001, 0b000, SYSTEM, ToRegister(0), rs1, rs2);
}

// Assembler Pseudo Instructions (Tables 25.2 and 25.3, RISC-V Unprivileged ISA)

void Assembler::nop() { addi(ToRegister(0), ToRegister(0), 0); }

void Assembler::RV_li(Register rd, int64_t imm) {
  // 64-bit imm is put in the register rd.
  // In most cases the imm is 32 bit and 2 instructions are generated. If a
  // temporary register is available, in the worst case, 6 instructions are
  // generated for a full 64-bit immediate. If temporay register is not
  // available the maximum will be 8 instructions. If imm is more than 32 bits
  // and a temp register is available, imm is divided into two 32-bit parts,
  // low_32 and up_32. Each part is built in a separate register. low_32 is
  // built before up_32. If low_32 is negative (upper 32 bits are 1), 0xffffffff
  // is subtracted from up_32 before up_32 is built. This compensates for 32
  // bits of 1's in the lower when the two registers are added. If no temp is
  // available, the upper 32 bit is built in rd, and the lower 32 bits are
  // devided to 3 parts (11, 11, and 10 bits). The parts are shifted and added
  // to the upper part built in rd.
  if (is_int32(imm + 0x800)) {
    // 32-bit case. Maximum of 2 instructions generated
    int64_t high_20 = ((imm + 0x800) >> 12);
    int64_t low_12 = imm << 52 >> 52;
    if (high_20) {
      lui(rd, (int32_t)high_20);
      if (low_12) {
        addi(rd, rd, low_12);
      }
    } else {
      addi(rd, zero_reg, low_12);
    }
    return;
  } else {
    // 64-bit case: divide imm into two 32-bit parts, upper and lower
    int64_t up_32 = imm >> 32;
    int64_t low_32 = imm & 0xffffffffull;
    Register temp_reg = rd;
    // Check if a temporary register is available
    if (up_32 == 0 || low_32 == 0) {
      // No temp register is needed
    } else {
      UseScratchRegisterScope temps(this);
      BlockTrampolinePoolScope block_trampoline_pool(this);
      temp_reg = temps.hasAvailable() ? temps.Acquire() : no_reg;
    }
    if (temp_reg != no_reg) {
      // keep track of hardware behavior for lower part in sim_low
      int64_t sim_low = 0;
      // Build lower part
      if (low_32 != 0) {
        int64_t high_20 = ((low_32 + 0x800) >> 12);
        int64_t low_12 = low_32 & 0xfff;
        if (high_20) {
          // Adjust to 20 bits for the case of overflow
          high_20 &= 0xfffff;
          sim_low = ((high_20 << 12) << 32) >> 32;
          lui(rd, (int32_t)high_20);
          if (low_12) {
            sim_low += (low_12 << 52 >> 52) | low_12;
            addi(rd, rd, low_12);
          }
        } else {
          sim_low = low_12;
          ori(rd, zero_reg, low_12);
        }
      }
      if (sim_low & 0x100000000) {
        // Bit 31 is 1. Either an overflow or a negative 64 bit
        if (up_32 == 0) {
          // Positive number, but overflow because of the add 0x800
          slli(rd, rd, 32);
          srli(rd, rd, 32);
          return;
        }
        // low_32 is a negative 64 bit after the build
        up_32 = (up_32 - 0xffffffff) & 0xffffffff;
      }
      if (up_32 == 0) {
        return;
      }
      // Build upper part in a temporary register
      if (low_32 == 0) {
        // Build upper part in rd
        temp_reg = rd;
      }
      int64_t high_20 = (up_32 + 0x800) >> 12;
      int64_t low_12 = up_32 & 0xfff;
      if (high_20) {
        // Adjust to 20 bits for the case of overflow
        high_20 &= 0xfffff;
        lui(temp_reg, (int32_t)high_20);
        if (low_12) {
          addi(temp_reg, temp_reg, low_12);
        }
      } else {
        ori(temp_reg, zero_reg, low_12);
      }
      // Put it at the bgining of register
      slli(temp_reg, temp_reg, 32);
      if (low_32 != 0) {
        add(rd, rd, temp_reg);
      }
      return;
    }
    // No temp register. Build imm in rd.
    // Build upper 32 bits first in rd. Divide lower 32 bits parts and add
    // parts to the upper part by doing shift and add.
    // First build upper part in rd.
    int64_t high_20 = (up_32 + 0x800) >> 12;
    int64_t low_12 = up_32 & 0xfff;
    if (high_20) {
      // Adjust to 20 bits for the case of overflow
      high_20 &= 0xfffff;
      lui(rd, (int32_t)high_20);
      if (low_12) {
        addi(rd, rd, low_12);
      }
    } else {
      ori(rd, zero_reg, low_12);
    }
    // upper part already in rd. Each part to be added to rd, has maximum of 11
    // bits, and always starts with a 1. rd is shifted by the size of the part
    // plus the number of zeros between the parts. Each part is added after the
    // left shift.
    uint32_t mask = 0x80000000;
    int32_t shift_val = 0;
    int32_t i;
    for (i = 0; i < 32; i++) {
      if ((low_32 & mask) == 0) {
        mask >>= 1;
        shift_val++;
        if (i == 31) {
          // rest is zero
          slli(rd, rd, shift_val);
        }
        continue;
      }
      // The first 1 seen
      int32_t part;
      if ((i + 11) < 32) {
        // Pick 11 bits
        part = ((uint32_t)(low_32 << i) >> i) >> (32 - (i + 11));
        slli(rd, rd, shift_val + 11);
        ori(rd, rd, part);
        i += 10;
        mask >>= 11;
      } else {
        part = (uint32_t)(low_32 << i) >> i;
        slli(rd, rd, shift_val + (32 - i));
        ori(rd, rd, part);
        break;
      }
      shift_val = 0;
    }
  }
}

int Assembler::li_estimate(int64_t imm, bool is_get_temp_reg) {
  int count = 0;
  // imitate Assembler::RV_li
  if (is_int32(imm + 0x800)) {
    // 32-bit case. Maximum of 2 instructions generated
    int64_t high_20 = ((imm + 0x800) >> 12);
    int64_t low_12 = imm << 52 >> 52;
    if (high_20) {
      count++;
      if (low_12) {
        count++;
      }
    } else {
      count++;
    }
    return count;
  } else {
    // 64-bit case: divide imm into two 32-bit parts, upper and lower
    int64_t up_32 = imm >> 32;
    int64_t low_32 = imm & 0xffffffffull;
    // Check if a temporary register is available
    if (is_get_temp_reg) {
      // keep track of hardware behavior for lower part in sim_low
      int64_t sim_low = 0;
      // Build lower part
      if (low_32 != 0) {
        int64_t high_20 = ((low_32 + 0x800) >> 12);
        int64_t low_12 = low_32 & 0xfff;
        if (high_20) {
          // Adjust to 20 bits for the case of overflow
          high_20 &= 0xfffff;
          sim_low = ((high_20 << 12) << 32) >> 32;
          count++;
          if (low_12) {
            sim_low += (low_12 << 52 >> 52) | low_12;
            count++;
          }
        } else {
          sim_low = low_12;
          count++;
        }
      }
      if (sim_low & 0x100000000) {
        // Bit 31 is 1. Either an overflow or a negative 64 bit
        if (up_32 == 0) {
          // Positive number, but overflow because of the add 0x800
          count++;
          count++;
          return count;
        }
        // low_32 is a negative 64 bit after the build
        up_32 = (up_32 - 0xffffffff) & 0xffffffff;
      }
      if (up_32 == 0) {
        return count;
      }
      int64_t high_20 = (up_32 + 0x800) >> 12;
      int64_t low_12 = up_32 & 0xfff;
      if (high_20) {
        // Adjust to 20 bits for the case of overflow
        high_20 &= 0xfffff;
        count++;
        if (low_12) {
          count++;
        }
      } else {
        count++;
      }
      // Put it at the bgining of register
      count++;
      if (low_32 != 0) {
        count++;
      }
      return count;
    }
    // No temp register. Build imm in rd.
    // Build upper 32 bits first in rd. Divide lower 32 bits parts and add
    // parts to the upper part by doing shift and add.
    // First build upper part in rd.
    int64_t high_20 = (up_32 + 0x800) >> 12;
    int64_t low_12 = up_32 & 0xfff;
    if (high_20) {
      // Adjust to 20 bits for the case of overflow
      high_20 &= 0xfffff;
      count++;
      if (low_12) {
        count++;
      }
    } else {
      count++;
    }
    // upper part already in rd. Each part to be added to rd, has maximum of 11
    // bits, and always starts with a 1. rd is shifted by the size of the part
    // plus the number of zeros between the parts. Each part is added after the
    // left shift.
    uint32_t mask = 0x80000000;
    int32_t shift_val = 0;
    int32_t i;
    for (i = 0; i < 32; i++) {
      if ((low_32 & mask) == 0) {
        mask >>= 1;
        shift_val++;
        if (i == 31) {
          // rest is zero
          count++;
        }
        continue;
      }
      // The first 1 seen
      int32_t part;
      if ((i + 11) < 32) {
        // Pick 11 bits
        part = ((uint32_t)(low_32 << i) >> i) >> (32 - (i + 11));
        count++;
        count++;
        i += 10;
        mask >>= 11;
      } else {
        part = (uint32_t)(low_32 << i) >> i;
        count++;
        count++;
        break;
      }
      shift_val = 0;
    }
  }
  return count;
}

void Assembler::li_ptr(Register rd, int64_t imm) {
  // Initialize rd with an address
  // Pointers are 48 bits
  // 6 fixed instructions are generated
  DCHECK_EQ((imm & 0xfff0000000000000ll), 0);
  int64_t a6 = imm & 0x3f;                      // bits 0:6. 6 bits
  int64_t b11 = (imm >> 6) & 0x7ff;             // bits 6:11. 11 bits
  int64_t high_31 = (imm >> 17) & 0x7fffffff;   // 31 bits
  int64_t high_20 = ((high_31 + 0x800) >> 12);  // 19 bits
  int64_t low_12 = high_31 & 0xfff;             // 12 bits
  lui(rd, (int32_t)high_20);
  addi(rd, rd, low_12);  // 31 bits in rd.
  slli(rd, rd, 11);      // Space for next 11 bis
  ori(rd, rd, b11);      // 11 bits are put in. 42 bit in rd
  slli(rd, rd, 6);       // Space for next 6 bits
  ori(rd, rd, a6);       // 6 bits are put in. 48 bis in rd
}

void Assembler::li_constant(Register rd, int64_t imm) {
  DEBUG_PRINTF("li_constant(%d, %lx <%ld>)\n", ToNumber(rd), imm, imm);
  lui(rd, (imm + (1LL << 47) + (1LL << 35) + (1LL << 23) + (1LL << 11)) >>
              48);  // Bits 63:48
  addiw(rd, rd,
        (imm + (1LL << 35) + (1LL << 23) + (1LL << 11)) << 16 >>
            52);  // Bits 47:36
  slli(rd, rd, 12);
  addi(rd, rd, (imm + (1LL << 23) + (1LL << 11)) << 28 >> 52);  // Bits 35:24
  slli(rd, rd, 12);
  addi(rd, rd, (imm + (1LL << 11)) << 40 >> 52);  // Bits 23:12
  slli(rd, rd, 12);
  addi(rd, rd, imm << 52 >> 52);  // Bits 11:0
}

// Break / Trap instructions.
void Assembler::break_(uint32_t code, bool break_as_stop) {
  // We need to invalidate breaks that could be stops as well because the
  // simulator expects a char pointer after the stop instruction.
  // See constants-mips.h for explanation.
  DCHECK(
      (break_as_stop && code <= kMaxStopCode && code > kMaxWatchpointCode) ||
      (!break_as_stop && (code > kMaxStopCode || code <= kMaxWatchpointCode)));

  // since ebreak does not allow additional immediate field, we use the
  // immediate field of lui instruction immediately following the ebreak to
  // encode the "code" info
  ebreak();
  DCHECK(is_uint20(code));
  lui(zero_reg, code);
}

void Assembler::stop(uint32_t code) {
  DCHECK_GT(code, kMaxWatchpointCode);
  DCHECK_LE(code, kMaxStopCode);
#if defined(V8_HOST_ARCH_RISCV64)
  break_(0x54321);
#else  // V8_HOST_ARCH_RISCV64
  break_(code, true);
#endif
}

// Original MIPS Instructions

// ------------Memory-instructions-------------

bool Assembler::NeedAdjustBaseAndOffset(const MemOperand& src,
                                        OffsetAccessType access_type,
                                        int second_access_add_to_offset) {
  bool two_accesses = static_cast<bool>(access_type);
  DCHECK_LE(second_access_add_to_offset, 7);  // Must be <= 7.

  // is_int12 must be passed a signed value, hence the static cast below.
  if (is_int12(src.offset()) &&
      (!two_accesses || is_int12(static_cast<int32_t>(
                            src.offset() + second_access_add_to_offset)))) {
    // Nothing to do: 'offset' (and, if needed, 'offset + 4', or other specified
    // value) fits into int12.
    return false;
  }
  return true;
}

void Assembler::AdjustBaseAndOffset(MemOperand* src, Register scratch,
                                    OffsetAccessType access_type,
                                    int second_Access_add_to_offset) {
  // This method is used to adjust the base register and offset pair
  // for a load/store when the offset doesn't fit into int12.

  // Must not overwrite the register 'base' while loading 'offset'.
  DCHECK(src->rm() != scratch);

  constexpr int32_t kMinOffsetForSimpleAdjustment = 0x7F8;
  constexpr int32_t kMaxOffsetForSimpleAdjustment =
      2 * kMinOffsetForSimpleAdjustment;
  if (0 <= src->offset() && src->offset() <= kMaxOffsetForSimpleAdjustment) {
    addi(scratch, src->rm(), kMinOffsetForSimpleAdjustment);
    src->offset_ -= kMinOffsetForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= src->offset() &&
             src->offset() < 0) {
    addi(scratch, src->rm(), -kMinOffsetForSimpleAdjustment);
    src->offset_ += kMinOffsetForSimpleAdjustment;
  } else if (access_type == OffsetAccessType::SINGLE_ACCESS) {
    RV_li(scratch, (static_cast<int64_t>(src->offset()) + 0x800) >> 12 << 12);
    add(scratch, scratch, src->rm());
    src->offset_ = src->offset() << 20 >> 20;
  } else {
    RV_li(scratch, src->offset());
    add(scratch, scratch, src->rm());
    src->offset_ = 0;
  }
  src->rm_ = scratch;
}

int Assembler::RelocateInternalReference(RelocInfo::Mode rmode, Address pc,
                                         intptr_t pc_delta) {
  if (RelocInfo::IsInternalReference(rmode)) {
    int64_t* p = reinterpret_cast<int64_t*>(pc);
    if (*p == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    *p += pc_delta;
    return 2;  // Number of instructions patched.
  }
  Instr instr = instr_at(pc);
  DCHECK(RelocInfo::IsInternalReferenceEncoded(rmode));
  if (IsLui(instr)) {
    uint64_t target_address = target_address_at(pc) + pc_delta;
    DEBUG_PRINTF("target_address 0x%lx\n", target_address);
    set_target_value_at(pc, target_address);
    return 8;  // Number of instructions patched.
  } else {
    UNIMPLEMENTED();
    return 1;
  }
}

void Assembler::RelocateRelativeReference(RelocInfo::Mode rmode, Address pc,
                                          intptr_t pc_delta) {
  Instr instr = instr_at(pc);
  Instr instr1 = instr_at(pc + 1 * kInstrSize);
  DCHECK(RelocInfo::IsRelativeCodeTarget(rmode));
  if (IsAuipc(instr) && IsJalr(instr1)) {
    int32_t imm;
    imm = BrachlongOffset(instr, instr1);
    imm -= pc_delta;
    PatchBranchlongOffset(pc, instr, instr1, imm);
    return;
  } else {
    UNREACHABLE();
  }
}

void Assembler::GrowBuffer() {
  DEBUG_PRINTF("GrowBuffer: %p -> ", buffer_start_);
  // Compute new buffer size.
  int old_size = buffer_->size();
  int new_size = std::min(2 * old_size, old_size + 1 * MB);

  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if (new_size > kMaximalBufferSize) {
    V8::FatalProcessOutOfMemory(nullptr, "Assembler::GrowBuffer");
  }

  // Set up new buffer.
  std::unique_ptr<AssemblerBuffer> new_buffer = buffer_->Grow(new_size);
  DCHECK_EQ(new_size, new_buffer->size());
  byte* new_start = new_buffer->start();

  // Copy the data.
  intptr_t pc_delta = new_start - buffer_start_;
  intptr_t rc_delta = (new_start + new_size) - (buffer_start_ + old_size);
  size_t reloc_size = (buffer_start_ + old_size) - reloc_info_writer.pos();
  MemMove(new_start, buffer_start_, pc_offset());
  MemMove(reloc_info_writer.pos() + rc_delta, reloc_info_writer.pos(),
          reloc_size);

  // Switch buffers.
  buffer_ = std::move(new_buffer);
  buffer_start_ = new_start;
  DEBUG_PRINTF("%p\n", buffer_start_);
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate runtime entries.
  Vector<byte> instructions{buffer_start_, pc_offset()};
  Vector<const byte> reloc_info{reloc_info_writer.pos(), reloc_size};
  for (RelocIterator it(instructions, reloc_info, 0); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::INTERNAL_REFERENCE) {
      RelocateInternalReference(rmode, it.rinfo()->pc(), pc_delta);
    }
  }
  DCHECK(!overflow());
}

void Assembler::db(uint8_t data) {
  if (!is_buffer_growth_blocked()) CheckBuffer();
  DEBUG_PRINTF("%p: constant 0x%x\n", pc_, data);
  EmitHelper(data);
}

void Assembler::dd(uint32_t data, RelocInfo::Mode rmode) {
  if (!RelocInfo::IsNone(rmode)) {
    DCHECK(RelocInfo::IsDataEmbeddedObject(rmode));
    RecordRelocInfo(rmode);
  }
  if (!is_buffer_growth_blocked()) CheckBuffer();
  DEBUG_PRINTF("%p: constant 0x%x\n", pc_, data);
  EmitHelper(data);
}

void Assembler::dq(uint64_t data, RelocInfo::Mode rmode) {
  if (!RelocInfo::IsNone(rmode)) {
    DCHECK(RelocInfo::IsDataEmbeddedObject(rmode));
    RecordRelocInfo(rmode);
  }
  if (!is_buffer_growth_blocked()) CheckBuffer();
  DEBUG_PRINTF("%p: constant 0x%lx\n", pc_, data);
  EmitHelper(data);
}

void Assembler::dd(Label* label) {
  uint64_t data;
  if (!is_buffer_growth_blocked()) CheckBuffer();
  if (label->is_bound()) {
    data = reinterpret_cast<uint64_t>(buffer_start_ + label->pos());
  } else {
    data = jump_address(label);
    internal_reference_positions_.insert(label->pos());
  }
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  EmitHelper(data);
}

void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  if (!ShouldRecordRelocInfo(rmode)) return;
  // We do not try to reuse pool constants.
  RelocInfo rinfo(reinterpret_cast<Address>(pc_), rmode, data, Code());
  DCHECK_GE(buffer_space(), kMaxRelocSize);  // Too late to grow buffer here.
  reloc_info_writer.Write(&rinfo);
}

void Assembler::BlockTrampolinePoolFor(int instructions) {
  DEBUG_PRINTF("\tBlockTrampolinePoolFor %d", instructions);
  CheckTrampolinePoolQuick(instructions);
  DEBUG_PRINTF("\tpc_offset %d,BlockTrampolinePoolBefore %d\n", pc_offset(),
               pc_offset() + instructions * kInstrSize);
  BlockTrampolinePoolBefore(pc_offset() + instructions * kInstrSize);
}

void Assembler::CheckTrampolinePool() {
  // Some small sequences of instructions must not be broken up by the
  // insertion of a trampoline pool; such sequences are protected by setting
  // either trampoline_pool_blocked_nesting_ or no_trampoline_pool_before_,
  // which are both checked here. Also, recursive calls to CheckTrampolinePool
  // are blocked by trampoline_pool_blocked_nesting_.
  DEBUG_PRINTF("\tpc_offset %d no_trampoline_pool_before:%d\n", pc_offset(),
               no_trampoline_pool_before_);
  DEBUG_PRINTF("\ttrampoline_pool_blocked_nesting:%d\n",
               trampoline_pool_blocked_nesting_);
  if ((trampoline_pool_blocked_nesting_ > 0) ||
      (pc_offset() < no_trampoline_pool_before_)) {
    // Emission is currently blocked; make sure we try again as soon as
    // possible.
    if (trampoline_pool_blocked_nesting_ > 0) {
      next_buffer_check_ = pc_offset() + kInstrSize;
    } else {
      next_buffer_check_ = no_trampoline_pool_before_;
    }
    return;
  }

  DCHECK(!trampoline_emitted_);
  DCHECK_GE(unbound_labels_count_, 0);
  if (unbound_labels_count_ > 0) {
    // First we emit jump, then we emit trampoline pool.
    {
      DEBUG_PRINTF("inserting trampoline pool at %p (%d)\n",
                   reinterpret_cast<Instr*>(buffer_start_ + pc_offset()),
                   pc_offset());
      BlockTrampolinePoolScope block_trampoline_pool(this);
      Label after_pool;
      j(&after_pool);

      int pool_start = pc_offset();
      for (int i = 0; i < unbound_labels_count_; i++) {
        int64_t imm64;
        imm64 = branch_long_offset(&after_pool);
        DCHECK(is_int32(imm64));
        int32_t Hi20 = (((int32_t)imm64 + 0x800) >> 12);
        int32_t Lo12 = (int32_t)imm64 << 20 >> 20;
        auipc(t6, Hi20);  // Read PC + Hi20 into t6
        jr(t6, Lo12);     // jump PC + Hi20 + Lo12
      }
      // If unbound_labels_count_ is big enough, label after_pool will
      // need a trampoline too, so we must create the trampoline before
      // the bind operation to make sure function 'bind' can get this
      // information.
      trampoline_ = Trampoline(pool_start, unbound_labels_count_);
      bind(&after_pool);

      trampoline_emitted_ = true;
      // As we are only going to emit trampoline once, we need to prevent any
      // further emission.
      next_buffer_check_ = kMaxInt;
    }
  } else {
    // Number of branches to unbound label at this point is zero, so we can
    // move next buffer check to maximum.
    next_buffer_check_ =
        pc_offset() + kMaxBranchOffset - kTrampolineSlotsSize * 16;
  }
  return;
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  Instr* instr = reinterpret_cast<Instr*>(pc);
  if (IsAuipc(*instr)) {
    if (IsLd(*reinterpret_cast<Instr*>(pc + 4))) {
      int32_t Hi20 = AuipcOffset(*instr);
      int32_t Lo12 = LdOffset(*reinterpret_cast<Instr*>(pc + 4));
      Memory<Address>(pc + Hi20 + Lo12) = target;
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        FlushInstructionCache(pc + Hi20 + Lo12, 2 * kInstrSize);
      }
    } else {
      DCHECK(IsJalr(*reinterpret_cast<Instr*>(pc + 4)));
      int64_t imm = (int64_t)target - (int64_t)pc;
      Instr instr = instr_at(pc);
      Instr instr1 = instr_at(pc + 1 * kInstrSize);
      DCHECK(is_int32(imm));
      int num = PatchBranchlongOffset(pc, instr, instr1, (int32_t)imm);
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        FlushInstructionCache(pc, num * kInstrSize);
      }
    }
  } else {
    set_target_address_at(pc, target, icache_flush_mode);
  }
}

Address Assembler::target_address_at(Address pc, Address constant_pool) {
  Instr* instr = reinterpret_cast<Instr*>(pc);
  if (IsAuipc(*instr)) {
    if (IsLd(*reinterpret_cast<Instr*>(pc + 4))) {
      int32_t Hi20 = AuipcOffset(*instr);
      int32_t Lo12 = LdOffset(*reinterpret_cast<Instr*>(pc + 4));
      return Memory<Address>(pc + Hi20 + Lo12);
    } else {
      DCHECK(IsJalr(*reinterpret_cast<Instr*>(pc + 4)));
      int32_t Hi20 = AuipcOffset(*instr);
      int32_t Lo12 = JalrOffset(*reinterpret_cast<Instr*>(pc + 4));
      return pc + Hi20 + Lo12;
    }

  } else {
    return target_address_at(pc);
  }
}
Address Assembler::target_address_at(Address pc) {
  DEBUG_PRINTF("target_address_at: pc: %lx\t", pc);
  Instruction* instr0 = Instruction::At((unsigned char*)pc);
  Instruction* instr1 = Instruction::At((unsigned char*)(pc + 1 * kInstrSize));
  Instruction* instr2 = Instruction::At((unsigned char*)(pc + 2 * kInstrSize));
  Instruction* instr3 = Instruction::At((unsigned char*)(pc + 3 * kInstrSize));
  Instruction* instr4 = Instruction::At((unsigned char*)(pc + 4 * kInstrSize));
  Instruction* instr5 = Instruction::At((unsigned char*)(pc + 5 * kInstrSize));

  // Interpret instructions for address generated by li: See listing in
  // Assembler::set_target_address_at() just below.
  if (IsLui(*reinterpret_cast<Instr*>(instr0)) &&
      IsAddi(*reinterpret_cast<Instr*>(instr1)) &&
      IsSlli(*reinterpret_cast<Instr*>(instr2)) &&
      IsOri(*reinterpret_cast<Instr*>(instr3)) &&
      IsSlli(*reinterpret_cast<Instr*>(instr4)) &&
      IsOri(*reinterpret_cast<Instr*>(instr5))) {
    // Assemble the 64 bit value.
    int64_t addr = (int64_t)(instr0->Imm20UValue() << kImm20Shift) +
                   (int64_t)instr1->Imm12Value();
    addr <<= 11;
    addr |= (int64_t)instr3->Imm12Value();
    addr <<= 6;
    addr |= (int64_t)instr5->Imm12Value();

    DEBUG_PRINTF("addr: %lx\n", addr);
    return static_cast<Address>(addr);
  }
  // We should never get here, force a bad address if we do.
  UNREACHABLE();
}
// On RISC-V, a 48-bit target address is stored in an 6-instruction sequence:
//  lui(reg, (int32_t)high_20); // 19 high bits
//  addi(reg, reg, low_12); // 12 following bits. total is 31 high bits in reg.
//  slli(reg, reg, 11); // Space for next 11 bits
//  ori(reg, reg, b11); // 11 bits are put in. 42 bit in reg
//  slli(reg, reg, 6); // Space for next 6 bits
//  ori(reg, reg, a6); // 6 bits are put in. all 48 bis in reg
//
// Patching the address must replace all instructions, and flush the i-cache.
// Note that this assumes the use of SV48, the 48-bit virtual memory system.
void Assembler::set_target_value_at(Address pc, uint64_t target,
                                    ICacheFlushMode icache_flush_mode) {
  DEBUG_PRINTF("set_target_value_at: pc: %lx\ttarget: %lx\n", pc, target);
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
  DCHECK_EQ((target & 0xffff000000000000ll), 0);
#ifdef DEBUG
  // Check we have the result from a li macro-instruction.
  Instruction* instr0 = Instruction::At((unsigned char*)pc);
  Instruction* instr1 = Instruction::At((unsigned char*)(pc + 1 * kInstrSize));
  Instruction* instr3 = Instruction::At((unsigned char*)(pc + 3 * kInstrSize));
  Instruction* instr5 = Instruction::At((unsigned char*)(pc + 5 * kInstrSize));
  DCHECK(IsLui(*reinterpret_cast<Instr*>(instr0)) &&
         IsAddi(*reinterpret_cast<Instr*>(instr1)) &&
         IsOri(*reinterpret_cast<Instr*>(instr3)) &&
         IsOri(*reinterpret_cast<Instr*>(instr5)));
#endif
  int64_t a6 = target & 0x3f;                     // bits 0:6. 6 bits
  int64_t b11 = (target >> 6) & 0x7ff;            // bits 6:11. 11 bits
  int64_t high_31 = (target >> 17) & 0x7fffffff;  // 31 bits
  int64_t high_20 = ((high_31 + 0x800) >> 12);    // 19 bits
  int64_t low_12 = high_31 & 0xfff;               // 12 bits
  *p = *p & 0xfff;
  *p = *p | ((int32_t)high_20 << 12);
  *(p + 1) = *(p + 1) & 0xfffff;
  *(p + 1) = *(p + 1) | ((int32_t)low_12 << 20);
  *(p + 2) = *(p + 2) & 0xfffff;
  *(p + 2) = *(p + 2) | (11 << 20);
  *(p + 3) = *(p + 3) & 0xfffff;
  *(p + 3) = *(p + 3) | ((int32_t)b11 << 20);
  *(p + 4) = *(p + 4) & 0xfffff;
  *(p + 4) = *(p + 4) | (6 << 20);
  *(p + 5) = *(p + 5) & 0xfffff;
  *(p + 5) = *(p + 5) | ((int32_t)a6 << 20);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc, 8 * kInstrSize);
  }
  DCHECK_EQ(target_address_at(pc), target);
}
UseScratchRegisterScope::UseScratchRegisterScope(Assembler* assembler)
    : available_(assembler->GetScratchRegisterList()),
      old_available_(*available_) {}

UseScratchRegisterScope::~UseScratchRegisterScope() {
  *available_ = old_available_;
}

Register UseScratchRegisterScope::Acquire() {
  DCHECK_NOT_NULL(available_);
  DCHECK_NE(*available_, 0);
  int index = static_cast<int>(base::bits::CountTrailingZeros32(*available_));
  *available_ &= ~(1UL << index);

  return Register::from_code(index);
}

bool UseScratchRegisterScope::hasAvailable() const { return *available_ != 0; }

bool Assembler::IsConstantPoolAt(Instruction* instr) {
  // The constant pool marker is made of two instructions. These instructions
  // will never be emitted by the JIT, so checking for the first one is enough:
  // 0: ld x0, t3, #offset
  Instr instr_value = *reinterpret_cast<Instr*>(instr);

  bool result = IsLd(instr_value) && (instr->RdValue() == kRegCode_zero_reg);
  // It is still worth asserting the marker is complete.
  // 4: j 0
#ifdef DEBUG
  Instruction* instr_fllowing = instr + kInstrSize;
  DCHECK(!result || (IsJal(*reinterpret_cast<Instr*>(instr_fllowing)) &&
                     instr_fllowing->Imm20JValue() == 0 &&
                     instr_fllowing->RdValue() == kRegCode_zero_reg));
#endif
  return result;
}

int Assembler::ConstantPoolSizeAt(Instruction* instr) {
  if (IsConstantPoolAt(instr)) {
    return instr->Imm12Value();
  } else {
    return -1;
  }
}

void Assembler::RecordConstPool(int size) {
  // We only need this for debugger support, to correctly compute offsets in the
  // code.
  Assembler::BlockPoolsScope block_pools(this);
  RecordRelocInfo(RelocInfo::CONST_POOL, static_cast<intptr_t>(size));
}

void Assembler::EmitPoolGuard() {
  // We must generate only one instruction as this is used in scopes that
  // control the size of the code generated.
  j(0);
}

// Constant Pool

void ConstantPool::EmitPrologue(Alignment require_alignment) {
  // Recorded constant pool size is expressed in number of 32-bits words,
  // and includes prologue and alignment, but not the jump around the pool
  // and the size of the marker itself.
  const int marker_size = 1;
  int word_count =
      ComputeSize(Jump::kOmitted, require_alignment) / kInt32Size - marker_size;
  assm_->ld(zero_reg, zero_reg, word_count);
  assm_->EmitPoolGuard();
}

int ConstantPool::PrologueSize(Jump require_jump) const {
  // Prologue is:
  //   j   over  ;; if require_jump
  //   ld x0, t3, #pool_size
  //   j xzr
  int prologue_size = require_jump == Jump::kRequired ? kInstrSize : 0;
  prologue_size += 2 * kInstrSize;
  return prologue_size;
}

void ConstantPool::SetLoadOffsetToConstPoolEntry(int load_offset,
                                                 Instruction* entry_offset,
                                                 const ConstantPoolKey& key) {
  Instr instr_auipc = assm_->instr_at(load_offset);
  Instr instr_ld = assm_->instr_at(load_offset + 4);
  // Instruction to patch must be 'ld t3, t3, offset' with offset == kInstrSize.
  DCHECK(assm_->IsAuipc(instr_auipc));
  DCHECK(assm_->IsLd(instr_ld));
  DCHECK_EQ(assm_->LdOffset(instr_ld), 0);
  DCHECK_EQ(assm_->AuipcOffset(instr_auipc), 0);
  int32_t distance = static_cast<int32_t>(
      reinterpret_cast<Address>(entry_offset) -
      reinterpret_cast<Address>(assm_->toAddress(load_offset)));
  int32_t Hi20 = (((int32_t)distance + 0x800) >> 12);
  int32_t Lo12 = (int32_t)distance << 20 >> 20;
  CHECK(is_int32(distance));
  assm_->instr_at_put(load_offset, SetAuipcOffset(Hi20, instr_auipc));
  assm_->instr_at_put(load_offset + 4, SetLdOffset(Lo12, instr_ld));
}

void ConstantPool::Check(Emission force_emit, Jump require_jump,
                         size_t margin) {
  // Some short sequence of instruction must not be broken up by constant pool
  // emission, such sequences are protected by a ConstPool::BlockScope.
  if (IsBlocked()) {
    // Something is wrong if emission is forced and blocked at the same time.
    DCHECK_EQ(force_emit, Emission::kIfNeeded);
    return;
  }

  // We emit a constant pool only if :
  //  * it is not empty
  //  * emission is forced by parameter force_emit (e.g. at function end).
  //  * emission is mandatory or opportune according to {ShouldEmitNow}.
  if (!IsEmpty() && (force_emit == Emission::kForced ||
                     ShouldEmitNow(require_jump, margin))) {
    // Emit veneers for branches that would go out of range during emission of
    // the constant pool.
    int worst_case_size = ComputeSize(Jump::kRequired, Alignment::kRequired);

    // Check that the code buffer is large enough before emitting the constant
    // pool (this includes the gap to the relocation information).
    int needed_space = worst_case_size + assm_->kGap;
    while (assm_->buffer_space() <= needed_space) {
      assm_->GrowBuffer();
    }

    EmitAndClear(require_jump);
  }
  // Since a constant pool is (now) empty, move the check offset forward by
  // the standard interval.
  SetNextCheckIn(ConstantPool::kCheckInterval);
}

// Pool entries are accessed with pc relative load therefore this cannot be more
// than 1 * MB. Since constant pool emission checks are interval based, and we
// want to keep entries close to the code, we try to emit every 64KB.
const size_t ConstantPool::kMaxDistToPool32 = 1 * MB;
const size_t ConstantPool::kMaxDistToPool64 = 1 * MB;
const size_t ConstantPool::kCheckInterval = 128 * kInstrSize;
const size_t ConstantPool::kApproxDistToPool32 = 64 * KB;
const size_t ConstantPool::kApproxDistToPool64 = kApproxDistToPool32;

const size_t ConstantPool::kOpportunityDistToPool32 = 64 * KB;
const size_t ConstantPool::kOpportunityDistToPool64 = 64 * KB;
const size_t ConstantPool::kApproxMaxEntryCount = 512;

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64

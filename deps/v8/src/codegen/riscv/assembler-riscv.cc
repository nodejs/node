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

#include "src/codegen/riscv/assembler-riscv.h"

#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/safepoint-table.h"
#include "src/common/code-memory-access-inl.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/objects/heap-number-inl.h"

namespace v8 {
namespace internal {
// Get the CPU features enabled by the build. For cross compilation the
// preprocessor symbols __riscv_f and __riscv_d
// can be defined to enable FPU instructions when building the
// snapshot.
static unsigned CpuFeaturesImpliedByCompiler() {
  unsigned answer = 0;
#if defined(__riscv_f) && defined(__riscv_d)
  answer |= 1u << FPU;
#endif  // def __riscv_f

#if (defined __riscv_vector) && (__riscv_v >= 1000000)
  answer |= 1u << RISCV_SIMD;
#endif  // def __riscv_vector && __riscv_v >= 1000000

#if (defined __riscv_zba)
  answer |= 1u << ZBA;
#endif  // def __riscv_zba

#if (defined __riscv_zbb)
  answer |= 1u << ZBB;
#endif  // def __riscv_zbb

#if (defined __riscv_zbs)
  answer |= 1u << ZBS;
#endif  // def __riscv_zbs

#if (defined __riscv_zicond)
  answer |= 1u << ZICOND;
#endif  // def __riscv_zicond
  return answer;
}

#ifdef RISCV_TARGET_SIMULATOR
static unsigned SimulatorFeatures() {
  unsigned answer = 0;
  answer |= 1u << RISCV_SIMD;
  answer |= 1u << ZBA;
  answer |= 1u << ZBB;
  answer |= 1u << ZBS;
  answer |= 1u << ZICOND;
  answer |= 1u << ZICFISS;
  answer |= 1u << FPU;
  return answer;
}
#endif

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(RISCV_SIMD); }

void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();

#ifdef RISCV_TARGET_SIMULATOR
  supported_ |= SimulatorFeatures();
  vlen_ = kSimulatorRvvVLEN;
#endif  // RISCV_TARGET_SIMULATOR
  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;
  // Probe for additional features at runtime.

#ifndef USE_SIMULATOR
  base::CPU cpu;
  if (cpu.has_fpu()) supported_ |= 1u << FPU;
  if (cpu.has_rvv()) {
    supported_ |= 1u << RISCV_SIMD;
    vlen_ = cpu.vlen();
    DCHECK_NE(vlen_, base::CPU::kUnknownVlen);
  }
  if (cpu.has_zba()) supported_ |= 1u << ZBA;
  if (cpu.has_zbb()) supported_ |= 1u << ZBB;
  if (cpu.has_zbs()) supported_ |= 1u << ZBS;
  if (v8_flags.riscv_b_extension) {
    supported_ |= (1u << ZBA) | (1u << ZBB) | (1u << ZBS);
  }
#ifdef V8_COMPRESS_POINTERS
  if (cpu.riscv_mmu() == base::CPU::RV_MMU_MODE::kRiscvSV57) {
    FATAL("SV57 is not supported");
    UNIMPLEMENTED();
  }
#endif  // V8_COMPRESS_POINTERS
#endif  // USE_SIMULATOR
  // Set a static value on whether SIMD is supported.
  // This variable is only used for certain archs to query SupportWasmSimd128()
  // at runtime in builtins using an extern ref. Other callers should use
  // CpuFeatures::SupportWasmSimd128().
  CpuFeatures::supports_wasm_simd_128_ = CpuFeatures::SupportsWasmSimd128();
}

void CpuFeatures::PrintTarget() {}
void CpuFeatures::PrintFeatures() {
  printf("supports_wasm_simd_128=%d", CpuFeatures::SupportsWasmSimd128());
  if (CpuFeatures::SupportsWasmSimd128()) {
    printf(", vlen=%u", CpuFeatures::vlen());
  }
  printf("\n");
  printf("RISC-V Extension zba=%d,zbb=%d,zbs=%d,ZICOND=%d\n",
         CpuFeatures::IsSupported(ZBA), CpuFeatures::IsSupported(ZBB),
         CpuFeatures::IsSupported(ZBS), CpuFeatures::IsSupported(ZICOND));
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo.

const int RelocInfo::kApplyMask =
    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
    RelocInfo::ModeMask(RelocInfo::NEAR_BUILTIN_ENTRY) |
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
  Instr instr = Assembler::instr_at(pc_);
  Instr instr1 = Assembler::instr_at(pc_ + 1 * kInstrSize);
  if (Assembler::IsAuipc(instr) && Assembler::IsJalr(instr1)) {
    DCHECK(reinterpret_cast<Instruction*>(pc_)->RdValue() ==
           reinterpret_cast<Instruction*>(pc_ + 4)->Rs1Value());
    return Assembler::BranchLongOffset(instr, instr1);
  } else {
    return static_cast<uint32_t>(
        Assembler::target_address_at(pc_, constant_pool_));
  }
}

// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand.
// See assembler-riscv-inl.h for inlined constructors.

Operand::Operand(Handle<HeapObject> handle, RelocInfo::Mode rmode)
    : rm_(no_reg), rmode_(rmode) {
  DCHECK(RelocInfo::IsEmbeddedObjectMode(rmode) || RelocInfo::IsNoInfo(rmode));
  value_.immediate = static_cast<intptr_t>(handle.address());
}

Operand Operand::EmbeddedNumber(double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) return Operand(Smi::FromInt(smi));
  Operand result(0, RelocInfo::FULL_EMBEDDED_OBJECT);
  result.is_heap_number_request_ = true;
  result.value_.heap_number_request = HeapNumberRequest(value);
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

void Assembler::PatchInHeapNumberRequest(Address pc,
                                         Handle<HeapNumber> object) {
#ifdef V8_TARGET_ARCH_RISCV64
  EmbeddedObjectIndex index = AddEmbeddedObject(object);
  set_embedded_object_index_referenced_from(pc, index);
#else
  set_target_value_at(pc, reinterpret_cast<uintptr_t>(object.location()));
#endif
}

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

Assembler::Assembler(const AssemblerOptions& options,
                     std::unique_ptr<AssemblerBuffer> buffer)
    : AssemblerBase(options, std::move(buffer)),
      VU(this),
      scratch_register_list_(DefaultTmpList()),
      scratch_double_register_list_(DefaultFPTmpList()),
      constpool_(this) {
  reloc_info_writer.Reposition(buffer_start_ + buffer_->size(), pc_);

  trampoline_check_ = v8_flags.force_long_branches
                          ? kMaxInt
                          : kMaxBranchOffset - BlockPoolsScope::kGap;
  CHECK(!v8_flags.force_long_branches || is_trampoline_emitted());
}

void Assembler::StartBlockPools(int margin) {
  int current = pools_blocked_nesting_;
  if (current == 0) {
    CheckTrampolinePoolQuick(margin);
    // TODO(kasperl): Once we can compute the next trampoline check
    // reliably, we can disable the trampoline checks here.
  }
  pools_blocked_nesting_ = current + 1;
  DEBUG_PRINTF("\tStartBlockPools @ %d (nesting=%d)\n", pc_offset(),
               pools_blocked_nesting_);
}

void Assembler::EndBlockPools() {
  pools_blocked_nesting_--;
  DEBUG_PRINTF("\tEndBlockPools @ %d (nesting=%d)\n", pc_offset(),
               pools_blocked_nesting_);
  DCHECK_GE(pools_blocked_nesting_, 0);
  if (pools_blocked_nesting_ > 0) return;  // Still blocked.
  CheckTrampolinePoolQuick(0);
}

void Assembler::AbortedCodeGeneration() { constpool_.Clear(); }
Assembler::~Assembler() { CHECK(constpool_.IsEmpty()); }

void Assembler::GetCode(Isolate* isolate, CodeDesc* desc) {
  GetCode(isolate->main_thread_local_isolate(), desc);
}
void Assembler::GetCode(LocalIsolate* isolate, CodeDesc* desc,
                        SafepointTableBuilderBase* safepoint_table_builder,
                        int handler_table_offset) {
  // In most cases, the constant pool will already have been emitted when
  // we get here. For now, it is safe to call this again, but it might be
  // worth changing its name to make that clearer.
  FinishCode();
  DCHECK(constpool_.IsEmpty());

  // As a crutch to avoid having to add manual Align calls wherever we use a
  // raw workflow to create InstructionStream objects (mostly in tests), add
  // another Align call here. It does no harm - the end of the InstructionStream
  // object is aligned to the (larger) kCodeAlignment anyways.
  // TODO(jgruber): Consider moving responsibility for proper alignment to
  // metadata table builders (safepoint, handler, constant pool, code
  // comments).
  DataAlign(InstructionStream::kMetadataAlignment);

  int code_comments_size = WriteCodeComments();
  DCHECK_GE(buffer_space(), 0);  // No buffer overflow.

  AllocateAndInstallRequestedHeapNumbers(isolate);

  // Set up code descriptor.
  // TODO(jgruber): Reconsider how these offsets and sizes are maintained up to
  // this point to make CodeDesc initialization less fiddly.

  static constexpr int kConstantPoolSize = 0;
  static constexpr int kBuiltinJumpTableInfoSize = 0;
  const int instruction_size = pc_offset();
  const int builtin_jump_table_info_offset =
      instruction_size - kBuiltinJumpTableInfoSize;
  const int code_comments_offset =
      builtin_jump_table_info_offset - code_comments_size;
  const int constant_pool_offset = code_comments_offset - kConstantPoolSize;
  const int handler_table_offset2 = (handler_table_offset == kNoHandlerTable)
                                        ? constant_pool_offset
                                        : handler_table_offset;
  const int safepoint_table_offset =
      (safepoint_table_builder == kNoSafepointTable)
          ? handler_table_offset2
          : safepoint_table_builder->safepoint_table_offset();
  const int reloc_info_offset =
      static_cast<int>(reloc_info_writer.pos() - buffer_->start());
  CodeDesc::Initialize(desc, this, safepoint_table_offset,
                       handler_table_offset2, constant_pool_offset,
                       code_comments_offset, builtin_jump_table_info_offset,
                       reloc_info_offset);
}

void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo(m));
  while ((pc_offset() & (m - 1)) != 0) {
    NOP();
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
static constexpr int kEndOfChain = -1;
static constexpr int kEndOfJumpChain = 0;

int Assembler::target_at(int pos, bool is_internal) {
  if (is_internal) {
    uintptr_t* p = reinterpret_cast<uintptr_t*>(buffer_start_ + pos);
    uintptr_t address = *p;
    if (address == kEndOfJumpChain) {
      return kEndOfChain;
    } else {
      uintptr_t instr_address = reinterpret_cast<uintptr_t>(p);
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
  DisassembleInstruction(buffer_start_ + pos);

  switch (instruction->InstructionOpcodeType()) {
    case BRANCH: {
      int32_t imm13 = BranchOffset(instr);
      if (imm13 == kEndOfJumpChain) {
        // EndOfChain sentinel is returned directly, not relative to pc or pos.
        return kEndOfChain;
      } else {
        return pos + imm13;
      }
    }
    case JAL: {
      int32_t imm21 = JumpOffset(instr);
      if (imm21 == kEndOfJumpChain) {
        // EndOfChain sentinel is returned directly, not relative to pc or pos.
        return kEndOfChain;
      } else {
        return pos + imm21;
      }
    }
    case JALR: {
      int32_t imm12 = instr >> 20;
      if (imm12 == kEndOfJumpChain) {
        // EndOfChain sentinel is returned directly, not relative to pc or pos.
        return kEndOfChain;
      } else {
        return pos + imm12;
      }
    }
    case LUI: {
      Address pc = reinterpret_cast<Address>(buffer_start_ + pos);
      pc = target_constant_address_at(pc);
      uintptr_t instr_address =
          reinterpret_cast<uintptr_t>(buffer_start_ + pos);
      uintptr_t imm = reinterpret_cast<uintptr_t>(pc);
      if (imm == kEndOfJumpChain) {
        return kEndOfChain;
      } else {
        DCHECK(instr_address - imm < INT_MAX);
        int32_t delta = static_cast<int32_t>(instr_address - imm);
        DCHECK(pos > delta);
        return pos - delta;
      }
    }
    case AUIPC: {
      Instr instr_auipc = instr;
      Instr instr_I = instr_at(pos + 4);
      DCHECK(IsJalr(instr_I) || IsAddi(instr_I));
      int32_t offset = BranchLongOffset(instr_auipc, instr_I);
      if (offset == kEndOfJumpChain) return kEndOfChain;
      return offset + pos;
    }
    case RO_C_J: {
      int32_t offset = instruction->RvcImm11CJValue();
      if (offset == kEndOfJumpChain) return kEndOfChain;
      return offset + pos;
    }
    case RO_C_BNEZ:
    case RO_C_BEQZ: {
      int32_t offset = instruction->RvcImm8BValue();
      if (offset == kEndOfJumpChain) return kEndOfChain;
      return pos + offset;
    }
    default: {
      if (instr == kEndOfJumpChain) {
        return kEndOfChain;
      } else {
        int32_t imm18 =
            ((instr & static_cast<int32_t>(kImm16Mask)) << 16) >> 14;
        return (imm18 + pos);
      }
    }
  }
}

[[nodiscard]] static inline Instr SetBranchOffset(int32_t pos,
                                                  int32_t target_pos,
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

[[nodiscard]] static inline Instr SetJalOffset(int32_t pos, int32_t target_pos,
                                               Instr instr) {
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

[[nodiscard]] static inline ShortInstr SetCJalOffset(int32_t pos,
                                                     int32_t target_pos,
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
[[nodiscard]] static inline Instr SetCBranchOffset(int32_t pos,
                                                   int32_t target_pos,
                                                   Instr instr) {
  DCHECK(Assembler::IsCBranch(instr));
  int32_t imm = target_pos - pos;
  DCHECK_EQ(imm & 1, 0);
  DCHECK(is_intn(imm, Assembler::kCBranchOffsetBits));

  instr &= ~kRvcBImm8Mask;
  int32_t imm8 = ((imm & 0x20) >> 5) | ((imm & 0x6)) | ((imm & 0xc0) >> 3) |
                 ((imm & 0x18) << 2) | ((imm & 0x100) >> 1);
  imm8 = ((imm8 & 0x1f) << 2) | ((imm8 & 0xe0) << 5);
  DCHECK(Assembler::IsCBranch(instr | imm8 & kRvcBImm8Mask));

  return instr | (imm8 & kRvcBImm8Mask);
}

// We have to use a temporary register for things that can be relocated even
// if they can be encoded in RISC-V's 12 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Assembler::MustUseReg(RelocInfo::Mode rmode) {
  return !RelocInfo::IsNoInfo(rmode);
}

void Assembler::DisassembleInstructionHelper(uint8_t* pc) {
  CHECK(v8_flags.riscv_debug);
  disasm::NameConverter converter;
  disasm::Disassembler disasm(converter);
  base::EmbeddedVector<char, 128> disasm_buffer;

  disasm.InstructionDecode(disasm_buffer, pc);
  DEBUG_PRINTF("%s\n", disasm_buffer.begin());
}

void Assembler::target_at_put(int pos, int target_pos, bool is_internal) {
  if (is_internal) {
    uintptr_t imm = reinterpret_cast<uintptr_t>(buffer_start_) + target_pos;
    *reinterpret_cast<uintptr_t*>(buffer_start_ + pos) = imm;
    return;
  }
  DEBUG_PRINTF("\ttarget_at_put: %p (%d) to %p (%d)\n",
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
      DCHECK(IsJal(instr));
      // We only use the 'jal' instruction if we're certain the final offset
      // will fit in the legal jump offset range, so we never need to patch
      // this to an 'auipc; jalr' sequence.
      DCHECK(is_intn(target_pos - pos, Assembler::kJumpOffsetBits));
      instr = SetJalOffset(pos, target_pos, instr);
      instr_at_put(pos, instr);
    } break;
    case LUI: {
      Address pc = reinterpret_cast<Address>(buffer_start_ + pos);
      set_target_value_at(
          pc, reinterpret_cast<uintptr_t>(buffer_start_ + target_pos));
    } break;
    case AUIPC: {
      // It would be possible to replace the two instruction 'auipc; jalr'
      // sequence with 'jal; nop' if the offset was in the legal jump offset
      // range, but it would introduce a subtle bug: There may be a safepoint
      // recorded after the call and we must be sure that the return address
      // stored in register ra matches the safepoint after any changes to the
      // instructions. The use of 'jal' would move the value of register back
      // with the size of instruction and break finding the correct safepoint.
      Instr instr_auipc = instr;
      Instr instr_I = instr_at(pos + 4);
      DCHECK(IsJalr(instr_I) || IsAddi(instr_I));

      intptr_t offset = target_pos - pos;
      CHECK(is_int32(offset + 0x800));
      int32_t Hi20 = (static_cast<int32_t>(offset) + 0x800) >> 12;
      int32_t Lo12 = static_cast<int32_t>(offset) << 20 >> 20;

      instr_auipc = SetHi20Offset(Hi20, instr_auipc);
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
    case RO_C_BNEZ:
    case RO_C_BEQZ: {
      instr = SetCBranchOffset(pos, target_pos, instr);
      instr_at_put(pos, instr);
    } break;
    default: {
      // Emitted label constant, not part of a branch.
      // Make label relative to Code pointer of generated InstructionStream
      // object.
      instr_at_put(
          pos, target_pos + (InstructionStream::kHeaderSize - kHeapObjectTag));
    } break;
  }

  DisassembleInstruction(buffer_start_ + pos);
  if (instruction->InstructionOpcodeType() == AUIPC) {
    DisassembleInstruction(buffer_start_ + pos + 4);
  }
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
  DEBUG_PRINTF("\tbinding %d to label %p\n", pos, L);
  if (L->is_linked() && !is_trampoline_emitted()) {
    unbound_labels_count_--;
    trampoline_check_ += kTrampolineSlotsSize;
  }

  int trampoline_pos = kInvalidSlotPos;
  while (L->is_linked()) {
    int fixup_pos = L->pos();
    int dist = pos - fixup_pos;
    bool is_internal = is_internal_reference(L);
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
            trampoline_pos = GetTrampolineEntry(fixup_pos);
          }
          DEBUG_PRINTF("\t\ttrampolining: %d\n", trampoline_pos);
          CHECK((trampoline_pos - fixup_pos) <= kMaxBranchOffset);
          target_at_put(fixup_pos, trampoline_pos, false);
          fixup_pos = trampoline_pos;
        }
        target_at_put(fixup_pos, pos, false);
      } else if (IsJal(instr)) {
        if (dist > kMaxJumpOffset) {
          if (trampoline_pos == kInvalidSlotPos) {
            trampoline_pos = GetTrampolineEntry(fixup_pos);
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
}

void Assembler::bind(Label* L) {
  DCHECK(!L->is_bound());  // Label can only be bound once.
  bind_to(L, pc_offset());
  VU.clear();
}

void Assembler::next(Label* L, bool is_internal) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos(), is_internal);
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    DCHECK_GE(link, 0);
    DEBUG_PRINTF("\tnext: %p to %p (%d)\n", L,
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

int Assembler::BranchLongOffset(Instr auipc, Instr instr_I) {
  DCHECK(reinterpret_cast<Instruction*>(&instr_I)->InstructionType() ==
         InstructionBase::kIType);
  DCHECK(IsAuipc(auipc));
  DCHECK_EQ((auipc & kRdFieldMask) >> kRdShift,
            (instr_I & kRs1FieldMask) >> kRs1Shift);
  int32_t imm_auipc = AuipcOffset(auipc);
  int32_t imm12 = static_cast<int32_t>(instr_I & kImm12Mask) >> 20;
  int32_t offset = imm12 + imm_auipc;
  return offset;
}

int Assembler::PatchBranchLongOffset(Address pc, Instr instr_auipc,
                                     Instr instr_jalr, int32_t offset,
                                     WritableJitAllocation* jit_allocation) {
  DCHECK(IsAuipc(instr_auipc));
  DCHECK(IsJalr(instr_jalr));
  CHECK(is_int32(offset + 0x800));
  int32_t Hi20 = (static_cast<int32_t>(offset) + 0x800) >> 12;
  int32_t Lo12 = static_cast<int32_t>(offset) << 20 >> 20;
  instr_at_put(pc, SetHi20Offset(Hi20, instr_auipc), jit_allocation);
  instr_at_put(pc + kInstrSize, SetLo12Offset(Lo12, instr_jalr),
               jit_allocation);
  DCHECK(offset == BranchLongOffset(Assembler::instr_at(pc),
                                    Assembler::instr_at(pc + 4)));
  return 2;
}

// Returns the next free trampoline entry.
int32_t Assembler::GetTrampolineEntry(int32_t pos) {
  DEBUG_PRINTF("\ttrampoline start: %d, pos: %d\n", trampoline_.start(), pos);
  CHECK(trampoline_.start() > pos);
  int32_t entry = trampoline_.take_slot();
  CHECK_NE(entry, kInvalidSlotPos);
  return entry;
}

int32_t Assembler::branch_long_offset(Label* L) {
  intptr_t target_pos;

  DEBUG_PRINTF("\tbranch_long_offset: %p to %p (%d)\n", L,
               reinterpret_cast<Instr*>(buffer_start_ + pc_offset()),
               pc_offset());
  if (L->is_bound()) {
    target_pos = L->pos();
    DEBUG_PRINTF("\tbound: %" PRIdPTR "\n", target_pos);
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset());
      DEBUG_PRINTF("\tadded to link: %" PRIdPTR "\n", target_pos);
    } else {
      L->link_to(pc_offset());
      if (!is_trampoline_emitted()) {
        unbound_labels_count_++;
        trampoline_check_ -= kTrampolineSlotsSize;
      }
      DEBUG_PRINTF("\tstarted link\n");
      return kEndOfJumpChain;
    }
  }
  intptr_t offset = target_pos - pc_offset();
  if (v8_flags.riscv_c_extension) {
    DCHECK_EQ(offset & 1, 0);
  } else {
    DCHECK_EQ(offset & 3, 0);
  }
  DCHECK(is_int32(offset));
  return static_cast<int32_t>(offset);
}

int32_t Assembler::branch_offset_helper(Label* L, OffsetSize bits) {
  int32_t offset = branch_long_offset(L);
  DCHECK(is_intn(offset, bits));
  DEBUG_PRINTF("\toffset = %d\n", offset);
  return offset;
}

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

// Definitions for using compressed vs non compressed

void Assembler::NOP() {
  if (v8_flags.riscv_c_extension) {
    c_nop();
  } else {
    nop();
  }
}

void Assembler::EBREAK() {
  if (v8_flags.riscv_c_extension) {
    c_ebreak();
  } else {
    ebreak();
  }
}

// Assembler Pseudo Instructions (Tables 25.2 and 25.3, RISC-V Unprivileged ISA)

void Assembler::nop() { addi(zero_reg, zero_reg, 0); }

inline int64_t SignExtend(uint64_t V, int N) {
  return static_cast<int64_t>(V << (64 - N)) >> (64 - N);
}

#if V8_TARGET_ARCH_RISCV64
void Assembler::RV_li(Register rd, int64_t imm) {
  UseScratchRegisterScope temps(this);
  if (RecursiveLiCount(imm) > GeneralLiCount(imm, temps.CanAcquire())) {
    GeneralLi(rd, imm);
  } else {
    RecursiveLi(rd, imm);
  }
}

int Assembler::RV_li_count(int64_t imm, bool is_get_temp_reg) {
  if (RecursiveLiCount(imm) > GeneralLiCount(imm, is_get_temp_reg)) {
    return GeneralLiCount(imm, is_get_temp_reg);
  } else {
    return RecursiveLiCount(imm);
  }
}

void Assembler::GeneralLi(Register rd, int64_t imm) {
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
      lui(rd, static_cast<int32_t>(high_20));
      if (low_12) {
        addi(rd, rd, low_12);
      }
    } else {
      addi(rd, zero_reg, low_12);
    }
    return;
  } else {
    UseScratchRegisterScope temps(this);
    // 64-bit case: divide imm into two 32-bit parts, upper and lower
    int64_t up_32 = imm >> 32;
    int64_t low_32 = imm & 0xffffffffull;
    Register temp_reg = rd;
    // Check if a temporary register is available
    if (up_32 == 0 || low_32 == 0) {
      // No temp register is needed
    } else {
      temp_reg = temps.CanAcquire() ? temps.Acquire() : no_reg;
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
          lui(rd, static_cast<int32_t>(high_20));
          if (low_12) {
            sim_low += (low_12 << 52 >> 52) | low_12;
            addi(rd, rd, low_12);
          }
        } else {
          sim_low = low_12;
          addi(rd, zero_reg, low_12);
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
        lui(temp_reg, static_cast<int32_t>(high_20));
        if (low_12) {
          addi(temp_reg, temp_reg, low_12);
        }
      } else {
        addi(temp_reg, zero_reg, low_12);
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
      lui(rd, static_cast<int32_t>(high_20));
      if (low_12) {
        addi(rd, rd, low_12);
      }
    } else {
      addi(rd, zero_reg, low_12);
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
        part = (static_cast<uint32_t>(low_32 << i) >> i) >> (32 - (i + 11));
        slli(rd, rd, shift_val + 11);
        ori(rd, rd, part);
        i += 10;
        mask >>= 11;
      } else {
        part = static_cast<uint32_t>(low_32 << i) >> i;
        slli(rd, rd, shift_val + (32 - i));
        ori(rd, rd, part);
        break;
      }
      shift_val = 0;
    }
  }
}

void Assembler::li_ptr(Register rd, int64_t imm) {
#ifdef RISCV_USE_SV39
  // Initialize rd with an address
  // Pointers are 39 bits
  // 4 fixed instructions are generated
  DCHECK_EQ((imm & 0xffffff8000000000ll), 0);
  int64_t a8 = imm & 0xff;                      // bits 0:7. 8 bits
  int64_t high_31 = (imm >> 8) & 0x7fffffff;    // 31 bits
  int64_t high_20 = ((high_31 + 0x800) >> 12);  // 19 bits
  int64_t low_12 = high_31 & 0xfff;             // 12 bits
  lui(rd, static_cast<int32_t>(high_20));
  addi(rd, rd, low_12);  // 31 bits in rd.
  slli(rd, rd, 8);       // Space for next 8 bis
  ori(rd, rd, a8);       // 8 bits are put in.
#else
  // Initialize rd with an address
  // Pointers are 48 bits
  // 6 fixed instructions are generated
  DCHECK_EQ((imm & 0xfff0000000000000ll), 0);
  int64_t a6 = imm & 0x3f;                      // bits 0:5. 6 bits
  int64_t b11 = (imm >> 6) & 0x7ff;             // bits 6:11. 11 bits
  int64_t high_31 = (imm >> 17) & 0x7fffffff;   // 31 bits
  int64_t high_20 = ((high_31 + 0x800) >> 12);  // 19 bits
  int64_t low_12 = high_31 & 0xfff;             // 12 bits
  lui(rd, static_cast<int32_t>(high_20));
  addi(rd, rd, low_12);  // 31 bits in rd.
  slli(rd, rd, 11);      // Space for next 11 bis
  ori(rd, rd, b11);      // 11 bits are put in. 42 bit in rd
  slli(rd, rd, 6);       // Space for next 6 bits
  ori(rd, rd, a6);       // 6 bits are put in. 48 bis in rd
#endif
}

void Assembler::li_constant(Register rd, int64_t imm) {
  DEBUG_PRINTF("\tli_constant(%d, %" PRIx64 " <%" PRId64 ">)\n", rd.code(), imm,
               imm);
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

void Assembler::li_constant32(Register rd, int32_t imm) {
  ASM_CODE_COMMENT(this);
  DEBUG_PRINTF("\tli_constant(%d, %x <%d>)\n", rd.code(), imm, imm);
  int32_t high_20 = ((imm + 0x800) >> 12);  // bits31:12
  int32_t low_12 = imm & 0xfff;             // bits11:0
  lui(rd, high_20);
  addi(rd, rd, low_12);
}

#elif V8_TARGET_ARCH_RISCV32
void Assembler::RV_li(Register rd, int32_t imm) {
  int32_t high_20 = ((imm + 0x800) >> 12);
  int32_t low_12 = imm & 0xfff;
  if (high_20) {
    lui(rd, high_20);
    if (low_12) {
      addi(rd, rd, low_12);
    }
  } else {
    addi(rd, zero_reg, low_12);
  }
}

int Assembler::RV_li_count(int32_t imm, bool is_get_temp_reg) {
  int count = 0;
  // imitate Assembler::RV_li
  int32_t high_20 = ((imm + 0x800) >> 12);
  int32_t low_12 = imm & 0xfff;
  if (high_20) {
    count++;
    if (low_12) {
      count++;
    }
  } else {
    // if high_20 is 0, always need one instruction to load the low_12 bit
    count++;
  }

  return count;
}

void Assembler::li_ptr(Register rd, int32_t imm) {
  // Initialize rd with an address
  // Pointers are 32 bits
  // 2 fixed instructions are generated
  int32_t high_20 = ((imm + 0x800) >> 12);  // bits31:12
  int32_t low_12 = imm & 0xfff;             // bits11:0
  lui(rd, high_20);
  addi(rd, rd, low_12);
}

void Assembler::li_constant(Register rd, int32_t imm) {
  ASM_CODE_COMMENT(this);
  DEBUG_PRINTF("\tli_constant(%d, %x <%d>)\n", rd.code(), imm, imm);
  int32_t high_20 = ((imm + 0x800) >> 12);  // bits31:12
  int32_t low_12 = imm & 0xfff;             // bits11:0
  lui(rd, high_20);
  addi(rd, rd, low_12);
}
#endif

// Break / Trap instructions.
void Assembler::break_(uint32_t code, bool break_as_stop) {
  // We need to invalidate breaks that could be stops as well because the
  // simulator expects a char pointer after the stop instruction.
  // See base-constants-riscv.h for explanation.
  DCHECK(
      (break_as_stop && code <= kMaxStopCode && code > kMaxTracepointCode) ||
      (!break_as_stop && (code > kMaxStopCode || code <= kMaxTracepointCode)));

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
#if defined(V8_HOST_ARCH_RISCV64) || defined(V8_HOST_ARCH_RISCV32)
  break_(0x54321);
#else  // V8_HOST_ARCH_RISCV64 || V8_HOST_ARCH_RISCV32
  break_(code, true);
#endif
}

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
    RV_li(scratch, (static_cast<intptr_t>(src->offset()) + 0x800) >> 12 << 12);
    add(scratch, scratch, src->rm());
    src->offset_ = src->offset() << 20 >> 20;
  } else {
    RV_li(scratch, src->offset());
    add(scratch, scratch, src->rm());
    src->offset_ = 0;
  }
  src->rm_ = scratch;
}

int Assembler::RelocateInternalReference(
    RelocInfo::Mode rmode, Address pc, intptr_t pc_delta,
    WritableJitAllocation* jit_allocation) {
  if (RelocInfo::IsInternalReference(rmode)) {
    intptr_t internal_ref = ReadUnalignedValue<intptr_t>(pc);
    if (internal_ref == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    internal_ref += pc_delta;
    if (jit_allocation) {
      jit_allocation->WriteUnalignedValue(pc, internal_ref);
    } else {
      WriteUnalignedValue<intptr_t>(pc, internal_ref);
    }

    return 2;  // Number of instructions patched.
  }
  Instr instr = instr_at(pc);
  DCHECK(RelocInfo::IsInternalReferenceEncoded(rmode));
  if (IsLui(instr)) {
    uintptr_t target_address = target_constant_address_at(pc) + pc_delta;
    DEBUG_PRINTF("\ttarget_address 0x%" PRIxPTR "\n", target_address);
    set_target_value_at(pc, target_address, jit_allocation);
#if V8_TARGET_ARCH_RISCV64
#ifdef RISCV_USE_SV39
    return 6;  // Number of instructions patched.
#else
    return 8;  // Number of instructions patched.
#endif
#elif V8_TARGET_ARCH_RISCV32
    return 2;  // Number of instructions patched.
#endif
  } else {
    UNIMPLEMENTED();
  }
}

void Assembler::RelocateRelativeReference(
    RelocInfo::Mode rmode, Address pc, intptr_t pc_delta,
    WritableJitAllocation* jit_allocation) {
  Instr instr = instr_at(pc);
  Instr instr1 = instr_at(pc + 1 * kInstrSize);
  DCHECK(RelocInfo::IsRelativeCodeTarget(rmode) ||
         RelocInfo::IsNearBuiltinEntry(rmode));
  if (IsAuipc(instr) && IsJalr(instr1)) {
    int32_t imm;
    imm = BranchLongOffset(instr, instr1);
    imm -= pc_delta;
    PatchBranchLongOffset(pc, instr, instr1, imm, jit_allocation);
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
  uint8_t* new_start = new_buffer->start();

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

  // None of our relocation types are pc relative pointing outside the code
  // buffer nor pc absolute pointing inside the code buffer, so there is no need
  // to relocate any emitted relocation entries.

  // Relocate internal references.
  // Relocate runtime entries.
  base::Vector<uint8_t> instructions{buffer_start_,
                                     static_cast<size_t>(pc_offset())};
  base::Vector<const uint8_t> reloc_info{reloc_info_writer.pos(), reloc_size};
  for (RelocIterator it(instructions, reloc_info, 0); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::INTERNAL_REFERENCE) {
      RelocateInternalReference(rmode, it.rinfo()->pc(), pc_delta);
    }
  }
}

void Assembler::db(uint8_t data) {
  DEBUG_PRINTF("%p(%d): constant 0x%x\n", pc_, pc_offset(), data);
  EmitHelper(data, false);
}

void Assembler::dd(uint32_t data) {
  DEBUG_PRINTF("%p(%d): constant 0x%x\n", pc_, pc_offset(), data);
  EmitHelper(data, false);
}

void Assembler::dq(uint64_t data) {
  DEBUG_PRINTF("%p(%d): constant 0x%" PRIx64 "\n", pc_, pc_offset(), data);
  EmitHelper(data, false);
}

#if defined(V8_TARGET_ARCH_RISCV64)
void Assembler::dq(Label* label) {
#elif defined(V8_TARGET_ARCH_RISCV32)
void Assembler::dd(Label* label) {
#endif
  int32_t offset = branch_long_offset(label);
  uintptr_t data = (offset != kEndOfJumpChain)
                       ? reinterpret_cast<uintptr_t>(pc_ + offset)
                       : kEndOfJumpChain;
  if (label->is_linked()) {
    // We only need to query the kind of use for unbound labels, so
    // we only insert those in the set.
    internal_reference_positions_.insert(label->pos());
  }
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  EmitHelper(data, false);
}

void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  if (!ShouldRecordRelocInfo(rmode)) return;
  // We do not try to reuse pool constants.
  RelocInfo rinfo(reinterpret_cast<Address>(pc_), rmode, data);
  DCHECK_GE(buffer_space(), kMaxRelocSize);  // Too late to grow buffer here.
  reloc_info_writer.Write(&rinfo);
}

void Assembler::CheckTrampolinePool() {
  // Once we've emitted the trampoline pool, we bump the next check position,
  // so we shouldn't get here again.
  CHECK(!is_trampoline_emitted());

  // Some small sequences of instructions must not be broken up by the
  // insertion of a trampoline pool; such sequences are protected by increasing
  // pools_blocked_nesting_. This is also used to block recursive calls to
  // CheckTrampolinePool.
  DEBUG_PRINTF("\tpools_blocked_nesting: %d\n", pools_blocked_nesting_);
  if (pools_blocked()) {
    // Emission is currently blocked; we will check again when we leave the
    // blocking scope. We shouldn't move the next check position here, because
    // it would interfere with the adjustments we make when we produce new
    // unbound labels and when we bind them.
    return;
  }

  DCHECK_GE(UnboundLabelsCount(), 0);
  if (UnboundLabelsCount() > 0) {
    // First we emit jump, then we emit trampoline pool.
    int size =
        kTrampolinePoolOverhead + UnboundLabelsCount() * kTrampolineSlotsSize;
    DEBUG_PRINTF("inserting trampoline pool at %p (%d) with size %d\n",
                 reinterpret_cast<Instr*>(buffer_start_ + pc_offset()),
                 pc_offset(), size);
    int pc_offset_for_safepoint_before = pc_offset_for_safepoint();
    USE(pc_offset_for_safepoint_before);  // Only used in DCHECK below.

    // As we are only going to emit the trampoline pool once, we do not have
    // to check ever again. To avoid recursive emissions occurring when the
    // pools are blocked below, we *eagerly* set the next check position to
    // something we will never reach.
    trampoline_check_ = kMaxInt;

    // By construction, we know that any branch or jump up until this point
    // can reach the last entry in the trampoline pool. Therefore, we can
    // safely jump around the pool as long as the last entry isn't too big
    // to allow skipping the pool with a single 'jump immediate' instruction.
    static_assert(kMaxBranchOffset <= kMaxJumpOffset - kTrampolineSlotsSize);
    int preamble_start = pc_offset();
    USE(preamble_start);  // Only used in DCHECK.
    BlockPoolsScope block_pools(this, size);
    j(size);

    int pool_start = pc_offset();
    DCHECK_EQ(pool_start - preamble_start, kTrampolinePoolOverhead);
    for (int i = 0; i < UnboundLabelsCount(); i++) {
      // Emit a dummy far branch. It will be patched later when one of the
      // unbound labels are bound.
      auipc(t6, 0);  // Read pc into t6.
      jr(t6, 0);     // Jump to t6 - the auipc instruction.
    }

    trampoline_ = Trampoline(pool_start, UnboundLabelsCount());
    int pool_size = pc_offset() - pool_start;
    USE(pool_size);  // Only used in DCHECK.
    DCHECK_EQ(pool_size, UnboundLabelsCount() * kTrampolineSlotsSize);

    // Make sure we didn't mess with the recorded pc for the next safepoint
    // as part of emitting the branch trampolines.
    DCHECK_EQ(pc_offset_for_safepoint(), pc_offset_for_safepoint_before);
  } else {
    // Number of branches to unbound label at this point is zero, so we can
    // move next trampoline check to maximum.
    trampoline_check_ = pc_offset() + kMaxBranchOffset - BlockPoolsScope::kGap;
  }
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      WritableJitAllocation* jit_allocation,
                                      ICacheFlushMode icache_flush_mode) {
  Instr* instr = reinterpret_cast<Instr*>(pc);
  if (IsAuipc(*instr)) {
#if V8_TARGET_ARCH_RISCV64
    if (IsLd(*reinterpret_cast<Instr*>(pc + 4))) {
#elif V8_TARGET_ARCH_RISCV32
    if (IsLw(*reinterpret_cast<Instr*>(pc + 4))) {
#endif
      int32_t Hi20 = AuipcOffset(*instr);
      int32_t Lo12 = LoadOffset(*reinterpret_cast<Instr*>(pc + 4));
      if (jit_allocation) {
        jit_allocation->WriteValue<Address>(
            reinterpret_cast<Address>(pc + Hi20 + Lo12), target);
      } else {
        Memory<Address>(reinterpret_cast<Address>(pc + Hi20 + Lo12)) = target;
      }
    } else {
      DCHECK(IsJalr(*reinterpret_cast<Instr*>(pc + 4)));
      intptr_t imm = static_cast<intptr_t>(target) - static_cast<intptr_t>(pc);
      Instr instr = instr_at(pc);
      Instr instr1 = instr_at(pc + 1 * kInstrSize);
      DCHECK(is_int32(imm + 0x800));
      int num = PatchBranchLongOffset(
          pc, instr, instr1, static_cast<int32_t>(imm), jit_allocation);
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        FlushInstructionCache(pc, num * kInstrSize);
      }
    }
  } else {
    set_target_value_at(pc, target, jit_allocation, icache_flush_mode);
  }
}

Address Assembler::target_address_at(Address pc, Address constant_pool) {
  Instr* instr = reinterpret_cast<Instr*>(pc);
  if (IsAuipc(*instr)) {
#if V8_TARGET_ARCH_RISCV64
    if (IsLd(*reinterpret_cast<Instr*>(pc + 4))) {
#elif V8_TARGET_ARCH_RISCV32
    if (IsLw(*reinterpret_cast<Instr*>(pc + 4))) {
#endif
      int32_t Hi20 = AuipcOffset(*instr);
      int32_t Lo12 = LoadOffset(*reinterpret_cast<Instr*>(pc + 4));
      return Memory<Address>(pc + Hi20 + Lo12);
    } else {
      DCHECK(IsJalr(*reinterpret_cast<Instr*>(pc + 4)));
      int32_t Hi20 = AuipcOffset(*instr);
      int32_t Lo12 = JalrOffset(*reinterpret_cast<Instr*>(pc + 4));
      return pc + Hi20 + Lo12;
    }

  } else {
    return target_constant_address_at(pc);
  }
}

#if V8_TARGET_ARCH_RISCV64
Address Assembler::target_constant_address_at(Address pc) {
#ifdef RISCV_USE_SV39
  Instruction* instr0 = Instruction::At((unsigned char*)pc);
  Instruction* instr1 = Instruction::At((unsigned char*)(pc + 1 * kInstrSize));
  Instruction* instr2 = Instruction::At((unsigned char*)(pc + 2 * kInstrSize));
  Instruction* instr3 = Instruction::At((unsigned char*)(pc + 3 * kInstrSize));

  // Interpret instructions for address generated by li: See listing in
  // Assembler::set_target_address_at() just below.
  if (IsLui(*reinterpret_cast<Instr*>(instr0)) &&
      IsAddi(*reinterpret_cast<Instr*>(instr1)) &&
      IsSlli(*reinterpret_cast<Instr*>(instr2)) &&
      IsOri(*reinterpret_cast<Instr*>(instr3))) {
    // Assemble the 64 bit value.
    int64_t addr = static_cast<int64_t>(instr0->Imm20UValue() << kImm20Shift) +
                   static_cast<int64_t>(instr1->Imm12Value());
    addr <<= 8;
    addr |= static_cast<int64_t>(instr3->Imm12Value());
#else
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
    int64_t addr = static_cast<int64_t>(instr0->Imm20UValue() << kImm20Shift) +
                   static_cast<int64_t>(instr1->Imm12Value());
    addr <<= 11;
    addr |= static_cast<int64_t>(instr3->Imm12Value());
    addr <<= 6;
    addr |= static_cast<int64_t>(instr5->Imm12Value());
#endif
    DEBUG_PRINTF("\taddr: %" PRIx64 "\n", addr);
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
// If define RISCV_USE_SV39, a 39-bit target address is stored in an
// 4-instruction sequence:
//  lui(reg, (int32_t)high_20); // 20 high bits
//  addi(reg, reg, low_12); // 12 following bits. total is 32 high bits in reg.
//  slli(reg, reg, 8); // Space for next 7 bits
//  ori(reg, reg, a7); // 7 bits are put in.
//
// Patching the address must replace all instructions, and flush the i-cache.
// Note that this assumes the use of SV48, the 48-bit virtual memory system.
void Assembler::set_target_value_at(Address pc, uint64_t target,
                                    WritableJitAllocation* jit_allocation,
                                    ICacheFlushMode icache_flush_mode) {
  DEBUG_PRINTF("\tset_target_value_at: pc: %" PRIxPTR "\ttarget: %" PRIx64
               "\told: %" PRIxPTR "\n",
               pc, target, target_address_at(pc, static_cast<Address>(0)));
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
#ifdef RISCV_USE_SV39
  DCHECK_EQ((target & 0xffffff8000000000ll), 0);
#ifdef DEBUG
  // Check we have the result from a li macro-instruction.
  Instruction* instr0 = Instruction::At((unsigned char*)pc);
  Instruction* instr1 = Instruction::At((unsigned char*)(pc + 1 * kInstrSize));
  Instruction* instr3 = Instruction::At((unsigned char*)(pc + 3 * kInstrSize));
  DCHECK(IsLui(*reinterpret_cast<Instr*>(instr0)) &&
         IsAddi(*reinterpret_cast<Instr*>(instr1)) &&
         IsOri(*reinterpret_cast<Instr*>(instr3)));
#endif
  int64_t a8 = target & 0xff;                    // bits 0:7. 8 bits
  int64_t high_31 = (target >> 8) & 0x7fffffff;  // 31 bits
  int64_t high_20 = ((high_31 + 0x800) >> 12);   // 19 bits
  int64_t low_12 = high_31 & 0xfff;              // 12 bits
  instr_at_put(pc, (*p & 0xfff) | (static_cast<int32_t>(high_20) << 12),
               jit_allocation);
  instr_at_put(pc + 1 * kInstrSize,
               (*(p + 1) & 0xfffff) | (static_cast<int32_t>(low_12) << 20),
               jit_allocation);
  instr_at_put(pc + 2 * kInstrSize, (*(p + 2) & 0xfffff) | (8 << 20),
               jit_allocation);
  instr_at_put(pc + 3 * kInstrSize,
               (*(p + 3) & 0xfffff) | (static_cast<int32_t>(a8) << 20),
               jit_allocation);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc, 6 * kInstrSize);
  }
#else
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
  instr_at_put(pc, (*p & 0xfff) | (static_cast<int32_t>(high_20) << 12),
               jit_allocation);
  instr_at_put(pc + 1 * kInstrSize,
               (*(p + 1) & 0xfffff) | (static_cast<int32_t>(low_12) << 20),
               jit_allocation);
  instr_at_put(pc + 2 * kInstrSize, (*(p + 2) & 0xfffff) | (11 << 20),
               jit_allocation);
  instr_at_put(pc + 3 * kInstrSize,
               (*(p + 3) & 0xfffff) | (static_cast<int32_t>(b11) << 20),
               jit_allocation);
  instr_at_put(pc + 4 * kInstrSize, (*(p + 4) & 0xfffff) | (6 << 20),
               jit_allocation);
  instr_at_put(pc + 5 * kInstrSize,
               (*(p + 5) & 0xfffff) | (static_cast<int32_t>(a6) << 20),
               jit_allocation);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc, 8 * kInstrSize);
  }
#endif
  DCHECK_EQ(target_constant_address_at(pc), target);
}

#elif V8_TARGET_ARCH_RISCV32
Address Assembler::target_constant_address_at(Address pc) {
  DEBUG_PRINTF("\ttarget_constant_address_at: pc: %x\t", pc);
  int32_t addr = target_constant32_at(pc);
  DEBUG_PRINTF("\taddr: %x\n", addr);
  return static_cast<Address>(addr);
}
// On RISC-V, a 32-bit target address is stored in an 2-instruction sequence:
//  lui(reg, high_20); // 20 high bits
//  addi(reg, reg, low_12); // 12 following bits. total is 31 high bits in reg.
//
// Patching the address must replace all instructions, and flush the i-cache.
void Assembler::set_target_value_at(Address pc, uint32_t target,
                                    WritableJitAllocation* jit_allocation,
                                    ICacheFlushMode icache_flush_mode) {
  DEBUG_PRINTF("\tset_target_value_at: pc: %x\ttarget: %x\n", pc, target);
  set_target_constant32_at(pc, target, jit_allocation, icache_flush_mode);
}
#endif

bool Assembler::IsConstantPoolAt(Instruction* instr) {
  // The constant pool marker is made of two instructions. These instructions
  // will never be emitted by the JIT, so checking for the first one is enough:
  // 0: auipc x0, #offset
  Instr instr_value = *reinterpret_cast<Instr*>(instr);
  bool result = IsAuipc(instr_value) && (instr->RdValue() == kRegCode_zero_reg);
#ifdef DEBUG
  // It is still worth asserting the marker is complete.
  // 1: j 0x0
  Instruction* instr_following = instr + kInstrSize;
  DCHECK(!result || (IsJal(*reinterpret_cast<Instr*>(instr_following)) &&
                     instr_following->Imm20JValue() == 0 &&
                     instr_following->RdValue() == kRegCode_zero_reg));
#endif
  return result;
}

int Assembler::ConstantPoolSizeAt(Instruction* instr) {
  if (IsConstantPoolAt(instr)) {
    return instr->Imm20UValue();
  } else {
    return -1;
  }
}

void Assembler::RecordConstPool(int size, const BlockPoolsScope& scope) {
  // We only need this for debugger support, to correctly compute offsets in the
  // code.
  RecordRelocInfo(RelocInfo::CONST_POOL, static_cast<intptr_t>(size));
}

void Assembler::EmitPoolGuard() {
  // We must generate only one instruction as this is used in scopes that
  // control the size of the code generated.
  j(0);
}

// -----------------------------------------------------------------------------
// Assembler.
template <typename T>
void Assembler::EmitHelper(T x, bool disassemble) {
  uint8_t* pc = pc_;
  *reinterpret_cast<T*>(pc) = x;
  if (disassemble) {
    DEBUG_PRINTF("%p(%d): ", pc, static_cast<int>(pc - buffer_start()));
    DisassembleInstruction(pc);
  }
  pc_ = pc + sizeof(x);
  CheckBuffer();
  CheckTrampolinePoolQuick(0);
}

void Assembler::emit(Instr x) { EmitHelper(x, true); }

void Assembler::emit(ShortInstr x) { EmitHelper(x, true); }

void Assembler::instr_at_put(int pos, Instr instr,
                             WritableJitAllocation* jit_allocation) {
  if (jit_allocation) {
    jit_allocation->WriteUnalignedValue(
        reinterpret_cast<Address>(buffer_start_ + pos), instr);
  } else {
    *reinterpret_cast<Instr*>(buffer_start_ + pos) = instr;
  }
}

void Assembler::instr_at_put(int pos, ShortInstr instr,
                             WritableJitAllocation* jit_allocation) {
  if (jit_allocation) {
    jit_allocation->WriteUnalignedValue(
        reinterpret_cast<Address>(buffer_start_ + pos), instr);
  } else {
    *reinterpret_cast<ShortInstr*>(buffer_start_ + pos) = instr;
  }
}

void Assembler::instr_at_put(Address pc, Instr instr,
                             WritableJitAllocation* jit_allocation) {
  if (jit_allocation) {
    jit_allocation->WriteUnalignedValue(pc, instr);
  } else {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
}

#if defined(V8_TARGET_ARCH_RISCV64)
// LLVM Code
//===- RISCVMatInt.cpp - Immediate materialisation -------------*- C++
//-*--===//
//
//  Part of the LLVM Project, under the Apache License v2.0 with LLVM
//  Exceptions. See https://llvm.org/LICENSE.txt for license information.
//  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
void Assembler::RecursiveLi(Register rd, int64_t val) {
  if (val > 0 && RecursiveLiImplCount(val) > 2) {
    unsigned LeadingZeros =
        base::bits::CountLeadingZeros(static_cast<uint64_t>(val));
    uint64_t ShiftedVal = static_cast<uint64_t>(val) << LeadingZeros;
    int countFillZero = RecursiveLiImplCount(ShiftedVal) + 1;
    if (countFillZero < RecursiveLiImplCount(val)) {
      RecursiveLiImpl(rd, ShiftedVal);
      srli(rd, rd, LeadingZeros);
      return;
    }
  }
  RecursiveLiImpl(rd, val);
}

int Assembler::RecursiveLiCount(int64_t val) {
  if (val > 0 && RecursiveLiImplCount(val) > 2) {
    unsigned LeadingZeros =
        base::bits::CountLeadingZeros(static_cast<uint64_t>(val));
    uint64_t ShiftedVal = static_cast<uint64_t>(val) << LeadingZeros;
    // Fill in the bits that will be shifted out with 1s. An example where
    // this helps is trailing one masks with 32 or more ones. This will
    // generate ADDI -1 and an SRLI.
    int countFillZero = RecursiveLiImplCount(ShiftedVal) + 1;
    if (countFillZero < RecursiveLiImplCount(val)) {
      return countFillZero;
    }
  }
  return RecursiveLiImplCount(val);
}

void Assembler::RecursiveLiImpl(Register rd, int64_t Val) {
  if (is_int32(Val)) {
    // Depending on the active bits in the immediate Value v, the following
    // instruction sequences are emitted:
    //
    // v == 0                        : ADDI
    // v[0,12) != 0 && v[12,32) == 0 : ADDI
    // v[0,12) == 0 && v[12,32) != 0 : LUI
    // v[0,32) != 0                  : LUI+ADDI(W)
    int64_t Hi20 = ((Val + 0x800) >> 12) & 0xFFFFF;
    int64_t Lo12 = Val << 52 >> 52;

    if (Hi20) {
      lui(rd, static_cast<int32_t>(Hi20));
    }

    if (Lo12 || Hi20 == 0) {
      if (Hi20) {
        addiw(rd, rd, Lo12);
      } else {
        addi(rd, zero_reg, Lo12);
      }
    }
    return;
  }

  // In the worst case, for a full 64-bit constant, a sequence of 8
  // instructions (i.e., LUI+ADDIW+SLLI+ADDI+SLLI+ADDI+SLLI+ADDI) has to be
  // emitted. Note that the first two instructions (LUI+ADDIW) can contribute
  // up to 32 bits while the following ADDI instructions contribute up to 12
  // bits each.
  //
  // On the first glance, implementing this seems to be possible by simply
  // emitting the most significant 32 bits (LUI+ADDIW) followed by as many
  // left shift (SLLI) and immediate additions (ADDI) as needed. However, due
  // to the fact that ADDI performs a sign extended addition, doing it like
  // that would only be possible when at most 11 bits of the ADDI instructions
  // are used. Using all 12 bits of the ADDI instructions, like done by GAS,
  // actually requires that the constant is processed starting with the least
  // significant bit.
  //
  // In the following, constants are processed from LSB to MSB but instruction
  // emission is performed from MSB to LSB by recursively calling
  // generateInstSeq. In each recursion, first the lowest 12 bits are removed
  // from the constant and the optimal shift amount, which can be greater than
  // 12 bits if the constant is sparse, is determined. Then, the shifted
  // remaining constant is processed recursively and gets emitted as soon as
  // it fits into 32 bits. The emission of the shifts and additions is
  // subsequently performed when the recursion returns.

  int64_t Lo12 = Val << 52 >> 52;
  int64_t Hi52 = (static_cast<uint64_t>(Val) + 0x800ull) >> 12;
  int ShiftAmount =
      12 + base::bits::CountTrailingZeros(static_cast<uint64_t>(Hi52));
  Hi52 = SignExtend(Hi52 >> (ShiftAmount - 12), 64 - ShiftAmount);

  // If the remaining bits don't fit in 12 bits, we might be able to reduce
  // the shift amount in order to use LUI which will zero the lower 12 bits.
  bool Unsigned = false;
  if (ShiftAmount > 12 && !is_int12(Hi52)) {
    if (is_int32(static_cast<uint64_t>(Hi52) << 12)) {
      // Reduce the shift amount and add zeros to the LSBs so it will match
      // LUI.
      ShiftAmount -= 12;
      Hi52 = static_cast<uint64_t>(Hi52) << 12;
    }
  }
  RecursiveLi(rd, Hi52);

  if (Unsigned) {
  } else {
    slli(rd, rd, ShiftAmount);
  }
  if (Lo12) {
    addi(rd, rd, Lo12);
  }
}

int Assembler::RecursiveLiImplCount(int64_t Val) {
  int count = 0;
  if (is_int32(Val)) {
    // Depending on the active bits in the immediate Value v, the following
    // instruction sequences are emitted:
    //
    // v == 0                        : ADDI
    // v[0,12) != 0 && v[12,32) == 0 : ADDI
    // v[0,12) == 0 && v[12,32) != 0 : LUI
    // v[0,32) != 0                  : LUI+ADDI(W)
    int64_t Hi20 = ((Val + 0x800) >> 12) & 0xFFFFF;
    int64_t Lo12 = Val << 52 >> 52;

    if (Hi20) {
      // lui(rd, static_cast<int32_t>(Hi20));
      count++;
    }

    if (Lo12 || Hi20 == 0) {
      //   unsigned AddiOpc = (IsRV64 && Hi20) ? RISCV::ADDIW : RISCV::ADDI;
      //   Res.push_back(RISCVMatInt::Inst(AddiOpc, Lo12));
      count++;
    }
    return count;
  }

  // In the worst case, for a full 64-bit constant, a sequence of 8
  // instructions (i.e., LUI+ADDIW+SLLI+ADDI+SLLI+ADDI+SLLI+ADDI) has to be
  // emitted. Note that the first two instructions (LUI+ADDIW) can contribute
  // up to 32 bits while the following ADDI instructions contribute up to 12
  // bits each.
  //
  // On the first glance, implementing this seems to be possible by simply
  // emitting the most significant 32 bits (LUI+ADDIW) followed by as many
  // left shift (SLLI) and immediate additions (ADDI) as needed. However, due
  // to the fact that ADDI performs a sign extended addition, doing it like
  // that would only be possible when at most 11 bits of the ADDI instructions
  // are used. Using all 12 bits of the ADDI instructions, like done by GAS,
  // actually requires that the constant is processed starting with the least
  // significant bit.
  //
  // In the following, constants are processed from LSB to MSB but instruction
  // emission is performed from MSB to LSB by recursively calling
  // generateInstSeq. In each recursion, first the lowest 12 bits are removed
  // from the constant and the optimal shift amount, which can be greater than
  // 12 bits if the constant is sparse, is determined. Then, the shifted
  // remaining constant is processed recursively and gets emitted as soon as
  // it fits into 32 bits. The emission of the shifts and additions is
  // subsequently performed when the recursion returns.

  int64_t Lo12 = Val << 52 >> 52;
  int64_t Hi52 = (static_cast<uint64_t>(Val) + 0x800ull) >> 12;
  int ShiftAmount =
      12 + base::bits::CountTrailingZeros(static_cast<uint64_t>(Hi52));
  Hi52 = SignExtend(Hi52 >> (ShiftAmount - 12), 64 - ShiftAmount);

  // If the remaining bits don't fit in 12 bits, we might be able to reduce
  // the shift amount in order to use LUI which will zero the lower 12 bits.
  bool Unsigned = false;
  if (ShiftAmount > 12 && !is_int12(Hi52)) {
    if (is_int32(static_cast<uint64_t>(Hi52) << 12)) {
      // Reduce the shift amount and add zeros to the LSBs so it will match
      // LUI.
      ShiftAmount -= 12;
      Hi52 = static_cast<uint64_t>(Hi52) << 12;
    }
  }

  count += RecursiveLiImplCount(Hi52);

  if (Unsigned) {
  } else {
    // slli(rd, rd, ShiftAmount);
    count++;
  }
  if (Lo12) {
    // addi(rd, rd, Lo12);
    count++;
  }
  return count;
}

int Assembler::GeneralLiCount(int64_t imm, bool is_get_temp_reg) {
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
    // upper part already in rd. Each part to be added to rd, has maximum of
    // 11 bits, and always starts with a 1. rd is shifted by the size of the
    // part plus the number of zeros between the parts. Each part is added
    // after the left shift.
    uint32_t mask = 0x80000000;
    int32_t i;
    for (i = 0; i < 32; i++) {
      if ((low_32 & mask) == 0) {
        mask >>= 1;
        if (i == 31) {
          // rest is zero
          count++;
        }
        continue;
      }
      // The first 1 seen
      if ((i + 11) < 32) {
        // Pick 11 bits
        count++;
        count++;
        i += 10;
        mask >>= 11;
      } else {
        count++;
        count++;
        break;
      }
    }
  }
  return count;
}
#endif

RegList Assembler::DefaultTmpList() { return {t3, t5}; }
DoubleRegList Assembler::DefaultFPTmpList() { return {kScratchDoubleReg}; }

}  // namespace internal
}  // namespace v8

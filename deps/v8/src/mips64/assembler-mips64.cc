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
// Copyright 2012 the V8 project authors. All rights reserved.

#include "src/mips64/assembler-mips64.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/base/cpu.h"
#include "src/code-stubs.h"
#include "src/mips64/assembler-mips64-inl.h"

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

  // If the compiler is allowed to use FPU then we can use FPU too in our code
  // generation even when generating snapshots.  This won't work for cross
  // compilation.
#if defined(__mips__) && defined(__mips_hard_float) && __mips_hard_float != 0
  answer |= 1u << FPU;
#endif

  return answer;
}


void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

  // If the compiler is allowed to use fpu then we can use fpu too in our
  // code generation.
#ifndef __mips__
  // For the simulator build, use FPU.
  supported_ |= 1u << FPU;
#else
  // Probe for additional features at runtime.
  base::CPU cpu;
  if (cpu.has_fpu()) supported_ |= 1u << FPU;
  if (cpu.has_msa()) supported_ |= 1u << MIPS_SIMD;
#endif
}


void CpuFeatures::PrintTarget() { }
void CpuFeatures::PrintFeatures() { }


int ToNumber(Register reg) {
  DCHECK(reg.is_valid());
  const int kNumbers[] = {
    0,    // zero_reg
    1,    // at
    2,    // v0
    3,    // v1
    4,    // a0
    5,    // a1
    6,    // a2
    7,    // a3
    8,    // a4
    9,    // a5
    10,   // a6
    11,   // a7
    12,   // t0
    13,   // t1
    14,   // t2
    15,   // t3
    16,   // s0
    17,   // s1
    18,   // s2
    19,   // s3
    20,   // s4
    21,   // s5
    22,   // s6
    23,   // s7
    24,   // t8
    25,   // t9
    26,   // k0
    27,   // k1
    28,   // gp
    29,   // sp
    30,   // fp
    31,   // ra
  };
  return kNumbers[reg.code()];
}


Register ToRegister(int num) {
  DCHECK(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] = {
    zero_reg,
    at,
    v0, v1,
    a0, a1, a2, a3, a4, a5, a6, a7,
    t0, t1, t2, t3,
    s0, s1, s2, s3, s4, s5, s6, s7,
    t8, t9,
    k0, k1,
    gp,
    sp,
    fp,
    ra
  };
  return kRegisters[num];
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo.

const int RelocInfo::kApplyMask = RelocInfo::kCodeTargetMask |
                                  1 << RelocInfo::INTERNAL_REFERENCE |
                                  1 << RelocInfo::INTERNAL_REFERENCE_ENCODED;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on MIPS means that it is a lui/ori instruction, and that is
  // always the case inside code objects.
  return true;
}


bool RelocInfo::IsInConstantPool() {
  return false;
}

Address RelocInfo::embedded_address() const {
  return Assembler::target_address_at(pc_, constant_pool_);
}

uint32_t RelocInfo::embedded_size() const {
  return static_cast<uint32_t>(reinterpret_cast<intptr_t>(
      (Assembler::target_address_at(pc_, constant_pool_))));
}

void RelocInfo::set_embedded_address(Isolate* isolate, Address address,
                                     ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate, pc_, constant_pool_, address,
                                   flush_mode);
}

void RelocInfo::set_embedded_size(Isolate* isolate, uint32_t size,
                                  ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate, pc_, constant_pool_,
                                   reinterpret_cast<Address>(size), flush_mode);
}

void RelocInfo::set_js_to_wasm_address(Isolate* isolate, Address address,
                                       ICacheFlushMode icache_flush_mode) {
  DCHECK_EQ(rmode_, JS_TO_WASM_CALL);
  set_embedded_address(isolate, address, icache_flush_mode);
}

Address RelocInfo::js_to_wasm_address() const {
  DCHECK_EQ(rmode_, JS_TO_WASM_CALL);
  return embedded_address();
}

// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand.
// See assembler-mips-inl.h for inlined constructors.

Operand::Operand(Handle<HeapObject> handle)
    : rm_(no_reg), rmode_(RelocInfo::EMBEDDED_OBJECT) {
  value_.immediate = reinterpret_cast<intptr_t>(handle.address());
}

Operand Operand::EmbeddedNumber(double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) return Operand(Smi::FromInt(smi));
  Operand result(0, RelocInfo::EMBEDDED_OBJECT);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(value);
  return result;
}

Operand Operand::EmbeddedCode(CodeStub* stub) {
  Operand result(0, RelocInfo::CODE_TARGET);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(stub);
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
  for (auto& request : heap_object_requests_) {
    Handle<HeapObject> object;
    switch (request.kind()) {
      case HeapObjectRequest::kHeapNumber:
        object = isolate->factory()->NewHeapNumber(request.heap_number(),
                                                   IMMUTABLE, TENURED);
        break;
      case HeapObjectRequest::kCodeStub:
        request.code_stub()->set_isolate(isolate);
        object = request.code_stub()->GetCode();
        break;
    }
    Address pc = buffer_ + request.offset();
    set_target_value_at(isolate, pc,
                        reinterpret_cast<uint64_t>(object.location()));
  }
}

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

// daddiu(sp, sp, 8) aka Pop() operation or part of Pop(r)
// operations as post-increment of sp.
const Instr kPopInstruction = DADDIU | (sp.code() << kRsShift) |
                              (sp.code() << kRtShift) |
                              (kPointerSize & kImm16Mask);  // NOLINT
// daddiu(sp, sp, -8) part of Push(r) operation as pre-decrement of sp.
const Instr kPushInstruction = DADDIU | (sp.code() << kRsShift) |
                               (sp.code() << kRtShift) |
                               (-kPointerSize & kImm16Mask);  // NOLINT
// Sd(r, MemOperand(sp, 0))
const Instr kPushRegPattern =
    SD | (sp.code() << kRsShift) | (0 & kImm16Mask);  // NOLINT
//  Ld(r, MemOperand(sp, 0))
const Instr kPopRegPattern =
    LD | (sp.code() << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpOffsetPattern =
    LW | (fp.code() << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kSwRegFpOffsetPattern =
    SW | (fp.code() << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpNegOffsetPattern =
    LW | (fp.code() << kRsShift) | (kNegOffset & kImm16Mask);  // NOLINT

const Instr kSwRegFpNegOffsetPattern =
    SW | (fp.code() << kRsShift) | (kNegOffset & kImm16Mask);  // NOLINT
// A mask for the Rt register for push, pop, lw, sw instructions.
const Instr kRtMask = kRtFieldMask;
const Instr kLwSwInstrTypeMask = 0xFFE00000;
const Instr kLwSwInstrArgumentMask  = ~kLwSwInstrTypeMask;
const Instr kLwSwOffsetMask = kImm16Mask;

Assembler::Assembler(IsolateData isolate_data, void* buffer, int buffer_size)
    : AssemblerBase(isolate_data, buffer, buffer_size),
      scratch_register_list_(at.bit()) {
  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);

  last_trampoline_pool_end_ = 0;
  no_trampoline_pool_before_ = 0;
  trampoline_pool_blocked_nesting_ = 0;
  // We leave space (16 * kTrampolineSlotsSize)
  // for BlockTrampolinePoolScope buffer.
  next_buffer_check_ = FLAG_force_long_branches
      ? kMaxInt : kMaxBranchOffset - kTrampolineSlotsSize * 16;
  internal_trampoline_exception_ = false;
  last_bound_pos_ = 0;

  trampoline_emitted_ = FLAG_force_long_branches;
  unbound_labels_count_ = 0;
  block_buffer_growth_ = false;
}

void Assembler::GetCode(Isolate* isolate, CodeDesc* desc) {
  EmitForbiddenSlotInstruction();
  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.

  AllocateAndInstallRequestedHeapObjects(isolate);

  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size =
      static_cast<int>((buffer_ + buffer_size_) - reloc_info_writer.pos());
  desc->origin = this;
  desc->constant_pool_size = 0;
  desc->unwinding_info_size = 0;
  desc->unwinding_info = nullptr;
}


void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo(m));
  EmitForbiddenSlotInstruction();
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::CodeTargetAlign() {
  // No advantage to aligning branch/call targets to more than
  // single instruction, that I am aware of.
  Align(4);
}


Register Assembler::GetRtReg(Instr instr) {
  return Register::from_code((instr & kRtFieldMask) >> kRtShift);
}


Register Assembler::GetRsReg(Instr instr) {
  return Register::from_code((instr & kRsFieldMask) >> kRsShift);
}


Register Assembler::GetRdReg(Instr instr) {
  return Register::from_code((instr & kRdFieldMask) >> kRdShift);
}


uint32_t Assembler::GetRt(Instr instr) {
  return (instr & kRtFieldMask) >> kRtShift;
}


uint32_t Assembler::GetRtField(Instr instr) {
  return instr & kRtFieldMask;
}


uint32_t Assembler::GetRs(Instr instr) {
  return (instr & kRsFieldMask) >> kRsShift;
}


uint32_t Assembler::GetRsField(Instr instr) {
  return instr & kRsFieldMask;
}


uint32_t Assembler::GetRd(Instr instr) {
  return  (instr & kRdFieldMask) >> kRdShift;
}


uint32_t Assembler::GetRdField(Instr instr) {
  return  instr & kRdFieldMask;
}


uint32_t Assembler::GetSa(Instr instr) {
  return (instr & kSaFieldMask) >> kSaShift;
}


uint32_t Assembler::GetSaField(Instr instr) {
  return instr & kSaFieldMask;
}


uint32_t Assembler::GetOpcodeField(Instr instr) {
  return instr & kOpcodeMask;
}


uint32_t Assembler::GetFunction(Instr instr) {
  return (instr & kFunctionFieldMask) >> kFunctionShift;
}


uint32_t Assembler::GetFunctionField(Instr instr) {
  return instr & kFunctionFieldMask;
}


uint32_t Assembler::GetImmediate16(Instr instr) {
  return instr & kImm16Mask;
}


uint32_t Assembler::GetLabelConst(Instr instr) {
  return instr & ~kImm16Mask;
}


bool Assembler::IsPop(Instr instr) {
  return (instr & ~kRtMask) == kPopRegPattern;
}


bool Assembler::IsPush(Instr instr) {
  return (instr & ~kRtMask) == kPushRegPattern;
}


bool Assembler::IsSwRegFpOffset(Instr instr) {
  return ((instr & kLwSwInstrTypeMask) == kSwRegFpOffsetPattern);
}


bool Assembler::IsLwRegFpOffset(Instr instr) {
  return ((instr & kLwSwInstrTypeMask) == kLwRegFpOffsetPattern);
}


bool Assembler::IsSwRegFpNegOffset(Instr instr) {
  return ((instr & (kLwSwInstrTypeMask | kNegOffset)) ==
          kSwRegFpNegOffsetPattern);
}


bool Assembler::IsLwRegFpNegOffset(Instr instr) {
  return ((instr & (kLwSwInstrTypeMask | kNegOffset)) ==
          kLwRegFpNegOffsetPattern);
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

// The link chain is terminated by a value in the instruction of -1,
// which is an otherwise illegal value (branch -1 is inf loop).
// The instruction 16-bit offset field addresses 32-bit words, but in
// code is conv to an 18-bit value addressing bytes, hence the -4 value.

const int kEndOfChain = -4;
// Determines the end of the Jump chain (a subset of the label link chain).
const int kEndOfJumpChain = 0;

bool Assembler::IsMsaBranch(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs_field = GetRsField(instr);
  if (opcode == COP1) {
    switch (rs_field) {
      case BZ_V:
      case BZ_B:
      case BZ_H:
      case BZ_W:
      case BZ_D:
      case BNZ_V:
      case BNZ_B:
      case BNZ_H:
      case BNZ_W:
      case BNZ_D:
        return true;
      default:
        return false;
    }
  } else {
    return false;
  }
}

bool Assembler::IsBranch(Instr instr) {
  uint32_t opcode   = GetOpcodeField(instr);
  uint32_t rt_field = GetRtField(instr);
  uint32_t rs_field = GetRsField(instr);
  // Checks if the instruction is a branch.
  bool isBranch =
      opcode == BEQ || opcode == BNE || opcode == BLEZ || opcode == BGTZ ||
      opcode == BEQL || opcode == BNEL || opcode == BLEZL || opcode == BGTZL ||
      (opcode == REGIMM && (rt_field == BLTZ || rt_field == BGEZ ||
                            rt_field == BLTZAL || rt_field == BGEZAL)) ||
      (opcode == COP1 && rs_field == BC1) ||  // Coprocessor branch.
      (opcode == COP1 && rs_field == BC1EQZ) ||
      (opcode == COP1 && rs_field == BC1NEZ) || IsMsaBranch(instr);
  if (!isBranch && kArchVariant == kMips64r6) {
    // All the 3 variants of POP10 (BOVC, BEQC, BEQZALC) and
    // POP30 (BNVC, BNEC, BNEZALC) are branch ops.
    isBranch |= opcode == POP10 || opcode == POP30 || opcode == BC ||
                opcode == BALC ||
                (opcode == POP66 && rs_field != 0) ||  // BEQZC
                (opcode == POP76 && rs_field != 0);    // BNEZC
  }
  return isBranch;
}


bool Assembler::IsBc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a BC or BALC.
  return opcode == BC || opcode == BALC;
}


bool Assembler::IsBzc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is BEQZC or BNEZC.
  return (opcode == POP66 && GetRsField(instr) != 0) ||
         (opcode == POP76 && GetRsField(instr) != 0);
}


bool Assembler::IsEmittedConstant(Instr instr) {
  uint32_t label_constant = GetLabelConst(instr);
  return label_constant == 0;  // Emitted label const in reg-exp engine.
}


bool Assembler::IsBeq(Instr instr) {
  return GetOpcodeField(instr) == BEQ;
}


bool Assembler::IsBne(Instr instr) {
  return GetOpcodeField(instr) == BNE;
}


bool Assembler::IsBeqzc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  return opcode == POP66 && GetRsField(instr) != 0;
}


bool Assembler::IsBnezc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  return opcode == POP76 && GetRsField(instr) != 0;
}


bool Assembler::IsBeqc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs = GetRsField(instr);
  uint32_t rt = GetRtField(instr);
  return opcode == POP10 && rs != 0 && rs < rt;  // && rt != 0
}


bool Assembler::IsBnec(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs = GetRsField(instr);
  uint32_t rt = GetRtField(instr);
  return opcode == POP30 && rs != 0 && rs < rt;  // && rt != 0
}


bool Assembler::IsJump(Instr instr) {
  uint32_t opcode   = GetOpcodeField(instr);
  uint32_t rt_field = GetRtField(instr);
  uint32_t rd_field = GetRdField(instr);
  uint32_t function_field = GetFunctionField(instr);
  // Checks if the instruction is a jump.
  return opcode == J || opcode == JAL ||
      (opcode == SPECIAL && rt_field == 0 &&
      ((function_field == JALR) || (rd_field == 0 && (function_field == JR))));
}


bool Assembler::IsJ(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a jump.
  return opcode == J;
}


bool Assembler::IsJal(Instr instr) {
  return GetOpcodeField(instr) == JAL;
}


bool Assembler::IsJr(Instr instr) {
  return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JR;
}


bool Assembler::IsJalr(Instr instr) {
  return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JALR;
}


bool Assembler::IsLui(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a load upper immediate.
  return opcode == LUI;
}


bool Assembler::IsOri(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a load upper immediate.
  return opcode == ORI;
}


bool Assembler::IsNop(Instr instr, unsigned int type) {
  // See Assembler::nop(type).
  DCHECK_LT(type, 32);
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t function = GetFunctionField(instr);
  uint32_t rt = GetRt(instr);
  uint32_t rd = GetRd(instr);
  uint32_t sa = GetSa(instr);

  // Traditional mips nop == sll(zero_reg, zero_reg, 0)
  // When marking non-zero type, use sll(zero_reg, at, type)
  // to avoid use of mips ssnop and ehb special encodings
  // of the sll instruction.

  Register nop_rt_reg = (type == 0) ? zero_reg : at;
  bool ret = (opcode == SPECIAL && function == SLL &&
              rd == static_cast<uint32_t>(ToNumber(zero_reg)) &&
              rt == static_cast<uint32_t>(ToNumber(nop_rt_reg)) &&
              sa == type);

  return ret;
}


int32_t Assembler::GetBranchOffset(Instr instr) {
  DCHECK(IsBranch(instr));
  return (static_cast<int16_t>(instr & kImm16Mask)) << 2;
}


bool Assembler::IsLw(Instr instr) {
  return (static_cast<uint32_t>(instr & kOpcodeMask) == LW);
}


int16_t Assembler::GetLwOffset(Instr instr) {
  DCHECK(IsLw(instr));
  return ((instr & kImm16Mask));
}


Instr Assembler::SetLwOffset(Instr instr, int16_t offset) {
  DCHECK(IsLw(instr));

  // We actually create a new lw instruction based on the original one.
  Instr temp_instr = LW | (instr & kRsFieldMask) | (instr & kRtFieldMask)
      | (offset & kImm16Mask);

  return temp_instr;
}


bool Assembler::IsSw(Instr instr) {
  return (static_cast<uint32_t>(instr & kOpcodeMask) == SW);
}


Instr Assembler::SetSwOffset(Instr instr, int16_t offset) {
  DCHECK(IsSw(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAddImmediate(Instr instr) {
  return ((instr & kOpcodeMask) == ADDIU || (instr & kOpcodeMask) == DADDIU);
}


Instr Assembler::SetAddImmediateOffset(Instr instr, int16_t offset) {
  DCHECK(IsAddImmediate(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAndImmediate(Instr instr) {
  return GetOpcodeField(instr) == ANDI;
}


static Assembler::OffsetSize OffsetSizeInBits(Instr instr) {
  if (kArchVariant == kMips64r6) {
    if (Assembler::IsBc(instr)) {
      return Assembler::OffsetSize::kOffset26;
    } else if (Assembler::IsBzc(instr)) {
      return Assembler::OffsetSize::kOffset21;
    }
  }
  return Assembler::OffsetSize::kOffset16;
}


static inline int32_t AddBranchOffset(int pos, Instr instr) {
  int bits = OffsetSizeInBits(instr);
  const int32_t mask = (1 << bits) - 1;
  bits = 32 - bits;

  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmetic shifts for signed integers.
  int32_t imm = ((instr & mask) << bits) >> (bits - 2);

  if (imm == kEndOfChain) {
    // EndOfChain sentinel is returned directly, not relative to pc or pos.
    return kEndOfChain;
  } else {
    return pos + Assembler::kBranchPCOffset + imm;
  }
}


int Assembler::target_at(int pos, bool is_internal) {
  if (is_internal) {
    int64_t* p = reinterpret_cast<int64_t*>(buffer_ + pos);
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
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    // Emitted label constant, not part of a branch.
    if (instr == 0) {
       return kEndOfChain;
     } else {
       int32_t imm18 =((instr & static_cast<int32_t>(kImm16Mask)) << 16) >> 14;
       return (imm18 + pos);
     }
  }
  // Check we have a branch or jump instruction.
  DCHECK(IsBranch(instr) || IsJ(instr) || IsJal(instr) || IsLui(instr));
  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmetic shifts for signed integers.
  if (IsBranch(instr)) {
    return AddBranchOffset(pos, instr);
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pos + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));

    // TODO(plind) create named constants for shift values.
    int64_t imm = static_cast<int64_t>(instr_lui & kImm16Mask) << 48;
    imm |= static_cast<int64_t>(instr_ori & kImm16Mask) << 32;
    imm |= static_cast<int64_t>(instr_ori2 & kImm16Mask) << 16;
    // Sign extend address;
    imm >>= 16;

    if (imm == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint64_t instr_address = reinterpret_cast<int64_t>(buffer_ + pos);
      DCHECK(instr_address - imm < INT_MAX);
      int delta = static_cast<int>(instr_address - imm);
      DCHECK(pos > delta);
      return pos - delta;
    }
  } else {
    DCHECK(IsJ(instr) || IsJal(instr));
    int32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    if (imm28 == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      // Sign extend 28-bit offset.
      int32_t delta = static_cast<int32_t>((imm28 << 4) >> 4);
      return pos + delta;
    }
  }
}


static inline Instr SetBranchOffset(int32_t pos, int32_t target_pos,
                                    Instr instr) {
  int32_t bits = OffsetSizeInBits(instr);
  int32_t imm = target_pos - (pos + Assembler::kBranchPCOffset);
  DCHECK_EQ(imm & 3, 0);
  imm >>= 2;

  const int32_t mask = (1 << bits) - 1;
  instr &= ~mask;
  DCHECK(is_intn(imm, bits));

  return instr | (imm & mask);
}


void Assembler::target_at_put(int pos, int target_pos, bool is_internal) {
  if (is_internal) {
    uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
    *reinterpret_cast<uint64_t*>(buffer_ + pos) = imm;
    return;
  }
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    DCHECK(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    return;
  }

  if (IsBranch(instr)) {
    instr = SetBranchOffset(pos, target_pos, instr);
    instr_at_put(pos, instr);
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pos + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));

    uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
    DCHECK_EQ(imm & 3, 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;
    instr_ori2 &= ~kImm16Mask;

    instr_at_put(pos + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> 32) & kImm16Mask));
    instr_at_put(pos + 1 * Assembler::kInstrSize,
                 instr_ori | ((imm >> 16) & kImm16Mask));
    instr_at_put(pos + 3 * Assembler::kInstrSize,
                 instr_ori2 | (imm & kImm16Mask));
  } else if (IsJ(instr) || IsJal(instr)) {
    int32_t imm28 = target_pos - pos;
    DCHECK_EQ(imm28 & 3, 0);

    uint32_t imm26 = static_cast<uint32_t>(imm28 >> 2);
    DCHECK(is_uint26(imm26));
    // Place 26-bit signed offset with markings.
    // When code is committed it will be resolved to j/jal.
    int32_t mark = IsJ(instr) ? kJRawMark : kJalRawMark;
    instr_at_put(pos, mark | (imm26 & kImm26Mask));
  } else {
    int32_t imm28 = target_pos - pos;
    DCHECK_EQ(imm28 & 3, 0);

    uint32_t imm26 = static_cast<uint32_t>(imm28 >> 2);
    DCHECK(is_uint26(imm26));
    // Place raw 26-bit signed offset.
    // When code is committed it will be resolved to j/jal.
    instr &= ~kImm26Mask;
    instr_at_put(pos, instr | (imm26 & kImm26Mask));
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
    next(L, is_internal);  // Call next before overwriting link with target at
                           // fixup_pos.
    Instr instr = instr_at(fixup_pos);
    if (is_internal) {
      target_at_put(fixup_pos, pos, is_internal);
    } else {
      if (IsBranch(instr)) {
        int branch_offset = BranchOffset(instr);
        if (dist > branch_offset) {
          if (trampoline_pos == kInvalidSlotPos) {
            trampoline_pos = get_trampoline_entry(fixup_pos);
            CHECK_NE(trampoline_pos, kInvalidSlotPos);
          }
          CHECK((trampoline_pos - fixup_pos) <= branch_offset);
          target_at_put(fixup_pos, trampoline_pos, false);
          fixup_pos = trampoline_pos;
        }
        target_at_put(fixup_pos, pos, false);
      } else {
        DCHECK(IsJ(instr) || IsJal(instr) || IsLui(instr) ||
               IsEmittedConstant(instr));
        target_at_put(fixup_pos, pos, false);
      }
    }
  }
  L->bind_to(pos);

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_)
    last_bound_pos_ = pos;
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
    DCHECK_GE(link, 0);
    L->link_to(link);
  }
}


bool Assembler::is_near(Label* L) {
  DCHECK(L->is_bound());
  return pc_offset() - L->pos() < kMaxBranchOffset - 4 * kInstrSize;
}


bool Assembler::is_near(Label* L, OffsetSize bits) {
  if (L == nullptr || !L->is_bound()) return true;
  return ((pc_offset() - L->pos()) <
          (1 << (bits + 2 - 1)) - 1 - 5 * kInstrSize);
}


bool Assembler::is_near_branch(Label* L) {
  DCHECK(L->is_bound());
  return kArchVariant == kMips64r6 ? is_near_r6(L) : is_near_pre_r6(L);
}


int Assembler::BranchOffset(Instr instr) {
  // At pre-R6 and for other R6 branches the offset is 16 bits.
  int bits = OffsetSize::kOffset16;

  if (kArchVariant == kMips64r6) {
    uint32_t opcode = GetOpcodeField(instr);
    switch (opcode) {
      // Checks BC or BALC.
      case BC:
      case BALC:
        bits = OffsetSize::kOffset26;
        break;

      // Checks BEQZC or BNEZC.
      case POP66:
      case POP76:
        if (GetRsField(instr) != 0) bits = OffsetSize::kOffset21;
        break;
      default:
        break;
    }
  }

  return (1 << (bits + 2 - 1)) - 1;
}


// We have to use a temporary register for things that can be relocated even
// if they can be encoded in the MIPS's 16 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Assembler::MustUseReg(RelocInfo::Mode rmode) {
  return !RelocInfo::IsNone(rmode);
}

void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 Register rd,
                                 uint16_t sa,
                                 SecondaryField func) {
  DCHECK(rd.is_valid() && rs.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 uint16_t msb,
                                 uint16_t lsb,
                                 SecondaryField func) {
  DCHECK(rs.is_valid() && rt.is_valid() && is_uint5(msb) && is_uint5(lsb));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (msb << kRdShift) | (lsb << kSaShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | fmt | (ft.code() << kFtShift) | (fs.code() << kFsShift)
      | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 FPURegister fr,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fr.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | (fr.code() << kFrShift) | (ft.code() << kFtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fs.is_valid() && rt.is_valid());
  Instr instr = opcode | fmt | (rt.code() << kRtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPUControlRegister fs,
                                 SecondaryField func) {
  DCHECK(fs.is_valid() && rt.is_valid());
  Instr instr =
      opcode | fmt | (rt.code() << kRtShift) | (fs.code() << kFsShift) | func;
  emit(instr);
}


// Instructions with immediate value.
// Registers are in the order of the instruction encoding, from left to right.
void Assembler::GenInstrImmediate(Opcode opcode, Register rs, Register rt,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && rt.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}

void Assembler::GenInstrImmediate(Opcode opcode, Register base, Register rt,
                                  int32_t offset9, int bit6,
                                  SecondaryField func) {
  DCHECK(base.is_valid() && rt.is_valid() && is_int9(offset9) &&
         is_uint1(bit6));
  Instr instr = opcode | (base.code() << kBaseShift) | (rt.code() << kRtShift) |
                ((offset9 << kImm9Shift) & kImm9Mask) | bit6 << kBit6Shift |
                func;
  emit(instr);
}

void Assembler::GenInstrImmediate(Opcode opcode, Register rs, SecondaryField SF,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | SF | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, FPURegister ft,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && ft.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (ft.code() << kFtShift)
      | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, int32_t offset21,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && (is_int21(offset21)));
  Instr instr = opcode | (rs.code() << kRsShift) | (offset21 & kImm21Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs,
                                  uint32_t offset21) {
  DCHECK(rs.is_valid() && (is_uint21(offset21)));
  Instr instr = opcode | (rs.code() << kRsShift) | (offset21 & kImm21Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode, int32_t offset26,
                                  CompactBranchType is_compact_branch) {
  DCHECK(is_int26(offset26));
  Instr instr = opcode | (offset26 & kImm26Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrJump(Opcode opcode,
                             uint32_t address) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(is_uint26(address));
  Instr instr = opcode | address;
  emit(instr);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}

// MSA instructions
void Assembler::GenInstrMsaI8(SecondaryField operation, uint32_t imm8,
                              MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(ws.is_valid() && wd.is_valid() && is_uint8(imm8));
  Instr instr = MSA | operation | ((imm8 & kImm8Mask) << kWtShift) |
                (ws.code() << kWsShift) | (wd.code() << kWdShift);
  emit(instr);
}

void Assembler::GenInstrMsaI5(SecondaryField operation, SecondaryField df,
                              int32_t imm5, MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(ws.is_valid() && wd.is_valid());
  DCHECK((operation == MAXI_S) || (operation == MINI_S) ||
                 (operation == CEQI) || (operation == CLTI_S) ||
                 (operation == CLEI_S)
             ? is_int5(imm5)
             : is_uint5(imm5));
  Instr instr = MSA | operation | df | ((imm5 & kImm5Mask) << kWtShift) |
                (ws.code() << kWsShift) | (wd.code() << kWdShift);
  emit(instr);
}

void Assembler::GenInstrMsaBit(SecondaryField operation, SecondaryField df,
                               uint32_t m, MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(ws.is_valid() && wd.is_valid() && is_valid_msa_df_m(df, m));
  Instr instr = MSA | operation | df | (m << kWtShift) |
                (ws.code() << kWsShift) | (wd.code() << kWdShift);
  emit(instr);
}

void Assembler::GenInstrMsaI10(SecondaryField operation, SecondaryField df,
                               int32_t imm10, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(wd.is_valid() && is_int10(imm10));
  Instr instr = MSA | operation | df | ((imm10 & kImm10Mask) << kWsShift) |
                (wd.code() << kWdShift);
  emit(instr);
}

template <typename RegType>
void Assembler::GenInstrMsa3R(SecondaryField operation, SecondaryField df,
                              RegType t, MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(t.is_valid() && ws.is_valid() && wd.is_valid());
  Instr instr = MSA | operation | df | (t.code() << kWtShift) |
                (ws.code() << kWsShift) | (wd.code() << kWdShift);
  emit(instr);
}

template <typename DstType, typename SrcType>
void Assembler::GenInstrMsaElm(SecondaryField operation, SecondaryField df,
                               uint32_t n, SrcType src, DstType dst) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(src.is_valid() && dst.is_valid() && is_valid_msa_df_n(df, n));
  Instr instr = MSA | operation | df | (n << kWtShift) |
                (src.code() << kWsShift) | (dst.code() << kWdShift) |
                MSA_ELM_MINOR;
  emit(instr);
}

void Assembler::GenInstrMsa3RF(SecondaryField operation, uint32_t df,
                               MSARegister wt, MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(wt.is_valid() && ws.is_valid() && wd.is_valid());
  DCHECK_LT(df, 2);
  Instr instr = MSA | operation | (df << 21) | (wt.code() << kWtShift) |
                (ws.code() << kWsShift) | (wd.code() << kWdShift);
  emit(instr);
}

void Assembler::GenInstrMsaVec(SecondaryField operation, MSARegister wt,
                               MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(wt.is_valid() && ws.is_valid() && wd.is_valid());
  Instr instr = MSA | operation | (wt.code() << kWtShift) |
                (ws.code() << kWsShift) | (wd.code() << kWdShift) |
                MSA_VEC_2R_2RF_MINOR;
  emit(instr);
}

void Assembler::GenInstrMsaMI10(SecondaryField operation, int32_t s10,
                                Register rs, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(rs.is_valid() && wd.is_valid() && is_int10(s10));
  Instr instr = MSA | operation | ((s10 & kImm10Mask) << kWtShift) |
                (rs.code() << kWsShift) | (wd.code() << kWdShift);
  emit(instr);
}

void Assembler::GenInstrMsa2R(SecondaryField operation, SecondaryField df,
                              MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(ws.is_valid() && wd.is_valid());
  Instr instr = MSA | MSA_2R_FORMAT | operation | df | (ws.code() << kWsShift) |
                (wd.code() << kWdShift) | MSA_VEC_2R_2RF_MINOR;
  emit(instr);
}

void Assembler::GenInstrMsa2RF(SecondaryField operation, SecondaryField df,
                               MSARegister ws, MSARegister wd) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(ws.is_valid() && wd.is_valid());
  Instr instr = MSA | MSA_2RF_FORMAT | operation | df |
                (ws.code() << kWsShift) | (wd.code() << kWdShift) |
                MSA_VEC_2R_2RF_MINOR;
  emit(instr);
}

void Assembler::GenInstrMsaBranch(SecondaryField operation, MSARegister wt,
                                  int32_t offset16) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(wt.is_valid() && is_int16(offset16));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Instr instr =
      COP1 | operation | (wt.code() << kWtShift) | (offset16 & kImm16Mask);
  emit(instr);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}

// Returns the next free trampoline entry.
int32_t Assembler::get_trampoline_entry(int32_t pos) {
  int32_t trampoline_entry = kInvalidSlotPos;
  if (!internal_trampoline_exception_) {
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
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      return kEndOfJumpChain;
    }
  }
  uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
  DCHECK_EQ(imm & 3, 0);

  return imm;
}


uint64_t Assembler::jump_offset(Label* L) {
  int64_t target_pos;
  int32_t pad = IsPrevInstrCompactBranch() ? kInstrSize : 0;

  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset() + pad);
    } else {
      L->link_to(pc_offset() + pad);
      return kEndOfJumpChain;
    }
  }
  int64_t imm = target_pos - (pc_offset() + pad);
  DCHECK_EQ(imm & 3, 0);

  return static_cast<uint64_t>(imm);
}


int32_t Assembler::branch_offset_helper(Label* L, OffsetSize bits) {
  int32_t target_pos;
  int32_t pad = IsPrevInstrCompactBranch() ? kInstrSize : 0;

  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset() + pad);
    } else {
      L->link_to(pc_offset() + pad);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - (pc_offset() + kBranchPCOffset + pad);
  DCHECK(is_intn(offset, bits + 2));
  DCHECK_EQ(offset & 3, 0);

  return offset;
}


void Assembler::label_at_put(Label* L, int at_offset) {
  int target_pos;
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
      target_pos = kEndOfChain;
      instr_at_put(at_offset, 0);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
    }
    L->link_to(at_offset);
  }
}


//------- Branch and jump instructions --------

void Assembler::b(int16_t offset) {
  beq(zero_reg, zero_reg, offset);
}


void Assembler::bal(int16_t offset) {
  bgezal(zero_reg, offset);
}


void Assembler::bc(int32_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrImmediate(BC, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::balc(int32_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrImmediate(BALC, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beq(Register rs, Register rt, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BEQ, rs, rt, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgezc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  GenInstrImmediate(BLEZL, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgeuc(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  DCHECK(rt != zero_reg);
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZ, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgec(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  DCHECK(rt != zero_reg);
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZL, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezal(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6 || rs == zero_reg);
  DCHECK(rs != ra);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZAL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgtz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BGTZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgtzc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  GenInstrImmediate(BGTZL, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::blez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BLEZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::blezc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  GenInstrImmediate(BLEZL, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltzc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  GenInstrImmediate(BGTZL, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltuc(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  DCHECK(rt != zero_reg);
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZ, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltc(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  DCHECK(rt != zero_reg);
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZL, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzal(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6 || rs == zero_reg);
  DCHECK(rs != ra);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZAL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bne(Register rs, Register rt, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BNE, rs, rt, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bovc(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  if (rs.code() >= rt.code()) {
    GenInstrImmediate(ADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(ADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::bnvc(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  if (rs.code() >= rt.code()) {
    GenInstrImmediate(DADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(DADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::blezalc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  DCHECK(rt != ra);
  GenInstrImmediate(BLEZ, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezalc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  DCHECK(rt != ra);
  GenInstrImmediate(BLEZ, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezall(Register rs, int16_t offset) {
  DCHECK_NE(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  DCHECK(rs != ra);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZALL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzalc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  DCHECK(rt != ra);
  GenInstrImmediate(BGTZ, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgtzalc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  DCHECK(rt != ra);
  GenInstrImmediate(BGTZ, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beqzalc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  DCHECK(rt != ra);
  GenInstrImmediate(ADDI, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bnezalc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rt != zero_reg);
  DCHECK(rt != ra);
  GenInstrImmediate(DADDI, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beqc(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.code() != rt.code() && rs.code() != 0 && rt.code() != 0);
  if (rs.code() < rt.code()) {
    GenInstrImmediate(ADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(ADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::beqzc(Register rs, int32_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  GenInstrImmediate(POP66, rs, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bnec(Register rs, Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.code() != rt.code() && rs.code() != 0 && rt.code() != 0);
  if (rs.code() < rt.code()) {
    GenInstrImmediate(DADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(DADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::bnezc(Register rs, int32_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs != zero_reg);
  GenInstrImmediate(POP76, rs, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::j(int64_t target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrJump(J, static_cast<uint32_t>(target >> 2) & kImm26Mask);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::j(Label* target) {
  uint64_t imm = jump_offset(target);
  if (target->is_bound()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    GenInstrJump(static_cast<Opcode>(kJRawMark),
                 static_cast<uint32_t>(imm >> 2) & kImm26Mask);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    j(imm);
  }
}


void Assembler::jal(Label* target) {
  uint64_t imm = jump_offset(target);
  if (target->is_bound()) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    GenInstrJump(static_cast<Opcode>(kJalRawMark),
                 static_cast<uint32_t>(imm >> 2) & kImm26Mask);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    jal(imm);
  }
}


void Assembler::jr(Register rs) {
  if (kArchVariant != kMips64r6) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    GenInstrRegister(SPECIAL, rs, zero_reg, zero_reg, 0, JR);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    jalr(rs, zero_reg);
  }
}


void Assembler::jal(int64_t target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrJump(JAL, static_cast<uint32_t>(target >> 2) & kImm26Mask);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jalr(Register rs, Register rd) {
  DCHECK(rs.code() != rd.code());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrRegister(SPECIAL, rs, zero_reg, rd, 0, JALR);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jic(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrImmediate(POP66, zero_reg, rt, offset);
}


void Assembler::jialc(Register rt, int16_t offset) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrImmediate(POP76, zero_reg, rt, offset);
}


// -------Data-processing-instructions---------

// Arithmetic.

void Assembler::addu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, ADDU);
}


void Assembler::addiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(ADDIU, rs, rd, j);
}


void Assembler::subu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SUBU);
}


void Assembler::mul(Register rd, Register rs, Register rt) {
  if (kArchVariant == kMips64r6) {
      GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH);
  } else {
      GenInstrRegister(SPECIAL2, rs, rt, rd, 0, MUL);
  }
}


void Assembler::muh(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH);
}


void Assembler::mulu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH_U);
}


void Assembler::muhu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH_U);
}


void Assembler::dmul(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, D_MUL_MUH);
}


void Assembler::dmuh(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, D_MUL_MUH);
}


void Assembler::dmulu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, D_MUL_MUH_U);
}


void Assembler::dmuhu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, D_MUL_MUH_U);
}


void Assembler::mult(Register rs, Register rt) {
  DCHECK_NE(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULT);
}


void Assembler::multu(Register rs, Register rt) {
  DCHECK_NE(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULTU);
}


void Assembler::daddiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(DADDIU, rs, rd, j);
}


void Assembler::div(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIV);
}


void Assembler::div(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD);
}


void Assembler::mod(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD);
}


void Assembler::divu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIVU);
}


void Assembler::divu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD_U);
}


void Assembler::modu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD_U);
}


void Assembler::daddu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DADDU);
}


void Assembler::dsubu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSUBU);
}


void Assembler::dmult(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DMULT);
}


void Assembler::dmultu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DMULTU);
}


void Assembler::ddiv(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DDIV);
}


void Assembler::ddiv(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, D_DIV_MOD);
}


void Assembler::dmod(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, D_DIV_MOD);
}


void Assembler::ddivu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DDIVU);
}


void Assembler::ddivu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, D_DIV_MOD_U);
}


void Assembler::dmodu(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, D_DIV_MOD_U);
}


// Logical.

void Assembler::and_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, AND);
}


void Assembler::andi(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(ANDI, rs, rt, j);
}


void Assembler::or_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, OR);
}


void Assembler::ori(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(ORI, rs, rt, j);
}


void Assembler::xor_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, XOR);
}


void Assembler::xori(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(XORI, rs, rt, j);
}


void Assembler::nor(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, NOR);
}


// Shifts.
void Assembler::sll(Register rd,
                    Register rt,
                    uint16_t sa,
                    bool coming_from_nop) {
  // Don't allow nop instructions in the form sll zero_reg, zero_reg to be
  // generated using the sll instruction. They must be generated using
  // nop(int/NopMarkerTypes).
  DCHECK(coming_from_nop || (rd != zero_reg && rt != zero_reg));
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SLL);
}


void Assembler::sllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLLV);
}


void Assembler::srl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SRL);
}


void Assembler::srlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRLV);
}


void Assembler::sra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SRA);
}


void Assembler::srav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRAV);
}


void Assembler::rotr(Register rd, Register rt, uint16_t sa) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | SRL;
  emit(instr);
}


void Assembler::rotrv(Register rd, Register rt, Register rs) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
     | (rd.code() << kRdShift) | (1 << kSaShift) | SRLV;
  emit(instr);
}


void Assembler::dsll(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSLL);
}


void Assembler::dsllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSLLV);
}


void Assembler::dsrl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRL);
}


void Assembler::dsrlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSRLV);
}


void Assembler::drotr(Register rd, Register rt, uint16_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | DSRL;
  emit(instr);
}

void Assembler::drotr32(Register rd, Register rt, uint16_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift) |
                (rd.code() << kRdShift) | (sa << kSaShift) | DSRL32;
  emit(instr);
}

void Assembler::drotrv(Register rd, Register rt, Register rs) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid() );
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (1 << kSaShift) | DSRLV;
  emit(instr);
}


void Assembler::dsra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRA);
}


void Assembler::dsrav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSRAV);
}


void Assembler::dsll32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSLL32);
}


void Assembler::dsrl32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRL32);
}


void Assembler::dsra32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, DSRA32);
}


void Assembler::lsa(Register rd, Register rt, Register rs, uint8_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK_LE(sa, 3);
  DCHECK_EQ(kArchVariant, kMips64r6);
  Instr instr = SPECIAL | rs.code() << kRsShift | rt.code() << kRtShift |
                rd.code() << kRdShift | sa << kSaShift | LSA;
  emit(instr);
}


void Assembler::dlsa(Register rd, Register rt, Register rs, uint8_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK_LE(sa, 3);
  DCHECK_EQ(kArchVariant, kMips64r6);
  Instr instr = SPECIAL | rs.code() << kRsShift | rt.code() << kRtShift |
                rd.code() << kRdShift | sa << kSaShift | DLSA;
  emit(instr);
}


// ------------Memory-instructions-------------

void Assembler::AdjustBaseAndOffset(MemOperand& src,
                                    OffsetAccessType access_type,
                                    int second_access_add_to_offset) {
  // This method is used to adjust the base register and offset pair
  // for a load/store when the offset doesn't fit into int16_t.
  // It is assumed that 'base + offset' is sufficiently aligned for memory
  // operands that are machine word in size or smaller. For doubleword-sized
  // operands it's assumed that 'base' is a multiple of 8, while 'offset'
  // may be a multiple of 4 (e.g. 4-byte-aligned long and double arguments
  // and spilled variables on the stack accessed relative to the stack
  // pointer register).
  // We preserve the "alignment" of 'offset' by adjusting it by a multiple of 8.

  bool doubleword_aligned = (src.offset() & (kDoubleSize - 1)) == 0;
  bool two_accesses = static_cast<bool>(access_type) || !doubleword_aligned;
  DCHECK_LE(second_access_add_to_offset, 7);  // Must be <= 7.

  // is_int16 must be passed a signed value, hence the static cast below.
  if (is_int16(src.offset()) &&
      (!two_accesses || is_int16(static_cast<int32_t>(
                            src.offset() + second_access_add_to_offset)))) {
    // Nothing to do: 'offset' (and, if needed, 'offset + 4', or other specified
    // value) fits into int16_t.
    return;
  }

  DCHECK(src.rm() !=
         at);  // Must not overwrite the register 'base' while loading 'offset'.

#ifdef DEBUG
  // Remember the "(mis)alignment" of 'offset', it will be checked at the end.
  uint32_t misalignment = src.offset() & (kDoubleSize - 1);
#endif

  // Do not load the whole 32-bit 'offset' if it can be represented as
  // a sum of two 16-bit signed offsets. This can save an instruction or two.
  // To simplify matters, only do this for a symmetric range of offsets from
  // about -64KB to about +64KB, allowing further addition of 4 when accessing
  // 64-bit variables with two 32-bit accesses.
  constexpr int32_t kMinOffsetForSimpleAdjustment =
      0x7FF8;  // Max int16_t that's a multiple of 8.
  constexpr int32_t kMaxOffsetForSimpleAdjustment =
      2 * kMinOffsetForSimpleAdjustment;

  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (0 <= src.offset() && src.offset() <= kMaxOffsetForSimpleAdjustment) {
    daddiu(scratch, src.rm(), kMinOffsetForSimpleAdjustment);
    src.offset_ -= kMinOffsetForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= src.offset() &&
             src.offset() < 0) {
    daddiu(scratch, src.rm(), -kMinOffsetForSimpleAdjustment);
    src.offset_ += kMinOffsetForSimpleAdjustment;
  } else if (kArchVariant == kMips64r6) {
    // On r6 take advantage of the daui instruction, e.g.:
    //    daui   at, base, offset_high
    //   [dahi   at, 1]                       // When `offset` is close to +2GB.
    //    lw     reg_lo, offset_low(at)
    //   [lw     reg_hi, (offset_low+4)(at)]  // If misaligned 64-bit load.
    // or when offset_low+4 overflows int16_t:
    //    daui   at, base, offset_high
    //    daddiu at, at, 8
    //    lw     reg_lo, (offset_low-8)(at)
    //    lw     reg_hi, (offset_low-4)(at)
    int16_t offset_low = static_cast<uint16_t>(src.offset());
    int32_t offset_low32 = offset_low;
    int16_t offset_high = static_cast<uint16_t>(src.offset() >> 16);
    bool increment_hi16 = offset_low < 0;
    bool overflow_hi16 = false;

    if (increment_hi16) {
      offset_high++;
      overflow_hi16 = (offset_high == -32768);
    }
    daui(scratch, src.rm(), static_cast<uint16_t>(offset_high));

    if (overflow_hi16) {
      dahi(scratch, 1);
    }

    if (two_accesses && !is_int16(static_cast<int32_t>(
                            offset_low32 + second_access_add_to_offset))) {
      // Avoid overflow in the 16-bit offset of the load/store instruction when
      // adding 4.
      daddiu(scratch, scratch, kDoubleSize);
      offset_low32 -= kDoubleSize;
    }

    src.offset_ = offset_low32;
  } else {
    // Do not load the whole 32-bit 'offset' if it can be represented as
    // a sum of three 16-bit signed offsets. This can save an instruction.
    // To simplify matters, only do this for a symmetric range of offsets from
    // about -96KB to about +96KB, allowing further addition of 4 when accessing
    // 64-bit variables with two 32-bit accesses.
    constexpr int32_t kMinOffsetForMediumAdjustment =
        2 * kMinOffsetForSimpleAdjustment;
    constexpr int32_t kMaxOffsetForMediumAdjustment =
        3 * kMinOffsetForSimpleAdjustment;
    if (0 <= src.offset() && src.offset() <= kMaxOffsetForMediumAdjustment) {
      daddiu(scratch, src.rm(), kMinOffsetForMediumAdjustment / 2);
      daddiu(scratch, scratch, kMinOffsetForMediumAdjustment / 2);
      src.offset_ -= kMinOffsetForMediumAdjustment;
    } else if (-kMaxOffsetForMediumAdjustment <= src.offset() &&
               src.offset() < 0) {
      daddiu(scratch, src.rm(), -kMinOffsetForMediumAdjustment / 2);
      daddiu(scratch, scratch, -kMinOffsetForMediumAdjustment / 2);
      src.offset_ += kMinOffsetForMediumAdjustment;
    } else {
      // Now that all shorter options have been exhausted, load the full 32-bit
      // offset.
      int32_t loaded_offset = RoundDown(src.offset(), kDoubleSize);
      lui(scratch, (loaded_offset >> kLuiShift) & kImm16Mask);
      ori(scratch, scratch, loaded_offset & kImm16Mask);  // Load 32-bit offset.
      daddu(scratch, scratch, src.rm());
      src.offset_ -= loaded_offset;
    }
  }
  src.rm_ = scratch;

  DCHECK(is_int16(src.offset()));
  if (two_accesses) {
    DCHECK(is_int16(
        static_cast<int32_t>(src.offset() + second_access_add_to_offset)));
  }
  DCHECK(misalignment == (src.offset() & (kDoubleSize - 1)));
}

void Assembler::lb(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LB, rs.rm(), rd, rs.offset_);
}


void Assembler::lbu(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LBU, rs.rm(), rd, rs.offset_);
}


void Assembler::lh(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LH, rs.rm(), rd, rs.offset_);
}


void Assembler::lhu(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LHU, rs.rm(), rd, rs.offset_);
}


void Assembler::lw(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LW, rs.rm(), rd, rs.offset_);
}


void Assembler::lwu(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LWU, rs.rm(), rd, rs.offset_);
}


void Assembler::lwl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(LWL, rs.rm(), rd, rs.offset_);
}


void Assembler::lwr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(LWR, rs.rm(), rd, rs.offset_);
}


void Assembler::sb(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SB, rs.rm(), rd, rs.offset_);
}


void Assembler::sh(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SH, rs.rm(), rd, rs.offset_);
}


void Assembler::sw(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SW, rs.rm(), rd, rs.offset_);
}


void Assembler::swl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(SWL, rs.rm(), rd, rs.offset_);
}


void Assembler::swr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(SWR, rs.rm(), rd, rs.offset_);
}

void Assembler::ll(Register rd, const MemOperand& rs) {
  if (kArchVariant == kMips64r6) {
    DCHECK(is_int9(rs.offset_));
    GenInstrImmediate(SPECIAL3, rs.rm(), rd, rs.offset_, 0, LL_R6);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    DCHECK(is_int16(rs.offset_));
    GenInstrImmediate(LL, rs.rm(), rd, rs.offset_);
  }
}

void Assembler::lld(Register rd, const MemOperand& rs) {
  if (kArchVariant == kMips64r6) {
    DCHECK(is_int9(rs.offset_));
    GenInstrImmediate(SPECIAL3, rs.rm(), rd, rs.offset_, 0, LLD_R6);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    DCHECK(is_int16(rs.offset_));
    GenInstrImmediate(LLD, rs.rm(), rd, rs.offset_);
  }
}

void Assembler::sc(Register rd, const MemOperand& rs) {
  if (kArchVariant == kMips64r6) {
    DCHECK(is_int9(rs.offset_));
    GenInstrImmediate(SPECIAL3, rs.rm(), rd, rs.offset_, 0, SC_R6);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    GenInstrImmediate(SC, rs.rm(), rd, rs.offset_);
  }
}

void Assembler::scd(Register rd, const MemOperand& rs) {
  if (kArchVariant == kMips64r6) {
    DCHECK(is_int9(rs.offset_));
    GenInstrImmediate(SPECIAL3, rs.rm(), rd, rs.offset_, 0, SCD_R6);
  } else {
    DCHECK_EQ(kArchVariant, kMips64r2);
    GenInstrImmediate(SCD, rs.rm(), rd, rs.offset_);
  }
}

void Assembler::lui(Register rd, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, zero_reg, rd, j);
}


void Assembler::aui(Register rt, Register rs, int32_t j) {
  // This instruction uses same opcode as 'lui'. The difference in encoding is
  // 'lui' has zero reg. for rs field.
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, rs, rt, j);
}


void Assembler::daui(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  DCHECK(rs != zero_reg);
  GenInstrImmediate(DAUI, rs, rt, j);
}


void Assembler::dahi(Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(REGIMM, rs, DAHI, j);
}


void Assembler::dati(Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(REGIMM, rs, DATI, j);
}


void Assembler::ldl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(LDL, rs.rm(), rd, rs.offset_);
}


void Assembler::ldr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(LDR, rs.rm(), rd, rs.offset_);
}


void Assembler::sdl(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(SDL, rs.rm(), rd, rs.offset_);
}


void Assembler::sdr(Register rd, const MemOperand& rs) {
  DCHECK(is_int16(rs.offset_));
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrImmediate(SDR, rs.rm(), rd, rs.offset_);
}


void Assembler::ld(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LD, rs.rm(), rd, rs.offset_);
}


void Assembler::sd(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SD, rs.rm(), rd, rs.offset_);
}


// ---------PC-Relative instructions-----------

void Assembler::addiupc(Register rs, int32_t imm19) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.is_valid() && is_int19(imm19));
  uint32_t imm21 = ADDIUPC << kImm19Bits | (imm19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::lwpc(Register rs, int32_t offset19) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.is_valid() && is_int19(offset19));
  uint32_t imm21 = LWPC << kImm19Bits | (offset19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::lwupc(Register rs, int32_t offset19) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.is_valid() && is_int19(offset19));
  uint32_t imm21 = LWUPC << kImm19Bits | (offset19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::ldpc(Register rs, int32_t offset18) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.is_valid() && is_int18(offset18));
  uint32_t imm21 = LDPC << kImm18Bits | (offset18 & kImm18Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::auipc(Register rs, int16_t imm16) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.is_valid());
  uint32_t imm21 = AUIPC << kImm16Bits | (imm16 & kImm16Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::aluipc(Register rs, int16_t imm16) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(rs.is_valid());
  uint32_t imm21 = ALUIPC << kImm16Bits | (imm16 & kImm16Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


// -------------Misc-instructions--------------

// Break / Trap instructions.
void Assembler::break_(uint32_t code, bool break_as_stop) {
  DCHECK_EQ(code & ~0xFFFFF, 0);
  // We need to invalidate breaks that could be stops as well because the
  // simulator expects a char pointer after the stop instruction.
  // See constants-mips.h for explanation.
  DCHECK((break_as_stop &&
          code <= kMaxStopCode &&
          code > kMaxWatchpointCode) ||
         (!break_as_stop &&
          (code > kMaxStopCode ||
           code <= kMaxWatchpointCode)));
  Instr break_instr = SPECIAL | BREAK | (code << 6);
  emit(break_instr);
}


void Assembler::stop(const char* msg, uint32_t code) {
  DCHECK_GT(code, kMaxWatchpointCode);
  DCHECK_LE(code, kMaxStopCode);
#if defined(V8_HOST_ARCH_MIPS) || defined(V8_HOST_ARCH_MIPS64)
  break_(0x54321);
#else  // V8_HOST_ARCH_MIPS
  break_(code, true);
#endif
}


void Assembler::tge(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr = SPECIAL | TGE | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tgeu(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr = SPECIAL | TGEU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tlt(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TLT | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tltu(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TLTU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::teq(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TEQ | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tne(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TNE | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}

void Assembler::sync() {
  Instr sync_instr = SPECIAL | SYNC;
  emit(sync_instr);
}

// Move from HI/LO register.

void Assembler::mfhi(Register rd) {
  GenInstrRegister(SPECIAL, zero_reg, zero_reg, rd, 0, MFHI);
}


void Assembler::mflo(Register rd) {
  GenInstrRegister(SPECIAL, zero_reg, zero_reg, rd, 0, MFLO);
}


// Set on less than instructions.
void Assembler::slt(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLT);
}


void Assembler::sltu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLTU);
}


void Assembler::slti(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(SLTI, rs, rt, j);
}


void Assembler::sltiu(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(SLTIU, rs, rt, j);
}


// Conditional move.
void Assembler::movz(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVZ);
}


void Assembler::movn(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVN);
}


void Assembler::movt(Register rd, Register rs, uint16_t cc) {
  Register rt = Register::from_code((cc & 0x0007) << 2 | 1);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::movf(Register rd, Register rs, uint16_t cc) {
  Register rt = Register::from_code((cc & 0x0007) << 2 | 0);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::min_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  min(S, fd, fs, ft);
}


void Assembler::min_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  min(D, fd, fs, ft);
}


void Assembler::max_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  max(S, fd, fs, ft);
}


void Assembler::max_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  max(D, fd, fs, ft);
}


void Assembler::mina_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  mina(S, fd, fs, ft);
}


void Assembler::mina_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  mina(D, fd, fs, ft);
}


void Assembler::maxa_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  maxa(S, fd, fs, ft);
}


void Assembler::maxa_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  maxa(D, fd, fs, ft);
}


void Assembler::max(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAX);
}


void Assembler::min(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MIN);
}


// GPR.
void Assembler::seleqz(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELEQZ_S);
}


// GPR.
void Assembler::selnez(Register rd, Register rs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELNEZ_S);
}


// Bit twiddling.
void Assembler::clz(Register rd, Register rs) {
  if (kArchVariant != kMips64r6) {
    // clz instr requires same GPR number in 'rd' and 'rt' fields.
    GenInstrRegister(SPECIAL2, rs, rd, rd, 0, CLZ);
  } else {
    GenInstrRegister(SPECIAL, rs, zero_reg, rd, 1, CLZ_R6);
  }
}


void Assembler::dclz(Register rd, Register rs) {
  if (kArchVariant != kMips64r6) {
    // dclz instr requires same GPR number in 'rd' and 'rt' fields.
    GenInstrRegister(SPECIAL2, rs, rd, rd, 0, DCLZ);
  } else {
    GenInstrRegister(SPECIAL, rs, zero_reg, rd, 1, DCLZ_R6);
  }
}


void Assembler::ins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ins.
  // ins instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK((kArchVariant == kMips64r2) || (kArchVariant == kMips64r6));
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, INS);
}


void Assembler::dins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dins.
  // dins instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, DINS);
}

void Assembler::dinsm_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dins.
  // dinsm instr has 'rt' field as dest, and two uint5: msbminus32, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1 - 32, pos, DINSM);
}

void Assembler::dinsu_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dins.
  // dinsu instr has 'rt' field as dest, and two uint5: msbminus32, lsbminus32.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1 - 32, pos - 32, DINSU);
}

void Assembler::ext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ext.
  // ext instr has 'rt' field as dest, and two uint5: msbd, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, EXT);
}


void Assembler::dext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dext.
  // dext instr has 'rt' field as dest, and two uint5: msbd, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, DEXT);
}

void Assembler::dextm_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dextm.
  // dextm instr has 'rt' field as dest, and two uint5: msbdminus32, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1 - 32, pos, DEXTM);
}

void Assembler::dextu_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Dextu.
  // dextu instr has 'rt' field as dest, and two uint5: msbd, lsbminus32.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos - 32, DEXTU);
}


void Assembler::bitswap(Register rd, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, 0, BSHFL);
}


void Assembler::dbitswap(Register rd, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, 0, DBSHFL);
}


void Assembler::pref(int32_t hint, const MemOperand& rs) {
  DCHECK(is_uint5(hint) && is_uint16(rs.offset_));
  Instr instr = PREF | (rs.rm().code() << kRsShift) | (hint << kRtShift)
      | (rs.offset_);
  emit(instr);
}


void Assembler::align(Register rd, Register rs, Register rt, uint8_t bp) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(is_uint3(bp));
  uint16_t sa = (ALIGN << kBp2Bits) | bp;
  GenInstrRegister(SPECIAL3, rs, rt, rd, sa, BSHFL);
}


void Assembler::dalign(Register rd, Register rs, Register rt, uint8_t bp) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK(is_uint3(bp));
  uint16_t sa = (DALIGN << kBp3Bits) | bp;
  GenInstrRegister(SPECIAL3, rs, rt, rd, sa, DBSHFL);
}

void Assembler::wsbh(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, WSBH, BSHFL);
}

void Assembler::dsbh(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, DSBH, DBSHFL);
}

void Assembler::dshd(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, DSHD, DBSHFL);
}

void Assembler::seh(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, SEH, BSHFL);
}

void Assembler::seb(Register rd, Register rt) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, SEB, BSHFL);
}

// --------Coprocessor-instructions----------------

// Load, store, move.
void Assembler::lwc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(LWC1, src.rm(), fd, src.offset_);
}


void Assembler::ldc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(LDC1, src.rm(), fd, src.offset_);
}

void Assembler::swc1(FPURegister fs, const MemOperand& src) {
  GenInstrImmediate(SWC1, src.rm(), fs, src.offset_);
}

void Assembler::sdc1(FPURegister fs, const MemOperand& src) {
  GenInstrImmediate(SDC1, src.rm(), fs, src.offset_);
}


void Assembler::mtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTC1, rt, fs, f0);
}


void Assembler::mthc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTHC1, rt, fs, f0);
}


void Assembler::dmtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, DMTC1, rt, fs, f0);
}


void Assembler::mfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFC1, rt, fs, f0);
}


void Assembler::mfhc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFHC1, rt, fs, f0);
}


void Assembler::dmfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, DMFC1, rt, fs, f0);
}


void Assembler::ctc1(Register rt, FPUControlRegister fs) {
  GenInstrRegister(COP1, CTC1, rt, fs);
}


void Assembler::cfc1(Register rt, FPUControlRegister fs) {
  GenInstrRegister(COP1, CFC1, rt, fs);
}


void Assembler::DoubleAsTwoUInt32(double d, uint32_t* lo, uint32_t* hi) {
  uint64_t i;
  memcpy(&i, &d, 8);

  *lo = i & 0xFFFFFFFF;
  *hi = i >> 32;
}


void Assembler::sel(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK((fmt == D) || (fmt == S));

  GenInstrRegister(COP1, fmt, ft, fs, fd, SEL);
}


void Assembler::sel_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  sel(S, fd, fs, ft);
}


void Assembler::sel_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  sel(D, fd, fs, ft);
}


// FPR.
void Assembler::seleqz(SecondaryField fmt, FPURegister fd, FPURegister fs,
                       FPURegister ft) {
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, SELEQZ_C);
}


void Assembler::seleqz_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  seleqz(D, fd, fs, ft);
}


void Assembler::seleqz_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  seleqz(S, fd, fs, ft);
}


void Assembler::selnez_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  selnez(D, fd, fs, ft);
}


void Assembler::selnez_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  selnez(S, fd, fs, ft);
}


void Assembler::movz_s(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrRegister(COP1, S, rt, fs, fd, MOVZ_C);
}


void Assembler::movz_d(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrRegister(COP1, D, rt, fs, fd, MOVZ_C);
}


void Assembler::movt_s(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  FPURegister ft = FPURegister::from_code((cc & 0x0007) << 2 | 1);
  GenInstrRegister(COP1, S, ft, fs, fd, MOVF);
}


void Assembler::movt_d(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  FPURegister ft = FPURegister::from_code((cc & 0x0007) << 2 | 1);
  GenInstrRegister(COP1, D, ft, fs, fd, MOVF);
}


void Assembler::movf_s(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  FPURegister ft = FPURegister::from_code((cc & 0x0007) << 2 | 0);
  GenInstrRegister(COP1, S, ft, fs, fd, MOVF);
}


void Assembler::movf_d(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  FPURegister ft = FPURegister::from_code((cc & 0x0007) << 2 | 0);
  GenInstrRegister(COP1, D, ft, fs, fd, MOVF);
}


void Assembler::movn_s(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrRegister(COP1, S, rt, fs, fd, MOVN_C);
}


void Assembler::movn_d(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK_EQ(kArchVariant, kMips64r2);
  GenInstrRegister(COP1, D, rt, fs, fd, MOVN_C);
}


// FPR.
void Assembler::selnez(SecondaryField fmt, FPURegister fd, FPURegister fs,
                       FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, SELNEZ_C);
}


// Arithmetic.

void Assembler::add_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, ADD_D);
}


void Assembler::add_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, ADD_D);
}


void Assembler::sub_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, SUB_D);
}


void Assembler::sub_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, SUB_D);
}


void Assembler::mul_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, MUL_D);
}


void Assembler::mul_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, MUL_D);
}

void Assembler::madd_s(FPURegister fd, FPURegister fr, FPURegister fs,
                       FPURegister ft) {
  // On Loongson 3A (MIPS64R2), MADD.S instruction is actually fused MADD.S and
  // this causes failure in some of the tests. Since this optimization is rarely
  // used, and not used at all on MIPS64R6, this isntruction is removed.
  UNREACHABLE();
}

void Assembler::madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
    FPURegister ft) {
  // On Loongson 3A (MIPS64R2), MADD.D instruction is actually fused MADD.D and
  // this causes failure in some of the tests. Since this optimization is rarely
  // used, and not used at all on MIPS64R6, this isntruction is removed.
  UNREACHABLE();
}

void Assembler::msub_s(FPURegister fd, FPURegister fr, FPURegister fs,
                       FPURegister ft) {
  // See explanation for instruction madd_s.
  UNREACHABLE();
}

void Assembler::msub_d(FPURegister fd, FPURegister fr, FPURegister fs,
                       FPURegister ft) {
  // See explanation for instruction madd_d.
  UNREACHABLE();
}

void Assembler::maddf_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, S, ft, fs, fd, MADDF_S);
}

void Assembler::maddf_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, D, ft, fs, fd, MADDF_D);
}

void Assembler::msubf_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, S, ft, fs, fd, MSUBF_S);
}

void Assembler::msubf_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, D, ft, fs, fd, MSUBF_D);
}

void Assembler::div_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, DIV_D);
}


void Assembler::div_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, DIV_D);
}


void Assembler::abs_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ABS_D);
}


void Assembler::abs_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ABS_D);
}


void Assembler::mov_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, MOV_D);
}


void Assembler::mov_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, MOV_S);
}


void Assembler::neg_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, NEG_D);
}


void Assembler::neg_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, NEG_D);
}


void Assembler::sqrt_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, SQRT_D);
}


void Assembler::sqrt_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, SQRT_D);
}


void Assembler::rsqrt_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, RSQRT_S);
}


void Assembler::rsqrt_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, RSQRT_D);
}


void Assembler::recip_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, RECIP_D);
}


void Assembler::recip_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, RECIP_S);
}


// Conversions.
void Assembler::cvt_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_W_S);
}


void Assembler::cvt_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_W_D);
}


void Assembler::trunc_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_W_S);
}


void Assembler::trunc_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_W_D);
}


void Assembler::round_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_W_S);
}


void Assembler::round_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_W_D);
}


void Assembler::floor_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_W_S);
}


void Assembler::floor_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_W_D);
}


void Assembler::ceil_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_W_S);
}


void Assembler::ceil_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_W_D);
}


void Assembler::rint_s(FPURegister fd, FPURegister fs) { rint(S, fd, fs); }


void Assembler::rint_d(FPURegister fd, FPURegister fs) { rint(D, fd, fs); }


void Assembler::rint(SecondaryField fmt, FPURegister fd, FPURegister fs) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, fmt, f0, fs, fd, RINT);
}


void Assembler::cvt_l_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_L_S);
}


void Assembler::cvt_l_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_L_D);
}


void Assembler::trunc_l_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_L_S);
}


void Assembler::trunc_l_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_L_D);
}


void Assembler::round_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_L_S);
}


void Assembler::round_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_L_D);
}


void Assembler::floor_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_L_S);
}


void Assembler::floor_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_L_D);
}


void Assembler::ceil_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_L_S);
}


void Assembler::ceil_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_L_D);
}


void Assembler::class_s(FPURegister fd, FPURegister fs) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, S, f0, fs, fd, CLASS_S);
}


void Assembler::class_d(FPURegister fd, FPURegister fs) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  GenInstrRegister(COP1, D, f0, fs, fd, CLASS_D);
}


void Assembler::mina(SecondaryField fmt, FPURegister fd, FPURegister fs,
                     FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MINA);
}


void Assembler::maxa(SecondaryField fmt, FPURegister fd, FPURegister fs,
                     FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAXA);
}


void Assembler::cvt_s_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_S_W);
}


void Assembler::cvt_s_l(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_S_L);
}


void Assembler::cvt_s_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_S_D);
}


void Assembler::cvt_d_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_D_W);
}


void Assembler::cvt_d_l(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_D_L);
}


void Assembler::cvt_d_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_D_S);
}


// Conditions for >= MIPSr6.
void Assembler::cmp(FPUCondition cond, SecondaryField fmt,
    FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  DCHECK_EQ(fmt & ~(31 << kRsShift), 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | (0 << 5) | cond;
  emit(instr);
}


void Assembler::cmp_s(FPUCondition cond, FPURegister fd, FPURegister fs,
                      FPURegister ft) {
  cmp(cond, W, fd, fs, ft);
}

void Assembler::cmp_d(FPUCondition cond, FPURegister fd, FPURegister fs,
                      FPURegister ft) {
  cmp(cond, L, fd, fs, ft);
}


void Assembler::bc1eqz(int16_t offset, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  Instr instr = COP1 | BC1EQZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1nez(int16_t offset, FPURegister ft) {
  DCHECK_EQ(kArchVariant, kMips64r6);
  Instr instr = COP1 | BC1NEZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


// Conditions for < MIPSr6.
void Assembler::c(FPUCondition cond, SecondaryField fmt,
    FPURegister fs, FPURegister ft, uint16_t cc) {
  DCHECK_NE(kArchVariant, kMips64r6);
  DCHECK(is_uint3(cc));
  DCHECK(fmt == S || fmt == D);
  DCHECK_EQ(fmt & ~(31 << kRsShift), 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift | fs.code() << kFsShift
      | cc << 8 | 3 << 4 | cond;
  emit(instr);
}


void Assembler::c_s(FPUCondition cond, FPURegister fs, FPURegister ft,
                    uint16_t cc) {
  c(cond, S, fs, ft, cc);
}


void Assembler::c_d(FPUCondition cond, FPURegister fs, FPURegister ft,
                    uint16_t cc) {
  c(cond, D, fs, ft, cc);
}


void Assembler::fcmp(FPURegister src1, const double src2,
      FPUCondition cond) {
  DCHECK_EQ(src2, 0.0);
  mtc1(zero_reg, f14);
  cvt_d_w(f14, f14);
  c(cond, D, src1, f14, 0);
}


void Assembler::bc1f(int16_t offset, uint16_t cc) {
  DCHECK(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 0 << 16 | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1t(int16_t offset, uint16_t cc) {
  DCHECK(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 1 << 16 | (offset & kImm16Mask);
  emit(instr);
}

// ---------- MSA instructions ------------
#define MSA_BRANCH_LIST(V) \
  V(bz_v, BZ_V)            \
  V(bz_b, BZ_B)            \
  V(bz_h, BZ_H)            \
  V(bz_w, BZ_W)            \
  V(bz_d, BZ_D)            \
  V(bnz_v, BNZ_V)          \
  V(bnz_b, BNZ_B)          \
  V(bnz_h, BNZ_H)          \
  V(bnz_w, BNZ_W)          \
  V(bnz_d, BNZ_D)

#define MSA_BRANCH(name, opcode)                         \
  void Assembler::name(MSARegister wt, int16_t offset) { \
    GenInstrMsaBranch(opcode, wt, offset);               \
  }

MSA_BRANCH_LIST(MSA_BRANCH)
#undef MSA_BRANCH
#undef MSA_BRANCH_LIST

#define MSA_LD_ST_LIST(V) \
  V(ld_b, LD_B)           \
  V(ld_h, LD_H)           \
  V(ld_w, LD_W)           \
  V(ld_d, LD_D)           \
  V(st_b, ST_B)           \
  V(st_h, ST_H)           \
  V(st_w, ST_W)           \
  V(st_d, ST_D)

#define MSA_LD_ST(name, opcode)                                  \
  void Assembler::name(MSARegister wd, const MemOperand& rs) {   \
    MemOperand source = rs;                                      \
    AdjustBaseAndOffset(source);                                 \
    if (is_int10(source.offset())) {                             \
      GenInstrMsaMI10(opcode, source.offset(), source.rm(), wd); \
    } else {                                                     \
      UseScratchRegisterScope temps(this);                       \
      Register scratch = temps.Acquire();                        \
      DCHECK(rs.rm() != scratch);                                \
      daddiu(scratch, source.rm(), source.offset());             \
      GenInstrMsaMI10(opcode, 0, scratch, wd);                   \
    }                                                            \
  }

MSA_LD_ST_LIST(MSA_LD_ST)
#undef MSA_LD_ST
#undef MSA_BRANCH_LIST

#define MSA_I10_LIST(V) \
  V(ldi_b, I5_DF_b)     \
  V(ldi_h, I5_DF_h)     \
  V(ldi_w, I5_DF_w)     \
  V(ldi_d, I5_DF_d)

#define MSA_I10(name, format)                           \
  void Assembler::name(MSARegister wd, int32_t imm10) { \
    GenInstrMsaI10(LDI, format, imm10, wd);             \
  }
MSA_I10_LIST(MSA_I10)
#undef MSA_I10
#undef MSA_I10_LIST

#define MSA_I5_LIST(V) \
  V(addvi, ADDVI)      \
  V(subvi, SUBVI)      \
  V(maxi_s, MAXI_S)    \
  V(maxi_u, MAXI_U)    \
  V(mini_s, MINI_S)    \
  V(mini_u, MINI_U)    \
  V(ceqi, CEQI)        \
  V(clti_s, CLTI_S)    \
  V(clti_u, CLTI_U)    \
  V(clei_s, CLEI_S)    \
  V(clei_u, CLEI_U)

#define MSA_I5_FORMAT(name, opcode, format)                       \
  void Assembler::name##_##format(MSARegister wd, MSARegister ws, \
                                  uint32_t imm5) {                \
    GenInstrMsaI5(opcode, I5_DF_##format, imm5, ws, wd);          \
  }

#define MSA_I5(name, opcode)     \
  MSA_I5_FORMAT(name, opcode, b) \
  MSA_I5_FORMAT(name, opcode, h) \
  MSA_I5_FORMAT(name, opcode, w) \
  MSA_I5_FORMAT(name, opcode, d)

MSA_I5_LIST(MSA_I5)
#undef MSA_I5
#undef MSA_I5_FORMAT
#undef MSA_I5_LIST

#define MSA_I8_LIST(V) \
  V(andi_b, ANDI_B)    \
  V(ori_b, ORI_B)      \
  V(nori_b, NORI_B)    \
  V(xori_b, XORI_B)    \
  V(bmnzi_b, BMNZI_B)  \
  V(bmzi_b, BMZI_B)    \
  V(bseli_b, BSELI_B)  \
  V(shf_b, SHF_B)      \
  V(shf_h, SHF_H)      \
  V(shf_w, SHF_W)

#define MSA_I8(name, opcode)                                            \
  void Assembler::name(MSARegister wd, MSARegister ws, uint32_t imm8) { \
    GenInstrMsaI8(opcode, imm8, ws, wd);                                \
  }

MSA_I8_LIST(MSA_I8)
#undef MSA_I8
#undef MSA_I8_LIST

#define MSA_VEC_LIST(V) \
  V(and_v, AND_V)       \
  V(or_v, OR_V)         \
  V(nor_v, NOR_V)       \
  V(xor_v, XOR_V)       \
  V(bmnz_v, BMNZ_V)     \
  V(bmz_v, BMZ_V)       \
  V(bsel_v, BSEL_V)

#define MSA_VEC(name, opcode)                                            \
  void Assembler::name(MSARegister wd, MSARegister ws, MSARegister wt) { \
    GenInstrMsaVec(opcode, wt, ws, wd);                                  \
  }

MSA_VEC_LIST(MSA_VEC)
#undef MSA_VEC
#undef MSA_VEC_LIST

#define MSA_2R_LIST(V) \
  V(pcnt, PCNT)        \
  V(nloc, NLOC)        \
  V(nlzc, NLZC)

#define MSA_2R_FORMAT(name, opcode, format)                         \
  void Assembler::name##_##format(MSARegister wd, MSARegister ws) { \
    GenInstrMsa2R(opcode, MSA_2R_DF_##format, ws, wd);              \
  }

#define MSA_2R(name, opcode)     \
  MSA_2R_FORMAT(name, opcode, b) \
  MSA_2R_FORMAT(name, opcode, h) \
  MSA_2R_FORMAT(name, opcode, w) \
  MSA_2R_FORMAT(name, opcode, d)

MSA_2R_LIST(MSA_2R)
#undef MSA_2R
#undef MSA_2R_FORMAT
#undef MSA_2R_LIST

#define MSA_FILL(format)                                              \
  void Assembler::fill_##format(MSARegister wd, Register rs) {        \
    DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));      \
    DCHECK(rs.is_valid() && wd.is_valid());                           \
    Instr instr = MSA | MSA_2R_FORMAT | FILL | MSA_2R_DF_##format |   \
                  (rs.code() << kWsShift) | (wd.code() << kWdShift) | \
                  MSA_VEC_2R_2RF_MINOR;                               \
    emit(instr);                                                      \
  }

MSA_FILL(b)
MSA_FILL(h)
MSA_FILL(w)
MSA_FILL(d)
#undef MSA_FILL

#define MSA_2RF_LIST(V) \
  V(fclass, FCLASS)     \
  V(ftrunc_s, FTRUNC_S) \
  V(ftrunc_u, FTRUNC_U) \
  V(fsqrt, FSQRT)       \
  V(frsqrt, FRSQRT)     \
  V(frcp, FRCP)         \
  V(frint, FRINT)       \
  V(flog2, FLOG2)       \
  V(fexupl, FEXUPL)     \
  V(fexupr, FEXUPR)     \
  V(ffql, FFQL)         \
  V(ffqr, FFQR)         \
  V(ftint_s, FTINT_S)   \
  V(ftint_u, FTINT_U)   \
  V(ffint_s, FFINT_S)   \
  V(ffint_u, FFINT_U)

#define MSA_2RF_FORMAT(name, opcode, format)                        \
  void Assembler::name##_##format(MSARegister wd, MSARegister ws) { \
    GenInstrMsa2RF(opcode, MSA_2RF_DF_##format, ws, wd);            \
  }

#define MSA_2RF(name, opcode)     \
  MSA_2RF_FORMAT(name, opcode, w) \
  MSA_2RF_FORMAT(name, opcode, d)

MSA_2RF_LIST(MSA_2RF)
#undef MSA_2RF
#undef MSA_2RF_FORMAT
#undef MSA_2RF_LIST

#define MSA_3R_LIST(V)  \
  V(sll, SLL_MSA)       \
  V(sra, SRA_MSA)       \
  V(srl, SRL_MSA)       \
  V(bclr, BCLR)         \
  V(bset, BSET)         \
  V(bneg, BNEG)         \
  V(binsl, BINSL)       \
  V(binsr, BINSR)       \
  V(addv, ADDV)         \
  V(subv, SUBV)         \
  V(max_s, MAX_S)       \
  V(max_u, MAX_U)       \
  V(min_s, MIN_S)       \
  V(min_u, MIN_U)       \
  V(max_a, MAX_A)       \
  V(min_a, MIN_A)       \
  V(ceq, CEQ)           \
  V(clt_s, CLT_S)       \
  V(clt_u, CLT_U)       \
  V(cle_s, CLE_S)       \
  V(cle_u, CLE_U)       \
  V(add_a, ADD_A)       \
  V(adds_a, ADDS_A)     \
  V(adds_s, ADDS_S)     \
  V(adds_u, ADDS_U)     \
  V(ave_s, AVE_S)       \
  V(ave_u, AVE_U)       \
  V(aver_s, AVER_S)     \
  V(aver_u, AVER_U)     \
  V(subs_s, SUBS_S)     \
  V(subs_u, SUBS_U)     \
  V(subsus_u, SUBSUS_U) \
  V(subsuu_s, SUBSUU_S) \
  V(asub_s, ASUB_S)     \
  V(asub_u, ASUB_U)     \
  V(mulv, MULV)         \
  V(maddv, MADDV)       \
  V(msubv, MSUBV)       \
  V(div_s, DIV_S_MSA)   \
  V(div_u, DIV_U)       \
  V(mod_s, MOD_S)       \
  V(mod_u, MOD_U)       \
  V(dotp_s, DOTP_S)     \
  V(dotp_u, DOTP_U)     \
  V(dpadd_s, DPADD_S)   \
  V(dpadd_u, DPADD_U)   \
  V(dpsub_s, DPSUB_S)   \
  V(dpsub_u, DPSUB_U)   \
  V(pckev, PCKEV)       \
  V(pckod, PCKOD)       \
  V(ilvl, ILVL)         \
  V(ilvr, ILVR)         \
  V(ilvev, ILVEV)       \
  V(ilvod, ILVOD)       \
  V(vshf, VSHF)         \
  V(srar, SRAR)         \
  V(srlr, SRLR)         \
  V(hadd_s, HADD_S)     \
  V(hadd_u, HADD_U)     \
  V(hsub_s, HSUB_S)     \
  V(hsub_u, HSUB_U)

#define MSA_3R_FORMAT(name, opcode, format)                             \
  void Assembler::name##_##format(MSARegister wd, MSARegister ws,       \
                                  MSARegister wt) {                     \
    GenInstrMsa3R<MSARegister>(opcode, MSA_3R_DF_##format, wt, ws, wd); \
  }

#define MSA_3R_FORMAT_SLD_SPLAT(name, opcode, format)                \
  void Assembler::name##_##format(MSARegister wd, MSARegister ws,    \
                                  Register rt) {                     \
    GenInstrMsa3R<Register>(opcode, MSA_3R_DF_##format, rt, ws, wd); \
  }

#define MSA_3R(name, opcode)     \
  MSA_3R_FORMAT(name, opcode, b) \
  MSA_3R_FORMAT(name, opcode, h) \
  MSA_3R_FORMAT(name, opcode, w) \
  MSA_3R_FORMAT(name, opcode, d)

#define MSA_3R_SLD_SPLAT(name, opcode)     \
  MSA_3R_FORMAT_SLD_SPLAT(name, opcode, b) \
  MSA_3R_FORMAT_SLD_SPLAT(name, opcode, h) \
  MSA_3R_FORMAT_SLD_SPLAT(name, opcode, w) \
  MSA_3R_FORMAT_SLD_SPLAT(name, opcode, d)

MSA_3R_LIST(MSA_3R)
MSA_3R_SLD_SPLAT(sld, SLD)
MSA_3R_SLD_SPLAT(splat, SPLAT)

#undef MSA_3R
#undef MSA_3R_FORMAT
#undef MSA_3R_FORMAT_SLD_SPLAT
#undef MSA_3R_SLD_SPLAT
#undef MSA_3R_LIST

#define MSA_3RF_LIST1(V) \
  V(fcaf, FCAF)          \
  V(fcun, FCUN)          \
  V(fceq, FCEQ)          \
  V(fcueq, FCUEQ)        \
  V(fclt, FCLT)          \
  V(fcult, FCULT)        \
  V(fcle, FCLE)          \
  V(fcule, FCULE)        \
  V(fsaf, FSAF)          \
  V(fsun, FSUN)          \
  V(fseq, FSEQ)          \
  V(fsueq, FSUEQ)        \
  V(fslt, FSLT)          \
  V(fsult, FSULT)        \
  V(fsle, FSLE)          \
  V(fsule, FSULE)        \
  V(fadd, FADD)          \
  V(fsub, FSUB)          \
  V(fmul, FMUL)          \
  V(fdiv, FDIV)          \
  V(fmadd, FMADD)        \
  V(fmsub, FMSUB)        \
  V(fexp2, FEXP2)        \
  V(fmin, FMIN)          \
  V(fmin_a, FMIN_A)      \
  V(fmax, FMAX)          \
  V(fmax_a, FMAX_A)      \
  V(fcor, FCOR)          \
  V(fcune, FCUNE)        \
  V(fcne, FCNE)          \
  V(fsor, FSOR)          \
  V(fsune, FSUNE)        \
  V(fsne, FSNE)

#define MSA_3RF_LIST2(V) \
  V(fexdo, FEXDO)        \
  V(ftq, FTQ)            \
  V(mul_q, MUL_Q)        \
  V(madd_q, MADD_Q)      \
  V(msub_q, MSUB_Q)      \
  V(mulr_q, MULR_Q)      \
  V(maddr_q, MADDR_Q)    \
  V(msubr_q, MSUBR_Q)

#define MSA_3RF_FORMAT(name, opcode, df, df_c)                \
  void Assembler::name##_##df(MSARegister wd, MSARegister ws, \
                              MSARegister wt) {               \
    GenInstrMsa3RF(opcode, df_c, wt, ws, wd);                 \
  }

#define MSA_3RF_1(name, opcode)      \
  MSA_3RF_FORMAT(name, opcode, w, 0) \
  MSA_3RF_FORMAT(name, opcode, d, 1)

#define MSA_3RF_2(name, opcode)      \
  MSA_3RF_FORMAT(name, opcode, h, 0) \
  MSA_3RF_FORMAT(name, opcode, w, 1)

MSA_3RF_LIST1(MSA_3RF_1)
MSA_3RF_LIST2(MSA_3RF_2)
#undef MSA_3RF_1
#undef MSA_3RF_2
#undef MSA_3RF_FORMAT
#undef MSA_3RF_LIST1
#undef MSA_3RF_LIST2

void Assembler::sldi_b(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SLDI, ELM_DF_B, n, ws, wd);
}

void Assembler::sldi_h(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SLDI, ELM_DF_H, n, ws, wd);
}

void Assembler::sldi_w(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SLDI, ELM_DF_W, n, ws, wd);
}

void Assembler::sldi_d(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SLDI, ELM_DF_D, n, ws, wd);
}

void Assembler::splati_b(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SPLATI, ELM_DF_B, n, ws, wd);
}

void Assembler::splati_h(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SPLATI, ELM_DF_H, n, ws, wd);
}

void Assembler::splati_w(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SPLATI, ELM_DF_W, n, ws, wd);
}

void Assembler::splati_d(MSARegister wd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<MSARegister, MSARegister>(SPLATI, ELM_DF_D, n, ws, wd);
}

void Assembler::copy_s_b(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_S, ELM_DF_B, n, ws, rd);
}

void Assembler::copy_s_h(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_S, ELM_DF_H, n, ws, rd);
}

void Assembler::copy_s_w(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_S, ELM_DF_W, n, ws, rd);
}

void Assembler::copy_s_d(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_S, ELM_DF_D, n, ws, rd);
}

void Assembler::copy_u_b(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_U, ELM_DF_B, n, ws, rd);
}

void Assembler::copy_u_h(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_U, ELM_DF_H, n, ws, rd);
}

void Assembler::copy_u_w(Register rd, MSARegister ws, uint32_t n) {
  GenInstrMsaElm<Register, MSARegister>(COPY_U, ELM_DF_W, n, ws, rd);
}

void Assembler::insert_b(MSARegister wd, uint32_t n, Register rs) {
  GenInstrMsaElm<MSARegister, Register>(INSERT, ELM_DF_B, n, rs, wd);
}

void Assembler::insert_h(MSARegister wd, uint32_t n, Register rs) {
  GenInstrMsaElm<MSARegister, Register>(INSERT, ELM_DF_H, n, rs, wd);
}

void Assembler::insert_w(MSARegister wd, uint32_t n, Register rs) {
  GenInstrMsaElm<MSARegister, Register>(INSERT, ELM_DF_W, n, rs, wd);
}

void Assembler::insert_d(MSARegister wd, uint32_t n, Register rs) {
  GenInstrMsaElm<MSARegister, Register>(INSERT, ELM_DF_D, n, rs, wd);
}

void Assembler::insve_b(MSARegister wd, uint32_t n, MSARegister ws) {
  GenInstrMsaElm<MSARegister, MSARegister>(INSVE, ELM_DF_B, n, ws, wd);
}

void Assembler::insve_h(MSARegister wd, uint32_t n, MSARegister ws) {
  GenInstrMsaElm<MSARegister, MSARegister>(INSVE, ELM_DF_H, n, ws, wd);
}

void Assembler::insve_w(MSARegister wd, uint32_t n, MSARegister ws) {
  GenInstrMsaElm<MSARegister, MSARegister>(INSVE, ELM_DF_W, n, ws, wd);
}

void Assembler::insve_d(MSARegister wd, uint32_t n, MSARegister ws) {
  GenInstrMsaElm<MSARegister, MSARegister>(INSVE, ELM_DF_D, n, ws, wd);
}

void Assembler::move_v(MSARegister wd, MSARegister ws) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(ws.is_valid() && wd.is_valid());
  Instr instr = MSA | MOVE_V | (ws.code() << kWsShift) |
                (wd.code() << kWdShift) | MSA_ELM_MINOR;
  emit(instr);
}

void Assembler::ctcmsa(MSAControlRegister cd, Register rs) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(cd.is_valid() && rs.is_valid());
  Instr instr = MSA | CTCMSA | (rs.code() << kWsShift) |
                (cd.code() << kWdShift) | MSA_ELM_MINOR;
  emit(instr);
}

void Assembler::cfcmsa(Register rd, MSAControlRegister cs) {
  DCHECK((kArchVariant == kMips64r6) && IsEnabled(MIPS_SIMD));
  DCHECK(rd.is_valid() && cs.is_valid());
  Instr instr = MSA | CFCMSA | (cs.code() << kWsShift) |
                (rd.code() << kWdShift) | MSA_ELM_MINOR;
  emit(instr);
}

#define MSA_BIT_LIST(V) \
  V(slli, SLLI)         \
  V(srai, SRAI)         \
  V(srli, SRLI)         \
  V(bclri, BCLRI)       \
  V(bseti, BSETI)       \
  V(bnegi, BNEGI)       \
  V(binsli, BINSLI)     \
  V(binsri, BINSRI)     \
  V(sat_s, SAT_S)       \
  V(sat_u, SAT_U)       \
  V(srari, SRARI)       \
  V(srlri, SRLRI)

#define MSA_BIT_FORMAT(name, opcode, format)                      \
  void Assembler::name##_##format(MSARegister wd, MSARegister ws, \
                                  uint32_t m) {                   \
    GenInstrMsaBit(opcode, BIT_DF_##format, m, ws, wd);           \
  }

#define MSA_BIT(name, opcode)     \
  MSA_BIT_FORMAT(name, opcode, b) \
  MSA_BIT_FORMAT(name, opcode, h) \
  MSA_BIT_FORMAT(name, opcode, w) \
  MSA_BIT_FORMAT(name, opcode, d)

MSA_BIT_LIST(MSA_BIT)
#undef MSA_BIT
#undef MSA_BIT_FORMAT
#undef MSA_BIT_LIST

int Assembler::RelocateInternalReference(RelocInfo::Mode rmode, byte* pc,
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
    Instr instr_lui = instr_at(pc + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pc + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pc + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));
    // TODO(plind): symbolic names for the shifts.
    int64_t imm = (instr_lui & static_cast<int64_t>(kImm16Mask)) << 48;
    imm |= (instr_ori & static_cast<int64_t>(kImm16Mask)) << 32;
    imm |= (instr_ori2 & static_cast<int64_t>(kImm16Mask)) << 16;
    // Sign extend address.
    imm >>= 16;

    if (imm == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    imm += pc_delta;
    DCHECK_EQ(imm & 3, 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;
    instr_ori2 &= ~kImm16Mask;

    instr_at_put(pc + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> 32) & kImm16Mask));
    instr_at_put(pc + 1 * Assembler::kInstrSize,
                 instr_ori | (imm >> 16 & kImm16Mask));
    instr_at_put(pc + 3 * Assembler::kInstrSize,
                 instr_ori2 | (imm & kImm16Mask));
    return 4;  // Number of instructions patched.
  } else if (IsJ(instr) || IsJal(instr)) {
    // Regular j/jal relocation.
    uint32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    imm28 += pc_delta;
    imm28 &= kImm28Mask;
    instr &= ~kImm26Mask;
    DCHECK_EQ(imm28 & 3, 0);
    uint32_t imm26 = static_cast<uint32_t>(imm28 >> 2);
    instr_at_put(pc, instr | (imm26 & kImm26Mask));
    return 1;  // Number of instructions patched.
  } else {
    DCHECK(((instr & kJumpRawMask) == kJRawMark) ||
           ((instr & kJumpRawMask) == kJalRawMark));
    // Unbox raw offset and emit j/jal.
    int32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    // Sign extend 28-bit offset to 32-bit.
    imm28 = (imm28 << 4) >> 4;
    uint64_t target =
        static_cast<int64_t>(imm28) + reinterpret_cast<uint64_t>(pc);
    target &= kImm28Mask;
    DCHECK_EQ(imm28 & 3, 0);
    uint32_t imm26 = static_cast<uint32_t>(target >> 2);
    // Check markings whether to emit j or jal.
    uint32_t unbox = (instr & kJRawMark) ? J : JAL;
    instr_at_put(pc, unbox | (imm26 & kImm26Mask));
    return 1;  // Number of instructions patched.
  }
}


void Assembler::GrowBuffer() {
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // the new buffer
  if (buffer_size_ < 1 * MB) {
    desc.buffer_size = 2*buffer_size_;
  } else {
    desc.buffer_size = buffer_size_ + 1*MB;
  }

  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if (desc.buffer_size > kMaximalBufferSize) {
    V8::FatalProcessOutOfMemory("Assembler::GrowBuffer");
  }

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;

  desc.instr_size = pc_offset();
  desc.reloc_size =
      static_cast<int>((buffer_ + buffer_size_) - reloc_info_writer.pos());

  // Copy the data.
  intptr_t pc_delta = desc.buffer - buffer_;
  intptr_t rc_delta = (desc.buffer + desc.buffer_size) -
      (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(reloc_info_writer.pos() + rc_delta,
              reloc_info_writer.pos(), desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate runtime entries.
  for (RelocIterator it(desc); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::INTERNAL_REFERENCE) {
      byte* p = reinterpret_cast<byte*>(it.rinfo()->pc());
      RelocateInternalReference(rmode, p, pc_delta);
    }
  }
  DCHECK(!overflow());
}


void Assembler::db(uint8_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dd(uint32_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dq(uint64_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dd(Label* label) {
  uint64_t data;
  CheckForEmitInForbiddenSlot();
  if (label->is_bound()) {
    data = reinterpret_cast<uint64_t>(buffer_ + label->pos());
  } else {
    data = jump_address(label);
    unbound_labels_count_++;
    internal_reference_positions_.insert(label->pos());
  }
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  EmitHelper(data);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(pc_, rmode, data, nullptr);
  if (!RelocInfo::IsNone(rinfo.rmode())) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
        !serializer_enabled() && !emit_debug_code()) {
      return;
    }
    DCHECK_GE(buffer_space(), kMaxRelocSize);  // Too late to grow buffer here.
    reloc_info_writer.Write(&rinfo);
  }
}


void Assembler::BlockTrampolinePoolFor(int instructions) {
  CheckTrampolinePoolQuick(instructions);
  BlockTrampolinePoolBefore(pc_offset() + instructions * kInstrSize);
}


void Assembler::CheckTrampolinePool() {
  // Some small sequences of instructions must not be broken up by the
  // insertion of a trampoline pool; such sequences are protected by setting
  // either trampoline_pool_blocked_nesting_ or no_trampoline_pool_before_,
  // which are both checked here. Also, recursive calls to CheckTrampolinePool
  // are blocked by trampoline_pool_blocked_nesting_.
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
    // First we emit jump (2 instructions), then we emit trampoline pool.
    { BlockTrampolinePoolScope block_trampoline_pool(this);
      Label after_pool;
      if (kArchVariant == kMips64r6) {
        bc(&after_pool);
      } else {
        b(&after_pool);
      }
      nop();

      int pool_start = pc_offset();
      for (int i = 0; i < unbound_labels_count_; i++) {
        { BlockGrowBufferScope block_buf_growth(this);
          // Buffer growth (and relocation) must be blocked for internal
          // references until associated instructions are emitted and available
          // to be patched.
          RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE_ENCODED);
          j(&after_pool);
        }
        nop();
      }
      bind(&after_pool);
      trampoline_ = Trampoline(pool_start, unbound_labels_count_);

      trampoline_emitted_ = true;
      // As we are only going to emit trampoline once, we need to prevent any
      // further emission.
      next_buffer_check_ = kMaxInt;
    }
  } else {
    // Number of branches to unbound label at this point is zero, so we can
    // move next buffer check to maximum.
    next_buffer_check_ = pc_offset() +
        kMaxBranchOffset - kTrampolineSlotsSize * 16;
  }
  return;
}


Address Assembler::target_address_at(Address pc) {
  Instr instr0 = instr_at(pc);
  Instr instr1 = instr_at(pc + 1 * kInstrSize);
  Instr instr3 = instr_at(pc + 3 * kInstrSize);

  // Interpret 4 instructions for address generated by li: See listing in
  // Assembler::set_target_address_at() just below.
  if ((GetOpcodeField(instr0) == LUI) && (GetOpcodeField(instr1) == ORI) &&
      (GetOpcodeField(instr3) == ORI)) {
    // Assemble the 48 bit value.
     int64_t addr  = static_cast<int64_t>(
          ((uint64_t)(GetImmediate16(instr0)) << 32) |
          ((uint64_t)(GetImmediate16(instr1)) << 16) |
          ((uint64_t)(GetImmediate16(instr3))));

    // Sign extend to get canonical address.
    addr = (addr << 16) >> 16;
    return reinterpret_cast<Address>(addr);
  }
  // We should never get here, force a bad address if we do.
  UNREACHABLE();
}


// MIPS and ia32 use opposite encoding for qNaN and sNaN, such that ia32
// qNaN is a MIPS sNaN, and ia32 sNaN is MIPS qNaN. If running from a heap
// snapshot generated on ia32, the resulting MIPS sNaN must be quieted.
// OS::nan_value() returns a qNaN.
void Assembler::QuietNaN(HeapObject* object) {
  HeapNumber::cast(object)->set_value(std::numeric_limits<double>::quiet_NaN());
}


// On Mips64, a target address is stored in a 4-instruction sequence:
//    0: lui(rd, (j.imm64_ >> 32) & kImm16Mask);
//    1: ori(rd, rd, (j.imm64_ >> 16) & kImm16Mask);
//    2: dsll(rd, rd, 16);
//    3: ori(rd, rd, j.imm32_ & kImm16Mask);
//
// Patching the address must replace all the lui & ori instructions,
// and flush the i-cache.
//
// There is an optimization below, which emits a nop when the address
// fits in just 16 bits. This is unlikely to help, and should be benchmarked,
// and possibly removed.
void Assembler::set_target_value_at(Isolate* isolate, Address pc,
                                    uint64_t target,
                                    ICacheFlushMode icache_flush_mode) {
  // There is an optimization where only 4 instructions are used to load address
  // in code on MIP64 because only 48-bits of address is effectively used.
  // It relies on fact the upper [63:48] bits are not used for virtual address
  // translation and they have to be set according to value of bit 47 in order
  // get canonical address.
  Instr instr1 = instr_at(pc + kInstrSize);
  uint32_t rt_code = GetRt(instr1);
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);

#ifdef DEBUG
  // Check we have the result from a li macro-instruction.
  Instr instr0 = instr_at(pc);
  Instr instr3 = instr_at(pc + kInstrSize * 3);
  DCHECK((GetOpcodeField(instr0) == LUI && GetOpcodeField(instr1) == ORI &&
          GetOpcodeField(instr3) == ORI));
#endif

  // Must use 4 instructions to insure patchable code.
  // lui rt, upper-16.
  // ori rt, rt, lower-16.
  // dsll rt, rt, 16.
  // ori rt rt, lower-16.
  *p = LUI | (rt_code << kRtShift) | ((target >> 32) & kImm16Mask);
  *(p + 1) = ORI | (rt_code << kRtShift) | (rt_code << kRsShift) |
             ((target >> 16) & kImm16Mask);
  *(p + 3) = ORI | (rt_code << kRsShift) | (rt_code << kRtShift) |
             (target & kImm16Mask);

  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(isolate, pc, 4 * Assembler::kInstrSize);
  }
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

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64

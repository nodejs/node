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
// Copyright 2012 the V8 project authors. All rights reserved.

#include "src/arm/assembler-arm.h"

#if V8_TARGET_ARCH_ARM

#include "src/arm/assembler-arm-inl.h"
#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

static const unsigned kArmv6 = 0u;
static const unsigned kArmv7 = kArmv6 | (1u << ARMv7);
static const unsigned kArmv7WithSudiv = kArmv7 | (1u << ARMv7_SUDIV);
static const unsigned kArmv8 = kArmv7WithSudiv | (1u << ARMv8);

static unsigned CpuFeaturesFromCommandLine() {
  unsigned result;
  if (strcmp(FLAG_arm_arch, "armv8") == 0) {
    result = kArmv8;
  } else if (strcmp(FLAG_arm_arch, "armv7+sudiv") == 0) {
    result = kArmv7WithSudiv;
  } else if (strcmp(FLAG_arm_arch, "armv7") == 0) {
    result = kArmv7;
  } else if (strcmp(FLAG_arm_arch, "armv6") == 0) {
    result = kArmv6;
  } else {
    fprintf(stderr, "Error: unrecognised value for --arm-arch ('%s').\n",
            FLAG_arm_arch);
    fprintf(stderr,
            "Supported values are:  armv8\n"
            "                       armv7+sudiv\n"
            "                       armv7\n"
            "                       armv6\n");
    CHECK(false);
  }

  // If any of the old (deprecated) flags are specified, print a warning, but
  // otherwise try to respect them for now.
  // TODO(jbramley): When all the old bots have been updated, remove this.
  if (FLAG_enable_armv7.has_value || FLAG_enable_vfp3.has_value ||
      FLAG_enable_32dregs.has_value || FLAG_enable_neon.has_value ||
      FLAG_enable_sudiv.has_value || FLAG_enable_armv8.has_value) {
    // As an approximation of the old behaviour, set the default values from the
    // arm_arch setting, then apply the flags over the top.
    bool enable_armv7 = (result & (1u << ARMv7)) != 0;
    bool enable_vfp3 = (result & (1u << ARMv7)) != 0;
    bool enable_32dregs = (result & (1u << ARMv7)) != 0;
    bool enable_neon = (result & (1u << ARMv7)) != 0;
    bool enable_sudiv = (result & (1u << ARMv7_SUDIV)) != 0;
    bool enable_armv8 = (result & (1u << ARMv8)) != 0;
    if (FLAG_enable_armv7.has_value) {
      fprintf(stderr,
              "Warning: --enable_armv7 is deprecated. "
              "Use --arm_arch instead.\n");
      enable_armv7 = FLAG_enable_armv7.value;
    }
    if (FLAG_enable_vfp3.has_value) {
      fprintf(stderr,
              "Warning: --enable_vfp3 is deprecated. "
              "Use --arm_arch instead.\n");
      enable_vfp3 = FLAG_enable_vfp3.value;
    }
    if (FLAG_enable_32dregs.has_value) {
      fprintf(stderr,
              "Warning: --enable_32dregs is deprecated. "
              "Use --arm_arch instead.\n");
      enable_32dregs = FLAG_enable_32dregs.value;
    }
    if (FLAG_enable_neon.has_value) {
      fprintf(stderr,
              "Warning: --enable_neon is deprecated. "
              "Use --arm_arch instead.\n");
      enable_neon = FLAG_enable_neon.value;
    }
    if (FLAG_enable_sudiv.has_value) {
      fprintf(stderr,
              "Warning: --enable_sudiv is deprecated. "
              "Use --arm_arch instead.\n");
      enable_sudiv = FLAG_enable_sudiv.value;
    }
    if (FLAG_enable_armv8.has_value) {
      fprintf(stderr,
              "Warning: --enable_armv8 is deprecated. "
              "Use --arm_arch instead.\n");
      enable_armv8 = FLAG_enable_armv8.value;
    }
    // Emulate the old implications.
    if (enable_armv8) {
      enable_vfp3 = true;
      enable_neon = true;
      enable_32dregs = true;
      enable_sudiv = true;
    }
    // Select the best available configuration.
    if (enable_armv7 && enable_vfp3 && enable_32dregs && enable_neon) {
      if (enable_sudiv) {
        if (enable_armv8) {
          result = kArmv8;
        } else {
          result = kArmv7WithSudiv;
        }
      } else {
        result = kArmv7;
      }
    } else {
      result = kArmv6;
    }
  }
  return result;
}

// Get the CPU features enabled by the build.
// For cross compilation the preprocessor symbols such as
// CAN_USE_ARMV7_INSTRUCTIONS and CAN_USE_VFP3_INSTRUCTIONS can be used to
// enable ARMv7 and VFPv3 instructions when building the snapshot. However,
// these flags should be consistent with a supported ARM configuration:
//  "armv6":       ARMv6 + VFPv2
//  "armv7":       ARMv7 + VFPv3-D32 + NEON
//  "armv7+sudiv": ARMv7 + VFPv4-D32 + NEON + SUDIV
//  "armv8":       ARMv8 (+ all of the above)
static constexpr unsigned CpuFeaturesFromCompiler() {
// TODO(jbramley): Once the build flags are simplified, these tests should
// also be simplified.

// Check *architectural* implications.
#if defined(CAN_USE_ARMV8_INSTRUCTIONS) && !defined(CAN_USE_ARMV7_INSTRUCTIONS)
#error "CAN_USE_ARMV8_INSTRUCTIONS should imply CAN_USE_ARMV7_INSTRUCTIONS"
#endif
#if defined(CAN_USE_ARMV8_INSTRUCTIONS) && !defined(CAN_USE_SUDIV)
#error "CAN_USE_ARMV8_INSTRUCTIONS should imply CAN_USE_SUDIV"
#endif
#if defined(CAN_USE_ARMV7_INSTRUCTIONS) != defined(CAN_USE_VFP3_INSTRUCTIONS)
// V8 requires VFP, and all ARMv7 devices with VFP have VFPv3. Similarly,
// VFPv3 isn't available before ARMv7.
#error "CAN_USE_ARMV7_INSTRUCTIONS should match CAN_USE_VFP3_INSTRUCTIONS"
#endif
#if defined(CAN_USE_NEON) && !defined(CAN_USE_ARMV7_INSTRUCTIONS)
#error "CAN_USE_NEON should imply CAN_USE_ARMV7_INSTRUCTIONS"
#endif

// Find compiler-implied features.
#if defined(CAN_USE_ARMV8_INSTRUCTIONS) &&                           \
    defined(CAN_USE_ARMV7_INSTRUCTIONS) && defined(CAN_USE_SUDIV) && \
    defined(CAN_USE_NEON) && defined(CAN_USE_VFP3_INSTRUCTIONS)
  return kArmv8;
#elif defined(CAN_USE_ARMV7_INSTRUCTIONS) && defined(CAN_USE_SUDIV) && \
    defined(CAN_USE_NEON) && defined(CAN_USE_VFP3_INSTRUCTIONS)
  return kArmv7WithSudiv;
#elif defined(CAN_USE_ARMV7_INSTRUCTIONS) && defined(CAN_USE_NEON) && \
    defined(CAN_USE_VFP3_INSTRUCTIONS)
  return kArmv7;
#else
  return kArmv6;
#endif
}


void CpuFeatures::ProbeImpl(bool cross_compile) {
  dcache_line_size_ = 64;

  unsigned command_line = CpuFeaturesFromCommandLine();
  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) {
    supported_ |= command_line & CpuFeaturesFromCompiler();
    return;
  }

#ifndef __arm__
  // For the simulator build, use whatever the flags specify.
  supported_ |= command_line;

#else  // __arm__
  // Probe for additional features at runtime.
  base::CPU cpu;
  // Runtime detection is slightly fuzzy, and some inferences are necessary.
  unsigned runtime = kArmv6;
  // NEON and VFPv3 imply at least ARMv7-A.
  if (cpu.has_neon() && cpu.has_vfp3_d32()) {
    DCHECK(cpu.has_vfp3());
    runtime |= kArmv7;
    if (cpu.has_idiva()) {
      runtime |= kArmv7WithSudiv;
      if (cpu.architecture() >= 8) {
        runtime |= kArmv8;
      }
    }
  }

  // Use the best of the features found by CPU detection and those inferred from
  // the build system. In both cases, restrict available features using the
  // command-line. Note that the command-line flags are very permissive (kArmv8)
  // by default.
  supported_ |= command_line & CpuFeaturesFromCompiler();
  supported_ |= command_line & runtime;

  // Additional tuning options.

  // ARM Cortex-A9 and Cortex-A5 have 32 byte cachelines.
  if (cpu.implementer() == base::CPU::ARM &&
      (cpu.part() == base::CPU::ARM_CORTEX_A5 ||
       cpu.part() == base::CPU::ARM_CORTEX_A9)) {
    dcache_line_size_ = 32;
  }
#endif

  DCHECK_IMPLIES(IsSupported(ARMv7_SUDIV), IsSupported(ARMv7));
  DCHECK_IMPLIES(IsSupported(ARMv8), IsSupported(ARMv7_SUDIV));
}


void CpuFeatures::PrintTarget() {
  const char* arm_arch = NULL;
  const char* arm_target_type = "";
  const char* arm_no_probe = "";
  const char* arm_fpu = "";
  const char* arm_thumb = "";
  const char* arm_float_abi = NULL;

#if !defined __arm__
  arm_target_type = " simulator";
#endif

#if defined ARM_TEST_NO_FEATURE_PROBE
  arm_no_probe = " noprobe";
#endif

#if defined CAN_USE_ARMV8_INSTRUCTIONS
  arm_arch = "arm v8";
#elif defined CAN_USE_ARMV7_INSTRUCTIONS
  arm_arch = "arm v7";
#else
  arm_arch = "arm v6";
#endif

#if defined CAN_USE_NEON
  arm_fpu = " neon";
#elif defined CAN_USE_VFP3_INSTRUCTIONS
#  if defined CAN_USE_VFP32DREGS
  arm_fpu = " vfp3";
#  else
  arm_fpu = " vfp3-d16";
#  endif
#else
  arm_fpu = " vfp2";
#endif

#ifdef __arm__
  arm_float_abi = base::OS::ArmUsingHardFloat() ? "hard" : "softfp";
#elif USE_EABI_HARDFLOAT
  arm_float_abi = "hard";
#else
  arm_float_abi = "softfp";
#endif

#if defined __arm__ && (defined __thumb__) || (defined __thumb2__)
  arm_thumb = " thumb";
#endif

  printf("target%s%s %s%s%s %s\n",
         arm_target_type, arm_no_probe, arm_arch, arm_fpu, arm_thumb,
         arm_float_abi);
}


void CpuFeatures::PrintFeatures() {
  printf("ARMv8=%d ARMv7=%d VFPv3=%d VFP32DREGS=%d NEON=%d SUDIV=%d",
         CpuFeatures::IsSupported(ARMv8), CpuFeatures::IsSupported(ARMv7),
         CpuFeatures::IsSupported(VFPv3), CpuFeatures::IsSupported(VFP32DREGS),
         CpuFeatures::IsSupported(NEON), CpuFeatures::IsSupported(SUDIV));
#ifdef __arm__
  bool eabi_hardfloat = base::OS::ArmUsingHardFloat();
#elif USE_EABI_HARDFLOAT
  bool eabi_hardfloat = true;
#else
  bool eabi_hardfloat = false;
#endif
  printf(" USE_EABI_HARDFLOAT=%d\n", eabi_hardfloat);
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// static
const int RelocInfo::kApplyMask = 0;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on ARM means that it is a movw/movt instruction, or is an
  // embedded constant pool entry.  These only occur if
  // FLAG_enable_embedded_constant_pool is true.
  return FLAG_enable_embedded_constant_pool;
}


bool RelocInfo::IsInConstantPool() {
  return Assembler::is_constant_pool_load(pc_);
}

Address RelocInfo::wasm_memory_reference() {
  DCHECK(IsWasmMemoryReference(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

uint32_t RelocInfo::wasm_memory_size_reference() {
  DCHECK(IsWasmMemorySizeReference(rmode_));
  return reinterpret_cast<uint32_t>(Assembler::target_address_at(pc_, host_));
}

Address RelocInfo::wasm_global_reference() {
  DCHECK(IsWasmGlobalReference(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

uint32_t RelocInfo::wasm_function_table_size_reference() {
  DCHECK(IsWasmFunctionTableSizeReference(rmode_));
  return reinterpret_cast<uint32_t>(Assembler::target_address_at(pc_, host_));
}

void RelocInfo::unchecked_update_wasm_memory_reference(
    Address address, ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate_, pc_, host_, address, flush_mode);
}

void RelocInfo::unchecked_update_wasm_size(uint32_t size,
                                           ICacheFlushMode flush_mode) {
  Assembler::set_target_address_at(isolate_, pc_, host_,
                                   reinterpret_cast<Address>(size), flush_mode);
}

// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand
// See assembler-arm-inl.h for inlined constructors

Operand::Operand(Handle<Object> handle) {
  AllowDeferredHandleDereference using_raw_address;
  rm_ = no_reg;
  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  if (obj->IsHeapObject()) {
    imm32_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // no relocation needed
    imm32_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE32;
  }
}


Operand::Operand(Register rm, ShiftOp shift_op, int shift_imm) {
  DCHECK(is_uint5(shift_imm));

  rm_ = rm;
  rs_ = no_reg;
  shift_op_ = shift_op;
  shift_imm_ = shift_imm & 31;

  if ((shift_op == ROR) && (shift_imm == 0)) {
    // ROR #0 is functionally equivalent to LSL #0 and this allow us to encode
    // RRX as ROR #0 (See below).
    shift_op = LSL;
  } else if (shift_op == RRX) {
    // encoded as ROR with shift_imm == 0
    DCHECK(shift_imm == 0);
    shift_op_ = ROR;
    shift_imm_ = 0;
  }
}


Operand::Operand(Register rm, ShiftOp shift_op, Register rs) {
  DCHECK(shift_op != RRX);
  rm_ = rm;
  rs_ = no_reg;
  shift_op_ = shift_op;
  rs_ = rs;
}


MemOperand::MemOperand(Register rn, int32_t offset, AddrMode am) {
  rn_ = rn;
  rm_ = no_reg;
  offset_ = offset;
  am_ = am;

  // Accesses below the stack pointer are not safe, and are prohibited by the
  // ABI. We can check obvious violations here.
  if (rn.is(sp)) {
    if (am == Offset) DCHECK_LE(0, offset);
    if (am == NegOffset) DCHECK_GE(0, offset);
  }
}


MemOperand::MemOperand(Register rn, Register rm, AddrMode am) {
  rn_ = rn;
  rm_ = rm;
  shift_op_ = LSL;
  shift_imm_ = 0;
  am_ = am;
}


MemOperand::MemOperand(Register rn, Register rm,
                       ShiftOp shift_op, int shift_imm, AddrMode am) {
  DCHECK(is_uint5(shift_imm));
  rn_ = rn;
  rm_ = rm;
  shift_op_ = shift_op;
  shift_imm_ = shift_imm & 31;
  am_ = am;
}


NeonMemOperand::NeonMemOperand(Register rn, AddrMode am, int align) {
  DCHECK((am == Offset) || (am == PostIndex));
  rn_ = rn;
  rm_ = (am == Offset) ? pc : sp;
  SetAlignment(align);
}


NeonMemOperand::NeonMemOperand(Register rn, Register rm, int align) {
  rn_ = rn;
  rm_ = rm;
  SetAlignment(align);
}


void NeonMemOperand::SetAlignment(int align) {
  switch (align) {
    case 0:
      align_ = 0;
      break;
    case 64:
      align_ = 1;
      break;
    case 128:
      align_ = 2;
      break;
    case 256:
      align_ = 3;
      break;
    default:
      UNREACHABLE();
      align_ = 0;
      break;
  }
}

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

// str(r, MemOperand(sp, 4, NegPreIndex), al) instruction (aka push(r))
// register r is not encoded.
const Instr kPushRegPattern =
    al | B26 | 4 | NegPreIndex | Register::kCode_sp * B16;
// ldr(r, MemOperand(sp, 4, PostIndex), al) instruction (aka pop(r))
// register r is not encoded.
const Instr kPopRegPattern =
    al | B26 | L | 4 | PostIndex | Register::kCode_sp * B16;
// ldr rd, [pc, #offset]
const Instr kLdrPCImmedMask = 15 * B24 | 7 * B20 | 15 * B16;
const Instr kLdrPCImmedPattern = 5 * B24 | L | Register::kCode_pc * B16;
// ldr rd, [pp, #offset]
const Instr kLdrPpImmedMask = 15 * B24 | 7 * B20 | 15 * B16;
const Instr kLdrPpImmedPattern = 5 * B24 | L | Register::kCode_r8 * B16;
// ldr rd, [pp, rn]
const Instr kLdrPpRegMask = 15 * B24 | 7 * B20 | 15 * B16;
const Instr kLdrPpRegPattern = 7 * B24 | L | Register::kCode_r8 * B16;
// vldr dd, [pc, #offset]
const Instr kVldrDPCMask = 15 * B24 | 3 * B20 | 15 * B16 | 15 * B8;
const Instr kVldrDPCPattern = 13 * B24 | L | Register::kCode_pc * B16 | 11 * B8;
// vldr dd, [pp, #offset]
const Instr kVldrDPpMask = 15 * B24 | 3 * B20 | 15 * B16 | 15 * B8;
const Instr kVldrDPpPattern = 13 * B24 | L | Register::kCode_r8 * B16 | 11 * B8;
// blxcc rm
const Instr kBlxRegMask =
    15 * B24 | 15 * B20 | 15 * B16 | 15 * B12 | 15 * B8 | 15 * B4;
const Instr kBlxRegPattern =
    B24 | B21 | 15 * B16 | 15 * B12 | 15 * B8 | BLX;
const Instr kBlxIp = al | kBlxRegPattern | ip.code();
const Instr kMovMvnMask = 0x6d * B21 | 0xf * B16;
const Instr kMovMvnPattern = 0xd * B21;
const Instr kMovMvnFlip = B22;
const Instr kMovLeaveCCMask = 0xdff * B16;
const Instr kMovLeaveCCPattern = 0x1a0 * B16;
const Instr kMovwPattern = 0x30 * B20;
const Instr kMovtPattern = 0x34 * B20;
const Instr kMovwLeaveCCFlip = 0x5 * B21;
const Instr kMovImmedMask = 0x7f * B21;
const Instr kMovImmedPattern = 0x1d * B21;
const Instr kOrrImmedMask = 0x7f * B21;
const Instr kOrrImmedPattern = 0x1c * B21;
const Instr kCmpCmnMask = 0xdd * B20 | 0xf * B12;
const Instr kCmpCmnPattern = 0x15 * B20;
const Instr kCmpCmnFlip = B21;
const Instr kAddSubFlip = 0x6 * B21;
const Instr kAndBicFlip = 0xe * B21;

// A mask for the Rd register for push, pop, ldr, str instructions.
const Instr kLdrRegFpOffsetPattern =
    al | B26 | L | Offset | Register::kCode_fp * B16;
const Instr kStrRegFpOffsetPattern =
    al | B26 | Offset | Register::kCode_fp * B16;
const Instr kLdrRegFpNegOffsetPattern =
    al | B26 | L | NegOffset | Register::kCode_fp * B16;
const Instr kStrRegFpNegOffsetPattern =
    al | B26 | NegOffset | Register::kCode_fp * B16;
const Instr kLdrStrInstrTypeMask = 0xffff0000;

Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size),
      recorded_ast_id_(TypeFeedbackId::None()),
      pending_32_bit_constants_(),
      pending_64_bit_constants_(),
      constant_pool_builder_(kLdrMaxReachBits, kVldrMaxReachBits) {
  pending_32_bit_constants_.reserve(kMinNumPendingConstants);
  pending_64_bit_constants_.reserve(kMinNumPendingConstants);
  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);
  next_buffer_check_ = 0;
  const_pool_blocked_nesting_ = 0;
  no_const_pool_before_ = 0;
  first_const_pool_32_use_ = -1;
  first_const_pool_64_use_ = -1;
  last_bound_pos_ = 0;
  ClearRecordedAstId();
  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    // Register objects tend to be abstracted and survive between scopes, so
    // it's awkward to use CpuFeatures::VFP32DREGS with CpuFeatureScope. To make
    // its use consistent with other features, we always enable it if we can.
    EnableCpuFeature(VFP32DREGS);
  }
}


Assembler::~Assembler() {
  DCHECK(const_pool_blocked_nesting_ == 0);
}


void Assembler::GetCode(CodeDesc* desc) {
  // Emit constant pool if necessary.
  int constant_pool_offset = 0;
  if (FLAG_enable_embedded_constant_pool) {
    constant_pool_offset = EmitEmbeddedConstantPool();
  } else {
    CheckConstPool(true, false);
    DCHECK(pending_32_bit_constants_.empty());
    DCHECK(pending_64_bit_constants_.empty());
  }
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


void Assembler::CodeTargetAlign() {
  // Preferred alignment of jump targets on some ARM chips.
  Align(8);
}


Condition Assembler::GetCondition(Instr instr) {
  return Instruction::ConditionField(instr);
}


bool Assembler::IsBranch(Instr instr) {
  return (instr & (B27 | B25)) == (B27 | B25);
}


int Assembler::GetBranchOffset(Instr instr) {
  DCHECK(IsBranch(instr));
  // Take the jump offset in the lower 24 bits, sign extend it and multiply it
  // with 4 to get the offset in bytes.
  return ((instr & kImm24Mask) << 8) >> 6;
}


bool Assembler::IsLdrRegisterImmediate(Instr instr) {
  return (instr & (B27 | B26 | B25 | B22 | B20)) == (B26 | B20);
}


bool Assembler::IsVldrDRegisterImmediate(Instr instr) {
  return (instr & (15 * B24 | 3 * B20 | 15 * B8)) == (13 * B24 | B20 | 11 * B8);
}


int Assembler::GetLdrRegisterImmediateOffset(Instr instr) {
  DCHECK(IsLdrRegisterImmediate(instr));
  bool positive = (instr & B23) == B23;
  int offset = instr & kOff12Mask;  // Zero extended offset.
  return positive ? offset : -offset;
}


int Assembler::GetVldrDRegisterImmediateOffset(Instr instr) {
  DCHECK(IsVldrDRegisterImmediate(instr));
  bool positive = (instr & B23) == B23;
  int offset = instr & kOff8Mask;  // Zero extended offset.
  offset <<= 2;
  return positive ? offset : -offset;
}


Instr Assembler::SetLdrRegisterImmediateOffset(Instr instr, int offset) {
  DCHECK(IsLdrRegisterImmediate(instr));
  bool positive = offset >= 0;
  if (!positive) offset = -offset;
  DCHECK(is_uint12(offset));
  // Set bit indicating whether the offset should be added.
  instr = (instr & ~B23) | (positive ? B23 : 0);
  // Set the actual offset.
  return (instr & ~kOff12Mask) | offset;
}


Instr Assembler::SetVldrDRegisterImmediateOffset(Instr instr, int offset) {
  DCHECK(IsVldrDRegisterImmediate(instr));
  DCHECK((offset & ~3) == offset);  // Must be 64-bit aligned.
  bool positive = offset >= 0;
  if (!positive) offset = -offset;
  DCHECK(is_uint10(offset));
  // Set bit indicating whether the offset should be added.
  instr = (instr & ~B23) | (positive ? B23 : 0);
  // Set the actual offset. Its bottom 2 bits are zero.
  return (instr & ~kOff8Mask) | (offset >> 2);
}


bool Assembler::IsStrRegisterImmediate(Instr instr) {
  return (instr & (B27 | B26 | B25 | B22 | B20)) == B26;
}


Instr Assembler::SetStrRegisterImmediateOffset(Instr instr, int offset) {
  DCHECK(IsStrRegisterImmediate(instr));
  bool positive = offset >= 0;
  if (!positive) offset = -offset;
  DCHECK(is_uint12(offset));
  // Set bit indicating whether the offset should be added.
  instr = (instr & ~B23) | (positive ? B23 : 0);
  // Set the actual offset.
  return (instr & ~kOff12Mask) | offset;
}


bool Assembler::IsAddRegisterImmediate(Instr instr) {
  return (instr & (B27 | B26 | B25 | B24 | B23 | B22 | B21)) == (B25 | B23);
}


Instr Assembler::SetAddRegisterImmediateOffset(Instr instr, int offset) {
  DCHECK(IsAddRegisterImmediate(instr));
  DCHECK(offset >= 0);
  DCHECK(is_uint12(offset));
  // Set the offset.
  return (instr & ~kOff12Mask) | offset;
}


Register Assembler::GetRd(Instr instr) {
  Register reg;
  reg.reg_code = Instruction::RdValue(instr);
  return reg;
}


Register Assembler::GetRn(Instr instr) {
  Register reg;
  reg.reg_code = Instruction::RnValue(instr);
  return reg;
}


Register Assembler::GetRm(Instr instr) {
  Register reg;
  reg.reg_code = Instruction::RmValue(instr);
  return reg;
}


Instr Assembler::GetConsantPoolLoadPattern() {
  if (FLAG_enable_embedded_constant_pool) {
    return kLdrPpImmedPattern;
  } else {
    return kLdrPCImmedPattern;
  }
}


Instr Assembler::GetConsantPoolLoadMask() {
  if (FLAG_enable_embedded_constant_pool) {
    return kLdrPpImmedMask;
  } else {
    return kLdrPCImmedMask;
  }
}


bool Assembler::IsPush(Instr instr) {
  return ((instr & ~kRdMask) == kPushRegPattern);
}


bool Assembler::IsPop(Instr instr) {
  return ((instr & ~kRdMask) == kPopRegPattern);
}


bool Assembler::IsStrRegFpOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kStrRegFpOffsetPattern);
}


bool Assembler::IsLdrRegFpOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kLdrRegFpOffsetPattern);
}


bool Assembler::IsStrRegFpNegOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kStrRegFpNegOffsetPattern);
}


bool Assembler::IsLdrRegFpNegOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kLdrRegFpNegOffsetPattern);
}


bool Assembler::IsLdrPcImmediateOffset(Instr instr) {
  // Check the instruction is indeed a
  // ldr<cond> <Rd>, [pc +/- offset_12].
  return (instr & kLdrPCImmedMask) == kLdrPCImmedPattern;
}


bool Assembler::IsLdrPpImmediateOffset(Instr instr) {
  // Check the instruction is indeed a
  // ldr<cond> <Rd>, [pp +/- offset_12].
  return (instr & kLdrPpImmedMask) == kLdrPpImmedPattern;
}


bool Assembler::IsLdrPpRegOffset(Instr instr) {
  // Check the instruction is indeed a
  // ldr<cond> <Rd>, [pp, +/- <Rm>].
  return (instr & kLdrPpRegMask) == kLdrPpRegPattern;
}


Instr Assembler::GetLdrPpRegOffsetPattern() { return kLdrPpRegPattern; }


bool Assembler::IsVldrDPcImmediateOffset(Instr instr) {
  // Check the instruction is indeed a
  // vldr<cond> <Dd>, [pc +/- offset_10].
  return (instr & kVldrDPCMask) == kVldrDPCPattern;
}


bool Assembler::IsVldrDPpImmediateOffset(Instr instr) {
  // Check the instruction is indeed a
  // vldr<cond> <Dd>, [pp +/- offset_10].
  return (instr & kVldrDPpMask) == kVldrDPpPattern;
}


bool Assembler::IsBlxReg(Instr instr) {
  // Check the instruction is indeed a
  // blxcc <Rm>
  return (instr & kBlxRegMask) == kBlxRegPattern;
}


bool Assembler::IsBlxIp(Instr instr) {
  // Check the instruction is indeed a
  // blx ip
  return instr == kBlxIp;
}


bool Assembler::IsTstImmediate(Instr instr) {
  return (instr & (B27 | B26 | I | kOpCodeMask | S | kRdMask)) ==
      (I | TST | S);
}


bool Assembler::IsCmpRegister(Instr instr) {
  return (instr & (B27 | B26 | I | kOpCodeMask | S | kRdMask | B4)) ==
      (CMP | S);
}


bool Assembler::IsCmpImmediate(Instr instr) {
  return (instr & (B27 | B26 | I | kOpCodeMask | S | kRdMask)) ==
      (I | CMP | S);
}


Register Assembler::GetCmpImmediateRegister(Instr instr) {
  DCHECK(IsCmpImmediate(instr));
  return GetRn(instr);
}


int Assembler::GetCmpImmediateRawImmediate(Instr instr) {
  DCHECK(IsCmpImmediate(instr));
  return instr & kOff12Mask;
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
//
// The linked labels form a link chain by making the branch offset
// in the instruction steam to point to the previous branch
// instruction using the same label.
//
// The link chain is terminated by a branch offset pointing to the
// same position.


int Assembler::target_at(int pos) {
  Instr instr = instr_at(pos);
  if (is_uint24(instr)) {
    // Emitted link to a label, not part of a branch.
    return instr;
  }
  DCHECK_EQ(5 * B25, instr & 7 * B25);  // b, bl, or blx imm24
  int imm26 = ((instr & kImm24Mask) << 8) >> 6;
  if ((Instruction::ConditionField(instr) == kSpecialCondition) &&
      ((instr & B24) != 0)) {
    // blx uses bit 24 to encode bit 2 of imm26
    imm26 += 2;
  }
  return pos + kPcLoadDelta + imm26;
}


void Assembler::target_at_put(int pos, int target_pos) {
  Instr instr = instr_at(pos);
  if (is_uint24(instr)) {
    DCHECK(target_pos == pos || target_pos >= 0);
    // Emitted link to a label, not part of a branch.
    // Load the position of the label relative to the generated code object
    // pointer in a register.

    // The existing code must be a single 24-bit label chain link, followed by
    // nops encoding the destination register. See mov_label_offset.

    // Extract the destination register from the first nop instructions.
    Register dst =
        Register::from_code(Instruction::RmValue(instr_at(pos + kInstrSize)));
    // In addition to the 24-bit label chain link, we expect to find one nop for
    // ARMv7 and above, or two nops for ARMv6. See mov_label_offset.
    DCHECK(IsNop(instr_at(pos + kInstrSize), dst.code()));
    if (!CpuFeatures::IsSupported(ARMv7)) {
      DCHECK(IsNop(instr_at(pos + 2 * kInstrSize), dst.code()));
    }

    // Here are the instructions we need to emit:
    //   For ARMv7: target24 => target16_1:target16_0
    //      movw dst, #target16_0
    //      movt dst, #target16_1
    //   For ARMv6: target24 => target8_2:target8_1:target8_0
    //      mov dst, #target8_0
    //      orr dst, dst, #target8_1 << 8
    //      orr dst, dst, #target8_2 << 16

    uint32_t target24 = target_pos + (Code::kHeaderSize - kHeapObjectTag);
    DCHECK(is_uint24(target24));
    if (is_uint8(target24)) {
      // If the target fits in a byte then only patch with a mov
      // instruction.
      CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos), 1,
                          CodePatcher::DONT_FLUSH);
      patcher.masm()->mov(dst, Operand(target24));
    } else {
      uint16_t target16_0 = target24 & kImm16Mask;
      uint16_t target16_1 = target24 >> 16;
      if (CpuFeatures::IsSupported(ARMv7)) {
        // Patch with movw/movt.
        if (target16_1 == 0) {
          CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos),
                              1, CodePatcher::DONT_FLUSH);
          CpuFeatureScope scope(patcher.masm(), ARMv7);
          patcher.masm()->movw(dst, target16_0);
        } else {
          CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos),
                              2, CodePatcher::DONT_FLUSH);
          CpuFeatureScope scope(patcher.masm(), ARMv7);
          patcher.masm()->movw(dst, target16_0);
          patcher.masm()->movt(dst, target16_1);
        }
      } else {
        // Patch with a sequence of mov/orr/orr instructions.
        uint8_t target8_0 = target16_0 & kImm8Mask;
        uint8_t target8_1 = target16_0 >> 8;
        uint8_t target8_2 = target16_1 & kImm8Mask;
        if (target8_2 == 0) {
          CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos),
                              2, CodePatcher::DONT_FLUSH);
          patcher.masm()->mov(dst, Operand(target8_0));
          patcher.masm()->orr(dst, dst, Operand(target8_1 << 8));
        } else {
          CodePatcher patcher(isolate(), reinterpret_cast<byte*>(buffer_ + pos),
                              3, CodePatcher::DONT_FLUSH);
          patcher.masm()->mov(dst, Operand(target8_0));
          patcher.masm()->orr(dst, dst, Operand(target8_1 << 8));
          patcher.masm()->orr(dst, dst, Operand(target8_2 << 16));
        }
      }
    }
    return;
  }
  int imm26 = target_pos - (pos + kPcLoadDelta);
  DCHECK_EQ(5 * B25, instr & 7 * B25);  // b, bl, or blx imm24
  if (Instruction::ConditionField(instr) == kSpecialCondition) {
    // blx uses bit 24 to encode bit 2 of imm26
    DCHECK_EQ(0, imm26 & 1);
    instr = (instr & ~(B24 | kImm24Mask)) | ((imm26 & 2) >> 1) * B24;
  } else {
    DCHECK_EQ(0, imm26 & 3);
    instr &= ~kImm24Mask;
  }
  int imm24 = imm26 >> 2;
  DCHECK(is_int24(imm24));
  instr_at_put(pos, instr | (imm24 & kImm24Mask));
}


void Assembler::print(Label* L) {
  if (L->is_unused()) {
    PrintF("unused label\n");
  } else if (L->is_bound()) {
    PrintF("bound label to %d\n", L->pos());
  } else if (L->is_linked()) {
    Label l = *L;
    PrintF("unbound label");
    while (l.is_linked()) {
      PrintF("@ %d ", l.pos());
      Instr instr = instr_at(l.pos());
      if ((instr & ~kImm24Mask) == 0) {
        PrintF("value\n");
      } else {
        DCHECK((instr & 7*B25) == 5*B25);  // b, bl, or blx
        Condition cond = Instruction::ConditionField(instr);
        const char* b;
        const char* c;
        if (cond == kSpecialCondition) {
          b = "blx";
          c = "";
        } else {
          if ((instr & B24) != 0)
            b = "bl";
          else
            b = "b";

          switch (cond) {
            case eq: c = "eq"; break;
            case ne: c = "ne"; break;
            case hs: c = "hs"; break;
            case lo: c = "lo"; break;
            case mi: c = "mi"; break;
            case pl: c = "pl"; break;
            case vs: c = "vs"; break;
            case vc: c = "vc"; break;
            case hi: c = "hi"; break;
            case ls: c = "ls"; break;
            case ge: c = "ge"; break;
            case lt: c = "lt"; break;
            case gt: c = "gt"; break;
            case le: c = "le"; break;
            case al: c = ""; break;
            default:
              c = "";
              UNREACHABLE();
          }
        }
        PrintF("%s%s\n", b, c);
      }
      next(&l);
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}


void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // must have a valid binding position
  while (L->is_linked()) {
    int fixup_pos = L->pos();
    next(L);  // call next before overwriting link with target at fixup_pos
    target_at_put(fixup_pos, pos);
  }
  L->bind_to(pos);

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_)
    last_bound_pos_ = pos;
}


void Assembler::bind(Label* L) {
  DCHECK(!L->is_bound());  // label can only be bound once
  bind_to(L, pc_offset());
}


void Assembler::next(Label* L) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos());
  if (link == L->pos()) {
    // Branch target points to the same instuction. This is the end of the link
    // chain.
    L->Unuse();
  } else {
    DCHECK(link >= 0);
    L->link_to(link);
  }
}


// Low-level code emission routines depending on the addressing mode.
// If this returns true then you have to use the rotate_imm and immed_8
// that it returns, because it may have already changed the instruction
// to match them!
static bool fits_shifter(uint32_t imm32,
                         uint32_t* rotate_imm,
                         uint32_t* immed_8,
                         Instr* instr) {
  // imm32 must be unsigned.
  for (int rot = 0; rot < 16; rot++) {
    uint32_t imm8 = base::bits::RotateLeft32(imm32, 2 * rot);
    if ((imm8 <= 0xff)) {
      *rotate_imm = rot;
      *immed_8 = imm8;
      return true;
    }
  }
  // If the opcode is one with a complementary version and the complementary
  // immediate fits, change the opcode.
  if (instr != NULL) {
    if ((*instr & kMovMvnMask) == kMovMvnPattern) {
      if (fits_shifter(~imm32, rotate_imm, immed_8, NULL)) {
        *instr ^= kMovMvnFlip;
        return true;
      } else if ((*instr & kMovLeaveCCMask) == kMovLeaveCCPattern) {
        if (CpuFeatures::IsSupported(ARMv7)) {
          if (imm32 < 0x10000) {
            *instr ^= kMovwLeaveCCFlip;
            *instr |= Assembler::EncodeMovwImmediate(imm32);
            *rotate_imm = *immed_8 = 0;  // Not used for movw.
            return true;
          }
        }
      }
    } else if ((*instr & kCmpCmnMask) == kCmpCmnPattern) {
      if (fits_shifter(-static_cast<int>(imm32), rotate_imm, immed_8, NULL)) {
        *instr ^= kCmpCmnFlip;
        return true;
      }
    } else {
      Instr alu_insn = (*instr & kALUMask);
      if (alu_insn == ADD ||
          alu_insn == SUB) {
        if (fits_shifter(-static_cast<int>(imm32), rotate_imm, immed_8, NULL)) {
          *instr ^= kAddSubFlip;
          return true;
        }
      } else if (alu_insn == AND ||
                 alu_insn == BIC) {
        if (fits_shifter(~imm32, rotate_imm, immed_8, NULL)) {
          *instr ^= kAndBicFlip;
          return true;
        }
      }
    }
  }
  return false;
}


// We have to use the temporary register for things that can be relocated even
// if they can be encoded in the ARM's 12 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Operand::must_output_reloc_info(const Assembler* assembler) const {
  if (rmode_ == RelocInfo::EXTERNAL_REFERENCE) {
    if (assembler != NULL && assembler->predictable_code_size()) return true;
    return assembler->serializer_enabled();
  } else if (RelocInfo::IsNone(rmode_)) {
    return false;
  }
  return true;
}


static bool use_mov_immediate_load(const Operand& x,
                                   const Assembler* assembler) {
  DCHECK(assembler != nullptr);
  if (FLAG_enable_embedded_constant_pool &&
      !assembler->is_constant_pool_available()) {
    return true;
  } else if (x.must_output_reloc_info(assembler)) {
    // Prefer constant pool if data is likely to be patched.
    return false;
  } else {
    // Otherwise, use immediate load if movw / movt is available.
    return CpuFeatures::IsSupported(ARMv7);
  }
}


int Operand::instructions_required(const Assembler* assembler,
                                   Instr instr) const {
  DCHECK(assembler != nullptr);
  if (rm_.is_valid()) return 1;
  uint32_t dummy1, dummy2;
  if (must_output_reloc_info(assembler) ||
      !fits_shifter(imm32_, &dummy1, &dummy2, &instr)) {
    // The immediate operand cannot be encoded as a shifter operand, or use of
    // constant pool is required.  First account for the instructions required
    // for the constant pool or immediate load
    int instructions;
    if (use_mov_immediate_load(*this, assembler)) {
      // A movw / movt or mov / orr immediate load.
      instructions = CpuFeatures::IsSupported(ARMv7) ? 2 : 4;
    } else if (assembler->ConstantPoolAccessIsInOverflow()) {
      // An overflowed constant pool load.
      instructions = CpuFeatures::IsSupported(ARMv7) ? 3 : 5;
    } else {
      // A small constant pool load.
      instructions = 1;
    }

    if ((instr & ~kCondMask) != 13 * B21) {  // mov, S not set
      // For a mov or mvn instruction which doesn't set the condition
      // code, the constant pool or immediate load is enough, otherwise we need
      // to account for the actual instruction being requested.
      instructions += 1;
    }
    return instructions;
  } else {
    // No use of constant pool and the immediate operand can be encoded as a
    // shifter operand.
    return 1;
  }
}


void Assembler::move_32_bit_immediate(Register rd,
                                      const Operand& x,
                                      Condition cond) {
  uint32_t imm32 = static_cast<uint32_t>(x.imm32_);
  if (x.must_output_reloc_info(this)) {
    RecordRelocInfo(x.rmode_);
  }

  if (use_mov_immediate_load(x, this)) {
    Register target = rd.code() == pc.code() ? ip : rd;
    if (CpuFeatures::IsSupported(ARMv7)) {
      CpuFeatureScope scope(this, ARMv7);
      if (!FLAG_enable_embedded_constant_pool &&
          x.must_output_reloc_info(this)) {
        // Make sure the movw/movt doesn't get separated.
        BlockConstPoolFor(2);
      }
      movw(target, imm32 & 0xffff, cond);
      movt(target, imm32 >> 16, cond);
    } else {
      DCHECK(FLAG_enable_embedded_constant_pool);
      mov(target, Operand(imm32 & kImm8Mask), LeaveCC, cond);
      orr(target, target, Operand(imm32 & (kImm8Mask << 8)), LeaveCC, cond);
      orr(target, target, Operand(imm32 & (kImm8Mask << 16)), LeaveCC, cond);
      orr(target, target, Operand(imm32 & (kImm8Mask << 24)), LeaveCC, cond);
    }
    if (target.code() != rd.code()) {
      mov(rd, target, LeaveCC, cond);
    }
  } else {
    DCHECK(!FLAG_enable_embedded_constant_pool || is_constant_pool_available());
    ConstantPoolEntry::Access access =
        ConstantPoolAddEntry(pc_offset(), x.rmode_, x.imm32_);
    if (access == ConstantPoolEntry::OVERFLOWED) {
      DCHECK(FLAG_enable_embedded_constant_pool);
      Register target = rd.code() == pc.code() ? ip : rd;
      // Emit instructions to load constant pool offset.
      if (CpuFeatures::IsSupported(ARMv7)) {
        CpuFeatureScope scope(this, ARMv7);
        movw(target, 0, cond);
        movt(target, 0, cond);
      } else {
        mov(target, Operand(0), LeaveCC, cond);
        orr(target, target, Operand(0), LeaveCC, cond);
        orr(target, target, Operand(0), LeaveCC, cond);
        orr(target, target, Operand(0), LeaveCC, cond);
      }
      // Load from constant pool at offset.
      ldr(rd, MemOperand(pp, target), cond);
    } else {
      DCHECK(access == ConstantPoolEntry::REGULAR);
      ldr(rd, MemOperand(FLAG_enable_embedded_constant_pool ? pp : pc, 0),
          cond);
    }
  }
}


void Assembler::addrmod1(Instr instr,
                         Register rn,
                         Register rd,
                         const Operand& x) {
  CheckBuffer();
  DCHECK((instr & ~(kCondMask | kOpCodeMask | S)) == 0);
  if (!x.rm_.is_valid()) {
    // Immediate.
    uint32_t rotate_imm;
    uint32_t immed_8;
    if (x.must_output_reloc_info(this) ||
        !fits_shifter(x.imm32_, &rotate_imm, &immed_8, &instr)) {
      // The immediate operand cannot be encoded as a shifter operand, so load
      // it first to register ip and change the original instruction to use ip.
      // However, if the original instruction is a 'mov rd, x' (not setting the
      // condition code), then replace it with a 'ldr rd, [pc]'.
      CHECK(!rn.is(ip));  // rn should never be ip, or will be trashed
      Condition cond = Instruction::ConditionField(instr);
      if ((instr & ~kCondMask) == 13*B21) {  // mov, S not set
        move_32_bit_immediate(rd, x, cond);
      } else {
        mov(ip, x, LeaveCC, cond);
        addrmod1(instr, rn, rd, Operand(ip));
      }
      return;
    }
    instr |= I | rotate_imm*B8 | immed_8;
  } else if (!x.rs_.is_valid()) {
    // Immediate shift.
    instr |= x.shift_imm_*B7 | x.shift_op_ | x.rm_.code();
  } else {
    // Register shift.
    DCHECK(!rn.is(pc) && !rd.is(pc) && !x.rm_.is(pc) && !x.rs_.is(pc));
    instr |= x.rs_.code()*B8 | x.shift_op_ | B4 | x.rm_.code();
  }
  emit(instr | rn.code()*B16 | rd.code()*B12);
  if (rn.is(pc) || x.rm_.is(pc)) {
    // Block constant pool emission for one instruction after reading pc.
    BlockConstPoolFor(1);
  }
}


void Assembler::addrmod2(Instr instr, Register rd, const MemOperand& x) {
  DCHECK((instr & ~(kCondMask | B | L)) == B26);
  int am = x.am_;
  if (!x.rm_.is_valid()) {
    // Immediate offset.
    int offset_12 = x.offset_;
    if (offset_12 < 0) {
      offset_12 = -offset_12;
      am ^= U;
    }
    if (!is_uint12(offset_12)) {
      // Immediate offset cannot be encoded, load it first to register ip
      // rn (and rd in a load) should never be ip, or will be trashed.
      DCHECK(!x.rn_.is(ip) && ((instr & L) == L || !rd.is(ip)));
      mov(ip, Operand(x.offset_), LeaveCC, Instruction::ConditionField(instr));
      addrmod2(instr, rd, MemOperand(x.rn_, ip, x.am_));
      return;
    }
    DCHECK(offset_12 >= 0);  // no masking needed
    instr |= offset_12;
  } else {
    // Register offset (shift_imm_ and shift_op_ are 0) or scaled
    // register offset the constructors make sure than both shift_imm_
    // and shift_op_ are initialized.
    DCHECK(!x.rm_.is(pc));
    instr |= B25 | x.shift_imm_*B7 | x.shift_op_ | x.rm_.code();
  }
  DCHECK((am & (P|W)) == P || !x.rn_.is(pc));  // no pc base with writeback
  emit(instr | am | x.rn_.code()*B16 | rd.code()*B12);
}


void Assembler::addrmod3(Instr instr, Register rd, const MemOperand& x) {
  DCHECK((instr & ~(kCondMask | L | S6 | H)) == (B4 | B7));
  DCHECK(x.rn_.is_valid());
  int am = x.am_;
  if (!x.rm_.is_valid()) {
    // Immediate offset.
    int offset_8 = x.offset_;
    if (offset_8 < 0) {
      offset_8 = -offset_8;
      am ^= U;
    }
    if (!is_uint8(offset_8)) {
      // Immediate offset cannot be encoded, load it first to register ip
      // rn (and rd in a load) should never be ip, or will be trashed.
      DCHECK(!x.rn_.is(ip) && ((instr & L) == L || !rd.is(ip)));
      mov(ip, Operand(x.offset_), LeaveCC, Instruction::ConditionField(instr));
      addrmod3(instr, rd, MemOperand(x.rn_, ip, x.am_));
      return;
    }
    DCHECK(offset_8 >= 0);  // no masking needed
    instr |= B | (offset_8 >> 4)*B8 | (offset_8 & 0xf);
  } else if (x.shift_imm_ != 0) {
    // Scaled register offset not supported, load index first
    // rn (and rd in a load) should never be ip, or will be trashed.
    DCHECK(!x.rn_.is(ip) && ((instr & L) == L || !rd.is(ip)));
    mov(ip, Operand(x.rm_, x.shift_op_, x.shift_imm_), LeaveCC,
        Instruction::ConditionField(instr));
    addrmod3(instr, rd, MemOperand(x.rn_, ip, x.am_));
    return;
  } else {
    // Register offset.
    DCHECK((am & (P|W)) == P || !x.rm_.is(pc));  // no pc index with writeback
    instr |= x.rm_.code();
  }
  DCHECK((am & (P|W)) == P || !x.rn_.is(pc));  // no pc base with writeback
  emit(instr | am | x.rn_.code()*B16 | rd.code()*B12);
}


void Assembler::addrmod4(Instr instr, Register rn, RegList rl) {
  DCHECK((instr & ~(kCondMask | P | U | W | L)) == B27);
  DCHECK(rl != 0);
  DCHECK(!rn.is(pc));
  emit(instr | rn.code()*B16 | rl);
}


void Assembler::addrmod5(Instr instr, CRegister crd, const MemOperand& x) {
  // Unindexed addressing is not encoded by this function.
  DCHECK_EQ((B27 | B26),
            (instr & ~(kCondMask | kCoprocessorMask | P | U | N | W | L)));
  DCHECK(x.rn_.is_valid() && !x.rm_.is_valid());
  int am = x.am_;
  int offset_8 = x.offset_;
  DCHECK((offset_8 & 3) == 0);  // offset must be an aligned word offset
  offset_8 >>= 2;
  if (offset_8 < 0) {
    offset_8 = -offset_8;
    am ^= U;
  }
  DCHECK(is_uint8(offset_8));  // unsigned word offset must fit in a byte
  DCHECK((am & (P|W)) == P || !x.rn_.is(pc));  // no pc base with writeback

  // Post-indexed addressing requires W == 1; different than in addrmod2/3.
  if ((am & P) == 0)
    am |= W;

  DCHECK(offset_8 >= 0);  // no masking needed
  emit(instr | am | x.rn_.code()*B16 | crd.code()*B12 | offset_8);
}


int Assembler::branch_offset(Label* L) {
  int target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      // Point to previous instruction that uses the link.
      target_pos = L->pos();
    } else {
      // First entry of the link chain points to itself.
      target_pos = pc_offset();
    }
    L->link_to(pc_offset());
  }

  // Block the emission of the constant pool, since the branch instruction must
  // be emitted at the pc offset recorded by the label.
  if (!is_const_pool_blocked()) BlockConstPoolFor(1);

  return target_pos - (pc_offset() + kPcLoadDelta);
}


// Branch instructions.
void Assembler::b(int branch_offset, Condition cond) {
  DCHECK((branch_offset & 3) == 0);
  int imm24 = branch_offset >> 2;
  CHECK(is_int24(imm24));
  emit(cond | B27 | B25 | (imm24 & kImm24Mask));

  if (cond == al) {
    // Dead code is a good location to emit the constant pool.
    CheckConstPool(false, false);
  }
}


void Assembler::bl(int branch_offset, Condition cond) {
  DCHECK((branch_offset & 3) == 0);
  int imm24 = branch_offset >> 2;
  CHECK(is_int24(imm24));
  emit(cond | B27 | B25 | B24 | (imm24 & kImm24Mask));
}

void Assembler::blx(int branch_offset) {
  DCHECK((branch_offset & 1) == 0);
  int h = ((branch_offset & 2) >> 1)*B24;
  int imm24 = branch_offset >> 2;
  CHECK(is_int24(imm24));
  emit(kSpecialCondition | B27 | B25 | h | (imm24 & kImm24Mask));
}

void Assembler::blx(Register target, Condition cond) {
  DCHECK(!target.is(pc));
  emit(cond | B24 | B21 | 15*B16 | 15*B12 | 15*B8 | BLX | target.code());
}

void Assembler::bx(Register target, Condition cond) {
  DCHECK(!target.is(pc));  // use of pc is actually allowed, but discouraged
  emit(cond | B24 | B21 | 15*B16 | 15*B12 | 15*B8 | BX | target.code());
}


void Assembler::b(Label* L, Condition cond) {
  CheckBuffer();
  b(branch_offset(L), cond);
}


void Assembler::bl(Label* L, Condition cond) {
  CheckBuffer();
  bl(branch_offset(L), cond);
}


void Assembler::blx(Label* L) {
  CheckBuffer();
  blx(branch_offset(L));
}


// Data-processing instructions.

void Assembler::and_(Register dst, Register src1, const Operand& src2,
                     SBit s, Condition cond) {
  addrmod1(cond | AND | s, src1, dst, src2);
}


void Assembler::eor(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | EOR | s, src1, dst, src2);
}


void Assembler::sub(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | SUB | s, src1, dst, src2);
}


void Assembler::rsb(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | RSB | s, src1, dst, src2);
}


void Assembler::add(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | ADD | s, src1, dst, src2);
}


void Assembler::adc(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | ADC | s, src1, dst, src2);
}


void Assembler::sbc(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | SBC | s, src1, dst, src2);
}


void Assembler::rsc(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | RSC | s, src1, dst, src2);
}


void Assembler::tst(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | TST | S, src1, r0, src2);
}


void Assembler::teq(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | TEQ | S, src1, r0, src2);
}


void Assembler::cmp(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | CMP | S, src1, r0, src2);
}


void Assembler::cmp_raw_immediate(
    Register src, int raw_immediate, Condition cond) {
  DCHECK(is_uint12(raw_immediate));
  emit(cond | I | CMP | S | src.code() << 16 | raw_immediate);
}


void Assembler::cmn(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | CMN | S, src1, r0, src2);
}


void Assembler::orr(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | ORR | s, src1, dst, src2);
}


void Assembler::mov(Register dst, const Operand& src, SBit s, Condition cond) {
  // Don't allow nop instructions in the form mov rn, rn to be generated using
  // the mov instruction. They must be generated using nop(int/NopMarkerTypes)
  // or MarkCode(int/NopMarkerTypes) pseudo instructions.
  DCHECK(!(src.is_reg() && src.rm().is(dst) && s == LeaveCC && cond == al));
  addrmod1(cond | MOV | s, r0, dst, src);
}


void Assembler::mov_label_offset(Register dst, Label* label) {
  if (label->is_bound()) {
    mov(dst, Operand(label->pos() + (Code::kHeaderSize - kHeapObjectTag)));
  } else {
    // Emit the link to the label in the code stream followed by extra nop
    // instructions.
    // If the label is not linked, then start a new link chain by linking it to
    // itself, emitting pc_offset().
    int link = label->is_linked() ? label->pos() : pc_offset();
    label->link_to(pc_offset());

    // When the label is bound, these instructions will be patched with a
    // sequence of movw/movt or mov/orr/orr instructions. They will load the
    // destination register with the position of the label from the beginning
    // of the code.
    //
    // The link will be extracted from the first instruction and the destination
    // register from the second.
    //   For ARMv7:
    //      link
    //      mov dst, dst
    //   For ARMv6:
    //      link
    //      mov dst, dst
    //      mov dst, dst
    //
    // When the label gets bound: target_at extracts the link and target_at_put
    // patches the instructions.
    CHECK(is_uint24(link));
    BlockConstPoolScope block_const_pool(this);
    emit(link);
    nop(dst.code());
    if (!CpuFeatures::IsSupported(ARMv7)) {
      nop(dst.code());
    }
  }
}


void Assembler::movw(Register reg, uint32_t immediate, Condition cond) {
  DCHECK(IsEnabled(ARMv7));
  emit(cond | 0x30*B20 | reg.code()*B12 | EncodeMovwImmediate(immediate));
}


void Assembler::movt(Register reg, uint32_t immediate, Condition cond) {
  DCHECK(IsEnabled(ARMv7));
  emit(cond | 0x34*B20 | reg.code()*B12 | EncodeMovwImmediate(immediate));
}


void Assembler::bic(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | BIC | s, src1, dst, src2);
}


void Assembler::mvn(Register dst, const Operand& src, SBit s, Condition cond) {
  addrmod1(cond | MVN | s, r0, dst, src);
}


// Multiply instructions.
void Assembler::mla(Register dst, Register src1, Register src2, Register srcA,
                    SBit s, Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc) && !srcA.is(pc));
  emit(cond | A | s | dst.code()*B16 | srcA.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::mls(Register dst, Register src1, Register src2, Register srcA,
                    Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc) && !srcA.is(pc));
  DCHECK(IsEnabled(ARMv7));
  emit(cond | B22 | B21 | dst.code()*B16 | srcA.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::sdiv(Register dst, Register src1, Register src2,
                     Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc));
  DCHECK(IsEnabled(SUDIV));
  emit(cond | B26 | B25| B24 | B20 | dst.code()*B16 | 0xf * B12 |
       src2.code()*B8 | B4 | src1.code());
}


void Assembler::udiv(Register dst, Register src1, Register src2,
                     Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc));
  DCHECK(IsEnabled(SUDIV));
  emit(cond | B26 | B25 | B24 | B21 | B20 | dst.code() * B16 | 0xf * B12 |
       src2.code() * B8 | B4 | src1.code());
}


void Assembler::mul(Register dst, Register src1, Register src2, SBit s,
                    Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc));
  // dst goes in bits 16-19 for this instruction!
  emit(cond | s | dst.code() * B16 | src2.code() * B8 | B7 | B4 | src1.code());
}


void Assembler::smmla(Register dst, Register src1, Register src2, Register srcA,
                      Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc) && !srcA.is(pc));
  emit(cond | B26 | B25 | B24 | B22 | B20 | dst.code() * B16 |
       srcA.code() * B12 | src2.code() * B8 | B4 | src1.code());
}


void Assembler::smmul(Register dst, Register src1, Register src2,
                      Condition cond) {
  DCHECK(!dst.is(pc) && !src1.is(pc) && !src2.is(pc));
  emit(cond | B26 | B25 | B24 | B22 | B20 | dst.code() * B16 | 0xf * B12 |
       src2.code() * B8 | B4 | src1.code());
}


void Assembler::smlal(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  DCHECK(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  DCHECK(!dstL.is(dstH));
  emit(cond | B23 | B22 | A | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::smull(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  DCHECK(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  DCHECK(!dstL.is(dstH));
  emit(cond | B23 | B22 | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::umlal(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  DCHECK(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  DCHECK(!dstL.is(dstH));
  emit(cond | B23 | A | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::umull(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  DCHECK(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  DCHECK(!dstL.is(dstH));
  emit(cond | B23 | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


// Miscellaneous arithmetic instructions.
void Assembler::clz(Register dst, Register src, Condition cond) {
  DCHECK(!dst.is(pc) && !src.is(pc));
  emit(cond | B24 | B22 | B21 | 15*B16 | dst.code()*B12 |
       15*B8 | CLZ | src.code());
}


// Saturating instructions.

// Unsigned saturate.
void Assembler::usat(Register dst,
                     int satpos,
                     const Operand& src,
                     Condition cond) {
  DCHECK(!dst.is(pc) && !src.rm_.is(pc));
  DCHECK((satpos >= 0) && (satpos <= 31));
  DCHECK((src.shift_op_ == ASR) || (src.shift_op_ == LSL));
  DCHECK(src.rs_.is(no_reg));

  int sh = 0;
  if (src.shift_op_ == ASR) {
      sh = 1;
  }

  emit(cond | 0x6*B24 | 0xe*B20 | satpos*B16 | dst.code()*B12 |
       src.shift_imm_*B7 | sh*B6 | 0x1*B4 | src.rm_.code());
}


// Bitfield manipulation instructions.

// Unsigned bit field extract.
// Extracts #width adjacent bits from position #lsb in a register, and
// writes them to the low bits of a destination register.
//   ubfx dst, src, #lsb, #width
void Assembler::ubfx(Register dst,
                     Register src,
                     int lsb,
                     int width,
                     Condition cond) {
  DCHECK(IsEnabled(ARMv7));
  DCHECK(!dst.is(pc) && !src.is(pc));
  DCHECK((lsb >= 0) && (lsb <= 31));
  DCHECK((width >= 1) && (width <= (32 - lsb)));
  emit(cond | 0xf*B23 | B22 | B21 | (width - 1)*B16 | dst.code()*B12 |
       lsb*B7 | B6 | B4 | src.code());
}


// Signed bit field extract.
// Extracts #width adjacent bits from position #lsb in a register, and
// writes them to the low bits of a destination register. The extracted
// value is sign extended to fill the destination register.
//   sbfx dst, src, #lsb, #width
void Assembler::sbfx(Register dst,
                     Register src,
                     int lsb,
                     int width,
                     Condition cond) {
  DCHECK(IsEnabled(ARMv7));
  DCHECK(!dst.is(pc) && !src.is(pc));
  DCHECK((lsb >= 0) && (lsb <= 31));
  DCHECK((width >= 1) && (width <= (32 - lsb)));
  emit(cond | 0xf*B23 | B21 | (width - 1)*B16 | dst.code()*B12 |
       lsb*B7 | B6 | B4 | src.code());
}


// Bit field clear.
// Sets #width adjacent bits at position #lsb in the destination register
// to zero, preserving the value of the other bits.
//   bfc dst, #lsb, #width
void Assembler::bfc(Register dst, int lsb, int width, Condition cond) {
  DCHECK(IsEnabled(ARMv7));
  DCHECK(!dst.is(pc));
  DCHECK((lsb >= 0) && (lsb <= 31));
  DCHECK((width >= 1) && (width <= (32 - lsb)));
  int msb = lsb + width - 1;
  emit(cond | 0x1f*B22 | msb*B16 | dst.code()*B12 | lsb*B7 | B4 | 0xf);
}


// Bit field insert.
// Inserts #width adjacent bits from the low bits of the source register
// into position #lsb of the destination register.
//   bfi dst, src, #lsb, #width
void Assembler::bfi(Register dst,
                    Register src,
                    int lsb,
                    int width,
                    Condition cond) {
  DCHECK(IsEnabled(ARMv7));
  DCHECK(!dst.is(pc) && !src.is(pc));
  DCHECK((lsb >= 0) && (lsb <= 31));
  DCHECK((width >= 1) && (width <= (32 - lsb)));
  int msb = lsb + width - 1;
  emit(cond | 0x1f*B22 | msb*B16 | dst.code()*B12 | lsb*B7 | B4 |
       src.code());
}


void Assembler::pkhbt(Register dst,
                      Register src1,
                      const Operand& src2,
                      Condition cond ) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.125.
  // cond(31-28) | 01101000(27-20) | Rn(19-16) |
  // Rd(15-12) | imm5(11-7) | 0(6) | 01(5-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src1.is(pc));
  DCHECK(!src2.rm().is(pc));
  DCHECK(!src2.rm().is(no_reg));
  DCHECK(src2.rs().is(no_reg));
  DCHECK((src2.shift_imm_ >= 0) && (src2.shift_imm_ <= 31));
  DCHECK(src2.shift_op() == LSL);
  emit(cond | 0x68*B20 | src1.code()*B16 | dst.code()*B12 |
       src2.shift_imm_*B7 | B4 | src2.rm().code());
}


void Assembler::pkhtb(Register dst,
                      Register src1,
                      const Operand& src2,
                      Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.125.
  // cond(31-28) | 01101000(27-20) | Rn(19-16) |
  // Rd(15-12) | imm5(11-7) | 1(6) | 01(5-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src1.is(pc));
  DCHECK(!src2.rm().is(pc));
  DCHECK(!src2.rm().is(no_reg));
  DCHECK(src2.rs().is(no_reg));
  DCHECK((src2.shift_imm_ >= 1) && (src2.shift_imm_ <= 32));
  DCHECK(src2.shift_op() == ASR);
  int asr = (src2.shift_imm_ == 32) ? 0 : src2.shift_imm_;
  emit(cond | 0x68*B20 | src1.code()*B16 | dst.code()*B12 |
       asr*B7 | B6 | B4 | src2.rm().code());
}


void Assembler::sxtb(Register dst, Register src, int rotate, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.233.
  // cond(31-28) | 01101010(27-20) | 1111(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6A * B20 | 0xF * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src.code());
}


void Assembler::sxtab(Register dst, Register src1, Register src2, int rotate,
                      Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.233.
  // cond(31-28) | 01101010(27-20) | Rn(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src1.is(pc));
  DCHECK(!src2.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6A * B20 | src1.code() * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src2.code());
}


void Assembler::sxth(Register dst, Register src, int rotate, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.235.
  // cond(31-28) | 01101011(27-20) | 1111(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6B * B20 | 0xF * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src.code());
}


void Assembler::sxtah(Register dst, Register src1, Register src2, int rotate,
                      Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.235.
  // cond(31-28) | 01101011(27-20) | Rn(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src1.is(pc));
  DCHECK(!src2.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6B * B20 | src1.code() * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src2.code());
}


void Assembler::uxtb(Register dst, Register src, int rotate, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.274.
  // cond(31-28) | 01101110(27-20) | 1111(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6E * B20 | 0xF * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src.code());
}


void Assembler::uxtab(Register dst, Register src1, Register src2, int rotate,
                      Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.271.
  // cond(31-28) | 01101110(27-20) | Rn(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src1.is(pc));
  DCHECK(!src2.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6E * B20 | src1.code() * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src2.code());
}


void Assembler::uxtb16(Register dst, Register src, int rotate, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.275.
  // cond(31-28) | 01101100(27-20) | 1111(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6C * B20 | 0xF * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src.code());
}


void Assembler::uxth(Register dst, Register src, int rotate, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.276.
  // cond(31-28) | 01101111(27-20) | 1111(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6F * B20 | 0xF * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src.code());
}


void Assembler::uxtah(Register dst, Register src1, Register src2, int rotate,
                      Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.273.
  // cond(31-28) | 01101111(27-20) | Rn(19-16) |
  // Rd(15-12) | rotate(11-10) | 00(9-8)| 0111(7-4) | Rm(3-0)
  DCHECK(!dst.is(pc));
  DCHECK(!src1.is(pc));
  DCHECK(!src2.is(pc));
  DCHECK(rotate == 0 || rotate == 8 || rotate == 16 || rotate == 24);
  emit(cond | 0x6F * B20 | src1.code() * B16 | dst.code() * B12 |
       ((rotate >> 1) & 0xC) * B8 | 7 * B4 | src2.code());
}


void Assembler::rbit(Register dst, Register src, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.144.
  // cond(31-28) | 011011111111(27-16) | Rd(15-12) | 11110011(11-4) | Rm(3-0)
  DCHECK(IsEnabled(ARMv7));
  DCHECK(!dst.is(pc));
  DCHECK(!src.is(pc));
  emit(cond | 0x6FF * B16 | dst.code() * B12 | 0xF3 * B4 | src.code());
}


// Status register access instructions.
void Assembler::mrs(Register dst, SRegister s, Condition cond) {
  DCHECK(!dst.is(pc));
  emit(cond | B24 | s | 15*B16 | dst.code()*B12);
}


void Assembler::msr(SRegisterFieldMask fields, const Operand& src,
                    Condition cond) {
  DCHECK((fields & 0x000f0000) != 0);  // At least one field must be set.
  DCHECK(((fields & 0xfff0ffff) == CPSR) || ((fields & 0xfff0ffff) == SPSR));
  Instr instr;
  if (!src.rm_.is_valid()) {
    // Immediate.
    uint32_t rotate_imm;
    uint32_t immed_8;
    if (src.must_output_reloc_info(this) ||
        !fits_shifter(src.imm32_, &rotate_imm, &immed_8, NULL)) {
      // Immediate operand cannot be encoded, load it first to register ip.
      move_32_bit_immediate(ip, src);
      msr(fields, Operand(ip), cond);
      return;
    }
    instr = I | rotate_imm*B8 | immed_8;
  } else {
    DCHECK(!src.rs_.is_valid() && src.shift_imm_ == 0);  // only rm allowed
    instr = src.rm_.code();
  }
  emit(cond | instr | B24 | B21 | fields | 15*B12);
}


// Load/Store instructions.
void Assembler::ldr(Register dst, const MemOperand& src, Condition cond) {
  addrmod2(cond | B26 | L, dst, src);
}


void Assembler::str(Register src, const MemOperand& dst, Condition cond) {
  addrmod2(cond | B26, src, dst);
}


void Assembler::ldrb(Register dst, const MemOperand& src, Condition cond) {
  addrmod2(cond | B26 | B | L, dst, src);
}


void Assembler::strb(Register src, const MemOperand& dst, Condition cond) {
  addrmod2(cond | B26 | B, src, dst);
}


void Assembler::ldrh(Register dst, const MemOperand& src, Condition cond) {
  addrmod3(cond | L | B7 | H | B4, dst, src);
}


void Assembler::strh(Register src, const MemOperand& dst, Condition cond) {
  addrmod3(cond | B7 | H | B4, src, dst);
}


void Assembler::ldrsb(Register dst, const MemOperand& src, Condition cond) {
  addrmod3(cond | L | B7 | S6 | B4, dst, src);
}


void Assembler::ldrsh(Register dst, const MemOperand& src, Condition cond) {
  addrmod3(cond | L | B7 | S6 | H | B4, dst, src);
}


void Assembler::ldrd(Register dst1, Register dst2,
                     const MemOperand& src, Condition cond) {
  DCHECK(src.rm().is(no_reg));
  DCHECK(!dst1.is(lr));  // r14.
  DCHECK_EQ(0, dst1.code() % 2);
  DCHECK_EQ(dst1.code() + 1, dst2.code());
  addrmod3(cond | B7 | B6 | B4, dst1, src);
}


void Assembler::strd(Register src1, Register src2,
                     const MemOperand& dst, Condition cond) {
  DCHECK(dst.rm().is(no_reg));
  DCHECK(!src1.is(lr));  // r14.
  DCHECK_EQ(0, src1.code() % 2);
  DCHECK_EQ(src1.code() + 1, src2.code());
  addrmod3(cond | B7 | B6 | B5 | B4, src1, dst);
}

// Load/Store exclusive instructions.
void Assembler::ldrex(Register dst, Register src, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.75.
  // cond(31-28) | 00011001(27-20) | Rn(19-16) | Rt(15-12) | 111110011111(11-0)
  emit(cond | B24 | B23 | B20 | src.code() * B16 | dst.code() * B12 | 0xf9f);
}

void Assembler::strex(Register src1, Register src2, Register dst,
                      Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.212.
  // cond(31-28) | 00011000(27-20) | Rn(19-16) | Rd(15-12) | 11111001(11-4) |
  // Rt(3-0)
  emit(cond | B24 | B23 | dst.code() * B16 | src1.code() * B12 | 0xf9 * B4 |
       src2.code());
}

void Assembler::ldrexb(Register dst, Register src, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.76.
  // cond(31-28) | 00011101(27-20) | Rn(19-16) | Rt(15-12) | 111110011111(11-0)
  emit(cond | B24 | B23 | B22 | B20 | src.code() * B16 | dst.code() * B12 |
       0xf9f);
}

void Assembler::strexb(Register src1, Register src2, Register dst,
                       Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.213.
  // cond(31-28) | 00011100(27-20) | Rn(19-16) | Rd(15-12) | 11111001(11-4) |
  // Rt(3-0)
  emit(cond | B24 | B23 | B22 | dst.code() * B16 | src1.code() * B12 |
       0xf9 * B4 | src2.code());
}

void Assembler::ldrexh(Register dst, Register src, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.78.
  // cond(31-28) | 00011111(27-20) | Rn(19-16) | Rt(15-12) | 111110011111(11-0)
  emit(cond | B24 | B23 | B22 | B21 | B20 | src.code() * B16 |
       dst.code() * B12 | 0xf9f);
}

void Assembler::strexh(Register src1, Register src2, Register dst,
                       Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.215.
  // cond(31-28) | 00011110(27-20) | Rn(19-16) | Rd(15-12) | 11111001(11-4) |
  // Rt(3-0)
  emit(cond | B24 | B23 | B22 | B21 | dst.code() * B16 | src1.code() * B12 |
       0xf9 * B4 | src2.code());
}

// Preload instructions.
void Assembler::pld(const MemOperand& address) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.128.
  // 1111(31-28) | 0111(27-24) | U(23) | R(22) | 01(21-20) | Rn(19-16) |
  // 1111(15-12) | imm5(11-07) | type(6-5) | 0(4)| Rm(3-0) |
  DCHECK(address.rm().is(no_reg));
  DCHECK(address.am() == Offset);
  int U = B23;
  int offset = address.offset();
  if (offset < 0) {
    offset = -offset;
    U = 0;
  }
  DCHECK(offset < 4096);
  emit(kSpecialCondition | B26 | B24 | U | B22 | B20 | address.rn().code()*B16 |
       0xf*B12 | offset);
}


// Load/Store multiple instructions.
void Assembler::ldm(BlockAddrMode am,
                    Register base,
                    RegList dst,
                    Condition cond) {
  // ABI stack constraint: ldmxx base, {..sp..}  base != sp  is not restartable.
  DCHECK(base.is(sp) || (dst & sp.bit()) == 0);

  addrmod4(cond | B27 | am | L, base, dst);

  // Emit the constant pool after a function return implemented by ldm ..{..pc}.
  if (cond == al && (dst & pc.bit()) != 0) {
    // There is a slight chance that the ldm instruction was actually a call,
    // in which case it would be wrong to return into the constant pool; we
    // recognize this case by checking if the emission of the pool was blocked
    // at the pc of the ldm instruction by a mov lr, pc instruction; if this is
    // the case, we emit a jump over the pool.
    CheckConstPool(true, no_const_pool_before_ == pc_offset() - kInstrSize);
  }
}


void Assembler::stm(BlockAddrMode am,
                    Register base,
                    RegList src,
                    Condition cond) {
  addrmod4(cond | B27 | am, base, src);
}


// Exception-generating instructions and debugging support.
// Stops with a non-negative code less than kNumOfWatchedStops support
// enabling/disabling and a counter feature. See simulator-arm.h .
void Assembler::stop(const char* msg, Condition cond, int32_t code) {
#ifndef __arm__
  DCHECK(code >= kDefaultStopCode);
  {
    // The Simulator will handle the stop instruction and get the message
    // address. It expects to find the address just after the svc instruction.
    BlockConstPoolScope block_const_pool(this);
    if (code >= 0) {
      svc(kStopCode + code, cond);
    } else {
      svc(kStopCode + kMaxStopCode, cond);
    }
    // Do not embed the message string address! We used to do this, but that
    // made snapshots created from position-independent executable builds
    // non-deterministic.
    // TODO(yangguo): remove this field entirely.
    nop();
  }
#else  // def __arm__
  if (cond != al) {
    Label skip;
    b(&skip, NegateCondition(cond));
    bkpt(0);
    bind(&skip);
  } else {
    bkpt(0);
  }
#endif  // def __arm__
}

void Assembler::bkpt(uint32_t imm16) {
  DCHECK(is_uint16(imm16));
  emit(al | B24 | B21 | (imm16 >> 4)*B8 | BKPT | (imm16 & 0xf));
}


void Assembler::svc(uint32_t imm24, Condition cond) {
  DCHECK(is_uint24(imm24));
  emit(cond | 15*B24 | imm24);
}


void Assembler::dmb(BarrierOption option) {
  if (CpuFeatures::IsSupported(ARMv7)) {
    // Details available in ARM DDI 0406C.b, A8-378.
    emit(kSpecialCondition | 0x57ff * B12 | 5 * B4 | option);
  } else {
    // Details available in ARM DDI 0406C.b, B3-1750.
    // CP15DMB: CRn=c7, opc1=0, CRm=c10, opc2=5, Rt is ignored.
    mcr(p15, 0, r0, cr7, cr10, 5);
  }
}


void Assembler::dsb(BarrierOption option) {
  if (CpuFeatures::IsSupported(ARMv7)) {
    // Details available in ARM DDI 0406C.b, A8-380.
    emit(kSpecialCondition | 0x57ff * B12 | 4 * B4 | option);
  } else {
    // Details available in ARM DDI 0406C.b, B3-1750.
    // CP15DSB: CRn=c7, opc1=0, CRm=c10, opc2=4, Rt is ignored.
    mcr(p15, 0, r0, cr7, cr10, 4);
  }
}


void Assembler::isb(BarrierOption option) {
  if (CpuFeatures::IsSupported(ARMv7)) {
    // Details available in ARM DDI 0406C.b, A8-389.
    emit(kSpecialCondition | 0x57ff * B12 | 6 * B4 | option);
  } else {
    // Details available in ARM DDI 0406C.b, B3-1750.
    // CP15ISB: CRn=c7, opc1=0, CRm=c5, opc2=4, Rt is ignored.
    mcr(p15, 0, r0, cr7, cr5, 4);
  }
}


// Coprocessor instructions.
void Assembler::cdp(Coprocessor coproc,
                    int opcode_1,
                    CRegister crd,
                    CRegister crn,
                    CRegister crm,
                    int opcode_2,
                    Condition cond) {
  DCHECK(is_uint4(opcode_1) && is_uint3(opcode_2));
  emit(cond | B27 | B26 | B25 | (opcode_1 & 15)*B20 | crn.code()*B16 |
       crd.code()*B12 | coproc*B8 | (opcode_2 & 7)*B5 | crm.code());
}

void Assembler::cdp2(Coprocessor coproc, int opcode_1, CRegister crd,
                     CRegister crn, CRegister crm, int opcode_2) {
  cdp(coproc, opcode_1, crd, crn, crm, opcode_2, kSpecialCondition);
}


void Assembler::mcr(Coprocessor coproc,
                    int opcode_1,
                    Register rd,
                    CRegister crn,
                    CRegister crm,
                    int opcode_2,
                    Condition cond) {
  DCHECK(is_uint3(opcode_1) && is_uint3(opcode_2));
  emit(cond | B27 | B26 | B25 | (opcode_1 & 7)*B21 | crn.code()*B16 |
       rd.code()*B12 | coproc*B8 | (opcode_2 & 7)*B5 | B4 | crm.code());
}

void Assembler::mcr2(Coprocessor coproc, int opcode_1, Register rd,
                     CRegister crn, CRegister crm, int opcode_2) {
  mcr(coproc, opcode_1, rd, crn, crm, opcode_2, kSpecialCondition);
}


void Assembler::mrc(Coprocessor coproc,
                    int opcode_1,
                    Register rd,
                    CRegister crn,
                    CRegister crm,
                    int opcode_2,
                    Condition cond) {
  DCHECK(is_uint3(opcode_1) && is_uint3(opcode_2));
  emit(cond | B27 | B26 | B25 | (opcode_1 & 7)*B21 | L | crn.code()*B16 |
       rd.code()*B12 | coproc*B8 | (opcode_2 & 7)*B5 | B4 | crm.code());
}

void Assembler::mrc2(Coprocessor coproc, int opcode_1, Register rd,
                     CRegister crn, CRegister crm, int opcode_2) {
  mrc(coproc, opcode_1, rd, crn, crm, opcode_2, kSpecialCondition);
}


void Assembler::ldc(Coprocessor coproc,
                    CRegister crd,
                    const MemOperand& src,
                    LFlag l,
                    Condition cond) {
  addrmod5(cond | B27 | B26 | l | L | coproc*B8, crd, src);
}


void Assembler::ldc(Coprocessor coproc,
                    CRegister crd,
                    Register rn,
                    int option,
                    LFlag l,
                    Condition cond) {
  // Unindexed addressing.
  DCHECK(is_uint8(option));
  emit(cond | B27 | B26 | U | l | L | rn.code()*B16 | crd.code()*B12 |
       coproc*B8 | (option & 255));
}

void Assembler::ldc2(Coprocessor coproc, CRegister crd, const MemOperand& src,
                     LFlag l) {
  ldc(coproc, crd, src, l, kSpecialCondition);
}

void Assembler::ldc2(Coprocessor coproc, CRegister crd, Register rn, int option,
                     LFlag l) {
  ldc(coproc, crd, rn, option, l, kSpecialCondition);
}


// Support for VFP.

void Assembler::vldr(const DwVfpRegister dst,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // Ddst = MEM(Rbase + offset).
  // Instruction details available in ARM DDI 0406C.b, A8-924.
  // cond(31-28) | 1101(27-24)| U(23) | D(22) | 01(21-20) | Rbase(19-16) |
  // Vd(15-12) | 1011(11-8) | offset
  DCHECK(VfpRegisterIsAvailable(dst));
  int u = 1;
  if (offset < 0) {
    CHECK(offset != kMinInt);
    offset = -offset;
    u = 0;
  }
  int vd, d;
  dst.split_code(&vd, &d);

  DCHECK(offset >= 0);
  if ((offset % 4) == 0 && (offset / 4) < 256) {
    emit(cond | 0xD*B24 | u*B23 | d*B22 | B20 | base.code()*B16 | vd*B12 |
         0xB*B8 | ((offset / 4) & 255));
  } else {
    // Larger offsets must be handled by computing the correct address
    // in the ip register.
    DCHECK(!base.is(ip));
    if (u == 1) {
      add(ip, base, Operand(offset));
    } else {
      sub(ip, base, Operand(offset));
    }
    emit(cond | 0xD*B24 | d*B22 | B20 | ip.code()*B16 | vd*B12 | 0xB*B8);
  }
}


void Assembler::vldr(const DwVfpRegister dst,
                     const MemOperand& operand,
                     const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(operand.am_ == Offset);
  if (operand.rm().is_valid()) {
    add(ip, operand.rn(),
        Operand(operand.rm(), operand.shift_op_, operand.shift_imm_));
    vldr(dst, ip, 0, cond);
  } else {
    vldr(dst, operand.rn(), operand.offset(), cond);
  }
}


void Assembler::vldr(const SwVfpRegister dst,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // Sdst = MEM(Rbase + offset).
  // Instruction details available in ARM DDI 0406A, A8-628.
  // cond(31-28) | 1101(27-24)| U001(23-20) | Rbase(19-16) |
  // Vdst(15-12) | 1010(11-8) | offset
  int u = 1;
  if (offset < 0) {
    offset = -offset;
    u = 0;
  }
  int sd, d;
  dst.split_code(&sd, &d);
  DCHECK(offset >= 0);

  if ((offset % 4) == 0 && (offset / 4) < 256) {
  emit(cond | u*B23 | d*B22 | 0xD1*B20 | base.code()*B16 | sd*B12 |
       0xA*B8 | ((offset / 4) & 255));
  } else {
    // Larger offsets must be handled by computing the correct address
    // in the ip register.
    DCHECK(!base.is(ip));
    if (u == 1) {
      add(ip, base, Operand(offset));
    } else {
      sub(ip, base, Operand(offset));
    }
    emit(cond | d*B22 | 0xD1*B20 | ip.code()*B16 | sd*B12 | 0xA*B8);
  }
}


void Assembler::vldr(const SwVfpRegister dst,
                     const MemOperand& operand,
                     const Condition cond) {
  DCHECK(operand.am_ == Offset);
  if (operand.rm().is_valid()) {
    add(ip, operand.rn(),
        Operand(operand.rm(), operand.shift_op_, operand.shift_imm_));
    vldr(dst, ip, 0, cond);
  } else {
    vldr(dst, operand.rn(), operand.offset(), cond);
  }
}


void Assembler::vstr(const DwVfpRegister src,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // MEM(Rbase + offset) = Dsrc.
  // Instruction details available in ARM DDI 0406C.b, A8-1082.
  // cond(31-28) | 1101(27-24)| U(23) | D(22) | 00(21-20) | Rbase(19-16) |
  // Vd(15-12) | 1011(11-8) | (offset/4)
  DCHECK(VfpRegisterIsAvailable(src));
  int u = 1;
  if (offset < 0) {
    CHECK(offset != kMinInt);
    offset = -offset;
    u = 0;
  }
  DCHECK(offset >= 0);
  int vd, d;
  src.split_code(&vd, &d);

  if ((offset % 4) == 0 && (offset / 4) < 256) {
    emit(cond | 0xD*B24 | u*B23 | d*B22 | base.code()*B16 | vd*B12 | 0xB*B8 |
         ((offset / 4) & 255));
  } else {
    // Larger offsets must be handled by computing the correct address
    // in the ip register.
    DCHECK(!base.is(ip));
    if (u == 1) {
      add(ip, base, Operand(offset));
    } else {
      sub(ip, base, Operand(offset));
    }
    emit(cond | 0xD*B24 | d*B22 | ip.code()*B16 | vd*B12 | 0xB*B8);
  }
}


void Assembler::vstr(const DwVfpRegister src,
                     const MemOperand& operand,
                     const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(src));
  DCHECK(operand.am_ == Offset);
  if (operand.rm().is_valid()) {
    add(ip, operand.rn(),
        Operand(operand.rm(), operand.shift_op_, operand.shift_imm_));
    vstr(src, ip, 0, cond);
  } else {
    vstr(src, operand.rn(), operand.offset(), cond);
  }
}


void Assembler::vstr(const SwVfpRegister src,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // MEM(Rbase + offset) = SSrc.
  // Instruction details available in ARM DDI 0406A, A8-786.
  // cond(31-28) | 1101(27-24)| U000(23-20) | Rbase(19-16) |
  // Vdst(15-12) | 1010(11-8) | (offset/4)
  int u = 1;
  if (offset < 0) {
    CHECK(offset != kMinInt);
    offset = -offset;
    u = 0;
  }
  int sd, d;
  src.split_code(&sd, &d);
  DCHECK(offset >= 0);
  if ((offset % 4) == 0 && (offset / 4) < 256) {
    emit(cond | u*B23 | d*B22 | 0xD0*B20 | base.code()*B16 | sd*B12 |
         0xA*B8 | ((offset / 4) & 255));
  } else {
    // Larger offsets must be handled by computing the correct address
    // in the ip register.
    DCHECK(!base.is(ip));
    if (u == 1) {
      add(ip, base, Operand(offset));
    } else {
      sub(ip, base, Operand(offset));
    }
    emit(cond | d*B22 | 0xD0*B20 | ip.code()*B16 | sd*B12 | 0xA*B8);
  }
}


void Assembler::vstr(const SwVfpRegister src,
                     const MemOperand& operand,
                     const Condition cond) {
  DCHECK(operand.am_ == Offset);
  if (operand.rm().is_valid()) {
    add(ip, operand.rn(),
        Operand(operand.rm(), operand.shift_op_, operand.shift_imm_));
    vstr(src, ip, 0, cond);
  } else {
    vstr(src, operand.rn(), operand.offset(), cond);
  }
}

void Assembler::vldm(BlockAddrMode am, Register base, DwVfpRegister first,
                     DwVfpRegister last, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-922.
  // cond(31-28) | 110(27-25)| PUDW1(24-20) | Rbase(19-16) |
  // first(15-12) | 1011(11-8) | (count * 2)
  DCHECK_LE(first.code(), last.code());
  DCHECK(VfpRegisterIsAvailable(last));
  DCHECK(am == ia || am == ia_w || am == db_w);
  DCHECK(!base.is(pc));

  int sd, d;
  first.split_code(&sd, &d);
  int count = last.code() - first.code() + 1;
  DCHECK(count <= 16);
  emit(cond | B27 | B26 | am | d*B22 | B20 | base.code()*B16 | sd*B12 |
       0xB*B8 | count*2);
}

void Assembler::vstm(BlockAddrMode am, Register base, DwVfpRegister first,
                     DwVfpRegister last, Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-1080.
  // cond(31-28) | 110(27-25)| PUDW0(24-20) | Rbase(19-16) |
  // first(15-12) | 1011(11-8) | (count * 2)
  DCHECK_LE(first.code(), last.code());
  DCHECK(VfpRegisterIsAvailable(last));
  DCHECK(am == ia || am == ia_w || am == db_w);
  DCHECK(!base.is(pc));

  int sd, d;
  first.split_code(&sd, &d);
  int count = last.code() - first.code() + 1;
  DCHECK(count <= 16);
  emit(cond | B27 | B26 | am | d*B22 | base.code()*B16 | sd*B12 |
       0xB*B8 | count*2);
}

void Assembler::vldm(BlockAddrMode am, Register base, SwVfpRegister first,
                     SwVfpRegister last, Condition cond) {
  // Instruction details available in ARM DDI 0406A, A8-626.
  // cond(31-28) | 110(27-25)| PUDW1(24-20) | Rbase(19-16) |
  // first(15-12) | 1010(11-8) | (count/2)
  DCHECK_LE(first.code(), last.code());
  DCHECK(am == ia || am == ia_w || am == db_w);
  DCHECK(!base.is(pc));

  int sd, d;
  first.split_code(&sd, &d);
  int count = last.code() - first.code() + 1;
  emit(cond | B27 | B26 | am | d*B22 | B20 | base.code()*B16 | sd*B12 |
       0xA*B8 | count);
}

void Assembler::vstm(BlockAddrMode am, Register base, SwVfpRegister first,
                     SwVfpRegister last, Condition cond) {
  // Instruction details available in ARM DDI 0406A, A8-784.
  // cond(31-28) | 110(27-25)| PUDW0(24-20) | Rbase(19-16) |
  // first(15-12) | 1011(11-8) | (count/2)
  DCHECK_LE(first.code(), last.code());
  DCHECK(am == ia || am == ia_w || am == db_w);
  DCHECK(!base.is(pc));

  int sd, d;
  first.split_code(&sd, &d);
  int count = last.code() - first.code() + 1;
  emit(cond | B27 | B26 | am | d*B22 | base.code()*B16 | sd*B12 |
       0xA*B8 | count);
}


static void DoubleAsTwoUInt32(double d, uint32_t* lo, uint32_t* hi) {
  uint64_t i;
  memcpy(&i, &d, 8);

  *lo = i & 0xffffffff;
  *hi = i >> 32;
}


// Only works for little endian floating point formats.
// We don't support VFP on the mixed endian floating point platform.
static bool FitsVmovFPImmediate(double d, uint32_t* encoding) {
  // VMOV can accept an immediate of the form:
  //
  //  +/- m * 2^(-n) where 16 <= m <= 31 and 0 <= n <= 7
  //
  // The immediate is encoded using an 8-bit quantity, comprised of two
  // 4-bit fields. For an 8-bit immediate of the form:
  //
  //  [abcdefgh]
  //
  // where a is the MSB and h is the LSB, an immediate 64-bit double can be
  // created of the form:
  //
  //  [aBbbbbbb,bbcdefgh,00000000,00000000,
  //      00000000,00000000,00000000,00000000]
  //
  // where B = ~b.
  //

  uint32_t lo, hi;
  DoubleAsTwoUInt32(d, &lo, &hi);

  // The most obvious constraint is the long block of zeroes.
  if ((lo != 0) || ((hi & 0xffff) != 0)) {
    return false;
  }

  // Bits 61:54 must be all clear or all set.
  if (((hi & 0x3fc00000) != 0) && ((hi & 0x3fc00000) != 0x3fc00000)) {
    return false;
  }

  // Bit 62 must be NOT bit 61.
  if (((hi ^ (hi << 1)) & (0x40000000)) == 0) {
    return false;
  }

  // Create the encoded immediate in the form:
  //  [00000000,0000abcd,00000000,0000efgh]
  *encoding  = (hi >> 16) & 0xf;      // Low nybble.
  *encoding |= (hi >> 4) & 0x70000;   // Low three bits of the high nybble.
  *encoding |= (hi >> 12) & 0x80000;  // Top bit of the high nybble.

  return true;
}


void Assembler::vmov(const SwVfpRegister dst, float imm) {
  uint32_t enc;
  if (CpuFeatures::IsSupported(VFPv3) && FitsVmovFPImmediate(imm, &enc)) {
    CpuFeatureScope scope(this, VFPv3);
    // The float can be encoded in the instruction.
    //
    // Sd = immediate
    // Instruction details available in ARM DDI 0406C.b, A8-936.
    // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | imm4H(19-16) |
    // Vd(15-12) | 101(11-9) | sz=0(8) | imm4L(3-0)
    int vd, d;
    dst.split_code(&vd, &d);
    emit(al | 0x1D * B23 | d * B22 | 0x3 * B20 | vd * B12 | 0x5 * B9 | enc);
  } else {
    mov(ip, Operand(bit_cast<int32_t>(imm)));
    vmov(dst, ip);
  }
}


void Assembler::vmov(const DwVfpRegister dst,
                     double imm,
                     const Register scratch) {
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(!scratch.is(ip));
  uint32_t enc;
  // If the embedded constant pool is disabled, we can use the normal, inline
  // constant pool. If the embedded constant pool is enabled (via
  // FLAG_enable_embedded_constant_pool), we can only use it where the pool
  // pointer (pp) is valid.
  bool can_use_pool =
      !FLAG_enable_embedded_constant_pool || is_constant_pool_available();
  if (CpuFeatures::IsSupported(VFPv3) && FitsVmovFPImmediate(imm, &enc)) {
    CpuFeatureScope scope(this, VFPv3);
    // The double can be encoded in the instruction.
    //
    // Dd = immediate
    // Instruction details available in ARM DDI 0406C.b, A8-936.
    // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | imm4H(19-16) |
    // Vd(15-12) | 101(11-9) | sz=1(8) | imm4L(3-0)
    int vd, d;
    dst.split_code(&vd, &d);
    emit(al | 0x1D*B23 | d*B22 | 0x3*B20 | vd*B12 | 0x5*B9 | B8 | enc);
  } else if (CpuFeatures::IsSupported(ARMv7) && FLAG_enable_vldr_imm &&
             can_use_pool) {
    CpuFeatureScope scope(this, ARMv7);
    // TODO(jfb) Temporarily turned off until we have constant blinding or
    //           some equivalent mitigation: an attacker can otherwise control
    //           generated data which also happens to be executable, a Very Bad
    //           Thing indeed.
    //           Blinding gets tricky because we don't have xor, we probably
    //           need to add/subtract without losing precision, which requires a
    //           cookie value that Lithium is probably better positioned to
    //           choose.
    //           We could also add a few peepholes here like detecting 0.0 and
    //           -0.0 and doing a vmov from the sequestered d14, forcing denorms
    //           to zero (we set flush-to-zero), and normalizing NaN values.
    //           We could also detect redundant values.
    //           The code could also randomize the order of values, though
    //           that's tricky because vldr has a limited reach. Furthermore
    //           it breaks load locality.
    ConstantPoolEntry::Access access = ConstantPoolAddEntry(pc_offset(), imm);
    if (access == ConstantPoolEntry::OVERFLOWED) {
      DCHECK(FLAG_enable_embedded_constant_pool);
      // Emit instructions to load constant pool offset.
      movw(ip, 0);
      movt(ip, 0);
      // Load from constant pool at offset.
      vldr(dst, MemOperand(pp, ip));
    } else {
      DCHECK(access == ConstantPoolEntry::REGULAR);
      vldr(dst, MemOperand(FLAG_enable_embedded_constant_pool ? pp : pc, 0));
    }
  } else {
    // Synthesise the double from ARM immediates.
    uint32_t lo, hi;
    DoubleAsTwoUInt32(imm, &lo, &hi);

    if (lo == hi) {
      // Move the low and high parts of the double to a D register in one
      // instruction.
      mov(ip, Operand(lo));
      vmov(dst, ip, ip);
    } else if (scratch.is(no_reg)) {
      mov(ip, Operand(lo));
      vmov(dst, VmovIndexLo, ip);
      if (((lo & 0xffff) == (hi & 0xffff)) &&
          CpuFeatures::IsSupported(ARMv7)) {
        CpuFeatureScope scope(this, ARMv7);
        movt(ip, hi >> 16);
      } else {
        mov(ip, Operand(hi));
      }
      vmov(dst, VmovIndexHi, ip);
    } else {
      // Move the low and high parts of the double to a D register in one
      // instruction.
      mov(ip, Operand(lo));
      mov(scratch, Operand(hi));
      vmov(dst, ip, scratch);
    }
  }
}


void Assembler::vmov(const SwVfpRegister dst,
                     const SwVfpRegister src,
                     const Condition cond) {
  // Sd = Sm
  // Instruction details available in ARM DDI 0406B, A8-642.
  int sd, d, sm, m;
  dst.split_code(&sd, &d);
  src.split_code(&sm, &m);
  emit(cond | 0xE*B24 | d*B22 | 0xB*B20 | sd*B12 | 0xA*B8 | B6 | m*B5 | sm);
}


void Assembler::vmov(const DwVfpRegister dst,
                     const DwVfpRegister src,
                     const Condition cond) {
  // Dd = Dm
  // Instruction details available in ARM DDI 0406C.b, A8-938.
  // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | 0000(19-16) | Vd(15-12) |
  // 101(11-9) | sz=1(8) | 0(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D*B23 | d*B22 | 0x3*B20 | vd*B12 | 0x5*B9 | B8 | B6 | m*B5 |
       vm);
}

void Assembler::vmov(const DwVfpRegister dst,
                     const VmovIndex index,
                     const Register src,
                     const Condition cond) {
  // Dd[index] = Rt
  // Instruction details available in ARM DDI 0406C.b, A8-940.
  // cond(31-28) | 1110(27-24) | 0(23) | opc1=0index(22-21) | 0(20) |
  // Vd(19-16) | Rt(15-12) | 1011(11-8) | D(7) | opc2=00(6-5) | 1(4) | 0000(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(index.index == 0 || index.index == 1);
  int vd, d;
  dst.split_code(&vd, &d);
  emit(cond | 0xE*B24 | index.index*B21 | vd*B16 | src.code()*B12 | 0xB*B8 |
       d*B7 | B4);
}


void Assembler::vmov(const Register dst,
                     const VmovIndex index,
                     const DwVfpRegister src,
                     const Condition cond) {
  // Dd[index] = Rt
  // Instruction details available in ARM DDI 0406C.b, A8.8.342.
  // cond(31-28) | 1110(27-24) | U=0(23) | opc1=0index(22-21) | 1(20) |
  // Vn(19-16) | Rt(15-12) | 1011(11-8) | N(7) | opc2=00(6-5) | 1(4) | 0000(3-0)
  DCHECK(VfpRegisterIsAvailable(src));
  DCHECK(index.index == 0 || index.index == 1);
  int vn, n;
  src.split_code(&vn, &n);
  emit(cond | 0xE*B24 | index.index*B21 | B20 | vn*B16 | dst.code()*B12 |
       0xB*B8 | n*B7 | B4);
}


void Assembler::vmov(const DwVfpRegister dst,
                     const Register src1,
                     const Register src2,
                     const Condition cond) {
  // Dm = <Rt,Rt2>.
  // Instruction details available in ARM DDI 0406C.b, A8-948.
  // cond(31-28) | 1100(27-24)| 010(23-21) | op=0(20) | Rt2(19-16) |
  // Rt(15-12) | 1011(11-8) | 00(7-6) | M(5) | 1(4) | Vm
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(!src1.is(pc) && !src2.is(pc));
  int vm, m;
  dst.split_code(&vm, &m);
  emit(cond | 0xC*B24 | B22 | src2.code()*B16 |
       src1.code()*B12 | 0xB*B8 | m*B5 | B4 | vm);
}


void Assembler::vmov(const Register dst1,
                     const Register dst2,
                     const DwVfpRegister src,
                     const Condition cond) {
  // <Rt,Rt2> = Dm.
  // Instruction details available in ARM DDI 0406C.b, A8-948.
  // cond(31-28) | 1100(27-24)| 010(23-21) | op=1(20) | Rt2(19-16) |
  // Rt(15-12) | 1011(11-8) | 00(7-6) | M(5) | 1(4) | Vm
  DCHECK(VfpRegisterIsAvailable(src));
  DCHECK(!dst1.is(pc) && !dst2.is(pc));
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0xC*B24 | B22 | B20 | dst2.code()*B16 |
       dst1.code()*B12 | 0xB*B8 | m*B5 | B4 | vm);
}


void Assembler::vmov(const SwVfpRegister dst,
                     const Register src,
                     const Condition cond) {
  // Sn = Rt.
  // Instruction details available in ARM DDI 0406A, A8-642.
  // cond(31-28) | 1110(27-24)| 000(23-21) | op=0(20) | Vn(19-16) |
  // Rt(15-12) | 1010(11-8) | N(7)=0 | 00(6-5) | 1(4) | 0000(3-0)
  DCHECK(!src.is(pc));
  int sn, n;
  dst.split_code(&sn, &n);
  emit(cond | 0xE*B24 | sn*B16 | src.code()*B12 | 0xA*B8 | n*B7 | B4);
}


void Assembler::vmov(const Register dst,
                     const SwVfpRegister src,
                     const Condition cond) {
  // Rt = Sn.
  // Instruction details available in ARM DDI 0406A, A8-642.
  // cond(31-28) | 1110(27-24)| 000(23-21) | op=1(20) | Vn(19-16) |
  // Rt(15-12) | 1010(11-8) | N(7)=0 | 00(6-5) | 1(4) | 0000(3-0)
  DCHECK(!dst.is(pc));
  int sn, n;
  src.split_code(&sn, &n);
  emit(cond | 0xE*B24 | B20 | sn*B16 | dst.code()*B12 | 0xA*B8 | n*B7 | B4);
}

// Type of data to read from or write to VFP register.
// Used as specifier in generic vcvt instruction.
enum VFPType { S32, U32, F32, F64 };


static bool IsSignedVFPType(VFPType type) {
  switch (type) {
    case S32:
      return true;
    case U32:
      return false;
    default:
      UNREACHABLE();
      return false;
  }
}


static bool IsIntegerVFPType(VFPType type) {
  switch (type) {
    case S32:
    case U32:
      return true;
    case F32:
    case F64:
      return false;
    default:
      UNREACHABLE();
      return false;
  }
}


static bool IsDoubleVFPType(VFPType type) {
  switch (type) {
    case F32:
      return false;
    case F64:
      return true;
    default:
      UNREACHABLE();
      return false;
  }
}


// Split five bit reg_code based on size of reg_type.
//  32-bit register codes are Vm:M
//  64-bit register codes are M:Vm
// where Vm is four bits, and M is a single bit.
static void SplitRegCode(VFPType reg_type,
                         int reg_code,
                         int* vm,
                         int* m) {
  DCHECK((reg_code >= 0) && (reg_code <= 31));
  if (IsIntegerVFPType(reg_type) || !IsDoubleVFPType(reg_type)) {
    // 32 bit type.
    *m  = reg_code & 0x1;
    *vm = reg_code >> 1;
  } else {
    // 64 bit type.
    *m  = (reg_code & 0x10) >> 4;
    *vm = reg_code & 0x0F;
  }
}


// Encode vcvt.src_type.dst_type instruction.
static Instr EncodeVCVT(const VFPType dst_type,
                        const int dst_code,
                        const VFPType src_type,
                        const int src_code,
                        VFPConversionMode mode,
                        const Condition cond) {
  DCHECK(src_type != dst_type);
  int D, Vd, M, Vm;
  SplitRegCode(src_type, src_code, &Vm, &M);
  SplitRegCode(dst_type, dst_code, &Vd, &D);

  if (IsIntegerVFPType(dst_type) || IsIntegerVFPType(src_type)) {
    // Conversion between IEEE floating point and 32-bit integer.
    // Instruction details available in ARM DDI 0406B, A8.6.295.
    // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 1(19) | opc2(18-16) |
    // Vd(15-12) | 101(11-9) | sz(8) | op(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
    DCHECK(!IsIntegerVFPType(dst_type) || !IsIntegerVFPType(src_type));

    int sz, opc2, op;

    if (IsIntegerVFPType(dst_type)) {
      opc2 = IsSignedVFPType(dst_type) ? 0x5 : 0x4;
      sz = IsDoubleVFPType(src_type) ? 0x1 : 0x0;
      op = mode;
    } else {
      DCHECK(IsIntegerVFPType(src_type));
      opc2 = 0x0;
      sz = IsDoubleVFPType(dst_type) ? 0x1 : 0x0;
      op = IsSignedVFPType(src_type) ? 0x1 : 0x0;
    }

    return (cond | 0xE*B24 | B23 | D*B22 | 0x3*B20 | B19 | opc2*B16 |
            Vd*B12 | 0x5*B9 | sz*B8 | op*B7 | B6 | M*B5 | Vm);
  } else {
    // Conversion between IEEE double and single precision.
    // Instruction details available in ARM DDI 0406B, A8.6.298.
    // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0111(19-16) |
    // Vd(15-12) | 101(11-9) | sz(8) | 1(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
    int sz = IsDoubleVFPType(src_type) ? 0x1 : 0x0;
    return (cond | 0xE*B24 | B23 | D*B22 | 0x3*B20 | 0x7*B16 |
            Vd*B12 | 0x5*B9 | sz*B8 | B7 | B6 | M*B5 | Vm);
  }
}


void Assembler::vcvt_f64_s32(const DwVfpRegister dst,
                             const SwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(dst));
  emit(EncodeVCVT(F64, dst.code(), S32, src.code(), mode, cond));
}


void Assembler::vcvt_f32_s32(const SwVfpRegister dst,
                             const SwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  emit(EncodeVCVT(F32, dst.code(), S32, src.code(), mode, cond));
}


void Assembler::vcvt_f64_u32(const DwVfpRegister dst,
                             const SwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(dst));
  emit(EncodeVCVT(F64, dst.code(), U32, src.code(), mode, cond));
}


void Assembler::vcvt_f32_u32(const SwVfpRegister dst, const SwVfpRegister src,
                             VFPConversionMode mode, const Condition cond) {
  emit(EncodeVCVT(F32, dst.code(), U32, src.code(), mode, cond));
}


void Assembler::vcvt_s32_f32(const SwVfpRegister dst, const SwVfpRegister src,
                             VFPConversionMode mode, const Condition cond) {
  emit(EncodeVCVT(S32, dst.code(), F32, src.code(), mode, cond));
}


void Assembler::vcvt_u32_f32(const SwVfpRegister dst, const SwVfpRegister src,
                             VFPConversionMode mode, const Condition cond) {
  emit(EncodeVCVT(U32, dst.code(), F32, src.code(), mode, cond));
}


void Assembler::vcvt_s32_f64(const SwVfpRegister dst,
                             const DwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeVCVT(S32, dst.code(), F64, src.code(), mode, cond));
}


void Assembler::vcvt_u32_f64(const SwVfpRegister dst,
                             const DwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeVCVT(U32, dst.code(), F64, src.code(), mode, cond));
}


void Assembler::vcvt_f64_f32(const DwVfpRegister dst,
                             const SwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(dst));
  emit(EncodeVCVT(F64, dst.code(), F32, src.code(), mode, cond));
}


void Assembler::vcvt_f32_f64(const SwVfpRegister dst,
                             const DwVfpRegister src,
                             VFPConversionMode mode,
                             const Condition cond) {
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeVCVT(F32, dst.code(), F64, src.code(), mode, cond));
}


void Assembler::vcvt_f64_s32(const DwVfpRegister dst,
                             int fraction_bits,
                             const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-874.
  // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | 1010(19-16) | Vd(15-12) |
  // 101(11-9) | sf=1(8) | sx=1(7) | 1(6) | i(5) | 0(4) | imm4(3-0)
  DCHECK(IsEnabled(VFPv3));
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(fraction_bits > 0 && fraction_bits <= 32);
  int vd, d;
  dst.split_code(&vd, &d);
  int imm5 = 32 - fraction_bits;
  int i = imm5 & 1;
  int imm4 = (imm5 >> 1) & 0xf;
  emit(cond | 0xE*B24 | B23 | d*B22 | 0x3*B20 | B19 | 0x2*B16 |
       vd*B12 | 0x5*B9 | B8 | B7 | B6 | i*B5 | imm4);
}


void Assembler::vneg(const DwVfpRegister dst,
                     const DwVfpRegister src,
                     const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-968.
  // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | 0001(19-16) | Vd(15-12) |
  // 101(11-9) | sz=1(8) | 0(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);

  emit(cond | 0x1D*B23 | d*B22 | 0x3*B20 | B16 | vd*B12 | 0x5*B9 | B8 | B6 |
       m*B5 | vm);
}


void Assembler::vneg(const SwVfpRegister dst, const SwVfpRegister src,
                     const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-968.
  // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | 0001(19-16) | Vd(15-12) |
  // 101(11-9) | sz=0(8) | 0(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);

  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | B16 | vd * B12 | 0x5 * B9 |
       B6 | m * B5 | vm);
}


void Assembler::vabs(const DwVfpRegister dst,
                     const DwVfpRegister src,
                     const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-524.
  // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | 0000(19-16) | Vd(15-12) |
  // 101(11-9) | sz=1(8) | 1(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D*B23 | d*B22 | 0x3*B20 | vd*B12 | 0x5*B9 | B8 | B7 | B6 |
       m*B5 | vm);
}


void Assembler::vabs(const SwVfpRegister dst, const SwVfpRegister src,
                     const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-524.
  // cond(31-28) | 11101(27-23) | D(22) | 11(21-20) | 0000(19-16) | Vd(15-12) |
  // 101(11-9) | sz=0(8) | 1(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | vd * B12 | 0x5 * B9 | B7 | B6 |
       m * B5 | vm);
}


void Assembler::vadd(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vadd(Dn, Dm) double precision floating point addition.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-830.
  // cond(31-28) | 11100(27-23)| D(22) | 11(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C*B23 | d*B22 | 0x3*B20 | vn*B16 | vd*B12 | 0x5*B9 | B8 |
       n*B7 | m*B5 | vm);
}


void Assembler::vadd(const SwVfpRegister dst, const SwVfpRegister src1,
                     const SwVfpRegister src2, const Condition cond) {
  // Sd = vadd(Sn, Sm) single precision floating point addition.
  // Sd = D:Vd; Sm=M:Vm; Sn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-830.
  // cond(31-28) | 11100(27-23)| D(22) | 11(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C * B23 | d * B22 | 0x3 * B20 | vn * B16 | vd * B12 |
       0x5 * B9 | n * B7 | m * B5 | vm);
}


void Assembler::vsub(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vsub(Dn, Dm) double precision floating point subtraction.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-1086.
  // cond(31-28) | 11100(27-23)| D(22) | 11(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C*B23 | d*B22 | 0x3*B20 | vn*B16 | vd*B12 | 0x5*B9 | B8 |
       n*B7 | B6 | m*B5 | vm);
}


void Assembler::vsub(const SwVfpRegister dst, const SwVfpRegister src1,
                     const SwVfpRegister src2, const Condition cond) {
  // Sd = vsub(Sn, Sm) single precision floating point subtraction.
  // Sd = D:Vd; Sm=M:Vm; Sn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-1086.
  // cond(31-28) | 11100(27-23)| D(22) | 11(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C * B23 | d * B22 | 0x3 * B20 | vn * B16 | vd * B12 |
       0x5 * B9 | n * B7 | B6 | m * B5 | vm);
}


void Assembler::vmul(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vmul(Dn, Dm) double precision floating point multiplication.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-960.
  // cond(31-28) | 11100(27-23)| D(22) | 10(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C*B23 | d*B22 | 0x2*B20 | vn*B16 | vd*B12 | 0x5*B9 | B8 |
       n*B7 | m*B5 | vm);
}


void Assembler::vmul(const SwVfpRegister dst, const SwVfpRegister src1,
                     const SwVfpRegister src2, const Condition cond) {
  // Sd = vmul(Sn, Sm) single precision floating point multiplication.
  // Sd = D:Vd; Sm=M:Vm; Sn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-960.
  // cond(31-28) | 11100(27-23)| D(22) | 10(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C * B23 | d * B22 | 0x2 * B20 | vn * B16 | vd * B12 |
       0x5 * B9 | n * B7 | m * B5 | vm);
}


void Assembler::vmla(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-932.
  // cond(31-28) | 11100(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | op=0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C*B23 | d*B22 | vn*B16 | vd*B12 | 0x5*B9 | B8 | n*B7 | m*B5 |
       vm);
}


void Assembler::vmla(const SwVfpRegister dst, const SwVfpRegister src1,
                     const SwVfpRegister src2, const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-932.
  // cond(31-28) | 11100(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | op=0(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C * B23 | d * B22 | vn * B16 | vd * B12 | 0x5 * B9 | n * B7 |
       m * B5 | vm);
}


void Assembler::vmls(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-932.
  // cond(31-28) | 11100(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | op=1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C*B23 | d*B22 | vn*B16 | vd*B12 | 0x5*B9 | B8 | n*B7 | B6 |
       m*B5 | vm);
}


void Assembler::vmls(const SwVfpRegister dst, const SwVfpRegister src1,
                     const SwVfpRegister src2, const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-932.
  // cond(31-28) | 11100(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | op=1(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1C * B23 | d * B22 | vn * B16 | vd * B12 | 0x5 * B9 | n * B7 |
       B6 | m * B5 | vm);
}


void Assembler::vdiv(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vdiv(Dn, Dm) double precision floating point division.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-882.
  // cond(31-28) | 11101(27-23)| D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1D*B23 | d*B22 | vn*B16 | vd*B12 | 0x5*B9 | B8 | n*B7 | m*B5 |
       vm);
}


void Assembler::vdiv(const SwVfpRegister dst, const SwVfpRegister src1,
                     const SwVfpRegister src2, const Condition cond) {
  // Sd = vdiv(Sn, Sm) single precision floating point division.
  // Sd = D:Vd; Sm=M:Vm; Sn=N:Vm.
  // Instruction details available in ARM DDI 0406C.b, A8-882.
  // cond(31-28) | 11101(27-23)| D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1D * B23 | d * B22 | vn * B16 | vd * B12 | 0x5 * B9 | n * B7 |
       m * B5 | vm);
}


void Assembler::vcmp(const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // vcmp(Dd, Dm) double precision floating point comparison.
  // Instruction details available in ARM DDI 0406C.b, A8-864.
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0100(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | E=0(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(VfpRegisterIsAvailable(src2));
  int vd, d;
  src1.split_code(&vd, &d);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1D*B23 | d*B22 | 0x3*B20 | 0x4*B16 | vd*B12 | 0x5*B9 | B8 | B6 |
       m*B5 | vm);
}


void Assembler::vcmp(const SwVfpRegister src1, const SwVfpRegister src2,
                     const Condition cond) {
  // vcmp(Sd, Sm) single precision floating point comparison.
  // Instruction details available in ARM DDI 0406C.b, A8-864.
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0100(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | E=0(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  src1.split_code(&vd, &d);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | 0x4 * B16 | vd * B12 |
       0x5 * B9 | B6 | m * B5 | vm);
}


void Assembler::vcmp(const DwVfpRegister src1,
                     const double src2,
                     const Condition cond) {
  // vcmp(Dd, #0.0) double precision floating point comparison.
  // Instruction details available in ARM DDI 0406C.b, A8-864.
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0101(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | E=0(7) | 1(6) | 0(5) | 0(4) | 0000(3-0)
  DCHECK(VfpRegisterIsAvailable(src1));
  DCHECK(src2 == 0.0);
  int vd, d;
  src1.split_code(&vd, &d);
  emit(cond | 0x1D*B23 | d*B22 | 0x3*B20 | 0x5*B16 | vd*B12 | 0x5*B9 | B8 | B6);
}


void Assembler::vcmp(const SwVfpRegister src1, const float src2,
                     const Condition cond) {
  // vcmp(Sd, #0.0) single precision floating point comparison.
  // Instruction details available in ARM DDI 0406C.b, A8-864.
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0101(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | E=0(7) | 1(6) | 0(5) | 0(4) | 0000(3-0)
  DCHECK(src2 == 0.0);
  int vd, d;
  src1.split_code(&vd, &d);
  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | 0x5 * B16 | vd * B12 |
       0x5 * B9 | B6);
}

void Assembler::vmaxnm(const DwVfpRegister dst, const DwVfpRegister src1,
                       const DwVfpRegister src2) {
  // kSpecialCondition(31-28) | 11101(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);

  emit(kSpecialCondition | 0x1D * B23 | d * B22 | vn * B16 | vd * B12 |
       0x5 * B9 | B8 | n * B7 | m * B5 | vm);
}

void Assembler::vmaxnm(const SwVfpRegister dst, const SwVfpRegister src1,
                       const SwVfpRegister src2) {
  // kSpecialCondition(31-28) | 11101(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);

  emit(kSpecialCondition | 0x1D * B23 | d * B22 | vn * B16 | vd * B12 |
       0x5 * B9 | n * B7 | m * B5 | vm);
}

void Assembler::vminnm(const DwVfpRegister dst, const DwVfpRegister src1,
                       const DwVfpRegister src2) {
  // kSpecialCondition(31-28) | 11101(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | N(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);

  emit(kSpecialCondition | 0x1D * B23 | d * B22 | vn * B16 | vd * B12 |
       0x5 * B9 | B8 | n * B7 | B6 | m * B5 | vm);
}

void Assembler::vminnm(const SwVfpRegister dst, const SwVfpRegister src1,
                       const SwVfpRegister src2) {
  // kSpecialCondition(31-28) | 11101(27-23) | D(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | N(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);

  emit(kSpecialCondition | 0x1D * B23 | d * B22 | vn * B16 | vd * B12 |
       0x5 * B9 | n * B7 | B6 | m * B5 | vm);
}

void Assembler::vsel(Condition cond, const DwVfpRegister dst,
                     const DwVfpRegister src1, const DwVfpRegister src2) {
  // cond=kSpecialCondition(31-28) | 11100(27-23) | D(22) |
  // vsel_cond=XX(21-20) | Vn(19-16) | Vd(15-12) | 101(11-9) | sz=1(8) | N(7) |
  // 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = 1;

  // VSEL has a special (restricted) condition encoding.
  //   eq(0b0000)... -> 0b00
  //   ge(0b1010)... -> 0b10
  //   gt(0b1100)... -> 0b11
  //   vs(0b0110)... -> 0b01
  // No other conditions are supported.
  int vsel_cond = (cond >> 30) & 0x3;
  if ((cond != eq) && (cond != ge) && (cond != gt) && (cond != vs)) {
    // We can implement some other conditions by swapping the inputs.
    DCHECK((cond == ne) | (cond == lt) | (cond == le) | (cond == vc));
    std::swap(vn, vm);
    std::swap(n, m);
  }

  emit(kSpecialCondition | 0x1C * B23 | d * B22 | vsel_cond * B20 | vn * B16 |
       vd * B12 | 0x5 * B9 | sz * B8 | n * B7 | m * B5 | vm);
}

void Assembler::vsel(Condition cond, const SwVfpRegister dst,
                     const SwVfpRegister src1, const SwVfpRegister src2) {
  // cond=kSpecialCondition(31-28) | 11100(27-23) | D(22) |
  // vsel_cond=XX(21-20) | Vn(19-16) | Vd(15-12) | 101(11-9) | sz=0(8) | N(7) |
  // 0(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = 0;

  // VSEL has a special (restricted) condition encoding.
  //   eq(0b0000)... -> 0b00
  //   ge(0b1010)... -> 0b10
  //   gt(0b1100)... -> 0b11
  //   vs(0b0110)... -> 0b01
  // No other conditions are supported.
  int vsel_cond = (cond >> 30) & 0x3;
  if ((cond != eq) && (cond != ge) && (cond != gt) && (cond != vs)) {
    // We can implement some other conditions by swapping the inputs.
    DCHECK((cond == ne) | (cond == lt) | (cond == le) | (cond == vc));
    std::swap(vn, vm);
    std::swap(n, m);
  }

  emit(kSpecialCondition | 0x1C * B23 | d * B22 | vsel_cond * B20 | vn * B16 |
       vd * B12 | 0x5 * B9 | sz * B8 | n * B7 | m * B5 | vm);
}

void Assembler::vsqrt(const DwVfpRegister dst,
                      const DwVfpRegister src,
                      const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-1058.
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0001(19-16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | 11(7-6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D*B23 | d*B22 | 0x3*B20 | B16 | vd*B12 | 0x5*B9 | B8 | 0x3*B6 |
       m*B5 | vm);
}


void Assembler::vsqrt(const SwVfpRegister dst, const SwVfpRegister src,
                      const Condition cond) {
  // Instruction details available in ARM DDI 0406C.b, A8-1058.
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0001(19-16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | 11(7-6) | M(5) | 0(4) | Vm(3-0)
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | B16 | vd * B12 | 0x5 * B9 |
       0x3 * B6 | m * B5 | vm);
}


void Assembler::vmsr(Register dst, Condition cond) {
  // Instruction details available in ARM DDI 0406A, A8-652.
  // cond(31-28) | 1110 (27-24) | 1110(23-20)| 0001 (19-16) |
  // Rt(15-12) | 1010 (11-8) | 0(7) | 00 (6-5) | 1(4) | 0000(3-0)
  emit(cond | 0xE * B24 | 0xE * B20 | B16 | dst.code() * B12 | 0xA * B8 | B4);
}


void Assembler::vmrs(Register dst, Condition cond) {
  // Instruction details available in ARM DDI 0406A, A8-652.
  // cond(31-28) | 1110 (27-24) | 1111(23-20)| 0001 (19-16) |
  // Rt(15-12) | 1010 (11-8) | 0(7) | 00 (6-5) | 1(4) | 0000(3-0)
  emit(cond | 0xE * B24 | 0xF * B20 | B16 | dst.code() * B12 | 0xA * B8 | B4);
}


void Assembler::vrinta(const SwVfpRegister dst, const SwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=00(17-16) |  Vd(15-12) | 101(11-9) | sz=0(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | vd * B12 |
       0x5 * B9 | B6 | m * B5 | vm);
}


void Assembler::vrinta(const DwVfpRegister dst, const DwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=00(17-16) |  Vd(15-12) | 101(11-9) | sz=1(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | vd * B12 |
       0x5 * B9 | B8 | B6 | m * B5 | vm);
}


void Assembler::vrintn(const SwVfpRegister dst, const SwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=01(17-16) |  Vd(15-12) | 101(11-9) | sz=0(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | 0x1 * B16 |
       vd * B12 | 0x5 * B9 | B6 | m * B5 | vm);
}


void Assembler::vrintn(const DwVfpRegister dst, const DwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=01(17-16) |  Vd(15-12) | 101(11-9) | sz=1(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | 0x1 * B16 |
       vd * B12 | 0x5 * B9 | B8 | B6 | m * B5 | vm);
}


void Assembler::vrintp(const SwVfpRegister dst, const SwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=10(17-16) |  Vd(15-12) | 101(11-9) | sz=0(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | 0x2 * B16 |
       vd * B12 | 0x5 * B9 | B6 | m * B5 | vm);
}


void Assembler::vrintp(const DwVfpRegister dst, const DwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=10(17-16) |  Vd(15-12) | 101(11-9) | sz=1(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | 0x2 * B16 |
       vd * B12 | 0x5 * B9 | B8 | B6 | m * B5 | vm);
}


void Assembler::vrintm(const SwVfpRegister dst, const SwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=11(17-16) |  Vd(15-12) | 101(11-9) | sz=0(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | 0x3 * B16 |
       vd * B12 | 0x5 * B9 | B6 | m * B5 | vm);
}


void Assembler::vrintm(const DwVfpRegister dst, const DwVfpRegister src) {
  // cond=kSpecialCondition(31-28) | 11101(27-23)| D(22) | 11(21-20) |
  // 10(19-18) | RM=11(17-16) |  Vd(15-12) | 101(11-9) | sz=1(8) | 01(7-6) |
  // M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(kSpecialCondition | 0x1D * B23 | d * B22 | 0x3 * B20 | B19 | 0x3 * B16 |
       vd * B12 | 0x5 * B9 | B8 | B6 | m * B5 | vm);
}


void Assembler::vrintz(const SwVfpRegister dst, const SwVfpRegister src,
                       const Condition cond) {
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 011(19-17) | 0(16) |
  // Vd(15-12) | 101(11-9) | sz=0(8) | op=1(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | 0x3 * B17 | vd * B12 |
       0x5 * B9 | B7 | B6 | m * B5 | vm);
}


void Assembler::vrintz(const DwVfpRegister dst, const DwVfpRegister src,
                       const Condition cond) {
  // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 011(19-17) | 0(16) |
  // Vd(15-12) | 101(11-9) | sz=1(8) | op=1(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(ARMv8));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(cond | 0x1D * B23 | d * B22 | 0x3 * B20 | 0x3 * B17 | vd * B12 |
       0x5 * B9 | B8 | B7 | B6 | m * B5 | vm);
}


// Support for NEON.

void Assembler::vld1(NeonSize size,
                     const NeonListOperand& dst,
                     const NeonMemOperand& src) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.320.
  // 1111(31-28) | 01000(27-23) | D(22) | 10(21-20) | Rn(19-16) |
  // Vd(15-12) | type(11-8) | size(7-6) | align(5-4) | Rm(3-0)
  DCHECK(IsEnabled(NEON));
  int vd, d;
  dst.base().split_code(&vd, &d);
  emit(0xFU*B28 | 4*B24 | d*B22 | 2*B20 | src.rn().code()*B16 | vd*B12 |
       dst.type()*B8 | size*B6 | src.align()*B4 | src.rm().code());
}


void Assembler::vst1(NeonSize size,
                     const NeonListOperand& src,
                     const NeonMemOperand& dst) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.404.
  // 1111(31-28) | 01000(27-23) | D(22) | 00(21-20) | Rn(19-16) |
  // Vd(15-12) | type(11-8) | size(7-6) | align(5-4) | Rm(3-0)
  DCHECK(IsEnabled(NEON));
  int vd, d;
  src.base().split_code(&vd, &d);
  emit(0xFU*B28 | 4*B24 | d*B22 | dst.rn().code()*B16 | vd*B12 | src.type()*B8 |
       size*B6 | dst.align()*B4 | dst.rm().code());
}


void Assembler::vmovl(NeonDataType dt, QwNeonRegister dst, DwVfpRegister src) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.346.
  // 1111(31-28) | 001(27-25) | U(24) | 1(23) | D(22) | imm3(21-19) |
  // 000(18-16) | Vd(15-12) | 101000(11-6) | M(5) | 1(4) | Vm(3-0)
  DCHECK(IsEnabled(NEON));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(0xFU*B28 | B25 | (dt & NeonDataTypeUMask) | B23 | d*B22 |
        (dt & NeonDataTypeSizeMask)*B19 | vd*B12 | 0xA*B8 | m*B5 | B4 | vm);
}

static int EncodeScalar(NeonDataType dt, int index) {
  int opc1_opc2 = 0;
  DCHECK_LE(0, index);
  switch (dt) {
    case NeonS8:
    case NeonU8:
      DCHECK_GT(8, index);
      opc1_opc2 = 0x8 | index;
      break;
    case NeonS16:
    case NeonU16:
      DCHECK_GT(4, index);
      opc1_opc2 = 0x1 | (index << 1);
      break;
    case NeonS32:
    case NeonU32:
      DCHECK_GT(2, index);
      opc1_opc2 = index << 2;
      break;
    default:
      UNREACHABLE();
      break;
  }
  return (opc1_opc2 >> 2) * B21 | (opc1_opc2 & 0x3) * B5;
}

void Assembler::vmov(NeonDataType dt, DwVfpRegister dst, int index,
                     Register src) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.940.
  // vmov ARM core register to scalar.
  DCHECK(dt == NeonS32 || dt == NeonU32 || IsEnabled(NEON));
  int vd, d;
  dst.split_code(&vd, &d);
  int opc1_opc2 = EncodeScalar(dt, index);
  emit(0xEEu * B24 | vd * B16 | src.code() * B12 | 0xB * B8 | d * B7 | B4 |
       opc1_opc2);
}

void Assembler::vmov(NeonDataType dt, Register dst, DwVfpRegister src,
                     int index) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.942.
  // vmov Arm scalar to core register.
  DCHECK(dt == NeonS32 || dt == NeonU32 || IsEnabled(NEON));
  int vn, n;
  src.split_code(&vn, &n);
  int opc1_opc2 = EncodeScalar(dt, index);
  int u = (dt & NeonDataTypeUMask) != 0 ? 1 : 0;
  emit(0xEEu * B24 | u * B23 | B20 | vn * B16 | dst.code() * B12 | 0xB * B8 |
       n * B7 | B4 | opc1_opc2);
}

void Assembler::vmov(const QwNeonRegister dst, const QwNeonRegister src) {
  // Instruction details available in ARM DDI 0406C.b, A8-938.
  // vmov is encoded as vorr.
  vorr(dst, src, src);
}

void Assembler::vmvn(const QwNeonRegister dst, const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  // Instruction details available in ARM DDI 0406C.b, A8-966.
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(0x1E7U * B23 | d * B22 | 3 * B20 | vd * B12 | 0x17 * B6 | m * B5 | vm);
}

void Assembler::vswp(DwVfpRegister dst, DwVfpRegister src) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.418.
  // 1111(31-28) | 00111(27-23) | D(22) | 110010(21-16) |
  // Vd(15-12) | 000000(11-6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(NEON));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(0xFU * B28 | 7 * B23 | d * B22 | 0x32 * B16 | vd * B12 | m * B5 | vm);
}

void Assembler::vswp(QwNeonRegister dst, QwNeonRegister src) {
  // Instruction details available in ARM DDI 0406C.b, A8.8.418.
  // 1111(31-28) | 00111(27-23) | D(22) | 110010(21-16) |
  // Vd(15-12) | 000000(11-6) | M(5) | 0(4) | Vm(3-0)
  DCHECK(IsEnabled(NEON));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  emit(0xFU * B28 | 7 * B23 | d * B22 | 0x32 * B16 | vd * B12 | B6 | m * B5 |
       vm);
}

void Assembler::vdup(NeonSize size, const QwNeonRegister dst,
                     const Register src) {
  DCHECK(IsEnabled(NEON));
  // Instruction details available in ARM DDI 0406C.b, A8-886.
  int B = 0, E = 0;
  switch (size) {
    case Neon8:
      B = 1;
      break;
    case Neon16:
      E = 1;
      break;
    case Neon32:
      break;
    default:
      UNREACHABLE();
      break;
  }
  int vd, d;
  dst.split_code(&vd, &d);

  emit(al | 0x1D * B23 | B * B22 | B21 | vd * B16 | src.code() * B12 |
       0xB * B8 | d * B7 | E * B5 | B4);
}

void Assembler::vdup(const QwNeonRegister dst, const SwVfpRegister src) {
  DCHECK(IsEnabled(NEON));
  // Instruction details available in ARM DDI 0406C.b, A8-884.
  int index = src.code() & 1;
  int d_reg = src.code() / 2;
  int imm4 = 4 | index << 3;  // esize = 32, index in bit 3.
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  DwVfpRegister::from_code(d_reg).split_code(&vm, &m);

  emit(0x1E7U * B23 | d * B22 | 0x3 * B20 | imm4 * B16 | vd * B12 | 0x18 * B7 |
       B6 | m * B5 | vm);
}

// Encode NEON vcvt.src_type.dst_type instruction.
static Instr EncodeNeonVCVT(const VFPType dst_type, const QwNeonRegister dst,
                            const VFPType src_type, const QwNeonRegister src) {
  DCHECK(src_type != dst_type);
  DCHECK(src_type == F32 || dst_type == F32);
  // Instruction details available in ARM DDI 0406C.b, A8.8.868.
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);

  int op = 0;
  if (src_type == F32) {
    DCHECK(dst_type == S32 || dst_type == U32);
    op = dst_type == U32 ? 3 : 2;
  } else {
    DCHECK(src_type == S32 || src_type == U32);
    op = src_type == U32 ? 1 : 0;
  }

  return 0x1E7U * B23 | d * B22 | 0x3B * B16 | vd * B12 | 0x3 * B9 | op * B7 |
         B6 | m * B5 | vm;
}

void Assembler::vcvt_f32_s32(const QwNeonRegister dst,
                             const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeNeonVCVT(F32, dst, S32, src));
}

void Assembler::vcvt_f32_u32(const QwNeonRegister dst,
                             const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeNeonVCVT(F32, dst, U32, src));
}

void Assembler::vcvt_s32_f32(const QwNeonRegister dst,
                             const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeNeonVCVT(S32, dst, F32, src));
}

void Assembler::vcvt_u32_f32(const QwNeonRegister dst,
                             const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  DCHECK(VfpRegisterIsAvailable(dst));
  DCHECK(VfpRegisterIsAvailable(src));
  emit(EncodeNeonVCVT(U32, dst, F32, src));
}

// op is instr->Bits(11, 7).
static Instr EncodeNeonUnaryOp(int op, bool is_float, NeonSize size,
                               const QwNeonRegister dst,
                               const QwNeonRegister src) {
  DCHECK_IMPLIES(is_float, size == Neon32);
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  int F = is_float ? 1 : 0;
  return 0x1E7U * B23 | d * B22 | 0x3 * B20 | size * B18 | B16 | vd * B12 |
         F * B10 | B8 | op * B7 | B6 | m * B5 | vm;
}

void Assembler::vabs(const QwNeonRegister dst, const QwNeonRegister src) {
  // Qd = vabs.f<size>(Qn, Qm) SIMD floating point absolute value.
  // Instruction details available in ARM DDI 0406C.b, A8.8.824.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonUnaryOp(0x6, true, Neon32, dst, src));
}

void Assembler::vabs(NeonSize size, const QwNeonRegister dst,
                     const QwNeonRegister src) {
  // Qd = vabs.s<size>(Qn, Qm) SIMD integer absolute value.
  // Instruction details available in ARM DDI 0406C.b, A8.8.824.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonUnaryOp(0x6, false, size, dst, src));
}

void Assembler::vneg(const QwNeonRegister dst, const QwNeonRegister src) {
  // Qd = vabs.f<size>(Qn, Qm) SIMD floating point negate.
  // Instruction details available in ARM DDI 0406C.b, A8.8.968.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonUnaryOp(0x7, true, Neon32, dst, src));
}

void Assembler::vneg(NeonSize size, const QwNeonRegister dst,
                     const QwNeonRegister src) {
  // Qd = vabs.s<size>(Qn, Qm) SIMD integer negate.
  // Instruction details available in ARM DDI 0406C.b, A8.8.968.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonUnaryOp(0x7, false, size, dst, src));
}

void Assembler::veor(DwVfpRegister dst, DwVfpRegister src1,
                     DwVfpRegister src2) {
  // Dd = veor(Dn, Dm) 64 bit integer exclusive OR.
  // Instruction details available in ARM DDI 0406C.b, A8.8.888.
  DCHECK(IsEnabled(NEON));
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(0x1E6U * B23 | d * B22 | vn * B16 | vd * B12 | B8 | n * B7 | m * B5 |
       B4 | vm);
}

enum BinaryBitwiseOp { VAND, VBIC, VBIF, VBIT, VBSL, VEOR, VORR, VORN };

static Instr EncodeNeonBinaryBitwiseOp(BinaryBitwiseOp op,
                                       const QwNeonRegister dst,
                                       const QwNeonRegister src1,
                                       const QwNeonRegister src2) {
  int op_encoding = 0;
  switch (op) {
    case VBIC:
      op_encoding = 0x1 * B20;
      break;
    case VBIF:
      op_encoding = B24 | 0x3 * B20;
      break;
    case VBIT:
      op_encoding = B24 | 0x2 * B20;
      break;
    case VBSL:
      op_encoding = B24 | 0x1 * B20;
      break;
    case VEOR:
      op_encoding = B24;
      break;
    case VORR:
      op_encoding = 0x2 * B20;
      break;
    case VORN:
      op_encoding = 0x3 * B20;
      break;
    case VAND:
      // op_encoding is 0.
      break;
    default:
      UNREACHABLE();
      break;
  }
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  return 0x1E4U * B23 | op_encoding | d * B22 | vn * B16 | vd * B12 | B8 |
         n * B7 | B6 | m * B5 | B4 | vm;
}

void Assembler::vand(QwNeonRegister dst, QwNeonRegister src1,
                     QwNeonRegister src2) {
  // Qd = vand(Qn, Qm) SIMD AND.
  // Instruction details available in ARM DDI 0406C.b, A8.8.836.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonBinaryBitwiseOp(VAND, dst, src1, src2));
}

void Assembler::vbsl(QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vbsl(Qn, Qm) SIMD bitwise select.
  // Instruction details available in ARM DDI 0406C.b, A8-844.
  emit(EncodeNeonBinaryBitwiseOp(VBSL, dst, src1, src2));
}

void Assembler::veor(QwNeonRegister dst, QwNeonRegister src1,
                     QwNeonRegister src2) {
  // Qd = veor(Qn, Qm) SIMD exclusive OR.
  // Instruction details available in ARM DDI 0406C.b, A8.8.888.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonBinaryBitwiseOp(VEOR, dst, src1, src2));
}

void Assembler::vorr(QwNeonRegister dst, QwNeonRegister src1,
                     QwNeonRegister src2) {
  // Qd = vorr(Qn, Qm) SIMD OR.
  // Instruction details available in ARM DDI 0406C.b, A8.8.976.
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonBinaryBitwiseOp(VORR, dst, src1, src2));
}

void Assembler::vadd(QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vadd(Qn, Qm) SIMD floating point addition.
  // Instruction details available in ARM DDI 0406C.b, A8-830.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(0x1E4U * B23 | d * B22 | vn * B16 | vd * B12 | 0xD * B8 | n * B7 | B6 |
       m * B5 | vm);
}

void Assembler::vadd(NeonSize size, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vadd(Qn, Qm) SIMD integer addition.
  // Instruction details available in ARM DDI 0406C.b, A8-828.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  emit(0x1E4U * B23 | d * B22 | sz * B20 | vn * B16 | vd * B12 | 0x8 * B8 |
       n * B7 | B6 | m * B5 | vm);
}

void Assembler::vsub(QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vsub(Qn, Qm) SIMD floating point subtraction.
  // Instruction details available in ARM DDI 0406C.b, A8-1086.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(0x1E4U * B23 | d * B22 | B21 | vn * B16 | vd * B12 | 0xD * B8 | n * B7 |
       B6 | m * B5 | vm);
}

void Assembler::vsub(NeonSize size, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vsub(Qn, Qm) SIMD integer subtraction.
  // Instruction details available in ARM DDI 0406C.b, A8-1084.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  emit(0x1E6U * B23 | d * B22 | sz * B20 | vn * B16 | vd * B12 | 0x8 * B8 |
       n * B7 | B6 | m * B5 | vm);
}

void Assembler::vmul(QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vadd(Qn, Qm) SIMD floating point multiply.
  // Instruction details available in ARM DDI 0406C.b, A8-958.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(0x1E6U * B23 | d * B22 | vn * B16 | vd * B12 | 0xD * B8 | n * B7 | B6 |
       m * B5 | B4 | vm);
}

void Assembler::vmul(NeonSize size, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vadd(Qn, Qm) SIMD integer multiply.
  // Instruction details available in ARM DDI 0406C.b, A8-960.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  emit(0x1E4U * B23 | d * B22 | sz * B20 | vn * B16 | vd * B12 | 0x9 * B8 |
       n * B7 | B6 | m * B5 | B4 | vm);
}

static Instr EncodeNeonMinMax(bool is_min, QwNeonRegister dst,
                              QwNeonRegister src1, QwNeonRegister src2) {
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int min = is_min ? 1 : 0;
  return 0x1E4U * B23 | d * B22 | min * B21 | vn * B16 | vd * B12 | 0xF * B8 |
         n * B7 | B6 | m * B5 | vm;
}

static Instr EncodeNeonMinMax(bool is_min, NeonDataType dt, QwNeonRegister dst,
                              QwNeonRegister src1, QwNeonRegister src2) {
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int min = is_min ? 1 : 0;
  int size = (dt & NeonDataTypeSizeMask) / 2;
  int U = dt & NeonDataTypeUMask;
  return 0x1E4U * B23 | U | d * B22 | size * B20 | vn * B16 | vd * B12 |
         0x6 * B8 | B6 | m * B5 | min * B4 | vm;
}

void Assembler::vmin(const QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vmin(Qn, Qm) SIMD floating point MIN.
  // Instruction details available in ARM DDI 0406C.b, A8-928.
  emit(EncodeNeonMinMax(true, dst, src1, src2));
}

void Assembler::vmin(NeonDataType dt, QwNeonRegister dst, QwNeonRegister src1,
                     QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vmin(Qn, Qm) SIMD integer MIN.
  // Instruction details available in ARM DDI 0406C.b, A8-926.
  emit(EncodeNeonMinMax(true, dt, dst, src1, src2));
}

void Assembler::vmax(QwNeonRegister dst, QwNeonRegister src1,
                     QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vmax(Qn, Qm) SIMD floating point MAX.
  // Instruction details available in ARM DDI 0406C.b, A8-928.
  emit(EncodeNeonMinMax(false, dst, src1, src2));
}

void Assembler::vmax(NeonDataType dt, QwNeonRegister dst, QwNeonRegister src1,
                     QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vmax(Qn, Qm) SIMD integer MAX.
  // Instruction details available in ARM DDI 0406C.b, A8-926.
  emit(EncodeNeonMinMax(false, dt, dst, src1, src2));
}

static Instr EncodeNeonEstimateOp(bool is_rsqrt, QwNeonRegister dst,
                                  QwNeonRegister src) {
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  int rsqrt = is_rsqrt ? 1 : 0;
  return 0x1E7U * B23 | d * B22 | 0x3B * B16 | vd * B12 | 0x5 * B8 |
         rsqrt * B7 | B6 | m * B5 | vm;
}

void Assembler::vrecpe(const QwNeonRegister dst, const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  // Qd = vrecpe(Qm) SIMD reciprocal estimate.
  // Instruction details available in ARM DDI 0406C.b, A8-1024.
  emit(EncodeNeonEstimateOp(false, dst, src));
}

void Assembler::vrsqrte(const QwNeonRegister dst, const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  // Qd = vrsqrte(Qm) SIMD reciprocal square root estimate.
  // Instruction details available in ARM DDI 0406C.b, A8-1038.
  emit(EncodeNeonEstimateOp(true, dst, src));
}

static Instr EncodeNeonRefinementOp(bool is_rsqrt, QwNeonRegister dst,
                                    QwNeonRegister src1, QwNeonRegister src2) {
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int rsqrt = is_rsqrt ? 1 : 0;
  return 0x1E4U * B23 | d * B22 | rsqrt * B21 | vn * B16 | vd * B12 | 0xF * B8 |
         n * B7 | B6 | m * B5 | B4 | vm;
}

void Assembler::vrecps(const QwNeonRegister dst, const QwNeonRegister src1,
                       const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vrecps(Qn, Qm) SIMD reciprocal refinement step.
  // Instruction details available in ARM DDI 0406C.b, A8-1026.
  emit(EncodeNeonRefinementOp(false, dst, src1, src2));
}

void Assembler::vrsqrts(const QwNeonRegister dst, const QwNeonRegister src1,
                        const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vrsqrts(Qn, Qm) SIMD reciprocal square root refinement step.
  // Instruction details available in ARM DDI 0406C.b, A8-1040.
  emit(EncodeNeonRefinementOp(true, dst, src1, src2));
}

void Assembler::vtst(NeonSize size, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vtst(Qn, Qm) SIMD test integer operands.
  // Instruction details available in ARM DDI 0406C.b, A8-1098.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  emit(0x1E4U * B23 | d * B22 | sz * B20 | vn * B16 | vd * B12 | 0x8 * B8 |
       n * B7 | B6 | m * B5 | B4 | vm);
}

void Assembler::vceq(const QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vceq(Qn, Qm) SIMD floating point compare equal.
  // Instruction details available in ARM DDI 0406C.b, A8-844.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  emit(0x1E4U * B23 | d * B22 | vn * B16 | vd * B12 | 0xe * B8 | n * B7 | B6 |
       m * B5 | vm);
}

void Assembler::vceq(NeonSize size, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vceq(Qn, Qm) SIMD integer compare equal.
  // Instruction details available in ARM DDI 0406C.b, A8-844.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  emit(0x1E6U * B23 | d * B22 | sz * B20 | vn * B16 | vd * B12 | 0x8 * B8 |
       n * B7 | B6 | m * B5 | B4 | vm);
}

static Instr EncodeNeonCompareOp(const QwNeonRegister dst,
                                 const QwNeonRegister src1,
                                 const QwNeonRegister src2, Condition cond) {
  DCHECK(cond == ge || cond == gt);
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int is_gt = (cond == gt) ? 1 : 0;
  return 0x1E6U * B23 | d * B22 | is_gt * B21 | vn * B16 | vd * B12 | 0xe * B8 |
         n * B7 | B6 | m * B5 | vm;
}

static Instr EncodeNeonCompareOp(NeonDataType dt, const QwNeonRegister dst,
                                 const QwNeonRegister src1,
                                 const QwNeonRegister src2, Condition cond) {
  DCHECK(cond == ge || cond == gt);
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  int size = (dt & NeonDataTypeSizeMask) / 2;
  int U = dt & NeonDataTypeUMask;
  int is_ge = (cond == ge) ? 1 : 0;
  return 0x1E4U * B23 | U | d * B22 | size * B20 | vn * B16 | vd * B12 |
         0x3 * B8 | n * B7 | B6 | m * B5 | is_ge * B4 | vm;
}

void Assembler::vcge(const QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vcge(Qn, Qm) SIMD floating point compare greater or equal.
  // Instruction details available in ARM DDI 0406C.b, A8-848.
  emit(EncodeNeonCompareOp(dst, src1, src2, ge));
}

void Assembler::vcge(NeonDataType dt, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vcge(Qn, Qm) SIMD integer compare greater or equal.
  // Instruction details available in ARM DDI 0406C.b, A8-848.
  emit(EncodeNeonCompareOp(dt, dst, src1, src2, ge));
}

void Assembler::vcgt(const QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vcgt(Qn, Qm) SIMD floating point compare greater than.
  // Instruction details available in ARM DDI 0406C.b, A8-852.
  emit(EncodeNeonCompareOp(dst, src1, src2, gt));
}

void Assembler::vcgt(NeonDataType dt, QwNeonRegister dst,
                     const QwNeonRegister src1, const QwNeonRegister src2) {
  DCHECK(IsEnabled(NEON));
  // Qd = vcgt(Qn, Qm) SIMD integer compare greater than.
  // Instruction details available in ARM DDI 0406C.b, A8-852.
  emit(EncodeNeonCompareOp(dt, dst, src1, src2, gt));
}

void Assembler::vext(QwNeonRegister dst, const QwNeonRegister src1,
                     const QwNeonRegister src2, int bytes) {
  DCHECK(IsEnabled(NEON));
  // Qd = vext(Qn, Qm) SIMD byte extract.
  // Instruction details available in ARM DDI 0406C.b, A8-890.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  src1.split_code(&vn, &n);
  int vm, m;
  src2.split_code(&vm, &m);
  DCHECK_GT(16, bytes);
  emit(0x1E5U * B23 | d * B22 | 0x3 * B20 | vn * B16 | vd * B12 | bytes * B8 |
       n * B7 | B6 | m * B5 | vm);
}

void Assembler::vzip(NeonSize size, QwNeonRegister dst,
                     const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  // Qd = vzip.<size>(Qn, Qm) SIMD zip (interleave).
  // Instruction details available in ARM DDI 0406C.b, A8-1102.
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  emit(0x1E7U * B23 | d * B22 | 0x3 * B20 | sz * B18 | 2 * B16 | vd * B12 |
       0x3 * B7 | B6 | m * B5 | vm);
}

static Instr EncodeNeonVREV(NeonSize op_size, NeonSize size,
                            const QwNeonRegister dst,
                            const QwNeonRegister src) {
  // Qd = vrev<op_size>.<size>(Qn, Qm) SIMD scalar reverse.
  // Instruction details available in ARM DDI 0406C.b, A8-1028.
  DCHECK_GT(op_size, static_cast<int>(size));
  int vd, d;
  dst.split_code(&vd, &d);
  int vm, m;
  src.split_code(&vm, &m);
  int sz = static_cast<int>(size);
  int op = static_cast<int>(Neon64) - static_cast<int>(op_size);
  return 0x1E7U * B23 | d * B22 | 0x3 * B20 | sz * B18 | vd * B12 | op * B7 |
         B6 | m * B5 | vm;
}

void Assembler::vrev16(NeonSize size, const QwNeonRegister dst,
                       const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonVREV(Neon16, size, dst, src));
}

void Assembler::vrev32(NeonSize size, const QwNeonRegister dst,
                       const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonVREV(Neon32, size, dst, src));
}

void Assembler::vrev64(NeonSize size, const QwNeonRegister dst,
                       const QwNeonRegister src) {
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonVREV(Neon64, size, dst, src));
}

// Encode NEON vtbl / vtbx instruction.
static Instr EncodeNeonVTB(const DwVfpRegister dst, const NeonListOperand& list,
                           const DwVfpRegister index, bool vtbx) {
  // Dd = vtbl(table, Dm) SIMD vector permute, zero at out of range indices.
  // Instruction details available in ARM DDI 0406C.b, A8-1094.
  // Dd = vtbx(table, Dm) SIMD vector permute, skip out of range indices.
  // Instruction details available in ARM DDI 0406C.b, A8-1094.
  int vd, d;
  dst.split_code(&vd, &d);
  int vn, n;
  list.base().split_code(&vn, &n);
  int vm, m;
  index.split_code(&vm, &m);
  int op = vtbx ? 1 : 0;  // vtbl = 0, vtbx = 1.
  return 0x1E7U * B23 | d * B22 | 0x3 * B20 | vn * B16 | vd * B12 | 0x2 * B10 |
         list.length() * B8 | n * B7 | op * B6 | m * B5 | vm;
}

void Assembler::vtbl(const DwVfpRegister dst, const NeonListOperand& list,
                     const DwVfpRegister index) {
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonVTB(dst, list, index, false));
}

void Assembler::vtbx(const DwVfpRegister dst, const NeonListOperand& list,
                     const DwVfpRegister index) {
  DCHECK(IsEnabled(NEON));
  emit(EncodeNeonVTB(dst, list, index, true));
}

// Pseudo instructions.
void Assembler::nop(int type) {
  // ARMv6{K/T2} and v7 have an actual NOP instruction but it serializes
  // some of the CPU's pipeline and has to issue. Older ARM chips simply used
  // MOV Rx, Rx as NOP and it performs better even in newer CPUs.
  // We therefore use MOV Rx, Rx, even on newer CPUs, and use Rx to encode
  // a type.
  DCHECK(0 <= type && type <= 14);  // mov pc, pc isn't a nop.
  emit(al | 13*B21 | type*B12 | type);
}


bool Assembler::IsMovT(Instr instr) {
  instr &= ~(((kNumberOfConditions - 1) << 28) |  // Mask off conditions
             ((kNumRegisters-1)*B12) |            // mask out register
             EncodeMovwImmediate(0xFFFF));        // mask out immediate value
  return instr == kMovtPattern;
}


bool Assembler::IsMovW(Instr instr) {
  instr &= ~(((kNumberOfConditions - 1) << 28) |  // Mask off conditions
             ((kNumRegisters-1)*B12) |            // mask out destination
             EncodeMovwImmediate(0xFFFF));        // mask out immediate value
  return instr == kMovwPattern;
}


Instr Assembler::GetMovTPattern() { return kMovtPattern; }


Instr Assembler::GetMovWPattern() { return kMovwPattern; }


Instr Assembler::EncodeMovwImmediate(uint32_t immediate) {
  DCHECK(immediate < 0x10000);
  return ((immediate & 0xf000) << 4) | (immediate & 0xfff);
}


Instr Assembler::PatchMovwImmediate(Instr instruction, uint32_t immediate) {
  instruction &= ~EncodeMovwImmediate(0xffff);
  return instruction | EncodeMovwImmediate(immediate);
}


int Assembler::DecodeShiftImm(Instr instr) {
  int rotate = Instruction::RotateValue(instr) * 2;
  int immed8 = Instruction::Immed8Value(instr);
  return base::bits::RotateRight32(immed8, rotate);
}


Instr Assembler::PatchShiftImm(Instr instr, int immed) {
  uint32_t rotate_imm = 0;
  uint32_t immed_8 = 0;
  bool immed_fits = fits_shifter(immed, &rotate_imm, &immed_8, NULL);
  DCHECK(immed_fits);
  USE(immed_fits);
  return (instr & ~kOff12Mask) | (rotate_imm << 8) | immed_8;
}


bool Assembler::IsNop(Instr instr, int type) {
  DCHECK(0 <= type && type <= 14);  // mov pc, pc isn't a nop.
  // Check for mov rx, rx where x = type.
  return instr == (al | 13*B21 | type*B12 | type);
}


bool Assembler::IsMovImmed(Instr instr) {
  return (instr & kMovImmedMask) == kMovImmedPattern;
}


bool Assembler::IsOrrImmed(Instr instr) {
  return (instr & kOrrImmedMask) == kOrrImmedPattern;
}


// static
bool Assembler::ImmediateFitsAddrMode1Instruction(int32_t imm32) {
  uint32_t dummy1;
  uint32_t dummy2;
  return fits_shifter(imm32, &dummy1, &dummy2, NULL);
}


bool Assembler::ImmediateFitsAddrMode2Instruction(int32_t imm32) {
  return is_uint12(abs(imm32));
}


// Debugging.
void Assembler::RecordConstPool(int size) {
  // We only need this for debugger support, to correctly compute offsets in the
  // code.
  RecordRelocInfo(RelocInfo::CONST_POOL, static_cast<intptr_t>(size));
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
  CHECK_GT(desc.buffer_size, 0);  // no overflow

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);

  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc.origin = this;

  // Copy the data.
  int pc_delta = desc.buffer - buffer_;
  int rc_delta = (desc.buffer + desc.buffer_size) - (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(reloc_info_writer.pos() + rc_delta, reloc_info_writer.pos(),
          desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // None of our relocation types are pc relative pointing outside the code
  // buffer nor pc absolute pointing inside the code buffer, so there is no need
  // to relocate any emitted relocation entries.
}


void Assembler::db(uint8_t data) {
  // db is used to write raw data. The constant pool should be emitted or
  // blocked before using db.
  DCHECK(is_const_pool_blocked() || pending_32_bit_constants_.empty());
  DCHECK(is_const_pool_blocked() || pending_64_bit_constants_.empty());
  CheckBuffer();
  *reinterpret_cast<uint8_t*>(pc_) = data;
  pc_ += sizeof(uint8_t);
}


void Assembler::dd(uint32_t data) {
  // dd is used to write raw data. The constant pool should be emitted or
  // blocked before using dd.
  DCHECK(is_const_pool_blocked() || pending_32_bit_constants_.empty());
  DCHECK(is_const_pool_blocked() || pending_64_bit_constants_.empty());
  CheckBuffer();
  *reinterpret_cast<uint32_t*>(pc_) = data;
  pc_ += sizeof(uint32_t);
}


void Assembler::dq(uint64_t value) {
  // dq is used to write raw data. The constant pool should be emitted or
  // blocked before using dq.
  DCHECK(is_const_pool_blocked() || pending_32_bit_constants_.empty());
  DCHECK(is_const_pool_blocked() || pending_64_bit_constants_.empty());
  CheckBuffer();
  *reinterpret_cast<uint64_t*>(pc_) = value;
  pc_ += sizeof(uint64_t);
}


void Assembler::emit_code_stub_address(Code* stub) {
  CheckBuffer();
  *reinterpret_cast<uint32_t*>(pc_) =
      reinterpret_cast<uint32_t>(stub->instruction_start());
  pc_ += sizeof(uint32_t);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  if (RelocInfo::IsNone(rmode) ||
      // Don't record external references unless the heap will be serialized.
      (rmode == RelocInfo::EXTERNAL_REFERENCE && !serializer_enabled() &&
       !emit_debug_code())) {
    return;
  }
  DCHECK(buffer_space() >= kMaxRelocSize);  // too late to grow buffer here
  if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
    data = RecordedAstId().ToInt();
    ClearRecordedAstId();
  }
  RelocInfo rinfo(isolate(), pc_, rmode, data, NULL);
  reloc_info_writer.Write(&rinfo);
}


ConstantPoolEntry::Access Assembler::ConstantPoolAddEntry(int position,
                                                          RelocInfo::Mode rmode,
                                                          intptr_t value) {
  DCHECK(rmode != RelocInfo::COMMENT && rmode != RelocInfo::CONST_POOL &&
         rmode != RelocInfo::NONE64);
  bool sharing_ok = RelocInfo::IsNone(rmode) ||
                    !(serializer_enabled() || rmode < RelocInfo::CELL);
  if (FLAG_enable_embedded_constant_pool) {
    return constant_pool_builder_.AddEntry(position, value, sharing_ok);
  } else {
    DCHECK(pending_32_bit_constants_.size() < kMaxNumPending32Constants);
    if (pending_32_bit_constants_.empty()) {
      first_const_pool_32_use_ = position;
    }
    ConstantPoolEntry entry(position, value, sharing_ok);
    pending_32_bit_constants_.push_back(entry);

    // Make sure the constant pool is not emitted in place of the next
    // instruction for which we just recorded relocation info.
    BlockConstPoolFor(1);
    return ConstantPoolEntry::REGULAR;
  }
}


ConstantPoolEntry::Access Assembler::ConstantPoolAddEntry(int position,
                                                          double value) {
  if (FLAG_enable_embedded_constant_pool) {
    return constant_pool_builder_.AddEntry(position, value);
  } else {
    DCHECK(pending_64_bit_constants_.size() < kMaxNumPending64Constants);
    if (pending_64_bit_constants_.empty()) {
      first_const_pool_64_use_ = position;
    }
    ConstantPoolEntry entry(position, value);
    pending_64_bit_constants_.push_back(entry);

    // Make sure the constant pool is not emitted in place of the next
    // instruction for which we just recorded relocation info.
    BlockConstPoolFor(1);
    return ConstantPoolEntry::REGULAR;
  }
}


void Assembler::BlockConstPoolFor(int instructions) {
  if (FLAG_enable_embedded_constant_pool) {
    // Should be a no-op if using an embedded constant pool.
    DCHECK(pending_32_bit_constants_.empty());
    DCHECK(pending_64_bit_constants_.empty());
    return;
  }

  int pc_limit = pc_offset() + instructions * kInstrSize;
  if (no_const_pool_before_ < pc_limit) {
    // Max pool start (if we need a jump and an alignment).
#ifdef DEBUG
    int start = pc_limit + kInstrSize + 2 * kPointerSize;
    DCHECK(pending_32_bit_constants_.empty() ||
           (start - first_const_pool_32_use_ +
                pending_64_bit_constants_.size() * kDoubleSize <
            kMaxDistToIntPool));
    DCHECK(pending_64_bit_constants_.empty() ||
           (start - first_const_pool_64_use_ < kMaxDistToFPPool));
#endif
    no_const_pool_before_ = pc_limit;
  }

  if (next_buffer_check_ < no_const_pool_before_) {
    next_buffer_check_ = no_const_pool_before_;
  }
}


void Assembler::CheckConstPool(bool force_emit, bool require_jump) {
  if (FLAG_enable_embedded_constant_pool) {
    // Should be a no-op if using an embedded constant pool.
    DCHECK(pending_32_bit_constants_.empty());
    DCHECK(pending_64_bit_constants_.empty());
    return;
  }

  // Some short sequence of instruction mustn't be broken up by constant pool
  // emission, such sequences are protected by calls to BlockConstPoolFor and
  // BlockConstPoolScope.
  if (is_const_pool_blocked()) {
    // Something is wrong if emission is forced and blocked at the same time.
    DCHECK(!force_emit);
    return;
  }

  // There is nothing to do if there are no pending constant pool entries.
  if (pending_32_bit_constants_.empty() && pending_64_bit_constants_.empty()) {
    // Calculate the offset of the next check.
    next_buffer_check_ = pc_offset() + kCheckPoolInterval;
    return;
  }

  // Check that the code buffer is large enough before emitting the constant
  // pool (include the jump over the pool and the constant pool marker and
  // the gap to the relocation information).
  int jump_instr = require_jump ? kInstrSize : 0;
  int size_up_to_marker = jump_instr + kInstrSize;
  int estimated_size_after_marker =
      pending_32_bit_constants_.size() * kPointerSize;
  bool has_int_values = !pending_32_bit_constants_.empty();
  bool has_fp_values = !pending_64_bit_constants_.empty();
  bool require_64_bit_align = false;
  if (has_fp_values) {
    require_64_bit_align =
        !IsAligned(reinterpret_cast<intptr_t>(pc_ + size_up_to_marker),
                   kDoubleAlignment);
    if (require_64_bit_align) {
      estimated_size_after_marker += kInstrSize;
    }
    estimated_size_after_marker +=
        pending_64_bit_constants_.size() * kDoubleSize;
  }
  int estimated_size = size_up_to_marker + estimated_size_after_marker;

  // We emit a constant pool when:
  //  * requested to do so by parameter force_emit (e.g. after each function).
  //  * the distance from the first instruction accessing the constant pool to
  //    any of the constant pool entries will exceed its limit the next
  //    time the pool is checked. This is overly restrictive, but we don't emit
  //    constant pool entries in-order so it's conservatively correct.
  //  * the instruction doesn't require a jump after itself to jump over the
  //    constant pool, and we're getting close to running out of range.
  if (!force_emit) {
    DCHECK(has_fp_values || has_int_values);
    bool need_emit = false;
    if (has_fp_values) {
      // The 64-bit constants are always emitted before the 32-bit constants, so
      // we can ignore the effect of the 32-bit constants on estimated_size.
      int dist64 = pc_offset() + estimated_size -
                   pending_32_bit_constants_.size() * kPointerSize -
                   first_const_pool_64_use_;
      if ((dist64 >= kMaxDistToFPPool - kCheckPoolInterval) ||
          (!require_jump && (dist64 >= kMaxDistToFPPool / 2))) {
        need_emit = true;
      }
    }
    if (has_int_values) {
      int dist32 = pc_offset() + estimated_size - first_const_pool_32_use_;
      if ((dist32 >= kMaxDistToIntPool - kCheckPoolInterval) ||
          (!require_jump && (dist32 >= kMaxDistToIntPool / 2))) {
        need_emit = true;
      }
    }
    if (!need_emit) return;
  }

  // Deduplicate constants.
  int size_after_marker = estimated_size_after_marker;
  for (size_t i = 0; i < pending_64_bit_constants_.size(); i++) {
    ConstantPoolEntry& entry = pending_64_bit_constants_[i];
    DCHECK(!entry.is_merged());
    for (size_t j = 0; j < i; j++) {
      if (entry.value64() == pending_64_bit_constants_[j].value64()) {
        DCHECK(!pending_64_bit_constants_[j].is_merged());
        entry.set_merged_index(j);
        size_after_marker -= kDoubleSize;
        break;
      }
    }
  }

  for (size_t i = 0; i < pending_32_bit_constants_.size(); i++) {
    ConstantPoolEntry& entry = pending_32_bit_constants_[i];
    DCHECK(!entry.is_merged());
    if (!entry.sharing_ok()) continue;
    for (size_t j = 0; j < i; j++) {
      if (entry.value() == pending_32_bit_constants_[j].value()) {
        DCHECK(!pending_32_bit_constants_[j].is_merged());
        entry.set_merged_index(j);
        size_after_marker -= kPointerSize;
        break;
      }
    }
  }

  int size = size_up_to_marker + size_after_marker;

  int needed_space = size + kGap;
  while (buffer_space() <= needed_space) GrowBuffer();

  {
    // Block recursive calls to CheckConstPool.
    BlockConstPoolScope block_const_pool(this);
    RecordComment("[ Constant Pool");
    RecordConstPool(size);

    Label size_check;
    bind(&size_check);

    // Emit jump over constant pool if necessary.
    Label after_pool;
    if (require_jump) {
      b(&after_pool);
    }

    // Put down constant pool marker "Undefined instruction".
    // The data size helps disassembly know what to print.
    emit(kConstantPoolMarker |
         EncodeConstantPoolLength(size_after_marker / kPointerSize));

    if (require_64_bit_align) {
      emit(kConstantPoolMarker);
    }

    // Emit 64-bit constant pool entries first: their range is smaller than
    // 32-bit entries.
    for (size_t i = 0; i < pending_64_bit_constants_.size(); i++) {
      ConstantPoolEntry& entry = pending_64_bit_constants_[i];

      Instr instr = instr_at(entry.position());
      // Instruction to patch must be 'vldr rd, [pc, #offset]' with offset == 0.
      DCHECK((IsVldrDPcImmediateOffset(instr) &&
              GetVldrDRegisterImmediateOffset(instr) == 0));

      int delta = pc_offset() - entry.position() - kPcLoadDelta;
      DCHECK(is_uint10(delta));

      if (entry.is_merged()) {
        ConstantPoolEntry& merged =
            pending_64_bit_constants_[entry.merged_index()];
        DCHECK(entry.value64() == merged.value64());
        Instr merged_instr = instr_at(merged.position());
        DCHECK(IsVldrDPcImmediateOffset(merged_instr));
        delta = GetVldrDRegisterImmediateOffset(merged_instr);
        delta += merged.position() - entry.position();
      }
      instr_at_put(entry.position(),
                   SetVldrDRegisterImmediateOffset(instr, delta));
      if (!entry.is_merged()) {
        DCHECK(IsAligned(reinterpret_cast<intptr_t>(pc_), kDoubleAlignment));
        dq(entry.value64());
      }
    }

    // Emit 32-bit constant pool entries.
    for (size_t i = 0; i < pending_32_bit_constants_.size(); i++) {
      ConstantPoolEntry& entry = pending_32_bit_constants_[i];
      Instr instr = instr_at(entry.position());

      // 64-bit loads shouldn't get here.
      DCHECK(!IsVldrDPcImmediateOffset(instr));
      DCHECK(!IsMovW(instr));
      DCHECK(IsLdrPcImmediateOffset(instr) &&
             GetLdrRegisterImmediateOffset(instr) == 0);

      int delta = pc_offset() - entry.position() - kPcLoadDelta;
      DCHECK(is_uint12(delta));
      // 0 is the smallest delta:
      //   ldr rd, [pc, #0]
      //   constant pool marker
      //   data

      if (entry.is_merged()) {
        DCHECK(entry.sharing_ok());
        ConstantPoolEntry& merged =
            pending_32_bit_constants_[entry.merged_index()];
        DCHECK(entry.value() == merged.value());
        Instr merged_instr = instr_at(merged.position());
        DCHECK(IsLdrPcImmediateOffset(merged_instr));
        delta = GetLdrRegisterImmediateOffset(merged_instr);
        delta += merged.position() - entry.position();
      }
      instr_at_put(entry.position(),
                   SetLdrRegisterImmediateOffset(instr, delta));
      if (!entry.is_merged()) {
        emit(entry.value());
      }
    }

    pending_32_bit_constants_.clear();
    pending_64_bit_constants_.clear();
    first_const_pool_32_use_ = -1;
    first_const_pool_64_use_ = -1;

    RecordComment("]");

    DCHECK_EQ(size, SizeOfCodeGeneratedSince(&size_check));

    if (after_pool.is_linked()) {
      bind(&after_pool);
    }
  }

  // Since a constant pool was just emitted, move the check offset forward by
  // the standard interval.
  next_buffer_check_ = pc_offset() + kCheckPoolInterval;
}


void Assembler::PatchConstantPoolAccessInstruction(
    int pc_offset, int offset, ConstantPoolEntry::Access access,
    ConstantPoolEntry::Type type) {
  DCHECK(FLAG_enable_embedded_constant_pool);
  Address pc = buffer_ + pc_offset;

  // Patch vldr/ldr instruction with correct offset.
  Instr instr = instr_at(pc);
  if (access == ConstantPoolEntry::OVERFLOWED) {
    if (CpuFeatures::IsSupported(ARMv7)) {
      CpuFeatureScope scope(this, ARMv7);
      // Instructions to patch must be 'movw rd, [#0]' and 'movt rd, [#0].
      Instr next_instr = instr_at(pc + kInstrSize);
      DCHECK((IsMovW(instr) && Instruction::ImmedMovwMovtValue(instr) == 0));
      DCHECK((IsMovT(next_instr) &&
              Instruction::ImmedMovwMovtValue(next_instr) == 0));
      instr_at_put(pc, PatchMovwImmediate(instr, offset & 0xffff));
      instr_at_put(pc + kInstrSize,
                   PatchMovwImmediate(next_instr, offset >> 16));
    } else {
      // Instructions to patch must be 'mov rd, [#0]' and 'orr rd, rd, [#0].
      Instr instr_2 = instr_at(pc + kInstrSize);
      Instr instr_3 = instr_at(pc + 2 * kInstrSize);
      Instr instr_4 = instr_at(pc + 3 * kInstrSize);
      DCHECK((IsMovImmed(instr) && Instruction::Immed8Value(instr) == 0));
      DCHECK((IsOrrImmed(instr_2) && Instruction::Immed8Value(instr_2) == 0) &&
             GetRn(instr_2).is(GetRd(instr_2)));
      DCHECK((IsOrrImmed(instr_3) && Instruction::Immed8Value(instr_3) == 0) &&
             GetRn(instr_3).is(GetRd(instr_3)));
      DCHECK((IsOrrImmed(instr_4) && Instruction::Immed8Value(instr_4) == 0) &&
             GetRn(instr_4).is(GetRd(instr_4)));
      instr_at_put(pc, PatchShiftImm(instr, (offset & kImm8Mask)));
      instr_at_put(pc + kInstrSize,
                   PatchShiftImm(instr_2, (offset & (kImm8Mask << 8))));
      instr_at_put(pc + 2 * kInstrSize,
                   PatchShiftImm(instr_3, (offset & (kImm8Mask << 16))));
      instr_at_put(pc + 3 * kInstrSize,
                   PatchShiftImm(instr_4, (offset & (kImm8Mask << 24))));
    }
  } else if (type == ConstantPoolEntry::DOUBLE) {
    // Instruction to patch must be 'vldr rd, [pp, #0]'.
    DCHECK((IsVldrDPpImmediateOffset(instr) &&
            GetVldrDRegisterImmediateOffset(instr) == 0));
    DCHECK(is_uint10(offset));
    instr_at_put(pc, SetVldrDRegisterImmediateOffset(instr, offset));
  } else {
    // Instruction to patch must be 'ldr rd, [pp, #0]'.
    DCHECK((IsLdrPpImmediateOffset(instr) &&
            GetLdrRegisterImmediateOffset(instr) == 0));
    DCHECK(is_uint12(offset));
    instr_at_put(pc, SetLdrRegisterImmediateOffset(instr, offset));
  }
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

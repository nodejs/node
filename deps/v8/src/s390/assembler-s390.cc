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

#include "src/s390/assembler-s390.h"
#include <sys/auxv.h>
#include <set>
#include <string>

#if V8_TARGET_ARCH_S390

#if V8_HOST_ARCH_S390
#include <elf.h>  // Required for auxv checks for STFLE support
#endif

#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/code-stubs.h"
#include "src/macro-assembler.h"
#include "src/s390/assembler-s390-inl.h"

namespace v8 {
namespace internal {

// Get the CPU features enabled by the build.
static unsigned CpuFeaturesImpliedByCompiler() {
  unsigned answer = 0;
  return answer;
}

static bool supportsCPUFeature(const char* feature) {
  static std::set<std::string> features;
  static std::set<std::string> all_available_features = {
      "iesan3", "zarch",  "stfle",    "msa", "ldisp", "eimm",
      "dfp",    "etf3eh", "highgprs", "te",  "vx"};
  if (features.empty()) {
#if V8_HOST_ARCH_S390

#ifndef HWCAP_S390_VX
#define HWCAP_S390_VX 2048
#endif
#define CHECK_AVAILABILITY_FOR(mask, value) \
  if (f & mask) features.insert(value);

    // initialize feature vector
    uint64_t f = getauxval(AT_HWCAP);
    CHECK_AVAILABILITY_FOR(HWCAP_S390_ESAN3, "iesan3")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_ZARCH, "zarch")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_STFLE, "stfle")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_MSA, "msa")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_LDISP, "ldisp")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_EIMM, "eimm")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_DFP, "dfp")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_ETF3EH, "etf3eh")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_HIGH_GPRS, "highgprs")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_TE, "te")
    CHECK_AVAILABILITY_FOR(HWCAP_S390_VX, "vx")
#else
    // import all features
    features.insert(all_available_features.begin(),
                    all_available_features.end());
#endif
  }
  USE(all_available_features);
  return features.find(feature) != features.end();
}

// Check whether Store Facility STFLE instruction is available on the platform.
// Instruction returns a bit vector of the enabled hardware facilities.
static bool supportsSTFLE() {
#if V8_HOST_ARCH_S390
  static bool read_tried = false;
  static uint32_t auxv_hwcap = 0;

  if (!read_tried) {
    // Open the AUXV (auxiliary vector) pseudo-file
    int fd = open("/proc/self/auxv", O_RDONLY);

    read_tried = true;
    if (fd != -1) {
#if V8_TARGET_ARCH_S390X
      static Elf64_auxv_t buffer[16];
      Elf64_auxv_t* auxv_element;
#else
      static Elf32_auxv_t buffer[16];
      Elf32_auxv_t* auxv_element;
#endif
      int bytes_read = 0;
      while (bytes_read >= 0) {
        // Read a chunk of the AUXV
        bytes_read = read(fd, buffer, sizeof(buffer));
        // Locate and read the platform field of AUXV if it is in the chunk
        for (auxv_element = buffer;
             auxv_element + sizeof(auxv_element) <= buffer + bytes_read &&
             auxv_element->a_type != AT_NULL;
             auxv_element++) {
          // We are looking for HWCAP entry in AUXV to search for STFLE support
          if (auxv_element->a_type == AT_HWCAP) {
            /* Note: Both auxv_hwcap and buffer are static */
            auxv_hwcap = auxv_element->a_un.a_val;
            goto done_reading;
          }
        }
      }
    done_reading:
      close(fd);
    }
  }

  // Did not find result
  if (0 == auxv_hwcap) {
    return false;
  }

  // HWCAP_S390_STFLE is defined to be 4 in include/asm/elf.h.  Currently
  // hardcoded in case that include file does not exist.
  const uint32_t _HWCAP_S390_STFLE = 4;
  return (auxv_hwcap & _HWCAP_S390_STFLE);
#else
  // STFLE is not available on non-s390 hosts
  return false;
#endif
}

void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();
  icache_line_size_ = 256;

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

#ifdef DEBUG
  initialized_ = true;
#endif

  static bool performSTFLE = supportsSTFLE();

// Need to define host, as we are generating inlined S390 assembly to test
// for facilities.
#if V8_HOST_ARCH_S390
  if (performSTFLE) {
    // STFLE D(B) requires:
    //    GPR0 to specify # of double words to update minus 1.
    //      i.e. GPR0 = 0 for 1 doubleword
    //    D(B) to specify to memory location to store the facilities bits
    // The facilities we are checking for are:
    //   Bit 45 - Distinct Operands for instructions like ARK, SRK, etc.
    // As such, we require only 1 double word
    int64_t facilities[3] = {0L};
    // LHI sets up GPR0
    // STFLE is specified as .insn, as opcode is not recognized.
    // We register the instructions kill r0 (LHI) and the CC (STFLE).
    asm volatile(
        "lhi   0,2\n"
        ".insn s,0xb2b00000,%0\n"
        : "=Q"(facilities)
        :
        : "cc", "r0");

    uint64_t one = static_cast<uint64_t>(1);
    // Test for Distinct Operands Facility - Bit 45
    if (facilities[0] & (one << (63 - 45))) {
      supported_ |= (1u << DISTINCT_OPS);
    }
    // Test for General Instruction Extension Facility - Bit 34
    if (facilities[0] & (one << (63 - 34))) {
      supported_ |= (1u << GENERAL_INSTR_EXT);
    }
    // Test for Floating Point Extension Facility - Bit 37
    if (facilities[0] & (one << (63 - 37))) {
      supported_ |= (1u << FLOATING_POINT_EXT);
    }
    // Test for Vector Facility - Bit 129
    if (facilities[2] & (one << (63 - (129 - 128))) &&
        supportsCPUFeature("vx")) {
      supported_ |= (1u << VECTOR_FACILITY);
    }
    // Test for Miscellaneous Instruction Extension Facility - Bit 58
    if (facilities[0] & (1lu << (63 - 58))) {
      supported_ |= (1u << MISC_INSTR_EXT2);
    }
  }
#else
  // All distinct ops instructions can be simulated
  supported_ |= (1u << DISTINCT_OPS);
  // RISBG can be simulated
  supported_ |= (1u << GENERAL_INSTR_EXT);
  supported_ |= (1u << FLOATING_POINT_EXT);
  supported_ |= (1u << MISC_INSTR_EXT2);
  USE(performSTFLE);  // To avoid assert
  USE(supportsCPUFeature);
  supported_ |= (1u << VECTOR_FACILITY);
#endif
  supported_ |= (1u << FPU);
}

void CpuFeatures::PrintTarget() {
  const char* s390_arch = nullptr;

#if V8_TARGET_ARCH_S390X
  s390_arch = "s390x";
#else
  s390_arch = "s390";
#endif

  printf("target %s\n", s390_arch);
}

void CpuFeatures::PrintFeatures() {
  printf("FPU=%d\n", CpuFeatures::IsSupported(FPU));
  printf("FPU_EXT=%d\n", CpuFeatures::IsSupported(FLOATING_POINT_EXT));
  printf("GENERAL_INSTR=%d\n", CpuFeatures::IsSupported(GENERAL_INSTR_EXT));
  printf("DISTINCT_OPS=%d\n", CpuFeatures::IsSupported(DISTINCT_OPS));
  printf("VECTOR_FACILITY=%d\n", CpuFeatures::IsSupported(VECTOR_FACILITY));
  printf("MISC_INSTR_EXT2=%d\n", CpuFeatures::IsSupported(MISC_INSTR_EXT2));
}

Register ToRegister(int num) {
  DCHECK(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] = {r0, r1, r2,  r3, r4, r5,  r6,  r7,
                                 r8, r9, r10, fp, ip, r13, r14, sp};
  return kRegisters[num];
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

const int RelocInfo::kApplyMask =
    RelocInfo::kCodeTargetMask | 1 << RelocInfo::INTERNAL_REFERENCE;

bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially
  // coded.  Being specially coded on S390 means that it is an iihf/iilf
  // instruction sequence, and that is always the case inside code
  // objects.
  return true;
}

bool RelocInfo::IsInConstantPool() { return false; }

Address RelocInfo::embedded_address() const {
  return Assembler::target_address_at(pc_, constant_pool_);
}

uint32_t RelocInfo::embedded_size() const {
  return static_cast<uint32_t>(reinterpret_cast<intptr_t>(
      Assembler::target_address_at(pc_, constant_pool_)));
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
// Implementation of Operand and MemOperand
// See assembler-s390-inl.h for inlined constructors

Operand::Operand(Handle<HeapObject> handle) {
  AllowHandleDereference using_location;
  rm_ = no_reg;
  value_.immediate = reinterpret_cast<intptr_t>(handle.address());
  rmode_ = RelocInfo::EMBEDDED_OBJECT;
}

Operand Operand::EmbeddedNumber(double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) return Operand(Smi::FromInt(smi));
  Operand result(0, RelocInfo::EMBEDDED_OBJECT);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(value);
  return result;
}

MemOperand::MemOperand(Register rn, int32_t offset)
    : baseRegister(rn), indexRegister(r0), offset_(offset) {}

MemOperand::MemOperand(Register rx, Register rb, int32_t offset)
    : baseRegister(rb), indexRegister(rx), offset_(offset) {}

void Assembler::AllocateAndInstallRequestedHeapObjects(Isolate* isolate) {
  for (auto& request : heap_object_requests_) {
    Handle<HeapObject> object;
    Address pc = buffer_ + request.offset();
    switch (request.kind()) {
      case HeapObjectRequest::kHeapNumber:
        object = isolate->factory()->NewHeapNumber(request.heap_number(),
                                                   IMMUTABLE, TENURED);
        set_target_address_at(nullptr, pc, static_cast<Address>(nullptr),
                              reinterpret_cast<Address>(object.location()),
                              SKIP_ICACHE_FLUSH);
        break;
      case HeapObjectRequest::kCodeStub:
        request.code_stub()->set_isolate(isolate);
        SixByteInstr instr =
            Instruction::InstructionBits(reinterpret_cast<const byte*>(pc));
        int index = instr & 0xFFFFFFFF;
        code_targets_[index] = request.code_stub()->GetCode();
        break;
    }
  }
}

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

Assembler::Assembler(IsolateData isolate_data, void* buffer, int buffer_size)
    : AssemblerBase(isolate_data, buffer, buffer_size) {
  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);
  code_targets_.reserve(100);

  last_bound_pos_ = 0;
  relocations_.reserve(128);
}

void Assembler::GetCode(Isolate* isolate, CodeDesc* desc) {
  EmitRelocations();

  AllocateAndInstallRequestedHeapObjects(isolate);

  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc->origin = this;
  desc->unwinding_info_size = 0;
  desc->unwinding_info = nullptr;
}

void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo(m));
  while ((pc_offset() & (m - 1)) != 0) {
    nop(0);
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

#if V8_TARGET_ARCH_S390X
// This code assumes a FIXED_SEQUENCE for 64bit loads (iihf/iilf)
bool Assembler::Is64BitLoadIntoIP(SixByteInstr instr1, SixByteInstr instr2) {
  // Check the instructions are the iihf/iilf load into ip
  return (((instr1 >> 32) == 0xC0C8) && ((instr2 >> 32) == 0xC0C9));
}
#else
// This code assumes a FIXED_SEQUENCE for 32bit loads (iilf)
bool Assembler::Is32BitLoadIntoIP(SixByteInstr instr) {
  // Check the instruction is an iilf load into ip/r12.
  return ((instr >> 32) == 0xC0C9);
}
#endif

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

// Returns the target address of the relative instructions, typically
// of the form: pos + imm (where immediate is in # of halfwords for
// BR* and LARL).
int Assembler::target_at(int pos) {
  SixByteInstr instr = instr_at(pos);
  // check which type of branch this is 16 or 26 bit offset
  Opcode opcode = Instruction::S390OpcodeValue(buffer_ + pos);

  if (BRC == opcode || BRCT == opcode || BRCTG == opcode) {
    int16_t imm16 = SIGN_EXT_IMM16((instr & kImm16Mask));
    imm16 <<= 1;  // BRC immediate is in # of halfwords
    if (imm16 == 0) return kEndOfChain;
    return pos + imm16;
  } else if (LLILF == opcode || BRCL == opcode || LARL == opcode ||
             BRASL == opcode) {
    int32_t imm32 =
        static_cast<int32_t>(instr & (static_cast<uint64_t>(0xFFFFFFFF)));
    if (LLILF != opcode)
      imm32 <<= 1;  // BR* + LARL treat immediate in # of halfwords
    if (imm32 == 0) return kEndOfChain;
    return pos + imm32;
  }

  // Unknown condition
  DCHECK(false);
  return -1;
}

// Update the target address of the current relative instruction.
void Assembler::target_at_put(int pos, int target_pos, bool* is_branch) {
  SixByteInstr instr = instr_at(pos);
  Opcode opcode = Instruction::S390OpcodeValue(buffer_ + pos);

  if (is_branch != nullptr) {
    *is_branch = (opcode == BRC || opcode == BRCT || opcode == BRCTG ||
                  opcode == BRCL || opcode == BRASL);
  }

  if (BRC == opcode || BRCT == opcode || BRCTG == opcode) {
    int16_t imm16 = target_pos - pos;
    instr &= (~0xFFFF);
    DCHECK(is_int16(imm16));
    instr_at_put<FourByteInstr>(pos, instr | (imm16 >> 1));
    return;
  } else if (BRCL == opcode || LARL == opcode || BRASL == opcode) {
    // Immediate is in # of halfwords
    int32_t imm32 = target_pos - pos;
    instr &= (~static_cast<uint64_t>(0xFFFFFFFF));
    instr_at_put<SixByteInstr>(pos, instr | (imm32 >> 1));
    return;
  } else if (LLILF == opcode) {
    DCHECK(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    int32_t imm32 = target_pos + (Code::kHeaderSize - kHeapObjectTag);
    instr &= (~static_cast<uint64_t>(0xFFFFFFFF));
    instr_at_put<SixByteInstr>(pos, instr | imm32);
    return;
  }
  DCHECK(false);
}

// Returns the maximum number of bits given instruction can address.
int Assembler::max_reach_from(int pos) {
  Opcode opcode = Instruction::S390OpcodeValue(buffer_ + pos);

  // Check which type of instr.  In theory, we can return
  // the values below + 1, given offset is # of halfwords
  if (BRC == opcode || BRCT == opcode || BRCTG == opcode) {
    return 16;
  } else if (LLILF == opcode || BRCL == opcode || LARL == opcode ||
             BRASL == opcode) {
    return 31;  // Using 31 as workaround instead of 32 as
                // is_intn(x,32) doesn't work on 32-bit platforms.
                // llilf: Emitted label constant, not part of
                //        a branch (regexp PushBacktrack).
  }
  DCHECK(false);
  return 16;
}

void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // must have a valid binding position
  bool is_branch = false;
  while (L->is_linked()) {
    int fixup_pos = L->pos();
#ifdef DEBUG
    int32_t offset = pos - fixup_pos;
    int maxReach = max_reach_from(fixup_pos);
#endif
    next(L);  // call next before overwriting link with target at fixup_pos
    DCHECK(is_intn(offset, maxReach));
    target_at_put(fixup_pos, pos, &is_branch);
  }
  L->bind_to(pos);

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
    DCHECK_GE(link, 0);
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

void Assembler::load_label_offset(Register r1, Label* L) {
  int target_pos;
  int constant;
  if (L->is_bound()) {
    target_pos = L->pos();
    constant = target_pos + (Code::kHeaderSize - kHeapObjectTag);
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link
    } else {
      // was: target_pos = kEndOfChain;
      // However, using branch to self to mark the first reference
      // should avoid most instances of branch offset overflow.  See
      // target_at() for where this is converted back to kEndOfChain.
      target_pos = pc_offset();
    }
    L->link_to(pc_offset());

    constant = target_pos - pc_offset();
  }
  llilf(r1, Operand(constant));
}

// Pseudo op - branch on condition
void Assembler::branchOnCond(Condition c, int branch_offset, bool is_bound) {
  int offset_in_halfwords = branch_offset / 2;
  if (is_bound && is_int16(offset_in_halfwords)) {
    brc(c, Operand(offset_in_halfwords));  // short jump
  } else {
    brcl(c, Operand(offset_in_halfwords));  // long jump
  }
}

// 32-bit Store Multiple - short displacement (12-bits unsigned)
void Assembler::stm(Register r1, Register r2, const MemOperand& src) {
  rs_form(STM, r1, r2, src.rb(), src.offset());
}

// 32-bit Store Multiple - long displacement (20-bits signed)
void Assembler::stmy(Register r1, Register r2, const MemOperand& src) {
  rsy_form(STMY, r1, r2, src.rb(), src.offset());
}

// 64-bit Store Multiple - long displacement (20-bits signed)
void Assembler::stmg(Register r1, Register r2, const MemOperand& src) {
  rsy_form(STMG, r1, r2, src.rb(), src.offset());
}

// Exception-generating instructions and debugging support.
// Stops with a non-negative code less than kNumOfWatchedStops support
// enabling/disabling and a counter feature. See simulator-s390.h .
void Assembler::stop(const char* msg, Condition cond, int32_t code,
                     CRegister cr) {
  if (cond != al) {
    Label skip;
    b(NegateCondition(cond), &skip, Label::kNear);
    bkpt(0);
    bind(&skip);
  } else {
    bkpt(0);
  }
}

void Assembler::bkpt(uint32_t imm16) {
  // GDB software breakpoint instruction
  emit2bytes(0x0001);
}

// Pseudo instructions.
void Assembler::nop(int type) {
  switch (type) {
    case 0:
      lr(r0, r0);
      break;
    case DEBUG_BREAK_NOP:
      // TODO(john.yan): Use a better NOP break
      oill(r3, Operand::Zero());
      break;
    default:
      UNIMPLEMENTED();
  }
}



// RI1 format: <insn> R1,I2
//    +--------+----+----+------------------+
//    | OpCode | R1 |OpCd|        I2        |
//    +--------+----+----+------------------+
//    0        8    12   16                31
#define RI1_FORM_EMIT(name, op) \
  void Assembler::name(Register r, const Operand& i2) { ri_form(op, r, i2); }

void Assembler::ri_form(Opcode op, Register r1, const Operand& i2) {
  DCHECK(is_uint12(op));
  DCHECK(is_uint16(i2.immediate()) || is_int16(i2.immediate()));
  emit4bytes((op & 0xFF0) * B20 | r1.code() * B20 | (op & 0xF) * B16 |
             (i2.immediate() & 0xFFFF));
}

// RI2 format: <insn> M1,I2
//    +--------+----+----+------------------+
//    | OpCode | M1 |OpCd|        I2        |
//    +--------+----+----+------------------+
//    0        8    12   16                31
#define RI2_FORM_EMIT(name, op) \
  void Assembler::name(Condition m, const Operand& i2) { ri_form(op, m, i2); }

void Assembler::ri_form(Opcode op, Condition m1, const Operand& i2) {
  DCHECK(is_uint12(op));
  DCHECK(is_uint4(m1));
  DCHECK(op == BRC ? is_int16(i2.immediate()) : is_uint16(i2.immediate()));
  emit4bytes((op & 0xFF0) * B20 | m1 * B20 | (op & 0xF) * B16 |
             (i2.immediate() & 0xFFFF));
}

// RIE-f format: <insn> R1,R2,I3,I4,I5
//    +--------+----+----+------------------+--------+--------+
//    | OpCode | R1 | R2 |   I3   |    I4   |   I5   | OpCode |
//    +--------+----+----+------------------+--------+--------+
//    0        8    12   16      24         32       40      47
void Assembler::rie_f_form(Opcode op, Register r1, Register r2,
                           const Operand& i3, const Operand& i4,
                           const Operand& i5) {
  DCHECK(is_uint16(op));
  DCHECK(is_uint8(i3.immediate()));
  DCHECK(is_uint8(i4.immediate()));
  DCHECK(is_uint8(i5.immediate()));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(r2.code())) * B32 |
                  (static_cast<uint64_t>(i3.immediate())) * B24 |
                  (static_cast<uint64_t>(i4.immediate())) * B16 |
                  (static_cast<uint64_t>(i5.immediate())) * B8 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RIE format: <insn> R1,R3,I2
//    +--------+----+----+------------------+--------+--------+
//    | OpCode | R1 | R3 |        I2        |////////| OpCode |
//    +--------+----+----+------------------+--------+--------+
//    0        8    12   16                 32       40      47
#define RIE_FORM_EMIT(name, op)                                       \
  void Assembler::name(Register r1, Register r3, const Operand& i2) { \
    rie_form(op, r1, r3, i2);                                         \
  }

void Assembler::rie_form(Opcode op, Register r1, Register r3,
                         const Operand& i2) {
  DCHECK(is_uint16(op));
  DCHECK(is_int16(i2.immediate()));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(r3.code())) * B32 |
                  (static_cast<uint64_t>(i2.immediate() & 0xFFFF)) * B16 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RS1 format: <insn> R1,R3,D2(B2)
//    +--------+----+----+----+-------------+
//    | OpCode | R1 | R3 | B2 |     D2      |
//    +--------+----+----+----+-------------+
//    0        8    12   16   20           31
#define RS1_FORM_EMIT(name, op)                                            \
  void Assembler::name(Register r1, Register r3, Register b2, Disp d2) {   \
    rs_form(op, r1, r3, b2, d2);                                           \
  }                                                                        \
  void Assembler::name(Register r1, Register r3, const MemOperand& opnd) { \
    name(r1, r3, opnd.getBaseRegister(), opnd.getDisplacement());          \
  }

void Assembler::rs_form(Opcode op, Register r1, Register r3, Register b2,
                        const Disp d2) {
  DCHECK(is_uint12(d2));
  emit4bytes(op * B24 | r1.code() * B20 | r3.code() * B16 | b2.code() * B12 |
             d2);
}

// RS2 format: <insn> R1,M3,D2(B2)
//    +--------+----+----+----+-------------+
//    | OpCode | R1 | M3 | B2 |     D2      |
//    +--------+----+----+----+-------------+
//    0        8    12   16   20           31
#define RS2_FORM_EMIT(name, op)                                             \
  void Assembler::name(Register r1, Condition m3, Register b2, Disp d2) {   \
    rs_form(op, r1, m3, b2, d2);                                            \
  }                                                                         \
  void Assembler::name(Register r1, Condition m3, const MemOperand& opnd) { \
    name(r1, m3, opnd.getBaseRegister(), opnd.getDisplacement());           \
  }

void Assembler::rs_form(Opcode op, Register r1, Condition m3, Register b2,
                        const Disp d2) {
  DCHECK(is_uint12(d2));
  emit4bytes(op * B24 | r1.code() * B20 | m3 * B16 | b2.code() * B12 | d2);
}

// RSI format: <insn> R1,R3,I2
//    +--------+----+----+------------------+
//    | OpCode | R1 | R3 |        RI2       |
//    +--------+----+----+------------------+
//    0        8    12   16                 31
#define RSI_FORM_EMIT(name, op)                                       \
  void Assembler::name(Register r1, Register r3, const Operand& i2) { \
    rsi_form(op, r1, r3, i2);                                         \
  }

void Assembler::rsi_form(Opcode op, Register r1, Register r3,
                         const Operand& i2) {
  DCHECK(is_uint8(op));
  DCHECK(is_uint16(i2.immediate()));
  emit4bytes(op * B24 | r1.code() * B20 | r3.code() * B16 |
             (i2.immediate() & 0xFFFF));
}

// RSL format: <insn> R1,R3,D2(B2)
//    +--------+----+----+----+-------------+--------+--------+
//    | OpCode | L1 |    | B2 |    D2       |        | OpCode |
//    +--------+----+----+----+-------------+--------+--------+
//    0        8    12   16   20            32       40      47
#define RSL_FORM_EMIT(name, op)                           \
  void Assembler::name(Length l1, Register b2, Disp d2) { \
    rsl_form(op, l1, b2, d2);                             \
  }

void Assembler::rsl_form(Opcode op, Length l1, Register b2, Disp d2) {
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(l1)) * B36 |
                  (static_cast<uint64_t>(b2.code())) * B28 |
                  (static_cast<uint64_t>(d2)) * B16 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RSY1 format: <insn> R1,R3,D2(B2)
//    +--------+----+----+----+-------------+--------+--------+
//    | OpCode | R1 | R3 | B2 |    DL2      |  DH2   | OpCode |
//    +--------+----+----+----+-------------+--------+--------+
//    0        8    12   16   20            32       40      47
#define RSY1_FORM_EMIT(name, op)                                           \
  void Assembler::name(Register r1, Register r3, Register b2, Disp d2) {   \
    rsy_form(op, r1, r3, b2, d2);                                          \
  }                                                                        \
  void Assembler::name(Register r1, Register r3, const MemOperand& opnd) { \
    name(r1, r3, opnd.getBaseRegister(), opnd.getDisplacement());          \
  }

void Assembler::rsy_form(Opcode op, Register r1, Register r3, Register b2,
                         const Disp d2) {
  DCHECK(is_int20(d2));
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(r3.code())) * B32 |
                  (static_cast<uint64_t>(b2.code())) * B28 |
                  (static_cast<uint64_t>(d2 & 0x0FFF)) * B16 |
                  (static_cast<uint64_t>(d2 & 0x0FF000)) >> 4 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RSY2 format: <insn> R1,M3,D2(B2)
//    +--------+----+----+----+-------------+--------+--------+
//    | OpCode | R1 | M3 | B2 |    DL2      |  DH2   | OpCode |
//    +--------+----+----+----+-------------+--------+--------+
//    0        8    12   16   20            32       40      47
#define RSY2_FORM_EMIT(name, op)                                            \
  void Assembler::name(Register r1, Condition m3, Register b2, Disp d2) {   \
    rsy_form(op, r1, m3, b2, d2);                                           \
  }                                                                         \
  void Assembler::name(Register r1, Condition m3, const MemOperand& opnd) { \
    name(r1, m3, opnd.getBaseRegister(), opnd.getDisplacement());           \
  }

void Assembler::rsy_form(Opcode op, Register r1, Condition m3, Register b2,
                         const Disp d2) {
  DCHECK(is_int20(d2));
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(m3)) * B32 |
                  (static_cast<uint64_t>(b2.code())) * B28 |
                  (static_cast<uint64_t>(d2 & 0x0FFF)) * B16 |
                  (static_cast<uint64_t>(d2 & 0x0FF000)) >> 4 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RXE format: <insn> R1,D2(X2,B2)
//    +--------+----+----+----+-------------+--------+--------+
//    | OpCode | R1 | X2 | B2 |     D2      |////////| OpCode |
//    +--------+----+----+----+-------------+--------+--------+
//    0        8    12   16   20            32       40      47
#define RXE_FORM_EMIT(name, op)                                          \
  void Assembler::name(Register r1, Register x2, Register b2, Disp d2) { \
    rxe_form(op, r1, x2, b2, d2);                                        \
  }                                                                      \
  void Assembler::name(Register r1, const MemOperand& opnd) {            \
    name(r1, opnd.getIndexRegister(), opnd.getBaseRegister(),            \
         opnd.getDisplacement());                                        \
  }

void Assembler::rxe_form(Opcode op, Register r1, Register x2, Register b2,
                         Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(x2.code())) * B32 |
                  (static_cast<uint64_t>(b2.code())) * B28 |
                  (static_cast<uint64_t>(d2 & 0x0FFF)) * B16 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RRS format: <insn> R1,R2,M3,D4(B4)
//    +--------+----+----+----+-------------+----+---+--------+
//    | OpCode | R1 | R2 | B4 |     D4      | M3 |///| OpCode |
//    +--------+----+----+----+-------------+----+---+--------+
//    0        8    12   16   20            32   36   40      47
#define RRS_FORM_EMIT(name, op)                                        \
  void Assembler::name(Register r1, Register r2, Register b4, Disp d4, \
                       Condition m3) {                                 \
    rrs_form(op, r1, r2, b4, d4, m3);                                  \
  }                                                                    \
  void Assembler::name(Register r1, Register r2, Condition m3,         \
                       const MemOperand& opnd) {                       \
    name(r1, r2, opnd.getBaseRegister(), opnd.getDisplacement(), m3);  \
  }

void Assembler::rrs_form(Opcode op, Register r1, Register r2, Register b4,
                         Disp d4, Condition m3) {
  DCHECK(is_uint12(d4));
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(r2.code())) * B32 |
                  (static_cast<uint64_t>(b4.code())) * B28 |
                  (static_cast<uint64_t>(d4)) * B16 |
                  (static_cast<uint64_t>(m3)) << 12 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// RIS format: <insn> R1,I2,M3,D4(B4)
//    +--------+----+----+----+-------------+--------+--------+
//    | OpCode | R1 | M3 | B4 |     D4      |   I2   | OpCode |
//    +--------+----+----+----+-------------+--------+--------+
//    0        8    12   16   20            32        40      47
#define RIS_FORM_EMIT(name, op)                                         \
  void Assembler::name(Register r1, Condition m3, Register b4, Disp d4, \
                       const Operand& i2) {                             \
    ris_form(op, r1, m3, b4, d4, i2);                                   \
  }                                                                     \
  void Assembler::name(Register r1, const Operand& i2, Condition m3,    \
                       const MemOperand& opnd) {                        \
    name(r1, m3, opnd.getBaseRegister(), opnd.getDisplacement(), i2);   \
  }

void Assembler::ris_form(Opcode op, Register r1, Condition m3, Register b4,
                         Disp d4, const Operand& i2) {
  DCHECK(is_uint12(d4));
  DCHECK(is_uint16(op));
  DCHECK(is_uint8(i2.immediate()));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(m3)) * B32 |
                  (static_cast<uint64_t>(b4.code())) * B28 |
                  (static_cast<uint64_t>(d4)) * B16 |
                  (static_cast<uint64_t>(i2.immediate())) << 8 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// S format: <insn> D2(B2)
//    +------------------+----+-------------+
//    |      OpCode      | B2 |     D2      |
//    +------------------+----+-------------+
//    0                  16   20           31
#define S_FORM_EMIT(name, op)                                        \
  void Assembler::name(Register b1, Disp d2) { s_form(op, b1, d2); } \
  void Assembler::name(const MemOperand& opnd) {                     \
    name(opnd.getBaseRegister(), opnd.getDisplacement());            \
  }

void Assembler::s_form(Opcode op, Register b1, Disp d2) {
  DCHECK(is_uint12(d2));
  emit4bytes(op << 16 | b1.code() * B12 | d2);
}

// SI format: <insn> D1(B1),I2
//    +--------+---------+----+-------------+
//    | OpCode |   I2    | B1 |     D1      |
//    +--------+---------+----+-------------+
//    0        8         16   20           31
#define SI_FORM_EMIT(name, op)                                      \
  void Assembler::name(const Operand& i2, Register b1, Disp d1) {   \
    si_form(op, i2, b1, d1);                                        \
  }                                                                 \
  void Assembler::name(const MemOperand& opnd, const Operand& i2) { \
    name(i2, opnd.getBaseRegister(), opnd.getDisplacement());       \
  }

void Assembler::si_form(Opcode op, const Operand& i2, Register b1, Disp d1) {
  emit4bytes((op & 0x00FF) << 24 | i2.immediate() * B16 | b1.code() * B12 | d1);
}

// SIY format: <insn> D1(B1),I2
//    +--------+---------+----+-------------+--------+--------+
//    | OpCode |   I2    | B1 |     DL1     |  DH1   | OpCode |
//    +--------+---------+----+-------------+--------+--------+
//    0        8         16   20            32   36   40      47
#define SIY_FORM_EMIT(name, op)                                     \
  void Assembler::name(const Operand& i2, Register b1, Disp d1) {   \
    siy_form(op, i2, b1, d1);                                       \
  }                                                                 \
  void Assembler::name(const MemOperand& opnd, const Operand& i2) { \
    name(i2, opnd.getBaseRegister(), opnd.getDisplacement());       \
  }

void Assembler::siy_form(Opcode op, const Operand& i2, Register b1, Disp d1) {
  DCHECK(is_uint20(d1) || is_int20(d1));
  DCHECK(is_uint16(op));
  DCHECK(is_uint8(i2.immediate()));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(i2.immediate())) * B32 |
                  (static_cast<uint64_t>(b1.code())) * B28 |
                  (static_cast<uint64_t>(d1 & 0x0FFF)) * B16 |
                  (static_cast<uint64_t>(d1 & 0x0FF000)) >> 4 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// SIL format: <insn> D1(B1),I2
//    +------------------+----+-------------+-----------------+
//    |     OpCode       | B1 |      D1     |        I2       |
//    +------------------+----+-------------+-----------------+
//    0                 16   20            32                47
#define SIL_FORM_EMIT(name, op)                                     \
  void Assembler::name(Register b1, Disp d1, const Operand& i2) {   \
    sil_form(op, b1, d1, i2);                                       \
  }                                                                 \
  void Assembler::name(const MemOperand& opnd, const Operand& i2) { \
    name(opnd.getBaseRegister(), opnd.getDisplacement(), i2);       \
  }

void Assembler::sil_form(Opcode op, Register b1, Disp d1, const Operand& i2) {
  DCHECK(is_uint12(d1));
  DCHECK(is_uint16(op));
  DCHECK(is_uint16(i2.immediate()));
  uint64_t code = (static_cast<uint64_t>(op)) * B32 |
                  (static_cast<uint64_t>(b1.code())) * B28 |
                  (static_cast<uint64_t>(d1)) * B16 |
                  (static_cast<uint64_t>(i2.immediate()));
  emit6bytes(code);
}

// RXF format: <insn> R1,R3,D2(X2,B2)
//    +--------+----+----+----+-------------+----+---+--------+
//    | OpCode | R3 | X2 | B2 |     D2      | R1 |///| OpCode |
//    +--------+----+----+----+-------------+----+---+--------+
//    0        8    12   16   20            32   36  40      47
#define RXF_FORM_EMIT(name, op)                                            \
  void Assembler::name(Register r1, Register r3, Register b2, Register x2, \
                       Disp d2) {                                          \
    rxf_form(op, r1, r3, b2, x2, d2);                                      \
  }                                                                        \
  void Assembler::name(Register r1, Register r3, const MemOperand& opnd) { \
    name(r1, r3, opnd.getBaseRegister(), opnd.getIndexRegister(),          \
         opnd.getDisplacement());                                          \
  }

void Assembler::rxf_form(Opcode op, Register r1, Register r3, Register b2,
                         Register x2, Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r3.code())) * B36 |
                  (static_cast<uint64_t>(x2.code())) * B32 |
                  (static_cast<uint64_t>(b2.code())) * B28 |
                  (static_cast<uint64_t>(d2)) * B16 |
                  (static_cast<uint64_t>(r1.code())) * B12 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
}

// SS1 format: <insn> D1(L,B1),D2(B3)
//    +--------+----+----+----+-------------+----+------------+
//    | OpCode |    L    | B1 |     D1      | B2 |     D2     |
//    +--------+----+----+----+-------------+----+------------+
//    0        8    12   16   20            32   36          47
#define SS1_FORM_EMIT(name, op)                                                \
  void Assembler::name(Register b1, Disp d1, Register b2, Disp d2, Length l) { \
    ss_form(op, l, b1, d1, b2, d2);                                            \
  }                                                                            \
  void Assembler::name(const MemOperand& opnd1, const MemOperand& opnd2,       \
                       Length length) {                                        \
    name(opnd1.getBaseRegister(), opnd1.getDisplacement(),                     \
         opnd2.getBaseRegister(), opnd2.getDisplacement(), length);            \
  }

void Assembler::ss_form(Opcode op, Length l, Register b1, Disp d1, Register b2,
                        Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint12(d1));
  DCHECK(is_uint8(op));
  DCHECK(is_uint8(l));
  uint64_t code =
      (static_cast<uint64_t>(op)) * B40 | (static_cast<uint64_t>(l)) * B32 |
      (static_cast<uint64_t>(b1.code())) * B28 |
      (static_cast<uint64_t>(d1)) * B16 |
      (static_cast<uint64_t>(b2.code())) * B12 | (static_cast<uint64_t>(d2));
  emit6bytes(code);
}

// SS2 format: <insn> D1(L1,B1), D2(L3,B3)
//    +--------+----+----+----+-------------+----+------------+
//    | OpCode | L1 | L2 | B1 |     D1      | B2 |     D2     |
//    +--------+----+----+----+-------------+----+------------+
//    0        8    12   16   20            32   36          47
#define SS2_FORM_EMIT(name, op)                                               \
  void Assembler::name(Register b1, Disp d1, Register b2, Disp d2, Length l1, \
                       Length l2) {                                           \
    ss_form(op, l1, l2, b1, d1, b2, d2);                                      \
  }                                                                           \
  void Assembler::name(const MemOperand& opnd1, const MemOperand& opnd2,      \
                       Length length1, Length length2) {                      \
    name(opnd1.getBaseRegister(), opnd1.getDisplacement(),                    \
         opnd2.getBaseRegister(), opnd2.getDisplacement(), length1, length2); \
  }

void Assembler::ss_form(Opcode op, Length l1, Length l2, Register b1, Disp d1,
                        Register b2, Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint12(d1));
  DCHECK(is_uint8(op));
  DCHECK(is_uint4(l2));
  DCHECK(is_uint4(l1));
  uint64_t code =
      (static_cast<uint64_t>(op)) * B40 | (static_cast<uint64_t>(l1)) * B36 |
      (static_cast<uint64_t>(l2)) * B32 |
      (static_cast<uint64_t>(b1.code())) * B28 |
      (static_cast<uint64_t>(d1)) * B16 |
      (static_cast<uint64_t>(b2.code())) * B12 | (static_cast<uint64_t>(d2));
  emit6bytes(code);
}

// SS3 format: <insn> D1(L1,B1), D2(I3,B2)
//    +--------+----+----+----+-------------+----+------------+
//    | OpCode | L1 | I3 | B1 |     D1      | B2 |     D2     |
//    +--------+----+----+----+-------------+----+------------+
//    0        8    12   16   20            32   36          47
#define SS3_FORM_EMIT(name, op)                                              \
  void Assembler::name(const Operand& i3, Register b1, Disp d1, Register b2, \
                       Disp d2, Length l1) {                                 \
    ss_form(op, l1, i3, b1, d1, b2, d2);                                     \
  }                                                                          \
  void Assembler::name(const MemOperand& opnd1, const MemOperand& opnd2,     \
                       Length length) {                                      \
    DCHECK(false);                                                           \
  }
void Assembler::ss_form(Opcode op, Length l1, const Operand& i3, Register b1,
                        Disp d1, Register b2, Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint12(d1));
  DCHECK(is_uint8(op));
  DCHECK(is_uint4(l1));
  DCHECK(is_uint4(i3.immediate()));
  uint64_t code =
      (static_cast<uint64_t>(op)) * B40 | (static_cast<uint64_t>(l1)) * B36 |
      (static_cast<uint64_t>(i3.immediate())) * B32 |
      (static_cast<uint64_t>(b1.code())) * B28 |
      (static_cast<uint64_t>(d1)) * B16 |
      (static_cast<uint64_t>(b2.code())) * B12 | (static_cast<uint64_t>(d2));
  emit6bytes(code);
}

// SS4 format: <insn> D1(R1,B1), D2(R3,B2)
//    +--------+----+----+----+-------------+----+------------+
//    | OpCode | R1 | R3 | B1 |     D1      | B2 |     D2     |
//    +--------+----+----+----+-------------+----+------------+
//    0        8    12   16   20            32   36          47
#define SS4_FORM_EMIT(name, op)                                            \
  void Assembler::name(Register r1, Register r3, Register b1, Disp d1,     \
                       Register b2, Disp d2) {                             \
    ss_form(op, r1, r3, b1, d1, b2, d2);                                   \
  }                                                                        \
  void Assembler::name(const MemOperand& opnd1, const MemOperand& opnd2) { \
    DCHECK(false);                                                         \
  }
void Assembler::ss_form(Opcode op, Register r1, Register r3, Register b1,
                        Disp d1, Register b2, Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint12(d1));
  DCHECK(is_uint8(op));
  uint64_t code = (static_cast<uint64_t>(op)) * B40 |
                  (static_cast<uint64_t>(r1.code())) * B36 |
                  (static_cast<uint64_t>(r3.code())) * B32 |
                  (static_cast<uint64_t>(b1.code())) * B28 |
                  (static_cast<uint64_t>(d1)) * B16 |
                  (static_cast<uint64_t>(b2.code())) * B12 |
                  (static_cast<uint64_t>(d2));
  emit6bytes(code);
}

// SS5 format: <insn> D1(R1,B1), D2(R3,B2)
//    +--------+----+----+----+-------------+----+------------+
//    | OpCode | R1 | R3 | B2 |     D2      | B4 |     D4     |
//    +--------+----+----+----+-------------+----+------------+
//    0        8    12   16   20            32   36          47
#define SS5_FORM_EMIT(name, op)                                            \
  void Assembler::name(Register r1, Register r3, Register b2, Disp d2,     \
                       Register b4, Disp d4) {                             \
    ss_form(op, r1, r3, b2, d2, b4, d4); /*SS5 use the same form as SS4*/  \
  }                                                                        \
  void Assembler::name(const MemOperand& opnd1, const MemOperand& opnd2) { \
    DCHECK(false);                                                         \
  }

#define SS6_FORM_EMIT(name, op) SS1_FORM_EMIT(name, op)

// SSE format: <insn> D1(B1),D2(B2)
//    +------------------+----+-------------+----+------------+
//    |      OpCode      | B1 |     D1      | B2 |     D2     |
//    +------------------+----+-------------+----+------------+
//    0        8    12   16   20            32   36           47
#define SSE_FORM_EMIT(name, op)                                            \
  void Assembler::name(Register b1, Disp d1, Register b2, Disp d2) {       \
    sse_form(op, b1, d1, b2, d2);                                          \
  }                                                                        \
  void Assembler::name(const MemOperand& opnd1, const MemOperand& opnd2) { \
    name(opnd1.getBaseRegister(), opnd1.getDisplacement(),                 \
         opnd2.getBaseRegister(), opnd2.getDisplacement());                \
  }
void Assembler::sse_form(Opcode op, Register b1, Disp d1, Register b2,
                         Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint12(d1));
  DCHECK(is_uint16(op));
  uint64_t code = (static_cast<uint64_t>(op)) * B32 |
                  (static_cast<uint64_t>(b1.code())) * B28 |
                  (static_cast<uint64_t>(d1)) * B16 |
                  (static_cast<uint64_t>(b2.code())) * B12 |
                  (static_cast<uint64_t>(d2));
  emit6bytes(code);
}

// SSF format: <insn> R3, D1(B1),D2(B2),R3
//    +--------+----+----+----+-------------+----+------------+
//    | OpCode | R3 |OpCd| B1 |     D1      | B2 |     D2     |
//    +--------+----+----+----+-------------+----+------------+
//    0        8    12   16   20            32   36           47
#define SSF_FORM_EMIT(name, op)                                        \
  void Assembler::name(Register r3, Register b1, Disp d1, Register b2, \
                       Disp d2) {                                      \
    ssf_form(op, r3, b1, d1, b2, d2);                                  \
  }                                                                    \
  void Assembler::name(Register r3, const MemOperand& opnd1,           \
                       const MemOperand& opnd2) {                      \
    name(r3, opnd1.getBaseRegister(), opnd1.getDisplacement(),         \
         opnd2.getBaseRegister(), opnd2.getDisplacement());            \
  }

void Assembler::ssf_form(Opcode op, Register r3, Register b1, Disp d1,
                         Register b2, Disp d2) {
  DCHECK(is_uint12(d2));
  DCHECK(is_uint12(d1));
  DCHECK(is_uint12(op));
  uint64_t code = (static_cast<uint64_t>(op & 0xFF0)) * B36 |
                  (static_cast<uint64_t>(r3.code())) * B36 |
                  (static_cast<uint64_t>(op & 0x00F)) * B32 |
                  (static_cast<uint64_t>(b1.code())) * B28 |
                  (static_cast<uint64_t>(d1)) * B16 |
                  (static_cast<uint64_t>(b2.code())) * B12 |
                  (static_cast<uint64_t>(d2));
  emit6bytes(code);
}

//  RRF1 format: <insn> R1,R2,R3
//    +------------------+----+----+----+----+
//    |      OpCode      | R3 |    | R1 | R2 |
//    +------------------+----+----+----+----+
//    0                  16   20   24   28  31
#define RRF1_FORM_EMIT(name, op)                                        \
  void Assembler::name(Register r1, Register r2, Register r3) {         \
    rrf1_form(op << 16 | r3.code() * B12 | r1.code() * B4 | r2.code()); \
  }

void Assembler::rrf1_form(Opcode op, Register r1, Register r2, Register r3) {
  uint32_t code = op << 16 | r3.code() * B12 | r1.code() * B4 | r2.code();
  emit4bytes(code);
}

void Assembler::rrf1_form(uint32_t code) { emit4bytes(code); }

//  RRF2 format: <insn> R1,R2,M3
//    +------------------+----+----+----+----+
//    |      OpCode      | M3 |    | R1 | R2 |
//    +------------------+----+----+----+----+
//    0                  16   20   24   28  31
#define RRF2_FORM_EMIT(name, op)                                 \
  void Assembler::name(Condition m3, Register r1, Register r2) { \
    rrf2_form(op << 16 | m3 * B12 | r1.code() * B4 | r2.code()); \
  }

void Assembler::rrf2_form(uint32_t code) { emit4bytes(code); }

//  RRF3 format: <insn> R1,R2,R3,M4
//    +------------------+----+----+----+----+
//    |      OpCode      | R3 | M4 | R1 | R2 |
//    +------------------+----+----+----+----+
//    0                  16   20   24   28  31
#define RRF3_FORM_EMIT(name, op)                                             \
  void Assembler::name(Register r3, Conition m4, Register r1, Register r2) { \
    rrf3_form(op << 16 | r3.code() * B12 | m4 * B8 | r1.code() * B4 |        \
              r2.code());                                                    \
  }

void Assembler::rrf3_form(uint32_t code) { emit4bytes(code); }

//  RRF-e format: <insn> R1,M3,R2,M4
//    +------------------+----+----+----+----+
//    |      OpCode      | M3 | M4 | R1 | R2 |
//    +------------------+----+----+----+----+
//    0                  16   20   24   28  31
void Assembler::rrfe_form(Opcode op, Condition m3, Condition m4, Register r1,
                          Register r2) {
  uint32_t code = op << 16 | m3 * B12 | m4 * B8 | r1.code() * B4 | r2.code();
  emit4bytes(code);
}

// end of S390 Instruction generation

// start of S390 instruction
SS1_FORM_EMIT(ed, ED)
SS1_FORM_EMIT(mvn, MVN)
SS1_FORM_EMIT(nc, NC)
SI_FORM_EMIT(ni, NI)
RI1_FORM_EMIT(nilh, NILH)
RI1_FORM_EMIT(nill, NILL)
RI1_FORM_EMIT(oill, OILL)
RI1_FORM_EMIT(tmll, TMLL)
SS1_FORM_EMIT(tr, TR)
S_FORM_EMIT(ts, TS)

// -------------------------
// Load Address Instructions
// -------------------------
// Load Address Relative Long
void Assembler::larl(Register r1, Label* l) {
  larl(r1, Operand(branch_offset(l)));
}

// -----------------
// Load Instructions
// -----------------
// Load Halfword Immediate (32)
void Assembler::lhi(Register r, const Operand& imm) { ri_form(LHI, r, imm); }

// Load Halfword Immediate (64)
void Assembler::lghi(Register r, const Operand& imm) { ri_form(LGHI, r, imm); }

// -------------------------
// Load Logical Instructions
// -------------------------
// Load On Condition R-R (32)
void Assembler::locr(Condition m3, Register r1, Register r2) {
  rrf2_form(LOCR << 16 | m3 * B12 | r1.code() * B4 | r2.code());
}

// Load On Condition R-R (64)
void Assembler::locgr(Condition m3, Register r1, Register r2) {
  rrf2_form(LOCGR << 16 | m3 * B12 | r1.code() * B4 | r2.code());
}

// Load On Condition R-M (32)
void Assembler::loc(Condition m3, Register r1, const MemOperand& src) {
  rsy_form(LOC, r1, m3, src.rb(), src.offset());
}

// Load On Condition R-M (64)
void Assembler::locg(Condition m3, Register r1, const MemOperand& src) {
  rsy_form(LOCG, r1, m3, src.rb(), src.offset());
}

// -------------------
// Branch Instructions
// -------------------
// Branch on Count (64)
// Branch Relative and Save (32)
void Assembler::bras(Register r, const Operand& opnd) {
  ri_form(BRAS, r, opnd);
}

// Branch relative on Condition (32)
void Assembler::brc(Condition c, const Operand& opnd) { ri_form(BRC, c, opnd); }

// Branch On Count (32)
void Assembler::brct(Register r1, const Operand& imm) {
  // BRCT encodes # of halfwords, so divide by 2.
  int16_t numHalfwords = static_cast<int16_t>(imm.immediate()) / 2;
  Operand halfwordOp = Operand(numHalfwords);
  halfwordOp.setBits(16);
  ri_form(BRCT, r1, halfwordOp);
}

// Branch On Count (32)
void Assembler::brctg(Register r1, const Operand& imm) {
  // BRCTG encodes # of halfwords, so divide by 2.
  int16_t numHalfwords = static_cast<int16_t>(imm.immediate()) / 2;
  Operand halfwordOp = Operand(numHalfwords);
  halfwordOp.setBits(16);
  ri_form(BRCTG, r1, halfwordOp);
}

// --------------------
// Compare Instructions
// --------------------
// Compare Halfword Immediate (32)
void Assembler::chi(Register r, const Operand& opnd) { ri_form(CHI, r, opnd); }

// Compare Halfword Immediate (64)
void Assembler::cghi(Register r, const Operand& opnd) {
  ri_form(CGHI, r, opnd);
}

// ----------------------------
// Compare Logical Instructions
// ----------------------------
// Compare Immediate (Mem - Imm) (8)
void Assembler::cli(const MemOperand& opnd, const Operand& imm) {
  si_form(CLI, imm, opnd.rb(), opnd.offset());
}

// Compare Immediate (Mem - Imm) (8)
void Assembler::cliy(const MemOperand& opnd, const Operand& imm) {
  siy_form(CLIY, imm, opnd.rb(), opnd.offset());
}

// Compare logical - mem to mem operation
void Assembler::clc(const MemOperand& opnd1, const MemOperand& opnd2,
                    Length length) {
  ss_form(CLC, length - 1, opnd1.getBaseRegister(), opnd1.getDisplacement(),
          opnd2.getBaseRegister(), opnd2.getDisplacement());
}

// ----------------------------
// Test Under Mask Instructions
// ----------------------------
// Test Under Mask (Mem - Imm) (8)
void Assembler::tm(const MemOperand& opnd, const Operand& imm) {
  si_form(TM, imm, opnd.rb(), opnd.offset());
}

// Test Under Mask (Mem - Imm) (8)
void Assembler::tmy(const MemOperand& opnd, const Operand& imm) {
  siy_form(TMY, imm, opnd.rb(), opnd.offset());
}

// -------------------------------
// Rotate and Insert Selected Bits
// -------------------------------
// Rotate-And-Insert-Selected-Bits
void Assembler::risbg(Register dst, Register src, const Operand& startBit,
                      const Operand& endBit, const Operand& shiftAmt,
                      bool zeroBits) {
  // High tag the top bit of I4/EndBit to zero out any unselected bits
  if (zeroBits)
    rie_f_form(RISBG, dst, src, startBit, Operand(endBit.immediate() | 0x80),
               shiftAmt);
  else
    rie_f_form(RISBG, dst, src, startBit, endBit, shiftAmt);
}

// Rotate-And-Insert-Selected-Bits
void Assembler::risbgn(Register dst, Register src, const Operand& startBit,
                       const Operand& endBit, const Operand& shiftAmt,
                       bool zeroBits) {
  // High tag the top bit of I4/EndBit to zero out any unselected bits
  if (zeroBits)
    rie_f_form(RISBGN, dst, src, startBit, Operand(endBit.immediate() | 0x80),
               shiftAmt);
  else
    rie_f_form(RISBGN, dst, src, startBit, endBit, shiftAmt);
}

// ---------------------------
// Move Character Instructions
// ---------------------------
// Move character - mem to mem operation
void Assembler::mvc(const MemOperand& opnd1, const MemOperand& opnd2,
                    uint32_t length) {
  ss_form(MVC, length - 1, opnd1.getBaseRegister(), opnd1.getDisplacement(),
          opnd2.getBaseRegister(), opnd2.getDisplacement());
}

// -----------------------
// 32-bit Add Instructions
// -----------------------
// Add Halfword Immediate (32)
void Assembler::ahi(Register r1, const Operand& i2) { ri_form(AHI, r1, i2); }

// Add Halfword Immediate (32)
void Assembler::ahik(Register r1, Register r3, const Operand& i2) {
  rie_form(AHIK, r1, r3, i2);
}

// Add Register-Register-Register (32)
void Assembler::ark(Register r1, Register r2, Register r3) {
  rrf1_form(ARK, r1, r2, r3);
}

// Add Storage-Imm (32)
void Assembler::asi(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  siy_form(ASI, Operand(0xFF & imm.immediate()), opnd.rb(),
           0xFFFFF & opnd.offset());
}

// -----------------------
// 64-bit Add Instructions
// -----------------------
// Add Halfword Immediate (64)
void Assembler::aghi(Register r1, const Operand& i2) { ri_form(AGHI, r1, i2); }

// Add Halfword Immediate (64)
void Assembler::aghik(Register r1, Register r3, const Operand& i2) {
  rie_form(AGHIK, r1, r3, i2);
}

// Add Register-Register-Register (64)
void Assembler::agrk(Register r1, Register r2, Register r3) {
  rrf1_form(AGRK, r1, r2, r3);
}

// Add Storage-Imm (64)
void Assembler::agsi(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  siy_form(AGSI, Operand(0xFF & imm.immediate()), opnd.rb(),
           0xFFFFF & opnd.offset());
}

// -------------------------------
// 32-bit Add Logical Instructions
// -------------------------------
// Add Logical Register-Register-Register (32)
void Assembler::alrk(Register r1, Register r2, Register r3) {
  rrf1_form(ALRK, r1, r2, r3);
}

// -------------------------------
// 64-bit Add Logical Instructions
// -------------------------------
// Add Logical Register-Register-Register (64)
void Assembler::algrk(Register r1, Register r2, Register r3) {
  rrf1_form(ALGRK, r1, r2, r3);
}

// ----------------------------
// 32-bit Subtract Instructions
// ----------------------------
// Subtract Register-Register-Register (32)
void Assembler::srk(Register r1, Register r2, Register r3) {
  rrf1_form(SRK, r1, r2, r3);
}

// ----------------------------
// 64-bit Subtract Instructions
// ----------------------------
// Subtract Register-Register-Register (64)
void Assembler::sgrk(Register r1, Register r2, Register r3) {
  rrf1_form(SGRK, r1, r2, r3);
}

// ------------------------------------
// 32-bit Subtract Logical Instructions
// ------------------------------------
// Subtract Logical Register-Register-Register (32)
void Assembler::slrk(Register r1, Register r2, Register r3) {
  rrf1_form(SLRK, r1, r2, r3);
}

// ------------------------------------
// 64-bit Subtract Logical Instructions
// ------------------------------------
// Subtract Logical Register-Register-Register (64)
void Assembler::slgrk(Register r1, Register r2, Register r3) {
  rrf1_form(SLGRK, r1, r2, r3);
}

// ----------------------------
// 32-bit Multiply Instructions
// ----------------------------
// Multiply Halfword Immediate (32)
void Assembler::mhi(Register r1, const Operand& opnd) {
  ri_form(MHI, r1, opnd);
}

// Multiply Single Register (32)
void Assembler::msrkc(Register r1, Register r2, Register r3) {
  rrf1_form(MSRKC, r1, r2, r3);
}

// Multiply Single Register (64)
void Assembler::msgrkc(Register r1, Register r2, Register r3) {
  rrf1_form(MSGRKC, r1, r2, r3);
}

// ----------------------------
// 64-bit Multiply Instructions
// ----------------------------
// Multiply Halfword Immediate (64)
void Assembler::mghi(Register r1, const Operand& opnd) {
  ri_form(MGHI, r1, opnd);
}

// --------------------
// Bitwise Instructions
// --------------------
// AND Register-Register-Register (32)
void Assembler::nrk(Register r1, Register r2, Register r3) {
  rrf1_form(NRK, r1, r2, r3);
}

// AND Register-Register-Register (64)
void Assembler::ngrk(Register r1, Register r2, Register r3) {
  rrf1_form(NGRK, r1, r2, r3);
}

// OR Register-Register-Register (32)
void Assembler::ork(Register r1, Register r2, Register r3) {
  rrf1_form(ORK, r1, r2, r3);
}

// OR Register-Register-Register (64)
void Assembler::ogrk(Register r1, Register r2, Register r3) {
  rrf1_form(OGRK, r1, r2, r3);
}

// XOR Register-Register-Register (32)
void Assembler::xrk(Register r1, Register r2, Register r3) {
  rrf1_form(XRK, r1, r2, r3);
}

// XOR Register-Register-Register (64)
void Assembler::xgrk(Register r1, Register r2, Register r3) {
  rrf1_form(XGRK, r1, r2, r3);
}

// XOR Storage-Storage
void Assembler::xc(const MemOperand& opnd1, const MemOperand& opnd2,
                   Length length) {
  ss_form(XC, length - 1, opnd1.getBaseRegister(), opnd1.getDisplacement(),
          opnd2.getBaseRegister(), opnd2.getDisplacement());
}

void Assembler::EnsureSpaceFor(int space_needed) {
  if (buffer_space() <= (kGap + space_needed)) {
    GrowBuffer(space_needed);
  }
}

// Rotate Left Single Logical (32)
void Assembler::rll(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(RLL, r1, r3, opnd, 0);
}

// Rotate Left Single Logical (32)
void Assembler::rll(Register r1, Register r3, const Operand& opnd) {
  rsy_form(RLL, r1, r3, r0, opnd.immediate());
}

// Rotate Left Single Logical (32)
void Assembler::rll(Register r1, Register r3, Register r2,
                    const Operand& opnd) {
  rsy_form(RLL, r1, r3, r2, opnd.immediate());
}

// Rotate Left Single Logical (64)
void Assembler::rllg(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(RLLG, r1, r3, opnd, 0);
}

// Rotate Left Single Logical (64)
void Assembler::rllg(Register r1, Register r3, const Operand& opnd) {
  rsy_form(RLLG, r1, r3, r0, opnd.immediate());
}

// Rotate Left Single Logical (64)
void Assembler::rllg(Register r1, Register r3, Register r2,
                     const Operand& opnd) {
  rsy_form(RLLG, r1, r3, r2, opnd.immediate());
}

// Shift Left Single Logical (32)
void Assembler::sll(Register r1, Register opnd) {
  DCHECK(opnd != r0);
  rs_form(SLL, r1, r0, opnd, 0);
}

// Shift Left Single Logical (32)
void Assembler::sll(Register r1, const Operand& opnd) {
  rs_form(SLL, r1, r0, r0, opnd.immediate());
}

// Shift Left Single Logical (32)
void Assembler::sllk(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SLLK, r1, r3, opnd, 0);
}

// Shift Left Single Logical (32)
void Assembler::sllk(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SLLK, r1, r3, r0, opnd.immediate());
}

// Shift Left Single Logical (64)
void Assembler::sllg(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SLLG, r1, r3, opnd, 0);
}

// Shift Left Single Logical (64)
void Assembler::sllg(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SLLG, r1, r3, r0, opnd.immediate());
}

// Shift Left Double Logical (64)
void Assembler::sldl(Register r1, Register b2, const Operand& opnd) {
  DCHECK_EQ(r1.code() % 2, 0);
  rs_form(SLDL, r1, r0, b2, opnd.immediate());
}

// Shift Right Single Logical (32)
void Assembler::srl(Register r1, Register opnd) {
  DCHECK(opnd != r0);
  rs_form(SRL, r1, r0, opnd, 0);
}

// Shift Right Double Arith (64)
void Assembler::srda(Register r1, Register b2, const Operand& opnd) {
  DCHECK_EQ(r1.code() % 2, 0);
  rs_form(SRDA, r1, r0, b2, opnd.immediate());
}

// Shift Right Double Logical (64)
void Assembler::srdl(Register r1, Register b2, const Operand& opnd) {
  DCHECK_EQ(r1.code() % 2, 0);
  rs_form(SRDL, r1, r0, b2, opnd.immediate());
}

// Shift Right Single Logical (32)
void Assembler::srl(Register r1, const Operand& opnd) {
  rs_form(SRL, r1, r0, r0, opnd.immediate());
}

// Shift Right Single Logical (32)
void Assembler::srlk(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SRLK, r1, r3, opnd, 0);
}

// Shift Right Single Logical (32)
void Assembler::srlk(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SRLK, r1, r3, r0, opnd.immediate());
}

// Shift Right Single Logical (64)
void Assembler::srlg(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SRLG, r1, r3, opnd, 0);
}

// Shift Right Single Logical (64)
void Assembler::srlg(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SRLG, r1, r3, r0, opnd.immediate());
}

// Shift Left Single (32)
void Assembler::sla(Register r1, Register opnd) {
  DCHECK(opnd != r0);
  rs_form(SLA, r1, r0, opnd, 0);
}

// Shift Left Single (32)
void Assembler::sla(Register r1, const Operand& opnd) {
  rs_form(SLA, r1, r0, r0, opnd.immediate());
}

// Shift Left Single (32)
void Assembler::slak(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SLAK, r1, r3, opnd, 0);
}

// Shift Left Single (32)
void Assembler::slak(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SLAK, r1, r3, r0, opnd.immediate());
}

// Shift Left Single (64)
void Assembler::slag(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SLAG, r1, r3, opnd, 0);
}

// Shift Left Single (64)
void Assembler::slag(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SLAG, r1, r3, r0, opnd.immediate());
}

// Shift Right Single (32)
void Assembler::sra(Register r1, Register opnd) {
  DCHECK(opnd != r0);
  rs_form(SRA, r1, r0, opnd, 0);
}

// Shift Right Single (32)
void Assembler::sra(Register r1, const Operand& opnd) {
  rs_form(SRA, r1, r0, r0, opnd.immediate());
}

// Shift Right Single (32)
void Assembler::srak(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SRAK, r1, r3, opnd, 0);
}

// Shift Right Single (32)
void Assembler::srak(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SRAK, r1, r3, r0, opnd.immediate());
}

// Shift Right Single (64)
void Assembler::srag(Register r1, Register r3, Register opnd) {
  DCHECK(opnd != r0);
  rsy_form(SRAG, r1, r3, opnd, 0);
}

void Assembler::srag(Register r1, Register r3, const Operand& opnd) {
  rsy_form(SRAG, r1, r3, r0, opnd.immediate());
}

// Shift Right Double
void Assembler::srda(Register r1, const Operand& opnd) {
  DCHECK_EQ(r1.code() % 2, 0);
  rs_form(SRDA, r1, r0, r0, opnd.immediate());
}

// Shift Right Double Logical
void Assembler::srdl(Register r1, const Operand& opnd) {
  DCHECK_EQ(r1.code() % 2, 0);
  rs_form(SRDL, r1, r0, r0, opnd.immediate());
}

void Assembler::call(Handle<Code> target, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);

  int32_t target_index = emit_code_target(target, rmode);
  brasl(r14, Operand(target_index));
}

void Assembler::call(CodeStub* stub) {
  EnsureSpace ensure_space(this);
  RequestHeapObject(HeapObjectRequest(stub));
  int32_t target_index =
      emit_code_target(Handle<Code>(), RelocInfo::CODE_TARGET);
  brasl(r14, Operand(target_index));
}

void Assembler::jump(Handle<Code> target, RelocInfo::Mode rmode,
                     Condition cond) {
  EnsureSpace ensure_space(this);

  int32_t target_index = emit_code_target(target, rmode);
  brcl(cond, Operand(target_index));
}

// 32-bit Load Multiple - short displacement (12-bits unsigned)
void Assembler::lm(Register r1, Register r2, const MemOperand& src) {
  rs_form(LM, r1, r2, src.rb(), src.offset());
}

// 32-bit Load Multiple - long displacement (20-bits signed)
void Assembler::lmy(Register r1, Register r2, const MemOperand& src) {
  rsy_form(LMY, r1, r2, src.rb(), src.offset());
}

// 64-bit Load Multiple - long displacement (20-bits signed)
void Assembler::lmg(Register r1, Register r2, const MemOperand& src) {
  rsy_form(LMG, r1, r2, src.rb(), src.offset());
}

// 32-bit Compare and Swap
void Assembler::cs(Register r1, Register r2, const MemOperand& src) {
  rs_form(CS, r1, r2, src.rb(), src.offset());
}

// 32-bit Compare and Swap
void Assembler::csy(Register r1, Register r2, const MemOperand& src) {
  rsy_form(CSY, r1, r2, src.rb(), src.offset());
}

// 64-bit Compare and Swap
void Assembler::csg(Register r1, Register r2, const MemOperand& src) {
  rsy_form(CSG, r1, r2, src.rb(), src.offset());
}

// Move integer (32)
void Assembler::mvhi(const MemOperand& opnd1, const Operand& i2) {
  sil_form(MVHI, opnd1.getBaseRegister(), opnd1.getDisplacement(), i2);
}

// Move integer (64)
void Assembler::mvghi(const MemOperand& opnd1, const Operand& i2) {
  sil_form(MVGHI, opnd1.getBaseRegister(), opnd1.getDisplacement(), i2);
}

// Insert Immediate (high high)
void Assembler::iihh(Register r1, const Operand& opnd) {
  ri_form(IIHH, r1, opnd);
}

// Insert Immediate (high low)
void Assembler::iihl(Register r1, const Operand& opnd) {
  ri_form(IIHL, r1, opnd);
}

// Insert Immediate (low high)
void Assembler::iilh(Register r1, const Operand& opnd) {
  ri_form(IILH, r1, opnd);
}

// Insert Immediate (low low)
void Assembler::iill(Register r1, const Operand& opnd) {
  ri_form(IILL, r1, opnd);
}

// GPR <-> FPR Instructions

// Floating point instructions
//
// Add Register-Storage (LB)
void Assembler::adb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(ADB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Add Register-Storage (LB)
void Assembler::aeb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(AEB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Sub Register-Storage (LB)
void Assembler::seb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(SEB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Divide Register-Storage (LB)
void Assembler::ddb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(DDB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Divide Register-Storage (LB)
void Assembler::deb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(DEB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Multiply Register-Storage (LB)
void Assembler::mdb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(MDB, Register::from_code(r1.code()), opnd.rb(), opnd.rx(),
           opnd.offset());
}

// Multiply Register-Storage (LB)
void Assembler::meeb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(MEEB, Register::from_code(r1.code()), opnd.rb(), opnd.rx(),
           opnd.offset());
}

// Subtract Register-Storage (LB)
void Assembler::sdb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(SDB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

void Assembler::ceb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(CEB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

void Assembler::cdb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(CDB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Square Root (LB)
void Assembler::sqdb(DoubleRegister r1, const MemOperand& opnd) {
  rxe_form(SQDB, Register::from_code(r1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Convert to Fixed point (64<-S)
void Assembler::cgebr(Condition m, Register r1, DoubleRegister r2) {
  rrfe_form(CGEBR, m, Condition(0), r1, Register::from_code(r2.code()));
}

// Convert to Fixed point (64<-L)
void Assembler::cgdbr(Condition m, Register r1, DoubleRegister r2) {
  rrfe_form(CGDBR, m, Condition(0), r1, Register::from_code(r2.code()));
}

// Convert to Fixed point (32<-L)
void Assembler::cfdbr(Condition m, Register r1, DoubleRegister r2) {
  rrfe_form(CFDBR, m, Condition(0), r1, Register::from_code(r2.code()));
}

// Convert to Fixed Logical (64<-L)
void Assembler::clgdbr(Condition m3, Condition m4, Register r1,
                       DoubleRegister r2) {
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CLGDBR, m3, m4, r1, Register::from_code(r2.code()));
}

// Convert to Fixed Logical (64<-F32)
void Assembler::clgebr(Condition m3, Condition m4, Register r1,
                       DoubleRegister r2) {
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CLGEBR, m3, m4, r1, Register::from_code(r2.code()));
}

// Convert to Fixed Logical (32<-F64)
void Assembler::clfdbr(Condition m3, Condition m4, Register r1,
                       DoubleRegister r2) {
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CLFDBR, m3, Condition(0), r1, Register::from_code(r2.code()));
}

// Convert to Fixed Logical (32<-F32)
void Assembler::clfebr(Condition m3, Condition m4, Register r1,
                       DoubleRegister r2) {
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CLFEBR, m3, Condition(0), r1, Register::from_code(r2.code()));
}

// Convert from Fixed Logical (L<-64)
void Assembler::celgbr(Condition m3, Condition m4, DoubleRegister r1,
                       Register r2) {
  DCHECK_EQ(m3, Condition(0));
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CELGBR, Condition(0), Condition(0), Register::from_code(r1.code()),
            r2);
}

// Convert from Fixed Logical (F32<-32)
void Assembler::celfbr(Condition m3, Condition m4, DoubleRegister r1,
                       Register r2) {
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CELFBR, m3, Condition(0), Register::from_code(r1.code()), r2);
}

// Convert from Fixed Logical (L<-64)
void Assembler::cdlgbr(Condition m3, Condition m4, DoubleRegister r1,
                       Register r2) {
  DCHECK_EQ(m3, Condition(0));
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CDLGBR, Condition(0), Condition(0), Register::from_code(r1.code()),
            r2);
}

// Convert from Fixed Logical (L<-32)
void Assembler::cdlfbr(Condition m3, Condition m4, DoubleRegister r1,
                       Register r2) {
  DCHECK_EQ(m4, Condition(0));
  rrfe_form(CDLFBR, m3, Condition(0), Register::from_code(r1.code()), r2);
}

// Convert from Fixed point (S<-32)
void Assembler::cefbr(Condition m3, DoubleRegister r1, Register r2) {
  rrfe_form(CEFBR, m3, Condition(0), Register::from_code(r1.code()), r2);
}

// Convert to Fixed point (32<-S)
void Assembler::cfebr(Condition m3, Register r1, DoubleRegister r2) {
  rrfe_form(CFEBR, m3, Condition(0), r1, Register::from_code(r2.code()));
}

// Load (L <- S)
void Assembler::ldeb(DoubleRegister d1, const MemOperand& opnd) {
  rxe_form(LDEB, Register::from_code(d1.code()), opnd.rx(), opnd.rb(),
           opnd.offset());
}

// Load FP Integer
void Assembler::fiebra(DoubleRegister d1, DoubleRegister d2, FIDBRA_MASK3 m3) {
  rrf2_form(FIEBRA << 16 | m3 * B12 | d1.code() * B4 | d2.code());
}

// Load FP Integer
void Assembler::fidbra(DoubleRegister d1, DoubleRegister d2, FIDBRA_MASK3 m3) {
  rrf2_form(FIDBRA << 16 | m3 * B12 | d1.code() * B4 | d2.code());
}

// end of S390instructions

bool Assembler::IsNop(SixByteInstr instr, int type) {
  DCHECK((0 == type) || (DEBUG_BREAK_NOP == type));
  if (DEBUG_BREAK_NOP == type) {
    return ((instr & 0xFFFFFFFF) == 0xA53B0000);  // oill r3, 0
  }
  return ((instr & 0xFFFF) == 0x1800);  // lr r0,r0
}

// dummy instruction reserved for special use.
void Assembler::dumy(int r1, int x2, int b2, int d2) {
#if defined(USE_SIMULATOR)
  int op = 0xE353;
  uint64_t code = (static_cast<uint64_t>(op & 0xFF00)) * B32 |
                  (static_cast<uint64_t>(r1) & 0xF) * B36 |
                  (static_cast<uint64_t>(x2) & 0xF) * B32 |
                  (static_cast<uint64_t>(b2) & 0xF) * B28 |
                  (static_cast<uint64_t>(d2 & 0x0FFF)) * B16 |
                  (static_cast<uint64_t>(d2 & 0x0FF000)) >> 4 |
                  (static_cast<uint64_t>(op & 0x00FF));
  emit6bytes(code);
#endif
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

  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if (desc.buffer_size > kMaximalBufferSize) {
    V8::FatalProcessOutOfMemory("Assembler::GrowBuffer");
  }

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

  // None of our relocation types are pc relative pointing outside the code
  // buffer nor pc absolute pointing inside the code buffer, so there is no need
  // to relocate any emitted relocation entries.
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
  DeferredRelocInfo rinfo(pc_offset(), rmode, data);
  relocations_.push_back(rinfo);
}

void Assembler::emit_label_addr(Label* label) {
  CheckBuffer();
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  int position = link(label);
  DCHECK(label->is_bound());
  // Keep internal references relative until EmitRelocations.
  dp(position);
}

void Assembler::EmitRelocations() {
  EnsureSpaceFor(relocations_.size() * kMaxRelocSize);

  for (std::vector<DeferredRelocInfo>::iterator it = relocations_.begin();
       it != relocations_.end(); it++) {
    RelocInfo::Mode rmode = it->rmode();
    Address pc = buffer_ + it->position();
    RelocInfo rinfo(pc, rmode, it->data(), nullptr);

    // Fix up internal references now that they are guaranteed to be bound.
    if (RelocInfo::IsInternalReference(rmode)) {
      // Jump table entry
      intptr_t pos = reinterpret_cast<intptr_t>(Memory::Address_at(pc));
      Memory::Address_at(pc) = buffer_ + pos;
    } else if (RelocInfo::IsInternalReferenceEncoded(rmode)) {
      // mov sequence
      intptr_t pos = reinterpret_cast<intptr_t>(target_address_at(pc, nullptr));
      set_target_address_at(nullptr, pc, nullptr, buffer_ + pos,
                            SKIP_ICACHE_FLUSH);
    }

    reloc_info_writer.Write(&rinfo);
  }
}

}  // namespace internal
}  // namespace v8
#endif  // V8_TARGET_ARCH_S390

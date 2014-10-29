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

// A light-weight ARM Assembler
// Generates user mode instructions for the ARM architecture up to version 5

#ifndef V8_ARM_ASSEMBLER_ARM_H_
#define V8_ARM_ASSEMBLER_ARM_H_

#include <stdio.h>
#include <vector>

#include "src/arm/constants-arm.h"
#include "src/assembler.h"
#include "src/serialize.h"

namespace v8 {
namespace internal {

// CPU Registers.
//
// 1) We would prefer to use an enum, but enum values are assignment-
// compatible with int, which has caused code-generation bugs.
//
// 2) We would prefer to use a class instead of a struct but we don't like
// the register initialization to depend on the particular initialization
// order (which appears to be different on OS X, Linux, and Windows for the
// installed versions of C++ we tried). Using a struct permits C-style
// "initialization". Also, the Register objects cannot be const as this
// forces initialization stubs in MSVC, making us dependent on initialization
// order.
//
// 3) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the struct in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.

// These constants are used in several locations, including static initializers
const int kRegister_no_reg_Code = -1;
const int kRegister_r0_Code = 0;
const int kRegister_r1_Code = 1;
const int kRegister_r2_Code = 2;
const int kRegister_r3_Code = 3;
const int kRegister_r4_Code = 4;
const int kRegister_r5_Code = 5;
const int kRegister_r6_Code = 6;
const int kRegister_r7_Code = 7;
const int kRegister_r8_Code = 8;
const int kRegister_r9_Code = 9;
const int kRegister_r10_Code = 10;
const int kRegister_fp_Code = 11;
const int kRegister_ip_Code = 12;
const int kRegister_sp_Code = 13;
const int kRegister_lr_Code = 14;
const int kRegister_pc_Code = 15;

// Core register
struct Register {
  static const int kNumRegisters = 16;
  static const int kMaxNumAllocatableRegisters =
      FLAG_enable_ool_constant_pool ? 8 : 9;
  static const int kSizeInBytes = 4;

  inline static int NumAllocatableRegisters();

  static int ToAllocationIndex(Register reg) {
    DCHECK(reg.code() < kMaxNumAllocatableRegisters);
    return reg.code();
  }

  static Register FromAllocationIndex(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    return from_code(index);
  }

  static const char* AllocationIndexToString(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    const char* const names[] = {
      "r0",
      "r1",
      "r2",
      "r3",
      "r4",
      "r5",
      "r6",
      "r7",
      "r8",
    };
    if (FLAG_enable_ool_constant_pool && (index >= 7)) {
      return names[index + 1];
    }
    return names[index];
  }

  static Register from_code(int code) {
    Register r = { code };
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(Register reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }

  void set_code(int code) {
    code_ = code;
    DCHECK(is_valid());
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};

const Register no_reg = { kRegister_no_reg_Code };

const Register r0  = { kRegister_r0_Code };
const Register r1  = { kRegister_r1_Code };
const Register r2  = { kRegister_r2_Code };
const Register r3  = { kRegister_r3_Code };
const Register r4  = { kRegister_r4_Code };
const Register r5  = { kRegister_r5_Code };
const Register r6  = { kRegister_r6_Code };
// Used as constant pool pointer register if FLAG_enable_ool_constant_pool.
const Register r7  = { kRegister_r7_Code };
// Used as context register.
const Register r8  = { kRegister_r8_Code };
// Used as lithium codegen scratch register.
const Register r9  = { kRegister_r9_Code };
// Used as roots register.
const Register r10 = { kRegister_r10_Code };
const Register fp  = { kRegister_fp_Code };
const Register ip  = { kRegister_ip_Code };
const Register sp  = { kRegister_sp_Code };
const Register lr  = { kRegister_lr_Code };
const Register pc  = { kRegister_pc_Code };

// Single word VFP register.
struct SwVfpRegister {
  static const int kSizeInBytes = 4;
  bool is_valid() const { return 0 <= code_ && code_ < 32; }
  bool is(SwVfpRegister reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }
  void split_code(int* vm, int* m) const {
    DCHECK(is_valid());
    *m = code_ & 0x1;
    *vm = code_ >> 1;
  }

  int code_;
};


// Double word VFP register.
struct DwVfpRegister {
  static const int kMaxNumRegisters = 32;
  // A few double registers are reserved: one as a scratch register and one to
  // hold 0.0, that does not fit in the immediate field of vmov instructions.
  //  d14: 0.0
  //  d15: scratch register.
  static const int kNumReservedRegisters = 2;
  static const int kMaxNumAllocatableRegisters = kMaxNumRegisters -
      kNumReservedRegisters;
  static const int kSizeInBytes = 8;

  // Note: the number of registers can be different at snapshot and run-time.
  // Any code included in the snapshot must be able to run both with 16 or 32
  // registers.
  inline static int NumRegisters();
  inline static int NumReservedRegisters();
  inline static int NumAllocatableRegisters();

  // TODO(turbofan): This is a temporary work-around required because our
  // register allocator does not yet support the aliasing of single/double
  // registers on ARM.
  inline static int NumAllocatableAliasedRegisters();

  inline static int ToAllocationIndex(DwVfpRegister reg);
  static const char* AllocationIndexToString(int index);
  inline static DwVfpRegister FromAllocationIndex(int index);

  static DwVfpRegister from_code(int code) {
    DwVfpRegister r = { code };
    return r;
  }

  bool is_valid() const {
    return 0 <= code_ && code_ < kMaxNumRegisters;
  }
  bool is(DwVfpRegister reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }
  void split_code(int* vm, int* m) const {
    DCHECK(is_valid());
    *m = (code_ & 0x10) >> 4;
    *vm = code_ & 0x0F;
  }

  int code_;
};


typedef DwVfpRegister DoubleRegister;


// Double word VFP register d0-15.
struct LowDwVfpRegister {
 public:
  static const int kMaxNumLowRegisters = 16;
  operator DwVfpRegister() const {
    DwVfpRegister r = { code_ };
    return r;
  }
  static LowDwVfpRegister from_code(int code) {
    LowDwVfpRegister r = { code };
    return r;
  }

  bool is_valid() const {
    return 0 <= code_ && code_ < kMaxNumLowRegisters;
  }
  bool is(DwVfpRegister reg) const { return code_ == reg.code_; }
  bool is(LowDwVfpRegister reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  SwVfpRegister low() const {
    SwVfpRegister reg;
    reg.code_ = code_ * 2;

    DCHECK(reg.is_valid());
    return reg;
  }
  SwVfpRegister high() const {
    SwVfpRegister reg;
    reg.code_ = (code_ * 2) + 1;

    DCHECK(reg.is_valid());
    return reg;
  }

  int code_;
};


// Quad word NEON register.
struct QwNeonRegister {
  static const int kMaxNumRegisters = 16;

  static QwNeonRegister from_code(int code) {
    QwNeonRegister r = { code };
    return r;
  }

  bool is_valid() const {
    return (0 <= code_) && (code_ < kMaxNumRegisters);
  }
  bool is(QwNeonRegister reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  void split_code(int* vm, int* m) const {
    DCHECK(is_valid());
    int encoded_code = code_ << 1;
    *m = (encoded_code & 0x10) >> 4;
    *vm = encoded_code & 0x0F;
  }

  int code_;
};


typedef QwNeonRegister QuadRegister;


// Support for the VFP registers s0 to s31 (d0 to d15).
// Note that "s(N):s(N+1)" is the same as "d(N/2)".
const SwVfpRegister s0  = {  0 };
const SwVfpRegister s1  = {  1 };
const SwVfpRegister s2  = {  2 };
const SwVfpRegister s3  = {  3 };
const SwVfpRegister s4  = {  4 };
const SwVfpRegister s5  = {  5 };
const SwVfpRegister s6  = {  6 };
const SwVfpRegister s7  = {  7 };
const SwVfpRegister s8  = {  8 };
const SwVfpRegister s9  = {  9 };
const SwVfpRegister s10 = { 10 };
const SwVfpRegister s11 = { 11 };
const SwVfpRegister s12 = { 12 };
const SwVfpRegister s13 = { 13 };
const SwVfpRegister s14 = { 14 };
const SwVfpRegister s15 = { 15 };
const SwVfpRegister s16 = { 16 };
const SwVfpRegister s17 = { 17 };
const SwVfpRegister s18 = { 18 };
const SwVfpRegister s19 = { 19 };
const SwVfpRegister s20 = { 20 };
const SwVfpRegister s21 = { 21 };
const SwVfpRegister s22 = { 22 };
const SwVfpRegister s23 = { 23 };
const SwVfpRegister s24 = { 24 };
const SwVfpRegister s25 = { 25 };
const SwVfpRegister s26 = { 26 };
const SwVfpRegister s27 = { 27 };
const SwVfpRegister s28 = { 28 };
const SwVfpRegister s29 = { 29 };
const SwVfpRegister s30 = { 30 };
const SwVfpRegister s31 = { 31 };

const DwVfpRegister no_dreg = { -1 };
const LowDwVfpRegister d0 = { 0 };
const LowDwVfpRegister d1 = { 1 };
const LowDwVfpRegister d2 = { 2 };
const LowDwVfpRegister d3 = { 3 };
const LowDwVfpRegister d4 = { 4 };
const LowDwVfpRegister d5 = { 5 };
const LowDwVfpRegister d6 = { 6 };
const LowDwVfpRegister d7 = { 7 };
const LowDwVfpRegister d8 = { 8 };
const LowDwVfpRegister d9 = { 9 };
const LowDwVfpRegister d10 = { 10 };
const LowDwVfpRegister d11 = { 11 };
const LowDwVfpRegister d12 = { 12 };
const LowDwVfpRegister d13 = { 13 };
const LowDwVfpRegister d14 = { 14 };
const LowDwVfpRegister d15 = { 15 };
const DwVfpRegister d16 = { 16 };
const DwVfpRegister d17 = { 17 };
const DwVfpRegister d18 = { 18 };
const DwVfpRegister d19 = { 19 };
const DwVfpRegister d20 = { 20 };
const DwVfpRegister d21 = { 21 };
const DwVfpRegister d22 = { 22 };
const DwVfpRegister d23 = { 23 };
const DwVfpRegister d24 = { 24 };
const DwVfpRegister d25 = { 25 };
const DwVfpRegister d26 = { 26 };
const DwVfpRegister d27 = { 27 };
const DwVfpRegister d28 = { 28 };
const DwVfpRegister d29 = { 29 };
const DwVfpRegister d30 = { 30 };
const DwVfpRegister d31 = { 31 };

const QwNeonRegister q0  = {  0 };
const QwNeonRegister q1  = {  1 };
const QwNeonRegister q2  = {  2 };
const QwNeonRegister q3  = {  3 };
const QwNeonRegister q4  = {  4 };
const QwNeonRegister q5  = {  5 };
const QwNeonRegister q6  = {  6 };
const QwNeonRegister q7  = {  7 };
const QwNeonRegister q8  = {  8 };
const QwNeonRegister q9  = {  9 };
const QwNeonRegister q10 = { 10 };
const QwNeonRegister q11 = { 11 };
const QwNeonRegister q12 = { 12 };
const QwNeonRegister q13 = { 13 };
const QwNeonRegister q14 = { 14 };
const QwNeonRegister q15 = { 15 };


// Aliases for double registers.  Defined using #define instead of
// "static const DwVfpRegister&" because Clang complains otherwise when a
// compilation unit that includes this header doesn't use the variables.
#define kFirstCalleeSavedDoubleReg d8
#define kLastCalleeSavedDoubleReg d15
#define kDoubleRegZero d14
#define kScratchDoubleReg d15


// Coprocessor register
struct CRegister {
  bool is_valid() const { return 0 <= code_ && code_ < 16; }
  bool is(CRegister creg) const { return code_ == creg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};


const CRegister no_creg = { -1 };

const CRegister cr0  = {  0 };
const CRegister cr1  = {  1 };
const CRegister cr2  = {  2 };
const CRegister cr3  = {  3 };
const CRegister cr4  = {  4 };
const CRegister cr5  = {  5 };
const CRegister cr6  = {  6 };
const CRegister cr7  = {  7 };
const CRegister cr8  = {  8 };
const CRegister cr9  = {  9 };
const CRegister cr10 = { 10 };
const CRegister cr11 = { 11 };
const CRegister cr12 = { 12 };
const CRegister cr13 = { 13 };
const CRegister cr14 = { 14 };
const CRegister cr15 = { 15 };


// Coprocessor number
enum Coprocessor {
  p0  = 0,
  p1  = 1,
  p2  = 2,
  p3  = 3,
  p4  = 4,
  p5  = 5,
  p6  = 6,
  p7  = 7,
  p8  = 8,
  p9  = 9,
  p10 = 10,
  p11 = 11,
  p12 = 12,
  p13 = 13,
  p14 = 14,
  p15 = 15
};


// -----------------------------------------------------------------------------
// Machine instruction Operands

// Class Operand represents a shifter operand in data processing instructions
class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(int32_t immediate,
         RelocInfo::Mode rmode = RelocInfo::NONE32));
  INLINE(static Operand Zero()) {
    return Operand(static_cast<int32_t>(0));
  }
  INLINE(explicit Operand(const ExternalReference& f));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // rm
  INLINE(explicit Operand(Register rm));

  // rm <shift_op> shift_imm
  explicit Operand(Register rm, ShiftOp shift_op, int shift_imm);
  INLINE(static Operand SmiUntag(Register rm)) {
    return Operand(rm, ASR, kSmiTagSize);
  }
  INLINE(static Operand PointerOffsetFromSmiKey(Register key)) {
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
    return Operand(key, LSL, kPointerSizeLog2 - kSmiTagSize);
  }
  INLINE(static Operand DoubleOffsetFromSmiKey(Register key)) {
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kDoubleSizeLog2);
    return Operand(key, LSL, kDoubleSizeLog2 - kSmiTagSize);
  }

  // rm <shift_op> rs
  explicit Operand(Register rm, ShiftOp shift_op, Register rs);

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  // Return the number of actual instructions required to implement the given
  // instruction for this particular operand. This can be a single instruction,
  // if no load into the ip register is necessary, or anything between 2 and 4
  // instructions when we need to load from the constant pool (depending upon
  // whether the constant pool entry is in the small or extended section). If
  // the instruction this operand is used for is a MOV or MVN instruction the
  // actual instruction to use is required for this calculation. For other
  // instructions instr is ignored.
  //
  // The value returned is only valid as long as no entries are added to the
  // constant pool between this call and the actual instruction being emitted.
  int instructions_required(const Assembler* assembler, Instr instr = 0) const;
  bool must_output_reloc_info(const Assembler* assembler) const;

  inline int32_t immediate() const {
    DCHECK(!rm_.is_valid());
    return imm32_;
  }

  Register rm() const { return rm_; }
  Register rs() const { return rs_; }
  ShiftOp shift_op() const { return shift_op_; }

 private:
  Register rm_;
  Register rs_;
  ShiftOp shift_op_;
  int shift_imm_;  // valid if rm_ != no_reg && rs_ == no_reg
  int32_t imm32_;  // valid if rm_ == no_reg
  RelocInfo::Mode rmode_;

  friend class Assembler;
};


// Class MemOperand represents a memory operand in load and store instructions
class MemOperand BASE_EMBEDDED {
 public:
  // [rn +/- offset]      Offset/NegOffset
  // [rn +/- offset]!     PreIndex/NegPreIndex
  // [rn], +/- offset     PostIndex/NegPostIndex
  // offset is any signed 32-bit value; offset is first loaded to register ip if
  // it does not fit the addressing mode (12-bit unsigned and sign bit)
  explicit MemOperand(Register rn, int32_t offset = 0, AddrMode am = Offset);

  // [rn +/- rm]          Offset/NegOffset
  // [rn +/- rm]!         PreIndex/NegPreIndex
  // [rn], +/- rm         PostIndex/NegPostIndex
  explicit MemOperand(Register rn, Register rm, AddrMode am = Offset);

  // [rn +/- rm <shift_op> shift_imm]      Offset/NegOffset
  // [rn +/- rm <shift_op> shift_imm]!     PreIndex/NegPreIndex
  // [rn], +/- rm <shift_op> shift_imm     PostIndex/NegPostIndex
  explicit MemOperand(Register rn, Register rm,
                      ShiftOp shift_op, int shift_imm, AddrMode am = Offset);
  INLINE(static MemOperand PointerAddressFromSmiKey(Register array,
                                                    Register key,
                                                    AddrMode am = Offset)) {
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
    return MemOperand(array, key, LSL, kPointerSizeLog2 - kSmiTagSize, am);
  }

  void set_offset(int32_t offset) {
      DCHECK(rm_.is(no_reg));
      offset_ = offset;
  }

  uint32_t offset() const {
      DCHECK(rm_.is(no_reg));
      return offset_;
  }

  Register rn() const { return rn_; }
  Register rm() const { return rm_; }
  AddrMode am() const { return am_; }

  bool OffsetIsUint12Encodable() const {
    return offset_ >= 0 ? is_uint12(offset_) : is_uint12(-offset_);
  }

 private:
  Register rn_;  // base
  Register rm_;  // register offset
  int32_t offset_;  // valid if rm_ == no_reg
  ShiftOp shift_op_;
  int shift_imm_;  // valid if rm_ != no_reg && rs_ == no_reg
  AddrMode am_;  // bits P, U, and W

  friend class Assembler;
};


// Class NeonMemOperand represents a memory operand in load and
// store NEON instructions
class NeonMemOperand BASE_EMBEDDED {
 public:
  // [rn {:align}]       Offset
  // [rn {:align}]!      PostIndex
  explicit NeonMemOperand(Register rn, AddrMode am = Offset, int align = 0);

  // [rn {:align}], rm   PostIndex
  explicit NeonMemOperand(Register rn, Register rm, int align = 0);

  Register rn() const { return rn_; }
  Register rm() const { return rm_; }
  int align() const { return align_; }

 private:
  void SetAlignment(int align);

  Register rn_;  // base
  Register rm_;  // register increment
  int align_;
};


// Class NeonListOperand represents a list of NEON registers
class NeonListOperand BASE_EMBEDDED {
 public:
  explicit NeonListOperand(DoubleRegister base, int registers_count = 1);
  DoubleRegister base() const { return base_; }
  NeonListType type() const { return type_; }
 private:
  DoubleRegister base_;
  NeonListType type_;
};


// Class used to build a constant pool.
class ConstantPoolBuilder BASE_EMBEDDED {
 public:
  ConstantPoolBuilder();
  ConstantPoolArray::LayoutSection AddEntry(Assembler* assm,
                                            const RelocInfo& rinfo);
  void Relocate(int pc_delta);
  bool IsEmpty();
  Handle<ConstantPoolArray> New(Isolate* isolate);
  void Populate(Assembler* assm, ConstantPoolArray* constant_pool);

  inline ConstantPoolArray::LayoutSection current_section() const {
    return current_section_;
  }

  inline ConstantPoolArray::NumberOfEntries* number_of_entries(
      ConstantPoolArray::LayoutSection section) {
    return &number_of_entries_[section];
  }

  inline ConstantPoolArray::NumberOfEntries* small_entries() {
    return number_of_entries(ConstantPoolArray::SMALL_SECTION);
  }

  inline ConstantPoolArray::NumberOfEntries* extended_entries() {
    return number_of_entries(ConstantPoolArray::EXTENDED_SECTION);
  }

 private:
  struct ConstantPoolEntry {
    ConstantPoolEntry(RelocInfo rinfo, ConstantPoolArray::LayoutSection section,
                      int merged_index)
        : rinfo_(rinfo), section_(section), merged_index_(merged_index) {}

    RelocInfo rinfo_;
    ConstantPoolArray::LayoutSection section_;
    int merged_index_;
  };

  ConstantPoolArray::Type GetConstantPoolType(RelocInfo::Mode rmode);

  std::vector<ConstantPoolEntry> entries_;
  ConstantPoolArray::LayoutSection current_section_;
  ConstantPoolArray::NumberOfEntries number_of_entries_[2];
};

struct VmovIndex {
  unsigned char index;
};
const VmovIndex VmovIndexLo = { 0 };
const VmovIndex VmovIndexHi = { 1 };

class Assembler : public AssemblerBase {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is NULL, the assembler allocates and grows its own
  // buffer, and buffer_size determines the initial buffer size. The buffer is
  // owned by the assembler and deallocated upon destruction of the assembler.
  //
  // If the provided buffer is not NULL, the assembler uses the provided buffer
  // for code generation and assumes its size to be buffer_size. If the buffer
  // is too small, a fatal error occurs. No deallocation of the buffer is done
  // upon destruction of the assembler.
  Assembler(Isolate* isolate, void* buffer, int buffer_size);
  virtual ~Assembler();

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

  // Label operations & relative jumps (PPUM Appendix D)
  //
  // Takes a branch opcode (cc) and a label (L) and generates
  // either a backward branch or a forward branch and links it
  // to the label fixup chain. Usage:
  //
  // Label L;    // unbound label
  // j(cc, &L);  // forward branch to unbound label
  // bind(&L);   // bind label to the current pc
  // j(cc, &L);  // backward branch to bound label
  // bind(&L);   // illegal: a label may be bound only once
  //
  // Note: The same Label can be used for forward and backward branches
  // but it may be bound only once.

  void bind(Label* L);  // binds an unbound label L to the current code position

  // Returns the branch offset to the given label from the current code position
  // Links the label to the current position if it is still unbound
  // Manages the jump elimination optimization if the second parameter is true.
  int branch_offset(Label* L, bool jump_elimination_allowed);

  // Returns true if the given pc address is the start of a constant pool load
  // instruction sequence.
  INLINE(static bool is_constant_pool_load(Address pc));

  // Return the address in the constant pool of the code target address used by
  // the branch/call instruction at pc, or the object in a mov.
  INLINE(static Address constant_pool_entry_address(
    Address pc, ConstantPoolArray* constant_pool));

  // Read/Modify the code target address in the branch/call instruction at pc.
  INLINE(static Address target_address_at(Address pc,
                                          ConstantPoolArray* constant_pool));
  INLINE(static void set_target_address_at(Address pc,
                                           ConstantPoolArray* constant_pool,
                                           Address target,
                                           ICacheFlushMode icache_flush_mode =
                                               FLUSH_ICACHE_IF_NEEDED));
  INLINE(static Address target_address_at(Address pc, Code* code)) {
    ConstantPoolArray* constant_pool = code ? code->constant_pool() : NULL;
    return target_address_at(pc, constant_pool);
  }
  INLINE(static void set_target_address_at(Address pc,
                                           Code* code,
                                           Address target,
                                           ICacheFlushMode icache_flush_mode =
                                               FLUSH_ICACHE_IF_NEEDED)) {
    ConstantPoolArray* constant_pool = code ? code->constant_pool() : NULL;
    set_target_address_at(pc, constant_pool, target, icache_flush_mode);
  }

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  INLINE(static Address target_address_from_return_address(Address pc));

  // Given the address of the beginning of a call, return the address
  // in the instruction stream that the call will return from.
  INLINE(static Address return_address_from_call_start(Address pc));

  // Return the code target address of the patch debug break slot
  INLINE(static Address break_address_from_return_address(Address pc));

  // This sets the branch destination (which is in the constant pool on ARM).
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address constant_pool_entry, Code* code, Address target);

  // Here we are patching the address in the constant pool, not the actual call
  // instruction.  The address in the constant pool is the same size as a
  // pointer.
  static const int kSpecialTargetSize = kPointerSize;

  // Size of an instruction.
  static const int kInstrSize = sizeof(Instr);

  // Distance between start of patched return sequence and the emitted address
  // to jump to.
  // Patched return sequence is:
  //  ldr  ip, [pc, #0]   @ emited address and start
  //  blx  ip
  static const int kPatchReturnSequenceAddressOffset =  0 * kInstrSize;

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  // Patched debug break slot code is:
  //  ldr  ip, [pc, #0]   @ emited address and start
  //  blx  ip
  static const int kPatchDebugBreakSlotAddressOffset =  0 * kInstrSize;

  static const int kPatchDebugBreakSlotReturnOffset = 2 * kInstrSize;

  // Difference between address of current opcode and value read from pc
  // register.
  static const int kPcLoadDelta = 8;

  static const int kJSReturnSequenceInstructions = 4;
  static const int kDebugBreakSlotInstructions = 3;
  static const int kDebugBreakSlotLength =
      kDebugBreakSlotInstructions * kInstrSize;

  // ---------------------------------------------------------------------------
  // Code generation

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Branch instructions
  void b(int branch_offset, Condition cond = al);
  void bl(int branch_offset, Condition cond = al);
  void blx(int branch_offset);  // v5 and above
  void blx(Register target, Condition cond = al);  // v5 and above
  void bx(Register target, Condition cond = al);  // v5 and above, plus v4t

  // Convenience branch instructions using labels
  void b(Label* L, Condition cond = al)  {
    b(branch_offset(L, cond == al), cond);
  }
  void b(Condition cond, Label* L)  { b(branch_offset(L, cond == al), cond); }
  void bl(Label* L, Condition cond = al)  { bl(branch_offset(L, false), cond); }
  void bl(Condition cond, Label* L)  { bl(branch_offset(L, false), cond); }
  void blx(Label* L)  { blx(branch_offset(L, false)); }  // v5 and above

  // Data-processing instructions

  void and_(Register dst, Register src1, const Operand& src2,
            SBit s = LeaveCC, Condition cond = al);

  void eor(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void sub(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);
  void sub(Register dst, Register src1, Register src2,
           SBit s = LeaveCC, Condition cond = al) {
    sub(dst, src1, Operand(src2), s, cond);
  }

  void rsb(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void add(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);
  void add(Register dst, Register src1, Register src2,
           SBit s = LeaveCC, Condition cond = al) {
    add(dst, src1, Operand(src2), s, cond);
  }

  void adc(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void sbc(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void rsc(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void tst(Register src1, const Operand& src2, Condition cond = al);
  void tst(Register src1, Register src2, Condition cond = al) {
    tst(src1, Operand(src2), cond);
  }

  void teq(Register src1, const Operand& src2, Condition cond = al);

  void cmp(Register src1, const Operand& src2, Condition cond = al);
  void cmp(Register src1, Register src2, Condition cond = al) {
    cmp(src1, Operand(src2), cond);
  }
  void cmp_raw_immediate(Register src1, int raw_immediate, Condition cond = al);

  void cmn(Register src1, const Operand& src2, Condition cond = al);

  void orr(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);
  void orr(Register dst, Register src1, Register src2,
           SBit s = LeaveCC, Condition cond = al) {
    orr(dst, src1, Operand(src2), s, cond);
  }

  void mov(Register dst, const Operand& src,
           SBit s = LeaveCC, Condition cond = al);
  void mov(Register dst, Register src, SBit s = LeaveCC, Condition cond = al) {
    mov(dst, Operand(src), s, cond);
  }

  // Load the position of the label relative to the generated code object
  // pointer in a register.
  void mov_label_offset(Register dst, Label* label);

  // ARMv7 instructions for loading a 32 bit immediate in two instructions.
  // The constant for movw and movt should be in the range 0-0xffff.
  void movw(Register reg, uint32_t immediate, Condition cond = al);
  void movt(Register reg, uint32_t immediate, Condition cond = al);

  void bic(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void mvn(Register dst, const Operand& src,
           SBit s = LeaveCC, Condition cond = al);

  // Shift instructions

  void asr(Register dst, Register src1, const Operand& src2, SBit s = LeaveCC,
           Condition cond = al) {
    if (src2.is_reg()) {
      mov(dst, Operand(src1, ASR, src2.rm()), s, cond);
    } else {
      mov(dst, Operand(src1, ASR, src2.immediate()), s, cond);
    }
  }

  void lsl(Register dst, Register src1, const Operand& src2, SBit s = LeaveCC,
           Condition cond = al) {
    if (src2.is_reg()) {
      mov(dst, Operand(src1, LSL, src2.rm()), s, cond);
    } else {
      mov(dst, Operand(src1, LSL, src2.immediate()), s, cond);
    }
  }

  void lsr(Register dst, Register src1, const Operand& src2, SBit s = LeaveCC,
           Condition cond = al) {
    if (src2.is_reg()) {
      mov(dst, Operand(src1, LSR, src2.rm()), s, cond);
    } else {
      mov(dst, Operand(src1, LSR, src2.immediate()), s, cond);
    }
  }

  // Multiply instructions

  void mla(Register dst, Register src1, Register src2, Register srcA,
           SBit s = LeaveCC, Condition cond = al);

  void mls(Register dst, Register src1, Register src2, Register srcA,
           Condition cond = al);

  void sdiv(Register dst, Register src1, Register src2,
            Condition cond = al);

  void udiv(Register dst, Register src1, Register src2, Condition cond = al);

  void mul(Register dst, Register src1, Register src2,
           SBit s = LeaveCC, Condition cond = al);

  void smlal(Register dstL, Register dstH, Register src1, Register src2,
             SBit s = LeaveCC, Condition cond = al);

  void smull(Register dstL, Register dstH, Register src1, Register src2,
             SBit s = LeaveCC, Condition cond = al);

  void umlal(Register dstL, Register dstH, Register src1, Register src2,
             SBit s = LeaveCC, Condition cond = al);

  void umull(Register dstL, Register dstH, Register src1, Register src2,
             SBit s = LeaveCC, Condition cond = al);

  // Miscellaneous arithmetic instructions

  void clz(Register dst, Register src, Condition cond = al);  // v5 and above

  // Saturating instructions. v6 and above.

  // Unsigned saturate.
  //
  // Saturate an optionally shifted signed value to an unsigned range.
  //
  //   usat dst, #satpos, src
  //   usat dst, #satpos, src, lsl #sh
  //   usat dst, #satpos, src, asr #sh
  //
  // Register dst will contain:
  //
  //   0,                 if s < 0
  //   (1 << satpos) - 1, if s > ((1 << satpos) - 1)
  //   s,                 otherwise
  //
  // where s is the contents of src after shifting (if used.)
  void usat(Register dst, int satpos, const Operand& src, Condition cond = al);

  // Bitfield manipulation instructions. v7 and above.

  void ubfx(Register dst, Register src, int lsb, int width,
            Condition cond = al);

  void sbfx(Register dst, Register src, int lsb, int width,
            Condition cond = al);

  void bfc(Register dst, int lsb, int width, Condition cond = al);

  void bfi(Register dst, Register src, int lsb, int width,
           Condition cond = al);

  void pkhbt(Register dst, Register src1, const Operand& src2,
             Condition cond = al);

  void pkhtb(Register dst, Register src1, const Operand& src2,
             Condition cond = al);

  void uxtb(Register dst, const Operand& src, Condition cond = al);

  void uxtab(Register dst, Register src1, const Operand& src2,
             Condition cond = al);

  void uxtb16(Register dst, const Operand& src, Condition cond = al);

  // Status register access instructions

  void mrs(Register dst, SRegister s, Condition cond = al);
  void msr(SRegisterFieldMask fields, const Operand& src, Condition cond = al);

  // Load/Store instructions
  void ldr(Register dst, const MemOperand& src, Condition cond = al);
  void str(Register src, const MemOperand& dst, Condition cond = al);
  void ldrb(Register dst, const MemOperand& src, Condition cond = al);
  void strb(Register src, const MemOperand& dst, Condition cond = al);
  void ldrh(Register dst, const MemOperand& src, Condition cond = al);
  void strh(Register src, const MemOperand& dst, Condition cond = al);
  void ldrsb(Register dst, const MemOperand& src, Condition cond = al);
  void ldrsh(Register dst, const MemOperand& src, Condition cond = al);
  void ldrd(Register dst1,
            Register dst2,
            const MemOperand& src, Condition cond = al);
  void strd(Register src1,
            Register src2,
            const MemOperand& dst, Condition cond = al);

  // Preload instructions
  void pld(const MemOperand& address);

  // Load/Store multiple instructions
  void ldm(BlockAddrMode am, Register base, RegList dst, Condition cond = al);
  void stm(BlockAddrMode am, Register base, RegList src, Condition cond = al);

  // Exception-generating instructions and debugging support
  void stop(const char* msg,
            Condition cond = al,
            int32_t code = kDefaultStopCode);

  void bkpt(uint32_t imm16);  // v5 and above
  void svc(uint32_t imm24, Condition cond = al);

  // Coprocessor instructions

  void cdp(Coprocessor coproc, int opcode_1,
           CRegister crd, CRegister crn, CRegister crm,
           int opcode_2, Condition cond = al);

  void cdp2(Coprocessor coproc, int opcode_1,
            CRegister crd, CRegister crn, CRegister crm,
            int opcode_2);  // v5 and above

  void mcr(Coprocessor coproc, int opcode_1,
           Register rd, CRegister crn, CRegister crm,
           int opcode_2 = 0, Condition cond = al);

  void mcr2(Coprocessor coproc, int opcode_1,
            Register rd, CRegister crn, CRegister crm,
            int opcode_2 = 0);  // v5 and above

  void mrc(Coprocessor coproc, int opcode_1,
           Register rd, CRegister crn, CRegister crm,
           int opcode_2 = 0, Condition cond = al);

  void mrc2(Coprocessor coproc, int opcode_1,
            Register rd, CRegister crn, CRegister crm,
            int opcode_2 = 0);  // v5 and above

  void ldc(Coprocessor coproc, CRegister crd, const MemOperand& src,
           LFlag l = Short, Condition cond = al);
  void ldc(Coprocessor coproc, CRegister crd, Register base, int option,
           LFlag l = Short, Condition cond = al);

  void ldc2(Coprocessor coproc, CRegister crd, const MemOperand& src,
            LFlag l = Short);  // v5 and above
  void ldc2(Coprocessor coproc, CRegister crd, Register base, int option,
            LFlag l = Short);  // v5 and above

  // Support for VFP.
  // All these APIs support S0 to S31 and D0 to D31.

  void vldr(const DwVfpRegister dst,
            const Register base,
            int offset,
            const Condition cond = al);
  void vldr(const DwVfpRegister dst,
            const MemOperand& src,
            const Condition cond = al);

  void vldr(const SwVfpRegister dst,
            const Register base,
            int offset,
            const Condition cond = al);
  void vldr(const SwVfpRegister dst,
            const MemOperand& src,
            const Condition cond = al);

  void vstr(const DwVfpRegister src,
            const Register base,
            int offset,
            const Condition cond = al);
  void vstr(const DwVfpRegister src,
            const MemOperand& dst,
            const Condition cond = al);

  void vstr(const SwVfpRegister src,
            const Register base,
            int offset,
            const Condition cond = al);
  void vstr(const SwVfpRegister src,
            const MemOperand& dst,
            const Condition cond = al);

  void vldm(BlockAddrMode am,
            Register base,
            DwVfpRegister first,
            DwVfpRegister last,
            Condition cond = al);

  void vstm(BlockAddrMode am,
            Register base,
            DwVfpRegister first,
            DwVfpRegister last,
            Condition cond = al);

  void vldm(BlockAddrMode am,
            Register base,
            SwVfpRegister first,
            SwVfpRegister last,
            Condition cond = al);

  void vstm(BlockAddrMode am,
            Register base,
            SwVfpRegister first,
            SwVfpRegister last,
            Condition cond = al);

  void vmov(const DwVfpRegister dst,
            double imm,
            const Register scratch = no_reg);
  void vmov(const SwVfpRegister dst,
            const SwVfpRegister src,
            const Condition cond = al);
  void vmov(const DwVfpRegister dst,
            const DwVfpRegister src,
            const Condition cond = al);
  void vmov(const DwVfpRegister dst,
            const VmovIndex index,
            const Register src,
            const Condition cond = al);
  void vmov(const Register dst,
            const VmovIndex index,
            const DwVfpRegister src,
            const Condition cond = al);
  void vmov(const DwVfpRegister dst,
            const Register src1,
            const Register src2,
            const Condition cond = al);
  void vmov(const Register dst1,
            const Register dst2,
            const DwVfpRegister src,
            const Condition cond = al);
  void vmov(const SwVfpRegister dst,
            const Register src,
            const Condition cond = al);
  void vmov(const Register dst,
            const SwVfpRegister src,
            const Condition cond = al);
  void vcvt_f64_s32(const DwVfpRegister dst,
                    const SwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_f32_s32(const SwVfpRegister dst,
                    const SwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_f64_u32(const DwVfpRegister dst,
                    const SwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_s32_f64(const SwVfpRegister dst,
                    const DwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_u32_f64(const SwVfpRegister dst,
                    const DwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_f64_f32(const DwVfpRegister dst,
                    const SwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_f32_f64(const SwVfpRegister dst,
                    const DwVfpRegister src,
                    VFPConversionMode mode = kDefaultRoundToZero,
                    const Condition cond = al);
  void vcvt_f64_s32(const DwVfpRegister dst,
                    int fraction_bits,
                    const Condition cond = al);

  void vneg(const DwVfpRegister dst,
            const DwVfpRegister src,
            const Condition cond = al);
  void vabs(const DwVfpRegister dst,
            const DwVfpRegister src,
            const Condition cond = al);
  void vadd(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vsub(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vmul(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vmla(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vmls(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vdiv(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vcmp(const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vcmp(const DwVfpRegister src1,
            const double src2,
            const Condition cond = al);
  void vmrs(const Register dst,
            const Condition cond = al);
  void vmsr(const Register dst,
            const Condition cond = al);
  void vsqrt(const DwVfpRegister dst,
             const DwVfpRegister src,
             const Condition cond = al);

  // Support for NEON.
  // All these APIs support D0 to D31 and Q0 to Q15.

  void vld1(NeonSize size,
            const NeonListOperand& dst,
            const NeonMemOperand& src);
  void vst1(NeonSize size,
            const NeonListOperand& src,
            const NeonMemOperand& dst);
  void vmovl(NeonDataType dt, QwNeonRegister dst, DwVfpRegister src);

  // Pseudo instructions

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    DEBUG_BREAK_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED
  };

  void nop(int type = 0);   // 0 is the default non-marking type.

  void push(Register src, Condition cond = al) {
    str(src, MemOperand(sp, 4, NegPreIndex), cond);
  }

  void pop(Register dst, Condition cond = al) {
    ldr(dst, MemOperand(sp, 4, PostIndex), cond);
  }

  void pop() {
    add(sp, sp, Operand(kPointerSize));
  }

  // Jump unconditionally to given label.
  void jmp(Label* L) { b(L, al); }

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Check the number of instructions generated from label to here.
  int InstructionsGeneratedSince(Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Check whether an immediate fits an addressing mode 1 instruction.
  static bool ImmediateFitsAddrMode1Instruction(int32_t imm32);

  // Check whether an immediate fits an addressing mode 2 instruction.
  bool ImmediateFitsAddrMode2Instruction(int32_t imm32);

  // Class for scoping postponing the constant pool generation.
  class BlockConstPoolScope {
   public:
    explicit BlockConstPoolScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockConstPool();
    }
    ~BlockConstPoolScope() {
      assem_->EndBlockConstPool();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockConstPoolScope);
  };

  // Debugging

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot();

  // Record the AST id of the CallIC being compiled, so that it can be placed
  // in the relocation information.
  void SetRecordedAstId(TypeFeedbackId ast_id) {
    DCHECK(recorded_ast_id_.IsNone());
    recorded_ast_id_ = ast_id;
  }

  TypeFeedbackId RecordedAstId() {
    DCHECK(!recorded_ast_id_.IsNone());
    return recorded_ast_id_;
  }

  void ClearRecordedAstId() { recorded_ast_id_ = TypeFeedbackId::None(); }

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record the emission of a constant pool.
  //
  // The emission of constant pool depends on the size of the code generated and
  // the number of RelocInfo recorded.
  // The Debug mechanism needs to map code offsets between two versions of a
  // function, compiled with and without debugger support (see for example
  // Debug::PrepareForBreakPoints()).
  // Compiling functions with debugger support generates additional code
  // (DebugCodegen::GenerateSlot()). This may affect the emission of the
  // constant pools and cause the version of the code with debugger support to
  // have constant pools generated in different places.
  // Recording the position and size of emitted constant pools allows to
  // correctly compute the offset mappings between the different versions of a
  // function in all situations.
  //
  // The parameter indicates the size of the constant pool (in bytes), including
  // the marker and branch over the data.
  void RecordConstPool(int size);

  // Writes a single byte or word of data in the code stream.  Used
  // for inline tables, e.g., jump-tables. The constant pool should be
  // emitted before any use of db and dd to ensure that constant pools
  // are not emitted as part of the tables generated.
  void db(uint8_t data);
  void dd(uint32_t data);

  // Emits the address of the code stub's first instruction.
  void emit_code_stub_address(Code* stub);

  PositionsRecorder* positions_recorder() { return &positions_recorder_; }

  // Read/patch instructions
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }
  static Instr instr_at(byte* pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(byte* pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  static Condition GetCondition(Instr instr);
  static bool IsBranch(Instr instr);
  static int GetBranchOffset(Instr instr);
  static bool IsLdrRegisterImmediate(Instr instr);
  static bool IsVldrDRegisterImmediate(Instr instr);
  static Instr GetConsantPoolLoadPattern();
  static Instr GetConsantPoolLoadMask();
  static bool IsLdrPpRegOffset(Instr instr);
  static Instr GetLdrPpRegOffsetPattern();
  static bool IsLdrPpImmediateOffset(Instr instr);
  static bool IsVldrDPpImmediateOffset(Instr instr);
  static int GetLdrRegisterImmediateOffset(Instr instr);
  static int GetVldrDRegisterImmediateOffset(Instr instr);
  static Instr SetLdrRegisterImmediateOffset(Instr instr, int offset);
  static Instr SetVldrDRegisterImmediateOffset(Instr instr, int offset);
  static bool IsStrRegisterImmediate(Instr instr);
  static Instr SetStrRegisterImmediateOffset(Instr instr, int offset);
  static bool IsAddRegisterImmediate(Instr instr);
  static Instr SetAddRegisterImmediateOffset(Instr instr, int offset);
  static Register GetRd(Instr instr);
  static Register GetRn(Instr instr);
  static Register GetRm(Instr instr);
  static bool IsPush(Instr instr);
  static bool IsPop(Instr instr);
  static bool IsStrRegFpOffset(Instr instr);
  static bool IsLdrRegFpOffset(Instr instr);
  static bool IsStrRegFpNegOffset(Instr instr);
  static bool IsLdrRegFpNegOffset(Instr instr);
  static bool IsLdrPcImmediateOffset(Instr instr);
  static bool IsVldrDPcImmediateOffset(Instr instr);
  static bool IsBlxReg(Instr instr);
  static bool IsBlxIp(Instr instr);
  static bool IsTstImmediate(Instr instr);
  static bool IsCmpRegister(Instr instr);
  static bool IsCmpImmediate(Instr instr);
  static Register GetCmpImmediateRegister(Instr instr);
  static int GetCmpImmediateRawImmediate(Instr instr);
  static bool IsNop(Instr instr, int type = NON_MARKING_NOP);
  static bool IsMovImmed(Instr instr);
  static bool IsOrrImmed(Instr instr);
  static bool IsMovT(Instr instr);
  static Instr GetMovTPattern();
  static bool IsMovW(Instr instr);
  static Instr GetMovWPattern();
  static Instr EncodeMovwImmediate(uint32_t immediate);
  static Instr PatchMovwImmediate(Instr instruction, uint32_t immediate);
  static int DecodeShiftImm(Instr instr);
  static Instr PatchShiftImm(Instr instr, int immed);

  // Constants in pools are accessed via pc relative addressing, which can
  // reach +/-4KB for integer PC-relative loads and +/-1KB for floating-point
  // PC-relative loads, thereby defining a maximum distance between the
  // instruction and the accessed constant.
  static const int kMaxDistToIntPool = 4*KB;
  static const int kMaxDistToFPPool = 1*KB;
  // All relocations could be integer, it therefore acts as the limit.
  static const int kMaxNumPending32RelocInfo = kMaxDistToIntPool/kInstrSize;
  static const int kMaxNumPending64RelocInfo = kMaxDistToFPPool/kInstrSize;

  // Postpone the generation of the constant pool for the specified number of
  // instructions.
  void BlockConstPoolFor(int instructions);

  // Check if is time to emit a constant pool.
  void CheckConstPool(bool force_emit, bool require_jump);

  // Allocate a constant pool of the correct size for the generated code.
  Handle<ConstantPoolArray> NewConstantPool(Isolate* isolate);

  // Generate the constant pool for the generated code.
  void PopulateConstantPool(ConstantPoolArray* constant_pool);

  bool is_constant_pool_available() const { return constant_pool_available_; }

  bool use_extended_constant_pool() const {
    return constant_pool_builder_.current_section() ==
           ConstantPoolArray::EXTENDED_SECTION;
  }


 protected:
  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  TypeFeedbackId recorded_ast_id_;

  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos
  int target_at(int pos);

  // Patch branch instruction at pos to branch to given branch target pos
  void target_at_put(int pos, int target_pos);

  // Prevent contant pool emission until EndBlockConstPool is called.
  // Call to this function can be nested but must be followed by an equal
  // number of call to EndBlockConstpool.
  void StartBlockConstPool() {
    if (const_pool_blocked_nesting_++ == 0) {
      // Prevent constant pool checks happening by setting the next check to
      // the biggest possible offset.
      next_buffer_check_ = kMaxInt;
    }
  }

  // Resume constant pool emission. Need to be called as many time as
  // StartBlockConstPool to have an effect.
  void EndBlockConstPool() {
    if (--const_pool_blocked_nesting_ == 0) {
#ifdef DEBUG
      // Max pool start (if we need a jump and an alignment).
      int start = pc_offset() + kInstrSize + 2 * kPointerSize;
      // Check the constant pool hasn't been blocked for too long.
      DCHECK((num_pending_32_bit_reloc_info_ == 0) ||
             (start + num_pending_64_bit_reloc_info_ * kDoubleSize <
              (first_const_pool_32_use_ + kMaxDistToIntPool)));
      DCHECK((num_pending_64_bit_reloc_info_ == 0) ||
             (start < (first_const_pool_64_use_ + kMaxDistToFPPool)));
#endif
      // Two cases:
      //  * no_const_pool_before_ >= next_buffer_check_ and the emission is
      //    still blocked
      //  * no_const_pool_before_ < next_buffer_check_ and the next emit will
      //    trigger a check.
      next_buffer_check_ = no_const_pool_before_;
    }
  }

  bool is_const_pool_blocked() const {
    return (const_pool_blocked_nesting_ > 0) ||
           (pc_offset() < no_const_pool_before_);
  }

  void set_constant_pool_available(bool available) {
    constant_pool_available_ = available;
  }

 private:
  int next_buffer_check_;  // pc offset of next buffer check

  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;

  // Constant pool generation
  // Pools are emitted in the instruction stream, preferably after unconditional
  // jumps or after returns from functions (in dead code locations).
  // If a long code sequence does not contain unconditional jumps, it is
  // necessary to emit the constant pool before the pool gets too far from the
  // location it is accessed from. In this case, we emit a jump over the emitted
  // constant pool.
  // Constants in the pool may be addresses of functions that gets relocated;
  // if so, a relocation info entry is associated to the constant pool entry.

  // Repeated checking whether the constant pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated. That also means that the sizing of the buffers is not
  // an exact science, and that we rely on some slop to not overrun buffers.
  static const int kCheckPoolIntervalInst = 32;
  static const int kCheckPoolInterval = kCheckPoolIntervalInst * kInstrSize;


  // Emission of the constant pool may be blocked in some code sequences.
  int const_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_const_pool_before_;  // Block emission before this pc offset.

  // Keep track of the first instruction requiring a constant pool entry
  // since the previous constant pool was emitted.
  int first_const_pool_32_use_;
  int first_const_pool_64_use_;

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static const int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // Relocation info records are also used during code generation as temporary
  // containers for constants and code target addresses until they are emitted
  // to the constant pool. These pending relocation info records are temporarily
  // stored in a separate buffer until a constant pool is emitted.
  // If every instruction in a long sequence is accessing the pool, we need one
  // pending relocation entry per instruction.

  // The buffers of pending relocation info.
  RelocInfo pending_32_bit_reloc_info_[kMaxNumPending32RelocInfo];
  RelocInfo pending_64_bit_reloc_info_[kMaxNumPending64RelocInfo];
  // Number of pending reloc info entries in the 32 bits buffer.
  int num_pending_32_bit_reloc_info_;
  // Number of pending reloc info entries in the 64 bits buffer.
  int num_pending_64_bit_reloc_info_;

  ConstantPoolBuilder constant_pool_builder_;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Indicates whether the constant pool can be accessed, which is only possible
  // if the pp register points to the current code object's constant pool.
  bool constant_pool_available_;

  // Code emission
  inline void CheckBuffer();
  void GrowBuffer();
  inline void emit(Instr x);

  // 32-bit immediate values
  void move_32_bit_immediate(Register rd,
                             const Operand& x,
                             Condition cond = al);

  // Instruction generation
  void addrmod1(Instr instr, Register rn, Register rd, const Operand& x);
  void addrmod2(Instr instr, Register rd, const MemOperand& x);
  void addrmod3(Instr instr, Register rd, const MemOperand& x);
  void addrmod4(Instr instr, Register rn, RegList rl);
  void addrmod5(Instr instr, CRegister crd, const MemOperand& x);

  // Labels
  void print(Label* L);
  void bind_to(Label* L, int pos);
  void next(Label* L);

  enum UseConstantPoolMode {
    USE_CONSTANT_POOL,
    DONT_USE_CONSTANT_POOL
  };

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);
  void RecordRelocInfo(const RelocInfo& rinfo);
  ConstantPoolArray::LayoutSection ConstantPoolAddEntry(const RelocInfo& rinfo);

  friend class RelocInfo;
  friend class CodePatcher;
  friend class BlockConstPoolScope;
  friend class FrameAndConstantPoolScope;
  friend class ConstantPoolUnavailableScope;

  PositionsRecorder positions_recorder_;
  friend class PositionsRecorder;
  friend class EnsureSpace;
};


class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) {
    assembler->CheckBuffer();
  }
};


} }  // namespace v8::internal

#endif  // V8_ARM_ASSEMBLER_ARM_H_

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
// Copyright 2010 the V8 project authors. All rights reserved.

// A light-weight ARM Assembler
// Generates user mode instructions for the ARM architecture up to version 5

#ifndef V8_ARM_ASSEMBLER_ARM_H_
#define V8_ARM_ASSEMBLER_ARM_H_
#include <stdio.h>
#include "assembler.h"
#include "serialize.h"

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
//
// Core register
struct Register {
  bool is_valid() const  { return 0 <= code_ && code_ < 16; }
  bool is(Register reg) const  { return code_ == reg.code_; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    ASSERT(is_valid());
    return 1 << code_;
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};

const Register no_reg = { -1 };

const Register r0  = {  0 };
const Register r1  = {  1 };
const Register r2  = {  2 };
const Register r3  = {  3 };
const Register r4  = {  4 };
const Register r5  = {  5 };
const Register r6  = {  6 };
const Register r7  = {  7 };
const Register r8  = {  8 };  // Used as context register.
const Register r9  = {  9 };
const Register r10 = { 10 };  // Used as roots register.
const Register fp  = { 11 };
const Register ip  = { 12 };
const Register sp  = { 13 };
const Register lr  = { 14 };
const Register pc  = { 15 };

// Single word VFP register.
struct SwVfpRegister {
  bool is_valid() const  { return 0 <= code_ && code_ < 32; }
  bool is(SwVfpRegister reg) const  { return code_ == reg.code_; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    ASSERT(is_valid());
    return 1 << code_;
  }

  int code_;
};


// Double word VFP register.
struct DwVfpRegister {
  // Supporting d0 to d15, can be later extended to d31.
  bool is_valid() const  { return 0 <= code_ && code_ < 16; }
  bool is(DwVfpRegister reg) const  { return code_ == reg.code_; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    ASSERT(is_valid());
    return 1 << code_;
  }

  int code_;
};


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

const DwVfpRegister d0  = {  0 };
const DwVfpRegister d1  = {  1 };
const DwVfpRegister d2  = {  2 };
const DwVfpRegister d3  = {  3 };
const DwVfpRegister d4  = {  4 };
const DwVfpRegister d5  = {  5 };
const DwVfpRegister d6  = {  6 };
const DwVfpRegister d7  = {  7 };
const DwVfpRegister d8  = {  8 };
const DwVfpRegister d9  = {  9 };
const DwVfpRegister d10 = { 10 };
const DwVfpRegister d11 = { 11 };
const DwVfpRegister d12 = { 12 };
const DwVfpRegister d13 = { 13 };
const DwVfpRegister d14 = { 14 };
const DwVfpRegister d15 = { 15 };


// Coprocessor register
struct CRegister {
  bool is_valid() const  { return 0 <= code_ && code_ < 16; }
  bool is(CRegister creg) const  { return code_ == creg.code_; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    ASSERT(is_valid());
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


// Condition field in instructions.
enum Condition {
  eq =  0 << 28,  // Z set            equal.
  ne =  1 << 28,  // Z clear          not equal.
  nz =  1 << 28,  // Z clear          not zero.
  cs =  2 << 28,  // C set            carry set.
  hs =  2 << 28,  // C set            unsigned higher or same.
  cc =  3 << 28,  // C clear          carry clear.
  lo =  3 << 28,  // C clear          unsigned lower.
  mi =  4 << 28,  // N set            negative.
  pl =  5 << 28,  // N clear          positive or zero.
  vs =  6 << 28,  // V set            overflow.
  vc =  7 << 28,  // V clear          no overflow.
  hi =  8 << 28,  // C set, Z clear   unsigned higher.
  ls =  9 << 28,  // C clear or Z set unsigned lower or same.
  ge = 10 << 28,  // N == V           greater or equal.
  lt = 11 << 28,  // N != V           less than.
  gt = 12 << 28,  // Z clear, N == V  greater than.
  le = 13 << 28,  // Z set or N != V  less then or equal
  al = 14 << 28   //                  always.
};


// Returns the equivalent of !cc.
INLINE(Condition NegateCondition(Condition cc));


// Corresponds to transposing the operands of a comparison.
inline Condition ReverseCondition(Condition cc) {
  switch (cc) {
    case lo:
      return hi;
    case hi:
      return lo;
    case hs:
      return ls;
    case ls:
      return hs;
    case lt:
      return gt;
    case gt:
      return lt;
    case ge:
      return le;
    case le:
      return ge;
    default:
      return cc;
  };
}


// Branch hints are not used on the ARM.  They are defined so that they can
// appear in shared function signatures, but will be ignored in ARM
// implementations.
enum Hint { no_hint };

// Hints are not used on the arm.  Negating is trivial.
inline Hint NegateHint(Hint ignored) { return no_hint; }


// -----------------------------------------------------------------------------
// Addressing modes and instruction variants

// Shifter operand shift operation
enum ShiftOp {
  LSL = 0 << 5,
  LSR = 1 << 5,
  ASR = 2 << 5,
  ROR = 3 << 5,
  RRX = -1
};


// Condition code updating mode
enum SBit {
  SetCC   = 1 << 20,  // set condition code
  LeaveCC = 0 << 20   // leave condition code unchanged
};


// Status register selection
enum SRegister {
  CPSR = 0 << 22,
  SPSR = 1 << 22
};


// Status register fields
enum SRegisterField {
  CPSR_c = CPSR | 1 << 16,
  CPSR_x = CPSR | 1 << 17,
  CPSR_s = CPSR | 1 << 18,
  CPSR_f = CPSR | 1 << 19,
  SPSR_c = SPSR | 1 << 16,
  SPSR_x = SPSR | 1 << 17,
  SPSR_s = SPSR | 1 << 18,
  SPSR_f = SPSR | 1 << 19
};

// Status register field mask (or'ed SRegisterField enum values)
typedef uint32_t SRegisterFieldMask;


// Memory operand addressing mode
enum AddrMode {
  // bit encoding P U W
  Offset       = (8|4|0) << 21,  // offset (without writeback to base)
  PreIndex     = (8|4|1) << 21,  // pre-indexed addressing with writeback
  PostIndex    = (0|4|0) << 21,  // post-indexed addressing with writeback
  NegOffset    = (8|0|0) << 21,  // negative offset (without writeback to base)
  NegPreIndex  = (8|0|1) << 21,  // negative pre-indexed with writeback
  NegPostIndex = (0|0|0) << 21   // negative post-indexed with writeback
};


// Load/store multiple addressing mode
enum BlockAddrMode {
  // bit encoding P U W
  da           = (0|0|0) << 21,  // decrement after
  ia           = (0|4|0) << 21,  // increment after
  db           = (8|0|0) << 21,  // decrement before
  ib           = (8|4|0) << 21,  // increment before
  da_w         = (0|0|1) << 21,  // decrement after with writeback to base
  ia_w         = (0|4|1) << 21,  // increment after with writeback to base
  db_w         = (8|0|1) << 21,  // decrement before with writeback to base
  ib_w         = (8|4|1) << 21   // increment before with writeback to base
};


// Coprocessor load/store operand size
enum LFlag {
  Long  = 1 << 22,  // long load/store coprocessor
  Short = 0 << 22   // short load/store coprocessor
};


// -----------------------------------------------------------------------------
// Machine instruction Operands

// Class Operand represents a shifter operand in data processing instructions
class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(int32_t immediate,
         RelocInfo::Mode rmode = RelocInfo::NONE));
  INLINE(explicit Operand(const ExternalReference& f));
  INLINE(explicit Operand(const char* s));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // rm
  INLINE(explicit Operand(Register rm));

  // rm <shift_op> shift_imm
  explicit Operand(Register rm, ShiftOp shift_op, int shift_imm);

  // rm <shift_op> rs
  explicit Operand(Register rm, ShiftOp shift_op, Register rs);

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  Register rm() const { return rm_; }

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

 private:
  Register rn_;  // base
  Register rm_;  // register offset
  int32_t offset_;  // valid if rm_ == no_reg
  ShiftOp shift_op_;
  int shift_imm_;  // valid if rm_ != no_reg && rs_ == no_reg
  AddrMode am_;  // bits P, U, and W

  friend class Assembler;
};

// CpuFeatures keeps track of which features are supported by the target CPU.
// Supported features must be enabled by a Scope before use.
class CpuFeatures : public AllStatic {
 public:
  // Detect features of the target CPU. Set safe defaults if the serializer
  // is enabled (snapshots must be portable).
  static void Probe();

  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(CpuFeature f) {
    if (f == VFP3 && !FLAG_enable_vfp3) return false;
    return (supported_ & (1u << f)) != 0;
  }

  // Check whether a feature is currently enabled.
  static bool IsEnabled(CpuFeature f) {
    return (enabled_ & (1u << f)) != 0;
  }

  // Enable a specified feature within a scope.
  class Scope BASE_EMBEDDED {
#ifdef DEBUG
   public:
    explicit Scope(CpuFeature f) {
      ASSERT(CpuFeatures::IsSupported(f));
      ASSERT(!Serializer::enabled() ||
             (found_by_runtime_probing_ & (1u << f)) == 0);
      old_enabled_ = CpuFeatures::enabled_;
      CpuFeatures::enabled_ |= 1u << f;
    }
    ~Scope() { CpuFeatures::enabled_ = old_enabled_; }
   private:
    unsigned old_enabled_;
#else
   public:
    explicit Scope(CpuFeature f) {}
#endif
  };

 private:
  static unsigned supported_;
  static unsigned enabled_;
  static unsigned found_by_runtime_probing_;
};


typedef int32_t Instr;


extern const Instr kMovLrPc;
extern const Instr kLdrPCMask;
extern const Instr kLdrPCPattern;
extern const Instr kBlxRegMask;
extern const Instr kBlxRegPattern;


class Assembler : public Malloced {
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
  Assembler(void* buffer, int buffer_size);
  ~Assembler();

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

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

  // Return the address in the constant pool of the code target address used by
  // the branch/call instruction at pc.
  INLINE(static Address target_address_address_at(Address pc));

  // Read/Modify the code target address in the branch/call instruction at pc.
  INLINE(static Address target_address_at(Address pc));
  INLINE(static void set_target_address_at(Address pc, Address target));

  // This sets the branch destination (which is in the constant pool on ARM).
  // This is for calls and branches within generated code.
  inline static void set_target_at(Address constant_pool_entry, Address target);

  // This sets the branch destination (which is in the constant pool on ARM).
  // This is for calls and branches to runtime code.
  inline static void set_external_target_at(Address constant_pool_entry,
                                            Address target) {
    set_target_at(constant_pool_entry, target);
  }

  // Here we are patching the address in the constant pool, not the actual call
  // instruction.  The address in the constant pool is the same size as a
  // pointer.
  static const int kCallTargetSize = kPointerSize;
  static const int kExternalTargetSize = kPointerSize;

  // Size of an instruction.
  static const int kInstrSize = sizeof(Instr);

  // Distance between the instruction referring to the address of the call
  // target and the return address.
#ifdef USE_BLX
  // Call sequence is:
  //  ldr  ip, [pc, #...] @ call address
  //  blx  ip
  //                      @ return address
  static const int kCallTargetAddressOffset = 2 * kInstrSize;
#else
  // Call sequence is:
  //  mov  lr, pc
  //  ldr  pc, [pc, #...] @ call address
  //                      @ return address
  static const int kCallTargetAddressOffset = kInstrSize;
#endif

  // Distance between start of patched return sequence and the emitted address
  // to jump to.
#ifdef USE_BLX
  // Return sequence is:
  //  ldr  ip, [pc, #0]   @ emited address and start
  //  blx  ip
  static const int kPatchReturnSequenceAddressOffset =  0 * kInstrSize;
#else
  // Return sequence is:
  //  mov  lr, pc         @ start of sequence
  //  ldr  pc, [pc, #-4]  @ emited address
  static const int kPatchReturnSequenceAddressOffset =  kInstrSize;
#endif

  // Difference between address of current opcode and value read from pc
  // register.
  static const int kPcLoadDelta = 8;

  static const int kJSReturnSequenceLength = 4;

  // ---------------------------------------------------------------------------
  // Code generation

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);

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
  void ubfx(Register dst, Register src1, const Operand& src2,
            const Operand& src3, Condition cond = al);

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

  void bic(Register dst, Register src1, const Operand& src2,
           SBit s = LeaveCC, Condition cond = al);

  void mvn(Register dst, const Operand& src,
           SBit s = LeaveCC, Condition cond = al);

  // Multiply instructions

  void mla(Register dst, Register src1, Register src2, Register srcA,
           SBit s = LeaveCC, Condition cond = al);

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

  // Load/Store multiple instructions
  void ldm(BlockAddrMode am, Register base, RegList dst, Condition cond = al);
  void stm(BlockAddrMode am, Register base, RegList src, Condition cond = al);

  // Semaphore instructions
  void swp(Register dst, Register src, Register base, Condition cond = al);
  void swpb(Register dst, Register src, Register base, Condition cond = al);

  // Exception-generating instructions and debugging support
  void stop(const char* msg);

  void bkpt(uint32_t imm16);  // v5 and above
  void swi(uint32_t imm24, Condition cond = al);

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

  void stc(Coprocessor coproc, CRegister crd, const MemOperand& dst,
           LFlag l = Short, Condition cond = al);
  void stc(Coprocessor coproc, CRegister crd, Register base, int option,
           LFlag l = Short, Condition cond = al);

  void stc2(Coprocessor coproc, CRegister crd, const MemOperand& dst,
            LFlag l = Short);  // v5 and above
  void stc2(Coprocessor coproc, CRegister crd, Register base, int option,
            LFlag l = Short);  // v5 and above

  // Support for VFP.
  // All these APIs support S0 to S31 and D0 to D15.
  // Currently these APIs do not support extended D registers, i.e, D16 to D31.
  // However, some simple modifications can allow
  // these APIs to support D16 to D31.

  void vldr(const DwVfpRegister dst,
            const Register base,
            int offset,  // Offset must be a multiple of 4.
            const Condition cond = al);
  void vstr(const DwVfpRegister src,
            const Register base,
            int offset,  // Offset must be a multiple of 4.
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
  void vcvt(const DwVfpRegister dst,
            const SwVfpRegister src,
            const Condition cond = al);
  void vcvt(const SwVfpRegister dst,
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
  void vdiv(const DwVfpRegister dst,
            const DwVfpRegister src1,
            const DwVfpRegister src2,
            const Condition cond = al);
  void vcmp(const DwVfpRegister src1,
            const DwVfpRegister src2,
            const SBit s = LeaveCC,
            const Condition cond = al);
  void vmrs(const Register dst,
            const Condition cond = al);

  // Pseudo instructions
  void nop()  { mov(r0, Operand(r0)); }

  void push(Register src, Condition cond = al) {
    str(src, MemOperand(sp, 4, NegPreIndex), cond);
  }

  void pop(Register dst, Condition cond = al) {
    ldr(dst, MemOperand(sp, 4, PostIndex), cond);
  }

  void pop() {
    add(sp, sp, Operand(kPointerSize));
  }

  // Load effective address of memory operand x into register dst
  void lea(Register dst, const MemOperand& x,
           SBit s = LeaveCC, Condition cond = al);

  // Jump unconditionally to given label.
  void jmp(Label* L) { b(L, al); }

  // Check the code size generated from label to here.
  int InstructionsGeneratedSince(Label* l) {
    return (pc_offset() - l->pos()) / kInstrSize;
  }

  // Check whether an immediate fits an addressing mode 1 instruction.
  bool ImmediateFitsAddrMode1Instruction(int32_t imm32);

  // Postpone the generation of the constant pool for the specified number of
  // instructions.
  void BlockConstPoolFor(int instructions);

  // Debugging

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --debug_code to enable.
  void RecordComment(const char* msg);

  void RecordPosition(int pos);
  void RecordStatementPosition(int pos);
  void WriteRecordedPositions();

  int pc_offset() const { return pc_ - buffer_; }
  int current_position() const { return current_position_; }
  int current_statement_position() const { return current_position_; }

 protected:
  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Read/patch instructions
  static Instr instr_at(byte* pc) { return *reinterpret_cast<Instr*>(pc); }
  void instr_at_put(byte* pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }

  // Decode branch instruction at pos and return branch target pos
  int target_at(int pos);

  // Patch branch instruction at pos to branch to given branch target pos
  void target_at_put(int pos, int target_pos);

  // Check if is time to emit a constant pool for pending reloc info entries
  void CheckConstPool(bool force_emit, bool require_jump);

  // Block the emission of the constant pool before pc_offset
  void BlockConstPoolBefore(int pc_offset) {
    if (no_const_pool_before_ < pc_offset) no_const_pool_before_ = pc_offset;
  }

 private:
  // Code buffer:
  // The buffer into which code and relocation info are generated.
  byte* buffer_;
  int buffer_size_;
  // True if the assembler owns the buffer, false if buffer is external.
  bool own_buffer_;

  // Buffer size and constant pool distance are checked together at regular
  // intervals of kBufferCheckInterval emitted bytes
  static const int kBufferCheckInterval = 1*KB/2;
  int next_buffer_check_;  // pc offset of next buffer check

  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;
  byte* pc_;  // the program counter; moves forward

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
  static const int kCheckConstIntervalInst = 32;
  static const int kCheckConstInterval = kCheckConstIntervalInst * kInstrSize;


  // Pools are emitted after function return and in dead code at (more or less)
  // regular intervals of kDistBetweenPools bytes
  static const int kDistBetweenPools = 1*KB;

  // Constants in pools are accessed via pc relative addressing, which can
  // reach +/-4KB thereby defining a maximum distance between the instruction
  // and the accessed constant. We satisfy this constraint by limiting the
  // distance between pools.
  static const int kMaxDistBetweenPools = 4*KB - 2*kBufferCheckInterval;

  // Emission of the constant pool may be blocked in some code sequences
  int no_const_pool_before_;  // block emission before this pc offset

  // Keep track of the last emitted pool to guarantee a maximal distance
  int last_const_pool_end_;  // pc offset following the last constant pool

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
  static const int kMaxNumPRInfo = kMaxDistBetweenPools/kInstrSize;
  RelocInfo prinfo_[kMaxNumPRInfo];  // the buffer of pending relocation info
  int num_prinfo_;  // number of pending reloc info entries in the buffer

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // source position information
  int current_position_;
  int current_statement_position_;
  int written_position_;
  int written_statement_position_;

  // Code emission
  inline void CheckBuffer();
  void GrowBuffer();
  inline void emit(Instr x);

  // Instruction generation
  void addrmod1(Instr instr, Register rn, Register rd, const Operand& x);
  void addrmod2(Instr instr, Register rd, const MemOperand& x);
  void addrmod3(Instr instr, Register rd, const MemOperand& x);
  void addrmod4(Instr instr, Register rn, RegList rl);
  void addrmod5(Instr instr, CRegister crd, const MemOperand& x);

  // Labels
  void print(Label* L);
  void bind_to(Label* L, int pos);
  void link_to(Label* L, Label* appendix);
  void next(Label* L);

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  friend class RegExpMacroAssemblerARM;
  friend class RelocInfo;
  friend class CodePatcher;
};

} }  // namespace v8::internal

#endif  // V8_ARM_ASSEMBLER_ARM_H_

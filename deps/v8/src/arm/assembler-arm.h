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
#include "assembler.h"
#include "constants-arm.h"
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

// Core register
struct Register {
  static const int kNumRegisters = 16;
  static const int kNumAllocatableRegisters = 8;
  static const int kSizeInBytes = 4;

  static int ToAllocationIndex(Register reg) {
    ASSERT(reg.code() < kNumAllocatableRegisters);
    return reg.code();
  }

  static Register FromAllocationIndex(int index) {
    ASSERT(index >= 0 && index < kNumAllocatableRegisters);
    return from_code(index);
  }

  static const char* AllocationIndexToString(int index) {
    ASSERT(index >= 0 && index < kNumAllocatableRegisters);
    const char* const names[] = {
      "r0",
      "r1",
      "r2",
      "r3",
      "r4",
      "r5",
      "r6",
      "r7",
    };
    return names[index];
  }

  static Register from_code(int code) {
    Register r = { code };
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(Register reg) const { return code_ == reg.code_; }
  int code() const {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const {
    ASSERT(is_valid());
    return 1 << code_;
  }

  void set_code(int code) {
    code_ = code;
    ASSERT(is_valid());
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
const Register r9  = {  9 };  // Used as lithium codegen scratch register.
const Register r10 = { 10 };  // Used as roots register.
const Register fp  = { 11 };
const Register ip  = { 12 };
const Register sp  = { 13 };
const Register lr  = { 14 };
const Register pc  = { 15 };

// Single word VFP register.
struct SwVfpRegister {
  bool is_valid() const { return 0 <= code_ && code_ < 32; }
  bool is(SwVfpRegister reg) const { return code_ == reg.code_; }
  int code() const {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const {
    ASSERT(is_valid());
    return 1 << code_;
  }
  void split_code(int* vm, int* m) const {
    ASSERT(is_valid());
    *m = code_ & 0x1;
    *vm = code_ >> 1;
  }

  int code_;
};


// Double word VFP register.
struct DwVfpRegister {
  static const int kNumRegisters = 16;
  // A few double registers are reserved: one as a scratch register and one to
  // hold 0.0, that does not fit in the immediate field of vmov instructions.
  //  d14: 0.0
  //  d15: scratch register.
  static const int kNumReservedRegisters = 2;
  static const int kNumAllocatableRegisters = kNumRegisters -
      kNumReservedRegisters;

  inline static int ToAllocationIndex(DwVfpRegister reg);

  static DwVfpRegister FromAllocationIndex(int index) {
    ASSERT(index >= 0 && index < kNumAllocatableRegisters);
    return from_code(index);
  }

  static const char* AllocationIndexToString(int index) {
    ASSERT(index >= 0 && index < kNumAllocatableRegisters);
    const char* const names[] = {
      "d0",
      "d1",
      "d2",
      "d3",
      "d4",
      "d5",
      "d6",
      "d7",
      "d8",
      "d9",
      "d10",
      "d11",
      "d12",
      "d13"
    };
    return names[index];
  }

  static DwVfpRegister from_code(int code) {
    DwVfpRegister r = { code };
    return r;
  }

  // Supporting d0 to d15, can be later extended to d31.
  bool is_valid() const { return 0 <= code_ && code_ < 16; }
  bool is(DwVfpRegister reg) const { return code_ == reg.code_; }
  SwVfpRegister low() const {
    SwVfpRegister reg;
    reg.code_ = code_ * 2;

    ASSERT(reg.is_valid());
    return reg;
  }
  SwVfpRegister high() const {
    SwVfpRegister reg;
    reg.code_ = (code_ * 2) + 1;

    ASSERT(reg.is_valid());
    return reg;
  }
  int code() const {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const {
    ASSERT(is_valid());
    return 1 << code_;
  }
  void split_code(int* vm, int* m) const {
    ASSERT(is_valid());
    *m = (code_ & 0x10) >> 4;
    *vm = code_ & 0x0F;
  }

  int code_;
};


typedef DwVfpRegister DoubleRegister;


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

// Aliases for double registers.
const DwVfpRegister kFirstCalleeSavedDoubleReg = d8;
const DwVfpRegister kLastCalleeSavedDoubleReg = d15;
const DwVfpRegister kDoubleRegZero = d14;
const DwVfpRegister kScratchDoubleReg = d15;


// Coprocessor register
struct CRegister {
  bool is_valid() const { return 0 <= code_ && code_ < 16; }
  bool is(CRegister creg) const { return code_ == creg.code_; }
  int code() const {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const {
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


// -----------------------------------------------------------------------------
// Machine instruction Operands

// Class Operand represents a shifter operand in data processing instructions
class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(int32_t immediate,
         RelocInfo::Mode rmode = RelocInfo::NONE));
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

  // rm <shift_op> rs
  explicit Operand(Register rm, ShiftOp shift_op, Register rs);

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  // Return true if this operand fits in one instruction so that no
  // 2-instruction solution with a load into the ip register is necessary. If
  // the instruction this operand is used for is a MOV or MVN instruction the
  // actual instruction to use is required for this calculation. For other
  // instructions instr is ignored.
  bool is_single_instruction(Instr instr = 0) const;
  bool must_use_constant_pool() const;

  inline int32_t immediate() const {
    ASSERT(!rm_.is_valid());
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

  void set_offset(int32_t offset) {
      ASSERT(rm_.is(no_reg));
      offset_ = offset;
  }

  uint32_t offset() const {
      ASSERT(rm_.is(no_reg));
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

// CpuFeatures keeps track of which features are supported by the target CPU.
// Supported features must be enabled by a Scope before use.
class CpuFeatures : public AllStatic {
 public:
  // Detect features of the target CPU. Set safe defaults if the serializer
  // is enabled (snapshots must be portable).
  static void Probe();

  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(CpuFeature f) {
    ASSERT(initialized_);
    if (f == VFP3 && !FLAG_enable_vfp3) return false;
    return (supported_ & (1u << f)) != 0;
  }

#ifdef DEBUG
  // Check whether a feature is currently enabled.
  static bool IsEnabled(CpuFeature f) {
    ASSERT(initialized_);
    Isolate* isolate = Isolate::UncheckedCurrent();
    if (isolate == NULL) {
      // When no isolate is available, work as if we're running in
      // release mode.
      return IsSupported(f);
    }
    unsigned enabled = static_cast<unsigned>(isolate->enabled_cpu_features());
    return (enabled & (1u << f)) != 0;
  }
#endif

  // Enable a specified feature within a scope.
  class Scope BASE_EMBEDDED {
#ifdef DEBUG

   public:
    explicit Scope(CpuFeature f) {
      unsigned mask = 1u << f;
      ASSERT(CpuFeatures::IsSupported(f));
      ASSERT(!Serializer::enabled() ||
             (CpuFeatures::found_by_runtime_probing_ & mask) == 0);
      isolate_ = Isolate::UncheckedCurrent();
      old_enabled_ = 0;
      if (isolate_ != NULL) {
        old_enabled_ = static_cast<unsigned>(isolate_->enabled_cpu_features());
        isolate_->set_enabled_cpu_features(old_enabled_ | mask);
      }
    }
    ~Scope() {
      ASSERT_EQ(Isolate::UncheckedCurrent(), isolate_);
      if (isolate_ != NULL) {
        isolate_->set_enabled_cpu_features(old_enabled_);
      }
    }

   private:
    Isolate* isolate_;
    unsigned old_enabled_;
#else

   public:
    explicit Scope(CpuFeature f) {}
#endif
  };

  class TryForceFeatureScope BASE_EMBEDDED {
   public:
    explicit TryForceFeatureScope(CpuFeature f)
        : old_supported_(CpuFeatures::supported_) {
      if (CanForce()) {
        CpuFeatures::supported_ |= (1u << f);
      }
    }

    ~TryForceFeatureScope() {
      if (CanForce()) {
        CpuFeatures::supported_ = old_supported_;
      }
    }

   private:
    static bool CanForce() {
      // It's only safe to temporarily force support of CPU features
      // when there's only a single isolate, which is guaranteed when
      // the serializer is enabled.
      return Serializer::enabled();
    }

    const unsigned old_supported_;
  };

 private:
#ifdef DEBUG
  static bool initialized_;
#endif
  static unsigned supported_;
  static unsigned found_by_runtime_probing_;

  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};


extern const Instr kMovLrPc;
extern const Instr kLdrPCMask;
extern const Instr kLdrPCPattern;
extern const Instr kBlxRegMask;
extern const Instr kBlxRegPattern;

extern const Instr kMovMvnMask;
extern const Instr kMovMvnPattern;
extern const Instr kMovMvnFlip;

extern const Instr kMovLeaveCCMask;
extern const Instr kMovLeaveCCPattern;
extern const Instr kMovwMask;
extern const Instr kMovwPattern;
extern const Instr kMovwLeaveCCFlip;

extern const Instr kCmpCmnMask;
extern const Instr kCmpCmnPattern;
extern const Instr kCmpCmnFlip;
extern const Instr kAddSubFlip;
extern const Instr kAndBicFlip;



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
  ~Assembler();

  // Overrides the default provided by FLAG_debug_code.
  void set_emit_debug_code(bool value) { emit_debug_code_ = value; }

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
  // Patched return sequence is:
  //  ldr  ip, [pc, #0]   @ emited address and start
  //  blx  ip
  static const int kPatchReturnSequenceAddressOffset =  0 * kInstrSize;
#else
  // Patched return sequence is:
  //  mov  lr, pc         @ start of sequence
  //  ldr  pc, [pc, #-4]  @ emited address
  static const int kPatchReturnSequenceAddressOffset =  kInstrSize;
#endif

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
#ifdef USE_BLX
  // Patched debug break slot code is:
  //  ldr  ip, [pc, #0]   @ emited address and start
  //  blx  ip
  static const int kPatchDebugBreakSlotAddressOffset =  0 * kInstrSize;
#else
  // Patched debug break slot code is:
  //  mov  lr, pc         @ start of sequence
  //  ldr  pc, [pc, #-4]  @ emited address
  static const int kPatchDebugBreakSlotAddressOffset =  kInstrSize;
#endif

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

  // ARMv7 instructions for loading a 32 bit immediate in two instructions.
  // This may actually emit a different mov instruction, but on an ARMv7 it
  // is guaranteed to only emit one instruction.
  void movw(Register reg, uint32_t immediate, Condition cond = al);
  // The constant for movt should be in the range 0-0xffff.
  void movt(Register reg, uint32_t immediate, Condition cond = al);

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
  // All these APIs support S0 to S31 and D0 to D15.
  // Currently these APIs do not support extended D registers, i.e, D16 to D31.
  // However, some simple modifications can allow
  // these APIs to support D16 to D31.

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
            const Condition cond = al);
  void vmov(const SwVfpRegister dst,
            const SwVfpRegister src,
            const Condition cond = al);
  void vmov(const DwVfpRegister dst,
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
  bool ImmediateFitsAddrMode1Instruction(int32_t imm32);

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
  void SetRecordedAstId(unsigned ast_id) {
    ASSERT(recorded_ast_id_ == kNoASTId);
    recorded_ast_id_ = ast_id;
  }

  unsigned RecordedAstId() {
    ASSERT(recorded_ast_id_ != kNoASTId);
    return recorded_ast_id_;
  }

  void ClearRecordedAstId() { recorded_ast_id_ = kNoASTId; }

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Writes a single byte or word of data in the code stream.  Used
  // for inline tables, e.g., jump-tables. The constant pool should be
  // emitted before any use of db and dd to ensure that constant pools
  // are not emitted as part of the tables generated.
  void db(uint8_t data);
  void dd(uint32_t data);

  int pc_offset() const { return pc_ - buffer_; }

  PositionsRecorder* positions_recorder() { return &positions_recorder_; }

  // Read/patch instructions
  static Instr instr_at(byte* pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(byte* pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  static Condition GetCondition(Instr instr);
  static bool IsBranch(Instr instr);
  static int GetBranchOffset(Instr instr);
  static bool IsLdrRegisterImmediate(Instr instr);
  static int GetLdrRegisterImmediateOffset(Instr instr);
  static Instr SetLdrRegisterImmediateOffset(Instr instr, int offset);
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
  static bool IsTstImmediate(Instr instr);
  static bool IsCmpRegister(Instr instr);
  static bool IsCmpImmediate(Instr instr);
  static Register GetCmpImmediateRegister(Instr instr);
  static int GetCmpImmediateRawImmediate(Instr instr);
  static bool IsNop(Instr instr, int type = NON_MARKING_NOP);

  // Constants in pools are accessed via pc relative addressing, which can
  // reach +/-4KB thereby defining a maximum distance between the instruction
  // and the accessed constant.
  static const int kMaxDistToPool = 4*KB;
  static const int kMaxNumPendingRelocInfo = kMaxDistToPool/kInstrSize;

  // Postpone the generation of the constant pool for the specified number of
  // instructions.
  void BlockConstPoolFor(int instructions);

  // Check if is time to emit a constant pool.
  void CheckConstPool(bool force_emit, bool require_jump);

 protected:
  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  unsigned recorded_ast_id_;

  bool emit_debug_code() const { return emit_debug_code_; }

  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Read/patch instructions
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }

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
      // Check the constant pool hasn't been blocked for too long.
      ASSERT((num_pending_reloc_info_ == 0) ||
             (pc_offset() < (first_const_pool_use_ + kMaxDistToPool)));
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

 private:
  // Code buffer:
  // The buffer into which code and relocation info are generated.
  byte* buffer_;
  int buffer_size_;
  // True if the assembler owns the buffer, false if buffer is external.
  bool own_buffer_;

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
  static const int kCheckPoolIntervalInst = 32;
  static const int kCheckPoolInterval = kCheckPoolIntervalInst * kInstrSize;


  // Average distance beetween a constant pool and the first instruction
  // accessing the constant pool. Longer distance should result in less I-cache
  // pollution.
  // In practice the distance will be smaller since constant pool emission is
  // forced after function return and sometimes after unconditional branches.
  static const int kAvgDistToPool = kMaxDistToPool - kCheckPoolInterval;

  // Emission of the constant pool may be blocked in some code sequences.
  int const_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_const_pool_before_;  // Block emission before this pc offset.

  // Keep track of the first instruction requiring a constant pool entry
  // since the previous constant pool was emitted.
  int first_const_pool_use_;

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

  // the buffer of pending relocation info
  RelocInfo pending_reloc_info_[kMaxNumPendingRelocInfo];
  // number of pending reloc info entries in the buffer
  int num_pending_reloc_info_;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

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
  friend class BlockConstPoolScope;

  PositionsRecorder positions_recorder_;
  bool emit_debug_code_;
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

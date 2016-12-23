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


#ifndef V8_MIPS_ASSEMBLER_MIPS_H_
#define V8_MIPS_ASSEMBLER_MIPS_H_

#include <stdio.h>

#include <set>

#include "src/assembler.h"
#include "src/mips/constants-mips.h"

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(zero_reg)  V(at)  V(v0)  V(v1)  V(a0)  V(a1)  V(a2)  V(a3)  \
  V(t0)  V(t1)  V(t2)  V(t3)  V(t4)  V(t5)  V(t6)  V(t7)  \
  V(s0)  V(s1)  V(s2)  V(s3)  V(s4)  V(s5)  V(s6)  V(s7)  V(t8)  V(t9) \
  V(k0)  V(k1)  V(gp)  V(sp)  V(fp)  V(ra)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(v0)  V(v1)  V(a0)  V(a1)  V(a2)  V(a3) \
  V(t0)  V(t1)  V(t2)  V(t3)  V(t4)  V(t5)  V(t6) V(s7)

#define DOUBLE_REGISTERS(V)                               \
  V(f0)  V(f1)  V(f2)  V(f3)  V(f4)  V(f5)  V(f6)  V(f7)  \
  V(f8)  V(f9)  V(f10) V(f11) V(f12) V(f13) V(f14) V(f15) \
  V(f16) V(f17) V(f18) V(f19) V(f20) V(f21) V(f22) V(f23) \
  V(f24) V(f25) V(f26) V(f27) V(f28) V(f29) V(f30) V(f31)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(f0)  V(f2)  V(f4)  V(f6)  V(f8)  V(f10) V(f12) V(f14) \
  V(f16) V(f18) V(f20) V(f22) V(f24) V(f26)
// clang-format on

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


// -----------------------------------------------------------------------------
// Implementation of Register and FPURegister.

struct Register {
  static const int kCpRegister = 23;  // cp (s7) is the 23rd register.

  enum Code {
#define REGISTER_CODE(R) kCode_##R,
    GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
        kAfterLast,
    kCode_no_reg = -1
  };

  static const int kNumRegisters = Code::kAfterLast;

#if defined(V8_TARGET_LITTLE_ENDIAN)
  static const int kMantissaOffset = 0;
  static const int kExponentOffset = 4;
#elif defined(V8_TARGET_BIG_ENDIAN)
  static const int kMantissaOffset = 4;
  static const int kExponentOffset = 0;
#else
#error Unknown endianness
#endif


  static Register from_code(int code) {
    DCHECK(code >= 0);
    DCHECK(code < kNumRegisters);
    Register r = {code};
    return r;
  }
  bool is_valid() const { return 0 <= reg_code && reg_code < kNumRegisters; }
  bool is(Register reg) const { return reg_code == reg.reg_code; }
  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }

  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

// s7: context register
// s3: lithium scratch
// s4: lithium scratch2
#define DECLARE_REGISTER(R) const Register R = {Register::kCode_##R};
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
const Register no_reg = {Register::kCode_no_reg};


int ToNumber(Register reg);

Register ToRegister(int num);

static const bool kSimpleFPAliasing = true;

// Coprocessor register.
struct FPURegister {
  enum Code {
#define REGISTER_CODE(R) kCode_##R,
    DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
        kAfterLast,
    kCode_no_reg = -1
  };

  static const int kMaxNumRegisters = Code::kAfterLast;

  inline static int NumRegisters();

  // TODO(plind): Warning, inconsistent numbering here. kNumFPURegisters refers
  // to number of 32-bit FPU regs, but kNumAllocatableRegisters refers to
  // number of Double regs (64-bit regs, or FPU-reg-pairs).

  bool is_valid() const { return 0 <= reg_code && reg_code < kMaxNumRegisters; }
  bool is(FPURegister reg) const { return reg_code == reg.reg_code; }
  FPURegister low() const {
    // Find low reg of a Double-reg pair, which is the reg itself.
    DCHECK(reg_code % 2 == 0);  // Specified Double reg must be even.
    FPURegister reg;
    reg.reg_code = reg_code;
    DCHECK(reg.is_valid());
    return reg;
  }
  FPURegister high() const {
    // Find high reg of a Doubel-reg pair, which is reg + 1.
    DCHECK(reg_code % 2 == 0);  // Specified Double reg must be even.
    FPURegister reg;
    reg.reg_code = reg_code + 1;
    DCHECK(reg.is_valid());
    return reg;
  }

  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }

  static FPURegister from_code(int code) {
    FPURegister r = {code};
    return r;
  }
  void setcode(int f) {
    reg_code = f;
    DCHECK(is_valid());
  }
  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

// A few double registers are reserved: one as a scratch register and one to
// hold 0.0.
//  f28: 0.0
//  f30: scratch register.

// V8 now supports the O32 ABI, and the FPU Registers are organized as 32
// 32-bit registers, f0 through f31. When used as 'double' they are used
// in pairs, starting with the even numbered register. So a double operation
// on f0 really uses f0 and f1.
// (Modern mips hardware also supports 32 64-bit registers, via setting
// (priviledged) Status Register FR bit to 1. This is used by the N32 ABI,
// but it is not in common use. Someday we will want to support this in v8.)

// For O32 ABI, Floats and Doubles refer to same set of 32 32-bit registers.
typedef FPURegister FloatRegister;

typedef FPURegister DoubleRegister;

// TODO(mips) Define SIMD registers.
typedef FPURegister Simd128Register;

const DoubleRegister no_freg = {-1};

const DoubleRegister f0 = {0};  // Return value in hard float mode.
const DoubleRegister f1 = {1};
const DoubleRegister f2 = {2};
const DoubleRegister f3 = {3};
const DoubleRegister f4 = {4};
const DoubleRegister f5 = {5};
const DoubleRegister f6 = {6};
const DoubleRegister f7 = {7};
const DoubleRegister f8 = {8};
const DoubleRegister f9 = {9};
const DoubleRegister f10 = {10};
const DoubleRegister f11 = {11};
const DoubleRegister f12 = {12};  // Arg 0 in hard float mode.
const DoubleRegister f13 = {13};
const DoubleRegister f14 = {14};  // Arg 1 in hard float mode.
const DoubleRegister f15 = {15};
const DoubleRegister f16 = {16};
const DoubleRegister f17 = {17};
const DoubleRegister f18 = {18};
const DoubleRegister f19 = {19};
const DoubleRegister f20 = {20};
const DoubleRegister f21 = {21};
const DoubleRegister f22 = {22};
const DoubleRegister f23 = {23};
const DoubleRegister f24 = {24};
const DoubleRegister f25 = {25};
const DoubleRegister f26 = {26};
const DoubleRegister f27 = {27};
const DoubleRegister f28 = {28};
const DoubleRegister f29 = {29};
const DoubleRegister f30 = {30};
const DoubleRegister f31 = {31};

// Register aliases.
// cp is assumed to be a callee saved register.
// Defined using #define instead of "static const Register&" because Clang
// complains otherwise when a compilation unit that includes this header
// doesn't use the variables.
#define kRootRegister s6
#define cp s7
#define kLithiumScratchReg s3
#define kLithiumScratchReg2 s4
#define kLithiumScratchDouble f30
#define kDoubleRegZero f28
// Used on mips32r6 for compare operations.
// We use the last non-callee saved odd register for O32 ABI
#define kDoubleCompareReg f19

// FPU (coprocessor 1) control registers.
// Currently only FCSR (#31) is implemented.
struct FPUControlRegister {
  bool is_valid() const { return reg_code == kFCSRRegister; }
  bool is(FPUControlRegister creg) const { return reg_code == creg.reg_code; }
  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }
  void setcode(int f) {
    reg_code = f;
    DCHECK(is_valid());
  }
  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

const FPUControlRegister no_fpucreg = { kInvalidFPUControlRegister };
const FPUControlRegister FCSR = { kFCSRRegister };

// -----------------------------------------------------------------------------
// Machine instruction Operands.

// Class Operand represents a shifter operand in data processing instructions.
class Operand BASE_EMBEDDED {
 public:
  // Immediate.
  INLINE(explicit Operand(int32_t immediate,
         RelocInfo::Mode rmode = RelocInfo::NONE32));
  INLINE(explicit Operand(const ExternalReference& f));
  INLINE(explicit Operand(const char* s));
  INLINE(explicit Operand(Object** opp));
  INLINE(explicit Operand(Context** cpp));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // Register.
  INLINE(explicit Operand(Register rm));

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  inline int32_t immediate() const {
    DCHECK(!is_reg());
    return imm32_;
  }

  Register rm() const { return rm_; }

 private:
  Register rm_;
  int32_t imm32_;  // Valid if rm_ == no_reg.
  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};


// On MIPS we have only one adressing mode with base_reg + offset.
// Class MemOperand represents a memory operand in load and store instructions.
class MemOperand : public Operand {
 public:
  // Immediate value attached to offset.
  enum OffsetAddend {
    offset_minus_one = -1,
    offset_zero = 0
  };

  explicit MemOperand(Register rn, int32_t offset = 0);
  explicit MemOperand(Register rn, int32_t unit, int32_t multiplier,
                      OffsetAddend offset_addend = offset_zero);
  int32_t offset() const { return offset_; }

  bool OffsetIsInt16Encodable() const {
    return is_int16(offset_);
  }

 private:
  int32_t offset_;

  friend class Assembler;
};


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
  virtual ~Assembler() { }

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

  // Label operations & relative jumps (PPUM Appendix D).
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
  void bind(Label* L);  // Binds an unbound label L to current code position.

  enum OffsetSize : int { kOffset26 = 26, kOffset21 = 21, kOffset16 = 16 };

  // Determines if Label is bound and near enough so that branch instruction
  // can be used to reach it, instead of jump instruction.
  bool is_near(Label* L);
  bool is_near(Label* L, OffsetSize bits);
  bool is_near_branch(Label* L);
  inline bool is_near_pre_r6(Label* L) {
    DCHECK(!IsMipsArchVariant(kMips32r6));
    return pc_offset() - L->pos() < kMaxBranchOffset - 4 * kInstrSize;
  }
  inline bool is_near_r6(Label* L) {
    DCHECK(IsMipsArchVariant(kMips32r6));
    return pc_offset() - L->pos() < kMaxCompactBranchOffset - 4 * kInstrSize;
  }

  int BranchOffset(Instr instr);

  // Returns the branch offset to the given label from the current code
  // position. Links the label to the current position if it is still unbound.
  // Manages the jump elimination optimization if the second parameter is true.
  int32_t branch_offset_helper(Label* L, OffsetSize bits);
  inline int32_t branch_offset(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset16);
  }
  inline int32_t branch_offset21(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset21);
  }
  inline int32_t branch_offset26(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset26);
  }
  inline int32_t shifted_branch_offset(Label* L) {
    return branch_offset(L) >> 2;
  }
  inline int32_t shifted_branch_offset21(Label* L) {
    return branch_offset21(L) >> 2;
  }
  inline int32_t shifted_branch_offset26(Label* L) {
    return branch_offset26(L) >> 2;
  }
  uint32_t jump_address(Label* L);

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

  // Read/Modify the code target address in the branch/call instruction at pc.
  static Address target_address_at(Address pc);
  static void set_target_address_at(
      Isolate* isolate, Address pc, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  // On MIPS there is no Constant Pool so we skip that parameter.
  INLINE(static Address target_address_at(Address pc, Address constant_pool)) {
    return target_address_at(pc);
  }
  INLINE(static void set_target_address_at(
      Isolate* isolate, Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED)) {
    set_target_address_at(isolate, pc, target, icache_flush_mode);
  }
  INLINE(static Address target_address_at(Address pc, Code* code)) {
    Address constant_pool = code ? code->constant_pool() : NULL;
    return target_address_at(pc, constant_pool);
  }
  INLINE(static void set_target_address_at(
      Isolate* isolate, Address pc, Code* code, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED)) {
    Address constant_pool = code ? code->constant_pool() : NULL;
    set_target_address_at(isolate, pc, constant_pool, target,
                          icache_flush_mode);
  }

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  static void QuietNaN(HeapObject* nan);

  // This sets the branch destination (which gets loaded at the call address).
  // This is for calls and branches within generated code.  The serializer
  // has already deserialized the lui/ori instructions etc.
  inline static void deserialization_set_special_target_at(
      Isolate* isolate, Address instruction_payload, Code* code,
      Address target) {
    set_target_address_at(
        isolate,
        instruction_payload - kInstructionsFor32BitConstant * kInstrSize, code,
        target);
  }

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Isolate* isolate, Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  // Size of an instruction.
  static const int kInstrSize = sizeof(Instr);

  // Difference between address of current opcode and target address offset.
  static const int kBranchPCOffset = 4;

  // Here we are patching the address in the LUI/ORI instruction pair.
  // These values are used in the serialization process and must be zero for
  // MIPS platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.
  static const int kSpecialTargetSize = 0;

  // Number of consecutive instructions used to store 32bit constant. This
  // constant is used in RelocInfo::target_address_address() function to tell
  // serializer address of the instruction that follows LUI/ORI instruction
  // pair.
  static const int kInstructionsFor32BitConstant = 2;

  // Distance between the instruction referring to the address of the call
  // target and the return address.
#ifdef _MIPS_ARCH_MIPS32R6
  static const int kCallTargetAddressOffset = 3 * kInstrSize;
#else
  static const int kCallTargetAddressOffset = 4 * kInstrSize;
#endif

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  static const int kPatchDebugBreakSlotAddressOffset = 4 * kInstrSize;

  // Difference between address of current opcode and value read from pc
  // register.
  static const int kPcLoadDelta = 4;

#ifdef _MIPS_ARCH_MIPS32R6
  static const int kDebugBreakSlotInstructions = 3;
#else
  static const int kDebugBreakSlotInstructions = 4;
#endif
  static const int kDebugBreakSlotLength =
      kDebugBreakSlotInstructions * kInstrSize;


  // ---------------------------------------------------------------------------
  // Code generation.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

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
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED,
    // Code aging
    CODE_AGE_MARKER_NOP = 6,
    CODE_AGE_SEQUENCE_NOP
  };

  // Type == 0 is the default non-marking nop. For mips this is a
  // sll(zero_reg, zero_reg, 0). We use rt_reg == at for non-zero
  // marking, to avoid conflict with ssnop and ehb instructions.
  void nop(unsigned int type = 0) {
    DCHECK(type < 32);
    Register nop_rt_reg = (type == 0) ? zero_reg : at;
    sll(zero_reg, nop_rt_reg, type, true);
  }


  // --------Branch-and-jump-instructions----------
  // We don't use likely variant of instructions.
  void b(int16_t offset);
  inline void b(Label* L) { b(shifted_branch_offset(L)); }
  void bal(int16_t offset);
  inline void bal(Label* L) { bal(shifted_branch_offset(L)); }
  void bc(int32_t offset);
  inline void bc(Label* L) { bc(shifted_branch_offset26(L)); }
  void balc(int32_t offset);
  inline void balc(Label* L) { balc(shifted_branch_offset26(L)); }

  void beq(Register rs, Register rt, int16_t offset);
  inline void beq(Register rs, Register rt, Label* L) {
    beq(rs, rt, shifted_branch_offset(L));
  }
  void bgez(Register rs, int16_t offset);
  void bgezc(Register rt, int16_t offset);
  inline void bgezc(Register rt, Label* L) {
    bgezc(rt, shifted_branch_offset(L));
  }
  void bgeuc(Register rs, Register rt, int16_t offset);
  inline void bgeuc(Register rs, Register rt, Label* L) {
    bgeuc(rs, rt, shifted_branch_offset(L));
  }
  void bgec(Register rs, Register rt, int16_t offset);
  inline void bgec(Register rs, Register rt, Label* L) {
    bgec(rs, rt, shifted_branch_offset(L));
  }
  void bgezal(Register rs, int16_t offset);
  void bgezalc(Register rt, int16_t offset);
  inline void bgezalc(Register rt, Label* L) {
    bgezalc(rt, shifted_branch_offset(L));
  }
  void bgezall(Register rs, int16_t offset);
  inline void bgezall(Register rs, Label* L) {
    bgezall(rs, branch_offset(L) >> 2);
  }
  void bgtz(Register rs, int16_t offset);
  void bgtzc(Register rt, int16_t offset);
  inline void bgtzc(Register rt, Label* L) {
    bgtzc(rt, shifted_branch_offset(L));
  }
  void blez(Register rs, int16_t offset);
  void blezc(Register rt, int16_t offset);
  inline void blezc(Register rt, Label* L) {
    blezc(rt, shifted_branch_offset(L));
  }
  void bltz(Register rs, int16_t offset);
  void bltzc(Register rt, int16_t offset);
  inline void bltzc(Register rt, Label* L) {
    bltzc(rt, shifted_branch_offset(L));
  }
  void bltuc(Register rs, Register rt, int16_t offset);
  inline void bltuc(Register rs, Register rt, Label* L) {
    bltuc(rs, rt, shifted_branch_offset(L));
  }
  void bltc(Register rs, Register rt, int16_t offset);
  inline void bltc(Register rs, Register rt, Label* L) {
    bltc(rs, rt, shifted_branch_offset(L));
  }
  void bltzal(Register rs, int16_t offset);
  void blezalc(Register rt, int16_t offset);
  inline void blezalc(Register rt, Label* L) {
    blezalc(rt, shifted_branch_offset(L));
  }
  void bltzalc(Register rt, int16_t offset);
  inline void bltzalc(Register rt, Label* L) {
    bltzalc(rt, shifted_branch_offset(L));
  }
  void bgtzalc(Register rt, int16_t offset);
  inline void bgtzalc(Register rt, Label* L) {
    bgtzalc(rt, shifted_branch_offset(L));
  }
  void beqzalc(Register rt, int16_t offset);
  inline void beqzalc(Register rt, Label* L) {
    beqzalc(rt, shifted_branch_offset(L));
  }
  void beqc(Register rs, Register rt, int16_t offset);
  inline void beqc(Register rs, Register rt, Label* L) {
    beqc(rs, rt, shifted_branch_offset(L));
  }
  void beqzc(Register rs, int32_t offset);
  inline void beqzc(Register rs, Label* L) {
    beqzc(rs, shifted_branch_offset21(L));
  }
  void bnezalc(Register rt, int16_t offset);
  inline void bnezalc(Register rt, Label* L) {
    bnezalc(rt, shifted_branch_offset(L));
  }
  void bnec(Register rs, Register rt, int16_t offset);
  inline void bnec(Register rs, Register rt, Label* L) {
    bnec(rs, rt, shifted_branch_offset(L));
  }
  void bnezc(Register rt, int32_t offset);
  inline void bnezc(Register rt, Label* L) {
    bnezc(rt, shifted_branch_offset21(L));
  }
  void bne(Register rs, Register rt, int16_t offset);
  inline void bne(Register rs, Register rt, Label* L) {
    bne(rs, rt, shifted_branch_offset(L));
  }
  void bovc(Register rs, Register rt, int16_t offset);
  inline void bovc(Register rs, Register rt, Label* L) {
    bovc(rs, rt, shifted_branch_offset(L));
  }
  void bnvc(Register rs, Register rt, int16_t offset);
  inline void bnvc(Register rs, Register rt, Label* L) {
    bnvc(rs, rt, shifted_branch_offset(L));
  }

  // Never use the int16_t b(l)cond version with a branch offset
  // instead of using the Label* version.

  // Jump targets must be in the current 256 MB-aligned region. i.e. 28 bits.
  void j(int32_t target);
  void jal(int32_t target);
  void jalr(Register rs, Register rd = ra);
  void jr(Register target);
  void jic(Register rt, int16_t offset);
  void jialc(Register rt, int16_t offset);


  // -------Data-processing-instructions---------

  // Arithmetic.
  void addu(Register rd, Register rs, Register rt);
  void subu(Register rd, Register rs, Register rt);
  void mult(Register rs, Register rt);
  void multu(Register rs, Register rt);
  void div(Register rs, Register rt);
  void divu(Register rs, Register rt);
  void div(Register rd, Register rs, Register rt);
  void divu(Register rd, Register rs, Register rt);
  void mod(Register rd, Register rs, Register rt);
  void modu(Register rd, Register rs, Register rt);
  void mul(Register rd, Register rs, Register rt);
  void muh(Register rd, Register rs, Register rt);
  void mulu(Register rd, Register rs, Register rt);
  void muhu(Register rd, Register rs, Register rt);

  void addiu(Register rd, Register rs, int32_t j);

  // Logical.
  void and_(Register rd, Register rs, Register rt);
  void or_(Register rd, Register rs, Register rt);
  void xor_(Register rd, Register rs, Register rt);
  void nor(Register rd, Register rs, Register rt);

  void andi(Register rd, Register rs, int32_t j);
  void ori(Register rd, Register rs, int32_t j);
  void xori(Register rd, Register rs, int32_t j);
  void lui(Register rd, int32_t j);
  void aui(Register rs, Register rt, int32_t j);

  // Shifts.
  // Please note: sll(zero_reg, zero_reg, x) instructions are reserved as nop
  // and may cause problems in normal code. coming_from_nop makes sure this
  // doesn't happen.
  void sll(Register rd, Register rt, uint16_t sa, bool coming_from_nop = false);
  void sllv(Register rd, Register rt, Register rs);
  void srl(Register rd, Register rt, uint16_t sa);
  void srlv(Register rd, Register rt, Register rs);
  void sra(Register rt, Register rd, uint16_t sa);
  void srav(Register rt, Register rd, Register rs);
  void rotr(Register rd, Register rt, uint16_t sa);
  void rotrv(Register rd, Register rt, Register rs);

  // ------------Memory-instructions-------------

  void lb(Register rd, const MemOperand& rs);
  void lbu(Register rd, const MemOperand& rs);
  void lh(Register rd, const MemOperand& rs);
  void lhu(Register rd, const MemOperand& rs);
  void lw(Register rd, const MemOperand& rs);
  void lwl(Register rd, const MemOperand& rs);
  void lwr(Register rd, const MemOperand& rs);
  void sb(Register rd, const MemOperand& rs);
  void sh(Register rd, const MemOperand& rs);
  void sw(Register rd, const MemOperand& rs);
  void swl(Register rd, const MemOperand& rs);
  void swr(Register rd, const MemOperand& rs);


  // ---------PC-Relative-instructions-----------

  void addiupc(Register rs, int32_t imm19);
  void lwpc(Register rs, int32_t offset19);
  void auipc(Register rs, int16_t imm16);
  void aluipc(Register rs, int16_t imm16);


  // ----------------Prefetch--------------------

  void pref(int32_t hint, const MemOperand& rs);


  // -------------Misc-instructions--------------

  // Break / Trap instructions.
  void break_(uint32_t code, bool break_as_stop = false);
  void stop(const char* msg, uint32_t code = kMaxStopCode);
  void tge(Register rs, Register rt, uint16_t code);
  void tgeu(Register rs, Register rt, uint16_t code);
  void tlt(Register rs, Register rt, uint16_t code);
  void tltu(Register rs, Register rt, uint16_t code);
  void teq(Register rs, Register rt, uint16_t code);
  void tne(Register rs, Register rt, uint16_t code);

  // Memory barrier instruction.
  void sync();

  // Move from HI/LO register.
  void mfhi(Register rd);
  void mflo(Register rd);

  // Set on less than.
  void slt(Register rd, Register rs, Register rt);
  void sltu(Register rd, Register rs, Register rt);
  void slti(Register rd, Register rs, int32_t j);
  void sltiu(Register rd, Register rs, int32_t j);

  // Conditional move.
  void movz(Register rd, Register rs, Register rt);
  void movn(Register rd, Register rs, Register rt);
  void movt(Register rd, Register rs, uint16_t cc = 0);
  void movf(Register rd, Register rs, uint16_t cc = 0);

  void sel(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void sel_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void sel_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void seleqz(Register rd, Register rs, Register rt);
  void seleqz(SecondaryField fmt, FPURegister fd, FPURegister fs,
              FPURegister ft);
  void selnez(Register rd, Register rs, Register rt);
  void selnez(SecondaryField fmt, FPURegister fd, FPURegister fs,
              FPURegister ft);
  void seleqz_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void seleqz_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void selnez_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void selnez_s(FPURegister fd, FPURegister fs, FPURegister ft);

  void movz_s(FPURegister fd, FPURegister fs, Register rt);
  void movz_d(FPURegister fd, FPURegister fs, Register rt);
  void movt_s(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movt_d(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movf_s(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movf_d(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movn_s(FPURegister fd, FPURegister fs, Register rt);
  void movn_d(FPURegister fd, FPURegister fs, Register rt);
  // Bit twiddling.
  void clz(Register rd, Register rs);
  void ins_(Register rt, Register rs, uint16_t pos, uint16_t size);
  void ext_(Register rt, Register rs, uint16_t pos, uint16_t size);
  void bitswap(Register rd, Register rt);
  void align(Register rd, Register rs, Register rt, uint8_t bp);

  void wsbh(Register rd, Register rt);
  void seh(Register rd, Register rt);
  void seb(Register rd, Register rt);

  // --------Coprocessor-instructions----------------

  // Load, store, and move.
  void lwc1(FPURegister fd, const MemOperand& src);
  void ldc1(FPURegister fd, const MemOperand& src);

  void swc1(FPURegister fs, const MemOperand& dst);
  void sdc1(FPURegister fs, const MemOperand& dst);

  void mtc1(Register rt, FPURegister fs);
  void mthc1(Register rt, FPURegister fs);

  void mfc1(Register rt, FPURegister fs);
  void mfhc1(Register rt, FPURegister fs);

  void ctc1(Register rt, FPUControlRegister fs);
  void cfc1(Register rt, FPUControlRegister fs);

  // Arithmetic.
  void add_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void add_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void sub_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void sub_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void mul_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void mul_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void madd_s(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void madd_d(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void msub_s(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void msub_d(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void maddf_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void maddf_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void msubf_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void msubf_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void div_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void div_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void abs_s(FPURegister fd, FPURegister fs);
  void abs_d(FPURegister fd, FPURegister fs);
  void mov_d(FPURegister fd, FPURegister fs);
  void mov_s(FPURegister fd, FPURegister fs);
  void neg_s(FPURegister fd, FPURegister fs);
  void neg_d(FPURegister fd, FPURegister fs);
  void sqrt_s(FPURegister fd, FPURegister fs);
  void sqrt_d(FPURegister fd, FPURegister fs);
  void rsqrt_s(FPURegister fd, FPURegister fs);
  void rsqrt_d(FPURegister fd, FPURegister fs);
  void recip_d(FPURegister fd, FPURegister fs);
  void recip_s(FPURegister fd, FPURegister fs);

  // Conversion.
  void cvt_w_s(FPURegister fd, FPURegister fs);
  void cvt_w_d(FPURegister fd, FPURegister fs);
  void trunc_w_s(FPURegister fd, FPURegister fs);
  void trunc_w_d(FPURegister fd, FPURegister fs);
  void round_w_s(FPURegister fd, FPURegister fs);
  void round_w_d(FPURegister fd, FPURegister fs);
  void floor_w_s(FPURegister fd, FPURegister fs);
  void floor_w_d(FPURegister fd, FPURegister fs);
  void ceil_w_s(FPURegister fd, FPURegister fs);
  void ceil_w_d(FPURegister fd, FPURegister fs);
  void rint_s(FPURegister fd, FPURegister fs);
  void rint_d(FPURegister fd, FPURegister fs);
  void rint(SecondaryField fmt, FPURegister fd, FPURegister fs);

  void cvt_l_s(FPURegister fd, FPURegister fs);
  void cvt_l_d(FPURegister fd, FPURegister fs);
  void trunc_l_s(FPURegister fd, FPURegister fs);
  void trunc_l_d(FPURegister fd, FPURegister fs);
  void round_l_s(FPURegister fd, FPURegister fs);
  void round_l_d(FPURegister fd, FPURegister fs);
  void floor_l_s(FPURegister fd, FPURegister fs);
  void floor_l_d(FPURegister fd, FPURegister fs);
  void ceil_l_s(FPURegister fd, FPURegister fs);
  void ceil_l_d(FPURegister fd, FPURegister fs);

  void class_s(FPURegister fd, FPURegister fs);
  void class_d(FPURegister fd, FPURegister fs);

  void min(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void mina(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void max(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void maxa(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void min_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void min_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void max_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void max_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void mina_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void mina_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void maxa_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void maxa_d(FPURegister fd, FPURegister fs, FPURegister ft);

  void cvt_s_w(FPURegister fd, FPURegister fs);
  void cvt_s_l(FPURegister fd, FPURegister fs);
  void cvt_s_d(FPURegister fd, FPURegister fs);

  void cvt_d_w(FPURegister fd, FPURegister fs);
  void cvt_d_l(FPURegister fd, FPURegister fs);
  void cvt_d_s(FPURegister fd, FPURegister fs);

  // Conditions and branches for MIPSr6.
  void cmp(FPUCondition cond, SecondaryField fmt,
         FPURegister fd, FPURegister ft, FPURegister fs);
  void cmp_s(FPUCondition cond, FPURegister fd, FPURegister fs, FPURegister ft);
  void cmp_d(FPUCondition cond, FPURegister fd, FPURegister fs, FPURegister ft);

  void bc1eqz(int16_t offset, FPURegister ft);
  inline void bc1eqz(Label* L, FPURegister ft) {
    bc1eqz(shifted_branch_offset(L), ft);
  }
  void bc1nez(int16_t offset, FPURegister ft);
  inline void bc1nez(Label* L, FPURegister ft) {
    bc1nez(shifted_branch_offset(L), ft);
  }

  // Conditions and branches for non MIPSr6.
  void c(FPUCondition cond, SecondaryField fmt,
         FPURegister ft, FPURegister fs, uint16_t cc = 0);
  void c_s(FPUCondition cond, FPURegister ft, FPURegister fs, uint16_t cc = 0);
  void c_d(FPUCondition cond, FPURegister ft, FPURegister fs, uint16_t cc = 0);

  void bc1f(int16_t offset, uint16_t cc = 0);
  inline void bc1f(Label* L, uint16_t cc = 0) {
    bc1f(shifted_branch_offset(L), cc);
  }
  void bc1t(int16_t offset, uint16_t cc = 0);
  inline void bc1t(Label* L, uint16_t cc = 0) {
    bc1t(shifted_branch_offset(L), cc);
  }
  void fcmp(FPURegister src1, const double src2, FPUCondition cond);

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Check the number of instructions generated from label to here.
  int InstructionsGeneratedSince(Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Class for scoping postponing the trampoline pool generation.
  class BlockTrampolinePoolScope {
   public:
    explicit BlockTrampolinePoolScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }
    ~BlockTrampolinePoolScope() {
      assem_->EndBlockTrampolinePool();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockTrampolinePoolScope);
  };

  // Class for postponing the assembly buffer growth. Typically used for
  // sequences of instructions that must be emitted as a unit, before
  // buffer growth (and relocation) can occur.
  // This blocking scope is not nestable.
  class BlockGrowBufferScope {
   public:
    explicit BlockGrowBufferScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockGrowBuffer();
    }
    ~BlockGrowBufferScope() {
      assem_->EndBlockGrowBuffer();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockGrowBufferScope);
  };

  // Debugging.

  // Mark generator continuation.
  void RecordGeneratorContinuation();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot(RelocInfo::Mode mode);

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

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, int raw_position, int id);

  static int RelocateInternalReference(RelocInfo::Mode rmode, byte* pc,
                                       intptr_t pc_delta);

  // Writes a single byte or word of data in the code stream.  Used for
  // inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data) { dd(data); }
  void dd(Label* label);

  // Postpone the generation of the trampoline pool for the specified number of
  // instructions.
  void BlockTrampolinePoolFor(int instructions);

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool overflow() const { return pc_ >= reloc_info_writer.pos() - kGap; }

  // Get the number of bytes available in the buffer.
  inline int available_space() const { return reloc_info_writer.pos() - pc_; }

  // Read/patch instructions.
  static Instr instr_at(byte* pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(byte* pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }

  // Check if an instruction is a branch of some kind.
  static bool IsBranch(Instr instr);
  static bool IsBc(Instr instr);
  static bool IsBzc(Instr instr);
  static bool IsBeq(Instr instr);
  static bool IsBne(Instr instr);
  static bool IsBeqzc(Instr instr);
  static bool IsBnezc(Instr instr);
  static bool IsBeqc(Instr instr);
  static bool IsBnec(Instr instr);
  static bool IsJicOrJialc(Instr instr);

  static bool IsJump(Instr instr);
  static bool IsJ(Instr instr);
  static bool IsLui(Instr instr);
  static bool IsOri(Instr instr);

  static bool IsJal(Instr instr);
  static bool IsJr(Instr instr);
  static bool IsJalr(Instr instr);

  static bool IsNop(Instr instr, unsigned int type);
  static bool IsPop(Instr instr);
  static bool IsPush(Instr instr);
  static bool IsLwRegFpOffset(Instr instr);
  static bool IsSwRegFpOffset(Instr instr);
  static bool IsLwRegFpNegOffset(Instr instr);
  static bool IsSwRegFpNegOffset(Instr instr);

  static Register GetRtReg(Instr instr);
  static Register GetRsReg(Instr instr);
  static Register GetRdReg(Instr instr);

  static uint32_t GetRt(Instr instr);
  static uint32_t GetRtField(Instr instr);
  static uint32_t GetRs(Instr instr);
  static uint32_t GetRsField(Instr instr);
  static uint32_t GetRd(Instr instr);
  static uint32_t GetRdField(Instr instr);
  static uint32_t GetSa(Instr instr);
  static uint32_t GetSaField(Instr instr);
  static uint32_t GetOpcodeField(Instr instr);
  static uint32_t GetFunction(Instr instr);
  static uint32_t GetFunctionField(Instr instr);
  static uint32_t GetImmediate16(Instr instr);
  static uint32_t GetLabelConst(Instr instr);

  static int32_t GetBranchOffset(Instr instr);
  static bool IsLw(Instr instr);
  static int16_t GetLwOffset(Instr instr);
  static int16_t GetJicOrJialcOffset(Instr instr);
  static int16_t GetLuiOffset(Instr instr);
  static Instr SetLwOffset(Instr instr, int16_t offset);

  static bool IsSw(Instr instr);
  static Instr SetSwOffset(Instr instr, int16_t offset);
  static bool IsAddImmediate(Instr instr);
  static Instr SetAddImmediateOffset(Instr instr, int16_t offset);
  static uint32_t CreateTargetAddress(Instr instr_lui, Instr instr_jic);
  static void UnpackTargetAddress(uint32_t address, int16_t& lui_offset,
                                  int16_t& jic_offset);
  static void UnpackTargetAddressUnsigned(uint32_t address,
                                          uint32_t& lui_offset,
                                          uint32_t& jic_offset);

  static bool IsAndImmediate(Instr instr);
  static bool IsEmittedConstant(Instr instr);

  void CheckTrampolinePool();

  void PatchConstantPoolAccessInstruction(int pc_offset, int offset,
                                          ConstantPoolEntry::Access access,
                                          ConstantPoolEntry::Type type) {
    // No embedded constant pool support.
    UNREACHABLE();
  }

  bool IsPrevInstrCompactBranch() { return prev_instr_compact_branch_; }

  inline int UnboundLabelsCount() { return unbound_labels_count_; }

 protected:
  // Load Scaled Address instruction.
  void lsa(Register rd, Register rt, Register rs, uint8_t sa);

  // Helpers.
  void LoadRegPlusOffsetToAt(const MemOperand& src);

  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  TypeFeedbackId recorded_ast_id_;

  int32_t buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos.
  int target_at(int pos, bool is_internal);

  // Patch branch instruction at pos to branch to given branch target pos.
  void target_at_put(int pos, int target_pos, bool is_internal);

  // Say if we need to relocate with this mode.
  bool MustUseReg(RelocInfo::Mode rmode);

  // Record reloc info for current pc_.
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(int pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void StartBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_++;
  }

  void EndBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_--;
  }

  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  bool has_exception() const {
    return internal_trampoline_exception_;
  }

  void DoubleAsTwoUInt32(double d, uint32_t* lo, uint32_t* hi);

  bool is_trampoline_emitted() const {
    return trampoline_emitted_;
  }

  // Temporarily block automatic assembly buffer growth.
  void StartBlockGrowBuffer() {
    DCHECK(!block_buffer_growth_);
    block_buffer_growth_ = true;
  }

  void EndBlockGrowBuffer() {
    DCHECK(block_buffer_growth_);
    block_buffer_growth_ = false;
  }

  bool is_buffer_growth_blocked() const {
    return block_buffer_growth_;
  }

  void EmitForbiddenSlotInstruction() {
    if (IsPrevInstrCompactBranch()) {
      nop();
    }
  }

  inline void CheckTrampolinePoolQuick(int extra_instructions = 0);

  inline void CheckBuffer();

 private:
  inline static void set_target_internal_reference_encoded_at(Address pc,
                                                              Address target);

  // Buffer size and constant pool distance are checked together at regular
  // intervals of kBufferCheckInterval emitted bytes.
  static const int kBufferCheckInterval = 1*KB/2;

  // Code generation.
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;


  // Repeated checking whether the trampoline pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated.
  static const int kCheckConstIntervalInst = 32;
  static const int kCheckConstInterval = kCheckConstIntervalInst * kInstrSize;

  int next_buffer_check_;  // pc offset of next buffer check.

  // Emission of the trampoline pool may be blocked in some code sequences.
  int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_trampoline_pool_before_;  // Block emission before this pc offset.

  // Keep track of the last emitted pool to guarantee a maximal distance.
  int last_trampoline_pool_end_;  // pc offset of the end of the last pool.

  // Automatic growth of the assembly buffer may be blocked for some sequences.
  bool block_buffer_growth_;  // Block growth when true.

  // Relocation information generation.
  // Each relocation is encoded as a variable size value.
  static const int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Readable constants for compact branch handling in emit()
  enum class CompactBranchType : bool { NO = false, COMPACT_BRANCH = true };

  // Code emission.
  void GrowBuffer();
  inline void emit(Instr x,
                   CompactBranchType is_compact_branch = CompactBranchType::NO);
  inline void emit(uint64_t x);
  inline void CheckForEmitInForbiddenSlot();
  template <typename T>
  inline void EmitHelper(T x);
  inline void EmitHelper(Instr x, CompactBranchType is_compact_branch);

  // Instruction generation.
  // We have 3 different kind of encoding layout on MIPS.
  // However due to many different types of objects encoded in the same fields
  // we have quite a few aliases for each mode.
  // Using the same structure to refer to Register and FPURegister would spare a
  // few aliases, but mixing both does not look clean to me.
  // Anyway we could surely implement this differently.

  void GenInstrRegister(Opcode opcode,
                        Register rs,
                        Register rt,
                        Register rd,
                        uint16_t sa = 0,
                        SecondaryField func = NULLSF);

  void GenInstrRegister(Opcode opcode,
                        Register rs,
                        Register rt,
                        uint16_t msb,
                        uint16_t lsb,
                        SecondaryField func);

  void GenInstrRegister(Opcode opcode,
                        SecondaryField fmt,
                        FPURegister ft,
                        FPURegister fs,
                        FPURegister fd,
                        SecondaryField func = NULLSF);

  void GenInstrRegister(Opcode opcode,
                        FPURegister fr,
                        FPURegister ft,
                        FPURegister fs,
                        FPURegister fd,
                        SecondaryField func = NULLSF);

  void GenInstrRegister(Opcode opcode,
                        SecondaryField fmt,
                        Register rt,
                        FPURegister fs,
                        FPURegister fd,
                        SecondaryField func = NULLSF);

  void GenInstrRegister(Opcode opcode,
                        SecondaryField fmt,
                        Register rt,
                        FPUControlRegister fs,
                        SecondaryField func = NULLSF);

  void GenInstrImmediate(
      Opcode opcode, Register rs, Register rt, int32_t j,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(
      Opcode opcode, Register rs, SecondaryField SF, int32_t j,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(
      Opcode opcode, Register r1, FPURegister r2, int32_t j,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(
      Opcode opcode, Register rs, int32_t offset21,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(Opcode opcode, Register rs, uint32_t offset21);
  void GenInstrImmediate(
      Opcode opcode, int32_t offset26,
      CompactBranchType is_compact_branch = CompactBranchType::NO);


  void GenInstrJump(Opcode opcode,
                     uint32_t address);


  // Labels.
  void print(Label* L);
  void bind_to(Label* L, int pos);
  void next(Label* L, bool is_internal);

  // One trampoline consists of:
  // - space for trampoline slots,
  // - space for labels.
  //
  // Space for trampoline slots is equal to slot_count * 2 * kInstrSize.
  // Space for trampoline slots preceeds space for labels. Each label is of one
  // instruction size, so total amount for labels is equal to
  // label_count *  kInstrSize.
  class Trampoline {
   public:
    Trampoline() {
      start_ = 0;
      next_slot_ = 0;
      free_slot_count_ = 0;
      end_ = 0;
    }
    Trampoline(int start, int slot_count) {
      start_ = start;
      next_slot_ = start;
      free_slot_count_ = slot_count;
      end_ = start + slot_count * kTrampolineSlotsSize;
    }
    int start() {
      return start_;
    }
    int end() {
      return end_;
    }
    int take_slot() {
      int trampoline_slot = kInvalidSlotPos;
      if (free_slot_count_ <= 0) {
        // We have run out of space on trampolines.
        // Make sure we fail in debug mode, so we become aware of each case
        // when this happens.
        DCHECK(0);
        // Internal exception will be caught.
      } else {
        trampoline_slot = next_slot_;
        free_slot_count_--;
        next_slot_ += kTrampolineSlotsSize;
      }
      return trampoline_slot;
    }

   private:
    int start_;
    int end_;
    int next_slot_;
    int free_slot_count_;
  };

  int32_t get_trampoline_entry(int32_t pos);
  int unbound_labels_count_;
  // If trampoline is emitted, generated code is becoming large. As this is
  // already a slow case which can possibly break our code generation for the
  // extreme case, we use this information to trigger different mode of
  // branch instruction generation, where we use jump instructions rather
  // than regular branch instructions.
  bool trampoline_emitted_;
#ifdef _MIPS_ARCH_MIPS32R6
  static const int kTrampolineSlotsSize = 2 * kInstrSize;
#else
  static const int kTrampolineSlotsSize = 4 * kInstrSize;
#endif
  static const int kMaxBranchOffset = (1 << (18 - 1)) - 1;
  static const int kMaxCompactBranchOffset = (1 << (28 - 1)) - 1;
  static const int kInvalidSlotPos = -1;

  // Internal reference positions, required for unbounded internal reference
  // labels.
  std::set<int> internal_reference_positions_;

  void EmittedCompactBranchInstruction() { prev_instr_compact_branch_ = true; }
  void ClearCompactBranchState() { prev_instr_compact_branch_ = false; }
  bool prev_instr_compact_branch_ = false;

  Trampoline trampoline_;
  bool internal_trampoline_exception_;

  friend class RegExpMacroAssemblerMIPS;
  friend class RelocInfo;
  friend class CodePatcher;
  friend class BlockTrampolinePoolScope;
  friend class EnsureSpace;
};


class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) {
    assembler->CheckBuffer();
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM_ASSEMBLER_MIPS_H_

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

// A lightweight X64 Assembler.

#ifndef V8_X64_ASSEMBLER_X64_H_
#define V8_X64_ASSEMBLER_X64_H_

#include <deque>

#include "src/assembler.h"

namespace v8 {
namespace internal {

// Utility functions

#define GENERAL_REGISTERS(V) \
  V(rax)                     \
  V(rcx)                     \
  V(rdx)                     \
  V(rbx)                     \
  V(rsp)                     \
  V(rbp)                     \
  V(rsi)                     \
  V(rdi)                     \
  V(r8)                      \
  V(r9)                      \
  V(r10)                     \
  V(r11)                     \
  V(r12)                     \
  V(r13)                     \
  V(r14)                     \
  V(r15)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(rax)                                 \
  V(rbx)                                 \
  V(rdx)                                 \
  V(rcx)                                 \
  V(rsi)                                 \
  V(rdi)                                 \
  V(r8)                                  \
  V(r9)                                  \
  V(r11)                                 \
  V(r12)                                 \
  V(r14)                                 \
  V(r15)


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
struct Register {
  enum Code {
#define REGISTER_CODE(R) kCode_##R,
    GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
        kAfterLast,
    kCode_no_reg = -1
  };

  static const int kNumRegisters = Code::kAfterLast;

  static Register from_code(int code) {
    DCHECK(code >= 0);
    DCHECK(code < kNumRegisters);
    Register r = {code};
    return r;
  }
  const char* ToString();
  bool IsAllocatable() const;
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

  bool is_byte_register() const { return reg_code <= 3; }
  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const { return reg_code >> 3; }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const { return reg_code & 0x7; }

  // Unfortunately we can't make this private in a struct when initializing
  // by assignment.
  int reg_code;
};


#define DECLARE_REGISTER(R) const Register R = {Register::kCode_##R};
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
const Register no_reg = {Register::kCode_no_reg};


#ifdef _WIN64
  // Windows calling convention
const Register arg_reg_1 = {Register::kCode_rcx};
const Register arg_reg_2 = {Register::kCode_rdx};
const Register arg_reg_3 = {Register::kCode_r8};
const Register arg_reg_4 = {Register::kCode_r9};
#else
  // AMD64 calling convention
const Register arg_reg_1 = {Register::kCode_rdi};
const Register arg_reg_2 = {Register::kCode_rsi};
const Register arg_reg_3 = {Register::kCode_rdx};
const Register arg_reg_4 = {Register::kCode_rcx};
#endif  // _WIN64


#define DOUBLE_REGISTERS(V) \
  V(xmm0)                   \
  V(xmm1)                   \
  V(xmm2)                   \
  V(xmm3)                   \
  V(xmm4)                   \
  V(xmm5)                   \
  V(xmm6)                   \
  V(xmm7)                   \
  V(xmm8)                   \
  V(xmm9)                   \
  V(xmm10)                  \
  V(xmm11)                  \
  V(xmm12)                  \
  V(xmm13)                  \
  V(xmm14)                  \
  V(xmm15)

#define ALLOCATABLE_DOUBLE_REGISTERS(V) \
  V(xmm1)                               \
  V(xmm2)                               \
  V(xmm3)                               \
  V(xmm4)                               \
  V(xmm5)                               \
  V(xmm6)                               \
  V(xmm7)                               \
  V(xmm8)                               \
  V(xmm9)                               \
  V(xmm10)                              \
  V(xmm11)                              \
  V(xmm12)                              \
  V(xmm13)                              \
  V(xmm14)                              \
  V(xmm15)


struct DoubleRegister {
  enum Code {
#define REGISTER_CODE(R) kCode_##R,
    DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
        kAfterLast,
    kCode_no_reg = -1
  };

  static const int kMaxNumRegisters = Code::kAfterLast;

  static DoubleRegister from_code(int code) {
    DoubleRegister result = {code};
    return result;
  }

  const char* ToString();
  bool IsAllocatable() const;
  bool is_valid() const { return 0 <= reg_code && reg_code < kMaxNumRegisters; }
  bool is(DoubleRegister reg) const { return reg_code == reg.reg_code; }
  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }

  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const { return reg_code >> 3; }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const { return reg_code & 0x7; }

  // Unfortunately we can't make this private in a struct when initializing
  // by assignment.
  int reg_code;
};


#define DECLARE_REGISTER(R) \
  const DoubleRegister R = {DoubleRegister::kCode_##R};
DOUBLE_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
const DoubleRegister no_double_reg = {DoubleRegister::kCode_no_reg};


typedef DoubleRegister XMMRegister;

typedef DoubleRegister Simd128Register;

enum Condition {
  // any value < 0 is considered no_condition
  no_condition  = -1,

  overflow      =  0,
  no_overflow   =  1,
  below         =  2,
  above_equal   =  3,
  equal         =  4,
  not_equal     =  5,
  below_equal   =  6,
  above         =  7,
  negative      =  8,
  positive      =  9,
  parity_even   = 10,
  parity_odd    = 11,
  less          = 12,
  greater_equal = 13,
  less_equal    = 14,
  greater       = 15,

  // Fake conditions that are handled by the
  // opcodes using them.
  always        = 16,
  never         = 17,
  // aliases
  carry         = below,
  not_carry     = above_equal,
  zero          = equal,
  not_zero      = not_equal,
  sign          = negative,
  not_sign      = positive,
  last_condition = greater
};


// Returns the equivalent of !cc.
// Negation of the default no_condition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc) {
  return static_cast<Condition>(cc ^ 1);
}


// Commute a condition such that {a cond b == b cond' a}.
inline Condition CommuteCondition(Condition cc) {
  switch (cc) {
    case below:
      return above;
    case above:
      return below;
    case above_equal:
      return below_equal;
    case below_equal:
      return above_equal;
    case less:
      return greater;
    case greater:
      return less;
    case greater_equal:
      return less_equal;
    case less_equal:
      return greater_equal;
    default:
      return cc;
  }
}


enum RoundingMode {
  kRoundToNearest = 0x0,
  kRoundDown = 0x1,
  kRoundUp = 0x2,
  kRoundToZero = 0x3
};


// -----------------------------------------------------------------------------
// Machine instruction Immediates

class Immediate BASE_EMBEDDED {
 public:
  explicit Immediate(int32_t value) : value_(value) {}
  explicit Immediate(Smi* value) {
    DCHECK(SmiValuesAre31Bits());  // Only available for 31-bit SMI.
    value_ = static_cast<int32_t>(reinterpret_cast<intptr_t>(value));
  }

 private:
  int32_t value_;

  friend class Assembler;
};


// -----------------------------------------------------------------------------
// Machine instruction Operands

enum ScaleFactor {
  times_1 = 0,
  times_2 = 1,
  times_4 = 2,
  times_8 = 3,
  times_int_size = times_4,
  times_pointer_size = (kPointerSize == 8) ? times_8 : times_4
};


class Operand BASE_EMBEDDED {
 public:
  // [base + disp/r]
  Operand(Register base, int32_t disp);

  // [base + index*scale + disp/r]
  Operand(Register base,
          Register index,
          ScaleFactor scale,
          int32_t disp);

  // [index*scale + disp/r]
  Operand(Register index,
          ScaleFactor scale,
          int32_t disp);

  // Offset from existing memory operand.
  // Offset is added to existing displacement as 32-bit signed values and
  // this must not overflow.
  Operand(const Operand& base, int32_t offset);

  // [rip + disp/r]
  explicit Operand(Label* label);

  // Checks whether either base or index register is the given register.
  // Does not check the "reg" part of the Operand.
  bool AddressUsesRegister(Register reg) const;

  // Queries related to the size of the generated instruction.
  // Whether the generated instruction will have a REX prefix.
  bool requires_rex() const { return rex_ != 0; }
  // Size of the ModR/M, SIB and displacement parts of the generated
  // instruction.
  int operand_size() const { return len_; }

 private:
  byte rex_;
  byte buf_[9];
  // The number of bytes of buf_ in use.
  byte len_;

  // Set the ModR/M byte without an encoded 'reg' register. The
  // register is encoded later as part of the emit_operand operation.
  // set_modrm can be called before or after set_sib and set_disp*.
  inline void set_modrm(int mod, Register rm);

  // Set the SIB byte if one is needed. Sets the length to 2 rather than 1.
  inline void set_sib(ScaleFactor scale, Register index, Register base);

  // Adds operand displacement fields (offsets added to the memory address).
  // Needs to be called after set_sib, not before it.
  inline void set_disp8(int disp);
  inline void set_disp32(int disp);
  inline void set_disp64(int64_t disp);  // for labels.

  friend class Assembler;
};


#define ASSEMBLER_INSTRUCTION_LIST(V) \
  V(add)                              \
  V(and)                              \
  V(cmp)                              \
  V(dec)                              \
  V(idiv)                             \
  V(div)                              \
  V(imul)                             \
  V(inc)                              \
  V(lea)                              \
  V(mov)                              \
  V(movzxb)                           \
  V(movzxw)                           \
  V(neg)                              \
  V(not)                              \
  V(or)                               \
  V(repmovs)                          \
  V(sbb)                              \
  V(sub)                              \
  V(test)                             \
  V(xchg)                             \
  V(xor)


// Shift instructions on operands/registers with kPointerSize, kInt32Size and
// kInt64Size.
#define SHIFT_INSTRUCTION_LIST(V)       \
  V(rol, 0x0)                           \
  V(ror, 0x1)                           \
  V(rcl, 0x2)                           \
  V(rcr, 0x3)                           \
  V(shl, 0x4)                           \
  V(shr, 0x5)                           \
  V(sar, 0x7)                           \


class Assembler : public AssemblerBase {
 private:
  // We check before assembling an instruction that there is sufficient
  // space to write an instruction and its relocation information.
  // The relocation writer's position must be kGap bytes above the end of
  // the generated instructions. This leaves enough space for the
  // longest possible x64 instruction, 15 bytes, and the longest possible
  // relocation information encoding, RelocInfoWriter::kMaxLength == 16.
  // (There is a 15 byte limit on x64 instruction length that rules out some
  // otherwise valid instructions.)
  // This allows for a single, fast space check per instruction.
  static const int kGap = 32;

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

  // Read/Modify the code target in the relative branch/call instruction at pc.
  // On the x64 architecture, we use relative jumps with a 32-bit displacement
  // to jump to other Code objects in the Code space in the heap.
  // Jumps to C functions are done indirectly through a 64-bit register holding
  // the absolute address of the target.
  // These functions convert between absolute Addresses of Code objects and
  // the relative displacements stored in the code.
  static inline Address target_address_at(Address pc, Address constant_pool);
  static inline void set_target_address_at(
      Isolate* isolate, Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  static inline Address target_address_at(Address pc, Code* code) {
    Address constant_pool = code ? code->constant_pool() : NULL;
    return target_address_at(pc, constant_pool);
  }
  static inline void set_target_address_at(
      Isolate* isolate, Address pc, Code* code, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED) {
    Address constant_pool = code ? code->constant_pool() : NULL;
    set_target_address_at(isolate, pc, constant_pool, target,
                          icache_flush_mode);
  }

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  static inline Address target_address_from_return_address(Address pc);

  // This sets the branch destination (which is in the instruction on x64).
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Isolate* isolate, Address instruction_payload, Code* code,
      Address target) {
    set_target_address_at(isolate, instruction_payload, code, target);
  }

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Isolate* isolate, Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  static inline RelocInfo::Mode RelocInfoNone() {
    if (kPointerSize == kInt64Size) {
      return RelocInfo::NONE64;
    } else {
      DCHECK(kPointerSize == kInt32Size);
      return RelocInfo::NONE32;
    }
  }

  inline Handle<Object> code_target_object_handle_at(Address pc);
  inline Address runtime_entry_at(Address pc);
  // Number of bytes taken up by the branch target in the code.
  static const int kSpecialTargetSize = 4;  // Use 32-bit displacement.
  // Distance between the address of the code target in the call instruction
  // and the return address pushed on the stack.
  static const int kCallTargetAddressOffset = 4;  // Use 32-bit displacement.
  // The length of call(kScratchRegister).
  static const int kCallScratchRegisterInstructionLength = 3;
  // The length of call(Immediate32).
  static const int kShortCallInstructionLength = 5;
  // The length of movq(kScratchRegister, address).
  static const int kMoveAddressIntoScratchRegisterInstructionLength =
      2 + kPointerSize;
  // The length of movq(kScratchRegister, address) and call(kScratchRegister).
  static const int kCallSequenceLength =
      kMoveAddressIntoScratchRegisterInstructionLength +
      kCallScratchRegisterInstructionLength;

  // The debug break slot must be able to contain an indirect call sequence.
  static const int kDebugBreakSlotLength = kCallSequenceLength;
  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  static const int kPatchDebugBreakSlotAddressOffset =
      kMoveAddressIntoScratchRegisterInstructionLength - kPointerSize;

  // One byte opcode for test eax,0xXXXXXXXX.
  static const byte kTestEaxByte = 0xA9;
  // One byte opcode for test al, 0xXX.
  static const byte kTestAlByte = 0xA8;
  // One byte opcode for nop.
  static const byte kNopByte = 0x90;

  // One byte prefix for a short conditional jump.
  static const byte kJccShortPrefix = 0x70;
  static const byte kJncShortOpcode = kJccShortPrefix | not_carry;
  static const byte kJcShortOpcode = kJccShortPrefix | carry;
  static const byte kJnzShortOpcode = kJccShortPrefix | not_zero;
  static const byte kJzShortOpcode = kJccShortPrefix | zero;

  // VEX prefix encodings.
  enum SIMDPrefix { kNone = 0x0, k66 = 0x1, kF3 = 0x2, kF2 = 0x3 };
  enum VectorLength { kL128 = 0x0, kL256 = 0x4, kLIG = kL128, kLZ = kL128 };
  enum VexW { kW0 = 0x0, kW1 = 0x80, kWIG = kW0 };
  enum LeadingOpcode { k0F = 0x1, k0F38 = 0x2, k0F3A = 0x3 };

  // ---------------------------------------------------------------------------
  // Code generation
  //
  // Function names correspond one-to-one to x64 instruction mnemonics.
  // Unless specified otherwise, instructions operate on 64-bit operands.
  //
  // If we need versions of an assembly instruction that operate on different
  // width arguments, we add a single-letter suffix specifying the width.
  // This is done for the following instructions: mov, cmp, inc, dec,
  // add, sub, and test.
  // There are no versions of these instructions without the suffix.
  // - Instructions on 8-bit (byte) operands/registers have a trailing 'b'.
  // - Instructions on 16-bit (word) operands/registers have a trailing 'w'.
  // - Instructions on 32-bit (doubleword) operands/registers use 'l'.
  // - Instructions on 64-bit (quadword) operands/registers use 'q'.
  // - Instructions on operands/registers with pointer size use 'p'.

  STATIC_ASSERT(kPointerSize == kInt64Size || kPointerSize == kInt32Size);

#define DECLARE_INSTRUCTION(instruction)                \
  template<class P1>                                    \
  void instruction##p(P1 p1) {                          \
    emit_##instruction(p1, kPointerSize);               \
  }                                                     \
                                                        \
  template<class P1>                                    \
  void instruction##l(P1 p1) {                          \
    emit_##instruction(p1, kInt32Size);                 \
  }                                                     \
                                                        \
  template<class P1>                                    \
  void instruction##q(P1 p1) {                          \
    emit_##instruction(p1, kInt64Size);                 \
  }                                                     \
                                                        \
  template<class P1, class P2>                          \
  void instruction##p(P1 p1, P2 p2) {                   \
    emit_##instruction(p1, p2, kPointerSize);           \
  }                                                     \
                                                        \
  template<class P1, class P2>                          \
  void instruction##l(P1 p1, P2 p2) {                   \
    emit_##instruction(p1, p2, kInt32Size);             \
  }                                                     \
                                                        \
  template<class P1, class P2>                          \
  void instruction##q(P1 p1, P2 p2) {                   \
    emit_##instruction(p1, p2, kInt64Size);             \
  }                                                     \
                                                        \
  template<class P1, class P2, class P3>                \
  void instruction##p(P1 p1, P2 p2, P3 p3) {            \
    emit_##instruction(p1, p2, p3, kPointerSize);       \
  }                                                     \
                                                        \
  template<class P1, class P2, class P3>                \
  void instruction##l(P1 p1, P2 p2, P3 p3) {            \
    emit_##instruction(p1, p2, p3, kInt32Size);         \
  }                                                     \
                                                        \
  template<class P1, class P2, class P3>                \
  void instruction##q(P1 p1, P2 p2, P3 p3) {            \
    emit_##instruction(p1, p2, p3, kInt64Size);         \
  }
  ASSEMBLER_INSTRUCTION_LIST(DECLARE_INSTRUCTION)
#undef DECLARE_INSTRUCTION

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m, where m must be a power of 2.
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  void Nop(int bytes = 1);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Stack
  void pushfq();
  void popfq();

  void pushq(Immediate value);
  // Push a 32 bit integer, and guarantee that it is actually pushed as a
  // 32 bit value, the normal push will optimize the 8 bit case.
  void pushq_imm32(int32_t imm32);
  void pushq(Register src);
  void pushq(const Operand& src);

  void popq(Register dst);
  void popq(const Operand& dst);

  void enter(Immediate size);
  void leave();

  // Moves
  void movb(Register dst, const Operand& src);
  void movb(Register dst, Immediate imm);
  void movb(const Operand& dst, Register src);
  void movb(const Operand& dst, Immediate imm);

  // Move the low 16 bits of a 64-bit register value to a 16-bit
  // memory location.
  void movw(Register dst, const Operand& src);
  void movw(const Operand& dst, Register src);
  void movw(const Operand& dst, Immediate imm);

  // Move the offset of the label location relative to the current
  // position (after the move) to the destination.
  void movl(const Operand& dst, Label* src);

  // Loads a pointer into a register with a relocation mode.
  void movp(Register dst, void* ptr, RelocInfo::Mode rmode);

  // Loads a 64-bit immediate into a register.
  void movq(Register dst, int64_t value);
  void movq(Register dst, uint64_t value);

  void movsxbl(Register dst, Register src);
  void movsxbl(Register dst, const Operand& src);
  void movsxbq(Register dst, const Operand& src);
  void movsxwl(Register dst, Register src);
  void movsxwl(Register dst, const Operand& src);
  void movsxwq(Register dst, const Operand& src);
  void movsxlq(Register dst, Register src);
  void movsxlq(Register dst, const Operand& src);

  // Repeated moves.

  void repmovsb();
  void repmovsw();
  void repmovsp() { emit_repmovs(kPointerSize); }
  void repmovsl() { emit_repmovs(kInt32Size); }
  void repmovsq() { emit_repmovs(kInt64Size); }

  // Instruction to load from an immediate 64-bit pointer into RAX.
  void load_rax(void* ptr, RelocInfo::Mode rmode);
  void load_rax(ExternalReference ext);

  // Conditional moves.
  void cmovq(Condition cc, Register dst, Register src);
  void cmovq(Condition cc, Register dst, const Operand& src);
  void cmovl(Condition cc, Register dst, Register src);
  void cmovl(Condition cc, Register dst, const Operand& src);

  void cmpb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x7, dst, src);
  }

  void cmpb_al(Immediate src);

  void cmpb(Register dst, Register src) {
    arithmetic_op_8(0x3A, dst, src);
  }

  void cmpb(Register dst, const Operand& src) {
    arithmetic_op_8(0x3A, dst, src);
  }

  void cmpb(const Operand& dst, Register src) {
    arithmetic_op_8(0x38, src, dst);
  }

  void cmpb(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_8(0x7, dst, src);
  }

  void cmpw(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_16(0x7, dst, src);
  }

  void cmpw(Register dst, Immediate src) {
    immediate_arithmetic_op_16(0x7, dst, src);
  }

  void cmpw(Register dst, const Operand& src) {
    arithmetic_op_16(0x3B, dst, src);
  }

  void cmpw(Register dst, Register src) {
    arithmetic_op_16(0x3B, dst, src);
  }

  void cmpw(const Operand& dst, Register src) {
    arithmetic_op_16(0x39, src, dst);
  }

  void andb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x4, dst, src);
  }

  void decb(Register dst);
  void decb(const Operand& dst);

  // Sign-extends rax into rdx:rax.
  void cqo();
  // Sign-extends eax into edx:eax.
  void cdq();

  // Multiply eax by src, put the result in edx:eax.
  void mull(Register src);
  void mull(const Operand& src);
  // Multiply rax by src, put the result in rdx:rax.
  void mulq(Register src);

#define DECLARE_SHIFT_INSTRUCTION(instruction, subcode)                       \
  void instruction##p(Register dst, Immediate imm8) {                         \
    shift(dst, imm8, subcode, kPointerSize);                                  \
  }                                                                           \
                                                                              \
  void instruction##l(Register dst, Immediate imm8) {                         \
    shift(dst, imm8, subcode, kInt32Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##q(Register dst, Immediate imm8) {                         \
    shift(dst, imm8, subcode, kInt64Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##p(Operand dst, Immediate imm8) {                          \
    shift(dst, imm8, subcode, kPointerSize);                                  \
  }                                                                           \
                                                                              \
  void instruction##l(Operand dst, Immediate imm8) {                          \
    shift(dst, imm8, subcode, kInt32Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##q(Operand dst, Immediate imm8) {                          \
    shift(dst, imm8, subcode, kInt64Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##p_cl(Register dst) { shift(dst, subcode, kPointerSize); } \
                                                                              \
  void instruction##l_cl(Register dst) { shift(dst, subcode, kInt32Size); }   \
                                                                              \
  void instruction##q_cl(Register dst) { shift(dst, subcode, kInt64Size); }   \
                                                                              \
  void instruction##p_cl(Operand dst) { shift(dst, subcode, kPointerSize); }  \
                                                                              \
  void instruction##l_cl(Operand dst) { shift(dst, subcode, kInt32Size); }    \
                                                                              \
  void instruction##q_cl(Operand dst) { shift(dst, subcode, kInt64Size); }
  SHIFT_INSTRUCTION_LIST(DECLARE_SHIFT_INSTRUCTION)
#undef DECLARE_SHIFT_INSTRUCTION

  // Shifts dst:src left by cl bits, affecting only dst.
  void shld(Register dst, Register src);

  // Shifts src:dst right by cl bits, affecting only dst.
  void shrd(Register dst, Register src);

  void store_rax(void* dst, RelocInfo::Mode mode);
  void store_rax(ExternalReference ref);

  void subb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x5, dst, src);
  }

  void testb(Register dst, Register src);
  void testb(Register reg, Immediate mask);
  void testb(const Operand& op, Immediate mask);
  void testb(const Operand& op, Register reg);

  // Bit operations.
  void bt(const Operand& dst, Register src);
  void bts(const Operand& dst, Register src);
  void bsrq(Register dst, Register src);
  void bsrq(Register dst, const Operand& src);
  void bsrl(Register dst, Register src);
  void bsrl(Register dst, const Operand& src);
  void bsfq(Register dst, Register src);
  void bsfq(Register dst, const Operand& src);
  void bsfl(Register dst, Register src);
  void bsfl(Register dst, const Operand& src);

  // Miscellaneous
  void clc();
  void cld();
  void cpuid();
  void hlt();
  void int3();
  void nop();
  void ret(int imm16);
  void ud2();
  void setcc(Condition cc, Register reg);

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

  // Calls
  // Call near relative 32-bit displacement, relative to next instruction.
  void call(Label* L);
  void call(Address entry, RelocInfo::Mode rmode);
  void call(Handle<Code> target,
            RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            TypeFeedbackId ast_id = TypeFeedbackId::None());

  // Calls directly to the given address using a relative offset.
  // Should only ever be used in Code objects for calls within the
  // same Code object. Should not be used when generating new code (use labels),
  // but only when patching existing code.
  void call(Address target);

  // Call near absolute indirect, address in register
  void call(Register adr);

  // Jumps
  // Jump short or near relative.
  // Use a 32-bit signed displacement.
  // Unconditional jump to L
  void jmp(Label* L, Label::Distance distance = Label::kFar);
  void jmp(Address entry, RelocInfo::Mode rmode);
  void jmp(Handle<Code> target, RelocInfo::Mode rmode);

  // Jump near absolute indirect (r64)
  void jmp(Register adr);
  void jmp(const Operand& src);

  // Conditional jumps
  void j(Condition cc,
         Label* L,
         Label::Distance distance = Label::kFar);
  void j(Condition cc, Address entry, RelocInfo::Mode rmode);
  void j(Condition cc, Handle<Code> target, RelocInfo::Mode rmode);

  // Floating-point operations
  void fld(int i);

  void fld1();
  void fldz();
  void fldpi();
  void fldln2();

  void fld_s(const Operand& adr);
  void fld_d(const Operand& adr);

  void fstp_s(const Operand& adr);
  void fstp_d(const Operand& adr);
  void fstp(int index);

  void fild_s(const Operand& adr);
  void fild_d(const Operand& adr);

  void fist_s(const Operand& adr);

  void fistp_s(const Operand& adr);
  void fistp_d(const Operand& adr);

  void fisttp_s(const Operand& adr);
  void fisttp_d(const Operand& adr);

  void fabs();
  void fchs();

  void fadd(int i);
  void fsub(int i);
  void fmul(int i);
  void fdiv(int i);

  void fisub_s(const Operand& adr);

  void faddp(int i = 1);
  void fsubp(int i = 1);
  void fsubrp(int i = 1);
  void fmulp(int i = 1);
  void fdivp(int i = 1);
  void fprem();
  void fprem1();

  void fxch(int i = 1);
  void fincstp();
  void ffree(int i = 0);

  void ftst();
  void fucomp(int i);
  void fucompp();
  void fucomi(int i);
  void fucomip();

  void fcompp();
  void fnstsw_ax();
  void fwait();
  void fnclex();

  void fsin();
  void fcos();
  void fptan();
  void fyl2x();
  void f2xm1();
  void fscale();
  void fninit();

  void frndint();

  void sahf();

  // SSE instructions
  void addss(XMMRegister dst, XMMRegister src);
  void addss(XMMRegister dst, const Operand& src);
  void subss(XMMRegister dst, XMMRegister src);
  void subss(XMMRegister dst, const Operand& src);
  void mulss(XMMRegister dst, XMMRegister src);
  void mulss(XMMRegister dst, const Operand& src);
  void divss(XMMRegister dst, XMMRegister src);
  void divss(XMMRegister dst, const Operand& src);

  void maxss(XMMRegister dst, XMMRegister src);
  void maxss(XMMRegister dst, const Operand& src);
  void minss(XMMRegister dst, XMMRegister src);
  void minss(XMMRegister dst, const Operand& src);

  void sqrtss(XMMRegister dst, XMMRegister src);
  void sqrtss(XMMRegister dst, const Operand& src);

  void ucomiss(XMMRegister dst, XMMRegister src);
  void ucomiss(XMMRegister dst, const Operand& src);
  void movaps(XMMRegister dst, XMMRegister src);

  // Don't use this unless it's important to keep the
  // top half of the destination register unchanged.
  // Use movaps when moving float values and movd for integer
  // values in xmm registers.
  void movss(XMMRegister dst, XMMRegister src);

  void movss(XMMRegister dst, const Operand& src);
  void movss(const Operand& dst, XMMRegister src);
  void shufps(XMMRegister dst, XMMRegister src, byte imm8);

  void cvttss2si(Register dst, const Operand& src);
  void cvttss2si(Register dst, XMMRegister src);
  void cvtlsi2ss(XMMRegister dst, const Operand& src);
  void cvtlsi2ss(XMMRegister dst, Register src);

  void andps(XMMRegister dst, XMMRegister src);
  void andps(XMMRegister dst, const Operand& src);
  void orps(XMMRegister dst, XMMRegister src);
  void orps(XMMRegister dst, const Operand& src);
  void xorps(XMMRegister dst, XMMRegister src);
  void xorps(XMMRegister dst, const Operand& src);

  void addps(XMMRegister dst, XMMRegister src);
  void addps(XMMRegister dst, const Operand& src);
  void subps(XMMRegister dst, XMMRegister src);
  void subps(XMMRegister dst, const Operand& src);
  void mulps(XMMRegister dst, XMMRegister src);
  void mulps(XMMRegister dst, const Operand& src);
  void divps(XMMRegister dst, XMMRegister src);
  void divps(XMMRegister dst, const Operand& src);

  void movmskps(Register dst, XMMRegister src);

  // SSE2 instructions
  void movd(XMMRegister dst, Register src);
  void movd(XMMRegister dst, const Operand& src);
  void movd(Register dst, XMMRegister src);
  void movq(XMMRegister dst, Register src);
  void movq(Register dst, XMMRegister src);
  void movq(XMMRegister dst, XMMRegister src);

  // Don't use this unless it's important to keep the
  // top half of the destination register unchanged.
  // Use movapd when moving double values and movq for integer
  // values in xmm registers.
  void movsd(XMMRegister dst, XMMRegister src);

  void movsd(const Operand& dst, XMMRegister src);
  void movsd(XMMRegister dst, const Operand& src);

  void movdqa(const Operand& dst, XMMRegister src);
  void movdqa(XMMRegister dst, const Operand& src);

  void movdqu(const Operand& dst, XMMRegister src);
  void movdqu(XMMRegister dst, const Operand& src);

  void movapd(XMMRegister dst, XMMRegister src);

  void psllq(XMMRegister reg, byte imm8);
  void psrlq(XMMRegister reg, byte imm8);
  void pslld(XMMRegister reg, byte imm8);
  void psrld(XMMRegister reg, byte imm8);

  void cvttsd2si(Register dst, const Operand& src);
  void cvttsd2si(Register dst, XMMRegister src);
  void cvttss2siq(Register dst, XMMRegister src);
  void cvttss2siq(Register dst, const Operand& src);
  void cvttsd2siq(Register dst, XMMRegister src);
  void cvttsd2siq(Register dst, const Operand& src);

  void cvtlsi2sd(XMMRegister dst, const Operand& src);
  void cvtlsi2sd(XMMRegister dst, Register src);

  void cvtqsi2ss(XMMRegister dst, const Operand& src);
  void cvtqsi2ss(XMMRegister dst, Register src);

  void cvtqsi2sd(XMMRegister dst, const Operand& src);
  void cvtqsi2sd(XMMRegister dst, Register src);


  void cvtss2sd(XMMRegister dst, XMMRegister src);
  void cvtss2sd(XMMRegister dst, const Operand& src);
  void cvtsd2ss(XMMRegister dst, XMMRegister src);
  void cvtsd2ss(XMMRegister dst, const Operand& src);

  void cvtsd2si(Register dst, XMMRegister src);
  void cvtsd2siq(Register dst, XMMRegister src);

  void addsd(XMMRegister dst, XMMRegister src);
  void addsd(XMMRegister dst, const Operand& src);
  void subsd(XMMRegister dst, XMMRegister src);
  void subsd(XMMRegister dst, const Operand& src);
  void mulsd(XMMRegister dst, XMMRegister src);
  void mulsd(XMMRegister dst, const Operand& src);
  void divsd(XMMRegister dst, XMMRegister src);
  void divsd(XMMRegister dst, const Operand& src);

  void maxsd(XMMRegister dst, XMMRegister src);
  void maxsd(XMMRegister dst, const Operand& src);
  void minsd(XMMRegister dst, XMMRegister src);
  void minsd(XMMRegister dst, const Operand& src);

  void andpd(XMMRegister dst, XMMRegister src);
  void orpd(XMMRegister dst, XMMRegister src);
  void xorpd(XMMRegister dst, XMMRegister src);
  void sqrtsd(XMMRegister dst, XMMRegister src);
  void sqrtsd(XMMRegister dst, const Operand& src);

  void ucomisd(XMMRegister dst, XMMRegister src);
  void ucomisd(XMMRegister dst, const Operand& src);
  void cmpltsd(XMMRegister dst, XMMRegister src);
  void pcmpeqd(XMMRegister dst, XMMRegister src);

  void movmskpd(Register dst, XMMRegister src);

  void punpckldq(XMMRegister dst, XMMRegister src);
  void punpckhdq(XMMRegister dst, XMMRegister src);

  // SSE 4.1 instruction
  void extractps(Register dst, XMMRegister src, byte imm8);

  void pextrd(Register dst, XMMRegister src, int8_t imm8);

  void pinsrd(XMMRegister dst, Register src, int8_t imm8);
  void pinsrd(XMMRegister dst, const Operand& src, int8_t imm8);

  void roundss(XMMRegister dst, XMMRegister src, RoundingMode mode);
  void roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode);

  // AVX instruction
  void vfmadd132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x99, dst, src1, src2);
  }
  void vfmadd213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xa9, dst, src1, src2);
  }
  void vfmadd231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xb9, dst, src1, src2);
  }
  void vfmadd132sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0x99, dst, src1, src2);
  }
  void vfmadd213sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xa9, dst, src1, src2);
  }
  void vfmadd231sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xb9, dst, src1, src2);
  }
  void vfmsub132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x9b, dst, src1, src2);
  }
  void vfmsub213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xab, dst, src1, src2);
  }
  void vfmsub231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xbb, dst, src1, src2);
  }
  void vfmsub132sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0x9b, dst, src1, src2);
  }
  void vfmsub213sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xab, dst, src1, src2);
  }
  void vfmsub231sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xbb, dst, src1, src2);
  }
  void vfnmadd132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x9d, dst, src1, src2);
  }
  void vfnmadd213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xad, dst, src1, src2);
  }
  void vfnmadd231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xbd, dst, src1, src2);
  }
  void vfnmadd132sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0x9d, dst, src1, src2);
  }
  void vfnmadd213sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xad, dst, src1, src2);
  }
  void vfnmadd231sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xbd, dst, src1, src2);
  }
  void vfnmsub132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x9f, dst, src1, src2);
  }
  void vfnmsub213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xaf, dst, src1, src2);
  }
  void vfnmsub231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xbf, dst, src1, src2);
  }
  void vfnmsub132sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0x9f, dst, src1, src2);
  }
  void vfnmsub213sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xaf, dst, src1, src2);
  }
  void vfnmsub231sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmasd(0xbf, dst, src1, src2);
  }
  void vfmasd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vfmasd(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2);

  void vfmadd132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x99, dst, src1, src2);
  }
  void vfmadd213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xa9, dst, src1, src2);
  }
  void vfmadd231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xb9, dst, src1, src2);
  }
  void vfmadd132ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0x99, dst, src1, src2);
  }
  void vfmadd213ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xa9, dst, src1, src2);
  }
  void vfmadd231ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xb9, dst, src1, src2);
  }
  void vfmsub132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x9b, dst, src1, src2);
  }
  void vfmsub213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xab, dst, src1, src2);
  }
  void vfmsub231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xbb, dst, src1, src2);
  }
  void vfmsub132ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0x9b, dst, src1, src2);
  }
  void vfmsub213ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xab, dst, src1, src2);
  }
  void vfmsub231ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xbb, dst, src1, src2);
  }
  void vfnmadd132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x9d, dst, src1, src2);
  }
  void vfnmadd213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xad, dst, src1, src2);
  }
  void vfnmadd231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xbd, dst, src1, src2);
  }
  void vfnmadd132ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0x9d, dst, src1, src2);
  }
  void vfnmadd213ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xad, dst, src1, src2);
  }
  void vfnmadd231ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xbd, dst, src1, src2);
  }
  void vfnmsub132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x9f, dst, src1, src2);
  }
  void vfnmsub213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xaf, dst, src1, src2);
  }
  void vfnmsub231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xbf, dst, src1, src2);
  }
  void vfnmsub132ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0x9f, dst, src1, src2);
  }
  void vfnmsub213ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xaf, dst, src1, src2);
  }
  void vfnmsub231ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vfmass(0xbf, dst, src1, src2);
  }
  void vfmass(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vfmass(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2);

  void vmovd(XMMRegister dst, Register src);
  void vmovd(XMMRegister dst, const Operand& src);
  void vmovd(Register dst, XMMRegister src);
  void vmovq(XMMRegister dst, Register src);
  void vmovq(XMMRegister dst, const Operand& src);
  void vmovq(Register dst, XMMRegister src);

  void vmovsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vsd(0x10, dst, src1, src2);
  }
  void vmovsd(XMMRegister dst, const Operand& src) {
    vsd(0x10, dst, xmm0, src);
  }
  void vmovsd(const Operand& dst, XMMRegister src) {
    vsd(0x11, src, xmm0, dst);
  }

#define AVX_SP_3(instr, opcode) \
  AVX_S_3(instr, opcode)        \
  AVX_P_3(instr, opcode)

#define AVX_S_3(instr, opcode)  \
  AVX_3(instr##ss, opcode, vss) \
  AVX_3(instr##sd, opcode, vsd)

#define AVX_P_3(instr, opcode)  \
  AVX_3(instr##ps, opcode, vps) \
  AVX_3(instr##pd, opcode, vpd)

#define AVX_3(instr, opcode, impl)                                     \
  void instr(XMMRegister dst, XMMRegister src1, XMMRegister src2) {    \
    impl(opcode, dst, src1, src2);                                     \
  }                                                                    \
  void instr(XMMRegister dst, XMMRegister src1, const Operand& src2) { \
    impl(opcode, dst, src1, src2);                                     \
  }

  AVX_SP_3(vsqrt, 0x51);
  AVX_SP_3(vadd, 0x58);
  AVX_SP_3(vsub, 0x5c);
  AVX_SP_3(vmul, 0x59);
  AVX_SP_3(vdiv, 0x5e);
  AVX_SP_3(vmin, 0x5d);
  AVX_SP_3(vmax, 0x5f);
  AVX_P_3(vand, 0x54);
  AVX_P_3(vor, 0x56);
  AVX_P_3(vxor, 0x57);
  AVX_3(vpcmpeqd, 0x76, vpd);
  AVX_3(vcvtsd2ss, 0x5a, vsd);

#undef AVX_3
#undef AVX_S_3
#undef AVX_P_3
#undef AVX_SP_3

  void vpsrlq(XMMRegister dst, XMMRegister src, byte imm8) {
    XMMRegister iop = {2};
    vpd(0x73, iop, dst, src);
    emit(imm8);
  }
  void vpsllq(XMMRegister dst, XMMRegister src, byte imm8) {
    XMMRegister iop = {6};
    vpd(0x73, iop, dst, src);
    emit(imm8);
  }
  void vcvtss2sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vsd(0x5a, dst, src1, src2, kF3, k0F, kWIG);
  }
  void vcvtss2sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vsd(0x5a, dst, src1, src2, kF3, k0F, kWIG);
  }
  void vcvtlsi2sd(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = {src2.code()};
    vsd(0x2a, dst, src1, isrc2, kF2, k0F, kW0);
  }
  void vcvtlsi2sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vsd(0x2a, dst, src1, src2, kF2, k0F, kW0);
  }
  void vcvtlsi2ss(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = {src2.code()};
    vsd(0x2a, dst, src1, isrc2, kF3, k0F, kW0);
  }
  void vcvtlsi2ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vsd(0x2a, dst, src1, src2, kF3, k0F, kW0);
  }
  void vcvtqsi2ss(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = {src2.code()};
    vsd(0x2a, dst, src1, isrc2, kF3, k0F, kW1);
  }
  void vcvtqsi2ss(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vsd(0x2a, dst, src1, src2, kF3, k0F, kW1);
  }
  void vcvtqsi2sd(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = {src2.code()};
    vsd(0x2a, dst, src1, isrc2, kF2, k0F, kW1);
  }
  void vcvtqsi2sd(XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vsd(0x2a, dst, src1, src2, kF2, k0F, kW1);
  }
  void vcvttss2si(Register dst, XMMRegister src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF3, k0F, kW0);
  }
  void vcvttss2si(Register dst, const Operand& src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF3, k0F, kW0);
  }
  void vcvttsd2si(Register dst, XMMRegister src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF2, k0F, kW0);
  }
  void vcvttsd2si(Register dst, const Operand& src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF2, k0F, kW0);
  }
  void vcvttss2siq(Register dst, XMMRegister src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF3, k0F, kW1);
  }
  void vcvttss2siq(Register dst, const Operand& src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF3, k0F, kW1);
  }
  void vcvttsd2siq(Register dst, XMMRegister src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF2, k0F, kW1);
  }
  void vcvttsd2siq(Register dst, const Operand& src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2c, idst, xmm0, src, kF2, k0F, kW1);
  }
  void vcvtsd2si(Register dst, XMMRegister src) {
    XMMRegister idst = {dst.code()};
    vsd(0x2d, idst, xmm0, src, kF2, k0F, kW0);
  }
  void vucomisd(XMMRegister dst, XMMRegister src) {
    vsd(0x2e, dst, xmm0, src, k66, k0F, kWIG);
  }
  void vucomisd(XMMRegister dst, const Operand& src) {
    vsd(0x2e, dst, xmm0, src, k66, k0F, kWIG);
  }
  void vroundss(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                RoundingMode mode) {
    vsd(0x0a, dst, src1, src2, k66, k0F3A, kWIG);
    emit(static_cast<byte>(mode) | 0x8);  // Mask precision exception.
  }
  void vroundsd(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                RoundingMode mode) {
    vsd(0x0b, dst, src1, src2, k66, k0F3A, kWIG);
    emit(static_cast<byte>(mode) | 0x8);  // Mask precision exception.
  }

  void vsd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vsd(op, dst, src1, src2, kF2, k0F, kWIG);
  }
  void vsd(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2) {
    vsd(op, dst, src1, src2, kF2, k0F, kWIG);
  }
  void vsd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2,
           SIMDPrefix pp, LeadingOpcode m, VexW w);
  void vsd(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2,
           SIMDPrefix pp, LeadingOpcode m, VexW w);

  void vmovss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vss(0x10, dst, src1, src2);
  }
  void vmovss(XMMRegister dst, const Operand& src) {
    vss(0x10, dst, xmm0, src);
  }
  void vmovss(const Operand& dst, XMMRegister src) {
    vss(0x11, src, xmm0, dst);
  }
  void vucomiss(XMMRegister dst, XMMRegister src);
  void vucomiss(XMMRegister dst, const Operand& src);
  void vss(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vss(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2);

  void vmovaps(XMMRegister dst, XMMRegister src) { vps(0x28, dst, xmm0, src); }
  void vmovapd(XMMRegister dst, XMMRegister src) { vpd(0x28, dst, xmm0, src); }
  void vmovmskpd(Register dst, XMMRegister src) {
    XMMRegister idst = {dst.code()};
    vpd(0x50, idst, xmm0, src);
  }

  void vps(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vps(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2);
  void vpd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vpd(byte op, XMMRegister dst, XMMRegister src1, const Operand& src2);

  // BMI instruction
  void andnq(Register dst, Register src1, Register src2) {
    bmi1q(0xf2, dst, src1, src2);
  }
  void andnq(Register dst, Register src1, const Operand& src2) {
    bmi1q(0xf2, dst, src1, src2);
  }
  void andnl(Register dst, Register src1, Register src2) {
    bmi1l(0xf2, dst, src1, src2);
  }
  void andnl(Register dst, Register src1, const Operand& src2) {
    bmi1l(0xf2, dst, src1, src2);
  }
  void bextrq(Register dst, Register src1, Register src2) {
    bmi1q(0xf7, dst, src2, src1);
  }
  void bextrq(Register dst, const Operand& src1, Register src2) {
    bmi1q(0xf7, dst, src2, src1);
  }
  void bextrl(Register dst, Register src1, Register src2) {
    bmi1l(0xf7, dst, src2, src1);
  }
  void bextrl(Register dst, const Operand& src1, Register src2) {
    bmi1l(0xf7, dst, src2, src1);
  }
  void blsiq(Register dst, Register src) {
    Register ireg = {3};
    bmi1q(0xf3, ireg, dst, src);
  }
  void blsiq(Register dst, const Operand& src) {
    Register ireg = {3};
    bmi1q(0xf3, ireg, dst, src);
  }
  void blsil(Register dst, Register src) {
    Register ireg = {3};
    bmi1l(0xf3, ireg, dst, src);
  }
  void blsil(Register dst, const Operand& src) {
    Register ireg = {3};
    bmi1l(0xf3, ireg, dst, src);
  }
  void blsmskq(Register dst, Register src) {
    Register ireg = {2};
    bmi1q(0xf3, ireg, dst, src);
  }
  void blsmskq(Register dst, const Operand& src) {
    Register ireg = {2};
    bmi1q(0xf3, ireg, dst, src);
  }
  void blsmskl(Register dst, Register src) {
    Register ireg = {2};
    bmi1l(0xf3, ireg, dst, src);
  }
  void blsmskl(Register dst, const Operand& src) {
    Register ireg = {2};
    bmi1l(0xf3, ireg, dst, src);
  }
  void blsrq(Register dst, Register src) {
    Register ireg = {1};
    bmi1q(0xf3, ireg, dst, src);
  }
  void blsrq(Register dst, const Operand& src) {
    Register ireg = {1};
    bmi1q(0xf3, ireg, dst, src);
  }
  void blsrl(Register dst, Register src) {
    Register ireg = {1};
    bmi1l(0xf3, ireg, dst, src);
  }
  void blsrl(Register dst, const Operand& src) {
    Register ireg = {1};
    bmi1l(0xf3, ireg, dst, src);
  }
  void tzcntq(Register dst, Register src);
  void tzcntq(Register dst, const Operand& src);
  void tzcntl(Register dst, Register src);
  void tzcntl(Register dst, const Operand& src);

  void lzcntq(Register dst, Register src);
  void lzcntq(Register dst, const Operand& src);
  void lzcntl(Register dst, Register src);
  void lzcntl(Register dst, const Operand& src);

  void popcntq(Register dst, Register src);
  void popcntq(Register dst, const Operand& src);
  void popcntl(Register dst, Register src);
  void popcntl(Register dst, const Operand& src);

  void bzhiq(Register dst, Register src1, Register src2) {
    bmi2q(kNone, 0xf5, dst, src2, src1);
  }
  void bzhiq(Register dst, const Operand& src1, Register src2) {
    bmi2q(kNone, 0xf5, dst, src2, src1);
  }
  void bzhil(Register dst, Register src1, Register src2) {
    bmi2l(kNone, 0xf5, dst, src2, src1);
  }
  void bzhil(Register dst, const Operand& src1, Register src2) {
    bmi2l(kNone, 0xf5, dst, src2, src1);
  }
  void mulxq(Register dst1, Register dst2, Register src) {
    bmi2q(kF2, 0xf6, dst1, dst2, src);
  }
  void mulxq(Register dst1, Register dst2, const Operand& src) {
    bmi2q(kF2, 0xf6, dst1, dst2, src);
  }
  void mulxl(Register dst1, Register dst2, Register src) {
    bmi2l(kF2, 0xf6, dst1, dst2, src);
  }
  void mulxl(Register dst1, Register dst2, const Operand& src) {
    bmi2l(kF2, 0xf6, dst1, dst2, src);
  }
  void pdepq(Register dst, Register src1, Register src2) {
    bmi2q(kF2, 0xf5, dst, src1, src2);
  }
  void pdepq(Register dst, Register src1, const Operand& src2) {
    bmi2q(kF2, 0xf5, dst, src1, src2);
  }
  void pdepl(Register dst, Register src1, Register src2) {
    bmi2l(kF2, 0xf5, dst, src1, src2);
  }
  void pdepl(Register dst, Register src1, const Operand& src2) {
    bmi2l(kF2, 0xf5, dst, src1, src2);
  }
  void pextq(Register dst, Register src1, Register src2) {
    bmi2q(kF3, 0xf5, dst, src1, src2);
  }
  void pextq(Register dst, Register src1, const Operand& src2) {
    bmi2q(kF3, 0xf5, dst, src1, src2);
  }
  void pextl(Register dst, Register src1, Register src2) {
    bmi2l(kF3, 0xf5, dst, src1, src2);
  }
  void pextl(Register dst, Register src1, const Operand& src2) {
    bmi2l(kF3, 0xf5, dst, src1, src2);
  }
  void sarxq(Register dst, Register src1, Register src2) {
    bmi2q(kF3, 0xf7, dst, src2, src1);
  }
  void sarxq(Register dst, const Operand& src1, Register src2) {
    bmi2q(kF3, 0xf7, dst, src2, src1);
  }
  void sarxl(Register dst, Register src1, Register src2) {
    bmi2l(kF3, 0xf7, dst, src2, src1);
  }
  void sarxl(Register dst, const Operand& src1, Register src2) {
    bmi2l(kF3, 0xf7, dst, src2, src1);
  }
  void shlxq(Register dst, Register src1, Register src2) {
    bmi2q(k66, 0xf7, dst, src2, src1);
  }
  void shlxq(Register dst, const Operand& src1, Register src2) {
    bmi2q(k66, 0xf7, dst, src2, src1);
  }
  void shlxl(Register dst, Register src1, Register src2) {
    bmi2l(k66, 0xf7, dst, src2, src1);
  }
  void shlxl(Register dst, const Operand& src1, Register src2) {
    bmi2l(k66, 0xf7, dst, src2, src1);
  }
  void shrxq(Register dst, Register src1, Register src2) {
    bmi2q(kF2, 0xf7, dst, src2, src1);
  }
  void shrxq(Register dst, const Operand& src1, Register src2) {
    bmi2q(kF2, 0xf7, dst, src2, src1);
  }
  void shrxl(Register dst, Register src1, Register src2) {
    bmi2l(kF2, 0xf7, dst, src2, src1);
  }
  void shrxl(Register dst, const Operand& src1, Register src2) {
    bmi2l(kF2, 0xf7, dst, src2, src1);
  }
  void rorxq(Register dst, Register src, byte imm8);
  void rorxq(Register dst, const Operand& src, byte imm8);
  void rorxl(Register dst, Register src, byte imm8);
  void rorxl(Register dst, const Operand& src, byte imm8);

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Mark generator continuation.
  void RecordGeneratorContinuation();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot(RelocInfo::Mode mode);

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(const int reason, int raw_position);

  void PatchConstantPoolAccessInstruction(int pc_offset, int offset,
                                          ConstantPoolEntry::Access access,
                                          ConstantPoolEntry::Type type) {
    // No embedded constant pool support.
    UNREACHABLE();
  }

  // Writes a single word of data in the code stream.
  // Used for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data) { dq(data); }
  void dq(Label* label);

  PositionsRecorder* positions_recorder() { return &positions_recorder_; }

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool buffer_overflow() const {
    return pc_ >= reloc_info_writer.pos() - kGap;
  }

  // Get the number of bytes available in the buffer.
  inline int available_space() const {
    return static_cast<int>(reloc_info_writer.pos() - pc_);
  }

  static bool IsNop(Address addr);

  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512*MB;

  byte byte_at(int pos)  { return buffer_[pos]; }
  void set_byte_at(int pos, byte value) { buffer_[pos] = value; }

 protected:
  // Call near indirect
  void call(const Operand& operand);

 private:
  byte* addr_at(int pos)  { return buffer_ + pos; }
  uint32_t long_at(int pos)  {
    return *reinterpret_cast<uint32_t*>(addr_at(pos));
  }
  void long_at_put(int pos, uint32_t x)  {
    *reinterpret_cast<uint32_t*>(addr_at(pos)) = x;
  }

  // code emission
  void GrowBuffer();

  void emit(byte x) { *pc_++ = x; }
  inline void emitl(uint32_t x);
  inline void emitp(void* x, RelocInfo::Mode rmode);
  inline void emitq(uint64_t x);
  inline void emitw(uint16_t x);
  inline void emit_code_target(Handle<Code> target,
                               RelocInfo::Mode rmode,
                               TypeFeedbackId ast_id = TypeFeedbackId::None());
  inline void emit_runtime_entry(Address entry, RelocInfo::Mode rmode);
  void emit(Immediate x) { emitl(x.value_); }

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of both register codes.
  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is set.
  inline void emit_rex_64(XMMRegister reg, Register rm_reg);
  inline void emit_rex_64(Register reg, XMMRegister rm_reg);
  inline void emit_rex_64(Register reg, Register rm_reg);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the destination, index, and base register codes.
  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is set.
  inline void emit_rex_64(Register reg, const Operand& op);
  inline void emit_rex_64(XMMRegister reg, const Operand& op);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the register code.
  // The high bit of register is used for REX.B.
  // REX.W is set and REX.R and REX.X are clear.
  inline void emit_rex_64(Register rm_reg);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the index and base register codes.
  // The high bit of op's base register is used for REX.B, and the high
  // bit of op's index register is used for REX.X.
  // REX.W is set and REX.R clear.
  inline void emit_rex_64(const Operand& op);

  // Emit a REX prefix that only sets REX.W to choose a 64-bit operand size.
  void emit_rex_64() { emit(0x48); }

  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is clear.
  inline void emit_rex_32(Register reg, Register rm_reg);

  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is cleared.
  inline void emit_rex_32(Register reg, const Operand& op);

  // High bit of rm_reg goes to REX.B.
  // REX.W, REX.R and REX.X are clear.
  inline void emit_rex_32(Register rm_reg);

  // High bit of base goes to REX.B and high bit of index to REX.X.
  // REX.W and REX.R are clear.
  inline void emit_rex_32(const Operand& op);

  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is cleared.  If no REX bits are set, no byte is emitted.
  inline void emit_optional_rex_32(Register reg, Register rm_reg);

  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is cleared.  If no REX bits are set, nothing
  // is emitted.
  inline void emit_optional_rex_32(Register reg, const Operand& op);

  // As for emit_optional_rex_32(Register, Register), except that
  // the registers are XMM registers.
  inline void emit_optional_rex_32(XMMRegister reg, XMMRegister base);

  // As for emit_optional_rex_32(Register, Register), except that
  // one of the registers is an XMM registers.
  inline void emit_optional_rex_32(XMMRegister reg, Register base);

  // As for emit_optional_rex_32(Register, Register), except that
  // one of the registers is an XMM registers.
  inline void emit_optional_rex_32(Register reg, XMMRegister base);

  // As for emit_optional_rex_32(Register, const Operand&), except that
  // the register is an XMM register.
  inline void emit_optional_rex_32(XMMRegister reg, const Operand& op);

  // Optionally do as emit_rex_32(Register) if the register number has
  // the high bit set.
  inline void emit_optional_rex_32(Register rm_reg);
  inline void emit_optional_rex_32(XMMRegister rm_reg);

  // Optionally do as emit_rex_32(const Operand&) if the operand register
  // numbers have a high bit set.
  inline void emit_optional_rex_32(const Operand& op);

  void emit_rex(int size) {
    if (size == kInt64Size) {
      emit_rex_64();
    } else {
      DCHECK(size == kInt32Size);
    }
  }

  template<class P1>
  void emit_rex(P1 p1, int size) {
    if (size == kInt64Size) {
      emit_rex_64(p1);
    } else {
      DCHECK(size == kInt32Size);
      emit_optional_rex_32(p1);
    }
  }

  template<class P1, class P2>
  void emit_rex(P1 p1, P2 p2, int size) {
    if (size == kInt64Size) {
      emit_rex_64(p1, p2);
    } else {
      DCHECK(size == kInt32Size);
      emit_optional_rex_32(p1, p2);
    }
  }

  // Emit vex prefix
  void emit_vex2_byte0() { emit(0xc5); }
  inline void emit_vex2_byte1(XMMRegister reg, XMMRegister v, VectorLength l,
                              SIMDPrefix pp);
  void emit_vex3_byte0() { emit(0xc4); }
  inline void emit_vex3_byte1(XMMRegister reg, XMMRegister rm, LeadingOpcode m);
  inline void emit_vex3_byte1(XMMRegister reg, const Operand& rm,
                              LeadingOpcode m);
  inline void emit_vex3_byte2(VexW w, XMMRegister v, VectorLength l,
                              SIMDPrefix pp);
  inline void emit_vex_prefix(XMMRegister reg, XMMRegister v, XMMRegister rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);
  inline void emit_vex_prefix(Register reg, Register v, Register rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);
  inline void emit_vex_prefix(XMMRegister reg, XMMRegister v, const Operand& rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);
  inline void emit_vex_prefix(Register reg, Register v, const Operand& rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);

  // Emit the ModR/M byte, and optionally the SIB byte and
  // 1- or 4-byte offset for a memory operand.  Also encodes
  // the second operand of the operation, a register or operation
  // subcode, into the reg field of the ModR/M byte.
  void emit_operand(Register reg, const Operand& adr) {
    emit_operand(reg.low_bits(), adr);
  }

  // Emit the ModR/M byte, and optionally the SIB byte and
  // 1- or 4-byte offset for a memory operand.  Also used to encode
  // a three-bit opcode extension into the ModR/M byte.
  void emit_operand(int rm, const Operand& adr);

  // Emit a ModR/M byte with registers coded in the reg and rm_reg fields.
  void emit_modrm(Register reg, Register rm_reg) {
    emit(0xC0 | reg.low_bits() << 3 | rm_reg.low_bits());
  }

  // Emit a ModR/M byte with an operation subcode in the reg field and
  // a register in the rm_reg field.
  void emit_modrm(int code, Register rm_reg) {
    DCHECK(is_uint3(code));
    emit(0xC0 | code << 3 | rm_reg.low_bits());
  }

  // Emit the code-object-relative offset of the label's position
  inline void emit_code_relative_offset(Label* label);

  // The first argument is the reg field, the second argument is the r/m field.
  void emit_sse_operand(XMMRegister dst, XMMRegister src);
  void emit_sse_operand(XMMRegister reg, const Operand& adr);
  void emit_sse_operand(Register reg, const Operand& adr);
  void emit_sse_operand(XMMRegister dst, Register src);
  void emit_sse_operand(Register dst, XMMRegister src);

  // Emit machine code for one of the operations ADD, ADC, SUB, SBC,
  // AND, OR, XOR, or CMP.  The encodings of these operations are all
  // similar, differing just in the opcode or in the reg field of the
  // ModR/M byte.
  void arithmetic_op_8(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op_8(byte opcode, Register reg, const Operand& rm_reg);
  void arithmetic_op_16(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op_16(byte opcode, Register reg, const Operand& rm_reg);
  // Operate on operands/registers with pointer size, 32-bit or 64-bit size.
  void arithmetic_op(byte opcode, Register reg, Register rm_reg, int size);
  void arithmetic_op(byte opcode,
                     Register reg,
                     const Operand& rm_reg,
                     int size);
  // Operate on a byte in memory or register.
  void immediate_arithmetic_op_8(byte subcode,
                                 Register dst,
                                 Immediate src);
  void immediate_arithmetic_op_8(byte subcode,
                                 const Operand& dst,
                                 Immediate src);
  // Operate on a word in memory or register.
  void immediate_arithmetic_op_16(byte subcode,
                                  Register dst,
                                  Immediate src);
  void immediate_arithmetic_op_16(byte subcode,
                                  const Operand& dst,
                                  Immediate src);
  // Operate on operands/registers with pointer size, 32-bit or 64-bit size.
  void immediate_arithmetic_op(byte subcode,
                               Register dst,
                               Immediate src,
                               int size);
  void immediate_arithmetic_op(byte subcode,
                               const Operand& dst,
                               Immediate src,
                               int size);

  // Emit machine code for a shift operation.
  void shift(Operand dst, Immediate shift_amount, int subcode, int size);
  void shift(Register dst, Immediate shift_amount, int subcode, int size);
  // Shift dst by cl % 64 bits.
  void shift(Register dst, int subcode, int size);
  void shift(Operand dst, int subcode, int size);

  void emit_farith(int b1, int b2, int i);

  // labels
  // void print(Label* L);
  void bind_to(Label* L, int pos);

  // record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Arithmetics
  void emit_add(Register dst, Register src, int size) {
    arithmetic_op(0x03, dst, src, size);
  }

  void emit_add(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x0, dst, src, size);
  }

  void emit_add(Register dst, const Operand& src, int size) {
    arithmetic_op(0x03, dst, src, size);
  }

  void emit_add(const Operand& dst, Register src, int size) {
    arithmetic_op(0x1, src, dst, size);
  }

  void emit_add(const Operand& dst, Immediate src, int size) {
    immediate_arithmetic_op(0x0, dst, src, size);
  }

  void emit_and(Register dst, Register src, int size) {
    arithmetic_op(0x23, dst, src, size);
  }

  void emit_and(Register dst, const Operand& src, int size) {
    arithmetic_op(0x23, dst, src, size);
  }

  void emit_and(const Operand& dst, Register src, int size) {
    arithmetic_op(0x21, src, dst, size);
  }

  void emit_and(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x4, dst, src, size);
  }

  void emit_and(const Operand& dst, Immediate src, int size) {
    immediate_arithmetic_op(0x4, dst, src, size);
  }

  void emit_cmp(Register dst, Register src, int size) {
    arithmetic_op(0x3B, dst, src, size);
  }

  void emit_cmp(Register dst, const Operand& src, int size) {
    arithmetic_op(0x3B, dst, src, size);
  }

  void emit_cmp(const Operand& dst, Register src, int size) {
    arithmetic_op(0x39, src, dst, size);
  }

  void emit_cmp(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x7, dst, src, size);
  }

  void emit_cmp(const Operand& dst, Immediate src, int size) {
    immediate_arithmetic_op(0x7, dst, src, size);
  }

  void emit_dec(Register dst, int size);
  void emit_dec(const Operand& dst, int size);

  // Divide rdx:rax by src.  Quotient in rax, remainder in rdx when size is 64.
  // Divide edx:eax by lower 32 bits of src.  Quotient in eax, remainder in edx
  // when size is 32.
  void emit_idiv(Register src, int size);
  void emit_div(Register src, int size);

  // Signed multiply instructions.
  // rdx:rax = rax * src when size is 64 or edx:eax = eax * src when size is 32.
  void emit_imul(Register src, int size);
  void emit_imul(const Operand& src, int size);
  void emit_imul(Register dst, Register src, int size);
  void emit_imul(Register dst, const Operand& src, int size);
  void emit_imul(Register dst, Register src, Immediate imm, int size);
  void emit_imul(Register dst, const Operand& src, Immediate imm, int size);

  void emit_inc(Register dst, int size);
  void emit_inc(const Operand& dst, int size);

  void emit_lea(Register dst, const Operand& src, int size);

  void emit_mov(Register dst, const Operand& src, int size);
  void emit_mov(Register dst, Register src, int size);
  void emit_mov(const Operand& dst, Register src, int size);
  void emit_mov(Register dst, Immediate value, int size);
  void emit_mov(const Operand& dst, Immediate value, int size);

  void emit_movzxb(Register dst, const Operand& src, int size);
  void emit_movzxb(Register dst, Register src, int size);
  void emit_movzxw(Register dst, const Operand& src, int size);
  void emit_movzxw(Register dst, Register src, int size);

  void emit_neg(Register dst, int size);
  void emit_neg(const Operand& dst, int size);

  void emit_not(Register dst, int size);
  void emit_not(const Operand& dst, int size);

  void emit_or(Register dst, Register src, int size) {
    arithmetic_op(0x0B, dst, src, size);
  }

  void emit_or(Register dst, const Operand& src, int size) {
    arithmetic_op(0x0B, dst, src, size);
  }

  void emit_or(const Operand& dst, Register src, int size) {
    arithmetic_op(0x9, src, dst, size);
  }

  void emit_or(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x1, dst, src, size);
  }

  void emit_or(const Operand& dst, Immediate src, int size) {
    immediate_arithmetic_op(0x1, dst, src, size);
  }

  void emit_repmovs(int size);

  void emit_sbb(Register dst, Register src, int size) {
    arithmetic_op(0x1b, dst, src, size);
  }

  void emit_sub(Register dst, Register src, int size) {
    arithmetic_op(0x2B, dst, src, size);
  }

  void emit_sub(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x5, dst, src, size);
  }

  void emit_sub(Register dst, const Operand& src, int size) {
    arithmetic_op(0x2B, dst, src, size);
  }

  void emit_sub(const Operand& dst, Register src, int size) {
    arithmetic_op(0x29, src, dst, size);
  }

  void emit_sub(const Operand& dst, Immediate src, int size) {
    immediate_arithmetic_op(0x5, dst, src, size);
  }

  void emit_test(Register dst, Register src, int size);
  void emit_test(Register reg, Immediate mask, int size);
  void emit_test(const Operand& op, Register reg, int size);
  void emit_test(const Operand& op, Immediate mask, int size);
  void emit_test(Register reg, const Operand& op, int size) {
    return emit_test(op, reg, size);
  }

  void emit_xchg(Register dst, Register src, int size);
  void emit_xchg(Register dst, const Operand& src, int size);

  void emit_xor(Register dst, Register src, int size) {
    if (size == kInt64Size && dst.code() == src.code()) {
    // 32 bit operations zero the top 32 bits of 64 bit registers. Therefore
    // there is no need to make this a 64 bit operation.
      arithmetic_op(0x33, dst, src, kInt32Size);
    } else {
      arithmetic_op(0x33, dst, src, size);
    }
  }

  void emit_xor(Register dst, const Operand& src, int size) {
    arithmetic_op(0x33, dst, src, size);
  }

  void emit_xor(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x6, dst, src, size);
  }

  void emit_xor(const Operand& dst, Immediate src, int size) {
    immediate_arithmetic_op(0x6, dst, src, size);
  }

  void emit_xor(const Operand& dst, Register src, int size) {
    arithmetic_op(0x31, src, dst, size);
  }

  // Most BMI instructions are similiar.
  void bmi1q(byte op, Register reg, Register vreg, Register rm);
  void bmi1q(byte op, Register reg, Register vreg, const Operand& rm);
  void bmi1l(byte op, Register reg, Register vreg, Register rm);
  void bmi1l(byte op, Register reg, Register vreg, const Operand& rm);
  void bmi2q(SIMDPrefix pp, byte op, Register reg, Register vreg, Register rm);
  void bmi2q(SIMDPrefix pp, byte op, Register reg, Register vreg,
             const Operand& rm);
  void bmi2l(SIMDPrefix pp, byte op, Register reg, Register vreg, Register rm);
  void bmi2l(SIMDPrefix pp, byte op, Register reg, Register vreg,
             const Operand& rm);

  friend class CodePatcher;
  friend class EnsureSpace;
  friend class RegExpMacroAssemblerX64;

  // code generation
  RelocInfoWriter reloc_info_writer;

  // Internal reference positions, required for (potential) patching in
  // GrowBuffer(); contains only those internal references whose labels
  // are already bound.
  std::deque<int> internal_reference_positions_;

  List< Handle<Code> > code_targets_;

  PositionsRecorder positions_recorder_;
  friend class PositionsRecorder;
};


// Helper class that ensures that there is enough space for generating
// instructions and relocation information.  The constructor makes
// sure that there is enough space and (in debug mode) the destructor
// checks that we did not generate too much.
class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) : assembler_(assembler) {
    if (assembler_->buffer_overflow()) assembler_->GrowBuffer();
#ifdef DEBUG
    space_before_ = assembler_->available_space();
#endif
  }

#ifdef DEBUG
  ~EnsureSpace() {
    int bytes_generated = space_before_ - assembler_->available_space();
    DCHECK(bytes_generated < assembler_->kGap);
  }
#endif

 private:
  Assembler* assembler_;
#ifdef DEBUG
  int space_before_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_X64_ASSEMBLER_X64_H_

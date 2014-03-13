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

#include "serialize.h"

namespace v8 {
namespace internal {

// Utility functions

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
  // The non-allocatable registers are:
  //  rsp - stack pointer
  //  rbp - frame pointer
  //  r10 - fixed scratch register
  //  r12 - smi constant register
  //  r13 - root register
  static const int kMaxNumAllocatableRegisters = 11;
  static int NumAllocatableRegisters() {
    return kMaxNumAllocatableRegisters;
  }
  static const int kNumRegisters = 16;

  static int ToAllocationIndex(Register reg) {
    return kAllocationIndexByRegisterCode[reg.code()];
  }

  static Register FromAllocationIndex(int index) {
    ASSERT(index >= 0 && index < kMaxNumAllocatableRegisters);
    Register result = { kRegisterCodeByAllocationIndex[index] };
    return result;
  }

  static const char* AllocationIndexToString(int index) {
    ASSERT(index >= 0 && index < kMaxNumAllocatableRegisters);
    const char* const names[] = {
      "rax",
      "rbx",
      "rdx",
      "rcx",
      "rsi",
      "rdi",
      "r8",
      "r9",
      "r11",
      "r14",
      "r15"
    };
    return names[index];
  }

  static Register from_code(int code) {
    Register r = { code };
    return r;
  }
  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(Register reg) const { return code_ == reg.code_; }
  // rax, rbx, rcx and rdx are byte registers, the rest are not.
  bool is_byte_register() const { return code_ <= 3; }
  int code() const {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const {
    return 1 << code_;
  }

  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const {
    return code_ >> 3;
  }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const {
    return code_ & 0x7;
  }

  // Unfortunately we can't make this private in a struct when initializing
  // by assignment.
  int code_;

 private:
  static const int kRegisterCodeByAllocationIndex[kMaxNumAllocatableRegisters];
  static const int kAllocationIndexByRegisterCode[kNumRegisters];
};

const int kRegister_rax_Code = 0;
const int kRegister_rcx_Code = 1;
const int kRegister_rdx_Code = 2;
const int kRegister_rbx_Code = 3;
const int kRegister_rsp_Code = 4;
const int kRegister_rbp_Code = 5;
const int kRegister_rsi_Code = 6;
const int kRegister_rdi_Code = 7;
const int kRegister_r8_Code = 8;
const int kRegister_r9_Code = 9;
const int kRegister_r10_Code = 10;
const int kRegister_r11_Code = 11;
const int kRegister_r12_Code = 12;
const int kRegister_r13_Code = 13;
const int kRegister_r14_Code = 14;
const int kRegister_r15_Code = 15;
const int kRegister_no_reg_Code = -1;

const Register rax = { kRegister_rax_Code };
const Register rcx = { kRegister_rcx_Code };
const Register rdx = { kRegister_rdx_Code };
const Register rbx = { kRegister_rbx_Code };
const Register rsp = { kRegister_rsp_Code };
const Register rbp = { kRegister_rbp_Code };
const Register rsi = { kRegister_rsi_Code };
const Register rdi = { kRegister_rdi_Code };
const Register r8 = { kRegister_r8_Code };
const Register r9 = { kRegister_r9_Code };
const Register r10 = { kRegister_r10_Code };
const Register r11 = { kRegister_r11_Code };
const Register r12 = { kRegister_r12_Code };
const Register r13 = { kRegister_r13_Code };
const Register r14 = { kRegister_r14_Code };
const Register r15 = { kRegister_r15_Code };
const Register no_reg = { kRegister_no_reg_Code };

#ifdef _WIN64
  // Windows calling convention
  const Register arg_reg_1 = { kRegister_rcx_Code };
  const Register arg_reg_2 = { kRegister_rdx_Code };
  const Register arg_reg_3 = { kRegister_r8_Code };
  const Register arg_reg_4 = { kRegister_r9_Code };
#else
  // AMD64 calling convention
  const Register arg_reg_1 = { kRegister_rdi_Code };
  const Register arg_reg_2 = { kRegister_rsi_Code };
  const Register arg_reg_3 = { kRegister_rdx_Code };
  const Register arg_reg_4 = { kRegister_rcx_Code };
#endif  // _WIN64

struct XMMRegister {
  static const int kMaxNumRegisters = 16;
  static const int kMaxNumAllocatableRegisters = 15;
  static int NumAllocatableRegisters() {
    return kMaxNumAllocatableRegisters;
  }

  static int ToAllocationIndex(XMMRegister reg) {
    ASSERT(reg.code() != 0);
    return reg.code() - 1;
  }

  static XMMRegister FromAllocationIndex(int index) {
    ASSERT(0 <= index && index < kMaxNumAllocatableRegisters);
    XMMRegister result = { index + 1 };
    return result;
  }

  static const char* AllocationIndexToString(int index) {
    ASSERT(index >= 0 && index < kMaxNumAllocatableRegisters);
    const char* const names[] = {
      "xmm1",
      "xmm2",
      "xmm3",
      "xmm4",
      "xmm5",
      "xmm6",
      "xmm7",
      "xmm8",
      "xmm9",
      "xmm10",
      "xmm11",
      "xmm12",
      "xmm13",
      "xmm14",
      "xmm15"
    };
    return names[index];
  }

  static XMMRegister from_code(int code) {
    ASSERT(code >= 0);
    ASSERT(code < kMaxNumRegisters);
    XMMRegister r = { code };
    return r;
  }
  bool is_valid() const { return 0 <= code_ && code_ < kMaxNumRegisters; }
  bool is(XMMRegister reg) const { return code_ == reg.code_; }
  int code() const {
    ASSERT(is_valid());
    return code_;
  }

  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const {
    return code_ >> 3;
  }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const {
    return code_ & 0x7;
  }

  int code_;
};

const XMMRegister xmm0 = { 0 };
const XMMRegister xmm1 = { 1 };
const XMMRegister xmm2 = { 2 };
const XMMRegister xmm3 = { 3 };
const XMMRegister xmm4 = { 4 };
const XMMRegister xmm5 = { 5 };
const XMMRegister xmm6 = { 6 };
const XMMRegister xmm7 = { 7 };
const XMMRegister xmm8 = { 8 };
const XMMRegister xmm9 = { 9 };
const XMMRegister xmm10 = { 10 };
const XMMRegister xmm11 = { 11 };
const XMMRegister xmm12 = { 12 };
const XMMRegister xmm13 = { 13 };
const XMMRegister xmm14 = { 14 };
const XMMRegister xmm15 = { 15 };


typedef XMMRegister DoubleRegister;


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


// Corresponds to transposing the operands of a comparison.
inline Condition ReverseCondition(Condition cc) {
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
  };
}


// -----------------------------------------------------------------------------
// Machine instruction Immediates

class Immediate BASE_EMBEDDED {
 public:
  explicit Immediate(int32_t value) : value_(value) {}

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
  byte buf_[6];
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

  friend class Assembler;
};


// CpuFeatures keeps track of which features are supported by the target CPU.
// Supported features must be enabled by a CpuFeatureScope before use.
// Example:
//   if (assembler->IsSupported(SSE3)) {
//     CpuFeatureScope fscope(assembler, SSE3);
//     // Generate SSE3 floating point code.
//   } else {
//     // Generate standard SSE2 floating point code.
//   }
class CpuFeatures : public AllStatic {
 public:
  // Detect features of the target CPU. Set safe defaults if the serializer
  // is enabled (snapshots must be portable).
  static void Probe();

  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(CpuFeature f) {
    if (Check(f, cross_compile_)) return true;
    ASSERT(initialized_);
    if (f == SSE3 && !FLAG_enable_sse3) return false;
    if (f == SSE4_1 && !FLAG_enable_sse4_1) return false;
    if (f == CMOV && !FLAG_enable_cmov) return false;
    if (f == SAHF && !FLAG_enable_sahf) return false;
    return Check(f, supported_);
  }

  static bool IsFoundByRuntimeProbingOnly(CpuFeature f) {
    ASSERT(initialized_);
    return Check(f, found_by_runtime_probing_only_);
  }

  static bool IsSafeForSnapshot(CpuFeature f) {
    return Check(f, cross_compile_) ||
           (IsSupported(f) &&
            (!Serializer::enabled() || !IsFoundByRuntimeProbingOnly(f)));
  }

  static bool VerifyCrossCompiling() {
    return cross_compile_ == 0;
  }

  static bool VerifyCrossCompiling(CpuFeature f) {
    uint64_t mask = flag2set(f);
    return cross_compile_ == 0 ||
           (cross_compile_ & mask) == mask;
  }

 private:
  static bool Check(CpuFeature f, uint64_t set) {
    return (set & flag2set(f)) != 0;
  }

  static uint64_t flag2set(CpuFeature f) {
    return static_cast<uint64_t>(1) << f;
  }

  // Safe defaults include CMOV for X64. It is always available, if
  // anyone checks, but they shouldn't need to check.
  // The required user mode extensions in X64 are (from AMD64 ABI Table A.1):
  //   fpu, tsc, cx8, cmov, mmx, sse, sse2, fxsr, syscall
  static const uint64_t kDefaultCpuFeatures = (1 << CMOV);

#ifdef DEBUG
  static bool initialized_;
#endif
  static uint64_t supported_;
  static uint64_t found_by_runtime_probing_only_;

  static uint64_t cross_compile_;

  friend class ExternalReference;
  friend class PlatformFeatureScope;
  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};


#define ASSEMBLER_INSTRUCTION_LIST(V)  \
  V(mov)


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
  static inline Address target_address_at(Address pc);
  static inline void set_target_address_at(Address pc, Address target);

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  static inline Address target_address_from_return_address(Address pc);

  // This sets the branch destination (which is in the instruction on x64).
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Address target) {
    set_target_address_at(instruction_payload, target);
  }

  static inline RelocInfo::Mode RelocInfoNone() {
    if (kPointerSize == kInt64Size) {
      return RelocInfo::NONE64;
    } else {
      ASSERT(kPointerSize == kInt32Size);
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

  // The js return and debug break slot must be able to contain an indirect
  // call sequence, some x64 JS code is padded with int3 to make it large
  // enough to hold an instruction when the debugger patches it.
  static const int kJSReturnSequenceLength = kCallSequenceLength;
  static const int kDebugBreakSlotLength = kCallSequenceLength;
  static const int kPatchDebugBreakSlotReturnOffset = kCallTargetAddressOffset;
  // Distance between the start of the JS return sequence and where the
  // 32-bit displacement of a short call would be. The short call is from
  // SetDebugBreakAtIC from debug-x64.cc.
  static const int kPatchReturnSequenceAddressOffset =
      kJSReturnSequenceLength - kPatchDebugBreakSlotReturnOffset;
  // Distance between the start of the JS return sequence and where the
  // 32-bit displacement of a short call would be. The short call is from
  // SetDebugBreakAtIC from debug-x64.cc.
  static const int kPatchDebugBreakSlotAddressOffset =
      kDebugBreakSlotLength - kPatchDebugBreakSlotReturnOffset;
  static const int kRealPatchReturnSequenceAddressOffset =
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
  //
  // Some mnemonics, such as "and", are the same as C++ keywords.
  // Naming conflicts with C++ keywords are resolved by adding a trailing '_'.

#define DECLARE_INSTRUCTION(instruction)                \
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
  }
  ASSEMBLER_INSTRUCTION_LIST(DECLARE_INSTRUCTION)
#undef DECLARE_INSTRUCTION

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m, where m must be a power of 2.
  void Align(int m);
  void Nop(int bytes = 1);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Stack
  void pushfq();
  void popfq();

  void push(Immediate value);
  // Push a 32 bit integer, and guarantee that it is actually pushed as a
  // 32 bit value, the normal push will optimize the 8 bit case.
  void push_imm32(int32_t imm32);
  void push(Register src);
  void push(const Operand& src);

  void pop(Register dst);
  void pop(const Operand& dst);

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

  void movsxbq(Register dst, const Operand& src);
  void movsxwq(Register dst, const Operand& src);
  void movsxlq(Register dst, Register src);
  void movsxlq(Register dst, const Operand& src);
  void movzxbq(Register dst, const Operand& src);
  void movzxbl(Register dst, const Operand& src);
  void movzxwq(Register dst, const Operand& src);
  void movzxwl(Register dst, const Operand& src);
  void movzxwl(Register dst, Register src);

  // Repeated moves.

  void repmovsb();
  void repmovsw();
  void repmovsl();
  void repmovsq();

  // Instruction to load from an immediate 64-bit pointer into RAX.
  void load_rax(void* ptr, RelocInfo::Mode rmode);
  void load_rax(ExternalReference ext);

  // Conditional moves.
  void cmovq(Condition cc, Register dst, Register src);
  void cmovq(Condition cc, Register dst, const Operand& src);
  void cmovl(Condition cc, Register dst, Register src);
  void cmovl(Condition cc, Register dst, const Operand& src);

  // Exchange two registers
  void xchgq(Register dst, Register src);
  void xchgl(Register dst, Register src);

  // Arithmetics
  void addl(Register dst, Register src) {
    arithmetic_op_32(0x03, dst, src);
  }

  void addl(Register dst, Immediate src) {
    immediate_arithmetic_op_32(0x0, dst, src);
  }

  void addl(Register dst, const Operand& src) {
    arithmetic_op_32(0x03, dst, src);
  }

  void addl(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_32(0x0, dst, src);
  }

  void addl(const Operand& dst, Register src) {
    arithmetic_op_32(0x01, src, dst);
  }

  void addq(Register dst, Register src) {
    arithmetic_op(0x03, dst, src);
  }

  void addq(Register dst, const Operand& src) {
    arithmetic_op(0x03, dst, src);
  }

  void addq(const Operand& dst, Register src) {
    arithmetic_op(0x01, src, dst);
  }

  void addq(Register dst, Immediate src) {
    immediate_arithmetic_op(0x0, dst, src);
  }

  void addq(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x0, dst, src);
  }

  void sbbl(Register dst, Register src) {
    arithmetic_op_32(0x1b, dst, src);
  }

  void sbbq(Register dst, Register src) {
    arithmetic_op(0x1b, dst, src);
  }

  void cmpb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x7, dst, src);
  }

  void cmpb_al(Immediate src);

  void cmpb(Register dst, Register src) {
    arithmetic_op(0x3A, dst, src);
  }

  void cmpb(Register dst, const Operand& src) {
    arithmetic_op(0x3A, dst, src);
  }

  void cmpb(const Operand& dst, Register src) {
    arithmetic_op(0x38, src, dst);
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

  void cmpl(Register dst, Register src) {
    arithmetic_op_32(0x3B, dst, src);
  }

  void cmpl(Register dst, const Operand& src) {
    arithmetic_op_32(0x3B, dst, src);
  }

  void cmpl(const Operand& dst, Register src) {
    arithmetic_op_32(0x39, src, dst);
  }

  void cmpl(Register dst, Immediate src) {
    immediate_arithmetic_op_32(0x7, dst, src);
  }

  void cmpl(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_32(0x7, dst, src);
  }

  void cmpq(Register dst, Register src) {
    arithmetic_op(0x3B, dst, src);
  }

  void cmpq(Register dst, const Operand& src) {
    arithmetic_op(0x3B, dst, src);
  }

  void cmpq(const Operand& dst, Register src) {
    arithmetic_op(0x39, src, dst);
  }

  void cmpq(Register dst, Immediate src) {
    immediate_arithmetic_op(0x7, dst, src);
  }

  void cmpq(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x7, dst, src);
  }

  void and_(Register dst, Register src) {
    arithmetic_op(0x23, dst, src);
  }

  void and_(Register dst, const Operand& src) {
    arithmetic_op(0x23, dst, src);
  }

  void and_(const Operand& dst, Register src) {
    arithmetic_op(0x21, src, dst);
  }

  void and_(Register dst, Immediate src) {
    immediate_arithmetic_op(0x4, dst, src);
  }

  void and_(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x4, dst, src);
  }

  void andl(Register dst, Immediate src) {
    immediate_arithmetic_op_32(0x4, dst, src);
  }

  void andl(Register dst, Register src) {
    arithmetic_op_32(0x23, dst, src);
  }

  void andl(Register dst, const Operand& src) {
    arithmetic_op_32(0x23, dst, src);
  }

  void andb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x4, dst, src);
  }

  void decq(Register dst);
  void decq(const Operand& dst);
  void decl(Register dst);
  void decl(const Operand& dst);
  void decb(Register dst);
  void decb(const Operand& dst);

  // Sign-extends rax into rdx:rax.
  void cqo();
  // Sign-extends eax into edx:eax.
  void cdq();

  // Divide rdx:rax by src.  Quotient in rax, remainder in rdx.
  void idivq(Register src);
  // Divide edx:eax by lower 32 bits of src.  Quotient in eax, rem. in edx.
  void idivl(Register src);

  // Signed multiply instructions.
  void imul(Register src);                               // rdx:rax = rax * src.
  void imul(Register dst, Register src);                 // dst = dst * src.
  void imul(Register dst, const Operand& src);           // dst = dst * src.
  void imul(Register dst, Register src, Immediate imm);  // dst = src * imm.
  // Signed 32-bit multiply instructions.
  void imull(Register dst, Register src);                 // dst = dst * src.
  void imull(Register dst, const Operand& src);           // dst = dst * src.
  void imull(Register dst, Register src, Immediate imm);  // dst = src * imm.

  void incq(Register dst);
  void incq(const Operand& dst);
  void incl(Register dst);
  void incl(const Operand& dst);

  void lea(Register dst, const Operand& src);
  void leal(Register dst, const Operand& src);

  // Multiply rax by src, put the result in rdx:rax.
  void mul(Register src);

  void neg(Register dst);
  void neg(const Operand& dst);
  void negl(Register dst);

  void not_(Register dst);
  void not_(const Operand& dst);
  void notl(Register dst);

  void or_(Register dst, Register src) {
    arithmetic_op(0x0B, dst, src);
  }

  void orl(Register dst, Register src) {
    arithmetic_op_32(0x0B, dst, src);
  }

  void or_(Register dst, const Operand& src) {
    arithmetic_op(0x0B, dst, src);
  }

  void orl(Register dst, const Operand& src) {
    arithmetic_op_32(0x0B, dst, src);
  }

  void or_(const Operand& dst, Register src) {
    arithmetic_op(0x09, src, dst);
  }

  void orl(const Operand& dst, Register src) {
    arithmetic_op_32(0x09, src, dst);
  }

  void or_(Register dst, Immediate src) {
    immediate_arithmetic_op(0x1, dst, src);
  }

  void orl(Register dst, Immediate src) {
    immediate_arithmetic_op_32(0x1, dst, src);
  }

  void or_(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x1, dst, src);
  }

  void orl(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_32(0x1, dst, src);
  }

  void rcl(Register dst, Immediate imm8) {
    shift(dst, imm8, 0x2);
  }

  void rol(Register dst, Immediate imm8) {
    shift(dst, imm8, 0x0);
  }

  void roll(Register dst, Immediate imm8) {
    shift_32(dst, imm8, 0x0);
  }

  void rcr(Register dst, Immediate imm8) {
    shift(dst, imm8, 0x3);
  }

  void ror(Register dst, Immediate imm8) {
    shift(dst, imm8, 0x1);
  }

  void rorl(Register dst, Immediate imm8) {
    shift_32(dst, imm8, 0x1);
  }

  void rorl_cl(Register dst) {
    shift_32(dst, 0x1);
  }

  // Shifts dst:src left by cl bits, affecting only dst.
  void shld(Register dst, Register src);

  // Shifts src:dst right by cl bits, affecting only dst.
  void shrd(Register dst, Register src);

  // Shifts dst right, duplicating sign bit, by shift_amount bits.
  // Shifting by 1 is handled efficiently.
  void sar(Register dst, Immediate shift_amount) {
    shift(dst, shift_amount, 0x7);
  }

  // Shifts dst right, duplicating sign bit, by shift_amount bits.
  // Shifting by 1 is handled efficiently.
  void sarl(Register dst, Immediate shift_amount) {
    shift_32(dst, shift_amount, 0x7);
  }

  // Shifts dst right, duplicating sign bit, by cl % 64 bits.
  void sar_cl(Register dst) {
    shift(dst, 0x7);
  }

  // Shifts dst right, duplicating sign bit, by cl % 64 bits.
  void sarl_cl(Register dst) {
    shift_32(dst, 0x7);
  }

  void shl(Register dst, Immediate shift_amount) {
    shift(dst, shift_amount, 0x4);
  }

  void shl_cl(Register dst) {
    shift(dst, 0x4);
  }

  void shll_cl(Register dst) {
    shift_32(dst, 0x4);
  }

  void shll(Register dst, Immediate shift_amount) {
    shift_32(dst, shift_amount, 0x4);
  }

  void shr(Register dst, Immediate shift_amount) {
    shift(dst, shift_amount, 0x5);
  }

  void shr_cl(Register dst) {
    shift(dst, 0x5);
  }

  void shrl_cl(Register dst) {
    shift_32(dst, 0x5);
  }

  void shrl(Register dst, Immediate shift_amount) {
    shift_32(dst, shift_amount, 0x5);
  }

  void store_rax(void* dst, RelocInfo::Mode mode);
  void store_rax(ExternalReference ref);

  void subq(Register dst, Register src) {
    arithmetic_op(0x2B, dst, src);
  }

  void subq(Register dst, const Operand& src) {
    arithmetic_op(0x2B, dst, src);
  }

  void subq(const Operand& dst, Register src) {
    arithmetic_op(0x29, src, dst);
  }

  void subq(Register dst, Immediate src) {
    immediate_arithmetic_op(0x5, dst, src);
  }

  void subq(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x5, dst, src);
  }

  void subl(Register dst, Register src) {
    arithmetic_op_32(0x2B, dst, src);
  }

  void subl(Register dst, const Operand& src) {
    arithmetic_op_32(0x2B, dst, src);
  }

  void subl(const Operand& dst, Register src) {
    arithmetic_op_32(0x29, src, dst);
  }

  void subl(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_32(0x5, dst, src);
  }

  void subl(Register dst, Immediate src) {
    immediate_arithmetic_op_32(0x5, dst, src);
  }

  void subb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x5, dst, src);
  }

  void testb(Register dst, Register src);
  void testb(Register reg, Immediate mask);
  void testb(const Operand& op, Immediate mask);
  void testb(const Operand& op, Register reg);
  void testl(Register dst, Register src);
  void testl(Register reg, Immediate mask);
  void testl(const Operand& op, Register reg);
  void testl(const Operand& op, Immediate mask);
  void testq(const Operand& op, Register reg);
  void testq(Register dst, Register src);
  void testq(Register dst, Immediate mask);

  void xor_(Register dst, Register src) {
    if (dst.code() == src.code()) {
      arithmetic_op_32(0x33, dst, src);
    } else {
      arithmetic_op(0x33, dst, src);
    }
  }

  void xorl(Register dst, Register src) {
    arithmetic_op_32(0x33, dst, src);
  }

  void xorl(Register dst, const Operand& src) {
    arithmetic_op_32(0x33, dst, src);
  }

  void xorl(Register dst, Immediate src) {
    immediate_arithmetic_op_32(0x6, dst, src);
  }

  void xorl(const Operand& dst, Register src) {
    arithmetic_op_32(0x31, src, dst);
  }

  void xorl(const Operand& dst, Immediate src) {
    immediate_arithmetic_op_32(0x6, dst, src);
  }

  void xor_(Register dst, const Operand& src) {
    arithmetic_op(0x33, dst, src);
  }

  void xor_(const Operand& dst, Register src) {
    arithmetic_op(0x31, src, dst);
  }

  void xor_(Register dst, Immediate src) {
    immediate_arithmetic_op(0x6, dst, src);
  }

  void xor_(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x6, dst, src);
  }

  // Bit operations.
  void bt(const Operand& dst, Register src);
  void bts(const Operand& dst, Register src);

  // Miscellaneous
  void clc();
  void cld();
  void cpuid();
  void hlt();
  void int3();
  void nop();
  void ret(int imm16);
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
  void movaps(XMMRegister dst, XMMRegister src);
  void movss(XMMRegister dst, const Operand& src);
  void movss(const Operand& dst, XMMRegister src);
  void shufps(XMMRegister dst, XMMRegister src, byte imm8);

  void cvttss2si(Register dst, const Operand& src);
  void cvttss2si(Register dst, XMMRegister src);
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
  void movd(Register dst, XMMRegister src);
  void movq(XMMRegister dst, Register src);
  void movq(Register dst, XMMRegister src);
  void movq(XMMRegister dst, XMMRegister src);

  // Don't use this unless it's important to keep the
  // top half of the destination register unchanged.
  // Used movaps when moving double values and movq for integer
  // values in xmm registers.
  void movsd(XMMRegister dst, XMMRegister src);

  void movsd(const Operand& dst, XMMRegister src);
  void movsd(XMMRegister dst, const Operand& src);

  void movdqa(const Operand& dst, XMMRegister src);
  void movdqa(XMMRegister dst, const Operand& src);

  void movdqu(const Operand& dst, XMMRegister src);
  void movdqu(XMMRegister dst, const Operand& src);

  void movapd(XMMRegister dst, XMMRegister src);

  void cvttsd2si(Register dst, const Operand& src);
  void cvttsd2si(Register dst, XMMRegister src);
  void cvttsd2siq(Register dst, XMMRegister src);

  void cvtlsi2sd(XMMRegister dst, const Operand& src);
  void cvtlsi2sd(XMMRegister dst, Register src);
  void cvtqsi2sd(XMMRegister dst, const Operand& src);
  void cvtqsi2sd(XMMRegister dst, Register src);


  void cvtss2sd(XMMRegister dst, XMMRegister src);
  void cvtss2sd(XMMRegister dst, const Operand& src);
  void cvtsd2ss(XMMRegister dst, XMMRegister src);

  void cvtsd2si(Register dst, XMMRegister src);
  void cvtsd2siq(Register dst, XMMRegister src);

  void addsd(XMMRegister dst, XMMRegister src);
  void addsd(XMMRegister dst, const Operand& src);
  void subsd(XMMRegister dst, XMMRegister src);
  void mulsd(XMMRegister dst, XMMRegister src);
  void mulsd(XMMRegister dst, const Operand& src);
  void divsd(XMMRegister dst, XMMRegister src);

  void andpd(XMMRegister dst, XMMRegister src);
  void orpd(XMMRegister dst, XMMRegister src);
  void xorpd(XMMRegister dst, XMMRegister src);
  void sqrtsd(XMMRegister dst, XMMRegister src);

  void ucomisd(XMMRegister dst, XMMRegister src);
  void ucomisd(XMMRegister dst, const Operand& src);
  void cmpltsd(XMMRegister dst, XMMRegister src);

  void movmskpd(Register dst, XMMRegister src);

  // SSE 4.1 instruction
  void extractps(Register dst, XMMRegister src, byte imm8);

  enum RoundingMode {
    kRoundToNearest = 0x0,
    kRoundDown      = 0x1,
    kRoundUp        = 0x2,
    kRoundToZero    = 0x3
  };

  void roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode);

  // Debugging
  void Print();

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot();

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg, bool force = false);

  // Writes a single word of data in the code stream.
  // Used for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);

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

  // Jump near absolute indirect (m64)
  void jmp(const Operand& src);

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

  // Optionally do as emit_rex_32(const Operand&) if the operand register
  // numbers have a high bit set.
  inline void emit_optional_rex_32(const Operand& op);

  template<class P1>
  void emit_rex(P1 p1, int size) {
    if (size == kInt64Size) {
      emit_rex_64(p1);
    } else {
      ASSERT(size == kInt32Size);
      emit_optional_rex_32(p1);
    }
  }

  template<class P1, class P2>
  void emit_rex(P1 p1, P2 p2, int size) {
    if (size == kInt64Size) {
      emit_rex_64(p1, p2);
    } else {
      ASSERT(size == kInt32Size);
      emit_optional_rex_32(p1, p2);
    }
  }

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
    ASSERT(is_uint3(code));
    emit(0xC0 | code << 3 | rm_reg.low_bits());
  }

  // Emit the code-object-relative offset of the label's position
  inline void emit_code_relative_offset(Label* label);

  // The first argument is the reg field, the second argument is the r/m field.
  void emit_sse_operand(XMMRegister dst, XMMRegister src);
  void emit_sse_operand(XMMRegister reg, const Operand& adr);
  void emit_sse_operand(XMMRegister dst, Register src);
  void emit_sse_operand(Register dst, XMMRegister src);

  // Emit machine code for one of the operations ADD, ADC, SUB, SBC,
  // AND, OR, XOR, or CMP.  The encodings of these operations are all
  // similar, differing just in the opcode or in the reg field of the
  // ModR/M byte.
  void arithmetic_op_16(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op_16(byte opcode, Register reg, const Operand& rm_reg);
  void arithmetic_op_32(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op_32(byte opcode, Register reg, const Operand& rm_reg);
  void arithmetic_op(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op(byte opcode, Register reg, const Operand& rm_reg);
  void immediate_arithmetic_op(byte subcode, Register dst, Immediate src);
  void immediate_arithmetic_op(byte subcode, const Operand& dst, Immediate src);
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
  // Operate on a 32-bit word in memory or register.
  void immediate_arithmetic_op_32(byte subcode,
                                  Register dst,
                                  Immediate src);
  void immediate_arithmetic_op_32(byte subcode,
                                  const Operand& dst,
                                  Immediate src);

  // Emit machine code for a shift operation.
  void shift(Register dst, Immediate shift_amount, int subcode);
  void shift_32(Register dst, Immediate shift_amount, int subcode);
  // Shift dst by cl % 64 bits.
  void shift(Register dst, int subcode);
  void shift_32(Register dst, int subcode);

  void emit_farith(int b1, int b2, int i);

  // labels
  // void print(Label* L);
  void bind_to(Label* L, int pos);

  // record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  void emit_mov(Register dst, const Operand& src, int size);
  void emit_mov(Register dst, Register src, int size);
  void emit_mov(const Operand& dst, Register src, int size);
  void emit_mov(Register dst, Immediate value, int size);
  void emit_mov(const Operand& dst, Immediate value, int size);

  friend class CodePatcher;
  friend class EnsureSpace;
  friend class RegExpMacroAssemblerX64;

  // code generation
  RelocInfoWriter reloc_info_writer;

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
    ASSERT(bytes_generated < assembler_->kGap);
  }
#endif

 private:
  Assembler* assembler_;
#ifdef DEBUG
  int space_before_;
#endif
};

} }  // namespace v8::internal

#endif  // V8_X64_ASSEMBLER_X64_H_

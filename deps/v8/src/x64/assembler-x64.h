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
// Copyright 2006-2009 the V8 project authors. All rights reserved.

// A lightweight X64 Assembler.

#ifndef V8_X64_ASSEMBLER_X64_H_
#define V8_X64_ASSEMBLER_X64_H_

namespace v8 {
namespace internal {

// Utility functions

// Test whether a 64-bit value is in a specific range.
static inline bool is_uint32(int64_t x) {
  const int64_t kUInt32Mask = V8_INT64_C(0xffffffff);
  return x == x & kUInt32Mask;
}

static inline bool is_int32(int64_t x) {
  const int64_t kMinIntValue = V8_INT64_C(-0x80000000);
  return is_uint32(x - kMinIntValue);
}

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
  static Register toRegister(int code) {
    Register r = {code};
    return r;
  }
  bool is_valid() const  { return 0 <= code_ && code_ < 16; }
  bool is(Register reg) const  { return code_ == reg.code_; }
  // The byte-register distinction of ai32 has dissapeared.
  bool is_byte_register() const  { return false; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }
  int bit() const  {
    UNIMPLEMENTED();
    return 0;
  }

  // (unfortunately we can't make this private in a struct)
  int code_;
};

extern Register rax;
extern Register rcx;
extern Register rdx;
extern Register rbx;
extern Register rsp;
extern Register rbp;
extern Register rsi;
extern Register rdi;
extern Register r8;
extern Register r9;
extern Register r10;
extern Register r11;
extern Register r12;
extern Register r13;
extern Register r14;
extern Register r15;
extern Register no_reg;

struct XMMRegister {
  bool is_valid() const  { return 0 <= code_ && code_ < 2; }
  int code() const  {
    ASSERT(is_valid());
    return code_;
  }

  int code_;
};

extern XMMRegister xmm0;
extern XMMRegister xmm1;
extern XMMRegister xmm2;
extern XMMRegister xmm3;
extern XMMRegister xmm4;
extern XMMRegister xmm5;
extern XMMRegister xmm6;
extern XMMRegister xmm7;
extern XMMRegister xmm8;
extern XMMRegister xmm9;
extern XMMRegister xmm10;
extern XMMRegister xmm11;
extern XMMRegister xmm12;
extern XMMRegister xmm13;
extern XMMRegister xmm14;
extern XMMRegister xmm15;

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

  // aliases
  carry         = below,
  not_carry     = above_equal,
  zero          = equal,
  not_zero      = not_equal,
  sign          = negative,
  not_sign      = positive
};


// Returns the equivalent of !cc.
// Negation of the default no_condition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc);

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

enum Hint {
  no_hint = 0,
  not_taken = 0x2e,
  taken = 0x3e
};

// The result of negating a hint is as if the corresponding condition
// were negated by NegateCondition.  That is, no_hint is mapped to
// itself and not_taken and taken are mapped to each other.
inline Hint NegateHint(Hint hint) {
  return (hint == no_hint)
      ? no_hint
      : ((hint == not_taken) ? taken : not_taken);
}


// -----------------------------------------------------------------------------
// Machine instruction Immediates

class Immediate BASE_EMBEDDED {
 public:
  explicit Immediate(int32_t value) : value_(value) {}
  inline explicit Immediate(Smi* value);

 private:
  int32_t value_;

  friend class Assembler;
};


// -----------------------------------------------------------------------------
// Machine instruction Operands

enum ScaleFactor {
  kTimes1 = 0,
  kTimes2 = 1,
  kTimes4 = 2,
  kTimes8 = 3,
  kTimesIntSize = kTimes4,
  kTimesPointerSize = kTimes8
};


class Operand BASE_EMBEDDED {
 public:
  // [base + disp/r]
  INLINE(Operand(Register base, int32_t disp));

  // [base + index*scale + disp/r]
  Operand(Register base,
          Register index,
          ScaleFactor scale,
          int32_t disp);

  // [index*scale + disp/r]
  Operand(Register index,
          ScaleFactor scale,
          int32_t disp);

 private:
  byte rex_;
  byte buf_[10];
  // The number of bytes in buf_.
  unsigned int len_;
  RelocInfo::Mode rmode_;

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
// Supported features must be enabled by a Scope before use.
// Example:
//   if (CpuFeatures::IsSupported(SSE2)) {
//     CpuFeatures::Scope fscope(SSE2);
//     // Generate SSE2 floating point code.
//   } else {
//     // Generate standard x87 floating point code.
//   }
class CpuFeatures : public AllStatic {
 public:
  // Feature flags bit positions. They are mostly based on the CPUID spec.
  // (We assign CPUID itself to one of the currently reserved bits --
  // feel free to change this if needed.)
  enum Feature { SSE3 = 32, SSE2 = 26, CMOV = 15, RDTSC = 4, CPUID = 10 };
  // Detect features of the target CPU. Set safe defaults if the serializer
  // is enabled (snapshots must be portable).
  static void Probe();
  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(Feature f) {
    return (supported_ & (V8_UINT64_C(1) << f)) != 0;
  }
  // Check whether a feature is currently enabled.
  static bool IsEnabled(Feature f) {
    return (enabled_ & (V8_UINT64_C(1) << f)) != 0;
  }
  // Enable a specified feature within a scope.
  class Scope BASE_EMBEDDED {
#ifdef DEBUG
   public:
    explicit Scope(Feature f) {
      ASSERT(CpuFeatures::IsSupported(f));
      old_enabled_ = CpuFeatures::enabled_;
      CpuFeatures::enabled_ |= (V8_UINT64_C(1) << f);
    }
    ~Scope() { CpuFeatures::enabled_ = old_enabled_; }
   private:
    uint64_t old_enabled_;
#else
   public:
    explicit Scope(Feature f) {}
#endif
  };
 private:
  static uint64_t supported_;
  static uint64_t enabled_;
};


class Assembler : public Malloced {
 private:
  // The relocation writer's position is kGap bytes below the end of
  // the generated instructions. This leaves enough space for the
  // longest possible x64 instruction (There is a 15 byte limit on
  // instruction length, ruling out some otherwise valid instructions) and
  // allows for a single, fast space check per instruction.
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
  Assembler(void* buffer, int buffer_size);
  ~Assembler();

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

  // Read/Modify the code target in the branch/call instruction at pc.
  // On the x64 architecture, the address is absolute, not relative.
  static inline Address target_address_at(Address pc);
  static inline void set_target_address_at(Address pc, Address target);

  // Distance between the address of the code target in the call instruction
  // and the return address
  static const int kTargetAddrToReturnAddrDist = kPointerSize;


  // ---------------------------------------------------------------------------
  // Code generation
  //
  // Function names correspond one-to-one to x64 instruction mnemonics.
  // Unless specified otherwise, instructions operate on 64-bit operands.
  //
  // If we need versions of an assembly instruction that operate on different
  // width arguments, we add a single-letter suffix specifying the width.
  // This is done for the following instructions: mov, cmp.
  // There are no versions of these instructions without the suffix.
  // - Instructions on 8-bit (byte) operands/registers have a trailing 'b'.
  // - Instructions on 16-bit (word) operands/registers have a trailing 'w'.
  // - Instructions on 32-bit (doubleword) operands/registers use 'l'.
  // - Instructions on 64-bit (quadword) operands/registers use 'q'.
  //
  // Some mnemonics, such as "and", are the same as C++ keywords.
  // Naming conflicts with C++ keywords are resolved by adding a trailing '_'.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2.
  void Align(int m);

  // Stack
  void pushfq();
  void popfq();

  void push(Immediate value);
  void push(Register src);
  void push(const Operand& src);
  void push(Label* label, RelocInfo::Mode relocation_mode);

  void pop(Register dst);
  void pop(const Operand& dst);

  void enter(Immediate size);
  void leave();

  // Moves
  void movb(Register dst, const Operand& src);
  void movb(Register dst, Immediate imm);
  void movb(const Operand& dst, Register src);

  void movl(Register dst, Register src);
  void movl(Register dst, const Operand& src);
  void movl(const Operand& dst, Register src);
  // Load a 32-bit immediate value, zero-extended to 64 bits.
  void movl(Register dst, Immediate imm32);

  void movq(Register dst, int32_t imm32);
  void movq(Register dst, const Operand& src);
  // Sign extends immediate 32-bit value to 64 bits.
  void movq(Register dst, Immediate x);
  void movq(Register dst, Register src);

  // Move 64 bit register value to 64-bit memory location.
  void movq(const Operand& dst, Register src);

  // New x64 instructions to load a 64-bit immediate into a register.
  // All 64-bit immediates must have a relocation mode.
  void movq(Register dst, void* ptr, RelocInfo::Mode rmode);
  void movq(Register dst, int64_t value, RelocInfo::Mode rmode);
  void movq(Register dst, const char* s, RelocInfo::Mode rmode);
  // Moves the address of the external reference into the register.
  void movq(Register dst, ExternalReference ext);
  void movq(Register dst, Handle<Object> handle, RelocInfo::Mode rmode);


  // New x64 instruction to load from an immediate 64-bit pointer into RAX.
  void load_rax(void* ptr, RelocInfo::Mode rmode);
  void load_rax(ExternalReference ext);

  void movsx_b(Register dst, const Operand& src);

  void movsx_w(Register dst, const Operand& src);

  void movzx_b(Register dst, const Operand& src);

  void movzx_w(Register dst, const Operand& src);

  // Conditional moves
  void cmov(Condition cc, Register dst, int32_t imm32);
  void cmov(Condition cc, Register dst, Handle<Object> handle);
  void cmov(Condition cc, Register dst, const Operand& src);

  // Exchange two registers
  void xchg(Register dst, Register src);

  // Arithmetics
  void add(Register dst, Register src) {
    arithmetic_op(0x03, dst, src);
  }

  void add(Register dst, const Operand& src) {
    arithmetic_op(0x03, dst, src);
  }


  void add(const Operand& dst, Register src) {
    arithmetic_op(0x01, src, dst);
  }

  void add(Register dst, Immediate src) {
    immediate_arithmetic_op(0x0, dst, src);
  }

  void add(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x0, dst, src);
  }

  void cmp(Register dst, Register src) {
    arithmetic_op(0x3B, dst, src);
  }

  void cmp(Register dst, const Operand& src) {
    arithmetic_op(0x3B, dst, src);
  }

  void cmp(const Operand& dst, Register src) {
    arithmetic_op(0x39, src, dst);
  }

  void cmp(Register dst, Immediate src) {
    immediate_arithmetic_op(0x7, dst, src);
  }

  void cmp(const Operand& dst, Immediate src) {
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

  void cmpb(const Operand& op, int8_t imm8);
  void cmpb_al(const Operand& op);
  void cmpw_ax(const Operand& op);
  void cmpw(const Operand& op, Immediate imm16);

  void dec_b(Register dst);

  void dec(Register dst);
  void dec(const Operand& dst);

  // Sign-extends rax into rdx:rax.
  void cqo();

  // Divide rdx:rax by src.  Quotient in rax, remainder in rdx.
  void idiv(Register src);

  void imul(Register dst, Register src);
  void imul(Register dst, const Operand& src);
  // Performs the operation dst = src * imm.
  void imul(Register dst, Register src, Immediate imm);

  void inc(Register dst);
  void inc(const Operand& dst);

  void lea(Register dst, const Operand& src);

  // Multiply rax by src, put the result in rdx:rax.
  void mul(Register src);

  void neg(Register dst);
  void neg(const Operand& dst);

  void not_(Register dst);
  void not_(const Operand& dst);

  void or_(Register dst, Register src) {
    arithmetic_op(0x0B, dst, src);
  }

  void or_(Register dst, const Operand& src) {
    arithmetic_op(0x0B, dst, src);
  }

  void or_(const Operand& dst, Register src) {
    arithmetic_op(0x09, src, dst);
  }

  void or_(Register dst, Immediate src) {
    immediate_arithmetic_op(0x1, dst, src);
  }

  void or_(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x1, dst, src);
  }


  void rcl(Register dst, uint8_t imm8);

  // Shifts dst:src left by cl bits, affecting only dst.
  void shld(Register dst, Register src);

  // Shifts src:dst right by cl bits, affecting only dst.
  void shrd(Register dst, Register src);

  // Shifts dst right, duplicating sign bit, by shift_amount bits.
  // Shifting by 1 is handled efficiently.
  void sar(Register dst, Immediate shift_amount) {
    shift(dst, shift_amount, 0x7);
  }

  // Shifts dst right, duplicating sign bit, by cl % 64 bits.
  void sar(Register dst) {
    shift(dst, 0x7);
  }

  void shl(Register dst, Immediate shift_amount) {
    shift(dst, shift_amount, 0x4);
  }

  void shl(Register dst) {
    shift(dst, 0x4);
  }

  void shr(Register dst, Immediate shift_amount) {
    shift(dst, shift_amount, 0x5);
  }

  void shr(Register dst) {
    shift(dst, 0x5);
  }

  void store_rax(void* dst, RelocInfo::Mode mode);
  void store_rax(ExternalReference ref);

  void sub(Register dst, Register src) {
    arithmetic_op(0x2B, dst, src);
  }

  void sub(Register dst, const Operand& src) {
    arithmetic_op(0x2B, dst, src);
  }

  void sub(const Operand& dst, Register src) {
    arithmetic_op(0x29, src, dst);
  }

  void sub(Register dst, Immediate src) {
    immediate_arithmetic_op(0x5, dst, src);
  }

  void sub(const Operand& dst, Immediate src) {
    immediate_arithmetic_op(0x5, dst, src);
  }

  void testb(Register reg, Immediate mask);
  void testb(const Operand& op, Immediate mask);
  void testl(Register reg, Immediate mask);
  void testl(const Operand& op, Immediate mask);
  void testq(const Operand& op, Register reg);
  void testq(Register dst, Register src);

  void xor_(Register dst, Register src) {
    arithmetic_op(0x33, dst, src);
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
  void hlt();
  void int3();
  void nop();
  void nop(int n);
  void rdtsc();
  void ret(int imm16);

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

  // Call near absolute indirect, address in register
  void call(Register adr);

  // Call near indirect
  void call(const Operand& operand);

  // Jumps
  // Jump short or near relative.
  void jmp(Label* L);  // unconditional jump to L

  // Jump near absolute indirect (r64)
  void jmp(Register adr);

  // Conditional jumps
  void j(Condition cc, Label* L);
  void j(Condition cc, byte* entry, RelocInfo::Mode rmode);
  void j(Condition cc, Handle<Code> code);

  // Floating-point operations
  void fld(int i);

  void fld1();
  void fldz();

  void fld_s(const Operand& adr);
  void fld_d(const Operand& adr);

  void fstp_s(const Operand& adr);
  void fstp_d(const Operand& adr);

  void fild_s(const Operand& adr);
  void fild_d(const Operand& adr);

  void fist_s(const Operand& adr);

  void fistp_s(const Operand& adr);
  void fistp_d(const Operand& adr);

  void fisttp_s(const Operand& adr);

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
  void fcompp();
  void fnstsw_ax();
  void fwait();
  void fnclex();

  void frndint();

  void sahf();
  void setcc(Condition cc, Register reg);

  void cpuid();

  // SSE2 instructions
  void cvttss2si(Register dst, const Operand& src);
  void cvttsd2si(Register dst, const Operand& src);

  void cvtsi2sd(XMMRegister dst, const Operand& src);

  void addsd(XMMRegister dst, XMMRegister src);
  void subsd(XMMRegister dst, XMMRegister src);
  void mulsd(XMMRegister dst, XMMRegister src);
  void divsd(XMMRegister dst, XMMRegister src);

  // Use either movsd or movlpd.
  void movdbl(XMMRegister dst, const Operand& src);
  void movdbl(const Operand& dst, XMMRegister src);

  // Debugging
  void Print();

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* l) { return pc_offset() - l->pos(); }

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --debug_code to enable.
  void RecordComment(const char* msg);

  void RecordPosition(int pos);
  void RecordStatementPosition(int pos);
  void WriteRecordedPositions();

  // Writes a doubleword of data in the code stream.
  // Used for inline tables, e.g., jump-tables.
  void dd(uint32_t data);

  // Writes a quadword of data in the code stream.
  // Used for inline tables, e.g., jump-tables.
  void dd(uint64_t data, RelocInfo::Mode reloc_info);

  // Writes the absolute address of a bound label at the given position in
  // the generated code. That positions should have the relocation mode
  // internal_reference!
  void WriteInternalReference(int position, const Label& bound_label);

  int pc_offset() const  { return pc_ - buffer_; }
  int current_statement_position() const { return current_statement_position_; }
  int current_position() const  { return current_position_; }

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool overflow() const { return pc_ >= reloc_info_writer.pos() - kGap; }

  // Get the number of bytes available in the buffer.
  inline int available_space() const { return reloc_info_writer.pos() - pc_; }

  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512*MB;
  static const int kMinimalBufferSize = 4*KB;

 protected:
  void movsd(XMMRegister dst, const Operand& src);
  void movsd(const Operand& dst, XMMRegister src);

  void emit_sse_operand(XMMRegister reg, const Operand& adr);
  void emit_sse_operand(XMMRegister dst, XMMRegister src);


 private:
  byte* addr_at(int pos)  { return buffer_ + pos; }
  byte byte_at(int pos)  { return buffer_[pos]; }
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
  inline void emit(Handle<Object> handle);
  inline void emitq(uint64_t x, RelocInfo::Mode rmode);
  inline void emitw(uint16_t x);
  void emit(Immediate x) { emitl(x.value_); }

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of both register codes.
  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is set.
  inline void emit_rex_64(Register reg, Register rm_reg);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the destination, index, and base register codes.
  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is set.
  inline void emit_rex_64(Register reg, const Operand& op);

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

  // Optionally do as emit_rex_32(Register) if the register number has
  // the high bit set.
  inline void emit_optional_rex_32(Register rm_reg);

  // Optionally do as emit_rex_32(const Operand&) if the operand register
  // numbers have a high bit set.
  inline void emit_optional_rex_32(const Operand& op);


  // Emit the ModR/M byte, and optionally the SIB byte and
  // 1- or 4-byte offset for a memory operand.  Also encodes
  // the second operand of the operation, a register or operation
  // subcode, into the reg field of the ModR/M byte.
  void emit_operand(Register reg, const Operand& adr) {
    emit_operand(reg.code() & 0x07, adr);
  }

  // Emit the ModR/M byte, and optionally the SIB byte and
  // 1- or 4-byte offset for a memory operand.  Also used to encode
  // a three-bit opcode extension into the ModR/M byte.
  void emit_operand(int rm, const Operand& adr);

  // Emit a ModR/M byte with registers coded in the reg and rm_reg fields.
  void emit_modrm(Register reg, Register rm_reg) {
    emit(0xC0 | (reg.code() & 0x7) << 3 | (rm_reg.code() & 0x7));
  }

  // Emit a ModR/M byte with an operation subcode in the reg field and
  // a register in the rm_reg field.
  void emit_modrm(int code, Register rm_reg) {
    ASSERT((code & ~0x7) == 0);
    emit(0xC0 | (code & 0x7) << 3 | (rm_reg.code() & 0x7));
  }

  // Emit the code-object-relative offset of the label's position
  inline void emit_code_relative_offset(Label* label);

  // Emit machine code for one of the operations ADD, ADC, SUB, SBC,
  // AND, OR, XOR, or CMP.  The encodings of these operations are all
  // similar, differing just in the opcode or in the reg field of the
  // ModR/M byte.
  void arithmetic_op(byte opcode, Register dst, Register src);
  void arithmetic_op(byte opcode, Register reg, const Operand& op);
  void immediate_arithmetic_op(byte subcode, Register dst, Immediate src);
  void immediate_arithmetic_op(byte subcode, const Operand& dst, Immediate src);
  // Emit machine code for a shift operation.
  void shift(Register dst, Immediate shift_amount, int subcode);
  // Shift dst by cl % 64 bits.
  void shift(Register dst, int subcode);

  void emit_farith(int b1, int b2, int i);

  // labels
  void print(Label* L);
  void bind_to(Label* L, int pos);
  void link_to(Label* L, Label* appendix);

  // record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  friend class CodePatcher;
  friend class EnsureSpace;

  // Code buffer:
  // The buffer into which code and relocation info are generated.
  byte* buffer_;
  int buffer_size_;
  // True if the assembler owns the buffer, false if buffer is external.
  bool own_buffer_;
  // A previously allocated buffer of kMinimalBufferSize bytes, or NULL.
  static byte* spare_buffer_;

  // code generation
  byte* pc_;  // the program counter; moves forward
  RelocInfoWriter reloc_info_writer;

  // push-pop elimination
  byte* last_pc_;

  // source position information
  int current_statement_position_;
  int current_position_;
  int written_statement_position_;
  int written_position_;
};


// Helper class that ensures that there is enough space for generating
// instructions and relocation information.  The constructor makes
// sure that there is enough space and (in debug mode) the destructor
// checks that we did not generate too much.
class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) : assembler_(assembler) {
    if (assembler_->overflow()) assembler_->GrowBuffer();
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

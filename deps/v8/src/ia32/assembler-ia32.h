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
// Copyright 2011 the V8 project authors. All rights reserved.

// A light-weight IA32 Assembler.

#ifndef V8_IA32_ASSEMBLER_IA32_H_
#define V8_IA32_ASSEMBLER_IA32_H_

#include "isolate.h"
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
struct Register {
  static const int kNumAllocatableRegisters = 6;
  static const int kNumRegisters = 8;

  static inline const char* AllocationIndexToString(int index);

  static inline int ToAllocationIndex(Register reg);

  static inline Register FromAllocationIndex(int index);

  static Register from_code(int code) {
    Register r = { code };
    return r;
  }
  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(Register reg) const { return code_ == reg.code_; }
  // eax, ebx, ecx and edx are byte registers, the rest are not.
  bool is_byte_register() const { return code_ <= 3; }
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


const Register eax = { 0 };
const Register ecx = { 1 };
const Register edx = { 2 };
const Register ebx = { 3 };
const Register esp = { 4 };
const Register ebp = { 5 };
const Register esi = { 6 };
const Register edi = { 7 };
const Register no_reg = { -1 };


inline const char* Register::AllocationIndexToString(int index) {
  ASSERT(index >= 0 && index < kNumAllocatableRegisters);
  // This is the mapping of allocation indices to registers.
  const char* const kNames[] = { "eax", "ecx", "edx", "ebx", "esi", "edi" };
  return kNames[index];
}


inline int Register::ToAllocationIndex(Register reg) {
  ASSERT(reg.is_valid() && !reg.is(esp) && !reg.is(ebp));
  return (reg.code() >= 6) ? reg.code() - 2 : reg.code();
}


inline Register Register::FromAllocationIndex(int index)  {
  ASSERT(index >= 0 && index < kNumAllocatableRegisters);
  return (index >= 4) ? from_code(index + 2) : from_code(index);
}


struct XMMRegister {
  static const int kNumAllocatableRegisters = 7;
  static const int kNumRegisters = 8;

  static int ToAllocationIndex(XMMRegister reg) {
    ASSERT(reg.code() != 0);
    return reg.code() - 1;
  }

  static XMMRegister FromAllocationIndex(int index) {
    ASSERT(index >= 0 && index < kNumAllocatableRegisters);
    return from_code(index + 1);
  }

  static const char* AllocationIndexToString(int index) {
    ASSERT(index >= 0 && index < kNumAllocatableRegisters);
    const char* const names[] = {
      "xmm1",
      "xmm2",
      "xmm3",
      "xmm4",
      "xmm5",
      "xmm6",
      "xmm7"
    };
    return names[index];
  }

  static XMMRegister from_code(int code) {
    XMMRegister r = { code };
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(XMMRegister reg) const { return code_ == reg.code_; }
  int code() const {
    ASSERT(is_valid());
    return code_;
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
  inline explicit Immediate(int x);
  inline explicit Immediate(const ExternalReference& ext);
  inline explicit Immediate(Handle<Object> handle);
  inline explicit Immediate(Smi* value);
  inline explicit Immediate(Address addr);

  static Immediate CodeRelativeOffset(Label* label) {
    return Immediate(label);
  }

  bool is_zero() const { return x_ == 0 && rmode_ == RelocInfo::NONE; }
  bool is_int8() const {
    return -128 <= x_ && x_ < 128 && rmode_ == RelocInfo::NONE;
  }
  bool is_int16() const {
    return -32768 <= x_ && x_ < 32768 && rmode_ == RelocInfo::NONE;
  }

 private:
  inline explicit Immediate(Label* value);

  int x_;
  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};


// -----------------------------------------------------------------------------
// Machine instruction Operands

enum ScaleFactor {
  times_1 = 0,
  times_2 = 1,
  times_4 = 2,
  times_8 = 3,
  times_int_size = times_4,
  times_half_pointer_size = times_2,
  times_pointer_size = times_4,
  times_twice_pointer_size = times_8
};


class Operand BASE_EMBEDDED {
 public:
  // reg
  INLINE(explicit Operand(Register reg));

  // XMM reg
  INLINE(explicit Operand(XMMRegister xmm_reg));

  // [disp/r]
  INLINE(explicit Operand(int32_t disp, RelocInfo::Mode rmode));
  // disp only must always be relocated

  // [base + disp/r]
  explicit Operand(Register base, int32_t disp,
                   RelocInfo::Mode rmode = RelocInfo::NONE);

  // [base + index*scale + disp/r]
  explicit Operand(Register base,
                   Register index,
                   ScaleFactor scale,
                   int32_t disp,
                   RelocInfo::Mode rmode = RelocInfo::NONE);

  // [index*scale + disp/r]
  explicit Operand(Register index,
                   ScaleFactor scale,
                   int32_t disp,
                   RelocInfo::Mode rmode = RelocInfo::NONE);

  static Operand StaticVariable(const ExternalReference& ext) {
    return Operand(reinterpret_cast<int32_t>(ext.address()),
                   RelocInfo::EXTERNAL_REFERENCE);
  }

  static Operand StaticArray(Register index,
                             ScaleFactor scale,
                             const ExternalReference& arr) {
    return Operand(index, scale, reinterpret_cast<int32_t>(arr.address()),
                   RelocInfo::EXTERNAL_REFERENCE);
  }

  static Operand Cell(Handle<JSGlobalPropertyCell> cell) {
    return Operand(reinterpret_cast<int32_t>(cell.location()),
                   RelocInfo::GLOBAL_PROPERTY_CELL);
  }

  // Returns true if this Operand is a wrapper for the specified register.
  bool is_reg(Register reg) const;

 private:
  byte buf_[6];
  // The number of bytes in buf_.
  unsigned int len_;
  // Only valid if len_ > 4.
  RelocInfo::Mode rmode_;

  // Set the ModRM byte without an encoded 'reg' register. The
  // register is encoded later as part of the emit_operand operation.
  inline void set_modrm(int mod, Register rm);

  inline void set_sib(ScaleFactor scale, Register index, Register base);
  inline void set_disp8(int8_t disp);
  inline void set_dispr(int32_t disp, RelocInfo::Mode rmode);

  friend class Assembler;
};


// -----------------------------------------------------------------------------
// A Displacement describes the 32bit immediate field of an instruction which
// may be used together with a Label in order to refer to a yet unknown code
// position. Displacements stored in the instruction stream are used to describe
// the instruction and to chain a list of instructions using the same Label.
// A Displacement contains 2 different fields:
//
// next field: position of next displacement in the chain (0 = end of list)
// type field: instruction type
//
// A next value of null (0) indicates the end of a chain (note that there can
// be no displacement at position zero, because there is always at least one
// instruction byte before the displacement).
//
// Displacement _data field layout
//
// |31.....2|1......0|
// [  next  |  type  |

class Displacement BASE_EMBEDDED {
 public:
  enum Type {
    UNCONDITIONAL_JUMP,
    CODE_RELATIVE,
    OTHER
  };

  int data() const { return data_; }
  Type type() const { return TypeField::decode(data_); }
  void next(Label* L) const {
    int n = NextField::decode(data_);
    n > 0 ? L->link_to(n) : L->Unuse();
  }
  void link_to(Label* L) { init(L, type()); }

  explicit Displacement(int data) { data_ = data; }

  Displacement(Label* L, Type type) { init(L, type); }

  void print() {
    PrintF("%s (%x) ", (type() == UNCONDITIONAL_JUMP ? "jmp" : "[other]"),
                       NextField::decode(data_));
  }

 private:
  int data_;

  class TypeField: public BitField<Type, 0, 2> {};
  class NextField: public BitField<int,  2, 32-2> {};

  void init(Label* L, Type type);
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
  // Detect features of the target CPU. Set safe defaults if the serializer
  // is enabled (snapshots must be portable).
  static void Probe();

  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(CpuFeature f) {
    ASSERT(initialized_);
    if (f == SSE2 && !FLAG_enable_sse2) return false;
    if (f == SSE3 && !FLAG_enable_sse3) return false;
    if (f == SSE4_1 && !FLAG_enable_sse4_1) return false;
    if (f == CMOV && !FLAG_enable_cmov) return false;
    if (f == RDTSC && !FLAG_enable_rdtsc) return false;
    return (supported_ & (static_cast<uint64_t>(1) << f)) != 0;
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
    uint64_t enabled = isolate->enabled_cpu_features();
    return (enabled & (static_cast<uint64_t>(1) << f)) != 0;
  }
#endif

  // Enable a specified feature within a scope.
  class Scope BASE_EMBEDDED {
#ifdef DEBUG

   public:
    explicit Scope(CpuFeature f) {
      uint64_t mask = static_cast<uint64_t>(1) << f;
      ASSERT(CpuFeatures::IsSupported(f));
      ASSERT(!Serializer::enabled() ||
             (CpuFeatures::found_by_runtime_probing_ & mask) == 0);
      isolate_ = Isolate::UncheckedCurrent();
      old_enabled_ = 0;
      if (isolate_ != NULL) {
        old_enabled_ = isolate_->enabled_cpu_features();
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
    uint64_t old_enabled_;
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
        CpuFeatures::supported_ |= (static_cast<uint64_t>(1) << f);
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

    const uint64_t old_supported_;
  };

 private:
#ifdef DEBUG
  static bool initialized_;
#endif
  static uint64_t supported_;
  static uint64_t found_by_runtime_probing_;

  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};


class Assembler : public AssemblerBase {
 private:
  // We check before assembling an instruction that there is sufficient
  // space to write an instruction and its relocation information.
  // The relocation writer's position must be kGap bytes above the end of
  // the generated instructions. This leaves enough space for the
  // longest possible ia32 instruction, 15 bytes, and the longest possible
  // relocation information encoding, RelocInfoWriter::kMaxLength == 16.
  // (There is a 15 byte limit on ia32 instruction length that rules out some
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
  // TODO(vitalyr): the assembler does not need an isolate.
  Assembler(Isolate* isolate, void* buffer, int buffer_size);
  ~Assembler();

  // Overrides the default provided by FLAG_debug_code.
  void set_emit_debug_code(bool value) { emit_debug_code_ = value; }

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

  // Read/Modify the code target in the branch/call instruction at pc.
  inline static Address target_address_at(Address pc);
  inline static void set_target_address_at(Address pc, Address target);

  // This sets the branch destination (which is in the instruction on x86).
  // This is for calls and branches within generated code.
  inline static void set_target_at(Address instruction_payload,
                                   Address target) {
    set_target_address_at(instruction_payload, target);
  }

  // This sets the branch destination (which is in the instruction on x86).
  // This is for calls and branches to runtime code.
  inline static void set_external_target_at(Address instruction_payload,
                                            Address target) {
    set_target_address_at(instruction_payload, target);
  }

  static const int kCallTargetSize = kPointerSize;
  static const int kExternalTargetSize = kPointerSize;

  // Distance between the address of the code target in the call instruction
  // and the return address
  static const int kCallTargetAddressOffset = kPointerSize;
  // Distance between start of patched return sequence and the emitted address
  // to jump to.
  static const int kPatchReturnSequenceAddressOffset = 1;  // JMP imm32.

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  static const int kPatchDebugBreakSlotAddressOffset = 1;  // JMP imm32.

  static const int kCallInstructionLength = 5;
  static const int kJSReturnSequenceLength = 6;

  // The debug break slot must be able to contain a call instruction.
  static const int kDebugBreakSlotLength = kCallInstructionLength;

  // One byte opcode for test eax,0xXXXXXXXX.
  static const byte kTestEaxByte = 0xA9;
  // One byte opcode for test al, 0xXX.
  static const byte kTestAlByte = 0xA8;
  // One byte opcode for nop.
  static const byte kNopByte = 0x90;

  // One byte opcode for a short unconditional jump.
  static const byte kJmpShortOpcode = 0xEB;
  // One byte prefix for a short conditional jump.
  static const byte kJccShortPrefix = 0x70;
  static const byte kJncShortOpcode = kJccShortPrefix | not_carry;
  static const byte kJcShortOpcode = kJccShortPrefix | carry;

  // ---------------------------------------------------------------------------
  // Code generation
  //
  // - function names correspond one-to-one to ia32 instruction mnemonics
  // - unless specified otherwise, instructions operate on 32bit operands
  // - instructions on 8bit (byte) operands/registers have a trailing '_b'
  // - instructions on 16bit (word) operands/registers have a trailing '_w'
  // - naming conflicts with C++ keywords are resolved via a trailing '_'

  // NOTE ON INTERFACE: Currently, the interface is not very consistent
  // in the sense that some operations (e.g. mov()) can be called in more
  // the one way to generate the same instruction: The Register argument
  // can in some cases be replaced with an Operand(Register) argument.
  // This should be cleaned up and made more orthogonal. The questions
  // is: should we always use Operands instead of Registers where an
  // Operand is possible, or should we have a Register (overloaded) form
  // instead? We must be careful to make sure that the selected instruction
  // is obvious from the parameters to avoid hard-to-find code generation
  // bugs.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2.
  void Align(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Stack
  void pushad();
  void popad();

  void pushfd();
  void popfd();

  void push(const Immediate& x);
  void push_imm32(int32_t imm32);
  void push(Register src);
  void push(const Operand& src);
  void push(Handle<Object> handle);

  void pop(Register dst);
  void pop(const Operand& dst);

  void enter(const Immediate& size);
  void leave();

  // Moves
  void mov_b(Register dst, const Operand& src);
  void mov_b(const Operand& dst, int8_t imm8);
  void mov_b(const Operand& dst, Register src);

  void mov_w(Register dst, const Operand& src);
  void mov_w(const Operand& dst, Register src);

  void mov(Register dst, int32_t imm32);
  void mov(Register dst, const Immediate& x);
  void mov(Register dst, Handle<Object> handle);
  void mov(Register dst, const Operand& src);
  void mov(Register dst, Register src);
  void mov(const Operand& dst, const Immediate& x);
  void mov(const Operand& dst, Handle<Object> handle);
  void mov(const Operand& dst, Register src);

  void movsx_b(Register dst, const Operand& src);

  void movsx_w(Register dst, const Operand& src);

  void movzx_b(Register dst, const Operand& src);

  void movzx_w(Register dst, const Operand& src);

  // Conditional moves
  void cmov(Condition cc, Register dst, int32_t imm32);
  void cmov(Condition cc, Register dst, Handle<Object> handle);
  void cmov(Condition cc, Register dst, const Operand& src);

  // Flag management.
  void cld();

  // Repetitive string instructions.
  void rep_movs();
  void rep_stos();
  void stos();

  // Exchange two registers
  void xchg(Register dst, Register src);

  // Arithmetics
  void adc(Register dst, int32_t imm32);
  void adc(Register dst, const Operand& src);

  void add(Register dst, const Operand& src);
  void add(const Operand& dst, const Immediate& x);

  void and_(Register dst, int32_t imm32);
  void and_(Register dst, const Immediate& x);
  void and_(Register dst, const Operand& src);
  void and_(const Operand& src, Register dst);
  void and_(const Operand& dst, const Immediate& x);

  void cmpb(const Operand& op, int8_t imm8);
  void cmpb(Register src, const Operand& dst);
  void cmpb(const Operand& dst, Register src);
  void cmpb_al(const Operand& op);
  void cmpw_ax(const Operand& op);
  void cmpw(const Operand& op, Immediate imm16);
  void cmp(Register reg, int32_t imm32);
  void cmp(Register reg, Handle<Object> handle);
  void cmp(Register reg, const Operand& op);
  void cmp(const Operand& op, const Immediate& imm);
  void cmp(const Operand& op, Handle<Object> handle);

  void dec_b(Register dst);
  void dec_b(const Operand& dst);

  void dec(Register dst);
  void dec(const Operand& dst);

  void cdq();

  void idiv(Register src);

  // Signed multiply instructions.
  void imul(Register src);                               // edx:eax = eax * src.
  void imul(Register dst, const Operand& src);           // dst = dst * src.
  void imul(Register dst, Register src, int32_t imm32);  // dst = src * imm32.

  void inc(Register dst);
  void inc(const Operand& dst);

  void lea(Register dst, const Operand& src);

  // Unsigned multiply instruction.
  void mul(Register src);                                // edx:eax = eax * reg.

  void neg(Register dst);

  void not_(Register dst);

  void or_(Register dst, int32_t imm32);
  void or_(Register dst, const Operand& src);
  void or_(const Operand& dst, Register src);
  void or_(const Operand& dst, const Immediate& x);

  void rcl(Register dst, uint8_t imm8);
  void rcr(Register dst, uint8_t imm8);

  void sar(Register dst, uint8_t imm8);
  void sar_cl(Register dst);

  void sbb(Register dst, const Operand& src);

  void shld(Register dst, const Operand& src);

  void shl(Register dst, uint8_t imm8);
  void shl_cl(Register dst);

  void shrd(Register dst, const Operand& src);

  void shr(Register dst, uint8_t imm8);
  void shr_cl(Register dst);

  void subb(const Operand& dst, int8_t imm8);
  void subb(Register dst, const Operand& src);
  void sub(const Operand& dst, const Immediate& x);
  void sub(Register dst, const Operand& src);
  void sub(const Operand& dst, Register src);

  void test(Register reg, const Immediate& imm);
  void test(Register reg, const Operand& op);
  void test_b(Register reg, const Operand& op);
  void test(const Operand& op, const Immediate& imm);
  void test_b(const Operand& op, uint8_t imm8);

  void xor_(Register dst, int32_t imm32);
  void xor_(Register dst, const Operand& src);
  void xor_(const Operand& src, Register dst);
  void xor_(const Operand& dst, const Immediate& x);

  // Bit operations.
  void bt(const Operand& dst, Register src);
  void bts(const Operand& dst, Register src);

  // Miscellaneous
  void hlt();
  void int3();
  void nop();
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
  void call(Label* L);
  void call(byte* entry, RelocInfo::Mode rmode);
  int CallSize(const Operand& adr);
  void call(const Operand& adr);
  int CallSize(Handle<Code> code, RelocInfo::Mode mode);
  void call(Handle<Code> code,
            RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            unsigned ast_id = kNoASTId);

  // Jumps
  // unconditional jump to L
  void jmp(Label* L, Label::Distance distance = Label::kFar);
  void jmp(byte* entry, RelocInfo::Mode rmode);
  void jmp(const Operand& adr);
  void jmp(Handle<Code> code, RelocInfo::Mode rmode);

  // Conditional jumps
  void j(Condition cc,
         Label* L,
         Label::Distance distance = Label::kFar);
  void j(Condition cc, byte* entry, RelocInfo::Mode rmode);
  void j(Condition cc, Handle<Code> code);

  // Floating-point operations
  void fld(int i);
  void fstp(int i);

  void fld1();
  void fldz();
  void fldpi();
  void fldln2();

  void fld_s(const Operand& adr);
  void fld_d(const Operand& adr);

  void fstp_s(const Operand& adr);
  void fstp_d(const Operand& adr);
  void fst_d(const Operand& adr);

  void fild_s(const Operand& adr);
  void fild_d(const Operand& adr);

  void fist_s(const Operand& adr);

  void fistp_s(const Operand& adr);
  void fistp_d(const Operand& adr);

  // The fisttp instructions require SSE3.
  void fisttp_s(const Operand& adr);
  void fisttp_d(const Operand& adr);

  void fabs();
  void fchs();
  void fcos();
  void fsin();
  void fyl2x();

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

  void frndint();

  void sahf();
  void setcc(Condition cc, Register reg);

  void cpuid();

  // SSE2 instructions
  void cvttss2si(Register dst, const Operand& src);
  void cvttsd2si(Register dst, const Operand& src);

  void cvtsi2sd(XMMRegister dst, const Operand& src);
  void cvtss2sd(XMMRegister dst, XMMRegister src);
  void cvtsd2ss(XMMRegister dst, XMMRegister src);

  void addsd(XMMRegister dst, XMMRegister src);
  void subsd(XMMRegister dst, XMMRegister src);
  void mulsd(XMMRegister dst, XMMRegister src);
  void divsd(XMMRegister dst, XMMRegister src);
  void xorpd(XMMRegister dst, XMMRegister src);
  void xorps(XMMRegister dst, XMMRegister src);
  void sqrtsd(XMMRegister dst, XMMRegister src);

  void andpd(XMMRegister dst, XMMRegister src);

  void ucomisd(XMMRegister dst, XMMRegister src);

  enum RoundingMode {
    kRoundToNearest = 0x0,
    kRoundDown      = 0x1,
    kRoundUp        = 0x2,
    kRoundToZero    = 0x3
  };

  void roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode);

  void movmskpd(Register dst, XMMRegister src);

  void cmpltsd(XMMRegister dst, XMMRegister src);

  void movaps(XMMRegister dst, XMMRegister src);

  void movdqa(XMMRegister dst, const Operand& src);
  void movdqa(const Operand& dst, XMMRegister src);
  void movdqu(XMMRegister dst, const Operand& src);
  void movdqu(const Operand& dst, XMMRegister src);

  // Use either movsd or movlpd.
  void movdbl(XMMRegister dst, const Operand& src);
  void movdbl(const Operand& dst, XMMRegister src);

  void movd(XMMRegister dst, const Operand& src);
  void movd(const Operand& src, XMMRegister dst);
  void movsd(XMMRegister dst, XMMRegister src);

  void movss(XMMRegister dst, const Operand& src);
  void movss(const Operand& src, XMMRegister dst);
  void movss(XMMRegister dst, XMMRegister src);

  void pand(XMMRegister dst, XMMRegister src);
  void pxor(XMMRegister dst, XMMRegister src);
  void por(XMMRegister dst, XMMRegister src);
  void ptest(XMMRegister dst, XMMRegister src);

  void psllq(XMMRegister reg, int8_t shift);
  void psllq(XMMRegister dst, XMMRegister src);
  void psrlq(XMMRegister reg, int8_t shift);
  void psrlq(XMMRegister dst, XMMRegister src);
  void pshufd(XMMRegister dst, XMMRegister src, int8_t shuffle);
  void pextrd(const Operand& dst, XMMRegister src, int8_t offset);
  void pinsrd(XMMRegister dst, const Operand& src, int8_t offset);

  // Parallel XMM operations.
  void movntdqa(XMMRegister src, const Operand& dst);
  void movntdq(const Operand& dst, XMMRegister src);
  // Prefetch src position into cache level.
  // Level 1, 2 or 3 specifies CPU cache level. Level 0 specifies a
  // non-temporal
  void prefetch(const Operand& src, int level);
  // TODO(lrn): Need SFENCE for movnt?

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
  // Use --code-comments to enable, or provide "force = true" flag to always
  // write a comment.
  void RecordComment(const char* msg, bool force = false);

  // Writes a single byte or word of data in the code stream.  Used for
  // inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);

  int pc_offset() const { return pc_ - buffer_; }

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool overflow() const { return pc_ >= reloc_info_writer.pos() - kGap; }

  // Get the number of bytes available in the buffer.
  inline int available_space() const { return reloc_info_writer.pos() - pc_; }

  static bool IsNop(Address addr) { return *addr == 0x90; }

  PositionsRecorder* positions_recorder() { return &positions_recorder_; }

  int relocation_writer_size() {
    return (buffer_ + buffer_size_) - reloc_info_writer.pos();
  }

  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512*MB;
  static const int kMinimalBufferSize = 4*KB;

 protected:
  bool emit_debug_code() const { return emit_debug_code_; }

  void movsd(XMMRegister dst, const Operand& src);
  void movsd(const Operand& dst, XMMRegister src);

  void emit_sse_operand(XMMRegister reg, const Operand& adr);
  void emit_sse_operand(XMMRegister dst, XMMRegister src);
  void emit_sse_operand(Register dst, XMMRegister src);

  byte* addr_at(int pos) { return buffer_ + pos; }

 private:
  byte byte_at(int pos)  { return buffer_[pos]; }
  void set_byte_at(int pos, byte value) { buffer_[pos] = value; }
  uint32_t long_at(int pos)  {
    return *reinterpret_cast<uint32_t*>(addr_at(pos));
  }
  void long_at_put(int pos, uint32_t x)  {
    *reinterpret_cast<uint32_t*>(addr_at(pos)) = x;
  }

  // code emission
  void GrowBuffer();
  inline void emit(uint32_t x);
  inline void emit(Handle<Object> handle);
  inline void emit(uint32_t x,
                   RelocInfo::Mode rmode,
                   unsigned ast_id = kNoASTId);
  inline void emit(const Immediate& x);
  inline void emit_w(const Immediate& x);

  // Emit the code-object-relative offset of the label's position
  inline void emit_code_relative_offset(Label* label);

  // instruction generation
  void emit_arith_b(int op1, int op2, Register dst, int imm8);

  // Emit a basic arithmetic instruction (i.e. first byte of the family is 0x81)
  // with a given destination expression and an immediate operand.  It attempts
  // to use the shortest encoding possible.
  // sel specifies the /n in the modrm byte (see the Intel PRM).
  void emit_arith(int sel, Operand dst, const Immediate& x);

  void emit_operand(Register reg, const Operand& adr);

  void emit_farith(int b1, int b2, int i);

  // labels
  void print(Label* L);
  void bind_to(Label* L, int pos);

  // displacements
  inline Displacement disp_at(Label* L);
  inline void disp_at_put(Label* L, Displacement disp);
  inline void emit_disp(Label* L, Displacement::Type type);
  inline void emit_near_disp(Label* L);

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

  // code generation
  byte* pc_;  // the program counter; moves forward
  RelocInfoWriter reloc_info_writer;

  PositionsRecorder positions_recorder_;

  bool emit_debug_code_;

  friend class PositionsRecorder;
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

#endif  // V8_IA32_ASSEMBLER_IA32_H_

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

// A light-weight S390 Assembler
// Generates user mode instructions for z/Architecture

#ifndef V8_S390_ASSEMBLER_S390_H_
#define V8_S390_ASSEMBLER_S390_H_
#include <stdio.h>
#if V8_HOST_ARCH_S390
// elf.h include is required for auxv check for STFLE facility used
// for hardware detection, which is sensible only on s390 hosts.
#include <elf.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include "src/assembler.h"
#include "src/s390/constants-s390.h"

#define ABI_USES_FUNCTION_DESCRIPTORS 0

#define ABI_PASSES_HANDLES_IN_REGS 1

// ObjectPair is defined under runtime/runtime-util.h.
// On 31-bit, ObjectPair == uint64_t.  ABI dictates long long
//            be returned with the lower addressed half in r2
//            and the higher addressed half in r3. (Returns in Regs)
// On 64-bit, ObjectPair is a Struct.  ABI dictaes Structs be
//            returned in a storage buffer allocated by the caller,
//            with the address of this buffer passed as a hidden
//            argument in r2. (Does NOT return in Regs)
// For x86 linux, ObjectPair is returned in registers.
#if V8_TARGET_ARCH_S390X
#define ABI_RETURNS_OBJECTPAIR_IN_REGS 0
#else
#define ABI_RETURNS_OBJECTPAIR_IN_REGS 1
#endif

#define ABI_CALL_VIA_IP 1

#define INSTR_AND_DATA_CACHE_COHERENCY LWSYNC

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(r0)  V(r1)  V(r2)  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)  \
  V(r8)  V(r9)  V(r10) V(fp) V(ip) V(r13) V(r14) V(sp)

#define ALLOCATABLE_GENERAL_REGISTERS(V)                  \
  V(r2)  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)                \
  V(r8)  V(r9)  V(r13)

#define DOUBLE_REGISTERS(V)                               \
  V(d0)  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)  \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d13) V(d14) V(d15)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)         \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d15) V(d0)

#define C_REGISTERS(V)                                            \
  V(cr0)  V(cr1)  V(cr2)  V(cr3)  V(cr4)  V(cr5)  V(cr6)  V(cr7)  \
  V(cr8)  V(cr9)  V(cr10) V(cr11) V(cr12) V(cr15)
// clang-format on

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding
const int kNumRegs = 16;

// Caller-saved/arguments registers
const RegList kJSCallerSaved = 1 << 1 | 1 << 2 |  // r2  a1
                               1 << 3 |           // r3  a2
                               1 << 4 |           // r4  a3
                               1 << 5;            // r5  a4

const int kNumJSCallerSaved = 5;

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved =
    1 << 6 |   // r6 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 7 |   // r7 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 8 |   // r8 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 9 |   // r9 (HandleScope logic in MacroAssembler)
    1 << 10 |  // r10 (Roots register in Javascript)
    1 << 11 |  // r11 (fp in Javascript)
    1 << 12 |  // r12 (ip in Javascript)
    1 << 13;   // r13 (cp in Javascript)
// 1 << 15;   // r15 (sp in Javascript)

const int kNumCalleeSaved = 8;

#ifdef V8_TARGET_ARCH_S390X

const RegList kCallerSavedDoubles = 1 << 0 |  // d0
                                    1 << 1 |  // d1
                                    1 << 2 |  // d2
                                    1 << 3 |  // d3
                                    1 << 4 |  // d4
                                    1 << 5 |  // d5
                                    1 << 6 |  // d6
                                    1 << 7;   // d7

const int kNumCallerSavedDoubles = 8;

const RegList kCalleeSavedDoubles = 1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d11
                                    1 << 12 |  // d12
                                    1 << 13 |  // d12
                                    1 << 14 |  // d12
                                    1 << 15;   // d13

const int kNumCalleeSavedDoubles = 8;

#else

const RegList kCallerSavedDoubles = 1 << 14 |  // d14
                                    1 << 15 |  // d15
                                    1 << 0 |   // d0
                                    1 << 1 |   // d1
                                    1 << 2 |   // d2
                                    1 << 3 |   // d3
                                    1 << 5 |   // d5
                                    1 << 7 |   // d7
                                    1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d10
                                    1 << 12 |  // d10
                                    1 << 13;   // d11

const int kNumCallerSavedDoubles = 14;

const RegList kCalleeSavedDoubles = 1 << 4 |  // d4
                                    1 << 6;   // d6

const int kNumCalleeSavedDoubles = 2;

#endif

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
// TODO(regis): Only 8 registers may actually be sufficient. Revisit.
const int kNumSafepointRegisters = 16;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

// The following constants describe the stack frame linkage area as
// defined by the ABI.

#if V8_TARGET_ARCH_S390X
// [0] Back Chain
// [1] Reserved for compiler use
// [2] GPR 2
// [3] GPR 3
// ...
// [15] GPR 15
// [16] FPR 0
// [17] FPR 2
// [18] FPR 4
// [19] FPR 6
const int kNumRequiredStackFrameSlots = 20;
const int kStackFrameRASlot = 14;
const int kStackFrameSPSlot = 15;
const int kStackFrameExtraParamSlot = 20;
#else
// [0] Back Chain
// [1] Reserved for compiler use
// [2] GPR 2
// [3] GPR 3
// ...
// [15] GPR 15
// [16..17] FPR 0
// [18..19] FPR 2
// [20..21] FPR 4
// [22..23] FPR 6
const int kNumRequiredStackFrameSlots = 24;
const int kStackFrameRASlot = 14;
const int kStackFrameSPSlot = 15;
const int kStackFrameExtraParamSlot = 24;
#endif

// zLinux ABI requires caller frames to include sufficient space for
// callee preserved register save area.
#if V8_TARGET_ARCH_S390X
const int kCalleeRegisterSaveAreaSize = 160;
#elif V8_TARGET_ARCH_S390
const int kCalleeRegisterSaveAreaSize = 96;
#else
const int kCalleeRegisterSaveAreaSize = 0;
#endif

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
#if V8_TARGET_LITTLE_ENDIAN
  static constexpr int kMantissaOffset = 0;
  static constexpr int kExponentOffset = 4;
#else
  static constexpr int kMantissaOffset = 4;
  static constexpr int kExponentOffset = 0;
#endif

 private:
  friend class RegisterBase;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);
static_assert(sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DEFINE_REGISTER(R) \
  constexpr Register R = Register::from_code<kRegCode_##R>();
GENERAL_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr Register no_reg = Register::no_reg();

// Register aliases
constexpr Register kRootRegister = r10;   // Roots array pointer.
constexpr Register cp = r13;              // JavaScript context pointer.

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

// Double word VFP register.
class DoubleRegister : public RegisterBase<DoubleRegister, kDoubleAfterLast> {
 public:
  // A few double registers are reserved: one as a scratch register and one to
  // hold 0.0, that does not fit in the immediate field of vmov instructions.
  // d14: 0.0
  // d15: scratch register.
  static constexpr int kSizeInBytes = 8;
  inline static int NumRegisters();

 private:
  friend class RegisterBase;

  explicit constexpr DoubleRegister(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(DoubleRegister);
static_assert(sizeof(DoubleRegister) == sizeof(int),
              "DoubleRegister can efficiently be passed by value");

typedef DoubleRegister FloatRegister;

// TODO(john.yan) Define SIMD registers.
typedef DoubleRegister Simd128Register;

#define DEFINE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code<kDoubleCode_##R>();
DOUBLE_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

constexpr DoubleRegister kDoubleRegZero = d14;
constexpr DoubleRegister kScratchDoubleReg = d13;

Register ToRegister(int num);

enum CRegisterCode {
#define REGISTER_CODE(R) kCCode_##R,
  C_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kCAfterLast
};

// Coprocessor register
class CRegister : public RegisterBase<CRegister, kCAfterLast> {
  friend class RegisterBase;
  explicit constexpr CRegister(int code) : RegisterBase(code) {}
};

constexpr CRegister no_creg = CRegister::no_reg();
#define DECLARE_C_REGISTER(R) \
  constexpr CRegister R = CRegister::from_code<kCCode_##R>();
C_REGISTERS(DECLARE_C_REGISTER)
#undef DECLARE_C_REGISTER

// -----------------------------------------------------------------------------
// Machine instruction Operands

// Class Operand represents a shifter operand in data processing instructions
// defining immediate numbers and masks
class Operand {
 public:
  // immediate
  V8_INLINE explicit Operand(intptr_t immediate,
                             RelocInfo::Mode rmode = RelocInfo::NONE)
      : rmode_(rmode) {
    value_.immediate = immediate;
  }
  V8_INLINE static Operand Zero() { return Operand(static_cast<intptr_t>(0)); }
  V8_INLINE explicit Operand(const ExternalReference& f)
      : rmode_(RelocInfo::EXTERNAL_REFERENCE) {
    value_.immediate = static_cast<intptr_t>(f.address());
  }
  explicit Operand(Handle<HeapObject> handle);
  V8_INLINE explicit Operand(Smi* value) : rmode_(RelocInfo::NONE) {
    value_.immediate = reinterpret_cast<intptr_t>(value);
  }

  // rm
  V8_INLINE explicit Operand(Register rm);

  static Operand EmbeddedNumber(double value);  // Smi or HeapNumber
  static Operand EmbeddedStringConstant(const StringConstantBase* str);

  // Return true if this is a register operand.
  V8_INLINE bool is_reg() const { return rm_.is_valid(); }

  bool must_output_reloc_info(const Assembler* assembler) const;

  inline intptr_t immediate() const {
    DCHECK(!rm_.is_valid());
    DCHECK(!is_heap_object_request());
    return value_.immediate;
  }

  HeapObjectRequest heap_object_request() const {
    DCHECK(is_heap_object_request());
    return value_.heap_object_request;
  }

  inline void setBits(int n) {
    value_.immediate =
        (static_cast<uint32_t>(value_.immediate) << (32 - n)) >> (32 - n);
  }

  Register rm() const { return rm_; }

  bool is_heap_object_request() const {
    DCHECK_IMPLIES(is_heap_object_request_, !rm_.is_valid());
    DCHECK_IMPLIES(is_heap_object_request_,
                   rmode_ == RelocInfo::EMBEDDED_OBJECT ||
                       rmode_ == RelocInfo::CODE_TARGET);
    return is_heap_object_request_;
  }

  RelocInfo::Mode rmode() const { return rmode_; }

 private:
  Register rm_ = no_reg;
  union Value {
    Value() {}
    HeapObjectRequest heap_object_request;  // if is_heap_object_request_
    intptr_t immediate;                     // otherwise
  } value_;                                 // valid if rm_ == no_reg
  bool is_heap_object_request_ = false;

  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};

typedef int32_t Disp;

// Class MemOperand represents a memory operand in load and store instructions
// On S390, we have various flavours of memory operands:
//   1) a base register + 16 bit unsigned displacement
//   2) a base register + index register + 16 bit unsigned displacement
//   3) a base register + index register + 20 bit signed displacement
class MemOperand {
 public:
  explicit MemOperand(Register rx, Disp offset = 0);
  explicit MemOperand(Register rx, Register rb, Disp offset = 0);

  int32_t offset() const { return offset_; }
  uint32_t getDisplacement() const { return offset(); }

  // Base register
  Register rb() const {
    DCHECK(baseRegister != no_reg);
    return baseRegister;
  }

  Register getBaseRegister() const { return rb(); }

  // Index Register
  Register rx() const {
    DCHECK(indexRegister != no_reg);
    return indexRegister;
  }
  Register getIndexRegister() const { return rx(); }

 private:
  Register baseRegister;   // base
  Register indexRegister;  // index
  int32_t offset_;         // offset

  friend class Assembler;
};

class DeferredRelocInfo {
 public:
  DeferredRelocInfo() {}
  DeferredRelocInfo(int position, RelocInfo::Mode rmode, intptr_t data)
      : position_(position), rmode_(rmode), data_(data) {}

  int position() const { return position_; }
  RelocInfo::Mode rmode() const { return rmode_; }
  intptr_t data() const { return data_; }

 private:
  int position_;
  RelocInfo::Mode rmode_;
  intptr_t data_;
};

class V8_EXPORT_PRIVATE Assembler : public AssemblerBase {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is nullptr, the assembler allocates and grows its
  // own buffer, and buffer_size determines the initial buffer size. The buffer
  // is owned by the assembler and deallocated upon destruction of the
  // assembler.
  //
  // If the provided buffer is not nullptr, the assembler uses the provided
  // buffer for code generation and assumes its size to be buffer_size. If the
  // buffer is too small, a fatal error occurs. No deallocation of the buffer is
  // done upon destruction of the assembler.
  Assembler(const AssemblerOptions& options, void* buffer, int buffer_size);
  virtual ~Assembler() {}

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(Isolate* isolate, CodeDesc* desc);

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

  // Links a label at the current pc_offset().  If already bound, returns the
  // bound position.  If already linked, returns the position of the prior link.
  // Otherwise, returns the current pc_offset().
  int link(Label* L);

  // Determines if Label is bound and near enough so that a single
  // branch instruction can be used to reach it.
  bool is_near(Label* L, Condition cond);

  // Returns the branch offset to the given label from the current code position
  // Links the label to the current position if it is still unbound
  int branch_offset(Label* L) { return link(L) - pc_offset(); }

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);
  void load_label_offset(Register r1, Label* L);

  // Read/Modify the code target address in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  V8_INLINE static Address target_address_at(Address pc, Address constant_pool);
  V8_INLINE static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // Given the address of the beginning of a call, return the address
  // in the instruction stream that the call will return to.
  V8_INLINE static Address return_address_from_call_start(Address pc);

  inline Handle<Object> code_target_object_handle_at(Address pc);
  // This sets the branch destination.
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code* code, Address target);

  // Get the size of the special target encoded at 'instruction_payload'.
  inline static int deserialization_special_target_size(
      Address instruction_payload);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  // Here we are patching the address in the IIHF/IILF instruction pair.
  // These values are used in the serialization process and must be zero for
  // S390 platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.
  static constexpr int kSpecialTargetSize = 0;

// Number of bytes for instructions used to store pointer sized constant.
#if V8_TARGET_ARCH_S390X
  static constexpr int kBytesForPtrConstant = 12;  // IIHF + IILF
#else
  static constexpr int kBytesForPtrConstant = 6;  // IILF
#endif

  // Distance between the instruction referring to the address of the call
  // target and the return address.

  // Offset between call target address and return address
  // for BRASL calls
  // Patch will be appiled to other FIXED_SEQUENCE call
  static constexpr int kCallTargetAddressOffset = 6;

// The length of FIXED_SEQUENCE call
// iihf    r8, <address_hi>  // <64-bit only>
// iilf    r8, <address_lo>
// basr    r14, r8
#if V8_TARGET_ARCH_S390X
  static constexpr int kCallSequenceLength = 14;
#else
  static constexpr int kCallSequenceLength = 8;
#endif

  // ---------------------------------------------------------------------------
  // Code generation

  template <class T, int size, int lo, int hi>
  inline T getfield(T value) {
    DCHECK(lo < hi);
    DCHECK_GT(size, 0);
    int mask = hi - lo;
    int shift = size * 8 - hi;
    uint32_t mask_value = (mask == 32) ? 0xffffffff : (1 << mask) - 1;
    return (value & mask_value) << shift;
  }

#define DECLARE_S390_RIL_AB_INSTRUCTIONS(name, op_name, op_value) \
  template <class R1>                                             \
  inline void name(R1 r1, const Operand& i2) {                    \
    ril_format(op_name, r1.code(), i2.immediate());               \
  }
#define DECLARE_S390_RIL_C_INSTRUCTIONS(name, op_name, op_value) \
  inline void name(Condition m1, const Operand& i2) {            \
    ril_format(op_name, m1, i2.immediate());                     \
  }

  inline void ril_format(Opcode opcode, int f1, int f2) {
    uint32_t op1 = opcode >> 4;
    uint32_t op2 = opcode & 0xf;
    emit6bytes(
        getfield<uint64_t, 6, 0, 8>(op1) | getfield<uint64_t, 6, 8, 12>(f1) |
        getfield<uint64_t, 6, 12, 16>(op2) | getfield<uint64_t, 6, 16, 48>(f2));
  }
  S390_RIL_A_OPCODE_LIST(DECLARE_S390_RIL_AB_INSTRUCTIONS)
  S390_RIL_B_OPCODE_LIST(DECLARE_S390_RIL_AB_INSTRUCTIONS)
  S390_RIL_C_OPCODE_LIST(DECLARE_S390_RIL_C_INSTRUCTIONS)
#undef DECLARE_S390_RIL_AB_INSTRUCTIONS
#undef DECLARE_S390_RIL_C_INSTRUCTIONS

#define DECLARE_S390_RR_INSTRUCTIONS(name, op_name, op_value) \
  inline void name(Register r1, Register r2) {                \
    rr_format(op_name, r1.code(), r2.code());                 \
  }                                                           \
  inline void name(DoubleRegister r1, DoubleRegister r2) {    \
    rr_format(op_name, r1.code(), r2.code());                 \
  }                                                           \
  inline void name(Condition m1, Register r2) {               \
    rr_format(op_name, m1, r2.code());                        \
  }

  inline void rr_format(Opcode opcode, int f1, int f2) {
    emit2bytes(getfield<uint16_t, 2, 0, 8>(opcode) |
               getfield<uint16_t, 2, 8, 12>(f1) |
               getfield<uint16_t, 2, 12, 16>(f2));
  }
  S390_RR_OPCODE_LIST(DECLARE_S390_RR_INSTRUCTIONS)
#undef DECLARE_S390_RR_INSTRUCTIONS

#define DECLARE_S390_RRD_INSTRUCTIONS(name, op_name, op_value) \
  template <class R1, class R2, class R3>                      \
  inline void name(R1 r1, R3 r3, R2 r2) {                      \
    rrd_format(op_name, r1.code(), r3.code(), r2.code());      \
  }
  inline void rrd_format(Opcode opcode, int f1, int f2, int f3) {
    emit4bytes(getfield<uint32_t, 4, 0, 16>(opcode) |
               getfield<uint32_t, 4, 16, 20>(f1) |
               getfield<uint32_t, 4, 24, 28>(f2) |
               getfield<uint32_t, 4, 28, 32>(f3));
  }
  S390_RRD_OPCODE_LIST(DECLARE_S390_RRD_INSTRUCTIONS)
#undef DECLARE_S390_RRD_INSTRUCTIONS

#define DECLARE_S390_RRE_INSTRUCTIONS(name, op_name, op_value) \
  template <class R1, class R2>                                \
  inline void name(R1 r1, R2 r2) {                             \
    rre_format(op_name, r1.code(), r2.code());                 \
  }
  inline void rre_format(Opcode opcode, int f1, int f2) {
    emit4bytes(getfield<uint32_t, 4, 0, 16>(opcode) |
               getfield<uint32_t, 4, 24, 28>(f1) |
               getfield<uint32_t, 4, 28, 32>(f2));
  }
  S390_RRE_OPCODE_LIST(DECLARE_S390_RRE_INSTRUCTIONS)
  // Special format
  void lzdr(DoubleRegister r1) { rre_format(LZDR, r1.code(), 0); }
#undef DECLARE_S390_RRE_INSTRUCTIONS

#define DECLARE_S390_RX_INSTRUCTIONS(name, op_name, op_value)            \
  template <class R1>                                                    \
  inline void name(R1 r1, Register x2, Register b2, const Operand& d2) { \
    rx_format(op_name, r1.code(), x2.code(), b2.code(),                  \
              d2.immediate());                                           \
  }                                                                      \
  template <class R1>                                                    \
  inline void name(R1 r1, const MemOperand& opnd) {                      \
    name(r1, opnd.getIndexRegister(), opnd.getBaseRegister(),            \
         Operand(opnd.getDisplacement()));                               \
  }

  inline void rx_format(Opcode opcode, int f1, int f2, int f3, int f4) {
    DCHECK(is_uint8(opcode));
    DCHECK(is_uint12(f4));
    emit4bytes(getfield<uint32_t, 4, 0, 8>(opcode) |
               getfield<uint32_t, 4, 8, 12>(f1) |
               getfield<uint32_t, 4, 12, 16>(f2) |
               getfield<uint32_t, 4, 16, 20>(f3) |
               getfield<uint32_t, 4, 20, 32>(f4));
  }
  S390_RX_A_OPCODE_LIST(DECLARE_S390_RX_INSTRUCTIONS)

  void bc(Condition cond, const MemOperand& opnd) {
    bc(cond, opnd.getIndexRegister(),
       opnd.getBaseRegister(), Operand(opnd.getDisplacement()));
  }
  void bc(Condition cond, Register x2, Register b2, const Operand& d2) {
    rx_format(BC, cond, x2.code(), b2.code(), d2.immediate());
  }
#undef DECLARE_S390_RX_INSTRUCTIONS

#define DECLARE_S390_RXY_INSTRUCTIONS(name, op_name, op_value)            \
  template <class R1, class R2>                                           \
  inline void name(R1 r1, R2 r2, Register b2, const Operand& d2) {        \
    rxy_format(op_name, r1.code(), r2.code(), b2.code(), d2.immediate()); \
  }                                                                       \
  template <class R1>                                                     \
  inline void name(R1 r1, const MemOperand& opnd) {                       \
    name(r1, opnd.getIndexRegister(), opnd.getBaseRegister(),             \
         Operand(opnd.getDisplacement()));                                \
  }

  inline void rxy_format(Opcode opcode, int f1, int f2, int f3, int f4) {
    DCHECK(is_uint16(opcode));
    DCHECK(is_int20(f4));
    emit6bytes(getfield<uint64_t, 6, 0, 8>(opcode >> 8) |
               getfield<uint64_t, 6, 8, 12>(f1) |
               getfield<uint64_t, 6, 12, 16>(f2) |
               getfield<uint64_t, 6, 16, 20>(f3) |
               getfield<uint64_t, 6, 20, 32>(f4 & 0x0fff) |
               getfield<uint64_t, 6, 32, 40>(f4 >> 12) |
               getfield<uint64_t, 6, 40, 48>(opcode & 0x00ff));
  }
  S390_RXY_A_OPCODE_LIST(DECLARE_S390_RXY_INSTRUCTIONS)

  void pfd(Condition cond, const MemOperand& opnd) {
    pfd(cond, opnd.getIndexRegister(),
        opnd.getBaseRegister(), Operand(opnd.getDisplacement()));
  }
  void pfd(Condition cond, Register x2, Register b2, const Operand& d2) {
    rxy_format(PFD, cond, x2.code(), b2.code(), d2.immediate());
  }
#undef DECLARE_S390_RXY_INSTRUCTIONS


inline void rsy_format(Opcode op, int f1, int f2, int f3, int f4) {
  DCHECK(is_int20(f4));
  DCHECK(is_uint16(op));
  uint64_t code = (getfield<uint64_t, 6, 0, 8>(op >> 8) |
                   getfield<uint64_t, 6, 8, 12>(f1) |
                   getfield<uint64_t, 6, 12, 16>(f2) |
                   getfield<uint64_t, 6, 16, 20>(f3) |
                   getfield<uint64_t, 6, 20, 32>(f4 & 0x0fff) |
                   getfield<uint64_t, 6, 32, 40>(f4 >> 12) |
                   getfield<uint64_t, 6, 40, 48>(op & 0xff));
  emit6bytes(code);
}

#define DECLARE_S390_RSY_A_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register r1, Register r3, Register b2,                         \
            const Operand& d2 = Operand::Zero()) {                         \
    rsy_format(op_name, r1.code(), r3.code(), b2.code(), d2.immediate());  \
  }                                                                        \
  void name(Register r1, Register r3, Operand d2) {                        \
    name(r1, r3, r0, d2);                                                  \
  }                                                                        \
  void name(Register r1, Register r3, const MemOperand& opnd) {            \
    name(r1, r3, opnd.getBaseRegister(), Operand(opnd.getDisplacement())); \
  }
  S390_RSY_A_OPCODE_LIST(DECLARE_S390_RSY_A_INSTRUCTIONS);
#undef DECLARE_S390_RSY_A_INSTRUCTIONS

#define DECLARE_S390_RSY_B_INSTRUCTIONS(name, op_name, op_value)            \
  void name(Register r1, Condition m3, Register b2, const Operand& d2) {    \
    rsy_format(op_name, r1.code(), m3, b2.code(), d2.immediate());          \
  }                                                                         \
  void name(Register r1, Condition m3, const MemOperand& opnd) {            \
    name(r1, m3, opnd.getBaseRegister(), Operand(opnd.getDisplacement()));  \
  }
  S390_RSY_B_OPCODE_LIST(DECLARE_S390_RSY_B_INSTRUCTIONS);
#undef DECLARE_S390_RSY_B_INSTRUCTIONS


inline void rs_format(Opcode op, int f1, int f2, int f3, const int f4) {
  uint32_t code = getfield<uint32_t, 4, 0, 8>(op) |
                  getfield<uint32_t, 4, 8, 12>(f1) |
                  getfield<uint32_t, 4, 12, 16>(f2) |
                  getfield<uint32_t, 4, 16, 20>(f3) |
                  getfield<uint32_t, 4, 20, 32>(f4);
  emit4bytes(code);
}

#define DECLARE_S390_RS_A_INSTRUCTIONS(name, op_name, op_value)             \
  void name(Register r1, Register r3, Register b2, const Operand& d2) {     \
    rs_format(op_name, r1.code(), r3.code(), b2.code(), d2.immediate());    \
  }                                                                         \
  void name(Register r1, Register r3, const MemOperand& opnd) {             \
    name(r1, r3, opnd.getBaseRegister(), Operand(opnd.getDisplacement()));  \
  }
  S390_RS_A_OPCODE_LIST(DECLARE_S390_RS_A_INSTRUCTIONS);
#undef DECLARE_S390_RS_A_INSTRUCTIONS

#define DECLARE_S390_RS_B_INSTRUCTIONS(name, op_name, op_value)             \
  void name(Register r1, Condition m3, Register b2, const Operand& d2) {    \
    rs_format(op_name, r1.code(), m3, b2.code(), d2.immediate());           \
  }                                                                         \
  void name(Register r1, Condition m3, const MemOperand& opnd) {            \
    name(r1, m3, opnd.getBaseRegister(), Operand(opnd.getDisplacement()));  \
  }
  S390_RS_B_OPCODE_LIST(DECLARE_S390_RS_B_INSTRUCTIONS);
#undef DECLARE_S390_RS_B_INSTRUCTIONS

#define DECLARE_S390_RS_SHIFT_FORMAT(name, opcode)                          \
  void name(Register r1, Register r2, const Operand& opnd =                 \
            Operand::Zero()) {                                              \
    DCHECK(r2 != r0);                                                       \
    rs_format(opcode, r1.code(), r0.code(), r2.code(), opnd.immediate());   \
  }                                                                         \
  void name(Register r1, const Operand& opnd) {                             \
    rs_format(opcode, r1.code(), r0.code(), r0.code(), opnd.immediate());   \
  }
  DECLARE_S390_RS_SHIFT_FORMAT(sll, SLL)
  DECLARE_S390_RS_SHIFT_FORMAT(srl, SRL)
  DECLARE_S390_RS_SHIFT_FORMAT(sla, SLA)
  DECLARE_S390_RS_SHIFT_FORMAT(sra, SRA)
  DECLARE_S390_RS_SHIFT_FORMAT(sldl, SLDL)
  DECLARE_S390_RS_SHIFT_FORMAT(srda, SRDA)
  DECLARE_S390_RS_SHIFT_FORMAT(srdl, SRDL)
#undef DECLARE_S390_RS_SHIFT_FORMAT


inline void rxe_format(Opcode op, int f1, int f2, int f3, int f4, int f5 = 0) {
  DCHECK(is_uint12(f4));
  DCHECK(is_uint16(op));
  uint64_t code = (getfield<uint64_t, 6, 0, 8>(op >> 8) |
                   getfield<uint64_t, 6, 8, 12>(f1) |
                   getfield<uint64_t, 6, 12, 16>(f2) |
                   getfield<uint64_t, 6, 16, 20>(f3) |
                   getfield<uint64_t, 6, 20, 32>(f4 & 0x0fff) |
                   getfield<uint64_t, 6, 32, 36>(f5) |
                   getfield<uint64_t, 6, 40, 48>(op & 0xff));
  emit6bytes(code);
}

#define DECLARE_S390_RXE_INSTRUCTIONS(name, op_name, op_value)             \
  void name(Register r1, Register x2, Register b2, const Operand& d2,      \
            Condition m3 = static_cast<Condition>(0)) {                    \
    rxe_format(op_name, r1.code(), x2.code(), b2.code(), d2.immediate(),   \
               m3);                                                        \
  }                                                                        \
  template<class _R1Type>                                                  \
  void name(_R1Type r1, const MemOperand& opnd) {                          \
    name(Register::from_code(r1.code()), opnd.rx(), opnd.rb(),             \
         Operand(opnd.offset()));                                          \
  }
  S390_RXE_OPCODE_LIST(DECLARE_S390_RXE_INSTRUCTIONS);
#undef DECLARE_S390_RXE_INSTRUCTIONS


inline void ri_format(Opcode opcode, int f1, int f2) {
  uint32_t op1 = opcode >> 4;
  uint32_t op2 = opcode & 0xf;
  emit4bytes(getfield<uint32_t, 4, 0, 8>(op1) |
             getfield<uint32_t, 4, 8, 12>(f1) |
             getfield<uint32_t, 4, 12, 16>(op2) |
             getfield<uint32_t, 4, 16, 32>(f2));
}

#define DECLARE_S390_RI_A_INSTRUCTIONS(name, op_name, op_value)            \
  void name(Register r, const Operand& i2) {                               \
    DCHECK(is_uint12(op_name));                                            \
    DCHECK(is_uint16(i2.immediate()) || is_int16(i2.immediate()));         \
    ri_format(op_name, r.code(), i2.immediate());                          \
  }
  S390_RI_A_OPCODE_LIST(DECLARE_S390_RI_A_INSTRUCTIONS);
#undef DECLARE_S390_RI_A_INSTRUCTIONS

#define DECLARE_S390_RI_B_INSTRUCTIONS(name, op_name, op_value)            \
  void name(Register r1, const Operand& imm) {                             \
    /* 2nd argument encodes # of halfwords, so divide by 2. */             \
    int16_t numHalfwords = static_cast<int16_t>(imm.immediate()) / 2;      \
    Operand halfwordOp = Operand(numHalfwords);                            \
    halfwordOp.setBits(16);                                                \
    ri_format(op_name, r1.code(), halfwordOp.immediate());                 \
  }
  S390_RI_B_OPCODE_LIST(DECLARE_S390_RI_B_INSTRUCTIONS);
#undef DECLARE_S390_RI_B_INSTRUCTIONS

#define DECLARE_S390_RI_C_INSTRUCTIONS(name, op_name, op_value)            \
  void name(Condition m, const Operand& i2) {                              \
    DCHECK(is_uint12(op_name));                                            \
    DCHECK(is_uint4(m));                                                   \
    DCHECK(op_name == BRC ?                                                \
           is_int16(i2.immediate()) : is_uint16(i2.immediate()));          \
    ri_format(op_name, m, i2.immediate());                                 \
  }
  S390_RI_C_OPCODE_LIST(DECLARE_S390_RI_C_INSTRUCTIONS);
#undef DECLARE_S390_RI_C_INSTRUCTIONS


inline void rrf_format(Opcode op, int f1, int f2, int f3, int f4) {
  uint32_t code = getfield<uint32_t, 4, 0, 16>(op) |
                  getfield<uint32_t, 4, 16, 20>(f1) |
                  getfield<uint32_t, 4, 20, 24>(f2) |
                  getfield<uint32_t, 4, 24, 28>(f3) |
                  getfield<uint32_t, 4, 28, 32>(f4);
  emit4bytes(code);
}

#define DECLARE_S390_RRF_A_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register r1, Condition m4, Register r2, Register r3) {         \
    rrf_format(op_name, r3.code(), m4, r1.code(), r2.code());              \
  }                                                                        \
  void name(Register r1, Register r2, Register r3) {                       \
    name(r1, Condition(0), r2, r3);                                        \
  }
  S390_RRF_A_OPCODE_LIST(DECLARE_S390_RRF_A_INSTRUCTIONS);
#undef DECLARE_S390_RRF_A_INSTRUCTIONS


#define DECLARE_S390_RRF_B_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register r1, Condition m4, Register r2, Register r3) {         \
    rrf_format(op_name, r3.code(), m4, r1.code(), r2.code());              \
  }                                                                        \
  void name(Register r1, Register r2, Register r3) {                       \
    name(r1, Condition(0), r2, r3);                                        \
  }
  S390_RRF_B_OPCODE_LIST(DECLARE_S390_RRF_B_INSTRUCTIONS);
#undef DECLARE_S390_RRF_B_INSTRUCTIONS


#define DECLARE_S390_RRF_C_INSTRUCTIONS(name, op_name, op_value)           \
  template <class R1, class R2>                                            \
  void name(Condition m3, Condition m4, R1 r1, R2 r2) {                    \
    rrf_format(op_name, m3, m4, r1.code(), r2.code());                     \
  }                                                                        \
  template <class R1, class R2>                                            \
  void name(Condition m3, R1 r1, R2 r2) {                                  \
    name(m3, Condition(0), r1, r2);                                        \
  }
  S390_RRF_C_OPCODE_LIST(DECLARE_S390_RRF_C_INSTRUCTIONS);
#undef DECLARE_S390_RRF_C_INSTRUCTIONS


#define DECLARE_S390_RRF_D_INSTRUCTIONS(name, op_name, op_value)           \
  template <class R1, class R2>                                            \
  void name(Condition m3, Condition m4, R1 r1, R2 r2) {                    \
    rrf_format(op_name, m3, m4, r1.code(), r2.code());                     \
  }                                                                        \
  template <class R1, class R2>                                            \
  void name(Condition m3, R1 r1, R2 r2) {                                  \
    name(m3, Condition(0), r1, r2);                                        \
  }
  S390_RRF_D_OPCODE_LIST(DECLARE_S390_RRF_D_INSTRUCTIONS);
#undef DECLARE_S390_RRF_D_INSTRUCTIONS


#define DECLARE_S390_RRF_E_INSTRUCTIONS(name, op_name, op_value)           \
  template <class M3, class M4, class R1, class R2>                        \
  void name(M3 m3, M4 m4, R1 r1, R2 r2) {                                  \
    rrf_format(op_name, m3, m4, r1.code(), r2.code());                     \
  }                                                                        \
  template <class M3, class R1, class R2>                                  \
  void name(M3 m3, R1 r1, R2 r2) {                                         \
    name(m3, Condition(0), r1, r2);                                        \
  }
  S390_RRF_E_OPCODE_LIST(DECLARE_S390_RRF_E_INSTRUCTIONS);
#undef DECLARE_S390_RRF_E_INSTRUCTIONS

enum FIDBRA_FLAGS {
  FIDBRA_CURRENT_ROUNDING_MODE = 0,
  FIDBRA_ROUND_TO_NEAREST_AWAY_FROM_0 = 1,
  // ...
  FIDBRA_ROUND_TOWARD_0 = 5,
  FIDBRA_ROUND_TOWARD_POS_INF = 6,
  FIDBRA_ROUND_TOWARD_NEG_INF = 7
};


inline void rsi_format(Opcode op, int f1, int f2, int f3) {
  DCHECK(is_uint8(op));
  DCHECK(is_uint16(f3) || is_int16(f3));
  uint32_t code = getfield<uint32_t, 4, 0, 8>(op) |
                  getfield<uint32_t, 4, 8, 12>(f1) |
                  getfield<uint32_t, 4, 12, 16>(f2) |
                  getfield<uint32_t, 4, 16, 32>(f3);
  emit4bytes(code);
}

#define DECLARE_S390_RSI_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register r1, Register r3, const Operand& i2) {               \
    rsi_format(op_name, r1.code(), r3.code(), i2.immediate());           \
  }
  S390_RSI_OPCODE_LIST(DECLARE_S390_RSI_INSTRUCTIONS);
#undef DECLARE_S390_RSI_INSTRUCTIONS


inline void rsl_format(Opcode op, uint16_t f1, int f2, int f3, int f4,
                       int f5) {
  DCHECK(is_uint16(op));
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op >> 8) |
                  getfield<uint64_t, 6, 8, 16>(f1) |
                  getfield<uint64_t, 6, 16, 20>(f2) |
                  getfield<uint64_t, 6, 20, 32>(f3) |
                  getfield<uint64_t, 6, 32, 36>(f4) |
                  getfield<uint64_t, 6, 36, 40>(f5) |
                  getfield<uint64_t, 6, 40, 48>(op & 0x00FF);
  emit6bytes(code);
}

#define DECLARE_S390_RSL_A_INSTRUCTIONS(name, op_name, op_value)         \
  void name(const Operand& l1, Register b1, const Operand& d1) {         \
    uint16_t L = static_cast<uint16_t>(l1.immediate() << 8);             \
    rsl_format(op_name, L, b1.code(), d1.immediate(), 0, 0);             \
  }
  S390_RSL_A_OPCODE_LIST(DECLARE_S390_RSL_A_INSTRUCTIONS);
#undef DECLARE_S390_RSL_A_INSTRUCTIONS

#define DECLARE_S390_RSL_B_INSTRUCTIONS(name, op_name, op_value)         \
  void name(const Operand& l2, Register b2, const Operand& d2,           \
            Register r1, Condition m3) {                                 \
    uint16_t L = static_cast<uint16_t>(l2.immediate());                  \
    rsl_format(op_name, L, b2.code(), d2.immediate(), r1.code(), m3);    \
  }
  S390_RSL_B_OPCODE_LIST(DECLARE_S390_RSL_B_INSTRUCTIONS);
#undef DECLARE_S390_RSL_B_INSTRUCTIONS


inline void s_format(Opcode op, int f1, int f2) {
  DCHECK_NE(op & 0xff00, 0);
  DCHECK(is_uint12(f2));
  uint32_t code = getfield<uint32_t, 4, 0, 16>(op) |
                  getfield<uint32_t, 4, 16, 20>(f1) |
                  getfield<uint32_t, 4, 20, 32>(f2);
  emit4bytes(code);
}

#define DECLARE_S390_S_INSTRUCTIONS(name, op_name, op_value)             \
  void name(Register b1, const Operand& d2) {                            \
    Opcode op = op_name;                                                 \
    if ((op & 0xFF00) == 0) {                                            \
      op = (Opcode)(op << 8);                                            \
    }                                                                    \
    s_format(op, b1.code(), d2.immediate());                             \
  }                                                                      \
  void name(const MemOperand& opnd) {                                    \
    Operand d2 = Operand(opnd.getDisplacement());                        \
    name(opnd.getBaseRegister(), d2);                                    \
  }
  S390_S_OPCODE_LIST(DECLARE_S390_S_INSTRUCTIONS);
#undef DECLARE_S390_S_INSTRUCTIONS


inline void si_format(Opcode op, int f1, int f2, int f3) {
  uint32_t code = getfield<uint32_t, 4, 0, 8>(op) |
                  getfield<uint32_t, 4, 8, 16>(f1) |
                  getfield<uint32_t, 4, 16, 20>(f2) |
                  getfield<uint32_t, 4, 20, 32>(f3);
  emit4bytes(code);
}

#define DECLARE_S390_SI_INSTRUCTIONS(name, op_name, op_value)            \
  void name(const Operand& i2, Register b1, const Operand& d1) {         \
    si_format(op_name, i2.immediate(), b1.code(), d1.immediate());       \
  }                                                                      \
  void name(const MemOperand& opnd, const Operand& i2) {                 \
    name(i2, opnd.getBaseRegister(), Operand(opnd.getDisplacement()));   \
  }
  S390_SI_OPCODE_LIST(DECLARE_S390_SI_INSTRUCTIONS);
#undef DECLARE_S390_SI_INSTRUCTIONS


inline void siy_format(Opcode op, int f1, int f2, int f3) {
  DCHECK(is_uint20(f3) || is_int20(f3));
  DCHECK(is_uint16(op));
  DCHECK(is_uint8(f1) || is_int8(f1));
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op >> 8) |
                  getfield<uint64_t, 6, 8, 16>(f1) |
                  getfield<uint64_t, 6, 16, 20>(f2) |
                  getfield<uint64_t, 6, 20, 32>(f3) |
                  getfield<uint64_t, 6, 32, 40>(f3 >> 12) |
                  getfield<uint64_t, 6, 40, 48>(op & 0x00FF);
  emit6bytes(code);
}

#define DECLARE_S390_SIY_INSTRUCTIONS(name, op_name, op_value)           \
  void name(const Operand& i2, Register b1, const Operand& d1) {         \
    siy_format(op_name, i2.immediate(), b1.code(), d1.immediate());      \
  }                                                                      \
  void name(const MemOperand& opnd, const Operand& i2) {                 \
    name(i2, opnd.getBaseRegister(), Operand(opnd.getDisplacement()));   \
  }
  S390_SIY_OPCODE_LIST(DECLARE_S390_SIY_INSTRUCTIONS);
#undef DECLARE_S390_SIY_INSTRUCTIONS


inline void rrs_format(Opcode op, int f1, int f2, int f3, int f4, int f5) {
  DCHECK(is_uint12(f4));
  DCHECK(is_uint16(op));
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op >> 8) |
                  getfield<uint64_t, 6, 8, 12>(f1) |
                  getfield<uint64_t, 6, 12, 16>(f2) |
                  getfield<uint64_t, 6, 16, 20>(f3) |
                  getfield<uint64_t, 6, 20, 32>(f4) |
                  getfield<uint64_t, 6, 32, 36>(f5) |
                  getfield<uint64_t, 6, 40, 48>(op & 0x00FF);
  emit6bytes(code);
}

#define DECLARE_S390_RRS_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register r1, Register r2, Register b4, const Operand& d4,    \
            Condition m3) {                                              \
    rrs_format(op_name, r1.code(), r2.code(), b4.code(), d4.immediate(), \
               m3);                                                      \
  }                                                                      \
  void name(Register r1, Register r2, Condition m3,                      \
            const MemOperand& opnd) {                                    \
    name(r1, r2, opnd.getBaseRegister(),                                 \
         Operand(opnd.getDisplacement()), m3);                           \
  }
  S390_RRS_OPCODE_LIST(DECLARE_S390_RRS_INSTRUCTIONS);
#undef DECLARE_S390_RRS_INSTRUCTIONS


inline void ris_format(Opcode op, int f1, int f2, int f3, int f4, int f5) {
  DCHECK(is_uint12(f3));
  DCHECK(is_uint16(op));
  DCHECK(is_uint8(f5));
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op >> 8) |
                  getfield<uint64_t, 6, 8, 12>(f1) |
                  getfield<uint64_t, 6, 12, 16>(f2) |
                  getfield<uint64_t, 6, 16, 20>(f3) |
                  getfield<uint64_t, 6, 20, 32>(f4) |
                  getfield<uint64_t, 6, 32, 40>(f5) |
                  getfield<uint64_t, 6, 40, 48>(op & 0x00FF);
  emit6bytes(code);
}

#define DECLARE_S390_RIS_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register r1, Condition m3, Register b4, const Operand& d4,   \
                       const Operand& i2) {                              \
    ris_format(op_name, r1.code(), m3, b4.code(), d4.immediate(),        \
               i2.immediate());                                          \
  }                                                                      \
  void name(Register r1, const Operand& i2, Condition m3,                \
                       const MemOperand& opnd) {                         \
    name(r1, m3, opnd.getBaseRegister(),                                 \
         Operand(opnd.getDisplacement()), i2);                           \
  }
  S390_RIS_OPCODE_LIST(DECLARE_S390_RIS_INSTRUCTIONS);
#undef DECLARE_S390_RIS_INSTRUCTIONS


inline void sil_format(Opcode op, int f1, int f2, int f3) {
  DCHECK(is_uint12(f2));
  DCHECK(is_uint16(op));
  DCHECK(is_uint16(f3));
  uint64_t code = getfield<uint64_t, 6, 0, 16>(op) |
                  getfield<uint64_t, 6, 16, 20>(f1) |
                  getfield<uint64_t, 6, 20, 32>(f2) |
                  getfield<uint64_t, 6, 32, 48>(f3);
  emit6bytes(code);
}

#define DECLARE_S390_SIL_INSTRUCTIONS(name, op_name, op_value)           \
  void name(Register b1, const Operand& d1, const Operand& i2) {         \
    sil_format(op_name, b1.code(), d1.immediate(), i2.immediate());      \
  }                                                                      \
  void name(const MemOperand& opnd, const Operand& i2) {                 \
    name(opnd.getBaseRegister(), Operand(opnd.getDisplacement()), i2);   \
  }
  S390_SIL_OPCODE_LIST(DECLARE_S390_SIL_INSTRUCTIONS);
#undef DECLARE_S390_SIL_INSTRUCTIONS


inline void rie_d_format(Opcode opcode, int f1, int f2, int f3, int f4) {
  uint32_t op1 = opcode >> 8;
  uint32_t op2 = opcode & 0xff;
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op1) |
                  getfield<uint64_t, 6, 8, 12>(f1) |
                  getfield<uint64_t, 6, 12, 16>(f2) |
                  getfield<uint64_t, 6, 16, 32>(f3) |
                  getfield<uint64_t, 6, 32, 40>(f4) |
                  getfield<uint64_t, 6, 40, 48>(op2);
  emit6bytes(code);
}

#define DECLARE_S390_RIE_D_INSTRUCTIONS(name, op_name, op_value)         \
  void name(Register r1, Register r3, const Operand& i2) {               \
    rie_d_format(op_name, r1.code(), r3.code(), i2.immediate(), 0);      \
  }
  S390_RIE_D_OPCODE_LIST(DECLARE_S390_RIE_D_INSTRUCTIONS)
#undef DECLARE_S390_RIE_D_INSTRUCTIONS


inline void rie_e_format(Opcode opcode, int f1, int f2, int f3) {
  uint32_t op1 = opcode >> 8;
  uint32_t op2 = opcode & 0xff;
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op1) |
                  getfield<uint64_t, 6, 8, 12>(f1) |
                  getfield<uint64_t, 6, 12, 16>(f2) |
                  getfield<uint64_t, 6, 16, 32>(f3) |
                  getfield<uint64_t, 6, 40, 48>(op2);
  emit6bytes(code);
}

#define DECLARE_S390_RIE_E_INSTRUCTIONS(name, op_name, op_value)         \
  void name(Register r1, Register r3, const Operand& i2) {               \
    rie_e_format(op_name, r1.code(), r3.code(), i2.immediate());         \
  }
  S390_RIE_E_OPCODE_LIST(DECLARE_S390_RIE_E_INSTRUCTIONS)
#undef DECLARE_S390_RIE_E_INSTRUCTIONS


inline void rie_f_format(Opcode opcode, int f1, int f2, int f3, int f4,
                         int f5) {
  uint32_t op1 = opcode >> 8;
  uint32_t op2 = opcode & 0xff;
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op1) |
                  getfield<uint64_t, 6, 8, 12>(f1) |
                  getfield<uint64_t, 6, 12, 16>(f2) |
                  getfield<uint64_t, 6, 16, 24>(f3) |
                  getfield<uint64_t, 6, 24, 32>(f4) |
                  getfield<uint64_t, 6, 32, 40>(f5) |
                  getfield<uint64_t, 6, 40, 48>(op2);
  emit6bytes(code);
}

#define DECLARE_S390_RIE_F_INSTRUCTIONS(name, op_name, op_value)         \
  void name(Register dst, Register src, const Operand& startBit,         \
            const Operand& endBit, const Operand& shiftAmt) {            \
    DCHECK(is_uint8(startBit.immediate()));                              \
    DCHECK(is_uint8(endBit.immediate()));                                \
    DCHECK(is_uint8(shiftAmt.immediate()));                              \
    rie_f_format(op_name, dst.code(), src.code(), startBit.immediate(),  \
                 endBit.immediate(), shiftAmt.immediate());              \
  }
  S390_RIE_F_OPCODE_LIST(DECLARE_S390_RIE_F_INSTRUCTIONS)
#undef DECLARE_S390_RIE_F_INSTRUCTIONS


inline void ss_a_format(Opcode op, int f1, int f2, int f3, int f4, int f5) {
  DCHECK(is_uint12(f5));
  DCHECK(is_uint12(f3));
  DCHECK(is_uint8(f1));
  DCHECK(is_uint8(op));
  uint64_t code = getfield<uint64_t, 6, 0, 8>(op) |
                  getfield<uint64_t, 6, 8, 16>(f1) |
                  getfield<uint64_t, 6, 16, 20>(f2) |
                  getfield<uint64_t, 6, 20, 32>(f3) |
                  getfield<uint64_t, 6, 32, 36>(f4) |
                  getfield<uint64_t, 6, 36, 48>(f5);
  emit6bytes(code);
}

#define DECLARE_S390_SS_A_INSTRUCTIONS(name, op_name, op_value)          \
  void name(Register b1, const Operand& d1, Register b2,                 \
            const Operand& d2, const Operand& length) {                  \
    ss_a_format(op_name, length.immediate(), b1.code(), d1.immediate(),  \
                b2.code(), d2.immediate());                              \
  }                                                                      \
  void name(const MemOperand& opnd1, const MemOperand& opnd2,            \
            const Operand& length) {                                     \
    ss_a_format(op_name, length.immediate(),                             \
                opnd1.getBaseRegister().code(),                          \
                opnd1.getDisplacement(), opnd2.getBaseRegister().code(), \
                opnd2.getDisplacement());                                \
  }
  S390_SS_A_OPCODE_LIST(DECLARE_S390_SS_A_INSTRUCTIONS)
#undef DECLARE_S390_SS_A_INSTRUCTIONS


  // Helper for unconditional branch to Label with update to save register
  void b(Register r, Label* l) {
    int32_t halfwords = branch_offset(l) / 2;
    brasl(r, Operand(halfwords));
  }

  // Conditional Branch Instruction - Generates either BRC / BRCL
  void branchOnCond(Condition c, int branch_offset, bool is_bound = false);

  // Helpers for conditional branch to Label
  void b(Condition cond, Label* l, Label::Distance dist = Label::kFar) {
    branchOnCond(cond, branch_offset(l),
                 l->is_bound() || (dist == Label::kNear));
  }

  void bc_short(Condition cond, Label* l, Label::Distance dist = Label::kFar) {
    b(cond, l, Label::kNear);
  }
  // Helpers for conditional branch to Label
  void beq(Label* l, Label::Distance dist = Label::kFar) { b(eq, l, dist); }
  void bne(Label* l, Label::Distance dist = Label::kFar) { b(ne, l, dist); }
  void blt(Label* l, Label::Distance dist = Label::kFar) { b(lt, l, dist); }
  void ble(Label* l, Label::Distance dist = Label::kFar) { b(le, l, dist); }
  void bgt(Label* l, Label::Distance dist = Label::kFar) { b(gt, l, dist); }
  void bge(Label* l, Label::Distance dist = Label::kFar) { b(ge, l, dist); }
  void b(Label* l, Label::Distance dist = Label::kFar) { b(al, l, dist); }
  void jmp(Label* l, Label::Distance dist = Label::kFar) { b(al, l, dist); }
  void bunordered(Label* l, Label::Distance dist = Label::kFar) {
    b(unordered, l, dist);
  }
  void bordered(Label* l, Label::Distance dist = Label::kFar) {
    b(ordered, l, dist);
  }

  // Helpers for conditional indirect branch off register
  void b(Condition cond, Register r) { bcr(cond, r); }
  void beq(Register r) { b(eq, r); }
  void bne(Register r) { b(ne, r); }
  void blt(Register r) { b(lt, r); }
  void ble(Register r) { b(le, r); }
  void bgt(Register r) { b(gt, r); }
  void bge(Register r) { b(ge, r); }
  void b(Register r) { b(al, r); }
  void jmp(Register r) { b(al, r); }
  void bunordered(Register r) { b(unordered, r); }
  void bordered(Register r) { b(ordered, r); }

  // wrappers around asm instr
  void brxh(Register dst, Register inc, Label* L) {
    int offset_halfwords = branch_offset(L) / 2;
    CHECK(is_int16(offset_halfwords));
    brxh(dst, inc, Operand(offset_halfwords));
  }

  void brxhg(Register dst, Register inc, Label* L) {
    int offset_halfwords = branch_offset(L) / 2;
    CHECK(is_int16(offset_halfwords));
    brxhg(dst, inc, Operand(offset_halfwords));
  }

  template <class R1, class R2>
  void ledbr(R1 r1, R2 r2) {
    ledbra(Condition(0), Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cdfbr(R1 r1, R2 r2) {
    cdfbra(Condition(0), Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cdgbr(R1 r1, R2 r2) {
    cdgbra(Condition(0), Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cegbr(R1 r1, R2 r2) {
    cegbra(Condition(0), Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cgebr(Condition m3, R1 r1, R2 r2) {
    cgebra(m3, Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cgdbr(Condition m3, R1 r1, R2 r2) {
    cgdbra(m3, Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cfdbr(Condition m3, R1 r1, R2 r2) {
    cfdbra(m3, Condition(0), r1, r2);
  }

  template <class R1, class R2>
  void cfebr(Condition m3, R1 r1, R2 r2) {
    cfebra(m3, Condition(0), r1, r2);
  }

  // ---------------------------------------------------------------------------
  // Code generation

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  void breakpoint(bool do_print) {
    if (do_print) {
      PrintF("DebugBreak is inserted to %p\n", static_cast<void*>(pc_));
    }
#if V8_HOST_ARCH_64_BIT
    int64_t value = reinterpret_cast<uint64_t>(&v8::base::OS::DebugBreak);
    int32_t hi_32 = static_cast<int64_t>(value) >> 32;
    int32_t lo_32 = static_cast<int32_t>(value);

    iihf(r1, Operand(hi_32));
    iilf(r1, Operand(lo_32));
#else
    iilf(r1, Operand(reinterpret_cast<uint32_t>(&v8::base::OS::DebugBreak)));
#endif
    basr(r14, r1);
  }

  void call(Handle<Code> target, RelocInfo::Mode rmode);
  void call(CodeStub* stub);
  void jump(Handle<Code> target, RelocInfo::Mode rmode, Condition cond);

// S390 instruction generation
#define DECLARE_VRR_A_INSTRUCTIONS(name, opcode_name, opcode_value)           \
  void name(DoubleRegister v1, DoubleRegister v2, Condition m5, Condition m4, \
            Condition m3) {                                                   \
    uint64_t code = (static_cast<uint64_t>(opcode_value & 0xFF00)) * B32 |    \
                    (static_cast<uint64_t>(v1.code())) * B36 |                \
                    (static_cast<uint64_t>(v2.code())) * B32 |                \
                    (static_cast<uint64_t>(m5 & 0xF)) * B20 |                 \
                    (static_cast<uint64_t>(m4 & 0xF)) * B16 |                 \
                    (static_cast<uint64_t>(m3 & 0xF)) * B12 |                 \
                    (static_cast<uint64_t>(opcode_value & 0x00FF));           \
    emit6bytes(code);                                                         \
  }
  S390_VRR_A_OPCODE_LIST(DECLARE_VRR_A_INSTRUCTIONS)
#undef DECLARE_VRR_A_INSTRUCTIONS

#define DECLARE_VRR_C_INSTRUCTIONS(name, opcode_name, opcode_value)        \
  void name(DoubleRegister v1, DoubleRegister v2, DoubleRegister v3,       \
            Condition m6, Condition m5, Condition m4) {                    \
    uint64_t code = (static_cast<uint64_t>(opcode_value & 0xFF00)) * B32 | \
                    (static_cast<uint64_t>(v1.code())) * B36 |             \
                    (static_cast<uint64_t>(v2.code())) * B32 |             \
                    (static_cast<uint64_t>(v3.code())) * B28 |             \
                    (static_cast<uint64_t>(m6 & 0xF)) * B20 |              \
                    (static_cast<uint64_t>(m5 & 0xF)) * B16 |              \
                    (static_cast<uint64_t>(m4 & 0xF)) * B12 |              \
                    (static_cast<uint64_t>(opcode_value & 0x00FF));        \
    emit6bytes(code);                                                      \
  }
  S390_VRR_C_OPCODE_LIST(DECLARE_VRR_C_INSTRUCTIONS)
#undef DECLARE_VRR_C_INSTRUCTIONS

  // Single Element format
  void vfa(DoubleRegister v1, DoubleRegister v2, DoubleRegister v3) {
    vfa(v1, v2, v3, static_cast<Condition>(0), static_cast<Condition>(8),
        static_cast<Condition>(3));
  }
  void vfs(DoubleRegister v1, DoubleRegister v2, DoubleRegister v3) {
    vfs(v1, v2, v3, static_cast<Condition>(0), static_cast<Condition>(8),
        static_cast<Condition>(3));
  }
  void vfm(DoubleRegister v1, DoubleRegister v2, DoubleRegister v3) {
    vfm(v1, v2, v3, static_cast<Condition>(0), static_cast<Condition>(8),
        static_cast<Condition>(3));
  }
  void vfd(DoubleRegister v1, DoubleRegister v2, DoubleRegister v3) {
    vfd(v1, v2, v3, static_cast<Condition>(0), static_cast<Condition>(8),
        static_cast<Condition>(3));
  }

  // Load Address Instructions
  void larl(Register r, Label* l);

  // Exception-generating instructions and debugging support
  void stop(const char* msg, Condition cond = al,
            int32_t code = kDefaultStopCode, CRegister cr = cr7);

  void bkpt(uint32_t imm16);  // v5 and above

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    GROUP_ENDING_NOP,
    DEBUG_BREAK_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED
  };

  void nop(int type = 0);  // 0 is the default non-marking type.

  void dumy(int r1, int x2, int b2, int d2);

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  // Writes a single byte or word of data in the code stream.  Used
  // for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data);

  void PatchConstantPoolAccessInstruction(int pc_offset, int offset,
                                          ConstantPoolEntry::Access access,
                                          ConstantPoolEntry::Type type) {
    // No embedded constant pool support.
    UNREACHABLE();
  }

  // Read/patch instructions
  SixByteInstr instr_at(int pos) {
    return Instruction::InstructionBits(buffer_ + pos);
  }
  template <typename T>
  void instr_at_put(int pos, T instr) {
    Instruction::SetInstructionBits<T>(buffer_ + pos, instr);
  }

  // Decodes instruction at pos, and returns its length
  int32_t instr_length_at(int pos) {
    return Instruction::InstructionLength(buffer_ + pos);
  }

  static SixByteInstr instr_at(byte* pc) {
    return Instruction::InstructionBits(pc);
  }

  static Condition GetCondition(Instr instr);

  static bool IsBranch(Instr instr);
#if V8_TARGET_ARCH_S390X
  static bool Is64BitLoadIntoIP(SixByteInstr instr1, SixByteInstr instr2);
#else
  static bool Is32BitLoadIntoIP(SixByteInstr instr);
#endif

  static bool IsCmpRegister(Instr instr);
  static bool IsCmpImmediate(Instr instr);
  static bool IsNop(SixByteInstr instr, int type = NON_MARKING_NOP);

  // The code currently calls CheckBuffer() too often. This has the side
  // effect of randomly growing the buffer in the middle of multi-instruction
  // sequences.
  //
  // This function allows outside callers to check and grow the buffer
  void EnsureSpaceFor(int space_needed);

  void EmitRelocations();
  void emit_label_addr(Label* label);

 public:
  byte* buffer_pos() const { return buffer_; }

 protected:
  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode instruction(s) at pos and return backchain to previous
  // label reference or kEndOfChain.
  int target_at(int pos);

  // Patch instruction(s) at pos to target target_pos (e.g. branch)
  void target_at_put(int pos, int target_pos, bool* is_branch = nullptr);

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

 private:
  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512 * MB;

  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static constexpr int kGap = 32;

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static constexpr int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;
  std::vector<DeferredRelocInfo> relocations_;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Code emission
  void CheckBuffer() {
    if (buffer_space() <= kGap) {
      GrowBuffer();
    }
  }
  void GrowBuffer(int needed = 0);
  inline void TrackBranch();
  inline void UntrackBranch();

  // Helper to emit the binary encoding of a 2 byte instruction
  void emit2bytes(uint16_t x) {
    CheckBuffer();
#if V8_TARGET_LITTLE_ENDIAN
    // We need to emit instructions in big endian format as disassembler /
    // simulator require the first byte of the instruction in order to decode
    // the instruction length.  Swap the bytes.
    x = ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
#endif
    *reinterpret_cast<uint16_t*>(pc_) = x;
    pc_ += 2;
  }

  // Helper to emit the binary encoding of a 4 byte instruction
  void emit4bytes(uint32_t x) {
    CheckBuffer();
#if V8_TARGET_LITTLE_ENDIAN
    // We need to emit instructions in big endian format as disassembler /
    // simulator require the first byte of the instruction in order to decode
    // the instruction length.  Swap the bytes.
    x = ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) |
        ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24);
#endif
    *reinterpret_cast<uint32_t*>(pc_) = x;
    pc_ += 4;
  }

  // Helper to emit the binary encoding of a 6 byte instruction
  void emit6bytes(uint64_t x) {
    CheckBuffer();
#if V8_TARGET_LITTLE_ENDIAN
    // We need to emit instructions in big endian format as disassembler /
    // simulator require the first byte of the instruction in order to decode
    // the instruction length.  Swap the bytes.
    x = (static_cast<uint64_t>(x & 0xFF) << 40) |
        (static_cast<uint64_t>((x >> 8) & 0xFF) << 32) |
        (static_cast<uint64_t>((x >> 16) & 0xFF) << 24) |
        (static_cast<uint64_t>((x >> 24) & 0xFF) << 16) |
        (static_cast<uint64_t>((x >> 32) & 0xFF) << 8) |
        (static_cast<uint64_t>((x >> 40) & 0xFF));
    x |= (*reinterpret_cast<uint64_t*>(pc_) >> 48) << 48;
#else
    // We need to pad two bytes of zeros in order to get the 6-bytes
    // stored from low address.
    x = x << 16;
    x |= *reinterpret_cast<uint64_t*>(pc_) & 0xFFFF;
#endif
    // It is safe to store 8-bytes, as CheckBuffer() guarantees we have kGap
    // space left over.
    *reinterpret_cast<uint64_t*>(pc_) = x;
    pc_ += 6;
  }

  // Labels
  void print(Label* L);
  int max_reach_from(int pos);
  void bind_to(Label* L, int pos);
  void next(Label* L);

  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

  friend class RegExpMacroAssemblerS390;
  friend class RelocInfo;
  friend class EnsureSpace;
};

class EnsureSpace {
 public:
  explicit EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_ASSEMBLER_S390_H_

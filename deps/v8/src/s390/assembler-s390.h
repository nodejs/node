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

static_assert(IS_TRIVIALLY_COPYABLE(Register) &&
                  sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DEFINE_REGISTER(R) \
  constexpr Register R = Register::from_code<kRegCode_##R>();
GENERAL_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr Register no_reg = Register::no_reg();

// Register aliases
constexpr Register kLithiumScratch = r1;  // lithium scratch.
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

static_assert(IS_TRIVIALLY_COPYABLE(DoubleRegister) &&
                  sizeof(DoubleRegister) == sizeof(int),
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

#if V8_TARGET_ARCH_S390X
constexpr RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE64;
#else
constexpr RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE32;
#endif

// Class Operand represents a shifter operand in data processing instructions
// defining immediate numbers and masks
typedef uint8_t Length;

struct Mask {
  uint8_t mask;
  uint8_t value() { return mask; }
  static Mask from_value(uint8_t input) {
    DCHECK_LE(input, 0x0F);
    Mask m = {input};
    return m;
  }
};

class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(intptr_t immediate,
                          RelocInfo::Mode rmode = kRelocInfo_NONEPTR)
         : rmode_(rmode)) {
    value_.immediate = immediate;
  }
  INLINE(static Operand Zero()) { return Operand(static_cast<intptr_t>(0)); }
  INLINE(explicit Operand(const ExternalReference& f)
         : rmode_(RelocInfo::EXTERNAL_REFERENCE)) {
    value_.immediate = reinterpret_cast<intptr_t>(f.address());
  }
  explicit Operand(Handle<HeapObject> handle);
  INLINE(explicit Operand(Smi* value) : rmode_(kRelocInfo_NONEPTR)) {
    value_.immediate = reinterpret_cast<intptr_t>(value);
  }

  // rm
  INLINE(explicit Operand(Register rm));

  static Operand EmbeddedNumber(double value);  // Smi or HeapNumber

  // Return true if this is a register operand.
  INLINE(bool is_reg() const) { return rm_.is_valid(); }

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
class MemOperand BASE_EMBEDDED {
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

class Assembler : public AssemblerBase {
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
  Assembler(Isolate* isolate, void* buffer, int buffer_size)
      : Assembler(IsolateData(isolate), buffer, buffer_size) {}
  Assembler(IsolateData isolate_data, void* buffer, int buffer_size);
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
  INLINE(static Address target_address_at(Address pc, Address constant_pool));
  INLINE(static void set_target_address_at(
      Isolate* isolate, Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // Given the address of the beginning of a call, return the address
  // in the instruction stream that the call will return to.
  INLINE(static Address return_address_from_call_start(Address pc));

  inline Handle<Object> code_target_object_handle_at(Address pc);
  // This sets the branch destination.
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Isolate* isolate, Address instruction_payload, Code* code,
      Address target);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Isolate* isolate, Address pc, Address target,
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

  static inline int encode_crbit(const CRegister& cr, enum CRBit crbit) {
    return ((cr.code() * CRWIDTH) + crbit);
  }

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

  // Declare generic instruction formats by fields
  inline void e_format(Opcode opcode) {
    emit2bytes(getfield<uint16_t, 2, 0, 16>(opcode));
  }

  inline void i_format(Opcode opcode, int f1) {
    emit2bytes(getfield<uint16_t, 2, 0, 8>(opcode) |
               getfield<uint16_t, 2, 8, 16>(f1));
  }

  inline void ie_format(Opcode opcode, int f1, int f2) {
    emit4bytes(getfield<uint32_t, 4, 0, 16>(opcode) |
               getfield<uint32_t, 4, 24, 28>(f1) |
               getfield<uint32_t, 4, 28, 32>(f2));
  }
  inline void mii_format(Opcode opcode, int f1, int f2, int f3) {
    emit6bytes(
        getfield<uint64_t, 6, 0, 8>(opcode) | getfield<uint64_t, 6, 8, 12>(f1) |
        getfield<uint64_t, 6, 12, 24>(f2) | getfield<uint64_t, 6, 24, 48>(f3));
  }

  inline void ri_format(Opcode opcode, int f1, int f2) {
    uint32_t op1 = opcode >> 4;
    uint32_t op2 = opcode & 0xf;
    emit4bytes(
        getfield<uint32_t, 4, 0, 8>(op1) | getfield<uint32_t, 4, 8, 12>(f1) |
        getfield<uint32_t, 4, 12, 16>(op2) | getfield<uint32_t, 4, 16, 32>(f2));
  }

  inline void rie_1_format(Opcode opcode, int f1, int f2, int f3, int f4) {
    uint32_t op1 = opcode >> 8;
    uint32_t op2 = opcode & 0xff;
    emit6bytes(
        getfield<uint64_t, 6, 0, 8>(op1) | getfield<uint64_t, 6, 8, 12>(f1) |
        getfield<uint64_t, 6, 12, 16>(f2) | getfield<uint64_t, 6, 16, 32>(f3) |
        getfield<uint64_t, 6, 32, 36>(f4) | getfield<uint64_t, 6, 40, 48>(op2));
  }

  inline void rie_2_format(Opcode opcode, int f1, int f2, int f3, int f4) {
    uint32_t op1 = opcode >> 8;
    uint32_t op2 = opcode & 0xff;
    emit6bytes(
        getfield<uint64_t, 6, 0, 8>(op1) | getfield<uint64_t, 6, 8, 12>(f1) |
        getfield<uint64_t, 6, 12, 16>(f2) | getfield<uint64_t, 6, 16, 32>(f3) |
        getfield<uint64_t, 6, 32, 40>(f4) | getfield<uint64_t, 6, 40, 48>(op2));
  }

  inline void rie_3_format(Opcode opcode, int f1, int f2, int f3, int f4,
                           int f5) {
    uint32_t op1 = opcode >> 8;
    uint32_t op2 = opcode & 0xff;
    emit6bytes(
        getfield<uint64_t, 6, 0, 8>(op1) | getfield<uint64_t, 6, 8, 12>(f1) |
        getfield<uint64_t, 6, 12, 16>(f2) | getfield<uint64_t, 6, 16, 24>(f3) |
        getfield<uint64_t, 6, 24, 32>(f4) | getfield<uint64_t, 6, 32, 40>(f5) |
        getfield<uint64_t, 6, 40, 48>(op2));
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

  inline void ris_format(Opcode opcode, int f1, int f2, int f3, int f4,
                         int f5) {
    uint32_t op1 = opcode >> 8;
    uint32_t op2 = opcode & 0xff;
    emit6bytes(
        getfield<uint64_t, 6, 0, 8>(op1) | getfield<uint64_t, 6, 8, 12>(f1) |
        getfield<uint64_t, 6, 12, 16>(f2) | getfield<uint64_t, 6, 16, 20>(f3) |
        getfield<uint64_t, 6, 20, 32>(f4) | getfield<uint64_t, 6, 32, 40>(f5) |
        getfield<uint64_t, 6, 40, 48>(op2));
  }

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

  inline void rrf_format(Opcode opcode, int f1, int f2, int f3, int f4) {
    emit4bytes(
        getfield<uint32_t, 4, 0, 16>(opcode) |
        getfield<uint32_t, 4, 16, 20>(f1) | getfield<uint32_t, 4, 20, 24>(f2) |
        getfield<uint32_t, 4, 24, 28>(f3) | getfield<uint32_t, 4, 28, 32>(f4));
  }

#define DECLARE_S390_RX_INSTRUCTIONS(name, op_name, op_value)        \
  template <class R1>                                                \
  inline void name(R1 r1, Register x2, Register b2, Disp d2) {       \
    rx_format(op_name, r1.code(), x2.code(), b2.code(), d2);         \
  }                                                                  \
  template <class R1>                                                \
  inline void name(R1 r1, const MemOperand& opnd) {                  \
    name(r1, opnd.getIndexRegister(),                                \
         opnd.getBaseRegister(), opnd.getDisplacement());            \
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
       opnd.getBaseRegister(), opnd.getDisplacement());
  }
  void bc(Condition cond, Register x2, Register b2, Disp d2) {
    rx_format(BC, cond, x2.code(), b2.code(), d2);
  }
#undef DECLARE_S390_RX_INSTRUCTIONS

#define DECLARE_S390_RXY_INSTRUCTIONS(name, op_name, op_value)       \
  template <class R1, class R2>                                      \
  inline void name(R1 r1, R2 r2, Register b2, Disp d2) {             \
    rxy_format(op_name, r1.code(), r2.code(), b2.code(), d2);        \
  }                                                                  \
  template <class R1>                                                \
  inline void name(R1 r1, const MemOperand& opnd) {                  \
    name(r1, opnd.getIndexRegister(),                                \
         opnd.getBaseRegister(), opnd.getDisplacement());            \
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
        opnd.getBaseRegister(), opnd.getDisplacement());
  }
  void pfd(Condition cond, Register x2, Register b2, Disp d2) {
    rxy_format(PFD, cond, x2.code(), b2.code(), d2);
  }
#undef DECLARE_S390_RXY_INSTRUCTIONS

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
#define I_FORM(name) void name(const Operand& i)

#define RR_FORM(name) void name(Register r1, Register r2)

#define RR2_FORM(name) void name(Condition m1, Register r2)

#define RI1_FORM(name) void name(Register r, const Operand& i)

#define RI2_FORM(name) void name(Condition m, const Operand& i)

#define RIE_FORM(name) void name(Register r1, Register R3, const Operand& i)

#define RIE_F_FORM(name)                                                    \
  void name(Register r1, Register r2, const Operand& i3, const Operand& i4, \
            const Operand& i5)

#define RXE_FORM(name)                            \
  void name(Register r1, const MemOperand& opnd); \
  void name(Register r1, Register b2, Register x2, Disp d2)

#define RXF_FORM(name)                                         \
  void name(Register r1, Register r3, const MemOperand& opnd); \
  void name(Register r1, Register r3, Register b2, Register x2, Disp d2)

#define RSI_FORM(name) void name(Register r1, Register r3, const Operand& i)

#define RIS_FORM(name)                                       \
  void name(Register r1, Condition m3, Register b4, Disp d4, \
            const Operand& i2);                              \
  void name(Register r1, const Operand& i2, Condition m3,    \
            const MemOperand& opnd)

#define SI_FORM(name)                                  \
  void name(const MemOperand& opnd, const Operand& i); \
  void name(const Operand& i2, Register b1, Disp d1)

#define SIL_FORM(name)                                \
  void name(Register b1, Disp d1, const Operand& i2); \
  void name(const MemOperand& opnd, const Operand& i2)

#define RRF1_FORM(name) void name(Register r1, Register r2, Register r3)

#define RRF2_FORM(name) void name(Condition m1, Register r1, Register r2)

#define RRF3_FORM(name) \
  void name(Register r3, Condition m4, Register r1, Register r2)

#define RS1_FORM(name)                                         \
  void name(Register r1, Register r3, const MemOperand& opnd); \
  void name(Register r1, Register r3, Register b2, Disp d2)

#define RS2_FORM(name)                                          \
  void name(Register r1, Condition m3, const MemOperand& opnd); \
  void name(Register r1, Condition m3, Register b2, Disp d2)

#define RSE_FORM(name)                                         \
  void name(Register r1, Register r3, const MemOperand& opnd); \
  void name(Register r1, Register r3, Register b2, Disp d2)

#define RSL_FORM(name)                       \
  void name(Length l, Register b2, Disp d2); \
  void name(const MemOperand& opnd)

#define RSY1_FORM(name)                                      \
  void name(Register r1, Register r3, Register b2, Disp d2); \
  void name(Register r1, Register r3, const MemOperand& opnd)

#define RSY2_FORM(name)                                       \
  void name(Register r1, Condition m3, Register b2, Disp d2); \
  void name(Register r1, Condition m3, const MemOperand& opnd)

#define RRS_FORM(name)                                                     \
  void name(Register r1, Register r2, Register b4, Disp d4, Condition m3); \
  void name(Register r1, Register r2, Condition m3, const MemOperand& opnd)

#define S_FORM(name)               \
  void name(Register b2, Disp d2); \
  void name(const MemOperand& opnd)

#define SIY_FORM(name)                                \
  void name(const Operand& i2, Register b1, Disp d1); \
  void name(const MemOperand& opnd, const Operand& i)

#define SS1_FORM(name)                                                  \
  void name(Register b1, Disp d1, Register b3, Disp d2, Length length); \
  void name(const MemOperand& opnd1, const MemOperand& opnd2, Length length)

#define SS2_FORM(name)                                                        \
  void name(const MemOperand& opnd1, const MemOperand& opnd2, Length length1, \
            Length length2);                                                  \
  void name(Register b1, Disp d1, Register b2, Disp d2, Length l1, Length l2)

#define SS3_FORM(name)                                                        \
  void name(const MemOperand& opnd1, const MemOperand& opnd2, Length length); \
  void name(const Operand& i3, Register b1, Disp d1, Register b2, Disp d2,    \
            Length l1)

#define SS4_FORM(name)                                                   \
  void name(const MemOperand& opnd1, const MemOperand& opnd2);           \
  void name(Register r1, Register r3, Register b1, Disp d1, Register b2, \
            Disp d2)

#define SS5_FORM(name)                                                   \
  void name(const MemOperand& opnd1, const MemOperand& opnd2);           \
  void name(Register r1, Register r3, Register b3, Disp d2, Register b4, \
            Disp d4)

#define SSE_FORM(name)                                   \
  void name(Register b1, Disp d1, Register b2, Disp d2); \
  void name(const MemOperand& opnd1, const MemOperand& opnd2)

#define SSF_FORM(name)                                                \
  void name(Register r3, Register b1, Disp d1, Register b2, Disp d2); \
  void name(Register r3, const MemOperand& opnd1, const MemOperand& opnd2)

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

  // S390 instruction sets
  RXE_FORM(ddb);
  SS1_FORM(ed);
  RRF2_FORM(fidbr);
  RI1_FORM(iihh);
  RI1_FORM(iihl);
  RI1_FORM(iilh);
  RI1_FORM(iill);
  RSY1_FORM(loc);
  RXE_FORM(mdb);
  SS4_FORM(mvck);
  SSF_FORM(mvcos);
  SS4_FORM(mvcs);
  SS1_FORM(mvn);
  SS1_FORM(nc);
  SI_FORM(ni);
  RI1_FORM(nilh);
  RI1_FORM(nill);
  RI1_FORM(oill);
  RXE_FORM(sdb);
  RS1_FORM(srdl);
  RI1_FORM(tmll);
  SS1_FORM(tr);
  S_FORM(ts);

  // Load Address Instructions
  void larl(Register r, Label* l);

  // Load Instructions
  void lhi(Register r, const Operand& imm);
  void lghi(Register r, const Operand& imm);

  // Load Multiple Instructions
  void lm(Register r1, Register r2, const MemOperand& src);
  void lmy(Register r1, Register r2, const MemOperand& src);
  void lmg(Register r1, Register r2, const MemOperand& src);

  // Load On Condition Instructions
  void locr(Condition m3, Register r1, Register r2);
  void locgr(Condition m3, Register r1, Register r2);
  void loc(Condition m3, Register r1, const MemOperand& src);
  void locg(Condition m3, Register r1, const MemOperand& src);

  // Store Instructions

  // Store Multiple Instructions
  void stm(Register r1, Register r2, const MemOperand& src);
  void stmy(Register r1, Register r2, const MemOperand& src);
  void stmg(Register r1, Register r2, const MemOperand& src);

  // Compare Instructions
  void chi(Register r, const Operand& opnd);
  void cghi(Register r, const Operand& opnd);

  // Compare Logical Instructions
  void cli(const MemOperand& mem, const Operand& imm);
  void cliy(const MemOperand& mem, const Operand& imm);
  void clc(const MemOperand& opnd1, const MemOperand& opnd2, Length length);

  // Compare and Swap Instructions
  void cs(Register r1, Register r2, const MemOperand& src);
  void csy(Register r1, Register r2, const MemOperand& src);
  void csg(Register r1, Register r2, const MemOperand& src);

  // Test Under Mask Instructions
  void tm(const MemOperand& mem, const Operand& imm);
  void tmy(const MemOperand& mem, const Operand& imm);

  // Rotate Instructions
  void rll(Register r1, Register r3, Register opnd);
  void rll(Register r1, Register r3, const Operand& opnd);
  void rll(Register r1, Register r3, Register r2, const Operand& opnd);
  void rllg(Register r1, Register r3, const Operand& opnd);
  void rllg(Register r1, Register r3, const Register opnd);
  void rllg(Register r1, Register r3, Register r2, const Operand& opnd);

  // Shift Instructions (32)
  void sll(Register r1, Register opnd);
  void sll(Register r1, const Operand& opnd);
  void sllk(Register r1, Register r3, Register opnd);
  void sllk(Register r1, Register r3, const Operand& opnd);
  void srl(Register r1, Register opnd);
  void srl(Register r1, const Operand& opnd);
  void srlk(Register r1, Register r3, Register opnd);
  void srlk(Register r1, Register r3, const Operand& opnd);
  void sra(Register r1, Register opnd);
  void sra(Register r1, const Operand& opnd);
  void srak(Register r1, Register r3, Register opnd);
  void srak(Register r1, Register r3, const Operand& opnd);
  void sla(Register r1, Register opnd);
  void sla(Register r1, const Operand& opnd);
  void slak(Register r1, Register r3, Register opnd);
  void slak(Register r1, Register r3, const Operand& opnd);

  // Shift Instructions (64)
  void sllg(Register r1, Register r3, const Operand& opnd);
  void sllg(Register r1, Register r3, const Register opnd);
  void srlg(Register r1, Register r3, const Operand& opnd);
  void srlg(Register r1, Register r3, const Register opnd);
  void srag(Register r1, Register r3, const Operand& opnd);
  void srag(Register r1, Register r3, const Register opnd);
  void srda(Register r1, const Operand& opnd);
  void srdl(Register r1, const Operand& opnd);
  void slag(Register r1, Register r3, const Operand& opnd);
  void slag(Register r1, Register r3, const Register opnd);
  void sldl(Register r1, Register b2, const Operand& opnd);
  void srdl(Register r1, Register b2, const Operand& opnd);
  void srda(Register r1, Register b2, const Operand& opnd);

  // Rotate and Insert Selected Bits
  void risbg(Register dst, Register src, const Operand& startBit,
             const Operand& endBit, const Operand& shiftAmt,
             bool zeroBits = true);
  void risbgn(Register dst, Register src, const Operand& startBit,
              const Operand& endBit, const Operand& shiftAmt,
              bool zeroBits = true);

  // Move Character (Mem to Mem)
  void mvc(const MemOperand& opnd1, const MemOperand& opnd2, uint32_t length);

  // Branch Instructions
  void bras(Register r, const Operand& opnd);
  void brc(Condition c, const Operand& opnd);
  void brct(Register r1, const Operand& opnd);
  void brctg(Register r1, const Operand& opnd);

  // 32-bit Add Instructions
  void ahi(Register r1, const Operand& opnd);
  void ahik(Register r1, Register r3, const Operand& opnd);
  void ark(Register r1, Register r2, Register r3);
  void asi(const MemOperand&, const Operand&);

  // 64-bit Add Instructions
  void aghi(Register r1, const Operand& opnd);
  void aghik(Register r1, Register r3, const Operand& opnd);
  void agrk(Register r1, Register r2, Register r3);
  void agsi(const MemOperand&, const Operand&);

  // 32-bit Add Logical Instructions
  void alrk(Register r1, Register r2, Register r3);

  // 64-bit Add Logical Instructions
  void algrk(Register r1, Register r2, Register r3);

  // 32-bit Subtract Instructions
  void srk(Register r1, Register r2, Register r3);

  // 64-bit Subtract Instructions
  void sgrk(Register r1, Register r2, Register r3);

  // 32-bit Subtract Logical Instructions
  void slrk(Register r1, Register r2, Register r3);

  // 64-bit Subtract Logical Instructions
  void slgrk(Register r1, Register r2, Register r3);

  // 32-bit Multiply Instructions
  void mhi(Register r1, const Operand& opnd);
  void msrkc(Register r1, Register r2, Register r3);
  void msgrkc(Register r1, Register r2, Register r3);

  // 64-bit Multiply Instructions
  void mghi(Register r1, const Operand& opnd);

  // Bitwise Instructions (AND / OR / XOR)
  void nrk(Register r1, Register r2, Register r3);
  void ngrk(Register r1, Register r2, Register r3);
  void ork(Register r1, Register r2, Register r3);
  void ogrk(Register r1, Register r2, Register r3);
  void xrk(Register r1, Register r2, Register r3);
  void xgrk(Register r1, Register r2, Register r3);
  void xc(const MemOperand& opnd1, const MemOperand& opnd2, Length length);

  // Floating <-> Fixed Point Conversion Instructions
  void cdlfbr(Condition m3, Condition m4, DoubleRegister fltReg,
              Register fixReg);
  void cdlgbr(Condition m3, Condition m4, DoubleRegister fltReg,
              Register fixReg);
  void celgbr(Condition m3, Condition m4, DoubleRegister fltReg,
              Register fixReg);
  void celfbr(Condition m3, Condition m4, DoubleRegister fltReg,
              Register fixReg);
  void clfdbr(Condition m3, Condition m4, Register fixReg,
              DoubleRegister fltReg);
  void clfebr(Condition m3, Condition m4, Register fixReg,
              DoubleRegister fltReg);
  void clgdbr(Condition m3, Condition m4, Register fixReg,
              DoubleRegister fltReg);
  void clgebr(Condition m3, Condition m4, Register fixReg,
              DoubleRegister fltReg);
  void cfdbr(Condition m, Register fixReg, DoubleRegister fltReg);
  void cgebr(Condition m, Register fixReg, DoubleRegister fltReg);
  void cgdbr(Condition m, Register fixReg, DoubleRegister fltReg);
  void cfebr(Condition m3, Register fixReg, DoubleRegister fltReg);
  void cefbr(Condition m3, DoubleRegister fltReg, Register fixReg);

  // Floating Point Compare Instructions
  void cdb(DoubleRegister r1, const MemOperand& opnd);
  void ceb(DoubleRegister r1, const MemOperand& opnd);

  // Floating Point Arithmetic Instructions
  void adb(DoubleRegister r1, const MemOperand& opnd);
  void aeb(DoubleRegister r1, const MemOperand& opnd);
  void sdb(DoubleRegister r1, const MemOperand& opnd);
  void seb(DoubleRegister r1, const MemOperand& opnd);
  void mdb(DoubleRegister r1, const MemOperand& opnd);
  void meeb(DoubleRegister r1, const MemOperand& opnd);
  void ddb(DoubleRegister r1, const MemOperand& opnd);
  void deb(DoubleRegister r1, const MemOperand& opnd);
  void sqdb(DoubleRegister r1, const MemOperand& opnd);
  void ldeb(DoubleRegister r1, const MemOperand& opnd);

  enum FIDBRA_MASK3 {
    FIDBRA_CURRENT_ROUNDING_MODE = 0,
    FIDBRA_ROUND_TO_NEAREST_AWAY_FROM_0 = 1,
    // ...
    FIDBRA_ROUND_TOWARD_0 = 5,
    FIDBRA_ROUND_TOWARD_POS_INF = 6,
    FIDBRA_ROUND_TOWARD_NEG_INF = 7
  };
  void fiebra(DoubleRegister d1, DoubleRegister d2, FIDBRA_MASK3 m3);
  void fidbra(DoubleRegister d1, DoubleRegister d2, FIDBRA_MASK3 m3);

  // Move integer
  void mvhi(const MemOperand& opnd1, const Operand& i2);
  void mvghi(const MemOperand& opnd1, const Operand& i2);

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
  void RequestHeapObject(HeapObjectRequest request);

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

  inline int32_t emit_code_target(
      Handle<Code> target, RelocInfo::Mode rmode);

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

  // Helpers to emit binary encoding for various instruction formats.

  inline void rr2_form(uint8_t op, Condition m1, Register r2);

  inline void ri_form(Opcode op, Register r1, const Operand& i2);
  inline void ri_form(Opcode op, Condition m1, const Operand& i2);

  inline void rie_form(Opcode op, Register r1, Register r3, const Operand& i2);
  inline void rie_f_form(Opcode op, Register r1, Register r2, const Operand& i3,
                         const Operand& i4, const Operand& i5);

  inline void ris_form(Opcode op, Register r1, Condition m3, Register b4,
                       Disp d4, const Operand& i2);

  inline void rrf1_form(Opcode op, Register r1, Register r2, Register r3);
  inline void rrf1_form(uint32_t x);
  inline void rrf2_form(uint32_t x);
  inline void rrf3_form(uint32_t x);
  inline void rrfe_form(Opcode op, Condition m3, Condition m4, Register r1,
                        Register r2);

  inline void rrs_form(Opcode op, Register r1, Register r2, Register b4,
                       Disp d4, Condition m3);

  inline void rs_form(Opcode op, Register r1, Condition m3, Register b2,
                      const Disp d2);
  inline void rs_form(Opcode op, Register r1, Register r3, Register b2,
                      const Disp d2);

  inline void rsi_form(Opcode op, Register r1, Register r3, const Operand& i2);
  inline void rsl_form(Opcode op, Length l1, Register b2, Disp d2);

  inline void rsy_form(Opcode op, Register r1, Register r3, Register b2,
                       const Disp d2);
  inline void rsy_form(Opcode op, Register r1, Condition m3, Register b2,
                       const Disp d2);

  inline void rxe_form(Opcode op, Register r1, Register x2, Register b2,
                       Disp d2);

  inline void rxf_form(Opcode op, Register r1, Register r3, Register b2,
                       Register x2, Disp d2);

  inline void s_form(Opcode op, Register b1, Disp d2);

  inline void si_form(Opcode op, const Operand& i2, Register b1, Disp d1);
  inline void siy_form(Opcode op, const Operand& i2, Register b1, Disp d1);

  inline void sil_form(Opcode op, Register b1, Disp d1, const Operand& i2);

  inline void ss_form(Opcode op, Length l, Register b1, Disp d1, Register b2,
                      Disp d2);
  inline void ss_form(Opcode op, Length l1, Length l2, Register b1, Disp d1,
                      Register b2, Disp d2);
  inline void ss_form(Opcode op, Length l1, const Operand& i3, Register b1,
                      Disp d1, Register b2, Disp d2);
  inline void ss_form(Opcode op, Register r1, Register r2, Register b1, Disp d1,
                      Register b2, Disp d2);
  inline void sse_form(Opcode op, Register b1, Disp d1, Register b2, Disp d2);
  inline void ssf_form(Opcode op, Register r3, Register b1, Disp d1,
                       Register b2, Disp d2);

  // Labels
  void print(Label* L);
  int max_reach_from(int pos);
  void bind_to(Label* L, int pos);
  void next(Label* L);

  // The following functions help with avoiding allocations of embedded heap
  // objects during the code assembly phase. {RequestHeapObject} records the
  // need for a future heap number allocation or code stub generation. After
  // code assembly, {AllocateAndInstallRequestedHeapObjects} will allocate these
  // objects and place them where they are expected (determined by the pc offset
  // associated with each request). That is, for each request, it will patch the
  // dummy heap object handle that we emitted during code assembly with the
  // actual heap object handle.
  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);
  std::forward_list<HeapObjectRequest> heap_object_requests_;

  friend class RegExpMacroAssemblerS390;
  friend class RelocInfo;

  std::vector<Handle<Code>> code_targets_;
  friend class EnsureSpace;
};

class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_ASSEMBLER_S390_H_

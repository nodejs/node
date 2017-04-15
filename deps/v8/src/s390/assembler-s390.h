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

struct Register {
  enum Code {
#define REGISTER_CODE(R) kCode_##R,
    GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
        kAfterLast,
    kCode_no_reg = -1
  };
  static const int kNumRegisters = Code::kAfterLast;

#define REGISTER_COUNT(R) 1 +
  static const int kNumAllocatable =
      ALLOCATABLE_GENERAL_REGISTERS(REGISTER_COUNT) 0;
#undef REGISTER_COUNT

#define REGISTER_BIT(R) 1 << kCode_##R |
  static const RegList kAllocatable =
      ALLOCATABLE_GENERAL_REGISTERS(REGISTER_BIT) 0;
#undef REGISTER_BIT

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

  void set_code(int code) {
    reg_code = code;
    DCHECK(is_valid());
  }

#if V8_TARGET_LITTLE_ENDIAN
  static const int kMantissaOffset = 0;
  static const int kExponentOffset = 4;
#else
  static const int kMantissaOffset = 4;
  static const int kExponentOffset = 0;
#endif

  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

typedef struct Register Register;

#define DECLARE_REGISTER(R) const Register R = {Register::kCode_##R};
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
const Register no_reg = {Register::kCode_no_reg};

// Register aliases
const Register kLithiumScratch = r1;  // lithium scratch.
const Register kRootRegister = r10;   // Roots array pointer.
const Register cp = r13;              // JavaScript context pointer.

static const bool kSimpleFPAliasing = true;

// Double word FP register.
struct DoubleRegister {
  enum Code {
#define REGISTER_CODE(R) kCode_##R,
    DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
        kAfterLast,
    kCode_no_reg = -1
  };

  static const int kNumRegisters = Code::kAfterLast;
  static const int kMaxNumRegisters = kNumRegisters;

  bool is_valid() const { return 0 <= reg_code && reg_code < kNumRegisters; }
  bool is(DoubleRegister reg) const { return reg_code == reg.reg_code; }

  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }

  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }

  static DoubleRegister from_code(int code) {
    DoubleRegister r = {code};
    return r;
  }

  int reg_code;
};

typedef DoubleRegister FloatRegister;

// TODO(john.yan) Define SIMD registers.
typedef DoubleRegister Simd128Register;

#define DECLARE_REGISTER(R) \
  const DoubleRegister R = {DoubleRegister::kCode_##R};
DOUBLE_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
const Register no_dreg = {Register::kCode_no_reg};

// Aliases for double registers.  Defined using #define instead of
// "static const DoubleRegister&" because Clang complains otherwise when a
// compilation unit that includes this header doesn't use the variables.
#define kDoubleRegZero d14
#define kScratchDoubleReg d13

Register ToRegister(int num);

// Coprocessor register
struct CRegister {
  bool is_valid() const { return 0 <= reg_code && reg_code < 8; }
  bool is(CRegister creg) const { return reg_code == creg.reg_code; }
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

const CRegister no_creg = {-1};

const CRegister cr0 = {0};
const CRegister cr1 = {1};
const CRegister cr2 = {2};
const CRegister cr3 = {3};
const CRegister cr4 = {4};
const CRegister cr5 = {5};
const CRegister cr6 = {6};
const CRegister cr7 = {7};

// -----------------------------------------------------------------------------
// Machine instruction Operands

#if V8_TARGET_ARCH_S390X
const RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE64;
#else
const RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE32;
#endif

// Class Operand represents a shifter operand in data processing instructions
// defining immediate numbers and masks
typedef uint8_t Length;

struct Mask {
  uint8_t mask;
  uint8_t value() { return mask; }
  static Mask from_value(uint8_t input) {
    DCHECK(input <= 0x0F);
    Mask m = {input};
    return m;
  }
};

class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(intptr_t immediate,
                          RelocInfo::Mode rmode = kRelocInfo_NONEPTR));
  INLINE(static Operand Zero()) { return Operand(static_cast<intptr_t>(0)); }
  INLINE(explicit Operand(const ExternalReference& f));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // rm
  INLINE(explicit Operand(Register rm));

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  bool must_output_reloc_info(const Assembler* assembler) const;

  inline intptr_t immediate() const {
    DCHECK(!rm_.is_valid());
    return imm_;
  }

  inline void setBits(int n) {
    imm_ = (static_cast<uint32_t>(imm_) << (32 - n)) >> (32 - n);
  }

  Register rm() const { return rm_; }

 private:
  Register rm_;
  intptr_t imm_;  // valid if rm_ == no_reg
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
    DCHECK(!baseRegister.is(no_reg));
    return baseRegister;
  }

  Register getBaseRegister() const { return rb(); }

  // Index Register
  Register rx() const {
    DCHECK(!indexRegister.is(no_reg));
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
  // If the provided buffer is NULL, the assembler allocates and grows its own
  // buffer, and buffer_size determines the initial buffer size. The buffer is
  // owned by the assembler and deallocated upon destruction of the assembler.
  //
  // If the provided buffer is not NULL, the assembler uses the provided buffer
  // for code generation and assumes its size to be buffer_size. If the buffer
  // is too small, a fatal error occurs. No deallocation of the buffer is done
  // upon destruction of the assembler.
  Assembler(Isolate* isolate, void* buffer, int buffer_size);
  virtual ~Assembler() {}

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
  INLINE(static Address target_address_at(Address pc, Address constant_pool));
  INLINE(static void set_target_address_at(
      Isolate* isolate, Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));
  INLINE(static Address target_address_at(Address pc, Code* code)) {
    Address constant_pool = NULL;
    return target_address_at(pc, constant_pool);
  }
  INLINE(static void set_target_address_at(
      Isolate* isolate, Address pc, Code* code, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED)) {
    Address constant_pool = NULL;
    set_target_address_at(isolate, pc, constant_pool, target,
                          icache_flush_mode);
  }

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
  static const int kSpecialTargetSize = 0;

// Number of bytes for instructions used to store pointer sized constant.
#if V8_TARGET_ARCH_S390X
  static const int kBytesForPtrConstant = 12;  // IIHF + IILF
#else
  static const int kBytesForPtrConstant = 6;  // IILF
#endif

  // Distance between the instruction referring to the address of the call
  // target and the return address.

  // Offset between call target address and return address
  // for BRASL calls
  // Patch will be appiled to other FIXED_SEQUENCE call
  static const int kCallTargetAddressOffset = 6;

// The length of FIXED_SEQUENCE call
// iihf    r8, <address_hi>  // <64-bit only>
// iilf    r8, <address_lo>
// basr    r14, r8
#if V8_TARGET_ARCH_S390X
  static const int kCallSequenceLength = 14;
#else
  static const int kCallSequenceLength = 8;
#endif

  // This is the length of the BreakLocationIterator::SetDebugBreakAtReturn()
  // code patch FIXED_SEQUENCE in bytes!
  // JS Return Sequence = Call Sequence + BKPT
  // static const int kJSReturnSequenceLength = kCallSequenceLength + 2;

  // This is the length of the code sequence from SetDebugBreakAtSlot()
  // FIXED_SEQUENCE in bytes!
  static const int kDebugBreakSlotLength = kCallSequenceLength;
  static const int kPatchDebugBreakSlotReturnOffset = kCallTargetAddressOffset;

  // Length to patch between the start of the JS return sequence
  // from SetDebugBreakAtReturn and the address from
  // break_address_from_return_address.
  //
  // frame->pc() in Debug::SetAfterBreakTarget will point to BKPT in
  // JS return sequence, so the length to patch will not include BKPT
  // instruction length.
  // static const int kPatchReturnSequenceAddressOffset =
  //     kCallSequenceLength - kPatchDebugBreakSlotReturnOffset;

  // Length to patch between the start of the FIXED call sequence from
  // SetDebugBreakAtSlot() and the the address from
  // break_address_from_return_address.
  static const int kPatchDebugBreakSlotAddressOffset =
      kDebugBreakSlotLength - kPatchDebugBreakSlotReturnOffset;

  static inline int encode_crbit(const CRegister& cr, enum CRBit crbit) {
    return ((cr.code() * CRWIDTH) + crbit);
  }

  // ---------------------------------------------------------------------------
  // Code generation

  template <class T, int size, int lo, int hi>
  inline T getfield(T value) {
    DCHECK(lo < hi);
    DCHECK(size > 0);
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

  void call(Handle<Code> target, RelocInfo::Mode rmode,
            TypeFeedbackId ast_id = TypeFeedbackId::None());
  void jump(Handle<Code> target, RelocInfo::Mode rmode, Condition cond);

// S390 instruction generation
#define I_FORM(name) void name(const Operand& i)

#define RR_FORM(name) void name(Register r1, Register r2)

#define RR2_FORM(name) void name(Condition m1, Register r2)

#define RX_FORM(name)                                        \
  void name(Register r1, Register x2, Register b2, Disp d2); \
  void name(Register r1, const MemOperand& opnd)

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

#define RXY_FORM(name)                                       \
  void name(Register r1, Register x2, Register b2, Disp d2); \
  void name(Register r1, const MemOperand& opnd)

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
  RX_FORM(bc);
  RX_FORM(cd);
  RXE_FORM(cdb);
  RXE_FORM(ceb);
  RXE_FORM(ddb);
  SS1_FORM(ed);
  RX_FORM(ex);
  RRF2_FORM(fidbr);
  RX_FORM(ic_z);
  RXY_FORM(icy);
  RI1_FORM(iihh);
  RI1_FORM(iihl);
  RI1_FORM(iilh);
  RI1_FORM(iill);
  RX_FORM(le_z);
  RXY_FORM(ley);
  RSY1_FORM(loc);
  RXY_FORM(lrv);
  RXY_FORM(lrvh);
  RXY_FORM(lrvg);
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
  RXY_FORM(pfd);
  RXE_FORM(sdb);
  RXY_FORM(slgf);
  RS1_FORM(srdl);
  RX_FORM(ste);
  RXY_FORM(stey);
  RXY_FORM(strv);
  RXY_FORM(strvh);
  RXY_FORM(strvg);
  RI1_FORM(tmll);
  SS1_FORM(tr);
  S_FORM(ts);

  // Load Address Instructions
  void la(Register r, const MemOperand& opnd);
  void lay(Register r, const MemOperand& opnd);
  void larl(Register r, Label* l);

  // Load Instructions
  void lb(Register r, const MemOperand& src);
  void lgb(Register r, const MemOperand& src);
  void lh(Register r, const MemOperand& src);
  void lhy(Register r, const MemOperand& src);
  void lgh(Register r, const MemOperand& src);
  void l(Register r, const MemOperand& src);
  void ly(Register r, const MemOperand& src);
  void lg(Register r, const MemOperand& src);
  void lgf(Register r, const MemOperand& src);
  void lhi(Register r, const Operand& imm);
  void lghi(Register r, const Operand& imm);

  // Load And Test Instructions
  void lt_z(Register r, const MemOperand& src);
  void ltg(Register r, const MemOperand& src);

  // Load Logical Instructions
  void llc(Register r, const MemOperand& src);
  void llgc(Register r, const MemOperand& src);
  void llgf(Register r, const MemOperand& src);
  void llh(Register r, const MemOperand& src);
  void llgh(Register r, const MemOperand& src);

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
  void st(Register r, const MemOperand& src);
  void stc(Register r, const MemOperand& src);
  void stcy(Register r, const MemOperand& src);
  void stg(Register r, const MemOperand& src);
  void sth(Register r, const MemOperand& src);
  void sthy(Register r, const MemOperand& src);
  void sty(Register r, const MemOperand& src);

  // Store Multiple Instructions
  void stm(Register r1, Register r2, const MemOperand& src);
  void stmy(Register r1, Register r2, const MemOperand& src);
  void stmg(Register r1, Register r2, const MemOperand& src);

  // Compare Instructions
  void c(Register r, const MemOperand& opnd);
  void cy(Register r, const MemOperand& opnd);
  void cg(Register r, const MemOperand& opnd);
  void ch(Register r, const MemOperand& opnd);
  void chy(Register r, const MemOperand& opnd);
  void chi(Register r, const Operand& opnd);
  void cghi(Register r, const Operand& opnd);

  // Compare Logical Instructions
  void cl(Register r, const MemOperand& opnd);
  void cly(Register r, const MemOperand& opnd);
  void clg(Register r, const MemOperand& opnd);
  void cli(const MemOperand& mem, const Operand& imm);
  void cliy(const MemOperand& mem, const Operand& imm);
  void clc(const MemOperand& opnd1, const MemOperand& opnd2, Length length);

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
  void bct(Register r, const MemOperand& opnd);
  void bctg(Register r, const MemOperand& opnd);
  void bras(Register r, const Operand& opnd);
  void brc(Condition c, const Operand& opnd);
  void brct(Register r1, const Operand& opnd);
  void brctg(Register r1, const Operand& opnd);

  // 32-bit Add Instructions
  void a(Register r1, const MemOperand& opnd);
  void ay(Register r1, const MemOperand& opnd);
  void ah(Register r1, const MemOperand& opnd);
  void ahy(Register r1, const MemOperand& opnd);
  void ahi(Register r1, const Operand& opnd);
  void ahik(Register r1, Register r3, const Operand& opnd);
  void ark(Register r1, Register r2, Register r3);
  void asi(const MemOperand&, const Operand&);

  // 64-bit Add Instructions
  void ag(Register r1, const MemOperand& opnd);
  void agf(Register r1, const MemOperand& opnd);
  void aghi(Register r1, const Operand& opnd);
  void aghik(Register r1, Register r3, const Operand& opnd);
  void agrk(Register r1, Register r2, Register r3);
  void agsi(const MemOperand&, const Operand&);

  // 32-bit Add Logical Instructions
  void al_z(Register r1, const MemOperand& opnd);
  void aly(Register r1, const MemOperand& opnd);
  void alrk(Register r1, Register r2, Register r3);

  // 64-bit Add Logical Instructions
  void alg(Register r1, const MemOperand& opnd);
  void algrk(Register r1, Register r2, Register r3);

  // 32-bit Subtract Instructions
  void s(Register r1, const MemOperand& opnd);
  void sy(Register r1, const MemOperand& opnd);
  void sh(Register r1, const MemOperand& opnd);
  void shy(Register r1, const MemOperand& opnd);
  void srk(Register r1, Register r2, Register r3);

  // 64-bit Subtract Instructions
  void sg(Register r1, const MemOperand& opnd);
  void sgf(Register r1, const MemOperand& opnd);
  void sgrk(Register r1, Register r2, Register r3);

  // 32-bit Subtract Logical Instructions
  void sl(Register r1, const MemOperand& opnd);
  void sly(Register r1, const MemOperand& opnd);
  void slrk(Register r1, Register r2, Register r3);

  // 64-bit Subtract Logical Instructions
  void slg(Register r1, const MemOperand& opnd);
  void slgrk(Register r1, Register r2, Register r3);

  // 32-bit Multiply Instructions
  void m(Register r1, const MemOperand& opnd);
  void mfy(Register r1, const MemOperand& opnd);
  void ml(Register r1, const MemOperand& opnd);
  void ms(Register r1, const MemOperand& opnd);
  void msy(Register r1, const MemOperand& opnd);
  void mh(Register r1, const MemOperand& opnd);
  void mhy(Register r1, const MemOperand& opnd);
  void mhi(Register r1, const Operand& opnd);
  void msrkc(Register r1, Register r2, Register r3);
  void msgrkc(Register r1, Register r2, Register r3);

  // 64-bit Multiply Instructions
  void mlg(Register r1, const MemOperand& opnd);
  void mghi(Register r1, const Operand& opnd);
  void msg(Register r1, const MemOperand& opnd);

  // 32-bit Divide Instructions
  void d(Register r1, const MemOperand& opnd);
  void dl(Register r1, const MemOperand& opnd);

  // Bitwise Instructions (AND / OR / XOR)
  void n(Register r1, const MemOperand& opnd);
  void ny(Register r1, const MemOperand& opnd);
  void nrk(Register r1, Register r2, Register r3);
  void ng(Register r1, const MemOperand& opnd);
  void ngrk(Register r1, Register r2, Register r3);
  void o(Register r1, const MemOperand& opnd);
  void oy(Register r1, const MemOperand& opnd);
  void ork(Register r1, Register r2, Register r3);
  void og(Register r1, const MemOperand& opnd);
  void ogrk(Register r1, Register r2, Register r3);
  void x(Register r1, const MemOperand& opnd);
  void xy(Register r1, const MemOperand& opnd);
  void xrk(Register r1, Register r2, Register r3);
  void xg(Register r1, const MemOperand& opnd);
  void xgrk(Register r1, Register r2, Register r3);
  void xc(const MemOperand& opnd1, const MemOperand& opnd2, Length length);

  // Floating Point Load / Store Instructions
  void ld(DoubleRegister r1, const MemOperand& opnd);
  void ldy(DoubleRegister r1, const MemOperand& opnd);
  void le_z(DoubleRegister r1, const MemOperand& opnd);
  void ley(DoubleRegister r1, const MemOperand& opnd);
  void std(DoubleRegister r1, const MemOperand& opnd);
  void stdy(DoubleRegister r1, const MemOperand& opnd);
  void ste(DoubleRegister r1, const MemOperand& opnd);
  void stey(DoubleRegister r1, const MemOperand& opnd);

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

  // Floating Point Arithmetic Instructions
  void adb(DoubleRegister r1, const MemOperand& opnd);
  void sdb(DoubleRegister r1, const MemOperand& opnd);
  void mdb(DoubleRegister r1, const MemOperand& opnd);
  void ddb(DoubleRegister r1, const MemOperand& opnd);
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

  // Debugging

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot(RelocInfo::Mode mode);

  // Record the AST id of the CallIC being compiled, so that it can be placed
  // in the relocation information.
  void SetRecordedAstId(TypeFeedbackId ast_id) { recorded_ast_id_ = ast_id; }

  TypeFeedbackId RecordedAstId() {
    // roohack - another issue??? DCHECK(!recorded_ast_id_.IsNone());
    return recorded_ast_id_;
  }

  void ClearRecordedAstId() { recorded_ast_id_ = TypeFeedbackId::None(); }

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
  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  TypeFeedbackId recorded_ast_id_;

  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode instruction(s) at pos and return backchain to previous
  // label reference or kEndOfChain.
  int target_at(int pos);

  // Patch instruction(s) at pos to target target_pos (e.g. branch)
  void target_at_put(int pos, int target_pos, bool* is_branch = nullptr);

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

 private:
  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static const int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;
  std::vector<DeferredRelocInfo> relocations_;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Code emission
  inline void CheckBuffer();
  void GrowBuffer(int needed = 0);
  inline void TrackBranch();
  inline void UntrackBranch();

  inline int32_t emit_code_target(
      Handle<Code> target, RelocInfo::Mode rmode,
      TypeFeedbackId ast_id = TypeFeedbackId::None());

  // Helpers to emit binary encoding of 2/4/6 byte instructions.
  inline void emit2bytes(uint16_t x);
  inline void emit4bytes(uint32_t x);
  inline void emit6bytes(uint64_t x);

  // Helpers to emit binary encoding for various instruction formats.

  inline void rr2_form(uint8_t op, Condition m1, Register r2);

  inline void rx_form(Opcode op, Register r1, Register x2, Register b2,
                      Disp d2);
  inline void rx_form(Opcode op, DoubleRegister r1, Register x2, Register b2,
                      Disp d2);

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

  inline void rxy_form(Opcode op, Register r1, Register x2, Register b2,
                       Disp d2);
  inline void rxy_form(Opcode op, Register r1, Condition m3, Register b2,
                       Disp d2);
  inline void rxy_form(Opcode op, DoubleRegister r1, Register x2, Register b2,
                       Disp d2);

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

  friend class RegExpMacroAssemblerS390;
  friend class RelocInfo;
  friend class CodePatcher;

  List<Handle<Code> > code_targets_;
  friend class EnsureSpace;
};

class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_ASSEMBLER_S390_H_

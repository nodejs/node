// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_ASSEMBLER_ARM64_H_
#define V8_ARM64_ASSEMBLER_ARM64_H_

#include <list>
#include <map>
#include <vector>

#include "src/arm64/instructions-arm64.h"
#include "src/assembler.h"
#include "src/globals.h"
#include "src/serialize.h"
#include "src/utils.h"


namespace v8 {
namespace internal {


// -----------------------------------------------------------------------------
// Registers.
#define REGISTER_CODE_LIST(R)                                                  \
R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)                                 \
R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)                                \
R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23)                                \
R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)


static const int kRegListSizeInBits = sizeof(RegList) * kBitsPerByte;


// Some CPURegister methods can return Register and FPRegister types, so we
// need to declare them in advance.
struct Register;
struct FPRegister;


struct CPURegister {
  enum RegisterType {
    // The kInvalid value is used to detect uninitialized static instances,
    // which are always zero-initialized before any constructors are called.
    kInvalid = 0,
    kRegister,
    kFPRegister,
    kNoRegister
  };

  static CPURegister Create(unsigned code, unsigned size, RegisterType type) {
    CPURegister r = {code, size, type};
    return r;
  }

  unsigned code() const;
  RegisterType type() const;
  RegList Bit() const;
  unsigned SizeInBits() const;
  int SizeInBytes() const;
  bool Is32Bits() const;
  bool Is64Bits() const;
  bool IsValid() const;
  bool IsValidOrNone() const;
  bool IsValidRegister() const;
  bool IsValidFPRegister() const;
  bool IsNone() const;
  bool Is(const CPURegister& other) const;
  bool Aliases(const CPURegister& other) const;

  bool IsZero() const;
  bool IsSP() const;

  bool IsRegister() const;
  bool IsFPRegister() const;

  Register X() const;
  Register W() const;
  FPRegister D() const;
  FPRegister S() const;

  bool IsSameSizeAndType(const CPURegister& other) const;

  // V8 compatibility.
  bool is(const CPURegister& other) const { return Is(other); }
  bool is_valid() const { return IsValid(); }

  unsigned reg_code;
  unsigned reg_size;
  RegisterType reg_type;
};


struct Register : public CPURegister {
  static Register Create(unsigned code, unsigned size) {
    return Register(CPURegister::Create(code, size, CPURegister::kRegister));
  }

  Register() {
    reg_code = 0;
    reg_size = 0;
    reg_type = CPURegister::kNoRegister;
  }

  explicit Register(const CPURegister& r) {
    reg_code = r.reg_code;
    reg_size = r.reg_size;
    reg_type = r.reg_type;
    DCHECK(IsValidOrNone());
  }

  Register(const Register& r) {  // NOLINT(runtime/explicit)
    reg_code = r.reg_code;
    reg_size = r.reg_size;
    reg_type = r.reg_type;
    DCHECK(IsValidOrNone());
  }

  bool IsValid() const {
    DCHECK(IsRegister() || IsNone());
    return IsValidRegister();
  }

  static Register XRegFromCode(unsigned code);
  static Register WRegFromCode(unsigned code);

  // Start of V8 compatibility section ---------------------
  // These memebers are necessary for compilation.
  // A few of them may be unused for now.

  static const int kNumRegisters = kNumberOfRegisters;
  static int NumRegisters() { return kNumRegisters; }

  // We allow crankshaft to use the following registers:
  //   - x0 to x15
  //   - x18 to x24
  //   - x27 (also context)
  //
  // TODO(all): Register x25 is currently free and could be available for
  // crankshaft, but we don't use it as we might use it as a per function
  // literal pool pointer in the future.
  //
  // TODO(all): Consider storing cp in x25 to have only two ranges.
  // We split allocatable registers in three ranges called
  //   - "low range"
  //   - "high range"
  //   - "context"
  static const unsigned kAllocatableLowRangeBegin = 0;
  static const unsigned kAllocatableLowRangeEnd = 15;
  static const unsigned kAllocatableHighRangeBegin = 18;
  static const unsigned kAllocatableHighRangeEnd = 24;
  static const unsigned kAllocatableContext = 27;

  // Gap between low and high ranges.
  static const int kAllocatableRangeGapSize =
      (kAllocatableHighRangeBegin - kAllocatableLowRangeEnd) - 1;

  static const int kMaxNumAllocatableRegisters =
      (kAllocatableLowRangeEnd - kAllocatableLowRangeBegin + 1) +
      (kAllocatableHighRangeEnd - kAllocatableHighRangeBegin + 1) + 1;  // cp
  static int NumAllocatableRegisters() { return kMaxNumAllocatableRegisters; }

  // Return true if the register is one that crankshaft can allocate.
  bool IsAllocatable() const {
    return ((reg_code == kAllocatableContext) ||
            (reg_code <= kAllocatableLowRangeEnd) ||
            ((reg_code >= kAllocatableHighRangeBegin) &&
             (reg_code <= kAllocatableHighRangeEnd)));
  }

  static Register FromAllocationIndex(unsigned index) {
    DCHECK(index < static_cast<unsigned>(NumAllocatableRegisters()));
    // cp is the last allocatable register.
    if (index == (static_cast<unsigned>(NumAllocatableRegisters() - 1))) {
      return from_code(kAllocatableContext);
    }

    // Handle low and high ranges.
    return (index <= kAllocatableLowRangeEnd)
        ? from_code(index)
        : from_code(index + kAllocatableRangeGapSize);
  }

  static const char* AllocationIndexToString(int index) {
    DCHECK((index >= 0) && (index < NumAllocatableRegisters()));
    DCHECK((kAllocatableLowRangeBegin == 0) &&
           (kAllocatableLowRangeEnd == 15) &&
           (kAllocatableHighRangeBegin == 18) &&
           (kAllocatableHighRangeEnd == 24) &&
           (kAllocatableContext == 27));
    const char* const names[] = {
      "x0", "x1", "x2", "x3", "x4",
      "x5", "x6", "x7", "x8", "x9",
      "x10", "x11", "x12", "x13", "x14",
      "x15", "x18", "x19", "x20", "x21",
      "x22", "x23", "x24", "x27",
    };
    return names[index];
  }

  static int ToAllocationIndex(Register reg) {
    DCHECK(reg.IsAllocatable());
    unsigned code = reg.code();
    if (code == kAllocatableContext) {
      return NumAllocatableRegisters() - 1;
    }

    return (code <= kAllocatableLowRangeEnd)
        ? code
        : code - kAllocatableRangeGapSize;
  }

  static Register from_code(int code) {
    // Always return an X register.
    return Register::Create(code, kXRegSizeInBits);
  }

  // End of V8 compatibility section -----------------------
};


struct FPRegister : public CPURegister {
  static FPRegister Create(unsigned code, unsigned size) {
    return FPRegister(
        CPURegister::Create(code, size, CPURegister::kFPRegister));
  }

  FPRegister() {
    reg_code = 0;
    reg_size = 0;
    reg_type = CPURegister::kNoRegister;
  }

  explicit FPRegister(const CPURegister& r) {
    reg_code = r.reg_code;
    reg_size = r.reg_size;
    reg_type = r.reg_type;
    DCHECK(IsValidOrNone());
  }

  FPRegister(const FPRegister& r) {  // NOLINT(runtime/explicit)
    reg_code = r.reg_code;
    reg_size = r.reg_size;
    reg_type = r.reg_type;
    DCHECK(IsValidOrNone());
  }

  bool IsValid() const {
    DCHECK(IsFPRegister() || IsNone());
    return IsValidFPRegister();
  }

  static FPRegister SRegFromCode(unsigned code);
  static FPRegister DRegFromCode(unsigned code);

  // Start of V8 compatibility section ---------------------
  static const int kMaxNumRegisters = kNumberOfFPRegisters;

  // Crankshaft can use all the FP registers except:
  //   - d15 which is used to keep the 0 double value
  //   - d30 which is used in crankshaft as a double scratch register
  //   - d31 which is used in the MacroAssembler as a double scratch register
  static const unsigned kAllocatableLowRangeBegin = 0;
  static const unsigned kAllocatableLowRangeEnd = 14;
  static const unsigned kAllocatableHighRangeBegin = 16;
  static const unsigned kAllocatableHighRangeEnd = 28;

  static const RegList kAllocatableFPRegisters = 0x1fff7fff;

  // Gap between low and high ranges.
  static const int kAllocatableRangeGapSize =
      (kAllocatableHighRangeBegin - kAllocatableLowRangeEnd) - 1;

  static const int kMaxNumAllocatableRegisters =
      (kAllocatableLowRangeEnd - kAllocatableLowRangeBegin + 1) +
      (kAllocatableHighRangeEnd - kAllocatableHighRangeBegin + 1);
  static int NumAllocatableRegisters() { return kMaxNumAllocatableRegisters; }

  // TODO(turbofan): Proper float32 support.
  static int NumAllocatableAliasedRegisters() {
    return NumAllocatableRegisters();
  }

  // Return true if the register is one that crankshaft can allocate.
  bool IsAllocatable() const {
    return (Bit() & kAllocatableFPRegisters) != 0;
  }

  static FPRegister FromAllocationIndex(unsigned int index) {
    DCHECK(index < static_cast<unsigned>(NumAllocatableRegisters()));

    return (index <= kAllocatableLowRangeEnd)
        ? from_code(index)
        : from_code(index + kAllocatableRangeGapSize);
  }

  static const char* AllocationIndexToString(int index) {
    DCHECK((index >= 0) && (index < NumAllocatableRegisters()));
    DCHECK((kAllocatableLowRangeBegin == 0) &&
           (kAllocatableLowRangeEnd == 14) &&
           (kAllocatableHighRangeBegin == 16) &&
           (kAllocatableHighRangeEnd == 28));
    const char* const names[] = {
      "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
      "d8", "d9", "d10", "d11", "d12", "d13", "d14",
      "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
      "d24", "d25", "d26", "d27", "d28"
    };
    return names[index];
  }

  static int ToAllocationIndex(FPRegister reg) {
    DCHECK(reg.IsAllocatable());
    unsigned code = reg.code();

    return (code <= kAllocatableLowRangeEnd)
        ? code
        : code - kAllocatableRangeGapSize;
  }

  static FPRegister from_code(int code) {
    // Always return a D register.
    return FPRegister::Create(code, kDRegSizeInBits);
  }
  // End of V8 compatibility section -----------------------
};


STATIC_ASSERT(sizeof(CPURegister) == sizeof(Register));
STATIC_ASSERT(sizeof(CPURegister) == sizeof(FPRegister));


#if defined(ARM64_DEFINE_REG_STATICS)
#define INITIALIZE_REGISTER(register_class, name, code, size, type)      \
  const CPURegister init_##register_class##_##name = {code, size, type}; \
  const register_class& name = *reinterpret_cast<const register_class*>( \
                                    &init_##register_class##_##name)
#define ALIAS_REGISTER(register_class, alias, name)                       \
  const register_class& alias = *reinterpret_cast<const register_class*>( \
                                     &init_##register_class##_##name)
#else
#define INITIALIZE_REGISTER(register_class, name, code, size, type) \
  extern const register_class& name
#define ALIAS_REGISTER(register_class, alias, name) \
  extern const register_class& alias
#endif  // defined(ARM64_DEFINE_REG_STATICS)

// No*Reg is used to indicate an unused argument, or an error case. Note that
// these all compare equal (using the Is() method). The Register and FPRegister
// variants are provided for convenience.
INITIALIZE_REGISTER(Register, NoReg, 0, 0, CPURegister::kNoRegister);
INITIALIZE_REGISTER(FPRegister, NoFPReg, 0, 0, CPURegister::kNoRegister);
INITIALIZE_REGISTER(CPURegister, NoCPUReg, 0, 0, CPURegister::kNoRegister);

// v8 compatibility.
INITIALIZE_REGISTER(Register, no_reg, 0, 0, CPURegister::kNoRegister);

#define DEFINE_REGISTERS(N)                                                  \
  INITIALIZE_REGISTER(Register, w##N, N,                                     \
                      kWRegSizeInBits, CPURegister::kRegister);              \
  INITIALIZE_REGISTER(Register, x##N, N,                                     \
                      kXRegSizeInBits, CPURegister::kRegister);
REGISTER_CODE_LIST(DEFINE_REGISTERS)
#undef DEFINE_REGISTERS

INITIALIZE_REGISTER(Register, wcsp, kSPRegInternalCode, kWRegSizeInBits,
                    CPURegister::kRegister);
INITIALIZE_REGISTER(Register, csp, kSPRegInternalCode, kXRegSizeInBits,
                    CPURegister::kRegister);

#define DEFINE_FPREGISTERS(N)                                                  \
  INITIALIZE_REGISTER(FPRegister, s##N, N,                                     \
                      kSRegSizeInBits, CPURegister::kFPRegister);              \
  INITIALIZE_REGISTER(FPRegister, d##N, N,                                     \
                      kDRegSizeInBits, CPURegister::kFPRegister);
REGISTER_CODE_LIST(DEFINE_FPREGISTERS)
#undef DEFINE_FPREGISTERS

#undef INITIALIZE_REGISTER

// Registers aliases.
ALIAS_REGISTER(Register, ip0, x16);
ALIAS_REGISTER(Register, ip1, x17);
ALIAS_REGISTER(Register, wip0, w16);
ALIAS_REGISTER(Register, wip1, w17);
// Root register.
ALIAS_REGISTER(Register, root, x26);
ALIAS_REGISTER(Register, rr, x26);
// Context pointer register.
ALIAS_REGISTER(Register, cp, x27);
// We use a register as a JS stack pointer to overcome the restriction on the
// architectural SP alignment.
// We chose x28 because it is contiguous with the other specific purpose
// registers.
STATIC_ASSERT(kJSSPCode == 28);
ALIAS_REGISTER(Register, jssp, x28);
ALIAS_REGISTER(Register, wjssp, w28);
ALIAS_REGISTER(Register, fp, x29);
ALIAS_REGISTER(Register, lr, x30);
ALIAS_REGISTER(Register, xzr, x31);
ALIAS_REGISTER(Register, wzr, w31);

// Keeps the 0 double value.
ALIAS_REGISTER(FPRegister, fp_zero, d15);
// Crankshaft double scratch register.
ALIAS_REGISTER(FPRegister, crankshaft_fp_scratch, d29);
// MacroAssembler double scratch registers.
ALIAS_REGISTER(FPRegister, fp_scratch, d30);
ALIAS_REGISTER(FPRegister, fp_scratch1, d30);
ALIAS_REGISTER(FPRegister, fp_scratch2, d31);

#undef ALIAS_REGISTER


Register GetAllocatableRegisterThatIsNotOneOf(Register reg1,
                                              Register reg2 = NoReg,
                                              Register reg3 = NoReg,
                                              Register reg4 = NoReg);


// AreAliased returns true if any of the named registers overlap. Arguments set
// to NoReg are ignored. The system stack pointer may be specified.
bool AreAliased(const CPURegister& reg1,
                const CPURegister& reg2,
                const CPURegister& reg3 = NoReg,
                const CPURegister& reg4 = NoReg,
                const CPURegister& reg5 = NoReg,
                const CPURegister& reg6 = NoReg,
                const CPURegister& reg7 = NoReg,
                const CPURegister& reg8 = NoReg);

// AreSameSizeAndType returns true if all of the specified registers have the
// same size, and are of the same type. The system stack pointer may be
// specified. Arguments set to NoReg are ignored, as are any subsequent
// arguments. At least one argument (reg1) must be valid (not NoCPUReg).
bool AreSameSizeAndType(const CPURegister& reg1,
                        const CPURegister& reg2,
                        const CPURegister& reg3 = NoCPUReg,
                        const CPURegister& reg4 = NoCPUReg,
                        const CPURegister& reg5 = NoCPUReg,
                        const CPURegister& reg6 = NoCPUReg,
                        const CPURegister& reg7 = NoCPUReg,
                        const CPURegister& reg8 = NoCPUReg);


typedef FPRegister DoubleRegister;


// -----------------------------------------------------------------------------
// Lists of registers.
class CPURegList {
 public:
  explicit CPURegList(CPURegister reg1,
                      CPURegister reg2 = NoCPUReg,
                      CPURegister reg3 = NoCPUReg,
                      CPURegister reg4 = NoCPUReg)
      : list_(reg1.Bit() | reg2.Bit() | reg3.Bit() | reg4.Bit()),
        size_(reg1.SizeInBits()), type_(reg1.type()) {
    DCHECK(AreSameSizeAndType(reg1, reg2, reg3, reg4));
    DCHECK(IsValid());
  }

  CPURegList(CPURegister::RegisterType type, unsigned size, RegList list)
      : list_(list), size_(size), type_(type) {
    DCHECK(IsValid());
  }

  CPURegList(CPURegister::RegisterType type, unsigned size,
             unsigned first_reg, unsigned last_reg)
      : size_(size), type_(type) {
    DCHECK(((type == CPURegister::kRegister) &&
            (last_reg < kNumberOfRegisters)) ||
           ((type == CPURegister::kFPRegister) &&
            (last_reg < kNumberOfFPRegisters)));
    DCHECK(last_reg >= first_reg);
    list_ = (1UL << (last_reg + 1)) - 1;
    list_ &= ~((1UL << first_reg) - 1);
    DCHECK(IsValid());
  }

  CPURegister::RegisterType type() const {
    DCHECK(IsValid());
    return type_;
  }

  RegList list() const {
    DCHECK(IsValid());
    return list_;
  }

  inline void set_list(RegList new_list) {
    DCHECK(IsValid());
    list_ = new_list;
  }

  // Combine another CPURegList into this one. Registers that already exist in
  // this list are left unchanged. The type and size of the registers in the
  // 'other' list must match those in this list.
  void Combine(const CPURegList& other);

  // Remove every register in the other CPURegList from this one. Registers that
  // do not exist in this list are ignored. The type of the registers in the
  // 'other' list must match those in this list.
  void Remove(const CPURegList& other);

  // Variants of Combine and Remove which take CPURegisters.
  void Combine(const CPURegister& other);
  void Remove(const CPURegister& other1,
              const CPURegister& other2 = NoCPUReg,
              const CPURegister& other3 = NoCPUReg,
              const CPURegister& other4 = NoCPUReg);

  // Variants of Combine and Remove which take a single register by its code;
  // the type and size of the register is inferred from this list.
  void Combine(int code);
  void Remove(int code);

  // Remove all callee-saved registers from the list. This can be useful when
  // preparing registers for an AAPCS64 function call, for example.
  void RemoveCalleeSaved();

  CPURegister PopLowestIndex();
  CPURegister PopHighestIndex();

  // AAPCS64 callee-saved registers.
  static CPURegList GetCalleeSaved(unsigned size = kXRegSizeInBits);
  static CPURegList GetCalleeSavedFP(unsigned size = kDRegSizeInBits);

  // AAPCS64 caller-saved registers. Note that this includes lr.
  static CPURegList GetCallerSaved(unsigned size = kXRegSizeInBits);
  static CPURegList GetCallerSavedFP(unsigned size = kDRegSizeInBits);

  // Registers saved as safepoints.
  static CPURegList GetSafepointSavedRegisters();

  bool IsEmpty() const {
    DCHECK(IsValid());
    return list_ == 0;
  }

  bool IncludesAliasOf(const CPURegister& other1,
                       const CPURegister& other2 = NoCPUReg,
                       const CPURegister& other3 = NoCPUReg,
                       const CPURegister& other4 = NoCPUReg) const {
    DCHECK(IsValid());
    RegList list = 0;
    if (!other1.IsNone() && (other1.type() == type_)) list |= other1.Bit();
    if (!other2.IsNone() && (other2.type() == type_)) list |= other2.Bit();
    if (!other3.IsNone() && (other3.type() == type_)) list |= other3.Bit();
    if (!other4.IsNone() && (other4.type() == type_)) list |= other4.Bit();
    return (list_ & list) != 0;
  }

  int Count() const {
    DCHECK(IsValid());
    return CountSetBits(list_, kRegListSizeInBits);
  }

  unsigned RegisterSizeInBits() const {
    DCHECK(IsValid());
    return size_;
  }

  unsigned RegisterSizeInBytes() const {
    int size_in_bits = RegisterSizeInBits();
    DCHECK((size_in_bits % kBitsPerByte) == 0);
    return size_in_bits / kBitsPerByte;
  }

  unsigned TotalSizeInBytes() const {
    DCHECK(IsValid());
    return RegisterSizeInBytes() * Count();
  }

 private:
  RegList list_;
  unsigned size_;
  CPURegister::RegisterType type_;

  bool IsValid() const {
    const RegList kValidRegisters = 0x8000000ffffffff;
    const RegList kValidFPRegisters = 0x0000000ffffffff;
    switch (type_) {
      case CPURegister::kRegister:
        return (list_ & kValidRegisters) == list_;
      case CPURegister::kFPRegister:
        return (list_ & kValidFPRegisters) == list_;
      case CPURegister::kNoRegister:
        return list_ == 0;
      default:
        UNREACHABLE();
        return false;
    }
  }
};


// AAPCS64 callee-saved registers.
#define kCalleeSaved CPURegList::GetCalleeSaved()
#define kCalleeSavedFP CPURegList::GetCalleeSavedFP()


// AAPCS64 caller-saved registers. Note that this includes lr.
#define kCallerSaved CPURegList::GetCallerSaved()
#define kCallerSavedFP CPURegList::GetCallerSavedFP()

// -----------------------------------------------------------------------------
// Immediates.
class Immediate {
 public:
  template<typename T>
  inline explicit Immediate(Handle<T> handle);

  // This is allowed to be an implicit constructor because Immediate is
  // a wrapper class that doesn't normally perform any type conversion.
  template<typename T>
  inline Immediate(T value);  // NOLINT(runtime/explicit)

  template<typename T>
  inline Immediate(T value, RelocInfo::Mode rmode);

  int64_t value() const { return value_; }
  RelocInfo::Mode rmode() const { return rmode_; }

 private:
  void InitializeHandle(Handle<Object> value);

  int64_t value_;
  RelocInfo::Mode rmode_;
};


// -----------------------------------------------------------------------------
// Operands.
const int kSmiShift = kSmiTagSize + kSmiShiftSize;
const uint64_t kSmiShiftMask = (1UL << kSmiShift) - 1;

// Represents an operand in a machine instruction.
class Operand {
  // TODO(all): If necessary, study more in details which methods
  // TODO(all): should be inlined or not.
 public:
  // rm, {<shift> {#<shift_amount>}}
  // where <shift> is one of {LSL, LSR, ASR, ROR}.
  //       <shift_amount> is uint6_t.
  // This is allowed to be an implicit constructor because Operand is
  // a wrapper class that doesn't normally perform any type conversion.
  inline Operand(Register reg,
                 Shift shift = LSL,
                 unsigned shift_amount = 0);  // NOLINT(runtime/explicit)

  // rm, <extend> {#<shift_amount>}
  // where <extend> is one of {UXTB, UXTH, UXTW, UXTX, SXTB, SXTH, SXTW, SXTX}.
  //       <shift_amount> is uint2_t.
  inline Operand(Register reg,
                 Extend extend,
                 unsigned shift_amount = 0);

  template<typename T>
  inline explicit Operand(Handle<T> handle);

  // Implicit constructor for all int types, ExternalReference, and Smi.
  template<typename T>
  inline Operand(T t);  // NOLINT(runtime/explicit)

  // Implicit constructor for int types.
  template<typename T>
  inline Operand(T t, RelocInfo::Mode rmode);

  inline bool IsImmediate() const;
  inline bool IsShiftedRegister() const;
  inline bool IsExtendedRegister() const;
  inline bool IsZero() const;

  // This returns an LSL shift (<= 4) operand as an equivalent extend operand,
  // which helps in the encoding of instructions that use the stack pointer.
  inline Operand ToExtendedRegister() const;

  inline Immediate immediate() const;
  inline int64_t ImmediateValue() const;
  inline Register reg() const;
  inline Shift shift() const;
  inline Extend extend() const;
  inline unsigned shift_amount() const;

  // Relocation information.
  bool NeedsRelocation(const Assembler* assembler) const;

  // Helpers
  inline static Operand UntagSmi(Register smi);
  inline static Operand UntagSmiAndScale(Register smi, int scale);

 private:
  Immediate immediate_;
  Register reg_;
  Shift shift_;
  Extend extend_;
  unsigned shift_amount_;
};


// MemOperand represents a memory operand in a load or store instruction.
class MemOperand {
 public:
  inline MemOperand();
  inline explicit MemOperand(Register base,
                             int64_t offset = 0,
                             AddrMode addrmode = Offset);
  inline explicit MemOperand(Register base,
                             Register regoffset,
                             Shift shift = LSL,
                             unsigned shift_amount = 0);
  inline explicit MemOperand(Register base,
                             Register regoffset,
                             Extend extend,
                             unsigned shift_amount = 0);
  inline explicit MemOperand(Register base,
                             const Operand& offset,
                             AddrMode addrmode = Offset);

  const Register& base() const { return base_; }
  const Register& regoffset() const { return regoffset_; }
  int64_t offset() const { return offset_; }
  AddrMode addrmode() const { return addrmode_; }
  Shift shift() const { return shift_; }
  Extend extend() const { return extend_; }
  unsigned shift_amount() const { return shift_amount_; }
  inline bool IsImmediateOffset() const;
  inline bool IsRegisterOffset() const;
  inline bool IsPreIndex() const;
  inline bool IsPostIndex() const;

  // For offset modes, return the offset as an Operand. This helper cannot
  // handle indexed modes.
  inline Operand OffsetAsOperand() const;

  enum PairResult {
    kNotPair,   // Can't use a pair instruction.
    kPairAB,    // Can use a pair instruction (operandA has lower address).
    kPairBA     // Can use a pair instruction (operandB has lower address).
  };
  // Check if two MemOperand are consistent for stp/ldp use.
  static PairResult AreConsistentForPair(const MemOperand& operandA,
                                         const MemOperand& operandB,
                                         int access_size_log2 = kXRegSizeLog2);

 private:
  Register base_;
  Register regoffset_;
  int64_t offset_;
  AddrMode addrmode_;
  Shift shift_;
  Extend extend_;
  unsigned shift_amount_;
};


class ConstPool {
 public:
  explicit ConstPool(Assembler* assm)
      : assm_(assm),
        first_use_(-1),
        shared_entries_count(0) {}
  void RecordEntry(intptr_t data, RelocInfo::Mode mode);
  int EntryCount() const {
    return shared_entries_count + unique_entries_.size();
  }
  bool IsEmpty() const {
    return shared_entries_.empty() && unique_entries_.empty();
  }
  // Distance in bytes between the current pc and the first instruction
  // using the pool. If there are no pending entries return kMaxInt.
  int DistanceToFirstUse();
  // Offset after which instructions using the pool will be out of range.
  int MaxPcOffset();
  // Maximum size the constant pool can be with current entries. It always
  // includes alignment padding and branch over.
  int WorstCaseSize();
  // Size in bytes of the literal pool *if* it is emitted at the current
  // pc. The size will include the branch over the pool if it was requested.
  int SizeIfEmittedAtCurrentPc(bool require_jump);
  // Emit the literal pool at the current pc with a branch over the pool if
  // requested.
  void Emit(bool require_jump);
  // Discard any pending pool entries.
  void Clear();

 private:
  bool CanBeShared(RelocInfo::Mode mode);
  void EmitMarker();
  void EmitGuard();
  void EmitEntries();

  Assembler* assm_;
  // Keep track of the first instruction requiring a constant pool entry
  // since the previous constant pool was emitted.
  int first_use_;
  // values, pc offset(s) of entries which can be shared.
  std::multimap<uint64_t, int> shared_entries_;
  // Number of distinct literal in shared entries.
  int shared_entries_count;
  // values, pc offset of entries which cannot be shared.
  std::vector<std::pair<uint64_t, int> > unique_entries_;
};


// -----------------------------------------------------------------------------
// Assembler.

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
  Assembler(Isolate* arg_isolate, void* buffer, int buffer_size);

  virtual ~Assembler();

  virtual void AbortedCodeGeneration() {
    constpool_.Clear();
  }

  // System functions ---------------------------------------------------------
  // Start generating code from the beginning of the buffer, discarding any code
  // and data that has already been emitted into the buffer.
  //
  // In order to avoid any accidental transfer of state, Reset DCHECKs that the
  // constant pool is not blocked.
  void Reset();

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  //
  // The descriptor (desc) can be NULL. In that case, the code is finalized as
  // usual, but the descriptor is not populated.
  void GetCode(CodeDesc* desc);

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);

  inline void Unreachable();

  // Label --------------------------------------------------------------------
  // Bind a label to the current pc. Note that labels can only be bound once,
  // and if labels are linked to other instructions, they _must_ be bound
  // before they go out of scope.
  void bind(Label* label);


  // RelocInfo and pools ------------------------------------------------------

  // Record relocation information for current pc_.
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Return the address in the constant pool of the code target address used by
  // the branch/call instruction at pc.
  inline static Address target_pointer_address_at(Address pc);

  // Read/Modify the code target address in the branch/call instruction at pc.
  inline static Address target_address_at(Address pc,
                                          ConstantPoolArray* constant_pool);
  inline static void set_target_address_at(Address pc,
                                           ConstantPoolArray* constant_pool,
                                           Address target,
                                           ICacheFlushMode icache_flush_mode =
                                               FLUSH_ICACHE_IF_NEEDED);
  static inline Address target_address_at(Address pc, Code* code);
  static inline void set_target_address_at(Address pc,
                                           Code* code,
                                           Address target,
                                           ICacheFlushMode icache_flush_mode =
                                               FLUSH_ICACHE_IF_NEEDED);

  // Return the code target address at a call site from the return address of
  // that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // Given the address of the beginning of a call, return the address in the
  // instruction stream that call will return from.
  inline static Address return_address_from_call_start(Address pc);

  // Return the code target address of the patch debug break slot
  inline static Address break_address_from_return_address(Address pc);

  // This sets the branch destination (which is in the constant pool on ARM).
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address constant_pool_entry, Code* code, Address target);

  // All addresses in the constant pool are the same size as pointers.
  static const int kSpecialTargetSize = kPointerSize;

  // The sizes of the call sequences emitted by MacroAssembler::Call.
  // Wherever possible, use MacroAssembler::CallSize instead of these constants,
  // as it will choose the correct value for a given relocation mode.
  //
  // Without relocation:
  //  movz  temp, #(target & 0x000000000000ffff)
  //  movk  temp, #(target & 0x00000000ffff0000)
  //  movk  temp, #(target & 0x0000ffff00000000)
  //  blr   temp
  //
  // With relocation:
  //  ldr   temp, =target
  //  blr   temp
  static const int kCallSizeWithoutRelocation = 4 * kInstructionSize;
  static const int kCallSizeWithRelocation = 2 * kInstructionSize;

  // Size of the generated code in bytes
  uint64_t SizeOfGeneratedCode() const {
    DCHECK((pc_ >= buffer_) && (pc_ < (buffer_ + buffer_size_)));
    return pc_ - buffer_;
  }

  // Return the code size generated from label to the current position.
  uint64_t SizeOfCodeGeneratedSince(const Label* label) {
    DCHECK(label->is_bound());
    DCHECK(pc_offset() >= label->pos());
    DCHECK(pc_offset() < buffer_size_);
    return pc_offset() - label->pos();
  }

  // Check the size of the code generated since the given label. This function
  // is used primarily to work around comparisons between signed and unsigned
  // quantities, since V8 uses both.
  // TODO(jbramley): Work out what sign to use for these things and if possible,
  // change things to be consistent.
  void AssertSizeOfCodeGeneratedSince(const Label* label, ptrdiff_t size) {
    DCHECK(size >= 0);
    DCHECK(static_cast<uint64_t>(size) == SizeOfCodeGeneratedSince(label));
  }

  // Return the number of instructions generated from label to the
  // current position.
  int InstructionsGeneratedSince(const Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstructionSize;
  }

  // Number of instructions generated for the return sequence in
  // FullCodeGenerator::EmitReturnSequence.
  static const int kJSRetSequenceInstructions = 7;
  // Distance between start of patched return sequence and the emitted address
  // to jump to.
  static const int kPatchReturnSequenceAddressOffset =  0;
  static const int kPatchDebugBreakSlotAddressOffset =  0;

  // Number of instructions necessary to be able to later patch it to a call.
  // See DebugCodegen::GenerateSlot() and
  // BreakLocationIterator::SetDebugBreakAtSlot().
  static const int kDebugBreakSlotInstructions = 4;
  static const int kDebugBreakSlotLength =
    kDebugBreakSlotInstructions * kInstructionSize;

  static const int kPatchDebugBreakSlotReturnOffset = 2 * kInstructionSize;

  // Prevent contant pool emission until EndBlockConstPool is called.
  // Call to this function can be nested but must be followed by an equal
  // number of call to EndBlockConstpool.
  void StartBlockConstPool();

  // Resume constant pool emission. Need to be called as many time as
  // StartBlockConstPool to have an effect.
  void EndBlockConstPool();

  bool is_const_pool_blocked() const;
  static bool IsConstantPoolAt(Instruction* instr);
  static int ConstantPoolSizeAt(Instruction* instr);
  // See Assembler::CheckConstPool for more info.
  void EmitPoolGuard();

  // Prevent veneer pool emission until EndBlockVeneerPool is called.
  // Call to this function can be nested but must be followed by an equal
  // number of call to EndBlockConstpool.
  void StartBlockVeneerPool();

  // Resume constant pool emission. Need to be called as many time as
  // StartBlockVeneerPool to have an effect.
  void EndBlockVeneerPool();

  bool is_veneer_pool_blocked() const {
    return veneer_pool_blocked_nesting_ > 0;
  }

  // Block/resume emission of constant pools and veneer pools.
  void StartBlockPools() {
    StartBlockConstPool();
    StartBlockVeneerPool();
  }
  void EndBlockPools() {
    EndBlockConstPool();
    EndBlockVeneerPool();
  }

  // Debugging ----------------------------------------------------------------
  PositionsRecorder* positions_recorder() { return &positions_recorder_; }
  void RecordComment(const char* msg);
  int buffer_space() const;

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot();

  // Record the emission of a constant pool.
  //
  // The emission of constant and veneer pools depends on the size of the code
  // generated and the number of RelocInfo recorded.
  // The Debug mechanism needs to map code offsets between two versions of a
  // function, compiled with and without debugger support (see for example
  // Debug::PrepareForBreakPoints()).
  // Compiling functions with debugger support generates additional code
  // (DebugCodegen::GenerateSlot()). This may affect the emission of the pools
  // and cause the version of the code with debugger support to have pools
  // generated in different places.
  // Recording the position and size of emitted pools allows to correctly
  // compute the offset mappings between the different versions of a function in
  // all situations.
  //
  // The parameter indicates the size of the pool (in bytes), including
  // the marker and branch over the data.
  void RecordConstPool(int size);


  // Instruction set functions ------------------------------------------------

  // Branch / Jump instructions.
  // For branches offsets are scaled, i.e. they in instrcutions not in bytes.
  // Branch to register.
  void br(const Register& xn);

  // Branch-link to register.
  void blr(const Register& xn);

  // Branch to register with return hint.
  void ret(const Register& xn = lr);

  // Unconditional branch to label.
  void b(Label* label);

  // Conditional branch to label.
  void b(Label* label, Condition cond);

  // Unconditional branch to PC offset.
  void b(int imm26);

  // Conditional branch to PC offset.
  void b(int imm19, Condition cond);

  // Branch-link to label / pc offset.
  void bl(Label* label);
  void bl(int imm26);

  // Compare and branch to label / pc offset if zero.
  void cbz(const Register& rt, Label* label);
  void cbz(const Register& rt, int imm19);

  // Compare and branch to label / pc offset if not zero.
  void cbnz(const Register& rt, Label* label);
  void cbnz(const Register& rt, int imm19);

  // Test bit and branch to label / pc offset if zero.
  void tbz(const Register& rt, unsigned bit_pos, Label* label);
  void tbz(const Register& rt, unsigned bit_pos, int imm14);

  // Test bit and branch to label / pc offset if not zero.
  void tbnz(const Register& rt, unsigned bit_pos, Label* label);
  void tbnz(const Register& rt, unsigned bit_pos, int imm14);

  // Address calculation instructions.
  // Calculate a PC-relative address. Unlike for branches the offset in adr is
  // unscaled (i.e. the result can be unaligned).
  void adr(const Register& rd, Label* label);
  void adr(const Register& rd, int imm21);

  // Data Processing instructions.
  // Add.
  void add(const Register& rd,
           const Register& rn,
           const Operand& operand);

  // Add and update status flags.
  void adds(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Compare negative.
  void cmn(const Register& rn, const Operand& operand);

  // Subtract.
  void sub(const Register& rd,
           const Register& rn,
           const Operand& operand);

  // Subtract and update status flags.
  void subs(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Compare.
  void cmp(const Register& rn, const Operand& operand);

  // Negate.
  void neg(const Register& rd,
           const Operand& operand);

  // Negate and update status flags.
  void negs(const Register& rd,
            const Operand& operand);

  // Add with carry bit.
  void adc(const Register& rd,
           const Register& rn,
           const Operand& operand);

  // Add with carry bit and update status flags.
  void adcs(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Subtract with carry bit.
  void sbc(const Register& rd,
           const Register& rn,
           const Operand& operand);

  // Subtract with carry bit and update status flags.
  void sbcs(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Negate with carry bit.
  void ngc(const Register& rd,
           const Operand& operand);

  // Negate with carry bit and update status flags.
  void ngcs(const Register& rd,
            const Operand& operand);

  // Logical instructions.
  // Bitwise and (A & B).
  void and_(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Bitwise and (A & B) and update status flags.
  void ands(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Bit test, and set flags.
  void tst(const Register& rn, const Operand& operand);

  // Bit clear (A & ~B).
  void bic(const Register& rd,
           const Register& rn,
           const Operand& operand);

  // Bit clear (A & ~B) and update status flags.
  void bics(const Register& rd,
            const Register& rn,
            const Operand& operand);

  // Bitwise or (A | B).
  void orr(const Register& rd, const Register& rn, const Operand& operand);

  // Bitwise nor (A | ~B).
  void orn(const Register& rd, const Register& rn, const Operand& operand);

  // Bitwise eor/xor (A ^ B).
  void eor(const Register& rd, const Register& rn, const Operand& operand);

  // Bitwise enor/xnor (A ^ ~B).
  void eon(const Register& rd, const Register& rn, const Operand& operand);

  // Logical shift left variable.
  void lslv(const Register& rd, const Register& rn, const Register& rm);

  // Logical shift right variable.
  void lsrv(const Register& rd, const Register& rn, const Register& rm);

  // Arithmetic shift right variable.
  void asrv(const Register& rd, const Register& rn, const Register& rm);

  // Rotate right variable.
  void rorv(const Register& rd, const Register& rn, const Register& rm);

  // Bitfield instructions.
  // Bitfield move.
  void bfm(const Register& rd,
           const Register& rn,
           unsigned immr,
           unsigned imms);

  // Signed bitfield move.
  void sbfm(const Register& rd,
            const Register& rn,
            unsigned immr,
            unsigned imms);

  // Unsigned bitfield move.
  void ubfm(const Register& rd,
            const Register& rn,
            unsigned immr,
            unsigned imms);

  // Bfm aliases.
  // Bitfield insert.
  void bfi(const Register& rd,
           const Register& rn,
           unsigned lsb,
           unsigned width) {
    DCHECK(width >= 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    bfm(rd, rn, (rd.SizeInBits() - lsb) & (rd.SizeInBits() - 1), width - 1);
  }

  // Bitfield extract and insert low.
  void bfxil(const Register& rd,
             const Register& rn,
             unsigned lsb,
             unsigned width) {
    DCHECK(width >= 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    bfm(rd, rn, lsb, lsb + width - 1);
  }

  // Sbfm aliases.
  // Arithmetic shift right.
  void asr(const Register& rd, const Register& rn, unsigned shift) {
    DCHECK(shift < rd.SizeInBits());
    sbfm(rd, rn, shift, rd.SizeInBits() - 1);
  }

  // Signed bitfield insert in zero.
  void sbfiz(const Register& rd,
             const Register& rn,
             unsigned lsb,
             unsigned width) {
    DCHECK(width >= 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    sbfm(rd, rn, (rd.SizeInBits() - lsb) & (rd.SizeInBits() - 1), width - 1);
  }

  // Signed bitfield extract.
  void sbfx(const Register& rd,
            const Register& rn,
            unsigned lsb,
            unsigned width) {
    DCHECK(width >= 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    sbfm(rd, rn, lsb, lsb + width - 1);
  }

  // Signed extend byte.
  void sxtb(const Register& rd, const Register& rn) {
    sbfm(rd, rn, 0, 7);
  }

  // Signed extend halfword.
  void sxth(const Register& rd, const Register& rn) {
    sbfm(rd, rn, 0, 15);
  }

  // Signed extend word.
  void sxtw(const Register& rd, const Register& rn) {
    sbfm(rd, rn, 0, 31);
  }

  // Ubfm aliases.
  // Logical shift left.
  void lsl(const Register& rd, const Register& rn, unsigned shift) {
    unsigned reg_size = rd.SizeInBits();
    DCHECK(shift < reg_size);
    ubfm(rd, rn, (reg_size - shift) % reg_size, reg_size - shift - 1);
  }

  // Logical shift right.
  void lsr(const Register& rd, const Register& rn, unsigned shift) {
    DCHECK(shift < rd.SizeInBits());
    ubfm(rd, rn, shift, rd.SizeInBits() - 1);
  }

  // Unsigned bitfield insert in zero.
  void ubfiz(const Register& rd,
             const Register& rn,
             unsigned lsb,
             unsigned width) {
    DCHECK(width >= 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    ubfm(rd, rn, (rd.SizeInBits() - lsb) & (rd.SizeInBits() - 1), width - 1);
  }

  // Unsigned bitfield extract.
  void ubfx(const Register& rd,
            const Register& rn,
            unsigned lsb,
            unsigned width) {
    DCHECK(width >= 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    ubfm(rd, rn, lsb, lsb + width - 1);
  }

  // Unsigned extend byte.
  void uxtb(const Register& rd, const Register& rn) {
    ubfm(rd, rn, 0, 7);
  }

  // Unsigned extend halfword.
  void uxth(const Register& rd, const Register& rn) {
    ubfm(rd, rn, 0, 15);
  }

  // Unsigned extend word.
  void uxtw(const Register& rd, const Register& rn) {
    ubfm(rd, rn, 0, 31);
  }

  // Extract.
  void extr(const Register& rd,
            const Register& rn,
            const Register& rm,
            unsigned lsb);

  // Conditional select: rd = cond ? rn : rm.
  void csel(const Register& rd,
            const Register& rn,
            const Register& rm,
            Condition cond);

  // Conditional select increment: rd = cond ? rn : rm + 1.
  void csinc(const Register& rd,
             const Register& rn,
             const Register& rm,
             Condition cond);

  // Conditional select inversion: rd = cond ? rn : ~rm.
  void csinv(const Register& rd,
             const Register& rn,
             const Register& rm,
             Condition cond);

  // Conditional select negation: rd = cond ? rn : -rm.
  void csneg(const Register& rd,
             const Register& rn,
             const Register& rm,
             Condition cond);

  // Conditional set: rd = cond ? 1 : 0.
  void cset(const Register& rd, Condition cond);

  // Conditional set minus: rd = cond ? -1 : 0.
  void csetm(const Register& rd, Condition cond);

  // Conditional increment: rd = cond ? rn + 1 : rn.
  void cinc(const Register& rd, const Register& rn, Condition cond);

  // Conditional invert: rd = cond ? ~rn : rn.
  void cinv(const Register& rd, const Register& rn, Condition cond);

  // Conditional negate: rd = cond ? -rn : rn.
  void cneg(const Register& rd, const Register& rn, Condition cond);

  // Extr aliases.
  void ror(const Register& rd, const Register& rs, unsigned shift) {
    extr(rd, rs, rs, shift);
  }

  // Conditional comparison.
  // Conditional compare negative.
  void ccmn(const Register& rn,
            const Operand& operand,
            StatusFlags nzcv,
            Condition cond);

  // Conditional compare.
  void ccmp(const Register& rn,
            const Operand& operand,
            StatusFlags nzcv,
            Condition cond);

  // Multiplication.
  // 32 x 32 -> 32-bit and 64 x 64 -> 64-bit multiply.
  void mul(const Register& rd, const Register& rn, const Register& rm);

  // 32 + 32 x 32 -> 32-bit and 64 + 64 x 64 -> 64-bit multiply accumulate.
  void madd(const Register& rd,
            const Register& rn,
            const Register& rm,
            const Register& ra);

  // -(32 x 32) -> 32-bit and -(64 x 64) -> 64-bit multiply.
  void mneg(const Register& rd, const Register& rn, const Register& rm);

  // 32 - 32 x 32 -> 32-bit and 64 - 64 x 64 -> 64-bit multiply subtract.
  void msub(const Register& rd,
            const Register& rn,
            const Register& rm,
            const Register& ra);

  // 32 x 32 -> 64-bit multiply.
  void smull(const Register& rd, const Register& rn, const Register& rm);

  // Xd = bits<127:64> of Xn * Xm.
  void smulh(const Register& rd, const Register& rn, const Register& rm);

  // Signed 32 x 32 -> 64-bit multiply and accumulate.
  void smaddl(const Register& rd,
              const Register& rn,
              const Register& rm,
              const Register& ra);

  // Unsigned 32 x 32 -> 64-bit multiply and accumulate.
  void umaddl(const Register& rd,
              const Register& rn,
              const Register& rm,
              const Register& ra);

  // Signed 32 x 32 -> 64-bit multiply and subtract.
  void smsubl(const Register& rd,
              const Register& rn,
              const Register& rm,
              const Register& ra);

  // Unsigned 32 x 32 -> 64-bit multiply and subtract.
  void umsubl(const Register& rd,
              const Register& rn,
              const Register& rm,
              const Register& ra);

  // Signed integer divide.
  void sdiv(const Register& rd, const Register& rn, const Register& rm);

  // Unsigned integer divide.
  void udiv(const Register& rd, const Register& rn, const Register& rm);

  // Bit count, bit reverse and endian reverse.
  void rbit(const Register& rd, const Register& rn);
  void rev16(const Register& rd, const Register& rn);
  void rev32(const Register& rd, const Register& rn);
  void rev(const Register& rd, const Register& rn);
  void clz(const Register& rd, const Register& rn);
  void cls(const Register& rd, const Register& rn);

  // Memory instructions.

  // Load integer or FP register.
  void ldr(const CPURegister& rt, const MemOperand& src);

  // Store integer or FP register.
  void str(const CPURegister& rt, const MemOperand& dst);

  // Load word with sign extension.
  void ldrsw(const Register& rt, const MemOperand& src);

  // Load byte.
  void ldrb(const Register& rt, const MemOperand& src);

  // Store byte.
  void strb(const Register& rt, const MemOperand& dst);

  // Load byte with sign extension.
  void ldrsb(const Register& rt, const MemOperand& src);

  // Load half-word.
  void ldrh(const Register& rt, const MemOperand& src);

  // Store half-word.
  void strh(const Register& rt, const MemOperand& dst);

  // Load half-word with sign extension.
  void ldrsh(const Register& rt, const MemOperand& src);

  // Load integer or FP register pair.
  void ldp(const CPURegister& rt, const CPURegister& rt2,
           const MemOperand& src);

  // Store integer or FP register pair.
  void stp(const CPURegister& rt, const CPURegister& rt2,
           const MemOperand& dst);

  // Load word pair with sign extension.
  void ldpsw(const Register& rt, const Register& rt2, const MemOperand& src);

  // Load integer or FP register pair, non-temporal.
  void ldnp(const CPURegister& rt, const CPURegister& rt2,
            const MemOperand& src);

  // Store integer or FP register pair, non-temporal.
  void stnp(const CPURegister& rt, const CPURegister& rt2,
            const MemOperand& dst);

  // Load literal to register from a pc relative address.
  void ldr_pcrel(const CPURegister& rt, int imm19);

  // Load literal to register.
  void ldr(const CPURegister& rt, const Immediate& imm);

  // Move instructions. The default shift of -1 indicates that the move
  // instruction will calculate an appropriate 16-bit immediate and left shift
  // that is equal to the 64-bit immediate argument. If an explicit left shift
  // is specified (0, 16, 32 or 48), the immediate must be a 16-bit value.
  //
  // For movk, an explicit shift can be used to indicate which half word should
  // be overwritten, eg. movk(x0, 0, 0) will overwrite the least-significant
  // half word with zero, whereas movk(x0, 0, 48) will overwrite the
  // most-significant.

  // Move and keep.
  void movk(const Register& rd, uint64_t imm, int shift = -1) {
    MoveWide(rd, imm, shift, MOVK);
  }

  // Move with non-zero.
  void movn(const Register& rd, uint64_t imm, int shift = -1) {
    MoveWide(rd, imm, shift, MOVN);
  }

  // Move with zero.
  void movz(const Register& rd, uint64_t imm, int shift = -1) {
    MoveWide(rd, imm, shift, MOVZ);
  }

  // Misc instructions.
  // Monitor debug-mode breakpoint.
  void brk(int code);

  // Halting debug-mode breakpoint.
  void hlt(int code);

  // Move register to register.
  void mov(const Register& rd, const Register& rn);

  // Move NOT(operand) to register.
  void mvn(const Register& rd, const Operand& operand);

  // System instructions.
  // Move to register from system register.
  void mrs(const Register& rt, SystemRegister sysreg);

  // Move from register to system register.
  void msr(SystemRegister sysreg, const Register& rt);

  // System hint.
  void hint(SystemHint code);

  // Data memory barrier
  void dmb(BarrierDomain domain, BarrierType type);

  // Data synchronization barrier
  void dsb(BarrierDomain domain, BarrierType type);

  // Instruction synchronization barrier
  void isb();

  // Alias for system instructions.
  void nop() { hint(NOP); }

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    DEBUG_BREAK_NOP,
    INTERRUPT_CODE_NOP,
    ADR_FAR_NOP,
    FIRST_NOP_MARKER = DEBUG_BREAK_NOP,
    LAST_NOP_MARKER = ADR_FAR_NOP
  };

  void nop(NopMarkerTypes n) {
    DCHECK((FIRST_NOP_MARKER <= n) && (n <= LAST_NOP_MARKER));
    mov(Register::XRegFromCode(n), Register::XRegFromCode(n));
  }

  // FP instructions.
  // Move immediate to FP register.
  void fmov(FPRegister fd, double imm);
  void fmov(FPRegister fd, float imm);

  // Move FP register to register.
  void fmov(Register rd, FPRegister fn);

  // Move register to FP register.
  void fmov(FPRegister fd, Register rn);

  // Move FP register to FP register.
  void fmov(FPRegister fd, FPRegister fn);

  // FP add.
  void fadd(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP subtract.
  void fsub(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP multiply.
  void fmul(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP fused multiply and add.
  void fmadd(const FPRegister& fd,
             const FPRegister& fn,
             const FPRegister& fm,
             const FPRegister& fa);

  // FP fused multiply and subtract.
  void fmsub(const FPRegister& fd,
             const FPRegister& fn,
             const FPRegister& fm,
             const FPRegister& fa);

  // FP fused multiply, add and negate.
  void fnmadd(const FPRegister& fd,
              const FPRegister& fn,
              const FPRegister& fm,
              const FPRegister& fa);

  // FP fused multiply, subtract and negate.
  void fnmsub(const FPRegister& fd,
              const FPRegister& fn,
              const FPRegister& fm,
              const FPRegister& fa);

  // FP divide.
  void fdiv(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP maximum.
  void fmax(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP minimum.
  void fmin(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP maximum.
  void fmaxnm(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP minimum.
  void fminnm(const FPRegister& fd, const FPRegister& fn, const FPRegister& fm);

  // FP absolute.
  void fabs(const FPRegister& fd, const FPRegister& fn);

  // FP negate.
  void fneg(const FPRegister& fd, const FPRegister& fn);

  // FP square root.
  void fsqrt(const FPRegister& fd, const FPRegister& fn);

  // FP round to integer (nearest with ties to away).
  void frinta(const FPRegister& fd, const FPRegister& fn);

  // FP round to integer (toward minus infinity).
  void frintm(const FPRegister& fd, const FPRegister& fn);

  // FP round to integer (nearest with ties to even).
  void frintn(const FPRegister& fd, const FPRegister& fn);

  // FP round to integer (towards plus infinity).
  void frintp(const FPRegister& fd, const FPRegister& fn);

  // FP round to integer (towards zero.)
  void frintz(const FPRegister& fd, const FPRegister& fn);

  // FP compare registers.
  void fcmp(const FPRegister& fn, const FPRegister& fm);

  // FP compare immediate.
  void fcmp(const FPRegister& fn, double value);

  // FP conditional compare.
  void fccmp(const FPRegister& fn,
             const FPRegister& fm,
             StatusFlags nzcv,
             Condition cond);

  // FP conditional select.
  void fcsel(const FPRegister& fd,
             const FPRegister& fn,
             const FPRegister& fm,
             Condition cond);

  // Common FP Convert function
  void FPConvertToInt(const Register& rd,
                      const FPRegister& fn,
                      FPIntegerConvertOp op);

  // FP convert between single and double precision.
  void fcvt(const FPRegister& fd, const FPRegister& fn);

  // Convert FP to unsigned integer (nearest with ties to away).
  void fcvtau(const Register& rd, const FPRegister& fn);

  // Convert FP to signed integer (nearest with ties to away).
  void fcvtas(const Register& rd, const FPRegister& fn);

  // Convert FP to unsigned integer (round towards -infinity).
  void fcvtmu(const Register& rd, const FPRegister& fn);

  // Convert FP to signed integer (round towards -infinity).
  void fcvtms(const Register& rd, const FPRegister& fn);

  // Convert FP to unsigned integer (nearest with ties to even).
  void fcvtnu(const Register& rd, const FPRegister& fn);

  // Convert FP to signed integer (nearest with ties to even).
  void fcvtns(const Register& rd, const FPRegister& fn);

  // Convert FP to unsigned integer (round towards zero).
  void fcvtzu(const Register& rd, const FPRegister& fn);

  // Convert FP to signed integer (rounf towards zero).
  void fcvtzs(const Register& rd, const FPRegister& fn);

  // Convert signed integer or fixed point to FP.
  void scvtf(const FPRegister& fd, const Register& rn, unsigned fbits = 0);

  // Convert unsigned integer or fixed point to FP.
  void ucvtf(const FPRegister& fd, const Register& rn, unsigned fbits = 0);

  // Instruction functions used only for test, debug, and patching.
  // Emit raw instructions in the instruction stream.
  void dci(Instr raw_inst) { Emit(raw_inst); }

  // Emit 8 bits of data in the instruction stream.
  void dc8(uint8_t data) { EmitData(&data, sizeof(data)); }

  // Emit 32 bits of data in the instruction stream.
  void dc32(uint32_t data) { EmitData(&data, sizeof(data)); }

  // Emit 64 bits of data in the instruction stream.
  void dc64(uint64_t data) { EmitData(&data, sizeof(data)); }

  // Copy a string into the instruction stream, including the terminating NULL
  // character. The instruction pointer (pc_) is then aligned correctly for
  // subsequent instructions.
  void EmitStringData(const char* string);

  // Pseudo-instructions ------------------------------------------------------

  // Parameters are described in arm64/instructions-arm64.h.
  void debug(const char* message, uint32_t code, Instr params = BREAK);

  // Required by V8.
  void dd(uint32_t data) { dc32(data); }
  void db(uint8_t data) { dc8(data); }

  // Code generation helpers --------------------------------------------------

  bool IsConstPoolEmpty() const { return constpool_.IsEmpty(); }

  Instruction* pc() const { return Instruction::Cast(pc_); }

  Instruction* InstructionAt(int offset) const {
    return reinterpret_cast<Instruction*>(buffer_ + offset);
  }

  ptrdiff_t InstructionOffset(Instruction* instr) const {
    return reinterpret_cast<byte*>(instr) - buffer_;
  }

  // Register encoding.
  static Instr Rd(CPURegister rd) {
    DCHECK(rd.code() != kSPRegInternalCode);
    return rd.code() << Rd_offset;
  }

  static Instr Rn(CPURegister rn) {
    DCHECK(rn.code() != kSPRegInternalCode);
    return rn.code() << Rn_offset;
  }

  static Instr Rm(CPURegister rm) {
    DCHECK(rm.code() != kSPRegInternalCode);
    return rm.code() << Rm_offset;
  }

  static Instr Ra(CPURegister ra) {
    DCHECK(ra.code() != kSPRegInternalCode);
    return ra.code() << Ra_offset;
  }

  static Instr Rt(CPURegister rt) {
    DCHECK(rt.code() != kSPRegInternalCode);
    return rt.code() << Rt_offset;
  }

  static Instr Rt2(CPURegister rt2) {
    DCHECK(rt2.code() != kSPRegInternalCode);
    return rt2.code() << Rt2_offset;
  }

  // These encoding functions allow the stack pointer to be encoded, and
  // disallow the zero register.
  static Instr RdSP(Register rd) {
    DCHECK(!rd.IsZero());
    return (rd.code() & kRegCodeMask) << Rd_offset;
  }

  static Instr RnSP(Register rn) {
    DCHECK(!rn.IsZero());
    return (rn.code() & kRegCodeMask) << Rn_offset;
  }

  // Flags encoding.
  inline static Instr Flags(FlagsUpdate S);
  inline static Instr Cond(Condition cond);

  // PC-relative address encoding.
  inline static Instr ImmPCRelAddress(int imm21);

  // Branch encoding.
  inline static Instr ImmUncondBranch(int imm26);
  inline static Instr ImmCondBranch(int imm19);
  inline static Instr ImmCmpBranch(int imm19);
  inline static Instr ImmTestBranch(int imm14);
  inline static Instr ImmTestBranchBit(unsigned bit_pos);

  // Data Processing encoding.
  inline static Instr SF(Register rd);
  inline static Instr ImmAddSub(int64_t imm);
  inline static Instr ImmS(unsigned imms, unsigned reg_size);
  inline static Instr ImmR(unsigned immr, unsigned reg_size);
  inline static Instr ImmSetBits(unsigned imms, unsigned reg_size);
  inline static Instr ImmRotate(unsigned immr, unsigned reg_size);
  inline static Instr ImmLLiteral(int imm19);
  inline static Instr BitN(unsigned bitn, unsigned reg_size);
  inline static Instr ShiftDP(Shift shift);
  inline static Instr ImmDPShift(unsigned amount);
  inline static Instr ExtendMode(Extend extend);
  inline static Instr ImmExtendShift(unsigned left_shift);
  inline static Instr ImmCondCmp(unsigned imm);
  inline static Instr Nzcv(StatusFlags nzcv);

  static bool IsImmAddSub(int64_t immediate);
  static bool IsImmLogical(uint64_t value,
                           unsigned width,
                           unsigned* n,
                           unsigned* imm_s,
                           unsigned* imm_r);

  // MemOperand offset encoding.
  inline static Instr ImmLSUnsigned(int imm12);
  inline static Instr ImmLS(int imm9);
  inline static Instr ImmLSPair(int imm7, LSDataSize size);
  inline static Instr ImmShiftLS(unsigned shift_amount);
  inline static Instr ImmException(int imm16);
  inline static Instr ImmSystemRegister(int imm15);
  inline static Instr ImmHint(int imm7);
  inline static Instr ImmBarrierDomain(int imm2);
  inline static Instr ImmBarrierType(int imm2);
  inline static LSDataSize CalcLSDataSize(LoadStoreOp op);

  static bool IsImmLSUnscaled(int64_t offset);
  static bool IsImmLSScaled(int64_t offset, LSDataSize size);

  // Move immediates encoding.
  inline static Instr ImmMoveWide(uint64_t imm);
  inline static Instr ShiftMoveWide(int64_t shift);

  // FP Immediates.
  static Instr ImmFP32(float imm);
  static Instr ImmFP64(double imm);
  inline static Instr FPScale(unsigned scale);

  // FP register type.
  inline static Instr FPType(FPRegister fd);

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

  // Check if is time to emit a constant pool.
  void CheckConstPool(bool force_emit, bool require_jump);

  // Allocate a constant pool of the correct size for the generated code.
  Handle<ConstantPoolArray> NewConstantPool(Isolate* isolate);

  // Generate the constant pool for the generated code.
  void PopulateConstantPool(ConstantPoolArray* constant_pool);

  // Returns true if we should emit a veneer as soon as possible for a branch
  // which can at most reach to specified pc.
  bool ShouldEmitVeneer(int max_reachable_pc,
                        int margin = kVeneerDistanceMargin);
  bool ShouldEmitVeneers(int margin = kVeneerDistanceMargin) {
    return ShouldEmitVeneer(unresolved_branches_first_limit(), margin);
  }

  // The maximum code size generated for a veneer. Currently one branch
  // instruction. This is for code size checking purposes, and can be extended
  // in the future for example if we decide to add nops between the veneers.
  static const int kMaxVeneerCodeSize = 1 * kInstructionSize;

  void RecordVeneerPool(int location_offset, int size);
  // Emits veneers for branches that are approaching their maximum range.
  // If need_protection is true, the veneers are protected by a branch jumping
  // over the code.
  void EmitVeneers(bool force_emit, bool need_protection,
                   int margin = kVeneerDistanceMargin);
  void EmitVeneersGuard() { EmitPoolGuard(); }
  // Checks whether veneers need to be emitted at this point.
  // If force_emit is set, a veneer is generated for *all* unresolved branches.
  void CheckVeneerPool(bool force_emit, bool require_jump,
                       int margin = kVeneerDistanceMargin);

  class BlockPoolsScope {
   public:
    explicit BlockPoolsScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockPools();
    }
    ~BlockPoolsScope() {
      assem_->EndBlockPools();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockPoolsScope);
  };

 protected:
  inline const Register& AppropriateZeroRegFor(const CPURegister& reg) const;

  void LoadStore(const CPURegister& rt,
                 const MemOperand& addr,
                 LoadStoreOp op);

  void LoadStorePair(const CPURegister& rt, const CPURegister& rt2,
                     const MemOperand& addr, LoadStorePairOp op);
  static bool IsImmLSPair(int64_t offset, LSDataSize size);

  void Logical(const Register& rd,
               const Register& rn,
               const Operand& operand,
               LogicalOp op);
  void LogicalImmediate(const Register& rd,
                        const Register& rn,
                        unsigned n,
                        unsigned imm_s,
                        unsigned imm_r,
                        LogicalOp op);

  void ConditionalCompare(const Register& rn,
                          const Operand& operand,
                          StatusFlags nzcv,
                          Condition cond,
                          ConditionalCompareOp op);
  static bool IsImmConditionalCompare(int64_t immediate);

  void AddSubWithCarry(const Register& rd,
                       const Register& rn,
                       const Operand& operand,
                       FlagsUpdate S,
                       AddSubWithCarryOp op);

  // Functions for emulating operands not directly supported by the instruction
  // set.
  void EmitShift(const Register& rd,
                 const Register& rn,
                 Shift shift,
                 unsigned amount);
  void EmitExtendShift(const Register& rd,
                       const Register& rn,
                       Extend extend,
                       unsigned left_shift);

  void AddSub(const Register& rd,
              const Register& rn,
              const Operand& operand,
              FlagsUpdate S,
              AddSubOp op);

  static bool IsImmFP32(float imm);
  static bool IsImmFP64(double imm);

  // Find an appropriate LoadStoreOp or LoadStorePairOp for the specified
  // registers. Only simple loads are supported; sign- and zero-extension (such
  // as in LDPSW_x or LDRB_w) are not supported.
  static inline LoadStoreOp LoadOpFor(const CPURegister& rt);
  static inline LoadStorePairOp LoadPairOpFor(const CPURegister& rt,
                                              const CPURegister& rt2);
  static inline LoadStoreOp StoreOpFor(const CPURegister& rt);
  static inline LoadStorePairOp StorePairOpFor(const CPURegister& rt,
                                               const CPURegister& rt2);
  static inline LoadStorePairNonTemporalOp LoadPairNonTemporalOpFor(
    const CPURegister& rt, const CPURegister& rt2);
  static inline LoadStorePairNonTemporalOp StorePairNonTemporalOpFor(
    const CPURegister& rt, const CPURegister& rt2);
  static inline LoadLiteralOp LoadLiteralOpFor(const CPURegister& rt);

  // Remove the specified branch from the unbound label link chain.
  // If available, a veneer for this label can be used for other branches in the
  // chain if the link chain cannot be fixed up without this branch.
  void RemoveBranchFromLabelLinkChain(Instruction* branch,
                                      Label* label,
                                      Instruction* label_veneer = NULL);

 private:
  // Instruction helpers.
  void MoveWide(const Register& rd,
                uint64_t imm,
                int shift,
                MoveWideImmediateOp mov_op);
  void DataProcShiftedRegister(const Register& rd,
                               const Register& rn,
                               const Operand& operand,
                               FlagsUpdate S,
                               Instr op);
  void DataProcExtendedRegister(const Register& rd,
                                const Register& rn,
                                const Operand& operand,
                                FlagsUpdate S,
                                Instr op);
  void LoadStorePairNonTemporal(const CPURegister& rt,
                                const CPURegister& rt2,
                                const MemOperand& addr,
                                LoadStorePairNonTemporalOp op);
  void ConditionalSelect(const Register& rd,
                         const Register& rn,
                         const Register& rm,
                         Condition cond,
                         ConditionalSelectOp op);
  void DataProcessing1Source(const Register& rd,
                             const Register& rn,
                             DataProcessing1SourceOp op);
  void DataProcessing3Source(const Register& rd,
                             const Register& rn,
                             const Register& rm,
                             const Register& ra,
                             DataProcessing3SourceOp op);
  void FPDataProcessing1Source(const FPRegister& fd,
                               const FPRegister& fn,
                               FPDataProcessing1SourceOp op);
  void FPDataProcessing2Source(const FPRegister& fd,
                               const FPRegister& fn,
                               const FPRegister& fm,
                               FPDataProcessing2SourceOp op);
  void FPDataProcessing3Source(const FPRegister& fd,
                               const FPRegister& fn,
                               const FPRegister& fm,
                               const FPRegister& fa,
                               FPDataProcessing3SourceOp op);

  // Label helpers.

  // Return an offset for a label-referencing instruction, typically a branch.
  int LinkAndGetByteOffsetTo(Label* label);

  // This is the same as LinkAndGetByteOffsetTo, but return an offset
  // suitable for fields that take instruction offsets.
  inline int LinkAndGetInstructionOffsetTo(Label* label);

  static const int kStartOfLabelLinkChain = 0;

  // Verify that a label's link chain is intact.
  void CheckLabelLinkChain(Label const * label);

  void RecordLiteral(int64_t imm, unsigned size);

  // Postpone the generation of the constant pool for the specified number of
  // instructions.
  void BlockConstPoolFor(int instructions);

  // Set how far from current pc the next constant pool check will be.
  void SetNextConstPoolCheckIn(int instructions) {
    next_constant_pool_check_ = pc_offset() + instructions * kInstructionSize;
  }

  // Emit the instruction at pc_.
  void Emit(Instr instruction) {
    STATIC_ASSERT(sizeof(*pc_) == 1);
    STATIC_ASSERT(sizeof(instruction) == kInstructionSize);
    DCHECK((pc_ + sizeof(instruction)) <= (buffer_ + buffer_size_));

    memcpy(pc_, &instruction, sizeof(instruction));
    pc_ += sizeof(instruction);
    CheckBuffer();
  }

  // Emit data inline in the instruction stream.
  void EmitData(void const * data, unsigned size) {
    DCHECK(sizeof(*pc_) == 1);
    DCHECK((pc_ + size) <= (buffer_ + buffer_size_));

    // TODO(all): Somehow register we have some data here. Then we can
    // disassemble it correctly.
    memcpy(pc_, data, size);
    pc_ += size;
    CheckBuffer();
  }

  void GrowBuffer();
  void CheckBufferSpace();
  void CheckBuffer();

  // Pc offset of the next constant pool check.
  int next_constant_pool_check_;

  // Constant pool generation
  // Pools are emitted in the instruction stream. They are emitted when:
  //  * the distance to the first use is above a pre-defined distance or
  //  * the numbers of entries in the pool is above a pre-defined size or
  //  * code generation is finished
  // If a pool needs to be emitted before code generation is finished a branch
  // over the emitted pool will be inserted.

  // Constants in the pool may be addresses of functions that gets relocated;
  // if so, a relocation info entry is associated to the constant pool entry.

  // Repeated checking whether the constant pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated. That also means that the sizing of the buffers is not
  // an exact science, and that we rely on some slop to not overrun buffers.
  static const int kCheckConstPoolInterval = 128;

  // Distance to first use after a which a pool will be emitted. Pool entries
  // are accessed with pc relative load therefore this cannot be more than
  // 1 * MB. Since constant pool emission checks are interval based this value
  // is an approximation.
  static const int kApproxMaxDistToConstPool = 64 * KB;

  // Number of pool entries after which a pool will be emitted. Since constant
  // pool emission checks are interval based this value is an approximation.
  static const int kApproxMaxPoolEntryCount = 512;

  // Emission of the constant pool may be blocked in some code sequences.
  int const_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_const_pool_before_;  // Block emission before this pc offset.

  // Emission of the veneer pools may be blocked in some code sequences.
  int veneer_pool_blocked_nesting_;  // Block emission if this is not zero.

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

  // The pending constant pool.
  ConstPool constpool_;

  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  TypeFeedbackId recorded_ast_id_;

  inline TypeFeedbackId RecordedAstId();
  inline void ClearRecordedAstId();

 protected:
  // Record the AST id of the CallIC being compiled, so that it can be placed
  // in the relocation information.
  void SetRecordedAstId(TypeFeedbackId ast_id) {
    DCHECK(recorded_ast_id_.IsNone());
    recorded_ast_id_ = ast_id;
  }

  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries, and debug strings encoded in the instruction
  // stream.
  static const int kGap = 128;

 public:
  class FarBranchInfo {
   public:
    FarBranchInfo(int offset, Label* label)
        : pc_offset_(offset), label_(label) {}
    // Offset of the branch in the code generation buffer.
    int pc_offset_;
    // The label branched to.
    Label* label_;
  };

 protected:
  // Information about unresolved (forward) branches.
  // The Assembler is only allowed to delete out-of-date information from here
  // after a label is bound. The MacroAssembler uses this information to
  // generate veneers.
  //
  // The second member gives information about the unresolved branch. The first
  // member of the pair is the maximum offset that the branch can reach in the
  // buffer. The map is sorted according to this reachable offset, allowing to
  // easily check when veneers need to be emitted.
  // Note that the maximum reachable offset (first member of the pairs) should
  // always be positive but has the same type as the return value for
  // pc_offset() for convenience.
  std::multimap<int, FarBranchInfo> unresolved_branches_;

  // We generate a veneer for a branch if we reach within this distance of the
  // limit of the range.
  static const int kVeneerDistanceMargin = 1 * KB;
  // The factor of 2 is a finger in the air guess. With a default margin of
  // 1KB, that leaves us an addional 256 instructions to avoid generating a
  // protective branch.
  static const int kVeneerNoProtectionFactor = 2;
  static const int kVeneerDistanceCheckMargin =
    kVeneerNoProtectionFactor * kVeneerDistanceMargin;
  int unresolved_branches_first_limit() const {
    DCHECK(!unresolved_branches_.empty());
    return unresolved_branches_.begin()->first;
  }
  // This is similar to next_constant_pool_check_ and helps reduce the overhead
  // of checking for veneer pools.
  // It is maintained to the closest unresolved branch limit minus the maximum
  // veneer margin (or kMaxInt if there are no unresolved branches).
  int next_veneer_pool_check_;

 private:
  // If a veneer is emitted for a branch instruction, that instruction must be
  // removed from the associated label's link chain so that the assembler does
  // not later attempt (likely unsuccessfully) to patch it to branch directly to
  // the label.
  void DeleteUnresolvedBranchInfoForLabel(Label* label);
  // This function deletes the information related to the label by traversing
  // the label chain, and for each PC-relative instruction in the chain checking
  // if pending unresolved information exists. Its complexity is proportional to
  // the length of the label chain.
  void DeleteUnresolvedBranchInfoForLabelTraverse(Label* label);

 private:
  PositionsRecorder positions_recorder_;
  friend class PositionsRecorder;
  friend class EnsureSpace;
  friend class ConstPool;
};

class PatchingAssembler : public Assembler {
 public:
  // Create an Assembler with a buffer starting at 'start'.
  // The buffer size is
  //   size of instructions to patch + kGap
  // Where kGap is the distance from which the Assembler tries to grow the
  // buffer.
  // If more or fewer instructions than expected are generated or if some
  // relocation information takes space in the buffer, the PatchingAssembler
  // will crash trying to grow the buffer.
  PatchingAssembler(Instruction* start, unsigned count)
    : Assembler(NULL,
                reinterpret_cast<byte*>(start),
                count * kInstructionSize + kGap) {
    StartBlockPools();
  }

  PatchingAssembler(byte* start, unsigned count)
    : Assembler(NULL, start, count * kInstructionSize + kGap) {
    // Block constant pool emission.
    StartBlockPools();
  }

  ~PatchingAssembler() {
    // Const pool should still be blocked.
    DCHECK(is_const_pool_blocked());
    EndBlockPools();
    // Verify we have generated the number of instruction we expected.
    DCHECK((pc_offset() + kGap) == buffer_size_);
    // Verify no relocation information has been emitted.
    DCHECK(IsConstPoolEmpty());
    // Flush the Instruction cache.
    size_t length = buffer_size_ - kGap;
    CpuFeatures::FlushICache(buffer_, length);
  }

  // See definition of PatchAdrFar() for details.
  static const int kAdrFarPatchableNNops = 2;
  static const int kAdrFarPatchableNInstrs = kAdrFarPatchableNNops + 2;
  void PatchAdrFar(int64_t target_offset);
};


class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) {
    assembler->CheckBufferSpace();
  }
};

} }  // namespace v8::internal

#endif  // V8_ARM64_ASSEMBLER_ARM64_H_

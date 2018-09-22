// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_ASSEMBLER_ARM64_H_
#define V8_ARM64_ASSEMBLER_ARM64_H_

#include <deque>
#include <list>
#include <map>
#include <vector>

#include "src/arm64/constants-arm64.h"
#include "src/arm64/instructions-arm64.h"
#include "src/assembler.h"
#include "src/base/optional.h"
#include "src/globals.h"
#include "src/utils.h"


namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Registers.
// clang-format off
#define GENERAL_REGISTER_CODE_LIST(R)                     \
  R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)          \
  R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)         \
  R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23)         \
  R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

#define GENERAL_REGISTERS(R)                              \
  R(x0)  R(x1)  R(x2)  R(x3)  R(x4)  R(x5)  R(x6)  R(x7)  \
  R(x8)  R(x9)  R(x10) R(x11) R(x12) R(x13) R(x14) R(x15) \
  R(x16) R(x17) R(x18) R(x19) R(x20) R(x21) R(x22) R(x23) \
  R(x24) R(x25) R(x26) R(x27) R(x28) R(x29) R(x30) R(x31)

#define ALLOCATABLE_GENERAL_REGISTERS(R)                  \
  R(x0)  R(x1)  R(x2)  R(x3)  R(x4)  R(x5)  R(x6)  R(x7)  \
  R(x8)  R(x9)  R(x10) R(x11) R(x12) R(x13) R(x14) R(x15) \
  R(x18) R(x19) R(x20) R(x21) R(x22) R(x23) R(x24) R(x25) \
  R(x27) R(x28)

#define FLOAT_REGISTERS(V)                                \
  V(s0)  V(s1)  V(s2)  V(s3)  V(s4)  V(s5)  V(s6)  V(s7)  \
  V(s8)  V(s9)  V(s10) V(s11) V(s12) V(s13) V(s14) V(s15) \
  V(s16) V(s17) V(s18) V(s19) V(s20) V(s21) V(s22) V(s23) \
  V(s24) V(s25) V(s26) V(s27) V(s28) V(s29) V(s30) V(s31)

#define DOUBLE_REGISTERS(R)                               \
  R(d0)  R(d1)  R(d2)  R(d3)  R(d4)  R(d5)  R(d6)  R(d7)  \
  R(d8)  R(d9)  R(d10) R(d11) R(d12) R(d13) R(d14) R(d15) \
  R(d16) R(d17) R(d18) R(d19) R(d20) R(d21) R(d22) R(d23) \
  R(d24) R(d25) R(d26) R(d27) R(d28) R(d29) R(d30) R(d31)

#define SIMD128_REGISTERS(V)                              \
  V(q0)  V(q1)  V(q2)  V(q3)  V(q4)  V(q5)  V(q6)  V(q7)  \
  V(q8)  V(q9)  V(q10) V(q11) V(q12) V(q13) V(q14) V(q15) \
  V(q16) V(q17) V(q18) V(q19) V(q20) V(q21) V(q22) V(q23) \
  V(q24) V(q25) V(q26) V(q27) V(q28) V(q29) V(q30) V(q31)

// Register d29 could be allocated, but we keep an even length list here, in
// order to make stack alignment easier for save and restore.
#define ALLOCATABLE_DOUBLE_REGISTERS(R)                   \
  R(d0)  R(d1)  R(d2)  R(d3)  R(d4)  R(d5)  R(d6)  R(d7)  \
  R(d8)  R(d9)  R(d10) R(d11) R(d12) R(d13) R(d14) R(d16) \
  R(d17) R(d18) R(d19) R(d20) R(d21) R(d22) R(d23) R(d24) \
  R(d25) R(d26) R(d27) R(d28)
// clang-format on

constexpr int kRegListSizeInBits = sizeof(RegList) * kBitsPerByte;

const int kNumRegs = kNumberOfRegisters;
// Registers x0-x17 are caller-saved.
const int kNumJSCallerSaved = 18;
const RegList kJSCallerSaved = 0x3ffff;

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of eight.
// TODO(all): Refine this number.
const int kNumSafepointRegisters = 32;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
#define kSafepointSavedRegisters CPURegList::GetSafepointSavedRegisters().list()
#define kNumSafepointSavedRegisters \
  CPURegList::GetSafepointSavedRegisters().Count()

// Some CPURegister methods can return Register and VRegister types, so we
// need to declare them in advance.
class Register;
class VRegister;

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class CPURegister : public RegisterBase<CPURegister, kRegAfterLast> {
 public:
  enum RegisterType {
    kRegister,
    kVRegister,
    kNoRegister
  };

  static constexpr CPURegister no_reg() {
    return CPURegister{0, 0, kNoRegister};
  }

  template <int code, int size, RegisterType type>
  static constexpr CPURegister Create() {
    static_assert(IsValid(code, size, type), "Cannot create invalid registers");
    return CPURegister{code, size, type};
  }

  static CPURegister Create(int code, int size, RegisterType type) {
    DCHECK(IsValid(code, size, type));
    return CPURegister{code, size, type};
  }

  RegisterType type() const { return reg_type_; }
  int SizeInBits() const {
    DCHECK(IsValid());
    return reg_size_;
  }
  int SizeInBytes() const {
    DCHECK(IsValid());
    DCHECK_EQ(SizeInBits() % 8, 0);
    return reg_size_ / 8;
  }
  bool Is8Bits() const {
    DCHECK(IsValid());
    return reg_size_ == 8;
  }
  bool Is16Bits() const {
    DCHECK(IsValid());
    return reg_size_ == 16;
  }
  bool Is32Bits() const {
    DCHECK(IsValid());
    return reg_size_ == 32;
  }
  bool Is64Bits() const {
    DCHECK(IsValid());
    return reg_size_ == 64;
  }
  bool Is128Bits() const {
    DCHECK(IsValid());
    return reg_size_ == 128;
  }
  bool IsValid() const { return reg_type_ != kNoRegister; }
  bool IsNone() const { return reg_type_ == kNoRegister; }
  bool Is(const CPURegister& other) const {
    return Aliases(other) && (reg_size_ == other.reg_size_);
  }
  bool Aliases(const CPURegister& other) const {
    return (reg_code_ == other.reg_code_) && (reg_type_ == other.reg_type_);
  }

  bool IsZero() const;
  bool IsSP() const;

  bool IsRegister() const { return reg_type_ == kRegister; }
  bool IsVRegister() const { return reg_type_ == kVRegister; }

  bool IsFPRegister() const { return IsS() || IsD(); }

  bool IsW() const { return IsRegister() && Is32Bits(); }
  bool IsX() const { return IsRegister() && Is64Bits(); }

  // These assertions ensure that the size and type of the register are as
  // described. They do not consider the number of lanes that make up a vector.
  // So, for example, Is8B() implies IsD(), and Is1D() implies IsD, but IsD()
  // does not imply Is1D() or Is8B().
  // Check the number of lanes, ie. the format of the vector, using methods such
  // as Is8B(), Is1D(), etc. in the VRegister class.
  bool IsV() const { return IsVRegister(); }
  bool IsB() const { return IsV() && Is8Bits(); }
  bool IsH() const { return IsV() && Is16Bits(); }
  bool IsS() const { return IsV() && Is32Bits(); }
  bool IsD() const { return IsV() && Is64Bits(); }
  bool IsQ() const { return IsV() && Is128Bits(); }

  Register Reg() const;
  VRegister VReg() const;

  Register X() const;
  Register W() const;
  VRegister V() const;
  VRegister B() const;
  VRegister H() const;
  VRegister D() const;
  VRegister S() const;
  VRegister Q() const;

  bool IsSameSizeAndType(const CPURegister& other) const;

  bool is(const CPURegister& other) const { return Is(other); }
  bool is_valid() const { return IsValid(); }

 protected:
  int reg_size_;
  RegisterType reg_type_;

  friend class RegisterBase;

  constexpr CPURegister(int code, int size, RegisterType type)
      : RegisterBase(code), reg_size_(size), reg_type_(type) {}

  static constexpr bool IsValidRegister(int code, int size) {
    return (size == kWRegSizeInBits || size == kXRegSizeInBits) &&
           (code < kNumberOfRegisters || code == kSPRegInternalCode);
  }

  static constexpr bool IsValidVRegister(int code, int size) {
    return (size == kBRegSizeInBits || size == kHRegSizeInBits ||
            size == kSRegSizeInBits || size == kDRegSizeInBits ||
            size == kQRegSizeInBits) &&
           code < kNumberOfVRegisters;
  }

  static constexpr bool IsValid(int code, int size, RegisterType type) {
    return (type == kRegister && IsValidRegister(code, size)) ||
           (type == kVRegister && IsValidVRegister(code, size));
  }

  static constexpr bool IsNone(int code, int size, RegisterType type) {
    return type == kNoRegister && code == 0 && size == 0;
  }
};

ASSERT_TRIVIALLY_COPYABLE(CPURegister);

class Register : public CPURegister {
 public:
  static constexpr Register no_reg() { return Register(CPURegister::no_reg()); }

  template <int code, int size>
  static constexpr Register Create() {
    return Register(CPURegister::Create<code, size, CPURegister::kRegister>());
  }

  static Register Create(int code, int size) {
    return Register(CPURegister::Create(code, size, CPURegister::kRegister));
  }

  static Register XRegFromCode(unsigned code);
  static Register WRegFromCode(unsigned code);

  static Register from_code(int code) {
    // Always return an X register.
    return Register::Create(code, kXRegSizeInBits);
  }

  template <int code>
  static Register from_code() {
    // Always return an X register.
    return Register::Create<code, kXRegSizeInBits>();
  }

 private:
  constexpr explicit Register(const CPURegister& r) : CPURegister(r) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);

constexpr bool kPadArguments = true;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

class VRegister : public CPURegister {
 public:
  static constexpr VRegister no_reg() {
    return VRegister(CPURegister::no_reg(), 0);
  }

  template <int code, int size, int lane_count = 1>
  static constexpr VRegister Create() {
    static_assert(IsValidLaneCount(lane_count), "Invalid lane count");
    return VRegister(CPURegister::Create<code, size, kVRegister>(), lane_count);
  }

  static VRegister Create(int code, int size, int lane_count = 1) {
    DCHECK(IsValidLaneCount(lane_count));
    return VRegister(CPURegister::Create(code, size, CPURegister::kVRegister),
                     lane_count);
  }

  static VRegister Create(int reg_code, VectorFormat format) {
    int reg_size = RegisterSizeInBitsFromFormat(format);
    int reg_count = IsVectorFormat(format) ? LaneCountFromFormat(format) : 1;
    return VRegister::Create(reg_code, reg_size, reg_count);
  }

  static VRegister BRegFromCode(unsigned code);
  static VRegister HRegFromCode(unsigned code);
  static VRegister SRegFromCode(unsigned code);
  static VRegister DRegFromCode(unsigned code);
  static VRegister QRegFromCode(unsigned code);
  static VRegister VRegFromCode(unsigned code);

  VRegister V8B() const {
    return VRegister::Create(code(), kDRegSizeInBits, 8);
  }
  VRegister V16B() const {
    return VRegister::Create(code(), kQRegSizeInBits, 16);
  }
  VRegister V4H() const {
    return VRegister::Create(code(), kDRegSizeInBits, 4);
  }
  VRegister V8H() const {
    return VRegister::Create(code(), kQRegSizeInBits, 8);
  }
  VRegister V2S() const {
    return VRegister::Create(code(), kDRegSizeInBits, 2);
  }
  VRegister V4S() const {
    return VRegister::Create(code(), kQRegSizeInBits, 4);
  }
  VRegister V2D() const {
    return VRegister::Create(code(), kQRegSizeInBits, 2);
  }
  VRegister V1D() const {
    return VRegister::Create(code(), kDRegSizeInBits, 1);
  }

  bool Is8B() const { return (Is64Bits() && (lane_count_ == 8)); }
  bool Is16B() const { return (Is128Bits() && (lane_count_ == 16)); }
  bool Is4H() const { return (Is64Bits() && (lane_count_ == 4)); }
  bool Is8H() const { return (Is128Bits() && (lane_count_ == 8)); }
  bool Is2S() const { return (Is64Bits() && (lane_count_ == 2)); }
  bool Is4S() const { return (Is128Bits() && (lane_count_ == 4)); }
  bool Is1D() const { return (Is64Bits() && (lane_count_ == 1)); }
  bool Is2D() const { return (Is128Bits() && (lane_count_ == 2)); }

  // For consistency, we assert the number of lanes of these scalar registers,
  // even though there are no vectors of equivalent total size with which they
  // could alias.
  bool Is1B() const {
    DCHECK(!(Is8Bits() && IsVector()));
    return Is8Bits();
  }
  bool Is1H() const {
    DCHECK(!(Is16Bits() && IsVector()));
    return Is16Bits();
  }
  bool Is1S() const {
    DCHECK(!(Is32Bits() && IsVector()));
    return Is32Bits();
  }

  bool IsLaneSizeB() const { return LaneSizeInBits() == kBRegSizeInBits; }
  bool IsLaneSizeH() const { return LaneSizeInBits() == kHRegSizeInBits; }
  bool IsLaneSizeS() const { return LaneSizeInBits() == kSRegSizeInBits; }
  bool IsLaneSizeD() const { return LaneSizeInBits() == kDRegSizeInBits; }

  bool IsScalar() const { return lane_count_ == 1; }
  bool IsVector() const { return lane_count_ > 1; }

  bool IsSameFormat(const VRegister& other) const {
    return (reg_size_ == other.reg_size_) && (lane_count_ == other.lane_count_);
  }

  int LaneCount() const { return lane_count_; }

  unsigned LaneSizeInBytes() const { return SizeInBytes() / lane_count_; }

  unsigned LaneSizeInBits() const { return LaneSizeInBytes() * 8; }

  static constexpr int kMaxNumRegisters = kNumberOfVRegisters;
  STATIC_ASSERT(kMaxNumRegisters == kDoubleAfterLast);

  static VRegister from_code(int code) {
    // Always return a D register.
    return VRegister::Create(code, kDRegSizeInBits);
  }

 private:
  int lane_count_;

  constexpr explicit VRegister(const CPURegister& r, int lane_count)
      : CPURegister(r), lane_count_(lane_count) {}

  static constexpr bool IsValidLaneCount(int lane_count) {
    return base::bits::IsPowerOfTwo(lane_count) && lane_count <= 16;
  }
};

ASSERT_TRIVIALLY_COPYABLE(VRegister);

// No*Reg is used to indicate an unused argument, or an error case. Note that
// these all compare equal (using the Is() method). The Register and VRegister
// variants are provided for convenience.
constexpr Register NoReg = Register::no_reg();
constexpr VRegister NoVReg = VRegister::no_reg();
constexpr CPURegister NoCPUReg = CPURegister::no_reg();
constexpr Register no_reg = NoReg;
constexpr VRegister no_dreg = NoVReg;

#define DEFINE_REGISTER(register_class, name, ...) \
  constexpr register_class name = register_class::Create<__VA_ARGS__>()
#define ALIAS_REGISTER(register_class, alias, name) \
  constexpr register_class alias = name

#define DEFINE_REGISTERS(N)                            \
  DEFINE_REGISTER(Register, w##N, N, kWRegSizeInBits); \
  DEFINE_REGISTER(Register, x##N, N, kXRegSizeInBits);
GENERAL_REGISTER_CODE_LIST(DEFINE_REGISTERS)
#undef DEFINE_REGISTERS

DEFINE_REGISTER(Register, wsp, kSPRegInternalCode, kWRegSizeInBits);
DEFINE_REGISTER(Register, sp, kSPRegInternalCode, kXRegSizeInBits);

#define DEFINE_VREGISTERS(N)                            \
  DEFINE_REGISTER(VRegister, b##N, N, kBRegSizeInBits); \
  DEFINE_REGISTER(VRegister, h##N, N, kHRegSizeInBits); \
  DEFINE_REGISTER(VRegister, s##N, N, kSRegSizeInBits); \
  DEFINE_REGISTER(VRegister, d##N, N, kDRegSizeInBits); \
  DEFINE_REGISTER(VRegister, q##N, N, kQRegSizeInBits); \
  DEFINE_REGISTER(VRegister, v##N, N, kQRegSizeInBits);
GENERAL_REGISTER_CODE_LIST(DEFINE_VREGISTERS)
#undef DEFINE_VREGISTERS

#undef DEFINE_REGISTER

// Registers aliases.
ALIAS_REGISTER(VRegister, v8_, v8);  // Avoid conflicts with namespace v8.
ALIAS_REGISTER(Register, ip0, x16);
ALIAS_REGISTER(Register, ip1, x17);
ALIAS_REGISTER(Register, wip0, w16);
ALIAS_REGISTER(Register, wip1, w17);
// Root register.
ALIAS_REGISTER(Register, kRootRegister, x26);
ALIAS_REGISTER(Register, rr, x26);
// Context pointer register.
ALIAS_REGISTER(Register, cp, x27);
ALIAS_REGISTER(Register, fp, x29);
ALIAS_REGISTER(Register, lr, x30);
ALIAS_REGISTER(Register, xzr, x31);
ALIAS_REGISTER(Register, wzr, w31);

// Register used for padding stack slots.
ALIAS_REGISTER(Register, padreg, x31);

// Keeps the 0 double value.
ALIAS_REGISTER(VRegister, fp_zero, d15);
// MacroAssembler fixed V Registers.
ALIAS_REGISTER(VRegister, fp_fixed1, d28);
ALIAS_REGISTER(VRegister, fp_fixed2, d29);

// MacroAssembler scratch V registers.
ALIAS_REGISTER(VRegister, fp_scratch, d30);
ALIAS_REGISTER(VRegister, fp_scratch1, d30);
ALIAS_REGISTER(VRegister, fp_scratch2, d31);

#undef ALIAS_REGISTER

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
bool AreSameSizeAndType(
    const CPURegister& reg1, const CPURegister& reg2 = NoCPUReg,
    const CPURegister& reg3 = NoCPUReg, const CPURegister& reg4 = NoCPUReg,
    const CPURegister& reg5 = NoCPUReg, const CPURegister& reg6 = NoCPUReg,
    const CPURegister& reg7 = NoCPUReg, const CPURegister& reg8 = NoCPUReg);

// AreSameFormat returns true if all of the specified VRegisters have the same
// vector format. Arguments set to NoVReg are ignored, as are any subsequent
// arguments. At least one argument (reg1) must be valid (not NoVReg).
bool AreSameFormat(const VRegister& reg1, const VRegister& reg2,
                   const VRegister& reg3 = NoVReg,
                   const VRegister& reg4 = NoVReg);

// AreConsecutive returns true if all of the specified VRegisters are
// consecutive in the register file. Arguments may be set to NoVReg, and if so,
// subsequent arguments must also be NoVReg. At least one argument (reg1) must
// be valid (not NoVReg).
bool AreConsecutive(const VRegister& reg1, const VRegister& reg2,
                    const VRegister& reg3 = NoVReg,
                    const VRegister& reg4 = NoVReg);

typedef VRegister FloatRegister;
typedef VRegister DoubleRegister;
typedef VRegister Simd128Register;

// -----------------------------------------------------------------------------
// Lists of registers.
class CPURegList {
 public:
  template <typename... CPURegisters>
  explicit CPURegList(CPURegister reg0, CPURegisters... regs)
      : list_(CPURegister::ListOf(reg0, regs...)),
        size_(reg0.SizeInBits()),
        type_(reg0.type()) {
    DCHECK(AreSameSizeAndType(reg0, regs...));
    DCHECK(IsValid());
  }

  CPURegList(CPURegister::RegisterType type, int size, RegList list)
      : list_(list), size_(size), type_(type) {
    DCHECK(IsValid());
  }

  CPURegList(CPURegister::RegisterType type, int size, int first_reg,
             int last_reg)
      : size_(size), type_(type) {
    DCHECK(
        ((type == CPURegister::kRegister) && (last_reg < kNumberOfRegisters)) ||
        ((type == CPURegister::kVRegister) &&
         (last_reg < kNumberOfVRegisters)));
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
  static CPURegList GetCalleeSaved(int size = kXRegSizeInBits);
  static CPURegList GetCalleeSavedV(int size = kDRegSizeInBits);

  // AAPCS64 caller-saved registers. Note that this includes lr.
  // TODO(all): Determine how we handle d8-d15 being callee-saved, but the top
  // 64-bits being caller-saved.
  static CPURegList GetCallerSaved(int size = kXRegSizeInBits);
  static CPURegList GetCallerSavedV(int size = kDRegSizeInBits);

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
    if (!other1.IsNone() && (other1.type() == type_)) list |= other1.bit();
    if (!other2.IsNone() && (other2.type() == type_)) list |= other2.bit();
    if (!other3.IsNone() && (other3.type() == type_)) list |= other3.bit();
    if (!other4.IsNone() && (other4.type() == type_)) list |= other4.bit();
    return (list_ & list) != 0;
  }

  int Count() const {
    DCHECK(IsValid());
    return CountSetBits(list_, kRegListSizeInBits);
  }

  int RegisterSizeInBits() const {
    DCHECK(IsValid());
    return size_;
  }

  int RegisterSizeInBytes() const {
    int size_in_bits = RegisterSizeInBits();
    DCHECK_EQ(size_in_bits % kBitsPerByte, 0);
    return size_in_bits / kBitsPerByte;
  }

  int TotalSizeInBytes() const {
    DCHECK(IsValid());
    return RegisterSizeInBytes() * Count();
  }

 private:
  RegList list_;
  int size_;
  CPURegister::RegisterType type_;

  bool IsValid() const {
    constexpr RegList kValidRegisters{0x8000000ffffffff};
    constexpr RegList kValidVRegisters{0x0000000ffffffff};
    switch (type_) {
      case CPURegister::kRegister:
        return (list_ & kValidRegisters) == list_;
      case CPURegister::kVRegister:
        return (list_ & kValidVRegisters) == list_;
      case CPURegister::kNoRegister:
        return list_ == 0;
      default:
        UNREACHABLE();
    }
  }
};


// AAPCS64 callee-saved registers.
#define kCalleeSaved CPURegList::GetCalleeSaved()
#define kCalleeSavedV CPURegList::GetCalleeSavedV()

// AAPCS64 caller-saved registers. Note that this includes lr.
#define kCallerSaved CPURegList::GetCallerSaved()
#define kCallerSavedV CPURegList::GetCallerSavedV()

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
  void InitializeHandle(Handle<HeapObject> value);

  int64_t value_;
  RelocInfo::Mode rmode_;
};


// -----------------------------------------------------------------------------
// Operands.
constexpr int kSmiShift = kSmiTagSize + kSmiShiftSize;
constexpr uint64_t kSmiShiftMask = (1UL << kSmiShift) - 1;

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

  static Operand EmbeddedNumber(double number);  // Smi or HeapNumber.
  static Operand EmbeddedCode(CodeStub* stub);
  static Operand EmbeddedStringConstant(const StringConstantBase* str);

  inline bool IsHeapObjectRequest() const;
  inline HeapObjectRequest heap_object_request() const;
  inline Immediate immediate_for_heap_object_request() const;

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
  inline RelocInfo::Mode ImmediateRMode() const;
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
  base::Optional<HeapObjectRequest> heap_object_request_;
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
  explicit ConstPool(Assembler* assm) : assm_(assm), first_use_(-1) {}
  // Returns true when we need to write RelocInfo and false when we do not.
  bool RecordEntry(intptr_t data, RelocInfo::Mode mode);
  int EntryCount() const { return static_cast<int>(entries_.size()); }
  bool IsEmpty() const { return entries_.empty(); }
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
  void EmitMarker();
  void EmitGuard();
  void EmitEntries();

  typedef std::map<uint64_t, int> SharedEntryMap;
  // Adds a shared entry to entries_, using 'entry_map' to determine whether we
  // already track this entry. Returns true if this is the first time we add
  // this entry, false otherwise.
  bool AddSharedEntry(SharedEntryMap& entry_map, uint64_t data, int offset);

  Assembler* assm_;
  // Keep track of the first instruction requiring a constant pool entry
  // since the previous constant pool was emitted.
  int first_use_;

  // Map of data to index in entries_ for shared entries.
  SharedEntryMap shared_entries_;

  // Map of address of handle to index in entries_. We need to keep track of
  // code targets separately from other shared entries, as they can be
  // relocated.
  SharedEntryMap handle_to_index_map_;

  // Values, pc offset(s) of entries. Use a vector to preserve the order of
  // insertion, as the serializer expects code target RelocInfo to point to
  // constant pool addresses in an ascending order.
  std::vector<std::pair<uint64_t, std::vector<int> > > entries_;
};


// -----------------------------------------------------------------------------
// Assembler.

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
  // The descriptor (desc) can be nullptr. In that case, the code is finalized
  // as usual, but the descriptor is not populated.
  void GetCode(Isolate* isolate, CodeDesc* desc);

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);

  inline void Unreachable();

  // Label --------------------------------------------------------------------
  // Bind a label to the current pc. Note that labels can only be bound once,
  // and if labels are linked to other instructions, they _must_ be bound
  // before they go out of scope.
  void bind(Label* label);


  // RelocInfo and pools ------------------------------------------------------

  // Record relocation information for current pc_.
  enum ConstantPoolMode { NEEDS_POOL_ENTRY, NO_POOL_ENTRY };
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0,
                       ConstantPoolMode constant_pool_mode = NEEDS_POOL_ENTRY);

  // Generate a B immediate instruction with the corresponding relocation info.
  // 'offset' is the immediate to encode in the B instruction (so it is the
  // difference between the target and the PC of the instruction, divided by
  // the instruction size).
  void near_jump(int offset, RelocInfo::Mode rmode);
  // Generate a BL immediate instruction with the corresponding relocation info.
  // As for near_jump, 'offset' is the immediate to encode in the BL
  // instruction.
  void near_call(int offset, RelocInfo::Mode rmode);
  // Generate a BL immediate instruction with the corresponding relocation info
  // for the input HeapObjectRequest.
  void near_call(HeapObjectRequest request);

  // Return the address in the constant pool of the code target address used by
  // the branch/call instruction at pc.
  inline static Address target_pointer_address_at(Address pc);

  // Read/Modify the code target address in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  inline static Address target_address_at(Address pc, Address constant_pool);
  inline static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Returns the handle for the code object called at 'pc'.
  // This might need to be temporarily encoded as an offset into code_targets_.
  inline Handle<Code> code_target_object_handle_at(Address pc);

  // Returns the target address for a runtime function for the call encoded
  // at 'pc'.
  // Runtime entries can be temporarily encoded as the offset between the
  // runtime function entrypoint and the code range start (stored in the
  // code_range_start field), in order to be encodable as we generate the code,
  // before it is moved into the code space.
  inline Address runtime_entry_at(Address pc);

  // Return the code target address at a call site from the return address of
  // that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // This sets the branch destination. 'location' here can be either the pc of
  // an immediate branch or the address of an entry in the constant pool.
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(Address location,
                                                           Code* code,
                                                           Address target);

  // Get the size of the special target encoded at 'location'.
  inline static int deserialization_special_target_size(Address location);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  // This value is used in the serialization process and must be zero for
  // ARM64, as the code target is split across multiple instructions and does
  // not exist separately in the code, so the serializer should not step
  // forwards in memory after a target is resolved and written.
  static constexpr int kSpecialTargetSize = 0;

  // The sizes of the call sequences emitted by MacroAssembler::Call.
  //
  // A "near" call is encoded in a BL immediate instruction:
  //  bl target
  //
  // whereas a "far" call will be encoded like this:
  //  ldr temp, =target
  //  blr temp
  static constexpr int kNearCallSize = 1 * kInstrSize;
  static constexpr int kFarCallSize = 2 * kInstrSize;

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

  // Return the number of instructions generated from label to the
  // current position.
  uint64_t InstructionsGeneratedSince(const Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Prevent contant pool emission until EndBlockConstPool is called.
  // Call to this function can be nested but must be followed by an equal
  // number of calls to EndBlockConstpool.
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
  // number of calls to EndBlockConstpool.
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
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  int buffer_space() const;

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

  // Bitwise and.
  void and_(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bit clear immediate.
  void bic(const VRegister& vd, const int imm8, const int left_shift = 0);

  // Bit clear.
  void bic(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bitwise insert if false.
  void bif(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bitwise insert if true.
  void bit(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bitwise select.
  void bsl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Polynomial multiply.
  void pmul(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Vector move immediate.
  void movi(const VRegister& vd, const uint64_t imm, Shift shift = LSL,
            const int shift_amount = 0);

  // Bitwise not.
  void mvn(const VRegister& vd, const VRegister& vn);

  // Vector move inverted immediate.
  void mvni(const VRegister& vd, const int imm8, Shift shift = LSL,
            const int shift_amount = 0);

  // Signed saturating accumulate of unsigned value.
  void suqadd(const VRegister& vd, const VRegister& vn);

  // Unsigned saturating accumulate of signed value.
  void usqadd(const VRegister& vd, const VRegister& vn);

  // Absolute value.
  void abs(const VRegister& vd, const VRegister& vn);

  // Signed saturating absolute value.
  void sqabs(const VRegister& vd, const VRegister& vn);

  // Negate.
  void neg(const VRegister& vd, const VRegister& vn);

  // Signed saturating negate.
  void sqneg(const VRegister& vd, const VRegister& vn);

  // Bitwise not.
  void not_(const VRegister& vd, const VRegister& vn);

  // Extract narrow.
  void xtn(const VRegister& vd, const VRegister& vn);

  // Extract narrow (second part).
  void xtn2(const VRegister& vd, const VRegister& vn);

  // Signed saturating extract narrow.
  void sqxtn(const VRegister& vd, const VRegister& vn);

  // Signed saturating extract narrow (second part).
  void sqxtn2(const VRegister& vd, const VRegister& vn);

  // Unsigned saturating extract narrow.
  void uqxtn(const VRegister& vd, const VRegister& vn);

  // Unsigned saturating extract narrow (second part).
  void uqxtn2(const VRegister& vd, const VRegister& vn);

  // Signed saturating extract unsigned narrow.
  void sqxtun(const VRegister& vd, const VRegister& vn);

  // Signed saturating extract unsigned narrow (second part).
  void sqxtun2(const VRegister& vd, const VRegister& vn);

  // Move register to register.
  void mov(const VRegister& vd, const VRegister& vn);

  // Bitwise not or.
  void orn(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bitwise exclusive or.
  void eor(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bitwise or (A | B).
  void orr(const Register& rd, const Register& rn, const Operand& operand);

  // Bitwise or.
  void orr(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Bitwise or immediate.
  void orr(const VRegister& vd, const int imm8, const int left_shift = 0);

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
  void bfm(const Register& rd, const Register& rn, int immr, int imms);

  // Signed bitfield move.
  void sbfm(const Register& rd, const Register& rn, int immr, int imms);

  // Unsigned bitfield move.
  void ubfm(const Register& rd, const Register& rn, int immr, int imms);

  // Bfm aliases.
  // Bitfield insert.
  void bfi(const Register& rd, const Register& rn, int lsb, int width) {
    DCHECK_GE(width, 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    bfm(rd, rn, (rd.SizeInBits() - lsb) & (rd.SizeInBits() - 1), width - 1);
  }

  // Bitfield extract and insert low.
  void bfxil(const Register& rd, const Register& rn, int lsb, int width) {
    DCHECK_GE(width, 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    bfm(rd, rn, lsb, lsb + width - 1);
  }

  // Sbfm aliases.
  // Arithmetic shift right.
  void asr(const Register& rd, const Register& rn, int shift) {
    DCHECK(shift < rd.SizeInBits());
    sbfm(rd, rn, shift, rd.SizeInBits() - 1);
  }

  // Signed bitfield insert in zero.
  void sbfiz(const Register& rd, const Register& rn, int lsb, int width) {
    DCHECK_GE(width, 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    sbfm(rd, rn, (rd.SizeInBits() - lsb) & (rd.SizeInBits() - 1), width - 1);
  }

  // Signed bitfield extract.
  void sbfx(const Register& rd, const Register& rn, int lsb, int width) {
    DCHECK_GE(width, 1);
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
  void lsl(const Register& rd, const Register& rn, int shift) {
    int reg_size = rd.SizeInBits();
    DCHECK(shift < reg_size);
    ubfm(rd, rn, (reg_size - shift) % reg_size, reg_size - shift - 1);
  }

  // Logical shift right.
  void lsr(const Register& rd, const Register& rn, int shift) {
    DCHECK(shift < rd.SizeInBits());
    ubfm(rd, rn, shift, rd.SizeInBits() - 1);
  }

  // Unsigned bitfield insert in zero.
  void ubfiz(const Register& rd, const Register& rn, int lsb, int width) {
    DCHECK_GE(width, 1);
    DCHECK(lsb + width <= rn.SizeInBits());
    ubfm(rd, rn, (rd.SizeInBits() - lsb) & (rd.SizeInBits() - 1), width - 1);
  }

  // Unsigned bitfield extract.
  void ubfx(const Register& rd, const Register& rn, int lsb, int width) {
    DCHECK_GE(width, 1);
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
  void extr(const Register& rd, const Register& rn, const Register& rm,
            int lsb);

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

  // Load literal to register from a pc relative address.
  void ldr_pcrel(const CPURegister& rt, int imm19);

  // Load literal to register.
  void ldr(const CPURegister& rt, const Immediate& imm);
  void ldr(const CPURegister& rt, const Operand& operand);

  // Load-acquire word.
  void ldar(const Register& rt, const Register& rn);

  // Load-acquire exclusive word.
  void ldaxr(const Register& rt, const Register& rn);

  // Store-release word.
  void stlr(const Register& rt, const Register& rn);

  // Store-release exclusive word.
  void stlxr(const Register& rs, const Register& rt, const Register& rn);

  // Load-acquire byte.
  void ldarb(const Register& rt, const Register& rn);

  // Load-acquire exclusive byte.
  void ldaxrb(const Register& rt, const Register& rn);

  // Store-release byte.
  void stlrb(const Register& rt, const Register& rn);

  // Store-release exclusive byte.
  void stlxrb(const Register& rs, const Register& rt, const Register& rn);

  // Load-acquire half-word.
  void ldarh(const Register& rt, const Register& rn);

  // Load-acquire exclusive half-word.
  void ldaxrh(const Register& rt, const Register& rn);

  // Store-release half-word.
  void stlrh(const Register& rt, const Register& rn);

  // Store-release exclusive half-word.
  void stlxrh(const Register& rs, const Register& rt, const Register& rn);

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

  // Conditional speculation barrier.
  void csdb();

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

  // Add.
  void add(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned halving add.
  void uhadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Subtract.
  void sub(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed halving add.
  void shadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Multiply by scalar element.
  void mul(const VRegister& vd, const VRegister& vn, const VRegister& vm,
           int vm_index);

  // Multiply-add by scalar element.
  void mla(const VRegister& vd, const VRegister& vn, const VRegister& vm,
           int vm_index);

  // Multiply-subtract by scalar element.
  void mls(const VRegister& vd, const VRegister& vn, const VRegister& vm,
           int vm_index);

  // Signed long multiply-add by scalar element.
  void smlal(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // Signed long multiply-add by scalar element (second part).
  void smlal2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              int vm_index);

  // Unsigned long multiply-add by scalar element.
  void umlal(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // Unsigned long multiply-add by scalar element (second part).
  void umlal2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              int vm_index);

  // Signed long multiply-sub by scalar element.
  void smlsl(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // Signed long multiply-sub by scalar element (second part).
  void smlsl2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              int vm_index);

  // Unsigned long multiply-sub by scalar element.
  void umlsl(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // Unsigned long multiply-sub by scalar element (second part).
  void umlsl2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              int vm_index);

  // Signed long multiply by scalar element.
  void smull(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // Signed long multiply by scalar element (second part).
  void smull2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              int vm_index);

  // Unsigned long multiply by scalar element.
  void umull(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // Unsigned long multiply by scalar element (second part).
  void umull2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              int vm_index);

  // Add narrow returning high half.
  void addhn(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Add narrow returning high half (second part).
  void addhn2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating double long multiply by element.
  void sqdmull(const VRegister& vd, const VRegister& vn, const VRegister& vm,
               int vm_index);

  // Signed saturating double long multiply by element (second part).
  void sqdmull2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                int vm_index);

  // Signed saturating doubling long multiply-add by element.
  void sqdmlal(const VRegister& vd, const VRegister& vn, const VRegister& vm,
               int vm_index);

  // Signed saturating doubling long multiply-add by element (second part).
  void sqdmlal2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                int vm_index);

  // Signed saturating doubling long multiply-sub by element.
  void sqdmlsl(const VRegister& vd, const VRegister& vn, const VRegister& vm,
               int vm_index);

  // Signed saturating doubling long multiply-sub by element (second part).
  void sqdmlsl2(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                int vm_index);

  // Compare bitwise to zero.
  void cmeq(const VRegister& vd, const VRegister& vn, int value);

  // Compare signed greater than or equal to zero.
  void cmge(const VRegister& vd, const VRegister& vn, int value);

  // Compare signed greater than zero.
  void cmgt(const VRegister& vd, const VRegister& vn, int value);

  // Compare signed less than or equal to zero.
  void cmle(const VRegister& vd, const VRegister& vn, int value);

  // Compare signed less than zero.
  void cmlt(const VRegister& vd, const VRegister& vn, int value);

  // Unsigned rounding halving add.
  void urhadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Compare equal.
  void cmeq(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Compare signed greater than or equal.
  void cmge(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Compare signed greater than.
  void cmgt(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Compare unsigned higher.
  void cmhi(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Compare unsigned higher or same.
  void cmhs(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Compare bitwise test bits nonzero.
  void cmtst(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed shift left by register.
  void sshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned shift left by register.
  void ushl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling long multiply-subtract.
  void sqdmlsl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling long multiply-subtract (second part).
  void sqdmlsl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling long multiply.
  void sqdmull(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling long multiply (second part).
  void sqdmull2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling multiply returning high half.
  void sqdmulh(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating rounding doubling multiply returning high half.
  void sqrdmulh(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling multiply element returning high half.
  void sqdmulh(const VRegister& vd, const VRegister& vn, const VRegister& vm,
               int vm_index);

  // Signed saturating rounding doubling multiply element returning high half.
  void sqrdmulh(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                int vm_index);

  // Unsigned long multiply long.
  void umull(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned long multiply (second part).
  void umull2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Rounding add narrow returning high half.
  void raddhn(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Subtract narrow returning high half.
  void subhn(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Subtract narrow returning high half (second part).
  void subhn2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Rounding add narrow returning high half (second part).
  void raddhn2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Rounding subtract narrow returning high half.
  void rsubhn(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Rounding subtract narrow returning high half (second part).
  void rsubhn2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating shift left by register.
  void sqshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned saturating shift left by register.
  void uqshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed rounding shift left by register.
  void srshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned rounding shift left by register.
  void urshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating rounding shift left by register.
  void sqrshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned saturating rounding shift left by register.
  void uqrshl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed absolute difference.
  void sabd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned absolute difference and accumulate.
  void uaba(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Shift left by immediate and insert.
  void sli(const VRegister& vd, const VRegister& vn, int shift);

  // Shift right by immediate and insert.
  void sri(const VRegister& vd, const VRegister& vn, int shift);

  // Signed maximum.
  void smax(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed pairwise maximum.
  void smaxp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Add across vector.
  void addv(const VRegister& vd, const VRegister& vn);

  // Signed add long across vector.
  void saddlv(const VRegister& vd, const VRegister& vn);

  // Unsigned add long across vector.
  void uaddlv(const VRegister& vd, const VRegister& vn);

  // FP maximum number across vector.
  void fmaxnmv(const VRegister& vd, const VRegister& vn);

  // FP maximum across vector.
  void fmaxv(const VRegister& vd, const VRegister& vn);

  // FP minimum number across vector.
  void fminnmv(const VRegister& vd, const VRegister& vn);

  // FP minimum across vector.
  void fminv(const VRegister& vd, const VRegister& vn);

  // Signed maximum across vector.
  void smaxv(const VRegister& vd, const VRegister& vn);

  // Signed minimum.
  void smin(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed minimum pairwise.
  void sminp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed minimum across vector.
  void sminv(const VRegister& vd, const VRegister& vn);

  // One-element structure store from one register.
  void st1(const VRegister& vt, const MemOperand& src);

  // One-element structure store from two registers.
  void st1(const VRegister& vt, const VRegister& vt2, const MemOperand& src);

  // One-element structure store from three registers.
  void st1(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const MemOperand& src);

  // One-element structure store from four registers.
  void st1(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const VRegister& vt4, const MemOperand& src);

  // One-element single structure store from one lane.
  void st1(const VRegister& vt, int lane, const MemOperand& src);

  // Two-element structure store from two registers.
  void st2(const VRegister& vt, const VRegister& vt2, const MemOperand& src);

  // Two-element single structure store from two lanes.
  void st2(const VRegister& vt, const VRegister& vt2, int lane,
           const MemOperand& src);

  // Three-element structure store from three registers.
  void st3(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const MemOperand& src);

  // Three-element single structure store from three lanes.
  void st3(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           int lane, const MemOperand& src);

  // Four-element structure store from four registers.
  void st4(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const VRegister& vt4, const MemOperand& src);

  // Four-element single structure store from four lanes.
  void st4(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const VRegister& vt4, int lane, const MemOperand& src);

  // Unsigned add long.
  void uaddl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned add long (second part).
  void uaddl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned add wide.
  void uaddw(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned add wide (second part).
  void uaddw2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed add long.
  void saddl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed add long (second part).
  void saddl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed add wide.
  void saddw(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed add wide (second part).
  void saddw2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned subtract long.
  void usubl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned subtract long (second part).
  void usubl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned subtract wide.
  void usubw(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed subtract long.
  void ssubl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed subtract long (second part).
  void ssubl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed integer subtract wide.
  void ssubw(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed integer subtract wide (second part).
  void ssubw2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned subtract wide (second part).
  void usubw2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned maximum.
  void umax(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned pairwise maximum.
  void umaxp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned maximum across vector.
  void umaxv(const VRegister& vd, const VRegister& vn);

  // Unsigned minimum.
  void umin(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned pairwise minimum.
  void uminp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned minimum across vector.
  void uminv(const VRegister& vd, const VRegister& vn);

  // Transpose vectors (primary).
  void trn1(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Transpose vectors (secondary).
  void trn2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unzip vectors (primary).
  void uzp1(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unzip vectors (secondary).
  void uzp2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Zip vectors (primary).
  void zip1(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Zip vectors (secondary).
  void zip2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed shift right by immediate.
  void sshr(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned shift right by immediate.
  void ushr(const VRegister& vd, const VRegister& vn, int shift);

  // Signed rounding shift right by immediate.
  void srshr(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned rounding shift right by immediate.
  void urshr(const VRegister& vd, const VRegister& vn, int shift);

  // Signed shift right by immediate and accumulate.
  void ssra(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned shift right by immediate and accumulate.
  void usra(const VRegister& vd, const VRegister& vn, int shift);

  // Signed rounding shift right by immediate and accumulate.
  void srsra(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned rounding shift right by immediate and accumulate.
  void ursra(const VRegister& vd, const VRegister& vn, int shift);

  // Shift right narrow by immediate.
  void shrn(const VRegister& vd, const VRegister& vn, int shift);

  // Shift right narrow by immediate (second part).
  void shrn2(const VRegister& vd, const VRegister& vn, int shift);

  // Rounding shift right narrow by immediate.
  void rshrn(const VRegister& vd, const VRegister& vn, int shift);

  // Rounding shift right narrow by immediate (second part).
  void rshrn2(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned saturating shift right narrow by immediate.
  void uqshrn(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned saturating shift right narrow by immediate (second part).
  void uqshrn2(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned saturating rounding shift right narrow by immediate.
  void uqrshrn(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned saturating rounding shift right narrow by immediate (second part).
  void uqrshrn2(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating shift right narrow by immediate.
  void sqshrn(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating shift right narrow by immediate (second part).
  void sqshrn2(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating rounded shift right narrow by immediate.
  void sqrshrn(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating rounded shift right narrow by immediate (second part).
  void sqrshrn2(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating shift right unsigned narrow by immediate.
  void sqshrun(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating shift right unsigned narrow by immediate (second part).
  void sqshrun2(const VRegister& vd, const VRegister& vn, int shift);

  // Signed sat rounded shift right unsigned narrow by immediate.
  void sqrshrun(const VRegister& vd, const VRegister& vn, int shift);

  // Signed sat rounded shift right unsigned narrow by immediate (second part).
  void sqrshrun2(const VRegister& vd, const VRegister& vn, int shift);

  // FP reciprocal step.
  void frecps(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP reciprocal estimate.
  void frecpe(const VRegister& vd, const VRegister& vn);

  // FP reciprocal square root estimate.
  void frsqrte(const VRegister& vd, const VRegister& vn);

  // FP reciprocal square root step.
  void frsqrts(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed absolute difference and accumulate long.
  void sabal(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed absolute difference and accumulate long (second part).
  void sabal2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned absolute difference and accumulate long.
  void uabal(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned absolute difference and accumulate long (second part).
  void uabal2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed absolute difference long.
  void sabdl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed absolute difference long (second part).
  void sabdl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned absolute difference long.
  void uabdl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned absolute difference long (second part).
  void uabdl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Polynomial multiply long.
  void pmull(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Polynomial multiply long (second part).
  void pmull2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed long multiply-add.
  void smlal(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed long multiply-add (second part).
  void smlal2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned long multiply-add.
  void umlal(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned long multiply-add (second part).
  void umlal2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed long multiply-sub.
  void smlsl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed long multiply-sub (second part).
  void smlsl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned long multiply-sub.
  void umlsl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned long multiply-sub (second part).
  void umlsl2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed long multiply.
  void smull(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed long multiply (second part).
  void smull2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling long multiply-add.
  void sqdmlal(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating doubling long multiply-add (second part).
  void sqdmlal2(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned absolute difference.
  void uabd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed absolute difference and accumulate.
  void saba(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP instructions.
  // Move immediate to FP register.
  void fmov(const VRegister& fd, double imm);
  void fmov(const VRegister& fd, float imm);

  // Move FP register to register.
  void fmov(const Register& rd, const VRegister& fn);

  // Move register to FP register.
  void fmov(const VRegister& fd, const Register& rn);

  // Move FP register to FP register.
  void fmov(const VRegister& fd, const VRegister& fn);

  // Move 64-bit register to top half of 128-bit FP register.
  void fmov(const VRegister& vd, int index, const Register& rn);

  // Move top half of 128-bit FP register to 64-bit register.
  void fmov(const Register& rd, const VRegister& vn, int index);

  // FP add.
  void fadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP subtract.
  void fsub(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP multiply.
  void fmul(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP compare equal to zero.
  void fcmeq(const VRegister& vd, const VRegister& vn, double imm);

  // FP greater than zero.
  void fcmgt(const VRegister& vd, const VRegister& vn, double imm);

  // FP greater than or equal to zero.
  void fcmge(const VRegister& vd, const VRegister& vn, double imm);

  // FP less than or equal to zero.
  void fcmle(const VRegister& vd, const VRegister& vn, double imm);

  // FP less than to zero.
  void fcmlt(const VRegister& vd, const VRegister& vn, double imm);

  // FP absolute difference.
  void fabd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP pairwise add vector.
  void faddp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP pairwise add scalar.
  void faddp(const VRegister& vd, const VRegister& vn);

  // FP pairwise maximum scalar.
  void fmaxp(const VRegister& vd, const VRegister& vn);

  // FP pairwise maximum number scalar.
  void fmaxnmp(const VRegister& vd, const VRegister& vn);

  // FP pairwise minimum number scalar.
  void fminnmp(const VRegister& vd, const VRegister& vn);

  // FP vector multiply accumulate.
  void fmla(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP vector multiply subtract.
  void fmls(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP vector multiply extended.
  void fmulx(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP absolute greater than or equal.
  void facge(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP absolute greater than.
  void facgt(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP multiply by element.
  void fmul(const VRegister& vd, const VRegister& vn, const VRegister& vm,
            int vm_index);

  // FP fused multiply-add to accumulator by element.
  void fmla(const VRegister& vd, const VRegister& vn, const VRegister& vm,
            int vm_index);

  // FP fused multiply-sub from accumulator by element.
  void fmls(const VRegister& vd, const VRegister& vn, const VRegister& vm,
            int vm_index);

  // FP multiply extended by element.
  void fmulx(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             int vm_index);

  // FP compare equal.
  void fcmeq(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP greater than.
  void fcmgt(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP greater than or equal.
  void fcmge(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP pairwise maximum vector.
  void fmaxp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP pairwise minimum vector.
  void fminp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP pairwise minimum scalar.
  void fminp(const VRegister& vd, const VRegister& vn);

  // FP pairwise maximum number vector.
  void fmaxnmp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP pairwise minimum number vector.
  void fminnmp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP fused multiply-add.
  void fmadd(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             const VRegister& va);

  // FP fused multiply-subtract.
  void fmsub(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             const VRegister& va);

  // FP fused multiply-add and negate.
  void fnmadd(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              const VRegister& va);

  // FP fused multiply-subtract and negate.
  void fnmsub(const VRegister& vd, const VRegister& vn, const VRegister& vm,
              const VRegister& va);

  // FP multiply-negate scalar.
  void fnmul(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP reciprocal exponent scalar.
  void frecpx(const VRegister& vd, const VRegister& vn);

  // FP divide.
  void fdiv(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP maximum.
  void fmax(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP minimum.
  void fmin(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP maximum.
  void fmaxnm(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP minimum.
  void fminnm(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // FP absolute.
  void fabs(const VRegister& vd, const VRegister& vn);

  // FP negate.
  void fneg(const VRegister& vd, const VRegister& vn);

  // FP square root.
  void fsqrt(const VRegister& vd, const VRegister& vn);

  // FP round to integer nearest with ties to away.
  void frinta(const VRegister& vd, const VRegister& vn);

  // FP round to integer, implicit rounding.
  void frinti(const VRegister& vd, const VRegister& vn);

  // FP round to integer toward minus infinity.
  void frintm(const VRegister& vd, const VRegister& vn);

  // FP round to integer nearest with ties to even.
  void frintn(const VRegister& vd, const VRegister& vn);

  // FP round to integer towards plus infinity.
  void frintp(const VRegister& vd, const VRegister& vn);

  // FP round to integer, exact, implicit rounding.
  void frintx(const VRegister& vd, const VRegister& vn);

  // FP round to integer towards zero.
  void frintz(const VRegister& vd, const VRegister& vn);

  // FP compare registers.
  void fcmp(const VRegister& vn, const VRegister& vm);

  // FP compare immediate.
  void fcmp(const VRegister& vn, double value);

  // FP conditional compare.
  void fccmp(const VRegister& vn, const VRegister& vm, StatusFlags nzcv,
             Condition cond);

  // FP conditional select.
  void fcsel(const VRegister& vd, const VRegister& vn, const VRegister& vm,
             Condition cond);

  // Common FP Convert functions.
  void NEONFPConvertToInt(const Register& rd, const VRegister& vn, Instr op);
  void NEONFPConvertToInt(const VRegister& vd, const VRegister& vn, Instr op);

  // FP convert between precisions.
  void fcvt(const VRegister& vd, const VRegister& vn);

  // FP convert to higher precision.
  void fcvtl(const VRegister& vd, const VRegister& vn);

  // FP convert to higher precision (second part).
  void fcvtl2(const VRegister& vd, const VRegister& vn);

  // FP convert to lower precision.
  void fcvtn(const VRegister& vd, const VRegister& vn);

  // FP convert to lower prevision (second part).
  void fcvtn2(const VRegister& vd, const VRegister& vn);

  // FP convert to lower precision, rounding to odd.
  void fcvtxn(const VRegister& vd, const VRegister& vn);

  // FP convert to lower precision, rounding to odd (second part).
  void fcvtxn2(const VRegister& vd, const VRegister& vn);

  // FP convert to signed integer, nearest with ties to away.
  void fcvtas(const Register& rd, const VRegister& vn);

  // FP convert to unsigned integer, nearest with ties to away.
  void fcvtau(const Register& rd, const VRegister& vn);

  // FP convert to signed integer, nearest with ties to away.
  void fcvtas(const VRegister& vd, const VRegister& vn);

  // FP convert to unsigned integer, nearest with ties to away.
  void fcvtau(const VRegister& vd, const VRegister& vn);

  // FP convert to signed integer, round towards -infinity.
  void fcvtms(const Register& rd, const VRegister& vn);

  // FP convert to unsigned integer, round towards -infinity.
  void fcvtmu(const Register& rd, const VRegister& vn);

  // FP convert to signed integer, round towards -infinity.
  void fcvtms(const VRegister& vd, const VRegister& vn);

  // FP convert to unsigned integer, round towards -infinity.
  void fcvtmu(const VRegister& vd, const VRegister& vn);

  // FP convert to signed integer, nearest with ties to even.
  void fcvtns(const Register& rd, const VRegister& vn);

  // FP convert to unsigned integer, nearest with ties to even.
  void fcvtnu(const Register& rd, const VRegister& vn);

  // FP convert to signed integer, nearest with ties to even.
  void fcvtns(const VRegister& rd, const VRegister& vn);

  // FP convert to unsigned integer, nearest with ties to even.
  void fcvtnu(const VRegister& rd, const VRegister& vn);

  // FP convert to signed integer or fixed-point, round towards zero.
  void fcvtzs(const Register& rd, const VRegister& vn, int fbits = 0);

  // FP convert to unsigned integer or fixed-point, round towards zero.
  void fcvtzu(const Register& rd, const VRegister& vn, int fbits = 0);

  // FP convert to signed integer or fixed-point, round towards zero.
  void fcvtzs(const VRegister& vd, const VRegister& vn, int fbits = 0);

  // FP convert to unsigned integer or fixed-point, round towards zero.
  void fcvtzu(const VRegister& vd, const VRegister& vn, int fbits = 0);

  // FP convert to signed integer, round towards +infinity.
  void fcvtps(const Register& rd, const VRegister& vn);

  // FP convert to unsigned integer, round towards +infinity.
  void fcvtpu(const Register& rd, const VRegister& vn);

  // FP convert to signed integer, round towards +infinity.
  void fcvtps(const VRegister& vd, const VRegister& vn);

  // FP convert to unsigned integer, round towards +infinity.
  void fcvtpu(const VRegister& vd, const VRegister& vn);

  // Convert signed integer or fixed point to FP.
  void scvtf(const VRegister& fd, const Register& rn, int fbits = 0);

  // Convert unsigned integer or fixed point to FP.
  void ucvtf(const VRegister& fd, const Register& rn, int fbits = 0);

  // Convert signed integer or fixed-point to FP.
  void scvtf(const VRegister& fd, const VRegister& vn, int fbits = 0);

  // Convert unsigned integer or fixed-point to FP.
  void ucvtf(const VRegister& fd, const VRegister& vn, int fbits = 0);

  // Extract vector from pair of vectors.
  void ext(const VRegister& vd, const VRegister& vn, const VRegister& vm,
           int index);

  // Duplicate vector element to vector or scalar.
  void dup(const VRegister& vd, const VRegister& vn, int vn_index);

  // Duplicate general-purpose register to vector.
  void dup(const VRegister& vd, const Register& rn);

  // Insert vector element from general-purpose register.
  void ins(const VRegister& vd, int vd_index, const Register& rn);

  // Move general-purpose register to a vector element.
  void mov(const VRegister& vd, int vd_index, const Register& rn);

  // Unsigned move vector element to general-purpose register.
  void umov(const Register& rd, const VRegister& vn, int vn_index);

  // Move vector element to general-purpose register.
  void mov(const Register& rd, const VRegister& vn, int vn_index);

  // Move vector element to scalar.
  void mov(const VRegister& vd, const VRegister& vn, int vn_index);

  // Insert vector element from another vector element.
  void ins(const VRegister& vd, int vd_index, const VRegister& vn,
           int vn_index);

  // Move vector element to another vector element.
  void mov(const VRegister& vd, int vd_index, const VRegister& vn,
           int vn_index);

  // Signed move vector element to general-purpose register.
  void smov(const Register& rd, const VRegister& vn, int vn_index);

  // One-element structure load to one register.
  void ld1(const VRegister& vt, const MemOperand& src);

  // One-element structure load to two registers.
  void ld1(const VRegister& vt, const VRegister& vt2, const MemOperand& src);

  // One-element structure load to three registers.
  void ld1(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const MemOperand& src);

  // One-element structure load to four registers.
  void ld1(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const VRegister& vt4, const MemOperand& src);

  // One-element single structure load to one lane.
  void ld1(const VRegister& vt, int lane, const MemOperand& src);

  // One-element single structure load to all lanes.
  void ld1r(const VRegister& vt, const MemOperand& src);

  // Two-element structure load.
  void ld2(const VRegister& vt, const VRegister& vt2, const MemOperand& src);

  // Two-element single structure load to one lane.
  void ld2(const VRegister& vt, const VRegister& vt2, int lane,
           const MemOperand& src);

  // Two-element single structure load to all lanes.
  void ld2r(const VRegister& vt, const VRegister& vt2, const MemOperand& src);

  // Three-element structure load.
  void ld3(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const MemOperand& src);

  // Three-element single structure load to one lane.
  void ld3(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           int lane, const MemOperand& src);

  // Three-element single structure load to all lanes.
  void ld3r(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
            const MemOperand& src);

  // Four-element structure load.
  void ld4(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const VRegister& vt4, const MemOperand& src);

  // Four-element single structure load to one lane.
  void ld4(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
           const VRegister& vt4, int lane, const MemOperand& src);

  // Four-element single structure load to all lanes.
  void ld4r(const VRegister& vt, const VRegister& vt2, const VRegister& vt3,
            const VRegister& vt4, const MemOperand& src);

  // Count leading sign bits.
  void cls(const VRegister& vd, const VRegister& vn);

  // Count leading zero bits (vector).
  void clz(const VRegister& vd, const VRegister& vn);

  // Population count per byte.
  void cnt(const VRegister& vd, const VRegister& vn);

  // Reverse bit order.
  void rbit(const VRegister& vd, const VRegister& vn);

  // Reverse elements in 16-bit halfwords.
  void rev16(const VRegister& vd, const VRegister& vn);

  // Reverse elements in 32-bit words.
  void rev32(const VRegister& vd, const VRegister& vn);

  // Reverse elements in 64-bit doublewords.
  void rev64(const VRegister& vd, const VRegister& vn);

  // Unsigned reciprocal square root estimate.
  void ursqrte(const VRegister& vd, const VRegister& vn);

  // Unsigned reciprocal estimate.
  void urecpe(const VRegister& vd, const VRegister& vn);

  // Signed pairwise long add and accumulate.
  void sadalp(const VRegister& vd, const VRegister& vn);

  // Signed pairwise long add.
  void saddlp(const VRegister& vd, const VRegister& vn);

  // Unsigned pairwise long add.
  void uaddlp(const VRegister& vd, const VRegister& vn);

  // Unsigned pairwise long add and accumulate.
  void uadalp(const VRegister& vd, const VRegister& vn);

  // Shift left by immediate.
  void shl(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating shift left by immediate.
  void sqshl(const VRegister& vd, const VRegister& vn, int shift);

  // Signed saturating shift left unsigned by immediate.
  void sqshlu(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned saturating shift left by immediate.
  void uqshl(const VRegister& vd, const VRegister& vn, int shift);

  // Signed shift left long by immediate.
  void sshll(const VRegister& vd, const VRegister& vn, int shift);

  // Signed shift left long by immediate (second part).
  void sshll2(const VRegister& vd, const VRegister& vn, int shift);

  // Signed extend long.
  void sxtl(const VRegister& vd, const VRegister& vn);

  // Signed extend long (second part).
  void sxtl2(const VRegister& vd, const VRegister& vn);

  // Unsigned shift left long by immediate.
  void ushll(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned shift left long by immediate (second part).
  void ushll2(const VRegister& vd, const VRegister& vn, int shift);

  // Shift left long by element size.
  void shll(const VRegister& vd, const VRegister& vn, int shift);

  // Shift left long by element size (second part).
  void shll2(const VRegister& vd, const VRegister& vn, int shift);

  // Unsigned extend long.
  void uxtl(const VRegister& vd, const VRegister& vn);

  // Unsigned extend long (second part).
  void uxtl2(const VRegister& vd, const VRegister& vn);

  // Signed rounding halving add.
  void srhadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned halving sub.
  void uhsub(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed halving sub.
  void shsub(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned saturating add.
  void uqadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating add.
  void sqadd(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Unsigned saturating subtract.
  void uqsub(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Signed saturating subtract.
  void sqsub(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Add pairwise.
  void addp(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Add pair of elements scalar.
  void addp(const VRegister& vd, const VRegister& vn);

  // Multiply-add to accumulator.
  void mla(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Multiply-subtract to accumulator.
  void mls(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Multiply.
  void mul(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Table lookup from one register.
  void tbl(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Table lookup from two registers.
  void tbl(const VRegister& vd, const VRegister& vn, const VRegister& vn2,
           const VRegister& vm);

  // Table lookup from three registers.
  void tbl(const VRegister& vd, const VRegister& vn, const VRegister& vn2,
           const VRegister& vn3, const VRegister& vm);

  // Table lookup from four registers.
  void tbl(const VRegister& vd, const VRegister& vn, const VRegister& vn2,
           const VRegister& vn3, const VRegister& vn4, const VRegister& vm);

  // Table lookup extension from one register.
  void tbx(const VRegister& vd, const VRegister& vn, const VRegister& vm);

  // Table lookup extension from two registers.
  void tbx(const VRegister& vd, const VRegister& vn, const VRegister& vn2,
           const VRegister& vm);

  // Table lookup extension from three registers.
  void tbx(const VRegister& vd, const VRegister& vn, const VRegister& vn2,
           const VRegister& vn3, const VRegister& vm);

  // Table lookup extension from four registers.
  void tbx(const VRegister& vd, const VRegister& vn, const VRegister& vn2,
           const VRegister& vn3, const VRegister& vn4, const VRegister& vm);

  // Instruction functions used only for test, debug, and patching.
  // Emit raw instructions in the instruction stream.
  void dci(Instr raw_inst) { Emit(raw_inst); }

  // Emit 8 bits of data in the instruction stream.
  void dc8(uint8_t data) { EmitData(&data, sizeof(data)); }

  // Emit 32 bits of data in the instruction stream.
  void dc32(uint32_t data) { EmitData(&data, sizeof(data)); }

  // Emit 64 bits of data in the instruction stream.
  void dc64(uint64_t data) { EmitData(&data, sizeof(data)); }

  // Emit an address in the instruction stream.
  void dcptr(Label* label);

  // Copy a string into the instruction stream, including the terminating
  // nullptr character. The instruction pointer (pc_) is then aligned correctly
  // for subsequent instructions.
  void EmitStringData(const char* string);

  // Pseudo-instructions ------------------------------------------------------

  // Parameters are described in arm64/instructions-arm64.h.
  void debug(const char* message, uint32_t code, Instr params = BREAK);

  // Required by V8.
  void dd(uint32_t data) { dc32(data); }
  void db(uint8_t data) { dc8(data); }
  void dq(uint64_t data) { dc64(data); }
  void dp(uintptr_t data) { dc64(data); }

  // Code generation helpers --------------------------------------------------

  bool IsConstPoolEmpty() const { return constpool_.IsEmpty(); }

  Instruction* pc() const { return Instruction::Cast(pc_); }

  Instruction* InstructionAt(ptrdiff_t offset) const {
    return reinterpret_cast<Instruction*>(buffer_ + offset);
  }

  ptrdiff_t InstructionOffset(Instruction* instr) const {
    return reinterpret_cast<byte*>(instr) - buffer_;
  }

  static const char* GetSpecialRegisterName(int code) {
    return (code == kSPRegInternalCode) ? "sp" : "UNKNOWN";
  }

  // Register encoding.
  static Instr Rd(CPURegister rd) {
    DCHECK_NE(rd.code(), kSPRegInternalCode);
    return rd.code() << Rd_offset;
  }

  static Instr Rn(CPURegister rn) {
    DCHECK_NE(rn.code(), kSPRegInternalCode);
    return rn.code() << Rn_offset;
  }

  static Instr Rm(CPURegister rm) {
    DCHECK_NE(rm.code(), kSPRegInternalCode);
    return rm.code() << Rm_offset;
  }

  static Instr RmNot31(CPURegister rm) {
    DCHECK_NE(rm.code(), kSPRegInternalCode);
    DCHECK(!rm.IsZero());
    return Rm(rm);
  }

  static Instr Ra(CPURegister ra) {
    DCHECK_NE(ra.code(), kSPRegInternalCode);
    return ra.code() << Ra_offset;
  }

  static Instr Rt(CPURegister rt) {
    DCHECK_NE(rt.code(), kSPRegInternalCode);
    return rt.code() << Rt_offset;
  }

  static Instr Rt2(CPURegister rt2) {
    DCHECK_NE(rt2.code(), kSPRegInternalCode);
    return rt2.code() << Rt2_offset;
  }

  static Instr Rs(CPURegister rs) {
    DCHECK_NE(rs.code(), kSPRegInternalCode);
    return rs.code() << Rs_offset;
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
  inline static Instr ImmAddSub(int imm);
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
  inline static Instr ImmLSPair(int imm7, unsigned size);
  inline static Instr ImmShiftLS(unsigned shift_amount);
  inline static Instr ImmException(int imm16);
  inline static Instr ImmSystemRegister(int imm15);
  inline static Instr ImmHint(int imm7);
  inline static Instr ImmBarrierDomain(int imm2);
  inline static Instr ImmBarrierType(int imm2);
  inline static unsigned CalcLSDataSize(LoadStoreOp op);

  // Instruction bits for vector format in data processing operations.
  static Instr VFormat(VRegister vd) {
    if (vd.Is64Bits()) {
      switch (vd.LaneCount()) {
        case 2:
          return NEON_2S;
        case 4:
          return NEON_4H;
        case 8:
          return NEON_8B;
        default:
          UNREACHABLE();
      }
    } else {
      DCHECK(vd.Is128Bits());
      switch (vd.LaneCount()) {
        case 2:
          return NEON_2D;
        case 4:
          return NEON_4S;
        case 8:
          return NEON_8H;
        case 16:
          return NEON_16B;
        default:
          UNREACHABLE();
      }
    }
  }

  // Instruction bits for vector format in floating point data processing
  // operations.
  static Instr FPFormat(VRegister vd) {
    if (vd.LaneCount() == 1) {
      // Floating point scalar formats.
      DCHECK(vd.Is32Bits() || vd.Is64Bits());
      return vd.Is64Bits() ? FP64 : FP32;
    }

    // Two lane floating point vector formats.
    if (vd.LaneCount() == 2) {
      DCHECK(vd.Is64Bits() || vd.Is128Bits());
      return vd.Is128Bits() ? NEON_FP_2D : NEON_FP_2S;
    }

    // Four lane floating point vector format.
    DCHECK((vd.LaneCount() == 4) && vd.Is128Bits());
    return NEON_FP_4S;
  }

  // Instruction bits for vector format in load and store operations.
  static Instr LSVFormat(VRegister vd) {
    if (vd.Is64Bits()) {
      switch (vd.LaneCount()) {
        case 1:
          return LS_NEON_1D;
        case 2:
          return LS_NEON_2S;
        case 4:
          return LS_NEON_4H;
        case 8:
          return LS_NEON_8B;
        default:
          UNREACHABLE();
      }
    } else {
      DCHECK(vd.Is128Bits());
      switch (vd.LaneCount()) {
        case 2:
          return LS_NEON_2D;
        case 4:
          return LS_NEON_4S;
        case 8:
          return LS_NEON_8H;
        case 16:
          return LS_NEON_16B;
        default:
          UNREACHABLE();
      }
    }
  }

  // Instruction bits for scalar format in data processing operations.
  static Instr SFormat(VRegister vd) {
    DCHECK(vd.IsScalar());
    switch (vd.SizeInBytes()) {
      case 1:
        return NEON_B;
      case 2:
        return NEON_H;
      case 4:
        return NEON_S;
      case 8:
        return NEON_D;
      default:
        UNREACHABLE();
    }
  }

  static Instr ImmNEONHLM(int index, int num_bits) {
    int h, l, m;
    if (num_bits == 3) {
      DCHECK(is_uint3(index));
      h = (index >> 2) & 1;
      l = (index >> 1) & 1;
      m = (index >> 0) & 1;
    } else if (num_bits == 2) {
      DCHECK(is_uint2(index));
      h = (index >> 1) & 1;
      l = (index >> 0) & 1;
      m = 0;
    } else {
      DCHECK(is_uint1(index) && (num_bits == 1));
      h = (index >> 0) & 1;
      l = 0;
      m = 0;
    }
    return (h << NEONH_offset) | (l << NEONL_offset) | (m << NEONM_offset);
  }

  static Instr ImmNEONExt(int imm4) {
    DCHECK(is_uint4(imm4));
    return imm4 << ImmNEONExt_offset;
  }

  static Instr ImmNEON5(Instr format, int index) {
    DCHECK(is_uint4(index));
    int s = LaneSizeInBytesLog2FromFormat(static_cast<VectorFormat>(format));
    int imm5 = (index << (s + 1)) | (1 << s);
    return imm5 << ImmNEON5_offset;
  }

  static Instr ImmNEON4(Instr format, int index) {
    DCHECK(is_uint4(index));
    int s = LaneSizeInBytesLog2FromFormat(static_cast<VectorFormat>(format));
    int imm4 = index << s;
    return imm4 << ImmNEON4_offset;
  }

  static Instr ImmNEONabcdefgh(int imm8) {
    DCHECK(is_uint8(imm8));
    Instr instr;
    instr = ((imm8 >> 5) & 7) << ImmNEONabc_offset;
    instr |= (imm8 & 0x1f) << ImmNEONdefgh_offset;
    return instr;
  }

  static Instr NEONCmode(int cmode) {
    DCHECK(is_uint4(cmode));
    return cmode << NEONCmode_offset;
  }

  static Instr NEONModImmOp(int op) {
    DCHECK(is_uint1(op));
    return op << NEONModImmOp_offset;
  }

  static bool IsImmLSUnscaled(int64_t offset);
  static bool IsImmLSScaled(int64_t offset, unsigned size);
  static bool IsImmLLiteral(int64_t offset);

  // Move immediates encoding.
  inline static Instr ImmMoveWide(int imm);
  inline static Instr ShiftMoveWide(int shift);

  // FP Immediates.
  static Instr ImmFP(double imm);
  static Instr ImmNEONFP(double imm);
  inline static Instr FPScale(unsigned scale);

  // FP register type.
  inline static Instr FPType(VRegister fd);

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

  void PatchConstantPoolAccessInstruction(int pc_offset, int offset,
                                          ConstantPoolEntry::Access access,
                                          ConstantPoolEntry::Type type) {
    // No embedded constant pool support.
    UNREACHABLE();
  }

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
  static constexpr int kMaxVeneerCodeSize = 1 * kInstrSize;

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
  void LoadStoreStruct(const VRegister& vt, const MemOperand& addr,
                       NEONLoadStoreMultiStructOp op);
  void LoadStoreStruct1(const VRegister& vt, int reg_count,
                        const MemOperand& addr);
  void LoadStoreStructSingle(const VRegister& vt, uint32_t lane,
                             const MemOperand& addr,
                             NEONLoadStoreSingleStructOp op);
  void LoadStoreStructSingleAllLanes(const VRegister& vt,
                                     const MemOperand& addr,
                                     NEONLoadStoreSingleStructOp op);
  void LoadStoreStructVerify(const VRegister& vt, const MemOperand& addr,
                             Instr op);

  static bool IsImmLSPair(int64_t offset, unsigned size);

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
  static inline LoadLiteralOp LoadLiteralOpFor(const CPURegister& rt);

  // Remove the specified branch from the unbound label link chain.
  // If available, a veneer for this label can be used for other branches in the
  // chain if the link chain cannot be fixed up without this branch.
  void RemoveBranchFromLabelLinkChain(Instruction* branch, Label* label,
                                      Instruction* label_veneer = nullptr);

 private:
  static uint32_t FPToImm8(double imm);

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
  void FPDataProcessing1Source(const VRegister& fd, const VRegister& fn,
                               FPDataProcessing1SourceOp op);
  void FPDataProcessing2Source(const VRegister& fd, const VRegister& fn,
                               const VRegister& fm,
                               FPDataProcessing2SourceOp op);
  void FPDataProcessing3Source(const VRegister& fd, const VRegister& fn,
                               const VRegister& fm, const VRegister& fa,
                               FPDataProcessing3SourceOp op);
  void NEONAcrossLanesL(const VRegister& vd, const VRegister& vn,
                        NEONAcrossLanesOp op);
  void NEONAcrossLanes(const VRegister& vd, const VRegister& vn,
                       NEONAcrossLanesOp op);
  void NEONModifiedImmShiftLsl(const VRegister& vd, const int imm8,
                               const int left_shift,
                               NEONModifiedImmediateOp op);
  void NEONModifiedImmShiftMsl(const VRegister& vd, const int imm8,
                               const int shift_amount,
                               NEONModifiedImmediateOp op);
  void NEON3Same(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                 NEON3SameOp vop);
  void NEONFP3Same(const VRegister& vd, const VRegister& vn,
                   const VRegister& vm, Instr op);
  void NEON3DifferentL(const VRegister& vd, const VRegister& vn,
                       const VRegister& vm, NEON3DifferentOp vop);
  void NEON3DifferentW(const VRegister& vd, const VRegister& vn,
                       const VRegister& vm, NEON3DifferentOp vop);
  void NEON3DifferentHN(const VRegister& vd, const VRegister& vn,
                        const VRegister& vm, NEON3DifferentOp vop);
  void NEONFP2RegMisc(const VRegister& vd, const VRegister& vn,
                      NEON2RegMiscOp vop, double value = 0.0);
  void NEON2RegMisc(const VRegister& vd, const VRegister& vn,
                    NEON2RegMiscOp vop, int value = 0);
  void NEONFP2RegMisc(const VRegister& vd, const VRegister& vn, Instr op);
  void NEONAddlp(const VRegister& vd, const VRegister& vn, NEON2RegMiscOp op);
  void NEONPerm(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                NEONPermOp op);
  void NEONFPByElement(const VRegister& vd, const VRegister& vn,
                       const VRegister& vm, int vm_index,
                       NEONByIndexedElementOp op);
  void NEONByElement(const VRegister& vd, const VRegister& vn,
                     const VRegister& vm, int vm_index,
                     NEONByIndexedElementOp op);
  void NEONByElementL(const VRegister& vd, const VRegister& vn,
                      const VRegister& vm, int vm_index,
                      NEONByIndexedElementOp op);
  void NEONShiftImmediate(const VRegister& vd, const VRegister& vn,
                          NEONShiftImmediateOp op, int immh_immb);
  void NEONShiftLeftImmediate(const VRegister& vd, const VRegister& vn,
                              int shift, NEONShiftImmediateOp op);
  void NEONShiftRightImmediate(const VRegister& vd, const VRegister& vn,
                               int shift, NEONShiftImmediateOp op);
  void NEONShiftImmediateL(const VRegister& vd, const VRegister& vn, int shift,
                           NEONShiftImmediateOp op);
  void NEONShiftImmediateN(const VRegister& vd, const VRegister& vn, int shift,
                           NEONShiftImmediateOp op);
  void NEONXtn(const VRegister& vd, const VRegister& vn, NEON2RegMiscOp vop);
  void NEONTable(const VRegister& vd, const VRegister& vn, const VRegister& vm,
                 NEONTableOp op);

  Instr LoadStoreStructAddrModeField(const MemOperand& addr);

  // Label helpers.

  // Return an offset for a label-referencing instruction, typically a branch.
  int LinkAndGetByteOffsetTo(Label* label);

  // This is the same as LinkAndGetByteOffsetTo, but return an offset
  // suitable for fields that take instruction offsets.
  inline int LinkAndGetInstructionOffsetTo(Label* label);

  static constexpr int kStartOfLabelLinkChain = 0;

  // Verify that a label's link chain is intact.
  void CheckLabelLinkChain(Label const * label);

  // Postpone the generation of the constant pool for the specified number of
  // instructions.
  void BlockConstPoolFor(int instructions);

  // Set how far from current pc the next constant pool check will be.
  void SetNextConstPoolCheckIn(int instructions) {
    next_constant_pool_check_ = pc_offset() + instructions * kInstrSize;
  }

  // Emit the instruction at pc_.
  void Emit(Instr instruction) {
    STATIC_ASSERT(sizeof(*pc_) == 1);
    STATIC_ASSERT(sizeof(instruction) == kInstrSize);
    DCHECK((pc_ + sizeof(instruction)) <= (buffer_ + buffer_size_));

    memcpy(pc_, &instruction, sizeof(instruction));
    pc_ += sizeof(instruction);
    CheckBuffer();
  }

  // Emit data inline in the instruction stream.
  void EmitData(void const * data, unsigned size) {
    DCHECK_EQ(sizeof(*pc_), 1);
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
  static constexpr int kCheckConstPoolInterval = 128;

  // Distance to first use after a which a pool will be emitted. Pool entries
  // are accessed with pc relative load therefore this cannot be more than
  // 1 * MB. Since constant pool emission checks are interval based this value
  // is an approximation.
  static constexpr int kApproxMaxDistToConstPool = 64 * KB;

  // Number of pool entries after which a pool will be emitted. Since constant
  // pool emission checks are interval based this value is an approximation.
  static constexpr int kApproxMaxPoolEntryCount = 512;

  // Emission of the constant pool may be blocked in some code sequences.
  int const_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_const_pool_before_;  // Block emission before this pc offset.

  // Emission of the veneer pools may be blocked in some code sequences.
  int veneer_pool_blocked_nesting_;  // Block emission if this is not zero.

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static constexpr int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // Internal reference positions, required for (potential) patching in
  // GrowBuffer(); contains only those internal references whose labels
  // are already bound.
  std::deque<int> internal_reference_positions_;

  // Relocation info records are also used during code generation as temporary
  // containers for constants and code target addresses until they are emitted
  // to the constant pool. These pending relocation info records are temporarily
  // stored in a separate buffer until a constant pool is emitted.
  // If every instruction in a long sequence is accessing the pool, we need one
  // pending relocation entry per instruction.

  // The pending constant pool.
  ConstPool constpool_;

 protected:
  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries, and debug strings encoded in the instruction
  // stream.
  static constexpr int kGap = 128;

 public:
#ifdef DEBUG
  // Functions used for testing.
  int GetConstantPoolEntriesSizeForTesting() const {
    // Do not include branch over the pool.
    return constpool_.EntryCount() * kPointerSize;
  }

  static constexpr int GetCheckConstPoolIntervalForTesting() {
    return kCheckConstPoolInterval;
  }

  static constexpr int GetApproxMaxDistToConstPoolForTesting() {
    return kApproxMaxDistToConstPool;
  }
#endif

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
  static constexpr int kVeneerDistanceMargin = 1 * KB;
  // The factor of 2 is a finger in the air guess. With a default margin of
  // 1KB, that leaves us an addional 256 instructions to avoid generating a
  // protective branch.
  static constexpr int kVeneerNoProtectionFactor = 2;
  static constexpr int kVeneerDistanceCheckMargin =
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
  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512 * MB;

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

  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

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
  // Note that the instruction cache will not be flushed.
  PatchingAssembler(const AssemblerOptions& options, byte* start,
                    unsigned count)
      : Assembler(options, start, count * kInstrSize + kGap) {
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
  }

  // See definition of PatchAdrFar() for details.
  static constexpr int kAdrFarPatchableNNops = 2;
  static constexpr int kAdrFarPatchableNInstrs = kAdrFarPatchableNNops + 2;
  void PatchAdrFar(int64_t target_offset);
  void PatchSubSp(uint32_t immediate);
};

class EnsureSpace {
 public:
  explicit EnsureSpace(Assembler* assembler) {
    assembler->CheckBufferSpace();
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_ASSEMBLER_ARM64_H_

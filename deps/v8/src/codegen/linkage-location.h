// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_LINKAGE_LOCATION_H_
#define V8_CODEGEN_LINKAGE_LOCATION_H_

#include "src/base/bit-field.h"
#include "src/codegen/machine-type.h"
#include "src/execution/frame-constants.h"

#if !defined(__clang__) && defined(_M_ARM64)
// _M_ARM64 is an MSVC-specific macro that clang-cl emulates.
#define NO_INLINE_FOR_ARM64_MSVC __declspec(noinline)
#else
#define NO_INLINE_FOR_ARM64_MSVC
#endif

namespace v8 {
namespace internal {
template <typename T>
class Signature;

// Describes the location for a parameter or a return value to a call.
class LinkageLocation {
 public:
  bool operator==(const LinkageLocation& other) const {
    return bit_field_ == other.bit_field_ &&
           machine_type_ == other.machine_type_;
  }

  bool operator!=(const LinkageLocation& other) const {
    return !(*this == other);
  }

  static bool IsSameLocation(const LinkageLocation& a,
                             const LinkageLocation& b) {
    // Different MachineTypes may end up at the same physical location. With the
    // sub-type check we make sure that types like {AnyTagged} and
    // {TaggedPointer} which would end up with the same physical location are
    // considered equal here.
    return (a.bit_field_ == b.bit_field_) &&
           (IsSubtype(a.machine_type_.representation(),
                      b.machine_type_.representation()) ||
            IsSubtype(b.machine_type_.representation(),
                      a.machine_type_.representation()));
  }

  static LinkageLocation ForNullRegister(
      int32_t reg, MachineType type = MachineType::None()) {
    return LinkageLocation(REGISTER, reg, type);
  }

  static LinkageLocation ForAnyRegister(
      MachineType type = MachineType::None()) {
    return LinkageLocation(REGISTER, ANY_REGISTER, type);
  }

  static LinkageLocation ForRegister(int32_t reg,
                                     MachineType type = MachineType::None()) {
    DCHECK_LE(0, reg);
    return LinkageLocation(REGISTER, reg, type);
  }

  static LinkageLocation ForCallerFrameSlot(int32_t slot, MachineType type) {
    DCHECK_GT(0, slot);
    return LinkageLocation(STACK_SLOT, slot, type);
  }

  static LinkageLocation ForCalleeFrameSlot(int32_t slot, MachineType type) {
    // TODO(titzer): bailout instead of crashing here.
    DCHECK(slot >= 0 && slot < LinkageLocation::MAX_STACK_SLOT);
    return LinkageLocation(STACK_SLOT, slot, type);
  }

  // TODO(ahaas): Extract these TurboFan-specific functions from the
  // LinkageLocation.
  static LinkageLocation ForSavedCallerReturnAddress() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kCallerPCOffset) /
                                  kSystemPointerSize,
                              MachineType::Pointer());
  }

  static LinkageLocation ForSavedCallerFramePtr() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kCallerFPOffset) /
                                  kSystemPointerSize,
                              MachineType::Pointer());
  }

  static LinkageLocation ForSavedCallerConstantPool() {
    DCHECK(V8_EMBEDDED_CONSTANT_POOL_BOOL);
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kConstantPoolOffset) /
                                  kSystemPointerSize,
                              MachineType::AnyTagged());
  }

  static LinkageLocation ForSavedCallerFunction() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kFunctionOffset) /
                                  kSystemPointerSize,
                              MachineType::AnyTagged());
  }

  static LinkageLocation ConvertToTailCallerLocation(
      LinkageLocation caller_location, int stack_param_delta) {
    if (!caller_location.IsRegister()) {
      return LinkageLocation(STACK_SLOT,
                             caller_location.GetLocation() + stack_param_delta,
                             caller_location.GetType());
    }
    return caller_location;
  }

  MachineType GetType() const { return machine_type_; }

  int GetSizeInPointers() const {
    return ElementSizeInPointers(GetType().representation());
  }

  int32_t GetLocation() const {
    // We can't use LocationField::decode here because it doesn't work for
    // negative values!
    return static_cast<int32_t>(bit_field_ & LocationField::kMask) >>
           LocationField::kShift;
  }

  bool IsNullRegister() const {
    return IsRegister() && GetLocation() < ANY_REGISTER;
  }
  NO_INLINE_FOR_ARM64_MSVC bool IsRegister() const {
    return TypeField::decode(bit_field_) == REGISTER;
  }
  bool IsAnyRegister() const {
    return IsRegister() && GetLocation() == ANY_REGISTER;
  }
  bool IsCallerFrameSlot() const { return !IsRegister() && GetLocation() < 0; }
  bool IsCalleeFrameSlot() const { return !IsRegister() && GetLocation() >= 0; }

  int32_t AsRegister() const {
    DCHECK(IsRegister());
    return GetLocation();
  }
  int32_t AsCallerFrameSlot() const {
    DCHECK(IsCallerFrameSlot());
    return GetLocation();
  }
  int32_t AsCalleeFrameSlot() const {
    DCHECK(IsCalleeFrameSlot());
    return GetLocation();
  }

 private:
  enum LocationType { REGISTER, STACK_SLOT };

  using TypeField = base::BitField<LocationType, 0, 1>;
  using LocationField = TypeField::Next<int32_t, 31>;

  static constexpr int32_t ANY_REGISTER = -1;
  static constexpr int32_t MAX_STACK_SLOT = 32767;

  LinkageLocation(LocationType type, int32_t location,
                  MachineType machine_type) {
    bit_field_ = TypeField::encode(type) |
                 // {location} can be -1 (ANY_REGISTER).
                 ((static_cast<uint32_t>(location) << LocationField::kShift) &
                  LocationField::kMask);
    machine_type_ = machine_type;
  }

  int32_t bit_field_;
  MachineType machine_type_;
};

using LocationSignature = Signature<LinkageLocation>;

}  // namespace internal
}  // namespace v8
#undef NO_INLINE_FOR_ARM64_MSVC

#endif  // V8_CODEGEN_LINKAGE_LOCATION_H_

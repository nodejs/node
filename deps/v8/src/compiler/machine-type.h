// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_TYPE_H_
#define V8_COMPILER_MACHINE_TYPE_H_

#include <iosfwd>

#include "src/base/bits.h"
#include "src/globals.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// Machine-level types and representations.
// TODO(titzer): Use the real type system instead of MachineType.
enum MachineType {
  // Representations.
  kRepBit = 1 << 0,
  kRepWord8 = 1 << 1,
  kRepWord16 = 1 << 2,
  kRepWord32 = 1 << 3,
  kRepWord64 = 1 << 4,
  kRepFloat32 = 1 << 5,
  kRepFloat64 = 1 << 6,
  kRepTagged = 1 << 7,

  // Types.
  kTypeBool = 1 << 8,
  kTypeInt32 = 1 << 9,
  kTypeUint32 = 1 << 10,
  kTypeInt64 = 1 << 11,
  kTypeUint64 = 1 << 12,
  kTypeNumber = 1 << 13,
  kTypeAny = 1 << 14,

  // Machine types.
  kMachNone = 0,
  kMachBool = kRepBit | kTypeBool,
  kMachFloat32 = kRepFloat32 | kTypeNumber,
  kMachFloat64 = kRepFloat64 | kTypeNumber,
  kMachInt8 = kRepWord8 | kTypeInt32,
  kMachUint8 = kRepWord8 | kTypeUint32,
  kMachInt16 = kRepWord16 | kTypeInt32,
  kMachUint16 = kRepWord16 | kTypeUint32,
  kMachInt32 = kRepWord32 | kTypeInt32,
  kMachUint32 = kRepWord32 | kTypeUint32,
  kMachInt64 = kRepWord64 | kTypeInt64,
  kMachUint64 = kRepWord64 | kTypeUint64,
  kMachIntPtr = (kPointerSize == 4) ? kMachInt32 : kMachInt64,
  kMachUintPtr = (kPointerSize == 4) ? kMachUint32 : kMachUint64,
  kMachPtr = (kPointerSize == 4) ? kRepWord32 : kRepWord64,
  kMachAnyTagged = kRepTagged | kTypeAny
};

std::ostream& operator<<(std::ostream& os, const MachineType& type);

typedef uint16_t MachineTypeUnion;

// Globally useful machine types and constants.
const MachineTypeUnion kRepMask = kRepBit | kRepWord8 | kRepWord16 |
                                  kRepWord32 | kRepWord64 | kRepFloat32 |
                                  kRepFloat64 | kRepTagged;
const MachineTypeUnion kTypeMask = kTypeBool | kTypeInt32 | kTypeUint32 |
                                   kTypeInt64 | kTypeUint64 | kTypeNumber |
                                   kTypeAny;

// Gets only the type of the given type.
inline MachineType TypeOf(MachineType machine_type) {
  int result = machine_type & kTypeMask;
  return static_cast<MachineType>(result);
}

// Gets only the representation of the given type.
inline MachineType RepresentationOf(MachineType machine_type) {
  int result = machine_type & kRepMask;
  CHECK(base::bits::IsPowerOfTwo32(result));
  return static_cast<MachineType>(result);
}

// Gets the log2 of the element size in bytes of the machine type.
inline int ElementSizeLog2Of(MachineType machine_type) {
  switch (RepresentationOf(machine_type)) {
    case kRepBit:
    case kRepWord8:
      return 0;
    case kRepWord16:
      return 1;
    case kRepWord32:
    case kRepFloat32:
      return 2;
    case kRepWord64:
    case kRepFloat64:
      return 3;
    case kRepTagged:
      return kPointerSizeLog2;
    default:
      break;
  }
  UNREACHABLE();
  return -1;
}

// Gets the element size in bytes of the machine type.
inline int ElementSizeOf(MachineType machine_type) {
  const int shift = ElementSizeLog2Of(machine_type);
  DCHECK_NE(-1, shift);
  return 1 << shift;
}

// Describes the inputs and outputs of a function or call.
template <typename T>
class Signature : public ZoneObject {
 public:
  Signature(size_t return_count, size_t parameter_count, T* reps)
      : return_count_(return_count),
        parameter_count_(parameter_count),
        reps_(reps) {}

  size_t return_count() const { return return_count_; }
  size_t parameter_count() const { return parameter_count_; }

  T GetParam(size_t index) const {
    DCHECK(index < parameter_count_);
    return reps_[return_count_ + index];
  }

  T GetReturn(size_t index = 0) const {
    DCHECK(index < return_count_);
    return reps_[index];
  }

  // For incrementally building signatures.
  class Builder {
   public:
    Builder(Zone* zone, size_t return_count, size_t parameter_count)
        : return_count_(return_count),
          parameter_count_(parameter_count),
          zone_(zone),
          rcursor_(0),
          pcursor_(0),
          buffer_(zone->NewArray<T>(
              static_cast<int>(return_count + parameter_count))) {}

    const size_t return_count_;
    const size_t parameter_count_;

    void AddReturn(T val) {
      DCHECK(rcursor_ < return_count_);
      buffer_[rcursor_++] = val;
    }
    void AddParam(T val) {
      DCHECK(pcursor_ < parameter_count_);
      buffer_[return_count_ + pcursor_++] = val;
    }
    Signature<T>* Build() {
      DCHECK(rcursor_ == return_count_);
      DCHECK(pcursor_ == parameter_count_);
      return new (zone_) Signature<T>(return_count_, parameter_count_, buffer_);
    }

   private:
    Zone* zone_;
    size_t rcursor_;
    size_t pcursor_;
    T* buffer_;
  };

 protected:
  size_t return_count_;
  size_t parameter_count_;
  T* reps_;
};

typedef Signature<MachineType> MachineSignature;
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_TYPE_H_

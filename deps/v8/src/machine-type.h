// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MACHINE_TYPE_H_
#define V8_MACHINE_TYPE_H_

#include <iosfwd>

#include "src/base/bits.h"
#include "src/globals.h"
#include "src/signature.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

enum class MachineRepresentation : uint8_t {
  kNone,
  kBit,
  kWord8,
  kWord16,
  kWord32,
  kWord64,
  kFloat32,
  kFloat64,
  kSimd128,
  kTagged
};

enum class MachineSemantic : uint8_t {
  kNone,
  kBool,
  kInt32,
  kUint32,
  kInt64,
  kUint64,
  kNumber,
  kAny
};

class MachineType {
 public:
  MachineType()
      : representation_(MachineRepresentation::kNone),
        semantic_(MachineSemantic::kNone) {}
  MachineType(MachineRepresentation representation, MachineSemantic semantic)
      : representation_(representation), semantic_(semantic) {}

  bool operator==(MachineType other) const {
    return representation() == other.representation() &&
           semantic() == other.semantic();
  }

  bool operator!=(MachineType other) const { return !(*this == other); }


  MachineRepresentation representation() const { return representation_; }
  MachineSemantic semantic() const { return semantic_; }

  bool IsSigned() {
    return semantic() == MachineSemantic::kInt32 ||
           semantic() == MachineSemantic::kInt64;
  }
  bool IsUnsigned() {
    return semantic() == MachineSemantic::kUint32 ||
           semantic() == MachineSemantic::kUint64;
  }

  static MachineRepresentation PointerRepresentation() {
    return (kPointerSize == 4) ? MachineRepresentation::kWord32
                               : MachineRepresentation::kWord64;
  }
  static MachineType Pointer() {
    return MachineType(PointerRepresentation(), MachineSemantic::kNone);
  }
  static MachineType IntPtr() {
    return (kPointerSize == 4) ? Int32() : Int64();
  }
  static MachineType Float32() {
    return MachineType(MachineRepresentation::kFloat32,
                       MachineSemantic::kNumber);
  }
  static MachineType Float64() {
    return MachineType(MachineRepresentation::kFloat64,
                       MachineSemantic::kNumber);
  }
  static MachineType Simd128() {
    return MachineType(MachineRepresentation::kSimd128, MachineSemantic::kNone);
  }
  static MachineType Int8() {
    return MachineType(MachineRepresentation::kWord8, MachineSemantic::kInt32);
  }
  static MachineType Uint8() {
    return MachineType(MachineRepresentation::kWord8, MachineSemantic::kUint32);
  }
  static MachineType Int16() {
    return MachineType(MachineRepresentation::kWord16, MachineSemantic::kInt32);
  }
  static MachineType Uint16() {
    return MachineType(MachineRepresentation::kWord16,
                       MachineSemantic::kUint32);
  }
  static MachineType Int32() {
    return MachineType(MachineRepresentation::kWord32, MachineSemantic::kInt32);
  }
  static MachineType Uint32() {
    return MachineType(MachineRepresentation::kWord32,
                       MachineSemantic::kUint32);
  }
  static MachineType Int64() {
    return MachineType(MachineRepresentation::kWord64, MachineSemantic::kInt64);
  }
  static MachineType Uint64() {
    return MachineType(MachineRepresentation::kWord64,
                       MachineSemantic::kUint64);
  }
  static MachineType AnyTagged() {
    return MachineType(MachineRepresentation::kTagged, MachineSemantic::kAny);
  }
  static MachineType Bool() {
    return MachineType(MachineRepresentation::kBit, MachineSemantic::kBool);
  }
  static MachineType TaggedBool() {
    return MachineType(MachineRepresentation::kTagged, MachineSemantic::kBool);
  }
  static MachineType None() {
    return MachineType(MachineRepresentation::kNone, MachineSemantic::kNone);
  }

  // These naked representations should eventually go away.
  static MachineType RepWord8() {
    return MachineType(MachineRepresentation::kWord8, MachineSemantic::kNone);
  }
  static MachineType RepWord16() {
    return MachineType(MachineRepresentation::kWord16, MachineSemantic::kNone);
  }
  static MachineType RepWord32() {
    return MachineType(MachineRepresentation::kWord32, MachineSemantic::kNone);
  }
  static MachineType RepWord64() {
    return MachineType(MachineRepresentation::kWord64, MachineSemantic::kNone);
  }
  static MachineType RepFloat32() {
    return MachineType(MachineRepresentation::kFloat32, MachineSemantic::kNone);
  }
  static MachineType RepFloat64() {
    return MachineType(MachineRepresentation::kFloat64, MachineSemantic::kNone);
  }
  static MachineType RepSimd128() {
    return MachineType(MachineRepresentation::kSimd128, MachineSemantic::kNone);
  }
  static MachineType RepTagged() {
    return MachineType(MachineRepresentation::kTagged, MachineSemantic::kNone);
  }
  static MachineType RepBit() {
    return MachineType(MachineRepresentation::kBit, MachineSemantic::kNone);
  }

 private:
  MachineRepresentation representation_;
  MachineSemantic semantic_;
};

V8_INLINE size_t hash_value(MachineRepresentation rep) {
  return static_cast<size_t>(rep);
}

V8_INLINE size_t hash_value(MachineType type) {
  return static_cast<size_t>(type.representation()) +
         static_cast<size_t>(type.semantic()) * 16;
}

std::ostream& operator<<(std::ostream& os, MachineRepresentation rep);
std::ostream& operator<<(std::ostream& os, MachineSemantic type);
std::ostream& operator<<(std::ostream& os, MachineType type);

inline bool IsFloatingPoint(MachineRepresentation rep) {
  return rep == MachineRepresentation::kFloat32 ||
         rep == MachineRepresentation::kFloat64 ||
         rep == MachineRepresentation::kSimd128;
}

// Gets the log2 of the element size in bytes of the machine type.
inline int ElementSizeLog2Of(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kBit:
    case MachineRepresentation::kWord8:
      return 0;
    case MachineRepresentation::kWord16:
      return 1;
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kFloat32:
      return 2;
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kFloat64:
      return 3;
    case MachineRepresentation::kSimd128:
      return 4;
    case MachineRepresentation::kTagged:
      return kPointerSizeLog2;
    default:
      break;
  }
  UNREACHABLE();
  return -1;
}

typedef Signature<MachineType> MachineSignature;

}  // namespace internal
}  // namespace v8

#endif  // V8_MACHINE_TYPE_H_

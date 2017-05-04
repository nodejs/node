// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MACHINE_TYPE_H_
#define V8_MACHINE_TYPE_H_

#include <iosfwd>

#include "src/base/bits.h"
#include "src/globals.h"
#include "src/signature.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

enum class MachineRepresentation {
  kNone,
  kBit,
  kWord8,
  kWord16,
  kWord32,
  kWord64,
  kTaggedSigned,
  kTaggedPointer,
  kTagged,
  // FP representations must be last, and in order of increasing size.
  kFloat32,
  kFloat64,
  kSimd128,
  kSimd1x4,  // SIMD boolean vector types.
  kSimd1x8,
  kSimd1x16,
  kFirstFPRepresentation = kFloat32,
  kLastRepresentation = kSimd1x16
};

static_assert(static_cast<int>(MachineRepresentation::kLastRepresentation) <
                  kIntSize * kBitsPerByte,
              "Bit masks of MachineRepresentation should fit in an int");

const char* MachineReprToString(MachineRepresentation);

enum class MachineSemantic {
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

  bool IsNone() { return representation() == MachineRepresentation::kNone; }

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
  static MachineType UintPtr() {
    return (kPointerSize == 4) ? Uint32() : Uint64();
  }
  static MachineType IntPtr() {
    return (kPointerSize == 4) ? Int32() : Int64();
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
  static MachineType Simd1x4() {
    return MachineType(MachineRepresentation::kSimd1x4, MachineSemantic::kNone);
  }
  static MachineType Simd1x8() {
    return MachineType(MachineRepresentation::kSimd1x8, MachineSemantic::kNone);
  }
  static MachineType Simd1x16() {
    return MachineType(MachineRepresentation::kSimd1x16,
                       MachineSemantic::kNone);
  }
  static MachineType Pointer() {
    return MachineType(PointerRepresentation(), MachineSemantic::kNone);
  }
  static MachineType TaggedPointer() {
    return MachineType(MachineRepresentation::kTaggedPointer,
                       MachineSemantic::kAny);
  }
  static MachineType TaggedSigned() {
    return MachineType(MachineRepresentation::kTaggedSigned,
                       MachineSemantic::kInt32);
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
  static MachineType RepSimd1x4() {
    return MachineType(MachineRepresentation::kSimd1x4, MachineSemantic::kNone);
  }
  static MachineType RepSimd1x8() {
    return MachineType(MachineRepresentation::kSimd1x8, MachineSemantic::kNone);
  }
  static MachineType RepSimd1x16() {
    return MachineType(MachineRepresentation::kSimd1x16,
                       MachineSemantic::kNone);
  }
  static MachineType RepTagged() {
    return MachineType(MachineRepresentation::kTagged, MachineSemantic::kNone);
  }
  static MachineType RepBit() {
    return MachineType(MachineRepresentation::kBit, MachineSemantic::kNone);
  }

  static MachineType TypeForRepresentation(const MachineRepresentation& rep,
                                           bool isSigned = true) {
    switch (rep) {
      case MachineRepresentation::kNone:
        return MachineType::None();
      case MachineRepresentation::kBit:
        return MachineType::Bool();
      case MachineRepresentation::kWord8:
        return isSigned ? MachineType::Int8() : MachineType::Uint8();
      case MachineRepresentation::kWord16:
        return isSigned ? MachineType::Int16() : MachineType::Uint16();
      case MachineRepresentation::kWord32:
        return isSigned ? MachineType::Int32() : MachineType::Uint32();
      case MachineRepresentation::kWord64:
        return isSigned ? MachineType::Int64() : MachineType::Uint64();
      case MachineRepresentation::kFloat32:
        return MachineType::Float32();
      case MachineRepresentation::kFloat64:
        return MachineType::Float64();
      case MachineRepresentation::kSimd128:
        return MachineType::Simd128();
      case MachineRepresentation::kSimd1x4:
        return MachineType::Simd1x4();
      case MachineRepresentation::kSimd1x8:
        return MachineType::Simd1x8();
      case MachineRepresentation::kSimd1x16:
        return MachineType::Simd1x16();
      case MachineRepresentation::kTagged:
        return MachineType::AnyTagged();
      case MachineRepresentation::kTaggedSigned:
        return MachineType::TaggedSigned();
      case MachineRepresentation::kTaggedPointer:
        return MachineType::TaggedPointer();
      default:
        UNREACHABLE();
        return MachineType::None();
    }
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

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           MachineRepresentation rep);
std::ostream& operator<<(std::ostream& os, MachineSemantic type);
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, MachineType type);

inline bool IsFloatingPoint(MachineRepresentation rep) {
  return rep >= MachineRepresentation::kFirstFPRepresentation;
}

inline bool CanBeTaggedPointer(MachineRepresentation rep) {
  return rep == MachineRepresentation::kTagged ||
         rep == MachineRepresentation::kTaggedPointer;
}

inline bool CanBeTaggedSigned(MachineRepresentation rep) {
  return rep == MachineRepresentation::kTagged ||
         rep == MachineRepresentation::kTaggedSigned;
}

inline bool IsAnyTagged(MachineRepresentation rep) {
  return CanBeTaggedPointer(rep) || rep == MachineRepresentation::kTaggedSigned;
}

// Gets the log2 of the element size in bytes of the machine type.
V8_EXPORT_PRIVATE inline int ElementSizeLog2Of(MachineRepresentation rep) {
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
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
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

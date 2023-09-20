// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_REPRESENTATIONS_H_
#define V8_COMPILER_TURBOSHAFT_REPRESENTATIONS_H_

#include <cstdint>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/turboshaft/utils.h"

namespace v8::internal::compiler::turboshaft {

class WordRepresentation;
class FloatRepresentation;

class RegisterRepresentation {
 public:
  enum class Enum : uint8_t {
    kWord32,
    kWord64,
    kFloat32,
    kFloat64,
    kTagged,
    kCompressed
  };

  explicit constexpr RegisterRepresentation(Enum value) : value_(value) {}
  RegisterRepresentation() : value_(kInvalid) {}

  constexpr Enum value() const {
    DCHECK_NE(value_, kInvalid);
    return value_;
  }
  constexpr operator Enum() const { return value(); }

  static constexpr RegisterRepresentation Word32() {
    return RegisterRepresentation(Enum::kWord32);
  }
  static constexpr RegisterRepresentation Word64() {
    return RegisterRepresentation(Enum::kWord64);
  }
  static constexpr RegisterRepresentation Float32() {
    return RegisterRepresentation(Enum::kFloat32);
  }
  static constexpr RegisterRepresentation Float64() {
    return RegisterRepresentation(Enum::kFloat64);
  }
  // A tagged pointer stored in a register, in the case of pointer compression
  // it is an uncompressed pointer or a Smi.
  static constexpr RegisterRepresentation Tagged() {
    return RegisterRepresentation(Enum::kTagged);
  }
  // A compressed tagged pointer stored in a register, the upper 32bit are
  // unspecified.
  static constexpr RegisterRepresentation Compressed() {
    return RegisterRepresentation(Enum::kCompressed);
  }
  // The equivalent of intptr_t/uintptr_t: An integral type with the same size
  // as machine pointers.
  static constexpr RegisterRepresentation PointerSized() {
    if constexpr (kSystemPointerSize == 4) {
      return Word32();
    } else {
      DCHECK_EQ(kSystemPointerSize, 8);
      return Word64();
    }
  }

  constexpr bool IsWord() {
    switch (*this) {
      case Enum::kWord32:
      case Enum::kWord64:
        return true;
      case Enum::kFloat32:
      case Enum::kFloat64:
      case Enum::kTagged:
      case Enum::kCompressed:
        return false;
    }
  }

  constexpr bool IsFloat() {
    switch (*this) {
      case Enum::kFloat32:
      case Enum::kFloat64:
        return true;
      case Enum::kWord32:
      case Enum::kWord64:
      case Enum::kTagged:
      case Enum::kCompressed:
        return false;
    }
  }

  uint64_t MaxUnsignedValue() const {
    switch (this->value()) {
      case Word32():
        return std::numeric_limits<uint32_t>::max();
      case Word64():
        return std::numeric_limits<uint64_t>::max();
      case Enum::kFloat32:
      case Enum::kFloat64:
      case Enum::kTagged:
      case Enum::kCompressed:
        UNREACHABLE();
    }
  }

  MachineRepresentation machine_representation() const {
    switch (*this) {
      case Word32():
        return MachineRepresentation::kWord32;
      case Word64():
        return MachineRepresentation::kWord64;
      case Float32():
        return MachineRepresentation::kFloat32;
      case Float64():
        return MachineRepresentation::kFloat64;
      case Tagged():
        return MachineRepresentation::kTagged;
      case Compressed():
        return MachineRepresentation::kCompressed;
    }
  }

  constexpr uint16_t bit_width() const {
    switch (*this) {
      case Word32():
        return 32;
      case Word64():
        return 64;
      case Float32():
        return 32;
      case Float64():
        return 64;
      case Tagged():
        return kSystemPointerSize;
      case Compressed():
        return kSystemPointerSize;
    }
  }

  static RegisterRepresentation FromMachineRepresentation(
      MachineRepresentation rep) {
    switch (rep) {
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return Word32();
      case MachineRepresentation::kWord64:
        return Word64();
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTagged:
        return Tagged();
      case MachineRepresentation::kCompressedPointer:
      case MachineRepresentation::kCompressed:
        return Compressed();
      case MachineRepresentation::kFloat32:
        return Float32();
      case MachineRepresentation::kFloat64:
        return Float64();
      case MachineRepresentation::kMapWord:
      case MachineRepresentation::kSandboxedPointer:
      case MachineRepresentation::kNone:
      case MachineRepresentation::kSimd128:
      case MachineRepresentation::kSimd256:
        UNREACHABLE();
    }
  }

 private:
  Enum value_;

  static constexpr Enum kInvalid = static_cast<Enum>(-1);
};

V8_INLINE constexpr bool operator==(RegisterRepresentation a,
                                    RegisterRepresentation b) {
  return a.value() == b.value();
}
V8_INLINE constexpr bool operator!=(RegisterRepresentation a,
                                    RegisterRepresentation b) {
  return a.value() != b.value();
}

V8_INLINE size_t hash_value(RegisterRepresentation rep) {
  return static_cast<size_t>(rep.value());
}

std::ostream& operator<<(std::ostream& os, RegisterRepresentation rep);

class WordRepresentation : public RegisterRepresentation {
 public:
  enum class Enum : uint8_t {
    kWord32 = static_cast<int>(RegisterRepresentation::Enum::kWord32),
    kWord64 = static_cast<int>(RegisterRepresentation::Enum::kWord64)
  };
  explicit constexpr WordRepresentation(Enum value)
      : RegisterRepresentation(
            static_cast<RegisterRepresentation::Enum>(value)) {}
  WordRepresentation() = default;
  explicit constexpr WordRepresentation(RegisterRepresentation rep)
      : WordRepresentation(static_cast<Enum>(rep.value())) {
    DCHECK(rep.IsWord());
  }

  static constexpr WordRepresentation Word32() {
    return WordRepresentation(Enum::kWord32);
  }
  static constexpr WordRepresentation Word64() {
    return WordRepresentation(Enum::kWord64);
  }

  static constexpr WordRepresentation PointerSized() {
    return WordRepresentation(RegisterRepresentation::PointerSized());
  }

  constexpr Enum value() const {
    return static_cast<Enum>(RegisterRepresentation::value());
  }
  constexpr operator Enum() const { return value(); }

  constexpr uint64_t MaxUnsignedValue() const {
    switch (this->value()) {
      case Word32():
        return std::numeric_limits<uint32_t>::max();
      case Word64():
        return std::numeric_limits<uint64_t>::max();
    }
  }
  constexpr int64_t MinSignedValue() const {
    switch (this->value()) {
      case Word32():
        return std::numeric_limits<int32_t>::min();
      case Word64():
        return std::numeric_limits<int64_t>::min();
    }
  }
  constexpr int64_t MaxSignedValue() const {
    switch (this->value()) {
      case Word32():
        return std::numeric_limits<int32_t>::max();
      case Word64():
        return std::numeric_limits<int64_t>::max();
    }
  }
};

class FloatRepresentation : public RegisterRepresentation {
 public:
  enum class Enum : uint8_t {
    kFloat32 = static_cast<int>(RegisterRepresentation::Enum::kFloat32),
    kFloat64 = static_cast<int>(RegisterRepresentation::Enum::kFloat64)
  };

  static constexpr FloatRepresentation Float32() {
    return FloatRepresentation(Enum::kFloat32);
  }
  static constexpr FloatRepresentation Float64() {
    return FloatRepresentation(Enum::kFloat64);
  }

  explicit constexpr FloatRepresentation(Enum value)
      : RegisterRepresentation(
            static_cast<RegisterRepresentation::Enum>(value)) {}
  FloatRepresentation() = default;

  constexpr Enum value() const {
    return static_cast<Enum>(RegisterRepresentation::value());
  }
  constexpr operator Enum() const { return value(); }
};

class MemoryRepresentation {
 public:
  enum class Enum : uint8_t {
    kInt8,
    kUint8,
    kInt16,
    kUint16,
    kInt32,
    kUint32,
    kInt64,
    kUint64,
    kFloat32,
    kFloat64,
    kAnyTagged,
    kTaggedPointer,
    kTaggedSigned,
    kSandboxedPointer,
  };

  explicit constexpr MemoryRepresentation(Enum value) : value_(value) {}
  MemoryRepresentation() : value_(kInvalid) {}
  constexpr Enum value() const {
    DCHECK_NE(value_, kInvalid);
    return value_;
  }
  constexpr operator Enum() const { return value(); }

  static constexpr MemoryRepresentation Int8() {
    return MemoryRepresentation(Enum::kInt8);
  }
  static constexpr MemoryRepresentation Uint8() {
    return MemoryRepresentation(Enum::kUint8);
  }
  static constexpr MemoryRepresentation Int16() {
    return MemoryRepresentation(Enum::kInt16);
  }
  static constexpr MemoryRepresentation Uint16() {
    return MemoryRepresentation(Enum::kUint16);
  }
  static constexpr MemoryRepresentation Int32() {
    return MemoryRepresentation(Enum::kInt32);
  }
  static constexpr MemoryRepresentation Uint32() {
    return MemoryRepresentation(Enum::kUint32);
  }
  static constexpr MemoryRepresentation Int64() {
    return MemoryRepresentation(Enum::kInt64);
  }
  static constexpr MemoryRepresentation Uint64() {
    return MemoryRepresentation(Enum::kUint64);
  }
  static constexpr MemoryRepresentation Float32() {
    return MemoryRepresentation(Enum::kFloat32);
  }
  static constexpr MemoryRepresentation Float64() {
    return MemoryRepresentation(Enum::kFloat64);
  }
  static constexpr MemoryRepresentation AnyTagged() {
    return MemoryRepresentation(Enum::kAnyTagged);
  }
  static constexpr MemoryRepresentation TaggedPointer() {
    return MemoryRepresentation(Enum::kTaggedPointer);
  }
  static constexpr MemoryRepresentation TaggedSigned() {
    return MemoryRepresentation(Enum::kTaggedSigned);
  }
  static constexpr MemoryRepresentation SandboxedPointer() {
    return MemoryRepresentation(Enum::kSandboxedPointer);
  }
  static constexpr MemoryRepresentation PointerSized() {
    if constexpr (kSystemPointerSize == 4) {
      return Uint32();
    } else {
      DCHECK_EQ(kSystemPointerSize, 8);
      return Uint64();
    }
  }

  bool IsWord() const {
    switch (*this) {
      case Int8():
      case Uint8():
      case Int16():
      case Uint16():
      case Int32():
      case Uint32():
      case Int64():
      case Uint64():
        return true;
      case Float32():
      case Float64():
      case AnyTagged():
      case TaggedPointer():
      case TaggedSigned():
      case SandboxedPointer():
        return false;
    }
  }

  bool IsSigned() const {
    switch (*this) {
      case Int8():
      case Int16():
      case Int32():
      case Int64():
        return true;
      case Uint8():
      case Uint16():
      case Uint32():
      case Uint64():
        return false;
      case Float32():
      case Float64():
      case AnyTagged():
      case TaggedPointer():
      case TaggedSigned():
      case SandboxedPointer():
        DCHECK(false);
        return false;
    }
  }

  bool IsTagged() const {
    switch (*this) {
      case AnyTagged():
      case TaggedPointer():
      case TaggedSigned():
        return true;
      case Int8():
      case Int16():
      case Int32():
      case Int64():
      case Uint8():
      case Uint16():
      case Uint32():
      case Uint64():
      case Float32():
      case Float64():
      case SandboxedPointer():
        return false;
    }
  }

  bool CanBeTaggedPointer() const {
    switch (*this) {
      case AnyTagged():
      case TaggedPointer():
        return true;
      case TaggedSigned():
      case Int8():
      case Int16():
      case Int32():
      case Int64():
      case Uint8():
      case Uint16():
      case Uint32():
      case Uint64():
      case Float32():
      case Float64():
      case SandboxedPointer():
        return false;
    }
  }

  RegisterRepresentation ToRegisterRepresentation() const {
    switch (*this) {
      case Int8():
      case Uint8():
      case Int16():
      case Uint16():
      case Int32():
      case Uint32():
        return RegisterRepresentation::Word32();
      case Int64():
      case Uint64():
        return RegisterRepresentation::Word64();
      case Float32():
        return RegisterRepresentation::Float32();
      case Float64():
        return RegisterRepresentation::Float64();
      case AnyTagged():
      case TaggedPointer():
      case TaggedSigned():
        return RegisterRepresentation::Tagged();
      case SandboxedPointer():
        return RegisterRepresentation::Word64();
    }
  }

  // The required register representation for storing a value. When pointer
  // compression is enabled, we only store the lower 32bit of a tagged value,
  // which we indicate as `RegisterRepresentation::Compressed()` here.
  RegisterRepresentation ToRegisterRepresentationForStore() const {
    RegisterRepresentation result = ToRegisterRepresentation();
#ifdef V8_COMPRESS_POINTERS
    if (result == RegisterRepresentation::Tagged()) {
      result = RegisterRepresentation::Compressed();
    }
#endif
    return result;
  }

  MachineType ToMachineType() const {
    switch (*this) {
      case Int8():
        return MachineType::Int8();
      case Uint8():
        return MachineType::Uint8();
      case Int16():
        return MachineType::Int16();
      case Uint16():
        return MachineType::Uint16();
      case Int32():
        return MachineType::Int32();
      case Uint32():
        return MachineType::Uint32();
      case Int64():
        return MachineType::Int64();
      case Uint64():
        return MachineType::Uint64();
      case Float32():
        return MachineType::Float32();
      case Float64():
        return MachineType::Float64();
      case AnyTagged():
        return MachineType::AnyTagged();
      case TaggedPointer():
        return MachineType::TaggedPointer();
      case TaggedSigned():
        return MachineType::TaggedSigned();
      case SandboxedPointer():
        return MachineType::SandboxedPointer();
    }
  }

  static MemoryRepresentation FromMachineType(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
        return type.IsSigned() ? Int8() : Uint8();
      case MachineRepresentation::kWord16:
        return type.IsSigned() ? Int16() : Uint16();
      case MachineRepresentation::kWord32:
        return type.IsSigned() ? Int32() : Uint32();
      case MachineRepresentation::kWord64:
        return type.IsSigned() ? Int64() : Uint64();
      case MachineRepresentation::kTaggedSigned:
        return TaggedSigned();
      case MachineRepresentation::kTaggedPointer:
        return TaggedPointer();
      case MachineRepresentation::kMapWord:
        // Turboshaft does not support map packing.
        DCHECK(!V8_MAP_PACKING_BOOL);
        return TaggedPointer();
      case MachineRepresentation::kTagged:
        return AnyTagged();
      case MachineRepresentation::kFloat32:
        return Float32();
      case MachineRepresentation::kFloat64:
        return Float64();
      case MachineRepresentation::kSandboxedPointer:
        return SandboxedPointer();
      case MachineRepresentation::kNone:
      case MachineRepresentation::kBit:
      case MachineRepresentation::kSimd128:
      case MachineRepresentation::kSimd256:
      case MachineRepresentation::kCompressedPointer:
      case MachineRepresentation::kCompressed:
        UNREACHABLE();
    }
  }

  static MemoryRepresentation FromMachineRepresentation(
      MachineRepresentation rep) {
    switch (rep) {
      case MachineRepresentation::kWord8:
        return Uint8();
      case MachineRepresentation::kWord16:
        return Uint16();
      case MachineRepresentation::kWord32:
        return Uint32();
      case MachineRepresentation::kWord64:
        return Uint64();
      case MachineRepresentation::kTaggedSigned:
        return TaggedSigned();
      case MachineRepresentation::kTaggedPointer:
        return TaggedPointer();
      case MachineRepresentation::kTagged:
        return AnyTagged();
      case MachineRepresentation::kFloat32:
        return Float32();
      case MachineRepresentation::kFloat64:
        return Float64();
      case MachineRepresentation::kSandboxedPointer:
        return SandboxedPointer();
      case MachineRepresentation::kNone:
      case MachineRepresentation::kMapWord:
      case MachineRepresentation::kBit:
      case MachineRepresentation::kSimd128:
      case MachineRepresentation::kSimd256:
      case MachineRepresentation::kCompressedPointer:
      case MachineRepresentation::kCompressed:
        UNREACHABLE();
    }
  }

  constexpr uint8_t SizeInBytes() const {
    return uint8_t{1} << SizeInBytesLog2();
  }

  constexpr uint8_t SizeInBytesLog2() const {
    switch (*this) {
      case Int8():
      case Uint8():
        return 0;
      case Int16():
      case Uint16():
        return 1;
      case Int32():
      case Uint32():
      case Float32():
        return 2;
      case Int64():
      case Uint64():
      case Float64():
      case SandboxedPointer():
        return 3;
      case AnyTagged():
      case TaggedPointer():
      case TaggedSigned():
        return kTaggedSizeLog2;
    }
  }

 private:
  Enum value_;

  static constexpr Enum kInvalid = static_cast<Enum>(-1);
};

V8_INLINE bool operator==(MemoryRepresentation a, MemoryRepresentation b) {
  return a.value() == b.value();
}
V8_INLINE bool operator!=(MemoryRepresentation a, MemoryRepresentation b) {
  return a.value() != b.value();
}

V8_INLINE size_t hash_value(MemoryRepresentation rep) {
  return static_cast<size_t>(rep.value());
}

std::ostream& operator<<(std::ostream& os, MemoryRepresentation rep);

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_REPRESENTATIONS_H_

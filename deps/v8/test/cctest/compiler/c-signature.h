// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_C_SIGNATURE_H_
#define V8_COMPILER_C_SIGNATURE_H_

#include "src/compiler/machine-type.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename T>
inline MachineType MachineTypeForC() {
  CHECK(false);  // Instantiated with invalid type.
  return kMachNone;
}

template <>
inline MachineType MachineTypeForC<void>() {
  return kMachNone;
}

template <>
inline MachineType MachineTypeForC<int8_t>() {
  return kMachInt8;
}

template <>
inline MachineType MachineTypeForC<uint8_t>() {
  return kMachUint8;
}

template <>
inline MachineType MachineTypeForC<int16_t>() {
  return kMachInt16;
}

template <>
inline MachineType MachineTypeForC<uint16_t>() {
  return kMachUint16;
}

template <>
inline MachineType MachineTypeForC<int32_t>() {
  return kMachInt32;
}

template <>
inline MachineType MachineTypeForC<uint32_t>() {
  return kMachUint32;
}

template <>
inline MachineType MachineTypeForC<int64_t>() {
  return kMachInt64;
}

template <>
inline MachineType MachineTypeForC<uint64_t>() {
  return kMachUint64;
}

template <>
inline MachineType MachineTypeForC<double>() {
  return kMachFloat64;
}

template <>
inline MachineType MachineTypeForC<Object*>() {
  return kMachAnyTagged;
}

template <typename Ret, uint16_t kParamCount>
class CSignatureOf : public MachineSignature {
 protected:
  MachineType storage_[1 + kParamCount];

  CSignatureOf()
      : MachineSignature(MachineTypeForC<Ret>() != kMachNone ? 1 : 0,
                         kParamCount,
                         reinterpret_cast<MachineType*>(&storage_)) {
    if (return_count_ == 1) storage_[0] = MachineTypeForC<Ret>();
  }
  void Set(int index, MachineType type) {
    DCHECK(index >= 0 && index < kParamCount);
    reps_[return_count_ + index] = type;
  }
};

// Helper classes for instantiating Signature objects to be callable from C.
template <typename Ret>
class CSignature0 : public CSignatureOf<Ret, 0> {
 public:
  CSignature0() : CSignatureOf<Ret, 0>() {}
};

template <typename Ret, typename P1>
class CSignature1 : public CSignatureOf<Ret, 1> {
 public:
  CSignature1() : CSignatureOf<Ret, 1>() {
    this->Set(0, MachineTypeForC<P1>());
  }
};

template <typename Ret, typename P1, typename P2>
class CSignature2 : public CSignatureOf<Ret, 2> {
 public:
  CSignature2() : CSignatureOf<Ret, 2>() {
    this->Set(0, MachineTypeForC<P1>());
    this->Set(1, MachineTypeForC<P2>());
  }
};

template <typename Ret, typename P1, typename P2, typename P3>
class CSignature3 : public CSignatureOf<Ret, 3> {
 public:
  CSignature3() : CSignatureOf<Ret, 3>() {
    this->Set(0, MachineTypeForC<P1>());
    this->Set(1, MachineTypeForC<P2>());
    this->Set(2, MachineTypeForC<P3>());
  }
};

static const CSignature2<int32_t, int32_t, int32_t> int32_int32_to_int32;
static const CSignature2<uint32_t, uint32_t, uint32_t> uint32_uint32_to_uint32;
static const CSignature2<double, double, double> float64_float64_to_float64;
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_C_SIGNATURE_H_

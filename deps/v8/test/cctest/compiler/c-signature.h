// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_C_SIGNATURE_H_
#define V8_COMPILER_C_SIGNATURE_H_

#include "src/machine-type.h"

namespace v8 {
namespace internal {
namespace compiler {

#define FOREACH_CTYPE_MACHINE_TYPE_MAPPING(V) \
  V(void, MachineType::None())                \
  V(bool, MachineType::Uint8())               \
  V(int8_t, MachineType::Int8())              \
  V(uint8_t, MachineType::Uint8())            \
  V(int16_t, MachineType::Int16())            \
  V(uint16_t, MachineType::Uint16())          \
  V(int32_t, MachineType::Int32())            \
  V(uint32_t, MachineType::Uint32())          \
  V(int64_t, MachineType::Int64())            \
  V(uint64_t, MachineType::Uint64())          \
  V(float, MachineType::Float32())            \
  V(double, MachineType::Float64())           \
  V(void*, MachineType::Pointer())            \
  V(int*, MachineType::Pointer())

template <typename T>
inline MachineType MachineTypeForC() {
  while (false) {
    // All other types T must be assignable to Object*
    *(static_cast<Object* volatile*>(0)) = static_cast<T>(0);
  }
  return MachineType::AnyTagged();
}

#define DECLARE_TEMPLATE_SPECIALIZATION(ctype, mtype) \
  template <>                                         \
  inline MachineType MachineTypeForC<ctype>() {       \
    return mtype;                                     \
  }
FOREACH_CTYPE_MACHINE_TYPE_MAPPING(DECLARE_TEMPLATE_SPECIALIZATION)
#undef DECLARE_TEMPLATE_SPECIALIZATION

// Helper for building machine signatures from C types.
class CSignature : public MachineSignature {
 protected:
  CSignature(size_t return_count, size_t parameter_count, MachineType* reps)
      : MachineSignature(return_count, parameter_count, reps) {}

 public:
  template <typename P1 = void, typename P2 = void, typename P3 = void,
            typename P4 = void, typename P5 = void>
  static void VerifyParams(MachineSignature* sig) {
    // Verifies the C signature against the machine types. Maximum {5} params.
    CHECK_LT(sig->parameter_count(), 6u);
    const int kMax = 5;
    MachineType params[] = {MachineTypeForC<P1>(), MachineTypeForC<P2>(),
                            MachineTypeForC<P3>(), MachineTypeForC<P4>(),
                            MachineTypeForC<P5>()};
    for (int p = kMax - 1; p >= 0; p--) {
      if (p < static_cast<int>(sig->parameter_count())) {
        CHECK_EQ(sig->GetParam(p), params[p]);
      } else {
        CHECK_EQ(MachineType::None(), params[p]);
      }
    }
  }

  static CSignature* FromMachine(Zone* zone, MachineSignature* msig) {
    return reinterpret_cast<CSignature*>(msig);
  }

  static CSignature* New(Zone* zone, MachineType ret,
                         MachineType p1 = MachineType::None(),
                         MachineType p2 = MachineType::None(),
                         MachineType p3 = MachineType::None(),
                         MachineType p4 = MachineType::None(),
                         MachineType p5 = MachineType::None()) {
    MachineType* buffer = zone->NewArray<MachineType>(6);
    int pos = 0;
    size_t return_count = 0;
    if (ret != MachineType::None()) {
      buffer[pos++] = ret;
      return_count++;
    }
    buffer[pos++] = p1;
    buffer[pos++] = p2;
    buffer[pos++] = p3;
    buffer[pos++] = p4;
    buffer[pos++] = p5;
    size_t param_count = 5;
    if (p5 == MachineType::None()) param_count--;
    if (p4 == MachineType::None()) param_count--;
    if (p3 == MachineType::None()) param_count--;
    if (p2 == MachineType::None()) param_count--;
    if (p1 == MachineType::None()) param_count--;
    for (size_t i = 0; i < param_count; i++) {
      // Check that there are no MachineType::None()'s in the middle of
      // parameters.
      CHECK_NE(MachineType::None(), buffer[return_count + i]);
    }
    return new (zone) CSignature(return_count, param_count, buffer);
  }
};


template <typename Ret, uint16_t kParamCount>
class CSignatureOf : public CSignature {
 protected:
  MachineType storage_[1 + kParamCount];

  CSignatureOf()
      : CSignature(MachineTypeForC<Ret>() != MachineType::None() ? 1 : 0,
                   kParamCount, reinterpret_cast<MachineType*>(&storage_)) {
    if (return_count_ == 1) storage_[0] = MachineTypeForC<Ret>();
  }
  void Set(int index, MachineType type) {
    CHECK_LE(0, index);
    CHECK_LT(index, kParamCount);
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

typedef CSignature2<int32_t, int32_t, int32_t> CSignature_i_ii;
typedef CSignature2<uint32_t, uint32_t, uint32_t> CSignature_u_uu;
typedef CSignature2<float, float, float> CSignature_f_ff;
typedef CSignature2<double, double, double> CSignature_d_dd;
typedef CSignature2<Object*, Object*, Object*> CSignature_o_oo;
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_C_SIGNATURE_H_

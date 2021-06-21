// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_SIGNATURE_H_
#define V8_CODEGEN_SIGNATURE_H_

#include "src/base/functional.h"
#include "src/base/iterator.h"
#include "src/codegen/machine-type.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Describes the inputs and outputs of a function or call.
template <typename T>
class Signature : public ZoneObject {
 public:
  constexpr Signature(size_t return_count, size_t parameter_count,
                      const T* reps)
      : return_count_(return_count),
        parameter_count_(parameter_count),
        reps_(reps) {
    DCHECK_EQ(kReturnCountOffset, offsetof(Signature, return_count_));
    DCHECK_EQ(kParameterCountOffset, offsetof(Signature, parameter_count_));
    DCHECK_EQ(kRepsOffset, offsetof(Signature, reps_));
    STATIC_ASSERT(std::is_standard_layout<Signature<T>>::value);
  }

  size_t return_count() const { return return_count_; }
  size_t parameter_count() const { return parameter_count_; }

  T GetParam(size_t index) const {
    DCHECK_LT(index, parameter_count_);
    return reps_[return_count_ + index];
  }

  T GetReturn(size_t index = 0) const {
    DCHECK_LT(index, return_count_);
    return reps_[index];
  }

  // Iteration support.
  base::iterator_range<const T*> parameters() const {
    return {reps_ + return_count_, reps_ + return_count_ + parameter_count_};
  }
  base::iterator_range<const T*> returns() const {
    return {reps_, reps_ + return_count_};
  }
  base::iterator_range<const T*> all() const {
    return {reps_, reps_ + return_count_ + parameter_count_};
  }

  bool operator==(const Signature& other) const {
    if (this == &other) return true;
    if (parameter_count() != other.parameter_count()) return false;
    if (return_count() != other.return_count()) return false;
    return std::equal(all().begin(), all().end(), other.all().begin());
  }
  bool operator!=(const Signature& other) const { return !(*this == other); }

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
      DCHECK_LT(rcursor_, return_count_);
      buffer_[rcursor_++] = val;
    }

    void AddParam(T val) {
      DCHECK_LT(pcursor_, parameter_count_);
      buffer_[return_count_ + pcursor_++] = val;
    }

    void AddParamAt(size_t index, T val) {
      DCHECK_LT(index, parameter_count_);
      buffer_[return_count_ + index] = val;
      pcursor_ = std::max(pcursor_, index + 1);
    }

    Signature<T>* Build() {
      DCHECK_EQ(rcursor_, return_count_);
      DCHECK_EQ(pcursor_, parameter_count_);
      return zone_->New<Signature<T>>(return_count_, parameter_count_, buffer_);
    }

   private:
    Zone* zone_;
    size_t rcursor_;
    size_t pcursor_;
    T* buffer_;
  };

  static constexpr size_t kReturnCountOffset = 0;
  static constexpr size_t kParameterCountOffset =
      kReturnCountOffset + kSizetSize;
  static constexpr size_t kRepsOffset = kParameterCountOffset + kSizetSize;

 protected:
  size_t return_count_;
  size_t parameter_count_;
  const T* reps_;
};

using MachineSignature = Signature<MachineType>;

template <typename T>
size_t hash_value(const Signature<T>& sig) {
  size_t hash = base::hash_combine(sig.parameter_count(), sig.return_count());
  for (const T& t : sig.all()) hash = base::hash_combine(hash, t);
  return hash;
}

template <typename T, size_t kNumReturns = 0, size_t kNumParams = 0>
class FixedSizeSignature : public Signature<T> {
 public:
  // Add return types to this signature (only allowed if there are none yet).
  template <typename... ReturnTypes>
  auto Returns(ReturnTypes... return_types) const {
    static_assert(kNumReturns == 0, "Please specify all return types at once");
    return FixedSizeSignature<T, sizeof...(ReturnTypes), kNumParams>{
        std::initializer_list<T>{return_types...}.begin(), reps_};
  }

  // Add parameters to this signature (only allowed if there are none yet).
  template <typename... ParamTypes>
  auto Params(ParamTypes... param_types) const {
    static_assert(kNumParams == 0, "Please specify all parameters at once");
    return FixedSizeSignature<T, kNumReturns, sizeof...(ParamTypes)>{
        reps_, std::initializer_list<T>{param_types...}.begin()};
  }

 private:
  // Other template instantiations can call the private constructor.
  template <typename T2, size_t kNumReturns2, size_t kNumParams2>
  friend class FixedSizeSignature;

  FixedSizeSignature(const T* returns, const T* params)
      : Signature<T>(kNumReturns, kNumParams, reps_) {
    std::copy(returns, returns + kNumReturns, reps_);
    std::copy(params, params + kNumParams, reps_ + kNumReturns);
  }

  T reps_[kNumReturns + kNumParams];
};

// Specialization for zero-sized signatures.
template <typename T>
class FixedSizeSignature<T, 0, 0> : public Signature<T> {
 public:
  constexpr FixedSizeSignature() : Signature<T>(0, 0, nullptr) {}

  // Add return types.
  template <typename... ReturnTypes>
  static auto Returns(ReturnTypes... return_types) {
    return FixedSizeSignature<T, sizeof...(ReturnTypes), 0>{
        std::initializer_list<T>{return_types...}.begin(), nullptr};
  }

  // Add parameters.
  template <typename... ParamTypes>
  static auto Params(ParamTypes... param_types) {
    return FixedSizeSignature<T, 0, sizeof...(ParamTypes)>{
        nullptr, std::initializer_list<T>{param_types...}.begin()};
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_SIGNATURE_H_

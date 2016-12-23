// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SIGNATURE_H_
#define V8_SIGNATURE_H_

#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Describes the inputs and outputs of a function or call.
template <typename T>
class Signature : public ZoneObject {
 public:
  Signature(size_t return_count, size_t parameter_count, const T* reps)
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

  const T* raw_data() const { return reps_; }

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
  const T* reps_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SIGNATURE_H_

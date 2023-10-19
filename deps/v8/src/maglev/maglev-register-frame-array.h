// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REGISTER_FRAME_ARRAY_H_
#define V8_MAGLEV_MAGLEV_REGISTER_FRAME_ARRAY_H_

#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

// Vector of values associated with a bytecode's register frame. Indexable by
// interpreter register.
template <typename T>
class RegisterFrameArray {
 public:
  explicit RegisterFrameArray(const MaglevCompilationUnit& info) {
    // The first local is at index zero, parameters are behind it with
    // negative indices, and the unoptimized frame header is between the two,
    // so the entire frame state including parameters is the number of locals
    // and parameters, plus the number of slots between them.
    constexpr interpreter::Register first_param =
        interpreter::Register::FromParameterIndex(0);
    static_assert(first_param.index() < 0);
    static_assert(interpreter::Register(0).index() == 0);
    constexpr int frame_size_between_params_and_locals = -first_param.index();

    T* frame = info.zone()->AllocateArray<T>(
        info.parameter_count() + frame_size_between_params_and_locals +
        info.register_count());

    // Set frame_start_ to a "butterfly" pointer into the middle of the above
    // Zone-allocated array, so that locals start at zero.
    frame_start_ =
        frame + info.parameter_count() + frame_size_between_params_and_locals;
  }

  // Disallow copy (use CopyFrom instead).
  RegisterFrameArray(const RegisterFrameArray& other) V8_NOEXCEPT = delete;
  RegisterFrameArray& operator=(const RegisterFrameArray& other)
      V8_NOEXCEPT = delete;

  // Allow move.
  RegisterFrameArray(RegisterFrameArray&& other) V8_NOEXCEPT = default;
  RegisterFrameArray& operator=(RegisterFrameArray&& other)
      V8_NOEXCEPT = default;

  void CopyFrom(const MaglevCompilationUnit& info,
                const RegisterFrameArray& other,
                const compiler::BytecodeLivenessState* liveness) {
    interpreter::Register last_param =
        interpreter::Register::FromParameterIndex(info.parameter_count() - 1);
    int end = 1;
    if (!liveness) {
      interpreter::Register last_local =
          interpreter::Register(info.register_count() - 1);
      end = last_local.index();
    }
    // All parameters are live.
    for (int index = last_param.index(); index <= end; ++index) {
      interpreter::Register reg(index);
      (*this)[reg] = other[reg];
    }
    if (liveness) {
      for (int index : *liveness) {
        interpreter::Register reg(index);
        (*this)[reg] = other[reg];
      }
    }
  }

  T& operator[](interpreter::Register reg) { return frame_start_[reg.index()]; }

  const T& operator[](interpreter::Register reg) const {
    return frame_start_[reg.index()];
  }

 private:
  static int DataSize(int register_count, int parameter_count) {
    // The first local is at index zero, parameters are behind it with
    // negative indices, and the unoptimized frame header is between the two,
    // so the entire frame state including parameters is the distance from the
    // last parameter to the last local frame register, plus one to include both
    // ends.
    interpreter::Register last_local =
        interpreter::Register(register_count - 1);
    interpreter::Register last_param =
        interpreter::Register::FromParameterIndex(parameter_count - 1);
    return last_local.index() - last_param.index() + 1;
  }

  T* data_begin(int parameter_count) const {
    return frame_start_ +
           interpreter::Register::FromParameterIndex(parameter_count - 1)
               .index();
  }

  // Butterfly pointer for registers, pointing into the middle of a
  // Zone-allocated Node array.
  //                                        |
  //                                        v
  // [Parameters] [Unoptimized Frame Header] [Locals]
  T* frame_start_ = nullptr;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REGISTER_FRAME_ARRAY_H_

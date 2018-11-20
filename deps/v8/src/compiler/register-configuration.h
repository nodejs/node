// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REGISTER_CONFIGURATION_H_
#define V8_COMPILER_REGISTER_CONFIGURATION_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace compiler {

// An architecture independent representation of the sets of registers available
// for instruction creation.
class RegisterConfiguration {
 public:
  // Architecture independent maxes.
  static const int kMaxGeneralRegisters = 32;
  static const int kMaxDoubleRegisters = 32;

  static const RegisterConfiguration* ArchDefault();

  RegisterConfiguration(int num_general_registers, int num_double_registers,
                        int num_aliased_double_registers,
                        const char* const* general_register_name,
                        const char* const* double_register_name);

  int num_general_registers() const { return num_general_registers_; }
  int num_double_registers() const { return num_double_registers_; }
  int num_aliased_double_registers() const {
    return num_aliased_double_registers_;
  }

  const char* general_register_name(int offset) const {
    DCHECK(offset >= 0 && offset < kMaxGeneralRegisters);
    return general_register_names_[offset];
  }
  const char* double_register_name(int offset) const {
    DCHECK(offset >= 0 && offset < kMaxDoubleRegisters);
    return double_register_names_[offset];
  }

 private:
  const int num_general_registers_;
  const int num_double_registers_;
  const int num_aliased_double_registers_;
  const char* const* general_register_names_;
  const char* const* double_register_names_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REGISTER_CONFIGURATION_H_

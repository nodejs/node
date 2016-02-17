// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REGISTER_CONFIGURATION_H_
#define V8_COMPILER_REGISTER_CONFIGURATION_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// An architecture independent representation of the sets of registers available
// for instruction creation.
class RegisterConfiguration {
 public:
  // Define the optimized compiler selector for register configuration
  // selection.
  //
  // TODO(X87): This distinction in RegisterConfigurations is temporary
  // until x87 TF supports all of the registers that Crankshaft does.
  enum CompilerSelector { CRANKSHAFT, TURBOFAN };

  // Architecture independent maxes.
  static const int kMaxGeneralRegisters = 32;
  static const int kMaxDoubleRegisters = 32;

  static const RegisterConfiguration* ArchDefault(CompilerSelector compiler);

  RegisterConfiguration(int num_general_registers, int num_double_registers,
                        int num_allocatable_general_registers,
                        int num_allocatable_double_registers,
                        int num_allocatable_aliased_double_registers,
                        const int* allocatable_general_codes,
                        const int* allocatable_double_codes,
                        char const* const* general_names,
                        char const* const* double_names);

  int num_general_registers() const { return num_general_registers_; }
  int num_double_registers() const { return num_double_registers_; }
  int num_allocatable_general_registers() const {
    return num_allocatable_general_registers_;
  }
  int num_allocatable_double_registers() const {
    return num_allocatable_double_registers_;
  }
  // TODO(turbofan): This is a temporary work-around required because our
  // register allocator does not yet support the aliasing of single/double
  // registers on ARM.
  int num_allocatable_aliased_double_registers() const {
    return num_allocatable_aliased_double_registers_;
  }
  int32_t allocatable_general_codes_mask() const {
    return allocatable_general_codes_mask_;
  }
  int32_t allocatable_double_codes_mask() const {
    return allocatable_double_codes_mask_;
  }
  int GetAllocatableGeneralCode(int index) const {
    return allocatable_general_codes_[index];
  }
  int GetAllocatableDoubleCode(int index) const {
    return allocatable_double_codes_[index];
  }
  const char* GetGeneralRegisterName(int code) const {
    return general_register_names_[code];
  }
  const char* GetDoubleRegisterName(int code) const {
    return double_register_names_[code];
  }
  const int* allocatable_general_codes() const {
    return allocatable_general_codes_;
  }
  const int* allocatable_double_codes() const {
    return allocatable_double_codes_;
  }

 private:
  const int num_general_registers_;
  const int num_double_registers_;
  int num_allocatable_general_registers_;
  int num_allocatable_double_registers_;
  int num_allocatable_aliased_double_registers_;
  int32_t allocatable_general_codes_mask_;
  int32_t allocatable_double_codes_mask_;
  const int* allocatable_general_codes_;
  const int* allocatable_double_codes_;
  char const* const* general_register_names_;
  char const* const* double_register_names_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REGISTER_CONFIGURATION_H_

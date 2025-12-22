/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_RUNTIME_FUNCTION_AARCH64_PACKED_H
#define LIEF_PE_RUNTIME_FUNCTION_AARCH64_PACKED_H

#include "LIEF/visibility.h"
#include <memory>

#include "LIEF/PE/exceptions_info/RuntimeFunctionAArch64.hpp"

namespace LIEF {
namespace PE {
namespace unwind_aarch64 {

/// This class represents a packed AArch64 exception entry.
///
/// An excepted entry can be packed if the unwind data fit in 30 bits
///
/// Reference: https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling?view=msvc-170#packed-unwind-data
class LIEF_API PackedFunction : public RuntimeFunctionAArch64 {
  public:
  static std::unique_ptr<PackedFunction>
    parse(Parser& ctx, BinaryStream& strm, uint32_t rva, uint32_t unwind_data);

  using RuntimeFunctionAArch64::RuntimeFunctionAArch64;

  PackedFunction(const PackedFunction&) = default;
  PackedFunction& operator=(const PackedFunction&) = default;

  PackedFunction(PackedFunction&&) = default;
  PackedFunction& operator=(PackedFunction&&) = default;

  ~PackedFunction() override = default;

  std::unique_ptr<ExceptionInfo> clone() const override {
    return std::unique_ptr<PackedFunction>(new PackedFunction(*this));
  }

  std::string to_string() const override;

  static bool classof(const ExceptionInfo* info) {
    if (!RuntimeFunctionAArch64::classof(info)) {
      return false;
    }
    const auto* aarch64 = static_cast<const RuntimeFunctionAArch64*>(info);
    return aarch64->flag() == PACKED_FLAGS::PACKED ||
           aarch64->flag() == PACKED_FLAGS::PACKED_FRAGMENT;
  }

  /// Size of the allocated stack
  uint8_t frame_size() const {
    return frame_size_;
  }

  /// Number of non-volatile INT registers (x19-x28) saved in the canonical
  /// stack location.
  uint8_t reg_I() const {
    return reg_I_;
  }

  /// Number of non-volatile FP registers (d8-d15) saved in the canonical stack
  /// location
  uint8_t reg_F() const {
    return reg_F_;
  }

  /// 1-bit flag indicating whether the function homes the integer parameter
  /// registers (x0-x7) by storing them at the very start of the function.
  /// (0 = doesn't home registers, 1 = homes registers).
  uint8_t H() const {
    return h_;
  }

  /// Flag indicating whether the function includes extra instructions to set
  /// up a frame chain and return link.
  uint8_t CR() const {
    return cr_;
  }

  PackedFunction& frame_size(uint8_t value) {
    frame_size_ = value;
    return *this;
  }

  PackedFunction& reg_I(uint8_t value) {
    reg_I_ = value;
    return *this;
  }

  PackedFunction& reg_F(uint8_t value) {
    reg_F_ = value;
    return *this;
  }

  PackedFunction& H(uint8_t value) {
    h_ = value;
    return *this;
  }

  PackedFunction& CR(uint8_t value) {
    cr_ = value;
    return *this;
  }

  private:
  uint8_t frame_size_ = 0;
  uint8_t cr_ = 0;
  uint8_t h_ = 0;
  uint8_t reg_I_ = 0;
  uint8_t reg_F_ = 0;

};
}
}
}
#endif

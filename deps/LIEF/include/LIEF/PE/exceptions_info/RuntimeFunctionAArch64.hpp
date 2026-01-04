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
#ifndef LIEF_PE_RUNTIME_FUNCTION_AARCH64_H
#define LIEF_PE_RUNTIME_FUNCTION_AARCH64_H

#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/PE/ExceptionInfo.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;

/// This class represents an entry in the exception table (`.pdata` section)
/// for the AArch64 architecture.
///
/// Since the ARM64 unwinding info can be encoded in a *packed* and *unpacked*
/// format, this class is inherited by LIEF::PE::unwind_aarch64::PackedFunction
/// and LIEF::PE::unwind_aarch64::UnpackedFunction
///
/// Reference: https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling#arm64-exception-handling-information
class LIEF_API RuntimeFunctionAArch64 : public ExceptionInfo {
  public:
  enum class PACKED_FLAGS {
    UNPACKED = 0,
    PACKED = 1,
    PACKED_FRAGMENT = 2,
    RESERVED = 3,
  };

  static std::unique_ptr<RuntimeFunctionAArch64>
    parse(Parser& ctx, BinaryStream& strm);

  RuntimeFunctionAArch64(uint64_t RVA, uint32_t length, PACKED_FLAGS flag) :
    ExceptionInfo(ARCH::ARM64, RVA),
    length_(length),
    flag_(flag)
  {}

  RuntimeFunctionAArch64(const RuntimeFunctionAArch64&) = default;
  RuntimeFunctionAArch64& operator=(const RuntimeFunctionAArch64&) = default;

  RuntimeFunctionAArch64(RuntimeFunctionAArch64&&) = default;
  RuntimeFunctionAArch64& operator=(RuntimeFunctionAArch64&&) = default;

  std::unique_ptr<ExceptionInfo> clone() const override {
    return std::unique_ptr<RuntimeFunctionAArch64>(new RuntimeFunctionAArch64(*this));
  }

  /// Length of the function in bytes
  uint32_t length() const {
    return length_;
  }

  /// Flag describing the format the unwind data
  PACKED_FLAGS flag() const {
    return flag_;
  }

  /// Function end address
  uint32_t rva_end() const {
    return rva_start() + length();
  }


  std::string to_string() const override;

  static bool classof(const ExceptionInfo* info) {
    return info->arch() == ExceptionInfo::ARCH::ARM64;
  }

  ~RuntimeFunctionAArch64() = default;

  protected:
  uint32_t length_ = 0;
  PACKED_FLAGS flag_ = PACKED_FLAGS::RESERVED;
};

}
}


#endif

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
#ifndef LIEF_PE_EXCEPTION_INFO_H
#define LIEF_PE_EXCEPTION_INFO_H

#include <ostream>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/PE/Header.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;


/// This class is the base class for any exception or runtime function entry
class LIEF_API ExceptionInfo {
  public:
  static std::unique_ptr<ExceptionInfo> parse(Parser& ctx, BinaryStream& strm);
  static std::unique_ptr<ExceptionInfo> parse(Parser& ctx, BinaryStream& strm,
                                              Header::MACHINE_TYPES arch);

  ExceptionInfo() = delete;

  ExceptionInfo(const ExceptionInfo&) = default;
  ExceptionInfo& operator=(const ExceptionInfo&) = default;

  ExceptionInfo(ExceptionInfo&&) = default;
  ExceptionInfo& operator=(ExceptionInfo&&) = default;

  virtual std::unique_ptr<ExceptionInfo> clone() const = 0;

  /// Arch discriminator for the subclasses
  enum class ARCH {
    UNKNOWN = 0,
    ARM64, X86_64
  };

  ExceptionInfo(ARCH arch, uint64_t rva) :
    arch_(arch),
    rva_(rva)
  {}

  ExceptionInfo(ARCH arch) :
    ExceptionInfo(arch, /*rva=*/0)
  {}

  /// Target architecture of this exception
  ARCH arch() const {
    return arch_;
  }

  /// Function start address
  uint32_t rva_start() const {
    return rva_;
  }

  /// Offset in the binary where the raw exception information associated with
  /// this entry is defined
  uint64_t offset() const {
    return offset_;
  }

  virtual std::string to_string() const = 0;

  virtual ~ExceptionInfo() = default;

  /// Helper to **downcast** an ExceptionInfo into a concrete implementation
  template<class T>
  T* as() {
    static_assert(std::is_base_of<ExceptionInfo, T>::value,
                  "Require ExceptionInfo inheritance");
    if (T::classof(this)) {
      return static_cast<T*>(this);
    }
    return nullptr;
  }

  template<class T>
  const T* as() const {
    return const_cast<ExceptionInfo*>(this)->as<T>();
  }

  /// \private
  void offset(uint64_t value) {
    offset_ = value;
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ExceptionInfo& info)
  {
    os << info.to_string();
    return os;
  }

  protected:
  ARCH arch_ = ARCH::UNKNOWN;
  uint32_t rva_ = 0;
  uint32_t offset_ = 0;
};

}
}
#endif

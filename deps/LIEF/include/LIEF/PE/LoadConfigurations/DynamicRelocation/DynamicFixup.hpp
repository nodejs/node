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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_H

#include <ostream>
#include <memory>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

namespace LIEF {
class SpanStream;

namespace PE {
class Parser;
class DynamicRelocation;

/// This is the base class for any fixups located in DynamicRelocation.
class LIEF_API DynamicFixup {
  public:
  enum KIND {
    /// If DynamicRelocation::symbol is a special value that is not
    /// supported by LIEF
    UNKNOWN = 0,

    /// If DynamicRelocation::symbol is not a special value
    GENERIC,

    /// If DynamicRelocation::symbol is set to `IMAGE_DYNAMIC_RELOCATION_ARM64X`
    ARM64X,

    /// If DynamicRelocation::symbol is set to `IMAGE_DYNAMIC_RELOCATION_FUNCTION_OVERRIDE`
    FUNCTION_OVERRIDE,

    /// If DynamicRelocation::symbol is set to `IMAGE_DYNAMIC_RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER`
    ARM64_KERNEL_IMPORT_CALL_TRANSFER,

    /// If DynamicRelocation::symbol is set to `IMAGE_DYNAMIC_RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER`
    GUARD_IMPORT_CONTROL_TRANSFER,
  };

  DynamicFixup() = delete;
  DynamicFixup(KIND kind) :
    kind_(kind)
  {}

  DynamicFixup(const DynamicFixup&) = default;
  DynamicFixup& operator=(const DynamicFixup&) = default;

  DynamicFixup(DynamicFixup&&) = default;
  DynamicFixup& operator=(DynamicFixup&&) = default;

  virtual std::unique_ptr<DynamicFixup> clone() const = 0;

  virtual std::string to_string() const = 0;

  /// Encoding of the fixups
  KIND kind() const {
    return kind_;
  }

  template<class T>
  T* as() {
    static_assert(std::is_base_of<DynamicFixup, T>::value,
                  "Require DynamicFixup inheritance");
    if (T::classof(this)) {
      return static_cast<T*>(this);
    }
    return nullptr;
  }

  template<class T>
  const T* as() const {
    return const_cast<DynamicFixup*>(this)->as<T>();
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const DynamicFixup& fixup)
  {
    os << fixup.to_string();
    return os;
  }

  virtual ~DynamicFixup() = default;

  /// \private
  static LIEF_LOCAL ok_error_t
    parse(Parser& ctx, SpanStream& stream, DynamicRelocation& R);

  protected:
  KIND kind_ = KIND::UNKNOWN;

};


}
}

#endif

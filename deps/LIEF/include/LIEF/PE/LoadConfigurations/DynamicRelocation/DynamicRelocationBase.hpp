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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_H

#include <ostream>
#include <cstdint>
#include <memory>
#include <string>

#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {
class DynamicFixup;

/// This is the base class for any `IMAGE_DYNAMIC_RELOCATION32`,
/// `IMAGE_DYNAMIC_RELOCATION32_V2`, `IMAGE_DYNAMIC_RELOCATION64`,
/// `IMAGE_DYNAMIC_RELOCATION64_V2` dynamic relocations.
class LIEF_API DynamicRelocation {
  public:
  /// Special symbol values as defined in `link.exe - GetDVRTSpecialSymbolName`
  enum IMAGE_DYNAMIC_RELOCATION {
    /// Mirror `IMAGE_DYNAMIC_RELOCATION_GUARD_RF_PROLOGUE`
    RELOCATION_GUARD_RF_PROLOGUE = 1,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_GUARD_RF_EPILOGUE`
    RELOCATION_GUARD_RF_EPILOGUE = 2,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER`
    RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER = 3,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_GUARD_INDIR_CONTROL_TRANSFER`
    RELOCATION_GUARD_INDIR_CONTROL_TRANSFER = 4,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_GUARD_SWITCHTABLE_BRANCH`
    RELOCATION_GUARD_SWITCHTABLE_BRANCH = 5,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_ARM64X`
    RELOCATION_ARM64X = 6,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_FUNCTION_OVERRIDE`
    RELOCATION_FUNCTION_OVERRIDE = 7,

    /// Mirror `IMAGE_DYNAMIC_RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER`
    RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER = 8,

    _RELOC_LAST_ENTRY,
  };

  DynamicRelocation() = delete;
  DynamicRelocation(uint32_t version);

  DynamicRelocation(const DynamicRelocation&);
  DynamicRelocation& operator=(const DynamicRelocation&);

  DynamicRelocation(DynamicRelocation&&);
  DynamicRelocation& operator=(DynamicRelocation&&);

  virtual std::unique_ptr<DynamicRelocation> clone() const = 0;

  /// Version of the structure
  uint32_t version() const {
    return version_;
  }

  /// Symbol address. Some values have a special meaning
  /// (c.f. IMAGE_DYNAMIC_RELOCATION) and define how fixups are encoded.
  uint64_t symbol() const {
    return symbol_;
  }

  const DynamicFixup* fixups() const {
    return fixups_.get();
  }

  /// Return fixups information, where the interpretation may depend on the
  /// symbol's value
  DynamicFixup* fixups() {
    return fixups_.get();
  }

  DynamicRelocation& symbol(uint64_t value) {
    symbol_ = value;
    return *this;
  }

  DynamicRelocation& fixups(std::unique_ptr<DynamicFixup> F);

  virtual std::string to_string() const = 0;

  template<class T>
  T* as() {
    static_assert(std::is_base_of<DynamicRelocation, T>::value,
                  "Require DynamicRelocation inheritance");
    if (T::classof(this)) {
      return static_cast<T*>(this);
    }
    return nullptr;
  }

  template<class T>
  const T* as() const {
    return const_cast<DynamicRelocation*>(this)->as<T>();
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const DynamicRelocation& reloc)
  {
    os << reloc.to_string();
    return os;
  }

  virtual ~DynamicRelocation();

  protected:
  uint32_t version_ = 0;
  uint64_t symbol_ = 0;
  std::unique_ptr<DynamicFixup> fixups_;
};

LIEF_API const char* to_string(DynamicRelocation::IMAGE_DYNAMIC_RELOCATION e);

}
}

#endif

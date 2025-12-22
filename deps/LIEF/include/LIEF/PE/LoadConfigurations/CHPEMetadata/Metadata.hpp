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
#ifndef LIEF_PE_LOAD_CONFIGURATION_CHPE_METADATA_H
#define LIEF_PE_LOAD_CONFIGURATION_CHPE_METADATA_H
#include <memory>
#include <string>
#include <cstdint>

#include "LIEF/visibility.h"

namespace LIEF {
class BinaryStream;
namespace PE {
class Parser;

/// Base class for any Compiled Hybrid Portable Executable (CHPE) metadata.
///
/// This class is inherited by architecture-specific implementation.
class LIEF_API CHPEMetadata {
  public:

  /// Discriminator for the subclasses
  enum class KIND {
    UNKNOWN = 0,
    ARM64, X86,
  };

  CHPEMetadata() = default;
  CHPEMetadata(KIND kind, uint32_t version) :
    kind_(kind), version_(version)
  {}

  CHPEMetadata(const CHPEMetadata&) = default;
  CHPEMetadata& operator=(const CHPEMetadata&) = default;

  CHPEMetadata(CHPEMetadata&&) = default;
  CHPEMetadata& operator=(CHPEMetadata&&) = default;

  virtual std::unique_ptr<CHPEMetadata> clone() const {
    return std::unique_ptr<CHPEMetadata>(new CHPEMetadata(*this));
  }

  static std::unique_ptr<CHPEMetadata> parse(Parser& ctx, BinaryStream& stream);

  /// Determine the type of the concrete implementation
  KIND kind() const {
    return kind_;
  }

  /// Version of the structure
  uint32_t version() const {
    return version_;
  }

  virtual std::string to_string() const {
    return std::string("CHPEMetadata{ version= ") + std::to_string(version_) + "}";
  }

  template<class T>
  T* as() {
    static_assert(std::is_base_of<CHPEMetadata, T>::value,
                  "Require CHPEMetadata inheritance");
    if (T::classof(this)) {
      return static_cast<T*>(this);
    }
    return nullptr;
  }

  template<class T>
  const T* as() const {
    return const_cast<CHPEMetadata*>(this)->as<T>();
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const CHPEMetadata& meta)
  {
    os << meta.to_string();
    return os;
  }

  virtual ~CHPEMetadata() = default;

  protected:
  KIND kind_ = KIND::UNKNOWN;
  uint32_t version_ = 0;
};
}
}

#endif

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
#ifndef LIEF_COFF_HEADER_H
#define LIEF_COFF_HEADER_H
#include <cstdint>
#include <memory>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/PE/Header.hpp"

namespace LIEF {
class BinaryStream;

namespace COFF {

/// Class that represents the COFF header. It is subclassed by
/// LIEF::COFF::RegularHeader and LIEF::COFF::BigObjHeader for normal vs
/// `/bigobj` files
class LIEF_API Header {
  public:

  enum class KIND {
    UNKNOWN = 0,
    REGULAR,
    BIGOBJ
  };

  /// The different architectures (mirrored from PE)
  using MACHINE_TYPES = LIEF::PE::Header::MACHINE_TYPES;

  /// Create a header from the given stream
  static std::unique_ptr<Header> create(BinaryStream& stream);
  static std::unique_ptr<Header> create(BinaryStream& stream, KIND kind);

  Header() = default;
  Header(KIND kind) :
    kind_(kind)
  {}

  Header& operator=(const Header&) = default;
  Header(const Header&) = default;

  Header& operator=(Header&&) = default;
  Header(Header&&) = default;

  virtual std::unique_ptr<Header> clone() const = 0;

  /// The type of this header: whether it is regular or using the `/bigobj`
  /// format
  KIND kind() const {
    return kind_;
  }

  /// The machine type targeted by this COFF
  MACHINE_TYPES machine() const {
    return machine_;
  }

  /// The number of sections
  uint32_t nb_sections() const {
    return nb_sections_;
  }

  /// Offset of the symbols table
  uint32_t pointerto_symbol_table() const {
    return pointerto_symbol_table_;
  }

  /// Number of symbols (including auxiliary symbols)
  uint32_t nb_symbols() const {
    return nb_symbols_;
  }

  /// Timestamp when the COFF has been generated
  uint32_t timedatestamp() const {
    return timedatestamp_;
  }

  void machine(MACHINE_TYPES machine) {
    machine_ = machine;
  }

  void nb_sections(uint32_t value) {
    nb_sections_ = value;
  }

  void pointerto_symbol_table(uint32_t value) {
    pointerto_symbol_table_ = value;
  }

  void nb_symbols(uint32_t value) {
    nb_symbols_ = value;
  }

  void timedatestamp(uint32_t value) {
    timedatestamp_ = value;
  }

  virtual std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr) {
    os << hdr.to_string();
    return os;
  }

  template<class T>
  const T* as() const {
    static_assert(std::is_base_of<Header, T>::value,
                  "Require Header inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  virtual ~Header() = default;

  protected:
  KIND kind_ = KIND::UNKNOWN;
  MACHINE_TYPES machine_ = MACHINE_TYPES::UNKNOWN;
  uint32_t nb_sections_ = 0;
  uint32_t pointerto_symbol_table_ = 0;
  uint32_t nb_symbols_ = 0;
  uint32_t timedatestamp_ = 0;
};

LIEF_API inline const char* to_string(Header::KIND kind) {
  switch (kind) {
    case Header::KIND::UNKNOWN: return "UNKNOWN";
    case Header::KIND::REGULAR: return "REGULAR";
    case Header::KIND::BIGOBJ: return "BIGOBJ";
  }
  return "UNKNOWN";
}

LIEF_API inline const char* to_string(Header::MACHINE_TYPES machine) {
  return LIEF::PE::to_string(machine);
}
}
}
#endif

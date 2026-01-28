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
#ifndef LIEF_ABSTRACT_HEADER_H
#define LIEF_ABSTRACT_HEADER_H

#include <ostream>
#include <cstdint>
#include <vector>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/enums.hpp"

namespace LIEF {
namespace ELF {
class Binary;
}

namespace PE {
class Binary;
}

namespace MachO {
class Binary;
}

class LIEF_API Header : public Object {
  public:
  enum class ARCHITECTURES {
    UNKNOWN = 0,
    ARM,
    ARM64,
    MIPS,
    X86,
    X86_64,
    PPC,
    SPARC,
    SYSZ,
    XCORE,
    RISCV,
    LOONGARCH,
    PPC64,
  };

  enum class ENDIANNESS {
    UNKNOWN = 0,
    BIG,
    LITTLE,
  };

  enum MODES : uint64_t {
    NONE = 0,

    BITS_16 = 1LLU << 0, /// 16-bits architecture
    BITS_32 = 1LLU << 1, /// 32-bits architecture
    BITS_64 = 1LLU << 2, /// 64-bits architecture
    THUMB   = 1LLU << 3, /// Support ARM Thumb mode

    ARM64E  = 1LLU << 4, /// ARM64 with extended (security) features
  };

  enum class OBJECT_TYPES {
    UNKNOWN = 0,
    EXECUTABLE,
    LIBRARY,
    OBJECT,
  };

  static Header from(const LIEF::ELF::Binary& elf);
  static Header from(const LIEF::PE::Binary& pe);
  static Header from(const LIEF::MachO::Binary& macho);

  Header() = default;
  Header(const Header&) = default;
  Header& operator=(const Header&) = default;
  ~Header() override = default;

  /// Target architecture
  ARCHITECTURES architecture() const {
    return architecture_;
  }

  /// Optional features for the given architecture
  MODES modes() const {
    return modes_;
  }

  /// MODES as a vector
  std::vector<MODES> modes_list() const;

  bool is(MODES m) const {
    return ((uint64_t)m & (uint64_t)modes_) != 0;
  }

  OBJECT_TYPES object_type() const {
    return object_type_;
  }
  uint64_t entrypoint() const {
    return entrypoint_;
  }

  ENDIANNESS endianness() const {
    return endianness_;
  }

  bool is_32() const {
    return ((uint64_t)modes_ & (uint64_t)MODES::BITS_32) != 0;
  }

  bool is_64() const {
    return ((uint64_t)modes_ & (uint64_t)MODES::BITS_64) != 0;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr);

  protected:
  ARCHITECTURES architecture_ = ARCHITECTURES::UNKNOWN;
  OBJECT_TYPES  object_type_ = OBJECT_TYPES::UNKNOWN;
  uint64_t entrypoint_ = 0;
  ENDIANNESS endianness_ = ENDIANNESS::UNKNOWN;
  MODES modes_ = MODES::NONE;
};

LIEF_API const char* to_string(Header::ARCHITECTURES e);
LIEF_API const char* to_string(Header::OBJECT_TYPES e);
LIEF_API const char* to_string(Header::MODES e);
LIEF_API const char* to_string(Header::ENDIANNESS e);
}

ENABLE_BITMASK_OPERATORS(LIEF::Header::MODES);

#endif

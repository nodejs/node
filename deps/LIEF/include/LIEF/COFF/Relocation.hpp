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
#ifndef LIEF_COFF_RELOCATION_H
#define LIEF_COFF_RELOCATION_H
#include <cstdint>
#include <memory>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/COFF/Header.hpp"
#include "LIEF/Abstract/Relocation.hpp"

namespace LIEF {
class BinaryStream;
namespace COFF {
class Section;
class Parser;
class Symbol;

/// This class represents a COFF relocation
class LIEF_API Relocation : public LIEF::Relocation {
  public:
  friend class Parser;

  static constexpr uint32_t I386  = 1 << 17;
  static constexpr uint32_t X64   = 1 << 18;
  static constexpr uint32_t ARM   = 1 << 19;
  static constexpr uint32_t ARM64 = 1 << 20;
  static constexpr uint32_t MIPS  = 1 << 21;

  /// The different relocation types.
  ///
  /// Please note that the original type is encoded on 16 bits but we encode
  /// the type on 32-bits by adding a discriminator from the 17th bit
  enum class TYPE : uint32_t {
    UNKNOWN = uint32_t(-1),
    I386_ABSOLUTE = I386 + 0x0000,
    I386_DIR16    = I386 + 0x0001,
    I386_REL16    = I386 + 0x0002,
    I386_DIR32    = I386 + 0x0006,
    I386_DIR32NB  = I386 + 0x0007,
    I386_SEG12    = I386 + 0x0009,
    I386_SECTION  = I386 + 0x000A,
    I386_SECREL   = I386 + 0x000B,
    I386_TOKEN    = I386 + 0x000C,
    I386_SECREL7  = I386 + 0x000D,
    I386_REL32    = I386 + 0x0014,

    AMD64_ABSOLUTE = X64 + 0x0000,
    AMD64_ADDR64   = X64 + 0x0001,
    AMD64_ADDR32   = X64 + 0x0002,
    AMD64_ADDR32NB = X64 + 0x0003,
    AMD64_REL32    = X64 + 0x0004,
    AMD64_REL32_1  = X64 + 0x0005,
    AMD64_REL32_2  = X64 + 0x0006,
    AMD64_REL32_3  = X64 + 0x0007,
    AMD64_REL32_4  = X64 + 0x0008,
    AMD64_REL32_5  = X64 + 0x0009,
    AMD64_SECTION  = X64 + 0x000A,
    AMD64_SECREL   = X64 + 0x000B,
    AMD64_SECREL7  = X64 + 0x000C,
    AMD64_TOKEN    = X64 + 0x000D,
    AMD64_SREL32   = X64 + 0x000E,
    AMD64_PAIR     = X64 + 0x000F,
    AMD64_SSPAN32  = X64 + 0x0010,

    ARM_ABSOLUTE  = ARM + 0x0000,
    ARM_ADDR32    = ARM + 0x0001,
    ARM_ADDR32NB  = ARM + 0x0002,
    ARM_BRANCH24  = ARM + 0x0003,
    ARM_BRANCH11  = ARM + 0x0004,
    ARM_TOKEN     = ARM + 0x0005,
    ARM_BLX24     = ARM + 0x0008,
    ARM_BLX11     = ARM + 0x0009,
    ARM_REL32     = ARM + 0x000A,
    ARM_SECTION   = ARM + 0x000E,
    ARM_SECREL    = ARM + 0x000F,
    ARM_MOV32A    = ARM + 0x0010,
    ARM_MOV32T    = ARM + 0x0011,
    ARM_BRANCH20T = ARM + 0x0012,
    ARM_BRANCH24T = ARM + 0x0014,
    ARM_BLX23T    = ARM + 0x0015,
    ARM_PAIR      = ARM + 0x0016,

    ARM64_ABSOLUTE       = ARM64 + 0x0000,
    ARM64_ADDR32         = ARM64 + 0x0001,
    ARM64_ADDR32NB       = ARM64 + 0x0002,
    ARM64_BRANCH26       = ARM64 + 0x0003,
    ARM64_PAGEBASE_REL21 = ARM64 + 0x0004,
    ARM64_REL21          = ARM64 + 0x0005,
    ARM64_PAGEOFFSET_12A = ARM64 + 0x0006,
    ARM64_PAGEOFFSET_12L = ARM64 + 0x0007,
    ARM64_SECREL         = ARM64 + 0x0008,
    ARM64_SECREL_LOW12A  = ARM64 + 0x0009,
    ARM64_SECREL_HIGH12A = ARM64 + 0x000A,
    ARM64_SECREL_LOW12L  = ARM64 + 0x000B,
    ARM64_TOKEN          = ARM64 + 0x000C,
    ARM64_SECTION        = ARM64 + 0x000D,
    ARM64_ADDR64         = ARM64 + 0x000E,
    ARM64_BRANCH19       = ARM64 + 0x000F,
    ARM64_BRANCH14       = ARM64 + 0x0010,
    ARM64_REL32          = ARM64 + 0x0011,

    MIPS_ABSOLUTE  = MIPS + 0x0000,
    MIPS_REFHALF   = MIPS + 0x0001,
    MIPS_REFWORD   = MIPS + 0x0002,
    MIPS_JMPADDR   = MIPS + 0x0003,
    MIPS_REFHI     = MIPS + 0x0004,
    MIPS_REFLO     = MIPS + 0x0005,
    MIPS_GPREL     = MIPS + 0x0006,
    MIPS_LITERAL   = MIPS + 0x0007,
    MIPS_SECTION   = MIPS + 0x000A,
    MIPS_SECREL    = MIPS + 0x000B,
    MIPS_SECRELLO  = MIPS + 0x000C,
    MIPS_SECRELHI  = MIPS + 0x000D,
    MIPS_JMPADDR16 = MIPS + 0x0010,
    MIPS_REFWORDNB = MIPS + 0x0022,
    MIPS_PAIR      = MIPS + 0x0025,
  };

  /// Convert a relocation enum type into a 16-bits value.
  static uint16_t to_value(TYPE rtype) {
    return (uint16_t)rtype;
  }

  /// Create a relocation type from its raw value and the architecture
  static TYPE from_value(uint16_t value, Header::MACHINE_TYPES arch) {
    switch (arch) {
      case Header::MACHINE_TYPES::ARM64:
        return TYPE(value + ARM64);

      case Header::MACHINE_TYPES::AMD64:
        return TYPE(value + X64);

      case Header::MACHINE_TYPES::I386:
        return TYPE(value + I386);

      case Header::MACHINE_TYPES::ARM:
      case Header::MACHINE_TYPES::ARMNT:
      case Header::MACHINE_TYPES::THUMB:
        return TYPE(value + ARM);

      case Header::MACHINE_TYPES::R4000:
        return TYPE(value + MIPS);

      default:
        return TYPE::UNKNOWN;
    }
    return TYPE::UNKNOWN;
  }

  /// Create a relocation from the given stream
  static std::unique_ptr<Relocation> parse(
      BinaryStream& stream, Header::MACHINE_TYPES arch);

  /// Symbol index associated with this relocation
  uint32_t symbol_idx() const {
    return symbol_idx_;
  }

  /// Symbol associated with the relocation (if any)
  Symbol* symbol() {
    return symbol_;
  }

  const Symbol* symbol() const {
    return symbol_;
  }

  /// Type of the relocation
  TYPE type() const {
    return type_;
  }

  /// Section in which the relocation takes place
  Section* section() {
    return section_;
  }

  const Section* section() const {
    return section_;
  }

  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Relocation& R) {
    os << R.to_string();
    return os;
  }

  ~Relocation() override = default;

  private:
  Relocation() = default;
  uint32_t symbol_idx_ = 0;
  TYPE type_ = TYPE::UNKNOWN;
  Section* section_ = nullptr;
  Symbol* symbol_ = nullptr;
};

LIEF_API const char* to_string(Relocation::TYPE e);

}
}
#endif

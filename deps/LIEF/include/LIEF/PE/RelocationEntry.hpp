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
#ifndef LIEF_PE_RELOCATION_ENTRY_H
#define LIEF_PE_RELOCATION_ENTRY_H

#include <ostream>
#include <cassert>

#include "LIEF/Abstract/Relocation.hpp"

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PE/Header.hpp"

namespace LIEF {
namespace PE {

class Relocation;

/// Class which represents an entry of the PE relocation table
///
/// It extends the LIEF::Relocation object to provide an uniform API across the file formats
class LIEF_API RelocationEntry : public LIEF::Relocation {

  friend class Parser;
  friend class Builder;
  friend class PE::Relocation;

  public:
  static constexpr auto MAX_ADDR = 1 << 12;

  /// Relocation type as described in
  /// https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#base-relocation-types
  enum class BASE_TYPES {
    UNKNOWN = -1,

    /// This value matches: `IMAGE_REL_BASED_ABSOLUTE`
    /// The base relocation is skipped. This type can be used to pad a block.
    ABS = 0,

    /// This value matches: `IMAGE_REL_BASED_HIGH `
    /// The base relocation adds the high 16 bits of the difference to the
    /// 16-bit field at offset. The 16-bit field represents the high value of a
    /// 32-bit word.
    ///
    /// Operation:
    ///
    /// ```cpp
    /// write<int16_t>(ADDR, ((read<int16_t>(ADDR) << 16) + DELTA) >> 16)
    /// ```
    HIGH = 1,

    /// The base relocation adds the low 16 bits of the difference to the 16-bit
    /// field at offset. The 16-bit field represents the low half of a 32-bit word.
    ///
    /// Operation:
    ///
    /// ```cpp
    /// write<int16_t>(ADDR, ((int32_t_t)read<int16_t>(ADDR) + DELTA))
    /// ```
    LOW = 2,

    /// This value matches `IMAGE_REL_BASED_HIGHLOW`
    ///
    /// The base relocation applies all 32 bits of the difference to the 32-bit
    /// field at offset.
    ///
    /// Operation:
    ///
    /// ```cpp
    /// write<int32_t_t>(ADDR, read<int32_t_t>(ADDR) + DELTA)
    /// ```
    HIGHLOW = 3,

    /// This value matches `IMAGE_REL_BASED_HIGHADJ`
    ///
    /// The base relocation adds the high 16 bits of the difference to the 16-bit
    /// field at offset. The 16-bit field represents the high value of a 32-bit
    /// word. The low 16 bits of the 32-bit value are stored in the 16-bit word
    /// that follows this base relocation. This means that this base relocation
    /// occupies two slots.
    HIGHADJ = 4,

    MIPS_JMPADDR = 5 | (1 << 8),
    ARM_MOV32 = 5 | (1 << 9),
    RISCV_HI20 = 5 | (1 << 10),

    SECTION = 6,

    THUMB_MOV32 = 7 | (1 << 11),
    RISCV_LOW12I = 7 | (1 << 12),

    RISCV_LOW12S = 8 | (1 << 13),
    LOONARCH_MARK_LA = 8 | (1 << 14),

    MIPS_JMPADDR16 = 9,

    /// This value matches `IMAGE_REL_BASED_DIR64`
    ///
    /// The base relocation applies the difference to the 64-bit field at offset.
    ///
    /// Operation:
    ///
    /// ```cpp
    /// write<int64_t_t>(ADDR, read<int64_t_t>(ADDR) + DELTA)
    /// ```
    DIR64  = 10,
    HIGH3ADJ = 11,
  };

  static uint16_t get_position(uint16_t data) {
    return data & 0xFFF;
  }

  static uint16_t get_type(uint16_t data) {
    return (data >> 12) & 0xFF;
  }

  static BASE_TYPES type_from_data(Header::MACHINE_TYPES arch, uint16_t data);

  RelocationEntry() = default;
  RelocationEntry(const RelocationEntry& other) :
    LIEF::Relocation(other),
    position_(other.position_),
    type_(other.type_),
    // Parent relocation is not forwarded during copy
    relocation_(nullptr)
  {}

  RelocationEntry& operator=(RelocationEntry other) {
    swap(other);
    return *this;
  }

  RelocationEntry(RelocationEntry&& other) = default;
  RelocationEntry& operator=(RelocationEntry&& other) = default;

  RelocationEntry(uint16_t position, BASE_TYPES type) :
    position_(position),
    type_(type)
  {
    assert(position_ < MAX_ADDR);
  }

  ~RelocationEntry() override = default;

  void swap(RelocationEntry& other) {
    LIEF::Relocation::swap(other);
    std::swap(position_,   other.position_);
    std::swap(type_,       other.type_);
    std::swap(relocation_, other.relocation_);
  }

  /// The address of the relocation
  uint64_t address() const override;

  void address(uint64_t address) override;

  /// The size of the relocatable pointer
  size_t size() const override;

  void size(size_t size) override;

  /// Raw data of the relocation:
  /// - The **high** 4 bits store the relocation type
  /// - The **low** 12 bits store the relocation offset
  uint16_t data() const {
    return (uint16_t((uint8_t)type_ & 0xFF) << 12) | position_;
  }

  /// Offset relative to Relocation::virtual_address where the relocation occurs.
  uint16_t position() const {
    return position_;
  }

  /// Type of the relocation
  BASE_TYPES type() const {
    return type_;
  }

  void position(uint16_t position) {
    assert(position < MAX_ADDR);
    position_ = position;
  }

  void type(BASE_TYPES type) {
    type_ = type;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const RelocationEntry& entry);

  /// \private
  LIEF_LOCAL PE::Relocation* parent() {
    return relocation_;
  }

  /// \private
  LIEF_LOCAL const PE::Relocation* parent() const {
    return relocation_;
  }

  /// \private
  LIEF_LOCAL void parent(PE::Relocation& R) {
    relocation_ = &R;
  }

  private:
  uint16_t position_ = 0;
  BASE_TYPES type_ = BASE_TYPES::ABS;
  PE::Relocation* relocation_ = nullptr; // Used to compute some information
};

LIEF_API const char* to_string(RelocationEntry::BASE_TYPES e);

}
}
#endif

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
#ifndef LIEF_MACHO_RELOCATION_COMMAND_H
#define LIEF_MACHO_RELOCATION_COMMAND_H
#include <ostream>
#include <memory>

#include "LIEF/Abstract/Relocation.hpp"

#include "LIEF/MachO/Header.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace MachO {
class BinaryParser;
class Section;
class SegmentCommand;
class Symbol;

/// Class that represents a Mach-O relocation
///
/// @see:
///   * MachO::RelocationObject
///   * MachO::RelocationDyld
///   * MachO::RelocationFixup
class LIEF_API Relocation : public LIEF::Relocation {

  friend class BinaryParser;

  public:
  using LIEF::Relocation::address;
  using LIEF::Relocation::size;

  enum class ORIGIN {
    UNKNOWN        = 0,
    DYLDINFO       = 1,
    RELOC_TABLE    = 2,
    CHAINED_FIXUPS = 3,
  };

  static constexpr auto R_SCATTERED = uint32_t(0x80000000);
  static constexpr auto R_ABS = uint32_t(0);

  Relocation() = default;
  Relocation(uint64_t address, uint8_t type);

  Relocation& operator=(const Relocation& other);
  Relocation(const Relocation& other);
  void swap(Relocation& other) noexcept;

  ~Relocation() override = default;

  virtual std::unique_ptr<Relocation> clone() const = 0;

  /// Indicates whether the item containing the address to be
  /// relocated is part of a CPU instruction that uses PC-relative addressing.
  ///
  /// For addresses contained in PC-relative instructions, the CPU adds the address of
  /// the instruction to the address contained in the instruction.
  virtual bool is_pc_relative() const = 0;

  /// Type of the relocation according to the
  /// Relocation::architecture and/or the Relocation::origin
  ///
  /// See:
  ///   * MachO::X86_RELOCATION
  ///   * MachO::X86_64_RELOCATION
  ///   * MachO::PPC_RELOCATION
  ///   * MachO::ARM_RELOCATION
  ///   * MachO::ARM64_RELOCATION
  ///   * MachO::REBASE_TYPES
  virtual uint8_t type() const {
    return type_;
  }

  /// Achitecture targeted by this relocation
  Header::CPU_TYPE architecture() const {
    return architecture_;
  }

  /// Origin of the relocation
  virtual ORIGIN origin() const = 0;

  /// ``true`` if the relocation has a symbol associated with
  bool has_symbol() const {
    return symbol() != nullptr;
  }

  /// Symbol associated with the relocation, if any,
  /// otherwise a nullptr.
  Symbol* symbol() {
    return symbol_;
  }
  const Symbol* symbol() const {
    return symbol_;
  }

  /// ``true`` if the relocation has a section associated with
  bool has_section() const {
    return section() != nullptr;
  }

  /// Section associated with the relocation, if any,
  /// otherwise a nullptr.
  Section* section() {
    return section_;
  }
  const Section* section() const {
    return section_;
  }

  /// ``true`` if the relocation has a SegmentCommand associated with
  bool has_segment() const {
    return segment() != nullptr;
  }

  /// SegmentCommand associated with the relocation, if any,
  /// otherwise a nullptr.
  SegmentCommand* segment() {
    return segment_;
  }
  const SegmentCommand* segment() const {
    return segment_;
  }

  template<class T>
  const T* cast() const {
    static_assert(std::is_base_of<Relocation, T>::value, "Require Relocation inheritance");
    if (T::classof(*this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* cast() {
    return const_cast<T*>(static_cast<const Relocation*>(this)->cast<T>());
  }

  virtual void pc_relative(bool val) = 0;
  virtual void type(uint8_t type);

  void accept(Visitor& visitor) const override;

  virtual std::ostream& print(std::ostream& os) const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Relocation& relocation);

  protected:
  Symbol*         symbol_ = nullptr;
  uint8_t         type_ = 0;
  Header::CPU_TYPE architecture_ = Header::CPU_TYPE::ANY;
  Section*        section_ = nullptr;
  SegmentCommand* segment_ = nullptr;
};


LIEF_API const char* to_string(Relocation::ORIGIN e);

}
}
#endif

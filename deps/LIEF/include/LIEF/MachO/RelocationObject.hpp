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
#ifndef LIEF_MACHO_RELOCATION_OBJECT_COMMAND_H
#define LIEF_MACHO_RELOCATION_OBJECT_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/Relocation.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;

namespace details {
struct relocation_info;
struct scattered_relocation_info;
}

/// Class that represents a relocation presents in the MachO object
/// file (``.o``). Usually, this kind of relocation is found in the MachO::Section
///
/// @see RelocationDyld
class LIEF_API RelocationObject : public Relocation {

  friend class BinaryParser;

  public:
  using Relocation::Relocation;
  RelocationObject() = default;
  RelocationObject(const details::relocation_info& relocinfo);
  RelocationObject(const details::scattered_relocation_info& scattered_relocinfo);

  RelocationObject& operator=(const RelocationObject& other) = default;
  RelocationObject(const RelocationObject& other) = default;

  void swap(RelocationObject& other) noexcept;

  ~RelocationObject() override = default;

  std::unique_ptr<Relocation> clone() const override {
    return std::unique_ptr<RelocationObject>(new RelocationObject(*this));
  }

  /// Whether the relocation is PC relative
  bool is_pc_relative() const override {
    return is_pcrel_;
  }

  /// Size of the relocation
  size_t size() const override;

  /// Address where the relocation is applied
  /// This address is relative to the start of the section where the relocation takes place
  uint64_t address() const override;

  /// ``true`` if the relocation is a scattered one
  bool is_scattered() const {
    return is_scattered_;
  }

  /// For **scattered** relocations:
  /// The address of the relocatable expression for the item in the file that needs
  /// to be updated if the address is changed.
  ///
  /// For relocatable expressions with the difference of two section addresses,
  /// the address from which to subtract (in mathematical terms, the minuend)
  /// is contained in the first relocation entry and the address to subtract (the subtrahend)
  /// is contained in the second relocation entry.
  int32_t value() const;

  /// Origin of the relocation. For this object it should be Relocation::ORIGIN::RELOC_TABLE)
  ORIGIN origin() const override {
    return ORIGIN::RELOC_TABLE;
  }

  void pc_relative(bool val) override {
    is_pcrel_ = val;
  }
  void size(size_t size) override;

  void value(int32_t value);

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override {
    return Relocation::print(os);
  }

  static bool classof(const Relocation& r) {
    return r.origin() == Relocation::ORIGIN::RELOC_TABLE;
  }

  private:
  bool is_pcrel_ = false;
  bool is_scattered_ = false;
  int32_t value_ = 0;
};

}
}
#endif

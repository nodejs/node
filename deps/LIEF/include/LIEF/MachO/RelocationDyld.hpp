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
#ifndef LIEF_MACHO_RELOCATION_DYLD_COMMAND_H
#define LIEF_MACHO_RELOCATION_DYLD_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/Relocation.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;

/// Class that represents a relocation found in the DyldInfo structure.
///
/// While this class does not have an associated structure in the Mach-O format specification,
/// it provides a convenient interface for the Dyld::rebase
class LIEF_API RelocationDyld : public Relocation {

  friend class BinaryParser;

  public:
  using Relocation::Relocation;
  using LIEF::Relocation::operator<;
  using LIEF::Relocation::operator<=;
  using LIEF::Relocation::operator>;
  using LIEF::Relocation::operator>=;
  RelocationDyld() = default;

  RelocationDyld& operator=(const RelocationDyld&) = default;
  RelocationDyld(const RelocationDyld&) = default;

  ~RelocationDyld() override = default;

  std::unique_ptr<Relocation> clone() const override {
    return std::unique_ptr<RelocationDyld>(new RelocationDyld(*this));
  }

  /// Indicates whether the item containing the address to be
  /// relocated is part of a CPU instruction that uses PC-relative addressing.
  ///
  /// For addresses contained in PC-relative instructions, the CPU adds the address of
  /// the instruction to the address contained in the instruction.
  bool is_pc_relative() const override;

  /// Origin of the relocation. For this concrete object, it
  /// should be Relocation::ORIGIN::DYLDINFO
  ORIGIN origin() const override {
    return ORIGIN::DYLDINFO;
  }

  void pc_relative(bool val) override;

  bool operator<(const RelocationDyld& rhs) const;
  bool operator>=(const RelocationDyld& rhs) const {
    return !(*this < rhs);
  }

  bool operator>(const RelocationDyld& rhs) const;
  bool operator<=(const RelocationDyld& rhs) const {
    return !(*this > rhs);
  }

  void accept(Visitor& visitor) const override;

  static bool classof(const Relocation& r) {
    return r.origin() == Relocation::ORIGIN::DYLDINFO;
  }

  std::ostream& print(std::ostream& os) const override {
    return Relocation::print(os);
  }
};

}
}
#endif

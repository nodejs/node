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
#ifndef LIEF_ELF_SYMBOL_VERSION_REQUIREMENTS_H
#define LIEF_ELF_SYMBOL_VERSION_REQUIREMENTS_H

#include <cstdint>
#include <string>
#include <ostream>
#include <vector>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

namespace LIEF {
namespace ELF {
class Parser;
class SymbolVersionAuxRequirement;

namespace details {
struct Elf64_Verneed;
struct Elf32_Verneed;
}

/// Class which represents an entry in the `DT_VERNEED` or `.gnu.version_r` table
class LIEF_API SymbolVersionRequirement : public Object {
  friend class Parser;

  public:
  using aux_requirement_t        = std::vector<std::unique_ptr<SymbolVersionAuxRequirement>>;
  using it_aux_requirement       = ref_iterator<aux_requirement_t&, SymbolVersionAuxRequirement*>;
  using it_const_aux_requirement = const_ref_iterator<const aux_requirement_t&, const SymbolVersionAuxRequirement*>;

  SymbolVersionRequirement() = default;
  SymbolVersionRequirement(const details::Elf64_Verneed& header);
  SymbolVersionRequirement(const details::Elf32_Verneed& header);
  ~SymbolVersionRequirement() override = default;

  SymbolVersionRequirement& operator=(SymbolVersionRequirement other);
  SymbolVersionRequirement(const SymbolVersionRequirement& other);
  void swap(SymbolVersionRequirement& other);

  /// Version revision
  ///
  /// This field should always have the value ``1``. It will be changed
  /// if the versioning implementation has to be changed in an incompatible way.
  uint16_t version() const {
    return version_;
  }

  /// Number of auxiliary entries
  size_t cnt() const {
    return aux_requirements_.size();
  }

  /// Auxiliary entries as an iterator over SymbolVersionAuxRequirement
  it_aux_requirement auxiliary_symbols() {
    return aux_requirements_;
  }

  it_const_aux_requirement auxiliary_symbols() const {
    return aux_requirements_;
  }

  /// Return the library name associated with this requirement (e.g. ``libc.so.6``)
  const std::string& name() const {
    return name_;
  }

  void version(uint16_t version) {
    version_ = version;
  }

  void name(const std::string& name) {
    name_ = name;
  }

  /// Add a version auxiliary requirement to the existing list
  SymbolVersionAuxRequirement& add_aux_requirement(const SymbolVersionAuxRequirement& aux_requirement);

  /// Try to find the SymbolVersionAuxRequirement with the given name (e.g. `GLIBC_2.27`)
  const SymbolVersionAuxRequirement* find_aux(const std::string& name) const;

  SymbolVersionAuxRequirement* find_aux(const std::string& name) {
    return const_cast<SymbolVersionAuxRequirement*>(static_cast<const SymbolVersionRequirement*>(this)->find_aux(name));
  }

  /// Try to remove the auxiliary requirement symbol with the given name.
  /// The function returns true if the operation succeed, false otherwise.
  ///
  /// \warning this function invalidates all the references (pointers) of
  ///          SymbolVersionAuxRequirement. Therefore, the user is responsible
  ///          to ensure that the auxiliary requirement is no longer used in the
  ///          ELF binary (e.g. in SymbolVersion)
  bool remove_aux_requirement(const std::string& name) {
    if (SymbolVersionAuxRequirement* aux = find_aux(name)) {
      return remove_aux_requirement(*aux);
    }
    return false;
  }

  /// Try to remove the given auxiliary requirement symbol.
  /// The function returns true if the operation succeed, false otherwise.
  ///
  /// \warning this function invalidates all the references (pointers) of
  ///          SymbolVersionAuxRequirement. Therefore, the user is responsible
  ///          to ensure that the auxiliary requirement is no longer used in the
  ///          ELF binary (e.g. in SymbolVersion)
  bool remove_aux_requirement(SymbolVersionAuxRequirement& aux);

  void accept(Visitor& visitor) const override;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const SymbolVersionRequirement& symr) {
    os << symr.version() << " " << symr.name();
    return os;
  }

  private:
  aux_requirement_t aux_requirements_;
  uint16_t    version_ = 0;
  std::string name_;
};

}
}
#endif


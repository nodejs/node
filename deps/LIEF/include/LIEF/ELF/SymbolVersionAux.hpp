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
#ifndef LIEF_ELF_SYMBOL_VERSION_AUX_H
#define LIEF_ELF_SYMBOL_VERSION_AUX_H

#include <string>
#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {
class Parser;

/// Class which represents an Auxiliary Symbol version
class LIEF_API SymbolVersionAux : public Object {
  friend class Parser;
  public:
  SymbolVersionAux(std::string name) :
    name_(std::move(name))
  {}
  SymbolVersionAux() = default;
  ~SymbolVersionAux() override = default;

  SymbolVersionAux& operator=(const SymbolVersionAux&) = default;
  SymbolVersionAux(const SymbolVersionAux&) = default;

  /// Symbol's aux name (e.g. ``GLIBC_2.2.5``)
  const std::string& name() const {
    return name_;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const SymbolVersionAux& aux) {
    os << aux.name();
    return os;
  }

  protected:
  std::string name_;
};

}
}
#endif

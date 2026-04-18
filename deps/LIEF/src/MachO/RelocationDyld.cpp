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
#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/RelocationDyld.hpp"
#include "LIEF/MachO/DyldInfo.hpp"

namespace LIEF {
namespace MachO {

bool RelocationDyld::is_pc_relative() const {
  return DyldInfo::REBASE_TYPE(type()) == DyldInfo::REBASE_TYPE::TEXT_PCREL32;
}

void RelocationDyld::pc_relative(bool val) {
  if (is_pc_relative() == val) {
    return;
  }

  if (val) {
    type_ = uint8_t(DyldInfo::REBASE_TYPE::TEXT_PCREL32);
  } else {
    if (size() == 32) {
      type_ = uint8_t(DyldInfo::REBASE_TYPE::TEXT_ABSOLUTE32);
    } else {
      type_ = uint8_t(DyldInfo::REBASE_TYPE::POINTER);
    }
  }
}

void RelocationDyld::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

bool RelocationDyld::operator<(const RelocationDyld& rhs) const {
  // From ld/OutputFile.h
  if (type() != rhs.type()) {
    return type() < rhs.type();
  }
  return address() < rhs.address();
}

bool RelocationDyld::operator>(const RelocationDyld& rhs) const {
  if (type() != rhs.type()) {
    return type() > rhs.type();
  }
  return address() > rhs.address();
}

}
}

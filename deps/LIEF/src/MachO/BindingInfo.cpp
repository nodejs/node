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

#include <spdlog/fmt/fmt.h>
#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/BindingInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"

namespace LIEF {
namespace MachO {

BindingInfo::BindingInfo(const BindingInfo& other) :
  Object{other},
  library_ordinal_{other.library_ordinal_},
  addend_{other.addend_},
  is_weak_import_{other.is_weak_import_},
  address_{other.address_}
{}


BindingInfo& BindingInfo::operator=(const BindingInfo& other) {
  if (&other == this) {
    return *this;
  }
  Object::operator=(other);
  library_ordinal_ = other.library_ordinal_;
  addend_ = other.addend_;
  is_weak_import_ = other.is_weak_import_;
  address_ = other.address_;
  return *this;
}

void BindingInfo::swap(BindingInfo& other) noexcept {
  std::swap(segment_,         other.segment_);
  std::swap(symbol_,          other.symbol_);
  std::swap(library_ordinal_, other.library_ordinal_);
  std::swap(addend_,          other.addend_);
  std::swap(is_weak_import_,  other.is_weak_import_);
  std::swap(library_,         other.library_);
  std::swap(address_,         other.address_);
}

void BindingInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const BindingInfo& info) {
  os << fmt::format("address=0x{:08x}, addend=0x{:x}",
                    info.address(), info.addend());
  if (const Symbol* sym = info.symbol()) {
    os << fmt::format("  symbol={}", sym->name());
  }
  if (const SegmentCommand* seg = info.segment()) {
    os << fmt::format("  segment={}", seg->name());
  }
  if (const DylibCommand* lib = info.library()) {
    os << fmt::format("  library={}", lib->name());
  }
  return os;
}

}
}

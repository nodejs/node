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
#include "spdlog/fmt/fmt.h"
#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/EnumToString.hpp"
#include "frozen.hpp"

namespace LIEF {
namespace MachO {

Relocation::Relocation(const Relocation& other) :
  LIEF::Relocation{other},
  type_{other.type_},
  architecture_{other.architecture_}
{}

Relocation::Relocation(uint64_t address, uint8_t type) {
  address_ = address;
  type_    = type;
}

Relocation& Relocation::operator=(const Relocation& other) {
  if (&other != this) {
    /* Do not copy pointer as they could be not bind to the same Binary */
    address_      = other.address_;
    size_         = other.size_;
    type_         = other.type_;
    architecture_ = other.architecture_;
  }
  return *this;
}
void Relocation::swap(Relocation& other) noexcept {
  LIEF::Relocation::swap(other);

  std::swap(symbol_,       other.symbol_);
  std::swap(type_,         other.type_);
  std::swap(architecture_, other.architecture_);
  std::swap(section_,      other.section_);
  std::swap(segment_,      other.segment_);
}

void Relocation::type(uint8_t type) {
  type_ = type;
}

void Relocation::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& Relocation::print(std::ostream& os) const {
  std::string symbol_name;
  if (const Symbol* sym = symbol()) {
    symbol_name = sym->name();
  }

  std::string section_name;
  if (const Section* sec = section()) {
    section_name = sec->name();
  }

  std::string segment_name;
  if (const SegmentCommand* seg = segment()) {
    segment_name = seg->name();
  }

  std::string segment_section_name;
  if (!section_name.empty() && !segment_name.empty()) {
    segment_section_name = segment_name + '.' + section_name;
  }
  else if (!segment_name.empty()) {
    segment_section_name = segment_name;
  }
  else if (!section_name.empty()) {
    segment_section_name = section_name;
  }

  std::string relocation_type;
  if (origin() == Relocation::ORIGIN::RELOC_TABLE) {
    switch (architecture()) {
      case Header::CPU_TYPE::X86:
        relocation_type = to_string(X86_RELOCATION(type()));
        break;

      case Header::CPU_TYPE::X86_64:
        relocation_type = to_string(X86_64_RELOCATION(type()));
        break;

      case Header::CPU_TYPE::ARM:
        relocation_type = to_string(ARM_RELOCATION(type()));
        break;

      case Header::CPU_TYPE::ARM64:
        relocation_type = to_string(ARM64_RELOCATION(type()));
        break;

      case Header::CPU_TYPE::POWERPC:
        relocation_type = to_string(PPC_RELOCATION(type()));
        break;

      default:
        relocation_type = std::to_string(type());
    }
  }

  if (origin() == Relocation::ORIGIN::DYLDINFO) {
    relocation_type = to_string(DyldInfo::REBASE_TYPE(type()));
  }

  os << fmt::format(
    "address=0x{:x}, type={}, size={}, origin={} ",
    address(), relocation_type, size(), to_string(origin())
  );
  if (!segment_section_name.empty()) {
      os << segment_section_name;
  } else {
    if (!section_name.empty()) {
      os << section_name;
    }

    if (!segment_name.empty()) {
      os << section_name;
    }
  }
  os << ' ' << symbol_name;
  return os;
}


std::ostream& operator<<(std::ostream& os, const Relocation& reloc) {
  return reloc.print(os);
}
const char* to_string(Relocation::ORIGIN e) {
  #define ENTRY(X) std::pair(Relocation::ORIGIN::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(DYLDINFO),
    ENTRY(RELOC_TABLE),
    ENTRY(CHAINED_FIXUPS),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}

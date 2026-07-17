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
#include "logging.hpp"

#include "LIEF/Visitor.hpp"

#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/Relocation.hpp"

#include "frozen.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::RelocationEntry::BASE_TYPES, LIEF::PE::to_string);

namespace LIEF {
namespace PE {

RelocationEntry::BASE_TYPES
  RelocationEntry::type_from_data(Header::MACHINE_TYPES arch, uint16_t data)
{
  uint8_t raw_type = get_type(data);
  // Discriminate the type based on the the provided architecture
  // for the values that depend on the arch.
  if (raw_type == 5) {
    if (Header::is_mips(arch)) {
      return BASE_TYPES::MIPS_JMPADDR;
    }

    if (Header::is_arm(arch)) {
      return BASE_TYPES::ARM_MOV32;
    }

    if (Header::is_riscv(arch)) {
      return BASE_TYPES::RISCV_HI20;
    }

    return BASE_TYPES::UNKNOWN;
  }

  if (raw_type == 7) {
    if (Header::is_thumb(arch)) {
      return BASE_TYPES::THUMB_MOV32;
    }

    if (Header::is_riscv(arch)) {
      return BASE_TYPES::RISCV_LOW12I;
    }

    return BASE_TYPES::UNKNOWN;
  }

  if (raw_type == 8) {
    if (Header::is_riscv(arch)) {
      return BASE_TYPES::RISCV_LOW12S;
    }
    if (Header::is_loonarch(arch)) {
      return BASE_TYPES::LOONARCH_MARK_LA;
    }
  }

  return BASE_TYPES(raw_type);
}

uint64_t RelocationEntry::address() const {
  if (relocation_ != nullptr) {
    return relocation_->virtual_address() + position();
  }

  return position();
}

void RelocationEntry::address(uint64_t address) {
  if (relocation_ == nullptr) {
    LIEF_ERR("parent relocation is not set");
    return;
  }
  int32_t delta = address - relocation_->virtual_address();
  if (delta < 0) {
    LIEF_ERR("Invalid address: 0x{:04x} (base address=0x{:04x})",
             address, relocation_->virtual_address());
    return;
  }

  if (delta > MAX_ADDR) {
    LIEF_ERR("0x{:04x} does not fit in a 12-bit encoding");
    return;
  }

  position_ = delta;
}

size_t RelocationEntry::size() const {
  switch (type()) {
    case BASE_TYPES::LOW:
    case BASE_TYPES::HIGH:
    case BASE_TYPES::HIGHADJ:
      return 16;

    case BASE_TYPES::HIGHLOW: // Addr += delta
      return 32;

    case BASE_TYPES::DIR64: // Addr += delta
      return 64;

    case BASE_TYPES::ABS:
    default:
      return 0;
  }
  return 0;
}

void RelocationEntry::size(size_t /*size*/) {
  LIEF_WARN("Setting size of a PE relocation is not supported!");
}

void RelocationEntry::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const RelocationEntry& entry) {
  os << fmt::format("0x{:04x} {:14} 0x{:08x}",
                    entry.data(), PE::to_string(entry.type()), entry.address());
  return os;
}

const char* to_string(RelocationEntry::BASE_TYPES type) {
  #define ENTRY(X) std::pair(RelocationEntry::BASE_TYPES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(ABS),
    ENTRY(HIGH),
    ENTRY(LOW),
    ENTRY(HIGHLOW),
    ENTRY(HIGHADJ),
    ENTRY(MIPS_JMPADDR),
    ENTRY(ARM_MOV32),
    ENTRY(RISCV_HI20),
    ENTRY(SECTION),
    ENTRY(THUMB_MOV32),
    ENTRY(RISCV_LOW12I),
    ENTRY(RISCV_LOW12S),
    ENTRY(MIPS_JMPADDR16),
    ENTRY(DIR64),
    ENTRY(HIGH3ADJ),
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}


}
}

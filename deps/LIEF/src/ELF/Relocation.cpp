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
#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/Relocation.hpp"
#include "LIEF/ELF/EnumToString.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/Binary.hpp"

#include "ELF/Structures.hpp"

#include "logging.hpp"

namespace LIEF {
namespace ELF {

int32_t get_reloc_size(Relocation::TYPE type);

template Relocation::Relocation(const details::Elf32_Rel&, PURPOSE, ENCODING, ARCH);
template Relocation::Relocation(const details::Elf32_Rela&, PURPOSE, ENCODING, ARCH);
template Relocation::Relocation(const details::Elf64_Rel&, PURPOSE, ENCODING, ARCH);
template Relocation::Relocation(const details::Elf64_Rela&, PURPOSE, ENCODING, ARCH);

Relocation::TYPE Relocation::type_from(uint32_t value, ARCH arch) {
  static std::set<ARCH> ERR;
  switch (arch) {
    case ARCH::X86_64:
      return TYPE(value | R_X64);
    case ARCH::AARCH64:
      return TYPE(value | R_AARCH64);
    case ARCH::I386:
      return TYPE(value | R_X86);
    case ARCH::ARM:
      return TYPE(value | R_ARM);
    case ARCH::HEXAGON:
      return TYPE(value | R_HEXAGON);
    case ARCH::LOONGARCH:
      return TYPE(value | R_LARCH);
    case ARCH::MIPS:
      return TYPE(value | R_MIPS);
    case ARCH::PPC:
      return TYPE(value | R_PPC);
    case ARCH::PPC64:
      return TYPE(value | R_PPC64);
    case ARCH::SPARC:
      return TYPE(value | R_SPARC);
    case ARCH::RISCV:
      return TYPE(value | R_RISCV);
    case ARCH::BPF:
      return TYPE(value | R_BPF);
    case ARCH::SH:
      return TYPE(value | R_SH4);
    case ARCH::S390:
      return TYPE(value | R_SYSZ);
    default:
      {
        if (ERR.insert(arch).second) {
          LIEF_ERR("LIEF does not support relocation for '{}'", to_string(arch));
        }
        return TYPE::UNKNOWN;
      }
  }
  return TYPE::UNKNOWN;
}

template<class T>
Relocation::Relocation(const T& header, PURPOSE purpose, ENCODING enc, ARCH arch) :
  LIEF::Relocation{header.r_offset, 0},
  encoding_{enc},
  architecture_{arch},
  purpose_{purpose}
{
  if constexpr (std::is_same_v<T, details::Elf32_Rel> ||
                std::is_same_v<T, details::Elf32_Rela>)
  {
    type_ = type_from(header.r_info & 0xff, arch);
    info_ = static_cast<uint32_t>(header.r_info >> 8);
  }

  if constexpr (std::is_same_v<T, details::Elf64_Rel> ||
                std::is_same_v<T, details::Elf64_Rela>)
  {
    type_ = type_from(header.r_info & 0xffffffff, arch);
    info_ = static_cast<uint32_t>(header.r_info >> 32);
  }

  if constexpr (std::is_same_v<T, details::Elf32_Rela> ||
                std::is_same_v<T, details::Elf64_Rela>)
  {
    addend_ = header.r_addend;
  }
}

Relocation::Relocation(uint64_t address, TYPE type, ENCODING encoding) :
  LIEF::Relocation(address, 0),
  type_(type),
  encoding_(encoding)
{
  if (type != TYPE::UNKNOWN) {
    auto raw_type = static_cast<uint64_t>(type);
    const uint64_t ID = (raw_type >> Relocation::R_BIT) << Relocation::R_BIT;
    if (ID == Relocation::R_X64) {
      architecture_ = ARCH::X86_64;
    }
    else if (ID == Relocation::R_AARCH64) {
      architecture_ = ARCH::AARCH64;
    }
    else if (ID == Relocation::R_ARM) {
      architecture_ = ARCH::ARM;
    }
    else if (ID == Relocation::R_HEXAGON) {
      architecture_ = ARCH::HEXAGON;
    }
    else if (ID == Relocation::R_X86) {
      architecture_ = ARCH::I386;
    }
    else if (ID == Relocation::R_LARCH) {
      architecture_ = ARCH::LOONGARCH;
    }
    else if (ID == Relocation::R_MIPS) {
      architecture_ = ARCH::MIPS;
    }
    else if (ID == Relocation::R_PPC) {
      architecture_ = ARCH::PPC;
    }
    else if (ID == Relocation::R_PPC64) {
      architecture_ = ARCH::PPC64;
    }
    else if (ID == Relocation::R_SPARC) {
      architecture_ = ARCH::SPARC;
    }
    else if (ID == Relocation::R_RISCV) {
      architecture_ = ARCH::RISCV;
    }
    else if (ID == Relocation::R_BPF) {
      architecture_ = ARCH::BPF;
    }
    else if (ID == Relocation::R_SH4) {
      architecture_ = ARCH::SH;
    }
    else if (ID == Relocation::R_SYSZ) {
      architecture_ = ARCH::S390;
    }
  }
}

result<uint64_t> Relocation::resolve(uint64_t base_address) const {
  int64_t A = this->addend();
  uint64_t S = 0;
  uint64_t P = this->address();
  const int32_t RSZ = size();
  if (const Symbol* sym = symbol()) {
    S = sym->value();
  }

  /* Memory based address */
  const uint64_t B = base_address;

  auto Q = [&] () -> int64_t {
    if (RSZ < 0) {
      LIEF_ERR("Missing size support for {}", to_string(type()));
      return 0;
    }

    switch (RSZ) {
      case sizeof(uint8_t):
        {
          auto V = binary_->get_int_from_virtual_address<uint8_t>(P);
          if (!V) {
            LIEF_ERR("{}: Can't access relocation data at offset: 0x{:06x} (8-bit)",
                     to_string(type()),P);
            return 0;
          }
          return (int64_t)((int8_t)*V);
        }

      case sizeof(uint16_t):
        {
          auto V = binary_->get_int_from_virtual_address<uint16_t>(P);
          if (!V) {
            LIEF_ERR("{}: Can't access relocation data at offset: 0x{:06x} (16-bit)",
                     to_string(type()),P);
            return 0;
          }
          return (int64_t)((int16_t)*V);
        }
      case sizeof(uint32_t):
        {
          auto V = binary_->get_int_from_virtual_address<uint32_t>(P);
          if (!V) {
            LIEF_ERR("{}: Can't access relocation data at offset: 0x{:06x} (32-bit)",
                     to_string(type()),P);
            return 0;
          }
          return (int64_t)((int32_t)*V);
        }
      case sizeof(uint64_t):
        {
          auto V = binary_->get_int_from_virtual_address<uint64_t>(P);
          if (!V) {
            LIEF_ERR("{}: Can't access relocation data at offset: 0x{:06x} (64-bit)",
                     to_string(type()),P);
            return 0;
          }
          return (int64_t)*V;
        }
      default:
        LIEF_ERR("Invalid size of {} (sz: {})", to_string(type()), RSZ);
        return 0;
    }
  };

  switch (type()) {
    /* X86_64 { */
      /* See ELF x86-64-ABI psABI[1] for the reference
       *
       * [1]: https://gitlab.com/x86-psABIs/x86-64-ABI
       */
      case TYPE::X86_64_NONE:
        return Q();
      case TYPE::X86_64_64:
      case TYPE::X86_64_DTPOFF32:
      case TYPE::X86_64_DTPOFF64:
        return S + A;
      case TYPE::X86_64_PC32:
      case TYPE::X86_64_PC64:
        return S + A - P;
      case TYPE::X86_64_32:
      case TYPE::X86_64_32S:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::X86_64_GLOB_DAT:
      case TYPE::X86_64_JUMP_SLOT:
        return S;
      case TYPE::X86_64_RELATIVE:
        return B + A;
      case TYPE::X86_64_RELATIVE64:
        return B + A;
    /* } */

    /* X86 { */
      case TYPE::X86_NONE:
        return Q();
      case TYPE::X86_32:
        return S + Q();
      case TYPE::X86_PC32:
        return S - P + Q();
      case TYPE::X86_GLOB_DAT:
      case TYPE::X86_JUMP_SLOT:
        return S;
      case TYPE::X86_RELATIVE:
        return B + Q();
    /* } */

    /* AArc64 { */
      case TYPE::AARCH64_ABS32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::AARCH64_ABS64:
        return S + A;
      case TYPE::AARCH64_PREL16:
        return (S + A - P) & 0xFFFF;
      case TYPE::AARCH64_PREL32:
        return (S + A - P) & 0xFFFFFFFF;
      case TYPE::AARCH64_PREL64:
        return S + A - P;
      case TYPE::AARCH64_GLOB_DAT:
      case TYPE::AARCH64_JUMP_SLOT:
        return S;
      case TYPE::AARCH64_RELATIVE:
        return B + A;
    /* } */

    /* ARM { */
      case TYPE::ARM_ABS32:
        return (S + Q() + A) & 0xFFFFFFFF;
      case TYPE::ARM_REL32:
        return (S + Q() + A - P) & 0xFFFFFFFF;
      case TYPE::ARM_GLOB_DAT:
      case TYPE::ARM_JUMP_SLOT:
        return S;
      case TYPE::ARM_RELATIVE:
        return B + Q();
    /* } */


    /* eBPF { */
      case TYPE::BPF_64_ABS32:
        return (S + Q()) & 0xFFFFFFFF;

      case TYPE::BPF_64_ABS64:
        return S + Q();
    /* } */

    /* MIPS { */
      case TYPE::MIPS_32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::MIPS_64:
        return S + A;
      case TYPE::MIPS_TLS_DTPREL64:
        return S + A - 0x8000;
      case TYPE::MIPS_PC32:
        return S + A - P;
    /* } */

    /* PPC64 { */
      case TYPE::PPC64_ADDR32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::PPC64_ADDR64:
        return S + A;
      case TYPE::PPC64_REL32:
        return (S + A - P) & 0xFFFFFFFF;
      case TYPE::PPC64_REL64:
        return S + A - P;
    /* } */

    /* PPC32 { */
      case TYPE::PPC_ADDR32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::PPC_REL32:
        return (S + A - P) & 0xFFFFFFFF;
    /* } */

    /* RISCV { */
      case TYPE::RISCV_NONE:
        return Q();
      case TYPE::RISCV_32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::RISCV_32_PCREL:
        return (S + A - P) & 0xFFFFFFFF;
      case TYPE::RISCV_64:
        return S + A;
      case TYPE::RISCV_SET6:
        return (A & 0xC0) | ((S + A) & 0x3F);
      case TYPE::RISCV_SUB6:
        return (A & 0xC0) | (((A & 0x3F) - (S + A)) & 0x3F);
      case TYPE::RISCV_SET8:
        return (S + A) & 0xFF;
      case TYPE::RISCV_ADD8:
        return (A + (S + A)) & 0xFF;
      case TYPE::RISCV_SUB8:
        return (A - (S + 1)) & 0xFF;
      case TYPE::RISCV_SET16:
        return (S + A) & 0xFFFF;
      case TYPE::RISCV_ADD16:
        return (A + (S + A)) & 0xFFFF;
      case TYPE::RISCV_SUB16:
        return (A - (S + A)) & 0xFFFF;
      case TYPE::RISCV_SET32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::RISCV_ADD32:
        return (A + (S + A)) & 0xFFFFFFFF;
      case TYPE::RISCV_SUB32:
        return (A - (S + A)) & 0xFFFFFFFF;
      case TYPE::RISCV_ADD64:
        return (A + (S + A));
      case TYPE::RISCV_SUB64:
        return (A - (S + A));
    /* } */

    /* LoongArch { */
      case TYPE::LARCH_NONE:
        return Q();
      case TYPE::LARCH_32:
        return (S + A) & 0xFFFFFFFF;
      case TYPE::LARCH_32_PCREL:
        return (S + A - P) & 0xFFFFFFFF;
      case TYPE::LARCH_64:
        return S + A;
      case TYPE::LARCH_ADD6:
        return (Q() & 0xC0) | ((Q() + S + A) & 0x3F);
      case TYPE::LARCH_SUB6:
        return (Q() & 0xC0) | ((Q() - (S + A)) & 0x3F);
      case TYPE::LARCH_ADD8:
        return (Q() + (S + A)) & 0xFF;
      case TYPE::LARCH_SUB8:
        return (Q() - (S + A)) & 0xFF;
      case TYPE::LARCH_ADD16:
        return (Q() + (S + A)) & 0xFFFF;
      case TYPE::LARCH_SUB16:
        return (Q() - (S + A)) & 0xFFFF;
      case TYPE::LARCH_ADD32:
        return (Q() + (S + A)) & 0xFFFFFFFF;
      case TYPE::LARCH_SUB32:
        return (Q() - (S + A)) & 0xFFFFFFFF;
      case TYPE::LARCH_ADD64:
        return (Q() + (S + A));
      case TYPE::LARCH_SUB64:
        return (Q() - (S + A));
    /* } */
    default:
      break;
  }

  LIEF_DEBUG("Relocation {} is not supported", to_string(type()));
  return make_error_code(lief_errors::not_supported);
}

size_t Relocation::size() const {
  return get_reloc_size(type_);
}

void Relocation::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Relocation& entry) {
  std::string symbol_name;

  if (const Symbol* symbol = entry.symbol()) {
    symbol_name = symbol->demangled_name();
    if (symbol_name.empty()) {
      symbol_name = symbol->name();
    }
  }
  if (entry.size() == uint64_t(-1)) {
    os << fmt::format("0x{:06x} {} 0x{:04x} 0x{:02x} {}",
                      entry.address(), to_string(entry.type()),
                      entry.addend(), entry.info(), symbol_name);
    return os;
  }

  os << fmt::format("0x{:06x} {} ({}) 0x{:04x} 0x{:02x} {}",
                    entry.address(), to_string(entry.type()),
                    entry.size(), entry.addend(), entry.info(), symbol_name);
  return os;
}
}
}

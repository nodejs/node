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
#include <utility>

#ifdef __unix__
  #include <cxxabi.h>
#endif
#include "LIEF/utils.hpp"
#include "LIEF/config.h"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/SymbolVersion.hpp"
#include "LIEF/Visitor.hpp"
#include "ELF/Structures.hpp"

#include "frozen.hpp"
#include <spdlog/fmt/fmt.h>


namespace LIEF {
namespace ELF {

Symbol& Symbol::operator=(Symbol other) {
  swap(other);
  return *this;
}

Symbol::Symbol(const Symbol& other) : LIEF::Symbol{other},
  type_{other.type_},
  binding_{other.binding_},
  other_{other.other_},
  shndx_{other.shndx_},
  arch_{other.arch_}
{}


void Symbol::swap(Symbol& other) {
  LIEF::Symbol::swap(other);
  std::swap(type_,           other.type_);
  std::swap(binding_,        other.binding_);
  std::swap(other_,          other.other_);
  std::swap(shndx_,          other.shndx_);
  std::swap(section_,        other.section_);
  std::swap(symbol_version_, other.symbol_version_);
  std::swap(arch_,           other.arch_);
}

template<class T>
Symbol::Symbol(const T& header, ARCH arch) :
  type_{type_from(header.st_info & 0x0f, arch)},
  binding_{binding_from(header.st_info >> 4, arch)},
  other_{header.st_other},
  shndx_{header.st_shndx},
  arch_{arch}
{
  value_ = header.st_value;
  size_  = header.st_size;
}

template Symbol::Symbol(const details::Elf32_Sym& header, ARCH);
template Symbol::Symbol(const details::Elf64_Sym& header, ARCH);


uint8_t Symbol::information() const {
  return static_cast<uint8_t>(
      (static_cast<uint8_t>(binding_) << 4) | (static_cast<uint8_t>(type_) & 0x0f)
  );
}

void Symbol::information(uint8_t info) {
  binding_ = binding_from(info >> 4, arch_);
  type_    = type_from(info & 0x0f, arch_);
}

std::string Symbol::demangled_name() const {
  if constexpr (lief_extended) {
    return LIEF::demangle(name()).value_or("");
  } else {
#if defined(__unix__)
    int status;
    const std::string& name = this->name().c_str();
    char* demangled_name = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);

    if (status == 0) {
      std::string realname = demangled_name;
      free(demangled_name);
      return realname;
    }

    return name;
#else
    return "";
#endif
  }
}

bool Symbol::is_exported() const {
  bool is_exported = shndx() != SECTION_INDEX::UNDEF;

  // An export must have an address
  is_exported = is_exported && (value() != 0 || (value() == 0 && size() > 0));

  // An export must be bind to GLOBAL or WEAK
  is_exported = is_exported && (binding() == BINDING::GLOBAL ||
                                binding() == BINDING::WEAK);

  // An export must have one of theses types:
  is_exported = is_exported && (type() == TYPE::FUNC ||
                                type() == TYPE::GNU_IFUNC ||
                                type() == TYPE::OBJECT);
  return is_exported;
}

void Symbol::set_exported(bool flag) {
  if (flag) {
    shndx(1);
    binding(BINDING::GLOBAL);
  } else {
    shndx(SECTION_INDEX::UNDEF);
    binding(BINDING::LOCAL);
  }
}

bool Symbol::is_imported() const {
  // An import must not be defined in a section
  bool is_imported = shndx() == SECTION_INDEX::UNDEF;

  const bool is_mips = arch_ == ARCH::MIPS || arch_ == ARCH::MIPS_X ||
                       arch_ == ARCH::MIPS_RS3_LE;
  const bool is_ppc = arch_ == ARCH::PPC || arch_ == ARCH::PPC64;

  const bool is_riscv = arch_ == ARCH::RISCV;

  // An import must not have an address (except for some architectures like Mips/PPC)
  if (!is_mips && !is_ppc && !is_riscv) {
    is_imported = is_imported && value() == 0;
  }

  // its name must not be empty
  is_imported = is_imported && !name().empty();

  // It must have a GLOBAL or WEAK bind
  is_imported = is_imported && (binding()  == BINDING::GLOBAL ||
                                 binding() == BINDING::WEAK);

  // It must be a FUNC or an OBJECT
  is_imported = is_imported && (type()  == TYPE::FUNC ||
                                 type() == TYPE::GNU_IFUNC ||
                                 type() == TYPE::OBJECT);
  return is_imported;
}

void Symbol::set_imported(bool flag) {
  if (flag) {
    shndx(SECTION_INDEX::UNDEF);
  } else {
    shndx(1);
  }
}

void Symbol::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Symbol& entry) {
  std::string name = entry.demangled_name();
  if (name.empty()) {
    name = entry.name();
  }

  os << fmt::format("{} ({}/{}): 0x{:06x} (0x{:02x})",
                    name, to_string(entry.type()), to_string(entry.binding()),
                    entry.value(), entry.size());

  if (const SymbolVersion* version = entry.symbol_version()) {
    os << *version;
  }
  return os;
}

const char* to_string(Symbol::BINDING e) {
  #define ENTRY(X) std::pair(Symbol::BINDING::X, #X)
  STRING_MAP enums2str {
    ENTRY(LOCAL),
    ENTRY(GLOBAL),
    ENTRY(WEAK),
    ENTRY(GNU_UNIQUE),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
const char* to_string(Symbol::TYPE e) {
  #define ENTRY(X) std::pair(Symbol::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(NOTYPE),
    ENTRY(OBJECT),
    ENTRY(FUNC),
    ENTRY(SECTION),
    ENTRY(FILE),
    ENTRY(COMMON),
    ENTRY(TLS),
    ENTRY(GNU_IFUNC),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
const char* to_string(Symbol::VISIBILITY e) {
  #define ENTRY(X) std::pair(Symbol::VISIBILITY::X, #X)
  STRING_MAP enums2str {
    ENTRY(DEFAULT),
    ENTRY(INTERNAL),
    ENTRY(HIDDEN),
    ENTRY(PROTECTED),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}

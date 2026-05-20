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
#include "LIEF/Abstract/Binary.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/config.h"

#include "logging.hpp"
#include "frozen.hpp"
#include "paging.hpp"

#include "LIEF/Abstract/Section.hpp"
#include "LIEF/Abstract/Symbol.hpp"
#include "LIEF/Abstract/DebugInfo.hpp"

#include "LIEF/asm/Engine.hpp"
#include "LIEF/asm/Instruction.hpp"

namespace LIEF {

Binary::Binary(FORMATS fmt) :
  format_{fmt}
{}

Binary::Binary() = default;
Binary::~Binary() = default;

const Symbol* Binary::get_symbol(const std::string& name) const {
  symbols_t symbols = const_cast<Binary*>(this)->get_abstract_symbols();
  const auto it_symbol = std::find_if(
    std::begin(symbols), std::end(symbols),
    [&name] (const Symbol* s) { return s->name() == name; }
  );

  if (it_symbol == std::end(symbols)) {
    return nullptr;
  }

  return *it_symbol;
}

result<uint64_t> Binary::get_function_address(const std::string& name) const {
  if constexpr (lief_extended) {
    if (const DebugInfo* dbg = debug_info()) {
      if (auto addr = dbg->find_function_address(name)) {
        return *addr;
      }
    }
  }
  return make_error_code(lief_errors::not_found);
}

std::vector<uint64_t> Binary::xref(uint64_t address) const {
  std::vector<uint64_t> result;

  for (Section* section : const_cast<Binary*>(this)->get_abstract_sections()) {
    std::vector<size_t> founds = section->search_all(address);
    for (size_t found : founds) {
      result.push_back(section->virtual_address() + found);
    }
  }

  return result;
}

uint64_t Binary::page_size() const {
  return get_pagesize(*this);
}


void Binary::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

const char* to_string(Binary::VA_TYPES e) {
  #define ENTRY(X) std::pair(Binary::VA_TYPES::X, #X)
  STRING_MAP enums2str {
    ENTRY(AUTO),
    ENTRY(RVA),
    ENTRY(VA),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

const char* to_string(Binary::FORMATS e) {
  #define ENTRY(X) std::pair(Binary::FORMATS::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(ELF),
    ENTRY(PE),
    ENTRY(MACHO),
    ENTRY(OAT),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}
}

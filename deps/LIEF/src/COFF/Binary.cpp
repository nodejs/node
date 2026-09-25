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
#include "LIEF/COFF/Binary.hpp"
#include "LIEF/COFF/Header.hpp"
#include "LIEF/COFF/Section.hpp"
#include "LIEF/COFF/Symbol.hpp"
#include "LIEF/COFF/Relocation.hpp"

#include "LIEF/asm/Engine.hpp"
#include "LIEF/asm/Instruction.hpp"

#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>
#include <sstream>

namespace LIEF::COFF {
Binary::Binary() = default;
Binary::~Binary() = default;


Binary::it_const_function Binary::functions() const {
  return {symbols_, [] (const std::unique_ptr<Symbol>& sym) {
    return sym->is_function();
    }
  };
}

Binary::it_functions Binary::functions() {
  return {symbols_, [] (const std::unique_ptr<Symbol>& sym) {
    return sym->is_function();
    }
  };
}

const Symbol* Binary::find_function(const std::string& name) const {
  for (const Symbol& func : functions()) {
    if (func.name() == name) {
      return &func;
    }
  }
  return nullptr;
}

const Symbol* Binary::find_demangled_function(const std::string& name) const {
  for (const Symbol& func : functions()) {
    if (func.demangled_name() == name) {
      return &func;
    }
  }
  return nullptr;
}

std::string Binary::to_string() const {
  using namespace fmt;
  std::ostringstream oss;

  oss << header() << '\n';

  {
    const auto secs = sections();
    for (size_t i = 0; i < secs.size(); ++i) {
      oss << fmt::format("Section #{:02} {{\n", i)
         << indent(LIEF::to_string(secs[i]), 2)
         << "}\n";
    }
  }

  if (auto syms = symbols(); !syms.empty()) {
    oss << fmt::format("Symbols (#{})\n", syms.size());
    for (size_t i = 0; i < syms.size(); ++i) {
      oss << fmt::format("Symbol[{:02d}] {{\n", i)
         << indent(LIEF::to_string(syms[i]), 2)
         << "}\n";
    }
  }

  if (auto relocs = relocations(); !relocs.empty()) {
    oss << fmt::format("Relocations (#{})\n", relocs.size());
    for (size_t i = 0; i < relocs.size(); ++i) {
      oss << "  " << relocs[i] << '\n';
    }
  }


  return oss.str();
}
}

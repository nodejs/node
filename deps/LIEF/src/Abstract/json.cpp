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
#include "LIEF/Abstract.hpp"
#include "Abstract/json_internal.hpp"

#include "LIEF/ELF.hpp"
#include "LIEF/PE.hpp"
#include "LIEF/MachO.hpp"
#include "LIEF/config.h"

#include "Object.tcc"

namespace LIEF {

void AbstractJsonVisitor::visit(const Binary& binary) {
  AbstractJsonVisitor header_visitor;
  header_visitor(binary.header());

  // Sections
  std::vector<json> sections;
  for (const Section& section : binary.sections()) {
    AbstractJsonVisitor visitor;
    visitor.visit(section);
    sections.emplace_back(visitor.get());
  }

  std::vector<json> symbols;
  for (const Symbol& sym : binary.symbols()) {
    AbstractJsonVisitor visitor;
    visitor.visit(sym);
    symbols.emplace_back(visitor.get());
  }

  std::vector<json> relocations;
  for (const Relocation& relocation : binary.relocations()) {
    AbstractJsonVisitor visitor;
    visitor.visit(relocation);
    relocations.emplace_back(visitor.get());
  }


  std::vector<json> imports;
  for (const Function& f : binary.imported_functions()) {
    AbstractJsonVisitor visitor;
    visitor.visit(f);
    relocations.emplace_back(visitor.get());
  }

  std::vector<json> exports;
  for (const Function& f : binary.exported_functions()) {
    AbstractJsonVisitor visitor;
    visitor.visit(f);
    relocations.emplace_back(visitor.get());
  }


  node_["entrypoint"]         = binary.entrypoint();
  node_["format"]             = to_string(binary.format());
  node_["original_size"]      = binary.original_size();
  node_["exported_functions"] = exports;
  node_["imported_libraries"] = binary.imported_libraries();
  node_["imported_functions"] = imports;
  node_["header"]             = header_visitor.get();
  node_["sections"]           = sections;
  node_["symbols"]            = symbols;
  node_["relocations"]        = relocations;
}


void AbstractJsonVisitor::visit(const Header& header) {
  node_["architecture"] = to_string(header.architecture());
  node_["object_type"]  = to_string(header.object_type());
  node_["entrypoint"]   = header.entrypoint();
  node_["endianness"]   = to_string(header.endianness());
}

void AbstractJsonVisitor::visit(const Section& section) {
  node_["name"]            = section.name();
  node_["size"]            = section.size();
  node_["offset"]          = section.offset();
  node_["virtual_address"] = section.virtual_address();
}

void AbstractJsonVisitor::visit(const Symbol& symbol) {
  node_["name"]  = symbol.name();
  node_["value"] = symbol.value();
  node_["size"]  = symbol.size();
}

void AbstractJsonVisitor::visit(const Relocation& relocation) {
  node_["address"] = relocation.address();
  node_["size"]    = relocation.size();
}


void AbstractJsonVisitor::visit(const Function& function) {

  std::vector<std::string> flags_str;
  std::vector<Function::FLAGS> flags = function.flags_list();
  flags_str.reserve(flags.size());
  for (Function::FLAGS f : flags) {
    flags_str.emplace_back(to_string(f));
  }

  node_["address"] = function.address();
  node_["size"]    = function.size();
  node_["name"]    = function.name();
  node_["flags"]   = flags_str;

}




} // namespace LIEF

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

#include "LIEF/Abstract/hash.hpp"
#include "LIEF/Abstract.hpp"
#include "Object.tcc"

namespace LIEF {

AbstractHash::~AbstractHash() = default;

size_t AbstractHash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::AbstractHash>(obj);
}


void AbstractHash::visit(const Binary& binary) {
  process(binary.format());
  process(binary.header());
  process(std::begin(binary.symbols()), std::end(binary.symbols()));
  process(std::begin(binary.sections()), std::end(binary.sections()));
  process(std::begin(binary.relocations()), std::end(binary.relocations()));
}

void AbstractHash::visit(const Header& header) {
  process(header.architecture());
  process(header.modes());
  process(header.object_type());
  process(header.entrypoint());
  process(header.endianness());
}

void AbstractHash::visit(const Section& section) {
  process(section.name());
  process(section.offset());
  process(section.size());
  process(section.virtual_address());
}

void AbstractHash::visit(const Symbol& symbol) {
  process(symbol.name());
  process(symbol.value());
  process(symbol.size());
}

void AbstractHash::visit(const Relocation& relocation) {
  process(relocation.address());
  process(relocation.size());
}

void AbstractHash::visit(const Function& function) {
  visit(*function.as<LIEF::Symbol>());
  process(function.flags());
}


} // namespace LIEF


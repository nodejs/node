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

#include "LIEF/DEX/hash.hpp"

#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/Field.hpp"
#include "LIEF/DEX/File.hpp"
#include "LIEF/DEX/Header.hpp"
#include "LIEF/DEX/MapItem.hpp"
#include "LIEF/DEX/MapList.hpp"
#include "LIEF/DEX/Method.hpp"
#include "LIEF/DEX/Prototype.hpp"

namespace LIEF {
namespace DEX {

Hash::~Hash() = default;

size_t Hash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::DEX::Hash>(obj);
}


void Hash::visit(const File& file) {
  process(file.location());
  process(file.header());

  process(std::begin(file.classes()), std::end(file.classes()));
  process(std::begin(file.methods()), std::end(file.methods()));
  process(std::begin(file.strings()), std::end(file.strings()));

}

void Hash::visit(const Header& header) {
  process(header.magic());
  process(header.checksum());
  process(header.signature());
  process(header.file_size());
  process(header.header_size());
  process(header.endian_tag());
  process(header.strings());
  process(header.link());
  process(header.types());
  process(header.prototypes());
  process(header.fields());
  process(header.methods());
  process(header.classes());
  process(header.data());
}

void Hash::visit(const CodeInfo& /*code_info*/) {
}

void Hash::visit(const Class& cls) {

  Class::it_const_fields fields = cls.fields();
  Class::it_const_methods methods = cls.methods();
  process(cls.fullname());
  process(cls.source_filename());
  process(cls.access_flags());

  process(std::begin(fields), std::end(fields));
  process(std::begin(methods), std::end(methods));
}

void Hash::visit(const Field& field) {
  process(field.name());
  if (const Type* t = field.type()) {
    process(*t);
  }
}

void Hash::visit(const Method& method) {
  process(method.name());
  process(method.bytecode());
  if (const auto* p = method.prototype()) {
    process(*p);
  }
}


void Hash::visit(const Type& type) {
  switch (type.type()) {
    case Type::TYPES::ARRAY:
      {
        process(type.dim());
        process(type.underlying_array_type());
        break;
      }

    case Type::TYPES::PRIMITIVE:
      {
        process(type.primitive());
        break;
      }

    case Type::TYPES::CLASS:
      {
        process(type.cls().fullname());
        break;
      }

    case Type::TYPES::UNKNOWN:
    default:
      {
        process(Type::TYPES::UNKNOWN);
      }
  }
}

void Hash::visit(const Prototype& type) {
  if (const auto* rty = type.return_type()) {
    process(*rty);
  }
  process(std::begin(type.parameters_type()),
          std::end(type.parameters_type()));
}

void Hash::visit(const MapItem& item) {
  process(item.size());
  process(item.offset());
  process(item.reserved());
  process(item.type());
}

void Hash::visit(const MapList& list) {
  process(std::begin(list.items()), std::end(list.items()));
}


} // namespace DEX
} // namespace LIEF


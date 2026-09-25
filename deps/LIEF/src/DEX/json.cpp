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

#include "DEX/json_internal.hpp"
#include "LIEF/DEX.hpp"

namespace LIEF {
namespace DEX {

json to_json_obj(const Object& v) {
  JsonVisitor visitor;
  visitor(v);
  return visitor.get();
}


void JsonVisitor::visit(const File& file) {
  JsonVisitor header_visitor;
  header_visitor(file.header());

  JsonVisitor map_item_visitor;
  map_item_visitor(file.map());

  // Classes
  std::vector<json> classes;
  for (const Class& cls : file.classes()) {
    JsonVisitor clsvisitor;
    clsvisitor(cls);
    classes.emplace_back(clsvisitor.get());
  }

  node_["header"]  = header_visitor.get();
  node_["classes"] = classes;
  node_["map"]     = map_item_visitor.get();
}

void JsonVisitor::visit(const Header& header) {
  node_["magic"]       = header.magic();
  node_["checksum"]    = header.checksum();
  node_["signature"]   = header.signature();
  node_["file_size"]   = header.file_size();
  node_["header_size"] = header.header_size();
  node_["endian_tag"]  = header.endian_tag();
  node_["map"]         = header.map();
  node_["strings"]     = header.strings();
  node_["link"]        = header.link();
  node_["types"]       = header.types();
  node_["prototypes"]  = header.prototypes();
  node_["fields"]      = header.fields();
  node_["methods"]     = header.methods();
  node_["classes"]     = header.classes();
  node_["data"]        = header.data();
}

void JsonVisitor::visit(const CodeInfo& /*code_info*/) {
}

void JsonVisitor::visit(const Class& cls) {
  std::vector<json> flags;
  for (ACCESS_FLAGS f : cls.access_flags()) {
    flags.emplace_back(to_string(f));
  }

  std::vector<json> fields;
  for (const Field& f : cls.fields()) {
    JsonVisitor fv;
    fv(f);
    fields.emplace_back(fv.get());
  }

  std::vector<json> methods;
  for (const Method& m : cls.methods()) {
    JsonVisitor mv;
    mv(m);
    methods.emplace_back(mv.get());
  }
  node_["fullname"]         = cls.fullname();
  node_["source_filename"]  = cls.source_filename();
  node_["access_flags"]     = flags;
  node_["index"]            = cls.index();
  node_["fields"]           = fields;
  node_["methods"]          = methods;

  if (const auto* parent = cls.parent()) {
    node_["parent"] = parent->fullname();
  }
}

void JsonVisitor::visit(const Field& field) {
  std::vector<json> flags;
  for (ACCESS_FLAGS f : field.access_flags()) {
    flags.emplace_back(to_string(f));
  }

  JsonVisitor type_visitor;
  type_visitor(*field.type());

  node_["name"]         = field.name();
  node_["index"]        = field.index();
  node_["is_static"]    = field.is_static();
  node_["type"]         = type_visitor.get();
  node_["access_flags"] = flags;
}

void JsonVisitor::visit(const Method& method) {
  std::vector<json> flags;
  for (ACCESS_FLAGS f : method.access_flags()) {
    flags.emplace_back(to_string(f));
  }

  JsonVisitor proto_visitor;
  if (const Prototype* p = method.prototype()) {
    proto_visitor(*p);
  }

  node_["name"]         = method.name();
  node_["code_offset"]  = method.code_offset();
  node_["index"]        = method.index();
  node_["is_virtual"]   = method.is_virtual();
  node_["prototype"]    = proto_visitor.get();
  node_["access_flags"] = flags;
}

void JsonVisitor::visit(const Type& type) {

  node_["type"] = to_string(type.type());
  switch(type.type()) {
    case Type::TYPES::CLASS:
      {
        node_["value"] = type.cls().fullname();
        break;
      }

    case Type::TYPES::PRIMITIVE:
      {
        node_["value"] = Type::pretty_name(type.primitive());
        break;
      }

    case Type::TYPES::ARRAY:
      {
        const Type& uderlying_t = type.underlying_array_type();
        node_["dim"] = type.dim();

        if (uderlying_t.type() == Type::TYPES::CLASS) {
          node_["value"] = uderlying_t.cls().fullname();
          break;
        }

        if (uderlying_t.type() == Type::TYPES::PRIMITIVE) {
          node_["value"] = Type::pretty_name(type.primitive());
          break;
        }
        break;
      }
    default:
      {}
  }
}

void JsonVisitor::visit(const Prototype& type) {
  JsonVisitor rtype_visitor;
  if (const Type* rty = type.return_type()) {
    rtype_visitor(*rty);
  }

  std::vector<json> params;
  for (const Type& t : type.parameters_type()) {
    JsonVisitor pvisitor;
    pvisitor(t);
    params.emplace_back(pvisitor.get());

  }

  node_["return_type"] = rtype_visitor.get();
  node_["parameters"]  = params;
}

void JsonVisitor::visit(const MapItem& item) {
  node_["offset"] = item.offset();
  node_["size"]   = item.size();
  node_["type"]   = to_string(item.type());

}

void JsonVisitor::visit(const MapList& list) {
  std::vector<json> items;
  for (const MapItem& i : list.items()) {
    JsonVisitor itemvisitor;
    itemvisitor(i);
    items.emplace_back(itemvisitor.get());
  }
  node_["map_items"] = items;
}



} // namespace DEX
} // namespace LIEF


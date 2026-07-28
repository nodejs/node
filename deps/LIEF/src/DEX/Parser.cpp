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

#include <memory>

#include "logging.hpp"

#include <LIEF/BinaryStream/VectorStream.hpp>

#include "LIEF/DEX/Parser.hpp"
#include "LIEF/DEX/File.hpp"
#include "LIEF/DEX/Type.hpp"
#include "LIEF/DEX/utils.hpp"
#include "DEX/Structures.hpp"

#include "Parser.tcc"

namespace LIEF {
namespace DEX {

Parser::~Parser() = default;
Parser::Parser()  = default;

std::unique_ptr<File> Parser::parse(const std::string& filename) {
  if (!is_dex(filename)) {
    LIEF_ERR("'{}' is not a DEX File", filename);
    return nullptr;
  }
  Parser parser{filename};
  dex_version_t version = DEX::version(filename);
  parser.init(filename, version);
  return std::move(parser.file_);
}

std::unique_ptr<File> Parser::parse(std::vector<uint8_t> data, const std::string& name) {
  if (!is_dex(data)) {
    LIEF_ERR("'{}' is not a DEX File", name);
    return nullptr;
  }
  dex_version_t version = DEX::version(data);

  Parser parser{std::move(data)};
  parser.init(name, version);
  return std::move(parser.file_);
}


Parser::Parser(std::vector<uint8_t> data) :
  file_{new File{}},
  stream_{std::make_unique<VectorStream>(std::move(data))}
{}

Parser::Parser(const std::string& file) :
  file_{new File{}}
{
  auto stream = VectorStream::from_file(file);
  if (!stream) {
    LIEF_ERR("Can't create the stream");
  } else {
    stream_ = std::make_unique<VectorStream>(std::move(*stream));
  }
}


void Parser::init(const std::string& name, dex_version_t version) {
  LIEF_DEBUG("Parsing file: {}", name);

  if (version == details::DEX_35::dex_version) {
    return parse_file<details::DEX35>();
  }

  if (version == details::DEX_37::dex_version) {
    return parse_file<details::DEX37>();
  }

  if (version == details::DEX_38::dex_version) {
    return parse_file<details::DEX38>();
  }

  if (version == details::DEX_39::dex_version) {
    return parse_file<details::DEX39>();
  }
}

void Parser::resolve_inheritance() {
  LIEF_DEBUG("Resolving inheritance relationship for #{:d} classes", inheritance_.size());

  for (const std::pair<const std::string, Class*>& p : inheritance_) {
    const std::string& parent_name = p.first;
    Class* child = p.second;

    const auto it_inner_class = file_->classes_.find(parent_name);
    if (it_inner_class == std::end(file_->classes_)) {
      auto external_class = std::make_unique<Class>(parent_name);
      child->parent_ = external_class.get();
      file_->add_class(std::move(external_class));
    } else {
      child->parent_ = it_inner_class->second;
    }
  }
}

void Parser::resolve_external_methods() {
  LIEF_DEBUG("Resolving external methods for #{:d} methods", class_method_map_.size());

  for (const std::pair<const std::string, Method*>& p : class_method_map_) {
    const std::string& clazz = p.first;
    Method* method = p.second;

    const auto it_inner_class = file_->classes_.find(clazz);
    if (it_inner_class == std::end(file_->classes_)) {
      auto cls = std::make_unique<Class>(clazz);
      cls->methods_.push_back(method);
      method->parent_ = cls.get();
      file_->add_class(std::move(cls));
    } else {
      Class* cls = it_inner_class->second;
      method->parent_ = cls;
      cls->methods_.push_back(method);
    }

  }
}

void Parser::resolve_external_fields() {
  LIEF_DEBUG("Resolving external fields for #{:d} fields", class_field_map_.size());

  for (const std::pair<const std::string, Field*>& p : class_field_map_) {
    const std::string& clazz = p.first;
    Field* field = p.second;

    const auto it_inner_class = file_->classes_.find(clazz);
    if (it_inner_class == std::end(file_->classes_)) {
      auto cls = std::make_unique<Class>(clazz);
      cls->fields_.push_back(field);
      field->parent_ = cls.get();
      file_->add_class(std::move(cls));
    } else {
      Class* cls = it_inner_class->second;
      field->parent_ = cls;
      cls->fields_.push_back(field);
    }

  }
}

void Parser::resolve_types() {
  for (const auto& p : class_type_map_) {
    if(Class* cls = file_->get_class(p.first)) {
      p.second->underlying_array_type().cls_ = cls;
    } else {
      auto new_cls = std::make_unique<Class>(p.first);
      p.second->underlying_array_type().cls_ = new_cls.get();
      file_->add_class(std::move(new_cls));
    }
  }
}



} // namespace DEX
} // namespace LIEF

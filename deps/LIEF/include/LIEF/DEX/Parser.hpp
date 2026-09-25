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
#ifndef LIEF_DEX_PARSER_H
#define LIEF_DEX_PARSER_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "LIEF/visibility.h"
#include "LIEF/DEX/types.hpp"

namespace LIEF {
class VectorStream;

namespace DEX {
class Class;
class Method;
class Field;
class File;
class Type;

/// Class which parses a DEX file to produce a DEX::File object
class LIEF_API Parser {
  public:

  /// Parse the DEX file from the file path given in parameter
  static std::unique_ptr<File> parse(const std::string& file);
  static std::unique_ptr<File> parse(std::vector<uint8_t> data, const std::string& name = "");

  Parser& operator=(const Parser& copy) = delete;
  Parser(const Parser& copy)            = delete;

  private:
  Parser();
  Parser(const std::string& file);
  Parser(std::vector<uint8_t> data);
  ~Parser();

  void init(const std::string& name, dex_version_t version);

  template<typename DEX_T>
  void parse_file();

  template<typename DEX_T>
  void parse_header();

  template<typename DEX_T>
  void parse_map();

  template<typename DEX_T>
  void parse_strings();

  template<typename DEX_T>
  void parse_types();

  template<typename DEX_T>
  void parse_fields();

  template<typename DEX_T>
  void parse_prototypes();

  template<typename DEX_T>
  void parse_methods();

  template<typename DEX_T>
  void parse_classes();

  template<typename DEX_T>
  void parse_class_data(uint32_t offset, Class& cls);

  template<typename DEX_T>
  void parse_field(size_t index, Class& cls, bool is_static);

  template<typename DEX_T>
  void parse_method(size_t index, Class& cls, bool is_virtual);

  template<typename DEX_T>
  void parse_code_info(uint32_t offset, Method& method);

  void resolve_inheritance();

  void resolve_external_methods();

  void resolve_external_fields();

  void resolve_types();

  std::unique_ptr<File> file_;

  // Map of inheritance relationship when parsing classes ('parse_classes')
  // The key is the parent class name of the value
  std::unordered_multimap<std::string, Class*> inheritance_;

  // Map of method/class relationship when parsing methods ('parse_methods')
  // The key is the Class name in which the method is defined
  std::unordered_multimap<std::string, Method*> class_method_map_;

  // Map of field/class relationship when parsing fields ('parse_fields')
  // The key is the Class name in which the field is defined
  std::unordered_multimap<std::string, Field*> class_field_map_;

  std::unordered_multimap<std::string, Type*> class_type_map_;

  std::unique_ptr<VectorStream> stream_;
};

} // namespace DEX
} // namespace LIEF
#endif

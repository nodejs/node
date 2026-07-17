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
#ifndef LIEF_OAT_PARSER_H
#define LIEF_OAT_PARSER_H
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/ELF/Parser.hpp"

namespace LIEF {

namespace VDEX {
class File;
}

namespace DEX {
class Class;
}

namespace OAT {
class Binary;
class Class;

/// Class to parse an OAT file to produce an OAT::Binary
class LIEF_API Parser : public ELF::Parser {
  public:
  /// Parse an OAT file
  static std::unique_ptr<Binary> parse(const std::string& oat_file);
  static std::unique_ptr<Binary> parse(const std::string& oat_file,
                                       const std::string& vdex_file);

  static std::unique_ptr<Binary> parse(std::vector<uint8_t> data);

  Parser& operator=(const Parser& copy) = delete;
  Parser(const Parser& copy)            = delete;

  protected:
  Parser();
  Parser(const std::string& oat_file);
  Parser(std::vector<uint8_t> data);
  ~Parser() override;

  Binary& oat_binary() {
    // The type of the parent binary_ is guaranteed by the constructor
    return *reinterpret_cast<Binary*>(binary_.get());
  }

  bool has_vdex() const;
  void set_vdex(std::unique_ptr<VDEX::File> file);

  template<typename OAT_T>
  void parse_binary();

  template<typename OAT_T>
  void parse_header();

  template<typename OAT_T>
  void parse_header_keys();

  template<typename OAT_T>
  void parse_dex_files();

  template<typename OAT_T>
  void parse_type_lookup_table();

  template<typename OAT_T>
  void parse_oat_classes();

  template<typename OAT_T>
  void parse_oat_methods(uint64_t methods_offsets, Class& clazz, const DEX::Class& dex_class);

  void init();

  std::unique_ptr<LIEF::VDEX::File> vdex_file_;

  uint64_t data_address_ = 0;
  uint64_t data_size_ = 0;

  uint64_t exec_start_ = 0;
  uint64_t exec_size_ = 0;
};

} // namespace OAT
} // namespace LIEF
#endif

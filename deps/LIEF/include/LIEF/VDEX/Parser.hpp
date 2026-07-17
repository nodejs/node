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
#ifndef LIEF_VDEX_PARSER_H
#define LIEF_VDEX_PARSER_H

#include <memory>
#include <vector>
#include <string>

#include "LIEF/VDEX/type_traits.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class VectorStream;
namespace VDEX {
class File;

/// Class which parse an VDEX file and transform into a VDEX::File object
class LIEF_API Parser {
  public:
  static std::unique_ptr<File> parse(const std::string& file);
  static std::unique_ptr<File> parse(const std::vector<uint8_t>& data,
                                     const std::string& name = "");

  Parser& operator=(const Parser& copy) = delete;
  Parser(const Parser& copy)            = delete;

  private:
  Parser();
  Parser(const std::string& file);
  Parser(const std::vector<uint8_t>& data, const std::string& name);
  virtual ~Parser();

  void init(const std::string& name, vdex_version_t version);

  template<typename VDEX_T>
  void parse_file();

  template<typename VDEX_T>
  void parse_header();

  template<typename VDEX_T>
  void parse_checksums();

  template<typename VDEX_T>
  void parse_dex_files();

  template<typename VDEX_T>
  void parse_verifier_deps();

  template<typename VDEX_T>
  void parse_quickening_info();

  LIEF::VDEX::File* file_ = nullptr;
  std::unique_ptr<VectorStream> stream_;
};

} // namespace VDEX
} // namespace LIEF
#endif

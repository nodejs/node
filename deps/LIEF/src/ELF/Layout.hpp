/* Copyright 2021 - 2025 R. Thomas
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
#ifndef LIEF_ELF_LAYOUT_H
#define LIEF_ELF_LAYOUT_H

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include "LIEF/ELF/Builder.hpp"

namespace LIEF {
namespace ELF {
class Section;
class Binary;
class Layout {
  public:
  Layout(Binary& bin, bool should_swap, const Builder::config_t& config) :
    binary_(&bin),
    should_swap_(should_swap),
    config_(&config)
  {}

  virtual const std::unordered_map<std::string, size_t>& shstr_map() const {
    return shstr_name_map_;
  }

  virtual const std::unordered_map<std::string, size_t>& strtab_map() const {
    return strtab_name_map_;
  }

  virtual const std::vector<uint8_t>& raw_shstr() const {
    return raw_shstrtab_;
  }

  virtual const std::vector<uint8_t>& raw_strtab() const {
    return raw_strtab_;
  }

  void set_strtab_section(Section& section) {
    strtab_section_ = &section;
  }

  void set_dyn_sym_idx(int32_t val) {
    new_symndx_ = val;
  }

  bool is_strtab_shared_shstrtab() const;
  size_t section_strtab_size();
  size_t section_shstr_size();

  virtual ~Layout() = default;
  Layout() = delete;

  bool should_swap() const {
    return should_swap_;
  }

  protected:
  Binary* binary_ = nullptr;

  std::unordered_map<std::string, size_t> shstr_name_map_;
  std::unordered_map<std::string, size_t> strtab_name_map_;

  std::vector<uint8_t> raw_shstrtab_;
  std::vector<uint8_t> raw_strtab_;

  Section* strtab_section_ = nullptr;
  int32_t new_symndx_ = -1;
  bool should_swap_ = false;
  const Builder::config_t* config_ = nullptr;
};
}
}
#endif

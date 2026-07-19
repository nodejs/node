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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_FUNC_OVERRIDE_INFO_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_FUNC_OVERRIDE_INFO_H

#include <memory>
#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

namespace LIEF {
class SpanStream;

namespace PE {
class Parser;
class Relocation;

class LIEF_API FunctionOverrideInfo {
  public:
  using relocations_t = std::vector<std::unique_ptr<Relocation>>;
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;
  using it_const_relocations = const_ref_iterator<const relocations_t&, const Relocation*>;

  FunctionOverrideInfo() = default;
  FunctionOverrideInfo(uint32_t original_rva, uint32_t bdd_offset,
                       uint32_t base_reloc_size);

  FunctionOverrideInfo(const FunctionOverrideInfo&);
  FunctionOverrideInfo& operator=(const FunctionOverrideInfo&);

  FunctionOverrideInfo(FunctionOverrideInfo&&);
  FunctionOverrideInfo& operator=(FunctionOverrideInfo&&);

  std::string to_string() const;

  /// RVA of the original function
  uint32_t original_rva() const {
    return original_rva_;
  }

  /// Offset into the BDD region
  uint32_t bdd_offset() const {
    return bdd_offset_;
  }

  /// Size in bytes taken by RVAs
  uint32_t rva_size() const {
    return rvas_.size() * sizeof(uint32_t);
  }

  /// Size in bytes taken by BaseRelocs
  uint32_t base_reloc_size() const {
    return base_relocsz_;
  }

  const std::vector<uint32_t>& functions_rva() const {
    return rvas_;
  }

  it_relocations relocations() {
    return relocations_;
  }

  it_const_relocations relocations() const {
    return relocations_;
  }

  FunctionOverrideInfo& original_rva(uint32_t value) {
    original_rva_ = value;
    return *this;
  }

  FunctionOverrideInfo& bdd_offset(uint32_t value) {
    bdd_offset_ = value;
    return *this;
  }

  FunctionOverrideInfo& base_reloc_size(uint32_t value) {
    base_relocsz_ = value;
    return *this;
  }

  FunctionOverrideInfo& overriding_funcs(std::vector<uint32_t> funcs) {
    rvas_ = std::move(funcs);
    return *this;
  }

  friend LIEF_API
    std::ostream& operator<<(std::ostream& os, const FunctionOverrideInfo& info)
  {
    os << info.to_string();
    return os;
  }

  ~FunctionOverrideInfo();

  /// \private
  LIEF_LOCAL static
    std::unique_ptr<FunctionOverrideInfo> parse(Parser& ctx, SpanStream& strm);

  private:
  uint32_t original_rva_ = 0;
  uint32_t bdd_offset_ = 0;
  uint32_t base_relocsz_ = 0;
  std::vector<uint32_t> rvas_;
  relocations_t relocations_;
};

}
}

#endif

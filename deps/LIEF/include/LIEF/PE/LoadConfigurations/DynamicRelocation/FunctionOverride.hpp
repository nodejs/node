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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_FUNC_OVERRIDE_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_FUNC_OVERRIDE_H

#include <memory>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"

namespace LIEF {
class SpanStream;

namespace PE {
class Parser;
class FunctionOverrideInfo;

/// This class represents `IMAGE_DYNAMIC_RELOCATION_FUNCTION_OVERRIDE`
class LIEF_API FunctionOverride : public DynamicFixup {
  public:

  /// Mirror `IMAGE_BDD_DYNAMIC_RELOCATION`
  struct image_bdd_dynamic_relocation_t {
    uint16_t left = 0;
    uint16_t right = 0;
    uint32_t value = 0;
  };

  /// Mirror `IMAGE_BDD_INFO`
  struct image_bdd_info_t {
    uint32_t version = 0;
    uint32_t original_size = 0;
    uint32_t original_offset = 0;

    /// If version == 1
    std::vector<image_bdd_dynamic_relocation_t> relocations;

    /// If version != 1
    std::vector<uint8_t> payload;
  };

  using func_overriding_info_t = std::vector<std::unique_ptr<FunctionOverrideInfo>>;
  using it_func_overriding_info = ref_iterator<func_overriding_info_t&, FunctionOverrideInfo*>;
  using it_const_func_overriding_info = const_ref_iterator<const func_overriding_info_t&, const FunctionOverrideInfo*>;

  using bdd_info_list_t = std::vector<image_bdd_info_t>;
  using it_bdd_info = ref_iterator<bdd_info_list_t&>;
  using it_const_bdd_info = const_ref_iterator<const bdd_info_list_t&>;

  FunctionOverride();

  FunctionOverride(const FunctionOverride&);
  FunctionOverride& operator=(const FunctionOverride&);

  FunctionOverride(FunctionOverride&&);
  FunctionOverride& operator=(FunctionOverride&&);

  std::unique_ptr<DynamicFixup> clone() const override {
    return std::unique_ptr<FunctionOverride>(new FunctionOverride(*this));
  }

  /// Iterator over the overriding info
  it_func_overriding_info func_overriding_info() {
    return overriding_info_;
  }

  it_const_func_overriding_info func_overriding_info() const {
    return overriding_info_;
  }

  /// Iterator over the BDD info
  it_bdd_info bdd_info() {
    return bdd_info_;
  }

  it_const_bdd_info bdd_info() const {
    return bdd_info_;
  }

  /// Find the `IMAGE_BDD_INFO` at the given offset
  image_bdd_info_t* find_bdd_info(uint32_t offset);

  /// Find the `IMAGE_BDD_INFO` associated with the given info
  image_bdd_info_t* find_bdd_info(const FunctionOverrideInfo& info);

  const image_bdd_info_t* find_bdd_info(uint32_t offset) const {
    return const_cast<FunctionOverride*>(this)->find_bdd_info(offset);
  }

  const image_bdd_info_t* find_bdd_info(const FunctionOverrideInfo& info) const {
    return const_cast<FunctionOverride*>(this)->find_bdd_info(info);
  }

  static bool classof(const DynamicFixup* fixup) {
    return fixup->kind() == DynamicFixup::KIND::FUNCTION_OVERRIDE;
  }

  std::string to_string() const override;

  ~FunctionOverride() override;

  /// \private
  LIEF_LOCAL static
    std::unique_ptr<FunctionOverride> parse(Parser& ctx, SpanStream& strm);

  /// \private
  LIEF_LOCAL static ok_error_t
    parse_override_info(Parser& ctx, SpanStream& strm, FunctionOverride& func);

  /// \private
  LIEF_LOCAL static ok_error_t
    parse_bdd_info(Parser& ctx, SpanStream& strm, FunctionOverride& func);

  private:
  func_overriding_info_t overriding_info_;
  std::vector<image_bdd_info_t> bdd_info_;
};

}
}

#endif

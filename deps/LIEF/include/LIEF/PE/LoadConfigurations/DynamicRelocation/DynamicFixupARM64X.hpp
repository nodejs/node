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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_ARM64X_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_ARM64X_H
#include <vector>
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"

#include "LIEF/iterators.hpp"

namespace LIEF {
namespace PE {

/// This class represents `IMAGE_DYNAMIC_RELOCATION_ARM64X`
class LIEF_API DynamicFixupARM64X : public DynamicFixup {
  public:
  enum class FIXUP_TYPE {
    ZEROFILL = 0,
    VALUE = 1,
    DELTA = 2,
  };

  struct LIEF_API reloc_entry_t {
    /// RVA where the fixup takes place
    uint32_t rva = 0;

    /// Fixup's kind
    FIXUP_TYPE type = FIXUP_TYPE::ZEROFILL;

    /// Size of the value to patch
    size_t size = 0;

    /// If the type is FIXUP_TYPE::VALUE, the bytes associated to
    /// the fixup entry.
    std::vector<uint8_t> bytes;

    /// If the type is FIXUP_TYPE::DELTA, the (signed) value.
    int64_t value = 0;

    std::string to_string() const;

    LIEF_API friend
      std::ostream& operator<<(std::ostream& os, const reloc_entry_t& entry)
    {
      os << entry.to_string();
      return os;
    }
  };

  using reloc_entries_t = std::vector<reloc_entry_t>;
  using it_relocations = ref_iterator<reloc_entries_t&>;
  using it_const_relocations = const_ref_iterator<const reloc_entries_t&>;

  DynamicFixupARM64X() :
    DynamicFixup(KIND::ARM64X)
  {}

  DynamicFixupARM64X(const DynamicFixupARM64X&) = default;
  DynamicFixupARM64X& operator=(const DynamicFixupARM64X&) = default;

  DynamicFixupARM64X(DynamicFixupARM64X&&) = default;
  DynamicFixupARM64X& operator=(DynamicFixupARM64X&&) = default;

  std::unique_ptr<DynamicFixup> clone() const override {
    return std::unique_ptr<DynamicFixupARM64X>(new DynamicFixupARM64X(*this));
  }

  std::string to_string() const override;

  /// Iterator over the different fixup entries
  it_relocations relocations() {
    return entries_;
  }

  it_const_relocations relocations() const {
    return entries_;
  }

  static bool classof(const DynamicFixup* fixup) {
    return fixup->kind() == KIND::ARM64X;
  }

  ~DynamicFixupARM64X() override = default;

  /// \private
  LIEF_LOCAL static
    std::unique_ptr<DynamicFixupARM64X> parse(Parser& ctx, SpanStream& strm);

  private:
  reloc_entries_t entries_;

};


}
}

#endif

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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_ARM64_KERNEL_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_ARM64_KERNEL_H
#include <vector>
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"

#include "LIEF/iterators.hpp"

namespace LIEF {
namespace PE {

/// This class wraps fixups associated with the (special) symbol value:
/// `IMAGE_DYNAMIC_RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER (8)`.
class LIEF_API DynamicFixupARM64Kernel : public DynamicFixup {
  public:
  static constexpr auto NO_IAT_INDEX = 0x7fff;

  enum class IMPORT_TYPE : uint8_t {
    STATIC = 0,
    DELAYED = 1,
  };

  /// Mirror `IMAGE_IMPORT_CONTROL_TRANSFER_ARM64_RELOCATION`
  struct LIEF_API reloc_entry_t {
    /// RVA to the call instruction
    uint32_t rva = 0;

    /// True if target instruction is a `blr`, false if it's a `br`.
    bool indirect_call = false;

    /// Register index used for the indirect call/jump.
    /// For instance, if the instruction is `br x3`, this index is set to `3`
    uint8_t register_index = 0;

    /// See IMPORT_TYPE
    IMPORT_TYPE import_type = IMPORT_TYPE::STATIC;

    /// IAT index of the corresponding import. `0x7FFF` is a special value
    /// indicating no index.
    uint16_t iat_index = NO_IAT_INDEX;

    std::string to_string() const;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const reloc_entry_t& entry)
    {
      os << entry.to_string();
      return os;
    }
  };

  using reloc_entries_t = std::vector<reloc_entry_t>;
  using it_relocations = ref_iterator<reloc_entries_t&>;
  using it_const_relocations = const_ref_iterator<const reloc_entries_t&>;

  DynamicFixupARM64Kernel() :
    DynamicFixup(KIND::ARM64_KERNEL_IMPORT_CALL_TRANSFER)
  {}

  DynamicFixupARM64Kernel(const DynamicFixupARM64Kernel&) = default;
  DynamicFixupARM64Kernel& operator=(const DynamicFixupARM64Kernel&) = default;

  DynamicFixupARM64Kernel(DynamicFixupARM64Kernel&&) = default;
  DynamicFixupARM64Kernel& operator=(DynamicFixupARM64Kernel&&) = default;

  std::unique_ptr<DynamicFixup> clone() const override {
    return std::unique_ptr<DynamicFixupARM64Kernel>(new DynamicFixupARM64Kernel(*this));
  }

  std::string to_string() const override;

  /// Iterator over the relocations
  it_relocations relocations() {
    return entries_;
  }

  it_const_relocations relocations() const {
    return entries_;
  }

  static bool classof(const DynamicFixup* fixup) {
    return fixup->kind() == KIND::ARM64_KERNEL_IMPORT_CALL_TRANSFER;
  }

  ~DynamicFixupARM64Kernel() override = default;

  /// \private
  LIEF_LOCAL static
    std::unique_ptr<DynamicFixupARM64Kernel> parse(Parser& ctx, SpanStream& strm);

  private:
  reloc_entries_t entries_;
};


}
}

#endif

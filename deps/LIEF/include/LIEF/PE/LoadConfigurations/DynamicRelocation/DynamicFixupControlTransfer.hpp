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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_CONTROL_TRANSFER_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_CONTROL_TRANSFER_H
#include <vector>
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"

#include "LIEF/iterators.hpp"

namespace LIEF {
namespace PE {

/// This class wraps fixups associated with the (special) symbol value:
/// `IMAGE_DYNAMIC_RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER (3)`.
class LIEF_API DynamicFixupControlTransfer : public DynamicFixup {
  public:
  static constexpr auto NO_IAT_INDEX = 0x7fff;

  /// Mirror `IMAGE_IMPORT_CONTROL_TRANSFER_DYNAMIC_RELOCATION`
  struct LIEF_API reloc_entry_t {
    /// RVA to the call instruction
    uint32_t rva = 0;

    /// True if target instruction is a `call`, false otherwise.
    bool is_call = false;

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

  DynamicFixupControlTransfer() :
    DynamicFixup(KIND::GUARD_IMPORT_CONTROL_TRANSFER)
  {}

  DynamicFixupControlTransfer(const DynamicFixupControlTransfer&) = default;
  DynamicFixupControlTransfer& operator=(const DynamicFixupControlTransfer&) = default;

  DynamicFixupControlTransfer(DynamicFixupControlTransfer&&) = default;
  DynamicFixupControlTransfer& operator=(DynamicFixupControlTransfer&&) = default;

  std::unique_ptr<DynamicFixup> clone() const override {
    return std::unique_ptr<DynamicFixupControlTransfer>(new DynamicFixupControlTransfer(*this));
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
    return fixup->kind() == KIND::GUARD_IMPORT_CONTROL_TRANSFER;
  }

  ~DynamicFixupControlTransfer() override = default;

  /// \private
  LIEF_LOCAL static
    std::unique_ptr<DynamicFixupControlTransfer> parse(Parser& ctx, SpanStream& strm);

  private:
  reloc_entries_t entries_;
};


}
}

#endif

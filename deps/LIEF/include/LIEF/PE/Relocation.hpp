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
#ifndef LIEF_PE_RELOCATION_H
#define LIEF_PE_RELOCATION_H
#include <cstdint>
#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;
class Builder;
class Binary;

class RelocationEntry;

namespace details {
struct pe_base_relocation_block;
}

/// Class which represents the *Base Relocation Block*
/// We usually find this structure in the ``.reloc`` section
class LIEF_API Relocation : public Object {
  friend class Parser;
  friend class Builder;
  friend class Binary;

  public:
  using entries_t        = std::vector<std::unique_ptr<RelocationEntry>>;
  using it_entries       = ref_iterator<entries_t&, RelocationEntry*>;
  using it_const_entries = const_ref_iterator<const entries_t&, RelocationEntry*>;

  Relocation();
  Relocation(uint32_t base, uint32_t block_size);
  Relocation(uint32_t base);

  Relocation(const Relocation& other);
  Relocation& operator=(Relocation other);

  Relocation(Relocation&&);
  Relocation& operator=(Relocation&& other);

  Relocation(const details::pe_base_relocation_block& header);
  ~Relocation() override;

  void swap(Relocation& other);

  /// The RVA for which the offset of the relocation entries (RelocationEntry) is added
  uint32_t virtual_address() const {
    return virtual_address_;
  }

  /// The total number of bytes in the base relocation block.
  ///
  /// ```
  /// block_size = sizeof(BaseRelocationBlock) +
  ///              nb_of_relocs * sizeof(uint16_t = RelocationEntry)
  /// ```
  uint32_t block_size() const {
    return block_size_;
  }

  /// Iterator over the RelocationEntry
  it_const_entries entries() const {
    return entries_;
  }

  it_entries entries() {
    return entries_;
  }

  void virtual_address(uint32_t virtual_address) {
    virtual_address_ = virtual_address;
  }

  void block_size(uint32_t block_size) {
    block_size_ = block_size;
  }

  RelocationEntry& add_entry(const RelocationEntry& entry);

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Relocation& relocation);

  using relocations_t = std::vector<std::unique_ptr<Relocation>>;

  LIEF_LOCAL static relocations_t parse(Parser& ctx, BinaryStream& stream);

  private:
  uint32_t block_size_ = 0;
  uint32_t virtual_address_ = 0;
  entries_t entries_;
};

}
}
#endif /* RELOCATION_H */

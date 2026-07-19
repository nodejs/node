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
#ifndef LIEF_MACHO_DATA_IN_CODE_COMMAND_H
#define LIEF_MACHO_DATA_IN_CODE_COMMAND_H
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/span.hpp"

#include "LIEF/MachO/LoadCommand.hpp"
#include "LIEF/MachO/DataCodeEntry.hpp"

namespace LIEF {
namespace MachO {
class BinaryParser;
class LinkEdit;

namespace details {
struct linkedit_data_command;
}

/// Interface of the LC_DATA_IN_CODE command
/// This command is used to list slices of code sections that contain data. The *slices*
/// information are stored as an array of DataCodeEntry
///
/// @see DataCodeEntry
class LIEF_API DataInCode : public LoadCommand {
  friend class BinaryParser;
  friend class LinkEdit;
  public:
  using entries_t        = std::vector<DataCodeEntry>;
  using it_const_entries = const_ref_iterator<const entries_t&>;
  using it_entries       = ref_iterator<entries_t&>;

  public:
  DataInCode() = default;
  DataInCode(const details::linkedit_data_command& cmd);

  DataInCode& operator=(const DataInCode&) = default;
  DataInCode(const DataInCode&) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DataInCode>(new DataInCode(*this));
  }

  /// Start of the array of the DataCodeEntry entries
  uint32_t data_offset() const {
    return data_offset_;
  }

  /// Size of the raw array (``size = sizeof(DataCodeEntry) * nb_elements``)
  uint32_t data_size() const {
    return data_size_;
  }

  void data_offset(uint32_t offset) {
    data_offset_ = offset;
  }
  void data_size(uint32_t size) {
    data_size_ = size;
  }

  /// Add a new entry
  DataInCode& add(DataCodeEntry entry) {
    entries_.push_back(std::move(entry));
    return *this;
  }

  /// Iterator over the DataCodeEntry
  it_const_entries entries() const {
    return entries_;
  }

  it_entries entries() {
    return entries_;
  }

  span<uint8_t> content() {
    return content_;
  }

  span<const uint8_t> content() const {
    return content_;
  }

  ~DataInCode() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::DATA_IN_CODE;
  }

  private:
  uint32_t  data_offset_ = 0;
  uint32_t  data_size_   = 0;
  entries_t entries_;
  span<uint8_t> content_;

};

}
}
#endif

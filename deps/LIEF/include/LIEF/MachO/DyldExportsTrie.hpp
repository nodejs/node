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
#ifndef LIEF_MACHO_DYLD_EXPORTS_TRIE_H
#define LIEF_MACHO_DYLD_EXPORTS_TRIE_H
#include <memory>
#include "LIEF/span.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"
#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;
class Builder;
class LinkEdit;
class ExportInfo;
class Binary;

namespace details {
struct linkedit_data_command;
}

/// Class that represents the LC_DYLD_EXPORTS_TRIE command
///
/// In recent Mach-O binaries, this command replace the DyldInfo export trie buffer
class LIEF_API DyldExportsTrie : public LoadCommand {
  friend class BinaryParser;
  friend class Builder;
  friend class LinkEdit;
  friend class Binary;

  public:
  /// Internal container for storing ExportInfo
  using export_info_t = std::vector<std::unique_ptr<ExportInfo>>;

  /// Iterator which outputs const ExportInfo&
  using it_export_info = ref_iterator<export_info_t&, ExportInfo*>;

  /// Iterator which outputs const ExportInfo&
  using it_const_export_info = const_ref_iterator<const export_info_t&, ExportInfo*>;

  DyldExportsTrie();
  DyldExportsTrie(const details::linkedit_data_command& cmd);
  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DyldExportsTrie>(new DyldExportsTrie(*this));
  }

  void swap(DyldExportsTrie& other) noexcept;

  ~DyldExportsTrie() override;

  /// Offset of the LC_DYLD_EXPORTS_TRIE.
  /// This offset should point in the __LINKEDIT segment
  uint32_t data_offset() const {
    return data_offset_;
  }

  /// Size of the LC_DYLD_EXPORTS_TRIE payload.
  uint32_t data_size() const {
    return data_size_;
  }

  void data_offset(uint32_t offset) {
    data_offset_ = offset;
  }
  void data_size(uint32_t size) {
    data_size_ = size;
  }

  span<const uint8_t> content() const {
    return content_;
  }

  /// Iterator over the ExportInfo entries
  it_export_info exports() {
    return export_info_;
  }

  it_const_export_info exports() const {
    return export_info_;
  }

  /// Print the exports trie in a humman-readable way
  std::string show_export_trie() const;

  /// Add an entrie in the current trie.
  /// See also: LIEF::MachO::Binary::add_exported_function
  void add(std::unique_ptr<ExportInfo> info);

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::DYLD_EXPORTS_TRIE;
  }

  private:
  LIEF_LOCAL DyldExportsTrie& operator=(DyldExportsTrie other);
  LIEF_LOCAL DyldExportsTrie(const DyldExportsTrie& other);

  uint32_t data_offset_ = 0;
  uint32_t data_size_   = 0;

  // Raw payload of the DyldChainedFixups.
  // This payload is located in the __LINKEDIT segment
  span<uint8_t> content_;

  export_info_t  export_info_;
};

}
}
#endif

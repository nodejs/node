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
#ifndef LIEF_MACHO_FILESET_COMMAND_H
#define LIEF_MACHO_FILESET_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"


namespace LIEF {
namespace MachO {
class Binary;
class BinaryParser;

namespace details {
struct fileset_entry_command;
}

/// Class associated with the LC_FILESET_ENTRY commands
class LIEF_API FilesetCommand : public LoadCommand {
  public:
  friend class BinaryParser;
  using content_t = std::vector<uint8_t>;

  FilesetCommand() = default;
  FilesetCommand(const details::fileset_entry_command& command);
  FilesetCommand(std::string name) :
    name_(std::move(name))
  {}

  FilesetCommand& operator=(FilesetCommand copy);
  FilesetCommand(const FilesetCommand& copy);

  void swap(FilesetCommand& other) noexcept;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<FilesetCommand>(new FilesetCommand(*this));
  }

  ~FilesetCommand() override = default;

  /// Name of the underlying MachO binary (e.g. ``com.apple.security.quarantine``)
  const std::string& name() const {
    return name_;
  }

  /// Memory address where the MachO file should be mapped
  uint64_t virtual_address() const {
    return virtual_address_;
  }

  /// Original offset in the kernel cache
  uint64_t file_offset() const {
    return file_offset_;
  }

  /// Return a pointer on the LIEF::MachO::Binary associated
  /// with this entry
  const Binary* binary() const {
    return binary_;
  }

  Binary* binary() {
    return binary_;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  void virtual_address(uint64_t virtual_address) {
    virtual_address_ = virtual_address;
  }
  void file_offset(uint64_t file_offset) {
    file_offset_ = file_offset;
  }

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::FILESET_ENTRY;
  }

  private:
  std::string name_;
  uint64_t virtual_address_ = 0;
  uint64_t file_offset_ = 0;
  Binary* binary_ = nullptr;
};

}
}
#endif

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
#ifndef LIEF_MACHO_NOTE_COMMAND_H
#define LIEF_MACHO_NOTE_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct note_command;
}

/// Class that represent the `LC_NOTE` command.
///
/// This command is used to include arbitrary notes or metadata within a binary.
class LIEF_API NoteCommand : public LoadCommand {
  public:
  NoteCommand() = default;
  NoteCommand(const details::note_command& cmd);

  NoteCommand& operator=(const NoteCommand& copy) = default;
  NoteCommand(const NoteCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<NoteCommand>(new NoteCommand(*this));
  }

  /// Offset of the data associated with this note
  uint64_t note_offset() const {
    return note_offset_;
  }

  /// Size of the data referenced by the note_offset
  uint64_t note_size() const {
    return note_size_;
  }

  /// Owner of the note (e.g. `AIR_METALLIB`)
  span<const char> owner() const {
    return owner_;
  }

  span<char> owner() {
    return owner_;
  }

  /// Owner as a zero-terminated string
  std::string owner_str() const {
    return std::string(owner_.data(), owner_.size()).c_str();
  }

  void note_offset(uint64_t offset) {
    note_offset_ = offset;
  }

  void note_size(uint64_t size) {
    note_size_ = size;
  }

  ~NoteCommand() override = default;

  std::ostream& print(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::NOTE;
  }

  private:
  std::array<char, 16> owner_;
  uint64_t note_offset_ = 0;
  uint64_t note_size_ = 0;
};

}
}
#endif

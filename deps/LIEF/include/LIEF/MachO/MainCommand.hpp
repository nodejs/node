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
#ifndef LIEF_MACHO_MAIN_COMMAND_H
#define LIEF_MACHO_MAIN_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct entry_point_command;
}

/// Class that represent the LC_MAIN command. This kind
/// of command can be used to determine the entrypoint of an executable
class LIEF_API MainCommand : public LoadCommand {
  public:
  MainCommand() = default;
  MainCommand(const details::entry_point_command& cmd);
  MainCommand(uint64_t entrypoint, uint64_t stacksize);

  MainCommand& operator=(const MainCommand& copy) = default;
  MainCommand(const MainCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<MainCommand>(new MainCommand(*this));
  }

  ~MainCommand() override = default;

  /// Offset of the *main* function relative to the ``__TEXT``
  /// segment
  uint64_t entrypoint() const {
    return entrypoint_;
  }

  /// The initial stack size
  uint64_t stack_size() const {
    return stack_size_;
  }

  void entrypoint(uint64_t entrypoint) {
    entrypoint_ = entrypoint;
  }
  void stack_size(uint64_t stacksize) {
    stack_size_ = stacksize;
  }

  std::ostream& print(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::MAIN;
  }

  private:
  uint64_t entrypoint_ = 0;
  uint64_t stack_size_ = 0;
};

}
}
#endif

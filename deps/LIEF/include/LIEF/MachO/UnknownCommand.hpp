/* Copyright 2024 - 2025 R. Thomas
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
#ifndef LIEF_MACHO_UNKNOWN_COMMAND_H
#define LIEF_MACHO_UNKNOWN_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct load_command;
}


/// Generic class when the command is not recognized by LIEF
class LIEF_API UnknownCommand : public LoadCommand {

  public:
  UnknownCommand() = delete;
  UnknownCommand(const details::load_command& command) :
    LoadCommand(command),
    original_command_(static_cast<uint64_t>(command_))
  {
    command_ = LoadCommand::TYPE::LIEF_UNKNOWN;
  }

  UnknownCommand& operator=(const UnknownCommand& copy) = default;
  UnknownCommand(const UnknownCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<UnknownCommand>(new UnknownCommand(*this));
  }

  ~UnknownCommand() override = default;

  /// The original `LC_` int that is not supported by LIEF
  uint64_t original_command() const {
    return original_command_;
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::LIEF_UNKNOWN;
  }

  private:
  uint64_t original_command_ = 0;
};

}
}
#endif

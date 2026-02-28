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
#ifndef LIEF_MACHO_DYLINKER_COMMAND_H
#define LIEF_MACHO_DYLINKER_COMMAND_H
#include <string>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct dylinker_command;
}

/// Class that represents the Mach-O linker, also named loader.
/// Most of the time, DylinkerCommand::name() should return ``/usr/lib/dyld``
class LIEF_API DylinkerCommand : public LoadCommand {
  public:
  DylinkerCommand() = default;
  DylinkerCommand(const details::dylinker_command& cmd);
  DylinkerCommand(std::string name);

  DylinkerCommand& operator=(const DylinkerCommand& copy) = default;
  DylinkerCommand(const DylinkerCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DylinkerCommand>(new DylinkerCommand(*this));
  }

  ~DylinkerCommand() override = default;

  std::ostream& print(std::ostream& os) const override;

  /// Path to the linker (or loader)
  const std::string& name() const {
    return name_;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  void accept(Visitor& visitor) const override;

  static bool classof(const LoadCommand* cmd) {
    const LoadCommand::TYPE type = cmd->command();
    return type == LoadCommand::TYPE::ID_DYLINKER ||
           type == LoadCommand::TYPE::LOAD_DYLINKER;
  }

  private:
  std::string name_;
};

}
}
#endif

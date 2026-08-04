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
#ifndef LIEF_MACHO_UUID_COMMAND_H
#define LIEF_MACHO_UUID_COMMAND_H
#include <ostream>
#include <array>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct uuid_command;
}

using uuid_t = std::array<uint8_t, 16>;

/// Class that represents the UUID command
class LIEF_API UUIDCommand : public LoadCommand {
  public:
  UUIDCommand() = default;
  UUIDCommand(const details::uuid_command& cmd);

  UUIDCommand& operator=(const UUIDCommand& copy) = default;
  UUIDCommand(const UUIDCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<UUIDCommand>(new UUIDCommand(*this));
  }

  ~UUIDCommand() override = default;

  /// The UUID as a 16-bytes array
  const uuid_t& uuid() const {
    return uuid_;
  }
  void uuid(const uuid_t& uuid) {
    uuid_ = uuid;
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::UUID;
  }

  private:
  uuid_t uuid_ = {0};
};

}
}
#endif

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
#ifndef LIEF_MACHO_DYLD_ENVIROMENT_COMMAND_H
#define LIEF_MACHO_DYLD_ENVIROMENT_COMMAND_H
#include <string>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct dylinker_command;
}

/// Class that represents a `LC_DYLD_ENVIRONMENT` command which is
/// used by the Mach-O linker/loader to initialize an environment variable
class LIEF_API DyldEnvironment : public LoadCommand {
  public:
  DyldEnvironment() = default;
  DyldEnvironment(const details::dylinker_command& cmd);

  DyldEnvironment& operator=(const DyldEnvironment& copy) = default;
  DyldEnvironment(const DyldEnvironment& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DyldEnvironment>(new DyldEnvironment(*this));
  }

  ~DyldEnvironment() override = default;

  std::ostream& print(std::ostream& os) const override;

  /// The actual environment variable
  const std::string& value() const {
    return value_;
  }

  void value(std::string value) {
    value_ = std::move(value);
  }

  void accept(Visitor& visitor) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::DYLD_ENVIRONMENT;
  }

  private:
  std::string value_;
};

}
}
#endif

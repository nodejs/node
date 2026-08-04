/* Copyright 2017 - 2021 J. Rieck (based on R. Thomas's work)
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
#ifndef LIEF_MACHO_RPATH_COMMAND_H
#define LIEF_MACHO_RPATH_COMMAND_H
#include <string>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct rpath_command;
}

/// Class that represents the `LC_RPATH` command.
///
/// This command is used to add path for searching libraries
/// associated with the `@rpath` prefix.
class LIEF_API RPathCommand : public LoadCommand {
  public:
  RPathCommand() = default;
  RPathCommand(std::string path);
  RPathCommand(const details::rpath_command& rpathCmd);

  RPathCommand& operator=(const RPathCommand& copy) = default;
  RPathCommand(const RPathCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<RPathCommand>(new RPathCommand(*this));
  }

  /// Create a new RPath command for the provided `path`
  static std::unique_ptr<RPathCommand> create(std::string path) {
    return std::unique_ptr<RPathCommand>(new RPathCommand(std::move(path)));
  }

  ~RPathCommand() override = default;

  /// The rpath value as a string
  const std::string& path() const {
    return path_;
  }

  /// Original string offset of the path
  uint32_t path_offset() const {
    return path_offset_;
  }

  void path(std::string path) {
    path_ = std::move(path);
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::RPATH;
  }

  private:
  uint32_t path_offset_ = 0;
  std::string path_;
};

}
}
#endif

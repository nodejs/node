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
#ifndef LIEF_MACHO_DYLIB_COMMAND_H
#define LIEF_MACHO_DYLIB_COMMAND_H
#include <array>
#include <string>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct dylib_command;
}

/// Class which represents a library dependency
class LIEF_API DylibCommand : public LoadCommand {
  public:
  using version_t = std::array<uint16_t, 3>;

  public:
  /// Helper to convert an integer into a version array
  static version_t int2version(uint32_t version) {
    return {{
      static_cast<uint16_t>(version >> 16),
      static_cast<uint16_t>((version >> 8) & 0xFF),
      static_cast<uint16_t>(version & 0xFF),
    }};
  }

  /// Helper to convert a version array into an integer
  static uint32_t version2int(version_t version) {
    return (version[2]) | (version[1] << 8) | (version[0] << 16);
  }

  /// Factory function to generate a LC_LOAD_WEAK_DYLIB library
  static DylibCommand weak_dylib(const std::string& name,
      uint32_t timestamp = 0,
      uint32_t current_version = 0,
      uint32_t compat_version = 0);

  /// Factory function to generate a LC_ID_DYLIB library
  static DylibCommand id_dylib(const std::string& name,
      uint32_t timestamp = 0,
      uint32_t current_version = 0,
      uint32_t compat_version = 0);

  /// Factory function to generate a LC_LOAD_DYLIB library
  static DylibCommand load_dylib(const std::string& name,
      uint32_t timestamp = 2,
      uint32_t current_version = 0,
      uint32_t compat_version = 0);

  /// Factory function to generate a LC_REEXPORT_DYLIB library
  static DylibCommand reexport_dylib(const std::string& name,
      uint32_t timestamp = 0,
      uint32_t current_version = 0,
      uint32_t compat_version = 0);

  /// Factory function to generate a LC_LOAD_UPWARD_DYLIB library
  static DylibCommand load_upward_dylib(const std::string& name,
      uint32_t timestamp = 0,
      uint32_t current_version = 0,
      uint32_t compat_version = 0);

  /// Factory function to generate a LC_LAZY_LOAD_DYLIB library
  static DylibCommand lazy_load_dylib(const std::string& name,
      uint32_t timestamp = 0,
      uint32_t current_version = 0,
      uint32_t compat_version = 0);

  public:
  DylibCommand() = default;
  DylibCommand(const details::dylib_command& cmd);

  DylibCommand& operator=(const DylibCommand& copy) = default;
  DylibCommand(const DylibCommand& copy) = default;

  ~DylibCommand() override = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DylibCommand>(new DylibCommand(*this));
  }

  /// Library name
  const std::string& name() const {
    return name_;
  }

  /// Original string offset of the name
  uint32_t name_offset() const {
    return name_offset_;
  }

  /// Date and Time when the shared library was built
  uint32_t timestamp() const {
    return timestamp_;
  }

  /// Current version of the shared library
  version_t current_version() const {
    return int2version(current_version_);
  }

  /// Compatibility version of the shared library
  version_t compatibility_version() const {
    return int2version(compatibility_version_);
  }

  void name(std::string name) {
    name_ = std::move(name);
  }
  void timestamp(uint32_t timestamp) {
    timestamp_ = timestamp;
  }
  void current_version(version_t version) {
    current_version_ = version2int(version);
  }
  void compatibility_version(version_t version) {
    compatibility_version_ = version2int(version);
  }

  std::ostream& print(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const LoadCommand* cmd) {
    const LoadCommand::TYPE type = cmd->command();
    return type == LoadCommand::TYPE::LOAD_WEAK_DYLIB ||
           type == LoadCommand::TYPE::ID_DYLIB ||
           type == LoadCommand::TYPE::LOAD_DYLIB ||
           type == LoadCommand::TYPE::LOAD_UPWARD_DYLIB ||
           type == LoadCommand::TYPE::REEXPORT_DYLIB ||
           type == LoadCommand::TYPE::LOAD_UPWARD_DYLIB ||
           type == LoadCommand::TYPE::LAZY_LOAD_DYLIB;
  }

  private:
  LIEF_LOCAL static DylibCommand create(
    LoadCommand::TYPE type, const std::string& name, uint32_t timestamp,
    uint32_t current_version, uint32_t compat_version);

  std::string name_;
  uint32_t name_offset_ = 0;
  uint32_t timestamp_ = 0;
  uint32_t current_version_ = 0;
  uint32_t compatibility_version_ = 0;
};


}
}
#endif

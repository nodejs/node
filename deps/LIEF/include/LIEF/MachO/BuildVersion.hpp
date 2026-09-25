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
#ifndef LIEF_MACHO_BUILD_VERSION_COMMAND_H
#define LIEF_MACHO_BUILD_VERSION_COMMAND_H
#include <vector>
#include <ostream>
#include <array>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"
#include "LIEF/MachO/BuildToolVersion.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct build_version_command;
}

class LIEF_API BuildVersion : public LoadCommand {
  friend class BinaryParser;

  public:
  /// Version is an array of **3** integers
  using version_t = std::array<uint32_t, 3>;

  using tools_list_t = std::vector<BuildToolVersion>;

  public:
  enum class PLATFORMS : uint32_t {
    UNKNOWN               = 0,
    MACOS                 = 1,
    IOS                   = 2,
    TVOS                  = 3,
    WATCHOS               = 4,
    BRIDGEOS              = 5,
    MAC_CATALYST          = 6,
    IOS_SIMULATOR         = 7,
    TVOS_SIMULATOR        = 8,
    WATCHOS_SIMULATOR     = 9,
    DRIVERKIT             = 10,
    VISIONOS              = 11,
    VISIONOS_SIMULATOR    = 12,
    FIRMWARE              = 13,
    SEPOS                 = 14,
    MACOS_EXCLAVE_CORE    = 15,
    MACOS_EXCLAVE_KIT     = 16,
    IOS_EXCLAVE_CORE      = 17,
    IOS_EXCLAVE_KIT       = 18,
    TVOS_EXCLAVE_CORE     = 19,
    TVOS_EXCLAVE_KIT      = 20,
    WATCHOS_EXCLAVE_CORE  = 21,
    WATCHOS_EXCLAVE_KIT   = 22,
    VISIONOS_EXCLAVE_CORE = 23,
    VISIONOS_EXCLAVE_KIT  = 24,

    ANY                = 0xFFFFFFFF
  };

  public:
  BuildVersion() = default;
  BuildVersion(const details::build_version_command& version_cmd);
  BuildVersion(const PLATFORMS platform,
               const version_t &minos,
               const version_t &sdk,
               const tools_list_t &tools);

  BuildVersion& operator=(const BuildVersion& copy) = default;
  BuildVersion(const BuildVersion& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<BuildVersion>(new BuildVersion(*this));
  }

  version_t minos() const {
    return minos_;
  }

  void minos(version_t version) {
    minos_ = version;
  }

  version_t sdk() const {
    return sdk_;
  }
  void sdk(version_t version) {
    sdk_ = version;
  }

  PLATFORMS platform() const {
    return platform_;
  }
  void platform(PLATFORMS plat) {
    platform_ = plat;
  }

  const tools_list_t& tools() const {
    return tools_;
  }

  ~BuildVersion() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::BUILD_VERSION;
  }

  private:
  PLATFORMS platform_ = PLATFORMS::UNKNOWN;
  version_t minos_;
  version_t sdk_;
  tools_list_t tools_;
};

LIEF_API const char* to_string(BuildVersion::PLATFORMS e);

}
}
#endif

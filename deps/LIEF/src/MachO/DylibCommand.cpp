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
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/ranges.h"
#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/DylibCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

DylibCommand::DylibCommand(const details::dylib_command& cmd) :
  LoadCommand::LoadCommand{static_cast<LoadCommand::TYPE>(cmd.cmd), cmd.cmdsize},
  name_offset_{cmd.name},
  timestamp_{cmd.timestamp},
  current_version_{cmd.current_version},
  compatibility_version_{cmd.compatibility_version}
{}


void DylibCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DylibCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("name={}, timestamp={}, "
                    "current_version={},  compatibility_version={}",
                    name(), timestamp(),
                    fmt::join(current_version(), "."),
                    fmt::join(compatibility_version(), "."));
   return os;
}

DylibCommand DylibCommand::create(LoadCommand::TYPE type,
    const std::string& name,
    uint32_t timestamp,
    uint32_t current_version,
    uint32_t compat_version) {

  details::dylib_command raw_cmd;
  raw_cmd.cmd                   = static_cast<uint32_t>(type);
  raw_cmd.cmdsize               = align(sizeof(details::dylib_command) + name.size() + 1, sizeof(uint64_t));
  raw_cmd.timestamp             = timestamp;
  raw_cmd.current_version       = current_version;
  raw_cmd.compatibility_version = compat_version;

  DylibCommand dylib{raw_cmd};
  dylib.name(name);
  dylib.data(LoadCommand::raw_t(raw_cmd.cmdsize, 0));

  return dylib;
}

DylibCommand DylibCommand::load_dylib(const std::string& name,
      uint32_t timestamp,
      uint32_t current_version,
      uint32_t compat_version)
{

  return DylibCommand::create(
      LoadCommand::TYPE::LOAD_DYLIB, name,
      timestamp, current_version, compat_version);
}

DylibCommand DylibCommand::weak_dylib(const std::string& name,
      uint32_t timestamp,
      uint32_t current_version,
      uint32_t compat_version)
{

  return DylibCommand::create(
      LoadCommand::TYPE::LOAD_WEAK_DYLIB, name,
      timestamp, current_version, compat_version);
}

DylibCommand DylibCommand::id_dylib(const std::string& name,
      uint32_t timestamp,
      uint32_t current_version,
      uint32_t compat_version)
{

  return DylibCommand::create(
      LoadCommand::TYPE::ID_DYLIB, name,
      timestamp, current_version, compat_version);
}

DylibCommand DylibCommand::reexport_dylib(const std::string& name,
      uint32_t timestamp,
      uint32_t current_version,
      uint32_t compat_version)
{

  return DylibCommand::create(
      LoadCommand::TYPE::REEXPORT_DYLIB, name,
      timestamp, current_version, compat_version);
}

DylibCommand DylibCommand::load_upward_dylib(const std::string& name,
      uint32_t timestamp,
      uint32_t current_version,
      uint32_t compat_version)
{

  return DylibCommand::create(
      LoadCommand::TYPE::LOAD_UPWARD_DYLIB, name,
      timestamp, current_version, compat_version);
}


DylibCommand DylibCommand::lazy_load_dylib(const std::string& name,
      uint32_t timestamp,
      uint32_t current_version,
      uint32_t compat_version)
{

  return DylibCommand::create(
      LoadCommand::TYPE::LAZY_LOAD_DYLIB, name,
      timestamp, current_version, compat_version);
}

}
}

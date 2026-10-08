/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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

#include "LIEF/runtime/osx/Module.hpp"
#include "LIEF/runtime/osx/Host.hpp"
#include "LIEF/runtime/osx/Process.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/ParserConfig.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF::runtime {
namespace details {
class ModuleIt {};
class Module {};
}
}

namespace LIEF::runtime::osx {

std::unique_ptr<Module> Module::from_handle(void* /*handle*/) {
  return nullptr;
}

void* Module::handle() const {
  return nullptr;
}

void* Module::dlsym(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<MachO::Binary> Module::parse_from_path() const {
  return nullptr;
}

std::unique_ptr<MachO::Binary>
    Module::parse_from_path(const MachO::ParserConfig& /*config*/) const {
  return nullptr;
}

std::unique_ptr<MachO::Binary> Module::parse_from_memory() const {
  return nullptr;
}

std::unique_ptr<MachO::Binary>
    Module::parse_from_memory(const MachO::ParserConfig& /*config*/) const {
  return nullptr;
}

std::unique_ptr<Module> dlopen(const std::string& /*name*/) {
  return nullptr;
}

std::string Host::os_version_name() {
  return "";
}

Host::version_t Host::os_version() {
  return {0, 0, 0};
}

bool Host::is_sip_enabled() {
  return true;
}

std::string Process::dyld_version() {
  return "";
}
}

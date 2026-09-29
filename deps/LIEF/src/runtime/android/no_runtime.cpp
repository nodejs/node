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

#include "LIEF/runtime/android/Module.hpp"
#include "LIEF/runtime/android/Host.hpp"
#include "LIEF/runtime/android/Process.hpp"
#include "LIEF/runtime/android/Property.hpp"

#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/ParserConfig.hpp"

namespace LIEF::runtime {
namespace details {
class ModuleIt {};
class Module {};
}
}

namespace LIEF::runtime::android {

void* Module::handle() const {
  return nullptr;
}

std::unique_ptr<Module> Module::from_handle(void* /*H*/) {
  return nullptr;
}

void* Module::dlsym(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<ELF::Binary> Module::parse_from_path() const {
  return nullptr;
}

std::unique_ptr<ELF::Binary>
    Module::parse_from_path(const ELF::ParserConfig& /*config*/) const {
  return nullptr;
}

std::unique_ptr<ELF::Binary> Module::parse_from_memory() const {
  return nullptr;
}

std::unique_ptr<ELF::Binary>
    Module::parse_from_memory(const ELF::ParserConfig& /*config*/) const {
  return nullptr;
}

std::unique_ptr<Module> dlopen(const std::string& /*name*/) {
  return nullptr;
}

optional<uint32_t> Host::sdk_version() {
  return nullopt();
}

std::string Process::cmdline() {
  return "";
}

optional<Property> Process::get_system_property(const std::string& /*name*/) {
  return nullopt();
}

Process::properties_t Process::properties() {
  return {};
}

std::string Property::to_string() const {
  return "";
}

Property Property::create_from(const prop_info& /*pi*/) {
  return {};
}

}

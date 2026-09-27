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
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/runtime/linux/Module.hpp"
#include "LIEF/runtime/linux/Host.hpp"
#include "LIEF/runtime/linux/Process.hpp"

namespace LIEF::runtime {
namespace details {
class ModuleIt {};
class Module {};
}
}

namespace LIEF::runtime::Linux {
std::unique_ptr<Module> Module::from_handle(void* /*H*/) {
  return nullptr;
}

void* Module::handle() const {
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

std::unique_ptr<ELF::Binary>
    Module::parse_from_memory(const ELF::ParserConfig& /*config*/) const {
  return nullptr;
}

std::unique_ptr<ELF::Binary> Module::parse_from_memory() const {
  return nullptr;
}

std::unique_ptr<Module> dlopen(const std::string& /*name*/) {
  return nullptr;
}

std::string Process::cmdline() {
  return "";
}

std::string Process::glibc_version() {
  return "";
}

std::string Host::sys_name() {
  return "";
}

std::string Host::sys_release() {
  return "";
}

std::string Host::sys_version() {
  return "";
}

std::string Host::hardware() {
  return "";
}

}

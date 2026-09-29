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
#include "LIEF/runtime/Process.hpp"
#include "LIEF/runtime/Module.hpp"
#include "LIEF/runtime/Memory.hpp"
#include "LIEF/runtime/Host.hpp"

#include "internal_utils.hpp"
#include "logging.hpp"

namespace LIEF::runtime {

namespace details {
class ModuleIt {};
class Module {};
}

// ----------------------------------------------------------------------------
// Host
// ----------------------------------------------------------------------------
std::string Host::name() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return "";
}

std::string Host::home_dir() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return "";
}

std::string Host::tmp_dir() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return "";
}

std::string Host::config_dir() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return "";
}

std::string Host::cache_dir() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return "";
}
// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------

int32_t Process::pid() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return -1;
}

uint32_t Process::tid() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return 0;
}

uint32_t Process::page_size() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return 0;
}

optional<std::string> Process::get_env(const std::string& /*key*/) {
  return nullopt();
}

Process::EnvVars Process::get_envs() {
  return {};
}

// ----------------------------------------------------------------------------
// Module
// ----------------------------------------------------------------------------

Module::Iterator::Iterator(const Iterator&) {}

// NOLINTNEXTLINE(bugprone-unhandled-self-assignment)
Module::Iterator& Module::Iterator::operator=(const Iterator& /*other*/) {
  return *this;
}

Module::Iterator::Iterator(Iterator&&) noexcept = default;
Module::Iterator& Module::Iterator::operator=(Iterator&&) noexcept = default;

Module::Iterator::Iterator(std::unique_ptr<details::ModuleIt>) {}

Module::Iterator::~Iterator() = default;

bool operator==(const Module::Iterator&, const Module::Iterator&) {
  return true;
}

Module::Iterator& Module::Iterator::operator++() {
  return *this;
}

void Module::Iterator::load() const {}

const Module& Module::Iterator::operator*() const {
  return *cached_;
}

const Module* Module::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Module> Module::Iterator::yield() {
  return nullptr;
}

Module::Module(std::unique_ptr<details::Module>) {}

Module::Module(Module&&) noexcept = default;
Module& Module::operator=(Module&&) noexcept = default;

Module::~Module() = default;


std::unique_ptr<Module> Module::clone() const {
  return nullptr;
}

uint64_t Module::imagebase() const {
  return 0;
}

uint64_t Module::size() const {
  return 0;
}

std::string Module::name() const {
  return "";
}

std::string Module::path() const {
  return "";
}

std::string Module::to_string() const {
  return "";
}

modules_t modules() {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return make_empty_iterator<Module>();
}

// ----------------------------------------------------------------------------
// Memory
// ----------------------------------------------------------------------------

uintptr_t Memory::Chunk::page_start() const {
  return 0;
}

uintptr_t Memory::Chunk::page_end() const {
  return 0;
}

Memory::Chunk& Memory::Chunk::cache_flush() {
  return *this;
}

std::string Memory::Chunk::to_string() const {
  return "";
}

optional<Memory::Chunk> Memory::mmap(size_t /*size*/, uint32_t /*flags*/,
                                     uint32_t /*permissions*/) {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return nullopt();
}

ok_error_t Memory::munmap(Chunk& /*C*/) {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return make_error_code(lief_errors::not_implemented);
}

ok_error_t Memory::mprotect(Memory::Chunk& /*C*/, uint32_t /*flags*/) {
  LIEF_ERR(LIEF_NEEDS_RUNTIME_MSG);
  return make_error_code(lief_errors::not_implemented);
}

std::string Memory::perm_str(uint32_t /*flags*/) {
  return "";
}

}

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

#include "LIEF/runtime/windows/Module.hpp"
#include "LIEF/runtime/windows/Host.hpp"
#include "LIEF/runtime/windows/PEB.hpp"
#include "LIEF/runtime/windows/LdrDataTableEntry.hpp"
#include "LIEF/runtime/windows/Process.hpp"
#include "LIEF/runtime/windows/injector.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/optional.hpp"


namespace LIEF::runtime {
namespace details {
class ModuleIt {};
class Module {};
}
}

namespace LIEF::runtime::windows {

namespace details {
class peb {};
class ldr_entry {};
class ldr_entry_it {};
}

std::unique_ptr<Module> dlopen(const std::string& /*name*/) {
  return nullptr;
}

std::unique_ptr<Module> find_module(const std::string& /*name*/) {
  return nullptr;
}

std::unique_ptr<Module> Module::from_handle(void* /*H*/) {
  return nullptr;
}

void* Module::handle() const {
  return nullptr;
}

void* Module::dlsym(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<PE::Binary> Module::parse_from_path() const {
  return nullptr;
}

std::unique_ptr<PE::Binary>
    Module::parse_from_path(const PE::ParserConfig&) const {
  return nullptr;
}

std::unique_ptr<PE::Binary> Module::parse_from_memory() const {
  return nullptr;
}

std::unique_ptr<PE::Binary>
    Module::parse_from_memory(const PE::ParserConfig&) const {
  return nullptr;
}

bool injection_context_t::validate() const {
  return true;
}

std::string injection_context_t::to_string() const {
  return "";
}

ok_error_t inject_spawn(const injection_context_t& /*ctx*/) {
  return make_error_code(lief_errors::not_supported);
}

Host::version_t Host::version() {
  return {};
}

std::unique_ptr<PEB> Process::peb() {
  return nullptr;
}

bool PEB::being_debugged() const {
  return false;
}

uintptr_t PEB::ldr() const {
  return 0;
}

uintptr_t PEB::process_parameters() const {
  return 0;
}

uintptr_t PEB::atl_thunk_slist_ptr() const {
  return 0;
}

uint32_t PEB::atl_thunk_slist_ptr32() const {
  return 0;
}

uintptr_t PEB::post_process_init_routine() const {
  return 0;
}

uint32_t PEB::session_id() const {
  return 0;
}

PEB::PEB(std::unique_ptr<details::peb> /*impl*/) :
  impl_(nullptr) {}

PEB::PEB(PEB&&) noexcept = default;
PEB& PEB::operator=(PEB&&) noexcept = default;

PEB::~PEB() = default;

PEB::entries_it PEB::entries() const {
  return make_range(LdrDataTableEntry::Iterator(), LdrDataTableEntry::Iterator());
}

LdrDataTableEntry::
    LdrDataTableEntry(std::unique_ptr<details::ldr_entry> /*impl*/) :
  impl_(nullptr) {}

LdrDataTableEntry::LdrDataTableEntry(LdrDataTableEntry&&) noexcept = default;
LdrDataTableEntry&
    LdrDataTableEntry::operator=(LdrDataTableEntry&&) noexcept = default;

LdrDataTableEntry::~LdrDataTableEntry() = default;

uintptr_t LdrDataTableEntry::dll_base() const {
  return 0;
}

uintptr_t LdrDataTableEntry::entry_point() const {
  return 0;
}

uint32_t LdrDataTableEntry::size_of_image() const {
  return 0;
}

std::string LdrDataTableEntry::full_dll_name() const {
  return "";
}

std::string LdrDataTableEntry::base_dll_name() const {
  return "";
}

uint32_t LdrDataTableEntry::flags() const {
  return 0;
}

uint16_t LdrDataTableEntry::obsolete_load_count() const {
  return 0;
}

uint16_t LdrDataTableEntry::tls_index() const {
  return 0;
}

uint32_t LdrDataTableEntry::time_date_stamp() const {
  return 0;
}

uintptr_t LdrDataTableEntry::entry_point_activation_context() const {
  return 0;
}

uintptr_t LdrDataTableEntry::lock() const {
  return 0;
}

optional<uintptr_t> LdrDataTableEntry::ddag_node() const {
  return nullopt();
}

optional<uintptr_t> LdrDataTableEntry::load_context() const {
  return nullopt();
}

optional<uintptr_t> LdrDataTableEntry::parent_dll_base() const {
  return nullopt();
}

optional<uintptr_t> LdrDataTableEntry::switch_back_context() const {
  return nullopt();
}

optional<uintptr_t> LdrDataTableEntry::original_base() const {
  return nullopt();
}

optional<int64_t> LdrDataTableEntry::load_time() const {
  return nullopt();
}

optional<uint32_t> LdrDataTableEntry::base_name_hash_value() const {
  return nullopt();
}

optional<int32_t> LdrDataTableEntry::load_reason() const {
  return nullopt();
}

optional<uint32_t> LdrDataTableEntry::implicit_path_options() const {
  return nullopt();
}

optional<uint32_t> LdrDataTableEntry::reference_count() const {
  return nullopt();
}

optional<uint32_t> LdrDataTableEntry::dependent_load_flags() const {
  return nullopt();
}

optional<uint8_t> LdrDataTableEntry::signing_level() const {
  return nullopt();
}

optional<uint32_t> LdrDataTableEntry::check_sum() const {
  return nullopt();
}

optional<uintptr_t> LdrDataTableEntry::active_patch_image_base() const {
  return nullopt();
}

optional<uint32_t> LdrDataTableEntry::hot_patch_state() const {
  return nullopt();
}

std::string LdrDataTableEntry::to_string() const {
  return "";
}

LdrDataTableEntry::Iterator::Iterator() = default;

LdrDataTableEntry::Iterator::
    Iterator(std::unique_ptr<details::ldr_entry_it> /*impl*/) :
  impl_(nullptr) {}

LdrDataTableEntry::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

LdrDataTableEntry::Iterator&
    LdrDataTableEntry::Iterator::operator=(const Iterator& other) {
  if (this != &other) {
    cached_.reset();
  }
  return *this;
}

LdrDataTableEntry::Iterator::Iterator(Iterator&&) noexcept = default;
LdrDataTableEntry::Iterator&
    LdrDataTableEntry::Iterator::operator=(Iterator&&) noexcept = default;

LdrDataTableEntry::Iterator::~Iterator() = default;

bool operator==(const LdrDataTableEntry::Iterator&,
                const LdrDataTableEntry::Iterator&) {
  return true;
}

LdrDataTableEntry::Iterator& LdrDataTableEntry::Iterator::operator++() {
  return *this;
}

LdrDataTableEntry::Iterator& LdrDataTableEntry::Iterator::operator--() {
  return *this;
}

void LdrDataTableEntry::Iterator::load() const {}

const LdrDataTableEntry& LdrDataTableEntry::Iterator::operator*() const {
  return *cached_;
}

const LdrDataTableEntry* LdrDataTableEntry::Iterator::operator->() const {
  return cached_.get();
}

std::unique_ptr<LdrDataTableEntry> LdrDataTableEntry::Iterator::yield() {
  return nullptr;
}

}

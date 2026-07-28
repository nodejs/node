/* Copyright 2022 - 2025 R. Thomas
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
#include "LIEF/DyldSharedCache/DyldSharedCache.hpp"
#include "LIEF/DyldSharedCache/Dylib.hpp"
#include "LIEF/DyldSharedCache/MappingInfo.hpp"
#include "LIEF/DyldSharedCache/SubCache.hpp"
#include "LIEF/DyldSharedCache/caching.hpp"
#include "LIEF/DyldSharedCache/utils.hpp"

#include "LIEF/MachO/Binary.hpp"

#include "logging.hpp"
#include "messages.hpp"
#include "internal_utils.hpp"

namespace LIEF::dsc {
namespace details {
class DyldSharedCache {};
class Dylib {};
class DylibIt {};

class MappingInfo {};
class MappingInfoIt {};

class SubCache {};
class SubCacheIt {};
}

// ----------------------------------------------------------------------------
// utils
// ----------------------------------------------------------------------------
bool is_shared_cache(BinaryStream& /*stream*/) {
  LIEF_ERR(DSC_NOT_SUPPORTED);
  return false;
}

// ----------------------------------------------------------------------------
// caching
// ----------------------------------------------------------------------------
bool enable_cache() {
  LIEF_ERR(DSC_NOT_SUPPORTED);
  return false;
}

bool enable_cache(const std::string&) {
  LIEF_ERR(DSC_NOT_SUPPORTED);
  return false;
}

// ----------------------------------------------------------------------------
// DyldSharedCache/DyldSharedCache.hpp
// ----------------------------------------------------------------------------
DyldSharedCache::DyldSharedCache(std::unique_ptr<details::DyldSharedCache>)
{}

DyldSharedCache::~DyldSharedCache() = default;

std::unique_ptr<DyldSharedCache>
  DyldSharedCache::from_path(const std::string&, const std::string&)
{
  LIEF_ERR(DSC_NOT_SUPPORTED);
  return nullptr;
}

std::unique_ptr<DyldSharedCache>
  DyldSharedCache::from_files(const std::vector<std::string>&)
{
  LIEF_ERR(DSC_NOT_SUPPORTED);
  return nullptr;
}

std::string DyldSharedCache::filename() const {
  return "";
}

DyldSharedCache::VERSION DyldSharedCache::version() const {
  return VERSION::UNKNOWN;
}

std::string DyldSharedCache::filepath() const {
  return "";
}

uint64_t DyldSharedCache::load_address() const {
  return 0;
}

std::string DyldSharedCache::arch_name() const {
  return "";
}

DyldSharedCache::DYLD_TARGET_PLATFORM DyldSharedCache::platform() const {
  return DYLD_TARGET_PLATFORM::UNKNOWN;
}

DyldSharedCache::DYLD_TARGET_ARCH DyldSharedCache::arch() const {
  return DYLD_TARGET_ARCH::UNKNOWN;
}

std::unique_ptr<Dylib> DyldSharedCache::find_lib_from_va(uint64_t) const {
  return nullptr;
}

std::unique_ptr<Dylib> DyldSharedCache::find_lib_from_path(const std::string&) const {
  return nullptr;
}

std::unique_ptr<Dylib> DyldSharedCache::find_lib_from_name(const std::string&) const {
  return nullptr;
}

bool DyldSharedCache::has_subcaches() const {
  return false;
}

DyldSharedCache::instructions_iterator DyldSharedCache::disassemble(uint64_t/*va*/) const {
  return make_range<assembly::Instruction::Iterator>(
      assembly::Instruction::Iterator(),
      assembly::Instruction::Iterator()
  );
}

std::vector<uint8_t> DyldSharedCache::get_content_from_va(uint64_t/*va*/, uint64_t/*size*/) const {
  return {};
}

std::unique_ptr<DyldSharedCache> DyldSharedCache::cache_for_address(uint64_t/*va*/) const {
  return nullptr;
}

std::unique_ptr<DyldSharedCache> DyldSharedCache::main_cache() const {
  return nullptr;
}

std::unique_ptr<DyldSharedCache> DyldSharedCache::find_subcache(const std::string&/*filename*/) const {
  return nullptr;
}

result<uint64_t> DyldSharedCache::va_to_offset(uint64_t/*va*/) const {
  return make_error_code(lief_errors::not_implemented);
}

DyldSharedCache::dylib_iterator DyldSharedCache::libraries() const {
  return make_empty_iterator<Dylib>();
}

FileStream& DyldSharedCache::stream() const {
  static FileStream* FS = nullptr;
  // This null deref is **on purpose**
  return *FS; // NOLINT(clang-analyzer-core.uninitialized.UndefReturn)
}

FileStream& DyldSharedCache::stream() {
  static FileStream* FS = nullptr;
  // This null deref is **on purpose**
  return *FS; // NOLINT(clang-analyzer-core.uninitialized.UndefReturn)
}

DyldSharedCache::mapping_info_iterator DyldSharedCache::mapping_info() const {
  return make_empty_iterator<MappingInfo>();
}

DyldSharedCache::subcache_iterator DyldSharedCache::subcaches() const {
  return make_empty_iterator<SubCache>();
}

void DyldSharedCache::enable_caching(const std::string&) const {}
void DyldSharedCache::flush_cache() const {}

// ----------------------------------------------------------------------------
// DyldSharedCache/Dylib.hpp
// ----------------------------------------------------------------------------

Dylib::Dylib::extract_opt_t::extract_opt_t() = default;

Dylib::Iterator::Iterator(std::unique_ptr<details::DylibIt>) {}

Dylib::Iterator::Iterator(Dylib::Iterator&&) noexcept {}

Dylib::Iterator& Dylib::Iterator::operator=(Dylib::Iterator&&) noexcept {
  return *this;
}

Dylib::Iterator::Iterator(const Dylib::Iterator&) {}
Dylib::Iterator& Dylib::Iterator::operator=(const Dylib::Iterator&) {
  return *this;
}

Dylib::Iterator::~Iterator() = default;
bool Dylib::Iterator::operator<(const Dylib::Iterator&) const {
  return false;
}

std::ptrdiff_t Dylib::Iterator::operator-(const Iterator&) const {
  return 0;
}

Dylib::Iterator& Dylib::Iterator::operator+=(std::ptrdiff_t) {
  return *this;
}

Dylib::Iterator& Dylib::Iterator::operator-=(std::ptrdiff_t) {
  return *this;
}

bool operator==(const Dylib::Iterator&, const Dylib::Iterator&) {
  return false;
}

std::unique_ptr<Dylib> Dylib::Iterator::operator*() const {
  return nullptr;
}


Dylib::Dylib(std::unique_ptr<details::Dylib>) {}

Dylib::~Dylib() = default;

std::unique_ptr<LIEF::MachO::Binary> Dylib::get(const Dylib::extract_opt_t&) const {
  return nullptr;
}

std::string Dylib::path() const {
  return "";
}

uint64_t Dylib::address() const {
  return 0;
}

uint64_t Dylib::modtime() const {
  return 0;
}

uint64_t Dylib::inode() const {
  return 0;
}

uint64_t Dylib::padding() const {
  return 0;
}

// ----------------------------------------------------------------------------
// DyldSharedCache/MappingInfo.hpp
// ----------------------------------------------------------------------------
MappingInfo::Iterator::Iterator(std::unique_ptr<details::MappingInfoIt>) {}

MappingInfo::Iterator::Iterator(MappingInfo::Iterator&&) noexcept {}

MappingInfo::Iterator& MappingInfo::Iterator::operator=(MappingInfo::Iterator&&) noexcept {
  return *this;
}

MappingInfo::Iterator::Iterator(const MappingInfo::Iterator&) {}
MappingInfo::Iterator& MappingInfo::Iterator::operator=(const MappingInfo::Iterator&) {
  return *this;
}

MappingInfo::Iterator::~Iterator() = default;
bool MappingInfo::Iterator::operator<(const MappingInfo::Iterator&) const {
  return false;
}

std::ptrdiff_t MappingInfo::Iterator::operator-(const Iterator&) const {
  return 0;
}

MappingInfo::Iterator& MappingInfo::Iterator::operator+=(std::ptrdiff_t) {
  return *this;
}

MappingInfo::Iterator& MappingInfo::Iterator::operator-=(std::ptrdiff_t) {
  return *this;
}

bool operator==(const MappingInfo::Iterator&, const MappingInfo::Iterator&) {
  return false;
}

std::unique_ptr<MappingInfo> MappingInfo::Iterator::operator*() const {
  return nullptr;
}

MappingInfo::MappingInfo(std::unique_ptr<details::MappingInfo>) {}
MappingInfo::~MappingInfo() = default;

uint64_t MappingInfo::address() const {
  return 0;
}

uint64_t MappingInfo::size() const {
  return 0;
}

uint64_t MappingInfo::file_offset() const {
  return 0;
}

uint32_t MappingInfo::max_prot() const {
  return 0;
}

uint32_t MappingInfo::init_prot() const {
  return 0;
}


// ----------------------------------------------------------------------------
// DyldSharedCache/SubCache.hpp
// ----------------------------------------------------------------------------
SubCache::Iterator::Iterator(std::unique_ptr<details::SubCacheIt>) {}

SubCache::Iterator::Iterator(SubCache::Iterator&&) noexcept {}

SubCache::Iterator& SubCache::Iterator::operator=(SubCache::Iterator&&) noexcept {
  return *this;
}

SubCache::Iterator::Iterator(const SubCache::Iterator&) {}
SubCache::Iterator& SubCache::Iterator::operator=(const SubCache::Iterator&) {
  return *this;
}

SubCache::Iterator::~Iterator() = default;
bool SubCache::Iterator::operator<(const SubCache::Iterator&) const {
  return false;
}

std::ptrdiff_t SubCache::Iterator::operator-(const Iterator&) const {
  return 0;
}

SubCache::Iterator& SubCache::Iterator::operator+=(std::ptrdiff_t) {
  return *this;
}

SubCache::Iterator& SubCache::Iterator::operator-=(std::ptrdiff_t) {
  return *this;
}

bool operator==(const SubCache::Iterator&, const SubCache::Iterator&) {
  return false;
}

std::unique_ptr<SubCache> SubCache::Iterator::operator*() const {
  return nullptr;
}

SubCache::SubCache(std::unique_ptr<details::SubCache>) {}
SubCache::~SubCache() = default;

sc_uuid_t SubCache::uuid() const {
  return {};
}

uint64_t SubCache::vm_offset() const {
  return 0;
}

std::string SubCache::suffix() const {
  return "";
}

std::unique_ptr<const DyldSharedCache> SubCache::cache() const {
  return nullptr;
}

std::ostream& operator<<(std::ostream& os, const SubCache&) {
  return os;
}

}

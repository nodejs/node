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

#include "LIEF/OAT/DexFile.hpp"
#include "LIEF/OAT/hash.hpp"
#include <ostream>

namespace LIEF {
namespace OAT {

DexFile::DexFile(const DexFile&) = default;
DexFile& DexFile::operator=(const DexFile&) = default;

DexFile::DexFile() = default;

const std::string& DexFile::location() const {
  return location_;
}

uint32_t DexFile::checksum() const {
  return checksum_;
}

uint32_t DexFile::dex_offset() const {
  return dex_offset_;
}

bool DexFile::has_dex_file() const {
  return dex_file_ != nullptr;
}

const DEX::File* DexFile::dex_file() const {
  return dex_file_;
}

void DexFile::location(const std::string& location) {
  location_ = location;
}

void DexFile::checksum(uint32_t checksum) {
  checksum_ = checksum;
}

void DexFile::dex_offset(uint32_t dex_offset) {
  dex_offset_ = dex_offset;
}

const std::vector<uint32_t>& DexFile::classes_offsets() const {
  return classes_offsets_;
}


// Android 7.X.X and Android 8.0.0
// ===============================
uint32_t DexFile::lookup_table_offset() const {
  return lookup_table_offset_;
}

void DexFile::lookup_table_offset(uint32_t offset) {
  lookup_table_offset_ = offset;
}
// ===============================

DEX::File* DexFile::dex_file() {
  return const_cast<DEX::File*>(static_cast<const DexFile*>(this)->dex_file());
}

void DexFile::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const DexFile& dex_file) {
  os << dex_file.location() << " - " << std::hex << std::showbase << "(Checksum: " << dex_file.checksum() << ")";
  return os;
}

DexFile::~DexFile() = default;



}
}

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
#ifndef LIEF_OAT_DEXFILE_H
#define LIEF_OAT_DEXFILE_H

#include <cstdint>
#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
class Visitor;

namespace DEX {
class File;
}

namespace OAT {
class Parser;

class LIEF_API DexFile : public Object {
  friend class Parser;

  public:
  DexFile();
  DexFile(const DexFile&);
  DexFile& operator=(const DexFile&);

  const std::string& location() const;

  uint32_t checksum() const;
  uint32_t dex_offset() const;

  bool has_dex_file() const;

  const DEX::File* dex_file() const;
  DEX::File* dex_file();

  void location(const std::string& location);
  void checksum(uint32_t checksum);
  void dex_offset(uint32_t dex_offset);

  const std::vector<uint32_t>& classes_offsets() const;

  // Android 7.X.X and Android 8.0.0
  // ===============================
  uint32_t lookup_table_offset() const;

  void class_offsets_offset(uint32_t offset);
  void lookup_table_offset(uint32_t offset);
  // ===============================

  void accept(Visitor& visitor) const override;


  ~DexFile() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const DexFile& dex_file);

  private:
  std::string location_;
  uint32_t checksum_ = 0;
  uint32_t dex_offset_ = 0;

  DEX::File* dex_file_ = nullptr;

  // OAT 64 (Android 6.X.X)
  std::vector<uint32_t> classes_offsets_;

  // OAT 79 / 88 (Android 7.X.X)
  uint32_t lookup_table_offset_ = 0;

  // OAT 131 (Android 8.1.0)
  uint32_t method_bss_mapping_offset_ = 0;
  uint32_t dex_sections_layout_offset_ = 0;
};

} // Namespace OAT
} // Namespace LIEF
#endif

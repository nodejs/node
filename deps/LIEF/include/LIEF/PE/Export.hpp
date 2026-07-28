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
#ifndef LIEF_PE_EXPORT_H
#define LIEF_PE_EXPORT_H

#include <ostream>
#include <string>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/ExportEntry.hpp"

namespace LIEF {
namespace PE {

class Builder;
class Parser;

namespace details {
struct pe_export_directory_table;
}

/// Class which represents a PE Export
class LIEF_API Export : public Object {
  friend class Builder;
  friend class Parser;

  public:
  using entries_t        = std::vector<std::unique_ptr<ExportEntry>>;
  using it_entries       = ref_iterator<entries_t&, ExportEntry*>;
  using it_const_entries = const_ref_iterator<const entries_t&, const ExportEntry*>;

  Export() = default;

  Export(std::string name, const std::vector<ExportEntry>& entries) :
    name_(std::move(name))
  {
    for (const ExportEntry& E : entries) {
      add_entry(E);
    }
  }

  Export(std::string name) :
    Export(std::move(name), {})
  {}

  Export(const details::pe_export_directory_table& header);

  Export(const Export&);
  Export& operator=(const Export&);

  Export(Export&&) = default;
  Export& operator=(Export&&) = default;

  ~Export() override = default;

  /// According to the PE specifications this value is reserved
  /// and should be set to 0
  uint32_t export_flags() const {
    return export_flags_;
  }

  /// The time and date that the export data was created
  uint32_t timestamp() const {
    return timestamp_;
  }

  /// The major version number (can be user-defined)
  uint16_t major_version() const {
    return major_version_;
  }

  /// The minor version number (can be user-defined)
  uint16_t minor_version() const {
    return minor_version_;
  }

  /// The starting number for the exports. Usually this value is set to 1
  uint32_t ordinal_base() const {
    return ordinal_base_;
  }

  /// The name of the library exported (e.g. `KERNEL32.dll`)
  const std::string& name() const {
    return name_;
  }

  /// Iterator over the ExportEntry
  it_entries entries() {
    return entries_;
  }

  it_const_entries entries() const {
    return entries_;
  }

  /// Address of the ASCII DLL's name (RVA)
  uint32_t name_rva() const {
    return name_rva_;
  }

  /// RVA of the export address table
  uint32_t export_addr_table_rva() const {
    return exp_addr_table_rva_;
  }

  /// Number of entries in the export address table
  uint32_t export_addr_table_cnt() const {
    return exp_addr_table_cnt_;
  }

  /// RVA to the list of exported names
  uint32_t names_addr_table_rva() const {
    return names_addr_table_rva_;
  }

  /// Number of exports by name
  uint32_t names_addr_table_cnt() const {
    return names_addr_table_cnt_;
  }

  /// RVA to the list of exported ordinals
  uint32_t ord_addr_table_rva() const {
    return ord_addr_table_rva_;
  }

  void export_flags(uint32_t flags) {
    export_flags_ = flags;
  }

  void timestamp(uint32_t timestamp) {
    timestamp_ = timestamp;
  }

  void major_version(uint16_t major_version) {
    major_version_ = major_version;
  }

  void minor_version(uint16_t minor_version) {
    minor_version_ = minor_version;
  }

  void ordinal_base(uint32_t ordinal_base) {
    ordinal_base_ = ordinal_base;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  /// Find the export entry with the given name
  const ExportEntry* find_entry(const std::string& name) const;

  ExportEntry* find_entry(const std::string& name) {
    return const_cast<ExportEntry*>(static_cast<const Export*>(this)->find_entry(name));
  }

  /// Find the export entry with the given ordinal number
  const ExportEntry* find_entry(uint32_t ordinal) const;

  ExportEntry* find_entry(uint32_t ordinal) {
    return const_cast<ExportEntry*>(static_cast<const Export*>(this)->find_entry(ordinal));
  }

  /// Find the export entry at the provided RVA
  const ExportEntry* find_entry_at(uint32_t rva) const;

  ExportEntry* find_entry_at(uint32_t rva) {
    return const_cast<ExportEntry*>(static_cast<const Export*>(this)->find_entry_at(rva));
  }

  /// Add the given export and return the newly created and added export
  ExportEntry& add_entry(const ExportEntry& exp);

  ExportEntry& add_entry(std::string name, uint32_t rva) {
    return add_entry(ExportEntry(std::move(name), rva));
  }

  /// Remove the given export entry
  bool remove_entry(const ExportEntry& exp);

  /// Remove the export entry with the given name
  bool remove_entry(const std::string& name) {
    if (const ExportEntry* entry = find_entry(name)) {
      return remove_entry(*entry);
    }
    return false;
  }

  /// Remove the export entry with the given RVA
  bool remove_entry(uint32_t rva) {
    if (const ExportEntry* entry = find_entry_at(rva)) {
      return remove_entry(*entry);
    }
    return false;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Export& exp);

  private:
  uint32_t export_flags_ = 0;
  uint32_t name_rva_ = 0;
  uint32_t timestamp_ = 0;
  uint16_t major_version_ = 0;
  uint16_t minor_version_ = 0;
  uint32_t ordinal_base_ = 0;
  uint32_t exp_addr_table_rva_ = 0;
  uint32_t exp_addr_table_cnt_ = 0;
  uint32_t names_addr_table_rva_ = 0;
  uint32_t names_addr_table_cnt_ = 0;
  uint32_t ord_addr_table_rva_ = 0;
  uint32_t max_ordinal_ = 0;
  entries_t entries_;
  std::string name_;
};

}
}

#endif /* PE_EXPORT_H */

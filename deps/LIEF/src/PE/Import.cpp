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
#include <algorithm>
#include <iomanip>

#include "LIEF/Visitor.hpp"

#include "LIEF/PE/ImportEntry.hpp"
#include "LIEF/PE/Import.hpp"
#include "PE/Structures.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

Import::Import(const details::pe_import& import) :
  ilt_rva_(import.ImportLookupTableRVA),
  timedatestamp_(import.TimeDateStamp),
  forwarder_chain_(import.ForwarderChain),
  name_rva_(import.NameRVA),
  iat_rva_(import.ImportAddressTableRVA)
{}


Import::Import(const Import& other) :
  Object(other),
  directory_(other.directory_),
  iat_directory_(other.iat_directory_),
  ilt_rva_(other.ilt_rva_),
  timedatestamp_(other.timedatestamp_),
  forwarder_chain_(other.forwarder_chain_),
  name_rva_(other.name_rva_),
  iat_rva_(other.iat_rva_),
  name_(other.name_),
  type_(other.type_),
  nb_original_func_(other.nb_original_func_)
{
  if (!other.entries_.empty()) {
    entries_.reserve(other.entries_.size());
    for (const ImportEntry& entry : other.entries()) {
      entries_.emplace_back(new ImportEntry(entry));
    }
  }
}


Import& Import::operator=(const Import& other) {
  if (&other == this) {
    return *this;
  }

  directory_ = other.directory_;
  iat_directory_ = other.iat_directory_;
  ilt_rva_ = other.ilt_rva_;
  timedatestamp_ = other.timedatestamp_;
  forwarder_chain_ = other.forwarder_chain_;
  name_rva_ = other.name_rva_;
  iat_rva_ = other.iat_rva_;
  name_ = other.name_;
  type_ = other.type_;
  nb_original_func_ = other.nb_original_func_;

  if (!other.entries_.empty()) {
    entries_.reserve(other.entries_.size());
    for (const ImportEntry& entry : other.entries()) {
      entries_.emplace_back(new ImportEntry(entry));
    }
  }

  return *this;
}

bool Import::remove_entry(const std::string& name) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
    [&name] (const std::unique_ptr<ImportEntry>& entry) {
      return entry->name() == name;
    }
  );
  if (it == entries_.end()) {
    return false;
  }

  entries_.erase(it);
  return true;
}

bool Import::remove_entry(uint32_t ordinal) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
    [ordinal] (const std::unique_ptr<ImportEntry>& entry) {
      return entry->is_ordinal() && entry->ordinal() == ordinal;
    }
  );
  if (it == entries_.end()) {
    return false;
  }

  entries_.erase(it);
  return true;
}

const ImportEntry* Import::get_entry(const std::string& name) const {
  const auto it_entry = std::find_if(std::begin(entries_), std::end(entries_),
      [&name] (const std::unique_ptr<ImportEntry>& entry) {
        return entry->name() == name;
      });
  if (it_entry == std::end(entries_)) {
    return nullptr;
  }
  return &**it_entry;
}

result<uint32_t> Import::get_function_rva_from_iat(const std::string& function) const {
  const auto it_function = std::find_if(std::begin(entries_), std::end(entries_),
      [&function] (const std::unique_ptr<ImportEntry>& entry) {
        return entry->name() == function;
      });

  if (it_function == std::end(entries_)) {
    return make_error_code(lief_errors::not_found);
  }

  // Index of the function in the imported functions
  uint32_t idx = std::distance(std::begin(entries_), it_function);

  if (type_ == PE_TYPE::PE32) {
    return idx * sizeof(uint32_t);
  }
  return idx * sizeof(uint64_t);
}

void Import::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Import& entry) {
  os << fmt::format(
  R"delim(
  Name: {} {{
    Name(RVA): 0x{:06x}
    IAT(RVA):  0x{:06x}
    ILT(RVA):  0x{:06x}
    FWD Chain: 0x{:06x}
    Timestamp: 0x{:06x}
  }}
  )delim",
  entry.name(), entry.name_rva_, entry.import_address_table_rva(),
  entry.import_lookup_table_rva(), entry.forwarder_chain(),
  entry.timedatestamp());
  os << '\n';
  for (const ImportEntry& functions: entry.entries()) {
    os << "    " << functions << '\n';
  }

  return os;
}
}
}

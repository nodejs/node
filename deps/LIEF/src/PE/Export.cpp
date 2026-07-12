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
#include "LIEF/Visitor.hpp"
#include "LIEF/config.h"

#include "LIEF/PE/Export.hpp"
#include "LIEF/PE/ExportEntry.hpp"

#include "PE/Structures.hpp"
#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

Export::Export(const details::pe_export_directory_table& header) :
  export_flags_{header.ExportFlags},
  name_rva_{header.NameRVA},
  timestamp_{header.Timestamp},
  major_version_{header.MajorVersion},
  minor_version_{header.MinorVersion},
  ordinal_base_{header.OrdinalBase},
  exp_addr_table_rva_{header.ExportAddressTableRVA},
  exp_addr_table_cnt_{header.AddressTableEntries},
  names_addr_table_rva_{header.NamePointerRVA},
  names_addr_table_cnt_{header.NumberOfNamePointers},
  ord_addr_table_rva_{header.OrdinalTableRVA}
{}


Export::Export(const Export& other) :
  Object(other),
  export_flags_(other.export_flags_),
  name_rva_(other.name_rva_),
  timestamp_(other.timestamp_),
  major_version_(other.major_version_),
  minor_version_(other.minor_version_),
  ordinal_base_(other.ordinal_base_),
  exp_addr_table_rva_(other.exp_addr_table_rva_),
  exp_addr_table_cnt_(other.exp_addr_table_cnt_),
  names_addr_table_rva_(other.names_addr_table_rva_),
  names_addr_table_cnt_(other.names_addr_table_cnt_),
  ord_addr_table_rva_(other.ord_addr_table_rva_),
  max_ordinal_(other.max_ordinal_),
  name_(other.name_)
{
  if (!other.entries_.empty()) {
    entries_.reserve(other.entries_.size());
    for (const ExportEntry& entry : other.entries()) {
      entries_.emplace_back(new ExportEntry(entry));
    }
  }
}

Export& Export::operator=(const Export& other) {
  if (&other == this) {
    return *this;
  }

  export_flags_ = other.export_flags_;
  name_rva_ = other.name_rva_;
  timestamp_ = other.timestamp_;
  major_version_ = other.major_version_;
  minor_version_ = other.minor_version_;
  ordinal_base_ = other.ordinal_base_;
  exp_addr_table_rva_ = other.exp_addr_table_rva_;
  exp_addr_table_cnt_ = other.exp_addr_table_cnt_;
  names_addr_table_rva_ = other.names_addr_table_rva_;
  names_addr_table_cnt_ = other.names_addr_table_cnt_;
  ord_addr_table_rva_ = other.ord_addr_table_rva_;
  max_ordinal_ = other.max_ordinal_;
  name_ = other.name_;

  if (!other.entries_.empty()) {
    entries_.reserve(other.entries_.size());
    for (const ExportEntry& entry : other.entries()) {
      entries_.emplace_back(new ExportEntry(entry));
    }
  }
  return *this;
}

void Export::accept(Visitor& visitor) const {
  visitor.visit(*this);
}


const ExportEntry* Export::find_entry(const std::string& name) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
    [&name] (const std::unique_ptr<ExportEntry>& E) {
      if constexpr (lief_extended) {
        return E->name() == name || E->demangled_name() == name;
      } else {
        return E->name() == name;
      }
    }
  );

  if (it == entries_.end()) {
    return nullptr;
  }

  return &**it;
}

const ExportEntry* Export::find_entry(uint32_t ordinal) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
    [ordinal] (const std::unique_ptr<ExportEntry>& E) {
      return E->ordinal() == ordinal;
    }
  );

  if (it == entries_.end()) {
    return nullptr;
  }

  return &**it;
}

const ExportEntry* Export::find_entry_at(uint32_t rva) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
    [rva] (const std::unique_ptr<ExportEntry>& E) {
      return E->address() == rva;
    }
  );

  if (it == entries_.end()) {
    return nullptr;
  }

  return &**it;
}

ExportEntry& Export::add_entry(const ExportEntry& exp) {
  entries_.emplace_back(new ExportEntry(exp));
  ++max_ordinal_;
  ExportEntry& new_entry = *entries_.back();
  new_entry.ordinal(max_ordinal_);
  return new_entry;
}


bool Export::remove_entry(const ExportEntry& exp) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
    [&exp] (const std::unique_ptr<ExportEntry>& E) { return &exp == E.get(); }
  );

  if (it == entries_.end()) {
    return false;
  }

  entries_.erase(it);
  return true;
}


std::ostream& operator<<(std::ostream& os, const Export& exp) {
  using namespace fmt;
  static constexpr auto WIDTH = 20;
  os << format("DLL Name: {}\n", exp.name())
     << format("  {:{}} 0x{:08x}\n", "Characteristics", WIDTH, exp.export_flags())
     << format("  {:{}} {} ({})\n", "Timestamp", WIDTH,
               exp.timestamp(), ts_to_str(exp.timestamp()))
     << format("  {:{}} {}.{}\n", "Version", WIDTH, exp.major_version(),
               exp.minor_version())
     << format("  {:{}} {}\n", "Ordinal Base", WIDTH, exp.ordinal_base())
     << format("  {:{}} {}\n", "Number of functions", WIDTH, exp.export_addr_table_cnt())
     << format("  {:{}} {}\n", "Number of names", WIDTH, exp.names_addr_table_cnt());
  for (const ExportEntry& E : exp.entries()) {
    os << "    " << E << '\n';
  }
  return os;
}

}
}

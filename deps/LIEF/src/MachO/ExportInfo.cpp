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
#include "fmt_formatter.hpp"

#include <algorithm>

#include "frozen.hpp"

#include "LIEF/MachO/hash.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/ExportInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"


FMT_FORMATTER(LIEF::MachO::ExportInfo::FLAGS, LIEF::MachO::to_string);
FMT_FORMATTER(LIEF::MachO::ExportInfo::KIND, LIEF::MachO::to_string);

namespace LIEF {
namespace MachO {

static constexpr auto ARRAY_FLAGS = {
  ExportInfo::FLAGS::WEAK_DEFINITION,
  ExportInfo::FLAGS::REEXPORT,
  ExportInfo::FLAGS::STUB_AND_RESOLVER,
};

ExportInfo& ExportInfo::operator=(ExportInfo other) {
  swap(other);
  return *this;
}

ExportInfo::ExportInfo(const ExportInfo& other) :
  Object{other},
  node_offset_{other.node_offset_},
  flags_{other.flags_},
  address_{other.address_},
  other_{other.other_}
{}

void ExportInfo::swap(ExportInfo& other) noexcept {
  std::swap(node_offset_,    other.node_offset_);
  std::swap(flags_,          other.flags_);
  std::swap(address_,        other.address_);
  std::swap(other_,          other.other_);
  std::swap(symbol_,         other.symbol_);
  std::swap(alias_,          other.alias_);
  std::swap(alias_location_, other.alias_location_);
}

bool ExportInfo::has(FLAGS flag) const {
  return (flags_ & static_cast<uint64_t>(flag)) != 0u;
}

ExportInfo::flag_list_t ExportInfo::flags_list() const {
  flag_list_t flags;

  std::copy_if(std::begin(ARRAY_FLAGS), std::end(ARRAY_FLAGS),
               std::back_inserter(flags),
               [this] (FLAGS f) { return has(f); });

  return flags;
}

void ExportInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ExportInfo& info) {
  const ExportInfo::flag_list_t& flags = info.flags_list();
  os << fmt::format(
    "offset=0x{:x}, flags={}, address=0x{:x}, kind={}",
    info.node_offset(), flags, info.address(), info.kind()
  );
  if (const Symbol* sym = info.symbol()) {
    os << fmt::format(" symbol={}", sym->name());
  }

  if (const Symbol* alias = info.alias()) {
    os << fmt::format(" alias={}", alias->name());
    if (const DylibCommand* lib = info.alias_library()) {
      os << fmt::format(" library={}", lib->name());
    }
  }
  return os;
}

const char* to_string(ExportInfo::KIND e) {
  #define ENTRY(X) std::pair(ExportInfo::KIND::X, #X)
  STRING_MAP enums2str {
    ENTRY(REGULAR),
    ENTRY(THREAD_LOCAL_KIND),
    ENTRY(ABSOLUTE_KIND),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(ExportInfo::FLAGS e) {
  #define ENTRY(X) std::pair(ExportInfo::FLAGS::X, #X)
  STRING_MAP enums2str {
    ENTRY(WEAK_DEFINITION),
    ENTRY(REEXPORT),
    ENTRY(STUB_AND_RESOLVER),
    ENTRY(STATIC_RESOLVER),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}


}
}

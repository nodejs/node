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
#include "LIEF/ELF/DynamicEntryFlags.hpp"

#include "frozen.hpp"
#include "logging.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::ELF::DynamicEntryFlags::FLAG, LIEF::ELF::to_string);

namespace LIEF {
namespace ELF {

static constexpr auto DF_FLAGS = {
  DynamicEntryFlags::FLAG::ORIGIN, DynamicEntryFlags::FLAG::SYMBOLIC,
  DynamicEntryFlags::FLAG::TEXTREL, DynamicEntryFlags::FLAG::BIND_NOW,
  DynamicEntryFlags::FLAG::STATIC_TLS,
};

static constexpr auto DF_FLAGS_1 = {
  DynamicEntryFlags::FLAG::NOW,
  DynamicEntryFlags::FLAG::GLOBAL,
  DynamicEntryFlags::FLAG::GROUP,
  DynamicEntryFlags::FLAG::NODELETE,
  DynamicEntryFlags::FLAG::LOADFLTR,
  DynamicEntryFlags::FLAG::INITFIRST,
  DynamicEntryFlags::FLAG::NOOPEN,
  DynamicEntryFlags::FLAG::HANDLE_ORIGIN,
  DynamicEntryFlags::FLAG::DIRECT,
  DynamicEntryFlags::FLAG::TRANS,
  DynamicEntryFlags::FLAG::INTERPOSE,
  DynamicEntryFlags::FLAG::NODEFLIB,
  DynamicEntryFlags::FLAG::NODUMP,
  DynamicEntryFlags::FLAG::CONFALT,
  DynamicEntryFlags::FLAG::ENDFILTEE,
  DynamicEntryFlags::FLAG::DISPRELDNE,
  DynamicEntryFlags::FLAG::DISPRELPND,
  DynamicEntryFlags::FLAG::NODIRECT,
  DynamicEntryFlags::FLAG::IGNMULDEF,
  DynamicEntryFlags::FLAG::NOKSYMS,
  DynamicEntryFlags::FLAG::NOHDR,
  DynamicEntryFlags::FLAG::EDITED,
  DynamicEntryFlags::FLAG::NORELOC,
  DynamicEntryFlags::FLAG::SYMINTPOSE,
  DynamicEntryFlags::FLAG::GLOBAUDIT,
  DynamicEntryFlags::FLAG::SINGLETON,
  DynamicEntryFlags::FLAG::PIE,
  DynamicEntryFlags::FLAG::KMOD,
  DynamicEntryFlags::FLAG::WEAKFILTER,
  DynamicEntryFlags::FLAG::NOCOMMON,
};

bool DynamicEntryFlags::has(DynamicEntryFlags::FLAG f) const {
  if (tag() == DynamicEntry::TAG::FLAGS) {
    auto raw = static_cast<uint64_t>(f);
    if (BASE <= raw) {
      return false;
    }
    return (value() & raw) > 0;
  }

  if (tag() == DynamicEntry::TAG::FLAGS_1) {
    auto raw = static_cast<uint64_t>(f);
    if (raw < BASE) {
      return false;
    }
    raw -= BASE;
    return (value() & raw) > 0;
  }

  return false;
}


DynamicEntryFlags::flags_list_t DynamicEntryFlags::flags() const {
  flags_list_t flags;

  if (tag() == DynamicEntry::TAG::FLAGS) {
    for (DynamicEntryFlags::FLAG f : DF_FLAGS) {
      if (has(f)) {
        flags.push_back(f);
      }
    }
    return flags;
  }

  if (tag() == DynamicEntry::TAG::FLAGS_1) {
    for (DynamicEntryFlags::FLAG f : DF_FLAGS_1) {
      if (has(f)) {
        flags.push_back(f);
      }
    }
    return flags;
  }

  return flags;
}

void DynamicEntryFlags::add(DynamicEntryFlags::FLAG f) {
  if (tag() == DynamicEntry::TAG::FLAGS) {
    auto raw = static_cast<uint64_t>(f);
    if (BASE <= raw) {
      return;
    }
    value(value() | raw);
    return;
  }

  if (tag() == DynamicEntry::TAG::FLAGS_1) {
    auto raw = static_cast<uint64_t>(f);
    if (raw < BASE) {
      return;
    }
    raw -= BASE;
    value(value() | raw);
    return;
  }
  return;
}

void DynamicEntryFlags::remove(DynamicEntryFlags::FLAG f) {
  if (tag() == DynamicEntry::TAG::FLAGS) {
    auto raw = static_cast<uint64_t>(f);
    if (BASE <= raw) {
      return;
    }
    value(value() & ~raw);
  }

  if (tag() == DynamicEntry::TAG::FLAGS_1) {
    auto raw = static_cast<uint64_t>(f);
    if (raw < BASE) {
      return;
    }
    raw -= BASE;
    value(value() & ~raw);
  }
}

void DynamicEntryFlags::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DynamicEntryFlags::print(std::ostream& os) const {
  DynamicEntry::print(os);
  os << fmt::to_string(flags());
  return os;
}

const char* to_string(DynamicEntryFlags::FLAG flag) {
  #define ENTRY(X) std::pair(DynamicEntryFlags::FLAG::X, #X)
  STRING_MAP enums2str {
    ENTRY(ORIGIN),
    ENTRY(SYMBOLIC),
    ENTRY(TEXTREL),
    ENTRY(BIND_NOW),
    ENTRY(STATIC_TLS),
    ENTRY(NOW),
    ENTRY(GLOBAL),
    ENTRY(GROUP),
    ENTRY(NODELETE),
    ENTRY(LOADFLTR),
    ENTRY(INITFIRST),
    ENTRY(NOOPEN),
    ENTRY(HANDLE_ORIGIN),
    ENTRY(DIRECT),
    ENTRY(TRANS),
    ENTRY(INTERPOSE),
    ENTRY(NODEFLIB),
    ENTRY(NODUMP),
    ENTRY(CONFALT),
    ENTRY(ENDFILTEE),
    ENTRY(DISPRELDNE),
    ENTRY(DISPRELPND),
    ENTRY(NODIRECT),
    ENTRY(IGNMULDEF),
    ENTRY(NOKSYMS),
    ENTRY(NOHDR),
    ENTRY(EDITED),
    ENTRY(NORELOC),
    ENTRY(SYMINTPOSE),
    ENTRY(GLOBAUDIT),
    ENTRY(SINGLETON),
    ENTRY(PIE),
    ENTRY(KMOD),
    ENTRY(WEAKFILTER),
    ENTRY(NOCOMMON),
  };
  #undef ENTRY
  if (auto it = enums2str.find(flag); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}


}
}




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
#ifndef LIEF_ELF_DYNAMIC_ENTRY_FLAGS_H
#define LIEF_ELF_DYNAMIC_ENTRY_FLAGS_H

#include <vector>
#include <ostream>
#include <numeric>

#include "LIEF/visibility.h"
#include "LIEF/ELF/DynamicEntry.hpp"

namespace LIEF {
namespace ELF {

class LIEF_API DynamicEntryFlags : public DynamicEntry {
  public:
  static constexpr uint64_t BASE = 0x100000000;

  enum class FLAG : uint64_t {
    ORIGIN        = 0x00000001, /**< The object may reference $ORIGIN. */
    SYMBOLIC      = 0x00000002, /**< Search the shared lib before searching the exe. */
    TEXTREL       = 0x00000004, /**< Relocations may modify a non-writable segment. */
    BIND_NOW      = 0x00000008, /**< Process all relocations on load. */
    STATIC_TLS    = 0x00000010, /**< Reject attempts to load dynamically. */

    NOW           = BASE + 0x000000001, /**< Set RTLD_NOW for this object. */
    GLOBAL        = BASE + 0x000000002, /**< Set RTLD_GLOBAL for this object. */
    GROUP         = BASE + 0x000000004, /**< Set RTLD_GROUP for this object. */
    NODELETE      = BASE + 0x000000008, /**< Set RTLD_NODELETE for this object. */
    LOADFLTR      = BASE + 0x000000010, /**< Trigger filtee loading at runtime. */
    INITFIRST     = BASE + 0x000000020, /**< Set RTLD_INITFIRST for this object. */
    NOOPEN        = BASE + 0x000000040, /**< Set RTLD_NOOPEN for this object. */
    HANDLE_ORIGIN = BASE + 0x000000080, /**< $ORIGIN must be handled. */
    DIRECT        = BASE + 0x000000100, /**< Direct binding enabled. */
    TRANS         = BASE + 0x000000200,
    INTERPOSE     = BASE + 0x000000400, /**< Object is used to interpose. */
    NODEFLIB      = BASE + 0x000000800, /**< Ignore default lib search path. */
    NODUMP        = BASE + 0x000001000, /**< Object can't be dldump'ed. */
    CONFALT       = BASE + 0x000002000, /**< Configuration alternative created. */
    ENDFILTEE     = BASE + 0x000004000, /**< Filtee terminates filters search. */
    DISPRELDNE    = BASE + 0x000008000, /**< Disp reloc applied at build time. */
    DISPRELPND    = BASE + 0x000010000, /**< Disp reloc applied at run-time. */
    NODIRECT      = BASE + 0x000020000, /**< Object has no-direct binding. */
    IGNMULDEF     = BASE + 0x000040000,
    NOKSYMS       = BASE + 0x000080000,
    NOHDR         = BASE + 0x000100000,
    EDITED        = BASE + 0x000200000, /**< Object is modified after built. */
    NORELOC       = BASE + 0x000400000,
    SYMINTPOSE    = BASE + 0x000800000, /**< Object has individual interposers. */
    GLOBAUDIT     = BASE + 0x001000000, /**< Global auditing required. */
    SINGLETON     = BASE + 0x002000000, /**< Singleton symbols are used. */
    PIE           = BASE + 0x008000000, /**< Singleton symbols are used. */
    KMOD          = BASE + 0x010000000,
    WEAKFILTER    = BASE + 0x020000000,
    NOCOMMON      = BASE + 0x040000000,
  };

  using flags_list_t = std::vector<FLAG>;

  public:
  using DynamicEntry::DynamicEntry;
  DynamicEntryFlags() = delete;

  static DynamicEntryFlags create_dt_flag(uint64_t value) {
    return DynamicEntryFlags(DynamicEntry::TAG::FLAGS, value);
  }

  static DynamicEntryFlags create_dt_flag_1(uint64_t value) {
    return DynamicEntryFlags(DynamicEntry::TAG::FLAGS_1, value);
  }

  DynamicEntryFlags& operator=(const DynamicEntryFlags&) = default;
  DynamicEntryFlags(const DynamicEntryFlags&) = default;

  std::unique_ptr<DynamicEntry> clone() const override {
    return std::unique_ptr<DynamicEntryFlags>(new DynamicEntryFlags(*this));
  }

  /// If the current entry has the given FLAG
  bool has(FLAG f) const;

  /// Return flags as a list of integers
  flags_list_t flags() const;

  uint64_t raw_flags() const {
    flags_list_t flags = this->flags();
    return std::accumulate(flags.begin(), flags.end(), uint64_t(0),
      [] (uint64_t value, FLAG f) {
        return value + (uint64_t)f;
      }
    );
  }

  /// Add the given FLAG
  void add(FLAG f);

  /// Remove the given FLAG
  void remove(FLAG f);

  DynamicEntryFlags& operator+=(FLAG f) {
    add(f);
    return *this;
  }

  DynamicEntryFlags& operator-=(FLAG f) {
    remove(f);
    return *this;
  }

  void accept(Visitor& visitor) const override;

  static bool classof(const DynamicEntry* entry) {
    return entry->tag() == DynamicEntry::TAG::FLAGS ||
           entry->tag() == DynamicEntry::TAG::FLAGS_1;
  }

  ~DynamicEntryFlags() = default;

  std::ostream& print(std::ostream& os) const override;
  private:
  DynamicEntryFlags(DynamicEntry::TAG tag, uint64_t flags) :
    DynamicEntry(tag, flags)
  {}
};

LIEF_API const char* to_string(DynamicEntryFlags::FLAG e);

}
}

#endif

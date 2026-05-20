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
#ifndef LIEF_ELF_DYNAMIC_ENTRY_LIBRARY_H
#define LIEF_ELF_DYNAMIC_ENTRY_LIBRARY_H

#include <string>
#include "LIEF/visibility.h"
#include "LIEF/ELF/DynamicEntry.hpp"

namespace LIEF {
namespace ELF {

/// Class which represents a ``DT_NEEDED`` entry in the dynamic table.
///
/// This kind of entry is usually used to create library dependency.
class LIEF_API DynamicEntryLibrary : public DynamicEntry {

  public:
  using DynamicEntry::DynamicEntry;

  DynamicEntryLibrary() :
    DynamicEntry::DynamicEntry{DynamicEntry::TAG::NEEDED, 0}
  {}

  DynamicEntryLibrary(std::string name) :
    DynamicEntry::DynamicEntry{DynamicEntry::TAG::NEEDED, 0},
    libname_(std::move(name))
  {}

  DynamicEntryLibrary& operator=(const DynamicEntryLibrary&) = default;
  DynamicEntryLibrary(const DynamicEntryLibrary&) = default;

  std::unique_ptr<DynamicEntry> clone() const override {
    return std::unique_ptr<DynamicEntryLibrary>(new DynamicEntryLibrary{*this});
  }

  /// Return the library associated with this entry (e.g. ``libc.so.6``)
  const std::string& name() const {
    return libname_;
  }

  void name(std::string name) {
    libname_ = std::move(name);
  }

  static bool classof(const DynamicEntry* entry) {
    return entry->tag() == DynamicEntry::TAG::NEEDED;
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  private:
  std::string libname_;
};
}
}

#endif

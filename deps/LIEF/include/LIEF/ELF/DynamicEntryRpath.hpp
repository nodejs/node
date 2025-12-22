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
#ifndef LIEF_ELF_DYNAMIC_ENTRY_RPATH_H
#define LIEF_ELF_DYNAMIC_ENTRY_RPATH_H

#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/ELF/DynamicEntry.hpp"

namespace LIEF {
namespace ELF {

/// Class which represents a ``DT_RPATH`` entry. This attribute is
/// deprecated (cf. ``man ld``) in favor of ``DT_RUNPATH`` (See DynamicEntryRunPath)
class LIEF_API DynamicEntryRpath : public DynamicEntry {
  public:
  static constexpr char delimiter = ':';
  using DynamicEntry::DynamicEntry;
  DynamicEntryRpath() :
    DynamicEntry::DynamicEntry(DynamicEntry::TAG::RPATH, 0)
  {}

  DynamicEntryRpath(std::string rpath) :
    DynamicEntry::DynamicEntry(DynamicEntry::TAG::RPATH, 0),
    rpath_(std::move(rpath))
  {}

  /// Constructor from a list of paths
  DynamicEntryRpath(const std::vector<std::string>& paths) :
    DynamicEntry::DynamicEntry(DynamicEntry::TAG::RPATH, 0)
  {
    this->paths(paths);
  }

  DynamicEntryRpath& operator=(const DynamicEntryRpath&) = default;
  DynamicEntryRpath(const DynamicEntryRpath&) = default;

  std::unique_ptr<DynamicEntry> clone() const override {
    return std::unique_ptr<DynamicEntryRpath>(new DynamicEntryRpath(*this));
  }

  /// The actual rpath as a string
  const std::string& rpath() const {
    return rpath_;
  }

  void rpath(std::string name) {
    rpath_ = std::move(name);
  }

  /// Paths as a list
  std::vector<std::string> paths() const;
  void paths(const std::vector<std::string>& paths);

  /// Insert a ``path`` at the given ``position``
  DynamicEntryRpath& insert(size_t pos, const std::string& path);

  /// Append the given ``path``
  DynamicEntryRpath& append(std::string path);

  /// Remove the given ``path``
  DynamicEntryRpath& remove(const std::string& path);

  DynamicEntryRpath& operator+=(std::string path) {
    return append(std::move(path));
  }

  DynamicEntryRpath& operator-=(const std::string& path) {
    return remove(path);
  }

  static bool classof(const DynamicEntry* entry) {
    return entry->tag() == DynamicEntry::TAG::RPATH;
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  ~DynamicEntryRpath() = default;

  private:
  std::string rpath_;
};
}
}

#endif

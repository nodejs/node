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
#ifndef LIEF_ELF_DYNAMIC_ENTRY_RUNPATH_H
#define LIEF_ELF_DYNAMIC_ENTRY_RUNPATH_H

#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/ELF/DynamicEntry.hpp"

namespace LIEF {
namespace ELF {

/// Class that represents a ``DT_RUNPATH`` which is used by the loader
/// to resolve libraries (DynamicEntryLibrary).
class LIEF_API DynamicEntryRunPath : public DynamicEntry {

  public:
  static constexpr char delimiter = ':';
  using DynamicEntry::DynamicEntry;

  DynamicEntryRunPath() :
    DynamicEntry::DynamicEntry(DynamicEntry::TAG::RUNPATH, 0)
  {}

  /// Constructor from (run)path
  DynamicEntryRunPath(std::string runpath) :
    DynamicEntry::DynamicEntry(DynamicEntry::TAG::RUNPATH, 0),
    runpath_(std::move(runpath))
  {}

  /// Constructor from a list of paths
  DynamicEntryRunPath(const std::vector<std::string>& paths) :
    DynamicEntry::DynamicEntry(DynamicEntry::TAG::RUNPATH, 0)
  {
    this->paths(paths);
  }

  DynamicEntryRunPath& operator=(const DynamicEntryRunPath&) = default;
  DynamicEntryRunPath(const DynamicEntryRunPath&) = default;

  std::unique_ptr<DynamicEntry> clone() const override {
    return std::unique_ptr<DynamicEntry>(new DynamicEntryRunPath(*this));
  }

  /// Runpath raw value
  const std::string& runpath() const {
    return runpath_;
  }

  void runpath(std::string runpath) {
    runpath_ = std::move(runpath);
  }

  /// Paths as a list
  std::vector<std::string> paths() const;
  void paths(const std::vector<std::string>& paths);

  /// Insert a ``path`` at the given ``position``
  DynamicEntryRunPath& insert(size_t pos, const std::string& path);

  /// Append the given ``path``
  DynamicEntryRunPath& append(const std::string& path);

  /// Remove the given ``path``
  DynamicEntryRunPath& remove(const std::string& path);

  DynamicEntryRunPath& operator+=(std::string path) {
    return append(std::move(path));
  }

  DynamicEntryRunPath& operator-=(const std::string& path) {
    return remove(path);
  }

  void accept(Visitor& visitor) const override;

  static bool classof(const DynamicEntry* entry) {
    return entry->tag() == DynamicEntry::TAG::RUNPATH;
  }

  std::ostream& print(std::ostream& os) const override;

  ~DynamicEntryRunPath() = default;

  private:
  std::string runpath_;
};
}
}
#endif

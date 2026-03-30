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
#ifndef LIEF_ELF_DYNAMIC_SHARED_OBJECT_H
#define LIEF_ELF_DYNAMIC_SHARED_OBJECT_H

#include <string>

#include "LIEF/visibility.h"
#include "LIEF/ELF/DynamicEntry.hpp"

namespace LIEF {
namespace ELF {

/// Class which represents a ``DT_SONAME`` entry in the dynamic table
/// This kind of entry is usually used to name the original library.
///
/// This entry is not present for executable.
class LIEF_API DynamicSharedObject : public DynamicEntry {

  public:
  using DynamicEntry::DynamicEntry;
  DynamicSharedObject() :
    DynamicEntry(DynamicEntry::TAG::SONAME, 0)
  {}

  DynamicSharedObject(std::string name) :
    DynamicEntry(DynamicEntry::TAG::SONAME, 0),
    name_(std::move(name))
  {}

  DynamicSharedObject& operator=(const DynamicSharedObject&) = default;
  DynamicSharedObject(const DynamicSharedObject&) = default;

  std::unique_ptr<DynamicEntry> clone() const override {
    return std::unique_ptr<DynamicSharedObject>(new DynamicSharedObject(*this));
  }

  /// The actual name (e.g. ``libMyLib.so``)
  const std::string& name() const {
    return name_;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const DynamicEntry* entry) {
    return entry->tag() == DynamicEntry::TAG::SONAME;
  }

  ~DynamicSharedObject() override = default;

  private:
  std::string name_;
};
}
}
#endif

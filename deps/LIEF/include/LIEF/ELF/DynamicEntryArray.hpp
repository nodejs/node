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
#ifndef LIEF_ELF_DYNAMIC_ENTRY_ARRAY_H
#define LIEF_ELF_DYNAMIC_ENTRY_ARRAY_H

#include "LIEF/visibility.h"
#include "LIEF/ELF/DynamicEntry.hpp"

#include <vector>

namespace LIEF {
namespace ELF {

/// Class that represent an Array in the dynamic table.
/// This entry is associated with constructors:
/// - ``DT_PREINIT_ARRAY``
/// - ``DT_INIT_ARRAY``
/// - ``DT_FINI_ARRAY``
///
/// The underlying values are 64-bits integers to cover both:
/// ELF32 and ELF64 binaries.
class LIEF_API DynamicEntryArray : public DynamicEntry {
  public:
  using array_t = std::vector<uint64_t>;
  using DynamicEntry::DynamicEntry;

  DynamicEntryArray() = delete;
  DynamicEntryArray(DynamicEntry::TAG tag, array_t array) :
    DynamicEntry(tag, 0),
    array_(std::move(array))
  {}

  DynamicEntryArray& operator=(const DynamicEntryArray&) = default;
  DynamicEntryArray(const DynamicEntryArray&) = default;

  std::unique_ptr<DynamicEntry> clone() const override {
    return std::unique_ptr<DynamicEntryArray>(new DynamicEntryArray(*this));
  }

  /// Return the array values (list of pointers)
  array_t& array() {
    return array_;
  }

  const array_t& array() const {
    return array_;
  }
  void array(const array_t& array) {
    array_ = array;
  }

  /// Insert the given function at ``pos``
  DynamicEntryArray& insert(size_t pos, uint64_t function);

  /// Append the given function
  DynamicEntryArray& append(uint64_t function) {
    array_.push_back(function);
    return *this;
  }

  /// Remove the given function
  DynamicEntryArray& remove(uint64_t function);

  /// Number of function registred in this array
  size_t size() const {
    return array_.size();
  }

  DynamicEntryArray& operator+=(uint64_t value) {
    return append(value);
  }

  DynamicEntryArray& operator-=(uint64_t value) {
    return remove(value);
  }

  const uint64_t& operator[](size_t idx) const;
  uint64_t&       operator[](size_t idx);

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  ~DynamicEntryArray() override = default;

  static bool classof(const DynamicEntry* entry) {
    const DynamicEntry::TAG tag = entry->tag();
    return tag == DynamicEntry::TAG::INIT_ARRAY ||
           tag == DynamicEntry::TAG::FINI_ARRAY ||
           tag == DynamicEntry::TAG::PREINIT_ARRAY;
  }

  private:
  array_t array_;
};
}
}

#endif

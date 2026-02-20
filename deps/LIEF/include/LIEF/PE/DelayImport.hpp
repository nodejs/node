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
#ifndef LIEF_PE_DELAY_IMPORT_H
#define LIEF_PE_DELAY_IMPORT_H

#include <string>
#include <ostream>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

#include "LIEF/PE/DelayImportEntry.hpp"

namespace LIEF {
namespace PE {

namespace details {
struct delay_imports;
}

/// Class that represents a PE delayed import.
class LIEF_API DelayImport : public Object {

  friend class Parser;
  friend class Builder;

  public:
  using entries_t        = std::vector<std::unique_ptr<DelayImportEntry>>;
  using it_entries       = ref_iterator<entries_t&, DelayImportEntry*>;
  using it_const_entries = const_ref_iterator<const entries_t&, const DelayImportEntry*>;

  DelayImport() = default;
  DelayImport(const details::delay_imports& import, PE_TYPE type);
  DelayImport(std::string name) :
    name_(std::move(name))
  {}

  ~DelayImport() override = default;

  DelayImport(const DelayImport& other);
  DelayImport& operator=(DelayImport other) {
    swap(other);
    return *this;
  }

  DelayImport(DelayImport&&) noexcept = default;
  DelayImport& operator=(DelayImport&&) noexcept = default;

  void swap(DelayImport& other);

  /// According to the official PE specifications,
  /// this value is reserved and should be set to 0.
  uint32_t attribute() const {
    return attribute_;
  }
  void attribute(uint32_t hdl) {
    attribute_ = hdl;
  }

  /// Return the library's name (e.g. `kernel32.dll`)
  const std::string& name() const {
    return name_;
  }
  void name(std::string name) {
    name_ = std::move(name);
  }

  /// The RVA of the module handle (in the ``.data`` section)
  /// It is used for storage by the routine that is supplied to
  /// manage delay-loading.
  uint32_t handle() const {
    return handle_;
  }
  void handle(uint32_t hdl) {
    handle_ = hdl;
  }

  /// RVA of the delay-load import address table.
  uint32_t iat() const {
    return iat_;
  }
  void iat(uint32_t iat) {
    iat_ = iat;
  }

  /// RVA of the delay-load import names table.
  /// The content of this table has the layout as the Import lookup table
  uint32_t names_table() const {
    return names_table_;
  }
  void names_table(uint32_t value) {
    names_table_ = value;
  }

  /// RVA of the **bound** delay-load import address table or 0
  /// if the table does not exist.
  uint32_t biat() const {
    return bound_iat_;
  }
  void biat(uint32_t value) {
    bound_iat_ = value;
  }

  /// RVA of the **unload** delay-load import address table or 0
  /// if the table does not exist.
  ///
  /// According to the PE specifications, this table is an
  /// exact copy of the delay import address table that can be
  /// used to to restore the original IAT the case of unloading.
  uint32_t uiat() const {
    return unload_iat_;
  }
  void uiat(uint32_t value) {
    unload_iat_ = value;
  }

  /// The timestamp of the DLL to which this image has been bound.
  uint32_t timestamp() const {
    return timestamp_;
  }
  void timestamp(uint32_t value) {
    timestamp_ = value;
  }

  /// Iterator over the DelayImport's entries (DelayImportEntry)
  it_entries entries() {
    return entries_;
  }

  /// Iterator over the DelayImport's entries (DelayImportEntry)
  it_const_entries entries() const {
    return entries_;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const DelayImport& entry);

  private:
  uint32_t attribute_ = 0;
  std::string name_;
  uint32_t handle_ = 0;
  uint32_t iat_ = 0;
  uint32_t names_table_ = 0;
  uint32_t bound_iat_ = 0;
  uint32_t unload_iat_ = 0;
  uint32_t timestamp_ = 0;
  entries_t entries_;

  PE_TYPE type_ = PE_TYPE::PE32;
};

}
}

#endif

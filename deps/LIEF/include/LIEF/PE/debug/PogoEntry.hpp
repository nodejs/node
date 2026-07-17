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
#ifndef LIEF_PE_DEBUG_POGO_ENTRY_H
#define LIEF_PE_DEBUG_POGO_ENTRY_H
#include <ostream>
#include <cstdint>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

class Builder;
class Parser;

class LIEF_API PogoEntry : public Object {
  friend class Builder;
  friend class Parser;

  public:
  PogoEntry() = default;
  PogoEntry(PogoEntry&& other) = default;
  PogoEntry& operator=(PogoEntry&& other) = default;
  PogoEntry(const PogoEntry&) = default;
  PogoEntry(uint32_t start_rva, uint32_t size, std::string name) :
    start_rva_{start_rva},
    size_{size},
    name_{std::move(name)}
  {}

  PogoEntry(uint32_t start_rva, uint32_t size) :
    PogoEntry{start_rva, size, ""}
  {}

  PogoEntry& operator=(const PogoEntry&) = default;
  ~PogoEntry() override = default;

  uint32_t start_rva() const {
    return start_rva_;
  }

  uint32_t size() const {
    return size_;
  }

  const std::string& name() const {
    return name_;
  }

  void start_rva(uint32_t start_rva) {
    start_rva_ = start_rva;
  }

  void size(uint32_t size) {
    size_ = size;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const PogoEntry& entry);

  protected:
  uint32_t start_rva_ = 0;
  uint32_t size_ = 0;
  std::string name_;
};

} // Namespace PE
} // Namespace LIEF

#endif

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
#ifndef LIEF_ABSTRACT_RELOCATION_H
#define LIEF_ABSTRACT_RELOCATION_H

#include <ostream>
#include <cstdint>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
/// Class which represents an abstracted Relocation
class LIEF_API Relocation : public Object {

  public:
  Relocation() = default;

  /// Constructor from a relocation's address and size
  Relocation(uint64_t address, uint8_t size) :
    address_(address),
    size_(size)
  {}

  ~Relocation() override = default;

  Relocation& operator=(const Relocation&) = default;
  Relocation(const Relocation&) = default;
  void swap(Relocation& other) {
    std::swap(address_, other.address_);
    std::swap(size_,    other.size_);
  }

  /// Relocation's address
  virtual uint64_t address() const {
    return address_;
  }

  /// Relocation size in **bits**
  virtual size_t size() const {
    return size_;
  }

  virtual void address(uint64_t address) {
    address_ = address;
  }

  virtual void size(size_t size) {
    size_ = (uint8_t)size;
  }

  /// Method so that the ``visitor`` can visit us
  void accept(Visitor& visitor) const override;


  /// Comparaison based on the Relocation's **address**
  virtual bool operator<(const Relocation& rhs) const {
    return address() < rhs.address();
  }

  /// Comparaison based on the Relocation's **address**
  virtual bool operator<=(const Relocation& rhs) const {
    return !(address() > rhs.address());
  }

  /// Comparaison based on the Relocation's **address**
  virtual bool operator>(const Relocation& rhs) const {
    return address() > rhs.address();
  }

  /// Comparaison based on the Relocation's **address**
  virtual bool operator>=(const Relocation& rhs) const {
    return !(address() < rhs.address());
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Relocation& entry);

  protected:
  uint64_t address_ = 0;
  uint8_t  size_ = 0;
};


}
#endif

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
#ifndef LIEF_ABSTRACT_SYMBOLS_H
#define LIEF_ABSTRACT_SYMBOLS_H

#include <cstdint>
#include <string>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {

/// This class represents a symbol in an executable format.
class LIEF_API Symbol : public Object {
  public:
  Symbol() = default;
  Symbol(std::string name) :
    name_(std::move(name))
  {}

  Symbol(std::string name, uint64_t value) :
    name_(std::move(name)), value_(value)
  {}

  Symbol(std::string name, uint64_t value, uint64_t size) :
    name_(std::move(name)), value_(value), size_(size)
  {}

  Symbol(const Symbol&) = default;
  Symbol& operator=(const Symbol&) = default;

  Symbol(Symbol&&) = default;
  Symbol& operator=(Symbol&&) = default;

  ~Symbol() override = default;

  void swap(Symbol& other) noexcept;

  /// Return the symbol's name
  virtual const std::string& name() const {
    return name_;
  }

  virtual std::string& name() {
    return name_;
  }

  /// Set symbol name
  virtual void name(std::string name) {
    name_ = std::move(name);
  }

  // Symbol's value which is usually the **address** of the symbol
  virtual uint64_t value() const {
    return value_;
  }
  virtual void value(uint64_t value) {
    value_ = value;
  }

  /// This size of the symbol (when applicable)
  virtual uint64_t size() const {
    return size_;
  }

  virtual void size(uint64_t value) {
    size_ = value;
  }

  /// Method so that the ``visitor`` can visit us
  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Symbol& entry);

  protected:
  std::string name_;
  uint64_t value_ = 0;
  uint64_t size_ = 0;
};
}

#endif


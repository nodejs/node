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
#ifndef LIEF_ABSTRACT_SECTION_H
#define LIEF_ABSTRACT_SECTION_H

#include <string>
#include <vector>
#include <ostream>

#include "LIEF/span.hpp"
#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
/// Class which represents an abstracted section
class LIEF_API Section : public Object {
  public:
  static constexpr size_t npos = -1;

  Section() = default;
  Section(std::string name) :
    name_(std::move(name))
  {}

  ~Section() override = default;

  Section& operator=(const Section&) = default;
  Section(const Section&) = default;

  /// section's name
  virtual std::string name() const {
    return name_.c_str();
  }

  /// Return the **complete** section's name which might
  /// trailing (``0``) bytes
  virtual const std::string& fullname() const {
    return name_;
  }

  ///  section's content
  virtual span<const uint8_t> content() const {
    return {};
  }

  /// Change the section size
  virtual void size(uint64_t size) {
    size_ = size;
  }

  /// section's size (size in the binary, not the virtual size)
  virtual uint64_t size() const {
    return size_;
  }

  /// Offset in the binary
  virtual uint64_t offset() const {
    return offset_;
  }

  /// Address where the section should be mapped
  virtual uint64_t virtual_address() const {
    return virtual_address_;
  }

  virtual void virtual_address(uint64_t virtual_address) {
    virtual_address_ = virtual_address;
  }

  /// Change the section's name
  virtual void name(std::string name) {
    name_ = std::move(name);
  }

  /// Change section content
  virtual void content(const std::vector<uint8_t>&) {}

  virtual void offset(uint64_t offset) {
    offset_ = offset;
  }

  /// Section's entropy
  double entropy() const;

  // Search functions
  // ================
  size_t search(uint64_t integer, size_t pos, size_t size) const;
  size_t search(const std::vector<uint8_t>& pattern, size_t pos = 0) const;
  size_t search(const std::string& pattern, size_t pos = 0) const;
  size_t search(uint64_t integer, size_t pos = 0) const;

  // Search all functions
  // ====================
  std::vector<size_t> search_all(uint64_t v, size_t size) const;

  std::vector<size_t> search_all(uint64_t v) const;

  std::vector<size_t> search_all(const std::string& v) const;

  /// Method so that the ``visitor`` can visit us
  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Section& entry);

  protected:
  std::string name_;
  uint64_t    virtual_address_ = 0;
  uint64_t    size_ = 0;
  uint64_t    offset_ = 0;

  private:
  template<typename T>
  std::vector<size_t> search_all_(const T& v) const;
};
}

#endif

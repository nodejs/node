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
#ifndef LIEF_COFF_REGULAR_HEADER_H
#define LIEF_COFF_REGULAR_HEADER_H
#include <cstdint>
#include "LIEF/COFF/Header.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace COFF {

/// This class represents the COFF header for non-bigobj
class LIEF_API RegularHeader : public Header {
  public:
  RegularHeader() :
    Header(KIND::REGULAR)
  {}

  /// Create a RegularHeader from the given stream
  static std::unique_ptr<RegularHeader> create(BinaryStream& stream);

  RegularHeader& operator=(const RegularHeader&) = default;
  RegularHeader(const RegularHeader&) = default;

  RegularHeader& operator=(RegularHeader&&) = default;
  RegularHeader(RegularHeader&&) = default;

  std::unique_ptr<Header> clone() const override {
    return std::unique_ptr<Header>(new RegularHeader(*this));
  }

  /// The size of the optional header that follows this header (should be 0)
  uint16_t sizeof_optionalheader() const {
    return sizeof_optionalheader_;
  }

  /// Characteristics
  uint16_t characteristics() const {
    return characteristics_;
  }

  void sizeof_optionalheader(uint16_t value) {
    sizeof_optionalheader_ = value;
  }

  void characteristics(uint16_t value) {
    characteristics_ = value;
  }

  static bool classof(const Header* header) {
    return header->kind() == Header::KIND::REGULAR;
  }

  ~RegularHeader() override = default;

  std::string to_string() const override;

  protected:
  uint16_t sizeof_optionalheader_ = 0;
  uint16_t characteristics_ = 0;
};

}
}
#endif

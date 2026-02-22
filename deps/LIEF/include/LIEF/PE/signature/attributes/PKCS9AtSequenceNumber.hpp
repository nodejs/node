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
#ifndef LIEF_PE_ATTRIBUTES_PKCS9_AT_SEQUENCE_NUMBER_H
#define LIEF_PE_ATTRIBUTES_PKCS9_AT_SEQUENCE_NUMBER_H
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {

class Parser;
class SignatureParser;

/// Interface over the structure described by the OID ``1.2.840.113549.1.9.25.4`` (PKCS #9)
///
/// The internal structure is described in the
/// [RFC #2985: PKCS #9 - Selected Object Classes and Attribute Types Version 2.0](https://tools.ietf.org/html/rfc2985)
///
/// ```text
/// sequenceNumber ATTRIBUTE ::= {
///   WITH SYNTAX SequenceNumber
///   EQUALITY MATCHING RULE integerMatch
///   SINGLE VALUE TRUE
///   ID pkcs-9-at-sequenceNumber
/// }
///
/// SequenceNumber ::= INTEGER (1..MAX)
/// ```
class LIEF_API PKCS9AtSequenceNumber : public Attribute {

  friend class Parser;
  friend class SignatureParser;

  public:
  PKCS9AtSequenceNumber() = delete;
  PKCS9AtSequenceNumber(uint32_t num) :
    Attribute(Attribute::TYPE::PKCS9_AT_SEQUENCE_NUMBER),
    number_{num}
  {}

  PKCS9AtSequenceNumber(const PKCS9AtSequenceNumber&) = default;
  PKCS9AtSequenceNumber& operator=(const PKCS9AtSequenceNumber&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new PKCS9AtSequenceNumber{*this});
  }

  /// Number as described in the RFC
  uint32_t number() const {
    return number_;
  }

  /// Print information about the attribute
  std::string print() const override;

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::PKCS9_AT_SEQUENCE_NUMBER;
  }

  void accept(Visitor& visitor) const override;

  ~PKCS9AtSequenceNumber() override = default;

  private:
  uint32_t number_ = 0;
};

}
}

#endif

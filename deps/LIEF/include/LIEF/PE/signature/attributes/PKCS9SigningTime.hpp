
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
#ifndef LIEF_PE_ATTRIBUTES_PKCS9_SIGNING_TIME_H
#define LIEF_PE_ATTRIBUTES_PKCS9_SIGNING_TIME_H
#include <array>

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {

/// Interface over the structure described by the OID ``1.2.840.113549.1.9.5`` (PKCS #9)
///
/// The internal structure is described in the
/// [RFC #2985: PKCS #9 - Selected Object Classes and Attribute Types Version 2.0](https://tools.ietf.org/html/rfc2985)
///
/// ```text
/// signingTime ATTRIBUTE ::= {
///         WITH SYNTAX SigningTime
///         EQUALITY MATCHING RULE signingTimeMatch
///         SINGLE VALUE TRUE
///         ID pkcs-9-at-signingTime
/// }
///
/// SigningTime ::= Time -- imported from ISO/IEC 9594-8
/// ```
class LIEF_API PKCS9SigningTime : public Attribute {

  friend class Parser;
  friend class SignatureParser;

  public:
  /// Time as an array [year, month, day, hour, min, sec]
  using time_t = std::array<int32_t, 6>;

  PKCS9SigningTime() = delete;
  PKCS9SigningTime(time_t time) :
    Attribute(Attribute::TYPE::PKCS9_SIGNING_TIME),
    time_{time}
  {}

  PKCS9SigningTime(const PKCS9SigningTime&) = default;
  PKCS9SigningTime& operator=(const PKCS9SigningTime&) = default;

  /// Time as an array [year, month, day, hour, min, sec]
  const time_t& time() const {
    return time_;
  }

  /// Print information about the attribute
  std::string print() const override;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new PKCS9SigningTime{*this});
  }

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::PKCS9_SIGNING_TIME;
  }

  void accept(Visitor& visitor) const override;

  ~PKCS9SigningTime() override = default;

  private:
  time_t time_;
};

}
}

#endif

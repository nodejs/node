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
#ifndef LIEF_PE_ATTRIBUTES_PKCS9_MESSAGE_DIGEST_H
#define LIEF_PE_ATTRIBUTES_PKCS9_MESSAGE_DIGEST_H

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"
#include "LIEF/span.hpp"

#include <vector>

namespace LIEF {
namespace PE {

class Parser;
class SignatureParser;

/// Interface over the structure described by the OID ``1.2.840.113549.1.9.4`` (PKCS #9)
///
/// The internal structure is described in the
/// [RFC #2985: PKCS #9 - Selected Object Classes and Attribute Types Version 2.0](https://tools.ietf.org/html/rfc2985)
///
/// ```text
/// messageDigest ATTRIBUTE ::= {
///   WITH SYNTAX MessageDigest
///   EQUALITY MATCHING RULE octetStringMatch
///   SINGLE VALUE TRUE
///   ID pkcs-9-at-messageDigest
/// }
///
/// MessageDigest ::= OCTET STRING
/// ```
class LIEF_API PKCS9MessageDigest : public Attribute {

  friend class Parser;
  friend class SignatureParser;

  public:
  PKCS9MessageDigest() = delete;
  PKCS9MessageDigest(std::vector<uint8_t> digest) :
    Attribute(Attribute::TYPE::PKCS9_MESSAGE_DIGEST),
    digest_{std::move(digest)}
  {}

  PKCS9MessageDigest(const PKCS9MessageDigest&) = default;
  PKCS9MessageDigest& operator=(const PKCS9MessageDigest&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new PKCS9MessageDigest{*this});
  }

  /// Message digeset as a blob of bytes as described in the RFC
  span<const uint8_t> digest() const {
    return digest_;
  }

  /// Print information about the attribute
  std::string print() const override;

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::PKCS9_MESSAGE_DIGEST;
  }

  void accept(Visitor& visitor) const override;

  ~PKCS9MessageDigest() override = default;

  private:
  std::vector<uint8_t> digest_;
};

}
}

#endif

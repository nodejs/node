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
#ifndef LIEF_PE_ATTRIBUTES_SIGNING_CERTIFICATE_V2_H
#define LIEF_PE_ATTRIBUTES_SIGNING_CERTIFICATE_V2_H

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {
/// SigningCertificateV2 ::= SEQUENCE {
///   certs    SEQUENCE OF ESSCertIDv2,
///   policies SEQUENCE OF PolicyInformation OPTIONAL
/// }
///
/// ESSCertIDv2 ::= SEQUENCE {
///   hashAlgorithm AlgorithmIdentifier DEFAULT {algorithm id-sha256},
///   certHash      OCTET STRING,
///   issuerSerial  IssuerSerial OPTIONAL
/// }
///
/// IssuerSerial ::= SEQUENCE {
///   issuer       GeneralNames,
///   serialNumber CertificateSerialNumber
/// }
///
/// PolicyInformation ::= SEQUENCE {
///   policyIdentifier   OBJECT IDENTIFIER,
///   policyQualifiers   SEQUENCE SIZE (1..MAX) OF PolicyQualifierInfo OPTIONAL
/// }
class LIEF_API SigningCertificateV2 : public Attribute {
  friend class Parser;
  friend class SignatureParser;

  public:
  SigningCertificateV2() :
    Attribute(Attribute::TYPE::SIGNING_CERTIFICATE_V2)
  {}
  SigningCertificateV2(const SigningCertificateV2&) = default;
  SigningCertificateV2& operator=(const SigningCertificateV2&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new SigningCertificateV2{*this});
  }

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::SIGNING_CERTIFICATE_V2;
  }

  std::string print() const override;

  void accept(Visitor& visitor) const override;

  ~SigningCertificateV2() override = default;
};

}
}

#endif

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
#ifndef LIEF_PE_PKCS9_TSTINFO_H
#define LIEF_PE_PKCS9_TSTINFO_H
#include <ostream>
#include <memory>

#include "LIEF/Visitor.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PE/signature/ContentInfo.hpp"

namespace LIEF {
namespace PE {

/// Interface over the structure described by the OID `1.2.840.113549.1.9.16.1.4` (PKCS #9)
///
/// The internal structure is described in the
/// [RFC #3161](https://tools.ietf.org/html/rfc3161)
///
/// ```text
/// TSTInfo ::= SEQUENCE  {
///  version        INTEGER  { v1(1) },
///  policy         TSAPolicyId,
///  messageImprint MessageImprint,
///  serialNumber   INTEGER,
///  genTime        GeneralizedTime,
///  accuracy       Accuracy                OPTIONAL,
///  ordering       BOOLEAN                 DEFAULT FALSE,
///  nonce          INTEGER                 OPTIONAL,
///  tsa            [0] GeneralName         OPTIONAL,
///  extensions     [1] IMPLICIT Extensions OPTIONAL
/// }
///
/// TSAPolicyId    ::= OBJECT IDENTIFIER
/// MessageImprint ::= SEQUENCE {
///   hashAlgorithm  AlgorithmIdentifier,
///   hashedMessage  OCTET STRING
/// }
///
/// Accuracy ::= SEQUENCE {
///   seconds        INTEGER           OPTIONAL,
///   millis     [0] INTEGER  (1..999) OPTIONAL,
///   micros     [1] INTEGER  (1..999) OPTIONAL
/// }
/// ```
class LIEF_API PKCS9TSTInfo : public ContentInfo::Content {
  friend class SignatureParser;

  public:
  static constexpr auto PKCS9_TSTINFO_OBJID = "1.2.840.113549.1.9.16.1.4";
  PKCS9TSTInfo() :
    ContentInfo::Content(PKCS9_TSTINFO_OBJID)
  {}
  PKCS9TSTInfo(const PKCS9TSTInfo&) = default;
  PKCS9TSTInfo& operator=(const PKCS9TSTInfo&) = default;

  std::unique_ptr<Content> clone() const override {
    return std::unique_ptr<Content>(new PKCS9TSTInfo{*this});
  }

  void print(std::ostream& /*os*/) const override {
    return;
  }

  void accept(Visitor& visitor) const override;

  ~PKCS9TSTInfo() override = default;

  static bool classof(const ContentInfo::Content* content) {
    return content->content_type() == PKCS9_TSTINFO_OBJID;
  }

  private:
  uint32_t version_ = 0;
};
}
}
#endif

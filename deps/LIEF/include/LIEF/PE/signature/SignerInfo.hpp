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
#ifndef LIEF_PE_SIGNER_INFO_H
#define LIEF_PE_SIGNER_INFO_H
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/PE/signature/types.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/enums.hpp"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {

class Signature;
class Attribute;
class Parser;
class SignatureParser;
class x509;

/** SignerInfo as described in the [RFC 2315](https://tools.ietf.org/html/rfc2315#section-9.2)
 *
 * ```text
 * SignerInfo ::= SEQUENCE {
 *  version Version,
 *  issuerAndSerialNumber     IssuerAndSerialNumber,
 *  digestAlgorithm           DigestAlgorithmIdentifier,
 *  authenticatedAttributes   [0] IMPLICIT Attributes OPTIONAL,
 *  digestEncryptionAlgorithm DigestEncryptionAlgorithmIdentifier,
 *  encryptedDigest           EncryptedDigest,
 *  unauthenticatedAttributes [1] IMPLICIT Attributes OPTIONAL
 * }
 *
 * EncryptedDigest ::= OCTET STRING
 * ```
 */
class LIEF_API SignerInfo : public Object {
  friend class Parser;
  friend class SignatureParser;
  friend class Signature;

  public:
  using encrypted_digest_t = std::vector<uint8_t>;

  /// Internal container used to store both
  /// authenticated and unauthenticated attributes
  using attributes_t = std::vector<std::unique_ptr<Attribute>>;

  /// Iterator which outputs const Attribute&
  using it_const_attributes_t = const_ref_iterator<const attributes_t&, const Attribute*>;

  SignerInfo();

  SignerInfo(const SignerInfo& other);
  SignerInfo& operator=(SignerInfo other);

  SignerInfo(SignerInfo&&);
  SignerInfo& operator=(SignerInfo&&);

  void swap(SignerInfo& other);

  /// Should be 1
  uint32_t version() const {
    return version_;
  }

  /// Return the serial number associated with the x509 certificate
  /// used by this signer.
  ///
  /// @see
  /// LIEF::PE::x509::serial_number
  /// SignerInfo::issuer
  span<const uint8_t> serial_number() const {
    return serialno_;
  }

  /// Return the x509::issuer used by this signer
  const std::string& issuer() const {
    return issuer_;
  }

  /// Algorithm (OID) used to hash the file.
  ///
  /// This value should match LIEF::PE::ContentInfo::digest_algorithm and
  /// LIEF::PE::Signature::digest_algorithm
  ALGORITHMS digest_algorithm() const {
    return digest_algorithm_;
  }

  /// Return the (public-key) algorithm used to encrypt
  /// the signature
  ALGORITHMS encryption_algorithm() const {
    return digest_enc_algorithm_;
  }

  /// Return the signature created by the signing
  /// certificate's private key
  const encrypted_digest_t& encrypted_digest() const {
    return encrypted_digest_;
  }

  /// Iterator over LIEF::PE::Attribute for **authenticated** attributes
  it_const_attributes_t authenticated_attributes() const {
    return authenticated_attributes_;
  }

  /// Iterator over LIEF::PE::Attribute for **unauthenticated** attributes
  it_const_attributes_t unauthenticated_attributes() const {
    return unauthenticated_attributes_;
  }

  /// Return the authenticated or un-authenticated attribute matching the
  /// given PE::SIG_ATTRIBUTE_TYPES.
  ///
  /// It returns **the first** entry that matches the given type. If it can't be
  /// found, it returns a nullptr.
  const Attribute* get_attribute(Attribute::TYPE type) const;

  /// Return the authenticated attribute matching the given PE::SIG_ATTRIBUTE_TYPES.
  ///
  /// It returns **the first** entry that matches the given type. If it can't be
  /// found, it returns a nullptr.
  const Attribute* get_auth_attribute(Attribute::TYPE type) const;

  /// Return the un-authenticated attribute matching the given PE::SIG_ATTRIBUTE_TYPES.
  ///
  /// It returns **the first** entry that matches the given type. If it can't be
  /// found, it returns a nullptr.
  const Attribute* get_unauth_attribute(Attribute::TYPE type) const;

  /// x509 certificate used by this signer. If it can't be found, it returns a nullptr
  const x509* cert() const {
    return cert_.get();
  }

  /// x509 certificate used by this signer. If it can't be found, it returns a nullptr
  x509* cert() {
    return cert_.get();
  }

  /// Raw blob that is signed by the signer certificate
  span<const uint8_t> raw_auth_data() const {
    return raw_auth_data_;
  }

  void accept(Visitor& visitor) const override;

  ~SignerInfo() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const SignerInfo& signer_info);

  private:
  uint32_t version_ = 0;
  std::string issuer_;
  std::vector<uint8_t> serialno_;

  ALGORITHMS digest_algorithm_     = ALGORITHMS::UNKNOWN;
  ALGORITHMS digest_enc_algorithm_ = ALGORITHMS::UNKNOWN;

  encrypted_digest_t encrypted_digest_;

  std::vector<uint8_t> raw_auth_data_;

  attributes_t authenticated_attributes_;
  attributes_t unauthenticated_attributes_;

  std::unique_ptr<x509> cert_;
};

}
}

#endif

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
#ifndef LIEF_PE_SIGNATURE_H
#define LIEF_PE_SIGNATURE_H

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/PE/signature/x509.hpp"
#include "LIEF/PE/signature/SignerInfo.hpp"
#include "LIEF/PE/signature/ContentInfo.hpp"

#include "LIEF/PE/enums.hpp"

#include "LIEF/iterators.hpp"
#include "LIEF/enums.hpp"

namespace LIEF {
namespace PE {

class SignatureParser;
class Binary;

/// Main interface for the PKCS #7 signature scheme
class LIEF_API Signature : public Object {

  friend class SignatureParser;
  friend class Parser;
  friend class Binary;

  public:
  /// Hash the input given the algorithm
  static std::vector<uint8_t> hash(const std::vector<uint8_t>& input, ALGORITHMS algo) {
    return hash(input.data(), input.size(), algo);
  }

  static std::vector<uint8_t> hash(const uint8_t* buffer, size_t size, ALGORITHMS algo);

  public:

  /// Iterator which outputs const x509& certificates
  using it_const_crt = const_ref_iterator<const std::vector<x509>&>;

  /// Iterator which outputs x509& certificates
  using it_crt = ref_iterator<std::vector<x509>&>;

  /// Iterator which outputs const SignerInfo&
  using it_const_signers_t = const_ref_iterator<const std::vector<SignerInfo>&>;

  /// Iterator which outputs SignerInfo&
  using it_signers_t = ref_iterator<std::vector<SignerInfo>&>;

  /// Flags returned by the verification functions
  enum class VERIFICATION_FLAGS : uint32_t {
    OK = 0,
    INVALID_SIGNER                = 1 << 0,
    UNSUPPORTED_ALGORITHM         = 1 << 1,
    INCONSISTENT_DIGEST_ALGORITHM = 1 << 2,
    CERT_NOT_FOUND                = 1 << 3,
    CORRUPTED_CONTENT_INFO        = 1 << 4,
    CORRUPTED_AUTH_DATA           = 1 << 5,
    MISSING_PKCS9_MESSAGE_DIGEST  = 1 << 6,
    BAD_DIGEST                    = 1 << 7,
    BAD_SIGNATURE                 = 1 << 8,
    NO_SIGNATURE                  = 1 << 9,
    CERT_EXPIRED                  = 1 << 10,
    CERT_FUTURE                   = 1 << 11,
  };

  /// Convert a verification flag into a humman representation.
  /// e.g VERIFICATION_FLAGS.BAD_DIGEST | VERIFICATION_FLAGS.BAD_SIGNATURE | VERIFICATION_FLAGS.CERT_EXPIRED
  static std::string flag_to_string(VERIFICATION_FLAGS flag);

  /// Flags to tweak the verification process of the signature
  ///
  /// See Signature::check and LIEF::PE::Binary::verify_signature
  enum class VERIFICATION_CHECKS : uint32_t {
    DEFAULT          = 1 << 0, /**< Default behavior that tries to follow the Microsoft verification process as close as possible */
    HASH_ONLY        = 1 << 1, /**< Only check that Binary::authentihash matches ContentInfo::digest regardless of the signature's validity */
    LIFETIME_SIGNING = 1 << 2, /**< Same semantic as [WTD_LIFETIME_SIGNING_FLAG](https://docs.microsoft.com/en-us/windows/win32/api/wintrust/ns-wintrust-wintrust_data#WTD_LIFETIME_SIGNING_FLAG) */
    SKIP_CERT_TIME   = 1 << 3, /**< Skip the verification of the certificates time validities so that even though a certificate expired, it returns VERIFICATION_FLAGS::OK */
  };

  Signature();
  Signature(const Signature&);
  Signature& operator=(const Signature&);

  Signature(Signature&&);
  Signature& operator=(Signature&&);

  /// Should be 1
  uint32_t version() const {
    return version_;
  }

  /// Algorithm used to *digest* the file.
  ///
  /// It should match SignerInfo::digest_algorithm
  ALGORITHMS digest_algorithm() const {
    return digest_algorithm_;
  }

  /// Return the ContentInfo
  const ContentInfo& content_info() const {
    return content_info_;
  }

  /// Return an iterator over x509 certificates
  it_const_crt certificates() const {
    return certificates_;
  }

  it_crt certificates()  {
    return certificates_;
  }

  /// Return an iterator over the signers (SignerInfo) defined in the PKCS #7 signature
  it_const_signers_t signers() const {
    return signers_;
  }

  it_signers_t signers() {
    return signers_;
  }

  /// Return the raw original PKCS7 signature
  span<const uint8_t> raw_der() const {
    return original_raw_signature_;
  }

  /// Find x509 certificate according to its serial number
  const x509* find_crt(const std::vector<uint8_t>& serialno) const;

  /// Find x509 certificate according to its subject
  const x509* find_crt_subject(const std::string& subject) const;

  /// Find x509 certificate according to its subject **AND** serial number
  const x509* find_crt_subject(const std::string& subject, const std::vector<uint8_t>& serialno) const;

  /// Find x509 certificate according to its issuer
  const x509* find_crt_issuer(const std::string& issuer) const;

  /// Find x509 certificate according to its issuer **AND** serial number
  const x509* find_crt_issuer(const std::string& issuer, const std::vector<uint8_t>& serialno) const;

  /// Check if this signature is valid according to the Authenticode/PKCS #7 verification scheme
  ///
  /// By default, it performs the following verifications:
  ///
  /// 1. It must contain only **one** signer info
  /// 2. Signature::digest_algorithm must match:
  ///    * ContentInfo::digest_algorithm
  ///    * SignerInfo::digest_algorithm
  /// 3. The x509 certificate specified by SignerInfo::serial_number **and** SignerInfo::issuer
  ///    must exist within Signature::certificates
  /// 4. Given the x509 certificate, compare SignerInfo::encrypted_digest against either:
  ///    * hash of authenticated attributes if present
  ///    * hash of ContentInfo
  /// 5. If authenticated attributes are present, check that a PKCS9_MESSAGE_DIGEST attribute exists
  ///    and that its value matches hash of ContentInfo
  /// 6. Check the validity of the PKCS #9 counter signature if present
  /// 7. If the signature doesn't embed a signing-time in the counter signature, check the certificate
  ///    validity. (See LIEF::PE::Signature::VERIFICATION_CHECKS::LIFETIME_SIGNING and LIEF::PE::Signature::VERIFICATION_CHECKS::SKIP_CERT_TIME)
  ///
  /// See: LIEF::PE::Signature::VERIFICATION_CHECKS to tweak the behavior
  VERIFICATION_FLAGS check(VERIFICATION_CHECKS checks = VERIFICATION_CHECKS::DEFAULT) const;

  void accept(Visitor& visitor) const override;

  ~Signature() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Signature& signature);

  private:
  uint32_t                version_ = 0;
  ALGORITHMS              digest_algorithm_ = ALGORITHMS::UNKNOWN;
  ContentInfo             content_info_;
  std::vector<x509>       certificates_;
  std::vector<SignerInfo> signers_;

  uint64_t                content_info_start_ = 0;
  uint64_t                content_info_end_ = 0;

  std::vector<uint8_t> original_raw_signature_;
};


}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::Signature::VERIFICATION_FLAGS);
ENABLE_BITMASK_OPERATORS(LIEF::PE::Signature::VERIFICATION_CHECKS);


#endif


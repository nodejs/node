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
#ifndef LIEF_PE_ATTRIBUTES_MS_COUNTER_SIGNATURE_H
#define LIEF_PE_ATTRIBUTES_MS_COUNTER_SIGNATURE_H

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"

#include "LIEF/PE/signature/x509.hpp"
#include "LIEF/PE/signature/SignerInfo.hpp"
#include "LIEF/PE/signature/ContentInfo.hpp"

#include <vector>

namespace LIEF {
namespace PE {

/// This class exposes the MS Counter Signature attribute
class LIEF_API MsCounterSign : public Attribute {
  friend class Parser;
  friend class SignatureParser;

  public:
  using certificates_t = std::vector<x509>;
  using it_const_certificates = const_ref_iterator<const certificates_t&>;
  using it_certificates = ref_iterator<certificates_t&>;

  using signers_t = std::vector<SignerInfo>;
  using it_const_signers = const_ref_iterator<const signers_t&>;
  using it_signers = ref_iterator<signers_t&>;

  MsCounterSign() :
    Attribute(Attribute::TYPE::MS_COUNTER_SIGN)
  {}

  MsCounterSign(const MsCounterSign&) = default;
  MsCounterSign& operator=(const MsCounterSign&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new MsCounterSign{*this});
  }

  uint32_t version() const {
    return version_;
  }

  /// Iterator over the LIEF::PE::x509 certificates of this counter signature
  it_const_certificates certificates() const {
    return certificates_;
  }

  it_certificates certificates() {
    return certificates_;
  }

  /// Signer iterator (same as LIEF::PE::SignerInfo)
  it_const_signers signers() const {
    return signers_;
  }

  it_signers signers() {
    return signers_;
  }

  ALGORITHMS digest_algorithm() const {
    return digest_algorithm_;
  }

  const ContentInfo& content_info() const {
    return content_info_;
  }

  std::string print() const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::MS_COUNTER_SIGN;
  }

  ~MsCounterSign() override = default;

  private:
  uint32_t version_ = 0;
  ALGORITHMS digest_algorithm_ = ALGORITHMS::UNKNOWN;
  ContentInfo content_info_;
  certificates_t certificates_;
  std::vector<SignerInfo> signers_;
};

}
}

#endif

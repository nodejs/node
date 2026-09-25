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
#ifndef LIEF_PE_ATTRIBUTES_MS_SPC_NESTED_SIG_H
#define LIEF_PE_ATTRIBUTES_MS_SPC_NESTED_SIG_H

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"
#include "LIEF/PE/signature/Signature.hpp"


namespace LIEF {
namespace PE {

/// Interface over the structure described by the OID ``1.3.6.1.4.1.311.2.4.1``
///
/// The internal structure is not documented but we can infer the following structure:
///
/// ```text
/// MsSpcNestedSignature ::= SET OF SignedData
/// ```
///
/// ``SignedData`` is the structure described in PKCS #7 RFC (LIEF::PE::Signature)
class LIEF_API MsSpcNestedSignature : public Attribute {

  friend class Parser;
  friend class SignatureParser;

  public:
  MsSpcNestedSignature() = delete;
  MsSpcNestedSignature(Signature sig) :
    Attribute(Attribute::TYPE::MS_SPC_NESTED_SIGN),
    sig_{std::move(sig)}
  {}
  MsSpcNestedSignature(const MsSpcNestedSignature&) = default;
  MsSpcNestedSignature& operator=(const MsSpcNestedSignature&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new MsSpcNestedSignature{*this});
  }

  /// Underlying Signature object
  const Signature& sig() const {
    return sig_;
  }

  /// Print information about the attribute
  std::string print() const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::MS_SPC_NESTED_SIGN;
  }

  ~MsSpcNestedSignature() override = default;

  private:
  Signature sig_;
};

}
}

#endif

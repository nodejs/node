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
#ifndef LIEF_PE_ATTRIBUTES_MS_SPC_STATEMENT_TYPE_H
#define LIEF_PE_ATTRIBUTES_MS_SPC_STATEMENT_TYPE_H

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"
#include "LIEF/PE/signature/types.hpp"

namespace LIEF {
namespace PE {

/// Interface over the structure described by the OID ``1.3.6.1.4.1.311.2.1.11``
///
/// The internal structure is described in the official document:
/// [Windows Authenticode Portable Executable Signature Format](http://download.microsoft.com/download/9/c/5/9c5b2167-8017-4bae-9fde-d599bac8184a/Authenticode_PE.docx)
///
/// ```text
/// SpcStatementType ::= SEQUENCE of OBJECT IDENTIFIER
/// ```
class LIEF_API MsSpcStatementType : public Attribute {

  friend class Parser;
  friend class SignatureParser;

  public:
  MsSpcStatementType() = delete;
  MsSpcStatementType(oid_t oid) :
    Attribute(Attribute::TYPE::MS_SPC_STATEMENT_TYPE),
    oid_{std::move(oid)}
  {}

  MsSpcStatementType(const MsSpcStatementType&) = default;
  MsSpcStatementType& operator=(const MsSpcStatementType&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new MsSpcStatementType{*this});
  }

  /// According to the documentation:
  /// > The SpcStatementType MUST contain one Object Identifier with either
  /// > the value ``1.3.6.1.4.1.311.2.1.21 (SPC_INDIVIDUAL_SP_KEY_PURPOSE_OBJID)`` or
  /// > ``1.3.6.1.4.1.311.2.1.22 (SPC_COMMERCIAL_SP_KEY_PURPOSE_OBJID)``.
  const oid_t& oid() const {
    return oid_;
  }

  /// Print information about the attribute
  std::string print() const override;

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::MS_SPC_STATEMENT_TYPE;
  }

  void accept(Visitor& visitor) const override;
  ~MsSpcStatementType() override = default;

  private:
  oid_t oid_;
};

}
}

#endif

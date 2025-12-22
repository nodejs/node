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
#ifndef LIEF_PE_ATTRIBUTES_GENERIC_TYPE_H
#define LIEF_PE_ATTRIBUTES_GENERIC_TYPE_H
#include <memory>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"
#include "LIEF/PE/signature/Attribute.hpp"
#include "LIEF/PE/signature/types.hpp"

namespace LIEF {
class VectorStream;
namespace PE {

/// Interface over an attribute for which the internal structure is not supported by LIEF
class LIEF_API GenericType : public Attribute {
  friend class Parser;
  friend class SignatureParser;

  public:
  GenericType() :
    Attribute(Attribute::TYPE::GENERIC_TYPE)
  {}
  GenericType(oid_t oid, std::vector<uint8_t> raw) :
    Attribute(Attribute::TYPE::GENERIC_TYPE),
    oid_{std::move(oid)},
    raw_{std::move(raw)}
  {}
  GenericType(const GenericType&) = default;
  GenericType& operator=(const GenericType&) = default;

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new GenericType{*this});
  }

  /// OID of the original attribute
  const oid_t& oid() const {
    return oid_;
  }

  /// Original DER blob of the attribute
  span<const uint8_t> raw_content() const {
    return raw_;
  }

  /// Print information about the attribute
  std::string print() const override;

  void accept(Visitor& visitor) const override;

  ~GenericType() override = default;

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::GENERIC_TYPE;
  }

  private:
  oid_t oid_;
  std::vector<uint8_t> raw_;
};

}
}

#endif

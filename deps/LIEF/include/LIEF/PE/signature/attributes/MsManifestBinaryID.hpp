
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
#ifndef LIEF_PE_ATTRIBUTES_MS_MANIFEST_BINARY_ID_H
#define LIEF_PE_ATTRIBUTES_MS_MANIFEST_BINARY_ID_H

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {

/// Interface over the structure described by the OID `1.3.6.1.4.1.311.10.3.28` (szOID_PLATFORM_MANIFEST_BINARY_ID)
///
/// The internal structure is not documented but we can infer the following structure:
///
/// ```text
/// szOID_PLATFORM_MANIFEST_BINARY_ID ::= SET OF BinaryID
/// ```
///
/// `BinaryID` being an alias of UTF8STRING
class LIEF_API MsManifestBinaryID : public Attribute {

  friend class Parser;
  friend class SignatureParser;

  public:
  MsManifestBinaryID() = delete;
  MsManifestBinaryID(std::string binid) :
    Attribute(Attribute::TYPE::MS_PLATFORM_MANIFEST_BINARY_ID),
    id_(std::move(binid))
  {}

  MsManifestBinaryID(const MsManifestBinaryID&) = default;
  MsManifestBinaryID& operator=(const MsManifestBinaryID&) = default;

  /// Print information about the attribute
  std::string print() const override {
    return id_;
  }

  /// The manifest id as a string
  const std::string& manifest_id() const {
    return id_;
  }

  void manifest_id(const std::string& id) {
    id_ = id;
  }

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new MsManifestBinaryID{*this});
  }

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::MS_PLATFORM_MANIFEST_BINARY_ID;
  }

  void accept(Visitor& visitor) const override;

  virtual ~MsManifestBinaryID() = default;

  private:
  std::string id_;
};

}
}

#endif

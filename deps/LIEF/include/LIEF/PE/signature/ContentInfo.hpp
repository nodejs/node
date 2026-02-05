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
#ifndef LIEF_PE_CONTENT_INFO_H
#define LIEF_PE_CONTENT_INFO_H

#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

#include "LIEF/PE/signature/types.hpp"
#include "LIEF/PE/enums.hpp"

namespace LIEF {
namespace PE {

class Parser;
class SignatureParser;

/**
 * ContentInfo as described in the RFC2315 (https://tools.ietf.org/html/rfc2315#section-7)
 *
 * ```text
 * ContentInfo ::= SEQUENCE {
 *   contentType ContentType,
 *   content     [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL
 * }
 *
 * ContentType ::= OBJECT IDENTIFIER
 * ```
 *
 * In the case of PE signature, ContentType **must** be set to SPC_INDIRECT_DATA_OBJID
 * OID: ``1.3.6.1.4.1.311.2.1.4`` and content is defined by the structure: ``SpcIndirectDataContent``
 *
 * ```text
 * SpcIndirectDataContent ::= SEQUENCE {
 *  data          SpcAttributeTypeAndOptionalValue,
 *  messageDigest DigestInfo
 * }
 *
 * SpcAttributeTypeAndOptionalValue ::= SEQUENCE {
 *  type  ObjectID,
 *  value [0] EXPLICIT ANY OPTIONAL
 * }
 * ```
 *
 * For PE signature, ``SpcAttributeTypeAndOptionalValue.type``
 * is set to ``SPC_PE_IMAGE_DATAOBJ`` (OID: ``1.3.6.1.4.1.311.2.1.15``) and the value is defined by
 * ``SpcPeImageData``
 *
 * ```text
 * DigestInfo ::= SEQUENCE {
 *  digestAlgorithm  AlgorithmIdentifier,
 *  digest           OCTETSTRING
 * }
 *
 * AlgorithmIdentifier ::= SEQUENCE {
 *  algorithm  ObjectID,
 *  parameters [0] EXPLICIT ANY OPTIONAL
 * }
 * ```
 */
class LIEF_API ContentInfo : public Object {
  friend class Parser;
  friend class SignatureParser;

  public:
  class Content : public Object {
    public:
    Content(oid_t oid) :
      type_(std::move(oid))
    {}

    const oid_t& content_type() const {
      return type_;
    }

    virtual std::unique_ptr<Content> clone() const = 0;
    virtual void print(std::ostream& os) const = 0;

    LIEF_API friend std::ostream& operator<<(std::ostream& os, const Content& content) {
      content.print(os);
      return os;
    }

    template<class T>
    const T* cast() const {
      static_assert(std::is_base_of<Content, T>::value,
                    "Require ContentInfo inheritance");
      if (T::classof(this)) {
        return static_cast<const T*>(this);
      }
      return nullptr;
    }

    template<class T>
    T* cast() {
      return const_cast<T*>(static_cast<const Content*>(this)->cast<T>());
    }

    ~Content() override = default;
    private:
    oid_t type_;
  };
  ContentInfo();
  ContentInfo(const ContentInfo& other);
  ContentInfo(ContentInfo&& other) noexcept = default;
  ContentInfo& operator=(ContentInfo other);

  void swap(ContentInfo& other) noexcept;

  /// Return the OID that describes the content wrapped by this object.
  /// It should match SPC_INDIRECT_DATA_OBJID (1.3.6.1.4.1.311.2.1.4)
  oid_t content_type() const {
    return value_->content_type();
  }

  Content& value() {
    return *value_;
  }

  const Content& value() const {
    return *value_;
  }

  /// Return the digest (authentihash) if the underlying content type is `SPC_INDIRECT_DATA_OBJID`
  /// Otherwise, return an empty vector
  std::vector<uint8_t> digest() const;

  /// Return the digest used to hash the file
  ALGORITHMS digest_algorithm() const;

  void accept(Visitor& visitor) const override;

  ~ContentInfo() override = default;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ContentInfo& content_info);

  private:
  std::unique_ptr<Content> value_;
};

}
}

#endif

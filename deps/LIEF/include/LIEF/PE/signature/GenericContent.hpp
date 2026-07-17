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
#ifndef LIEF_PE_GENERIC_CONTENT_H
#define LIEF_PE_GENERIC_CONTENT_H

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/PE/signature/ContentInfo.hpp"

namespace LIEF {
namespace PE {
class LIEF_API GenericContent : public ContentInfo::Content {
  friend class SignatureParser;

  public:
  GenericContent();
  GenericContent(oid_t oid);
  GenericContent(const GenericContent&) = default;
  GenericContent& operator=(const GenericContent&) = default;

  std::unique_ptr<Content> clone() const override {
    return std::unique_ptr<Content>(new GenericContent{*this});
  }

  const oid_t& oid() const {
    return oid_;
  }

  span<const uint8_t> raw() const {
    return raw_;
  }

  span<uint8_t> raw() {
    return raw_;
  }

  ~GenericContent() override;

  void print(std::ostream& os) const override;
  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const GenericContent& content) {
    content.print(os);
    return os;
  }

  static bool classof(const ContentInfo::Content* content);

  private:
  oid_t oid_;
  std::vector<uint8_t> raw_;
};
}
}
#endif

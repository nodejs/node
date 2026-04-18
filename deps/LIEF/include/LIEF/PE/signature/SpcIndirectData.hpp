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
#ifndef LIEF_PE_SPC_INDIRECT_DATA_H
#define LIEF_PE_SPC_INDIRECT_DATA_H
#include <ostream>
#include <string>
#include <vector>
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/PE/signature/ContentInfo.hpp"

namespace LIEF {
namespace PE {
class LIEF_API SpcIndirectData : public ContentInfo::Content {
  friend class SignatureParser;

  public:
  static constexpr auto SPC_INDIRECT_DATA_OBJID = "1.3.6.1.4.1.311.2.1.4";

  SpcIndirectData() :
    ContentInfo::Content(SPC_INDIRECT_DATA_OBJID)
  {}
  SpcIndirectData(const SpcIndirectData&) = default;
  SpcIndirectData& operator=(const SpcIndirectData&) = default;

  std::unique_ptr<Content> clone() const override {
    return std::unique_ptr<Content>(new SpcIndirectData{*this});
  }

  /// Digest used to hash the file
  ///
  /// It should match LIEF::PE::SignerInfo::digest_algorithm
  ALGORITHMS digest_algorithm() const {
    return digest_algorithm_;
  }

  /// PE's authentihash
  ///
  /// @see LIEF::PE::Binary::authentihash
  span<const uint8_t> digest() const {
    return digest_;
  }

  const std::string& file() const {
    return file_;
  }

  const std::string& url() const {
    return url_;
  }

  void print(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const SpcIndirectData& content) {
    content.print(os);
    return os;
  }

  ~SpcIndirectData() override = default;

  static bool classof(const ContentInfo::Content* content) {
    return content->content_type() == SPC_INDIRECT_DATA_OBJID;
  }

  private:
  std::string file_;
  std::string url_;
  uint8_t flags_ = 0;
  ALGORITHMS digest_algorithm_ = ALGORITHMS::UNKNOWN;
  std::vector<uint8_t> digest_;
};
}
}
#endif

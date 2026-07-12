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
#include <string>
#include <ostream>

#include "frozen.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {

void Attribute::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

const char* to_string(Attribute::TYPE e) {
  #define ENTRY(X) std::pair(Attribute::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(CONTENT_TYPE),
    ENTRY(GENERIC_TYPE),
    ENTRY(SIGNING_CERTIFICATE_V2),
    ENTRY(SPC_SP_OPUS_INFO),
    ENTRY(SPC_RELAXED_PE_MARKER_CHECK),
    ENTRY(MS_COUNTER_SIGN),
    ENTRY(MS_SPC_NESTED_SIGN),
    ENTRY(MS_SPC_STATEMENT_TYPE),
    ENTRY(MS_PLATFORM_MANIFEST_BINARY_ID),
    ENTRY(PKCS9_AT_SEQUENCE_NUMBER),
    ENTRY(PKCS9_COUNTER_SIGNATURE),
    ENTRY(PKCS9_MESSAGE_DIGEST),
    ENTRY(PKCS9_SIGNING_TIME),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

}
}

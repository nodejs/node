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
#ifndef LIEF_COFF_UTILS_H
#define LIEF_COFF_UTILS_H
#include "LIEF/visibility.h"
#include "LIEF/COFF/Header.hpp"
#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include <string>

namespace LIEF {
namespace COFF {

/// This function determines if the given stream wraps a COFF binary and if so,
/// whether it's a regular or bigobj COFF.
LIEF_API Header::KIND get_kind(BinaryStream& stream);

/// Check if the given stream wraps a COFF file
LIEF_API inline bool is_coff(BinaryStream& stream) {
  return get_kind(stream) != Header::KIND::UNKNOWN;
}

/// Check if the `file` is a COFF
LIEF_API bool is_coff(const std::string& file);

/// Check if the given buffer points to a COFF file
LIEF_API inline bool is_coff(const uint8_t* buffer, size_t size) {
  LIEF::SpanStream strm(buffer, size);
  return is_coff(strm);
}

/// Check if the given buffer points to a COFF file
LIEF_API inline bool is_coff(const std::vector<uint8_t>& buffer) {
  return is_coff(buffer.data(), buffer.size());
}

/// Check if the COFF file wrapped by the given stream is a `bigobj`
LIEF_API inline bool is_bigobj(BinaryStream& stream) {
  return get_kind(stream) == Header::KIND::BIGOBJ;
}

/// Check if the COFF file wrapped by the given stream is regular
/// (i.e. not a bigobj)
LIEF_API inline bool is_regular(BinaryStream& stream) {
  return get_kind(stream) == Header::KIND::REGULAR;
}

}
}
#endif

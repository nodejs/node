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
#ifndef LIEF_DSC_UTILS_H
#define LIEF_DSC_UTILS_H

#include <LIEF/visibility.h>

#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include <string>

namespace LIEF {
namespace dsc {

/// Check if the given stream wraps a Dyld Shared Cache
LIEF_API bool is_shared_cache(BinaryStream& stream);

/// Check if the `file` is a dyld shared cache
inline bool is_shared_cache(const std::string& file) {
  auto fs = LIEF::FileStream::from_file(file);
  if (!fs) {
    return false;
  }
  return is_shared_cache(*fs);
}

/// Check if the given buffer points to a dyld shared cache file
inline bool is_shared_cache(const uint8_t* buffer, size_t size) {
  LIEF::SpanStream strm(buffer, size);
  return is_shared_cache(strm);
}

/// Check if the given buffer points to a dyld shared cache file
inline bool is_shared_cache(const std::vector<uint8_t>& buffer) {
  return is_shared_cache(buffer.data(), buffer.size());
}

}
}
#endif

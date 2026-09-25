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
#ifndef LIEF_DEX_UTILS_H
#define LIEF_DEX_UTILS_H

#include <string>
#include <vector>

#include "LIEF/DEX/types.hpp"

#include "LIEF/types.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class BinaryStream;
namespace DEX {

/// Check if the given file is a DEX.
LIEF_API bool is_dex(const std::string& file);

/// Check if the given raw data is a DEX.
LIEF_API bool is_dex(const std::vector<uint8_t>& raw);

/// Return the DEX version of the given file
LIEF_API dex_version_t version(const std::string& file);

/// Return the DEX version of the raw data
LIEF_API dex_version_t version(const std::vector<uint8_t>& raw);

dex_version_t version(BinaryStream& stream);

}
}


#endif

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
#ifndef LIEF_OAT_UTILS_H
#define LIEF_OAT_UTILS_H

#include <string>
#include <vector>

#include "LIEF/types.hpp"
#include "LIEF/visibility.h"

#include "LIEF/OAT/type_traits.hpp"
#include "LIEF/platforms/android.hpp"

namespace LIEF {
class BinaryStream;
namespace ELF {
class Binary;
}
namespace OAT {

LIEF_API bool is_oat(BinaryStream& stream);

/// Check if the given LIEF::ELF::Binary is an OAT one.
LIEF_API bool is_oat(const LIEF::ELF::Binary& elf_binary);

/// Check if the given file is an OAT one.
LIEF_API bool is_oat(const std::string& file);

/// Check if the given raw data is an OAT one.
LIEF_API bool is_oat(const std::vector<uint8_t>& raw);

/// Return the OAT version of the given file
LIEF_API oat_version_t version(const std::string& file);

/// Return the OAT version of the raw data
LIEF_API oat_version_t version(const std::vector<uint8_t>& raw);

/// Return the OAT version of the given LIEF::ELF::Binary
LIEF_API oat_version_t version(const LIEF::ELF::Binary& elf_binary);

/// Return the ANDROID_VERSIONS associated with the given OAT version
LIEF_API LIEF::Android::ANDROID_VERSIONS android_version(oat_version_t version);


}
}


#endif

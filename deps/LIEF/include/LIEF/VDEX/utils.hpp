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
#ifndef LIEF_VDEX_UTILS_H
#define LIEF_VDEX_UTILS_H

#include <string>
#include <vector>

#include "LIEF/VDEX/type_traits.hpp"

#include "LIEF/platforms/android.hpp"

#include "LIEF/types.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace VDEX {

/// Check if the given file is an VDEX one.
LIEF_API bool is_vdex(const std::string& file);

/// Check if the given raw data is an VDEX one.
LIEF_API bool is_vdex(const std::vector<uint8_t>& raw);

/// Return the VDEX version of the given file
LIEF_API vdex_version_t version(const std::string& file);

/// Return the VDEX version of the raw data
LIEF_API vdex_version_t version(const std::vector<uint8_t>& raw);

/// Return the ANDROID_VERSIONS associated with the given VDEX version
LIEF_API Android::ANDROID_VERSIONS android_version(vdex_version_t version);

}
}


#endif

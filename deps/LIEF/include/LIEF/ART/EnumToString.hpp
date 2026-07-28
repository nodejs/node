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
#ifndef LIEF_ART_ENUM_TO_STRING_H
#define LIEF_ART_ENUM_TO_STRING_H
#include "LIEF/visibility.h"
#include "LIEF/ART/enums.hpp"

namespace LIEF {
namespace ART {

LIEF_API const char* to_string(STORAGE_MODES e);

LIEF_API const char* to_string(ART_17::IMAGE_SECTIONS e);
LIEF_API const char* to_string(ART_29::IMAGE_SECTIONS e);
LIEF_API const char* to_string(ART_30::IMAGE_SECTIONS e);

LIEF_API const char* to_string(ART_17::IMAGE_METHODS e);
LIEF_API const char* to_string(ART_44::IMAGE_METHODS e);

LIEF_API const char* to_string(ART_17::IMAGE_ROOTS e);
LIEF_API const char* to_string(ART_44::IMAGE_ROOTS e);

} // namespace ART
} // namespace LIEF

#endif


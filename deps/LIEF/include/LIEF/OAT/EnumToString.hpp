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
#ifndef OAT_ENUM_TO_STRING_H
#define OAT_ENUM_TO_STRING_H
#include "LIEF/visibility.h"
#include "LIEF/OAT/enums.hpp"

namespace LIEF {
namespace OAT {

LIEF_API const char* to_string(OAT_CLASS_TYPES e);
LIEF_API const char* to_string(OAT_CLASS_STATUS e);
LIEF_API const char* to_string(HEADER_KEYS e);
LIEF_API const char* to_string(INSTRUCTION_SETS e);

} // namespace OAT
} // namespace LIEF

#endif


/* Copyright 2021 - 2025 R. Thomas
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
#ifndef LIEF_EXPECTED_H
#define LIEF_EXPECTED_H
#include "LIEF/config.h"

#undef TL_EXPECTED_EXCEPTIONS_ENABLED

#ifndef LIEF_EXTERNAL_EXPECTED
#include <LIEF/third-party/internal/expected.hpp>
#else
#include <tl/expected.hpp>
#endif

#endif

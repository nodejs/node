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
#ifndef LIEF_TO_JSON_H
#define LIEF_TO_JSON_H

#include "LIEF/config.h"

#ifdef LIEF_JSON_SUPPORT


#ifdef LIEF_ELF_SUPPORT
#include "LIEF/ELF/json.hpp"
#endif

#ifdef LIEF_PE_SUPPORT
#include "LIEF/PE/json.hpp"
#endif

#include "LIEF/Abstract/json.hpp"

#include "LIEF/Abstract.hpp"
#include "LIEF/ELF.hpp"
#include "LIEF/PE.hpp"

#endif // LIEF_JSON_SUPPORT

#endif

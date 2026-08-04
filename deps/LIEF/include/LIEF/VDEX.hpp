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
#ifndef LIEF_VDEX_H
#define LIEF_VDEX_H

#include "LIEF/config.h"

#if defined(LIEF_VDEX_SUPPORT)
#if !defined(LIEF_DEX_SUPPORT)
#error "The VDEX module can't be used without the DEX support"
#endif
#include "LIEF/DEX.hpp"
#include "LIEF/VDEX/Parser.hpp"
#include "LIEF/VDEX/utils.hpp"
#include "LIEF/VDEX/File.hpp"
#endif

#endif

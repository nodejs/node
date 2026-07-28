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
#ifndef  LIEF_COFF_H
#define  LIEF_COFF_H
#include "LIEF/config.h"

#if defined(LIEF_COFF_SUPPORT)
#include "LIEF/COFF/Binary.hpp"
#include "LIEF/COFF/utils.hpp"
#include "LIEF/COFF/Parser.hpp"
#include "LIEF/COFF/Header.hpp"
#include "LIEF/COFF/BigObjHeader.hpp"
#include "LIEF/COFF/RegularHeader.hpp"
#include "LIEF/COFF/ParserConfig.hpp"
#include "LIEF/COFF/Section.hpp"
#include "LIEF/COFF/Relocation.hpp"
#include "LIEF/COFF/Symbol.hpp"
#include "LIEF/COFF/String.hpp"

#include "LIEF/COFF/AuxiliarySymbol.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarybfAndefSymbol.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryCLRToken.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryFile.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryFunctionDefinition.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarySectionDefinition.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryWeakExternal.hpp"
#endif

#endif

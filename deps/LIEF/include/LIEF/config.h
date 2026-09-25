/* Copyright 2017 - 2021 A. Guinet
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

#ifndef LIEF_CONFIG_H
#define LIEF_CONFIG_H

// Main formats
#define LIEF_PE_SUPPORT 1
#define LIEF_ELF_SUPPORT 1
#define LIEF_MACHO_SUPPORT 1
#define LIEF_COFF_SUPPORT 1

// Android formats
/* #undef LIEF_OAT_SUPPORT */
/* #undef LIEF_DEX_SUPPORT */
/* #undef LIEF_VDEX_SUPPORT */
/* #undef LIEF_ART_SUPPORT */

// Extended features
/* #undef LIEF_DEBUG_INFO */
/* #undef LIEF_OBJC */
/* #undef LIEF_DYLD_SHARED_CACHE */
/* #undef LIEF_ASM_SUPPORT */
/* #undef LIEF_EXTENDED */

// LIEF options
/* #undef LIEF_JSON_SUPPORT */
/* #undef LIEF_LOGGING_SUPPORT */
/* #undef LIEF_LOGGING_DEBUG */
#define LIEF_FROZEN_ENABLED 1
/* #undef LIEF_EXTERNAL_EXPECTED */
/* #undef LIEF_EXTERNAL_UTF8CPP */
/* #undef LIEF_EXTERNAL_MBEDTLS */
/* #undef LIEF_EXTERNAL_FROZEN */
/* #undef LIEF_EXTERNAL_SPAN */

/* #undef LIEF_NLOHMANN_JSON_EXTERNAL */


#ifdef __cplusplus

static constexpr bool lief_pe_support      = 1;
static constexpr bool lief_elf_support     = 1;
static constexpr bool lief_macho_support   = 1;
static constexpr bool lief_coff_support    = 1;

static constexpr bool lief_oat_support     = 0;
static constexpr bool lief_dex_support     = 0;
static constexpr bool lief_vdex_support    = 0;
static constexpr bool lief_art_support     = 0;

static constexpr bool lief_debug_info      = 0;
static constexpr bool lief_objc            = 0;
static constexpr bool lief_extended        = 0;

static constexpr bool lief_json_support    = 0;
static constexpr bool lief_logging_support = 0;
static constexpr bool lief_logging_debug   = 0;
static constexpr bool lief_frozen_enabled  = 1;


#endif // __cplusplus

#endif

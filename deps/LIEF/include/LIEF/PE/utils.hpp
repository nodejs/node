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
#ifndef LIEF_PE_UTILS_H
#define LIEF_PE_UTILS_H
#include <vector>
#include <string>

#include "LIEF/PE/enums.hpp"
#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Binary;
class Import;

/// Enum to define the behavior of LIEF::PE::get_imphash
enum class IMPHASH_MODE {
  DEFAULT = 0,    /**< Default implementation */
  LIEF = DEFAULT, /**< Same as IMPHASH_MODE::DEFAULT */
  PEFILE,         /**< Use pefile algorithm */
  VT = PEFILE,    /**< Same as IMPHASH_MODE::PEFILE since Virus Total is using pefile */
};

/// Check if the given stream wraps a PE binary
LIEF_API bool is_pe(BinaryStream& stream);

/// check if the `file` is a PE file
LIEF_API bool is_pe(const std::string& file);

/// check if the raw data is a PE file
LIEF_API bool is_pe(const std::vector<uint8_t>& raw);

/// if the input `file` is a PE one, return `PE32` or `PE32+`
LIEF_API result<PE_TYPE> get_type(const std::string& file);

/// Return `PE32` or `PE32+`
LIEF_API result<PE_TYPE> get_type(const std::vector<uint8_t>& raw);

// In this case we assume that stream contains a valid PE stream
LIEF_LOCAL result<PE_TYPE> get_type_from_stream(BinaryStream& stream);

/// Compute the hash of imported functions
///
/// By default, it generates an hash with the following properties:
///   * Order agnostic
///   * Casse agnostic
///   * Ordinal (**in some extent**) agnostic
///
/// If one needs the same output as Virus Total (i.e. pefile), you can pass IMPHASH_MODE::PEFILE
/// as second parameter.
///
/// @warning The default algorithm used to compute the *imphash* value has some variations compared to Yara, pefile, VT implementation
///
/// @see https://www.fireeye.com/blog/threat-research/2014/01/tracking-malware-import-hashing.html
LIEF_API std::string get_imphash(const Binary& binary, IMPHASH_MODE mode = IMPHASH_MODE::DEFAULT);

/// Take a PE::Import as entry and try to resolve imports
/// by ordinal.
///
/// The ``strict`` boolean parameter enables to throw an LIEF::not_found exception
/// if the ordinal can't be resolved. Otherwise it skips the entry.
///
/// @param[in] import Import to resolve
/// @param[in] strict If set to ``true``, throw an exception if the import can't be resolved
/// @param[in] use_std If ``true``, it will use the [pefile](https://github.com/erocarrera/pefile/tree/09264be6f731bf8578aee8638cc4046154e03abf/ordlookup) look-up table for resolving imports
///
/// @return The PE::import resolved with PE::ImportEntry::name set
LIEF_API result<Import> resolve_ordinals(const Import& import, bool strict=false, bool use_std=false);

LIEF_API ALGORITHMS algo_from_oid(const std::string& oid);

/// Check that the layout of the given Binary is correct from the Windows loader
/// perspective
LIEF_API bool check_layout(const Binary& bin, std::string* error_info = nullptr);

}
}
#endif

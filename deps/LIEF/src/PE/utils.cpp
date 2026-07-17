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
#include <algorithm>
#include <iterator>

#include "logging.hpp"
#include "mbedtls/md5.h"

#include "LIEF/PE/utils.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Import.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/ImportEntry.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/BinaryStream/FileStream.hpp"
#include "PE/Structures.hpp"

#include "LIEF/utils.hpp"

#include "utils/ordinals_lookup_tables/libraries_table.hpp"
#include "utils/ordinals_lookup_tables_std/libraries_table.hpp"

#include "hash_stream.hpp"
#include "internal_utils.hpp"

namespace LIEF {
namespace PE {

static constexpr size_t SIZEOF_OPT_HEADER_32 = 0xE0;
static constexpr size_t SIZEOF_OPT_HEADER_64 = 0xF0;

inline std::string to_lower(std::string str) {
  std::string lower = str;
  std::transform(std::begin(str), std::end(str),
                 std::begin(lower), ::tolower);
  return lower;
}

bool is_pe(BinaryStream& stream) {
  using signature_t = std::array<char, sizeof(details::PE_Magic)>;
  ScopedStream scoped(stream, 0);
  if (auto dos_header = scoped->read<details::pe_dos_header>()) {
    if (dos_header->Magic != /* MZ */0x5a4d) {
      return false;
    }
    scoped->setpos(dos_header->AddressOfNewExeHeader);
    if (auto res_sig = scoped->read<signature_t>()) {
      const auto signature = *res_sig;
      return std::equal(std::begin(signature), std::end(signature),
                        std::begin(details::PE_Magic));
    }
  }
  return false;
}

bool is_pe(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return is_pe(*stream);
  }
  return false;
}


bool is_pe(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return is_pe(*stream);
  }
  return false;
}


result<PE_TYPE> get_type_from_stream(BinaryStream& stream) {
  const uint64_t cpos = stream.pos();
  stream.setpos(0);
  auto dos_hdr = stream.read<details::pe_dos_header>();
  if (!dos_hdr) {
    LIEF_ERR("Can't read the DOS Header structure");
    return make_error_code(dos_hdr.error());
  }
  stream.setpos(dos_hdr->AddressOfNewExeHeader);
  if (!stream.can_read<details::pe_header>()) {
    LIEF_ERR("Can't read the PE header");
    return make_error_code(lief_errors::read_error);
  }

  auto header = stream.read<details::pe_header>();
  if (!header) {
    LIEF_ERR("Can't read the PE header");
    return make_error_code(header.error());
  }

  const size_t sizeof_opt_header = header->SizeOfOptionalHeader;
  if (sizeof_opt_header != SIZEOF_OPT_HEADER_32 &&
      sizeof_opt_header != SIZEOF_OPT_HEADER_64) {
    LIEF_WARN("The value of the SizeOfOptionalHeader in the PE header seems corrupted 0x{:x}", sizeof_opt_header);
  }

  auto opt_hdr = stream.read<details::pe32_optional_header>();
  if (!opt_hdr) {
    LIEF_ERR("Can't read the PE optional header");
    return make_error_code(opt_hdr.error());
  }
  const auto type = static_cast<PE_TYPE>(opt_hdr->Magic);
  stream.setpos(cpos); // Restore the original position

  /*
   * We first try to determine PE's type from the OptionalHeader's Magic.
   * In some cases (cf. https://github.com/lief-project/LIEF/issues/644),
   * these magic bytes can be "corrupted" without afecting the executing.
   * So the second test to determine the PE's type is to check the value
   * of SizeOfOptionalHeader we should match sizeof(pe32_optional_header)
   * for a 32 bits PE or sizeof(pe64_optional_header) for a 64 bits PE file
   */
  if (type == PE_TYPE::PE32 || type == PE_TYPE::PE32_PLUS) {
    return type;
  }

  if (sizeof_opt_header == SIZEOF_OPT_HEADER_32) {
    return PE_TYPE::PE32;
  }

  if (sizeof_opt_header == SIZEOF_OPT_HEADER_64) {
    return PE_TYPE::PE32_PLUS;
  }

  LIEF_ERR("Can't determine the PE's type (PE32 / PE32+)");
  return make_error_code(lief_errors::file_format_error);
}

result<PE_TYPE> get_type(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return get_type_from_stream(*stream);
  }
  return make_error_code(lief_errors::file_format_error);
}

result<PE_TYPE> get_type(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return get_type_from_stream(*stream);
  }
  return make_error_code(lief_errors::file_format_error);
}


std::string get_imphash_std(const Binary& binary) {
  static const std::set<std::string> ALLOWED_EXT = {"dll", "ocx", "sys"};
  std::vector<uint8_t> md5_buffer(16);
  if (!binary.has_imports()) {
    return "";
  }
  std::string lstr;
  bool first_entry = true;
  hashstream hs(hashstream::HASH::MD5);
  for (const Import& imp : binary.imports()) {
    Import resolved = imp;
    if (auto resolution = resolve_ordinals(imp, /* strict */ false, /* use_std */ true)) {
      resolved = std::move(*resolution);
    }

    size_t ext_idx = resolved.name().find_last_of('.');
    std::string name = resolved.name();
    std::string ext;
    if (ext_idx != std::string::npos) {
      ext = to_lower(resolved.name().substr(ext_idx + 1));
    }
    if (ALLOWED_EXT.find(ext) != std::end(ALLOWED_EXT)) {
      name = name.substr(0, ext_idx);
    }

    std::string entries_string;
    for (const ImportEntry& e : resolved.entries()) {
      std::string funcname;
      if (e.is_ordinal()) {
        funcname = "ord" + std::to_string(e.ordinal());
      } else {
        funcname = e.name();
      }

      if (!entries_string.empty()) {
        entries_string += ',';
      }
      entries_string.append(name).append(".").append(funcname);
    }
    if (!first_entry) {
      lstr += ',';
    } else {
      first_entry = false;
    }
    lstr += to_lower(entries_string);

    // use write(uint8_t*, size_t) instead of write(const std::string&) to avoid null char
    hs.write(reinterpret_cast<const uint8_t*>(lstr.data()), lstr.size());
    lstr.clear();
  }

  return hex_dump(hs.raw(), "");
}


std::string get_imphash_lief(const Binary& binary) {
  std::vector<uint8_t> md5_buffer(16);
  if (!binary.has_imports()) {
    return std::to_string(0);
  }

  Binary::it_const_imports imports = binary.imports();

  std::string import_list;
  for (const Import& imp : imports) {
    Import resolved = imp;
    if (auto resolution = resolve_ordinals(imp)) {
      resolved = std::move(*resolution);
    }

    size_t ext_idx = resolved.name().find_last_of('.');
    std::string name_without_ext = resolved.name();

    if (ext_idx != std::string::npos) {
      name_without_ext = name_without_ext.substr(0, ext_idx);
    }

    std::string entries_string;
    for (const ImportEntry& e : resolved.entries()) {
      if (e.is_ordinal()) {
        entries_string.append(name_without_ext)
                      .append(".#")
                      .append(std::to_string(e.ordinal()));
      } else {
        entries_string.append(name_without_ext)
                      .append(".")
                      .append(e.name());
      }
    }
    import_list += to_lower(entries_string);
  }

  std::sort(std::begin(import_list), std::end(import_list),
            std::less<>());

  mbedtls_md5(
      reinterpret_cast<const uint8_t*>(import_list.data()),
      import_list.size(),
      md5_buffer.data());
  return hex_dump(md5_buffer, "");
}

std::string get_imphash(const Binary& binary, IMPHASH_MODE mode) {
  switch (mode) {
    case IMPHASH_MODE::LIEF:
      {
        return get_imphash_lief(binary);
      }
    case IMPHASH_MODE::PEFILE:
      {
        return get_imphash_std(binary);
      }
  }
  return "";
}

result<Import> resolve_ordinals(const Import& import, bool strict, bool use_std) {
  static const std::unordered_map<std::string, const char* (*)(uint32_t)>
  ordinals_library_tables =
  {
    { "kernel32.dll",   &kernel32_dll_lookup },
    { "ntdll.dll",      &ntdll_dll_lookup    },
    { "advapi32.dll",   &advapi32_dll_lookup },
    { "msvcp110.dll",   &msvcp110_dll_lookup },
    { "msvcp120.dll",   &msvcp120_dll_lookup },
    { "msvcr100.dll",   &msvcr100_dll_lookup },
    { "msvcr110.dll",   &msvcr110_dll_lookup },
    { "msvcr120.dll",   &msvcr120_dll_lookup },
    { "user32.dll",     &user32_dll_lookup   },
    { "comctl32.dll",   &comctl32_dll_lookup },
    { "ws2_32.dll",     &ws2_32_dll_lookup   },
    { "shcore.dll",     &shcore_dll_lookup   },
    { "oleaut32.dll",   &oleaut32_dll_lookup },
    { "mfc42u.dll",     &mfc42u_dll_lookup   },
    { "shlwapi.dll",    &shlwapi_dll_lookup  },
    { "gdi32.dll",      &gdi32_dll_lookup    },
    { "shell32.dll",    &shell32_dll_lookup  },
  };

  static const std::unordered_map<std::string, const char* (*)(uint32_t)>
  imphashstd_ordinals_library_tables =
  {
    { "ws2_32.dll",     &ws2_32_dll_lookup   },
    { "wsock32.dll",    &ws2_32_dll_lookup   },
    { "oleaut32.dll",   &oleaut32_dll_lookup },
  };

  using ordinal_resolver_t = const char*(*)(uint32_t);

  Import::it_const_entries entries = import.entries();

  if (std::all_of(std::begin(entries), std::end(entries),
                  [] (const ImportEntry& entry) { return !entry.is_ordinal(); })) {
    //LIEF_DEBUG("All imports use name. No ordinal!");
    return import;
  }

  std::string name = to_lower(import.name());

  ordinal_resolver_t ordinal_resolver = nullptr;
  if (use_std) {
    auto it = imphashstd_ordinals_library_tables.find(name);
    if (it != std::end(imphashstd_ordinals_library_tables)) {
      ordinal_resolver = it->second;
    }
  } else {
    auto it = ordinals_library_tables.find(name);
    if (it != std::end(ordinals_library_tables)) {
      ordinal_resolver = it->second;
    }
  }

  if (ordinal_resolver == nullptr) {
    if (strict) {
      return make_error_code(lief_errors::not_implemented);
    }
    LIEF_DEBUG("Ordinal lookup table for '{}' not implemented", name);
    return import;
  }

  Import resolved_import = import;
  for (ImportEntry& entry : resolved_import.entries()) {
    if (entry.is_ordinal()) {
      LIEF_DEBUG("Dealing with: {}", to_string(entry));
      const char* entry_name = ordinal_resolver(static_cast<uint32_t>(entry.ordinal()));
      if (entry_name == nullptr) {
        if (strict) {
          return make_error_code(lief_errors::not_supported);
        }
        LIEF_DEBUG("Unable to resolve ordinal: #{:d}", entry.ordinal());
        continue;
      }
      entry.data(0);
      entry.name(entry_name);
    }
  }

  return resolved_import;
}
ALGORITHMS algo_from_oid(const std::string& oid) {
  static const std::unordered_map<std::string, ALGORITHMS> OID_MAP = {
    { "2.16.840.1.101.3.4.2.3", ALGORITHMS::SHA_512 },
    { "2.16.840.1.101.3.4.2.2", ALGORITHMS::SHA_384 },
    { "2.16.840.1.101.3.4.2.1", ALGORITHMS::SHA_256 },
    { "1.3.14.3.2.26",          ALGORITHMS::SHA_1   },

    { "1.2.840.113549.2.5",     ALGORITHMS::MD5 },
    { "1.2.840.113549.2.4",     ALGORITHMS::MD4 },
    { "1.2.840.113549.2.2",     ALGORITHMS::MD2 },

    { "1.2.840.113549.1.1.1",   ALGORITHMS::RSA },
    { "1.2.840.10045.2.1",      ALGORITHMS::EC  },

    { "1.2.840.113549.1.1.4",   ALGORITHMS::MD5_RSA        },
    { "1.2.840.10040.4.3",      ALGORITHMS::SHA1_DSA       },
    { "1.2.840.113549.1.1.5",   ALGORITHMS::SHA1_RSA       },
    { "1.2.840.113549.1.1.11",  ALGORITHMS::SHA_256_RSA    },
    { "1.2.840.113549.1.1.12",  ALGORITHMS::SHA_384_RSA    },
    { "1.2.840.113549.1.1.13",  ALGORITHMS::SHA_512_RSA    },
    { "1.2.840.10045.4.1",      ALGORITHMS::SHA1_ECDSA     },
    { "1.2.840.10045.4.3.2",    ALGORITHMS::SHA_256_ECDSA  },
    { "1.2.840.10045.4.3.3",    ALGORITHMS::SHA_384_ECDSA  },
    { "1.2.840.10045.4.3.4",    ALGORITHMS::SHA_512_ECDSA  },
  };


  const auto it = OID_MAP.find(oid.c_str());
  if (it == std::end(OID_MAP)) {
    return ALGORITHMS::UNKNOWN;
  }
  return it->second;
}

}
}

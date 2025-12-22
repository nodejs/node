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
#include <sstream>

#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/ResourceData.hpp"
#include "LIEF/PE/resources/ResourceVersion.hpp"
#include "LIEF/PE/resources/ResourceStringFileInfo.hpp"
#include "LIEF/PE/resources/ResourceVarFileInfo.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"
#include "frozen.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::ResourceVersion::fixed_file_info_t::VERSION_OS, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceVersion::fixed_file_info_t::FILE_TYPE, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceVersion::fixed_file_info_t::FILE_TYPE_DETAILS, LIEF::PE::to_string);
FMT_FORMATTER(LIEF::PE::ResourceVersion::fixed_file_info_t::FILE_FLAGS, LIEF::PE::to_string);

namespace LIEF {
namespace PE {

static constexpr auto FILE_FLAGS_ARRAY = {
  ResourceVersion::fixed_file_info_t::FILE_FLAGS::DEBUG,
  ResourceVersion::fixed_file_info_t::FILE_FLAGS::INFO_INFERRED,
  ResourceVersion::fixed_file_info_t::FILE_FLAGS::PATCHED,
  ResourceVersion::fixed_file_info_t::FILE_FLAGS::PRERELEASE,
  ResourceVersion::fixed_file_info_t::FILE_FLAGS::PRIVATEBUILD,
  ResourceVersion::fixed_file_info_t::FILE_FLAGS::SPECIALBUILD,
};


// True is the stream points to a StringFileInfo structure
inline bool is_string_file_info(BinaryStream& strm) {
  ScopedStream scope(strm);
  [[maybe_unused]] auto wLength = scope->read<uint16_t>().value_or(0);
  [[maybe_unused]] auto wValueLength = scope->read<uint16_t>().value_or(0);
  auto wType = scope->read<uint16_t>().value_or(-1);
  auto szKey = scope->read_u16string().value_or(std::u16string());
  if (wType != 0 && wType != 1) {
    return false;
  }

  return u16tou8(szKey) == "StringFileInfo";
}

// True is the stream points to a VarFileInfo structure
inline bool is_var_file_info(BinaryStream& strm) {
  ScopedStream scope(strm);
  [[maybe_unused]] auto wLength = scope->read<uint16_t>().value_or(0);
  [[maybe_unused]] auto wValueLength = scope->read<uint16_t>().value_or(0);
  auto wType = scope->read<uint16_t>().value_or(-1);
  auto szKey = scope->read_u16string().value_or(std::u16string());
  if (wType != 0 && wType != 1) {
    return false;
  }

  return u16tou8(szKey) == "VarFileInfo";
}

result<ResourceVersion> ResourceVersion::parse(const ResourceData& node) {
  span<const uint8_t> content = node.content();
  if (content.empty()) {
    return make_error_code(lief_errors::corrupted);
  }
  return parse(content.data(), content.size());
}
result<ResourceVersion> ResourceVersion::parse(const uint8_t* p, size_t sz) {
  SpanStream strm(p, sz);
  return parse(strm);
}

result<ResourceVersion> ResourceVersion::parse(BinaryStream& stream) {
  // typedef struct {
  //   WORD             wLength;
  //   WORD             wValueLength;
  //   WORD             wType;
  //   WCHAR            szKey;
  //   WORD             Padding1;
  //   VS_FIXEDFILEINFO Value;
  //   WORD             Padding2;
  //   WORD             Children;
  // } VS_VERSIONINFO;
  ResourceVersion version;

  // The length, in bytes, of the VS_VERSIONINFO structure.
  // This length does not include any padding that aligns any subsequent version
  // resource data on a 32-bit boundary.
  auto wLength = stream.read<uint16_t>();
  if (!wLength) {
    return make_error_code(wLength.error());
  }

  // The length, in bytes, of the Value member. This value is zero if there is
  // no Value member associated with the current version structure.
  auto wValueLength = stream.read<uint16_t>();
  if (!wValueLength) {
    return make_error_code(wValueLength.error());
  }

  // The type of data in the version resource. This member is 1 if the version
  // resource contains text data and 0 if the version resource contains binary data.
  auto wType = stream.read<uint16_t>();
  if (!wType) {
    return make_error_code(wType.error());
  }

  if (*wType != 0 && *wType != 1) {
    LIEF_WARN("VS_VERSIONINFO.wType should be 0 or 1. Got: {}", *wType);
    return make_error_code(lief_errors::corrupted);
  }

  // The Unicode string L"VS_VERSION_INFO".
  auto szKey = stream.read_u16string();
  if (!szKey) {
    return make_error_code(wType.error());
  }

  std::string szKey_u8 = u16tou8(*szKey);
  if (szKey_u8 != "VS_VERSION_INFO") {
    LIEF_WARN("VS_VERSIONINFO.szKey should be 'VS_VERSION_INFO'. Got: {}",
              szKey_u8);
    return make_error_code(lief_errors::corrupted);
  }

  // Contains as many zero words as necessary to align the Value member on a 32-bit boundary.
  stream.align(sizeof(uint32_t));

  // Type: VS_FIXEDFILEINFO
  // Arbitrary data associated with this VS_VERSIONINFO structure.
  // The wValueLength member specifies the length of this member;
  // if wValueLength is zero, this member does not exist.
  if (*wValueLength > 0) {
    std::vector<uint8_t> Value;
    if (auto is_ok = stream.read_data(Value, *wValueLength); !is_ok) {
      return make_error_code(is_ok.error());
    }
    SpanStream fixed_strm(Value);
    if (auto is_ok = parse_fixed_file_info(version, fixed_strm); !is_ok) {
      LIEF_WARN("Failed to parse the 'VS_FIXEDFILEINFO' structure");
    }
  }

  // As many zero words as necessary to align the Children member on a
  // 32-bit boundary. These bytes are not included in wValueLength.
  // This member is optional.
  stream.align(sizeof(uint32_t));

  version
    .type(*wType)
    .key(std::move(*szKey));

  // Children:
  // An array of zero or one StringFileInfo structures, and zero or one
  // VarFileInfo structures that are children of the current VS_VERSIONINFO
  // structure.
  if (auto is_ok = parse_children(version, stream); !is_ok) {
    LIEF_WARN("Failed to parse 'VS_VERSIONINFO' children");
  }

  LIEF_DEBUG("Bytes left: {}", stream.size() - stream.pos());
  return version;
}


ok_error_t ResourceVersion::parse_fixed_file_info(ResourceVersion& version,
                                                  BinaryStream& stream)
{
  // typedef struct tagVS_FIXEDFILEINFO {
  //   DWORD dwSignature;
  //   DWORD dwStrucVersion;
  //   DWORD dwFileVersionMS;
  //   DWORD dwFileVersionLS;
  //   DWORD dwProductVersionMS;
  //   DWORD dwProductVersionLS;
  //   DWORD dwFileFlagsMask;
  //   DWORD dwFileFlags;
  //   DWORD dwFileOS;
  //   DWORD dwFileType;
  //   DWORD dwFileSubtype;
  //   DWORD dwFileDateMS;
  //   DWORD dwFileDateLS;
  // } VS_FIXEDFILEINFO;

  // Contains the value 0xFEEF04BD. This is used with the szKey member of the
  // VS_VERSIONINFO structure when searching a file for the VS_FIXEDFILEINFO structure.
  auto dwSignature = stream.read<uint32_t>();
  if (!dwSignature) { return make_error_code(dwSignature.error()); }

  if (*dwSignature != fixed_file_info_t::SIGNATURE_VALUE) {
    LIEF_WARN("VS_FIXEDFILEINFO.dwSignature: expecting 0x{:08x}, got: 0x{:08x}",
              fixed_file_info_t::SIGNATURE_VALUE, *dwSignature);
  }

  auto dwStrucVersion = stream.read<uint32_t>();
  if (!dwStrucVersion) { return make_error_code(dwStrucVersion.error()); }

  auto dwFileVersionMS = stream.read<uint32_t>();
  if (!dwFileVersionMS) { return make_error_code(dwFileVersionMS.error()); }

  auto dwFileVersionLS = stream.read<uint32_t>();
  if (!dwFileVersionLS) { return make_error_code(dwFileVersionLS.error()); }

  auto dwProductVersionMS = stream.read<uint32_t>();
  if (!dwProductVersionMS) { return make_error_code(dwProductVersionMS.error()); }

  auto dwProductVersionLS = stream.read<uint32_t>();
  if (!dwProductVersionLS) { return make_error_code(dwProductVersionLS.error()); }

  auto dwFileFlagsMask = stream.read<uint32_t>();
  if (!dwFileFlagsMask) { return make_error_code(dwFileFlagsMask.error()); }

  auto dwFileFlags = stream.read<uint32_t>();
  if (!dwFileFlags) { return make_error_code(dwFileFlags.error()); }

  auto dwFileOS = stream.read<uint32_t>();
  if (!dwFileOS) { return make_error_code(dwFileOS.error()); }

  auto dwFileType = stream.read<uint32_t>();
  if (!dwFileType) { return make_error_code(dwFileType.error()); }

  auto dwFileSubtype = stream.read<uint32_t>();
  if (!dwFileSubtype) { return make_error_code(dwFileSubtype.error()); }

  auto dwFileDateMS = stream.read<uint32_t>();
  if (!dwFileDateMS) { return make_error_code(dwFileDateMS.error()); }

  auto dwFileDateLS = stream.read<uint32_t>();
  if (!dwFileDateLS) { return make_error_code(dwFileDateLS.error()); }

  version.fixed_file_info_.signature = *dwSignature;
  version.fixed_file_info_.struct_version = *dwStrucVersion;
  version.fixed_file_info_.file_version_ms = *dwFileVersionMS;
  version.fixed_file_info_.file_version_ls = *dwFileVersionLS;
  version.fixed_file_info_.product_version_ms = *dwProductVersionMS;
  version.fixed_file_info_.product_version_ls = *dwProductVersionLS;
  version.fixed_file_info_.file_flags_mask = *dwFileFlagsMask;
  version.fixed_file_info_.file_flags = *dwFileFlags;
  version.fixed_file_info_.file_os = *dwFileOS;
  version.fixed_file_info_.file_type = *dwFileType;
  version.fixed_file_info_.file_subtype = *dwFileSubtype;
  version.fixed_file_info_.file_date_ms = *dwFileDateMS;
  version.fixed_file_info_.file_date_ls = *dwFileDateLS;

  return ok();
}

inline std::string version_to_str(uint32_t lsb, uint32_t msb) {
  return fmt::format("{}-{}-{}-{}", (msb >> 16) & 0xFFFF, msb & 0xFFFF,
                     (lsb >> 16) & 0xFFFF, lsb & 0xFFFF);
}


std::string ResourceVersion::fixed_file_info_t::to_string() const {
  static constexpr auto WIDTH = 20;
  std::ostringstream oss;
  oss << fmt::format("{:{}}: 0x{:06x}\n", "Struct Version", WIDTH, struct_version);
  oss << fmt::format("{:{}}: {}\n", "File version", WIDTH,
                     version_to_str(file_version_ls, file_version_ms));

  oss << fmt::format("{:{}}: {}\n", "Product version", WIDTH,
                     version_to_str(product_version_ls, product_version_ms));

  if (file_flags != 0) {
    oss << fmt::format("{:{}}: {} (0x{:08x})\n", "Flags", WIDTH,
                       fmt::to_string(flags()), file_flags);
  }
  oss << fmt::format("{:{}}: {} (0x{:08x})\n", "File OS", WIDTH,
                     VERSION_OS(file_os), file_os);

  oss << fmt::format("{:{}}: {} (0x{:08x})\n", "File Type", WIDTH,
                     FILE_TYPE(file_type), file_type);

  if (file_subtype > 0) {
    oss << fmt::format("{:{}}: {} (0x{:08x})\n", "File Subtype", WIDTH,
                       file_type_details(), file_subtype);
  }

  if (file_date_ms > 0 || file_date_ls > 0) {
    uint64_t ts = uint64_t(file_date_ms) << 32 | file_date_ls;
    oss << fmt::format("{:{}}: {}\n", "File Date", WIDTH, ts);
  }

  return oss.str();
}

std::vector<ResourceVersion::fixed_file_info_t::FILE_FLAGS>
  ResourceVersion::fixed_file_info_t::flags() const
{
  std::vector<FILE_FLAGS> out;
  std::copy_if(FILE_FLAGS_ARRAY.begin(), FILE_FLAGS_ARRAY.end(),
               std::back_inserter(out),
               [this] (FILE_FLAGS c) { return has(c); });
  return out;
}

ok_error_t ResourceVersion::parse_children(ResourceVersion& version,
                                           BinaryStream& stream)
{

  stream.align(sizeof(uint32_t));

  /* First Child */ {
    auto wLength = stream.peek<uint16_t>();
    if (!wLength) {
      return make_error_code(wLength.error());
    }

    std::vector<uint8_t> payload;
    if (auto is_ok = stream.read_data(payload, *wLength); !is_ok) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return make_error_code(is_ok.error());
    }

    SpanStream child_strm(payload);
    if (auto is_ok = parse_child(version, child_strm); !is_ok) {
      LIEF_WARN("Failed to parse the first child");
    }
  }

  stream.align(sizeof(uint32_t));
  /* Second Child */ {
    auto wLength = stream.peek<uint16_t>();
    if (!wLength) {
      return make_error_code(wLength.error());
    }

    std::vector<uint8_t> payload;
    if (auto is_ok = stream.read_data(payload, *wLength); !is_ok) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return make_error_code(is_ok.error());
    }
    SpanStream child_strm(payload);
    if (auto is_ok = parse_child(version, child_strm); !is_ok) {
      LIEF_WARN("Failed to parse the second child");
    }
  }
  return ok();
}


ok_error_t ResourceVersion::parse_child(ResourceVersion& version,
                                        BinaryStream& stream)
{
  if (is_var_file_info(stream)) {
    return parse_var_file_info(version, stream);
  }

  if (is_string_file_info(stream)) {
    return parse_str_file_info(version, stream);
  }
  LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
  return make_error_code(lief_errors::conversion_error);
}

ok_error_t ResourceVersion::parse_str_file_info(ResourceVersion& version,
                                                BinaryStream& stream)
{
  auto info = ResourceStringFileInfo::parse(stream);
  if (!info) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(info.error());
  }

  version.string_file_info(std::move(*info));
  return ok();
}

ok_error_t ResourceVersion::parse_var_file_info(ResourceVersion& version,
                                                BinaryStream& stream)
{
  auto info = ResourceVarFileInfo::parse(stream);
  if (!info) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(info.error());
  }

  version.var_file_info(std::move(*info));
  return ok();
}

void ResourceVersion::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string ResourceVersion::key_u8() const {
  return u16tou8(key());
}

std::ostream& operator<<(std::ostream& os, const ResourceVersion& version) {
  os << version.file_info();

  if (const ResourceStringFileInfo* info = version.string_file_info()) {
    os << *info << '\n';
  }

  if (const ResourceVarFileInfo* info = version.var_file_info()) {
    os << *info << '\n';
  }

  return os;
}

const char* to_string(ResourceVersion::fixed_file_info_t::FILE_FLAGS e) {
  #define ENTRY(X) std::pair(ResourceVersion::fixed_file_info_t::FILE_FLAGS::X, #X)
  STRING_MAP enums2str {
    ENTRY(DEBUG),
    ENTRY(INFO_INFERRED),
    ENTRY(PATCHED),
    ENTRY(PRERELEASE),
    ENTRY(PRIVATEBUILD),
    ENTRY(SPECIALBUILD),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(ResourceVersion::fixed_file_info_t::VERSION_OS e) {
  #define ENTRY(X) std::pair(ResourceVersion::fixed_file_info_t::VERSION_OS::X, #X)
  STRING_MAP enums2str {
    ENTRY(DOS_WINDOWS16),
    ENTRY(DOS_WINDOWS32),
    ENTRY(NT),
    ENTRY(NT_WINDOWS32),
    ENTRY(OS216),
    ENTRY(OS216_PM16),
    ENTRY(OS232),
    ENTRY(OS232_PM32),
    ENTRY(PM16),
    ENTRY(PM32),
    ENTRY(UNKNOWN),
    ENTRY(WINCE),
    ENTRY(WINDOWS16),
    ENTRY(WINDOWS32),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(ResourceVersion::fixed_file_info_t::FILE_TYPE e) {
  #define ENTRY(X) std::pair(ResourceVersion::fixed_file_info_t::FILE_TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(APP),
    ENTRY(DLL),
    ENTRY(DRV),
    ENTRY(FONT),
    ENTRY(STATIC_LIB),
    ENTRY(VXD),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(ResourceVersion::fixed_file_info_t::FILE_TYPE_DETAILS e) {
  #define ENTRY(X) std::pair(ResourceVersion::fixed_file_info_t::FILE_TYPE_DETAILS::X, #X)
  STRING_MAP enums2str {
    ENTRY(DRV_COMM),
    ENTRY(DRV_DISPLAY),
    ENTRY(DRV_INPUTMETHOD),
    ENTRY(DRV_INSTALLABLE),
    ENTRY(DRV_KEYBOARD),
    ENTRY(DRV_LANGUAGE),
    ENTRY(DRV_MOUSE),
    ENTRY(DRV_NETWORK),
    ENTRY(DRV_PRINTER),
    ENTRY(DRV_SOUND),
    ENTRY(DRV_SYSTEM),
    ENTRY(DRV_VERSIONED_PRINTER),
    ENTRY(FONT_RASTER),
    ENTRY(FONT_TRUETYPE),
    ENTRY(FONT_VECTOR),
    ENTRY(UNKNOWN),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}

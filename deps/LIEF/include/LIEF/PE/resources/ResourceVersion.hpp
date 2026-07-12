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
#ifndef LIEF_PE_RESOURCE_VERSION_H
#define LIEF_PE_RESOURCE_VERSION_H
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/optional.hpp"

#include "LIEF/PE/resources/ResourceStringFileInfo.hpp"
#include "LIEF/PE/resources/ResourceVarFileInfo.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class ResourceData;

/// Representation of the data associated with the `RT_VERSION` entry
///
/// See: `VS_VERSIONINFO` at https://learn.microsoft.com/en-us/windows/win32/menurc/vs-versioninfo
class LIEF_API ResourceVersion : public Object {
  public:
  static result<ResourceVersion> parse(const ResourceData& node);
  static result<ResourceVersion> parse(const uint8_t* p, size_t sz);
  static result<ResourceVersion> parse(BinaryStream& stream);

  /// This structure represents the `VS_FIXEDFILEINFO` structure defined
  /// in `verrsrc.h`.
  struct LIEF_API fixed_file_info_t {
    enum class VERSION_OS : uint32_t {
      /// The operating system for which the file was designed is unknown to the system.
      UNKNOWN = 0x00000000,

      /// The file was designed for MS-DOS.
      DOS = 0x00010000,

      /// The file was designed for 16-bit OS/2.
      OS216 = 0x00020000,

      /// The file was designed for 32-bit OS/2.
      OS232 = 0x00030000,

      /// The file was designed for Windows NT.
      NT = 0x00040000,

      /// The file was designed for Windows CE (Windows Embedded Compact).
      WINCE = 0x00050000,

      /// The file was designed for 16-bit Windows.
      WINDOWS16 = 0x00000001,

      /// The file was designed for 16-bit Presentation Manager.
      PM16 = 0x00000002,

      /// The file was designed for 32-bit Presentation Manager.
      PM32 = 0x00000003,

      /// The file was designed for 32-bit Windows.
      WINDOWS32 = 0x00000004,

      /// The file was designed for 16-bit Windows running on MS-DOS.
      DOS_WINDOWS16 = DOS | WINDOWS16,

      /// The file was designed for 32-bit Windows running on MS-DOS.
      DOS_WINDOWS32 = DOS | WINDOWS32,

      /// The file was designed for 16-bit Presentation Manager running on
      /// 16-bit OS/2.
      OS216_PM16 = OS216 | PM16,

      /// The file was designed for 32-bit Presentation Manager running on
      /// 32-bit OS/2.
      OS232_PM32 = OS232 | PM32,

      /// The file was designed for Windows NT.
      NT_WINDOWS32 = NT | WINDOWS32,
    };

    enum class FILE_TYPE : uint32_t  {
      /// The file type is unknown to the system.
      UNKNOWN = 0x00000000,

      /// The file contains an application.
      APP = 0x00000001,

      /// The file contains a DLL.
      DLL = 0x00000002,

      /// The file contains a device driver. If dwFileType is VFT_DRV,
      /// dwFileSubtype contains a more specific description of the driver.
      DRV = 0x00000003,

      /// The file contains a font. If dwFileType is VFT_FONT, dwFileSubtype
      /// contains a more specific description of the font file.
      FONT = 0x00000004,

      /// The file contains a virtual device.
      VXD = 0x00000005,

      /// The file contains a static-link library.
      STATIC_LIB = 0x00000007,
    };

    static constexpr auto DRV_K = uint64_t(1) << 33;
    static constexpr auto FONT_K = uint64_t(1) << 34;

    enum class FILE_TYPE_DETAILS : uint64_t {
      /// The type is unknown by the system.
      UNKNOWN = 0x00000000,
      /// The file contains a printer driver.
      DRV_PRINTER = 0x00000001 | DRV_K,
      /// The file contains a keyboard driver.
      DRV_KEYBOARD = 0x00000002 | DRV_K,
      /// The file contains a language driver.
      DRV_LANGUAGE = 0x00000003 | DRV_K,
      /// The file contains a display driver.
      DRV_DISPLAY = 0x00000004 | DRV_K,
      /// The file contains a mouse driver.
      DRV_MOUSE = 0x00000005 | DRV_K,
      /// The file contains a network driver.
      DRV_NETWORK = 0x00000006 | DRV_K,
      /// The file contains a system driver.
      DRV_SYSTEM = 0x00000007 | DRV_K,
      /// The file contains an installable driver.
      DRV_INSTALLABLE = 0x00000008 | DRV_K,
      /// The file contains a sound driver.
      DRV_SOUND = 0x00000009 | DRV_K,
      /// The file contains a communications driver.
      DRV_COMM = 0x0000000A  | DRV_K,

      DRV_INPUTMETHOD = 0x0000000B | DRV_K,

      /// The file contains a versioned printer driver.
      DRV_VERSIONED_PRINTER = 0x0000000C,

      /// The file contains a raster font.
      FONT_RASTER = 0x00000001 | FONT_K,

      /// The file contains a vector font.
      FONT_VECTOR = 0x00000002 | FONT_K,

      /// The file contains a TrueType font.
      FONT_TRUETYPE = 0x00000003 | FONT_K,
    };

    enum class FILE_FLAGS : uint32_t  {
      /// The file contains debugging information or is compiled with debugging
      /// features enabled.
      DEBUG = 0x00000001,

      /// The file's version structure was created dynamically; therefore, some
      /// of the members in this structure may be empty or incorrect. This flag
      /// should never be set in a file's `VS_VERSIONINFO` data.
      INFO_INFERRED = 0x00000010,

      /// The file has been modified and is not identical to the original
      /// shipping file of the same version number.
      PATCHED = 0x00000004,

      /// The file is a development version, not a commercially released product.
      PRERELEASE = 0x00000002,

      /// The file was not built using standard release procedures. If this flag
      /// is set, the StringFileInfo structure should contain a PrivateBuild entry.
      PRIVATEBUILD = 0x00000008,

      /// The file was built by the original company using standard release
      /// procedures but is a variation of the normal file of the same version
      /// number. If this flag is set, the StringFileInfo structure should
      /// contain a SpecialBuild entry.
      SPECIALBUILD = 0x00000020,
    };

    static constexpr auto SIGNATURE_VALUE = 0xFEEF04BD;
    /// Contains the value `0xFEEF04BD`. This is used with the `szKey` member of
    /// the `VS_VERSIONINFO` structure when searching a file for the
    /// `VS_FIXEDFILEINFO` structure.
    uint32_t signature = 0;

    /// The binary version number of this structure. The high-order word of
    /// this member contains the major version number, and the low-order word
    /// contains the minor version number.
    uint32_t struct_version = 0;

    /// The most significant 32 bits of the file's binary version number.
    /// This member is used with file_version_ls to form a 64-bit value used
    /// for numeric comparisons.
    uint32_t file_version_ms = 0;

    /// The least significant 32 bits of the file's binary version number.
    /// This member is used with file_version_ms to form a 64-bit value used for
    /// numeric comparisons.
    uint32_t file_version_ls = 0;

    /// The most significant 32 bits of the binary version number of the product
    /// with which this file was distributed. This member is used with
    /// product_version_ls to form a 64-bit value used for numeric comparisons.
    uint32_t product_version_ms = 0;

    /// The least significant 32 bits of the binary version number of the product
    /// with which this file was distributed. This member is used with
    /// product_version_ms to form a 64-bit value used for numeric comparisons.
    uint32_t product_version_ls = 0;

    /// Contains a bitmask that specifies the valid bits in file_flags.
    /// A bit is valid only if it was defined when the file was created.
    uint32_t file_flags_mask = 0;

    /// Contains a bitmask that specifies the Boolean attributes of the file.
    /// This member can include one or more of the values specified in FILE_FLAGS
    uint32_t file_flags = 0;

    /// The operating system for which this file was designed. This member can
    /// be one of the values specified in VERSION_OS.
    uint32_t file_os = 0;

    /// The general type of file. This member can be one of the values specified
    /// in FILE_TYPE. All other values are reserved.
    uint32_t file_type = 0;

    /// The function of the file. The possible values depend on the value of
    /// file_type.
    uint32_t file_subtype = 0;

    /// The most significant 32 bits of the file's 64-bit binary creation date
    /// and time stamp.
    uint32_t file_date_ms = 0;

    /// The least significant 32 bits of the file's 64-bit binary creation date
    /// and time stamp.
    uint32_t file_date_ls = 0;

    /// Check if the given FILE_FLAGS is present
    bool has(FILE_FLAGS f) const {
      return ((file_flags & file_flags_mask) & (uint32_t)f) != 0;
    }

    /// List of FILE_FLAGS
    std::vector<FILE_FLAGS> flags() const;

    FILE_TYPE_DETAILS file_type_details() const {
      if (file_subtype == 0) {
        return FILE_TYPE_DETAILS::UNKNOWN;
      }

      auto ty = FILE_TYPE(file_type);
      if (ty == FILE_TYPE::DRV) {
        return FILE_TYPE_DETAILS(file_subtype | DRV_K);
      }

      if (ty == FILE_TYPE::FONT) {
        return FILE_TYPE_DETAILS(file_subtype | FONT_K);
      }

      return FILE_TYPE_DETAILS::UNKNOWN;
    }

    std::string to_string() const;

    LIEF_API friend
      std::ostream& operator<<(std::ostream& os, const fixed_file_info_t& info)
    {
      os << info.to_string();
      return os;
    }
  };

  ResourceVersion(const ResourceVersion&) = default;
  ResourceVersion& operator=(const ResourceVersion&) = default;

  ResourceVersion(ResourceVersion&&) = default;
  ResourceVersion& operator=(ResourceVersion&&) = default;

  ~ResourceVersion() override = default;

  /// Return the fixed file info (`VS_FIXEDFILEINFO`)
  const fixed_file_info_t& file_info() const {
    return fixed_file_info_;
  }

  fixed_file_info_t& file_info() {
    return fixed_file_info_;
  }

  /// Return the `StringFileInfo` element
  ResourceStringFileInfo* string_file_info() {
    if (auto& info = string_file_info_) {
      return &info.value();
    }
    return nullptr;
  }

  const ResourceStringFileInfo* string_file_info() const {
    return const_cast<ResourceVersion*>(this)->string_file_info();
  }

  /// Return the `VarFileInfo` element
  ResourceVarFileInfo* var_file_info() {
    if (auto& info = var_file_info_) {
      return &info.value();
    }
    return nullptr;
  }

  /// The type of data in the version resource
  /// * `1` if it contains text data
  /// * `0` if it contains binary data
  uint16_t type() const {
    return type_;
  }

  /// The Unicode string `L"VS_VERSION_INFO"`.
  const std::u16string& key() const {
    return key_;
  }

  /// The key as an utf8 string
  std::string key_u8() const;

  ResourceVersion& type(uint16_t value) {
    type_ = value;
    return *this;
  }

  ResourceVersion& key(std::u16string value) {
    key_ = std::move(value);
    return *this;
  }
  const ResourceVarFileInfo* var_file_info() const {
    return const_cast<ResourceVersion*>(this)->var_file_info();
  }

  ResourceVersion& var_file_info(ResourceVarFileInfo info) {
    var_file_info_ = std::move(info);
    return *this;
  }

  ResourceVersion& string_file_info(ResourceStringFileInfo info) {
    string_file_info_ = std::move(info);
    return *this;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ResourceVersion& version);

  private:
  ResourceVersion() = default;

  static ok_error_t parse_children(ResourceVersion& version, BinaryStream& stream);
  static ok_error_t parse_child(ResourceVersion& version, BinaryStream& stream);
  static ok_error_t parse_fixed_file_info(ResourceVersion& version, BinaryStream& stream);
  static ok_error_t parse_str_file_info(ResourceVersion& version, BinaryStream& stream);
  static ok_error_t parse_var_file_info(ResourceVersion& version, BinaryStream& stream);

  uint16_t type_ = 0;
  std::u16string key_;
  fixed_file_info_t fixed_file_info_;

  optional<ResourceStringFileInfo> string_file_info_;
  optional<ResourceVarFileInfo> var_file_info_;
};

LIEF_API const char* to_string(ResourceVersion::fixed_file_info_t::FILE_FLAGS e);
LIEF_API const char* to_string(ResourceVersion::fixed_file_info_t::VERSION_OS e);
LIEF_API const char* to_string(ResourceVersion::fixed_file_info_t::FILE_TYPE e);
LIEF_API const char* to_string(ResourceVersion::fixed_file_info_t::FILE_TYPE_DETAILS e);

}
}

#endif

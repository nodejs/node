/* Copyright 2022 - 2025 R. Thomas
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
#ifndef LIEF_PDB_BUILD_METADATA_H
#define LIEF_PDB_BUILD_METADATA_H
#include <memory>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/optional.hpp"

namespace LIEF {
namespace pdb {

namespace details {
class BuildMetadata;
}

/// This class wraps build metadata represented by the codeview symbols:
/// `S_COMPILE3, S_COMPILE2, S_BUILDINFO, S_ENVBLOCK`
class LIEF_API BuildMetadata {
  public:

  enum class LANG : uint8_t {
    C           = 0x00,
    CPP         = 0x01,
    FORTRAN     = 0x02,
    MASM        = 0x03,
    PASCAL_LANG = 0x04,
    BASIC       = 0x05,
    COBOL       = 0x06,
    LINK        = 0x07,
    CVTRES      = 0x08,
    CVTPGD      = 0x09,
    CSHARP      = 0x0a,
    VB          = 0x0b,
    ILASM       = 0x0c,
    JAVA        = 0x0d,
    JSCRIPT     = 0x0e,
    MSIL        = 0x0f,
    HLSL        = 0x10,
    OBJC        = 0x11,
    OBJCPP      = 0x12,
    SWIFT       = 0x13,
    ALIASOBJ    = 0x14,
    RUST        = 0x15,
    GO          = 0x16,

    UNKNOWN = 0xFF,
  };

  enum class CPU : uint16_t {
    INTEL_8080      = 0x0,
    INTEL_8086      = 0x1,
    INTEL_80286     = 0x2,
    INTEL_80386     = 0x3,
    INTEL_80486     = 0x4,
    PENTIUM         = 0x5,
    PENTIUMPRO      = 0x6,
    PENTIUM3        = 0x7,
    MIPS            = 0x10,
    MIPS16          = 0x11,
    MIPS32          = 0x12,
    MIPS64          = 0x13,
    MIPSI           = 0x14,
    MIPSII          = 0x15,
    MIPSIII         = 0x16,
    MIPSIV          = 0x17,
    MIPSV           = 0x18,
    M68000          = 0x20,
    M68010          = 0x21,
    M68020          = 0x22,
    M68030          = 0x23,
    M68040          = 0x24,
    ALPHA           = 0x30,
    ALPHA_21164     = 0x31,
    ALPHA_21164A    = 0x32,
    ALPHA_21264     = 0x33,
    ALPHA_21364     = 0x34,
    PPC601          = 0x40,
    PPC603          = 0x41,
    PPC604          = 0x42,
    PPC620          = 0x43,
    PPCFP           = 0x44,
    PPCBE           = 0x45,
    SH3             = 0x50,
    SH3E            = 0x51,
    SH3DSP          = 0x52,
    SH4             = 0x53,
    SHMEDIA         = 0x54,
    ARM3            = 0x60,
    ARM4            = 0x61,
    ARM4T           = 0x62,
    ARM5            = 0x63,
    ARM5T           = 0x64,
    ARM6            = 0x65,
    ARM_XMAC        = 0x66,
    ARM_WMMX        = 0x67,
    ARM7            = 0x68,
    OMNI            = 0x70,
    IA64            = 0x80,
    IA64_2          = 0x81,
    CEE             = 0x90,
    AM33            = 0xa0,
    M32R            = 0xb0,
    TRICORE         = 0xc0,
    X64             = 0xd0,
    EBC             = 0xe0,
    THUMB           = 0xf0,
    ARMNT           = 0xf4,
    ARM64           = 0xf6,
    HYBRID_X86ARM64 = 0xf7,
    ARM64EC         = 0xf8,
    ARM64X          = 0xf9,
    D3D11_SHADER    = 0x100,
    UNKNOWN         = 0xff,
  };

  /// This structure represents a version for the backend or the frontend.
  struct version_t {
    /// Major version
    uint16_t major = 0;

    /// Minor version
    uint16_t minor = 0;

    /// Build version
    uint16_t build = 0;

    /// Quick Fix Engineeringa version
    uint16_t qfe = 0;
  };

  /// This structure represents information wrapped by the `S_BUILDINFO`
  /// symbol
  struct build_info_t {
    /// Working directory where the *build tool* was invoked
    std::string cwd;

    /// Path to the build tool (e.g. `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.36.32532\bin\HostX64\x64\CL.exe`)
    std::string build_tool;

    /// Source file consumed by the *build tool*
    std::string source_file;

    /// PDB path
    std::string pdb;

    /// Command line arguments used to invoke the *build tool*
    std::string command_line;
  };

  BuildMetadata(std::unique_ptr<details::BuildMetadata> impl);
  ~BuildMetadata();

  /// Version of the frontend (e.g. `19.36.32537`)
  version_t frontend_version() const;

  /// Version of the backend (e.g. `14.36.32537`)
  version_t backend_version() const;

  /// Version of the *tool* as a string. For instance, `Microsoft (R) CVTRES`,
  /// `Microsoft (R) LINK`.
  std::string version() const;

  /// Source language
  LANG language() const;

  /// Target CPU
  CPU target_cpu() const;

  /// Build information represented by the `S_BUILDINFO` symbol
  optional<build_info_t> build_info() const;

  /// Environment information represented by the `S_ENVBLOCK` symbol
  std::vector<std::string> env() const;

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const BuildMetadata& meta)
  {
    os << meta.to_string();
    return os;
  }

  private:
  std::unique_ptr<details::BuildMetadata> impl_;
};

LIEF_API const char* to_string(BuildMetadata::CPU cpu);
LIEF_API const char* to_string(BuildMetadata::LANG cpu);

}
}
#endif


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
#include <memory>
#include <algorithm>
#include <utility>

#include "LIEF/utils.hpp"
#include "LIEF/ELF/hash.hpp"
#include "LIEF/ELF/Note.hpp"
#include "LIEF/ELF/NoteDetails/NoteAbi.hpp"
#include "LIEF/ELF/NoteDetails/QNXStack.hpp"
#include "LIEF/ELF/NoteDetails/AndroidIdent.hpp"
#include "LIEF/ELF/NoteDetails/core/CoreAuxv.hpp"
#include "LIEF/ELF/NoteDetails/core/CoreFile.hpp"
#include "LIEF/ELF/NoteDetails/core/CorePrPsInfo.hpp"
#include "LIEF/ELF/NoteDetails/core/CorePrStatus.hpp"
#include "LIEF/ELF/NoteDetails/core/CoreSigInfo.hpp"
#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"
#include "LIEF/iostream.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "frozen.hpp"
#include "logging.hpp"
#include "internal_utils.hpp"

#define IMPL_READ_AT(T) \
  template result<T> Note::read_at(size_t) const;

#define IMPL_WRITE_AT(T) \
  template ok_error_t Note::write_at(size_t offset, const T& value);

namespace LIEF {
namespace ELF {

static constexpr auto NT_CORE_NAME = "CORE";
static constexpr auto NT_GNU_NAME = "GNU";
static constexpr auto NT_ANDROID_NAME = "Android";
static constexpr auto NT_LINUX_NAME = "LINUX";
static constexpr auto NT_GO_NAME = "Go";
static constexpr auto NT_STAPSDT_NAME = "stapsdt";
static constexpr auto NT_CRASHPAD_NAME = "Crashpad";
static constexpr auto NT_QNX = "QNX";

CONST_MAP_ALT GNU_TYPES {
  std::pair(1,     Note::TYPE::GNU_ABI_TAG),
  std::pair(2,     Note::TYPE::GNU_HWCAP),
  std::pair(3,     Note::TYPE::GNU_BUILD_ID),
  std::pair(4,     Note::TYPE::GNU_GOLD_VERSION),
  std::pair(5,     Note::TYPE::GNU_PROPERTY_TYPE_0),
};

CONST_MAP_ALT GENERIC_TYPES {
  std::pair(0x100, Note::TYPE::GNU_BUILD_ATTRIBUTE_OPEN),
  std::pair(0x101, Note::TYPE::GNU_BUILD_ATTRIBUTE_FUNC),
};

/* Core note types. */
CONST_MAP_ALT CORE_TYPES {
  std::pair(1,          Note::TYPE::CORE_PRSTATUS),
  std::pair(2,          Note::TYPE::CORE_FPREGSET),
  std::pair(3,          Note::TYPE::CORE_PRPSINFO),
  std::pair(4,          Note::TYPE::CORE_TASKSTRUCT),
  std::pair(6,          Note::TYPE::CORE_AUXV),
  std::pair(10,         Note::TYPE::CORE_PSTATUS),
  std::pair(12,         Note::TYPE::CORE_FPREGS),
  std::pair(13,         Note::TYPE::CORE_PSINFO),
  std::pair(16,         Note::TYPE::CORE_LWPSTATUS),
  std::pair(17,         Note::TYPE::CORE_LWPSINFO),
  std::pair(18,         Note::TYPE::CORE_WIN32PSTATUS),
  std::pair(0x53494749, Note::TYPE::CORE_SIGINFO),
  std::pair(0x46e62b7f, Note::TYPE::CORE_PRXFPREG),
  std::pair(0x46494c45, Note::TYPE::CORE_FILE),
};

CONST_MAP_ALT CORE_ARM_YPES {
  std::pair(0x400,      Note::TYPE::CORE_ARM_VFP),
  std::pair(0x401,      Note::TYPE::CORE_ARM_TLS),
  std::pair(0x402,      Note::TYPE::CORE_ARM_HW_BREAK),
  std::pair(0x403,      Note::TYPE::CORE_ARM_HW_WATCH),
  std::pair(0x404,      Note::TYPE::CORE_ARM_SYSTEM_CALL),
  std::pair(0x405,      Note::TYPE::CORE_ARM_SVE),
  std::pair(0x406,      Note::TYPE::CORE_ARM_PAC_MASK),
  std::pair(0x407,      Note::TYPE::CORE_ARM_PACA_KEYS),
  std::pair(0x408,      Note::TYPE::CORE_ARM_PACG_KEYS),
  std::pair(0x409,      Note::TYPE::CORE_TAGGED_ADDR_CTRL),
  std::pair(0x40a,      Note::TYPE::CORE_PAC_ENABLED_KEYS),
};

CONST_MAP_ALT CORE_X86_TYPES {
  std::pair(0x200,      Note::TYPE::CORE_X86_TLS),
  std::pair(0x201,      Note::TYPE::CORE_X86_IOPERM),
  std::pair(0x202,      Note::TYPE::CORE_X86_XSTATE),
  std::pair(0x203,      Note::TYPE::CORE_X86_CET),
};


/* Android notes */
CONST_MAP_ALT ANDROID_TYPES {
  std::pair(1, Note::TYPE::ANDROID_IDENT),
  std::pair(3, Note::TYPE::ANDROID_KUSER),
  std::pair(4, Note::TYPE::ANDROID_MEMTAG),
};

/* Go types. */
CONST_MAP_ALT GO_TYPES {
  std::pair(4, Note::TYPE::GO_BUILDID),
};

/* Stapsdt types. */
CONST_MAP_ALT STAPSDT_TYPES {
  std::pair(3, Note::TYPE::STAPSDT),
};

/* Crashpad types. */
CONST_MAP_ALT CRASHPAD_TYPES {
  std::pair(0x4f464e49, Note::TYPE::CRASHPAD),
};

/* Crashpad types. */
CONST_MAP_ALT QNX_TYPES {
  std::pair(3, Note::TYPE::QNX_STACK),
};

static inline std::string strip_zero(const std::string& name) {
  return name.c_str();
}

result<const char*> Note::type_to_section(TYPE type) {
  CONST_MAP_ALT TYPE2SECTION {
    std::pair(TYPE::GNU_ABI_TAG,              ".note.ABI-tag"),
    std::pair(TYPE::GNU_HWCAP,                ".note.gnu.hwcap"),
    std::pair(TYPE::GNU_BUILD_ID,             ".note.gnu.build-id"),
    std::pair(TYPE::GNU_GOLD_VERSION,         ".note.gnu.gold-version"),
    std::pair(TYPE::GNU_PROPERTY_TYPE_0,      ".note.gnu.property"),
    std::pair(TYPE::GNU_BUILD_ATTRIBUTE_OPEN, ".gnu.build.attributes"),
    std::pair(TYPE::GNU_BUILD_ATTRIBUTE_FUNC, ".gnu.build.attributes"),

    std::pair(TYPE::STAPSDT,                  ".note.stapsdt"),
    std::pair(TYPE::CRASHPAD,                 ".note.crashpad.info"),
    std::pair(TYPE::ANDROID_IDENT,            ".note.android.ident"),
    std::pair(TYPE::GO_BUILDID,               ".note.go.buildid"),
    std::pair(TYPE::QNX_STACK,                ".note"),
  };

  if (auto it = TYPE2SECTION.find(type); it != TYPE2SECTION.end()) {
    return it->second;
  }
  return make_error_code(lief_errors::not_found);
}

inline result<uint32_t> raw_type(const std::string& name, Note::TYPE type) {
  std::string norm_name = strip_zero(name);

  if (norm_name == NT_CORE_NAME) {
    for (const auto& [nt, ntype] : CORE_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_LINUX_NAME) {
    for (const auto& [nt, ntype] : CORE_ARM_YPES) {
      if (ntype == type) {
        return nt;
      }
    }
    for (const auto& [nt, ntype] : CORE_X86_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_ANDROID_NAME) {
    for (const auto& [nt, ntype] : ANDROID_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_GNU_NAME) {
    for (const auto& [nt, ntype] : GNU_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }

    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_GO_NAME) {
    for (const auto& [nt, ntype] : GO_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }

    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_STAPSDT_NAME) {
    for (const auto& [nt, ntype] : STAPSDT_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_CRASHPAD_NAME) {
    for (const auto& [nt, ntype] : CRASHPAD_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }

    return make_error_code(lief_errors::not_found);
  }


  if (norm_name == NT_QNX) {
    for (const auto& [nt, ntype] : QNX_TYPES) {
      if (ntype == type) {
        return nt;
      }
    }

    return make_error_code(lief_errors::not_found);
  }

  for (const auto& [nt, ntype] : GENERIC_TYPES) {
    if (ntype == type) {
      return nt;
    }
  }

  return make_error_code(lief_errors::not_found);
}



result<const char*> Note::type_owner(Note::TYPE type) {
  CONST_MAP_ALT TYPE2OWNER {
    std::pair(TYPE::GNU_ABI_TAG           ,NT_GNU_NAME),
    std::pair(TYPE::GNU_HWCAP             ,NT_GNU_NAME),
    std::pair(TYPE::GNU_BUILD_ID          ,NT_GNU_NAME),
    std::pair(TYPE::GNU_GOLD_VERSION      ,NT_GNU_NAME),
    std::pair(TYPE::GNU_PROPERTY_TYPE_0   ,NT_GNU_NAME),
    std::pair(TYPE::CORE_PRSTATUS         ,NT_CORE_NAME),
    std::pair(TYPE::CORE_FPREGSET         ,NT_CORE_NAME),
    std::pair(TYPE::CORE_PRPSINFO         ,NT_CORE_NAME),
    std::pair(TYPE::CORE_TASKSTRUCT       ,NT_CORE_NAME),
    std::pair(TYPE::CORE_AUXV             ,NT_CORE_NAME),
    std::pair(TYPE::CORE_PSTATUS          ,NT_CORE_NAME),
    std::pair(TYPE::CORE_FPREGS           ,NT_CORE_NAME),
    std::pair(TYPE::CORE_PSINFO           ,NT_CORE_NAME),
    std::pair(TYPE::CORE_LWPSTATUS        ,NT_CORE_NAME),
    std::pair(TYPE::CORE_LWPSINFO         ,NT_CORE_NAME),
    std::pair(TYPE::CORE_WIN32PSTATUS     ,NT_CORE_NAME),
    std::pair(TYPE::CORE_FILE             ,NT_CORE_NAME),
    std::pair(TYPE::CORE_PRXFPREG         ,NT_CORE_NAME),
    std::pair(TYPE::CORE_SIGINFO          ,NT_CORE_NAME),
    std::pair(TYPE::CORE_ARM_VFP          ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_TLS          ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_HW_BREAK     ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_HW_WATCH     ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_SYSTEM_CALL  ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_SVE          ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_PAC_MASK     ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_PACA_KEYS    ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_ARM_PACG_KEYS    ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_TAGGED_ADDR_CTRL ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_PAC_ENABLED_KEYS ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_X86_TLS          ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_X86_IOPERM       ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_X86_XSTATE       ,NT_LINUX_NAME),
    std::pair(TYPE::CORE_X86_CET          ,NT_LINUX_NAME),
    std::pair(TYPE::ANDROID_MEMTAG        ,NT_ANDROID_NAME),
    std::pair(TYPE::ANDROID_KUSER         ,NT_ANDROID_NAME),
    std::pair(TYPE::ANDROID_IDENT         ,NT_ANDROID_NAME),
    std::pair(TYPE::GO_BUILDID            ,NT_GO_NAME),
    std::pair(TYPE::STAPSDT               ,NT_STAPSDT_NAME),
    std::pair(TYPE::CRASHPAD              ,NT_CRASHPAD_NAME),
    std::pair(TYPE::QNX_STACK             ,NT_QNX),
  };

  if (auto it = TYPE2OWNER.find(type); it != TYPE2OWNER.end()) {
    return it->second;
  }
  return make_error_code(lief_errors::not_found);
}


result<Note::TYPE> Note::convert_type(Header::FILE_TYPE ftype, uint32_t type,
                                      const std::string& name)
{
  std::string norm_name = strip_zero(name);

  if (ftype == Header::FILE_TYPE::CORE) {
    if (norm_name == NT_CORE_NAME) {
      if (auto it = CORE_TYPES.find(type); it != CORE_TYPES.end()) {
        return it->second;
      }
      return make_error_code(lief_errors::not_found);
    }

    if (norm_name == NT_LINUX_NAME) {
      if (auto it = CORE_ARM_YPES.find(type); it != CORE_ARM_YPES.end()) {
        return it->second;
      }
      if (auto it = CORE_X86_TYPES.find(type); it != CORE_X86_TYPES.end()) {
        return it->second;
      }
      return make_error_code(lief_errors::not_found);
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_ANDROID_NAME) {
    if (auto it = ANDROID_TYPES.find(type); it != ANDROID_TYPES.end()) {
      return it->second;
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_GNU_NAME) {
    if (auto it = GNU_TYPES.find(type); it != GNU_TYPES.end()) {
      return it->second;
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_GO_NAME) {
    if (auto it = GO_TYPES.find(type); it != GO_TYPES.end()) {
      return it->second;
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_STAPSDT_NAME) {
    if (auto it = STAPSDT_TYPES.find(type); it != STAPSDT_TYPES.end()) {
      return it->second;
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_CRASHPAD_NAME) {
    if (auto it = CRASHPAD_TYPES.find(type); it != CRASHPAD_TYPES.end()) {
      return it->second;
    }
    return make_error_code(lief_errors::not_found);
  }

  if (norm_name == NT_QNX) {
    if (auto it = QNX_TYPES.find(type); it != CRASHPAD_TYPES.end()) {
      return it->second;
    }
    return make_error_code(lief_errors::not_found);
  }

  // Generic types
  if (auto it = GENERIC_TYPES.find(type); it != GENERIC_TYPES.end()) {
    return it->second;
  }

  return make_error_code(lief_errors::not_found);
}

std::unique_ptr<Note>
Note::create(BinaryStream& stream, std::string section_name,
             Header::FILE_TYPE ftype, ARCH arch, Header::CLASS cls)
{
  static constexpr uint32_t MAX_NOTE_DESCRIPTION = 1_MB;
  const size_t pos = stream.pos();
  auto res_namesz = stream.read<uint32_t>();
  if (!res_namesz) {
    return nullptr;
  }

  const auto namesz = *res_namesz;
  LIEF_DEBUG("[0x{:06x}] Name size: 0x{:x}", pos, namesz);

  auto res_descz = stream.read<uint32_t>();
  if (!res_descz) {
    return nullptr;
  }

  uint32_t descsz = std::min(*res_descz, MAX_NOTE_DESCRIPTION);
  LIEF_DEBUG("Description size: 0x{:x}", descsz);

  auto res_type = stream.read<uint32_t>();
  if (!res_type) {
    return nullptr;
  }

  uint32_t type = *res_type;
  LIEF_DEBUG("Type: 0x{:x}", type);

  if (namesz == 0) { // System reserves
    return nullptr;
  }
  std::vector<uint8_t> name_buffer(namesz, 0);
  if (!stream.read_data(name_buffer, namesz)) {
    LIEF_ERR("Can't read note name");
    return nullptr;
  }

  std::string name(reinterpret_cast<const char*>(name_buffer.data()), name_buffer.size());
  LIEF_DEBUG("Name: {}", name);

  stream.align(sizeof(uint32_t));

  std::vector<uint32_t> description;
  if (descsz > 0) {
    const size_t nb_chunks = (descsz - 1) / sizeof(uint32_t) + 1;
    description.reserve(nb_chunks);
    for (size_t i = 0; i < nb_chunks; ++i) {
      if (const auto chunk = stream.read<uint32_t>()) {
        description.push_back(*chunk);
      } else {
        break;
      }
    }
    stream.align(sizeof(uint32_t));
  }
  std::vector<uint8_t> desc_bytes;
  if (!description.empty()) {
    const auto* start_ptr = reinterpret_cast<const uint8_t*>(description.data());
    desc_bytes = {start_ptr,
                  start_ptr + description.size() * sizeof(uint32_t)};
  }

  return create(name, type, std::move(desc_bytes), std::move(section_name),
                ftype, arch, cls);
}

std::unique_ptr<Note>
Note::create(const std::string& name, Note::TYPE ntype, description_t description,
             std::string section_name,
             ARCH arch, Header::CLASS cls)
{
  std::string owner;
  auto res_owner = type_owner(ntype);
  if (res_owner) {
    if (!name.empty() && strip_zero(name) != *res_owner) {
      LIEF_WARN("The note owner of '{}' should be '{}' while '{}' is provided",
                 to_string(ntype), *res_owner, name);
    }
    owner = *res_owner;
  } else {
    if (!name.empty()) {
      owner = name;
    } else {
      LIEF_ERR("Note owner name can't be resolved!");
      return nullptr;
    }
  }

  auto int_type = raw_type(owner, ntype);
  if (!int_type) {
    LIEF_ERR("Can't determine the raw type of note '{}' for the owner '{}'",
             to_string(ntype), owner);
    return nullptr;
  }
  std::string norm_name = owner;

  // The name of these types have a special meaning
  if (ntype != Note::TYPE::GNU_BUILD_ATTRIBUTE_FUNC &&
      ntype != Note::TYPE::GNU_BUILD_ATTRIBUTE_OPEN)
  {
    norm_name = strip_zero(owner);
  }
  switch (ntype) {
    case Note::TYPE::CORE_PRSTATUS:
      {
        if (cls != Header::CLASS::ELF32 && cls != Header::CLASS::ELF64) {
          LIEF_WARN("CORE_PRSTATUS requires a valid ELF class");
          return nullptr;
        }

        if (arch == ARCH::NONE) {
          LIEF_WARN("CORE_PRSTATUS requires a valid architecture");
          return nullptr;
        }
        return std::unique_ptr<CorePrStatus>(new CorePrStatus(
              arch, cls, std::move(norm_name), *int_type, std::move(description)
        ));
      }
    case Note::TYPE::CORE_PRPSINFO:
      {
        if (cls != Header::CLASS::ELF32 && cls != Header::CLASS::ELF64) {
          LIEF_WARN("CORE_PRPSINFO requires a valid ELF class");
          return nullptr;
        }

        if (arch == ARCH::NONE) {
          LIEF_WARN("CORE_PRPSINFO requires a valid architecture");
          return nullptr;
        }
        return std::unique_ptr<CorePrPsInfo>(new CorePrPsInfo(
            arch, cls, std::move(norm_name), *int_type, std::move(description)
        ));
      }
    case Note::TYPE::CORE_FILE:
      {
        if (cls != Header::CLASS::ELF32 && cls != Header::CLASS::ELF64) {
          LIEF_WARN("CORE_FILE requires a valid ELF class");
          return nullptr;
        }

        if (arch == ARCH::NONE) {
          LIEF_WARN("CORE_FILE requires a valid architecture");
          return nullptr;
        }
        return std::unique_ptr<CoreFile>(new CoreFile(
            arch, cls, std::move(norm_name), *int_type, std::move(description)
        ));
      }
    case Note::TYPE::CORE_AUXV:
      {
        if (cls != Header::CLASS::ELF32 && cls != Header::CLASS::ELF64) {
          LIEF_WARN("CORE_AUXV requires a valid ELF class");
          return nullptr;
        }

        if (arch == ARCH::NONE) {
          LIEF_WARN("CORE_AUXV requires a valid architecture");
          return nullptr;
        }
        return std::unique_ptr<CoreAuxv>(new CoreAuxv(
            arch, cls, std::move(norm_name), *int_type, std::move(description)
        ));
      }
    case Note::TYPE::CORE_SIGINFO:
      {
        return std::unique_ptr<CoreSigInfo>(new CoreSigInfo(
            std::move(norm_name), ntype, *int_type, std::move(description),
            std::move(section_name)
        ));
      }
    case Note::TYPE::GNU_PROPERTY_TYPE_0:
      {
        if (cls != Header::CLASS::ELF32 && cls != Header::CLASS::ELF64) {
          LIEF_WARN("GNU_PROPERTY_TYPE_0 requires a valid ELF class");
          return nullptr;
        }

        if (arch == ARCH::NONE) {
          LIEF_WARN("GNU_PROPERTY_TYPE_0 requires a valid architecture");
          return nullptr;
        }
        return std::unique_ptr<NoteGnuProperty>(new NoteGnuProperty(
            arch, cls, std::move(norm_name), *int_type, std::move(description),
            std::move(section_name)
        ));
      }
    case Note::TYPE::ANDROID_IDENT:
        return std::unique_ptr<AndroidIdent>(new AndroidIdent(
            std::move(norm_name), ntype, *int_type, std::move(description),
            std::move(section_name)
        ));
    case Note::TYPE::QNX_STACK:
        return std::unique_ptr<QNXStack>(new QNXStack(
            std::move(norm_name), ntype, *int_type, std::move(description),
            std::move(section_name)
        ));
    case Note::TYPE::GNU_ABI_TAG:
        return std::unique_ptr<NoteAbi>(new NoteAbi(
            std::move(norm_name), ntype, *int_type, std::move(description),
            std::move(section_name)
        ));

    default:
        return std::unique_ptr<Note>(new Note(
            std::move(norm_name), ntype, *int_type, std::move(description),
            std::move(section_name)
        ));
  }
  return nullptr;
}


std::unique_ptr<Note>
Note::create(const std::string& name, uint32_t type, description_t description,
             std::string section_name,
             Header::FILE_TYPE ftype, ARCH arch, Header::CLASS cls)
{
  auto conv = Note::convert_type(ftype, type, name);
  if (!conv) {
    LIEF_DEBUG("Note type: 0x{:x} is not supported for owner: '{}'", type, name);
    return std::unique_ptr<Note>(new Note(name, Note::TYPE::UNKNOWN, type,
                                  std::move(description), std::move(section_name)));
  }
  return create(name, *conv, std::move(description),
                std::move(section_name), arch, cls);
}


uint64_t Note::size() const {
  uint64_t size = 0;
  size += 3 * sizeof(uint32_t);
  size += name().size() + 1;
  size = align(size, sizeof(uint32_t));
  size += description().size();
  size = align(size, sizeof(uint32_t));
  return size;
}

void Note::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void Note::dump(std::ostream& os) const {
  std::string note_name = printable_string(name());
  os << fmt::format("{}(0x{:04x}) '{}' [{}]",
                    to_string(type()), original_type(), note_name,
                    to_hex(description(), 10));
}


const char* to_string(Note::TYPE type) {
  #define ENTRY(X) std::pair(Note::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(GNU_ABI_TAG),
    ENTRY(GNU_HWCAP),
    ENTRY(GNU_ABI_TAG),
    ENTRY(GNU_HWCAP),
    ENTRY(GNU_BUILD_ID),
    ENTRY(GNU_GOLD_VERSION),
    ENTRY(GNU_PROPERTY_TYPE_0),
    ENTRY(GNU_BUILD_ATTRIBUTE_OPEN),
    ENTRY(GNU_BUILD_ATTRIBUTE_FUNC),
    ENTRY(CRASHPAD),
    ENTRY(CORE_PRSTATUS),
    ENTRY(CORE_FPREGSET),
    ENTRY(CORE_PRPSINFO),
    ENTRY(CORE_TASKSTRUCT),
    ENTRY(CORE_AUXV),
    ENTRY(CORE_PSTATUS),
    ENTRY(CORE_FPREGS),
    ENTRY(CORE_PSINFO),
    ENTRY(CORE_LWPSTATUS),
    ENTRY(CORE_LWPSINFO),
    ENTRY(CORE_WIN32PSTATUS),
    ENTRY(CORE_FILE),
    ENTRY(CORE_PRXFPREG),
    ENTRY(CORE_SIGINFO),

    ENTRY(CORE_ARM_VFP),
    ENTRY(CORE_ARM_TLS),
    ENTRY(CORE_ARM_HW_BREAK),
    ENTRY(CORE_ARM_HW_WATCH),
    ENTRY(CORE_ARM_SYSTEM_CALL),
    ENTRY(CORE_ARM_SVE),
    ENTRY(CORE_ARM_PAC_MASK),
    ENTRY(CORE_ARM_PACA_KEYS),
    ENTRY(CORE_ARM_PACG_KEYS),
    ENTRY(CORE_TAGGED_ADDR_CTRL),
    ENTRY(CORE_PAC_ENABLED_KEYS),
    ENTRY(CORE_X86_TLS),
    ENTRY(CORE_X86_IOPERM),
    ENTRY(CORE_X86_XSTATE),
    ENTRY(CORE_X86_CET),

    ENTRY(ANDROID_MEMTAG),
    ENTRY(ANDROID_KUSER),
    ENTRY(ANDROID_IDENT),
    ENTRY(STAPSDT),
    ENTRY(GO_BUILDID),
    ENTRY(QNX_STACK),
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}


template<class T>
result<T> Note::read_at(size_t offset) const {
  auto stream = SpanStream::from_vector(description_);
  if (!stream) {
    return make_error_code(get_error(stream));
  }
  return stream->peek<T>(offset);
}


result<std::string> Note::read_string_at(size_t offset, size_t max_size) const {
  auto stream = SpanStream::from_vector(description_);
  if (!stream) {
    return make_error_code(get_error(stream));
  }
  if (max_size > 0) {
    return stream->peek_string_at(offset, max_size);
  }

  return stream->peek_string_at(offset);
}

template<class T>
ok_error_t Note::write_at(size_t offset, const T& value) {
  Note::description_t original = description_;
  vector_iostream ios;
  ios.write(original);
  ios.seekp(offset);
  ios.write<T>(value);
  ios.move(description_);
  return ok();
}

ok_error_t Note::write_string_at(size_t offset, const std::string& value) {
  Note::description_t original = description_;
  const auto* data = reinterpret_cast<const uint8_t*>(value.data());

  vector_iostream ios;
  ios.write(original);
  ios.seekp(offset);
  ios.write(data, value.size());
  ios.move(description_);
  return ok();
}

IMPL_READ_AT(bool)
IMPL_READ_AT(uint8_t)
IMPL_READ_AT(int8_t)
IMPL_READ_AT(uint16_t)
IMPL_READ_AT(int16_t)
IMPL_READ_AT(uint32_t)
IMPL_READ_AT(int32_t)
IMPL_READ_AT(uint64_t)
IMPL_READ_AT(int64_t)

IMPL_WRITE_AT(bool)
IMPL_WRITE_AT(uint8_t)
IMPL_WRITE_AT(int8_t)
IMPL_WRITE_AT(uint16_t)
IMPL_WRITE_AT(int16_t)
IMPL_WRITE_AT(uint32_t)
IMPL_WRITE_AT(int32_t)
IMPL_WRITE_AT(uint64_t)
IMPL_WRITE_AT(int64_t)

} // namespace ELF
} // namespace LIEF

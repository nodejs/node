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
#include "LIEF/ELF/Relocation.hpp"
#include "frozen.hpp"
#include "logging.hpp"

#define ELF_RELOC(X, _) std::pair(Relocation::TYPE::X, #X),

namespace LIEF {
namespace ELF {
template<uint32_t>
const char* to_string(Relocation::TYPE type);

template<>
const char* to_string<Relocation::R_X64>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/x86_64.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_AARCH64>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/AArch64.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_ARM>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/ARM.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_HEXAGON>(Relocation::TYPE type) {
  #define ENTRY(X) std::pair(Relocation::TYPE::X, #X)
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/Hexagon.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_X86>(Relocation::TYPE type) {
  #define ENTRY(X) std::pair(Relocation::TYPE::X, #X)
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/i386.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_LARCH>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/LoongArch.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_MIPS>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/Mips.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_PPC>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/PowerPC.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_PPC64>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/PowerPC64.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_SPARC>(Relocation::TYPE type) {
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/Sparc.def"
  };

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_SYSZ>(Relocation::TYPE type) {
  #define ENTRY(X) std::pair(Relocation::TYPE::X, #X)
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/SystemZ.def"
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_RISCV>(Relocation::TYPE type) {
  #define ENTRY(X) std::pair(Relocation::TYPE::X, #X)
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/RISCV.def"
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_BPF>(Relocation::TYPE type) {
  #define ENTRY(X) std::pair(Relocation::TYPE::X, #X)
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/BPF.def"
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

template<>
const char* to_string<Relocation::R_SH4>(Relocation::TYPE type) {
  #define ENTRY(X) std::pair(Relocation::TYPE::X, #X)
  STRING_MAP enums2str {
    #include "LIEF/ELF/Relocations/SH4.def"
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Relocation::TYPE type) {
  auto raw_type = static_cast<uint64_t>(type);

  const uint64_t ID = (raw_type >> Relocation::R_BIT) << Relocation::R_BIT;

  if (ID == Relocation::R_X64) {
    return to_string<Relocation::R_X64>(type);
  }

  if (ID == Relocation::R_AARCH64) {
    return to_string<Relocation::R_AARCH64>(type);
  }

  if (ID == Relocation::R_ARM) {
    return to_string<Relocation::R_ARM>(type);
  }

  if (ID == Relocation::R_HEXAGON) {
    return to_string<Relocation::R_HEXAGON>(type);
  }

  if (ID == Relocation::R_X86) {
    return to_string<Relocation::R_X86>(type);
  }

  if (ID == Relocation::R_LARCH) {
    return to_string<Relocation::R_LARCH>(type);
  }

  if (ID == Relocation::R_MIPS) {
    return to_string<Relocation::R_MIPS>(type);
  }

  if (ID == Relocation::R_PPC) {
    return to_string<Relocation::R_PPC>(type);
  }

  if (ID == Relocation::R_PPC64) {
    return to_string<Relocation::R_PPC64>(type);
  }

  if (ID == Relocation::R_SPARC) {
    return to_string<Relocation::R_SPARC>(type);
  }

  if (ID == Relocation::R_SYSZ) {
    return to_string<Relocation::R_SYSZ>(type);
  }

  if (ID == Relocation::R_RISCV) {
    return to_string<Relocation::R_RISCV>(type);
  }

  if (ID == Relocation::R_BPF) {
    return to_string<Relocation::R_BPF>(type);
  }

  if (ID == Relocation::R_SH4) {
    return to_string<Relocation::R_SH4>(type);
  }

  return "UNKNOWN";
}
}
}

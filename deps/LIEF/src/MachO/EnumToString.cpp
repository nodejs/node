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
#include "LIEF/MachO/enums.hpp"
#include "LIEF/MachO/EnumToString.hpp"
#include "frozen.hpp"

namespace LIEF {
namespace MachO {

const char* to_string(MACHO_TYPES e) {
  CONST_MAP(MACHO_TYPES, const char*, 7) enumStrings {
      { MACHO_TYPES::MAGIC,        "MAGIC"},
      { MACHO_TYPES::CIGAM,        "CIGAM"},
      { MACHO_TYPES::MAGIC_64,     "MAGIC_64"},
      { MACHO_TYPES::CIGAM_64,     "CIGAM_64"},
      { MACHO_TYPES::MAGIC_FAT,    "MAGIC_FAT"},
      { MACHO_TYPES::CIGAM_FAT,    "CIGAM_FAT"},
      { MACHO_TYPES::NEURAL_MODEL, "NEURAL_MODEL"}
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "Out of range" : it->second;
}

const char* to_string(X86_RELOCATION e) {
  CONST_MAP(X86_RELOCATION, const char*, 6) enumStrings {
    { X86_RELOCATION::GENERIC_RELOC_VANILLA,        "VANILLA"        },
    { X86_RELOCATION::GENERIC_RELOC_PAIR,           "PAIR"           },
    { X86_RELOCATION::GENERIC_RELOC_SECTDIFF,       "SECTDIFF"       },
    { X86_RELOCATION::GENERIC_RELOC_PB_LA_PTR,      "PB_LA_PTR"      },
    { X86_RELOCATION::GENERIC_RELOC_LOCAL_SECTDIFF, "LOCAL_SECTDIFF" },
    { X86_RELOCATION::GENERIC_RELOC_TLV,            "TLV"            },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "Out of range" : it->second;
}


const char* to_string(X86_64_RELOCATION e) {
  CONST_MAP(X86_64_RELOCATION, const char*, 10) enumStrings {
    { X86_64_RELOCATION::X86_64_RELOC_UNSIGNED,   "UNSIGNED"   },
    { X86_64_RELOCATION::X86_64_RELOC_SIGNED,     "SIGNED"     },
    { X86_64_RELOCATION::X86_64_RELOC_BRANCH,     "BRANCH"     },
    { X86_64_RELOCATION::X86_64_RELOC_GOT_LOAD,   "GOT_LOAD"   },
    { X86_64_RELOCATION::X86_64_RELOC_GOT,        "GOT"        },
    { X86_64_RELOCATION::X86_64_RELOC_SUBTRACTOR, "SUBTRACTOR" },
    { X86_64_RELOCATION::X86_64_RELOC_SIGNED_1,   "SIGNED_1"   },
    { X86_64_RELOCATION::X86_64_RELOC_SIGNED_2,   "SIGNED_2"   },
    { X86_64_RELOCATION::X86_64_RELOC_SIGNED_4,   "SIGNED_4"   },
    { X86_64_RELOCATION::X86_64_RELOC_TLV,        "TLV"        },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "Out of range" : it->second;
}


const char* to_string(PPC_RELOCATION e) {
  CONST_MAP(PPC_RELOCATION, const char*, 16) enumStrings {
    { PPC_RELOCATION::PPC_RELOC_VANILLA,        "VANILLA"        },
    { PPC_RELOCATION::PPC_RELOC_PAIR,           "PAIR"           },
    { PPC_RELOCATION::PPC_RELOC_BR14,           "BR14"           },
    { PPC_RELOCATION::PPC_RELOC_BR24,           "BR24"           },
    { PPC_RELOCATION::PPC_RELOC_HI16,           "HI16"           },
    { PPC_RELOCATION::PPC_RELOC_LO16,           "LO16"           },
    { PPC_RELOCATION::PPC_RELOC_HA16,           "HA16"           },
    { PPC_RELOCATION::PPC_RELOC_LO14,           "LO14"           },
    { PPC_RELOCATION::PPC_RELOC_SECTDIFF,       "SECTDIFF"       },
    { PPC_RELOCATION::PPC_RELOC_PB_LA_PTR,      "PB_LA_PTR"      },
    { PPC_RELOCATION::PPC_RELOC_HI16_SECTDIFF,  "HI16_SECTDIFF"  },
    { PPC_RELOCATION::PPC_RELOC_LO16_SECTDIFF,  "LO16_SECTDIFF"  },
    { PPC_RELOCATION::PPC_RELOC_HA16_SECTDIFF,  "HA16_SECTDIFF"  },
    { PPC_RELOCATION::PPC_RELOC_JBSR,           "JBSR"           },
    { PPC_RELOCATION::PPC_RELOC_LO14_SECTDIFF,  "LO14_SECTDIFF"  },
    { PPC_RELOCATION::PPC_RELOC_LOCAL_SECTDIFF, "LOCAL_SECTDIFF" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "Out of range" : it->second;
}


const char* to_string(ARM_RELOCATION e) {
  CONST_MAP(ARM_RELOCATION, const char*, 10) enumStrings {
    { ARM_RELOCATION::ARM_RELOC_VANILLA,        "VANILLA"             },
    { ARM_RELOCATION::ARM_RELOC_PAIR,           "PAIR"                },
    { ARM_RELOCATION::ARM_RELOC_SECTDIFF,       "SECTDIFF"            },
    { ARM_RELOCATION::ARM_RELOC_LOCAL_SECTDIFF, "LOCAL_SECTDIFF"      },
    { ARM_RELOCATION::ARM_RELOC_PB_LA_PTR,      "PB_LA_PTR"           },
    { ARM_RELOCATION::ARM_RELOC_BR24,           "BR24"                },
    { ARM_RELOCATION::ARM_THUMB_RELOC_BR22,     "THUMB_RELOC_BR22"    },
    { ARM_RELOCATION::ARM_THUMB_32BIT_BRANCH,   "THUMB_32BIT_BRANCH"  },
    { ARM_RELOCATION::ARM_RELOC_HALF,           "HALF"                },
    { ARM_RELOCATION::ARM_RELOC_HALF_SECTDIFF,  "HALF_SECTDIFF"       },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "Out of range" : it->second;
}


const char* to_string(ARM64_RELOCATION e) {
  CONST_MAP(ARM64_RELOCATION, const char*, 11) enumStrings {
    { ARM64_RELOCATION::ARM64_RELOC_UNSIGNED,            "UNSIGNED"            },
    { ARM64_RELOCATION::ARM64_RELOC_SUBTRACTOR,          "SUBTRACTOR"          },
    { ARM64_RELOCATION::ARM64_RELOC_BRANCH26,            "BRANCH26"            },
    { ARM64_RELOCATION::ARM64_RELOC_PAGE21,              "PAGE21"              },
    { ARM64_RELOCATION::ARM64_RELOC_PAGEOFF12,           "PAGEOFF12"           },
    { ARM64_RELOCATION::ARM64_RELOC_GOT_LOAD_PAGE21,     "GOT_LOAD_PAGE21"     },
    { ARM64_RELOCATION::ARM64_RELOC_GOT_LOAD_PAGEOFF12,  "GOT_LOAD_PAGEOFF12"  },
    { ARM64_RELOCATION::ARM64_RELOC_POINTER_TO_GOT,      "POINTER_TO_GOT"      },
    { ARM64_RELOCATION::ARM64_RELOC_TLVP_LOAD_PAGE21,    "TLVP_LOAD_PAGE21"    },
    { ARM64_RELOCATION::ARM64_RELOC_TLVP_LOAD_PAGEOFF12, "TLVP_LOAD_PAGEOFF12" },
    { ARM64_RELOCATION::ARM64_RELOC_ADDEND,              "ADDEND"              },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "Out of range" : it->second;
}

}
}

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
#include "LIEF/OAT/enums.hpp"
#include "LIEF/OAT/EnumToString.hpp"

#include "frozen.hpp"

namespace LIEF {
namespace OAT {

const char* to_string(OAT_CLASS_TYPES e) {
  CONST_MAP(OAT_CLASS_TYPES, const char*, 3) enumStrings {
    { OAT_CLASS_TYPES::OAT_CLASS_ALL_COMPILED,  "ALL_COMPILED"  },
    { OAT_CLASS_TYPES::OAT_CLASS_SOME_COMPILED, "SOME_COMPILED" },
    { OAT_CLASS_TYPES::OAT_CLASS_NONE_COMPILED, "NONE_COMPILED" },

  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(OAT_CLASS_STATUS e) {
  CONST_MAP(OAT_CLASS_STATUS, const char*, 13) enumStrings {
    { OAT_CLASS_STATUS::STATUS_RETIRED,                       "RETIRED"                 },
    { OAT_CLASS_STATUS::STATUS_ERROR,                         "ERROR"                   },
    { OAT_CLASS_STATUS::STATUS_NOTREADY,                      "NOTREADY"                },
    { OAT_CLASS_STATUS::STATUS_IDX,                           "IDX"                     },
    { OAT_CLASS_STATUS::STATUS_LOADED,                        "LOADED"                  },
    { OAT_CLASS_STATUS::STATUS_RESOLVING,                     "RESOLVING"               },
    { OAT_CLASS_STATUS::STATUS_RESOLVED,                      "RESOLVED"                },
    { OAT_CLASS_STATUS::STATUS_VERIFYING,                     "VERIFYING"               },
    { OAT_CLASS_STATUS::STATUS_RETRY_VERIFICATION_AT_RUNTIME, "VERIFICATION_AT_RUNTIME" },
    { OAT_CLASS_STATUS::STATUS_VERIFYING_AT_RUNTIME,          "VERIFYING_AT_RUNTIME"    },
    { OAT_CLASS_STATUS::STATUS_VERIFIED,                      "VERIFIED"                },
    { OAT_CLASS_STATUS::STATUS_INITIALIZING,                  "INITIALIZING"            },
    { OAT_CLASS_STATUS::STATUS_INITIALIZED,                   "INITIALIZED"             },

  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}


const char* to_string(HEADER_KEYS e) {
  CONST_MAP(HEADER_KEYS, const char*, 11) enumStrings {
    { HEADER_KEYS::KEY_IMAGE_LOCATION,     "IMAGE_LOCATION"     },
    { HEADER_KEYS::KEY_DEX2OAT_CMD_LINE,   "DEX2OAT_CMD_LINE"   },
    { HEADER_KEYS::KEY_DEX2OAT_HOST,       "DEX2OAT_HOST"       },
    { HEADER_KEYS::KEY_PIC,                "PIC"                },
    { HEADER_KEYS::KEY_HAS_PATCH_INFO,     "HAS_PATCH_INFO"     },
    { HEADER_KEYS::KEY_DEBUGGABLE,         "DEBUGGABLE"         },
    { HEADER_KEYS::KEY_NATIVE_DEBUGGABLE,  "NATIVE_DEBUGGABLE"  },
    { HEADER_KEYS::KEY_COMPILER_FILTER,    "COMPILER_FILTER"    },
    { HEADER_KEYS::KEY_CLASS_PATH,         "CLASS_PATH"         },
    { HEADER_KEYS::KEY_BOOT_CLASS_PATH,    "BOOT_CLASS_PATH"    },
    { HEADER_KEYS::KEY_CONCURRENT_COPYING, "CONCURRENT_COPYING" },

  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}


const char* to_string(INSTRUCTION_SETS e) {
  CONST_MAP(INSTRUCTION_SETS, const char*, 8) enumStrings {
    { INSTRUCTION_SETS::INST_SET_NONE,    "NONE"    },
    { INSTRUCTION_SETS::INST_SET_ARM,     "ARM"     },
    { INSTRUCTION_SETS::INST_SET_ARM_64,  "ARM_64"  },
    { INSTRUCTION_SETS::INST_SET_THUMB2,  "THUMB2"  },
    { INSTRUCTION_SETS::INST_SET_X86,     "X86"     },
    { INSTRUCTION_SETS::INST_SET_X86_64,  "X86_64"  },
    { INSTRUCTION_SETS::INST_SET_MIPS,    "MIPS"    },
    { INSTRUCTION_SETS::INST_SET_MIPS_64, "MIPS_64" },

  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}





} // namespace OAT
} // namespace LIEF




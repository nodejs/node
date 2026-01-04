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
#include "LIEF/ART/enums.hpp"
#include "LIEF/ART/EnumToString.hpp"
#include <map>

namespace LIEF {
namespace ART {

const char* to_string(STORAGE_MODES e) {
  const std::map<STORAGE_MODES, const char*> enumStrings {
    { STORAGE_MODES::STORAGE_UNCOMPRESSED,  "UNCOMPRESSED" },
    { STORAGE_MODES::STORAGE_LZ4,           "LZ4"          },
    { STORAGE_MODES::STORAGE_LZ4HC,         "LZ4HC"        },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ART_17::IMAGE_SECTIONS e) {
  const std::map<ART_17::IMAGE_SECTIONS, const char*> enumStrings {
    { ART_17::IMAGE_SECTIONS::SECTION_OBJECTS,           "OBJECTS"          },
    { ART_17::IMAGE_SECTIONS::SECTION_ART_FIELDS,        "ART_FIELDS"       },
    { ART_17::IMAGE_SECTIONS::SECTION_ART_METHODS,       "ART_METHODS"      },
    { ART_17::IMAGE_SECTIONS::SECTION_INTERNED_STRINGS,  "INTERNED_STRINGS" },
    { ART_17::IMAGE_SECTIONS::SECTION_IMAGE_BITMAP,      "IMAGE_BITMAP"     },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ART_29::IMAGE_SECTIONS e) {
  const std::map<ART_29::IMAGE_SECTIONS, const char*> enumStrings {
    { ART_29::IMAGE_SECTIONS::SECTION_OBJECTS,             "OBJECTS"             },
    { ART_29::IMAGE_SECTIONS::SECTION_ART_FIELDS,          "ART_FIELDS"          },
    { ART_29::IMAGE_SECTIONS::SECTION_ART_METHODS,         "ART_METHODS"         },
    { ART_29::IMAGE_SECTIONS::SECTION_RUNTIME_METHODS,     "RUNTIME_METHODS"     },
    { ART_29::IMAGE_SECTIONS::SECTION_IMT_CONFLICT_TABLES, "IMT_CONFLICT_TABLES" },
    { ART_29::IMAGE_SECTIONS::SECTION_DEX_CACHE_ARRAYS,    "DEX_CACHE_ARRAYS"    },
    { ART_29::IMAGE_SECTIONS::SECTION_INTERNED_STRINGS,    "INTERNED_STRINGS"    },
    { ART_29::IMAGE_SECTIONS::SECTION_CLASS_TABLE,         "CLASS_TABLE"         },
    { ART_29::IMAGE_SECTIONS::SECTION_IMAGE_BITMAP,        "IMAGE_BITMAP"        },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ART_30::IMAGE_SECTIONS e) {
  const std::map<ART_30::IMAGE_SECTIONS, const char*> enumStrings {
    { ART_30::IMAGE_SECTIONS::SECTION_OBJECTS,             "OBJECTS"             },
    { ART_30::IMAGE_SECTIONS::SECTION_ART_FIELDS,          "ART_FIELDS"          },
    { ART_30::IMAGE_SECTIONS::SECTION_ART_METHODS,         "ART_METHODS"         },
    { ART_30::IMAGE_SECTIONS::SECTION_RUNTIME_METHODS,     "RUNTIME_METHODS"     },
    { ART_30::IMAGE_SECTIONS::SECTION_IM_TABLES,           "IM_TABLES"           },
    { ART_30::IMAGE_SECTIONS::SECTION_IMT_CONFLICT_TABLES, "IMT_CONFLICT_TABLES" },
    { ART_30::IMAGE_SECTIONS::SECTION_DEX_CACHE_ARRAYS,    "DEX_CACHE_ARRAYS"    },
    { ART_30::IMAGE_SECTIONS::SECTION_INTERNED_STRINGS,    "INTERNED_STRINGS"    },
    { ART_30::IMAGE_SECTIONS::SECTION_CLASS_TABLE,         "CLASS_TABLE"         },
    { ART_30::IMAGE_SECTIONS::SECTION_IMAGE_BITMAP,        "IMAGE_BITMAP"        },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}


const char* to_string(ART_17::IMAGE_METHODS e) {
  const std::map<ART_17::IMAGE_METHODS, const char*> enumStrings {
    { ART_17::IMAGE_METHODS::RESOLUTION_METHOD,         "RESOLUTION_METHOD"          },
    { ART_17::IMAGE_METHODS::IMT_CONFLICT_METHOD,       "IMT_CONFLICT_METHOD"        },
    { ART_17::IMAGE_METHODS::IMT_UNIMPLEMENTED_METHOD,  "IMT_UNIMPLEMENTED_METHOD"   },
    { ART_17::IMAGE_METHODS::CALLEE_SAVE_METHOD,        "CALLEE_SAVE_METHOD"         },
    { ART_17::IMAGE_METHODS::REFS_ONLY_SAVE_METHOD,     "REFS_ONLY_SAVE_METHOD"      },
    { ART_17::IMAGE_METHODS::REFS_AND_ARGS_SAVE_METHOD, "REFS_AND_ARGS_SAVE_METHOD"  },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ART_44::IMAGE_METHODS e) {
  const std::map<ART_44::IMAGE_METHODS, const char*> enumStrings {
    { ART_44::IMAGE_METHODS::RESOLUTION_METHOD,            "RESOLUTION_METHOD"            },
    { ART_44::IMAGE_METHODS::IMT_CONFLICT_METHOD,          "IMT_CONFLICT_METHOD"          },
    { ART_44::IMAGE_METHODS::IMT_UNIMPLEMENTED_METHOD,     "IMT_UNIMPLEMENTED_METHOD"     },
    { ART_44::IMAGE_METHODS::SAVE_ALL_CALLEE_SAVES_METHOD, "SAVE_ALL_CALLEE_SAVES_METHOD" },
    { ART_44::IMAGE_METHODS::SAVE_REFS_ONLY_METHOD,        "SAVE_REFS_ONLY_METHOD"        },
    { ART_44::IMAGE_METHODS::SAVE_REFS_AND_ARGS_METHOD,    "SAVE_REFS_AND_ARGS_METHOD"    },
    { ART_44::IMAGE_METHODS::SAVE_EVERYTHING_METHOD,       "SAVE_EVERYTHING_METHOD"       },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ART_17::IMAGE_ROOTS e) {
  const std::map<ART_17::IMAGE_ROOTS, const char*> enumStrings {
    { ART_17::IMAGE_ROOTS::DEX_CACHES,    "DEX_CACHES"  },
    { ART_17::IMAGE_ROOTS::CLASS_ROOTS,   "CLASS_ROOTS" },
  };
  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ART_44::IMAGE_ROOTS e) {
  const std::map<ART_44::IMAGE_ROOTS, const char*> enumStrings {
    { ART_44::IMAGE_ROOTS::DEX_CACHES,   "DEX_CACHES"   },
    { ART_44::IMAGE_ROOTS::CLASS_ROOTS,  "CLASS_ROOTS"  },
    { ART_44::IMAGE_ROOTS::CLASS_LOADER, "CLASS_LOADER" },
  };

  auto   it  = enumStrings.find(e);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}

} // namespace ART
} // namespace LIEF


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
#include "LIEF/platforms/android/version.hpp"
#include "frozen.hpp"
namespace LIEF {
namespace Android {

const char* code_name(ANDROID_VERSIONS version) {
  CONST_MAP(ANDROID_VERSIONS, const char*, 8) version2code {
    { ANDROID_VERSIONS::VERSION_UNKNOWN,  "UNKNOWN"     },
    { ANDROID_VERSIONS::VERSION_601,      "Marshmallow" },
    { ANDROID_VERSIONS::VERSION_700,      "Nougat"      },
    { ANDROID_VERSIONS::VERSION_710,      "Nougat"      },
    { ANDROID_VERSIONS::VERSION_712,      "Nougat"      },
    { ANDROID_VERSIONS::VERSION_800,      "Oreo"        },
    { ANDROID_VERSIONS::VERSION_810,      "Oreo"        },
    { ANDROID_VERSIONS::VERSION_900,      "Pie"        },
  };
  auto   it  = version2code.find(version);
  return it == version2code.end() ? "UNDEFINED" : it->second;
}

const char* version_string(ANDROID_VERSIONS version) {
  CONST_MAP(ANDROID_VERSIONS, const char*, 8) version2code {
    { ANDROID_VERSIONS::VERSION_UNKNOWN,  "UNKNOWN" },
    { ANDROID_VERSIONS::VERSION_601,      "6.0.1"   },
    { ANDROID_VERSIONS::VERSION_700,      "7.0.0"   },
    { ANDROID_VERSIONS::VERSION_710,      "7.1.0"   },
    { ANDROID_VERSIONS::VERSION_712,      "7.1.2"   },
    { ANDROID_VERSIONS::VERSION_800,      "8.0.0"   },
    { ANDROID_VERSIONS::VERSION_810,      "8.1.0"   },
    { ANDROID_VERSIONS::VERSION_900,      "9.0.0"   },

  };
  auto   it  = version2code.find(version);
  return it == version2code.end() ? "UNDEFINED" : it->second;
}

const char* to_string(ANDROID_VERSIONS version) {
  CONST_MAP(ANDROID_VERSIONS, const char*, 8) enumStrings {
    { ANDROID_VERSIONS::VERSION_UNKNOWN,  "UNKNOWN"     },
    { ANDROID_VERSIONS::VERSION_601,      "VERSION_601" },
    { ANDROID_VERSIONS::VERSION_700,      "VERSION_700" },
    { ANDROID_VERSIONS::VERSION_710,      "VERSION_710" },
    { ANDROID_VERSIONS::VERSION_712,      "VERSION_712" },
    { ANDROID_VERSIONS::VERSION_800,      "VERSION_800" },
    { ANDROID_VERSIONS::VERSION_810,      "VERSION_810" },
    { ANDROID_VERSIONS::VERSION_900,      "VERSION_900" },

  };
  auto   it  = enumStrings.find(version);
  return it == enumStrings.end() ? "UNDEFINED" : it->second;
}


}
}

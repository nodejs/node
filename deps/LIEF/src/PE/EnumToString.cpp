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
#include "LIEF/PE/EnumToString.hpp"
#include "LIEF/PE/enums.hpp"

#include "frozen.hpp"

namespace LIEF {
namespace PE {

const char* to_string(PE_TYPE e) {
  switch (e) {
    case PE_TYPE::PE32:
      return "PE32";
    case PE_TYPE::PE32_PLUS:
      return "PE32_PLUS";
    default:
      return "UNKNOWN";
  }

  return "UNKNOWN";
}

const char* to_string(ALGORITHMS e) {
  CONST_MAP(ALGORITHMS, const char*, 20) enumStrings {
    { ALGORITHMS::UNKNOWN,  "UNKNOWN"  },
    { ALGORITHMS::SHA_512,  "SHA_512"  },
    { ALGORITHMS::SHA_384,  "SHA_384"  },
    { ALGORITHMS::SHA_256,  "SHA_256"  },
    { ALGORITHMS::SHA_1,    "SHA_1"    },

    { ALGORITHMS::MD5,      "MD5"      },
    { ALGORITHMS::MD4,      "MD4"      },
    { ALGORITHMS::MD2,      "MD2"      },

    { ALGORITHMS::RSA,      "RSA"      },
    { ALGORITHMS::EC,       "EC"       },

    { ALGORITHMS::MD5_RSA,          "MD5_RSA"       },
    { ALGORITHMS::SHA1_DSA,         "SHA1_DSA"      },
    { ALGORITHMS::SHA1_RSA,         "SHA1_RSA"      },
    { ALGORITHMS::SHA_256_RSA,      "SHA_256_RSA"   },
    { ALGORITHMS::SHA_384_RSA,      "SHA_384_RSA"   },
    { ALGORITHMS::SHA_512_RSA,      "SHA_512_RSA"   },
    { ALGORITHMS::SHA1_ECDSA,       "SHA1_ECDSA"    },
    { ALGORITHMS::SHA_256_ECDSA,    "SHA_256_ECDSA" },
    { ALGORITHMS::SHA_384_ECDSA,    "SHA_384_ECDSA" },
    { ALGORITHMS::SHA_512_ECDSA,    "SHA_512_ECDSA" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}

} // namespace PE
} // namespace LIEF

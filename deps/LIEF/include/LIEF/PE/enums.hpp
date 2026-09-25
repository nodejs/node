/* Copyright 2021 - 2025 R. Thomas
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
#ifndef LIEF_PE_ENUMS_H
#define LIEF_PE_ENUMS_H
#include <cstdint>

namespace LIEF {
namespace PE {

enum class PE_TYPE : uint16_t {
  PE32      = 0x10b, ///< 32bits
  PE32_PLUS = 0x20b  ///< 64 bits
};

/// Cryptography algorithms
enum class ALGORITHMS : uint32_t {
  UNKNOWN = 0,
  SHA_512,
  SHA_384,
  SHA_256,
  SHA_1,

  MD5,
  MD4,
  MD2,

  RSA,
  EC,

  MD5_RSA,
  SHA1_DSA,
  SHA1_RSA,
  SHA_256_RSA,
  SHA_384_RSA,
  SHA_512_RSA,
  SHA1_ECDSA,
  SHA_256_ECDSA,
  SHA_384_ECDSA,
  SHA_512_ECDSA,
};

}
}

#endif

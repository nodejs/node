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
#ifndef LIEF_PE_SIGNATURE_RSA_INFO_H
#define LIEF_PE_SIGNATURE_RSA_INFO_H
#include <vector>
#include <ostream>
#include <cstdint>

#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {
class x509;

/// Object that wraps a RSA key
class LIEF_API RsaInfo {
  friend class x509;

  public:
  using rsa_ctx_handle = void*;
  using bignum_wrapper_t = std::vector<uint8_t>; ///< Container for BigInt

  public:
  RsaInfo(const RsaInfo& other);
  RsaInfo(RsaInfo&& other);
  RsaInfo& operator=(RsaInfo other);

  /// True if it embeds a public key
  bool has_public_key() const;

  /// True if it embeds a private key
  bool has_private_key() const;

  /// RSA public modulus
  bignum_wrapper_t N() const;

  /// RSA public exponent
  bignum_wrapper_t E() const;

  /// RSA private exponent
  bignum_wrapper_t D() const;

  /// First prime factor
  bignum_wrapper_t P() const;

  /// Second prime factor
  bignum_wrapper_t Q() const;

  /// Size of the public modulus (in bits)
  size_t key_size() const;

  void swap(RsaInfo& other);
  operator bool() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const RsaInfo& info);

  ~RsaInfo();

  private:
  RsaInfo();
  RsaInfo(const rsa_ctx_handle ctx);
  rsa_ctx_handle ctx_;
};

}
}
#endif

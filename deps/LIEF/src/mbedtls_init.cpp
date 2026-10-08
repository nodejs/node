/* Copyright 2021 - 2026 R. Thomas
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
#include <mutex>

#include "psa/crypto.h"
#include "threading_alt.h"
#include "mbedtls_init.hpp"
#include "logging.hpp"

#if defined(MBEDTLS_THREADING_C)
  #if defined(MBEDTLS_SELF_TEST)
    #error "MBEDTLS_SELF_TEST is not thread-safe"
  #endif

  #if defined(MBEDTLS_ECP_RESTARTABLE)
    #error "MBEDTLS_ECP_RESTARTABLE is not thread-safe"
  #endif
#endif

namespace LIEF {
void mbedtls_init() {
  static std::once_flag ONCE;
  std::call_once(ONCE, [] {
    mbedtls_init_threading_alt();
    if (psa_status_t status = psa_crypto_init(); status != PSA_SUCCESS) {
      LIEF_WARN("psa_crypto_init() didn't succeed: {}", (int)status);
    }
    std::atexit(mbedtls_psa_crypto_free);
  });
}
}

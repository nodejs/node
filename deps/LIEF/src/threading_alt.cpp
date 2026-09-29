/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include "threading_alt.h"

#include <mutex>
#include <condition_variable>
#include <mbedtls/threading.h>

inline auto* get_mu(mbedtls_platform_mutex_t* mu) {
  return reinterpret_cast<std::mutex*>(mu->mutex);
}

inline auto* get_cv(mbedtls_platform_condition_variable_t* cv) {
  return reinterpret_cast<std::condition_variable*>(cv->cv);
}

void mbedtls_init_threading_alt() {
#if defined(MBEDTLS_THREADING_ALT)
  mbedtls_threading_set_alt(
      /*mutex_init=*/
      [](mbedtls_platform_mutex_t* ctx) -> int {
        ctx->mutex = new std::mutex{};
        return 0;
      },
      /*mutex_destroy=*/
      [](mbedtls_platform_mutex_t* ctx) {
        delete get_mu(ctx);
        ctx->mutex = nullptr;
      },
      /*mutex_lock=*/
      [](mbedtls_platform_mutex_t* ctx) -> int {
        if (ctx->mutex == nullptr) {
          return MBEDTLS_ERR_THREADING_USAGE_ERROR;
        }
        get_mu(ctx)->lock();
        return 0;
      },
      /*mutex_unlock=*/
      [](mbedtls_platform_mutex_t* ctx) -> int {
        if (ctx->mutex == nullptr) {
          return MBEDTLS_ERR_THREADING_USAGE_ERROR;
        }
        get_mu(ctx)->unlock();
        return 0;
      },
      /*cond_init=*/
      [](mbedtls_platform_condition_variable_t* cv) -> int {
        cv->cv = new std::condition_variable{};
        return 0;
      },
      /*cond_destroy=*/
      [](mbedtls_platform_condition_variable_t* cv) {
        delete get_cv(cv);
        cv->cv = nullptr;
      },
      /*cond_signal=*/
      [](mbedtls_platform_condition_variable_t* cv) -> int {
        if (cv->cv == nullptr) {
          return MBEDTLS_ERR_THREADING_USAGE_ERROR;
        }
        get_cv(cv)->notify_one();
        return 0;
      },
      /*cond_broadcast=*/
      [](mbedtls_platform_condition_variable_t* cv) -> int {
        if (cv->cv == nullptr) {
          return MBEDTLS_ERR_THREADING_USAGE_ERROR;
        }
        get_cv(cv)->notify_all();
        return 0;
      },
      /*cond_wait=*/
      [](mbedtls_platform_condition_variable_t* cv,
         mbedtls_platform_mutex_t* mu) -> int {
        if (cv->cv == nullptr || mu->mutex == nullptr) {
          return MBEDTLS_ERR_THREADING_USAGE_ERROR;
        }
        std::unique_lock<std::mutex> lk(*get_mu(mu), std::adopt_lock);
        get_cv(cv)->wait(lk);
        lk.release();
        return 0;
      }
  );
#endif
}

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
#include "logging.hpp"
#include "hash_stream.hpp"
#include "mbedtls/md.h"

namespace LIEF {

inline mbedtls_md_context_t* cast(std::unique_ptr<intptr_t>& in) {
  return reinterpret_cast<mbedtls_md_context_t*>(in.get());
}

hashstream::hashstream(HASH type) :
  ctx_{reinterpret_cast<intptr_t*>(new mbedtls_md_context_t{})}
{
  int ret = 0;
  mbedtls_md_init(cast(ctx_));
  switch (type) {
    case HASH::MD5:
      {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
        ret = mbedtls_md_setup(cast(ctx_), info, 0);
        this->output_.resize(mbedtls_md_get_size(info));
        break;
      }

    case HASH::SHA1:
      {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
        ret = mbedtls_md_setup(cast(ctx_), info, 0);
        this->output_.resize(mbedtls_md_get_size(info));
        break;
      }

    case HASH::SHA224:
      {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA224);
        ret = mbedtls_md_setup(cast(ctx_), info, 0);
        this->output_.resize(mbedtls_md_get_size(info));
        break;
      }

    case HASH::SHA256:
      {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        ret = mbedtls_md_setup(cast(ctx_), info, 0);
        this->output_.resize(mbedtls_md_get_size(info));
        break;
      }

    case HASH::SHA384:
      {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
        ret = mbedtls_md_setup(cast(ctx_), info, 0);
        this->output_.resize(mbedtls_md_get_size(info));
        break;
      }


    case HASH::SHA512:
      {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA512);
        ret = mbedtls_md_setup(cast(ctx_), info, 0);
        this->output_.resize(mbedtls_md_get_size(info));
        break;
      }
  }
  mbedtls_md_starts(cast(this->ctx_));
  if (ret != 0) {
    LIEF_WARN("Error while setting up hash function");
  }
}

hashstream& hashstream::write(const uint8_t* s, size_t n) {
  int ret = mbedtls_md_update(cast(this->ctx_), s, n);
  if (ret != 0) {
    LIEF_WARN("mbedtls_md_update(0x{}, 0x{:x}) failed with retcode: 0x{:x}", reinterpret_cast<uintptr_t>(s), n, ret);
  }
  return *this;
}

hashstream& hashstream::flush() {
  int ret = mbedtls_md_finish(cast(this->ctx_), this->output_.data());
  if (ret != 0) {
    LIEF_WARN("mbedtls_md_finish() failed with retcode: 0x{:x}", ret);
  }
  return *this;
}

hashstream::~hashstream() {
  mbedtls_md_free(cast(ctx_));
  delete reinterpret_cast<mbedtls_md_context_t*>(ctx_.release());
}


}


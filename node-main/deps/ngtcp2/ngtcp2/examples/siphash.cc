/* Copyright 2019 The BoringSSL Authors
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

/*
 * ngtcp2
 *
 * Copyright (c) 2025 nghttp2 contributors
 * Copyright (c) 2025 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <cstdint>
#include <cstring>

#include "siphash.h"

namespace {
auto CRYPTO_load_u64_le(std::span<const uint8_t, sizeof(uint64_t)> in) {
  uint64_t v;

  memcpy(&v, in.data(), sizeof(v));

  if constexpr (std::endian::native == std::endian::big) {
    return byteswap(v);
  }

  return v;
}
} // namespace

namespace {
constexpr void siphash_round(uint64_t v[4]) {
  v[0] += v[1];
  v[2] += v[3];
  v[1] = std::rotl(v[1], 13);
  v[3] = std::rotl(v[3], 16);
  v[1] ^= v[0];
  v[3] ^= v[2];
  v[0] = std::rotl(v[0], 32);
  v[2] += v[1];
  v[0] += v[3];
  v[1] = std::rotl(v[1], 17);
  v[3] = std::rotl(v[3], 21);
  v[1] ^= v[2];
  v[3] ^= v[0];
  v[2] = std::rotl(v[2], 32);
}
} // namespace

uint64_t siphash24(std::span<const uint64_t, 2> key,
                   std::span<const uint8_t> input) {
  const auto orig_input_len = input.size();
  uint64_t v[]{
    key[0] ^ UINT64_C(0x736f6d6570736575),
    key[1] ^ UINT64_C(0x646f72616e646f6d),
    key[0] ^ UINT64_C(0x6c7967656e657261),
    key[1] ^ UINT64_C(0x7465646279746573),
  };

  while (input.size() >= sizeof(uint64_t)) {
    auto m = CRYPTO_load_u64_le(input.first<sizeof(uint64_t)>());
    v[3] ^= m;
    siphash_round(v);
    siphash_round(v);
    v[0] ^= m;

    input = input.subspan(sizeof(uint64_t));
  }

  std::array<uint8_t, sizeof(uint64_t)> last_block{};
  std::ranges::copy(input, std::ranges::begin(last_block));
  last_block.back() = orig_input_len & 0xff;

  auto last_block_word = CRYPTO_load_u64_le(last_block);
  v[3] ^= last_block_word;
  siphash_round(v);
  siphash_round(v);
  v[0] ^= last_block_word;

  v[2] ^= 0xff;
  siphash_round(v);
  siphash_round(v);
  siphash_round(v);
  siphash_round(v);

  return v[0] ^ v[1] ^ v[2] ^ v[3];
}

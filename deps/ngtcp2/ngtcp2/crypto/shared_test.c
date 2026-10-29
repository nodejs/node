/*
 * ngtcp2
 *
 * Copyright (c) 2026 ngtcp2 contributors
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
#include "shared_test.h"

#include <stdio.h>

#include "shared.h"
#include "ngtcp2_macro.h"

static const MunitTest tests[] = {
  munit_void_test(test_ngtcp2_crypto_verify_retry_token),
  munit_void_test(test_ngtcp2_crypto_verify_regular_token),
  munit_test_end(),
};

const MunitSuite shared_suite = {
  .prefix = "/shared",
  .tests = tests,
};

void test_ngtcp2_crypto_verify_retry_token(void) {
  const uint8_t secret[] = "retry-token-secret";
  const ngtcp2_sockaddr_in6 in6addr = {
    .sin6_family = NGTCP2_AF_INET6,
    .sin6_port = 39918,
  };
  const ngtcp2_sockaddr_in inaddr = {
    .sin_family = NGTCP2_AF_INET,
    .sin_port = 39918,
  };
  const ngtcp2_cid retry_scid = {
    .datalen = NGTCP2_MAX_CIDLEN,
    .data = {0xBA, 0xAD, 0xF0, 0x0D},
  };
  const ngtcp2_cid odcid = {
    .datalen = NGTCP2_MAX_CIDLEN,
    .data = {0xBA, 0xAD, 0xCA, 0xCE},
  };
  const ngtcp2_cid dcid = {
    .datalen = NGTCP2_MAX_CIDLEN,
    .data = {0xDE, 0xAD, 0xF1, 0x5b},
  };
  ngtcp2_cid decoded_odcid;
  ngtcp2_tstamp t = 3600 * NGTCP2_SECONDS;
  uint8_t token[NGTCP2_CRYPTO_MAX_RETRY_TOKENLEN2];
  ngtcp2_ssize tokenlen;
  int rv;

  tokenlen = ngtcp2_crypto_generate_retry_token2(
    token, secret, ngtcp2_strlen_lit(secret), NGTCP2_PROTO_VER_V1,
    (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr), &retry_scid, &odcid, t);

  assert_ptrdiff(NGTCP2_CRYPTO_MAX_RETRY_TOKENLEN2, ==, tokenlen);

  /* Successful validation */
  rv = ngtcp2_crypto_verify_retry_token2(
    &decoded_odcid, token, (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    NGTCP2_PROTO_VER_V1, (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr),
    &retry_scid, 10 * NGTCP2_SECONDS, t);

  assert_int(0, ==, rv);
  assert_true(ngtcp2_cid_eq(&odcid, &decoded_odcid));

  /* Timeout */
  rv = ngtcp2_crypto_verify_retry_token2(
    &decoded_odcid, token, (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    NGTCP2_PROTO_VER_V1, (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr),
    &retry_scid, 10 * NGTCP2_SECONDS, t + 10 * NGTCP2_SECONDS);

  assert_int(NGTCP2_CRYPTO_ERR_VERIFY_TOKEN, ==, rv);

  /* Bad DCID */
  rv = ngtcp2_crypto_verify_retry_token2(
    &decoded_odcid, token, (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    NGTCP2_PROTO_VER_V1, (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr),
    &dcid, 10 * NGTCP2_SECONDS, t);

  assert_int(NGTCP2_CRYPTO_ERR_UNREADABLE_TOKEN, ==, rv);

  /* Bad address */
  rv = ngtcp2_crypto_verify_retry_token2(
    &decoded_odcid, token, (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    NGTCP2_PROTO_VER_V1, (const ngtcp2_sockaddr *)&inaddr, sizeof(inaddr),
    &retry_scid, 10 * NGTCP2_SECONDS, t);

  assert_int(NGTCP2_CRYPTO_ERR_VERIFY_TOKEN, ==, rv);

  /* Truncated token */
  rv = ngtcp2_crypto_verify_retry_token2(
    &decoded_odcid, token, (size_t)tokenlen - 1, secret,
    ngtcp2_strlen_lit(secret), NGTCP2_PROTO_VER_V1,
    (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr), &retry_scid,
    10 * NGTCP2_SECONDS, t);

  assert_int(NGTCP2_CRYPTO_ERR_UNREADABLE_TOKEN, ==, rv);

  /* Bad magic */
  token[0] = 0;

  rv = ngtcp2_crypto_verify_retry_token2(
    &decoded_odcid, token, (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    NGTCP2_PROTO_VER_V1, (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr),
    &retry_scid, 10 * NGTCP2_SECONDS, t);

  assert_int(NGTCP2_CRYPTO_ERR_UNREADABLE_TOKEN, ==, rv);
}

void test_ngtcp2_crypto_verify_regular_token(void) {
  const uint8_t secret[] = "regular-token-secret";
  const ngtcp2_sockaddr_in6 in6addr = {
    .sin6_family = NGTCP2_AF_INET6,
    .sin6_port = 39918,
  };
  const ngtcp2_sockaddr_in inaddr = {
    .sin_family = NGTCP2_AF_INET,
    .sin_port = 39918,
  };
  const uint8_t token_data[] = "I am the token data";
  const ngtcp2_tstamp timeout = 10 * NGTCP2_SECONDS;
  ngtcp2_tstamp t = 3600 * NGTCP2_SECONDS;
  uint8_t token[NGTCP2_CRYPTO_MAX_REGULAR_TOKENLEN + 256];
  ngtcp2_ssize tokenlen;
  ngtcp2_ssize token_datalen;
  uint8_t decoded_token_data[256];

  tokenlen = ngtcp2_crypto_generate_regular_token2(
    token, secret, ngtcp2_strlen_lit(secret), (const ngtcp2_sockaddr *)&in6addr,
    sizeof(in6addr), token_data, ngtcp2_strlen_lit(token_data), t);

  assert_ptrdiff(NGTCP2_CRYPTO_MAX_REGULAR_TOKENLEN +
                   ngtcp2_strlen_lit(token_data),
                 ==, tokenlen);

  /* Successful validation */
  token_datalen = ngtcp2_crypto_verify_regular_token2(
    decoded_token_data, sizeof(decoded_token_data), token, (size_t)tokenlen,
    secret, ngtcp2_strlen_lit(secret), (const ngtcp2_sockaddr *)&in6addr,
    sizeof(in6addr), timeout, t);

  assert_ptrdiff(ngtcp2_strlen_lit(token_data), ==, token_datalen);
  assert_memory_equal(ngtcp2_strlen_lit(token_data), token_data,
                      decoded_token_data);

  /* Timeout */
  token_datalen = ngtcp2_crypto_verify_regular_token2(
    decoded_token_data, sizeof(decoded_token_data), token, (size_t)tokenlen,
    secret, ngtcp2_strlen_lit(secret), (const ngtcp2_sockaddr *)&in6addr,
    sizeof(in6addr), timeout, t + timeout);

  assert_ptrdiff(NGTCP2_CRYPTO_ERR_VERIFY_TOKEN, ==, token_datalen);

  /* Bad address */
  token_datalen = ngtcp2_crypto_verify_regular_token2(
    decoded_token_data, sizeof(decoded_token_data), token, (size_t)tokenlen,
    secret, ngtcp2_strlen_lit(secret), (const ngtcp2_sockaddr *)&inaddr,
    sizeof(inaddr), timeout, t);

  assert_ptrdiff(NGTCP2_CRYPTO_ERR_UNREADABLE_TOKEN, ==, token_datalen);

  /* Insufficient data buffer */
  memset(decoded_token_data, 0, sizeof(decoded_token_data));

  token_datalen = ngtcp2_crypto_verify_regular_token2(
    decoded_token_data, ngtcp2_strlen_lit(token_data) - 1, token,
    (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr), timeout, t);

  assert_ptrdiff(0, ==, token_datalen);

  /* NULL buffer */
  memset(decoded_token_data, 0, sizeof(decoded_token_data));

  token_datalen = ngtcp2_crypto_verify_regular_token2(
    NULL, 0, token, (size_t)tokenlen, secret, ngtcp2_strlen_lit(secret),
    (const ngtcp2_sockaddr *)&in6addr, sizeof(in6addr), timeout, t);

  assert_ptrdiff(0, ==, token_datalen);

  /* Truncated token */
  token_datalen = ngtcp2_crypto_verify_regular_token2(
    decoded_token_data, sizeof(decoded_token_data), token, (size_t)tokenlen - 1,
    secret, ngtcp2_strlen_lit(secret), (const ngtcp2_sockaddr *)&in6addr,
    sizeof(in6addr), timeout, t);

  assert_ptrdiff(NGTCP2_CRYPTO_ERR_UNREADABLE_TOKEN, ==, token_datalen);

  /* Bad magic */
  token[0] = 0;

  token_datalen = ngtcp2_crypto_verify_regular_token2(
    decoded_token_data, sizeof(decoded_token_data), token, (size_t)tokenlen,
    secret, ngtcp2_strlen_lit(secret), (const ngtcp2_sockaddr *)&in6addr,
    sizeof(in6addr), timeout, t);

  assert_ptrdiff(NGTCP2_CRYPTO_ERR_UNREADABLE_TOKEN, ==, token_datalen);
}

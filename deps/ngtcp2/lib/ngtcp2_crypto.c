/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
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
#include "ngtcp2_crypto.h"

#include <string.h>
#include <assert.h>

#include "ngtcp2_str.h"
#include "ngtcp2_conv.h"
#include "ngtcp2_conn.h"

int ngtcp2_crypto_km_new(ngtcp2_crypto_km **pckm, const uint8_t *secret,
                         size_t secretlen, const uint8_t *key, size_t keylen,
                         const uint8_t *iv, size_t ivlen,
                         const ngtcp2_mem *mem) {
  int rv = ngtcp2_crypto_km_nocopy_new(pckm, secretlen, keylen, ivlen, mem);
  if (rv != 0) {
    return rv;
  }

  if (secretlen) {
    memcpy((*pckm)->secret.base, secret, secretlen);
  }
  memcpy((*pckm)->key.base, key, keylen);
  memcpy((*pckm)->iv.base, iv, ivlen);

  return 0;
}

int ngtcp2_crypto_km_nocopy_new(ngtcp2_crypto_km **pckm, size_t secretlen,
                                size_t keylen, size_t ivlen,
                                const ngtcp2_mem *mem) {
  size_t len;
  uint8_t *p;

  len = sizeof(ngtcp2_crypto_km) + secretlen + keylen + ivlen;

  *pckm = ngtcp2_mem_malloc(mem, len);
  if (*pckm == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  p = (uint8_t *)(*pckm) + sizeof(ngtcp2_crypto_km);
  (*pckm)->secret.base = p;
  (*pckm)->secret.len = secretlen;
  p += secretlen;
  (*pckm)->key.base = p;
  (*pckm)->key.len = keylen;
  p += keylen;
  (*pckm)->iv.base = p;
  (*pckm)->iv.len = ivlen;
  (*pckm)->pkt_num = -1;
  (*pckm)->flags = NGTCP2_CRYPTO_KM_FLAG_NONE;

  return 0;
}

void ngtcp2_crypto_km_del(ngtcp2_crypto_km *ckm, const ngtcp2_mem *mem) {
  if (ckm == NULL) {
    return;
  }

  ngtcp2_mem_free(mem, ckm);
}

void ngtcp2_crypto_create_nonce(uint8_t *dest, const uint8_t *iv, size_t ivlen,
                                int64_t pkt_num) {
  size_t i;
  uint64_t n;

  memcpy(dest, iv, ivlen);
  n = ngtcp2_htonl64((uint64_t)pkt_num);

  for (i = 0; i < 8; ++i) {
    dest[ivlen - 8 + i] ^= ((uint8_t *)&n)[i];
  }
}

/*
 * varint_paramlen returns the length of a single transport parameter
 * which has variable integer in its parameter.
 */
static size_t varint_paramlen(ngtcp2_transport_param_id id, uint64_t param) {
  size_t valuelen = ngtcp2_put_varint_len(param);
  return ngtcp2_put_varint_len(id) + ngtcp2_put_varint_len(valuelen) + valuelen;
}

/*
 * write_varint_param writes parameter |id| of the given |value| in
 * varint encoding.  It returns p + the number of bytes written.
 */
static uint8_t *write_varint_param(uint8_t *p, ngtcp2_transport_param_id id,
                                   uint64_t value) {
  p = ngtcp2_put_varint(p, id);
  p = ngtcp2_put_varint(p, ngtcp2_put_varint_len(value));
  return ngtcp2_put_varint(p, value);
}

ngtcp2_ssize
ngtcp2_encode_transport_params(uint8_t *dest, size_t destlen,
                               ngtcp2_transport_params_type exttype,
                               const ngtcp2_transport_params *params) {
  uint8_t *p;
  size_t len = 0;
  /* For some reason, gcc 7.3.0 requires this initialization. */
  size_t preferred_addrlen = 0;

  switch (exttype) {
  case NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO:
    break;
  case NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS:
    if (params->stateless_reset_token_present) {
      len +=
          ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN) +
          ngtcp2_put_varint_len(NGTCP2_STATELESS_RESET_TOKENLEN) +
          NGTCP2_STATELESS_RESET_TOKENLEN;
    }
    if (params->preferred_address_present) {
      assert(params->preferred_address.cid.datalen == 0 ||
             params->preferred_address.cid.datalen >= NGTCP2_MIN_CIDLEN);
      assert(params->preferred_address.cid.datalen <= NGTCP2_MAX_CIDLEN);
      preferred_addrlen = 4 /* ipv4Address */ + 2 /* ipv4Port */ +
                          16 /* ipv6Address */ + 2 /* ipv6Port */
                          + 1 +
                          params->preferred_address.cid.datalen /* CID */ +
                          NGTCP2_STATELESS_RESET_TOKENLEN;
      len += ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS) +
             ngtcp2_put_varint_len(preferred_addrlen) + preferred_addrlen;
    }
    if (params->original_connection_id_present) {
      len +=
          ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_ORIGINAL_CONNECTION_ID) +
          ngtcp2_put_varint_len(params->original_connection_id.datalen) +
          params->original_connection_id.datalen;
    }
    break;
  default:
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (params->initial_max_stream_data_bidi_local) {
    len += varint_paramlen(
        NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
        params->initial_max_stream_data_bidi_local);
  }
  if (params->initial_max_stream_data_bidi_remote) {
    len += varint_paramlen(
        NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
        params->initial_max_stream_data_bidi_remote);
  }
  if (params->initial_max_stream_data_uni) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_UNI,
                           params->initial_max_stream_data_uni);
  }
  if (params->initial_max_data) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_DATA,
                           params->initial_max_data);
  }
  if (params->initial_max_streams_bidi) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_BIDI,
                           params->initial_max_streams_bidi);
  }
  if (params->initial_max_streams_uni) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_UNI,
                           params->initial_max_streams_uni);
  }
  if (params->max_packet_size != NGTCP2_MAX_PKT_SIZE) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_MAX_PACKET_SIZE,
                           params->max_packet_size);
  }
  if (params->ack_delay_exponent != NGTCP2_DEFAULT_ACK_DELAY_EXPONENT) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_ACK_DELAY_EXPONENT,
                           params->ack_delay_exponent);
  }
  if (params->disable_active_migration) {
    len +=
        ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_DISABLE_ACTIVE_MIGRATION) +
        ngtcp2_put_varint_len(0);
  }
  if (params->max_ack_delay != NGTCP2_DEFAULT_MAX_ACK_DELAY) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_MAX_ACK_DELAY,
                           params->max_ack_delay / NGTCP2_MILLISECONDS);
  }
  if (params->max_idle_timeout) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_MAX_IDLE_TIMEOUT,
                           params->max_idle_timeout / NGTCP2_MILLISECONDS);
  }
  if (params->active_connection_id_limit &&
      params->active_connection_id_limit !=
          NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_ACTIVE_CONNECTION_ID_LIMIT,
                           params->active_connection_id_limit);
  }

  if (destlen < len) {
    return NGTCP2_ERR_NOBUF;
  }

  p = dest;

  if (exttype == NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
    if (params->stateless_reset_token_present) {
      p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN);
      p = ngtcp2_put_varint(p, sizeof(params->stateless_reset_token));
      p = ngtcp2_cpymem(p, params->stateless_reset_token,
                        sizeof(params->stateless_reset_token));
    }
    if (params->preferred_address_present) {
      p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS);
      p = ngtcp2_put_varint(p, preferred_addrlen);

      p = ngtcp2_cpymem(p, params->preferred_address.ipv4_addr,
                        sizeof(params->preferred_address.ipv4_addr));
      p = ngtcp2_put_uint16be(p, params->preferred_address.ipv4_port);

      p = ngtcp2_cpymem(p, params->preferred_address.ipv6_addr,
                        sizeof(params->preferred_address.ipv6_addr));
      p = ngtcp2_put_uint16be(p, params->preferred_address.ipv6_port);

      *p++ = (uint8_t)params->preferred_address.cid.datalen;
      if (params->preferred_address.cid.datalen) {
        p = ngtcp2_cpymem(p, params->preferred_address.cid.data,
                          params->preferred_address.cid.datalen);
      }
      p = ngtcp2_cpymem(
          p, params->preferred_address.stateless_reset_token,
          sizeof(params->preferred_address.stateless_reset_token));
    }
    if (params->original_connection_id_present) {
      p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_ORIGINAL_CONNECTION_ID);
      p = ngtcp2_put_varint(p, params->original_connection_id.datalen);
      p = ngtcp2_cpymem(p, params->original_connection_id.data,
                        params->original_connection_id.datalen);
    }
  }

  if (params->initial_max_stream_data_bidi_local) {
    p = write_varint_param(
        p, NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
        params->initial_max_stream_data_bidi_local);
  }

  if (params->initial_max_stream_data_bidi_remote) {
    p = write_varint_param(
        p, NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
        params->initial_max_stream_data_bidi_remote);
  }

  if (params->initial_max_stream_data_uni) {
    p = write_varint_param(p,
                           NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_UNI,
                           params->initial_max_stream_data_uni);
  }

  if (params->initial_max_data) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_DATA,
                           params->initial_max_data);
  }

  if (params->initial_max_streams_bidi) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_BIDI,
                           params->initial_max_streams_bidi);
  }

  if (params->initial_max_streams_uni) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_UNI,
                           params->initial_max_streams_uni);
  }

  if (params->max_packet_size != NGTCP2_MAX_PKT_SIZE) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_MAX_PACKET_SIZE,
                           params->max_packet_size);
  }

  if (params->ack_delay_exponent != NGTCP2_DEFAULT_ACK_DELAY_EXPONENT) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_ACK_DELAY_EXPONENT,
                           params->ack_delay_exponent);
  }

  if (params->disable_active_migration) {
    p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_DISABLE_ACTIVE_MIGRATION);
    p = ngtcp2_put_varint(p, 0);
  }

  if (params->max_ack_delay != NGTCP2_DEFAULT_MAX_ACK_DELAY) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_MAX_ACK_DELAY,
                           params->max_ack_delay / NGTCP2_MILLISECONDS);
  }

  if (params->max_idle_timeout) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_MAX_IDLE_TIMEOUT,
                           params->max_idle_timeout / NGTCP2_MILLISECONDS);
  }

  if (params->active_connection_id_limit &&
      params->active_connection_id_limit !=
          NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_ACTIVE_CONNECTION_ID_LIMIT,
                           params->active_connection_id_limit);
  }

  assert((size_t)(p - dest) == len);

  return (ngtcp2_ssize)len;
}

/*
 * decode_varint decodes a single varint from the buffer pointed by
 * |p| of length |end - p|.  If it decodes an integer successfully, it
 * stores the integer in |*pdest| and returns 0.  Otherwise it returns
 * -1.
 */
static ngtcp2_ssize decode_varint(uint64_t *pdest, const uint8_t *p,
                                  const uint8_t *end) {
  size_t len;

  if (p == end) {
    return -1;
  }

  len = ngtcp2_get_varint_len(p);
  if ((uint64_t)(end - p) < len) {
    return -1;
  }

  *pdest = ngtcp2_get_varint(&len, p);

  return (ngtcp2_ssize)len;
}

/*
 * decode_varint_param decodes length prefixed value from the buffer
 * pointed by |p| of length |end - p|.  The length and value are
 * encoded in varint form.  If it decodes a value successfully, it
 * stores the value in |*pdest| and returns 0.  Otherwise it returns
 * -1.
 */
static ngtcp2_ssize decode_varint_param(uint64_t *pdest, const uint8_t *p,
                                        const uint8_t *end) {
  const uint8_t *begin = p;
  ngtcp2_ssize nread;
  uint64_t valuelen;
  size_t n;

  nread = decode_varint(&valuelen, p, end);
  if (nread < 0) {
    return -1;
  }

  p += nread;

  if ((uint64_t)(end - p) < valuelen) {
    return -1;
  }

  if (ngtcp2_get_varint_len(p) != valuelen) {
    return -1;
  }

  *pdest = ngtcp2_get_varint(&n, p);

  p += valuelen;

  return (ngtcp2_ssize)(p - begin);
}

int ngtcp2_decode_transport_params(ngtcp2_transport_params *params,
                                   ngtcp2_transport_params_type exttype,
                                   const uint8_t *data, size_t datalen) {
  const uint8_t *p, *end;
  size_t len;
  uint64_t param_type;
  uint64_t valuelen;
  ngtcp2_ssize nread;
  uint8_t scb[8192];
  size_t scb_idx;
  size_t scb_shift;

  p = data;
  end = data + datalen;

  /* Set default values */
  params->initial_max_streams_bidi = 0;
  params->initial_max_streams_uni = 0;
  params->initial_max_stream_data_bidi_local = 0;
  params->initial_max_stream_data_bidi_remote = 0;
  params->initial_max_stream_data_uni = 0;
  params->max_packet_size = NGTCP2_MAX_PKT_SIZE;
  params->ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;
  params->stateless_reset_token_present = 0;
  params->preferred_address_present = 0;
  memset(&params->preferred_address, 0, sizeof(params->preferred_address));
  params->disable_active_migration = 0;
  params->max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;
  params->max_idle_timeout = 0;
  params->active_connection_id_limit =
      NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
  params->original_connection_id_present = 0;

  if (datalen == 0) {
    return 0;
  }

  memset(scb, 0, sizeof(scb));

  for (; (size_t)(end - p) >= 2;) {
    nread = decode_varint(&param_type, p, end);
    if (nread < 0) {
      return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
    }
    p += nread;

    scb_idx = param_type / 8;
    scb_shift = param_type % 8;

    if (scb[scb_idx] & (1 << scb_shift)) {
      return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
    }
    scb[scb_idx] |= (uint8_t)(1 << scb_shift);
    switch (param_type) {
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_LOCAL:
      nread = decode_varint_param(&params->initial_max_stream_data_bidi_local,
                                  p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_REMOTE:
      nread = decode_varint_param(&params->initial_max_stream_data_bidi_remote,
                                  p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_UNI:
      nread = decode_varint_param(&params->initial_max_stream_data_uni, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_DATA:
      nread = decode_varint_param(&params->initial_max_data, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_BIDI:
      nread = decode_varint_param(&params->initial_max_streams_bidi, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (params->initial_max_streams_bidi > NGTCP2_MAX_STREAMS) {
        return NGTCP2_ERR_STREAM_LIMIT;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_UNI:
      nread = decode_varint_param(&params->initial_max_streams_uni, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (params->initial_max_streams_uni > NGTCP2_MAX_STREAMS) {
        return NGTCP2_ERR_STREAM_LIMIT;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_MAX_IDLE_TIMEOUT:
      nread = decode_varint_param(&params->max_idle_timeout, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      params->max_idle_timeout *= NGTCP2_MILLISECONDS;
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_MAX_PACKET_SIZE:
      nread = decode_varint_param(&params->max_packet_size, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      if ((size_t)valuelen != sizeof(params->stateless_reset_token)) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if ((size_t)(end - p) < sizeof(params->stateless_reset_token)) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }

      memcpy(params->stateless_reset_token, p,
             sizeof(params->stateless_reset_token));
      params->stateless_reset_token_present = 1;

      p += sizeof(params->stateless_reset_token);
      break;
    case NGTCP2_TRANSPORT_PARAM_ACK_DELAY_EXPONENT:
      nread = decode_varint_param(&params->ack_delay_exponent, p, end);
      if (nread < 0 || params->ack_delay_exponent > 20) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      if ((size_t)(end - p) < valuelen) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      len = 4 /* ipv4Address */ + 2 /* ipv4Port */ + 16 /* ipv6Address */ +
            2 /* ipv6Port */
            + 1 /* cid length */ + NGTCP2_STATELESS_RESET_TOKENLEN;
      if (valuelen < len) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }

      memcpy(params->preferred_address.ipv4_addr, p,
             sizeof(params->preferred_address.ipv4_addr));
      p += sizeof(params->preferred_address.ipv4_addr);
      params->preferred_address.ipv4_port = ngtcp2_get_uint16(p);
      p += sizeof(uint16_t);

      memcpy(params->preferred_address.ipv6_addr, p,
             sizeof(params->preferred_address.ipv6_addr));
      p += sizeof(params->preferred_address.ipv6_addr);
      params->preferred_address.ipv6_port = ngtcp2_get_uint16(p);
      p += sizeof(uint16_t);

      /* cid */
      params->preferred_address.cid.datalen = *p++;
      len += params->preferred_address.cid.datalen;
      if (valuelen != len ||
          params->preferred_address.cid.datalen > NGTCP2_MAX_CIDLEN ||
          (params->preferred_address.cid.datalen != 0 &&
           params->preferred_address.cid.datalen < NGTCP2_MIN_CIDLEN)) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (params->preferred_address.cid.datalen) {
        memcpy(params->preferred_address.cid.data, p,
               params->preferred_address.cid.datalen);
        p += params->preferred_address.cid.datalen;
      }

      /* stateless reset token */
      memcpy(params->preferred_address.stateless_reset_token, p,
             sizeof(params->preferred_address.stateless_reset_token));
      p += sizeof(params->preferred_address.stateless_reset_token);
      params->preferred_address_present = 1;
      break;
    case NGTCP2_TRANSPORT_PARAM_DISABLE_ACTIVE_MIGRATION:
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0 || valuelen != 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      params->disable_active_migration = 1;
      break;
    case NGTCP2_TRANSPORT_PARAM_ORIGINAL_CONNECTION_ID:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      if (valuelen < NGTCP2_MIN_CIDLEN || valuelen > NGTCP2_MAX_CIDLEN ||
          (size_t)(end - p) < valuelen) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      ngtcp2_cid_init(&params->original_connection_id, p, valuelen);
      params->original_connection_id_present = 1;
      p += valuelen;
      break;
    case NGTCP2_TRANSPORT_PARAM_MAX_ACK_DELAY:
      nread = decode_varint_param(&params->max_ack_delay, p, end);
      if (nread < 0 || params->max_ack_delay >= 16384) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      params->max_ack_delay *= NGTCP2_MILLISECONDS;
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_ACTIVE_CONNECTION_ID_LIMIT:
      nread = decode_varint_param(&params->active_connection_id_limit, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    default:
      /* Ignore unknown parameter */
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      if ((size_t)(end - p) < valuelen) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += valuelen;
      break;
    }
  }

  if (end - p != 0) {
    return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
  }

  return 0;
}

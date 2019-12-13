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

int ngtcp2_crypto_km_new(ngtcp2_crypto_km **pckm, const uint8_t *key,
                         size_t keylen, const uint8_t *iv, size_t ivlen,
                         const ngtcp2_mem *mem) {
  int rv = ngtcp2_crypto_km_nocopy_new(pckm, keylen, ivlen, mem);
  if (rv != 0) {
    return rv;
  }

  memcpy((*pckm)->key.base, key, keylen);
  memcpy((*pckm)->iv.base, iv, ivlen);

  return 0;
}

int ngtcp2_crypto_km_nocopy_new(ngtcp2_crypto_km **pckm, size_t keylen,
                                size_t ivlen, const ngtcp2_mem *mem) {
  size_t len;
  uint8_t *p;

  len = sizeof(ngtcp2_crypto_km) + keylen + ivlen;

  *pckm = ngtcp2_mem_malloc(mem, len);
  if (*pckm == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  p = (uint8_t *)(*pckm) + sizeof(ngtcp2_crypto_km);
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
static size_t varint_paramlen(uint64_t param) {
  return 4 + ngtcp2_put_varint_len(param);
}

/*
 * write_varint_param writes parameter |id| of the given |value| in
 * varint encoding.  It returns p + the number of bytes written.
 */
static uint8_t *write_varint_param(uint8_t *p, ngtcp2_transport_param_id id,
                                   uint64_t value) {
  p = ngtcp2_put_uint16be(p, (uint16_t)id);
  p = ngtcp2_put_uint16be(p, (uint16_t)ngtcp2_put_varint_len(value));
  return ngtcp2_put_varint(p, value);
}

ngtcp2_ssize
ngtcp2_encode_transport_params(uint8_t *dest, size_t destlen,
                               ngtcp2_transport_params_type exttype,
                               const ngtcp2_transport_params *params) {
  uint8_t *p;
  size_t len = sizeof(uint16_t);
  /* For some reason, gcc 7.3.0 requires this initialization. */
  size_t preferred_addrlen = 0;

  switch (exttype) {
  case NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO:
    break;
  case NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS:
    if (params->stateless_reset_token_present) {
      len += 20;
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
      len += 4 + preferred_addrlen;
    }
    if (params->original_connection_id_present) {
      len += 4 + params->original_connection_id.datalen;
    }
    break;
  default:
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (params->initial_max_stream_data_bidi_local) {
    len += varint_paramlen(params->initial_max_stream_data_bidi_local);
  }
  if (params->initial_max_stream_data_bidi_remote) {
    len += varint_paramlen(params->initial_max_stream_data_bidi_remote);
  }
  if (params->initial_max_stream_data_uni) {
    len += varint_paramlen(params->initial_max_stream_data_uni);
  }
  if (params->initial_max_data) {
    len += varint_paramlen(params->initial_max_data);
  }
  if (params->initial_max_streams_bidi) {
    len += varint_paramlen(params->initial_max_streams_bidi);
  }
  if (params->initial_max_streams_uni) {
    len += varint_paramlen(params->initial_max_streams_uni);
  }
  if (params->max_packet_size != NGTCP2_MAX_PKT_SIZE) {
    len += varint_paramlen(params->max_packet_size);
  }
  if (params->ack_delay_exponent != NGTCP2_DEFAULT_ACK_DELAY_EXPONENT) {
    len += varint_paramlen(params->ack_delay_exponent);
  }
  if (params->disable_active_migration) {
    len += 4;
  }
  if (params->max_ack_delay != NGTCP2_DEFAULT_MAX_ACK_DELAY) {
    len += varint_paramlen(params->max_ack_delay / NGTCP2_MILLISECONDS);
  }
  if (params->idle_timeout) {
    len += varint_paramlen(params->idle_timeout / NGTCP2_MILLISECONDS);
  }
  if (params->active_connection_id_limit) {
    len += varint_paramlen(params->active_connection_id_limit);
  }

  if (destlen < len) {
    return NGTCP2_ERR_NOBUF;
  }

  p = dest;
  p = ngtcp2_put_uint16be(p, (uint16_t)(len - sizeof(uint16_t)));

  if (exttype == NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
    if (params->stateless_reset_token_present) {
      p = ngtcp2_put_uint16be(p, NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN);
      p = ngtcp2_put_uint16be(p, sizeof(params->stateless_reset_token));
      p = ngtcp2_cpymem(p, params->stateless_reset_token,
                        sizeof(params->stateless_reset_token));
    }
    if (params->preferred_address_present) {
      p = ngtcp2_put_uint16be(p, NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS);
      p = ngtcp2_put_uint16be(p, (uint16_t)preferred_addrlen);

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
      p = ngtcp2_put_uint16be(p, NGTCP2_TRANSPORT_PARAM_ORIGINAL_CONNECTION_ID);
      p = ngtcp2_put_uint16be(p,
                              (uint16_t)params->original_connection_id.datalen);
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
    p = ngtcp2_put_uint16be(p, NGTCP2_TRANSPORT_PARAM_DISABLE_ACTIVE_MIGRATION);
    p = ngtcp2_put_uint16be(p, 0);
  }

  if (params->max_ack_delay != NGTCP2_DEFAULT_MAX_ACK_DELAY) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_MAX_ACK_DELAY,
                           params->max_ack_delay / NGTCP2_MILLISECONDS);
  }

  if (params->idle_timeout) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_IDLE_TIMEOUT,
                           params->idle_timeout / NGTCP2_MILLISECONDS);
  }

  if (params->active_connection_id_limit) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_ACTIVE_CONNECTION_ID_LIMIT,
                           params->active_connection_id_limit);
  }

  assert((size_t)(p - dest) == len);

  return (ngtcp2_ssize)len;
}

static ngtcp2_ssize decode_varint(uint64_t *pdest, const uint8_t *p,
                                  const uint8_t *end) {
  uint16_t len = ngtcp2_get_uint16(p);
  size_t n;

  p += sizeof(uint16_t);

  switch (len) {
  case 1:
  case 2:
  case 4:
  case 8:
    break;
  default:
    return -1;
  }

  if ((size_t)(end - p) < len) {
    return -1;
  }

  n = ngtcp2_get_varint_len(p);
  if (n != len) {
    return -1;
  }

  *pdest = ngtcp2_get_varint(&n, p);

  return (ngtcp2_ssize)(sizeof(uint16_t) + len);
}

int ngtcp2_decode_transport_params(ngtcp2_transport_params *params,
                                   ngtcp2_transport_params_type exttype,
                                   const uint8_t *data, size_t datalen) {
  const uint8_t *p, *end;
  size_t len;
  size_t tplen;
  uint16_t param_type;
  size_t valuelen;
  ngtcp2_ssize nread;
  uint8_t scb[8192];
  size_t scb_idx;
  size_t scb_shift;

  if (datalen < sizeof(uint16_t)) {
    return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
  }

  p = data;
  end = data + datalen;

  tplen = ngtcp2_get_uint16(p);
  p += sizeof(uint16_t);

  if (sizeof(uint16_t) + tplen != datalen) {
    return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
  }

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
  params->idle_timeout = 0;
  params->active_connection_id_limit = 0;
  params->original_connection_id_present = 0;

  memset(scb, 0, sizeof(scb));

  for (; (size_t)(end - p) >= sizeof(uint16_t) * 2;) {
    param_type = ngtcp2_get_uint16(p);
    p += sizeof(uint16_t);

    scb_idx = param_type / 8;
    scb_shift = param_type % 8;

    if (scb[scb_idx] & (1 << scb_shift)) {
      return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
    }
    scb[scb_idx] |= (uint8_t)(1 << scb_shift);
    switch (param_type) {
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_LOCAL:
      nread =
          decode_varint(&params->initial_max_stream_data_bidi_local, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_REMOTE:
      nread =
          decode_varint(&params->initial_max_stream_data_bidi_remote, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_UNI:
      nread = decode_varint(&params->initial_max_stream_data_uni, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_DATA:
      nread = decode_varint(&params->initial_max_data, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_BIDI:
      nread = decode_varint(&params->initial_max_streams_bidi, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (params->initial_max_streams_bidi > NGTCP2_MAX_STREAMS) {
        return NGTCP2_ERR_STREAM_LIMIT;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_UNI:
      nread = decode_varint(&params->initial_max_streams_uni, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (params->initial_max_streams_uni > NGTCP2_MAX_STREAMS) {
        return NGTCP2_ERR_STREAM_LIMIT;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_IDLE_TIMEOUT:
      nread = decode_varint(&params->idle_timeout, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      params->idle_timeout *= NGTCP2_MILLISECONDS;
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_MAX_PACKET_SIZE:
      nread = decode_varint(&params->max_packet_size, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (ngtcp2_get_uint16(p) != sizeof(params->stateless_reset_token)) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += sizeof(uint16_t);
      if ((size_t)(end - p) < sizeof(params->stateless_reset_token)) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }

      memcpy(params->stateless_reset_token, p,
             sizeof(params->stateless_reset_token));
      params->stateless_reset_token_present = 1;

      p += sizeof(params->stateless_reset_token);
      break;
    case NGTCP2_TRANSPORT_PARAM_ACK_DELAY_EXPONENT:
      nread = decode_varint(&params->ack_delay_exponent, p, end);
      if (nread < 0 || params->ack_delay_exponent > 20) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      valuelen = ngtcp2_get_uint16(p);
      p += sizeof(uint16_t);
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
      if (ngtcp2_get_uint16(p) != 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += sizeof(uint16_t);
      params->disable_active_migration = 1;
      break;
    case NGTCP2_TRANSPORT_PARAM_ORIGINAL_CONNECTION_ID:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      len = ngtcp2_get_uint16(p);
      p += sizeof(uint16_t);
      if (len < NGTCP2_MIN_CIDLEN || len > NGTCP2_MAX_CIDLEN ||
          (size_t)(end - p) < len) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      ngtcp2_cid_init(&params->original_connection_id, p, len);
      params->original_connection_id_present = 1;
      p += len;
      break;
    case NGTCP2_TRANSPORT_PARAM_MAX_ACK_DELAY:
      nread = decode_varint(&params->max_ack_delay, p, end);
      if (nread < 0 || params->max_ack_delay >= 16384) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      params->max_ack_delay *= NGTCP2_MILLISECONDS;
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_ACTIVE_CONNECTION_ID_LIMIT:
      nread = decode_varint(&params->active_connection_id_limit, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    default:
      /* Ignore unknown parameter */
      valuelen = ngtcp2_get_uint16(p);
      p += sizeof(uint16_t);
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

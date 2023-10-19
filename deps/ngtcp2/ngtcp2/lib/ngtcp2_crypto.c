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
#include "ngtcp2_net.h"

int ngtcp2_crypto_km_new(ngtcp2_crypto_km **pckm, const uint8_t *secret,
                         size_t secretlen,
                         const ngtcp2_crypto_aead_ctx *aead_ctx,
                         const uint8_t *iv, size_t ivlen,
                         const ngtcp2_mem *mem) {
  int rv = ngtcp2_crypto_km_nocopy_new(pckm, secretlen, ivlen, mem);
  if (rv != 0) {
    return rv;
  }

  if (secretlen) {
    memcpy((*pckm)->secret.base, secret, secretlen);
  }
  if (aead_ctx) {
    (*pckm)->aead_ctx = *aead_ctx;
  }
  memcpy((*pckm)->iv.base, iv, ivlen);

  return 0;
}

int ngtcp2_crypto_km_nocopy_new(ngtcp2_crypto_km **pckm, size_t secretlen,
                                size_t ivlen, const ngtcp2_mem *mem) {
  size_t len;
  uint8_t *p;

  len = sizeof(ngtcp2_crypto_km) + secretlen + ivlen;

  *pckm = ngtcp2_mem_malloc(mem, len);
  if (*pckm == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  p = (uint8_t *)(*pckm) + sizeof(ngtcp2_crypto_km);
  (*pckm)->secret.base = p;
  (*pckm)->secret.len = secretlen;
  p += secretlen;
  (*pckm)->iv.base = p;
  (*pckm)->iv.len = ivlen;
  (*pckm)->aead_ctx.native_handle = NULL;
  (*pckm)->pkt_num = -1;
  (*pckm)->use_count = 0;
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

  assert(ivlen >= 8);

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

/*
 * cid_paramlen returns the length of a single transport parameter
 * which has |cid| as value.
 */
static size_t cid_paramlen(ngtcp2_transport_param_id id,
                           const ngtcp2_cid *cid) {
  return ngtcp2_put_varint_len(id) + ngtcp2_put_varint_len(cid->datalen) +
         cid->datalen;
}

/*
 * write_cid_param writes parameter |id| of the given |cid|.  It
 * returns p + the number of bytes written.
 */
static uint8_t *write_cid_param(uint8_t *p, ngtcp2_transport_param_id id,
                                const ngtcp2_cid *cid) {
  assert(cid->datalen == 0 || cid->datalen >= NGTCP2_MIN_CIDLEN);
  assert(cid->datalen <= NGTCP2_MAX_CIDLEN);

  p = ngtcp2_put_varint(p, id);
  p = ngtcp2_put_varint(p, cid->datalen);
  if (cid->datalen) {
    p = ngtcp2_cpymem(p, cid->data, cid->datalen);
  }
  return p;
}

static const uint8_t empty_address[16];

ngtcp2_ssize ngtcp2_encode_transport_params_versioned(
    uint8_t *dest, size_t destlen, ngtcp2_transport_params_type exttype,
    int transport_params_version, const ngtcp2_transport_params *params) {
  uint8_t *p;
  size_t len = 0;
  /* For some reason, gcc 7.3.0 requires this initialization. */
  size_t preferred_addrlen = 0;
  size_t version_infolen = 0;
  (void)transport_params_version;

  switch (exttype) {
  case NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO:
    break;
  case NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS:
    len +=
        cid_paramlen(NGTCP2_TRANSPORT_PARAM_ORIGINAL_DESTINATION_CONNECTION_ID,
                     &params->original_dcid);

    if (params->stateless_reset_token_present) {
      len +=
          ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN) +
          ngtcp2_put_varint_len(NGTCP2_STATELESS_RESET_TOKENLEN) +
          NGTCP2_STATELESS_RESET_TOKENLEN;
    }
    if (params->preferred_address_present) {
      assert(params->preferred_address.cid.datalen >= NGTCP2_MIN_CIDLEN);
      assert(params->preferred_address.cid.datalen <= NGTCP2_MAX_CIDLEN);
      preferred_addrlen = 4 /* ipv4Address */ + 2 /* ipv4Port */ +
                          16 /* ipv6Address */ + 2 /* ipv6Port */
                          + 1 +
                          params->preferred_address.cid.datalen /* CID */ +
                          NGTCP2_STATELESS_RESET_TOKENLEN;
      len += ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS) +
             ngtcp2_put_varint_len(preferred_addrlen) + preferred_addrlen;
    }
    if (params->retry_scid_present) {
      len += cid_paramlen(NGTCP2_TRANSPORT_PARAM_RETRY_SOURCE_CONNECTION_ID,
                          &params->retry_scid);
    }
    break;
  default:
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  len += cid_paramlen(NGTCP2_TRANSPORT_PARAM_INITIAL_SOURCE_CONNECTION_ID,
                      &params->initial_scid);

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
  if (params->max_udp_payload_size !=
      NGTCP2_DEFAULT_MAX_RECV_UDP_PAYLOAD_SIZE) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_MAX_UDP_PAYLOAD_SIZE,
                           params->max_udp_payload_size);
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
  if (params->max_datagram_frame_size) {
    len += varint_paramlen(NGTCP2_TRANSPORT_PARAM_MAX_DATAGRAM_FRAME_SIZE,
                           params->max_datagram_frame_size);
  }
  if (params->grease_quic_bit) {
    len += ngtcp2_put_varint_len(NGTCP2_TRANSPORT_PARAM_GREASE_QUIC_BIT) +
           ngtcp2_put_varint_len(0);
  }
  if (params->version_info_present) {
    version_infolen = sizeof(uint32_t) + params->version_info.other_versionslen;
    len += ngtcp2_put_varint_len(
               NGTCP2_TRANSPORT_PARAM_VERSION_INFORMATION_DRAFT) +
           ngtcp2_put_varint_len(version_infolen) + version_infolen;
  }

  if (dest == NULL && destlen == 0) {
    return (ngtcp2_ssize)len;
  }

  if (destlen < len) {
    return NGTCP2_ERR_NOBUF;
  }

  p = dest;

  if (exttype == NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
    p = write_cid_param(
        p, NGTCP2_TRANSPORT_PARAM_ORIGINAL_DESTINATION_CONNECTION_ID,
        &params->original_dcid);

    if (params->stateless_reset_token_present) {
      p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN);
      p = ngtcp2_put_varint(p, sizeof(params->stateless_reset_token));
      p = ngtcp2_cpymem(p, params->stateless_reset_token,
                        sizeof(params->stateless_reset_token));
    }
    if (params->preferred_address_present) {
      p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS);
      p = ngtcp2_put_varint(p, preferred_addrlen);

      if (params->preferred_address.ipv4_present) {
        p = ngtcp2_cpymem(p, params->preferred_address.ipv4_addr,
                          sizeof(params->preferred_address.ipv4_addr));
        p = ngtcp2_put_uint16be(p, params->preferred_address.ipv4_port);
      } else {
        p = ngtcp2_cpymem(p, empty_address,
                          sizeof(params->preferred_address.ipv4_addr));
        p = ngtcp2_put_uint16be(p, 0);
      }

      if (params->preferred_address.ipv6_present) {
        p = ngtcp2_cpymem(p, params->preferred_address.ipv6_addr,
                          sizeof(params->preferred_address.ipv6_addr));
        p = ngtcp2_put_uint16be(p, params->preferred_address.ipv6_port);
      } else {
        p = ngtcp2_cpymem(p, empty_address,
                          sizeof(params->preferred_address.ipv6_addr));
        p = ngtcp2_put_uint16be(p, 0);
      }

      *p++ = (uint8_t)params->preferred_address.cid.datalen;
      if (params->preferred_address.cid.datalen) {
        p = ngtcp2_cpymem(p, params->preferred_address.cid.data,
                          params->preferred_address.cid.datalen);
      }
      p = ngtcp2_cpymem(
          p, params->preferred_address.stateless_reset_token,
          sizeof(params->preferred_address.stateless_reset_token));
    }
    if (params->retry_scid_present) {
      p = write_cid_param(p, NGTCP2_TRANSPORT_PARAM_RETRY_SOURCE_CONNECTION_ID,
                          &params->retry_scid);
    }
  }

  p = write_cid_param(p, NGTCP2_TRANSPORT_PARAM_INITIAL_SOURCE_CONNECTION_ID,
                      &params->initial_scid);

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

  if (params->max_udp_payload_size !=
      NGTCP2_DEFAULT_MAX_RECV_UDP_PAYLOAD_SIZE) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_MAX_UDP_PAYLOAD_SIZE,
                           params->max_udp_payload_size);
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

  if (params->max_datagram_frame_size) {
    p = write_varint_param(p, NGTCP2_TRANSPORT_PARAM_MAX_DATAGRAM_FRAME_SIZE,
                           params->max_datagram_frame_size);
  }

  if (params->grease_quic_bit) {
    p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_GREASE_QUIC_BIT);
    p = ngtcp2_put_varint(p, 0);
  }

  if (params->version_info_present) {
    p = ngtcp2_put_varint(p, NGTCP2_TRANSPORT_PARAM_VERSION_INFORMATION_DRAFT);
    p = ngtcp2_put_varint(p, version_infolen);
    p = ngtcp2_put_uint32be(p, params->version_info.chosen_version);
    if (params->version_info.other_versionslen) {
      p = ngtcp2_cpymem(p, params->version_info.other_versions,
                        params->version_info.other_versionslen);
    }
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

  if (p == end) {
    return -1;
  }

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

/*
 * decode_cid_param decodes length prefixed ngtcp2_cid from the buffer
 * pointed by |p| of length |end - p|.  The length is encoded in
 * varint form.  If it decodes a value successfully, it stores the
 * value in |*pdest| and returns the number of bytes read.  Otherwise
 * it returns -1.
 */
static ngtcp2_ssize decode_cid_param(ngtcp2_cid *pdest, const uint8_t *p,
                                     const uint8_t *end) {
  const uint8_t *begin = p;
  uint64_t valuelen;
  ngtcp2_ssize nread = decode_varint(&valuelen, p, end);

  if (nread < 0) {
    return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
  }

  p += nread;

  if ((valuelen != 0 && valuelen < NGTCP2_MIN_CIDLEN) ||
      valuelen > NGTCP2_MAX_CIDLEN || (size_t)(end - p) < valuelen) {
    return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
  }

  ngtcp2_cid_init(pdest, p, (size_t)valuelen);

  p += valuelen;

  return (ngtcp2_ssize)(p - begin);
}

int ngtcp2_decode_transport_params_versioned(
    int transport_params_version, ngtcp2_transport_params *params,
    ngtcp2_transport_params_type exttype, const uint8_t *data, size_t datalen) {
  const uint8_t *p, *end;
  size_t len;
  uint64_t param_type;
  uint64_t valuelen;
  ngtcp2_ssize nread;
  int initial_scid_present = 0;
  int original_dcid_present = 0;
  size_t i;
  (void)transport_params_version;

  if (datalen == 0) {
    return NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM;
  }

  /* Set default values */
  memset(params, 0, sizeof(*params));
  params->initial_max_streams_bidi = 0;
  params->initial_max_streams_uni = 0;
  params->initial_max_stream_data_bidi_local = 0;
  params->initial_max_stream_data_bidi_remote = 0;
  params->initial_max_stream_data_uni = 0;
  params->max_udp_payload_size = NGTCP2_DEFAULT_MAX_RECV_UDP_PAYLOAD_SIZE;
  params->ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;
  params->stateless_reset_token_present = 0;
  params->preferred_address_present = 0;
  params->disable_active_migration = 0;
  params->max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;
  params->max_idle_timeout = 0;
  params->active_connection_id_limit =
      NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
  params->retry_scid_present = 0;
  params->max_datagram_frame_size = 0;
  memset(&params->retry_scid, 0, sizeof(params->retry_scid));
  memset(&params->initial_scid, 0, sizeof(params->initial_scid));
  memset(&params->original_dcid, 0, sizeof(params->original_dcid));
  params->version_info_present = 0;

  p = data;
  end = data + datalen;

  for (; (size_t)(end - p) >= 2;) {
    nread = decode_varint(&param_type, p, end);
    if (nread < 0) {
      return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
    }
    p += nread;

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
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_UNI:
      nread = decode_varint_param(&params->initial_max_streams_uni, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (params->initial_max_streams_uni > NGTCP2_MAX_STREAMS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
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
    case NGTCP2_TRANSPORT_PARAM_MAX_UDP_PAYLOAD_SIZE:
      nread = decode_varint_param(&params->max_udp_payload_size, p, end);
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

      if (params->preferred_address.ipv4_port ||
          memcmp(empty_address, params->preferred_address.ipv4_addr,
                 sizeof(params->preferred_address.ipv4_addr)) != 0) {
        params->preferred_address.ipv4_present = 1;
      }

      memcpy(params->preferred_address.ipv6_addr, p,
             sizeof(params->preferred_address.ipv6_addr));
      p += sizeof(params->preferred_address.ipv6_addr);
      params->preferred_address.ipv6_port = ngtcp2_get_uint16(p);
      p += sizeof(uint16_t);

      if (params->preferred_address.ipv6_port ||
          memcmp(empty_address, params->preferred_address.ipv6_addr,
                 sizeof(params->preferred_address.ipv6_addr)) != 0) {
        params->preferred_address.ipv6_present = 1;
      }

      /* cid */
      params->preferred_address.cid.datalen = *p++;
      len += params->preferred_address.cid.datalen;
      if (valuelen != len ||
          params->preferred_address.cid.datalen > NGTCP2_MAX_CIDLEN ||
          params->preferred_address.cid.datalen < NGTCP2_MIN_CIDLEN) {
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
    case NGTCP2_TRANSPORT_PARAM_ORIGINAL_DESTINATION_CONNECTION_ID:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      nread = decode_cid_param(&params->original_dcid, p, end);
      if (nread < 0) {
        return (int)nread;
      }
      original_dcid_present = 1;
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_RETRY_SOURCE_CONNECTION_ID:
      if (exttype != NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      nread = decode_cid_param(&params->retry_scid, p, end);
      if (nread < 0) {
        return (int)nread;
      }
      params->retry_scid_present = 1;
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_INITIAL_SOURCE_CONNECTION_ID:
      nread = decode_cid_param(&params->initial_scid, p, end);
      if (nread < 0) {
        return (int)nread;
      }
      initial_scid_present = 1;
      p += nread;
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
    case NGTCP2_TRANSPORT_PARAM_MAX_DATAGRAM_FRAME_SIZE:
      nread = decode_varint_param(&params->max_datagram_frame_size, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      break;
    case NGTCP2_TRANSPORT_PARAM_GREASE_QUIC_BIT:
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0 || valuelen != 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      params->grease_quic_bit = 1;
      break;
    case NGTCP2_TRANSPORT_PARAM_VERSION_INFORMATION_DRAFT:
      nread = decode_varint(&valuelen, p, end);
      if (nread < 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += nread;
      if ((size_t)(end - p) < valuelen) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      if (valuelen < sizeof(uint32_t) || (valuelen & 0x3)) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      params->version_info.chosen_version = ngtcp2_get_uint32(p);
      if (params->version_info.chosen_version == 0) {
        return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
      }
      p += sizeof(uint32_t);
      if (valuelen > sizeof(uint32_t)) {
        params->version_info.other_versions = (uint8_t *)p;
        params->version_info.other_versionslen =
            (size_t)valuelen - sizeof(uint32_t);

        for (i = sizeof(uint32_t); i < valuelen;
             i += sizeof(uint32_t), p += sizeof(uint32_t)) {
          if (ngtcp2_get_uint32(p) == 0) {
            return NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM;
          }
        }
      }
      params->version_info_present = 1;
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

  if (!initial_scid_present ||
      (exttype == NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS &&
       !original_dcid_present)) {
    return NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM;
  }

  return 0;
}

static int transport_params_copy_new(ngtcp2_transport_params **pdest,
                                     const ngtcp2_transport_params *src,
                                     const ngtcp2_mem *mem) {
  size_t len = sizeof(**pdest);
  ngtcp2_transport_params *dest;
  uint8_t *p;

  if (src->version_info_present) {
    len += src->version_info.other_versionslen;
  }

  dest = ngtcp2_mem_malloc(mem, len);
  if (dest == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  *dest = *src;

  if (src->version_info_present && src->version_info.other_versionslen) {
    p = (uint8_t *)dest + sizeof(*dest);
    memcpy(p, src->version_info.other_versions,
           src->version_info.other_versionslen);
    dest->version_info.other_versions = p;
  }

  *pdest = dest;

  return 0;
}

int ngtcp2_decode_transport_params_new(ngtcp2_transport_params **pparams,
                                       ngtcp2_transport_params_type exttype,
                                       const uint8_t *data, size_t datalen,
                                       const ngtcp2_mem *mem) {
  int rv;
  ngtcp2_transport_params params;

  rv = ngtcp2_decode_transport_params(&params, exttype, data, datalen);
  if (rv < 0) {
    return rv;
  }

  if (mem == NULL) {
    mem = ngtcp2_mem_default();
  }

  return transport_params_copy_new(pparams, &params, mem);
}

void ngtcp2_transport_params_del(ngtcp2_transport_params *params,
                                 const ngtcp2_mem *mem) {
  if (params == NULL) {
    return;
  }

  if (mem == NULL) {
    mem = ngtcp2_mem_default();
  }

  ngtcp2_mem_free(mem, params);
}

int ngtcp2_transport_params_copy_new(ngtcp2_transport_params **pdest,
                                     const ngtcp2_transport_params *src,
                                     const ngtcp2_mem *mem) {
  if (src == NULL) {
    *pdest = NULL;
    return 0;
  }

  return transport_params_copy_new(pdest, src, mem);
}

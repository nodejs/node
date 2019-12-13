/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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
#ifndef NGTCP2_SHARED_H
#define NGTCP2_SHARED_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2_crypto.h>

/**
 * @function
 *
 * `ngtcp2_crypto_derive_initial_secrets` derives initial secrets.
 * |rx_secret| and |tx_secret| must point to the buffer of at least 32
 * bytes capacity.  rx for read and tx for write.  This function
 * writes rx and tx secrets into |rx_secret| and |tx_secret|
 * respectively.  The length of secret is 32 bytes long.
 * |client_dcid| is the destination connection ID in first Initial
 * packet of client.  If |initial_secret| is not NULL, the initial
 * secret is written to it.  It must point to the buffer which has at
 * least 32 bytes capacity.  The initial secret is 32 bytes long.
 * |side| specifies the side of application.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_derive_initial_secrets(uint8_t *rx_secret, uint8_t *tx_secret,
                                         uint8_t *initial_secret,
                                         const ngtcp2_cid *client_dcid,
                                         ngtcp2_crypto_side side);

/**
 * @function
 *
 * `ngtcp2_crypto_update_traffic_secret` derives the next generation
 * of the traffic secret.  |secret| specifies the current secret and
 * its length is given in |secretlen|.  The length of new key is the
 * same as the current key.  This function writes new key into the
 * buffer pointed by |dest|.  |dest| must have the enough capacity to
 * store the new key.
 *
 * This function returns 0 if it succeeds, or -1.
 */
int ngtcp2_crypto_update_traffic_secret(uint8_t *dest,
                                        const ngtcp2_crypto_md *md,
                                        const uint8_t *secret,
                                        size_t secretlen);

#endif /* NGTCP2_SHARED_H */

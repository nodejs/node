/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_PACKET_LOCL_H
# define HEADER_PACKET_LOCL_H

# include <string.h>
# include <openssl/bn.h>
# include <openssl/buffer.h>
# include <openssl/crypto.h>
# include <openssl/e_os2.h>

# include "internal/numbers.h"

# ifdef __cplusplus
extern "C" {
# endif

typedef struct {
    /* Pointer to where we are currently reading from */
    const unsigned char *curr;
    /* Number of bytes remaining */
    size_t remaining;
} PACKET;

/* Internal unchecked shorthand; don't use outside this file. */
static ossl_inline void packet_forward(PACKET *pkt, size_t len)
{
    pkt->curr += len;
    pkt->remaining -= len;
}

/*
 * Returns the number of bytes remaining to be read in the PACKET
 */
static ossl_inline size_t PACKET_remaining(const PACKET *pkt)
{
    return pkt->remaining;
}

/*
 * Returns a pointer to the first byte after the packet data.
 * Useful for integrating with non-PACKET parsing code.
 * Specifically, we use PACKET_end() to verify that a d2i_... call
 * has consumed the entire packet contents.
 */
static ossl_inline const unsigned char *PACKET_end(const PACKET *pkt)
{
    return pkt->curr + pkt->remaining;
}

/*
 * Returns a pointer to the PACKET's current position.
 * For use in non-PACKETized APIs.
 */
static ossl_inline const unsigned char *PACKET_data(const PACKET *pkt)
{
    return pkt->curr;
}

/*
 * Initialise a PACKET with |len| bytes held in |buf|. This does not make a
 * copy of the data so |buf| must be present for the whole time that the PACKET
 * is being used.
 */
__owur static ossl_inline int PACKET_buf_init(PACKET *pkt,
                                              const unsigned char *buf,
                                              size_t len)
{
    /* Sanity check for negative values. */
    if (len > (size_t)(SIZE_MAX / 2))
        return 0;

    pkt->curr = buf;
    pkt->remaining = len;
    return 1;
}

/* Initialize a PACKET to hold zero bytes. */
static ossl_inline void PACKET_null_init(PACKET *pkt)
{
    pkt->curr = NULL;
    pkt->remaining = 0;
}

/*
 * Returns 1 if the packet has length |num| and its contents equal the |num|
 * bytes read from |ptr|. Returns 0 otherwise (lengths or contents not equal).
 * If lengths are equal, performs the comparison in constant time.
 */
__owur static ossl_inline int PACKET_equal(const PACKET *pkt, const void *ptr,
                                           size_t num)
{
    if (PACKET_remaining(pkt) != num)
        return 0;
    return CRYPTO_memcmp(pkt->curr, ptr, num) == 0;
}

/*
 * Peek ahead and initialize |subpkt| with the next |len| bytes read from |pkt|.
 * Data is not copied: the |subpkt| packet will share its underlying buffer with
 * the original |pkt|, so data wrapped by |pkt| must outlive the |subpkt|.
 */
__owur static ossl_inline int PACKET_peek_sub_packet(const PACKET *pkt,
                                                     PACKET *subpkt, size_t len)
{
    if (PACKET_remaining(pkt) < len)
        return 0;

    return PACKET_buf_init(subpkt, pkt->curr, len);
}

/*
 * Initialize |subpkt| with the next |len| bytes read from |pkt|. Data is not
 * copied: the |subpkt| packet will share its underlying buffer with the
 * original |pkt|, so data wrapped by |pkt| must outlive the |subpkt|.
 */
__owur static ossl_inline int PACKET_get_sub_packet(PACKET *pkt,
                                                    PACKET *subpkt, size_t len)
{
    if (!PACKET_peek_sub_packet(pkt, subpkt, len))
        return 0;

    packet_forward(pkt, len);

    return 1;
}

/*
 * Peek ahead at 2 bytes in network order from |pkt| and store the value in
 * |*data|
 */
__owur static ossl_inline int PACKET_peek_net_2(const PACKET *pkt,
                                                unsigned int *data)
{
    if (PACKET_remaining(pkt) < 2)
        return 0;

    *data = ((unsigned int)(*pkt->curr)) << 8;
    *data |= *(pkt->curr + 1);

    return 1;
}

/* Equivalent of n2s */
/* Get 2 bytes in network order from |pkt| and store the value in |*data| */
__owur static ossl_inline int PACKET_get_net_2(PACKET *pkt, unsigned int *data)
{
    if (!PACKET_peek_net_2(pkt, data))
        return 0;

    packet_forward(pkt, 2);

    return 1;
}

/*
 * Peek ahead at 3 bytes in network order from |pkt| and store the value in
 * |*data|
 */
__owur static ossl_inline int PACKET_peek_net_3(const PACKET *pkt,
                                                unsigned long *data)
{
    if (PACKET_remaining(pkt) < 3)
        return 0;

    *data = ((unsigned long)(*pkt->curr)) << 16;
    *data |= ((unsigned long)(*(pkt->curr + 1))) << 8;
    *data |= *(pkt->curr + 2);

    return 1;
}

/* Equivalent of n2l3 */
/* Get 3 bytes in network order from |pkt| and store the value in |*data| */
__owur static ossl_inline int PACKET_get_net_3(PACKET *pkt, unsigned long *data)
{
    if (!PACKET_peek_net_3(pkt, data))
        return 0;

    packet_forward(pkt, 3);

    return 1;
}

/*
 * Peek ahead at 4 bytes in network order from |pkt| and store the value in
 * |*data|
 */
__owur static ossl_inline int PACKET_peek_net_4(const PACKET *pkt,
                                                unsigned long *data)
{
    if (PACKET_remaining(pkt) < 4)
        return 0;

    *data = ((unsigned long)(*pkt->curr)) << 24;
    *data |= ((unsigned long)(*(pkt->curr + 1))) << 16;
    *data |= ((unsigned long)(*(pkt->curr + 2))) << 8;
    *data |= *(pkt->curr + 3);

    return 1;
}

/* Equivalent of n2l */
/* Get 4 bytes in network order from |pkt| and store the value in |*data| */
__owur static ossl_inline int PACKET_get_net_4(PACKET *pkt, unsigned long *data)
{
    if (!PACKET_peek_net_4(pkt, data))
        return 0;

    packet_forward(pkt, 4);

    return 1;
}

/* Peek ahead at 1 byte from |pkt| and store the value in |*data| */
__owur static ossl_inline int PACKET_peek_1(const PACKET *pkt,
                                            unsigned int *data)
{
    if (!PACKET_remaining(pkt))
        return 0;

    *data = *pkt->curr;

    return 1;
}

/* Get 1 byte from |pkt| and store the value in |*data| */
__owur static ossl_inline int PACKET_get_1(PACKET *pkt, unsigned int *data)
{
    if (!PACKET_peek_1(pkt, data))
        return 0;

    packet_forward(pkt, 1);

    return 1;
}

/*
 * Peek ahead at 4 bytes in reverse network order from |pkt| and store the value
 * in |*data|
 */
__owur static ossl_inline int PACKET_peek_4(const PACKET *pkt,
                                            unsigned long *data)
{
    if (PACKET_remaining(pkt) < 4)
        return 0;

    *data = *pkt->curr;
    *data |= ((unsigned long)(*(pkt->curr + 1))) << 8;
    *data |= ((unsigned long)(*(pkt->curr + 2))) << 16;
    *data |= ((unsigned long)(*(pkt->curr + 3))) << 24;

    return 1;
}

/* Equivalent of c2l */
/*
 * Get 4 bytes in reverse network order from |pkt| and store the value in
 * |*data|
 */
__owur static ossl_inline int PACKET_get_4(PACKET *pkt, unsigned long *data)
{
    if (!PACKET_peek_4(pkt, data))
        return 0;

    packet_forward(pkt, 4);

    return 1;
}

/*
 * Peek ahead at |len| bytes from the |pkt| and store a pointer to them in
 * |*data|. This just points at the underlying buffer that |pkt| is using. The
 * caller should not free this data directly (it will be freed when the
 * underlying buffer gets freed
 */
__owur static ossl_inline int PACKET_peek_bytes(const PACKET *pkt,
                                                const unsigned char **data,
                                                size_t len)
{
    if (PACKET_remaining(pkt) < len)
        return 0;

    *data = pkt->curr;

    return 1;
}

/*
 * Read |len| bytes from the |pkt| and store a pointer to them in |*data|. This
 * just points at the underlying buffer that |pkt| is using. The caller should
 * not free this data directly (it will be freed when the underlying buffer gets
 * freed
 */
__owur static ossl_inline int PACKET_get_bytes(PACKET *pkt,
                                               const unsigned char **data,
                                               size_t len)
{
    if (!PACKET_peek_bytes(pkt, data, len))
        return 0;

    packet_forward(pkt, len);

    return 1;
}

/* Peek ahead at |len| bytes from |pkt| and copy them to |data| */
__owur static ossl_inline int PACKET_peek_copy_bytes(const PACKET *pkt,
                                                     unsigned char *data,
                                                     size_t len)
{
    if (PACKET_remaining(pkt) < len)
        return 0;

    memcpy(data, pkt->curr, len);

    return 1;
}

/*
 * Read |len| bytes from |pkt| and copy them to |data|.
 * The caller is responsible for ensuring that |data| can hold |len| bytes.
 */
__owur static ossl_inline int PACKET_copy_bytes(PACKET *pkt,
                                                unsigned char *data, size_t len)
{
    if (!PACKET_peek_copy_bytes(pkt, data, len))
        return 0;

    packet_forward(pkt, len);

    return 1;
}

/*
 * Copy packet data to |dest|, and set |len| to the number of copied bytes.
 * If the packet has more than |dest_len| bytes, nothing is copied.
 * Returns 1 if the packet data fits in |dest_len| bytes, 0 otherwise.
 * Does not forward PACKET position (because it is typically the last thing
 * done with a given PACKET).
 */
__owur static ossl_inline int PACKET_copy_all(const PACKET *pkt,
                                              unsigned char *dest,
                                              size_t dest_len, size_t *len)
{
    if (PACKET_remaining(pkt) > dest_len) {
        *len = 0;
        return 0;
    }
    *len = pkt->remaining;
    memcpy(dest, pkt->curr, pkt->remaining);
    return 1;
}

/*
 * Copy |pkt| bytes to a newly allocated buffer and store a pointer to the
 * result in |*data|, and the length in |len|.
 * If |*data| is not NULL, the old data is OPENSSL_free'd.
 * If the packet is empty, or malloc fails, |*data| will be set to NULL.
 * Returns 1 if the malloc succeeds and 0 otherwise.
 * Does not forward PACKET position (because it is typically the last thing
 * done with a given PACKET).
 */
__owur static ossl_inline int PACKET_memdup(const PACKET *pkt,
                                            unsigned char **data, size_t *len)
{
    size_t length;

    OPENSSL_free(*data);
    *data = NULL;
    *len = 0;

    length = PACKET_remaining(pkt);

    if (length == 0)
        return 1;

    *data = OPENSSL_memdup(pkt->curr, length);
    if (*data == NULL)
        return 0;

    *len = length;
    return 1;
}

/*
 * Read a C string from |pkt| and copy to a newly allocated, NUL-terminated
 * buffer. Store a pointer to the result in |*data|.
 * If |*data| is not NULL, the old data is OPENSSL_free'd.
 * If the data in |pkt| does not contain a NUL-byte, the entire data is
 * copied and NUL-terminated.
 * Returns 1 if the malloc succeeds and 0 otherwise.
 * Does not forward PACKET position (because it is typically the last thing done
 * with a given PACKET).
 */
__owur static ossl_inline int PACKET_strndup(const PACKET *pkt, char **data)
{
    OPENSSL_free(*data);

    /* This will succeed on an empty packet, unless pkt->curr == NULL. */
    *data = OPENSSL_strndup((const char *)pkt->curr, PACKET_remaining(pkt));
    return (*data != NULL);
}

/* Returns 1 if |pkt| contains at least one 0-byte, 0 otherwise. */
static ossl_inline int PACKET_contains_zero_byte(const PACKET *pkt)
{
    return memchr(pkt->curr, 0, pkt->remaining) != NULL;
}

/* Move the current reading position forward |len| bytes */
__owur static ossl_inline int PACKET_forward(PACKET *pkt, size_t len)
{
    if (PACKET_remaining(pkt) < len)
        return 0;

    packet_forward(pkt, len);

    return 1;
}

/*
 * Reads a variable-length vector prefixed with a one-byte length, and stores
 * the contents in |subpkt|. |pkt| can equal |subpkt|.
 * Data is not copied: the |subpkt| packet will share its underlying buffer with
 * the original |pkt|, so data wrapped by |pkt| must outlive the |subpkt|.
 * Upon failure, the original |pkt| and |subpkt| are not modified.
 */
__owur static ossl_inline int PACKET_get_length_prefixed_1(PACKET *pkt,
                                                           PACKET *subpkt)
{
    unsigned int length;
    const unsigned char *data;
    PACKET tmp = *pkt;
    if (!PACKET_get_1(&tmp, &length) ||
        !PACKET_get_bytes(&tmp, &data, (size_t)length)) {
        return 0;
    }

    *pkt = tmp;
    subpkt->curr = data;
    subpkt->remaining = length;

    return 1;
}

/*
 * Like PACKET_get_length_prefixed_1, but additionally, fails when there are
 * leftover bytes in |pkt|.
 */
__owur static ossl_inline int PACKET_as_length_prefixed_1(PACKET *pkt,
                                                          PACKET *subpkt)
{
    unsigned int length;
    const unsigned char *data;
    PACKET tmp = *pkt;
    if (!PACKET_get_1(&tmp, &length) ||
        !PACKET_get_bytes(&tmp, &data, (size_t)length) ||
        PACKET_remaining(&tmp) != 0) {
        return 0;
    }

    *pkt = tmp;
    subpkt->curr = data;
    subpkt->remaining = length;

    return 1;
}

/*
 * Reads a variable-length vector prefixed with a two-byte length, and stores
 * the contents in |subpkt|. |pkt| can equal |subpkt|.
 * Data is not copied: the |subpkt| packet will share its underlying buffer with
 * the original |pkt|, so data wrapped by |pkt| must outlive the |subpkt|.
 * Upon failure, the original |pkt| and |subpkt| are not modified.
 */
__owur static ossl_inline int PACKET_get_length_prefixed_2(PACKET *pkt,
                                                           PACKET *subpkt)
{
    unsigned int length;
    const unsigned char *data;
    PACKET tmp = *pkt;

    if (!PACKET_get_net_2(&tmp, &length) ||
        !PACKET_get_bytes(&tmp, &data, (size_t)length)) {
        return 0;
    }

    *pkt = tmp;
    subpkt->curr = data;
    subpkt->remaining = length;

    return 1;
}

/*
 * Like PACKET_get_length_prefixed_2, but additionally, fails when there are
 * leftover bytes in |pkt|.
 */
__owur static ossl_inline int PACKET_as_length_prefixed_2(PACKET *pkt,
                                                          PACKET *subpkt)
{
    unsigned int length;
    const unsigned char *data;
    PACKET tmp = *pkt;

    if (!PACKET_get_net_2(&tmp, &length) ||
        !PACKET_get_bytes(&tmp, &data, (size_t)length) ||
        PACKET_remaining(&tmp) != 0) {
        return 0;
    }

    *pkt = tmp;
    subpkt->curr = data;
    subpkt->remaining = length;

    return 1;
}

/*
 * Reads a variable-length vector prefixed with a three-byte length, and stores
 * the contents in |subpkt|. |pkt| can equal |subpkt|.
 * Data is not copied: the |subpkt| packet will share its underlying buffer with
 * the original |pkt|, so data wrapped by |pkt| must outlive the |subpkt|.
 * Upon failure, the original |pkt| and |subpkt| are not modified.
 */
__owur static ossl_inline int PACKET_get_length_prefixed_3(PACKET *pkt,
                                                           PACKET *subpkt)
{
    unsigned long length;
    const unsigned char *data;
    PACKET tmp = *pkt;
    if (!PACKET_get_net_3(&tmp, &length) ||
        !PACKET_get_bytes(&tmp, &data, (size_t)length)) {
        return 0;
    }

    *pkt = tmp;
    subpkt->curr = data;
    subpkt->remaining = length;

    return 1;
}
# ifdef __cplusplus
}
# endif

#endif                          /* HEADER_PACKET_LOCL_H */

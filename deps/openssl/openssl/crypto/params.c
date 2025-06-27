/*
 * Copyright 2019-2023 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2019, Oracle and/or its affiliates.  All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/params.h>
#include "internal/thread_once.h"
#include "internal/numbers.h"
#include "internal/endian.h"

#ifndef OPENSSL_SYS_UEFI
/*
 * Return the number of bits in the mantissa of a double.  This is used to
 * shift a larger integral value to determine if it will exactly fit into a
 * double.
 */
static unsigned int real_shift(void)
{
    return sizeof(double) == 4 ? 24 : 53;
}
#endif

OSSL_PARAM *OSSL_PARAM_locate(OSSL_PARAM *p, const char *key)
{
    if (p != NULL && key != NULL)
        for (; p->key != NULL; p++)
            if (strcmp(key, p->key) == 0)
                return p;
    return NULL;
}

const OSSL_PARAM *OSSL_PARAM_locate_const(const OSSL_PARAM *p, const char *key)
{
    return OSSL_PARAM_locate((OSSL_PARAM *)p, key);
}

static OSSL_PARAM ossl_param_construct(const char *key, unsigned int data_type,
                                       void *data, size_t data_size)
{
    OSSL_PARAM res;

    res.key = key;
    res.data_type = data_type;
    res.data = data;
    res.data_size = data_size;
    res.return_size = OSSL_PARAM_UNMODIFIED;
    return res;
}

int OSSL_PARAM_modified(const OSSL_PARAM *p)
{
    return p != NULL && p->return_size != OSSL_PARAM_UNMODIFIED;
}

void OSSL_PARAM_set_all_unmodified(OSSL_PARAM *p)
{
    if (p != NULL)
        while (p->key != NULL)
            p++->return_size = OSSL_PARAM_UNMODIFIED;
}

/* Return non-zero if the signed number is negative */
static int is_negative(const void *number, size_t s)
{
    const unsigned char *n = number;
    DECLARE_IS_ENDIAN;

    return 0x80 & (IS_BIG_ENDIAN ? n[0] : n[s - 1]);
}

/* Check that all the bytes specified match the expected sign byte */
static int check_sign_bytes(const unsigned char *p, size_t n, unsigned char s)
{
    size_t i;

    for (i = 0; i < n; i++)
        if (p[i] != s)
            return 0;
    return 1;
}

/*
 * Copy an integer to another integer.
 * Handle different length integers and signed and unsigned integers.
 * Both integers are in native byte ordering.
 */
static int copy_integer(unsigned char *dest, size_t dest_len,
                        const unsigned char *src, size_t src_len,
                        unsigned char pad, int signed_int)
{
    size_t n;
    DECLARE_IS_ENDIAN;

    if (IS_BIG_ENDIAN) {
        if (src_len < dest_len) {
            n = dest_len - src_len;
            memset(dest, pad, n);
            memcpy(dest + n, src, src_len);
        } else {
            n = src_len - dest_len;
            if (!check_sign_bytes(src, n, pad)
                    /*
                     * Shortening a signed value must retain the correct sign.
                     * Avoiding this kind of thing: -253 = 0xff03 -> 0x03 = 3
                     */
                    || (signed_int && ((pad ^ src[n]) & 0x80) != 0))
                return 0;
            memcpy(dest, src + n, dest_len);
        }
    } else /* IS_LITTLE_ENDIAN */ {
        if (src_len < dest_len) {
            n = dest_len - src_len;
            memset(dest + src_len, pad, n);
            memcpy(dest, src, src_len);
        } else {
            n = src_len - dest_len;
            if (!check_sign_bytes(src + dest_len, n, pad)
                    /*
                     * Shortening a signed value must retain the correct sign.
                     * Avoiding this kind of thing: 130 = 0x0082 -> 0x82 = -126
                     */
                    || (signed_int && ((pad ^ src[dest_len - 1]) & 0x80) != 0))
                return 0;
            memcpy(dest, src, dest_len);
        }
    }
    return 1;
}

/* Copy a signed number to a signed number of possibly different length */
static int signed_from_signed(void *dest, size_t dest_len,
                              const void *src, size_t src_len)
{
    return copy_integer(dest, dest_len, src, src_len,
                        is_negative(src, src_len) ? 0xff : 0, 1);
}

/* Copy an unsigned number to a signed number of possibly different length */
static int signed_from_unsigned(void *dest, size_t dest_len,
                                const void *src, size_t src_len)
{
    return copy_integer(dest, dest_len, src, src_len, 0, 1);
}

/* Copy a signed number to an unsigned number of possibly different length */
static int unsigned_from_signed(void *dest, size_t dest_len,
                                const void *src, size_t src_len)
{
    if (is_negative(src, src_len))
        return 0;
    return copy_integer(dest, dest_len, src, src_len, 0, 0);
}

/* Copy an unsigned number to an unsigned number of possibly different length */
static int unsigned_from_unsigned(void *dest, size_t dest_len,
                                  const void *src, size_t src_len)
{
    return copy_integer(dest, dest_len, src, src_len, 0, 0);
}

/* General purpose get integer parameter call that handles odd sizes */
static int general_get_int(const OSSL_PARAM *p, void *val, size_t val_size)
{
    if (p->data_type == OSSL_PARAM_INTEGER)
        return signed_from_signed(val, val_size, p->data, p->data_size);
    if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER)
        return signed_from_unsigned(val, val_size, p->data, p->data_size);
    return 0;
}

/* General purpose set integer parameter call that handles odd sizes */
static int general_set_int(OSSL_PARAM *p, void *val, size_t val_size)
{
    int r = 0;

    p->return_size = val_size; /* Expected size */
    if (p->data == NULL)
        return 1;
    if (p->data_type == OSSL_PARAM_INTEGER)
        r = signed_from_signed(p->data, p->data_size, val, val_size);
    else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER)
        r = unsigned_from_signed(p->data, p->data_size, val, val_size);
    p->return_size = r ? p->data_size : val_size;
    return r;
}

/* General purpose get unsigned integer parameter call that handles odd sizes */
static int general_get_uint(const OSSL_PARAM *p, void *val, size_t val_size)
{
    if (p->data_type == OSSL_PARAM_INTEGER)
        return unsigned_from_signed(val, val_size, p->data, p->data_size);
    if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER)
        return unsigned_from_unsigned(val, val_size, p->data, p->data_size);
    return 0;
}

/* General purpose set unsigned integer parameter call that handles odd sizes */
static int general_set_uint(OSSL_PARAM *p, void *val, size_t val_size)
{
    int r = 0;

    p->return_size = val_size; /* Expected size */
    if (p->data == NULL)
        return 1;
    if (p->data_type == OSSL_PARAM_INTEGER)
        r = signed_from_unsigned(p->data, p->data_size, val, val_size);
    else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER)
        r = unsigned_from_unsigned(p->data, p->data_size, val, val_size);
    p->return_size = r ? p->data_size : val_size;
    return r;
}

int OSSL_PARAM_get_int(const OSSL_PARAM *p, int *val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(int)) {
    case sizeof(int32_t):
        return OSSL_PARAM_get_int32(p, (int32_t *)val);
    case sizeof(int64_t):
        return OSSL_PARAM_get_int64(p, (int64_t *)val);
    }
#endif
    return general_get_int(p, val, sizeof(*val));
}

int OSSL_PARAM_set_int(OSSL_PARAM *p, int val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(int)) {
    case sizeof(int32_t):
        return OSSL_PARAM_set_int32(p, (int32_t)val);
    case sizeof(int64_t):
        return OSSL_PARAM_set_int64(p, (int64_t)val);
    }
#endif
    return general_set_int(p, &val, sizeof(val));
}

OSSL_PARAM OSSL_PARAM_construct_int(const char *key, int *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_INTEGER, buf, sizeof(int));
}

int OSSL_PARAM_get_uint(const OSSL_PARAM *p, unsigned int *val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(unsigned int)) {
    case sizeof(uint32_t):
        return OSSL_PARAM_get_uint32(p, (uint32_t *)val);
    case sizeof(uint64_t):
        return OSSL_PARAM_get_uint64(p, (uint64_t *)val);
    }
#endif
    return general_get_uint(p, val, sizeof(*val));
}

int OSSL_PARAM_set_uint(OSSL_PARAM *p, unsigned int val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(unsigned int)) {
    case sizeof(uint32_t):
        return OSSL_PARAM_set_uint32(p, (uint32_t)val);
    case sizeof(uint64_t):
        return OSSL_PARAM_set_uint64(p, (uint64_t)val);
    }
#endif
    return general_set_uint(p, &val, sizeof(val));
}

OSSL_PARAM OSSL_PARAM_construct_uint(const char *key, unsigned int *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_UNSIGNED_INTEGER, buf,
                                sizeof(unsigned int));
}

int OSSL_PARAM_get_long(const OSSL_PARAM *p, long int *val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(long int)) {
    case sizeof(int32_t):
        return OSSL_PARAM_get_int32(p, (int32_t *)val);
    case sizeof(int64_t):
        return OSSL_PARAM_get_int64(p, (int64_t *)val);
    }
#endif
    return general_get_int(p, val, sizeof(*val));
}

int OSSL_PARAM_set_long(OSSL_PARAM *p, long int val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(long int)) {
    case sizeof(int32_t):
        return OSSL_PARAM_set_int32(p, (int32_t)val);
    case sizeof(int64_t):
        return OSSL_PARAM_set_int64(p, (int64_t)val);
    }
#endif
    return general_set_int(p, &val, sizeof(val));
}

OSSL_PARAM OSSL_PARAM_construct_long(const char *key, long int *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_INTEGER, buf, sizeof(long int));
}

int OSSL_PARAM_get_ulong(const OSSL_PARAM *p, unsigned long int *val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(unsigned long int)) {
    case sizeof(uint32_t):
        return OSSL_PARAM_get_uint32(p, (uint32_t *)val);
    case sizeof(uint64_t):
        return OSSL_PARAM_get_uint64(p, (uint64_t *)val);
    }
#endif
    return general_get_uint(p, val, sizeof(*val));
}

int OSSL_PARAM_set_ulong(OSSL_PARAM *p, unsigned long int val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(unsigned long int)) {
    case sizeof(uint32_t):
        return OSSL_PARAM_set_uint32(p, (uint32_t)val);
    case sizeof(uint64_t):
        return OSSL_PARAM_set_uint64(p, (uint64_t)val);
    }
#endif
    return general_set_uint(p, &val, sizeof(val));
}

OSSL_PARAM OSSL_PARAM_construct_ulong(const char *key, unsigned long int *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_UNSIGNED_INTEGER, buf,
                                sizeof(unsigned long int));
}

int OSSL_PARAM_get_int32(const OSSL_PARAM *p, int32_t *val)
{
    if (val == NULL || p == NULL )
        return 0;

    if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        int64_t i64;

        switch (p->data_size) {
        case sizeof(int32_t):
            *val = *(const int32_t *)p->data;
            return 1;
        case sizeof(int64_t):
            i64 = *(const int64_t *)p->data;
            if (i64 >= INT32_MIN && i64 <= INT32_MAX) {
                *val = (int32_t)i64;
                return 1;
            }
            return 0;
        }
#endif
        return general_get_int(p, val, sizeof(*val));

    } else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        uint32_t u32;
        uint64_t u64;

        switch (p->data_size) {
        case sizeof(uint32_t):
            u32 = *(const uint32_t *)p->data;
            if (u32 <= INT32_MAX) {
                *val = (int32_t)u32;
                return 1;
            }
            return 0;
        case sizeof(uint64_t):
            u64 = *(const uint64_t *)p->data;
            if (u64 <= INT32_MAX) {
                *val = (int32_t)u64;
                return 1;
            }
            return 0;
        }
#endif
        return general_get_int(p, val, sizeof(*val));

    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        double d;

        switch (p->data_size) {
        case sizeof(double):
            d = *(const double *)p->data;
            if (d >= INT32_MIN && d <= INT32_MAX && d == (int32_t)d) {
                *val = (int32_t)d;
                return 1;
            }
            break;
        }
#endif
    }
    return 0;
}

int OSSL_PARAM_set_int32(OSSL_PARAM *p, int32_t val)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;
    if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(int32_t); /* Minimum expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(int32_t):
            *(int32_t *)p->data = val;
            return 1;
        case sizeof(int64_t):
            p->return_size = sizeof(int64_t);
            *(int64_t *)p->data = (int64_t)val;
            return 1;
        }
#endif
        return general_set_int(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER && val >= 0) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(uint32_t); /* Minimum expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(uint32_t):
            *(uint32_t *)p->data = (uint32_t)val;
            return 1;
        case sizeof(uint64_t):
            p->return_size = sizeof(uint64_t);
            *(uint64_t *)p->data = (uint64_t)val;
            return 1;
        }
#endif
        return general_set_int(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        p->return_size = sizeof(double);
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(double):
            *(double *)p->data = (double)val;
            return 1;
        }
#endif
    }
    return 0;
}

OSSL_PARAM OSSL_PARAM_construct_int32(const char *key, int32_t *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_INTEGER, buf,
                                sizeof(int32_t));
}

int OSSL_PARAM_get_uint32(const OSSL_PARAM *p, uint32_t *val)
{
    if (val == NULL || p == NULL)
        return 0;

    if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        uint64_t u64;

        switch (p->data_size) {
        case sizeof(uint32_t):
            *val = *(const uint32_t *)p->data;
            return 1;
        case sizeof(uint64_t):
            u64 = *(const uint64_t *)p->data;
            if (u64 <= UINT32_MAX) {
                *val = (uint32_t)u64;
                return 1;
            }
            return 0;
        }
#endif
        return general_get_uint(p, val, sizeof(*val));
    } else if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        int32_t i32;
        int64_t i64;

        switch (p->data_size) {
        case sizeof(int32_t):
            i32 = *(const int32_t *)p->data;
            if (i32 >= 0) {
                *val = i32;
                return 1;
            }
            return 0;
        case sizeof(int64_t):
            i64 = *(const int64_t *)p->data;
            if (i64 >= 0 && i64 <= UINT32_MAX) {
                *val = (uint32_t)i64;
                return 1;
            }
            return 0;
        }
#endif
        return general_get_uint(p, val, sizeof(*val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        double d;

        switch (p->data_size) {
        case sizeof(double):
            d = *(const double *)p->data;
            if (d >= 0 && d <= UINT32_MAX && d == (uint32_t)d) {
                *val = (uint32_t)d;
                return 1;
            }
            break;
        }
#endif
    }
    return 0;
}

int OSSL_PARAM_set_uint32(OSSL_PARAM *p, uint32_t val)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;

    if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(uint32_t); /* Minimum expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(uint32_t):
            *(uint32_t *)p->data = val;
            return 1;
        case sizeof(uint64_t):
            p->return_size = sizeof(uint64_t);
            *(uint64_t *)p->data = val;
            return 1;
        }
#endif
        return general_set_uint(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(int32_t); /* Minimum expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(int32_t):
            if (val <= INT32_MAX) {
                *(int32_t *)p->data = (int32_t)val;
                return 1;
            }
            return 0;
        case sizeof(int64_t):
            p->return_size = sizeof(int64_t);
            *(int64_t *)p->data = (int64_t)val;
            return 1;
        }
#endif
        return general_set_uint(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        p->return_size = sizeof(double);
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(double):
            *(double *)p->data = (double)val;
            return 1;
        }
#endif
    }
    return 0;
}

OSSL_PARAM OSSL_PARAM_construct_uint32(const char *key, uint32_t *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_UNSIGNED_INTEGER, buf,
                                sizeof(uint32_t));
}

int OSSL_PARAM_get_int64(const OSSL_PARAM *p, int64_t *val)
{
    if (val == NULL || p == NULL )
        return 0;

    if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        switch (p->data_size) {
        case sizeof(int32_t):
            *val = *(const int32_t *)p->data;
            return 1;
        case sizeof(int64_t):
            *val = *(const int64_t *)p->data;
            return 1;
        }
#endif
        return general_get_int(p, val, sizeof(*val));
    } else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        uint64_t u64;

        switch (p->data_size) {
        case sizeof(uint32_t):
            *val = *(const uint32_t *)p->data;
            return 1;
        case sizeof(uint64_t):
            u64 = *(const uint64_t *)p->data;
            if (u64 <= INT64_MAX) {
                *val = (int64_t)u64;
                return 1;
            }
            return 0;
        }
#endif
        return general_get_int(p, val, sizeof(*val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        double d;

        switch (p->data_size) {
        case sizeof(double):
            d = *(const double *)p->data;
            if (d >= INT64_MIN
                    /*
                     * By subtracting 65535 (2^16-1) we cancel the low order
                     * 15 bits of INT64_MAX to avoid using imprecise floating
                     * point values.
                     */
                    && d < (double)(INT64_MAX - 65535) + 65536.0
                    && d == (int64_t)d) {
                *val = (int64_t)d;
                return 1;
            }
            break;
        }
#endif
    }
    return 0;
}

int OSSL_PARAM_set_int64(OSSL_PARAM *p, int64_t val)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;
    if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(int64_t); /* Expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(int32_t):
            if (val >= INT32_MIN && val <= INT32_MAX) {
                p->return_size = sizeof(int32_t);
                *(int32_t *)p->data = (int32_t)val;
                return 1;
            }
            return 0;
        case sizeof(int64_t):
            *(int64_t *)p->data = val;
            return 1;
        }
#endif
        return general_set_int(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER && val >= 0) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(uint64_t); /* Expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(uint32_t):
            if (val <= UINT32_MAX) {
                p->return_size = sizeof(uint32_t);
                *(uint32_t *)p->data = (uint32_t)val;
                return 1;
            }
            return 0;
        case sizeof(uint64_t):
            *(uint64_t *)p->data = (uint64_t)val;
            return 1;
        }
#endif
        return general_set_int(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        uint64_t u64;

        p->return_size = sizeof(double);
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(double):
            u64 = val < 0 ? -val : val;
            if ((u64 >> real_shift()) == 0) {
                *(double *)p->data = (double)val;
                return 1;
            }
            break;
        }
#endif
    }
    return 0;
}

OSSL_PARAM OSSL_PARAM_construct_int64(const char *key, int64_t *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_INTEGER, buf, sizeof(int64_t));
}

int OSSL_PARAM_get_uint64(const OSSL_PARAM *p, uint64_t *val)
{
    if (val == NULL || p == NULL)
        return 0;

    if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        switch (p->data_size) {
        case sizeof(uint32_t):
            *val = *(const uint32_t *)p->data;
            return 1;
        case sizeof(uint64_t):
            *val = *(const uint64_t *)p->data;
            return 1;
        }
#endif
        return general_get_uint(p, val, sizeof(*val));
    } else if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        int32_t i32;
        int64_t i64;

        switch (p->data_size) {
        case sizeof(int32_t):
            i32 = *(const int32_t *)p->data;
            if (i32 >= 0) {
                *val = (uint64_t)i32;
                return 1;
            }
            return 0;
        case sizeof(int64_t):
            i64 = *(const int64_t *)p->data;
            if (i64 >= 0) {
                *val = (uint64_t)i64;
                return 1;
            }
            return 0;
        }
#endif
        return general_get_uint(p, val, sizeof(*val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        double d;

        switch (p->data_size) {
        case sizeof(double):
            d = *(const double *)p->data;
            if (d >= 0
                    /*
                     * By subtracting 65535 (2^16-1) we cancel the low order
                     * 15 bits of UINT64_MAX to avoid using imprecise floating
                     * point values.
                     */
                    && d < (double)(UINT64_MAX - 65535) + 65536.0
                    && d == (uint64_t)d) {
                *val = (uint64_t)d;
                return 1;
            }
            break;
        }
#endif
    }
    return 0;
}

int OSSL_PARAM_set_uint64(OSSL_PARAM *p, uint64_t val)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;

    if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(uint64_t); /* Expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(uint32_t):
            if (val <= UINT32_MAX) {
                p->return_size = sizeof(uint32_t);
                *(uint32_t *)p->data = (uint32_t)val;
                return 1;
            }
            return 0;
        case sizeof(uint64_t):
            *(uint64_t *)p->data = val;
            return 1;
        }
#endif
        return general_set_uint(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_INTEGER) {
#ifndef OPENSSL_SMALL_FOOTPRINT
        p->return_size = sizeof(int64_t); /* Expected size */
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(int32_t):
            if (val <= INT32_MAX) {
                p->return_size = sizeof(int32_t);
                *(int32_t *)p->data = (int32_t)val;
                return 1;
            }
            return 0;
        case sizeof(int64_t):
            if (val <= INT64_MAX) {
                *(int64_t *)p->data = (int64_t)val;
                return 1;
            }
            return 0;
        }
#endif
        return general_set_uint(p, &val, sizeof(val));
    } else if (p->data_type == OSSL_PARAM_REAL) {
#ifndef OPENSSL_SYS_UEFI
        p->return_size = sizeof(double);
        switch (p->data_size) {
        case sizeof(double):
            if ((val >> real_shift()) == 0) {
                *(double *)p->data = (double)val;
                return 1;
            }
            break;
        }
#endif
    }
    return 0;
}

OSSL_PARAM OSSL_PARAM_construct_uint64(const char *key, uint64_t *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_UNSIGNED_INTEGER, buf,
                                sizeof(uint64_t));
}

int OSSL_PARAM_get_size_t(const OSSL_PARAM *p, size_t *val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(size_t)) {
    case sizeof(uint32_t):
        return OSSL_PARAM_get_uint32(p, (uint32_t *)val);
    case sizeof(uint64_t):
        return OSSL_PARAM_get_uint64(p, (uint64_t *)val);
    }
#endif
    return general_get_uint(p, val, sizeof(*val));
}

int OSSL_PARAM_set_size_t(OSSL_PARAM *p, size_t val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(size_t)) {
    case sizeof(uint32_t):
        return OSSL_PARAM_set_uint32(p, (uint32_t)val);
    case sizeof(uint64_t):
        return OSSL_PARAM_set_uint64(p, (uint64_t)val);
    }
#endif
    return general_set_uint(p, &val, sizeof(val));
}

OSSL_PARAM OSSL_PARAM_construct_size_t(const char *key, size_t *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_UNSIGNED_INTEGER, buf,
                                sizeof(size_t));
}

int OSSL_PARAM_get_time_t(const OSSL_PARAM *p, time_t *val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(time_t)) {
    case sizeof(int32_t):
        return OSSL_PARAM_get_int32(p, (int32_t *)val);
    case sizeof(int64_t):
        return OSSL_PARAM_get_int64(p, (int64_t *)val);
    }
#endif
    return general_get_int(p, val, sizeof(*val));
}

int OSSL_PARAM_set_time_t(OSSL_PARAM *p, time_t val)
{
#ifndef OPENSSL_SMALL_FOOTPRINT
    switch (sizeof(time_t)) {
    case sizeof(int32_t):
        return OSSL_PARAM_set_int32(p, (int32_t)val);
    case sizeof(int64_t):
        return OSSL_PARAM_set_int64(p, (int64_t)val);
    }
#endif
    return general_set_int(p, &val, sizeof(val));
}

OSSL_PARAM OSSL_PARAM_construct_time_t(const char *key, time_t *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_INTEGER, buf, sizeof(time_t));
}

int OSSL_PARAM_get_BN(const OSSL_PARAM *p, BIGNUM **val)
{
    BIGNUM *b;

    if (val == NULL
        || p == NULL
        || p->data_type != OSSL_PARAM_UNSIGNED_INTEGER)
        return 0;

    b = BN_native2bn(p->data, (int)p->data_size, *val);
    if (b != NULL) {
        *val = b;
        return 1;
    }
    return 0;
}

int OSSL_PARAM_set_BN(OSSL_PARAM *p, const BIGNUM *val)
{
    size_t bytes;

    if (p == NULL)
        return 0;
    p->return_size = 0;
    if (val == NULL || p->data_type != OSSL_PARAM_UNSIGNED_INTEGER)
        return 0;

    /* For the moment, only positive values are permitted */
    if (BN_is_negative(val))
        return 0;

    bytes = (size_t)BN_num_bytes(val);
    /* We make sure that at least one byte is used, so zero is properly set */
    if (bytes == 0)
        bytes++;

    p->return_size = bytes;
    if (p->data == NULL)
        return 1;
    if (p->data_size >= bytes) {
        p->return_size = p->data_size;
        return BN_bn2nativepad(val, p->data, p->data_size) >= 0;
    }
    return 0;
}

OSSL_PARAM OSSL_PARAM_construct_BN(const char *key, unsigned char *buf,
                                   size_t bsize)
{
    return ossl_param_construct(key, OSSL_PARAM_UNSIGNED_INTEGER,
                                buf, bsize);
}

#ifndef OPENSSL_SYS_UEFI
int OSSL_PARAM_get_double(const OSSL_PARAM *p, double *val)
{
    int64_t i64;
    uint64_t u64;

    if (val == NULL || p == NULL)
        return 0;

    if (p->data_type == OSSL_PARAM_REAL) {
        switch (p->data_size) {
        case sizeof(double):
            *val = *(const double *)p->data;
            return 1;
        }
    } else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER) {
        switch (p->data_size) {
        case sizeof(uint32_t):
            *val = *(const uint32_t *)p->data;
            return 1;
        case sizeof(uint64_t):
            u64 = *(const uint64_t *)p->data;
            if ((u64 >> real_shift()) == 0) {
                *val = (double)u64;
                return 1;
            }
            break;
        }
    } else if (p->data_type == OSSL_PARAM_INTEGER) {
        switch (p->data_size) {
        case sizeof(int32_t):
            *val = *(const int32_t *)p->data;
            return 1;
        case sizeof(int64_t):
            i64 = *(const int64_t *)p->data;
            u64 = i64 < 0 ? -i64 : i64;
            if ((u64 >> real_shift()) == 0) {
                *val = 0.0 + i64;
                return 1;
            }
            break;
        }
    }
    return 0;
}

int OSSL_PARAM_set_double(OSSL_PARAM *p, double val)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;

    if (p->data_type == OSSL_PARAM_REAL) {
        p->return_size = sizeof(double);
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(double):
            *(double *)p->data = val;
            return 1;
        }
    } else if (p->data_type == OSSL_PARAM_UNSIGNED_INTEGER
               && val == (uint64_t)val) {
        p->return_size = sizeof(double);
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(uint32_t):
            if (val >= 0 && val <= UINT32_MAX) {
                p->return_size = sizeof(uint32_t);
                *(uint32_t *)p->data = (uint32_t)val;
                return 1;
            }
            break;
        case sizeof(uint64_t):
            if (val >= 0
                    /*
                     * By subtracting 65535 (2^16-1) we cancel the low order
                     * 15 bits of UINT64_MAX to avoid using imprecise floating
                     * point values.
                     */
                    && val < (double)(UINT64_MAX - 65535) + 65536.0) {
                p->return_size = sizeof(uint64_t);
                *(uint64_t *)p->data = (uint64_t)val;
                return 1;
            }
            break;            }
    } else if (p->data_type == OSSL_PARAM_INTEGER && val == (int64_t)val) {
        p->return_size = sizeof(double);
        if (p->data == NULL)
            return 1;
        switch (p->data_size) {
        case sizeof(int32_t):
            if (val >= INT32_MIN && val <= INT32_MAX) {
                p->return_size = sizeof(int32_t);
                *(int32_t *)p->data = (int32_t)val;
                return 1;
            }
            break;
        case sizeof(int64_t):
            if (val >= INT64_MIN
                    /*
                     * By subtracting 65535 (2^16-1) we cancel the low order
                     * 15 bits of INT64_MAX to avoid using imprecise floating
                     * point values.
                     */
                    && val < (double)(INT64_MAX - 65535) + 65536.0) {
                p->return_size = sizeof(int64_t);
                *(int64_t *)p->data = (int64_t)val;
                return 1;
            }
            break;
        }
    }
    return 0;
}

OSSL_PARAM OSSL_PARAM_construct_double(const char *key, double *buf)
{
    return ossl_param_construct(key, OSSL_PARAM_REAL, buf, sizeof(double));
}
#endif

static int get_string_internal(const OSSL_PARAM *p, void **val,
                               size_t *max_len, size_t *used_len,
                               unsigned int type)
{
    size_t sz, alloc_sz;

    if ((val == NULL && used_len == NULL) || p == NULL || p->data_type != type)
        return 0;

    sz = p->data_size;
    /*
     * If the input size is 0, or the input string needs NUL byte
     * termination, allocate an extra byte.
     */
    alloc_sz = sz + (type == OSSL_PARAM_UTF8_STRING || sz == 0);

    if (used_len != NULL)
        *used_len = sz;

    if (p->data == NULL)
        return 0;

    if (val == NULL)
        return 1;

    if (*val == NULL) {
        char *const q = OPENSSL_malloc(alloc_sz);

        if (q == NULL)
            return 0;
        *val = q;
        *max_len = alloc_sz;
    }

    if (*max_len < sz)
        return 0;
    memcpy(*val, p->data, sz);
    return 1;
}

int OSSL_PARAM_get_utf8_string(const OSSL_PARAM *p, char **val, size_t max_len)
{
    int ret = get_string_internal(p, (void **)val, &max_len, NULL,
                                  OSSL_PARAM_UTF8_STRING);

    /*
     * We try to ensure that the copied string is terminated with a
     * NUL byte.  That should be easy, just place a NUL byte at
     * |((char*)*val)[p->data_size]|.
     * Unfortunately, we have seen cases where |p->data_size| doesn't
     * correctly reflect the length of the string, and just happens
     * to be out of bounds according to |max_len|, so in that case, we
     * make the extra step of trying to find the true length of the
     * string that |p->data| points at, and use that as an index to
     * place the NUL byte in |*val|.
     */
    size_t data_length = p->data_size;

    if (ret == 0)
        return 0;
    if (data_length >= max_len)
        data_length = OPENSSL_strnlen(p->data, data_length);
    if (data_length >= max_len)
        return 0;            /* No space for a terminating NUL byte */
    (*val)[data_length] = '\0';

    return ret;
}

int OSSL_PARAM_get_octet_string(const OSSL_PARAM *p, void **val, size_t max_len,
                                size_t *used_len)
{
    return get_string_internal(p, val, &max_len, used_len,
                               OSSL_PARAM_OCTET_STRING);
}

static int set_string_internal(OSSL_PARAM *p, const void *val, size_t len,
                               unsigned int type)
{
    p->return_size = len;
    if (p->data == NULL)
        return 1;
    if (p->data_type != type || p->data_size < len)
        return 0;

    memcpy(p->data, val, len);
    /* If possible within the size of p->data, add a NUL terminator byte */
    if (type == OSSL_PARAM_UTF8_STRING && p->data_size > len)
        ((char *)p->data)[len] = '\0';
    return 1;
}

int OSSL_PARAM_set_utf8_string(OSSL_PARAM *p, const char *val)
{
    if (p == NULL)
        return 0;

    p->return_size = 0;
    if (val == NULL)
        return 0;
    return set_string_internal(p, val, strlen(val), OSSL_PARAM_UTF8_STRING);
}

int OSSL_PARAM_set_octet_string(OSSL_PARAM *p, const void *val,
                                size_t len)
{
    if (p == NULL)
        return 0;

    p->return_size = 0;
    if (val == NULL)
        return 0;
    return set_string_internal(p, val, len, OSSL_PARAM_OCTET_STRING);
}

OSSL_PARAM OSSL_PARAM_construct_utf8_string(const char *key, char *buf,
                                            size_t bsize)
{
    if (buf != NULL && bsize == 0)
        bsize = strlen(buf);
    return ossl_param_construct(key, OSSL_PARAM_UTF8_STRING, buf, bsize);
}

OSSL_PARAM OSSL_PARAM_construct_octet_string(const char *key, void *buf,
                                             size_t bsize)
{
    return ossl_param_construct(key, OSSL_PARAM_OCTET_STRING, buf, bsize);
}

static int get_ptr_internal(const OSSL_PARAM *p, const void **val,
                            size_t *used_len, unsigned int type)
{
    if (val == NULL || p == NULL || p->data_type != type)
        return 0;
    if (used_len != NULL)
        *used_len = p->data_size;
    *val = *(const void **)p->data;
    return 1;
}

int OSSL_PARAM_get_utf8_ptr(const OSSL_PARAM *p, const char **val)
{
    return get_ptr_internal(p, (const void **)val, NULL, OSSL_PARAM_UTF8_PTR);
}

int OSSL_PARAM_get_octet_ptr(const OSSL_PARAM *p, const void **val,
                             size_t *used_len)
{
    return get_ptr_internal(p, val, used_len, OSSL_PARAM_OCTET_PTR);
}

static int set_ptr_internal(OSSL_PARAM *p, const void *val,
                            unsigned int type, size_t len)
{
    p->return_size = len;
    if (p->data_type != type)
        return 0;
    if (p->data != NULL)
        *(const void **)p->data = val;
    return 1;
}

int OSSL_PARAM_set_utf8_ptr(OSSL_PARAM *p, const char *val)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;
    return set_ptr_internal(p, val, OSSL_PARAM_UTF8_PTR,
                            val == NULL ? 0 : strlen(val));
}

int OSSL_PARAM_set_octet_ptr(OSSL_PARAM *p, const void *val,
                             size_t used_len)
{
    if (p == NULL)
        return 0;
    p->return_size = 0;
    return set_ptr_internal(p, val, OSSL_PARAM_OCTET_PTR, used_len);
}

OSSL_PARAM OSSL_PARAM_construct_utf8_ptr(const char *key, char **buf,
                                         size_t bsize)
{
    return ossl_param_construct(key, OSSL_PARAM_UTF8_PTR, buf, bsize);
}

OSSL_PARAM OSSL_PARAM_construct_octet_ptr(const char *key, void **buf,
                                          size_t bsize)
{
    return ossl_param_construct(key, OSSL_PARAM_OCTET_PTR, buf, bsize);
}

OSSL_PARAM OSSL_PARAM_construct_end(void)
{
    OSSL_PARAM end = OSSL_PARAM_END;

    return end;
}

static int get_string_ptr_internal(const OSSL_PARAM *p, const void **val,
                                   size_t *used_len, unsigned int type)
{
    if (val == NULL || p == NULL || p->data_type != type)
        return 0;
    if (used_len != NULL)
        *used_len = p->data_size;
    *val = p->data;
    return 1;
}

int OSSL_PARAM_get_utf8_string_ptr(const OSSL_PARAM *p, const char **val)
{
    return OSSL_PARAM_get_utf8_ptr(p, val)
        || get_string_ptr_internal(p, (const void **)val, NULL,
                                   OSSL_PARAM_UTF8_STRING);
}

int OSSL_PARAM_get_octet_string_ptr(const OSSL_PARAM *p, const void **val,
                                    size_t *used_len)
{
    return OSSL_PARAM_get_octet_ptr(p, val, used_len)
        || get_string_ptr_internal(p, val, used_len, OSSL_PARAM_OCTET_STRING);
}

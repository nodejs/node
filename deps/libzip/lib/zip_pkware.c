/*
  zip_pkware.c -- Traditional PKWARE de/encryption backend routines
  Copyright (C) 2009-2020 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdlib.h>
#include <zlib.h>

#include "zipint.h"

#define PKWARE_KEY0 305419896
#define PKWARE_KEY1 591751049
#define PKWARE_KEY2 878082192


static void
update_keys(zip_pkware_keys_t *keys, zip_uint8_t b) {
    keys->key[0] = (zip_uint32_t)crc32(keys->key[0] ^ 0xffffffffUL, &b, 1) ^ 0xffffffffUL;
    keys->key[1] = (keys->key[1] + (keys->key[0] & 0xff)) * 134775813 + 1;
    b = (zip_uint8_t)(keys->key[1] >> 24);
    keys->key[2] = (zip_uint32_t)crc32(keys->key[2] ^ 0xffffffffUL, &b, 1) ^ 0xffffffffUL;
}


static zip_uint8_t
crypt_byte(zip_pkware_keys_t *keys) {
    zip_uint16_t tmp;
    tmp = (zip_uint16_t)(keys->key[2] | 2);
    tmp = (zip_uint16_t)(((zip_uint32_t)tmp * (tmp ^ 1)) >> 8);
    return (zip_uint8_t)tmp;
}


void
_zip_pkware_keys_reset(zip_pkware_keys_t *keys) {
    keys->key[0] = PKWARE_KEY0;
    keys->key[1] = PKWARE_KEY1;
    keys->key[2] = PKWARE_KEY2;
}


void
_zip_pkware_encrypt(zip_pkware_keys_t *keys, zip_uint8_t *out, const zip_uint8_t *in, zip_uint64_t len) {
    zip_uint64_t i;
    zip_uint8_t b;
    zip_uint8_t tmp;

    for (i = 0; i < len; i++) {
        b = in[i];

        if (out != NULL) {
            tmp = crypt_byte(keys);
            update_keys(keys, b);
            b ^= tmp;
            out[i] = b;
        }
        else {
            /* during initialization, we're only interested in key updates */
            update_keys(keys, b);
        }
    }
}


void
_zip_pkware_decrypt(zip_pkware_keys_t *keys, zip_uint8_t *out, const zip_uint8_t *in, zip_uint64_t len) {
    zip_uint64_t i;
    zip_uint8_t b;
    zip_uint8_t tmp;

    for (i = 0; i < len; i++) {
        b = in[i];

        /* during initialization, we're only interested in key updates */
        if (out != NULL) {
            tmp = crypt_byte(keys);
            b ^= tmp;
            out[i] = b;
        }

        update_keys(keys, b);
    }
}

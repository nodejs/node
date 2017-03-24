/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This has been a quickly hacked 'ideatest.c'.  When I add tests for other
 * RC2 modes, more of the code will be uncommented.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../e_os.h"

#ifdef OPENSSL_NO_RC2
int main(int argc, char *argv[])
{
    printf("No RC2 support\n");
    return (0);
}
#else
# include <openssl/rc2.h>

static unsigned char RC2key[4][16] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
     0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
};

static unsigned char RC2plain[4][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static unsigned char RC2cipher[4][8] = {
    {0x1C, 0x19, 0x8A, 0x83, 0x8D, 0xF0, 0x28, 0xB7},
    {0x21, 0x82, 0x9C, 0x78, 0xA9, 0xF9, 0xC0, 0x74},
    {0x13, 0xDB, 0x35, 0x17, 0xD3, 0x21, 0x86, 0x9E},
    {0x50, 0xDC, 0x01, 0x62, 0xBD, 0x75, 0x7F, 0x31},
};

int main(int argc, char *argv[])
{
    int i, n, err = 0;
    RC2_KEY key;
    unsigned char buf[8], buf2[8];

    for (n = 0; n < 4; n++) {
        RC2_set_key(&key, 16, &(RC2key[n][0]), 0 /* or 1024 */ );

        RC2_ecb_encrypt(&(RC2plain[n][0]), buf, &key, RC2_ENCRYPT);
        if (memcmp(&(RC2cipher[n][0]), buf, 8) != 0) {
            printf("ecb rc2 error encrypting\n");
            printf("got     :");
            for (i = 0; i < 8; i++)
                printf("%02X ", buf[i]);
            printf("\n");
            printf("expected:");
            for (i = 0; i < 8; i++)
                printf("%02X ", RC2cipher[n][i]);
            err = 20;
            printf("\n");
        }

        RC2_ecb_encrypt(buf, buf2, &key, RC2_DECRYPT);
        if (memcmp(&(RC2plain[n][0]), buf2, 8) != 0) {
            printf("ecb RC2 error decrypting\n");
            printf("got     :");
            for (i = 0; i < 8; i++)
                printf("%02X ", buf[i]);
            printf("\n");
            printf("expected:");
            for (i = 0; i < 8; i++)
                printf("%02X ", RC2plain[n][i]);
            printf("\n");
            err = 3;
        }
    }

    if (err == 0)
        printf("ecb RC2 ok\n");

    EXIT(err);
}

#endif

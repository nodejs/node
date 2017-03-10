/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

/*
 * Password based encryption (PBE) table ordering test.
 * Attempt to look up all supported algorithms.
 */

int main(int argc, char **argv)
{
    size_t i;
    int rv = 0;
    int pbe_type, pbe_nid;
    int last_type = -1, last_nid = -1;
    for (i = 0; EVP_PBE_get(&pbe_type, &pbe_nid, i) != 0; i++) {
        if (EVP_PBE_find(pbe_type, pbe_nid, NULL, NULL, 0) == 0) {
            rv = 1;
            break;
        }
    }
    if (rv == 0)
        return 0;
    /* Error: print out whole table */
    for (i = 0; EVP_PBE_get(&pbe_type, &pbe_nid, i) != 0; i++) {
        if (pbe_type > last_type)
            rv = 0;
        else if (pbe_type < last_type || pbe_nid < last_nid)
            rv = 1;
        else
            rv = 0;
        fprintf(stderr, "PBE type=%d %d (%s): %s\n", pbe_type, pbe_nid,
                OBJ_nid2sn(pbe_nid), rv ? "ERROR" : "OK");
        last_type = pbe_type;
        last_nid = pbe_nid;
    }
    return 1;
}

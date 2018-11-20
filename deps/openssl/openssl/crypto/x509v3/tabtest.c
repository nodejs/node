/*
 * Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Simple program to check the ext_dat.h is correct and print out problems if
 * it is not.
 */

#include <stdio.h>

#include <openssl/x509v3.h>

#include "ext_dat.h"

main()
{
    int i, prev = -1, bad = 0;
    X509V3_EXT_METHOD **tmp;
    i = OSSL_NELEM(standard_exts);
    if (i != STANDARD_EXTENSION_COUNT)
        fprintf(stderr, "Extension number invalid expecting %d\n", i);
    tmp = standard_exts;
    for (i = 0; i < STANDARD_EXTENSION_COUNT; i++, tmp++) {
        if ((*tmp)->ext_nid < prev)
            bad = 1;
        prev = (*tmp)->ext_nid;

    }
    if (bad) {
        tmp = standard_exts;
        fprintf(stderr, "Extensions out of order!\n");
        for (i = 0; i < STANDARD_EXTENSION_COUNT; i++, tmp++)
            printf("%d : %s\n", (*tmp)->ext_nid, OBJ_nid2sn((*tmp)->ext_nid));
    } else
        fprintf(stderr, "Order OK\n");
}

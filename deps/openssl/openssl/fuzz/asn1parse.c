/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL licenses, (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

/*
 * Fuzz the parser used for dumping ASN.1 using "openssl asn1parse".
 */

#include <stdio.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include "fuzzer.h"

int FuzzerInitialize(int *argc, char ***argv) {
    return 1;
}

int FuzzerTestOneInput(const uint8_t *buf, size_t len) {
    static BIO *bio_out;

    if (bio_out == NULL)
        bio_out = BIO_new_file("/dev/null", "w");

    (void)ASN1_parse_dump(bio_out, buf, len, 0, 0);
    return 0;
}

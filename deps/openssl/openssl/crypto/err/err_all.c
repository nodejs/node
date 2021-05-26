/*
 * Copyright 1995-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/err.h>
#include "crypto/err.h"
#include "crypto/cryptoerr.h"
#include "crypto/asn1err.h"
#include "crypto/bnerr.h"
#include "crypto/ecerr.h"
#include "crypto/buffererr.h"
#include "crypto/bioerr.h"
#include "crypto/comperr.h"
#include "crypto/rsaerr.h"
#include "crypto/dherr.h"
#include "crypto/dsaerr.h"
#include "crypto/evperr.h"
#include "crypto/objectserr.h"
#include "crypto/pemerr.h"
#include "crypto/pkcs7err.h"
#include "crypto/x509err.h"
#include "crypto/x509v3err.h"
#include "crypto/conferr.h"
#include "crypto/pkcs12err.h"
#include "crypto/randerr.h"
#include "internal/dsoerr.h"
#include "crypto/engineerr.h"
#include "crypto/uierr.h"
#include "crypto/httperr.h"
#include "crypto/ocsperr.h"
#include "crypto/tserr.h"
#include "crypto/cmserr.h"
#include "crypto/crmferr.h"
#include "crypto/cmperr.h"
#include "crypto/cterr.h"
#include "crypto/asyncerr.h"
#include "crypto/storeerr.h"
#include "crypto/esserr.h"
#include "internal/propertyerr.h"
#include "prov/proverr.h"

int err_load_crypto_strings_int(void)
{
    if (0
#ifndef OPENSSL_NO_ERR
        || err_load_ERR_strings_int() == 0 /* include error strings for SYSerr */
        || err_load_BN_strings_int() == 0
        || err_load_RSA_strings_int() == 0
# ifndef OPENSSL_NO_DH
        || err_load_DH_strings_int() == 0
# endif
        || err_load_EVP_strings_int() == 0
        || err_load_BUF_strings_int() == 0
        || err_load_OBJ_strings_int() == 0
        || err_load_PEM_strings_int() == 0
# ifndef OPENSSL_NO_DSA
        || err_load_DSA_strings_int() == 0
# endif
        || err_load_X509_strings_int() == 0
        || err_load_ASN1_strings_int() == 0
        || err_load_CONF_strings_int() == 0
        || err_load_CRYPTO_strings_int() == 0
# ifndef OPENSSL_NO_COMP
        || err_load_COMP_strings_int() == 0
# endif
# ifndef OPENSSL_NO_EC
        || err_load_EC_strings_int() == 0
# endif
        /* skip err_load_SSL_strings_int() because it is not in this library */
        || err_load_BIO_strings_int() == 0
        || err_load_PKCS7_strings_int() == 0
        || err_load_X509V3_strings_int() == 0
        || err_load_PKCS12_strings_int() == 0
        || err_load_RAND_strings_int() == 0
        || err_load_DSO_strings_int() == 0
# ifndef OPENSSL_NO_TS
        || err_load_TS_strings_int() == 0
# endif
# ifndef OPENSSL_NO_ENGINE
        || err_load_ENGINE_strings_int() == 0
# endif
        || err_load_HTTP_strings_int() == 0
# ifndef OPENSSL_NO_OCSP
        || err_load_OCSP_strings_int() == 0
# endif
        || err_load_UI_strings_int() == 0
# ifndef OPENSSL_NO_CMS
        || err_load_CMS_strings_int() == 0
# endif
# ifndef OPENSSL_NO_CRMF
        || err_load_CRMF_strings_int() == 0
        || err_load_CMP_strings_int() == 0
# endif
# ifndef OPENSSL_NO_CT
        || err_load_CT_strings_int() == 0
# endif
        || err_load_ESS_strings_int() == 0
        || err_load_ASYNC_strings_int() == 0
        || err_load_OSSL_STORE_strings_int() == 0
        || err_load_PROP_strings_int() == 0
        || err_load_PROV_strings_int() == 0
#endif
        )
        return 0;

    return 1;
}

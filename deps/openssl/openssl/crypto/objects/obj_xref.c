/*
 * Copyright 2006-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/objects.h>
#include "obj_xref.h"
#include "e_os.h"

static STACK_OF(nid_triple) *sig_app, *sigx_app;

static int sig_cmp(const nid_triple *a, const nid_triple *b)
{
    return a->sign_id - b->sign_id;
}

DECLARE_OBJ_BSEARCH_CMP_FN(nid_triple, nid_triple, sig);
IMPLEMENT_OBJ_BSEARCH_CMP_FN(nid_triple, nid_triple, sig);

static int sig_sk_cmp(const nid_triple *const *a, const nid_triple *const *b)
{
    return (*a)->sign_id - (*b)->sign_id;
}

DECLARE_OBJ_BSEARCH_CMP_FN(const nid_triple *, const nid_triple *, sigx);

static int sigx_cmp(const nid_triple *const *a, const nid_triple *const *b)
{
    int ret;
    ret = (*a)->hash_id - (*b)->hash_id;
    if (ret)
        return ret;
    return (*a)->pkey_id - (*b)->pkey_id;
}

IMPLEMENT_OBJ_BSEARCH_CMP_FN(const nid_triple *, const nid_triple *, sigx);

int OBJ_find_sigid_algs(int signid, int *pdig_nid, int *ppkey_nid)
{
    nid_triple tmp;
    const nid_triple *rv = NULL;
    tmp.sign_id = signid;

    if (sig_app) {
        int idx = sk_nid_triple_find(sig_app, &tmp);
        if (idx >= 0)
            rv = sk_nid_triple_value(sig_app, idx);
    }
#ifndef OBJ_XREF_TEST2
    if (rv == NULL) {
        rv = OBJ_bsearch_sig(&tmp, sigoid_srt, OSSL_NELEM(sigoid_srt));
    }
#endif
    if (rv == NULL)
        return 0;
    if (pdig_nid)
        *pdig_nid = rv->hash_id;
    if (ppkey_nid)
        *ppkey_nid = rv->pkey_id;
    return 1;
}

int OBJ_find_sigid_by_algs(int *psignid, int dig_nid, int pkey_nid)
{
    nid_triple tmp;
    const nid_triple *t = &tmp;
    const nid_triple **rv = NULL;

    tmp.hash_id = dig_nid;
    tmp.pkey_id = pkey_nid;

    if (sigx_app) {
        int idx = sk_nid_triple_find(sigx_app, &tmp);
        if (idx >= 0) {
            t = sk_nid_triple_value(sigx_app, idx);
            rv = &t;
        }
    }
#ifndef OBJ_XREF_TEST2
    if (rv == NULL) {
        rv = OBJ_bsearch_sigx(&t, sigoid_srt_xref, OSSL_NELEM(sigoid_srt_xref));
    }
#endif
    if (rv == NULL)
        return 0;
    if (psignid)
        *psignid = (*rv)->sign_id;
    return 1;
}

int OBJ_add_sigid(int signid, int dig_id, int pkey_id)
{
    nid_triple *ntr;
    if (sig_app == NULL)
        sig_app = sk_nid_triple_new(sig_sk_cmp);
    if (sig_app == NULL)
        return 0;
    if (sigx_app == NULL)
        sigx_app = sk_nid_triple_new(sigx_cmp);
    if (sigx_app == NULL)
        return 0;
    ntr = OPENSSL_malloc(sizeof(*ntr));
    if (ntr == NULL)
        return 0;
    ntr->sign_id = signid;
    ntr->hash_id = dig_id;
    ntr->pkey_id = pkey_id;

    if (!sk_nid_triple_push(sig_app, ntr)) {
        OPENSSL_free(ntr);
        return 0;
    }

    if (!sk_nid_triple_push(sigx_app, ntr))
        return 0;

    sk_nid_triple_sort(sig_app);
    sk_nid_triple_sort(sigx_app);

    return 1;
}

static void sid_free(nid_triple *tt)
{
    OPENSSL_free(tt);
}

void OBJ_sigid_free(void)
{
    sk_nid_triple_pop_free(sig_app, sid_free);
    sig_app = NULL;
    sk_nid_triple_free(sigx_app);
    sigx_app = NULL;
}

#ifdef OBJ_XREF_TEST

main()
{
    int n1, n2, n3;

    int i, rv;
# ifdef OBJ_XREF_TEST2
    for (i = 0; i < OSSL_NELEM(sigoid_srt); i++) {
        OBJ_add_sigid(sigoid_srt[i][0], sigoid_srt[i][1], sigoid_srt[i][2]);
    }
# endif

    for (i = 0; i < OSSL_NELEM(sigoid_srt); i++) {
        n1 = sigoid_srt[i][0];
        rv = OBJ_find_sigid_algs(n1, &n2, &n3);
        printf("Forward: %d, %s %s %s\n", rv,
               OBJ_nid2ln(n1), OBJ_nid2ln(n2), OBJ_nid2ln(n3));
        n1 = 0;
        rv = OBJ_find_sigid_by_algs(&n1, n2, n3);
        printf("Reverse: %d, %s %s %s\n", rv,
               OBJ_nid2ln(n1), OBJ_nid2ln(n2), OBJ_nid2ln(n3));
    }
}

#endif

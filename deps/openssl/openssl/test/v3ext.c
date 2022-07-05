/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "internal/nelem.h"

#include "testutil.h"

static const char *infile;

static int test_pathlen(void)
{
    X509 *x = NULL;
    BIO *b = NULL;
    long pathlen;
    int ret = 0;

    if (!TEST_ptr(b = BIO_new_file(infile, "r"))
            || !TEST_ptr(x = PEM_read_bio_X509(b, NULL, NULL, NULL))
            || !TEST_int_eq(pathlen = X509_get_pathlen(x), 6))
        goto end;

    ret = 1;

end:
    BIO_free(b);
    X509_free(x);
    return ret;
}

#ifndef OPENSSL_NO_RFC3779
static int test_asid(void)
{
    ASN1_INTEGER *val1 = NULL, *val2 = NULL;
    ASIdentifiers *asid1 = ASIdentifiers_new(), *asid2 = ASIdentifiers_new(),
                  *asid3 = ASIdentifiers_new(), *asid4 = ASIdentifiers_new();
    int testresult = 0;

    if (!TEST_ptr(asid1)
            || !TEST_ptr(asid2)
            || !TEST_ptr(asid3))
        goto err;

    if (!TEST_ptr(val1 = ASN1_INTEGER_new())
            || !TEST_true(ASN1_INTEGER_set_int64(val1, 64496)))
        goto err;

    if (!TEST_true(X509v3_asid_add_id_or_range(asid1, V3_ASID_ASNUM, val1, NULL)))
        goto err;

    val1 = NULL;
    if (!TEST_ptr(val2 = ASN1_INTEGER_new())
            || !TEST_true(ASN1_INTEGER_set_int64(val2, 64497)))
        goto err;

    if (!TEST_true(X509v3_asid_add_id_or_range(asid2, V3_ASID_ASNUM, val2, NULL)))
        goto err;

    val2 = NULL;
    if (!TEST_ptr(val1 = ASN1_INTEGER_new())
            || !TEST_true(ASN1_INTEGER_set_int64(val1, 64496))
            || !TEST_ptr(val2 = ASN1_INTEGER_new())
            || !TEST_true(ASN1_INTEGER_set_int64(val2, 64497)))
        goto err;

    /*
     * Just tests V3_ASID_ASNUM for now. Could be extended at some point to also
     * test V3_ASID_RDI if we think it is worth it.
     */
    if (!TEST_true(X509v3_asid_add_id_or_range(asid3, V3_ASID_ASNUM, val1, val2)))
        goto err;
    val1 = val2 = NULL;

    /* Actual subsets */
    if (!TEST_true(X509v3_asid_subset(NULL, NULL))
            || !TEST_true(X509v3_asid_subset(NULL, asid1))
            || !TEST_true(X509v3_asid_subset(asid1, asid1))
            || !TEST_true(X509v3_asid_subset(asid2, asid2))
            || !TEST_true(X509v3_asid_subset(asid1, asid3))
            || !TEST_true(X509v3_asid_subset(asid2, asid3))
            || !TEST_true(X509v3_asid_subset(asid3, asid3))
            || !TEST_true(X509v3_asid_subset(asid4, asid1))
            || !TEST_true(X509v3_asid_subset(asid4, asid2))
            || !TEST_true(X509v3_asid_subset(asid4, asid3)))
        goto err;

    /* Not subsets */
    if (!TEST_false(X509v3_asid_subset(asid1, NULL))
            || !TEST_false(X509v3_asid_subset(asid1, asid2))
            || !TEST_false(X509v3_asid_subset(asid2, asid1))
            || !TEST_false(X509v3_asid_subset(asid3, asid1))
            || !TEST_false(X509v3_asid_subset(asid3, asid2))
            || !TEST_false(X509v3_asid_subset(asid1, asid4))
            || !TEST_false(X509v3_asid_subset(asid2, asid4))
            || !TEST_false(X509v3_asid_subset(asid3, asid4)))
        goto err;

    testresult = 1;
 err:
    ASN1_INTEGER_free(val1);
    ASN1_INTEGER_free(val2);
    ASIdentifiers_free(asid1);
    ASIdentifiers_free(asid2);
    ASIdentifiers_free(asid3);
    ASIdentifiers_free(asid4);
    return testresult;
}

static struct ip_ranges_st {
    const unsigned int afi;
    const char *ip1;
    const char *ip2;
    int rorp;
} ranges[] = {
    { IANA_AFI_IPV4, "192.168.0.0", "192.168.0.1", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV4, "192.168.0.0", "192.168.0.2", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV4, "192.168.0.0", "192.168.0.3", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV4, "192.168.0.0", "192.168.0.254", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV4, "192.168.0.0", "192.168.0.255", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV4, "192.168.0.1", "192.168.0.255", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV4, "192.168.0.1", "192.168.0.1", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV4, "192.168.0.0", "192.168.255.255", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV4, "192.168.1.0", "192.168.255.255", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV6, "2001:0db8::0", "2001:0db8::1", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV6, "2001:0db8::0", "2001:0db8::2", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV6, "2001:0db8::0", "2001:0db8::3", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV6, "2001:0db8::0", "2001:0db8::fffe", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV6, "2001:0db8::0", "2001:0db8::ffff", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV6, "2001:0db8::1", "2001:0db8::ffff", IPAddressOrRange_addressRange},
    { IANA_AFI_IPV6, "2001:0db8::1", "2001:0db8::1", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV6, "2001:0db8::0:0", "2001:0db8::ffff:ffff", IPAddressOrRange_addressPrefix},
    { IANA_AFI_IPV6, "2001:0db8::1:0", "2001:0db8::ffff:ffff", IPAddressOrRange_addressRange}
};

static int check_addr(IPAddrBlocks *addr, int type)
{
    IPAddressFamily *fam;
    IPAddressOrRange *aorr;

    if (!TEST_int_eq(sk_IPAddressFamily_num(addr), 1))
        return 0;

    fam = sk_IPAddressFamily_value(addr, 0);
    if (!TEST_ptr(fam))
        return 0;

    if (!TEST_int_eq(fam->ipAddressChoice->type, IPAddressChoice_addressesOrRanges))
        return 0;

    if (!TEST_int_eq(sk_IPAddressOrRange_num(fam->ipAddressChoice->u.addressesOrRanges), 1))
        return 0;

    aorr = sk_IPAddressOrRange_value(fam->ipAddressChoice->u.addressesOrRanges, 0);
    if (!TEST_ptr(aorr))
        return 0;

    if (!TEST_int_eq(aorr->type, type))
        return 0;

    return 1;
}

static int test_addr_ranges(void)
{
    IPAddrBlocks *addr = NULL;
    ASN1_OCTET_STRING *ip1 = NULL, *ip2 = NULL;
    size_t i;
    int testresult = 0;

    for (i = 0; i < OSSL_NELEM(ranges); i++) {
        addr = sk_IPAddressFamily_new_null();
        if (!TEST_ptr(addr))
            goto end;
        /*
         * Has the side effect of installing the comparison function onto the
         * stack.
         */
        if (!TEST_true(X509v3_addr_canonize(addr)))
            goto end;

        ip1 = a2i_IPADDRESS(ranges[i].ip1);
        if (!TEST_ptr(ip1))
            goto end;
        if (!TEST_true(ip1->length == 4 || ip1->length == 16))
            goto end;
        ip2 = a2i_IPADDRESS(ranges[i].ip2);
        if (!TEST_ptr(ip2))
            goto end;
        if (!TEST_int_eq(ip2->length, ip1->length))
            goto end;
        if (!TEST_true(memcmp(ip1->data, ip2->data, ip1->length) <= 0))
            goto end;

        if (!TEST_true(X509v3_addr_add_range(addr, ranges[i].afi, NULL, ip1->data, ip2->data)))
            goto end;

        if (!TEST_true(X509v3_addr_is_canonical(addr)))
            goto end;

        if (!check_addr(addr, ranges[i].rorp))
            goto end;

        sk_IPAddressFamily_pop_free(addr, IPAddressFamily_free);
        addr = NULL;
        ASN1_OCTET_STRING_free(ip1);
        ASN1_OCTET_STRING_free(ip2);
        ip1 = ip2 = NULL;
    }

    testresult = 1;
 end:
    sk_IPAddressFamily_pop_free(addr, IPAddressFamily_free);
    ASN1_OCTET_STRING_free(ip1);
    ASN1_OCTET_STRING_free(ip2);
    return testresult;
}
#endif /* OPENSSL_NO_RFC3779 */

int setup_tests(void)
{
    if (!TEST_ptr(infile = test_get_argument(0)))
        return 0;

    ADD_TEST(test_pathlen);
#ifndef OPENSSL_NO_RFC3779
    ADD_TEST(test_asid);
    ADD_TEST(test_addr_ranges);
#endif /* OPENSSL_NO_RFC3779 */
    return 1;
}

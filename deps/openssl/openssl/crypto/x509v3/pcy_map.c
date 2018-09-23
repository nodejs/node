/* pcy_map.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include "cryptlib.h"
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "pcy_int.h"

/*
 * Set policy mapping entries in cache. Note: this modifies the passed
 * POLICY_MAPPINGS structure
 */

int policy_cache_set_mapping(X509 *x, POLICY_MAPPINGS *maps)
{
    POLICY_MAPPING *map;
    X509_POLICY_DATA *data;
    X509_POLICY_CACHE *cache = x->policy_cache;
    int i;
    int ret = 0;
    if (sk_POLICY_MAPPING_num(maps) == 0) {
        ret = -1;
        goto bad_mapping;
    }
    for (i = 0; i < sk_POLICY_MAPPING_num(maps); i++) {
        map = sk_POLICY_MAPPING_value(maps, i);
        /* Reject if map to or from anyPolicy */
        if ((OBJ_obj2nid(map->subjectDomainPolicy) == NID_any_policy)
            || (OBJ_obj2nid(map->issuerDomainPolicy) == NID_any_policy)) {
            ret = -1;
            goto bad_mapping;
        }

        /* Attempt to find matching policy data */
        data = policy_cache_find_data(cache, map->issuerDomainPolicy);
        /* If we don't have anyPolicy can't map */
        if (!data && !cache->anyPolicy)
            continue;

        /* Create a NODE from anyPolicy */
        if (!data) {
            data = policy_data_new(NULL, map->issuerDomainPolicy,
                                   cache->anyPolicy->flags
                                   & POLICY_DATA_FLAG_CRITICAL);
            if (!data)
                goto bad_mapping;
            data->qualifier_set = cache->anyPolicy->qualifier_set;
            /*
             * map->issuerDomainPolicy = NULL;
             */
            data->flags |= POLICY_DATA_FLAG_MAPPED_ANY;
            data->flags |= POLICY_DATA_FLAG_SHARED_QUALIFIERS;
            if (!sk_X509_POLICY_DATA_push(cache->data, data)) {
                policy_data_free(data);
                goto bad_mapping;
            }
        } else
            data->flags |= POLICY_DATA_FLAG_MAPPED;
        if (!sk_ASN1_OBJECT_push(data->expected_policy_set,
                                 map->subjectDomainPolicy))
            goto bad_mapping;
        map->subjectDomainPolicy = NULL;

    }

    ret = 1;
 bad_mapping:
    if (ret == -1)
        x->ex_flags |= EXFLAG_INVALID_POLICY;
    sk_POLICY_MAPPING_pop_free(maps, POLICY_MAPPING_free);
    return ret;

}

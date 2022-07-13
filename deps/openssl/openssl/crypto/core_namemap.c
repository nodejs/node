/*
 * Copyright 2019-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/namemap.h"
#include <openssl/lhash.h>
#include "crypto/lhash.h"      /* ossl_lh_strcasehash */
#include "internal/tsan_assist.h"
#include "internal/sizes.h"

/*-
 * The namenum entry
 * =================
 */
typedef struct {
    char *name;
    int number;
} NAMENUM_ENTRY;

DEFINE_LHASH_OF(NAMENUM_ENTRY);

/*-
 * The namemap itself
 * ==================
 */

struct ossl_namemap_st {
    /* Flags */
    unsigned int stored:1; /* If 1, it's stored in a library context */

    CRYPTO_RWLOCK *lock;
    LHASH_OF(NAMENUM_ENTRY) *namenum;  /* Name->number mapping */

    TSAN_QUALIFIER int max_number;     /* Current max number */
};

/* LHASH callbacks */

static unsigned long namenum_hash(const NAMENUM_ENTRY *n)
{
    return ossl_lh_strcasehash(n->name);
}

static int namenum_cmp(const NAMENUM_ENTRY *a, const NAMENUM_ENTRY *b)
{
    return OPENSSL_strcasecmp(a->name, b->name);
}

static void namenum_free(NAMENUM_ENTRY *n)
{
    if (n != NULL)
        OPENSSL_free(n->name);
    OPENSSL_free(n);
}

/* OSSL_LIB_CTX_METHOD functions for a namemap stored in a library context */

static void *stored_namemap_new(OSSL_LIB_CTX *libctx)
{
    OSSL_NAMEMAP *namemap = ossl_namemap_new();

    if (namemap != NULL)
        namemap->stored = 1;

    return namemap;
}

static void stored_namemap_free(void *vnamemap)
{
    OSSL_NAMEMAP *namemap = vnamemap;

    if (namemap != NULL) {
        /* Pretend it isn't stored, or ossl_namemap_free() will do nothing */
        namemap->stored = 0;
        ossl_namemap_free(namemap);
    }
}

static const OSSL_LIB_CTX_METHOD stored_namemap_method = {
    OSSL_LIB_CTX_METHOD_DEFAULT_PRIORITY,
    stored_namemap_new,
    stored_namemap_free,
};

/*-
 * API functions
 * =============
 */

int ossl_namemap_empty(OSSL_NAMEMAP *namemap)
{
#ifdef TSAN_REQUIRES_LOCKING
    /* No TSAN support */
    int rv;

    if (namemap == NULL)
        return 1;

    if (!CRYPTO_THREAD_read_lock(namemap->lock))
        return -1;
    rv = namemap->max_number == 0;
    CRYPTO_THREAD_unlock(namemap->lock);
    return rv;
#else
    /* Have TSAN support */
    return namemap == NULL || tsan_load(&namemap->max_number) == 0;
#endif
}

typedef struct doall_names_data_st {
    int number;
    const char **names;
    int found;
} DOALL_NAMES_DATA;

static void do_name(const NAMENUM_ENTRY *namenum, DOALL_NAMES_DATA *data)
{
    if (namenum->number == data->number)
        data->names[data->found++] = namenum->name;
}

IMPLEMENT_LHASH_DOALL_ARG_CONST(NAMENUM_ENTRY, DOALL_NAMES_DATA);

/*
 * Call the callback for all names in the namemap with the given number.
 * A return value 1 means that the callback was called for all names. A
 * return value of 0 means that the callback was not called for any names.
 */
int ossl_namemap_doall_names(const OSSL_NAMEMAP *namemap, int number,
                             void (*fn)(const char *name, void *data),
                             void *data)
{
    DOALL_NAMES_DATA cbdata;
    size_t num_names;
    int i;

    cbdata.number = number;
    cbdata.found = 0;

    /*
     * We collect all the names first under a read lock. Subsequently we call
     * the user function, so that we're not holding the read lock when in user
     * code. This could lead to deadlocks.
     */
    if (!CRYPTO_THREAD_read_lock(namemap->lock))
        return 0;

    num_names = lh_NAMENUM_ENTRY_num_items(namemap->namenum);
    if (num_names == 0) {
        CRYPTO_THREAD_unlock(namemap->lock);
        return 0;
    }
    cbdata.names = OPENSSL_malloc(sizeof(*cbdata.names) * num_names);
    if (cbdata.names == NULL) {
        CRYPTO_THREAD_unlock(namemap->lock);
        return 0;
    }
    lh_NAMENUM_ENTRY_doall_DOALL_NAMES_DATA(namemap->namenum, do_name,
                                            &cbdata);
    CRYPTO_THREAD_unlock(namemap->lock);

    for (i = 0; i < cbdata.found; i++)
        fn(cbdata.names[i], data);

    OPENSSL_free(cbdata.names);
    return 1;
}

static int namemap_name2num_n(const OSSL_NAMEMAP *namemap,
                              const char *name, size_t name_len)
{
    NAMENUM_ENTRY *namenum_entry, namenum_tmpl;

    if ((namenum_tmpl.name = OPENSSL_strndup(name, name_len)) == NULL)
        return 0;
    namenum_tmpl.number = 0;
    namenum_entry =
        lh_NAMENUM_ENTRY_retrieve(namemap->namenum, &namenum_tmpl);
    OPENSSL_free(namenum_tmpl.name);
    return namenum_entry != NULL ? namenum_entry->number : 0;
}

int ossl_namemap_name2num_n(const OSSL_NAMEMAP *namemap,
                            const char *name, size_t name_len)
{
    int number;

#ifndef FIPS_MODULE
    if (namemap == NULL)
        namemap = ossl_namemap_stored(NULL);
#endif

    if (namemap == NULL)
        return 0;

    if (!CRYPTO_THREAD_read_lock(namemap->lock))
        return 0;
    number = namemap_name2num_n(namemap, name, name_len);
    CRYPTO_THREAD_unlock(namemap->lock);

    return number;
}

int ossl_namemap_name2num(const OSSL_NAMEMAP *namemap, const char *name)
{
    if (name == NULL)
        return 0;

    return ossl_namemap_name2num_n(namemap, name, strlen(name));
}

struct num2name_data_st {
    size_t idx;                  /* Countdown */
    const char *name;            /* Result */
};

static void do_num2name(const char *name, void *vdata)
{
    struct num2name_data_st *data = vdata;

    if (data->idx > 0)
        data->idx--;
    else if (data->name == NULL)
        data->name = name;
}

const char *ossl_namemap_num2name(const OSSL_NAMEMAP *namemap, int number,
                                  size_t idx)
{
    struct num2name_data_st data;

    data.idx = idx;
    data.name = NULL;
    if (!ossl_namemap_doall_names(namemap, number, do_num2name, &data))
        return NULL;
    return data.name;
}

static int namemap_add_name_n(OSSL_NAMEMAP *namemap, int number,
                              const char *name, size_t name_len)
{
    NAMENUM_ENTRY *namenum = NULL;
    int tmp_number;

    /* If it already exists, we don't add it */
    if ((tmp_number = namemap_name2num_n(namemap, name, name_len)) != 0)
        return tmp_number;

    if ((namenum = OPENSSL_zalloc(sizeof(*namenum))) == NULL
        || (namenum->name = OPENSSL_strndup(name, name_len)) == NULL)
        goto err;

    /* The tsan_counter use here is safe since we're under lock */
    namenum->number =
        number != 0 ? number : 1 + tsan_counter(&namemap->max_number);
    (void)lh_NAMENUM_ENTRY_insert(namemap->namenum, namenum);

    if (lh_NAMENUM_ENTRY_error(namemap->namenum))
        goto err;
    return namenum->number;

 err:
    namenum_free(namenum);
    return 0;
}

int ossl_namemap_add_name_n(OSSL_NAMEMAP *namemap, int number,
                            const char *name, size_t name_len)
{
    int tmp_number;

#ifndef FIPS_MODULE
    if (namemap == NULL)
        namemap = ossl_namemap_stored(NULL);
#endif

    if (name == NULL || name_len == 0 || namemap == NULL)
        return 0;

    if (!CRYPTO_THREAD_write_lock(namemap->lock))
        return 0;
    tmp_number = namemap_add_name_n(namemap, number, name, name_len);
    CRYPTO_THREAD_unlock(namemap->lock);
    return tmp_number;
}

int ossl_namemap_add_name(OSSL_NAMEMAP *namemap, int number, const char *name)
{
    if (name == NULL)
        return 0;

    return ossl_namemap_add_name_n(namemap, number, name, strlen(name));
}

int ossl_namemap_add_names(OSSL_NAMEMAP *namemap, int number,
                           const char *names, const char separator)
{
    const char *p, *q;
    size_t l;

    /* Check that we have a namemap */
    if (!ossl_assert(namemap != NULL)) {
        ERR_raise(ERR_LIB_CRYPTO, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    if (!CRYPTO_THREAD_write_lock(namemap->lock))
        return 0;
    /*
     * Check that no name is an empty string, and that all names have at
     * most one numeric identity together.
     */
    for (p = names; *p != '\0'; p = (q == NULL ? p + l : q + 1)) {
        int this_number;

        if ((q = strchr(p, separator)) == NULL)
            l = strlen(p);       /* offset to \0 */
        else
            l = q - p;           /* offset to the next separator */

        this_number = namemap_name2num_n(namemap, p, l);

        if (*p == '\0' || *p == separator) {
            ERR_raise(ERR_LIB_CRYPTO, CRYPTO_R_BAD_ALGORITHM_NAME);
            goto err;
        }
        if (number == 0) {
            number = this_number;
        } else if (this_number != 0 && this_number != number) {
            ERR_raise_data(ERR_LIB_CRYPTO, CRYPTO_R_CONFLICTING_NAMES,
                           "\"%.*s\" has an existing different identity %d (from \"%s\")",
                           l, p, this_number, names);
            goto err;
        }
    }

    /* Now that we have checked, register all names */
    for (p = names; *p != '\0'; p = (q == NULL ? p + l : q + 1)) {
        int this_number;

        if ((q = strchr(p, separator)) == NULL)
            l = strlen(p);       /* offset to \0 */
        else
            l = q - p;           /* offset to the next separator */

        this_number = namemap_add_name_n(namemap, number, p, l);
        if (number == 0) {
            number = this_number;
        } else if (this_number != number) {
            ERR_raise_data(ERR_LIB_CRYPTO, ERR_R_INTERNAL_ERROR,
                           "Got number %d when expecting %d",
                           this_number, number);
            goto err;
        }
    }

    CRYPTO_THREAD_unlock(namemap->lock);
    return number;

 err:
    CRYPTO_THREAD_unlock(namemap->lock);
    return 0;
}

/*-
 * Pre-population
 * ==============
 */

#ifndef FIPS_MODULE
#include <openssl/evp.h>

/* Creates an initial namemap with names found in the legacy method db */
static void get_legacy_evp_names(int base_nid, int nid, const char *pem_name,
                                 void *arg)
{
    int num = 0;
    ASN1_OBJECT *obj;

    if (base_nid != NID_undef) {
        num = ossl_namemap_add_name(arg, num, OBJ_nid2sn(base_nid));
        num = ossl_namemap_add_name(arg, num, OBJ_nid2ln(base_nid));
    }

    if (nid != NID_undef) {
        num = ossl_namemap_add_name(arg, num, OBJ_nid2sn(nid));
        num = ossl_namemap_add_name(arg, num, OBJ_nid2ln(nid));
        if ((obj = OBJ_nid2obj(nid)) != NULL) {
            char txtoid[OSSL_MAX_NAME_SIZE];

            if (OBJ_obj2txt(txtoid, sizeof(txtoid), obj, 1) > 0)
                num = ossl_namemap_add_name(arg, num, txtoid);
        }
    }
    if (pem_name != NULL)
        num = ossl_namemap_add_name(arg, num, pem_name);
}

static void get_legacy_cipher_names(const OBJ_NAME *on, void *arg)
{
    const EVP_CIPHER *cipher = (void *)OBJ_NAME_get(on->name, on->type);

    if (cipher != NULL)
        get_legacy_evp_names(NID_undef, EVP_CIPHER_get_type(cipher), NULL, arg);
}

static void get_legacy_md_names(const OBJ_NAME *on, void *arg)
{
    const EVP_MD *md = (void *)OBJ_NAME_get(on->name, on->type);

    if (md != NULL)
        get_legacy_evp_names(0, EVP_MD_get_type(md), NULL, arg);
}

static void get_legacy_pkey_meth_names(const EVP_PKEY_ASN1_METHOD *ameth,
                                       void *arg)
{
    int nid = 0, base_nid = 0, flags = 0;
    const char *pem_name = NULL;

    EVP_PKEY_asn1_get0_info(&nid, &base_nid, &flags, NULL, &pem_name, ameth);
    if (nid != NID_undef) {
        if ((flags & ASN1_PKEY_ALIAS) == 0) {
            switch (nid) {
            case EVP_PKEY_DHX:
                /* We know that the name "DHX" is used too */
                get_legacy_evp_names(0, nid, "DHX", arg);
                /* FALLTHRU */
            default:
                get_legacy_evp_names(0, nid, pem_name, arg);
            }
        } else {
            /*
             * Treat aliases carefully, some of them are undesirable, or
             * should not be treated as such for providers.
             */

            switch (nid) {
            case EVP_PKEY_SM2:
                /*
                 * SM2 is a separate keytype with providers, not an alias for
                 * EC.
                 */
                get_legacy_evp_names(0, nid, pem_name, arg);
                break;
            default:
                /* Use the short name of the base nid as the common reference */
                get_legacy_evp_names(base_nid, nid, pem_name, arg);
            }
        }
    }
}
#endif

/*-
 * Constructors / destructors
 * ==========================
 */

OSSL_NAMEMAP *ossl_namemap_stored(OSSL_LIB_CTX *libctx)
{
#ifndef FIPS_MODULE
    int nms;
#endif
    OSSL_NAMEMAP *namemap =
        ossl_lib_ctx_get_data(libctx, OSSL_LIB_CTX_NAMEMAP_INDEX,
                              &stored_namemap_method);

    if (namemap == NULL)
        return NULL;

#ifndef FIPS_MODULE
    nms = ossl_namemap_empty(namemap);
    if (nms < 0) {
        /*
         * Could not get lock to make the count, so maybe internal objects
         * weren't added. This seems safest.
         */
        return NULL;
    }
    if (nms == 1) {
        int i, end;

        /* Before pilfering, we make sure the legacy database is populated */
        OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS
                            | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);

        OBJ_NAME_do_all(OBJ_NAME_TYPE_CIPHER_METH,
                        get_legacy_cipher_names, namemap);
        OBJ_NAME_do_all(OBJ_NAME_TYPE_MD_METH,
                        get_legacy_md_names, namemap);

        /* We also pilfer data from the legacy EVP_PKEY_ASN1_METHODs */
        for (i = 0, end = EVP_PKEY_asn1_get_count(); i < end; i++)
            get_legacy_pkey_meth_names(EVP_PKEY_asn1_get0(i), namemap);
    }
#endif

    return namemap;
}

OSSL_NAMEMAP *ossl_namemap_new(void)
{
    OSSL_NAMEMAP *namemap;

    if ((namemap = OPENSSL_zalloc(sizeof(*namemap))) != NULL
        && (namemap->lock = CRYPTO_THREAD_lock_new()) != NULL
        && (namemap->namenum =
            lh_NAMENUM_ENTRY_new(namenum_hash, namenum_cmp)) != NULL)
        return namemap;

    ossl_namemap_free(namemap);
    return NULL;
}

void ossl_namemap_free(OSSL_NAMEMAP *namemap)
{
    if (namemap == NULL || namemap->stored)
        return;

    lh_NAMENUM_ENTRY_doall(namemap->namenum, namenum_free);
    lh_NAMENUM_ENTRY_free(namemap->namenum);

    CRYPTO_THREAD_lock_free(namemap->lock);
    OPENSSL_free(namemap);
}

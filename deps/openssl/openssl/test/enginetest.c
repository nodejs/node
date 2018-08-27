/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <openssl/e_os2.h>

#ifdef OPENSSL_NO_ENGINE
int main(int argc, char *argv[])
{
    printf("No ENGINE support\n");
    return (0);
}
#else
# include <openssl/buffer.h>
# include <openssl/crypto.h>
# include <openssl/engine.h>
# include <openssl/err.h>
# include <openssl/rsa.h>
# include <openssl/bn.h>

static void display_engine_list(void)
{
    ENGINE *h;
    int loop;

    h = ENGINE_get_first();
    loop = 0;
    printf("listing available engine types\n");
    while (h) {
        printf("engine %i, id = \"%s\", name = \"%s\"\n",
               loop++, ENGINE_get_id(h), ENGINE_get_name(h));
        h = ENGINE_get_next(h);
    }
    printf("end of list\n");
    /*
     * ENGINE_get_first() increases the struct_ref counter, so we must call
     * ENGINE_free() to decrease it again
     */
    ENGINE_free(h);
}

/* Test EVP_PKEY method */
static EVP_PKEY_METHOD *test_rsa = NULL;

static int called_encrypt = 0;

/* Test function to check operation has been redirected */
static int test_encrypt(EVP_PKEY_CTX *ctx, unsigned char *sig,
                        size_t *siglen, const unsigned char *tbs, size_t tbslen)
{
    called_encrypt = 1;
    return 1;
}

static int test_pkey_meths(ENGINE *e, EVP_PKEY_METHOD **pmeth,
                           const int **pnids, int nid)
{
    static const int rnid = EVP_PKEY_RSA;
    if (pmeth == NULL) {
        *pnids = &rnid;
        return 1;
    }

    if (nid == EVP_PKEY_RSA) {
        *pmeth = test_rsa;
        return 1;
    }

    *pmeth = NULL;
    return 0;
}

/* Return a test EVP_PKEY value */

static EVP_PKEY *get_test_pkey(void)
{
    static unsigned char n[] =
        "\x00\xAA\x36\xAB\xCE\x88\xAC\xFD\xFF\x55\x52\x3C\x7F\xC4\x52\x3F"
        "\x90\xEF\xA0\x0D\xF3\x77\x4A\x25\x9F\x2E\x62\xB4\xC5\xD9\x9C\xB5"
        "\xAD\xB3\x00\xA0\x28\x5E\x53\x01\x93\x0E\x0C\x70\xFB\x68\x76\x93"
        "\x9C\xE6\x16\xCE\x62\x4A\x11\xE0\x08\x6D\x34\x1E\xBC\xAC\xA0\xA1"
        "\xF5";
    static unsigned char e[] = "\x11";

    RSA *rsa = RSA_new();
    EVP_PKEY *pk = EVP_PKEY_new();

    if (rsa == NULL || pk == NULL || !EVP_PKEY_assign_RSA(pk, rsa)) {
        RSA_free(rsa);
        EVP_PKEY_free(pk);
        return NULL;
    }

    if (!RSA_set0_key(rsa, BN_bin2bn(n, sizeof(n)-1, NULL),
                      BN_bin2bn(e, sizeof(e)-1, NULL), NULL)) {
        EVP_PKEY_free(pk);
        return NULL;
    }

    return pk;
}

static int test_redirect(void)
{
    const unsigned char pt[] = "Hello World\n";
    unsigned char *tmp = NULL;
    size_t len;
    EVP_PKEY_CTX *ctx = NULL;
    ENGINE *e = NULL;
    EVP_PKEY *pkey = NULL;

    int to_return = 0;

    printf("\nRedirection test\n");

    if ((pkey = get_test_pkey()) == NULL) {
        printf("Get test key failed\n");
        goto err;
    }

    len = EVP_PKEY_size(pkey);
    if ((tmp = OPENSSL_malloc(len)) == NULL) {
        printf("Buffer alloc failed\n");
        goto err;
    }

    if ((ctx = EVP_PKEY_CTX_new(pkey, NULL)) == NULL) {
        printf("Key context allocation failure\n");
        goto err;
    }
    printf("EVP_PKEY_encrypt test: no redirection\n");
    /* Encrypt some data: should succeed but not be redirected */
    if (EVP_PKEY_encrypt_init(ctx) <= 0
            || EVP_PKEY_encrypt(ctx, tmp, &len, pt, sizeof(pt)) <= 0
            || called_encrypt) {
        printf("Test encryption failure\n");
        goto err;
    }
    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    /* Create a test ENGINE */
    if ((e = ENGINE_new()) == NULL
            || !ENGINE_set_id(e, "Test redirect engine")
            || !ENGINE_set_name(e, "Test redirect engine")) {
        printf("Redirection engine setup failure\n");
        goto err;
    }

    /*
     * Try to create a context for this engine and test key.
     * Try setting test key engine. Both should fail because the
     * engine has no public key methods.
     */
    if (EVP_PKEY_CTX_new(pkey, e) != NULL
            || EVP_PKEY_set1_engine(pkey, e) > 0) {
        printf("Unexpected redirection success\n");
        goto err;
    }

    /* Setup an empty test EVP_PKEY_METHOD and set callback to return it */
    if ((test_rsa = EVP_PKEY_meth_new(EVP_PKEY_RSA, 0)) == NULL) {
        printf("Test RSA algorithm setup failure\n");
        goto err;
    }
    ENGINE_set_pkey_meths(e, test_pkey_meths);

    /* Getting a context for test ENGINE should now succeed */
    if ((ctx = EVP_PKEY_CTX_new(pkey, e)) == NULL) {
        printf("Redirected context allocation failed\n");
        goto err;
    }
    /* Encrypt should fail because operation is not supported */
    if (EVP_PKEY_encrypt_init(ctx) > 0) {
        printf("Encryption redirect unexpected success\n");
        goto err;
    }
    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    /* Add test encrypt operation to method */
    EVP_PKEY_meth_set_encrypt(test_rsa, 0, test_encrypt);

    printf("EVP_PKEY_encrypt test: redirection via EVP_PKEY_CTX_new()\n");
    if ((ctx = EVP_PKEY_CTX_new(pkey, e)) == NULL) {
        printf("Redirected context allocation failed\n");
        goto err;
    }
    /* Encrypt some data: should succeed and be redirected */
    if (EVP_PKEY_encrypt_init(ctx) <= 0
            || EVP_PKEY_encrypt(ctx, tmp, &len, pt, sizeof(pt)) <= 0
            || !called_encrypt) {
        printf("Redirected key context encryption failed\n");
        goto err;
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;
    called_encrypt = 0;

    printf("EVP_PKEY_encrypt test: check default operation not redirected\n");

    /* Create context with default engine: should not be redirected */
    if ((ctx = EVP_PKEY_CTX_new(pkey, NULL)) == NULL
            || EVP_PKEY_encrypt_init(ctx) <= 0
            || EVP_PKEY_encrypt(ctx, tmp, &len, pt, sizeof(pt)) <= 0
            || called_encrypt) {
        printf("Unredirected key context encryption failed\n");
        goto err;
    }

    EVP_PKEY_CTX_free(ctx);
    ctx = NULL;

    /* Set engine explicitly for test key */
    if (!EVP_PKEY_set1_engine(pkey, e)) {
        printf("Key engine set failed\n");
        goto err;
    }

    printf("EVP_PKEY_encrypt test: redirection via EVP_PKEY_set1_engine()\n");

    /* Create context with default engine: should be redirected now */
    if ((ctx = EVP_PKEY_CTX_new(pkey, NULL)) == NULL
            || EVP_PKEY_encrypt_init(ctx) <= 0
            || EVP_PKEY_encrypt(ctx, tmp, &len, pt, sizeof(pt)) <= 0
            || !called_encrypt) {
        printf("Key redirection failure\n");
        goto err;
    }

    to_return = 1;

 err:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    ENGINE_free(e);
    OPENSSL_free(tmp);
    return to_return;
}

int main(int argc, char *argv[])
{
    ENGINE *block[512];
    char buf[256];
    const char *id, *name, *p;
    ENGINE *ptr;
    int loop;
    int to_return = 1;
    ENGINE *new_h1 = NULL;
    ENGINE *new_h2 = NULL;
    ENGINE *new_h3 = NULL;
    ENGINE *new_h4 = NULL;

    p = getenv("OPENSSL_DEBUG_MEMORY");
    if (p != NULL && strcmp(p, "on") == 0)
        CRYPTO_set_mem_debug(1);

    memset(block, 0, sizeof(block));
    if (((new_h1 = ENGINE_new()) == NULL) ||
        !ENGINE_set_id(new_h1, "test_id0") ||
        !ENGINE_set_name(new_h1, "First test item") ||
        ((new_h2 = ENGINE_new()) == NULL) ||
        !ENGINE_set_id(new_h2, "test_id1") ||
        !ENGINE_set_name(new_h2, "Second test item") ||
        ((new_h3 = ENGINE_new()) == NULL) ||
        !ENGINE_set_id(new_h3, "test_id2") ||
        !ENGINE_set_name(new_h3, "Third test item") ||
        ((new_h4 = ENGINE_new()) == NULL) ||
        !ENGINE_set_id(new_h4, "test_id3") ||
        !ENGINE_set_name(new_h4, "Fourth test item")) {
        printf("Couldn't set up test ENGINE structures\n");
        goto end;
    }
    printf("\nenginetest beginning\n\n");
    display_engine_list();
    if (!ENGINE_add(new_h1)) {
        printf("Add failed!\n");
        goto end;
    }
    display_engine_list();
    ptr = ENGINE_get_first();
    if (!ENGINE_remove(ptr)) {
        printf("Remove failed!\n");
        goto end;
    }
    ENGINE_free(ptr);
    display_engine_list();
    if (!ENGINE_add(new_h3) || !ENGINE_add(new_h2)) {
        printf("Add failed!\n");
        goto end;
    }
    display_engine_list();
    if (!ENGINE_remove(new_h2)) {
        printf("Remove failed!\n");
        goto end;
    }
    display_engine_list();
    if (!ENGINE_add(new_h4)) {
        printf("Add failed!\n");
        goto end;
    }
    display_engine_list();
    if (ENGINE_add(new_h3)) {
        printf("Add *should* have failed but didn't!\n");
        goto end;
    } else
        printf("Add that should fail did.\n");
    ERR_clear_error();
    if (ENGINE_remove(new_h2)) {
        printf("Remove *should* have failed but didn't!\n");
        goto end;
    } else
        printf("Remove that should fail did.\n");
    ERR_clear_error();
    if (!ENGINE_remove(new_h3)) {
        printf("Remove failed!\n");
        goto end;
    }
    display_engine_list();
    if (!ENGINE_remove(new_h4)) {
        printf("Remove failed!\n");
        goto end;
    }
    display_engine_list();
    /*
     * Depending on whether there's any hardware support compiled in, this
     * remove may be destined to fail.
     */
    ptr = ENGINE_get_first();
    if (ptr)
        if (!ENGINE_remove(ptr))
            printf("Remove failed!i - probably no hardware "
                   "support present.\n");
    ENGINE_free(ptr);
    display_engine_list();
    if (!ENGINE_add(new_h1) || !ENGINE_remove(new_h1)) {
        printf("Couldn't add and remove to an empty list!\n");
        goto end;
    } else
        printf("Successfully added and removed to an empty list!\n");
    printf("About to beef up the engine-type list\n");
    for (loop = 0; loop < 512; loop++) {
        sprintf(buf, "id%i", loop);
        id = OPENSSL_strdup(buf);
        sprintf(buf, "Fake engine type %i", loop);
        name = OPENSSL_strdup(buf);
        if (((block[loop] = ENGINE_new()) == NULL) ||
            !ENGINE_set_id(block[loop], id) ||
            !ENGINE_set_name(block[loop], name)) {
            printf("Couldn't create block of ENGINE structures.\n"
                   "I'll probably also core-dump now, damn.\n");
            goto end;
        }
    }
    for (loop = 0; loop < 512; loop++) {
        if (!ENGINE_add(block[loop])) {
            printf("\nAdding stopped at %i, (%s,%s)\n",
                   loop, ENGINE_get_id(block[loop]),
                   ENGINE_get_name(block[loop]));
            goto cleanup_loop;
        } else
            printf(".");
        fflush(stdout);
    }
 cleanup_loop:
    printf("\nAbout to empty the engine-type list\n");
    while ((ptr = ENGINE_get_first()) != NULL) {
        if (!ENGINE_remove(ptr)) {
            printf("\nRemove failed!\n");
            goto end;
        }
        ENGINE_free(ptr);
        printf(".");
        fflush(stdout);
    }
    for (loop = 0; loop < 512; loop++) {
        OPENSSL_free((void *)ENGINE_get_id(block[loop]));
        OPENSSL_free((void *)ENGINE_get_name(block[loop]));
    }
    if (!test_redirect())
        goto end;
    printf("\nTests completed happily\n");
    to_return = 0;
 end:
    if (to_return)
        ERR_print_errors_fp(stderr);
    ENGINE_free(new_h1);
    ENGINE_free(new_h2);
    ENGINE_free(new_h3);
    ENGINE_free(new_h4);
    for (loop = 0; loop < 512; loop++)
        ENGINE_free(block[loop]);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        to_return = 1;
#endif
    return to_return;
}
#endif

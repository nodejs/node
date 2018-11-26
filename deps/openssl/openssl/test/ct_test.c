/*
 * Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/ct.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include "testutil.h"

#ifndef OPENSSL_NO_CT

/* Used when declaring buffers to read text files into */
#define CT_TEST_MAX_FILE_SIZE 8096

static char *certs_dir = NULL;
static char *ct_dir = NULL;

typedef struct ct_test_fixture {
    const char *test_case_name;
    /* The current time in milliseconds */
    uint64_t epoch_time_in_ms;
    /* The CT log store to use during tests */
    CTLOG_STORE* ctlog_store;
    /* Set the following to test handling of SCTs in X509 certificates */
    const char *certs_dir;
    char *certificate_file;
    char *issuer_file;
    /* Expected number of SCTs */
    int expected_sct_count;
    /* Expected number of valid SCTS */
    int expected_valid_sct_count;
    /* Set the following to test handling of SCTs in TLS format */
    const unsigned char *tls_sct_list;
    size_t tls_sct_list_len;
    STACK_OF(SCT) *sct_list;
    /*
     * A file to load the expected SCT text from.
     * This text will be compared to the actual text output during the test.
     * A maximum of |CT_TEST_MAX_FILE_SIZE| bytes will be read of this file.
     */
    const char *sct_dir;
    const char *sct_text_file;
    /* Whether to test the validity of the SCT(s) */
    int test_validity;
} CT_TEST_FIXTURE;

static CT_TEST_FIXTURE set_up(const char *const test_case_name)
{
    CT_TEST_FIXTURE fixture;
    int setup_ok = 1;

    memset(&fixture, 0, sizeof(fixture));

    fixture.test_case_name = test_case_name;
    fixture.epoch_time_in_ms = 1473269626000; /* Sep 7 17:33:46 2016 GMT */
    fixture.ctlog_store = CTLOG_STORE_new();

    if (fixture.ctlog_store == NULL) {
        setup_ok = 0;
        fprintf(stderr, "Failed to create a new CT log store\n");
        goto end;
    }

    if (CTLOG_STORE_load_default_file(fixture.ctlog_store) != 1) {
        setup_ok = 0;
        fprintf(stderr, "Failed to load CT log list\n");
        goto end;
    }

end:
    if (!setup_ok) {
        CTLOG_STORE_free(fixture.ctlog_store);
        exit(EXIT_FAILURE);
    }
    return fixture;
}

static void tear_down(CT_TEST_FIXTURE fixture)
{
    CTLOG_STORE_free(fixture.ctlog_store);
    SCT_LIST_free(fixture.sct_list);
    ERR_print_errors_fp(stderr);
}

static char *mk_file_path(const char *dir, const char *file)
{
    char *full_file = NULL;
    size_t full_file_l = 0;
    const char *sep = "";
#ifndef OPENSSL_SYS_VMS
    sep = "/";
#endif

    full_file_l = strlen(dir) + strlen(sep) + strlen(file) + 1;
    full_file = OPENSSL_zalloc(full_file_l);
    if (full_file != NULL) {
        OPENSSL_strlcpy(full_file, dir, full_file_l);
        OPENSSL_strlcat(full_file, sep, full_file_l);
        OPENSSL_strlcat(full_file, file, full_file_l);
    }

    return full_file;
}

static X509 *load_pem_cert(const char *dir, const char *file)
{
    X509 *cert = NULL;
    char *file_path = mk_file_path(dir, file);

    if (file_path != NULL) {
        BIO *cert_io = BIO_new_file(file_path, "r");
        OPENSSL_free(file_path);

        if (cert_io != NULL)
            cert = PEM_read_bio_X509(cert_io, NULL, NULL, NULL);

        BIO_free(cert_io);
    }
    return cert;
}

static int read_text_file(const char *dir, const char *file,
                          char *buffer, int buffer_length)
{
    int result = -1;
    char *file_path = mk_file_path(dir, file);

    if (file_path != NULL) {
        BIO *file_io = BIO_new_file(file_path, "r");
        OPENSSL_free(file_path);

        if (file_io != NULL) {
            result = BIO_read(file_io, buffer, buffer_length);
            BIO_free(file_io);
        }
    }

    return result;
}

static int compare_sct_list_printout(STACK_OF(SCT) *sct,
    const char *expected_output)
{
    BIO *text_buffer = NULL;
    char *actual_output = NULL;
    int result = 1;

    text_buffer = BIO_new(BIO_s_mem());
    if (text_buffer == NULL) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto end;
    }

    SCT_LIST_print(sct, text_buffer, 0, "\n", NULL);

    /* Append null terminator because we're about to use the buffer contents
    * as a string. */
    if (BIO_write(text_buffer, "\0", 1) != 1) {
        fprintf(stderr, "Failed to append null terminator to SCT text\n");
        goto end;
    }

    BIO_get_mem_data(text_buffer, &actual_output);
    result = strcmp(actual_output, expected_output);

    if (result != 0) {
        fprintf(stderr,
            "Expected SCT printout:\n%s\nActual SCT printout:\n%s\n",
            expected_output, actual_output);
    }

end:
    BIO_free(text_buffer);
    return result;
}

static int compare_extension_printout(X509_EXTENSION *extension,
                                      const char *expected_output)
{
    BIO *text_buffer = NULL;
    char *actual_output = NULL;
    int result = 1;

    text_buffer = BIO_new(BIO_s_mem());
    if (text_buffer == NULL) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto end;
    }

    if (!X509V3_EXT_print(text_buffer, extension, X509V3_EXT_DEFAULT, 0)) {
        fprintf(stderr, "Failed to print extension\n");
        goto end;
    }

    /* Append null terminator because we're about to use the buffer contents
     * as a string. */
    if (BIO_write(text_buffer, "\0", 1) != 1) {
        fprintf(stderr,
                "Failed to append null terminator to extension text\n");
        goto end;
    }

    BIO_get_mem_data(text_buffer, &actual_output);
    result = strcmp(actual_output, expected_output);

    if (result != 0) {
        fprintf(stderr,
                "Expected SCT printout:\n%s\nActual SCT printout:\n%s\n",
                expected_output, actual_output);
    }

end:
    BIO_free(text_buffer);
    return result;
}

static int assert_validity(CT_TEST_FIXTURE fixture,
                           STACK_OF(SCT) *scts,
                           CT_POLICY_EVAL_CTX *policy_ctx) {
    int invalid_sct_count = 0;
    int valid_sct_count = 0;
    int i;

    if (SCT_LIST_validate(scts, policy_ctx) < 0) {
        fprintf(stderr, "Error verifying SCTs\n");
        return 0;
    }

    for (i = 0; i < sk_SCT_num(scts); ++i) {
        SCT *sct_i = sk_SCT_value(scts, i);
        switch (SCT_get_validation_status(sct_i)) {
        case SCT_VALIDATION_STATUS_VALID:
            ++valid_sct_count;
            break;
        case SCT_VALIDATION_STATUS_INVALID:
            ++invalid_sct_count;
            break;
        default:
            /* Ignore other validation statuses. */
            break;
        }
    }

    if (valid_sct_count != fixture.expected_valid_sct_count) {
        int unverified_sct_count = sk_SCT_num(scts) -
                invalid_sct_count - valid_sct_count;

        fprintf(stderr,
                "%d SCTs failed verification\n"
                "%d SCTs passed verification (%d expected)\n"
                "%d SCTs were unverified\n",
                invalid_sct_count,
                valid_sct_count,
                fixture.expected_valid_sct_count,
                unverified_sct_count);
        return 0;
    }

    return 1;
}

static int execute_cert_test(CT_TEST_FIXTURE fixture)
{
    int success = 0;
    X509 *cert = NULL, *issuer = NULL;
    STACK_OF(SCT) *scts = NULL;
    SCT *sct = NULL;
    char expected_sct_text[CT_TEST_MAX_FILE_SIZE];
    int sct_text_len = 0;
    unsigned char *tls_sct_list = NULL;
    size_t tls_sct_list_len = 0;
    CT_POLICY_EVAL_CTX *ct_policy_ctx = CT_POLICY_EVAL_CTX_new();

    if (fixture.sct_text_file != NULL) {
        sct_text_len = read_text_file(fixture.sct_dir, fixture.sct_text_file,
                                      expected_sct_text,
                                      CT_TEST_MAX_FILE_SIZE - 1);

        if (sct_text_len < 0) {
            fprintf(stderr, "Test data file not found: %s\n",
                fixture.sct_text_file);
            goto end;
        }

        expected_sct_text[sct_text_len] = '\0';
    }

    CT_POLICY_EVAL_CTX_set_shared_CTLOG_STORE(
            ct_policy_ctx, fixture.ctlog_store);

    CT_POLICY_EVAL_CTX_set_time(ct_policy_ctx, fixture.epoch_time_in_ms);

    if (fixture.certificate_file != NULL) {
        int sct_extension_index;
        X509_EXTENSION *sct_extension = NULL;
        cert = load_pem_cert(fixture.certs_dir, fixture.certificate_file);

        if (cert == NULL) {
            fprintf(stderr, "Unable to load certificate: %s\n",
                fixture.certificate_file);
            goto end;
        }

        CT_POLICY_EVAL_CTX_set1_cert(ct_policy_ctx, cert);

        if (fixture.issuer_file != NULL) {
            issuer = load_pem_cert(fixture.certs_dir, fixture.issuer_file);

            if (issuer == NULL) {
                fprintf(stderr, "Unable to load issuer certificate: %s\n",
                        fixture.issuer_file);
                goto end;
            }

            CT_POLICY_EVAL_CTX_set1_issuer(ct_policy_ctx, issuer);
        }

        sct_extension_index =
                X509_get_ext_by_NID(cert, NID_ct_precert_scts, -1);
        sct_extension = X509_get_ext(cert, sct_extension_index);
        if (fixture.expected_sct_count > 0) {
            if (sct_extension == NULL) {
                fprintf(stderr, "SCT extension not found in: %s\n",
                    fixture.certificate_file);
                goto end;
            }

            if (fixture.sct_text_file
                && compare_extension_printout(sct_extension,
                                              expected_sct_text)) {
                    goto end;
            }

            if (fixture.test_validity) {
                int i;

                scts = X509V3_EXT_d2i(sct_extension);
                for (i = 0; i < sk_SCT_num(scts); ++i) {
                    SCT *sct_i = sk_SCT_value(scts, i);

                    if (!SCT_set_source(sct_i, SCT_SOURCE_X509V3_EXTENSION)) {
                        fprintf(stderr,
                                "Error setting SCT source to X509v3 extension\n");
                        goto end;
                    }
                }

                if (!assert_validity(fixture, scts, ct_policy_ctx))
                    goto end;
            }
        } else if (sct_extension != NULL) {
            fprintf(stderr,
                    "Expected no SCTs, but found SCT extension in: %s\n",
                    fixture.certificate_file);
            goto end;
        }
    }

    if (fixture.tls_sct_list != NULL) {
        const unsigned char *p = fixture.tls_sct_list;
        if (o2i_SCT_LIST(&scts, &p, fixture.tls_sct_list_len) == NULL) {
            fprintf(stderr, "Failed to decode SCTs from TLS format\n");
            goto end;
        }

        if (fixture.test_validity && cert != NULL) {
            if (!assert_validity(fixture, scts, ct_policy_ctx))
                goto end;
        }

        if (fixture.sct_text_file
            && compare_sct_list_printout(scts, expected_sct_text)) {
                goto end;
        }

        tls_sct_list_len = i2o_SCT_LIST(scts, &tls_sct_list);
        if (tls_sct_list_len != fixture.tls_sct_list_len ||
            memcmp(fixture.tls_sct_list, tls_sct_list, tls_sct_list_len) != 0) {
            fprintf(stderr,
                    "Failed to encode SCTs into TLS format correctly\n");
            goto end;
        }
    }
    success = 1;

end:
    X509_free(cert);
    X509_free(issuer);
    SCT_LIST_free(scts);
    SCT_free(sct);
    CT_POLICY_EVAL_CTX_free(ct_policy_ctx);
    OPENSSL_free(tls_sct_list);
    return success;
}

#define SETUP_CT_TEST_FIXTURE() SETUP_TEST_FIXTURE(CT_TEST_FIXTURE, set_up)
#define EXECUTE_CT_TEST() EXECUTE_TEST(execute_cert_test, tear_down)

static int test_no_scts_in_certificate()
{
    SETUP_CT_TEST_FIXTURE();
    fixture.certs_dir = certs_dir;
    fixture.certificate_file = "leaf.pem";
    fixture.issuer_file = "subinterCA.pem";
    fixture.expected_sct_count = 0;
    EXECUTE_CT_TEST();
}

static int test_one_sct_in_certificate()
{
    SETUP_CT_TEST_FIXTURE();
    fixture.certs_dir = certs_dir;
    fixture.certificate_file = "embeddedSCTs1.pem";
    fixture.issuer_file = "embeddedSCTs1_issuer.pem";
    fixture.expected_sct_count = 1;
    fixture.sct_dir = certs_dir;
    fixture.sct_text_file = "embeddedSCTs1.sct";
    EXECUTE_CT_TEST();
}

static int test_multiple_scts_in_certificate()
{
    SETUP_CT_TEST_FIXTURE();
    fixture.certs_dir = certs_dir;
    fixture.certificate_file = "embeddedSCTs3.pem";
    fixture.issuer_file = "embeddedSCTs3_issuer.pem";
    fixture.expected_sct_count = 3;
    fixture.sct_dir = certs_dir;
    fixture.sct_text_file = "embeddedSCTs3.sct";
    EXECUTE_CT_TEST();
}

static int test_verify_one_sct()
{
    SETUP_CT_TEST_FIXTURE();
    fixture.certs_dir = certs_dir;
    fixture.certificate_file = "embeddedSCTs1.pem";
    fixture.issuer_file = "embeddedSCTs1_issuer.pem";
    fixture.expected_sct_count = fixture.expected_valid_sct_count = 1;
    fixture.test_validity = 1;
    EXECUTE_CT_TEST();
}

static int test_verify_multiple_scts()
{
    SETUP_CT_TEST_FIXTURE();
    fixture.certs_dir = certs_dir;
    fixture.certificate_file = "embeddedSCTs3.pem";
    fixture.issuer_file = "embeddedSCTs3_issuer.pem";
    fixture.expected_sct_count = fixture.expected_valid_sct_count = 3;
    fixture.test_validity = 1;
    EXECUTE_CT_TEST();
}

static int test_verify_fails_for_future_sct()
{
    SETUP_CT_TEST_FIXTURE();
    fixture.epoch_time_in_ms = 1365094800000; /* Apr 4 17:00:00 2013 GMT */
    fixture.certs_dir = certs_dir;
    fixture.certificate_file = "embeddedSCTs1.pem";
    fixture.issuer_file = "embeddedSCTs1_issuer.pem";
    fixture.expected_sct_count = 1;
    fixture.expected_valid_sct_count = 0;
    fixture.test_validity = 1;
    EXECUTE_CT_TEST();
}

static int test_decode_tls_sct()
{
    const unsigned char tls_sct_list[] = "\x00\x78" /* length of list */
        "\x00\x76"
        "\x00" /* version */
        /* log ID */
        "\xDF\x1C\x2E\xC1\x15\x00\x94\x52\x47\xA9\x61\x68\x32\x5D\xDC\x5C\x79"
        "\x59\xE8\xF7\xC6\xD3\x88\xFC\x00\x2E\x0B\xBD\x3F\x74\xD7\x64"
        "\x00\x00\x01\x3D\xDB\x27\xDF\x93" /* timestamp */
        "\x00\x00" /* extensions length */
        "" /* extensions */
        "\x04\x03" /* hash and signature algorithms */
        "\x00\x47" /* signature length */
        /* signature */
        "\x30\x45\x02\x20\x48\x2F\x67\x51\xAF\x35\xDB\xA6\x54\x36\xBE\x1F\xD6"
        "\x64\x0F\x3D\xBF\x9A\x41\x42\x94\x95\x92\x45\x30\x28\x8F\xA3\xE5\xE2"
        "\x3E\x06\x02\x21\x00\xE4\xED\xC0\xDB\x3A\xC5\x72\xB1\xE2\xF5\xE8\xAB"
        "\x6A\x68\x06\x53\x98\x7D\xCF\x41\x02\x7D\xFE\xFF\xA1\x05\x51\x9D\x89"
        "\xED\xBF\x08";

    SETUP_CT_TEST_FIXTURE();
    fixture.tls_sct_list = tls_sct_list;
    fixture.tls_sct_list_len = 0x7a;
    fixture.sct_dir = ct_dir;
    fixture.sct_text_file = "tls1.sct";
    EXECUTE_CT_TEST();
}

static int test_encode_tls_sct()
{
    const char log_id[] = "3xwuwRUAlFJHqWFoMl3cXHlZ6PfG04j8AC4LvT9012Q=";
    const uint64_t timestamp = 1;
    const char extensions[] = "";
    const char signature[] = "BAMARzBAMiBIL2dRrzXbplQ2vh/WZA89v5pBQpSVkkUwKI+j5"
            "eI+BgIhAOTtwNs6xXKx4vXoq2poBlOYfc9BAn3+/6EFUZ2J7b8I";
    SCT *sct = NULL;

    SETUP_CT_TEST_FIXTURE();

    fixture.sct_list = sk_SCT_new_null();
    sct = SCT_new_from_base64(SCT_VERSION_V1, log_id,
                              CT_LOG_ENTRY_TYPE_X509, timestamp,
                              extensions, signature);

    if (sct == NULL) {
        tear_down(fixture);
        fprintf(stderr, "Failed to create SCT from base64-encoded test data\n");
        return 0;
    }

    sk_SCT_push(fixture.sct_list, sct);
    fixture.sct_dir = ct_dir;
    fixture.sct_text_file = "tls1.sct";
    EXECUTE_CT_TEST();
}

/*
 * Tests that the CT_POLICY_EVAL_CTX default time is approximately now.
 * Allow +-10 minutes, as it may compensate for clock skew.
 */
static int test_default_ct_policy_eval_ctx_time_is_now()
{
    int success = 0;
    CT_POLICY_EVAL_CTX *ct_policy_ctx = CT_POLICY_EVAL_CTX_new();
    const time_t default_time =
        (time_t)(CT_POLICY_EVAL_CTX_get_time(ct_policy_ctx) / 1000);
    const time_t time_tolerance = 600;  /* 10 minutes */

    if (fabs(difftime(time(NULL), default_time)) > time_tolerance) {
        fprintf(stderr,
                "Default CT_POLICY_EVAL_CTX time is not approximately now.\n");
        goto end;
    }

    success = 1;
end:
    CT_POLICY_EVAL_CTX_free(ct_policy_ctx);
    return success;
}

int main(int argc, char *argv[])
{
    int result = 0;
    char *tmp_env = NULL;

    tmp_env = getenv("OPENSSL_DEBUG_MEMORY");
    if (tmp_env != NULL && strcmp(tmp_env, "on") == 0)
        CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    tmp_env = getenv("CT_DIR");
    ct_dir = OPENSSL_strdup(tmp_env != NULL ? tmp_env : "ct");
    tmp_env = getenv("CERTS_DIR");
    certs_dir = OPENSSL_strdup(tmp_env != NULL ? tmp_env : "certs");

    ADD_TEST(test_no_scts_in_certificate);
    ADD_TEST(test_one_sct_in_certificate);
    ADD_TEST(test_multiple_scts_in_certificate);
    ADD_TEST(test_verify_one_sct);
    ADD_TEST(test_verify_multiple_scts);
    ADD_TEST(test_verify_fails_for_future_sct);
    ADD_TEST(test_decode_tls_sct);
    ADD_TEST(test_encode_tls_sct);
    ADD_TEST(test_default_ct_policy_eval_ctx_time_is_now);

    result = run_tests(argv[0]);
    ERR_print_errors_fp(stderr);

    OPENSSL_free(ct_dir);
    OPENSSL_free(certs_dir);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        result = 1;
#endif

    return result;
}

#else /* OPENSSL_NO_CT */

int main(int argc, char* argv[])
{
    return EXIT_SUCCESS;
}

#endif /* OPENSSL_NO_CT */

#include <string.h>

#include <openssl/cms.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#include "testutil.h"

static X509 *cert = NULL;
static EVP_PKEY *privkey = NULL;

static int test_encrypt_decrypt(void)
{
    int testresult = 0;
    STACK_OF(X509) *certstack = sk_X509_new_null();
    const char *msg = "Hello world";
    BIO *msgbio = BIO_new_mem_buf(msg, strlen(msg));
    BIO *outmsgbio = BIO_new(BIO_s_mem());
    CMS_ContentInfo* content = NULL;
    char buf[80];

    if (!TEST_ptr(certstack) || !TEST_ptr(msgbio) || !TEST_ptr(outmsgbio))
        goto end;

    if (!TEST_int_gt(sk_X509_push(certstack, cert), 0))
        goto end;

    content = CMS_encrypt(certstack, msgbio, EVP_aes_128_cbc(), CMS_TEXT);
    if (!TEST_ptr(content))
        goto end;

    if (!TEST_true(CMS_decrypt(content, privkey, cert, NULL, outmsgbio,
                               CMS_TEXT)))
        goto end;

    /* Check we got the message we first started with */
    if (!TEST_int_eq(BIO_gets(outmsgbio, buf, sizeof(buf)), strlen(msg))
            || !TEST_int_eq(strcmp(buf, msg), 0))
        goto end;

    testresult = 1;
 end:
    sk_X509_free(certstack);
    BIO_free(msgbio);
    BIO_free(outmsgbio);
    CMS_ContentInfo_free(content);

    return testresult;
}

int setup_tests(void)
{
    char *certin = NULL, *privkeyin = NULL;
    BIO *certbio = NULL, *privkeybio = NULL;

    if (!TEST_ptr(certin = test_get_argument(0))
            || !TEST_ptr(privkeyin = test_get_argument(1)))
        return 0;

    certbio = BIO_new_file(certin, "r");
    if (!TEST_ptr(certbio))
        return 0;
    if (!TEST_true(PEM_read_bio_X509(certbio, &cert, NULL, NULL))) {
        BIO_free(certbio);
        return 0;
    }
    BIO_free(certbio);

    privkeybio = BIO_new_file(privkeyin, "r");
    if (!TEST_ptr(privkeybio)) {
        X509_free(cert);
        cert = NULL;
        return 0;
    }
    if (!TEST_true(PEM_read_bio_PrivateKey(privkeybio, &privkey, NULL, NULL))) {
        BIO_free(privkeybio);
        X509_free(cert);
        cert = NULL;
        return 0;
    }
    BIO_free(privkeybio);

    ADD_TEST(test_encrypt_decrypt);

    return 1;
}

void cleanup_tests(void)
{
    X509_free(cert);
    EVP_PKEY_free(privkey);
}

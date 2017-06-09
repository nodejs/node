/* Simple S/MIME encrypt example */
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL;
    X509 *rcert = NULL;
    STACK_OF(X509) *recips = NULL;
    PKCS7 *p7 = NULL;
    int ret = 1;

    /*
     * On OpenSSL 0.9.9 only:
     * for streaming set PKCS7_STREAM
     */
    int flags = PKCS7_STREAM;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Read in recipient certificate */
    tbio = BIO_new_file("signer.pem", "r");

    if (!tbio)
        goto err;

    rcert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (!rcert)
        goto err;

    /* Create recipient STACK and add recipient cert to it */
    recips = sk_X509_new_null();

    if (!recips || !sk_X509_push(recips, rcert))
        goto err;

    /*
     * sk_X509_pop_free will free up recipient STACK and its contents so set
     * rcert to NULL so it isn't freed up twice.
     */
    rcert = NULL;

    /* Open content being encrypted */

    in = BIO_new_file("encr.txt", "r");

    if (!in)
        goto err;

    /* encrypt content */
    p7 = PKCS7_encrypt(recips, in, EVP_des_ede3_cbc(), flags);

    if (!p7)
        goto err;

    out = BIO_new_file("smencr.txt", "w");
    if (!out)
        goto err;

    /* Write out S/MIME message */
    if (!SMIME_write_PKCS7(out, p7, in, flags))
        goto err;

    ret = 0;

 err:

    if (ret) {
        fprintf(stderr, "Error Encrypting Data\n");
        ERR_print_errors_fp(stderr);
    }

    if (p7)
        PKCS7_free(p7);
    if (rcert)
        X509_free(rcert);
    if (recips)
        sk_X509_pop_free(recips, X509_free);

    if (in)
        BIO_free(in);
    if (out)
        BIO_free(out);
    if (tbio)
        BIO_free(tbio);

    return ret;

}

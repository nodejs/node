/* Simple S/MIME signing example */
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL;
    X509 *rcert = NULL;
    EVP_PKEY *rkey = NULL;
    PKCS7 *p7 = NULL;
    int ret = 1;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Read in recipient certificate and private key */
    tbio = BIO_new_file("signer.pem", "r");

    if (!tbio)
        goto err;

    rcert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    BIO_reset(tbio);

    rkey = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

    if (!rcert || !rkey)
        goto err;

    /* Open content being signed */

    in = BIO_new_file("smencr.txt", "r");

    if (!in)
        goto err;

    /* Sign content */
    p7 = SMIME_read_PKCS7(in, NULL);

    if (!p7)
        goto err;

    out = BIO_new_file("encrout.txt", "w");
    if (!out)
        goto err;

    /* Decrypt S/MIME message */
    if (!PKCS7_decrypt(p7, rkey, rcert, out, 0))
        goto err;

    ret = 0;

 err:

    if (ret) {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }

    if (p7)
        PKCS7_free(p7);
    if (rcert)
        X509_free(rcert);
    if (rkey)
        EVP_PKEY_free(rkey);

    if (in)
        BIO_free(in);
    if (out)
        BIO_free(out);
    if (tbio)
        BIO_free(tbio);

    return ret;

}

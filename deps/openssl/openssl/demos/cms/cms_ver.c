/* Simple S/MIME verification example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
	{
	BIO *in = NULL, *out = NULL, *tbio = NULL, *cont = NULL;
	X509_STORE *st = NULL;
	X509 *cacert = NULL;
	CMS_ContentInfo *cms = NULL;

	int ret = 1;

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	/* Set up trusted CA certificate store */

	st = X509_STORE_new();

	/* Read in CA certificate */
	tbio = BIO_new_file("cacert.pem", "r");

	if (!tbio)
		goto err;

	cacert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

	if (!cacert)
		goto err;

	if (!X509_STORE_add_cert(st, cacert))
		goto err;

	/* Open message being verified */

	in = BIO_new_file("smout.txt", "r");

	if (!in)
		goto err;

	/* parse message */
	cms = SMIME_read_CMS(in, &cont);

	if (!cms)
		goto err;

	/* File to output verified content to */
	out = BIO_new_file("smver.txt", "w");
	if (!out)
		goto err;

	if (!CMS_verify(cms, NULL, st, cont, out, 0))
		{
		fprintf(stderr, "Verification Failure\n");
		goto err;
		}

	fprintf(stderr, "Verification Successful\n");

	ret = 0;

	err:

	if (ret)
		{
		fprintf(stderr, "Error Verifying Data\n");
		ERR_print_errors_fp(stderr);
		}

	if (cms)
		CMS_ContentInfo_free(cms);

	if (cacert)
		X509_free(cacert);

	if (in)
		BIO_free(in);
	if (out)
		BIO_free(out);
	if (tbio)
		BIO_free(tbio);

	return ret;

	}

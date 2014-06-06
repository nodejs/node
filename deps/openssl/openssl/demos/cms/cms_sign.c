/* Simple S/MIME signing example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
	{
	BIO *in = NULL, *out = NULL, *tbio = NULL;
	X509 *scert = NULL;
	EVP_PKEY *skey = NULL;
	CMS_ContentInfo *cms = NULL;
	int ret = 1;

	/* For simple S/MIME signing use CMS_DETACHED.
	 * On OpenSSL 1.0.0 only:
	 * for streaming detached set CMS_DETACHED|CMS_STREAM
	 * for streaming non-detached set CMS_STREAM
	 */
	int flags = CMS_DETACHED|CMS_STREAM;

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	/* Read in signer certificate and private key */
	tbio = BIO_new_file("signer.pem", "r");

	if (!tbio)
		goto err;

	scert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

	BIO_reset(tbio);

	skey = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

	if (!scert || !skey)
		goto err;

	/* Open content being signed */

	in = BIO_new_file("sign.txt", "r");

	if (!in)
		goto err;

	/* Sign content */
	cms = CMS_sign(scert, skey, NULL, in, flags);

	if (!cms)
		goto err;

	out = BIO_new_file("smout.txt", "w");
	if (!out)
		goto err;

	if (!(flags & CMS_STREAM))
		BIO_reset(in);

	/* Write out S/MIME message */
	if (!SMIME_write_CMS(out, cms, in, flags))
		goto err;

	ret = 0;

	err:

	if (ret)
		{
		fprintf(stderr, "Error Signing Data\n");
		ERR_print_errors_fp(stderr);
		}

	if (cms)
		CMS_ContentInfo_free(cms);
	if (scert)
		X509_free(scert);
	if (skey)
		EVP_PKEY_free(skey);

	if (in)
		BIO_free(in);
	if (out)
		BIO_free(out);
	if (tbio)
		BIO_free(tbio);

	return ret;

	}

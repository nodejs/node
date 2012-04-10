/* Simple S/MIME compress example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
	{
	BIO *in = NULL, *out = NULL;
	CMS_ContentInfo *cms = NULL;
	int ret = 1;

	/*
	 * On OpenSSL 0.9.9 only:
	 * for streaming set CMS_STREAM
	 */
	int flags = CMS_STREAM;

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	/* Open content being compressed */

	in = BIO_new_file("comp.txt", "r");

	if (!in)
		goto err;

	/* compress content */
	cms = CMS_compress(in, NID_zlib_compression, flags);

	if (!cms)
		goto err;

	out = BIO_new_file("smcomp.txt", "w");
	if (!out)
		goto err;

	/* Write out S/MIME message */
	if (!SMIME_write_CMS(out, cms, in, flags))
		goto err;

	ret = 0;

	err:

	if (ret)
		{
		fprintf(stderr, "Error Compressing Data\n");
		ERR_print_errors_fp(stderr);
		}

	if (cms)
		CMS_ContentInfo_free(cms);
	if (in)
		BIO_free(in);
	if (out)
		BIO_free(out);

	return ret;

	}

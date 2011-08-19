#include <stdio.h>
#include <openssl/rsa.h>

/* This is a simple program to generate an RSA private key.  It then
 * saves both the public and private key into a char array, then
 * re-reads them.  It saves them as DER encoded binary data.
 */

void callback(stage,count,arg)
int stage,count;
char *arg;
	{
	FILE *out;

	out=(FILE *)arg;
	fprintf(out,"%d",stage);
	if (stage == 3)
		fprintf(out,"\n");
	fflush(out);
	}

main()
	{
	RSA *rsa,*pub_rsa,*priv_rsa;
	int len;
	unsigned char buf[1024],*p;

	rsa=RSA_generate_key(512,RSA_F4,callback,(char *)stdout);

	p=buf;

	/* Save the public key into buffer, we know it will be big enough
	 * but we should really check how much space we need by calling the
	 * i2d functions with a NULL second parameter */
	len=i2d_RSAPublicKey(rsa,&p);
	len+=i2d_RSAPrivateKey(rsa,&p);

	printf("The public and private key are now both in a char array\n");
	printf("and are taking up %d bytes\n",len);

	RSA_free(rsa);

	p=buf;
	pub_rsa=d2i_RSAPublicKey(NULL,&p,(long)len);
	len-=(p-buf);
	priv_rsa=d2i_RSAPrivateKey(NULL,&p,(long)len);

	if ((pub_rsa == NULL) || (priv_rsa == NULL))
		ERR_print_errors_fp(stderr);

	RSA_free(pub_rsa);
	RSA_free(priv_rsa);
	}

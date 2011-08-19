/* NOCW */
/* demos/spkigen.c
 * 18-Mar-1997 - eay - A quick hack :-) 
 * 		version 1.1, it would probably help to save or load the
 *		private key :-)
 */
#include <stdio.h>
#include <stdlib.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

/* The following two don't exist in SSLeay but they are in here as
 * examples */
#define PEM_write_SPKI(fp,x) \
	PEM_ASN1_write((int (*)())i2d_NETSCAPE_SPKI,"SPKI",fp,\
			(char *)x,NULL,NULL,0,NULL)
int SPKI_set_pubkey(NETSCAPE_SPKI *x, EVP_PKEY *pkey);

/* These are defined in the next version of SSLeay */
int EVP_PKEY_assign(EVP_PKEY *pkey, int type,char *key);
#define RSA_F4	0x10001
#define EVP_PKEY_assign_RSA(pkey,rsa) EVP_PKEY_assign((pkey),EVP_PKEY_RSA,\
					(char *)(rsa))

int main(argc,argv)
int argc;
char *argv[];
	{
	RSA *rsa=NULL;
	NETSCAPE_SPKI *spki=NULL;
	EVP_PKEY *pkey=NULL;
	char buf[128];
	int ok=0,i;
	FILE *fp;

	pkey=EVP_PKEY_new();
	 
	if (argc < 2)
		{
		/* Generate an RSA key, the random state should have been seeded
		 * with lots of calls to RAND_seed(....) */
		fprintf(stderr,"generating RSA key, could take some time...\n");
		if ((rsa=RSA_generate_key(512,RSA_F4,NULL)) == NULL) goto err;
		}
	else
		{
		if ((fp=fopen(argv[1],"r")) == NULL)
			{ perror(argv[1]); goto err; }
		if ((rsa=PEM_read_RSAPrivateKey(fp,NULL,NULL)) == NULL)
			goto err;
		fclose(fp);
		}
	
	if (!EVP_PKEY_assign_RSA(pkey,rsa)) goto err;
	rsa=NULL;

	/* lets make the spki and set the public key and challenge */
	if ((spki=NETSCAPE_SPKI_new()) == NULL) goto err;

	if (!SPKI_set_pubkey(spki,pkey)) goto err;

	fprintf(stderr,"please enter challenge string:");
	fflush(stderr);
	buf[0]='\0';
	fgets(buf,sizeof buf,stdin);
	i=strlen(buf);
	if (i > 0) buf[--i]='\0';
	if (!ASN1_STRING_set((ASN1_STRING *)spki->spkac->challenge,
		buf,i)) goto err;

	if (!NETSCAPE_SPKI_sign(spki,pkey,EVP_md5())) goto err;
	PEM_write_SPKI(stdout,spki);
	if (argc < 2)
		PEM_write_RSAPrivateKey(stdout,pkey->pkey.rsa,NULL,NULL,0,NULL);

	ok=1;
err:
	if (!ok)
		{
		fprintf(stderr,"something bad happened....");
		ERR_print_errors_fp(stderr);
		}
	NETSCAPE_SPKI_free(spki);
	EVP_PKEY_free(pkey);
	exit(!ok);
	}

/* This function is in the next version of SSLeay */
int EVP_PKEY_assign(pkey,type,key)
EVP_PKEY *pkey;
int type;
char *key;
	{
	if (pkey == NULL) return(0);
	if (pkey->pkey.ptr != NULL)
		{
		if (pkey->type == EVP_PKEY_RSA)
			RSA_free(pkey->pkey.rsa);
		/* else memory leak */
		}
	pkey->type=type;
	pkey->pkey.ptr=key;
	return(1);
	}

/* While I have a 
 * X509_set_pubkey() and X509_REQ_set_pubkey(), SPKI_set_pubkey() does
 * not currently exist so here is a version of it.
 * The next SSLeay release will probably have
 * X509_set_pubkey(),
 * X509_REQ_set_pubkey() and
 * NETSCAPE_SPKI_set_pubkey()
 * as macros calling the same function */
int SPKI_set_pubkey(x,pkey)
NETSCAPE_SPKI *x;
EVP_PKEY *pkey;
	{
	int ok=0;
	X509_PUBKEY *pk;
	X509_ALGOR *a;
	ASN1_OBJECT *o;
	unsigned char *s,*p;
	int i;

	if (x == NULL) return(0);

	if ((pk=X509_PUBKEY_new()) == NULL) goto err;
	a=pk->algor;

	/* set the algorithm id */
	if ((o=OBJ_nid2obj(pkey->type)) == NULL) goto err;
	ASN1_OBJECT_free(a->algorithm);
	a->algorithm=o;

	/* Set the parameter list */
	if ((a->parameter == NULL) || (a->parameter->type != V_ASN1_NULL))
		{
		ASN1_TYPE_free(a->parameter);
		a->parameter=ASN1_TYPE_new();
		a->parameter->type=V_ASN1_NULL;
		}
	i=i2d_PublicKey(pkey,NULL);
	if ((s=(unsigned char *)malloc(i+1)) == NULL) goto err;
	p=s;
	i2d_PublicKey(pkey,&p);
	if (!ASN1_BIT_STRING_set(pk->public_key,s,i)) goto err;
	free(s);

	X509_PUBKEY_free(x->spkac->pubkey);
	x->spkac->pubkey=pk;
	pk=NULL;
	ok=1;
err:
	if (pk != NULL) X509_PUBKEY_free(pk);
	return(ok);
	}


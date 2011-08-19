/* NOCW */
/*
        Please read the README file for condition of use, before
        using this software.

        Maurice Gittens  <mgittens@gits.nl>   January 1997
*/

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include "loadkeys.h"

#define PUBFILE   "cert.pem"
#define PRIVFILE  "privkey.pem"
#define STDIN     0
#define STDOUT    1 

int main()
{
        char *ct = "This the clear text";
	char *buf;   
	char *buf2;
  	EVP_PKEY *pubKey;
  	EVP_PKEY *privKey;
	int len;

        ERR_load_crypto_strings();

        privKey = ReadPrivateKey(PRIVFILE);
        if (!privKey) 
	{  
		ERR_print_errors_fp (stderr);    
		exit (1);  
	}

        pubKey = ReadPublicKey(PUBFILE);  
	if(!pubKey)
	{
	   EVP_PKEY_free(privKey);   
           fprintf(stderr,"Error: can't load public key");
	   exit(1);
	}

	/* No error checking */
        buf = malloc(EVP_PKEY_size(pubKey));
        buf2 = malloc(EVP_PKEY_size(pubKey));

	len = RSA_public_encrypt(strlen(ct)+1, ct, buf, pubKey->pkey.rsa,RSA_PKCS1_PADDING);

	if (len != EVP_PKEY_size(pubKey))
	{
	    fprintf(stderr,"Error: ciphertext should match length of key\n");
	    exit(1);
	}

	RSA_private_decrypt(len, buf, buf2, privKey->pkey.rsa,RSA_PKCS1_PADDING);

	printf("%s\n", buf2);

	EVP_PKEY_free(privKey);
	EVP_PKEY_free(pubKey);
	free(buf);
	free(buf2);
        return 0;
}

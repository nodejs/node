/* NOCW */
/*
	Please read the README file for condition of use, before
	using this software.
	
	Maurice Gittens  <mgittens@gits.nl>   January 1997
*/

#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <strings.h>
#include <stdlib.h>

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

void main_encrypt(void);
void main_decrypt(void);

static const char *usage = "Usage: example1 [-d]\n";

int main(int argc, char *argv[])
{

        ERR_load_crypto_strings();

	if ((argc == 1))	
	{
		main_encrypt();
	}	
	else if ((argc == 2) && !strcmp(argv[1],"-d"))
	{
		main_decrypt();
	}
	else
	{
		printf("%s",usage);
		exit(1);
	}

	return 0;		
}

void main_encrypt(void)
{
	unsigned int ebuflen;
        EVP_CIPHER_CTX ectx;
        unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char *ekey[1]; 
	int readlen;
	int ekeylen, net_ekeylen; 
	EVP_PKEY *pubKey[1];
	char buf[512];
	char ebuf[512];
	
 	memset(iv, '\0', sizeof(iv));

        pubKey[0] = ReadPublicKey(PUBFILE);

	if(!pubKey[0])
	{
           fprintf(stderr,"Error: can't load public key");
           exit(1);
        }      

        ekey[0] = malloc(EVP_PKEY_size(pubKey[0]));  
        if (!ekey[0])
	{
	   EVP_PKEY_free(pubKey[0]); 
	   perror("malloc");
	   exit(1);
	}

	EVP_SealInit(&ectx,
                   EVP_des_ede3_cbc(),
		   ekey,
		   &ekeylen,
		   iv,
		   pubKey,
		   1); 

	net_ekeylen = htonl(ekeylen);	
	write(STDOUT, (char*)&net_ekeylen, sizeof(net_ekeylen));
        write(STDOUT, ekey[0], ekeylen);
        write(STDOUT, iv, sizeof(iv));

	while(1)
	{
		readlen = read(STDIN, buf, sizeof(buf));

		if (readlen <= 0)
		{
		   if (readlen < 0)
			perror("read");

		   break;
		}

		EVP_SealUpdate(&ectx, ebuf, &ebuflen, buf, readlen);

		write(STDOUT, ebuf, ebuflen);
	}

        EVP_SealFinal(&ectx, ebuf, &ebuflen);
        
	write(STDOUT, ebuf, ebuflen);

        EVP_PKEY_free(pubKey[0]);
	free(ekey[0]);
}

void main_decrypt(void)
{
	char buf[520];
	char ebuf[512];
	unsigned int buflen;
        EVP_CIPHER_CTX ectx;
        unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char *encryptKey; 
	unsigned int ekeylen; 
	EVP_PKEY *privateKey;

	memset(iv, '\0', sizeof(iv));

	privateKey = ReadPrivateKey(PRIVFILE);
	if (!privateKey)
	{
		fprintf(stderr, "Error: can't load private key");
		exit(1);	
	}

     	read(STDIN, &ekeylen, sizeof(ekeylen));
	ekeylen = ntohl(ekeylen);

	if (ekeylen != EVP_PKEY_size(privateKey))
	{
        	EVP_PKEY_free(privateKey);
		fprintf(stderr, "keylength mismatch");
		exit(1);	
	}

	encryptKey = malloc(sizeof(char) * ekeylen);
	if (!encryptKey)
	{
        	EVP_PKEY_free(privateKey);
		perror("malloc");
		exit(1);
	}

	read(STDIN, encryptKey, ekeylen);
	read(STDIN, iv, sizeof(iv));
	EVP_OpenInit(&ectx,
		   EVP_des_ede3_cbc(), 
		   encryptKey,
		   ekeylen,
		   iv,
		   privateKey); 	

	while(1)
	{
		int readlen = read(STDIN, ebuf, sizeof(ebuf));

		if (readlen <= 0)
		{
			if (readlen < 0)
				perror("read");

			break;
		}

		EVP_OpenUpdate(&ectx, buf, &buflen, ebuf, readlen);
		write(STDOUT, buf, buflen);
	}

        EVP_OpenFinal(&ectx, buf, &buflen);

	write(STDOUT, buf, buflen);

        EVP_PKEY_free(privateKey);
	free(encryptKey);
}



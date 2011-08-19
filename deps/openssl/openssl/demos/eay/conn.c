/* NOCW */
/* demos/eay/conn.c */

/* A minimal program to connect to a port using the sock4a protocol.
 *
 * cc -I../../include conn.c -L../.. -lcrypto
 */
#include <stdio.h>
#include <stdlib.h>
#include <openssl/err.h>
#include <openssl/bio.h>
/* #include "proxy.h" */

extern int errno;

int main(argc,argv)
int argc;
char *argv[];
	{
	PROXY *pxy;
	char *host;
	char buf[1024*10],*p;
	BIO *bio;
	int i,len,off,ret=1;

	if (argc <= 1)
		host="localhost:4433";
	else
		host=argv[1];

	/* Lets get nice error messages */
	ERR_load_crypto_strings();

	/* First, configure proxy settings */
	pxy=PROXY_new();
	PROXY_add_server(pxy,PROXY_PROTOCOL_SOCKS,"gromit:1080");

	bio=BIO_new(BIO_s_socks4a_connect());

	BIO_set_conn_hostname(bio,host);
	BIO_set_proxies(bio,pxy);
	BIO_set_socks_userid(bio,"eay");
	BIO_set_nbio(bio,1);

	p="GET / HTTP/1.0\r\n\r\n";
	len=strlen(p);

	off=0;
	for (;;)
		{
		i=BIO_write(bio,&(p[off]),len);
		if (i <= 0)
			{
			if (BIO_should_retry(bio))
				{
				fprintf(stderr,"write DELAY\n");
				sleep(1);
				continue;
				}
			else
				{
				goto err;
				}
			}
		off+=i;
		len-=i;
		if (len <= 0) break;
		}

	for (;;)
		{
		i=BIO_read(bio,buf,sizeof(buf));
		if (i == 0) break;
		if (i < 0)
			{
			if (BIO_should_retry(bio))
				{
				fprintf(stderr,"read DELAY\n");
				sleep(1);
				continue;
				}
			goto err;
			}
		fwrite(buf,1,i,stdout);
		}

	ret=1;

	if (0)
		{
err:
		if (ERR_peek_error() == 0) /* system call error */
			{
			fprintf(stderr,"errno=%d ",errno);
			perror("error");
			}
		else
			ERR_print_errors_fp(stderr);
		}
	BIO_free_all(bio);
	if (pxy != NULL) PROXY_free(pxy);
	exit(!ret);
	return(ret);
	}


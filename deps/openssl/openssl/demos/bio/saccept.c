/* NOCW */
/* demos/bio/saccept.c */

/* A minimal program to server an SSL connection.
 * It uses blocking.
 * saccept host:port
 * host is the interface IP to use.  If any interface, use *:port
 * The default it *:4433
 *
 * cc -I../../include saccept.c -L../.. -lssl -lcrypto
 */

#include <stdio.h>
#include <signal.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#define CERT_FILE	"server.pem"

BIO *in=NULL;

void close_up()
	{
	if (in != NULL)
		BIO_free(in);
	}

int main(argc,argv)
int argc;
char *argv[];
	{
	char *port=NULL;
	BIO *ssl_bio,*tmp;
	SSL_CTX *ctx;
	SSL *ssl;
	char buf[512];
	int ret=1,i;

        if (argc <= 1)
		port="*:4433";
	else
		port=argv[1];

	signal(SIGINT,close_up);

	SSL_load_error_strings();

#ifdef WATT32
	dbug_init();
	sock_init();
#endif

	/* Add ciphers and message digests */
	OpenSSL_add_ssl_algorithms();

	ctx=SSL_CTX_new(SSLv23_server_method());
	if (!SSL_CTX_use_certificate_file(ctx,CERT_FILE,SSL_FILETYPE_PEM))
		goto err;
	if (!SSL_CTX_use_PrivateKey_file(ctx,CERT_FILE,SSL_FILETYPE_PEM))
		goto err;
	if (!SSL_CTX_check_private_key(ctx))
		goto err;

	/* Setup server side SSL bio */
	ssl=SSL_new(ctx);
	ssl_bio=BIO_new_ssl(ctx,0);

	if ((in=BIO_new_accept(port)) == NULL) goto err;

	/* This means that when a new connection is acceptede on 'in',
	 * The ssl_bio will be 'dupilcated' and have the new socket
	 * BIO push into it.  Basically it means the SSL BIO will be
	 * automatically setup */
	BIO_set_accept_bios(in,ssl_bio);

again:
	/* The first call will setup the accept socket, and the second
	 * will get a socket.  In this loop, the first actual accept
	 * will occur in the BIO_read() function. */

	if (BIO_do_accept(in) <= 0) goto err;

	for (;;)
		{
		i=BIO_read(in,buf,512);
		if (i == 0)
			{
			/* If we have finished, remove the underlying
			 * BIO stack so the next time we call any function
			 * for this BIO, it will attempt to do an
			 * accept */
			printf("Done\n");
			tmp=BIO_pop(in);
			BIO_free_all(tmp);
			goto again;
			}
		if (i < 0) goto err;
		fwrite(buf,1,i,stdout);
		fflush(stdout);
		}

	ret=0;
err:
	if (ret)
		{
		ERR_print_errors_fp(stderr);
		}
	if (in != NULL) BIO_free(in);
	exit(ret);
	return(!ret);
	}


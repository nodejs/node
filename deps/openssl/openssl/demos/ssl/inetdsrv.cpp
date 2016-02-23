/* inetdserv.cpp  -  Minimal ssleay server for Unix inetd.conf
 * 30.9.1996, Sampo Kellomaki <sampo@iki.fi>
 * From /etc/inetd.conf:
 *     1111 stream tcp nowait sampo /usr/users/sampo/demo/inetdserv inetdserv
 */

#include <stdio.h>
#include <errno.h>

#include "rsa.h"       /* SSLeay stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define HOME "/usr/users/sampo/demo/"
#define CERTF HOME "plain-cert.pem"
#define KEYF  HOME "plain-key.pem"

#define CHK_NULL(x) if ((x)==NULL) exit (1)
#define CHK_ERR(err,s) if ((err)==-1) \
                         { fprintf(log, "%s %d\n", (s), errno); exit(1); }
#define CHK_SSL(err) if ((err)==-1) { ERR_print_errors_fp(log); exit(2); }

void main ()
{
  int err;
  SSL_CTX* ctx;
  SSL*     ssl;
  X509*    client_cert;
  char*    str;
  char     buf [4096];
  FILE* log;
  
  log = fopen ("/dev/console", "a");                     CHK_NULL(log);
  fprintf (log, "inetdserv %ld\n", (long)getpid());
  
  SSL_load_error_strings();
  ctx = SSL_CTX_new (); CHK_NULL(ctx);
  
  err = SSL_CTX_use_RSAPrivateKey_file (ctx, KEYF,  SSL_FILETYPE_PEM);
  CHK_SSL (err);
  
  err = SSL_CTX_use_certificate_file   (ctx, CERTF, SSL_FILETYPE_PEM);
  CHK_SSL (err);

  /* inetd has already opened the TCP connection, so we can get right
     down to business. */
  
  ssl = SSL_new (ctx);  CHK_NULL(ssl);
  SSL_set_fd (ssl,  fileno(stdin));
  err = SSL_accept (ssl);                                CHK_SSL(err);
  
  /* Get the cipher - opt */
  
  fprintf (log, "SSL connection using %s\n", SSL_get_cipher (ssl));
  
  /* Get client's certificate (note: beware of dynamic allocation) - opt */

  client_cert = SSL_get_peer_certificate (ssl);
  if (client_cert != NULL) {
    fprintf (log, "Client certificate:\n");
    
    str = X509_NAME_oneline (X509_get_subject_name (client_cert));
    CHK_NULL(str);
    fprintf (log, "\t subject: %s\n", str);
    OPENSSL_free (str);
    
    str = X509_NAME_oneline (X509_get_issuer_name  (client_cert));
    CHK_NULL(str);
    fprintf (log, "\t issuer: %s\n", str);
    OPENSSL_free (str);
    
    /* We could do all sorts of certificate verification stuff here before
       deallocating the certificate. */
    
    X509_free (client_cert);
  } else
    fprintf (log, "Client doe not have certificate.\n");

  /* ------------------------------------------------- */
  /* DATA EXCHANGE: Receive message and send reply  */
  
  err = SSL_read (ssl, buf, sizeof(buf) - 1);  CHK_SSL(err);
  buf[err] = '\0';
  fprintf (log, "Got %d chars:'%s'\n", err, buf);
  
  err = SSL_write (ssl, "Loud and clear.", strlen("Loud and clear."));
  CHK_SSL(err);

  /* Clean up. */

  fclose (log);
  SSL_free (ssl);
  SSL_CTX_free (ctx);
}
/* EOF - inetdserv.cpp */

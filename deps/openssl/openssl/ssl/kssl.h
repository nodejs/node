/* ssl/kssl.h -*- mode: C; c-file-style: "eay" -*- */
/* Written by Vern Staats <staatsvr@asc.hpc.mil> for the OpenSSL project 2000.
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/*
**	19990701	VRS 	Started.
*/

#ifndef	KSSL_H
#define	KSSL_H

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_KRB5

#include <stdio.h>
#include <ctype.h>
#include <krb5.h>
#ifdef OPENSSL_SYS_WIN32
/* These can sometimes get redefined indirectly by krb5 header files
 * after they get undefed in ossl_typ.h
 */
#undef X509_NAME
#undef X509_EXTENSIONS
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/*
**	Depending on which KRB5 implementation used, some types from
**	the other may be missing.  Resolve that here and now
*/
#ifdef KRB5_HEIMDAL
typedef unsigned char krb5_octet;
#define FAR
#else

#ifndef FAR
#define FAR
#endif

#endif

/*	Uncomment this to debug kssl problems or
**	to trace usage of the Kerberos session key
**
**	#define		KSSL_DEBUG
*/

#ifndef	KRB5SVC
#define KRB5SVC	"host"
#endif

#ifndef	KRB5KEYTAB
#define KRB5KEYTAB	"/etc/krb5.keytab"
#endif

#ifndef KRB5SENDAUTH
#define KRB5SENDAUTH	1
#endif

#ifndef KRB5CHECKAUTH
#define KRB5CHECKAUTH	1
#endif

#ifndef KSSL_CLOCKSKEW
#define	KSSL_CLOCKSKEW	300;
#endif

#define	KSSL_ERR_MAX	255
typedef struct kssl_err_st  {
	int  reason;
	char text[KSSL_ERR_MAX+1];
	} KSSL_ERR;


/*	Context for passing
**		(1) Kerberos session key to SSL, and
**		(2)	Config data between application and SSL lib
*/
typedef struct kssl_ctx_st
        {
                                /*	used by:    disposition:            */
	char *service_name;	/*	C,S	    default ok (kssl)       */
	char *service_host;	/*	C	    input, REQUIRED         */
	char *client_princ;	/*	S	    output from krb5 ticket */
	char *keytab_file;	/*      S	    NULL (/etc/krb5.keytab) */
	char *cred_cache;	/*	C	    NULL (default)          */
	krb5_enctype enctype;
	int length;
	krb5_octet FAR *key;
	} KSSL_CTX;

#define	KSSL_CLIENT 	1
#define KSSL_SERVER 	2
#define	KSSL_SERVICE	3
#define	KSSL_KEYTAB 	4

#define KSSL_CTX_OK 	0
#define KSSL_CTX_ERR	1
#define KSSL_NOMEM	2

/* Public (for use by applications that use OpenSSL with Kerberos 5 support */
krb5_error_code kssl_ctx_setstring(KSSL_CTX *kssl_ctx, int which, char *text);
KSSL_CTX *kssl_ctx_new(void);
KSSL_CTX *kssl_ctx_free(KSSL_CTX *kssl_ctx);
void kssl_ctx_show(KSSL_CTX *kssl_ctx);
krb5_error_code kssl_ctx_setprinc(KSSL_CTX *kssl_ctx, int which,
        krb5_data *realm, krb5_data *entity, int nentities);
krb5_error_code	kssl_cget_tkt(KSSL_CTX *kssl_ctx,  krb5_data **enc_tktp,
        krb5_data *authenp, KSSL_ERR *kssl_err);
krb5_error_code	kssl_sget_tkt(KSSL_CTX *kssl_ctx,  krb5_data *indata,
        krb5_ticket_times *ttimes, KSSL_ERR *kssl_err);
krb5_error_code kssl_ctx_setkey(KSSL_CTX *kssl_ctx, krb5_keyblock *session);
void	kssl_err_set(KSSL_ERR *kssl_err, int reason, char *text);
void kssl_krb5_free_data_contents(krb5_context context, krb5_data *data);
krb5_error_code  kssl_build_principal_2(krb5_context context,
			krb5_principal *princ, int rlen, const char *realm,
			int slen, const char *svc, int hlen, const char *host);
krb5_error_code  kssl_validate_times(krb5_timestamp atime,
					krb5_ticket_times *ttimes);
krb5_error_code  kssl_check_authent(KSSL_CTX *kssl_ctx, krb5_data *authentp,
			            krb5_timestamp *atimep, KSSL_ERR *kssl_err);
unsigned char	*kssl_skip_confound(krb5_enctype enctype, unsigned char *authn);

void SSL_set0_kssl_ctx(SSL *s, KSSL_CTX *kctx);
KSSL_CTX * SSL_get0_kssl_ctx(SSL *s);
char *kssl_ctx_get0_client_princ(KSSL_CTX *kctx);

#ifdef  __cplusplus
}
#endif
#endif	/* OPENSSL_NO_KRB5	*/
#endif	/* KSSL_H 	*/

/* apps/openssl.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define OPENSSL_C /* tells apps.h to use complete apps_startup() */
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/lhash.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#define USE_SOCKETS /* needed for the _O_BINARY defs in the MS world */
#include "progs.h"
#include "s_apps.h"
#include <openssl/err.h>

/* The LHASH callbacks ("hash" & "cmp") have been replaced by functions with the
 * base prototypes (we cast each variable inside the function to the required
 * type of "FUNCTION*"). This removes the necessity for macro-generated wrapper
 * functions. */

/* static unsigned long MS_CALLBACK hash(FUNCTION *a); */
static unsigned long MS_CALLBACK hash(const void *a_void);
/* static int MS_CALLBACK cmp(FUNCTION *a,FUNCTION *b); */
static int MS_CALLBACK cmp(const void *a_void,const void *b_void);
static LHASH *prog_init(void );
static int do_cmd(LHASH *prog,int argc,char *argv[]);
char *default_config_file=NULL;

/* Make sure there is only one when MONOLITH is defined */
#ifdef MONOLITH
CONF *config=NULL;
BIO *bio_err=NULL;
int in_FIPS_mode=0;
#endif


static void lock_dbg_cb(int mode, int type, const char *file, int line)
	{
	static int modes[CRYPTO_NUM_LOCKS]; /* = {0, 0, ... } */
	const char *errstr = NULL;
	int rw;
	
	rw = mode & (CRYPTO_READ|CRYPTO_WRITE);
	if (!((rw == CRYPTO_READ) || (rw == CRYPTO_WRITE)))
		{
		errstr = "invalid mode";
		goto err;
		}

	if (type < 0 || type >= CRYPTO_NUM_LOCKS)
		{
		errstr = "type out of bounds";
		goto err;
		}

	if (mode & CRYPTO_LOCK)
		{
		if (modes[type])
			{
			errstr = "already locked";
			/* must not happen in a single-threaded program
			 * (would deadlock) */
			goto err;
			}

		modes[type] = rw;
		}
	else if (mode & CRYPTO_UNLOCK)
		{
		if (!modes[type])
			{
			errstr = "not locked";
			goto err;
			}
		
		if (modes[type] != rw)
			{
			errstr = (rw == CRYPTO_READ) ?
				"CRYPTO_r_unlock on write lock" :
				"CRYPTO_w_unlock on read lock";
			}

		modes[type] = 0;
		}
	else
		{
		errstr = "invalid mode";
		goto err;
		}

 err:
	if (errstr)
		{
		/* we cannot use bio_err here */
		fprintf(stderr, "openssl (lock_dbg_cb): %s (mode=%d, type=%d) at %s:%d\n",
			errstr, mode, type, file, line);
		}
	}


int main(int Argc, char *Argv[])
	{
	ARGS arg;
#define PROG_NAME_SIZE	39
	char pname[PROG_NAME_SIZE+1];
	FUNCTION f,*fp;
	MS_STATIC const char *prompt;
	MS_STATIC char buf[1024];
	char *to_free=NULL;
	int n,i,ret=0;
	int argc;
	char **argv,*p;
	LHASH *prog=NULL;
	long errline;
 
	arg.data=NULL;
	arg.count=0;

	in_FIPS_mode = 0;

	if(getenv("OPENSSL_FIPS")) {
#ifdef OPENSSL_FIPS
		if (!FIPS_mode_set(1)) {
			ERR_load_crypto_strings();
			ERR_print_errors(BIO_new_fp(stderr,BIO_NOCLOSE));
			EXIT(1);
		}
		in_FIPS_mode = 1;
#else
		fprintf(stderr, "FIPS mode not supported.\n");
		EXIT(1);
#endif
		}

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	if (getenv("OPENSSL_DEBUG_MEMORY") != NULL) /* if not defined, use compiled-in library defaults */
		{
		if (!(0 == strcmp(getenv("OPENSSL_DEBUG_MEMORY"), "off")))
			{
			CRYPTO_malloc_debug_init();
			CRYPTO_set_mem_debug_options(V_CRYPTO_MDEBUG_ALL);
			}
		else
			{
			/* OPENSSL_DEBUG_MEMORY=off */
			CRYPTO_set_mem_debug_functions(0, 0, 0, 0, 0);
			}
		}
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

#if 0
	if (getenv("OPENSSL_DEBUG_LOCKING") != NULL)
#endif
		{
		CRYPTO_set_locking_callback(lock_dbg_cb);
		}

	apps_startup();

	/* Lets load up our environment a little */
	p=getenv("OPENSSL_CONF");
	if (p == NULL)
		p=getenv("SSLEAY_CONF");
	if (p == NULL)
		p=to_free=make_config_name();

	default_config_file=p;

	config=NCONF_new(NULL);
	i=NCONF_load(config,p,&errline);
	if (i == 0)
		{
		NCONF_free(config);
		config = NULL;
		ERR_clear_error();
		}

	prog=prog_init();

	/* first check the program name */
	program_name(Argv[0],pname,sizeof pname);

	f.name=pname;
	fp=(FUNCTION *)lh_retrieve(prog,&f);
	if (fp != NULL)
		{
		Argv[0]=pname;
		ret=fp->func(Argc,Argv);
		goto end;
		}

	/* ok, now check that there are not arguments, if there are,
	 * run with them, shifting the ssleay off the front */
	if (Argc != 1)
		{
		Argc--;
		Argv++;
		ret=do_cmd(prog,Argc,Argv);
		if (ret < 0) ret=0;
		goto end;
		}

	/* ok, lets enter the old 'OpenSSL>' mode */
	
	for (;;)
		{
		ret=0;
		p=buf;
		n=sizeof buf;
		i=0;
		for (;;)
			{
			p[0]='\0';
			if (i++)
				prompt=">";
			else	prompt="OpenSSL> ";
			fputs(prompt,stdout);
			fflush(stdout);
			if (!fgets(p,n,stdin))
				goto end;
			if (p[0] == '\0') goto end;
			i=strlen(p);
			if (i <= 1) break;
			if (p[i-2] != '\\') break;
			i-=2;
			p+=i;
			n-=i;
			}
		if (!chopup_args(&arg,buf,&argc,&argv)) break;

		ret=do_cmd(prog,argc,argv);
		if (ret < 0)
			{
			ret=0;
			goto end;
			}
		if (ret != 0)
			BIO_printf(bio_err,"error in %s\n",argv[0]);
		(void)BIO_flush(bio_err);
		}
	BIO_printf(bio_err,"bad exit\n");
	ret=1;
end:
	if (to_free)
		OPENSSL_free(to_free);
	if (config != NULL)
		{
		NCONF_free(config);
		config=NULL;
		}
	if (prog != NULL) lh_free(prog);
	if (arg.data != NULL) OPENSSL_free(arg.data);

	apps_shutdown();

	CRYPTO_mem_leaks(bio_err);
	if (bio_err != NULL)
		{
		BIO_free(bio_err);
		bio_err=NULL;
		}
	OPENSSL_EXIT(ret);
	}

#define LIST_STANDARD_COMMANDS "list-standard-commands"
#define LIST_MESSAGE_DIGEST_COMMANDS "list-message-digest-commands"
#define LIST_CIPHER_COMMANDS "list-cipher-commands"

static int do_cmd(LHASH *prog, int argc, char *argv[])
	{
	FUNCTION f,*fp;
	int i,ret=1,tp,nl;

	if ((argc <= 0) || (argv[0] == NULL))
		{ ret=0; goto end; }
	f.name=argv[0];
	fp=(FUNCTION *)lh_retrieve(prog,&f);
	if (fp != NULL)
		{
		ret=fp->func(argc,argv);
		}
	else if ((strncmp(argv[0],"no-",3)) == 0)
		{
		BIO *bio_stdout = BIO_new_fp(stdout,BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
		{
		BIO *tmpbio = BIO_new(BIO_f_linebuffer());
		bio_stdout = BIO_push(tmpbio, bio_stdout);
		}
#endif
		f.name=argv[0]+3;
		ret = (lh_retrieve(prog,&f) != NULL);
		if (!ret)
			BIO_printf(bio_stdout, "%s\n", argv[0]);
		else
			BIO_printf(bio_stdout, "%s\n", argv[0]+3);
		BIO_free_all(bio_stdout);
		goto end;
		}
	else if ((strcmp(argv[0],"quit") == 0) ||
		(strcmp(argv[0],"q") == 0) ||
		(strcmp(argv[0],"exit") == 0) ||
		(strcmp(argv[0],"bye") == 0))
		{
		ret= -1;
		goto end;
		}
	else if ((strcmp(argv[0],LIST_STANDARD_COMMANDS) == 0) ||
		(strcmp(argv[0],LIST_MESSAGE_DIGEST_COMMANDS) == 0) ||
		(strcmp(argv[0],LIST_CIPHER_COMMANDS) == 0))
		{
		int list_type;
		BIO *bio_stdout;

		if (strcmp(argv[0],LIST_STANDARD_COMMANDS) == 0)
			list_type = FUNC_TYPE_GENERAL;
		else if (strcmp(argv[0],LIST_MESSAGE_DIGEST_COMMANDS) == 0)
			list_type = FUNC_TYPE_MD;
		else /* strcmp(argv[0],LIST_CIPHER_COMMANDS) == 0 */
			list_type = FUNC_TYPE_CIPHER;
		bio_stdout = BIO_new_fp(stdout,BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
		{
		BIO *tmpbio = BIO_new(BIO_f_linebuffer());
		bio_stdout = BIO_push(tmpbio, bio_stdout);
		}
#endif
		
		for (fp=functions; fp->name != NULL; fp++)
			if (fp->type == list_type)
				BIO_printf(bio_stdout, "%s\n", fp->name);
		BIO_free_all(bio_stdout);
		ret=0;
		goto end;
		}
	else
		{
		BIO_printf(bio_err,"openssl:Error: '%s' is an invalid command.\n",
			argv[0]);
		BIO_printf(bio_err, "\nStandard commands");
		i=0;
		tp=0;
		for (fp=functions; fp->name != NULL; fp++)
			{
			nl=0;
#ifdef OPENSSL_NO_CAMELLIA
			if (((i++) % 5) == 0)
#else
			if (((i++) % 4) == 0)
#endif
				{
				BIO_printf(bio_err,"\n");
				nl=1;
				}
			if (fp->type != tp)
				{
				tp=fp->type;
				if (!nl) BIO_printf(bio_err,"\n");
				if (tp == FUNC_TYPE_MD)
					{
					i=1;
					BIO_printf(bio_err,
						"\nMessage Digest commands (see the `dgst' command for more details)\n");
					}
				else if (tp == FUNC_TYPE_CIPHER)
					{
					i=1;
					BIO_printf(bio_err,"\nCipher commands (see the `enc' command for more details)\n");
					}
				}
#ifdef OPENSSL_NO_CAMELLIA
			BIO_printf(bio_err,"%-15s",fp->name);
#else
			BIO_printf(bio_err,"%-18s",fp->name);
#endif
			}
		BIO_printf(bio_err,"\n\n");
		ret=0;
		}
end:
	return(ret);
	}

static int SortFnByName(const void *_f1,const void *_f2)
    {
    const FUNCTION *f1=_f1;
    const FUNCTION *f2=_f2;

    if(f1->type != f2->type)
	return f1->type-f2->type;
    return strcmp(f1->name,f2->name);
    }

static LHASH *prog_init(void)
	{
	LHASH *ret;
	FUNCTION *f;
	size_t i;

	/* Purely so it looks nice when the user hits ? */
	for(i=0,f=functions ; f->name != NULL ; ++f,++i)
	    ;
	qsort(functions,i,sizeof *functions,SortFnByName);

	if ((ret=lh_new(hash, cmp)) == NULL)
		return(NULL);

	for (f=functions; f->name != NULL; f++)
		lh_insert(ret,f);
	return(ret);
	}

/* static int MS_CALLBACK cmp(FUNCTION *a, FUNCTION *b) */
static int MS_CALLBACK cmp(const void *a_void, const void *b_void)
	{
	return(strncmp(((const FUNCTION *)a_void)->name,
			((const FUNCTION *)b_void)->name,8));
	}

/* static unsigned long MS_CALLBACK hash(FUNCTION *a) */
static unsigned long MS_CALLBACK hash(const void *a_void)
	{
	return(lh_strhash(((const FUNCTION *)a_void)->name));
	}

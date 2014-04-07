/* apps/req.c */
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

/* Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code */
#ifdef OPENSSL_NO_DEPRECATED
#undef OPENSSL_NO_DEPRECATED
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef OPENSSL_NO_STDIO
#define APPS_WIN16
#endif
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif

#define SECTION		"req"

#define BITS		"default_bits"
#define KEYFILE		"default_keyfile"
#define PROMPT		"prompt"
#define DISTINGUISHED_NAME	"distinguished_name"
#define ATTRIBUTES	"attributes"
#define V3_EXTENSIONS	"x509_extensions"
#define REQ_EXTENSIONS	"req_extensions"
#define STRING_MASK	"string_mask"
#define UTF8_IN		"utf8"

#define DEFAULT_KEY_LENGTH	512
#define MIN_KEY_LENGTH		384

#undef PROG
#define PROG	req_main

/* -inform arg	- input format - default PEM (DER or PEM)
 * -outform arg - output format - default PEM
 * -in arg	- input file - default stdin
 * -out arg	- output file - default stdout
 * -verify	- check request signature
 * -noout	- don't print stuff out.
 * -text	- print out human readable text.
 * -nodes	- no des encryption
 * -config file	- Load configuration file.
 * -key file	- make a request using key in file (or use it for verification).
 * -keyform arg	- key file format.
 * -rand file(s) - load the file(s) into the PRNG.
 * -newkey	- make a key and a request.
 * -modulus	- print RSA modulus.
 * -pubkey	- output Public Key.
 * -x509	- output a self signed X509 structure instead.
 * -asn1-kludge	- output new certificate request in a format that some CA's
 *		  require.  This format is wrong
 */

static int make_REQ(X509_REQ *req,EVP_PKEY *pkey,char *dn,int mutlirdn,
		int attribs,unsigned long chtype);
static int build_subject(X509_REQ *req, char *subj, unsigned long chtype,
		int multirdn);
static int prompt_info(X509_REQ *req,
		STACK_OF(CONF_VALUE) *dn_sk, char *dn_sect,
		STACK_OF(CONF_VALUE) *attr_sk, char *attr_sect, int attribs,
		unsigned long chtype);
static int auto_info(X509_REQ *req, STACK_OF(CONF_VALUE) *sk,
				STACK_OF(CONF_VALUE) *attr, int attribs,
				unsigned long chtype);
static int add_attribute_object(X509_REQ *req, char *text, const char *def,
				char *value, int nid, int n_min,
				int n_max, unsigned long chtype);
static int add_DN_object(X509_NAME *n, char *text, const char *def, char *value,
	int nid,int n_min,int n_max, unsigned long chtype, int mval);
static int genpkey_cb(EVP_PKEY_CTX *ctx);
static int req_check_len(int len,int n_min,int n_max);
static int check_end(const char *str, const char *end);
static EVP_PKEY_CTX *set_keygen_ctx(BIO *err, const char *gstr, int *pkey_type,
					long *pkeylen, char **palgnam,
					ENGINE *keygen_engine);
#ifndef MONOLITH
static char *default_config_file=NULL;
#endif
static CONF *req_conf=NULL;
static int batch=0;

int MAIN(int, char **);

int MAIN(int argc, char **argv)
	{
	ENGINE *e = NULL, *gen_eng = NULL;
	unsigned long nmflag = 0, reqflag = 0;
	int ex=1,x509=0,days=30;
	X509 *x509ss=NULL;
	X509_REQ *req=NULL;
	EVP_PKEY_CTX *genctx = NULL;
	const char *keyalg = NULL;
	char *keyalgstr = NULL;
	STACK_OF(OPENSSL_STRING) *pkeyopts = NULL, *sigopts = NULL;
	EVP_PKEY *pkey=NULL;
	int i=0,badops=0,newreq=0,verbose=0,pkey_type=-1;
	long newkey = -1;
	BIO *in=NULL,*out=NULL;
	int informat,outformat,verify=0,noout=0,text=0,keyform=FORMAT_PEM;
	int nodes=0,kludge=0,newhdr=0,subject=0,pubkey=0;
	char *infile,*outfile,*prog,*keyfile=NULL,*template=NULL,*keyout=NULL;
#ifndef OPENSSL_NO_ENGINE
	char *engine=NULL;
#endif
	char *extensions = NULL;
	char *req_exts = NULL;
	const EVP_CIPHER *cipher=NULL;
	ASN1_INTEGER *serial = NULL;
	int modulus=0;
	char *inrand=NULL;
	char *passargin = NULL, *passargout = NULL;
	char *passin = NULL, *passout = NULL;
	char *p;
	char *subj = NULL;
	int multirdn = 0;
	const EVP_MD *md_alg=NULL,*digest=NULL;
	unsigned long chtype = MBSTRING_ASC;
#ifndef MONOLITH
	char *to_free;
	long errline;
#endif

	req_conf = NULL;
#ifndef OPENSSL_NO_DES
	cipher=EVP_des_ede3_cbc();
#endif
	apps_startup();

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	infile=NULL;
	outfile=NULL;
	informat=FORMAT_PEM;
	outformat=FORMAT_PEM;

	prog=argv[0];
	argc--;
	argv++;
	while (argc >= 1)
		{
		if 	(strcmp(*argv,"-inform") == 0)
			{
			if (--argc < 1) goto bad;
			informat=str2fmt(*(++argv));
			}
		else if (strcmp(*argv,"-outform") == 0)
			{
			if (--argc < 1) goto bad;
			outformat=str2fmt(*(++argv));
			}
#ifndef OPENSSL_NO_ENGINE
		else if (strcmp(*argv,"-engine") == 0)
			{
			if (--argc < 1) goto bad;
			engine= *(++argv);
			}
		else if (strcmp(*argv,"-keygen_engine") == 0)
			{
			if (--argc < 1) goto bad;
			gen_eng = ENGINE_by_id(*(++argv));
			if (gen_eng == NULL)
				{
				BIO_printf(bio_err, "Can't find keygen engine %s\n", *argv);
				goto end;
				}
			}
#endif
		else if (strcmp(*argv,"-key") == 0)
			{
			if (--argc < 1) goto bad;
			keyfile= *(++argv);
			}
		else if (strcmp(*argv,"-pubkey") == 0)
			{
			pubkey=1;
			}
		else if (strcmp(*argv,"-new") == 0)
			{
			newreq=1;
			}
		else if (strcmp(*argv,"-config") == 0)
			{	
			if (--argc < 1) goto bad;
			template= *(++argv);
			}
		else if (strcmp(*argv,"-keyform") == 0)
			{
			if (--argc < 1) goto bad;
			keyform=str2fmt(*(++argv));
			}
		else if (strcmp(*argv,"-in") == 0)
			{
			if (--argc < 1) goto bad;
			infile= *(++argv);
			}
		else if (strcmp(*argv,"-out") == 0)
			{
			if (--argc < 1) goto bad;
			outfile= *(++argv);
			}
		else if (strcmp(*argv,"-keyout") == 0)
			{
			if (--argc < 1) goto bad;
			keyout= *(++argv);
			}
		else if (strcmp(*argv,"-passin") == 0)
			{
			if (--argc < 1) goto bad;
			passargin= *(++argv);
			}
		else if (strcmp(*argv,"-passout") == 0)
			{
			if (--argc < 1) goto bad;
			passargout= *(++argv);
			}
		else if (strcmp(*argv,"-rand") == 0)
			{
			if (--argc < 1) goto bad;
			inrand= *(++argv);
			}
		else if (strcmp(*argv,"-newkey") == 0)
			{
			if (--argc < 1)
				goto bad;
			keyalg = *(++argv);
			newreq=1;
			}
		else if (strcmp(*argv,"-pkeyopt") == 0)
			{
			if (--argc < 1)
				goto bad;
			if (!pkeyopts)
				pkeyopts = sk_OPENSSL_STRING_new_null();
			if (!pkeyopts || !sk_OPENSSL_STRING_push(pkeyopts, *(++argv)))
				goto bad;
			}
		else if (strcmp(*argv,"-sigopt") == 0)
			{
			if (--argc < 1)
				goto bad;
			if (!sigopts)
				sigopts = sk_OPENSSL_STRING_new_null();
			if (!sigopts || !sk_OPENSSL_STRING_push(sigopts, *(++argv)))
				goto bad;
			}
		else if (strcmp(*argv,"-batch") == 0)
			batch=1;
		else if (strcmp(*argv,"-newhdr") == 0)
			newhdr=1;
		else if (strcmp(*argv,"-modulus") == 0)
			modulus=1;
		else if (strcmp(*argv,"-verify") == 0)
			verify=1;
		else if (strcmp(*argv,"-nodes") == 0)
			nodes=1;
		else if (strcmp(*argv,"-noout") == 0)
			noout=1;
		else if (strcmp(*argv,"-verbose") == 0)
			verbose=1;
		else if (strcmp(*argv,"-utf8") == 0)
			chtype = MBSTRING_UTF8;
		else if (strcmp(*argv,"-nameopt") == 0)
			{
			if (--argc < 1) goto bad;
			if (!set_name_ex(&nmflag, *(++argv))) goto bad;
			}
		else if (strcmp(*argv,"-reqopt") == 0)
			{
			if (--argc < 1) goto bad;
			if (!set_cert_ex(&reqflag, *(++argv))) goto bad;
			}
		else if (strcmp(*argv,"-subject") == 0)
			subject=1;
		else if (strcmp(*argv,"-text") == 0)
			text=1;
		else if (strcmp(*argv,"-x509") == 0)
			x509=1;
		else if (strcmp(*argv,"-asn1-kludge") == 0)
			kludge=1;
		else if (strcmp(*argv,"-no-asn1-kludge") == 0)
			kludge=0;
		else if (strcmp(*argv,"-subj") == 0)
			{
			if (--argc < 1) goto bad;
			subj= *(++argv);
			}
		else if (strcmp(*argv,"-multivalue-rdn") == 0)
			multirdn=1;
		else if (strcmp(*argv,"-days") == 0)
			{
			if (--argc < 1) goto bad;
			days= atoi(*(++argv));
			if (days == 0) days=30;
			}
		else if (strcmp(*argv,"-set_serial") == 0)
			{
			if (--argc < 1) goto bad;
			serial = s2i_ASN1_INTEGER(NULL, *(++argv));
			if (!serial) goto bad;
			}
		else if (strcmp(*argv,"-extensions") == 0)
			{
			if (--argc < 1) goto bad;
			extensions = *(++argv);
			}
		else if (strcmp(*argv,"-reqexts") == 0)
			{
			if (--argc < 1) goto bad;
			req_exts = *(++argv);
			}
		else if ((md_alg=EVP_get_digestbyname(&((*argv)[1]))) != NULL)
			{
			/* ok */
			digest=md_alg;
			}
		else
			{
			BIO_printf(bio_err,"unknown option %s\n",*argv);
			badops=1;
			break;
			}
		argc--;
		argv++;
		}

	if (badops)
		{
bad:
		BIO_printf(bio_err,"%s [options] <infile >outfile\n",prog);
		BIO_printf(bio_err,"where options  are\n");
		BIO_printf(bio_err," -inform arg    input format - DER or PEM\n");
		BIO_printf(bio_err," -outform arg   output format - DER or PEM\n");
		BIO_printf(bio_err," -in arg        input file\n");
		BIO_printf(bio_err," -out arg       output file\n");
		BIO_printf(bio_err," -text          text form of request\n");
		BIO_printf(bio_err," -pubkey        output public key\n");
		BIO_printf(bio_err," -noout         do not output REQ\n");
		BIO_printf(bio_err," -verify        verify signature on REQ\n");
		BIO_printf(bio_err," -modulus       RSA modulus\n");
		BIO_printf(bio_err," -nodes         don't encrypt the output key\n");
#ifndef OPENSSL_NO_ENGINE
		BIO_printf(bio_err," -engine e      use engine e, possibly a hardware device\n");
#endif
		BIO_printf(bio_err," -subject       output the request's subject\n");
		BIO_printf(bio_err," -passin        private key password source\n");
		BIO_printf(bio_err," -key file      use the private key contained in file\n");
		BIO_printf(bio_err," -keyform arg   key file format\n");
		BIO_printf(bio_err," -keyout arg    file to send the key to\n");
		BIO_printf(bio_err," -rand file%cfile%c...\n", LIST_SEPARATOR_CHAR, LIST_SEPARATOR_CHAR);
		BIO_printf(bio_err,"                load the file (or the files in the directory) into\n");
		BIO_printf(bio_err,"                the random number generator\n");
		BIO_printf(bio_err," -newkey rsa:bits generate a new RSA key of 'bits' in size\n");
		BIO_printf(bio_err," -newkey dsa:file generate a new DSA key, parameters taken from CA in 'file'\n");
#ifndef OPENSSL_NO_ECDSA
		BIO_printf(bio_err," -newkey ec:file generate a new EC key, parameters taken from CA in 'file'\n");
#endif
		BIO_printf(bio_err," -[digest]      Digest to sign with (md5, sha1, md2, mdc2, md4)\n");
		BIO_printf(bio_err," -config file   request template file.\n");
		BIO_printf(bio_err," -subj arg      set or modify request subject\n");
		BIO_printf(bio_err," -multivalue-rdn enable support for multivalued RDNs\n");
		BIO_printf(bio_err," -new           new request.\n");
		BIO_printf(bio_err," -batch         do not ask anything during request generation\n");
		BIO_printf(bio_err," -x509          output a x509 structure instead of a cert. req.\n");
		BIO_printf(bio_err," -days          number of days a certificate generated by -x509 is valid for.\n");
		BIO_printf(bio_err," -set_serial    serial number to use for a certificate generated by -x509.\n");
		BIO_printf(bio_err," -newhdr        output \"NEW\" in the header lines\n");
		BIO_printf(bio_err," -asn1-kludge   Output the 'request' in a format that is wrong but some CA's\n");
		BIO_printf(bio_err,"                have been reported as requiring\n");
		BIO_printf(bio_err," -extensions .. specify certificate extension section (override value in config file)\n");
		BIO_printf(bio_err," -reqexts ..    specify request extension section (override value in config file)\n");
		BIO_printf(bio_err," -utf8          input characters are UTF8 (default ASCII)\n");
		BIO_printf(bio_err," -nameopt arg    - various certificate name options\n");
		BIO_printf(bio_err," -reqopt arg    - various request text options\n\n");
		goto end;
		}

	ERR_load_crypto_strings();
	if(!app_passwd(bio_err, passargin, passargout, &passin, &passout)) {
		BIO_printf(bio_err, "Error getting passwords\n");
		goto end;
	}

#ifndef MONOLITH /* else this has happened in openssl.c (global `config') */
	/* Lets load up our environment a little */
	p=getenv("OPENSSL_CONF");
	if (p == NULL)
		p=getenv("SSLEAY_CONF");
	if (p == NULL)
		p=to_free=make_config_name();
	default_config_file=p;
	config=NCONF_new(NULL);
	i=NCONF_load(config, p, &errline);
#endif

	if (template != NULL)
		{
		long errline = -1;

		if( verbose )
			BIO_printf(bio_err,"Using configuration from %s\n",template);
		req_conf=NCONF_new(NULL);
		i=NCONF_load(req_conf,template,&errline);
		if (i == 0)
			{
			BIO_printf(bio_err,"error on line %ld of %s\n",errline,template);
			goto end;
			}
		}
	else
		{
		req_conf=config;

		if (req_conf == NULL)
			{
			BIO_printf(bio_err,"Unable to load config info from %s\n", default_config_file);
			if (newreq)
				goto end;
			}
		else if( verbose )
			BIO_printf(bio_err,"Using configuration from %s\n",
			default_config_file);
		}

	if (req_conf != NULL)
		{
		if (!load_config(bio_err, req_conf))
			goto end;
		p=NCONF_get_string(req_conf,NULL,"oid_file");
		if (p == NULL)
			ERR_clear_error();
		if (p != NULL)
			{
			BIO *oid_bio;

			oid_bio=BIO_new_file(p,"r");
			if (oid_bio == NULL) 
				{
				/*
				BIO_printf(bio_err,"problems opening %s for extra oid's\n",p);
				ERR_print_errors(bio_err);
				*/
				}
			else
				{
				OBJ_create_objects(oid_bio);
				BIO_free(oid_bio);
				}
			}
		}
	if(!add_oid_section(bio_err, req_conf)) goto end;

	if (md_alg == NULL)
		{
		p=NCONF_get_string(req_conf,SECTION,"default_md");
		if (p == NULL)
			ERR_clear_error();
		if (p != NULL)
			{
			if ((md_alg=EVP_get_digestbyname(p)) != NULL)
				digest=md_alg;
			}
		}

	if (!extensions)
		{
		extensions = NCONF_get_string(req_conf, SECTION, V3_EXTENSIONS);
		if (!extensions)
			ERR_clear_error();
		}
	if (extensions) {
		/* Check syntax of file */
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, req_conf);
		if(!X509V3_EXT_add_nconf(req_conf, &ctx, extensions, NULL)) {
			BIO_printf(bio_err,
			 "Error Loading extension section %s\n", extensions);
			goto end;
		}
	}

	if(!passin)
		{
		passin = NCONF_get_string(req_conf, SECTION, "input_password");
		if (!passin)
			ERR_clear_error();
		}
	
	if(!passout)
		{
		passout = NCONF_get_string(req_conf, SECTION, "output_password");
		if (!passout)
			ERR_clear_error();
		}

	p = NCONF_get_string(req_conf, SECTION, STRING_MASK);
	if (!p)
		ERR_clear_error();

	if(p && !ASN1_STRING_set_default_mask_asc(p)) {
		BIO_printf(bio_err, "Invalid global string mask setting %s\n", p);
		goto end;
	}

	if (chtype != MBSTRING_UTF8)
		{
		p = NCONF_get_string(req_conf, SECTION, UTF8_IN);
		if (!p)
			ERR_clear_error();
		else if (!strcmp(p, "yes"))
			chtype = MBSTRING_UTF8;
		}


	if(!req_exts)
		{
		req_exts = NCONF_get_string(req_conf, SECTION, REQ_EXTENSIONS);
		if (!req_exts)
			ERR_clear_error();
		}
	if(req_exts) {
		/* Check syntax of file */
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, req_conf);
		if(!X509V3_EXT_add_nconf(req_conf, &ctx, req_exts, NULL)) {
			BIO_printf(bio_err,
			 "Error Loading request extension section %s\n",
								req_exts);
			goto end;
		}
	}

	in=BIO_new(BIO_s_file());
	out=BIO_new(BIO_s_file());
	if ((in == NULL) || (out == NULL))
		goto end;

#ifndef OPENSSL_NO_ENGINE
        e = setup_engine(bio_err, engine, 0);
#endif

	if (keyfile != NULL)
		{
		pkey = load_key(bio_err, keyfile, keyform, 0, passin, e,
			"Private Key");
		if (!pkey)
			{
			/* load_key() has already printed an appropriate
			   message */
			goto end;
			}
		else
			{
			char *randfile = NCONF_get_string(req_conf,SECTION,"RANDFILE");
			if (randfile == NULL)
				ERR_clear_error();
			app_RAND_load_file(randfile, bio_err, 0);
			}
		}

	if (newreq && (pkey == NULL))
		{
		char *randfile = NCONF_get_string(req_conf,SECTION,"RANDFILE");
		if (randfile == NULL)
			ERR_clear_error();
		app_RAND_load_file(randfile, bio_err, 0);
		if (inrand)
			app_RAND_load_files(inrand);

		if (!NCONF_get_number(req_conf,SECTION,BITS, &newkey))
			{
			newkey=DEFAULT_KEY_LENGTH;
			}

		if (keyalg)
			{
			genctx = set_keygen_ctx(bio_err, keyalg, &pkey_type, &newkey,
							&keyalgstr, gen_eng);
			if (!genctx)
				goto end;
			}
	
		if (newkey < MIN_KEY_LENGTH && (pkey_type == EVP_PKEY_RSA || pkey_type == EVP_PKEY_DSA))
			{
			BIO_printf(bio_err,"private key length is too short,\n");
			BIO_printf(bio_err,"it needs to be at least %d bits, not %ld\n",MIN_KEY_LENGTH,newkey);
			goto end;
			}

		if (!genctx)
			{
			genctx = set_keygen_ctx(bio_err, NULL, &pkey_type, &newkey,
							&keyalgstr, gen_eng);
			if (!genctx)
				goto end;
			}

		if (pkeyopts)
			{
			char *genopt;
			for (i = 0; i < sk_OPENSSL_STRING_num(pkeyopts); i++)
				{
				genopt = sk_OPENSSL_STRING_value(pkeyopts, i);
				if (pkey_ctrl_string(genctx, genopt) <= 0)
					{
					BIO_printf(bio_err,
						"parameter error \"%s\"\n",
						genopt);
					ERR_print_errors(bio_err);
					goto end;
					}
				}
			}

		BIO_printf(bio_err,"Generating a %ld bit %s private key\n",
				newkey, keyalgstr);

		EVP_PKEY_CTX_set_cb(genctx, genpkey_cb);
		EVP_PKEY_CTX_set_app_data(genctx, bio_err);

		if (EVP_PKEY_keygen(genctx, &pkey) <= 0)
			{
			BIO_puts(bio_err, "Error Generating Key\n");
			goto end;
			}

		EVP_PKEY_CTX_free(genctx);
		genctx = NULL;

		app_RAND_write_file(randfile, bio_err);

		if (keyout == NULL)
			{
			keyout=NCONF_get_string(req_conf,SECTION,KEYFILE);
			if (keyout == NULL)
				ERR_clear_error();
			}
		
		if (keyout == NULL)
			{
			BIO_printf(bio_err,"writing new private key to stdout\n");
			BIO_set_fp(out,stdout,BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
			{
			BIO *tmpbio = BIO_new(BIO_f_linebuffer());
			out = BIO_push(tmpbio, out);
			}
#endif
			}
		else
			{
			BIO_printf(bio_err,"writing new private key to '%s'\n",keyout);
			if (BIO_write_filename(out,keyout) <= 0)
				{
				perror(keyout);
				goto end;
				}
			}

		p=NCONF_get_string(req_conf,SECTION,"encrypt_rsa_key");
		if (p == NULL)
			{
			ERR_clear_error();
			p=NCONF_get_string(req_conf,SECTION,"encrypt_key");
			if (p == NULL)
				ERR_clear_error();
			}
		if ((p != NULL) && (strcmp(p,"no") == 0))
			cipher=NULL;
		if (nodes) cipher=NULL;
		
		i=0;
loop:
		if (!PEM_write_bio_PrivateKey(out,pkey,cipher,
			NULL,0,NULL,passout))
			{
			if ((ERR_GET_REASON(ERR_peek_error()) ==
				PEM_R_PROBLEMS_GETTING_PASSWORD) && (i < 3))
				{
				ERR_clear_error();
				i++;
				goto loop;
				}
			goto end;
			}
		BIO_printf(bio_err,"-----\n");
		}

	if (!newreq)
		{
		/* Since we are using a pre-existing certificate
		 * request, the kludge 'format' info should not be
		 * changed. */
		kludge= -1;
		if (infile == NULL)
			BIO_set_fp(in,stdin,BIO_NOCLOSE);
		else
			{
			if (BIO_read_filename(in,infile) <= 0)
				{
				perror(infile);
				goto end;
				}
			}

		if	(informat == FORMAT_ASN1)
			req=d2i_X509_REQ_bio(in,NULL);
		else if (informat == FORMAT_PEM)
			req=PEM_read_bio_X509_REQ(in,NULL,NULL,NULL);
		else
			{
			BIO_printf(bio_err,"bad input format specified for X509 request\n");
			goto end;
			}
		if (req == NULL)
			{
			BIO_printf(bio_err,"unable to load X509 request\n");
			goto end;
			}
		}

	if (newreq || x509)
		{
		if (pkey == NULL)
			{
			BIO_printf(bio_err,"you need to specify a private key\n");
			goto end;
			}

		if (req == NULL)
			{
			req=X509_REQ_new();
			if (req == NULL)
				{
				goto end;
				}

			i=make_REQ(req,pkey,subj,multirdn,!x509, chtype);
			subj=NULL; /* done processing '-subj' option */
			if ((kludge > 0) && !sk_X509_ATTRIBUTE_num(req->req_info->attributes))
				{
				sk_X509_ATTRIBUTE_free(req->req_info->attributes);
				req->req_info->attributes = NULL;
				}
			if (!i)
				{
				BIO_printf(bio_err,"problems making Certificate Request\n");
				goto end;
				}
			}
		if (x509)
			{
			EVP_PKEY *tmppkey;
			X509V3_CTX ext_ctx;
			if ((x509ss=X509_new()) == NULL) goto end;

			/* Set version to V3 */
			if(extensions && !X509_set_version(x509ss, 2)) goto end;
			if (serial)
				{
				if (!X509_set_serialNumber(x509ss, serial)) goto end;
				}
			else
				{
				if (!rand_serial(NULL,
					X509_get_serialNumber(x509ss)))
						goto end;
				}

			if (!X509_set_issuer_name(x509ss, X509_REQ_get_subject_name(req))) goto end;
			if (!X509_gmtime_adj(X509_get_notBefore(x509ss),0)) goto end;
			if (!X509_time_adj_ex(X509_get_notAfter(x509ss), days, 0, NULL)) goto end;
			if (!X509_set_subject_name(x509ss, X509_REQ_get_subject_name(req))) goto end;
			tmppkey = X509_REQ_get_pubkey(req);
			if (!tmppkey || !X509_set_pubkey(x509ss,tmppkey)) goto end;
			EVP_PKEY_free(tmppkey);

			/* Set up V3 context struct */

			X509V3_set_ctx(&ext_ctx, x509ss, x509ss, NULL, NULL, 0);
			X509V3_set_nconf(&ext_ctx, req_conf);

			/* Add extensions */
			if(extensions && !X509V3_EXT_add_nconf(req_conf, 
				 	&ext_ctx, extensions, x509ss))
				{
				BIO_printf(bio_err,
					"Error Loading extension section %s\n",
					extensions);
				goto end;
				}

			i=do_X509_sign(bio_err, x509ss, pkey, digest, sigopts);
			if (!i)
				{
				ERR_print_errors(bio_err);
				goto end;
				}
			}
		else
			{
			X509V3_CTX ext_ctx;

			/* Set up V3 context struct */

			X509V3_set_ctx(&ext_ctx, NULL, NULL, req, NULL, 0);
			X509V3_set_nconf(&ext_ctx, req_conf);

			/* Add extensions */
			if(req_exts && !X509V3_EXT_REQ_add_nconf(req_conf, 
				 	&ext_ctx, req_exts, req))
				{
				BIO_printf(bio_err,
					"Error Loading extension section %s\n",
					req_exts);
				goto end;
				}
			i=do_X509_REQ_sign(bio_err, req, pkey, digest, sigopts);
			if (!i)
				{
				ERR_print_errors(bio_err);
				goto end;
				}
			}
		}

	if (subj && x509)
		{
		BIO_printf(bio_err, "Cannot modifiy certificate subject\n");
		goto end;
		}

	if (subj && !x509)
		{
		if (verbose)
			{
			BIO_printf(bio_err, "Modifying Request's Subject\n");
			print_name(bio_err, "old subject=", X509_REQ_get_subject_name(req), nmflag);
			}

		if (build_subject(req, subj, chtype, multirdn) == 0)
			{
			BIO_printf(bio_err, "ERROR: cannot modify subject\n");
			ex=1;
			goto end;
			}

		req->req_info->enc.modified = 1;

		if (verbose)
			{
			print_name(bio_err, "new subject=", X509_REQ_get_subject_name(req), nmflag);
			}
		}

	if (verify && !x509)
		{
		int tmp=0;

		if (pkey == NULL)
			{
			pkey=X509_REQ_get_pubkey(req);
			tmp=1;
			if (pkey == NULL) goto end;
			}

		i=X509_REQ_verify(req,pkey);
		if (tmp) {
			EVP_PKEY_free(pkey);
			pkey=NULL;
		}

		if (i < 0)
			{
			goto end;
			}
		else if (i == 0)
			{
			BIO_printf(bio_err,"verify failure\n");
			ERR_print_errors(bio_err);
			}
		else /* if (i > 0) */
			BIO_printf(bio_err,"verify OK\n");
		}

	if (noout && !text && !modulus && !subject && !pubkey)
		{
		ex=0;
		goto end;
		}

	if (outfile == NULL)
		{
		BIO_set_fp(out,stdout,BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
		{
		BIO *tmpbio = BIO_new(BIO_f_linebuffer());
		out = BIO_push(tmpbio, out);
		}
#endif
		}
	else
		{
		if ((keyout != NULL) && (strcmp(outfile,keyout) == 0))
			i=(int)BIO_append_filename(out,outfile);
		else
			i=(int)BIO_write_filename(out,outfile);
		if (!i)
			{
			perror(outfile);
			goto end;
			}
		}

	if (pubkey)
		{
		EVP_PKEY *tpubkey; 
		tpubkey=X509_REQ_get_pubkey(req);
		if (tpubkey == NULL)
			{
			BIO_printf(bio_err,"Error getting public key\n");
			ERR_print_errors(bio_err);
			goto end;
			}
		PEM_write_bio_PUBKEY(out, tpubkey);
		EVP_PKEY_free(tpubkey);
		}

	if (text)
		{
		if (x509)
			X509_print_ex(out, x509ss, nmflag, reqflag);
		else	
			X509_REQ_print_ex(out, req, nmflag, reqflag);
		}

	if(subject) 
		{
		if(x509)
			print_name(out, "subject=", X509_get_subject_name(x509ss), nmflag);
		else
			print_name(out, "subject=", X509_REQ_get_subject_name(req), nmflag);
		}

	if (modulus)
		{
		EVP_PKEY *tpubkey;

		if (x509)
			tpubkey=X509_get_pubkey(x509ss);
		else
			tpubkey=X509_REQ_get_pubkey(req);
		if (tpubkey == NULL)
			{
			fprintf(stdout,"Modulus=unavailable\n");
			goto end; 
			}
		fprintf(stdout,"Modulus=");
#ifndef OPENSSL_NO_RSA
		if (EVP_PKEY_base_id(tpubkey) == EVP_PKEY_RSA)
			BN_print(out,tpubkey->pkey.rsa->n);
		else
#endif
			fprintf(stdout,"Wrong Algorithm type");
		EVP_PKEY_free(tpubkey);
		fprintf(stdout,"\n");
		}

	if (!noout && !x509)
		{
		if 	(outformat == FORMAT_ASN1)
			i=i2d_X509_REQ_bio(out,req);
		else if (outformat == FORMAT_PEM) {
			if(newhdr) i=PEM_write_bio_X509_REQ_NEW(out,req);
			else i=PEM_write_bio_X509_REQ(out,req);
		} else {
			BIO_printf(bio_err,"bad output format specified for outfile\n");
			goto end;
			}
		if (!i)
			{
			BIO_printf(bio_err,"unable to write X509 request\n");
			goto end;
			}
		}
	if (!noout && x509 && (x509ss != NULL))
		{
		if 	(outformat == FORMAT_ASN1)
			i=i2d_X509_bio(out,x509ss);
		else if (outformat == FORMAT_PEM)
			i=PEM_write_bio_X509(out,x509ss);
		else	{
			BIO_printf(bio_err,"bad output format specified for outfile\n");
			goto end;
			}
		if (!i)
			{
			BIO_printf(bio_err,"unable to write X509 certificate\n");
			goto end;
			}
		}
	ex=0;
end:
#ifndef MONOLITH
	if(to_free)
		OPENSSL_free(to_free);
#endif
	if (ex)
		{
		ERR_print_errors(bio_err);
		}
	if ((req_conf != NULL) && (req_conf != config)) NCONF_free(req_conf);
	BIO_free(in);
	BIO_free_all(out);
	EVP_PKEY_free(pkey);
	if (genctx)
		EVP_PKEY_CTX_free(genctx);
	if (pkeyopts)
		sk_OPENSSL_STRING_free(pkeyopts);
	if (sigopts)
		sk_OPENSSL_STRING_free(sigopts);
#ifndef OPENSSL_NO_ENGINE
	if (gen_eng)
		ENGINE_free(gen_eng);
#endif
	if (keyalgstr)
		OPENSSL_free(keyalgstr);
	X509_REQ_free(req);
	X509_free(x509ss);
	ASN1_INTEGER_free(serial);
	if(passargin && passin) OPENSSL_free(passin);
	if(passargout && passout) OPENSSL_free(passout);
	OBJ_cleanup();
	apps_shutdown();
	OPENSSL_EXIT(ex);
	}

static int make_REQ(X509_REQ *req, EVP_PKEY *pkey, char *subj, int multirdn,
			int attribs, unsigned long chtype)
	{
	int ret=0,i;
	char no_prompt = 0;
	STACK_OF(CONF_VALUE) *dn_sk, *attr_sk = NULL;
	char *tmp, *dn_sect,*attr_sect;

	tmp=NCONF_get_string(req_conf,SECTION,PROMPT);
	if (tmp == NULL)
		ERR_clear_error();
	if((tmp != NULL) && !strcmp(tmp, "no")) no_prompt = 1;

	dn_sect=NCONF_get_string(req_conf,SECTION,DISTINGUISHED_NAME);
	if (dn_sect == NULL)
		{
		BIO_printf(bio_err,"unable to find '%s' in config\n",
			DISTINGUISHED_NAME);
		goto err;
		}
	dn_sk=NCONF_get_section(req_conf,dn_sect);
	if (dn_sk == NULL)
		{
		BIO_printf(bio_err,"unable to get '%s' section\n",dn_sect);
		goto err;
		}

	attr_sect=NCONF_get_string(req_conf,SECTION,ATTRIBUTES);
	if (attr_sect == NULL)
		{
		ERR_clear_error();		
		attr_sk=NULL;
		}
	else
		{
		attr_sk=NCONF_get_section(req_conf,attr_sect);
		if (attr_sk == NULL)
			{
			BIO_printf(bio_err,"unable to get '%s' section\n",attr_sect);
			goto err;
			}
		}

	/* setup version number */
	if (!X509_REQ_set_version(req,0L)) goto err; /* version 1 */

	if (no_prompt) 
		i = auto_info(req, dn_sk, attr_sk, attribs, chtype);
	else 
		{
		if (subj)
			i = build_subject(req, subj, chtype, multirdn);
		else
			i = prompt_info(req, dn_sk, dn_sect, attr_sk, attr_sect, attribs, chtype);
		}
	if(!i) goto err;

	if (!X509_REQ_set_pubkey(req,pkey)) goto err;

	ret=1;
err:
	return(ret);
	}

/*
 * subject is expected to be in the format /type0=value0/type1=value1/type2=...
 * where characters may be escaped by \
 */
static int build_subject(X509_REQ *req, char *subject, unsigned long chtype, int multirdn)
	{
	X509_NAME *n;

	if (!(n = parse_name(subject, chtype, multirdn)))
		return 0;

	if (!X509_REQ_set_subject_name(req, n))
		{
		X509_NAME_free(n);
		return 0;
		}
	X509_NAME_free(n);
	return 1;
}


static int prompt_info(X509_REQ *req,
		STACK_OF(CONF_VALUE) *dn_sk, char *dn_sect,
		STACK_OF(CONF_VALUE) *attr_sk, char *attr_sect, int attribs,
		unsigned long chtype)
	{
	int i;
	char *p,*q;
	char buf[100];
	int nid, mval;
	long n_min,n_max;
	char *type, *value;
	const char *def;
	CONF_VALUE *v;
	X509_NAME *subj;
	subj = X509_REQ_get_subject_name(req);

	if(!batch)
		{
		BIO_printf(bio_err,"You are about to be asked to enter information that will be incorporated\n");
		BIO_printf(bio_err,"into your certificate request.\n");
		BIO_printf(bio_err,"What you are about to enter is what is called a Distinguished Name or a DN.\n");
		BIO_printf(bio_err,"There are quite a few fields but you can leave some blank\n");
		BIO_printf(bio_err,"For some fields there will be a default value,\n");
		BIO_printf(bio_err,"If you enter '.', the field will be left blank.\n");
		BIO_printf(bio_err,"-----\n");
		}


	if (sk_CONF_VALUE_num(dn_sk))
		{
		i= -1;
start:		for (;;)
			{
			i++;
			if (sk_CONF_VALUE_num(dn_sk) <= i) break;

			v=sk_CONF_VALUE_value(dn_sk,i);
			p=q=NULL;
			type=v->name;
			if(!check_end(type,"_min") || !check_end(type,"_max") ||
				!check_end(type,"_default") ||
					 !check_end(type,"_value")) continue;
			/* Skip past any leading X. X: X, etc to allow for
			 * multiple instances 
			 */
			for(p = v->name; *p ; p++) 
				if ((*p == ':') || (*p == ',') ||
							 (*p == '.')) {
					p++;
					if(*p) type = p;
					break;
				}
			if (*type == '+')
				{
				mval = -1;
				type++;
				}
			else
				mval = 0;
			/* If OBJ not recognised ignore it */
			if ((nid=OBJ_txt2nid(type)) == NID_undef) goto start;
			if (BIO_snprintf(buf,sizeof buf,"%s_default",v->name)
				>= (int)sizeof(buf))
			   {
			   BIO_printf(bio_err,"Name '%s' too long\n",v->name);
			   return 0;
			   }

			if ((def=NCONF_get_string(req_conf,dn_sect,buf)) == NULL)
				{
				ERR_clear_error();
				def="";
				}
				
			BIO_snprintf(buf,sizeof buf,"%s_value",v->name);
			if ((value=NCONF_get_string(req_conf,dn_sect,buf)) == NULL)
				{
				ERR_clear_error();
				value=NULL;
				}

			BIO_snprintf(buf,sizeof buf,"%s_min",v->name);
			if (!NCONF_get_number(req_conf,dn_sect,buf, &n_min))
				{
				ERR_clear_error();
				n_min = -1;
				}

			BIO_snprintf(buf,sizeof buf,"%s_max",v->name);
			if (!NCONF_get_number(req_conf,dn_sect,buf, &n_max))
				{
				ERR_clear_error();
				n_max = -1;
				}

			if (!add_DN_object(subj,v->value,def,value,nid,
				n_min,n_max, chtype, mval))
				return 0;
			}
		if (X509_NAME_entry_count(subj) == 0)
			{
			BIO_printf(bio_err,"error, no objects specified in config file\n");
			return 0;
			}

		if (attribs)
			{
			if ((attr_sk != NULL) && (sk_CONF_VALUE_num(attr_sk) > 0) && (!batch))
				{
				BIO_printf(bio_err,"\nPlease enter the following 'extra' attributes\n");
				BIO_printf(bio_err,"to be sent with your certificate request\n");
				}

			i= -1;
start2:			for (;;)
				{
				i++;
				if ((attr_sk == NULL) ||
					    (sk_CONF_VALUE_num(attr_sk) <= i))
					break;

				v=sk_CONF_VALUE_value(attr_sk,i);
				type=v->name;
				if ((nid=OBJ_txt2nid(type)) == NID_undef)
					goto start2;

				if (BIO_snprintf(buf,sizeof buf,"%s_default",type)
					>= (int)sizeof(buf))
				   {
				   BIO_printf(bio_err,"Name '%s' too long\n",v->name);
				   return 0;
				   }

				if ((def=NCONF_get_string(req_conf,attr_sect,buf))
					== NULL)
					{
					ERR_clear_error();
					def="";
					}
				
				
				BIO_snprintf(buf,sizeof buf,"%s_value",type);
				if ((value=NCONF_get_string(req_conf,attr_sect,buf))
					== NULL)
					{
					ERR_clear_error();
					value=NULL;
					}

				BIO_snprintf(buf,sizeof buf,"%s_min",type);
				if (!NCONF_get_number(req_conf,attr_sect,buf, &n_min))
					{
					ERR_clear_error();
					n_min = -1;
					}

				BIO_snprintf(buf,sizeof buf,"%s_max",type);
				if (!NCONF_get_number(req_conf,attr_sect,buf, &n_max))
					{
					ERR_clear_error();
					n_max = -1;
					}

				if (!add_attribute_object(req,
					v->value,def,value,nid,n_min,n_max, chtype))
					return 0;
				}
			}
		}
	else
		{
		BIO_printf(bio_err,"No template, please set one up.\n");
		return 0;
		}

	return 1;

	}

static int auto_info(X509_REQ *req, STACK_OF(CONF_VALUE) *dn_sk,
			STACK_OF(CONF_VALUE) *attr_sk, int attribs, unsigned long chtype)
	{
	int i;
	char *p,*q;
	char *type;
	CONF_VALUE *v;
	X509_NAME *subj;

	subj = X509_REQ_get_subject_name(req);

	for (i = 0; i < sk_CONF_VALUE_num(dn_sk); i++)
		{
		int mval;
		v=sk_CONF_VALUE_value(dn_sk,i);
		p=q=NULL;
		type=v->name;
		/* Skip past any leading X. X: X, etc to allow for
		 * multiple instances 
		 */
		for(p = v->name; *p ; p++) 
#ifndef CHARSET_EBCDIC
			if ((*p == ':') || (*p == ',') || (*p == '.')) {
#else
			if ((*p == os_toascii[':']) || (*p == os_toascii[',']) || (*p == os_toascii['.'])) {
#endif
				p++;
				if(*p) type = p;
				break;
			}
#ifndef CHARSET_EBCDIC
		if (*p == '+')
#else
		if (*p == os_toascii['+'])
#endif
			{
			p++;
			mval = -1;
			}
		else
			mval = 0;
		if (!X509_NAME_add_entry_by_txt(subj,type, chtype,
				(unsigned char *) v->value,-1,-1,mval)) return 0;

		}

		if (!X509_NAME_entry_count(subj))
			{
			BIO_printf(bio_err,"error, no objects specified in config file\n");
			return 0;
			}
		if (attribs)
			{
			for (i = 0; i < sk_CONF_VALUE_num(attr_sk); i++)
				{
				v=sk_CONF_VALUE_value(attr_sk,i);
				if(!X509_REQ_add1_attr_by_txt(req, v->name, chtype,
					(unsigned char *)v->value, -1)) return 0;
				}
			}
	return 1;
	}


static int add_DN_object(X509_NAME *n, char *text, const char *def, char *value,
	     int nid, int n_min, int n_max, unsigned long chtype, int mval)
	{
	int i,ret=0;
	MS_STATIC char buf[1024];
start:
	if (!batch) BIO_printf(bio_err,"%s [%s]:",text,def);
	(void)BIO_flush(bio_err);
	if(value != NULL)
		{
		BUF_strlcpy(buf,value,sizeof buf);
		BUF_strlcat(buf,"\n",sizeof buf);
		BIO_printf(bio_err,"%s\n",value);
		}
	else
		{
		buf[0]='\0';
		if (!batch)
			{
			if (!fgets(buf,sizeof buf,stdin))
				return 0;
			}
		else
			{
			buf[0] = '\n';
			buf[1] = '\0';
			}
		}

	if (buf[0] == '\0') return(0);
	else if (buf[0] == '\n')
		{
		if ((def == NULL) || (def[0] == '\0'))
			return(1);
		BUF_strlcpy(buf,def,sizeof buf);
		BUF_strlcat(buf,"\n",sizeof buf);
		}
	else if ((buf[0] == '.') && (buf[1] == '\n')) return(1);

	i=strlen(buf);
	if (buf[i-1] != '\n')
		{
		BIO_printf(bio_err,"weird input :-(\n");
		return(0);
		}
	buf[--i]='\0';
#ifdef CHARSET_EBCDIC
	ebcdic2ascii(buf, buf, i);
#endif
	if(!req_check_len(i, n_min, n_max)) goto start;
	if (!X509_NAME_add_entry_by_NID(n,nid, chtype,
				(unsigned char *) buf, -1,-1,mval)) goto err;
	ret=1;
err:
	return(ret);
	}

static int add_attribute_object(X509_REQ *req, char *text, const char *def,
				char *value, int nid, int n_min,
				int n_max, unsigned long chtype)
	{
	int i;
	static char buf[1024];

start:
	if (!batch) BIO_printf(bio_err,"%s [%s]:",text,def);
	(void)BIO_flush(bio_err);
	if (value != NULL)
		{
		BUF_strlcpy(buf,value,sizeof buf);
		BUF_strlcat(buf,"\n",sizeof buf);
		BIO_printf(bio_err,"%s\n",value);
		}
	else
		{
		buf[0]='\0';
		if (!batch)
			{
			if (!fgets(buf,sizeof buf,stdin))
				return 0;
			}
		else
			{
			buf[0] = '\n';
			buf[1] = '\0';
			}
		}

	if (buf[0] == '\0') return(0);
	else if (buf[0] == '\n')
		{
		if ((def == NULL) || (def[0] == '\0'))
			return(1);
		BUF_strlcpy(buf,def,sizeof buf);
		BUF_strlcat(buf,"\n",sizeof buf);
		}
	else if ((buf[0] == '.') && (buf[1] == '\n')) return(1);

	i=strlen(buf);
	if (buf[i-1] != '\n')
		{
		BIO_printf(bio_err,"weird input :-(\n");
		return(0);
		}
	buf[--i]='\0';
#ifdef CHARSET_EBCDIC
	ebcdic2ascii(buf, buf, i);
#endif
	if(!req_check_len(i, n_min, n_max)) goto start;

	if(!X509_REQ_add1_attr_by_NID(req, nid, chtype,
					(unsigned char *)buf, -1)) {
		BIO_printf(bio_err, "Error adding attribute\n");
		ERR_print_errors(bio_err);
		goto err;
	}

	return(1);
err:
	return(0);
	}

static int req_check_len(int len, int n_min, int n_max)
	{
	if ((n_min > 0) && (len < n_min))
		{
		BIO_printf(bio_err,"string is too short, it needs to be at least %d bytes long\n",n_min);
		return(0);
		}
	if ((n_max >= 0) && (len > n_max))
		{
		BIO_printf(bio_err,"string is too long, it needs to be less than  %d bytes long\n",n_max);
		return(0);
		}
	return(1);
	}

/* Check if the end of a string matches 'end' */
static int check_end(const char *str, const char *end)
{
	int elen, slen;	
	const char *tmp;
	elen = strlen(end);
	slen = strlen(str);
	if(elen > slen) return 1;
	tmp = str + slen - elen;
	return strcmp(tmp, end);
}

static EVP_PKEY_CTX *set_keygen_ctx(BIO *err, const char *gstr, int *pkey_type,
					long *pkeylen, char **palgnam,
					ENGINE *keygen_engine)
	{
	EVP_PKEY_CTX *gctx = NULL;
	EVP_PKEY *param = NULL;
	long keylen = -1;
	BIO *pbio = NULL;
	const char *paramfile = NULL;

	if (gstr == NULL)
		{
		*pkey_type = EVP_PKEY_RSA;
		keylen = *pkeylen;
		}
	else if (gstr[0] >= '0' && gstr[0] <= '9')
		{
		*pkey_type = EVP_PKEY_RSA;
		keylen = atol(gstr);
		*pkeylen = keylen;
		}
	else if (!strncmp(gstr, "param:", 6))
		paramfile = gstr + 6;
	else
		{
		const char *p = strchr(gstr, ':');
		int len;
		ENGINE *tmpeng;
		const EVP_PKEY_ASN1_METHOD *ameth;

		if (p)
			len = p - gstr;
		else
			len = strlen(gstr);
		/* The lookup of a the string will cover all engines so
		 * keep a note of the implementation.
		 */

		ameth = EVP_PKEY_asn1_find_str(&tmpeng, gstr, len);

		if (!ameth)
			{
			BIO_printf(err, "Unknown algorithm %.*s\n", len, gstr);
			return NULL;
			}

		EVP_PKEY_asn1_get0_info(NULL, pkey_type, NULL, NULL, NULL,
									ameth);
#ifndef OPENSSL_NO_ENGINE
		if (tmpeng)
			ENGINE_finish(tmpeng);
#endif
		if (*pkey_type == EVP_PKEY_RSA)
			{
			if (p)
				{
				keylen = atol(p + 1);
				*pkeylen = keylen;
				}
			else
				keylen = *pkeylen;
			}
		else if (p)
			paramfile = p + 1;
		}

	if (paramfile)
		{
		pbio = BIO_new_file(paramfile, "r");
		if (!pbio)
			{
			BIO_printf(err, "Can't open parameter file %s\n",
					paramfile);
			return NULL;
			}
		param = PEM_read_bio_Parameters(pbio, NULL);

		if (!param)
			{
			X509 *x;
			(void)BIO_reset(pbio);
			x = PEM_read_bio_X509(pbio, NULL, NULL, NULL);
			if (x)
				{
				param = X509_get_pubkey(x);
				X509_free(x);
				}
			}

		BIO_free(pbio);

		if (!param)
			{
			BIO_printf(err, "Error reading parameter file %s\n",
					paramfile);
			return NULL;
			}
		if (*pkey_type == -1)
			*pkey_type = EVP_PKEY_id(param);
		else if (*pkey_type != EVP_PKEY_base_id(param))
			{
			BIO_printf(err, "Key Type does not match parameters\n");
			EVP_PKEY_free(param);
			return NULL;
			}
		}

	if (palgnam)
		{
		const EVP_PKEY_ASN1_METHOD *ameth;
		ENGINE *tmpeng;
		const char *anam;
		ameth = EVP_PKEY_asn1_find(&tmpeng, *pkey_type);
		if (!ameth)
			{
			BIO_puts(err, "Internal error: can't find key algorithm\n");
			return NULL;
			}
		EVP_PKEY_asn1_get0_info(NULL, NULL, NULL, NULL, &anam, ameth);
		*palgnam = BUF_strdup(anam);
#ifndef OPENSSL_NO_ENGINE
		if (tmpeng)
			ENGINE_finish(tmpeng);
#endif
		}

	if (param)
		{
		gctx = EVP_PKEY_CTX_new(param, keygen_engine);
		*pkeylen = EVP_PKEY_bits(param);
		EVP_PKEY_free(param);
		}
	else
		gctx = EVP_PKEY_CTX_new_id(*pkey_type, keygen_engine);

	if (!gctx)
		{
		BIO_puts(err, "Error allocating keygen context\n");
		ERR_print_errors(err);
		return NULL;
		}

	if (EVP_PKEY_keygen_init(gctx) <= 0)
		{
		BIO_puts(err, "Error initializing keygen context\n");
		ERR_print_errors(err);
		return NULL;
		}
#ifndef OPENSSL_NO_RSA
	if ((*pkey_type == EVP_PKEY_RSA) && (keylen != -1))
		{
		if (EVP_PKEY_CTX_set_rsa_keygen_bits(gctx, keylen) <= 0)
			{
			BIO_puts(err, "Error setting RSA keysize\n");
			ERR_print_errors(err);
			EVP_PKEY_CTX_free(gctx);
			return NULL;
			}
		}
#endif

	return gctx;
	}

static int genpkey_cb(EVP_PKEY_CTX *ctx)
	{
	char c='*';
	BIO *b = EVP_PKEY_CTX_get_app_data(ctx);
	int p;
	p = EVP_PKEY_CTX_get_keygen_info(ctx, 0);
	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	BIO_write(b,&c,1);
	(void)BIO_flush(b);
#ifdef LINT
	p=n;
#endif
	return 1;
	}

static int do_sign_init(BIO *err, EVP_MD_CTX *ctx, EVP_PKEY *pkey,
			const EVP_MD *md, STACK_OF(OPENSSL_STRING) *sigopts)
	{
	EVP_PKEY_CTX *pkctx = NULL;
	int i;
	EVP_MD_CTX_init(ctx);
	if (!EVP_DigestSignInit(ctx, &pkctx, md, NULL, pkey))
		return 0;
	for (i = 0; i < sk_OPENSSL_STRING_num(sigopts); i++)
		{
		char *sigopt = sk_OPENSSL_STRING_value(sigopts, i);
		if (pkey_ctrl_string(pkctx, sigopt) <= 0)
			{
			BIO_printf(err, "parameter error \"%s\"\n", sigopt);
			ERR_print_errors(bio_err);
			return 0;
			}
		}
	return 1;
	}

int do_X509_sign(BIO *err, X509 *x, EVP_PKEY *pkey, const EVP_MD *md,
			STACK_OF(OPENSSL_STRING) *sigopts)
	{
	int rv;
	EVP_MD_CTX mctx;
	EVP_MD_CTX_init(&mctx);
	rv = do_sign_init(err, &mctx, pkey, md, sigopts);
	if (rv > 0)
		rv = X509_sign_ctx(x, &mctx);
	EVP_MD_CTX_cleanup(&mctx);
	return rv > 0 ? 1 : 0;
	}


int do_X509_REQ_sign(BIO *err, X509_REQ *x, EVP_PKEY *pkey, const EVP_MD *md,
			STACK_OF(OPENSSL_STRING) *sigopts)
	{
	int rv;
	EVP_MD_CTX mctx;
	EVP_MD_CTX_init(&mctx);
	rv = do_sign_init(err, &mctx, pkey, md, sigopts);
	if (rv > 0)
		rv = X509_REQ_sign_ctx(x, &mctx);
	EVP_MD_CTX_cleanup(&mctx);
	return rv > 0 ? 1 : 0;
	}
		
	

int do_X509_CRL_sign(BIO *err, X509_CRL *x, EVP_PKEY *pkey, const EVP_MD *md,
			STACK_OF(OPENSSL_STRING) *sigopts)
	{
	int rv;
	EVP_MD_CTX mctx;
	EVP_MD_CTX_init(&mctx);
	rv = do_sign_init(err, &mctx, pkey, md, sigopts);
	if (rv > 0)
		rv = X509_CRL_sign_ctx(x, &mctx);
	EVP_MD_CTX_cleanup(&mctx);
	return rv > 0 ? 1 : 0;
	}
		
	

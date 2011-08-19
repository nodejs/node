/* apps/asn1pars.c */
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

/* A nice addition from Dr Stephen Henson <steve@openssl.org> to 
 * add the -strparse option which parses nested binary structures
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

/* -inform arg	- input format - default PEM (DER or PEM)
 * -in arg	- input file - default stdin
 * -i		- indent the details by depth
 * -offset	- where in the file to start
 * -length	- how many bytes to use
 * -oid file	- extra oid description file
 */

#undef PROG
#define PROG	asn1parse_main

int MAIN(int, char **);

static int do_generate(BIO *bio, char *genstr, char *genconf, BUF_MEM *buf);

int MAIN(int argc, char **argv)
	{
	int i,badops=0,offset=0,ret=1,j;
	unsigned int length=0;
	long num,tmplen;
	BIO *in=NULL,*out=NULL,*b64=NULL, *derout = NULL;
	int informat,indent=0, noout = 0, dump = 0;
	char *infile=NULL,*str=NULL,*prog,*oidfile=NULL, *derfile=NULL;
	char *genstr=NULL, *genconf=NULL;
	unsigned char *tmpbuf;
	const unsigned char *ctmpbuf;
	BUF_MEM *buf=NULL;
	STACK *osk=NULL;
	ASN1_TYPE *at=NULL;

	informat=FORMAT_PEM;

	apps_startup();

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto end;

	prog=argv[0];
	argc--;
	argv++;
	if ((osk=sk_new_null()) == NULL)
		{
		BIO_printf(bio_err,"Memory allocation failure\n");
		goto end;
		}
	while (argc >= 1)
		{
		if 	(strcmp(*argv,"-inform") == 0)
			{
			if (--argc < 1) goto bad;
			informat=str2fmt(*(++argv));
			}
		else if (strcmp(*argv,"-in") == 0)
			{
			if (--argc < 1) goto bad;
			infile= *(++argv);
			}
		else if (strcmp(*argv,"-out") == 0)
			{
			if (--argc < 1) goto bad;
			derfile= *(++argv);
			}
		else if (strcmp(*argv,"-i") == 0)
			{
			indent=1;
			}
		else if (strcmp(*argv,"-noout") == 0) noout = 1;
		else if (strcmp(*argv,"-oid") == 0)
			{
			if (--argc < 1) goto bad;
			oidfile= *(++argv);
			}
		else if (strcmp(*argv,"-offset") == 0)
			{
			if (--argc < 1) goto bad;
			offset= atoi(*(++argv));
			}
		else if (strcmp(*argv,"-length") == 0)
			{
			if (--argc < 1) goto bad;
			length= atoi(*(++argv));
			if (length == 0) goto bad;
			}
		else if (strcmp(*argv,"-dump") == 0)
			{
			dump= -1;
			}
		else if (strcmp(*argv,"-dlimit") == 0)
			{
			if (--argc < 1) goto bad;
			dump= atoi(*(++argv));
			if (dump <= 0) goto bad;
			}
		else if (strcmp(*argv,"-strparse") == 0)
			{
			if (--argc < 1) goto bad;
			sk_push(osk,*(++argv));
			}
		else if (strcmp(*argv,"-genstr") == 0)
			{
			if (--argc < 1) goto bad;
			genstr= *(++argv);
			}
		else if (strcmp(*argv,"-genconf") == 0)
			{
			if (--argc < 1) goto bad;
			genconf= *(++argv);
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
		BIO_printf(bio_err,"%s [options] <infile\n",prog);
		BIO_printf(bio_err,"where options are\n");
		BIO_printf(bio_err," -inform arg   input format - one of DER PEM\n");
		BIO_printf(bio_err," -in arg       input file\n");
		BIO_printf(bio_err," -out arg      output file (output format is always DER\n");
		BIO_printf(bio_err," -noout arg    don't produce any output\n");
		BIO_printf(bio_err," -offset arg   offset into file\n");
		BIO_printf(bio_err," -length arg   length of section in file\n");
		BIO_printf(bio_err," -i            indent entries\n");
		BIO_printf(bio_err," -dump         dump unknown data in hex form\n");
		BIO_printf(bio_err," -dlimit arg   dump the first arg bytes of unknown data in hex form\n");
		BIO_printf(bio_err," -oid file     file of extra oid definitions\n");
		BIO_printf(bio_err," -strparse offset\n");
		BIO_printf(bio_err,"               a series of these can be used to 'dig' into multiple\n");
		BIO_printf(bio_err,"               ASN1 blob wrappings\n");
		BIO_printf(bio_err," -genstr str   string to generate ASN1 structure from\n");
		BIO_printf(bio_err," -genconf file file to generate ASN1 structure from\n");
		goto end;
		}

	ERR_load_crypto_strings();

	in=BIO_new(BIO_s_file());
	out=BIO_new(BIO_s_file());
	if ((in == NULL) || (out == NULL))
		{
		ERR_print_errors(bio_err);
		goto end;
		}
	BIO_set_fp(out,stdout,BIO_NOCLOSE|BIO_FP_TEXT);
#ifdef OPENSSL_SYS_VMS
	{
	BIO *tmpbio = BIO_new(BIO_f_linebuffer());
	out = BIO_push(tmpbio, out);
	}
#endif

	if (oidfile != NULL)
		{
		if (BIO_read_filename(in,oidfile) <= 0)
			{
			BIO_printf(bio_err,"problems opening %s\n",oidfile);
			ERR_print_errors(bio_err);
			goto end;
			}
		OBJ_create_objects(in);
		}

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

	if (derfile) {
		if(!(derout = BIO_new_file(derfile, "wb"))) {
			BIO_printf(bio_err,"problems opening %s\n",derfile);
			ERR_print_errors(bio_err);
			goto end;
		}
	}

	if ((buf=BUF_MEM_new()) == NULL) goto end;
	if (!BUF_MEM_grow(buf,BUFSIZ*8)) goto end; /* Pre-allocate :-) */

	if (genstr || genconf)
		{
		num = do_generate(bio_err, genstr, genconf, buf);
		if (num < 0)
			{
			ERR_print_errors(bio_err);
			goto end;
			}
		}

	else
		{

		if (informat == FORMAT_PEM)
			{
			BIO *tmp;

			if ((b64=BIO_new(BIO_f_base64())) == NULL)
				goto end;
			BIO_push(b64,in);
			tmp=in;
			in=b64;
			b64=tmp;
			}

		num=0;
		for (;;)
			{
			if (!BUF_MEM_grow(buf,(int)num+BUFSIZ)) goto end;
			i=BIO_read(in,&(buf->data[num]),BUFSIZ);
			if (i <= 0) break;
			num+=i;
			}
		}
	str=buf->data;

	/* If any structs to parse go through in sequence */

	if (sk_num(osk))
		{
		tmpbuf=(unsigned char *)str;
		tmplen=num;
		for (i=0; i<sk_num(osk); i++)
			{
			ASN1_TYPE *atmp;
			int typ;
			j=atoi(sk_value(osk,i));
			if (j == 0)
				{
				BIO_printf(bio_err,"'%s' is an invalid number\n",sk_value(osk,i));
				continue;
				}
			tmpbuf+=j;
			tmplen-=j;
			atmp = at;
			ctmpbuf = tmpbuf;
			at = d2i_ASN1_TYPE(NULL,&ctmpbuf,tmplen);
			ASN1_TYPE_free(atmp);
			if(!at)
				{
				BIO_printf(bio_err,"Error parsing structure\n");
				ERR_print_errors(bio_err);
				goto end;
				}
			typ = ASN1_TYPE_get(at);
			if ((typ == V_ASN1_OBJECT)
				|| (typ == V_ASN1_NULL))
				{
				BIO_printf(bio_err, "Can't parse %s type\n",
					typ == V_ASN1_NULL ? "NULL" : "OBJECT");
				ERR_print_errors(bio_err);
				goto end;
				}
			/* hmm... this is a little evil but it works */
			tmpbuf=at->value.asn1_string->data;
			tmplen=at->value.asn1_string->length;
			}
		str=(char *)tmpbuf;
		num=tmplen;
		}

	if (offset >= num)
		{
		BIO_printf(bio_err, "Error: offset too large\n");
		goto end;
		}

	num -= offset;

	if ((length == 0) || ((long)length > num)) length=(unsigned int)num;
	if(derout) {
		if(BIO_write(derout, str + offset, length) != (int)length) {
			BIO_printf(bio_err, "Error writing output\n");
			ERR_print_errors(bio_err);
			goto end;
		}
	}
	if (!noout &&
	    !ASN1_parse_dump(out,(unsigned char *)&(str[offset]),length,
		    indent,dump))
		{
		ERR_print_errors(bio_err);
		goto end;
		}
	ret=0;
end:
	BIO_free(derout);
	if (in != NULL) BIO_free(in);
	if (out != NULL) BIO_free_all(out);
	if (b64 != NULL) BIO_free(b64);
	if (ret != 0)
		ERR_print_errors(bio_err);
	if (buf != NULL) BUF_MEM_free(buf);
	if (at != NULL) ASN1_TYPE_free(at);
	if (osk != NULL) sk_free(osk);
	OBJ_cleanup();
	apps_shutdown();
	OPENSSL_EXIT(ret);
	}

static int do_generate(BIO *bio, char *genstr, char *genconf, BUF_MEM *buf)
	{
	CONF *cnf = NULL;
	int len;
	long errline;
	unsigned char *p;
	ASN1_TYPE *atyp = NULL;

	if (genconf)
		{
		cnf = NCONF_new(NULL);
		if (!NCONF_load(cnf, genconf, &errline))
			goto conferr;
		if (!genstr)
			genstr = NCONF_get_string(cnf, "default", "asn1");
		if (!genstr)
			{
			BIO_printf(bio, "Can't find 'asn1' in '%s'\n", genconf);
			goto err;
			}
		}

	atyp = ASN1_generate_nconf(genstr, cnf);
	NCONF_free(cnf);

	if (!atyp)
		return -1;

	len = i2d_ASN1_TYPE(atyp, NULL);

	if (len <= 0)
		goto err;

	if (!BUF_MEM_grow(buf,len))
		goto err;

	p=(unsigned char *)buf->data;

	i2d_ASN1_TYPE(atyp, &p);

	ASN1_TYPE_free(atyp);
	return len;

	conferr:

	if (errline > 0)
		BIO_printf(bio, "Error on line %ld of config file '%s'\n",
							errline, genconf);
	else
		BIO_printf(bio, "Error loading config file '%s'\n", genconf);

	err:
	NCONF_free(cnf);
	ASN1_TYPE_free(atyp);

	return -1;

	}

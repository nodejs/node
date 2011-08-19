/* crypto/asn1/t_pkey.c */
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
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * Binary polynomial ECC support in OpenSSL originally developed by 
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/objects.h>
#include <openssl/buffer.h>
#include <openssl/bn.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif
#ifndef OPENSSL_NO_EC
#include <openssl/ec.h>
#endif

static int print(BIO *fp,const char *str, const BIGNUM *num,
		unsigned char *buf,int off);
#ifndef OPENSSL_NO_EC
static int print_bin(BIO *fp, const char *str, const unsigned char *num,
		size_t len, int off);
#endif
#ifndef OPENSSL_NO_RSA
#ifndef OPENSSL_NO_FP_API
int RSA_print_fp(FILE *fp, const RSA *x, int off)
	{
	BIO *b;
	int ret;

	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		RSAerr(RSA_F_RSA_PRINT_FP,ERR_R_BUF_LIB);
		return(0);
		}
	BIO_set_fp(b,fp,BIO_NOCLOSE);
	ret=RSA_print(b,x,off);
	BIO_free(b);
	return(ret);
	}
#endif

int RSA_print(BIO *bp, const RSA *x, int off)
	{
	char str[128];
	const char *s;
	unsigned char *m=NULL;
	int ret=0, mod_len = 0;
	size_t buf_len=0, i;

	if (x->n)
		buf_len = (size_t)BN_num_bytes(x->n);
	if (x->e)
		if (buf_len < (i = (size_t)BN_num_bytes(x->e)))
			buf_len = i;
	if (x->d)
		if (buf_len < (i = (size_t)BN_num_bytes(x->d)))
			buf_len = i;
	if (x->p)
		if (buf_len < (i = (size_t)BN_num_bytes(x->p)))
			buf_len = i;
	if (x->q)
		if (buf_len < (i = (size_t)BN_num_bytes(x->q)))
			buf_len = i;
	if (x->dmp1)
		if (buf_len < (i = (size_t)BN_num_bytes(x->dmp1)))
			buf_len = i;
	if (x->dmq1)
		if (buf_len < (i = (size_t)BN_num_bytes(x->dmq1)))
			buf_len = i;
	if (x->iqmp)
		if (buf_len < (i = (size_t)BN_num_bytes(x->iqmp)))
			buf_len = i;

	m=(unsigned char *)OPENSSL_malloc(buf_len+10);
	if (m == NULL)
		{
		RSAerr(RSA_F_RSA_PRINT,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (x->n != NULL)
		mod_len = BN_num_bits(x->n);

	if (x->d != NULL)
		{
		if(!BIO_indent(bp,off,128))
		   goto err;
		if (BIO_printf(bp,"Private-Key: (%d bit)\n", mod_len)
			<= 0) goto err;
		}

	if (x->d == NULL)
		BIO_snprintf(str,sizeof str,"Modulus (%d bit):", mod_len);
	else
		BUF_strlcpy(str,"modulus:",sizeof str);
	if (!print(bp,str,x->n,m,off)) goto err;
	s=(x->d == NULL)?"Exponent:":"publicExponent:";
	if ((x->e != NULL) && !print(bp,s,x->e,m,off))
		goto err;
	if ((x->d != NULL) && !print(bp,"privateExponent:",x->d,m,off))
		goto err;
	if ((x->p != NULL) && !print(bp,"prime1:",x->p,m,off))
		goto err;
	if ((x->q != NULL) && !print(bp,"prime2:",x->q,m,off))
		goto err;
	if ((x->dmp1 != NULL) && !print(bp,"exponent1:",x->dmp1,m,off))
		goto err;
	if ((x->dmq1 != NULL) && !print(bp,"exponent2:",x->dmq1,m,off))
		goto err;
	if ((x->iqmp != NULL) && !print(bp,"coefficient:",x->iqmp,m,off))
		goto err;
	ret=1;
err:
	if (m != NULL) OPENSSL_free(m);
	return(ret);
	}
#endif /* OPENSSL_NO_RSA */

#ifndef OPENSSL_NO_DSA
#ifndef OPENSSL_NO_FP_API
int DSA_print_fp(FILE *fp, const DSA *x, int off)
	{
	BIO *b;
	int ret;

	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		DSAerr(DSA_F_DSA_PRINT_FP,ERR_R_BUF_LIB);
		return(0);
		}
	BIO_set_fp(b,fp,BIO_NOCLOSE);
	ret=DSA_print(b,x,off);
	BIO_free(b);
	return(ret);
	}
#endif

int DSA_print(BIO *bp, const DSA *x, int off)
	{
	unsigned char *m=NULL;
	int ret=0;
	size_t buf_len=0,i;

	if (x->p)
		buf_len = (size_t)BN_num_bytes(x->p);
	else
		{
		DSAerr(DSA_F_DSA_PRINT,DSA_R_MISSING_PARAMETERS);
		goto err;
		}
	if (x->q)
		if (buf_len < (i = (size_t)BN_num_bytes(x->q)))
			buf_len = i;
	if (x->g)
		if (buf_len < (i = (size_t)BN_num_bytes(x->g)))
			buf_len = i;
	if (x->priv_key)
		if (buf_len < (i = (size_t)BN_num_bytes(x->priv_key)))
			buf_len = i;
	if (x->pub_key)
		if (buf_len < (i = (size_t)BN_num_bytes(x->pub_key)))
			buf_len = i;

	m=(unsigned char *)OPENSSL_malloc(buf_len+10);
	if (m == NULL)
		{
		DSAerr(DSA_F_DSA_PRINT,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (x->priv_key != NULL)
		{
		if(!BIO_indent(bp,off,128))
		   goto err;
		if (BIO_printf(bp,"Private-Key: (%d bit)\n",BN_num_bits(x->p))
			<= 0) goto err;
		}

	if ((x->priv_key != NULL) && !print(bp,"priv:",x->priv_key,m,off))
		goto err;
	if ((x->pub_key  != NULL) && !print(bp,"pub: ",x->pub_key,m,off))
		goto err;
	if ((x->p != NULL) && !print(bp,"P:   ",x->p,m,off)) goto err;
	if ((x->q != NULL) && !print(bp,"Q:   ",x->q,m,off)) goto err;
	if ((x->g != NULL) && !print(bp,"G:   ",x->g,m,off)) goto err;
	ret=1;
err:
	if (m != NULL) OPENSSL_free(m);
	return(ret);
	}
#endif /* !OPENSSL_NO_DSA */

#ifndef OPENSSL_NO_EC
#ifndef OPENSSL_NO_FP_API
int ECPKParameters_print_fp(FILE *fp, const EC_GROUP *x, int off)
	{
	BIO *b;
	int ret;

	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		ECerr(EC_F_ECPKPARAMETERS_PRINT_FP,ERR_R_BUF_LIB);
		return(0);
		}
	BIO_set_fp(b, fp, BIO_NOCLOSE);
	ret = ECPKParameters_print(b, x, off);
	BIO_free(b);
	return(ret);
	}

int EC_KEY_print_fp(FILE *fp, const EC_KEY *x, int off)
	{
	BIO *b;
	int ret;
 
	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		ECerr(EC_F_EC_KEY_PRINT_FP, ERR_R_BIO_LIB);
		return(0);
		}
	BIO_set_fp(b, fp, BIO_NOCLOSE);
	ret = EC_KEY_print(b, x, off);
	BIO_free(b);
	return(ret);
	}
#endif

int ECPKParameters_print(BIO *bp, const EC_GROUP *x, int off)
	{
	unsigned char *buffer=NULL;
	size_t	buf_len=0, i;
	int     ret=0, reason=ERR_R_BIO_LIB;
	BN_CTX  *ctx=NULL;
	const EC_POINT *point=NULL;
	BIGNUM	*p=NULL, *a=NULL, *b=NULL, *gen=NULL,
		*order=NULL, *cofactor=NULL;
	const unsigned char *seed;
	size_t	seed_len=0;
	
	static const char *gen_compressed = "Generator (compressed):";
	static const char *gen_uncompressed = "Generator (uncompressed):";
	static const char *gen_hybrid = "Generator (hybrid):";
 
	if (!x)
		{
		reason = ERR_R_PASSED_NULL_PARAMETER;
		goto err;
		}

	if (EC_GROUP_get_asn1_flag(x))
		{
		/* the curve parameter are given by an asn1 OID */
		int nid;

		if (!BIO_indent(bp, off, 128))
			goto err;

		nid = EC_GROUP_get_curve_name(x);
		if (nid == 0)
			goto err;

		if (BIO_printf(bp, "ASN1 OID: %s", OBJ_nid2sn(nid)) <= 0)
			goto err;
		if (BIO_printf(bp, "\n") <= 0)
			goto err;
		}
	else
		{
		/* explicit parameters */
		int is_char_two = 0;
		point_conversion_form_t form;
		int tmp_nid = EC_METHOD_get_field_type(EC_GROUP_method_of(x));

		if (tmp_nid == NID_X9_62_characteristic_two_field)
			is_char_two = 1;

		if ((p = BN_new()) == NULL || (a = BN_new()) == NULL ||
			(b = BN_new()) == NULL || (order = BN_new()) == NULL ||
			(cofactor = BN_new()) == NULL)
			{
			reason = ERR_R_MALLOC_FAILURE;
			goto err;
			}

		if (is_char_two)
			{
			if (!EC_GROUP_get_curve_GF2m(x, p, a, b, ctx))
				{
				reason = ERR_R_EC_LIB;
				goto err;
				}
			}
		else /* prime field */
			{
			if (!EC_GROUP_get_curve_GFp(x, p, a, b, ctx))
				{
				reason = ERR_R_EC_LIB;
				goto err;
				}
			}

		if ((point = EC_GROUP_get0_generator(x)) == NULL)
			{
			reason = ERR_R_EC_LIB;
			goto err;
			}
		if (!EC_GROUP_get_order(x, order, NULL) || 
            		!EC_GROUP_get_cofactor(x, cofactor, NULL))
			{
			reason = ERR_R_EC_LIB;
			goto err;
			}
		
		form = EC_GROUP_get_point_conversion_form(x);

		if ((gen = EC_POINT_point2bn(x, point, 
				form, NULL, ctx)) == NULL)
			{
			reason = ERR_R_EC_LIB;
			goto err;
			}

		buf_len = (size_t)BN_num_bytes(p);
		if (buf_len < (i = (size_t)BN_num_bytes(a)))
			buf_len = i;
		if (buf_len < (i = (size_t)BN_num_bytes(b)))
			buf_len = i;
		if (buf_len < (i = (size_t)BN_num_bytes(gen)))
			buf_len = i;
		if (buf_len < (i = (size_t)BN_num_bytes(order)))
			buf_len = i;
		if (buf_len < (i = (size_t)BN_num_bytes(cofactor))) 
			buf_len = i;

		if ((seed = EC_GROUP_get0_seed(x)) != NULL)
			seed_len = EC_GROUP_get_seed_len(x);

		buf_len += 10;
		if ((buffer = OPENSSL_malloc(buf_len)) == NULL)
			{
			reason = ERR_R_MALLOC_FAILURE;
			goto err;
			}

		if (!BIO_indent(bp, off, 128))
			goto err;

		/* print the 'short name' of the field type */
		if (BIO_printf(bp, "Field Type: %s\n", OBJ_nid2sn(tmp_nid))
			<= 0)
			goto err;  

		if (is_char_two)
			{
			/* print the 'short name' of the base type OID */
			int basis_type = EC_GROUP_get_basis_type(x);
			if (basis_type == 0)
				goto err;

			if (!BIO_indent(bp, off, 128))
				goto err;

			if (BIO_printf(bp, "Basis Type: %s\n", 
				OBJ_nid2sn(basis_type)) <= 0)
				goto err;

			/* print the polynomial */
			if ((p != NULL) && !print(bp, "Polynomial:", p, buffer,
				off))
				goto err;
			}
		else
			{
			if ((p != NULL) && !print(bp, "Prime:", p, buffer,off))
				goto err;
			}
		if ((a != NULL) && !print(bp, "A:   ", a, buffer, off)) 
			goto err;
		if ((b != NULL) && !print(bp, "B:   ", b, buffer, off))
			goto err;
		if (form == POINT_CONVERSION_COMPRESSED)
			{
			if ((gen != NULL) && !print(bp, gen_compressed, gen,
				buffer, off))
				goto err;
			}
		else if (form == POINT_CONVERSION_UNCOMPRESSED)
			{
			if ((gen != NULL) && !print(bp, gen_uncompressed, gen,
				buffer, off))
				goto err;
			}
		else /* form == POINT_CONVERSION_HYBRID */
			{
			if ((gen != NULL) && !print(bp, gen_hybrid, gen,
				buffer, off))
				goto err;
			}
		if ((order != NULL) && !print(bp, "Order: ", order, 
			buffer, off)) goto err;
		if ((cofactor != NULL) && !print(bp, "Cofactor: ", cofactor, 
			buffer, off)) goto err;
		if (seed && !print_bin(bp, "Seed:", seed, seed_len, off))
			goto err;
		}
	ret=1;
err:
	if (!ret)
 		ECerr(EC_F_ECPKPARAMETERS_PRINT, reason);
	if (p) 
		BN_free(p);
	if (a) 
		BN_free(a);
	if (b)
		BN_free(b);
	if (gen)
		BN_free(gen);
	if (order)
		BN_free(order);
	if (cofactor)
		BN_free(cofactor);
	if (ctx)
		BN_CTX_free(ctx);
	if (buffer != NULL) 
		OPENSSL_free(buffer);
	return(ret);	
	}

int EC_KEY_print(BIO *bp, const EC_KEY *x, int off)
	{
	unsigned char *buffer=NULL;
	size_t	buf_len=0, i;
	int     ret=0, reason=ERR_R_BIO_LIB;
	BIGNUM  *pub_key=NULL, *order=NULL;
	BN_CTX  *ctx=NULL;
	const EC_GROUP *group;
	const EC_POINT *public_key;
	const BIGNUM *priv_key;
 
	if (x == NULL || (group = EC_KEY_get0_group(x)) == NULL)
		{
		reason = ERR_R_PASSED_NULL_PARAMETER;
		goto err;
		}

	public_key = EC_KEY_get0_public_key(x);
	if ((pub_key = EC_POINT_point2bn(group, public_key,
		EC_KEY_get_conv_form(x), NULL, ctx)) == NULL)
		{
		reason = ERR_R_EC_LIB;
		goto err;
		}

	buf_len = (size_t)BN_num_bytes(pub_key);
	priv_key = EC_KEY_get0_private_key(x);
	if (priv_key != NULL)
		{
		if ((i = (size_t)BN_num_bytes(priv_key)) > buf_len)
			buf_len = i;
		}

	buf_len += 10;
	if ((buffer = OPENSSL_malloc(buf_len)) == NULL)
		{
		reason = ERR_R_MALLOC_FAILURE;
		goto err;
		}

	if (priv_key != NULL)
		{
		if (!BIO_indent(bp, off, 128))
			goto err;
		if ((order = BN_new()) == NULL)
			goto err;
		if (!EC_GROUP_get_order(group, order, NULL))
			goto err;
		if (BIO_printf(bp, "Private-Key: (%d bit)\n", 
			BN_num_bits(order)) <= 0) goto err;
		}
  
	if ((priv_key != NULL) && !print(bp, "priv:", priv_key, 
		buffer, off))
		goto err;
	if ((pub_key != NULL) && !print(bp, "pub: ", pub_key,
		buffer, off))
		goto err;
	if (!ECPKParameters_print(bp, group, off))
		goto err;
	ret=1;
err:
	if (!ret)
 		ECerr(EC_F_EC_KEY_PRINT, reason);
	if (pub_key) 
		BN_free(pub_key);
	if (order)
		BN_free(order);
	if (ctx)
		BN_CTX_free(ctx);
	if (buffer != NULL)
		OPENSSL_free(buffer);
	return(ret);
	}
#endif /* OPENSSL_NO_EC */

static int print(BIO *bp, const char *number, const BIGNUM *num, unsigned char *buf,
	     int off)
	{
	int n,i;
	const char *neg;

	if (num == NULL) return(1);
	neg = (BN_is_negative(num))?"-":"";
	if(!BIO_indent(bp,off,128))
		return 0;
	if (BN_is_zero(num))
		{
		if (BIO_printf(bp, "%s 0\n", number) <= 0)
			return 0;
		return 1;
		}

	if (BN_num_bytes(num) <= BN_BYTES)
		{
		if (BIO_printf(bp,"%s %s%lu (%s0x%lx)\n",number,neg,
			(unsigned long)num->d[0],neg,(unsigned long)num->d[0])
			<= 0) return(0);
		}
	else
		{
		buf[0]=0;
		if (BIO_printf(bp,"%s%s",number,
			(neg[0] == '-')?" (Negative)":"") <= 0)
			return(0);
		n=BN_bn2bin(num,&buf[1]);
	
		if (buf[1] & 0x80)
			n++;
		else	buf++;

		for (i=0; i<n; i++)
			{
			if ((i%15) == 0)
				{
				if(BIO_puts(bp,"\n") <= 0
				   || !BIO_indent(bp,off+4,128))
				    return 0;
				}
			if (BIO_printf(bp,"%02x%s",buf[i],((i+1) == n)?"":":")
				<= 0) return(0);
			}
		if (BIO_write(bp,"\n",1) <= 0) return(0);
		}
	return(1);
	}

#ifndef OPENSSL_NO_EC
static int print_bin(BIO *fp, const char *name, const unsigned char *buf,
		size_t len, int off)
	{
	size_t i;
	char str[128];

	if (buf == NULL)
		return 1;
	if (off)
		{
		if (off > 128)
			off=128;
		memset(str,' ',off);
		if (BIO_write(fp, str, off) <= 0)
			return 0;
		}

	if (BIO_printf(fp,"%s", name) <= 0)
		return 0;

	for (i=0; i<len; i++)
		{
		if ((i%15) == 0)
			{
			str[0]='\n';
			memset(&(str[1]),' ',off+4);
			if (BIO_write(fp, str, off+1+4) <= 0)
				return 0;
			}
		if (BIO_printf(fp,"%02x%s",buf[i],((i+1) == len)?"":":") <= 0)
			return 0;
		}
	if (BIO_write(fp,"\n",1) <= 0)
		return 0;

	return 1;
	}
#endif

#ifndef OPENSSL_NO_DH
#ifndef OPENSSL_NO_FP_API
int DHparams_print_fp(FILE *fp, const DH *x)
	{
	BIO *b;
	int ret;

	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		DHerr(DH_F_DHPARAMS_PRINT_FP,ERR_R_BUF_LIB);
		return(0);
		}
	BIO_set_fp(b,fp,BIO_NOCLOSE);
	ret=DHparams_print(b, x);
	BIO_free(b);
	return(ret);
	}
#endif

int DHparams_print(BIO *bp, const DH *x)
	{
	unsigned char *m=NULL;
	int reason=ERR_R_BUF_LIB,ret=0;
	size_t buf_len=0, i;

	if (x->p)
		buf_len = (size_t)BN_num_bytes(x->p);
	else
		{
		reason = ERR_R_PASSED_NULL_PARAMETER;
		goto err;
		}
	if (x->g)
		if (buf_len < (i = (size_t)BN_num_bytes(x->g)))
			buf_len = i;
	m=(unsigned char *)OPENSSL_malloc(buf_len+10);
	if (m == NULL)
		{
		reason=ERR_R_MALLOC_FAILURE;
		goto err;
		}

	if (BIO_printf(bp,"Diffie-Hellman-Parameters: (%d bit)\n",
		BN_num_bits(x->p)) <= 0)
		goto err;
	if (!print(bp,"prime:",x->p,m,4)) goto err;
	if (!print(bp,"generator:",x->g,m,4)) goto err;
	if (x->length != 0)
		{
		if (BIO_printf(bp,"    recommended-private-length: %d bits\n",
			(int)x->length) <= 0) goto err;
		}
	ret=1;
	if (0)
		{
err:
		DHerr(DH_F_DHPARAMS_PRINT,reason);
		}
	if (m != NULL) OPENSSL_free(m);
	return(ret);
	}
#endif

#ifndef OPENSSL_NO_DSA
#ifndef OPENSSL_NO_FP_API
int DSAparams_print_fp(FILE *fp, const DSA *x)
	{
	BIO *b;
	int ret;

	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		DSAerr(DSA_F_DSAPARAMS_PRINT_FP,ERR_R_BUF_LIB);
		return(0);
		}
	BIO_set_fp(b,fp,BIO_NOCLOSE);
	ret=DSAparams_print(b, x);
	BIO_free(b);
	return(ret);
	}
#endif

int DSAparams_print(BIO *bp, const DSA *x)
	{
	unsigned char *m=NULL;
	int ret=0;
	size_t buf_len=0,i;

	if (x->p)
		buf_len = (size_t)BN_num_bytes(x->p);
	else
		{
		DSAerr(DSA_F_DSAPARAMS_PRINT,DSA_R_MISSING_PARAMETERS);
		goto err;
		}
	if (x->q)
		if (buf_len < (i = (size_t)BN_num_bytes(x->q)))
			buf_len = i;
	if (x->g)
		if (buf_len < (i = (size_t)BN_num_bytes(x->g)))
			buf_len = i;
	m=(unsigned char *)OPENSSL_malloc(buf_len+10);
	if (m == NULL)
		{
		DSAerr(DSA_F_DSAPARAMS_PRINT,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (BIO_printf(bp,"DSA-Parameters: (%d bit)\n",
		BN_num_bits(x->p)) <= 0)
		goto err;
	if (!print(bp,"p:",x->p,m,4)) goto err;
	if ((x->q != NULL) && !print(bp,"q:",x->q,m,4)) goto err;
	if ((x->g != NULL) && !print(bp,"g:",x->g,m,4)) goto err;
	ret=1;
err:
	if (m != NULL) OPENSSL_free(m);
	return(ret);
	}

#endif /* !OPENSSL_NO_DSA */

#ifndef OPENSSL_NO_EC
#ifndef OPENSSL_NO_FP_API
int ECParameters_print_fp(FILE *fp, const EC_KEY *x)
	{
	BIO *b;
	int ret;
 
	if ((b=BIO_new(BIO_s_file())) == NULL)
		{
		ECerr(EC_F_ECPARAMETERS_PRINT_FP, ERR_R_BIO_LIB);
		return(0);
		}
	BIO_set_fp(b, fp, BIO_NOCLOSE);
	ret = ECParameters_print(b, x);
	BIO_free(b);
	return(ret);
	}
#endif

int ECParameters_print(BIO *bp, const EC_KEY *x)
	{
	int     reason=ERR_R_EC_LIB, ret=0;
	BIGNUM	*order=NULL;
	const EC_GROUP *group;
 
	if (x == NULL || (group = EC_KEY_get0_group(x)) == NULL)
		{
		reason = ERR_R_PASSED_NULL_PARAMETER;;
		goto err;
		}

	if ((order = BN_new()) == NULL)
		{
		reason = ERR_R_MALLOC_FAILURE;
		goto err;
		}

	if (!EC_GROUP_get_order(group, order, NULL))
		{
		reason = ERR_R_EC_LIB;
		goto err;
		}
 
	if (BIO_printf(bp, "ECDSA-Parameters: (%d bit)\n", 
		BN_num_bits(order)) <= 0)
		goto err;
	if (!ECPKParameters_print(bp, group, 4))
		goto err;
	ret=1;
err:
	if (order)
		BN_free(order);
	ECerr(EC_F_ECPARAMETERS_PRINT, reason);
	return(ret);
	}
  
#endif

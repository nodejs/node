/* fips_dsa_sign.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2007.
 */
/* ====================================================================
 * Copyright (c) 2007 The OpenSSL Project.  All rights reserved.
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

#include <string.h>
#include <openssl/evp.h>
#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/bn.h>

#ifdef OPENSSL_FIPS

/* FIPS versions of DSA_sign() and DSA_verify().
 * These include a tiny ASN1 encoder/decoder to handle the specific
 * case of a DSA signature.
 */

#if 0
int FIPS_dsa_size(DSA *r)
	{
	int ilen;
	ilen = BN_num_bytes(r->q);
	if (ilen > 20)
		return -1;
	/* If MSB set need padding byte */
	ilen ++;
	/* Also need 2 bytes INTEGER header for r and s plus
	 * 2 bytes SEQUENCE header making 6 in total.
	 */
	return ilen * 2 + 6;
	}
#endif

/* Tiny ASN1 encoder for DSA_SIG structure. We can assume r, s smaller than
 * 0x80 octets as by the DSA standards they will be less than 2^160
 */

int FIPS_dsa_sig_encode(unsigned char *out, DSA_SIG *sig)
	{
	int rlen, slen, rpad, spad, seqlen;
	rlen = BN_num_bytes(sig->r);
	if (rlen > 20)
		return -1;
	if (BN_num_bits(sig->r) & 0x7)
		rpad = 0;
	else
		rpad = 1;
	slen = BN_num_bytes(sig->s);
	if (slen > 20)
		return -1;
	if (BN_num_bits(sig->s) & 0x7)
		spad = 0;
	else
		spad = 1;
	/* Length of SEQUENCE, (1 tag + 1 len octet) * 2 + content octets */
	seqlen = rlen + rpad + slen + spad + 4;
	/* Actual encoded length: include SEQUENCE header */
	if (!out)
		return seqlen + 2;

	/* Output SEQUENCE header */
	*out++ = V_ASN1_SEQUENCE|V_ASN1_CONSTRUCTED;
	*out++ = (unsigned char)seqlen;

	/* Output r */
	*out++ = V_ASN1_INTEGER;
	*out++ = (unsigned char)(rlen + rpad);
	if (rpad)
		*out++ = 0;
	BN_bn2bin(sig->r, out);
	out += rlen;

	/* Output s */
	*out++ = V_ASN1_INTEGER;
	*out++ = (unsigned char)(slen + spad);
	if (spad)
		*out++ = 0;
	BN_bn2bin(sig->s, out);
	return seqlen + 2;
	}

/* Companion DSA_SIG decoder */

int FIPS_dsa_sig_decode(DSA_SIG *sig, const unsigned char *in, int inlen)
	{
	int seqlen, rlen, slen;
	const unsigned char *rbin;
	/* Sanity check */

	/* Need SEQUENCE tag */
	if (*in++ != (V_ASN1_SEQUENCE|V_ASN1_CONSTRUCTED))
		return 0;
	/* Get length octet */
	seqlen = *in++;
	/* Check sensible length value */
	if (seqlen < 4 || seqlen > 0x7F)
		return 0;
	/* Check INTEGER tag */
	if (*in++ != V_ASN1_INTEGER)
		return 0;
	rlen = *in++;
	seqlen -= 2 + rlen;
	/* Check sensible seqlen value */
	if (seqlen < 2)
		return 0;
	rbin = in;
	in += rlen;
	/* Check INTEGER tag */
	if (*in++ != V_ASN1_INTEGER)
		return 0;
	slen = *in++;
	/* Remaining bytes of SEQUENCE should exactly match
	 * encoding of s
	 */
	if (seqlen != (slen + 2))
		return 0;
	if (!sig->r && !(sig->r = BN_new()))
		return 0;
	if (!sig->s && !(sig->s = BN_new()))
		return 0;
	if (!BN_bin2bn(rbin, rlen, sig->r))
		return 0;
	if (!BN_bin2bn(in, slen, sig->s))
		return 0;
	return 1;
	}

static int fips_dsa_sign(int type, const unsigned char *x, int y,
	     unsigned char *sig, unsigned int *siglen, EVP_MD_SVCTX *sv)
	{
	DSA *dsa = sv->key;
	unsigned char dig[EVP_MAX_MD_SIZE];
	unsigned int dlen;
	DSA_SIG *s;
        EVP_DigestFinal_ex(sv->mctx, dig, &dlen);
	s=dsa->meth->dsa_do_sign(dig,dlen,dsa);
	OPENSSL_cleanse(dig, dlen);
	if (s == NULL)
		{
		*siglen=0;
		return 0;
		}
	*siglen= FIPS_dsa_sig_encode(sig, s);
	DSA_SIG_free(s);
	if (*siglen < 0)
		return 0;
	return 1;
	}

static int fips_dsa_verify(int type, const unsigned char *x, int y,
	     const unsigned char *sigbuf, unsigned int siglen, EVP_MD_SVCTX *sv)
	{
	DSA *dsa = sv->key;
	DSA_SIG *s;
	int ret=-1;
	unsigned char dig[EVP_MAX_MD_SIZE];
	unsigned int dlen;

	s = DSA_SIG_new();
	if (s == NULL)
		return ret;
	if (!FIPS_dsa_sig_decode(s,sigbuf,siglen))
		goto err;
        EVP_DigestFinal_ex(sv->mctx, dig, &dlen);
	ret=dsa->meth->dsa_do_verify(dig,dlen,s,dsa);
	OPENSSL_cleanse(dig, dlen);
err:
	DSA_SIG_free(s);
	return ret;
	}

static int init(EVP_MD_CTX *ctx)
	{ return SHA1_Init(ctx->md_data); }

static int update(EVP_MD_CTX *ctx,const void *data,size_t count)
	{ return SHA1_Update(ctx->md_data,data,count); }

static int final(EVP_MD_CTX *ctx,unsigned char *md)
	{ return SHA1_Final(md,ctx->md_data); }

static const EVP_MD dss1_md=
	{
	NID_dsa,
	NID_dsaWithSHA1,
	SHA_DIGEST_LENGTH,
	EVP_MD_FLAG_FIPS|EVP_MD_FLAG_SVCTX,
	init,
	update,
	final,
	NULL,
	NULL,
	(evp_sign_method *)fips_dsa_sign,
	(evp_verify_method *)fips_dsa_verify,
	{EVP_PKEY_DSA,EVP_PKEY_DSA2,EVP_PKEY_DSA3, EVP_PKEY_DSA4,0},
	SHA_CBLOCK,
	sizeof(EVP_MD *)+sizeof(SHA_CTX),
	};

const EVP_MD *EVP_dss1(void)
	{
	return(&dss1_md);
	}
#endif

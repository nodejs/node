/* ssl/t1_enc.c */
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
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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
#include "ssl_locl.h"
#ifndef OPENSSL_NO_COMP
#include <openssl/comp.h>
#endif
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#ifdef KSSL_DEBUG
#include <openssl/des.h>
#endif

static void tls1_P_hash(const EVP_MD *md, const unsigned char *sec,
			int sec_len, unsigned char *seed, int seed_len,
			unsigned char *out, int olen)
	{
	int chunk,n;
	unsigned int j;
	HMAC_CTX ctx;
	HMAC_CTX ctx_tmp;
	unsigned char A1[EVP_MAX_MD_SIZE];
	unsigned int A1_len;
	
	chunk=EVP_MD_size(md);

	HMAC_CTX_init(&ctx);
	HMAC_CTX_init(&ctx_tmp);
	HMAC_CTX_set_flags(&ctx, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
	HMAC_CTX_set_flags(&ctx_tmp, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
	HMAC_Init_ex(&ctx,sec,sec_len,md, NULL);
	HMAC_Init_ex(&ctx_tmp,sec,sec_len,md, NULL);
	HMAC_Update(&ctx,seed,seed_len);
	HMAC_Final(&ctx,A1,&A1_len);

	n=0;
	for (;;)
		{
		HMAC_Init_ex(&ctx,NULL,0,NULL,NULL); /* re-init */
		HMAC_Init_ex(&ctx_tmp,NULL,0,NULL,NULL); /* re-init */
		HMAC_Update(&ctx,A1,A1_len);
		HMAC_Update(&ctx_tmp,A1,A1_len);
		HMAC_Update(&ctx,seed,seed_len);

		if (olen > chunk)
			{
			HMAC_Final(&ctx,out,&j);
			out+=j;
			olen-=j;
			HMAC_Final(&ctx_tmp,A1,&A1_len); /* calc the next A1 value */
			}
		else	/* last one */
			{
			HMAC_Final(&ctx,A1,&A1_len);
			memcpy(out,A1,olen);
			break;
			}
		}
	HMAC_CTX_cleanup(&ctx);
	HMAC_CTX_cleanup(&ctx_tmp);
	OPENSSL_cleanse(A1,sizeof(A1));
	}

static void tls1_PRF(const EVP_MD *md5, const EVP_MD *sha1,
		     unsigned char *label, int label_len,
		     const unsigned char *sec, int slen, unsigned char *out1,
		     unsigned char *out2, int olen)
	{
	int len,i;
	const unsigned char *S1,*S2;

	len=slen/2;
	S1=sec;
	S2= &(sec[len]);
	len+=(slen&1); /* add for odd, make longer */

	
	tls1_P_hash(md5 ,S1,len,label,label_len,out1,olen);
	tls1_P_hash(sha1,S2,len,label,label_len,out2,olen);

	for (i=0; i<olen; i++)
		out1[i]^=out2[i];
	}

static void tls1_generate_key_block(SSL *s, unsigned char *km,
	     unsigned char *tmp, int num)
	{
	unsigned char *p;
	unsigned char buf[SSL3_RANDOM_SIZE*2+
		TLS_MD_MAX_CONST_SIZE];
	p=buf;

	memcpy(p,TLS_MD_KEY_EXPANSION_CONST,
		TLS_MD_KEY_EXPANSION_CONST_SIZE);
	p+=TLS_MD_KEY_EXPANSION_CONST_SIZE;
	memcpy(p,s->s3->server_random,SSL3_RANDOM_SIZE);
	p+=SSL3_RANDOM_SIZE;
	memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
	p+=SSL3_RANDOM_SIZE;

	tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,(int)(p-buf),
		 s->session->master_key,s->session->master_key_length,
		 km,tmp,num);
#ifdef KSSL_DEBUG
	printf("tls1_generate_key_block() ==> %d byte master_key =\n\t",
                s->session->master_key_length);
	{
        int i;
        for (i=0; i < s->session->master_key_length; i++)
                {
                printf("%02X", s->session->master_key[i]);
                }
        printf("\n");  }
#endif    /* KSSL_DEBUG */
	}

int tls1_change_cipher_state(SSL *s, int which)
	{
	static const unsigned char empty[]="";
	unsigned char *p,*key_block,*mac_secret;
	unsigned char *exp_label,buf[TLS_MD_MAX_CONST_SIZE+
		SSL3_RANDOM_SIZE*2];
	unsigned char tmp1[EVP_MAX_KEY_LENGTH];
	unsigned char tmp2[EVP_MAX_KEY_LENGTH];
	unsigned char iv1[EVP_MAX_IV_LENGTH*2];
	unsigned char iv2[EVP_MAX_IV_LENGTH*2];
	unsigned char *ms,*key,*iv,*er1,*er2;
	int client_write;
	EVP_CIPHER_CTX *dd;
	const EVP_CIPHER *c;
#ifndef OPENSSL_NO_COMP
	const SSL_COMP *comp;
#endif
	const EVP_MD *m;
	int is_export,n,i,j,k,exp_label_len,cl;
	int reuse_dd = 0;

	is_export=SSL_C_IS_EXPORT(s->s3->tmp.new_cipher);
	c=s->s3->tmp.new_sym_enc;
	m=s->s3->tmp.new_hash;
#ifndef OPENSSL_NO_COMP
	comp=s->s3->tmp.new_compression;
#endif
	key_block=s->s3->tmp.key_block;

#ifdef KSSL_DEBUG
	printf("tls1_change_cipher_state(which= %d) w/\n", which);
	printf("\talg= %ld, comp= %p\n", s->s3->tmp.new_cipher->algorithms,
                (void *)comp);
	printf("\tevp_cipher == %p ==? &d_cbc_ede_cipher3\n", (void *)c);
	printf("\tevp_cipher: nid, blksz= %d, %d, keylen=%d, ivlen=%d\n",
                c->nid,c->block_size,c->key_len,c->iv_len);
	printf("\tkey_block: len= %d, data= ", s->s3->tmp.key_block_length);
	{
        int ki;
        for (ki=0; ki<s->s3->tmp.key_block_length; ki++)
		printf("%02x", key_block[ki]);  printf("\n");
        }
#endif	/* KSSL_DEBUG */

	if (which & SSL3_CC_READ)
		{
		if (s->enc_read_ctx != NULL)
			reuse_dd = 1;
		else if ((s->enc_read_ctx=OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
			goto err;
		else
			/* make sure it's intialized in case we exit later with an error */
			EVP_CIPHER_CTX_init(s->enc_read_ctx);
		dd= s->enc_read_ctx;
		s->read_hash=m;
#ifndef OPENSSL_NO_COMP
		if (s->expand != NULL)
			{
			COMP_CTX_free(s->expand);
			s->expand=NULL;
			}
		if (comp != NULL)
			{
			s->expand=COMP_CTX_new(comp->method);
			if (s->expand == NULL)
				{
				SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,SSL_R_COMPRESSION_LIBRARY_ERROR);
				goto err2;
				}
			if (s->s3->rrec.comp == NULL)
				s->s3->rrec.comp=(unsigned char *)
					OPENSSL_malloc(SSL3_RT_MAX_ENCRYPTED_LENGTH);
			if (s->s3->rrec.comp == NULL)
				goto err;
			}
#endif
		/* this is done by dtls1_reset_seq_numbers for DTLS1_VERSION */
 		if (s->version != DTLS1_VERSION)
			memset(&(s->s3->read_sequence[0]),0,8);
		mac_secret= &(s->s3->read_mac_secret[0]);
		}
	else
		{
		if (s->enc_write_ctx != NULL)
			reuse_dd = 1;
		else if ((s->enc_write_ctx=OPENSSL_malloc(sizeof(EVP_CIPHER_CTX))) == NULL)
			goto err;
		else
			/* make sure it's intialized in case we exit later with an error */
			EVP_CIPHER_CTX_init(s->enc_write_ctx);
		dd= s->enc_write_ctx;
		s->write_hash=m;
#ifndef OPENSSL_NO_COMP
		if (s->compress != NULL)
			{
			COMP_CTX_free(s->compress);
			s->compress=NULL;
			}
		if (comp != NULL)
			{
			s->compress=COMP_CTX_new(comp->method);
			if (s->compress == NULL)
				{
				SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,SSL_R_COMPRESSION_LIBRARY_ERROR);
				goto err2;
				}
			}
#endif
		/* this is done by dtls1_reset_seq_numbers for DTLS1_VERSION */
 		if (s->version != DTLS1_VERSION)
			memset(&(s->s3->write_sequence[0]),0,8);
		mac_secret= &(s->s3->write_mac_secret[0]);
		}

	if (reuse_dd)
		EVP_CIPHER_CTX_cleanup(dd);

	p=s->s3->tmp.key_block;
	i=EVP_MD_size(m);
	cl=EVP_CIPHER_key_length(c);
	j=is_export ? (cl < SSL_C_EXPORT_KEYLENGTH(s->s3->tmp.new_cipher) ?
	               cl : SSL_C_EXPORT_KEYLENGTH(s->s3->tmp.new_cipher)) : cl;
	/* Was j=(exp)?5:EVP_CIPHER_key_length(c); */
	k=EVP_CIPHER_iv_length(c);
	er1= &(s->s3->client_random[0]);
	er2= &(s->s3->server_random[0]);
	if (	(which == SSL3_CHANGE_CIPHER_CLIENT_WRITE) ||
		(which == SSL3_CHANGE_CIPHER_SERVER_READ))
		{
		ms=  &(p[ 0]); n=i+i;
		key= &(p[ n]); n+=j+j;
		iv=  &(p[ n]); n+=k+k;
		exp_label=(unsigned char *)TLS_MD_CLIENT_WRITE_KEY_CONST;
		exp_label_len=TLS_MD_CLIENT_WRITE_KEY_CONST_SIZE;
		client_write=1;
		}
	else
		{
		n=i;
		ms=  &(p[ n]); n+=i+j;
		key= &(p[ n]); n+=j+k;
		iv=  &(p[ n]); n+=k;
		exp_label=(unsigned char *)TLS_MD_SERVER_WRITE_KEY_CONST;
		exp_label_len=TLS_MD_SERVER_WRITE_KEY_CONST_SIZE;
		client_write=0;
		}

	if (n > s->s3->tmp.key_block_length)
		{
		SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,ERR_R_INTERNAL_ERROR);
		goto err2;
		}

	memcpy(mac_secret,ms,i);
#ifdef TLS_DEBUG
printf("which = %04X\nmac key=",which);
{ int z; for (z=0; z<i; z++) printf("%02X%c",ms[z],((z+1)%16)?' ':'\n'); }
#endif
	if (is_export)
		{
		/* In here I set both the read and write key/iv to the
		 * same value since only the correct one will be used :-).
		 */
		p=buf;
		memcpy(p,exp_label,exp_label_len);
		p+=exp_label_len;
		memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
		p+=SSL3_RANDOM_SIZE;
		memcpy(p,s->s3->server_random,SSL3_RANDOM_SIZE);
		p+=SSL3_RANDOM_SIZE;
		tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,(int)(p-buf),key,j,
			 tmp1,tmp2,EVP_CIPHER_key_length(c));
		key=tmp1;

		if (k > 0)
			{
			p=buf;
			memcpy(p,TLS_MD_IV_BLOCK_CONST,
				TLS_MD_IV_BLOCK_CONST_SIZE);
			p+=TLS_MD_IV_BLOCK_CONST_SIZE;
			memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
			p+=SSL3_RANDOM_SIZE;
			memcpy(p,s->s3->server_random,SSL3_RANDOM_SIZE);
			p+=SSL3_RANDOM_SIZE;
			tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,p-buf,empty,0,
				 iv1,iv2,k*2);
			if (client_write)
				iv=iv1;
			else
				iv= &(iv1[k]);
			}
		}

	s->session->key_arg_length=0;
#ifdef KSSL_DEBUG
	{
        int ki;
	printf("EVP_CipherInit_ex(dd,c,key=,iv=,which)\n");
	printf("\tkey= ");
	for (ki=0; ki<c->key_len; ki++) printf("%02x", key[ki]);
	printf("\n");
	printf("\t iv= ");
	for (ki=0; ki<c->iv_len; ki++) printf("%02x", iv[ki]);
	printf("\n");
	}
#endif	/* KSSL_DEBUG */

	EVP_CipherInit_ex(dd,c,NULL,key,iv,(which & SSL3_CC_WRITE));
#ifdef TLS_DEBUG
printf("which = %04X\nkey=",which);
{ int z; for (z=0; z<EVP_CIPHER_key_length(c); z++) printf("%02X%c",key[z],((z+1)%16)?' ':'\n'); }
printf("\niv=");
{ int z; for (z=0; z<k; z++) printf("%02X%c",iv[z],((z+1)%16)?' ':'\n'); }
printf("\n");
#endif

	OPENSSL_cleanse(tmp1,sizeof(tmp1));
	OPENSSL_cleanse(tmp2,sizeof(tmp1));
	OPENSSL_cleanse(iv1,sizeof(iv1));
	OPENSSL_cleanse(iv2,sizeof(iv2));
	return(1);
err:
	SSLerr(SSL_F_TLS1_CHANGE_CIPHER_STATE,ERR_R_MALLOC_FAILURE);
err2:
	return(0);
	}

int tls1_setup_key_block(SSL *s)
	{
	unsigned char *p1,*p2;
	const EVP_CIPHER *c;
	const EVP_MD *hash;
	int num;
	SSL_COMP *comp;

#ifdef KSSL_DEBUG
	printf ("tls1_setup_key_block()\n");
#endif	/* KSSL_DEBUG */

	if (s->s3->tmp.key_block_length != 0)
		return(1);

	if (!ssl_cipher_get_evp(s->session,&c,&hash,&comp))
		{
		SSLerr(SSL_F_TLS1_SETUP_KEY_BLOCK,SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
		return(0);
		}

	s->s3->tmp.new_sym_enc=c;
	s->s3->tmp.new_hash=hash;

	num=EVP_CIPHER_key_length(c)+EVP_MD_size(hash)+EVP_CIPHER_iv_length(c);
	num*=2;

	ssl3_cleanup_key_block(s);

	if ((p1=(unsigned char *)OPENSSL_malloc(num)) == NULL)
		goto err;
	if ((p2=(unsigned char *)OPENSSL_malloc(num)) == NULL)
		goto err;

	s->s3->tmp.key_block_length=num;
	s->s3->tmp.key_block=p1;


#ifdef TLS_DEBUG
printf("client random\n");
{ int z; for (z=0; z<SSL3_RANDOM_SIZE; z++) printf("%02X%c",s->s3->client_random[z],((z+1)%16)?' ':'\n'); }
printf("server random\n");
{ int z; for (z=0; z<SSL3_RANDOM_SIZE; z++) printf("%02X%c",s->s3->server_random[z],((z+1)%16)?' ':'\n'); }
printf("pre-master\n");
{ int z; for (z=0; z<s->session->master_key_length; z++) printf("%02X%c",s->session->master_key[z],((z+1)%16)?' ':'\n'); }
#endif
	tls1_generate_key_block(s,p1,p2,num);
	OPENSSL_cleanse(p2,num);
	OPENSSL_free(p2);
#ifdef TLS_DEBUG
printf("\nkey block\n");
{ int z; for (z=0; z<num; z++) printf("%02X%c",p1[z],((z+1)%16)?' ':'\n'); }
#endif

	if (!(s->options & SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS))
		{
		/* enable vulnerability countermeasure for CBC ciphers with
		 * known-IV problem (http://www.openssl.org/~bodo/tls-cbc.txt)
		 */
		s->s3->need_empty_fragments = 1;

		if (s->session->cipher != NULL)
			{
			if ((s->session->cipher->algorithms & SSL_ENC_MASK) == SSL_eNULL)
				s->s3->need_empty_fragments = 0;
			
#ifndef OPENSSL_NO_RC4
			if ((s->session->cipher->algorithms & SSL_ENC_MASK) == SSL_RC4)
				s->s3->need_empty_fragments = 0;
#endif
			}
		}
		
	return(1);
err:
	SSLerr(SSL_F_TLS1_SETUP_KEY_BLOCK,ERR_R_MALLOC_FAILURE);
	return(0);
	}

int tls1_enc(SSL *s, int send)
	{
	SSL3_RECORD *rec;
	EVP_CIPHER_CTX *ds;
	unsigned long l;
	int bs,i,ii,j,k,n=0;
	const EVP_CIPHER *enc;

	if (send)
		{
		if (s->write_hash != NULL)
			n=EVP_MD_size(s->write_hash);
		ds=s->enc_write_ctx;
		rec= &(s->s3->wrec);
		if (s->enc_write_ctx == NULL)
			enc=NULL;
		else
			enc=EVP_CIPHER_CTX_cipher(s->enc_write_ctx);
		}
	else
		{
		if (s->read_hash != NULL)
			n=EVP_MD_size(s->read_hash);
		ds=s->enc_read_ctx;
		rec= &(s->s3->rrec);
		if (s->enc_read_ctx == NULL)
			enc=NULL;
		else
			enc=EVP_CIPHER_CTX_cipher(s->enc_read_ctx);
		}

#ifdef KSSL_DEBUG
	printf("tls1_enc(%d)\n", send);
#endif    /* KSSL_DEBUG */

	if ((s->session == NULL) || (ds == NULL) ||
		(enc == NULL))
		{
		memmove(rec->data,rec->input,rec->length);
		rec->input=rec->data;
		}
	else
		{
		l=rec->length;
		bs=EVP_CIPHER_block_size(ds->cipher);

		if ((bs != 1) && send)
			{
			i=bs-((int)l%bs);

			/* Add weird padding of upto 256 bytes */

			/* we need to add 'i' padding bytes of value j */
			j=i-1;
			if (s->options & SSL_OP_TLS_BLOCK_PADDING_BUG)
				{
				if (s->s3->flags & TLS1_FLAGS_TLS_PADDING_BUG)
					j++;
				}
			for (k=(int)l; k<(int)(l+i); k++)
				rec->input[k]=j;
			l+=i;
			rec->length+=i;
			}

#ifdef KSSL_DEBUG
		{
                unsigned long ui;
		printf("EVP_Cipher(ds=%p,rec->data=%p,rec->input=%p,l=%ld) ==>\n",
                        (void *)ds,rec->data,rec->input,l);
		printf("\tEVP_CIPHER_CTX: %d buf_len, %d key_len [%ld %ld], %d iv_len\n",
                        ds->buf_len, ds->cipher->key_len,
                        (unsigned long)DES_KEY_SZ,
			(unsigned long)DES_SCHEDULE_SZ,
                        ds->cipher->iv_len);
		printf("\t\tIV: ");
		for (i=0; i<ds->cipher->iv_len; i++) printf("%02X", ds->iv[i]);
		printf("\n");
		printf("\trec->input=");
		for (ui=0; ui<l; ui++) printf(" %02x", rec->input[ui]);
		printf("\n");
		}
#endif	/* KSSL_DEBUG */

		if (!send)
			{
			if (l == 0 || l%bs != 0)
				{
				SSLerr(SSL_F_TLS1_ENC,SSL_R_BLOCK_CIPHER_PAD_IS_WRONG);
				ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECRYPTION_FAILED);
				return 0;
				}
			}
		
		EVP_Cipher(ds,rec->data,rec->input,l);

#ifdef KSSL_DEBUG
		{
                unsigned long ki;
                printf("\trec->data=");
		for (ki=0; ki<l; i++)
                        printf(" %02x", rec->data[ki]);  printf("\n");
                }
#endif	/* KSSL_DEBUG */

		if ((bs != 1) && !send)
			{
			ii=i=rec->data[l-1]; /* padding_length */
			i++;
			/* NB: if compression is in operation the first packet
			 * may not be of even length so the padding bug check
			 * cannot be performed. This bug workaround has been
			 * around since SSLeay so hopefully it is either fixed
			 * now or no buggy implementation supports compression 
			 * [steve]
			 */
			if ( (s->options&SSL_OP_TLS_BLOCK_PADDING_BUG)
				&& !s->expand)
				{
				/* First packet is even in size, so check */
				if ((memcmp(s->s3->read_sequence,
					"\0\0\0\0\0\0\0\0",8) == 0) && !(ii & 1))
					s->s3->flags|=TLS1_FLAGS_TLS_PADDING_BUG;
				if (s->s3->flags & TLS1_FLAGS_TLS_PADDING_BUG)
					i--;
				}
			/* TLS 1.0 does not bound the number of padding bytes by the block size.
			 * All of them must have value 'padding_length'. */
			if (i > (int)rec->length)
				{
				/* Incorrect padding. SSLerr() and ssl3_alert are done
				 * by caller: we don't want to reveal whether this is
				 * a decryption error or a MAC verification failure
				 * (see http://www.openssl.org/~bodo/tls-cbc.txt) */
				return -1;
				}
			for (j=(int)(l-i); j<(int)l; j++)
				{
				if (rec->data[j] != ii)
					{
					/* Incorrect padding */
					return -1;
					}
				}
			rec->length-=i;
			}
		}
	return(1);
	}

int tls1_cert_verify_mac(SSL *s, EVP_MD_CTX *in_ctx, unsigned char *out)
	{
	unsigned int ret;
	EVP_MD_CTX ctx;

	EVP_MD_CTX_init(&ctx);
	EVP_MD_CTX_copy_ex(&ctx,in_ctx);
	EVP_DigestFinal_ex(&ctx,out,&ret);
	EVP_MD_CTX_cleanup(&ctx);
	return((int)ret);
	}

int tls1_final_finish_mac(SSL *s, EVP_MD_CTX *in1_ctx, EVP_MD_CTX *in2_ctx,
	     const char *str, int slen, unsigned char *out)
	{
	unsigned int i;
	EVP_MD_CTX ctx;
	unsigned char buf[TLS_MD_MAX_CONST_SIZE+MD5_DIGEST_LENGTH+SHA_DIGEST_LENGTH];
	unsigned char *q,buf2[12];

	q=buf;
	memcpy(q,str,slen);
	q+=slen;

	EVP_MD_CTX_init(&ctx);
	EVP_MD_CTX_copy_ex(&ctx,in1_ctx);
	EVP_DigestFinal_ex(&ctx,q,&i);
	q+=i;
	EVP_MD_CTX_copy_ex(&ctx,in2_ctx);
	EVP_DigestFinal_ex(&ctx,q,&i);
	q+=i;

	tls1_PRF(s->ctx->md5,s->ctx->sha1,buf,(int)(q-buf),
		s->session->master_key,s->session->master_key_length,
		out,buf2,sizeof buf2);
	EVP_MD_CTX_cleanup(&ctx);

	return sizeof buf2;
	}

int tls1_mac(SSL *ssl, unsigned char *md, int send)
	{
	SSL3_RECORD *rec;
	unsigned char *mac_sec,*seq;
	const EVP_MD *hash;
	unsigned int md_size;
	int i;
	HMAC_CTX hmac;
	unsigned char buf[5]; 

	if (send)
		{
		rec= &(ssl->s3->wrec);
		mac_sec= &(ssl->s3->write_mac_secret[0]);
		seq= &(ssl->s3->write_sequence[0]);
		hash=ssl->write_hash;
		}
	else
		{
		rec= &(ssl->s3->rrec);
		mac_sec= &(ssl->s3->read_mac_secret[0]);
		seq= &(ssl->s3->read_sequence[0]);
		hash=ssl->read_hash;
		}

	md_size=EVP_MD_size(hash);

	buf[0]=rec->type;
	if (ssl->version == DTLS1_VERSION && ssl->client_version == DTLS1_BAD_VER)
		{
		buf[1]=TLS1_VERSION_MAJOR;
		buf[2]=TLS1_VERSION_MINOR;
		}
	else	{
		buf[1]=(unsigned char)(ssl->version>>8);
		buf[2]=(unsigned char)(ssl->version);
		}

	buf[3]=rec->length>>8;
	buf[4]=rec->length&0xff;

	/* I should fix this up TLS TLS TLS TLS TLS XXXXXXXX */
	HMAC_CTX_init(&hmac);
	HMAC_Init_ex(&hmac,mac_sec,EVP_MD_size(hash),hash,NULL);

	if (ssl->version == DTLS1_BAD_VER ||
	    (ssl->version == DTLS1_VERSION && ssl->client_version != DTLS1_BAD_VER))
		{
		unsigned char dtlsseq[8],*p=dtlsseq;
		s2n(send?ssl->d1->w_epoch:ssl->d1->r_epoch, p);
		memcpy (p,&seq[2],6);

		HMAC_Update(&hmac,dtlsseq,8);
		}
	else
		HMAC_Update(&hmac,seq,8);

	HMAC_Update(&hmac,buf,5);
	HMAC_Update(&hmac,rec->input,rec->length);
	HMAC_Final(&hmac,md,&md_size);
	HMAC_CTX_cleanup(&hmac);

#ifdef TLS_DEBUG
printf("sec=");
{unsigned int z; for (z=0; z<md_size; z++) printf("%02X ",mac_sec[z]); printf("\n"); }
printf("seq=");
{int z; for (z=0; z<8; z++) printf("%02X ",seq[z]); printf("\n"); }
printf("buf=");
{int z; for (z=0; z<5; z++) printf("%02X ",buf[z]); printf("\n"); }
printf("rec=");
{unsigned int z; for (z=0; z<rec->length; z++) printf("%02X ",buf[z]); printf("\n"); }
#endif

	if ( SSL_version(ssl) != DTLS1_VERSION && SSL_version(ssl) != DTLS1_BAD_VER)
		{
		for (i=7; i>=0; i--)
			{
			++seq[i];
			if (seq[i] != 0) break; 
			}
		}

#ifdef TLS_DEBUG
{unsigned int z; for (z=0; z<md_size; z++) printf("%02X ",md[z]); printf("\n"); }
#endif
	return(md_size);
	}

int tls1_generate_master_secret(SSL *s, unsigned char *out, unsigned char *p,
	     int len)
	{
	unsigned char buf[SSL3_RANDOM_SIZE*2+TLS_MD_MASTER_SECRET_CONST_SIZE];
	unsigned char buff[SSL_MAX_MASTER_KEY_LENGTH];

#ifdef KSSL_DEBUG
	printf ("tls1_generate_master_secret(%p,%p, %p, %d)\n", (void *)s,out, p,len);
#endif	/* KSSL_DEBUG */

	/* Setup the stuff to munge */
	memcpy(buf,TLS_MD_MASTER_SECRET_CONST,
		TLS_MD_MASTER_SECRET_CONST_SIZE);
	memcpy(&(buf[TLS_MD_MASTER_SECRET_CONST_SIZE]),
		s->s3->client_random,SSL3_RANDOM_SIZE);
	memcpy(&(buf[SSL3_RANDOM_SIZE+TLS_MD_MASTER_SECRET_CONST_SIZE]),
		s->s3->server_random,SSL3_RANDOM_SIZE);
	tls1_PRF(s->ctx->md5,s->ctx->sha1,
		buf,TLS_MD_MASTER_SECRET_CONST_SIZE+SSL3_RANDOM_SIZE*2,p,len,
		s->session->master_key,buff,sizeof buff);
#ifdef KSSL_DEBUG
	printf ("tls1_generate_master_secret() complete\n");
#endif	/* KSSL_DEBUG */
	return(SSL3_MASTER_SECRET_SIZE);
	}

int tls1_alert_code(int code)
	{
	switch (code)
		{
	case SSL_AD_CLOSE_NOTIFY:	return(SSL3_AD_CLOSE_NOTIFY);
	case SSL_AD_UNEXPECTED_MESSAGE:	return(SSL3_AD_UNEXPECTED_MESSAGE);
	case SSL_AD_BAD_RECORD_MAC:	return(SSL3_AD_BAD_RECORD_MAC);
	case SSL_AD_DECRYPTION_FAILED:	return(TLS1_AD_DECRYPTION_FAILED);
	case SSL_AD_RECORD_OVERFLOW:	return(TLS1_AD_RECORD_OVERFLOW);
	case SSL_AD_DECOMPRESSION_FAILURE:return(SSL3_AD_DECOMPRESSION_FAILURE);
	case SSL_AD_HANDSHAKE_FAILURE:	return(SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_NO_CERTIFICATE:	return(-1);
	case SSL_AD_BAD_CERTIFICATE:	return(SSL3_AD_BAD_CERTIFICATE);
	case SSL_AD_UNSUPPORTED_CERTIFICATE:return(SSL3_AD_UNSUPPORTED_CERTIFICATE);
	case SSL_AD_CERTIFICATE_REVOKED:return(SSL3_AD_CERTIFICATE_REVOKED);
	case SSL_AD_CERTIFICATE_EXPIRED:return(SSL3_AD_CERTIFICATE_EXPIRED);
	case SSL_AD_CERTIFICATE_UNKNOWN:return(SSL3_AD_CERTIFICATE_UNKNOWN);
	case SSL_AD_ILLEGAL_PARAMETER:	return(SSL3_AD_ILLEGAL_PARAMETER);
	case SSL_AD_UNKNOWN_CA:		return(TLS1_AD_UNKNOWN_CA);
	case SSL_AD_ACCESS_DENIED:	return(TLS1_AD_ACCESS_DENIED);
	case SSL_AD_DECODE_ERROR:	return(TLS1_AD_DECODE_ERROR);
	case SSL_AD_DECRYPT_ERROR:	return(TLS1_AD_DECRYPT_ERROR);
	case SSL_AD_EXPORT_RESTRICTION:	return(TLS1_AD_EXPORT_RESTRICTION);
	case SSL_AD_PROTOCOL_VERSION:	return(TLS1_AD_PROTOCOL_VERSION);
	case SSL_AD_INSUFFICIENT_SECURITY:return(TLS1_AD_INSUFFICIENT_SECURITY);
	case SSL_AD_INTERNAL_ERROR:	return(TLS1_AD_INTERNAL_ERROR);
	case SSL_AD_USER_CANCELLED:	return(TLS1_AD_USER_CANCELLED);
	case SSL_AD_NO_RENEGOTIATION:	return(TLS1_AD_NO_RENEGOTIATION);
#ifdef DTLS1_AD_MISSING_HANDSHAKE_MESSAGE
	case DTLS1_AD_MISSING_HANDSHAKE_MESSAGE: return 
					  (DTLS1_AD_MISSING_HANDSHAKE_MESSAGE);
#endif
	default:			return(-1);
		}
	}


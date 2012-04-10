/* crypto/asn1/asn1_mac.h */
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

#ifndef HEADER_ASN1_MAC_H
#define HEADER_ASN1_MAC_H

#include <openssl/asn1.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef ASN1_MAC_ERR_LIB
#define ASN1_MAC_ERR_LIB	ERR_LIB_ASN1
#endif 

#define ASN1_MAC_H_err(f,r,line) \
	ERR_PUT_error(ASN1_MAC_ERR_LIB,(f),(r),__FILE__,(line))

#define M_ASN1_D2I_vars(a,type,func) \
	ASN1_const_CTX c; \
	type ret=NULL; \
	\
	c.pp=(const unsigned char **)pp; \
	c.q= *(const unsigned char **)pp; \
	c.error=ERR_R_NESTED_ASN1_ERROR; \
	if ((a == NULL) || ((*a) == NULL)) \
		{ if ((ret=(type)func()) == NULL) \
			{ c.line=__LINE__; goto err; } } \
	else	ret=(*a);

#define M_ASN1_D2I_Init() \
	c.p= *(const unsigned char **)pp; \
	c.max=(length == 0)?0:(c.p+length);

#define M_ASN1_D2I_Finish_2(a) \
	if (!asn1_const_Finish(&c)) \
		{ c.line=__LINE__; goto err; } \
	*(const unsigned char **)pp=c.p; \
	if (a != NULL) (*a)=ret; \
	return(ret);

#define M_ASN1_D2I_Finish(a,func,e) \
	M_ASN1_D2I_Finish_2(a); \
err:\
	ASN1_MAC_H_err((e),c.error,c.line); \
	asn1_add_error(*(const unsigned char **)pp,(int)(c.q- *pp)); \
	if ((ret != NULL) && ((a == NULL) || (*a != ret))) func(ret); \
	return(NULL)

#define M_ASN1_D2I_start_sequence() \
	if (!asn1_GetSequence(&c,&length)) \
		{ c.line=__LINE__; goto err; }
/* Begin reading ASN1 without a surrounding sequence */
#define M_ASN1_D2I_begin() \
	c.slen = length;

/* End reading ASN1 with no check on length */
#define M_ASN1_D2I_Finish_nolen(a, func, e) \
	*pp=c.p; \
	if (a != NULL) (*a)=ret; \
	return(ret); \
err:\
	ASN1_MAC_H_err((e),c.error,c.line); \
	asn1_add_error(*pp,(int)(c.q- *pp)); \
	if ((ret != NULL) && ((a == NULL) || (*a != ret))) func(ret); \
	return(NULL)

#define M_ASN1_D2I_end_sequence() \
	(((c.inf&1) == 0)?(c.slen <= 0): \
		(c.eos=ASN1_const_check_infinite_end(&c.p,c.slen)))

/* Don't use this with d2i_ASN1_BOOLEAN() */
#define M_ASN1_D2I_get(b, func) \
	c.q=c.p; \
	if (func(&(b),&c.p,c.slen) == NULL) \
		{c.line=__LINE__; goto err; } \
	c.slen-=(c.p-c.q);

/* Don't use this with d2i_ASN1_BOOLEAN() */
#define M_ASN1_D2I_get_x(type,b,func) \
	c.q=c.p; \
	if (((D2I_OF(type))func)(&(b),&c.p,c.slen) == NULL) \
		{c.line=__LINE__; goto err; } \
	c.slen-=(c.p-c.q);

/* use this instead () */
#define M_ASN1_D2I_get_int(b,func) \
	c.q=c.p; \
	if (func(&(b),&c.p,c.slen) < 0) \
		{c.line=__LINE__; goto err; } \
	c.slen-=(c.p-c.q);

#define M_ASN1_D2I_get_opt(b,func,type) \
	if ((c.slen != 0) && ((M_ASN1_next & (~V_ASN1_CONSTRUCTED)) \
		== (V_ASN1_UNIVERSAL|(type)))) \
		{ \
		M_ASN1_D2I_get(b,func); \
		}

#define M_ASN1_D2I_get_int_opt(b,func,type) \
	if ((c.slen != 0) && ((M_ASN1_next & (~V_ASN1_CONSTRUCTED)) \
		== (V_ASN1_UNIVERSAL|(type)))) \
		{ \
		M_ASN1_D2I_get_int(b,func); \
		}

#define M_ASN1_D2I_get_imp(b,func, type) \
	M_ASN1_next=(_tmp& V_ASN1_CONSTRUCTED)|type; \
	c.q=c.p; \
	if (func(&(b),&c.p,c.slen) == NULL) \
		{c.line=__LINE__; M_ASN1_next_prev = _tmp; goto err; } \
	c.slen-=(c.p-c.q);\
	M_ASN1_next_prev=_tmp;

#define M_ASN1_D2I_get_IMP_opt(b,func,tag,type) \
	if ((c.slen != 0) && ((M_ASN1_next & (~V_ASN1_CONSTRUCTED)) == \
		(V_ASN1_CONTEXT_SPECIFIC|(tag)))) \
		{ \
		unsigned char _tmp = M_ASN1_next; \
		M_ASN1_D2I_get_imp(b,func, type);\
		}

#define M_ASN1_D2I_get_set(r,func,free_func) \
		M_ASN1_D2I_get_imp_set(r,func,free_func, \
			V_ASN1_SET,V_ASN1_UNIVERSAL);

#define M_ASN1_D2I_get_set_type(type,r,func,free_func) \
		M_ASN1_D2I_get_imp_set_type(type,r,func,free_func, \
			V_ASN1_SET,V_ASN1_UNIVERSAL);

#define M_ASN1_D2I_get_set_opt(r,func,free_func) \
	if ((c.slen != 0) && (M_ASN1_next == (V_ASN1_UNIVERSAL| \
		V_ASN1_CONSTRUCTED|V_ASN1_SET)))\
		{ M_ASN1_D2I_get_set(r,func,free_func); }

#define M_ASN1_D2I_get_set_opt_type(type,r,func,free_func) \
	if ((c.slen != 0) && (M_ASN1_next == (V_ASN1_UNIVERSAL| \
		V_ASN1_CONSTRUCTED|V_ASN1_SET)))\
		{ M_ASN1_D2I_get_set_type(type,r,func,free_func); }

#define M_ASN1_I2D_len_SET_opt(a,f) \
	if ((a != NULL) && (sk_num(a) != 0)) \
		M_ASN1_I2D_len_SET(a,f);

#define M_ASN1_I2D_put_SET_opt(a,f) \
	if ((a != NULL) && (sk_num(a) != 0)) \
		M_ASN1_I2D_put_SET(a,f);

#define M_ASN1_I2D_put_SEQUENCE_opt(a,f) \
	if ((a != NULL) && (sk_num(a) != 0)) \
		M_ASN1_I2D_put_SEQUENCE(a,f);

#define M_ASN1_I2D_put_SEQUENCE_opt_type(type,a,f) \
	if ((a != NULL) && (sk_##type##_num(a) != 0)) \
		M_ASN1_I2D_put_SEQUENCE_type(type,a,f);

#define M_ASN1_D2I_get_IMP_set_opt(b,func,free_func,tag) \
	if ((c.slen != 0) && \
		(M_ASN1_next == \
		(V_ASN1_CONTEXT_SPECIFIC|V_ASN1_CONSTRUCTED|(tag))))\
		{ \
		M_ASN1_D2I_get_imp_set(b,func,free_func,\
			tag,V_ASN1_CONTEXT_SPECIFIC); \
		}

#define M_ASN1_D2I_get_IMP_set_opt_type(type,b,func,free_func,tag) \
	if ((c.slen != 0) && \
		(M_ASN1_next == \
		(V_ASN1_CONTEXT_SPECIFIC|V_ASN1_CONSTRUCTED|(tag))))\
		{ \
		M_ASN1_D2I_get_imp_set_type(type,b,func,free_func,\
			tag,V_ASN1_CONTEXT_SPECIFIC); \
		}

#define M_ASN1_D2I_get_seq(r,func,free_func) \
		M_ASN1_D2I_get_imp_set(r,func,free_func,\
			V_ASN1_SEQUENCE,V_ASN1_UNIVERSAL);

#define M_ASN1_D2I_get_seq_type(type,r,func,free_func) \
		M_ASN1_D2I_get_imp_set_type(type,r,func,free_func,\
					    V_ASN1_SEQUENCE,V_ASN1_UNIVERSAL)

#define M_ASN1_D2I_get_seq_opt(r,func,free_func) \
	if ((c.slen != 0) && (M_ASN1_next == (V_ASN1_UNIVERSAL| \
		V_ASN1_CONSTRUCTED|V_ASN1_SEQUENCE)))\
		{ M_ASN1_D2I_get_seq(r,func,free_func); }

#define M_ASN1_D2I_get_seq_opt_type(type,r,func,free_func) \
	if ((c.slen != 0) && (M_ASN1_next == (V_ASN1_UNIVERSAL| \
		V_ASN1_CONSTRUCTED|V_ASN1_SEQUENCE)))\
		{ M_ASN1_D2I_get_seq_type(type,r,func,free_func); }

#define M_ASN1_D2I_get_IMP_set(r,func,free_func,x) \
		M_ASN1_D2I_get_imp_set(r,func,free_func,\
			x,V_ASN1_CONTEXT_SPECIFIC);

#define M_ASN1_D2I_get_IMP_set_type(type,r,func,free_func,x) \
		M_ASN1_D2I_get_imp_set_type(type,r,func,free_func,\
			x,V_ASN1_CONTEXT_SPECIFIC);

#define M_ASN1_D2I_get_imp_set(r,func,free_func,a,b) \
	c.q=c.p; \
	if (d2i_ASN1_SET(&(r),&c.p,c.slen,(char *(*)())func,\
		(void (*)())free_func,a,b) == NULL) \
		{ c.line=__LINE__; goto err; } \
	c.slen-=(c.p-c.q);

#define M_ASN1_D2I_get_imp_set_type(type,r,func,free_func,a,b) \
	c.q=c.p; \
	if (d2i_ASN1_SET_OF_##type(&(r),&c.p,c.slen,func,\
				   free_func,a,b) == NULL) \
		{ c.line=__LINE__; goto err; } \
	c.slen-=(c.p-c.q);

#define M_ASN1_D2I_get_set_strings(r,func,a,b) \
	c.q=c.p; \
	if (d2i_ASN1_STRING_SET(&(r),&c.p,c.slen,a,b) == NULL) \
		{ c.line=__LINE__; goto err; } \
	c.slen-=(c.p-c.q);

#define M_ASN1_D2I_get_EXP_opt(r,func,tag) \
	if ((c.slen != 0L) && (M_ASN1_next == \
		(V_ASN1_CONSTRUCTED|V_ASN1_CONTEXT_SPECIFIC|tag))) \
		{ \
		int Tinf,Ttag,Tclass; \
		long Tlen; \
		\
		c.q=c.p; \
		Tinf=ASN1_get_object(&c.p,&Tlen,&Ttag,&Tclass,c.slen); \
		if (Tinf & 0x80) \
			{ c.error=ERR_R_BAD_ASN1_OBJECT_HEADER; \
			c.line=__LINE__; goto err; } \
		if (Tinf == (V_ASN1_CONSTRUCTED+1)) \
					Tlen = c.slen - (c.p - c.q) - 2; \
		if (func(&(r),&c.p,Tlen) == NULL) \
			{ c.line=__LINE__; goto err; } \
		if (Tinf == (V_ASN1_CONSTRUCTED+1)) { \
			Tlen = c.slen - (c.p - c.q); \
			if(!ASN1_const_check_infinite_end(&c.p, Tlen)) \
				{ c.error=ERR_R_MISSING_ASN1_EOS; \
				c.line=__LINE__; goto err; } \
		}\
		c.slen-=(c.p-c.q); \
		}

#define M_ASN1_D2I_get_EXP_set_opt(r,func,free_func,tag,b) \
	if ((c.slen != 0) && (M_ASN1_next == \
		(V_ASN1_CONSTRUCTED|V_ASN1_CONTEXT_SPECIFIC|tag))) \
		{ \
		int Tinf,Ttag,Tclass; \
		long Tlen; \
		\
		c.q=c.p; \
		Tinf=ASN1_get_object(&c.p,&Tlen,&Ttag,&Tclass,c.slen); \
		if (Tinf & 0x80) \
			{ c.error=ERR_R_BAD_ASN1_OBJECT_HEADER; \
			c.line=__LINE__; goto err; } \
		if (Tinf == (V_ASN1_CONSTRUCTED+1)) \
					Tlen = c.slen - (c.p - c.q) - 2; \
		if (d2i_ASN1_SET(&(r),&c.p,Tlen,(char *(*)())func, \
			(void (*)())free_func, \
			b,V_ASN1_UNIVERSAL) == NULL) \
			{ c.line=__LINE__; goto err; } \
		if (Tinf == (V_ASN1_CONSTRUCTED+1)) { \
			Tlen = c.slen - (c.p - c.q); \
			if(!ASN1_check_infinite_end(&c.p, Tlen)) \
				{ c.error=ERR_R_MISSING_ASN1_EOS; \
				c.line=__LINE__; goto err; } \
		}\
		c.slen-=(c.p-c.q); \
		}

#define M_ASN1_D2I_get_EXP_set_opt_type(type,r,func,free_func,tag,b) \
	if ((c.slen != 0) && (M_ASN1_next == \
		(V_ASN1_CONSTRUCTED|V_ASN1_CONTEXT_SPECIFIC|tag))) \
		{ \
		int Tinf,Ttag,Tclass; \
		long Tlen; \
		\
		c.q=c.p; \
		Tinf=ASN1_get_object(&c.p,&Tlen,&Ttag,&Tclass,c.slen); \
		if (Tinf & 0x80) \
			{ c.error=ERR_R_BAD_ASN1_OBJECT_HEADER; \
			c.line=__LINE__; goto err; } \
		if (Tinf == (V_ASN1_CONSTRUCTED+1)) \
					Tlen = c.slen - (c.p - c.q) - 2; \
		if (d2i_ASN1_SET_OF_##type(&(r),&c.p,Tlen,func, \
			free_func,b,V_ASN1_UNIVERSAL) == NULL) \
			{ c.line=__LINE__; goto err; } \
		if (Tinf == (V_ASN1_CONSTRUCTED+1)) { \
			Tlen = c.slen - (c.p - c.q); \
			if(!ASN1_check_infinite_end(&c.p, Tlen)) \
				{ c.error=ERR_R_MISSING_ASN1_EOS; \
				c.line=__LINE__; goto err; } \
		}\
		c.slen-=(c.p-c.q); \
		}

/* New macros */
#define M_ASN1_New_Malloc(ret,type) \
	if ((ret=(type *)OPENSSL_malloc(sizeof(type))) == NULL) \
		{ c.line=__LINE__; goto err2; }

#define M_ASN1_New(arg,func) \
	if (((arg)=func()) == NULL) return(NULL)

#define M_ASN1_New_Error(a) \
/*	err:	ASN1_MAC_H_err((a),ERR_R_NESTED_ASN1_ERROR,c.line); \
		return(NULL);*/ \
	err2:	ASN1_MAC_H_err((a),ERR_R_MALLOC_FAILURE,c.line); \
		return(NULL)


/* BIG UGLY WARNING!  This is so damn ugly I wanna puke.  Unfortunately,
   some macros that use ASN1_const_CTX still insist on writing in the input
   stream.  ARGH!  ARGH!  ARGH!  Let's get rid of this macro package.
   Please?						-- Richard Levitte */
#define M_ASN1_next		(*((unsigned char *)(c.p)))
#define M_ASN1_next_prev	(*((unsigned char *)(c.q)))

/*************************************************/

#define M_ASN1_I2D_vars(a)	int r=0,ret=0; \
				unsigned char *p; \
				if (a == NULL) return(0)

/* Length Macros */
#define M_ASN1_I2D_len(a,f)	ret+=f(a,NULL)
#define M_ASN1_I2D_len_IMP_opt(a,f)	if (a != NULL) M_ASN1_I2D_len(a,f)

#define M_ASN1_I2D_len_SET(a,f) \
		ret+=i2d_ASN1_SET(a,NULL,f,V_ASN1_SET,V_ASN1_UNIVERSAL,IS_SET);

#define M_ASN1_I2D_len_SET_type(type,a,f) \
		ret+=i2d_ASN1_SET_OF_##type(a,NULL,f,V_ASN1_SET, \
					    V_ASN1_UNIVERSAL,IS_SET);

#define M_ASN1_I2D_len_SEQUENCE(a,f) \
		ret+=i2d_ASN1_SET(a,NULL,f,V_ASN1_SEQUENCE,V_ASN1_UNIVERSAL, \
				  IS_SEQUENCE);

#define M_ASN1_I2D_len_SEQUENCE_type(type,a,f) \
		ret+=i2d_ASN1_SET_OF_##type(a,NULL,f,V_ASN1_SEQUENCE, \
					    V_ASN1_UNIVERSAL,IS_SEQUENCE)

#define M_ASN1_I2D_len_SEQUENCE_opt(a,f) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			M_ASN1_I2D_len_SEQUENCE(a,f);

#define M_ASN1_I2D_len_SEQUENCE_opt_type(type,a,f) \
		if ((a != NULL) && (sk_##type##_num(a) != 0)) \
			M_ASN1_I2D_len_SEQUENCE_type(type,a,f);

#define M_ASN1_I2D_len_IMP_SET(a,f,x) \
		ret+=i2d_ASN1_SET(a,NULL,f,x,V_ASN1_CONTEXT_SPECIFIC,IS_SET);

#define M_ASN1_I2D_len_IMP_SET_type(type,a,f,x) \
		ret+=i2d_ASN1_SET_OF_##type(a,NULL,f,x, \
					    V_ASN1_CONTEXT_SPECIFIC,IS_SET);

#define M_ASN1_I2D_len_IMP_SET_opt(a,f,x) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			ret+=i2d_ASN1_SET(a,NULL,f,x,V_ASN1_CONTEXT_SPECIFIC, \
					  IS_SET);

#define M_ASN1_I2D_len_IMP_SET_opt_type(type,a,f,x) \
		if ((a != NULL) && (sk_##type##_num(a) != 0)) \
			ret+=i2d_ASN1_SET_OF_##type(a,NULL,f,x, \
					       V_ASN1_CONTEXT_SPECIFIC,IS_SET);

#define M_ASN1_I2D_len_IMP_SEQUENCE(a,f,x) \
		ret+=i2d_ASN1_SET(a,NULL,f,x,V_ASN1_CONTEXT_SPECIFIC, \
				  IS_SEQUENCE);

#define M_ASN1_I2D_len_IMP_SEQUENCE_opt(a,f,x) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			ret+=i2d_ASN1_SET(a,NULL,f,x,V_ASN1_CONTEXT_SPECIFIC, \
					  IS_SEQUENCE);

#define M_ASN1_I2D_len_IMP_SEQUENCE_opt_type(type,a,f,x) \
		if ((a != NULL) && (sk_##type##_num(a) != 0)) \
			ret+=i2d_ASN1_SET_OF_##type(a,NULL,f,x, \
						    V_ASN1_CONTEXT_SPECIFIC, \
						    IS_SEQUENCE);

#define M_ASN1_I2D_len_EXP_opt(a,f,mtag,v) \
		if (a != NULL)\
			{ \
			v=f(a,NULL); \
			ret+=ASN1_object_size(1,v,mtag); \
			}

#define M_ASN1_I2D_len_EXP_SET_opt(a,f,mtag,tag,v) \
		if ((a != NULL) && (sk_num(a) != 0))\
			{ \
			v=i2d_ASN1_SET(a,NULL,f,tag,V_ASN1_UNIVERSAL,IS_SET); \
			ret+=ASN1_object_size(1,v,mtag); \
			}

#define M_ASN1_I2D_len_EXP_SEQUENCE_opt(a,f,mtag,tag,v) \
		if ((a != NULL) && (sk_num(a) != 0))\
			{ \
			v=i2d_ASN1_SET(a,NULL,f,tag,V_ASN1_UNIVERSAL, \
				       IS_SEQUENCE); \
			ret+=ASN1_object_size(1,v,mtag); \
			}

#define M_ASN1_I2D_len_EXP_SEQUENCE_opt_type(type,a,f,mtag,tag,v) \
		if ((a != NULL) && (sk_##type##_num(a) != 0))\
			{ \
			v=i2d_ASN1_SET_OF_##type(a,NULL,f,tag, \
						 V_ASN1_UNIVERSAL, \
						 IS_SEQUENCE); \
			ret+=ASN1_object_size(1,v,mtag); \
			}

/* Put Macros */
#define M_ASN1_I2D_put(a,f)	f(a,&p)

#define M_ASN1_I2D_put_IMP_opt(a,f,t)	\
		if (a != NULL) \
			{ \
			unsigned char *q=p; \
			f(a,&p); \
			*q=(V_ASN1_CONTEXT_SPECIFIC|t|(*q&V_ASN1_CONSTRUCTED));\
			}

#define M_ASN1_I2D_put_SET(a,f) i2d_ASN1_SET(a,&p,f,V_ASN1_SET,\
			V_ASN1_UNIVERSAL,IS_SET)
#define M_ASN1_I2D_put_SET_type(type,a,f) \
     i2d_ASN1_SET_OF_##type(a,&p,f,V_ASN1_SET,V_ASN1_UNIVERSAL,IS_SET)
#define M_ASN1_I2D_put_IMP_SET(a,f,x) i2d_ASN1_SET(a,&p,f,x,\
			V_ASN1_CONTEXT_SPECIFIC,IS_SET)
#define M_ASN1_I2D_put_IMP_SET_type(type,a,f,x) \
     i2d_ASN1_SET_OF_##type(a,&p,f,x,V_ASN1_CONTEXT_SPECIFIC,IS_SET)
#define M_ASN1_I2D_put_IMP_SEQUENCE(a,f,x) i2d_ASN1_SET(a,&p,f,x,\
			V_ASN1_CONTEXT_SPECIFIC,IS_SEQUENCE)

#define M_ASN1_I2D_put_SEQUENCE(a,f) i2d_ASN1_SET(a,&p,f,V_ASN1_SEQUENCE,\
					     V_ASN1_UNIVERSAL,IS_SEQUENCE)

#define M_ASN1_I2D_put_SEQUENCE_type(type,a,f) \
     i2d_ASN1_SET_OF_##type(a,&p,f,V_ASN1_SEQUENCE,V_ASN1_UNIVERSAL, \
			    IS_SEQUENCE)

#define M_ASN1_I2D_put_SEQUENCE_opt(a,f) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			M_ASN1_I2D_put_SEQUENCE(a,f);

#define M_ASN1_I2D_put_IMP_SET_opt(a,f,x) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			{ i2d_ASN1_SET(a,&p,f,x,V_ASN1_CONTEXT_SPECIFIC, \
				       IS_SET); }

#define M_ASN1_I2D_put_IMP_SET_opt_type(type,a,f,x) \
		if ((a != NULL) && (sk_##type##_num(a) != 0)) \
			{ i2d_ASN1_SET_OF_##type(a,&p,f,x, \
						 V_ASN1_CONTEXT_SPECIFIC, \
						 IS_SET); }

#define M_ASN1_I2D_put_IMP_SEQUENCE_opt(a,f,x) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			{ i2d_ASN1_SET(a,&p,f,x,V_ASN1_CONTEXT_SPECIFIC, \
				       IS_SEQUENCE); }

#define M_ASN1_I2D_put_IMP_SEQUENCE_opt_type(type,a,f,x) \
		if ((a != NULL) && (sk_##type##_num(a) != 0)) \
			{ i2d_ASN1_SET_OF_##type(a,&p,f,x, \
						 V_ASN1_CONTEXT_SPECIFIC, \
						 IS_SEQUENCE); }

#define M_ASN1_I2D_put_EXP_opt(a,f,tag,v) \
		if (a != NULL) \
			{ \
			ASN1_put_object(&p,1,v,tag,V_ASN1_CONTEXT_SPECIFIC); \
			f(a,&p); \
			}

#define M_ASN1_I2D_put_EXP_SET_opt(a,f,mtag,tag,v) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			{ \
			ASN1_put_object(&p,1,v,mtag,V_ASN1_CONTEXT_SPECIFIC); \
			i2d_ASN1_SET(a,&p,f,tag,V_ASN1_UNIVERSAL,IS_SET); \
			}

#define M_ASN1_I2D_put_EXP_SEQUENCE_opt(a,f,mtag,tag,v) \
		if ((a != NULL) && (sk_num(a) != 0)) \
			{ \
			ASN1_put_object(&p,1,v,mtag,V_ASN1_CONTEXT_SPECIFIC); \
			i2d_ASN1_SET(a,&p,f,tag,V_ASN1_UNIVERSAL,IS_SEQUENCE); \
			}

#define M_ASN1_I2D_put_EXP_SEQUENCE_opt_type(type,a,f,mtag,tag,v) \
		if ((a != NULL) && (sk_##type##_num(a) != 0)) \
			{ \
			ASN1_put_object(&p,1,v,mtag,V_ASN1_CONTEXT_SPECIFIC); \
			i2d_ASN1_SET_OF_##type(a,&p,f,tag,V_ASN1_UNIVERSAL, \
					       IS_SEQUENCE); \
			}

#define M_ASN1_I2D_seq_total() \
		r=ASN1_object_size(1,ret,V_ASN1_SEQUENCE); \
		if (pp == NULL) return(r); \
		p= *pp; \
		ASN1_put_object(&p,1,ret,V_ASN1_SEQUENCE,V_ASN1_UNIVERSAL)

#define M_ASN1_I2D_INF_seq_start(tag,ctx) \
		*(p++)=(V_ASN1_CONSTRUCTED|(tag)|(ctx)); \
		*(p++)=0x80

#define M_ASN1_I2D_INF_seq_end() *(p++)=0x00; *(p++)=0x00

#define M_ASN1_I2D_finish()	*pp=p; \
				return(r);

int asn1_GetSequence(ASN1_const_CTX *c, long *length);
void asn1_add_error(const unsigned char *address,int offset);
#ifdef  __cplusplus
}
#endif

#endif

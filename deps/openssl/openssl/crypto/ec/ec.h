/* crypto/ec/ec.h */
/*
 * Originally written by Bodo Moeller for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by 
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by 
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#ifndef HEADER_EC_H
#define HEADER_EC_H

#include <openssl/opensslconf.h>

#ifdef OPENSSL_NO_EC
#error EC is disabled.
#endif

#include <openssl/asn1.h>
#include <openssl/symhacks.h>
#ifndef OPENSSL_NO_DEPRECATED
#include <openssl/bn.h>
#endif

#ifdef  __cplusplus
extern "C" {
#elif defined(__SUNPRO_C)
# if __SUNPRO_C >= 0x520
# pragma error_messages (off,E_ARRAY_OF_INCOMPLETE_NONAME,E_ARRAY_OF_INCOMPLETE)
# endif
#endif


#ifndef OPENSSL_ECC_MAX_FIELD_BITS
# define OPENSSL_ECC_MAX_FIELD_BITS 661
#endif

typedef enum {
	/* values as defined in X9.62 (ECDSA) and elsewhere */
	POINT_CONVERSION_COMPRESSED = 2,
	POINT_CONVERSION_UNCOMPRESSED = 4,
	POINT_CONVERSION_HYBRID = 6
} point_conversion_form_t;


typedef struct ec_method_st EC_METHOD;

typedef struct ec_group_st
	/*
	 EC_METHOD *meth;
	 -- field definition
	 -- curve coefficients
	 -- optional generator with associated information (order, cofactor)
	 -- optional extra data (precomputed table for fast computation of multiples of generator)
	 -- ASN1 stuff
	*/
	EC_GROUP;

typedef struct ec_point_st EC_POINT;


/* EC_METHODs for curves over GF(p).
 * EC_GFp_simple_method provides the basis for the optimized methods.
 */
const EC_METHOD *EC_GFp_simple_method(void);
const EC_METHOD *EC_GFp_mont_method(void);
const EC_METHOD *EC_GFp_nist_method(void);

/* EC_METHOD for curves over GF(2^m).
 */
const EC_METHOD *EC_GF2m_simple_method(void);


EC_GROUP *EC_GROUP_new(const EC_METHOD *);
void EC_GROUP_free(EC_GROUP *);
void EC_GROUP_clear_free(EC_GROUP *);
int EC_GROUP_copy(EC_GROUP *, const EC_GROUP *);
EC_GROUP *EC_GROUP_dup(const EC_GROUP *);

const EC_METHOD *EC_GROUP_method_of(const EC_GROUP *);
int EC_METHOD_get_field_type(const EC_METHOD *);

int EC_GROUP_set_generator(EC_GROUP *, const EC_POINT *generator, const BIGNUM *order, const BIGNUM *cofactor);
const EC_POINT *EC_GROUP_get0_generator(const EC_GROUP *);
int EC_GROUP_get_order(const EC_GROUP *, BIGNUM *order, BN_CTX *);
int EC_GROUP_get_cofactor(const EC_GROUP *, BIGNUM *cofactor, BN_CTX *);

void EC_GROUP_set_curve_name(EC_GROUP *, int nid);
int EC_GROUP_get_curve_name(const EC_GROUP *);

void EC_GROUP_set_asn1_flag(EC_GROUP *, int flag);
int EC_GROUP_get_asn1_flag(const EC_GROUP *);

void EC_GROUP_set_point_conversion_form(EC_GROUP *, point_conversion_form_t);
point_conversion_form_t EC_GROUP_get_point_conversion_form(const EC_GROUP *);

unsigned char *EC_GROUP_get0_seed(const EC_GROUP *);
size_t EC_GROUP_get_seed_len(const EC_GROUP *);
size_t EC_GROUP_set_seed(EC_GROUP *, const unsigned char *, size_t len);

int EC_GROUP_set_curve_GFp(EC_GROUP *, const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *);
int EC_GROUP_get_curve_GFp(const EC_GROUP *, BIGNUM *p, BIGNUM *a, BIGNUM *b, BN_CTX *);
int EC_GROUP_set_curve_GF2m(EC_GROUP *, const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *);
int EC_GROUP_get_curve_GF2m(const EC_GROUP *, BIGNUM *p, BIGNUM *a, BIGNUM *b, BN_CTX *);

/* returns the number of bits needed to represent a field element */
int EC_GROUP_get_degree(const EC_GROUP *);

/* EC_GROUP_check() returns 1 if 'group' defines a valid group, 0 otherwise */
int EC_GROUP_check(const EC_GROUP *group, BN_CTX *ctx);
/* EC_GROUP_check_discriminant() returns 1 if the discriminant of the
 * elliptic curve is not zero, 0 otherwise */
int EC_GROUP_check_discriminant(const EC_GROUP *, BN_CTX *);

/* EC_GROUP_cmp() returns 0 if both groups are equal and 1 otherwise */
int EC_GROUP_cmp(const EC_GROUP *, const EC_GROUP *, BN_CTX *);

/* EC_GROUP_new_GF*() calls EC_GROUP_new() and EC_GROUP_set_GF*()
 * after choosing an appropriate EC_METHOD */
EC_GROUP *EC_GROUP_new_curve_GFp(const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *);
EC_GROUP *EC_GROUP_new_curve_GF2m(const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *);

/* EC_GROUP_new_by_curve_name() creates a EC_GROUP structure
 * specified by a curve name (in form of a NID) */
EC_GROUP *EC_GROUP_new_by_curve_name(int nid);
/* handling of internal curves */
typedef struct { 
	int nid;
	const char *comment;
	} EC_builtin_curve;
/* EC_builtin_curves(EC_builtin_curve *r, size_t size) returns number 
 * of all available curves or zero if a error occurred. 
 * In case r ist not zero nitems EC_builtin_curve structures 
 * are filled with the data of the first nitems internal groups */
size_t EC_get_builtin_curves(EC_builtin_curve *r, size_t nitems);


/* EC_POINT functions */

EC_POINT *EC_POINT_new(const EC_GROUP *);
void EC_POINT_free(EC_POINT *);
void EC_POINT_clear_free(EC_POINT *);
int EC_POINT_copy(EC_POINT *, const EC_POINT *);
EC_POINT *EC_POINT_dup(const EC_POINT *, const EC_GROUP *);
 
const EC_METHOD *EC_POINT_method_of(const EC_POINT *);

int EC_POINT_set_to_infinity(const EC_GROUP *, EC_POINT *);
int EC_POINT_set_Jprojective_coordinates_GFp(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, const BIGNUM *y, const BIGNUM *z, BN_CTX *);
int EC_POINT_get_Jprojective_coordinates_GFp(const EC_GROUP *, const EC_POINT *,
	BIGNUM *x, BIGNUM *y, BIGNUM *z, BN_CTX *);
int EC_POINT_set_affine_coordinates_GFp(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, const BIGNUM *y, BN_CTX *);
int EC_POINT_get_affine_coordinates_GFp(const EC_GROUP *, const EC_POINT *,
	BIGNUM *x, BIGNUM *y, BN_CTX *);
int EC_POINT_set_compressed_coordinates_GFp(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, int y_bit, BN_CTX *);

int EC_POINT_set_affine_coordinates_GF2m(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, const BIGNUM *y, BN_CTX *);
int EC_POINT_get_affine_coordinates_GF2m(const EC_GROUP *, const EC_POINT *,
	BIGNUM *x, BIGNUM *y, BN_CTX *);
int EC_POINT_set_compressed_coordinates_GF2m(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, int y_bit, BN_CTX *);

size_t EC_POINT_point2oct(const EC_GROUP *, const EC_POINT *, point_conversion_form_t form,
        unsigned char *buf, size_t len, BN_CTX *);
int EC_POINT_oct2point(const EC_GROUP *, EC_POINT *,
        const unsigned char *buf, size_t len, BN_CTX *);

/* other interfaces to point2oct/oct2point: */
BIGNUM *EC_POINT_point2bn(const EC_GROUP *, const EC_POINT *,
	point_conversion_form_t form, BIGNUM *, BN_CTX *);
EC_POINT *EC_POINT_bn2point(const EC_GROUP *, const BIGNUM *,
	EC_POINT *, BN_CTX *);
char *EC_POINT_point2hex(const EC_GROUP *, const EC_POINT *,
	point_conversion_form_t form, BN_CTX *);
EC_POINT *EC_POINT_hex2point(const EC_GROUP *, const char *,
	EC_POINT *, BN_CTX *);

int EC_POINT_add(const EC_GROUP *, EC_POINT *r, const EC_POINT *a, const EC_POINT *b, BN_CTX *);
int EC_POINT_dbl(const EC_GROUP *, EC_POINT *r, const EC_POINT *a, BN_CTX *);
int EC_POINT_invert(const EC_GROUP *, EC_POINT *, BN_CTX *);

int EC_POINT_is_at_infinity(const EC_GROUP *, const EC_POINT *);
int EC_POINT_is_on_curve(const EC_GROUP *, const EC_POINT *, BN_CTX *);
int EC_POINT_cmp(const EC_GROUP *, const EC_POINT *a, const EC_POINT *b, BN_CTX *);

int EC_POINT_make_affine(const EC_GROUP *, EC_POINT *, BN_CTX *);
int EC_POINTs_make_affine(const EC_GROUP *, size_t num, EC_POINT *[], BN_CTX *);


int EC_POINTs_mul(const EC_GROUP *, EC_POINT *r, const BIGNUM *, size_t num, const EC_POINT *[], const BIGNUM *[], BN_CTX *);
int EC_POINT_mul(const EC_GROUP *, EC_POINT *r, const BIGNUM *, const EC_POINT *, const BIGNUM *, BN_CTX *);

/* EC_GROUP_precompute_mult() stores multiples of generator for faster point multiplication */
int EC_GROUP_precompute_mult(EC_GROUP *, BN_CTX *);
/* EC_GROUP_have_precompute_mult() reports whether such precomputation has been done */
int EC_GROUP_have_precompute_mult(const EC_GROUP *);



/* ASN1 stuff */

/* EC_GROUP_get_basis_type() returns the NID of the basis type
 * used to represent the field elements */
int EC_GROUP_get_basis_type(const EC_GROUP *);
int EC_GROUP_get_trinomial_basis(const EC_GROUP *, unsigned int *k);
int EC_GROUP_get_pentanomial_basis(const EC_GROUP *, unsigned int *k1, 
	unsigned int *k2, unsigned int *k3);

#define OPENSSL_EC_NAMED_CURVE	0x001

typedef struct ecpk_parameters_st ECPKPARAMETERS;

EC_GROUP *d2i_ECPKParameters(EC_GROUP **, const unsigned char **in, long len);
int i2d_ECPKParameters(const EC_GROUP *, unsigned char **out);

#define d2i_ECPKParameters_bio(bp,x) ASN1_d2i_bio_of(EC_GROUP,NULL,d2i_ECPKParameters,bp,x)
#define i2d_ECPKParameters_bio(bp,x) ASN1_i2d_bio_of_const(EC_GROUP,i2d_ECPKParameters,bp,x)
#define d2i_ECPKParameters_fp(fp,x) (EC_GROUP *)ASN1_d2i_fp(NULL, \
                (char *(*)())d2i_ECPKParameters,(fp),(unsigned char **)(x))
#define i2d_ECPKParameters_fp(fp,x) ASN1_i2d_fp(i2d_ECPKParameters,(fp), \
		(unsigned char *)(x))

#ifndef OPENSSL_NO_BIO
int     ECPKParameters_print(BIO *bp, const EC_GROUP *x, int off);
#endif
#ifndef OPENSSL_NO_FP_API
int     ECPKParameters_print_fp(FILE *fp, const EC_GROUP *x, int off);
#endif

/* the EC_KEY stuff */
typedef struct ec_key_st EC_KEY;

/* some values for the encoding_flag */
#define EC_PKEY_NO_PARAMETERS	0x001
#define EC_PKEY_NO_PUBKEY	0x002

EC_KEY *EC_KEY_new(void);
EC_KEY *EC_KEY_new_by_curve_name(int nid);
void EC_KEY_free(EC_KEY *);
EC_KEY *EC_KEY_copy(EC_KEY *, const EC_KEY *);
EC_KEY *EC_KEY_dup(const EC_KEY *);

int EC_KEY_up_ref(EC_KEY *);

const EC_GROUP *EC_KEY_get0_group(const EC_KEY *);
int EC_KEY_set_group(EC_KEY *, const EC_GROUP *);
const BIGNUM *EC_KEY_get0_private_key(const EC_KEY *);
int EC_KEY_set_private_key(EC_KEY *, const BIGNUM *);
const EC_POINT *EC_KEY_get0_public_key(const EC_KEY *);
int EC_KEY_set_public_key(EC_KEY *, const EC_POINT *);
unsigned EC_KEY_get_enc_flags(const EC_KEY *);
void EC_KEY_set_enc_flags(EC_KEY *, unsigned int);
point_conversion_form_t EC_KEY_get_conv_form(const EC_KEY *);
void EC_KEY_set_conv_form(EC_KEY *, point_conversion_form_t);
/* functions to set/get method specific data  */
void *EC_KEY_get_key_method_data(EC_KEY *, 
	void *(*dup_func)(void *), void (*free_func)(void *), void (*clear_free_func)(void *));
void EC_KEY_insert_key_method_data(EC_KEY *, void *data,
	void *(*dup_func)(void *), void (*free_func)(void *), void (*clear_free_func)(void *));
/* wrapper functions for the underlying EC_GROUP object */
void EC_KEY_set_asn1_flag(EC_KEY *, int);
int EC_KEY_precompute_mult(EC_KEY *, BN_CTX *ctx);

/* EC_KEY_generate_key() creates a ec private (public) key */
int EC_KEY_generate_key(EC_KEY *);
/* EC_KEY_check_key() */
int EC_KEY_check_key(const EC_KEY *);

/* de- and encoding functions for SEC1 ECPrivateKey */
EC_KEY *d2i_ECPrivateKey(EC_KEY **a, const unsigned char **in, long len);
int i2d_ECPrivateKey(EC_KEY *a, unsigned char **out);
/* de- and encoding functions for EC parameters */
EC_KEY *d2i_ECParameters(EC_KEY **a, const unsigned char **in, long len);
int i2d_ECParameters(EC_KEY *a, unsigned char **out);
/* de- and encoding functions for EC public key
 * (octet string, not DER -- hence 'o2i' and 'i2o') */
EC_KEY *o2i_ECPublicKey(EC_KEY **a, const unsigned char **in, long len);
int i2o_ECPublicKey(EC_KEY *a, unsigned char **out);

#ifndef OPENSSL_NO_BIO
int	ECParameters_print(BIO *bp, const EC_KEY *x);
int	EC_KEY_print(BIO *bp, const EC_KEY *x, int off);
#endif
#ifndef OPENSSL_NO_FP_API
int	ECParameters_print_fp(FILE *fp, const EC_KEY *x);
int	EC_KEY_print_fp(FILE *fp, const EC_KEY *x, int off);
#endif

#define ECParameters_dup(x) ASN1_dup_of(EC_KEY,i2d_ECParameters,d2i_ECParameters,x)

#ifndef __cplusplus
#if defined(__SUNPRO_C)
#  if __SUNPRO_C >= 0x520
# pragma error_messages (default,E_ARRAY_OF_INCOMPLETE_NONAME,E_ARRAY_OF_INCOMPLETE)
#  endif
# endif
#endif

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_EC_strings(void);

/* Error codes for the EC functions. */

/* Function codes. */
#define EC_F_COMPUTE_WNAF				 143
#define EC_F_D2I_ECPARAMETERS				 144
#define EC_F_D2I_ECPKPARAMETERS				 145
#define EC_F_D2I_ECPRIVATEKEY				 146
#define EC_F_ECPARAMETERS_PRINT				 147
#define EC_F_ECPARAMETERS_PRINT_FP			 148
#define EC_F_ECPKPARAMETERS_PRINT			 149
#define EC_F_ECPKPARAMETERS_PRINT_FP			 150
#define EC_F_ECP_NIST_MOD_192				 203
#define EC_F_ECP_NIST_MOD_224				 204
#define EC_F_ECP_NIST_MOD_256				 205
#define EC_F_ECP_NIST_MOD_521				 206
#define EC_F_EC_ASN1_GROUP2CURVE			 153
#define EC_F_EC_ASN1_GROUP2FIELDID			 154
#define EC_F_EC_ASN1_GROUP2PARAMETERS			 155
#define EC_F_EC_ASN1_GROUP2PKPARAMETERS			 156
#define EC_F_EC_ASN1_PARAMETERS2GROUP			 157
#define EC_F_EC_ASN1_PKPARAMETERS2GROUP			 158
#define EC_F_EC_EX_DATA_SET_DATA			 211
#define EC_F_EC_GF2M_MONTGOMERY_POINT_MULTIPLY		 208
#define EC_F_EC_GF2M_SIMPLE_GROUP_CHECK_DISCRIMINANT	 159
#define EC_F_EC_GF2M_SIMPLE_GROUP_SET_CURVE		 195
#define EC_F_EC_GF2M_SIMPLE_OCT2POINT			 160
#define EC_F_EC_GF2M_SIMPLE_POINT2OCT			 161
#define EC_F_EC_GF2M_SIMPLE_POINT_GET_AFFINE_COORDINATES 162
#define EC_F_EC_GF2M_SIMPLE_POINT_SET_AFFINE_COORDINATES 163
#define EC_F_EC_GF2M_SIMPLE_SET_COMPRESSED_COORDINATES	 164
#define EC_F_EC_GFP_MONT_FIELD_DECODE			 133
#define EC_F_EC_GFP_MONT_FIELD_ENCODE			 134
#define EC_F_EC_GFP_MONT_FIELD_MUL			 131
#define EC_F_EC_GFP_MONT_FIELD_SET_TO_ONE		 209
#define EC_F_EC_GFP_MONT_FIELD_SQR			 132
#define EC_F_EC_GFP_MONT_GROUP_SET_CURVE		 189
#define EC_F_EC_GFP_MONT_GROUP_SET_CURVE_GFP		 135
#define EC_F_EC_GFP_NIST_FIELD_MUL			 200
#define EC_F_EC_GFP_NIST_FIELD_SQR			 201
#define EC_F_EC_GFP_NIST_GROUP_SET_CURVE		 202
#define EC_F_EC_GFP_SIMPLE_GROUP_CHECK_DISCRIMINANT	 165
#define EC_F_EC_GFP_SIMPLE_GROUP_SET_CURVE		 166
#define EC_F_EC_GFP_SIMPLE_GROUP_SET_CURVE_GFP		 100
#define EC_F_EC_GFP_SIMPLE_GROUP_SET_GENERATOR		 101
#define EC_F_EC_GFP_SIMPLE_MAKE_AFFINE			 102
#define EC_F_EC_GFP_SIMPLE_OCT2POINT			 103
#define EC_F_EC_GFP_SIMPLE_POINT2OCT			 104
#define EC_F_EC_GFP_SIMPLE_POINTS_MAKE_AFFINE		 137
#define EC_F_EC_GFP_SIMPLE_POINT_GET_AFFINE_COORDINATES	 167
#define EC_F_EC_GFP_SIMPLE_POINT_GET_AFFINE_COORDINATES_GFP 105
#define EC_F_EC_GFP_SIMPLE_POINT_SET_AFFINE_COORDINATES	 168
#define EC_F_EC_GFP_SIMPLE_POINT_SET_AFFINE_COORDINATES_GFP 128
#define EC_F_EC_GFP_SIMPLE_SET_COMPRESSED_COORDINATES	 169
#define EC_F_EC_GFP_SIMPLE_SET_COMPRESSED_COORDINATES_GFP 129
#define EC_F_EC_GROUP_CHECK				 170
#define EC_F_EC_GROUP_CHECK_DISCRIMINANT		 171
#define EC_F_EC_GROUP_COPY				 106
#define EC_F_EC_GROUP_GET0_GENERATOR			 139
#define EC_F_EC_GROUP_GET_COFACTOR			 140
#define EC_F_EC_GROUP_GET_CURVE_GF2M			 172
#define EC_F_EC_GROUP_GET_CURVE_GFP			 130
#define EC_F_EC_GROUP_GET_DEGREE			 173
#define EC_F_EC_GROUP_GET_ORDER				 141
#define EC_F_EC_GROUP_GET_PENTANOMIAL_BASIS		 193
#define EC_F_EC_GROUP_GET_TRINOMIAL_BASIS		 194
#define EC_F_EC_GROUP_NEW				 108
#define EC_F_EC_GROUP_NEW_BY_CURVE_NAME			 174
#define EC_F_EC_GROUP_NEW_FROM_DATA			 175
#define EC_F_EC_GROUP_PRECOMPUTE_MULT			 142
#define EC_F_EC_GROUP_SET_CURVE_GF2M			 176
#define EC_F_EC_GROUP_SET_CURVE_GFP			 109
#define EC_F_EC_GROUP_SET_EXTRA_DATA			 110
#define EC_F_EC_GROUP_SET_GENERATOR			 111
#define EC_F_EC_KEY_CHECK_KEY				 177
#define EC_F_EC_KEY_COPY				 178
#define EC_F_EC_KEY_GENERATE_KEY			 179
#define EC_F_EC_KEY_NEW					 182
#define EC_F_EC_KEY_PRINT				 180
#define EC_F_EC_KEY_PRINT_FP				 181
#define EC_F_EC_POINTS_MAKE_AFFINE			 136
#define EC_F_EC_POINTS_MUL				 138
#define EC_F_EC_POINT_ADD				 112
#define EC_F_EC_POINT_CMP				 113
#define EC_F_EC_POINT_COPY				 114
#define EC_F_EC_POINT_DBL				 115
#define EC_F_EC_POINT_GET_AFFINE_COORDINATES_GF2M	 183
#define EC_F_EC_POINT_GET_AFFINE_COORDINATES_GFP	 116
#define EC_F_EC_POINT_GET_JPROJECTIVE_COORDINATES_GFP	 117
#define EC_F_EC_POINT_INVERT				 210
#define EC_F_EC_POINT_IS_AT_INFINITY			 118
#define EC_F_EC_POINT_IS_ON_CURVE			 119
#define EC_F_EC_POINT_MAKE_AFFINE			 120
#define EC_F_EC_POINT_MUL				 184
#define EC_F_EC_POINT_NEW				 121
#define EC_F_EC_POINT_OCT2POINT				 122
#define EC_F_EC_POINT_POINT2OCT				 123
#define EC_F_EC_POINT_SET_AFFINE_COORDINATES_GF2M	 185
#define EC_F_EC_POINT_SET_AFFINE_COORDINATES_GFP	 124
#define EC_F_EC_POINT_SET_COMPRESSED_COORDINATES_GF2M	 186
#define EC_F_EC_POINT_SET_COMPRESSED_COORDINATES_GFP	 125
#define EC_F_EC_POINT_SET_JPROJECTIVE_COORDINATES_GFP	 126
#define EC_F_EC_POINT_SET_TO_INFINITY			 127
#define EC_F_EC_PRE_COMP_DUP				 207
#define EC_F_EC_PRE_COMP_NEW				 196
#define EC_F_EC_WNAF_MUL				 187
#define EC_F_EC_WNAF_PRECOMPUTE_MULT			 188
#define EC_F_I2D_ECPARAMETERS				 190
#define EC_F_I2D_ECPKPARAMETERS				 191
#define EC_F_I2D_ECPRIVATEKEY				 192
#define EC_F_I2O_ECPUBLICKEY				 151
#define EC_F_O2I_ECPUBLICKEY				 152

/* Reason codes. */
#define EC_R_ASN1_ERROR					 115
#define EC_R_ASN1_UNKNOWN_FIELD				 116
#define EC_R_BUFFER_TOO_SMALL				 100
#define EC_R_D2I_ECPKPARAMETERS_FAILURE			 117
#define EC_R_DISCRIMINANT_IS_ZERO			 118
#define EC_R_EC_GROUP_NEW_BY_NAME_FAILURE		 119
#define EC_R_FIELD_TOO_LARGE				 138
#define EC_R_GROUP2PKPARAMETERS_FAILURE			 120
#define EC_R_I2D_ECPKPARAMETERS_FAILURE			 121
#define EC_R_INCOMPATIBLE_OBJECTS			 101
#define EC_R_INVALID_ARGUMENT				 112
#define EC_R_INVALID_COMPRESSED_POINT			 110
#define EC_R_INVALID_COMPRESSION_BIT			 109
#define EC_R_INVALID_ENCODING				 102
#define EC_R_INVALID_FIELD				 103
#define EC_R_INVALID_FORM				 104
#define EC_R_INVALID_GROUP_ORDER			 122
#define EC_R_INVALID_PENTANOMIAL_BASIS			 132
#define EC_R_INVALID_PRIVATE_KEY			 123
#define EC_R_INVALID_TRINOMIAL_BASIS			 137
#define EC_R_MISSING_PARAMETERS				 124
#define EC_R_MISSING_PRIVATE_KEY			 125
#define EC_R_NOT_A_NIST_PRIME				 135
#define EC_R_NOT_A_SUPPORTED_NIST_PRIME			 136
#define EC_R_NOT_IMPLEMENTED				 126
#define EC_R_NOT_INITIALIZED				 111
#define EC_R_NO_FIELD_MOD				 133
#define EC_R_PASSED_NULL_PARAMETER			 134
#define EC_R_PKPARAMETERS2GROUP_FAILURE			 127
#define EC_R_POINT_AT_INFINITY				 106
#define EC_R_POINT_IS_NOT_ON_CURVE			 107
#define EC_R_SLOT_FULL					 108
#define EC_R_UNDEFINED_GENERATOR			 113
#define EC_R_UNDEFINED_ORDER				 128
#define EC_R_UNKNOWN_GROUP				 129
#define EC_R_UNKNOWN_ORDER				 114
#define EC_R_UNSUPPORTED_FIELD				 131
#define EC_R_WRONG_ORDER				 130

#ifdef  __cplusplus
}
#endif
#endif

/* crypto/ec/ec2_smpl.c */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The software is originally written by Sheueling Chang Shantz and
 * Douglas Stebila of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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

#include <openssl/err.h>

#include "ec_lcl.h"


const EC_METHOD *EC_GF2m_simple_method(void)
	{
	static const EC_METHOD ret = {
		NID_X9_62_characteristic_two_field,
		ec_GF2m_simple_group_init,
		ec_GF2m_simple_group_finish,
		ec_GF2m_simple_group_clear_finish,
		ec_GF2m_simple_group_copy,
		ec_GF2m_simple_group_set_curve,
		ec_GF2m_simple_group_get_curve,
		ec_GF2m_simple_group_get_degree,
		ec_GF2m_simple_group_check_discriminant,
		ec_GF2m_simple_point_init,
		ec_GF2m_simple_point_finish,
		ec_GF2m_simple_point_clear_finish,
		ec_GF2m_simple_point_copy,
		ec_GF2m_simple_point_set_to_infinity,
		0 /* set_Jprojective_coordinates_GFp */,
		0 /* get_Jprojective_coordinates_GFp */,
		ec_GF2m_simple_point_set_affine_coordinates,
		ec_GF2m_simple_point_get_affine_coordinates,
		ec_GF2m_simple_set_compressed_coordinates,
		ec_GF2m_simple_point2oct,
		ec_GF2m_simple_oct2point,
		ec_GF2m_simple_add,
		ec_GF2m_simple_dbl,
		ec_GF2m_simple_invert,
		ec_GF2m_simple_is_at_infinity,
		ec_GF2m_simple_is_on_curve,
		ec_GF2m_simple_cmp,
		ec_GF2m_simple_make_affine,
		ec_GF2m_simple_points_make_affine,

		/* the following three method functions are defined in ec2_mult.c */
		ec_GF2m_simple_mul,
		ec_GF2m_precompute_mult,
		ec_GF2m_have_precompute_mult,

		ec_GF2m_simple_field_mul,
		ec_GF2m_simple_field_sqr,
		ec_GF2m_simple_field_div,
		0 /* field_encode */,
		0 /* field_decode */,
		0 /* field_set_to_one */ };

	return &ret;
	}


/* Initialize a GF(2^m)-based EC_GROUP structure.
 * Note that all other members are handled by EC_GROUP_new.
 */
int ec_GF2m_simple_group_init(EC_GROUP *group)
	{
	BN_init(&group->field);
	BN_init(&group->a);
	BN_init(&group->b);
	return 1;
	}


/* Free a GF(2^m)-based EC_GROUP structure.
 * Note that all other members are handled by EC_GROUP_free.
 */
void ec_GF2m_simple_group_finish(EC_GROUP *group)
	{
	BN_free(&group->field);
	BN_free(&group->a);
	BN_free(&group->b);
	}


/* Clear and free a GF(2^m)-based EC_GROUP structure.
 * Note that all other members are handled by EC_GROUP_clear_free.
 */
void ec_GF2m_simple_group_clear_finish(EC_GROUP *group)
	{
	BN_clear_free(&group->field);
	BN_clear_free(&group->a);
	BN_clear_free(&group->b);
	group->poly[0] = 0;
	group->poly[1] = 0;
	group->poly[2] = 0;
	group->poly[3] = 0;
	group->poly[4] = 0;
	group->poly[5] = -1;
	}


/* Copy a GF(2^m)-based EC_GROUP structure.
 * Note that all other members are handled by EC_GROUP_copy.
 */
int ec_GF2m_simple_group_copy(EC_GROUP *dest, const EC_GROUP *src)
	{
	int i;
	if (!BN_copy(&dest->field, &src->field)) return 0;
	if (!BN_copy(&dest->a, &src->a)) return 0;
	if (!BN_copy(&dest->b, &src->b)) return 0;
	dest->poly[0] = src->poly[0];
	dest->poly[1] = src->poly[1];
	dest->poly[2] = src->poly[2];
	dest->poly[3] = src->poly[3];
	dest->poly[4] = src->poly[4];
	dest->poly[5] = src->poly[5];
	if (bn_wexpand(&dest->a, (int)(dest->poly[0] + BN_BITS2 - 1) / BN_BITS2) == NULL) return 0;
	if (bn_wexpand(&dest->b, (int)(dest->poly[0] + BN_BITS2 - 1) / BN_BITS2) == NULL) return 0;
	for (i = dest->a.top; i < dest->a.dmax; i++) dest->a.d[i] = 0;
	for (i = dest->b.top; i < dest->b.dmax; i++) dest->b.d[i] = 0;
	return 1;
	}


/* Set the curve parameters of an EC_GROUP structure. */
int ec_GF2m_simple_group_set_curve(EC_GROUP *group,
	const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
	{
	int ret = 0, i;

	/* group->field */
	if (!BN_copy(&group->field, p)) goto err;
	i = BN_GF2m_poly2arr(&group->field, group->poly, 6) - 1;
	if ((i != 5) && (i != 3))
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_GROUP_SET_CURVE, EC_R_UNSUPPORTED_FIELD);
		goto err;
		}

	/* group->a */
	if (!BN_GF2m_mod_arr(&group->a, a, group->poly)) goto err;
	if(bn_wexpand(&group->a, (int)(group->poly[0] + BN_BITS2 - 1) / BN_BITS2) == NULL) goto err;
	for (i = group->a.top; i < group->a.dmax; i++) group->a.d[i] = 0;
	
	/* group->b */
	if (!BN_GF2m_mod_arr(&group->b, b, group->poly)) goto err;
	if(bn_wexpand(&group->b, (int)(group->poly[0] + BN_BITS2 - 1) / BN_BITS2) == NULL) goto err;
	for (i = group->b.top; i < group->b.dmax; i++) group->b.d[i] = 0;
		
	ret = 1;
  err:
	return ret;
	}


/* Get the curve parameters of an EC_GROUP structure.
 * If p, a, or b are NULL then there values will not be set but the method will return with success.
 */
int ec_GF2m_simple_group_get_curve(const EC_GROUP *group, BIGNUM *p, BIGNUM *a, BIGNUM *b, BN_CTX *ctx)
	{
	int ret = 0;
	
	if (p != NULL)
		{
		if (!BN_copy(p, &group->field)) return 0;
		}

	if (a != NULL)
		{
		if (!BN_copy(a, &group->a)) goto err;
		}

	if (b != NULL)
		{
		if (!BN_copy(b, &group->b)) goto err;
		}
	
	ret = 1;
	
  err:
	return ret;
	}


/* Gets the degree of the field.  For a curve over GF(2^m) this is the value m. */
int ec_GF2m_simple_group_get_degree(const EC_GROUP *group)
	{
	return BN_num_bits(&group->field)-1;
	}


/* Checks the discriminant of the curve.
 * y^2 + x*y = x^3 + a*x^2 + b is an elliptic curve <=> b != 0 (mod p) 
 */
int ec_GF2m_simple_group_check_discriminant(const EC_GROUP *group, BN_CTX *ctx)
	{
	int ret = 0;
	BIGNUM *b;
	BN_CTX *new_ctx = NULL;

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_GROUP_CHECK_DISCRIMINANT, ERR_R_MALLOC_FAILURE);
			goto err;
			}
		}
	BN_CTX_start(ctx);
	b = BN_CTX_get(ctx);
	if (b == NULL) goto err;

	if (!BN_GF2m_mod_arr(b, &group->b, group->poly)) goto err;
	
	/* check the discriminant:
	 * y^2 + x*y = x^3 + a*x^2 + b is an elliptic curve <=> b != 0 (mod p) 
	 */
	if (BN_is_zero(b)) goto err;

	ret = 1;

err:
	if (ctx != NULL)
		BN_CTX_end(ctx);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return ret;
	}


/* Initializes an EC_POINT. */
int ec_GF2m_simple_point_init(EC_POINT *point)
	{
	BN_init(&point->X);
	BN_init(&point->Y);
	BN_init(&point->Z);
	return 1;
	}


/* Frees an EC_POINT. */
void ec_GF2m_simple_point_finish(EC_POINT *point)
	{
	BN_free(&point->X);
	BN_free(&point->Y);
	BN_free(&point->Z);
	}


/* Clears and frees an EC_POINT. */
void ec_GF2m_simple_point_clear_finish(EC_POINT *point)
	{
	BN_clear_free(&point->X);
	BN_clear_free(&point->Y);
	BN_clear_free(&point->Z);
	point->Z_is_one = 0;
	}


/* Copy the contents of one EC_POINT into another.  Assumes dest is initialized. */
int ec_GF2m_simple_point_copy(EC_POINT *dest, const EC_POINT *src)
	{
	if (!BN_copy(&dest->X, &src->X)) return 0;
	if (!BN_copy(&dest->Y, &src->Y)) return 0;
	if (!BN_copy(&dest->Z, &src->Z)) return 0;
	dest->Z_is_one = src->Z_is_one;

	return 1;
	}


/* Set an EC_POINT to the point at infinity.  
 * A point at infinity is represented by having Z=0.
 */
int ec_GF2m_simple_point_set_to_infinity(const EC_GROUP *group, EC_POINT *point)
	{
	point->Z_is_one = 0;
	BN_zero(&point->Z);
	return 1;
	}


/* Set the coordinates of an EC_POINT using affine coordinates. 
 * Note that the simple implementation only uses affine coordinates.
 */
int ec_GF2m_simple_point_set_affine_coordinates(const EC_GROUP *group, EC_POINT *point,
	const BIGNUM *x, const BIGNUM *y, BN_CTX *ctx)
	{
	int ret = 0;	
	if (x == NULL || y == NULL)
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_POINT_SET_AFFINE_COORDINATES, ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}

	if (!BN_copy(&point->X, x)) goto err;
	BN_set_negative(&point->X, 0);
	if (!BN_copy(&point->Y, y)) goto err;
	BN_set_negative(&point->Y, 0);
	if (!BN_copy(&point->Z, BN_value_one())) goto err;
	BN_set_negative(&point->Z, 0);
	point->Z_is_one = 1;
	ret = 1;

  err:
	return ret;
	}


/* Gets the affine coordinates of an EC_POINT. 
 * Note that the simple implementation only uses affine coordinates.
 */
int ec_GF2m_simple_point_get_affine_coordinates(const EC_GROUP *group, const EC_POINT *point,
	BIGNUM *x, BIGNUM *y, BN_CTX *ctx)
	{
	int ret = 0;

	if (EC_POINT_is_at_infinity(group, point))
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_POINT_GET_AFFINE_COORDINATES, EC_R_POINT_AT_INFINITY);
		return 0;
		}

	if (BN_cmp(&point->Z, BN_value_one())) 
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_POINT_GET_AFFINE_COORDINATES, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;
		}
	if (x != NULL)
		{
		if (!BN_copy(x, &point->X)) goto err;
		BN_set_negative(x, 0);
		}
	if (y != NULL)
		{
		if (!BN_copy(y, &point->Y)) goto err;
		BN_set_negative(y, 0);
		}
	ret = 1;
		
 err:
	return ret;
	}


/* Calculates and sets the affine coordinates of an EC_POINT from the given
 * compressed coordinates.  Uses algorithm 2.3.4 of SEC 1. 
 * Note that the simple implementation only uses affine coordinates.
 *
 * The method is from the following publication:
 * 
 *     Harper, Menezes, Vanstone:
 *     "Public-Key Cryptosystems with Very Small Key Lengths",
 *     EUROCRYPT '92, Springer-Verlag LNCS 658,
 *     published February 1993
 *
 * US Patents 6,141,420 and 6,618,483 (Vanstone, Mullin, Agnew) describe
 * the same method, but claim no priority date earlier than July 29, 1994
 * (and additionally fail to cite the EUROCRYPT '92 publication as prior art).
 */
int ec_GF2m_simple_set_compressed_coordinates(const EC_GROUP *group, EC_POINT *point,
	const BIGNUM *x_, int y_bit, BN_CTX *ctx)
	{
	BN_CTX *new_ctx = NULL;
	BIGNUM *tmp, *x, *y, *z;
	int ret = 0, z0;

	/* clear error queue */
	ERR_clear_error();

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
		}

	y_bit = (y_bit != 0) ? 1 : 0;

	BN_CTX_start(ctx);
	tmp = BN_CTX_get(ctx);
	x = BN_CTX_get(ctx);
	y = BN_CTX_get(ctx);
	z = BN_CTX_get(ctx);
	if (z == NULL) goto err;

	if (!BN_GF2m_mod_arr(x, x_, group->poly)) goto err;
	if (BN_is_zero(x))
		{
		if (!BN_GF2m_mod_sqrt_arr(y, &group->b, group->poly, ctx)) goto err;
		}
	else
		{
		if (!group->meth->field_sqr(group, tmp, x, ctx)) goto err;
		if (!group->meth->field_div(group, tmp, &group->b, tmp, ctx)) goto err;
		if (!BN_GF2m_add(tmp, &group->a, tmp)) goto err;
		if (!BN_GF2m_add(tmp, x, tmp)) goto err;
		if (!BN_GF2m_mod_solve_quad_arr(z, tmp, group->poly, ctx))
			{
			unsigned long err = ERR_peek_last_error();
			
			if (ERR_GET_LIB(err) == ERR_LIB_BN && ERR_GET_REASON(err) == BN_R_NO_SOLUTION)
				{
				ERR_clear_error();
				ECerr(EC_F_EC_GF2M_SIMPLE_SET_COMPRESSED_COORDINATES, EC_R_INVALID_COMPRESSED_POINT);
				}
			else
				ECerr(EC_F_EC_GF2M_SIMPLE_SET_COMPRESSED_COORDINATES, ERR_R_BN_LIB);
			goto err;
			}
		z0 = (BN_is_odd(z)) ? 1 : 0;
		if (!group->meth->field_mul(group, y, x, z, ctx)) goto err;
		if (z0 != y_bit)
			{
			if (!BN_GF2m_add(y, y, x)) goto err;
			}
		}

	if (!EC_POINT_set_affine_coordinates_GF2m(group, point, x, y, ctx)) goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return ret;
	}


/* Converts an EC_POINT to an octet string.  
 * If buf is NULL, the encoded length will be returned.
 * If the length len of buf is smaller than required an error will be returned.
 */
size_t ec_GF2m_simple_point2oct(const EC_GROUP *group, const EC_POINT *point, point_conversion_form_t form,
	unsigned char *buf, size_t len, BN_CTX *ctx)
	{
	size_t ret;
	BN_CTX *new_ctx = NULL;
	int used_ctx = 0;
	BIGNUM *x, *y, *yxi;
	size_t field_len, i, skip;

	if ((form != POINT_CONVERSION_COMPRESSED)
		&& (form != POINT_CONVERSION_UNCOMPRESSED)
		&& (form != POINT_CONVERSION_HYBRID))
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, EC_R_INVALID_FORM);
		goto err;
		}

	if (EC_POINT_is_at_infinity(group, point))
		{
		/* encodes to a single 0 octet */
		if (buf != NULL)
			{
			if (len < 1)
				{
				ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, EC_R_BUFFER_TOO_SMALL);
				return 0;
				}
			buf[0] = 0;
			}
		return 1;
		}


	/* ret := required output buffer length */
	field_len = (EC_GROUP_get_degree(group) + 7) / 8;
	ret = (form == POINT_CONVERSION_COMPRESSED) ? 1 + field_len : 1 + 2*field_len;

	/* if 'buf' is NULL, just return required length */
	if (buf != NULL)
		{
		if (len < ret)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, EC_R_BUFFER_TOO_SMALL);
			goto err;
			}

		if (ctx == NULL)
			{
			ctx = new_ctx = BN_CTX_new();
			if (ctx == NULL)
				return 0;
			}

		BN_CTX_start(ctx);
		used_ctx = 1;
		x = BN_CTX_get(ctx);
		y = BN_CTX_get(ctx);
		yxi = BN_CTX_get(ctx);
		if (yxi == NULL) goto err;

		if (!EC_POINT_get_affine_coordinates_GF2m(group, point, x, y, ctx)) goto err;

		buf[0] = form;
		if ((form != POINT_CONVERSION_UNCOMPRESSED) && !BN_is_zero(x))
			{
			if (!group->meth->field_div(group, yxi, y, x, ctx)) goto err;
			if (BN_is_odd(yxi)) buf[0]++;
			}

		i = 1;
		
		skip = field_len - BN_num_bytes(x);
		if (skip > field_len)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, ERR_R_INTERNAL_ERROR);
			goto err;
			}
		while (skip > 0)
			{
			buf[i++] = 0;
			skip--;
			}
		skip = BN_bn2bin(x, buf + i);
		i += skip;
		if (i != 1 + field_len)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, ERR_R_INTERNAL_ERROR);
			goto err;
			}

		if (form == POINT_CONVERSION_UNCOMPRESSED || form == POINT_CONVERSION_HYBRID)
			{
			skip = field_len - BN_num_bytes(y);
			if (skip > field_len)
				{
				ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			while (skip > 0)
				{
				buf[i++] = 0;
				skip--;
				}
			skip = BN_bn2bin(y, buf + i);
			i += skip;
			}

		if (i != ret)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_POINT2OCT, ERR_R_INTERNAL_ERROR);
			goto err;
			}
		}
	
	if (used_ctx)
		BN_CTX_end(ctx);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return ret;

 err:
	if (used_ctx)
		BN_CTX_end(ctx);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return 0;
	}


/* Converts an octet string representation to an EC_POINT. 
 * Note that the simple implementation only uses affine coordinates.
 */
int ec_GF2m_simple_oct2point(const EC_GROUP *group, EC_POINT *point,
	const unsigned char *buf, size_t len, BN_CTX *ctx)
	{
	point_conversion_form_t form;
	int y_bit;
	BN_CTX *new_ctx = NULL;
	BIGNUM *x, *y, *yxi;
	size_t field_len, enc_len;
	int ret = 0;

	if (len == 0)
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_BUFFER_TOO_SMALL);
		return 0;
		}
	form = buf[0];
	y_bit = form & 1;
	form = form & ~1U;
	if ((form != 0)	&& (form != POINT_CONVERSION_COMPRESSED)
		&& (form != POINT_CONVERSION_UNCOMPRESSED)
		&& (form != POINT_CONVERSION_HYBRID))
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
		return 0;
		}
	if ((form == 0 || form == POINT_CONVERSION_UNCOMPRESSED) && y_bit)
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
		return 0;
		}

	if (form == 0)
		{
		if (len != 1)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
			return 0;
			}

		return EC_POINT_set_to_infinity(group, point);
		}
	
	field_len = (EC_GROUP_get_degree(group) + 7) / 8;
	enc_len = (form == POINT_CONVERSION_COMPRESSED) ? 1 + field_len : 1 + 2*field_len;

	if (len != enc_len)
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
		return 0;
		}

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
		}

	BN_CTX_start(ctx);
	x = BN_CTX_get(ctx);
	y = BN_CTX_get(ctx);
	yxi = BN_CTX_get(ctx);
	if (yxi == NULL) goto err;

	if (!BN_bin2bn(buf + 1, field_len, x)) goto err;
	if (BN_ucmp(x, &group->field) >= 0)
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
		goto err;
		}

	if (form == POINT_CONVERSION_COMPRESSED)
		{
		if (!EC_POINT_set_compressed_coordinates_GF2m(group, point, x, y_bit, ctx)) goto err;
		}
	else
		{
		if (!BN_bin2bn(buf + 1 + field_len, field_len, y)) goto err;
		if (BN_ucmp(y, &group->field) >= 0)
			{
			ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
			goto err;
			}
		if (form == POINT_CONVERSION_HYBRID)
			{
			if (!group->meth->field_div(group, yxi, y, x, ctx)) goto err;
			if (y_bit != BN_is_odd(yxi))
				{
				ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_INVALID_ENCODING);
				goto err;
				}
			}

		if (!EC_POINT_set_affine_coordinates_GF2m(group, point, x, y, ctx)) goto err;
		}
	
	if (!EC_POINT_is_on_curve(group, point, ctx)) /* test required by X9.62 */
		{
		ECerr(EC_F_EC_GF2M_SIMPLE_OCT2POINT, EC_R_POINT_IS_NOT_ON_CURVE);
		goto err;
		}

	ret = 1;
	
 err:
	BN_CTX_end(ctx);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return ret;
	}


/* Computes a + b and stores the result in r.  r could be a or b, a could be b.
 * Uses algorithm A.10.2 of IEEE P1363.
 */
int ec_GF2m_simple_add(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a, const EC_POINT *b, BN_CTX *ctx)
	{
	BN_CTX *new_ctx = NULL;
	BIGNUM *x0, *y0, *x1, *y1, *x2, *y2, *s, *t;
	int ret = 0;
	
	if (EC_POINT_is_at_infinity(group, a))
		{
		if (!EC_POINT_copy(r, b)) return 0;
		return 1;
		}

	if (EC_POINT_is_at_infinity(group, b))
		{
		if (!EC_POINT_copy(r, a)) return 0;
		return 1;
		}

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
		}

	BN_CTX_start(ctx);
	x0 = BN_CTX_get(ctx);
	y0 = BN_CTX_get(ctx);
	x1 = BN_CTX_get(ctx);
	y1 = BN_CTX_get(ctx);
	x2 = BN_CTX_get(ctx);
	y2 = BN_CTX_get(ctx);
	s = BN_CTX_get(ctx);
	t = BN_CTX_get(ctx);
	if (t == NULL) goto err;

	if (a->Z_is_one) 
		{
		if (!BN_copy(x0, &a->X)) goto err;
		if (!BN_copy(y0, &a->Y)) goto err;
		}
	else
		{
		if (!EC_POINT_get_affine_coordinates_GF2m(group, a, x0, y0, ctx)) goto err;
		}
	if (b->Z_is_one) 
		{
		if (!BN_copy(x1, &b->X)) goto err;
		if (!BN_copy(y1, &b->Y)) goto err;
		}
	else
		{
		if (!EC_POINT_get_affine_coordinates_GF2m(group, b, x1, y1, ctx)) goto err;
		}


	if (BN_GF2m_cmp(x0, x1))
		{
		if (!BN_GF2m_add(t, x0, x1)) goto err;
		if (!BN_GF2m_add(s, y0, y1)) goto err;
		if (!group->meth->field_div(group, s, s, t, ctx)) goto err;
		if (!group->meth->field_sqr(group, x2, s, ctx)) goto err;
		if (!BN_GF2m_add(x2, x2, &group->a)) goto err;
		if (!BN_GF2m_add(x2, x2, s)) goto err;
		if (!BN_GF2m_add(x2, x2, t)) goto err;
		}
	else
		{
		if (BN_GF2m_cmp(y0, y1) || BN_is_zero(x1))
			{
			if (!EC_POINT_set_to_infinity(group, r)) goto err;
			ret = 1;
			goto err;
			}
		if (!group->meth->field_div(group, s, y1, x1, ctx)) goto err;
		if (!BN_GF2m_add(s, s, x1)) goto err;
		
		if (!group->meth->field_sqr(group, x2, s, ctx)) goto err;
		if (!BN_GF2m_add(x2, x2, s)) goto err;
		if (!BN_GF2m_add(x2, x2, &group->a)) goto err;
		}

	if (!BN_GF2m_add(y2, x1, x2)) goto err;
	if (!group->meth->field_mul(group, y2, y2, s, ctx)) goto err;
	if (!BN_GF2m_add(y2, y2, x2)) goto err;
	if (!BN_GF2m_add(y2, y2, y1)) goto err;

	if (!EC_POINT_set_affine_coordinates_GF2m(group, r, x2, y2, ctx)) goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return ret;
	}


/* Computes 2 * a and stores the result in r.  r could be a.
 * Uses algorithm A.10.2 of IEEE P1363.
 */
int ec_GF2m_simple_dbl(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a, BN_CTX *ctx)
	{
	return ec_GF2m_simple_add(group, r, a, a, ctx);
	}


int ec_GF2m_simple_invert(const EC_GROUP *group, EC_POINT *point, BN_CTX *ctx)
	{
	if (EC_POINT_is_at_infinity(group, point) || BN_is_zero(&point->Y))
		/* point is its own inverse */
		return 1;
	
	if (!EC_POINT_make_affine(group, point, ctx)) return 0;
	return BN_GF2m_add(&point->Y, &point->X, &point->Y);
	}


/* Indicates whether the given point is the point at infinity. */
int ec_GF2m_simple_is_at_infinity(const EC_GROUP *group, const EC_POINT *point)
	{
	return BN_is_zero(&point->Z);
	}


/* Determines whether the given EC_POINT is an actual point on the curve defined
 * in the EC_GROUP.  A point is valid if it satisfies the Weierstrass equation:
 *      y^2 + x*y = x^3 + a*x^2 + b.
 */
int ec_GF2m_simple_is_on_curve(const EC_GROUP *group, const EC_POINT *point, BN_CTX *ctx)
	{
	int ret = -1;
	BN_CTX *new_ctx = NULL;
	BIGNUM *lh, *y2;
	int (*field_mul)(const EC_GROUP *, BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
	int (*field_sqr)(const EC_GROUP *, BIGNUM *, const BIGNUM *, BN_CTX *);

	if (EC_POINT_is_at_infinity(group, point))
		return 1;

	field_mul = group->meth->field_mul;
	field_sqr = group->meth->field_sqr;	

	/* only support affine coordinates */
	if (!point->Z_is_one) return -1;

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return -1;
		}

	BN_CTX_start(ctx);
	y2 = BN_CTX_get(ctx);
	lh = BN_CTX_get(ctx);
	if (lh == NULL) goto err;

	/* We have a curve defined by a Weierstrass equation
	 *      y^2 + x*y = x^3 + a*x^2 + b.
	 *  <=> x^3 + a*x^2 + x*y + b + y^2 = 0
	 *  <=> ((x + a) * x + y ) * x + b + y^2 = 0
	 */
	if (!BN_GF2m_add(lh, &point->X, &group->a)) goto err;
	if (!field_mul(group, lh, lh, &point->X, ctx)) goto err;
	if (!BN_GF2m_add(lh, lh, &point->Y)) goto err;
	if (!field_mul(group, lh, lh, &point->X, ctx)) goto err;
	if (!BN_GF2m_add(lh, lh, &group->b)) goto err;
	if (!field_sqr(group, y2, &point->Y, ctx)) goto err;
	if (!BN_GF2m_add(lh, lh, y2)) goto err;
	ret = BN_is_zero(lh);
 err:
	if (ctx) BN_CTX_end(ctx);
	if (new_ctx) BN_CTX_free(new_ctx);
	return ret;
	}


/* Indicates whether two points are equal.
 * Return values:
 *  -1   error
 *   0   equal (in affine coordinates)
 *   1   not equal
 */
int ec_GF2m_simple_cmp(const EC_GROUP *group, const EC_POINT *a, const EC_POINT *b, BN_CTX *ctx)
	{
	BIGNUM *aX, *aY, *bX, *bY;
	BN_CTX *new_ctx = NULL;
	int ret = -1;

	if (EC_POINT_is_at_infinity(group, a))
		{
		return EC_POINT_is_at_infinity(group, b) ? 0 : 1;
		}

	if (EC_POINT_is_at_infinity(group, b))
		return 1;
	
	if (a->Z_is_one && b->Z_is_one)
		{
		return ((BN_cmp(&a->X, &b->X) == 0) && BN_cmp(&a->Y, &b->Y) == 0) ? 0 : 1;
		}

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return -1;
		}

	BN_CTX_start(ctx);
	aX = BN_CTX_get(ctx);
	aY = BN_CTX_get(ctx);
	bX = BN_CTX_get(ctx);
	bY = BN_CTX_get(ctx);
	if (bY == NULL) goto err;

	if (!EC_POINT_get_affine_coordinates_GF2m(group, a, aX, aY, ctx)) goto err;
	if (!EC_POINT_get_affine_coordinates_GF2m(group, b, bX, bY, ctx)) goto err;
	ret = ((BN_cmp(aX, bX) == 0) && BN_cmp(aY, bY) == 0) ? 0 : 1;

  err:	
	if (ctx) BN_CTX_end(ctx);
	if (new_ctx) BN_CTX_free(new_ctx);
	return ret;
	}


/* Forces the given EC_POINT to internally use affine coordinates. */
int ec_GF2m_simple_make_affine(const EC_GROUP *group, EC_POINT *point, BN_CTX *ctx)
	{
	BN_CTX *new_ctx = NULL;
	BIGNUM *x, *y;
	int ret = 0;

	if (point->Z_is_one || EC_POINT_is_at_infinity(group, point))
		return 1;
	
	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
		}

	BN_CTX_start(ctx);
	x = BN_CTX_get(ctx);
	y = BN_CTX_get(ctx);
	if (y == NULL) goto err;
	
	if (!EC_POINT_get_affine_coordinates_GF2m(group, point, x, y, ctx)) goto err;
	if (!BN_copy(&point->X, x)) goto err;
	if (!BN_copy(&point->Y, y)) goto err;
	if (!BN_one(&point->Z)) goto err;
	
	ret = 1;		

  err:
	if (ctx) BN_CTX_end(ctx);
	if (new_ctx) BN_CTX_free(new_ctx);
	return ret;
	}


/* Forces each of the EC_POINTs in the given array to use affine coordinates. */
int ec_GF2m_simple_points_make_affine(const EC_GROUP *group, size_t num, EC_POINT *points[], BN_CTX *ctx)
	{
	size_t i;

	for (i = 0; i < num; i++)
		{
		if (!group->meth->make_affine(group, points[i], ctx)) return 0;
		}

	return 1;
	}


/* Wrapper to simple binary polynomial field multiplication implementation. */
int ec_GF2m_simple_field_mul(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
	{
	return BN_GF2m_mod_mul_arr(r, a, b, group->poly, ctx);
	}


/* Wrapper to simple binary polynomial field squaring implementation. */
int ec_GF2m_simple_field_sqr(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a, BN_CTX *ctx)
	{
	return BN_GF2m_mod_sqr_arr(r, a, group->poly, ctx);
	}


/* Wrapper to simple binary polynomial field division implementation. */
int ec_GF2m_simple_field_div(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
	{
	return BN_GF2m_mod_div(r, a, b, &group->field, ctx);
	}

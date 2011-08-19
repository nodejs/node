/* crypto/ec/ec2_smpt.c */
/* This code was originally written by Douglas Stebila 
 * <dstebila@student.math.uwaterloo.ca> for the OpenSSL project.
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


/* Calaculates and sets the affine coordinates of an EC_POINT from the given
 * compressed coordinates.  Uses algorithm 2.3.4 of SEC 1. 
 * Note that the simple implementation only uses affine coordinates.
 *
 * This algorithm is patented by Certicom Corp. under US Patent 6,141,420
 * (for licensing information, contact licensing@certicom.com).
 * This function is disabled by default and can be enabled by defining the 
 * preprocessor macro OPENSSL_EC_BIN_PT_COMP at Configure-time.
 */
int ec_GF2m_simple_set_compressed_coordinates(const EC_GROUP *group, EC_POINT *point,
	const BIGNUM *x_, int y_bit, BN_CTX *ctx)
	{
#ifndef OPENSSL_EC_BIN_PT_COMP	
	ECerr(EC_F_EC_GF2M_SIMPLE_SET_COMPRESSED_COORDINATES, ERR_R_DISABLED);
	return 0;
#else
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
#endif
	}

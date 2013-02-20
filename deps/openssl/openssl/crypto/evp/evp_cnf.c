/* evp_cnf.c */
/* Written by Stephen Henson (steve@openssl.org) for the OpenSSL
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

#include <stdio.h>
#include <ctype.h>
#include <openssl/crypto.h>
#include "cryptlib.h"
#include <openssl/conf.h>
#include <openssl/dso.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#ifdef OPENSSL_FIPS
#include <openssl/fips.h>
#endif


/* Algorithm configuration module. */

static int alg_module_init(CONF_IMODULE *md, const CONF *cnf)
	{
	int i;
	const char *oid_section;
	STACK_OF(CONF_VALUE) *sktmp;
	CONF_VALUE *oval;
	oid_section = CONF_imodule_get_value(md);
	if(!(sktmp = NCONF_get_section(cnf, oid_section)))
		{
		EVPerr(EVP_F_ALG_MODULE_INIT, EVP_R_ERROR_LOADING_SECTION);
		return 0;
		}
	for(i = 0; i < sk_CONF_VALUE_num(sktmp); i++)
		{
		oval = sk_CONF_VALUE_value(sktmp, i);
		if (!strcmp(oval->name, "fips_mode"))
			{
			int m;
			if (!X509V3_get_value_bool(oval, &m))
				{
				EVPerr(EVP_F_ALG_MODULE_INIT, EVP_R_INVALID_FIPS_MODE);
				return 0;
				}
			if (m > 0)
				{
#ifdef OPENSSL_FIPS
				if (!FIPS_mode() && !FIPS_mode_set(1))
					{
					EVPerr(EVP_F_ALG_MODULE_INIT, EVP_R_ERROR_SETTING_FIPS_MODE);
					return 0;
					}
#else
				EVPerr(EVP_F_ALG_MODULE_INIT, EVP_R_FIPS_MODE_NOT_SUPPORTED);
				return 0;
#endif
				}
			}
		else
			{
			EVPerr(EVP_F_ALG_MODULE_INIT, EVP_R_UNKNOWN_OPTION);
			ERR_add_error_data(4, "name=", oval->name,
						", value=", oval->value);
			}
				
		}
	return 1;
	}

void EVP_add_alg_module(void)
	{
	CONF_module_add("alg_section", alg_module_init, 0);
	}

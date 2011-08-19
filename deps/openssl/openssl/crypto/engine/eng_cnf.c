/* eng_cnf.c */
/* Written by Stephen Henson (steve@openssl.org) for the OpenSSL
 * project 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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

#include "eng_int.h"
#include <openssl/conf.h>

/* #define ENGINE_CONF_DEBUG */

/* ENGINE config module */

static char *skip_dot(char *name)
	{
	char *p;
	p = strchr(name, '.');
	if (p)
		return p + 1;
	return name;
	}

static STACK_OF(ENGINE) *initialized_engines = NULL;

static int int_engine_init(ENGINE *e)
	{
	if (!ENGINE_init(e))
		return 0;
	if (!initialized_engines)
		initialized_engines = sk_ENGINE_new_null();
	if (!initialized_engines || !sk_ENGINE_push(initialized_engines, e))
		{
		ENGINE_finish(e);
		return 0;
		}
	return 1;
	}
	

static int int_engine_configure(char *name, char *value, const CONF *cnf)
	{
	int i;
	int ret = 0;
	long do_init = -1;
	STACK_OF(CONF_VALUE) *ecmds;
	CONF_VALUE *ecmd = NULL;
	char *ctrlname, *ctrlvalue;
	ENGINE *e = NULL;
	int soft = 0;

	name = skip_dot(name);
#ifdef ENGINE_CONF_DEBUG
	fprintf(stderr, "Configuring engine %s\n", name);
#endif
	/* Value is a section containing ENGINE commands */
	ecmds = NCONF_get_section(cnf, value);

	if (!ecmds)
		{
		ENGINEerr(ENGINE_F_INT_ENGINE_CONFIGURE, ENGINE_R_ENGINE_SECTION_ERROR);
		return 0;
		}

	for (i = 0; i < sk_CONF_VALUE_num(ecmds); i++)
		{
		ecmd = sk_CONF_VALUE_value(ecmds, i);
		ctrlname = skip_dot(ecmd->name);
		ctrlvalue = ecmd->value;
#ifdef ENGINE_CONF_DEBUG
	fprintf(stderr, "ENGINE conf: doing ctrl(%s,%s)\n", ctrlname, ctrlvalue);
#endif

		/* First handle some special pseudo ctrls */

		/* Override engine name to use */
		if (!strcmp(ctrlname, "engine_id"))
			name = ctrlvalue;
		else if (!strcmp(ctrlname, "soft_load"))
			soft = 1;
		/* Load a dynamic ENGINE */
		else if (!strcmp(ctrlname, "dynamic_path"))
			{
			e = ENGINE_by_id("dynamic");
			if (!e)
				goto err;
			if (!ENGINE_ctrl_cmd_string(e, "SO_PATH", ctrlvalue, 0))
				goto err;
			if (!ENGINE_ctrl_cmd_string(e, "LIST_ADD", "2", 0))
				goto err;
			if (!ENGINE_ctrl_cmd_string(e, "LOAD", NULL, 0))
				goto err;
			}
		/* ... add other pseudos here ... */
		else
			{
			/* At this point we need an ENGINE structural reference
			 * if we don't already have one.
			 */
			if (!e)
				{
				e = ENGINE_by_id(name);
				if (!e && soft)
					{
					ERR_clear_error();
					return 1;
					}
				if (!e)
					goto err;
				}
			/* Allow "EMPTY" to mean no value: this allows a valid
			 * "value" to be passed to ctrls of type NO_INPUT
		 	 */
			if (!strcmp(ctrlvalue, "EMPTY"))
				ctrlvalue = NULL;
			if (!strcmp(ctrlname, "init"))
				{
				if (!NCONF_get_number_e(cnf, value, "init", &do_init))
					goto err;
				if (do_init == 1)
					{
					if (!int_engine_init(e))
						goto err;
					}
				else if (do_init != 0)
					{
					ENGINEerr(ENGINE_F_INT_ENGINE_CONFIGURE, ENGINE_R_INVALID_INIT_VALUE);
					goto err;
					}
				}
			else if (!strcmp(ctrlname, "default_algorithms"))
				{
				if (!ENGINE_set_default_string(e, ctrlvalue))
					goto err;
				}
			else if (!ENGINE_ctrl_cmd_string(e,
					ctrlname, ctrlvalue, 0))
				goto err;
			}



		}
	if (e && (do_init == -1) && !int_engine_init(e))
		{
		ecmd = NULL;
		goto err;
		}
	ret = 1;
	err:
	if (ret != 1)
		{
		ENGINEerr(ENGINE_F_INT_ENGINE_CONFIGURE, ENGINE_R_ENGINE_CONFIGURATION_ERROR);
		if (ecmd)
			ERR_add_error_data(6, "section=", ecmd->section, 
						", name=", ecmd->name,
						", value=", ecmd->value);
		}
	if (e)
		ENGINE_free(e);
	return ret;
	}


static int int_engine_module_init(CONF_IMODULE *md, const CONF *cnf)
	{
	STACK_OF(CONF_VALUE) *elist;
	CONF_VALUE *cval;
	int i;
#ifdef ENGINE_CONF_DEBUG
	fprintf(stderr, "Called engine module: name %s, value %s\n",
			CONF_imodule_get_name(md), CONF_imodule_get_value(md));
#endif
	/* Value is a section containing ENGINEs to configure */
	elist = NCONF_get_section(cnf, CONF_imodule_get_value(md));

	if (!elist)
		{
		ENGINEerr(ENGINE_F_INT_ENGINE_MODULE_INIT, ENGINE_R_ENGINES_SECTION_ERROR);
		return 0;
		}

	for (i = 0; i < sk_CONF_VALUE_num(elist); i++)
		{
		cval = sk_CONF_VALUE_value(elist, i);
		if (!int_engine_configure(cval->name, cval->value, cnf))
			return 0;
		}

	return 1;
	}

static void int_engine_module_finish(CONF_IMODULE *md)
	{
	ENGINE *e;
	while ((e = sk_ENGINE_pop(initialized_engines)))
		ENGINE_finish(e);
	sk_ENGINE_free(initialized_engines);
	initialized_engines = NULL;
	}
	

void ENGINE_add_conf_module(void)
	{
	CONF_module_add("engines",
			int_engine_module_init,
			int_engine_module_finish);
	}

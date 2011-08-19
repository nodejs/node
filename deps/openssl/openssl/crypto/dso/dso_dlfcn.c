/* dso_dlfcn.c -*- mode:C; c-file-style: "eay" -*- */
/* Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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
#include "cryptlib.h"
#include <openssl/dso.h>

#ifndef DSO_DLFCN
DSO_METHOD *DSO_METHOD_dlfcn(void)
	{
	return NULL;
	}
#else

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

/* Part of the hack in "dlfcn_load" ... */
#define DSO_MAX_TRANSLATED_SIZE 256

static int dlfcn_load(DSO *dso);
static int dlfcn_unload(DSO *dso);
static void *dlfcn_bind_var(DSO *dso, const char *symname);
static DSO_FUNC_TYPE dlfcn_bind_func(DSO *dso, const char *symname);
#if 0
static int dlfcn_unbind(DSO *dso, char *symname, void *symptr);
static int dlfcn_init(DSO *dso);
static int dlfcn_finish(DSO *dso);
static long dlfcn_ctrl(DSO *dso, int cmd, long larg, void *parg);
#endif
static char *dlfcn_name_converter(DSO *dso, const char *filename);
static char *dlfcn_merger(DSO *dso, const char *filespec1,
	const char *filespec2);

static DSO_METHOD dso_meth_dlfcn = {
	"OpenSSL 'dlfcn' shared library method",
	dlfcn_load,
	dlfcn_unload,
	dlfcn_bind_var,
	dlfcn_bind_func,
/* For now, "unbind" doesn't exist */
#if 0
	NULL, /* unbind_var */
	NULL, /* unbind_func */
#endif
	NULL, /* ctrl */
	dlfcn_name_converter,
	dlfcn_merger,
	NULL, /* init */
	NULL  /* finish */
	};

DSO_METHOD *DSO_METHOD_dlfcn(void)
	{
	return(&dso_meth_dlfcn);
	}

/* Prior to using the dlopen() function, we should decide on the flag
 * we send. There's a few different ways of doing this and it's a
 * messy venn-diagram to match up which platforms support what. So
 * as we don't have autoconf yet, I'm implementing a hack that could
 * be hacked further relatively easily to deal with cases as we find
 * them. Initially this is to cope with OpenBSD. */
#if defined(__OpenBSD__) || defined(__NetBSD__)
#	ifdef DL_LAZY
#		define DLOPEN_FLAG DL_LAZY
#	else
#		ifdef RTLD_NOW
#			define DLOPEN_FLAG RTLD_NOW
#		else
#			define DLOPEN_FLAG 0
#		endif
#	endif
#else
#	ifdef OPENSSL_SYS_SUNOS
#		define DLOPEN_FLAG 1
#	else
#		define DLOPEN_FLAG RTLD_NOW /* Hope this works everywhere else */
#	endif
#endif

/* For this DSO_METHOD, our meth_data STACK will contain;
 * (i) the handle (void*) returned from dlopen().
 */

static int dlfcn_load(DSO *dso)
	{
	void *ptr = NULL;
	/* See applicable comments in dso_dl.c */
	char *filename = DSO_convert_filename(dso, NULL);
	int flags = DLOPEN_FLAG;

	if(filename == NULL)
		{
		DSOerr(DSO_F_DLFCN_LOAD,DSO_R_NO_FILENAME);
		goto err;
		}

#ifdef RTLD_GLOBAL
	if (dso->flags & DSO_FLAG_GLOBAL_SYMBOLS)
		flags |= RTLD_GLOBAL;
#endif
	ptr = dlopen(filename, flags);
	if(ptr == NULL)
		{
		DSOerr(DSO_F_DLFCN_LOAD,DSO_R_LOAD_FAILED);
		ERR_add_error_data(4, "filename(", filename, "): ", dlerror());
		goto err;
		}
	if(!sk_push(dso->meth_data, (char *)ptr))
		{
		DSOerr(DSO_F_DLFCN_LOAD,DSO_R_STACK_ERROR);
		goto err;
		}
	/* Success */
	dso->loaded_filename = filename;
	return(1);
err:
	/* Cleanup! */
	if(filename != NULL)
		OPENSSL_free(filename);
	if(ptr != NULL)
		dlclose(ptr);
	return(0);
}

static int dlfcn_unload(DSO *dso)
	{
	void *ptr;
	if(dso == NULL)
		{
		DSOerr(DSO_F_DLFCN_UNLOAD,ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if(sk_num(dso->meth_data) < 1)
		return(1);
	ptr = (void *)sk_pop(dso->meth_data);
	if(ptr == NULL)
		{
		DSOerr(DSO_F_DLFCN_UNLOAD,DSO_R_NULL_HANDLE);
		/* Should push the value back onto the stack in
		 * case of a retry. */
		sk_push(dso->meth_data, (char *)ptr);
		return(0);
		}
	/* For now I'm not aware of any errors associated with dlclose() */
	dlclose(ptr);
	return(1);
	}

static void *dlfcn_bind_var(DSO *dso, const char *symname)
	{
	void *ptr, *sym;

	if((dso == NULL) || (symname == NULL))
		{
		DSOerr(DSO_F_DLFCN_BIND_VAR,ERR_R_PASSED_NULL_PARAMETER);
		return(NULL);
		}
	if(sk_num(dso->meth_data) < 1)
		{
		DSOerr(DSO_F_DLFCN_BIND_VAR,DSO_R_STACK_ERROR);
		return(NULL);
		}
	ptr = (void *)sk_value(dso->meth_data, sk_num(dso->meth_data) - 1);
	if(ptr == NULL)
		{
		DSOerr(DSO_F_DLFCN_BIND_VAR,DSO_R_NULL_HANDLE);
		return(NULL);
		}
	sym = dlsym(ptr, symname);
	if(sym == NULL)
		{
		DSOerr(DSO_F_DLFCN_BIND_VAR,DSO_R_SYM_FAILURE);
		ERR_add_error_data(4, "symname(", symname, "): ", dlerror());
		return(NULL);
		}
	return(sym);
	}

static DSO_FUNC_TYPE dlfcn_bind_func(DSO *dso, const char *symname)
	{
	void *ptr;
	union {
		DSO_FUNC_TYPE sym;
		void *dlret;
	} u;

	if((dso == NULL) || (symname == NULL))
		{
		DSOerr(DSO_F_DLFCN_BIND_FUNC,ERR_R_PASSED_NULL_PARAMETER);
		return(NULL);
		}
	if(sk_num(dso->meth_data) < 1)
		{
		DSOerr(DSO_F_DLFCN_BIND_FUNC,DSO_R_STACK_ERROR);
		return(NULL);
		}
	ptr = (void *)sk_value(dso->meth_data, sk_num(dso->meth_data) - 1);
	if(ptr == NULL)
		{
		DSOerr(DSO_F_DLFCN_BIND_FUNC,DSO_R_NULL_HANDLE);
		return(NULL);
		}
	u.dlret = dlsym(ptr, symname);
	if(u.dlret == NULL)
		{
		DSOerr(DSO_F_DLFCN_BIND_FUNC,DSO_R_SYM_FAILURE);
		ERR_add_error_data(4, "symname(", symname, "): ", dlerror());
		return(NULL);
		}
	return u.sym;
	}

static char *dlfcn_merger(DSO *dso, const char *filespec1,
	const char *filespec2)
	{
	char *merged;

	if(!filespec1 && !filespec2)
		{
		DSOerr(DSO_F_DLFCN_MERGER,
				ERR_R_PASSED_NULL_PARAMETER);
		return(NULL);
		}
	/* If the first file specification is a rooted path, it rules.
	   same goes if the second file specification is missing. */
	if (!filespec2 || filespec1[0] == '/')
		{
		merged = OPENSSL_malloc(strlen(filespec1) + 1);
		if(!merged)
			{
			DSOerr(DSO_F_DLFCN_MERGER,
				ERR_R_MALLOC_FAILURE);
			return(NULL);
			}
		strcpy(merged, filespec1);
		}
	/* If the first file specification is missing, the second one rules. */
	else if (!filespec1)
		{
		merged = OPENSSL_malloc(strlen(filespec2) + 1);
		if(!merged)
			{
			DSOerr(DSO_F_DLFCN_MERGER,
				ERR_R_MALLOC_FAILURE);
			return(NULL);
			}
		strcpy(merged, filespec2);
		}
	else
		/* This part isn't as trivial as it looks.  It assumes that
		   the second file specification really is a directory, and
		   makes no checks whatsoever.  Therefore, the result becomes
		   the concatenation of filespec2 followed by a slash followed
		   by filespec1. */
		{
		int spec2len, len;

		spec2len = (filespec2 ? strlen(filespec2) : 0);
		len = spec2len + (filespec1 ? strlen(filespec1) : 0);

		if(filespec2 && filespec2[spec2len - 1] == '/')
			{
			spec2len--;
			len--;
			}
		merged = OPENSSL_malloc(len + 2);
		if(!merged)
			{
			DSOerr(DSO_F_DLFCN_MERGER,
				ERR_R_MALLOC_FAILURE);
			return(NULL);
			}
		strcpy(merged, filespec2);
		merged[spec2len] = '/';
		strcpy(&merged[spec2len + 1], filespec1);
		}
	return(merged);
	}

#ifdef OPENSSL_SYS_MACOSX
#define DSO_ext	".dylib"
#define DSO_extlen 6
#else
#define DSO_ext	".so"
#define DSO_extlen 3
#endif


static char *dlfcn_name_converter(DSO *dso, const char *filename)
	{
	char *translated;
	int len, rsize, transform;

	len = strlen(filename);
	rsize = len + 1;
	transform = (strstr(filename, "/") == NULL);
	if(transform)
		{
		/* We will convert this to "%s.so" or "lib%s.so" etc */
		rsize += DSO_extlen;	/* The length of ".so" */
		if ((DSO_flags(dso) & DSO_FLAG_NAME_TRANSLATION_EXT_ONLY) == 0)
			rsize += 3; /* The length of "lib" */
		}
	translated = OPENSSL_malloc(rsize);
	if(translated == NULL)
		{
		DSOerr(DSO_F_DLFCN_NAME_CONVERTER,
				DSO_R_NAME_TRANSLATION_FAILED);
		return(NULL);
		}
	if(transform)
		{
		if ((DSO_flags(dso) & DSO_FLAG_NAME_TRANSLATION_EXT_ONLY) == 0)
			sprintf(translated, "lib%s" DSO_ext, filename);
		else
			sprintf(translated, "%s" DSO_ext, filename);
		}
	else
		sprintf(translated, "%s", filename);
	return(translated);
	}

#endif /* DSO_DLFCN */

/* crypto/err/err.c */
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
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
#include <stdarg.h>
#include <string.h>
#include "cryptlib.h"
#include <openssl/lhash.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/err.h>

static unsigned long get_error_values(int inc,int top,
					const char **file,int *line,
					const char **data,int *flags);

#define err_clear_data(p,i) \
	do { \
	if (((p)->err_data[i] != NULL) && \
		(p)->err_data_flags[i] & ERR_TXT_MALLOCED) \
		{  \
		OPENSSL_free((p)->err_data[i]); \
		(p)->err_data[i]=NULL; \
		} \
	(p)->err_data_flags[i]=0; \
	} while(0)

#define err_clear(p,i) \
	do { \
	(p)->err_flags[i]=0; \
	(p)->err_buffer[i]=0; \
	err_clear_data(p,i); \
	(p)->err_file[i]=NULL; \
	(p)->err_line[i]= -1; \
	} while(0)

void ERR_put_error(int lib, int func, int reason, const char *file,
	     int line)
	{
	ERR_STATE *es;

#ifdef _OSD_POSIX
	/* In the BS2000-OSD POSIX subsystem, the compiler generates
	 * path names in the form "*POSIX(/etc/passwd)".
	 * This dirty hack strips them to something sensible.
	 * @@@ We shouldn't modify a const string, though.
	 */
	if (strncmp(file,"*POSIX(", sizeof("*POSIX(")-1) == 0) {
		char *end;

		/* Skip the "*POSIX(" prefix */
		file += sizeof("*POSIX(")-1;
		end = &file[strlen(file)-1];
		if (*end == ')')
			*end = '\0';
		/* Optional: use the basename of the path only. */
		if ((end = strrchr(file, '/')) != NULL)
			file = &end[1];
	}
#endif
	es=ERR_get_state();

	es->top=(es->top+1)%ERR_NUM_ERRORS;
	if (es->top == es->bottom)
		es->bottom=(es->bottom+1)%ERR_NUM_ERRORS;
	es->err_flags[es->top]=0;
	es->err_buffer[es->top]=ERR_PACK(lib,func,reason);
	es->err_file[es->top]=file;
	es->err_line[es->top]=line;
	err_clear_data(es,es->top);
	}

void ERR_clear_error(void)
	{
	int i;
	ERR_STATE *es;

	es=ERR_get_state();

	for (i=0; i<ERR_NUM_ERRORS; i++)
		{
		err_clear(es,i);
		}
	es->top=es->bottom=0;
	}


unsigned long ERR_get_error(void)
	{ return(get_error_values(1,0,NULL,NULL,NULL,NULL)); }

unsigned long ERR_get_error_line(const char **file,
	     int *line)
	{ return(get_error_values(1,0,file,line,NULL,NULL)); }

unsigned long ERR_get_error_line_data(const char **file, int *line,
	     const char **data, int *flags)
	{ return(get_error_values(1,0,file,line,data,flags)); }


unsigned long ERR_peek_error(void)
	{ return(get_error_values(0,0,NULL,NULL,NULL,NULL)); }

unsigned long ERR_peek_error_line(const char **file, int *line)
	{ return(get_error_values(0,0,file,line,NULL,NULL)); }

unsigned long ERR_peek_error_line_data(const char **file, int *line,
	     const char **data, int *flags)
	{ return(get_error_values(0,0,file,line,data,flags)); }


unsigned long ERR_peek_last_error(void)
	{ return(get_error_values(0,1,NULL,NULL,NULL,NULL)); }

unsigned long ERR_peek_last_error_line(const char **file, int *line)
	{ return(get_error_values(0,1,file,line,NULL,NULL)); }

unsigned long ERR_peek_last_error_line_data(const char **file, int *line,
	     const char **data, int *flags)
	{ return(get_error_values(0,1,file,line,data,flags)); }


static unsigned long get_error_values(int inc, int top, const char **file, int *line,
	     const char **data, int *flags)
	{	
	int i=0;
	ERR_STATE *es;
	unsigned long ret;

	es=ERR_get_state();

	if (inc && top)
		{
		if (file) *file = "";
		if (line) *line = 0;
		if (data) *data = "";
		if (flags) *flags = 0;
			
		return ERR_R_INTERNAL_ERROR;
		}

	if (es->bottom == es->top) return 0;
	if (top)
		i=es->top;			 /* last error */
	else
		i=(es->bottom+1)%ERR_NUM_ERRORS; /* first error */

	ret=es->err_buffer[i];
	if (inc)
		{
		es->bottom=i;
		es->err_buffer[i]=0;
		}

	if ((file != NULL) && (line != NULL))
		{
		if (es->err_file[i] == NULL)
			{
			*file="NA";
			if (line != NULL) *line=0;
			}
		else
			{
			*file=es->err_file[i];
			if (line != NULL) *line=es->err_line[i];
			}
		}

	if (data == NULL)
		{
		if (inc)
			{
			err_clear_data(es, i);
			}
		}
	else
		{
		if (es->err_data[i] == NULL)
			{
			*data="";
			if (flags != NULL) *flags=0;
			}
		else
			{
			*data=es->err_data[i];
			if (flags != NULL) *flags=es->err_data_flags[i];
			}
		}
	return ret;
	}

void ERR_set_error_data(char *data, int flags)
	{
	ERR_STATE *es;
	int i;

	es=ERR_get_state();

	i=es->top;
	if (i == 0)
		i=ERR_NUM_ERRORS-1;

	err_clear_data(es,i);
	es->err_data[i]=data;
	es->err_data_flags[i]=flags;
	}

void ERR_add_error_data(int num, ...)
	{
	va_list args;
	int i,n,s;
	char *str,*p,*a;

	s=80;
	str=OPENSSL_malloc(s+1);
	if (str == NULL) return;
	str[0]='\0';

	va_start(args, num);
	n=0;
	for (i=0; i<num; i++)
		{
		a=va_arg(args, char*);
		/* ignore NULLs, thanks to Bob Beck <beck@obtuse.com> */
		if (a != NULL)
			{
			n+=strlen(a);
			if (n > s)
				{
				s=n+20;
				p=OPENSSL_realloc(str,s+1);
				if (p == NULL)
					{
					OPENSSL_free(str);
					goto err;
					}
				else
					str=p;
				}
			BUF_strlcat(str,a,(size_t)s+1);
			}
		}
	ERR_set_error_data(str,ERR_TXT_MALLOCED|ERR_TXT_STRING);

err:
	va_end(args);
	}

int ERR_set_mark(void)
	{
	ERR_STATE *es;

	es=ERR_get_state();

	if (es->bottom == es->top) return 0;
	es->err_flags[es->top]|=ERR_FLAG_MARK;
	return 1;
	}

int ERR_pop_to_mark(void)
	{
	ERR_STATE *es;

	es=ERR_get_state();

	while(es->bottom != es->top
		&& (es->err_flags[es->top] & ERR_FLAG_MARK) == 0)
		{
		err_clear(es,es->top);
		es->top-=1;
		if (es->top == -1) es->top=ERR_NUM_ERRORS-1;
		}
		
	if (es->bottom == es->top) return 0;
	es->err_flags[es->top]&=~ERR_FLAG_MARK;
	return 1;
	}

#ifdef OPENSSL_FIPS

static ERR_STATE *fget_state(void)
	{
	static ERR_STATE fstate;
	return &fstate;
	}

ERR_STATE *(*get_state_func)(void) = fget_state;
void (*remove_state_func)(unsigned long pid);

ERR_STATE *ERR_get_state(void)
	{
	return get_state_func();
	}

void int_ERR_set_state_func(ERR_STATE *(*get_func)(void),
				void (*remove_func)(unsigned long pid))
	{
	get_state_func = get_func;
	remove_state_func = remove_func;
	}

void ERR_remove_state(unsigned long pid)
	{
	if (remove_state_func)
		remove_state_func(pid);
	}

#endif

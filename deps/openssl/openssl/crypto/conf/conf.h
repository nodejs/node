/* crypto/conf/conf.h */
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

#ifndef  HEADER_CONF_H
#define HEADER_CONF_H

#include <openssl/bio.h>
#include <openssl/lhash.h>
#include <openssl/stack.h>
#include <openssl/safestack.h>
#include <openssl/e_os2.h>

#include <openssl/ossl_typ.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct
	{
	char *section;
	char *name;
	char *value;
	} CONF_VALUE;

DECLARE_STACK_OF(CONF_VALUE)
DECLARE_STACK_OF(CONF_MODULE)
DECLARE_STACK_OF(CONF_IMODULE)

struct conf_st;
struct conf_method_st;
typedef struct conf_method_st CONF_METHOD;

struct conf_method_st
	{
	const char *name;
	CONF *(*create)(CONF_METHOD *meth);
	int (*init)(CONF *conf);
	int (*destroy)(CONF *conf);
	int (*destroy_data)(CONF *conf);
	int (*load_bio)(CONF *conf, BIO *bp, long *eline);
	int (*dump)(const CONF *conf, BIO *bp);
	int (*is_number)(const CONF *conf, char c);
	int (*to_int)(const CONF *conf, char c);
	int (*load)(CONF *conf, const char *name, long *eline);
	};

/* Module definitions */

typedef struct conf_imodule_st CONF_IMODULE;
typedef struct conf_module_st CONF_MODULE;

/* DSO module function typedefs */
typedef int conf_init_func(CONF_IMODULE *md, const CONF *cnf);
typedef void conf_finish_func(CONF_IMODULE *md);

#define	CONF_MFLAGS_IGNORE_ERRORS	0x1
#define CONF_MFLAGS_IGNORE_RETURN_CODES	0x2
#define CONF_MFLAGS_SILENT		0x4
#define CONF_MFLAGS_NO_DSO		0x8
#define CONF_MFLAGS_IGNORE_MISSING_FILE	0x10
#define CONF_MFLAGS_DEFAULT_SECTION	0x20

int CONF_set_default_method(CONF_METHOD *meth);
void CONF_set_nconf(CONF *conf,LHASH *hash);
LHASH *CONF_load(LHASH *conf,const char *file,long *eline);
#ifndef OPENSSL_NO_FP_API
LHASH *CONF_load_fp(LHASH *conf, FILE *fp,long *eline);
#endif
LHASH *CONF_load_bio(LHASH *conf, BIO *bp,long *eline);
STACK_OF(CONF_VALUE) *CONF_get_section(LHASH *conf,const char *section);
char *CONF_get_string(LHASH *conf,const char *group,const char *name);
long CONF_get_number(LHASH *conf,const char *group,const char *name);
void CONF_free(LHASH *conf);
int CONF_dump_fp(LHASH *conf, FILE *out);
int CONF_dump_bio(LHASH *conf, BIO *out);

void OPENSSL_config(const char *config_name);
void OPENSSL_no_config(void);

/* New conf code.  The semantics are different from the functions above.
   If that wasn't the case, the above functions would have been replaced */

struct conf_st
	{
	CONF_METHOD *meth;
	void *meth_data;
	LHASH *data;
	};

CONF *NCONF_new(CONF_METHOD *meth);
CONF_METHOD *NCONF_default(void);
CONF_METHOD *NCONF_WIN32(void);
#if 0 /* Just to give you an idea of what I have in mind */
CONF_METHOD *NCONF_XML(void);
#endif
void NCONF_free(CONF *conf);
void NCONF_free_data(CONF *conf);

int NCONF_load(CONF *conf,const char *file,long *eline);
#ifndef OPENSSL_NO_FP_API
int NCONF_load_fp(CONF *conf, FILE *fp,long *eline);
#endif
int NCONF_load_bio(CONF *conf, BIO *bp,long *eline);
STACK_OF(CONF_VALUE) *NCONF_get_section(const CONF *conf,const char *section);
char *NCONF_get_string(const CONF *conf,const char *group,const char *name);
int NCONF_get_number_e(const CONF *conf,const char *group,const char *name,
		       long *result);
int NCONF_dump_fp(const CONF *conf, FILE *out);
int NCONF_dump_bio(const CONF *conf, BIO *out);

#if 0 /* The following function has no error checking,
	 and should therefore be avoided */
long NCONF_get_number(CONF *conf,char *group,char *name);
#else
#define NCONF_get_number(c,g,n,r) NCONF_get_number_e(c,g,n,r)
#endif
  
/* Module functions */

int CONF_modules_load(const CONF *cnf, const char *appname,
		      unsigned long flags);
int CONF_modules_load_file(const char *filename, const char *appname,
			   unsigned long flags);
void CONF_modules_unload(int all);
void CONF_modules_finish(void);
void CONF_modules_free(void);
int CONF_module_add(const char *name, conf_init_func *ifunc,
		    conf_finish_func *ffunc);

const char *CONF_imodule_get_name(const CONF_IMODULE *md);
const char *CONF_imodule_get_value(const CONF_IMODULE *md);
void *CONF_imodule_get_usr_data(const CONF_IMODULE *md);
void CONF_imodule_set_usr_data(CONF_IMODULE *md, void *usr_data);
CONF_MODULE *CONF_imodule_get_module(const CONF_IMODULE *md);
unsigned long CONF_imodule_get_flags(const CONF_IMODULE *md);
void CONF_imodule_set_flags(CONF_IMODULE *md, unsigned long flags);
void *CONF_module_get_usr_data(CONF_MODULE *pmod);
void CONF_module_set_usr_data(CONF_MODULE *pmod, void *usr_data);

char *CONF_get1_default_config_file(void);

int CONF_parse_list(const char *list, int sep, int nospc,
	int (*list_cb)(const char *elem, int len, void *usr), void *arg);

void OPENSSL_load_builtin_modules(void);

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_CONF_strings(void);

/* Error codes for the CONF functions. */

/* Function codes. */
#define CONF_F_CONF_DUMP_FP				 104
#define CONF_F_CONF_LOAD				 100
#define CONF_F_CONF_LOAD_BIO				 102
#define CONF_F_CONF_LOAD_FP				 103
#define CONF_F_CONF_MODULES_LOAD			 116
#define CONF_F_DEF_LOAD					 120
#define CONF_F_DEF_LOAD_BIO				 121
#define CONF_F_MODULE_INIT				 115
#define CONF_F_MODULE_LOAD_DSO				 117
#define CONF_F_MODULE_RUN				 118
#define CONF_F_NCONF_DUMP_BIO				 105
#define CONF_F_NCONF_DUMP_FP				 106
#define CONF_F_NCONF_GET_NUMBER				 107
#define CONF_F_NCONF_GET_NUMBER_E			 112
#define CONF_F_NCONF_GET_SECTION			 108
#define CONF_F_NCONF_GET_STRING				 109
#define CONF_F_NCONF_LOAD				 113
#define CONF_F_NCONF_LOAD_BIO				 110
#define CONF_F_NCONF_LOAD_FP				 114
#define CONF_F_NCONF_NEW				 111
#define CONF_F_STR_COPY					 101

/* Reason codes. */
#define CONF_R_ERROR_LOADING_DSO			 110
#define CONF_R_MISSING_CLOSE_SQUARE_BRACKET		 100
#define CONF_R_MISSING_EQUAL_SIGN			 101
#define CONF_R_MISSING_FINISH_FUNCTION			 111
#define CONF_R_MISSING_INIT_FUNCTION			 112
#define CONF_R_MODULE_INITIALIZATION_ERROR		 109
#define CONF_R_NO_CLOSE_BRACE				 102
#define CONF_R_NO_CONF					 105
#define CONF_R_NO_CONF_OR_ENVIRONMENT_VARIABLE		 106
#define CONF_R_NO_SECTION				 107
#define CONF_R_NO_SUCH_FILE				 114
#define CONF_R_NO_VALUE					 108
#define CONF_R_UNABLE_TO_CREATE_NEW_SECTION		 103
#define CONF_R_UNKNOWN_MODULE_NAME			 113
#define CONF_R_VARIABLE_HAS_NO_VALUE			 104

#ifdef  __cplusplus
}
#endif
#endif

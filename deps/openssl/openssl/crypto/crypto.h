/* crypto/crypto.h */
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
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECDH support in OpenSSL originally developed by 
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#ifndef HEADER_CRYPTO_H
#define HEADER_CRYPTO_H

#include <stdlib.h>

#include <openssl/e_os2.h>

#ifndef OPENSSL_NO_FP_API
#include <stdio.h>
#endif

#include <openssl/stack.h>
#include <openssl/safestack.h>
#include <openssl/opensslv.h>
#include <openssl/ossl_typ.h>

#ifdef CHARSET_EBCDIC
#include <openssl/ebcdic.h>
#endif

/* Resolve problems on some operating systems with symbol names that clash
   one way or another */
#include <openssl/symhacks.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Backward compatibility to SSLeay */
/* This is more to be used to check the correct DLL is being used
 * in the MS world. */
#define SSLEAY_VERSION_NUMBER	OPENSSL_VERSION_NUMBER
#define SSLEAY_VERSION		0
/* #define SSLEAY_OPTIONS	1 no longer supported */
#define SSLEAY_CFLAGS		2
#define SSLEAY_BUILT_ON		3
#define SSLEAY_PLATFORM		4
#define SSLEAY_DIR		5

/* Already declared in ossl_typ.h */
#if 0
typedef struct crypto_ex_data_st CRYPTO_EX_DATA;
/* Called when a new object is created */
typedef int CRYPTO_EX_new(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
					int idx, long argl, void *argp);
/* Called when an object is free()ed */
typedef void CRYPTO_EX_free(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
					int idx, long argl, void *argp);
/* Called when we need to dup an object */
typedef int CRYPTO_EX_dup(CRYPTO_EX_DATA *to, CRYPTO_EX_DATA *from, void *from_d, 
					int idx, long argl, void *argp);
#endif

/* A generic structure to pass assorted data in a expandable way */
typedef struct openssl_item_st
	{
	int code;
	void *value;		/* Not used for flag attributes */
	size_t value_size;	/* Max size of value for output, length for input */
	size_t *value_length;	/* Returned length of value for output */
	} OPENSSL_ITEM;


/* When changing the CRYPTO_LOCK_* list, be sure to maintin the text lock
 * names in cryptlib.c
 */

#define	CRYPTO_LOCK_ERR			1
#define	CRYPTO_LOCK_EX_DATA		2
#define	CRYPTO_LOCK_X509		3
#define	CRYPTO_LOCK_X509_INFO		4
#define	CRYPTO_LOCK_X509_PKEY		5
#define CRYPTO_LOCK_X509_CRL		6
#define CRYPTO_LOCK_X509_REQ		7
#define CRYPTO_LOCK_DSA			8
#define CRYPTO_LOCK_RSA			9
#define CRYPTO_LOCK_EVP_PKEY		10
#define CRYPTO_LOCK_X509_STORE		11
#define CRYPTO_LOCK_SSL_CTX		12
#define CRYPTO_LOCK_SSL_CERT		13
#define CRYPTO_LOCK_SSL_SESSION		14
#define CRYPTO_LOCK_SSL_SESS_CERT	15
#define CRYPTO_LOCK_SSL			16
#define CRYPTO_LOCK_SSL_METHOD		17
#define CRYPTO_LOCK_RAND		18
#define CRYPTO_LOCK_RAND2		19
#define CRYPTO_LOCK_MALLOC		20
#define CRYPTO_LOCK_BIO			21
#define CRYPTO_LOCK_GETHOSTBYNAME	22
#define CRYPTO_LOCK_GETSERVBYNAME	23
#define CRYPTO_LOCK_READDIR		24
#define CRYPTO_LOCK_RSA_BLINDING	25
#define CRYPTO_LOCK_DH			26
#define CRYPTO_LOCK_MALLOC2		27
#define CRYPTO_LOCK_DSO			28
#define CRYPTO_LOCK_DYNLOCK		29
#define CRYPTO_LOCK_ENGINE		30
#define CRYPTO_LOCK_UI			31
#define CRYPTO_LOCK_ECDSA               32
#define CRYPTO_LOCK_EC			33
#define CRYPTO_LOCK_ECDH		34
#define CRYPTO_LOCK_BN  		35
#define CRYPTO_LOCK_EC_PRE_COMP		36
#define CRYPTO_LOCK_STORE		37
#define CRYPTO_LOCK_COMP		38
#ifndef OPENSSL_FIPS
#define CRYPTO_NUM_LOCKS		39
#else
#define CRYPTO_LOCK_FIPS		39
#define CRYPTO_LOCK_FIPS2		40
#define CRYPTO_NUM_LOCKS		41
#endif

#define CRYPTO_LOCK		1
#define CRYPTO_UNLOCK		2
#define CRYPTO_READ		4
#define CRYPTO_WRITE		8

#ifndef OPENSSL_NO_LOCKING
#ifndef CRYPTO_w_lock
#define CRYPTO_w_lock(type)	\
	CRYPTO_lock(CRYPTO_LOCK|CRYPTO_WRITE,type,__FILE__,__LINE__)
#define CRYPTO_w_unlock(type)	\
	CRYPTO_lock(CRYPTO_UNLOCK|CRYPTO_WRITE,type,__FILE__,__LINE__)
#define CRYPTO_r_lock(type)	\
	CRYPTO_lock(CRYPTO_LOCK|CRYPTO_READ,type,__FILE__,__LINE__)
#define CRYPTO_r_unlock(type)	\
	CRYPTO_lock(CRYPTO_UNLOCK|CRYPTO_READ,type,__FILE__,__LINE__)
#define CRYPTO_add(addr,amount,type)	\
	CRYPTO_add_lock(addr,amount,type,__FILE__,__LINE__)
#endif
#else
#define CRYPTO_w_lock(a)
#define CRYPTO_w_unlock(a)
#define CRYPTO_r_lock(a)
#define CRYPTO_r_unlock(a)
#define CRYPTO_add(a,b,c)	((*(a))+=(b))
#endif

/* Some applications as well as some parts of OpenSSL need to allocate
   and deallocate locks in a dynamic fashion.  The following typedef
   makes this possible in a type-safe manner.  */
/* struct CRYPTO_dynlock_value has to be defined by the application. */
typedef struct
	{
	int references;
	struct CRYPTO_dynlock_value *data;
	} CRYPTO_dynlock;


/* The following can be used to detect memory leaks in the SSLeay library.
 * It used, it turns on malloc checking */

#define CRYPTO_MEM_CHECK_OFF	0x0	/* an enume */
#define CRYPTO_MEM_CHECK_ON	0x1	/* a bit */
#define CRYPTO_MEM_CHECK_ENABLE	0x2	/* a bit */
#define CRYPTO_MEM_CHECK_DISABLE 0x3	/* an enume */

/* The following are bit values to turn on or off options connected to the
 * malloc checking functionality */

/* Adds time to the memory checking information */
#define V_CRYPTO_MDEBUG_TIME	0x1 /* a bit */
/* Adds thread number to the memory checking information */
#define V_CRYPTO_MDEBUG_THREAD	0x2 /* a bit */

#define V_CRYPTO_MDEBUG_ALL (V_CRYPTO_MDEBUG_TIME | V_CRYPTO_MDEBUG_THREAD)


/* predec of the BIO type */
typedef struct bio_st BIO_dummy;

struct crypto_ex_data_st
	{
	STACK *sk;
	int dummy; /* gcc is screwing up this data structure :-( */
	};

/* This stuff is basically class callback functions
 * The current classes are SSL_CTX, SSL, SSL_SESSION, and a few more */

typedef struct crypto_ex_data_func_st
	{
	long argl;	/* Arbitary long */
	void *argp;	/* Arbitary void * */
	CRYPTO_EX_new *new_func;
	CRYPTO_EX_free *free_func;
	CRYPTO_EX_dup *dup_func;
	} CRYPTO_EX_DATA_FUNCS;

DECLARE_STACK_OF(CRYPTO_EX_DATA_FUNCS)

/* Per class, we have a STACK of CRYPTO_EX_DATA_FUNCS for each CRYPTO_EX_DATA
 * entry.
 */

#define CRYPTO_EX_INDEX_BIO		0
#define CRYPTO_EX_INDEX_SSL		1
#define CRYPTO_EX_INDEX_SSL_CTX		2
#define CRYPTO_EX_INDEX_SSL_SESSION	3
#define CRYPTO_EX_INDEX_X509_STORE	4
#define CRYPTO_EX_INDEX_X509_STORE_CTX	5
#define CRYPTO_EX_INDEX_RSA		6
#define CRYPTO_EX_INDEX_DSA		7
#define CRYPTO_EX_INDEX_DH		8
#define CRYPTO_EX_INDEX_ENGINE		9
#define CRYPTO_EX_INDEX_X509		10
#define CRYPTO_EX_INDEX_UI		11
#define CRYPTO_EX_INDEX_ECDSA		12
#define CRYPTO_EX_INDEX_ECDH		13
#define CRYPTO_EX_INDEX_COMP		14
#define CRYPTO_EX_INDEX_STORE		15

/* Dynamically assigned indexes start from this value (don't use directly, use
 * via CRYPTO_ex_data_new_class). */
#define CRYPTO_EX_INDEX_USER		100


/* This is the default callbacks, but we can have others as well:
 * this is needed in Win32 where the application malloc and the
 * library malloc may not be the same.
 */
#define CRYPTO_malloc_init()	CRYPTO_set_mem_functions(\
	malloc, realloc, free)

#if defined CRYPTO_MDEBUG_ALL || defined CRYPTO_MDEBUG_TIME || defined CRYPTO_MDEBUG_THREAD
# ifndef CRYPTO_MDEBUG /* avoid duplicate #define */
#  define CRYPTO_MDEBUG
# endif
#endif

/* Set standard debugging functions (not done by default
 * unless CRYPTO_MDEBUG is defined) */
void CRYPTO_malloc_debug_init(void);

int CRYPTO_mem_ctrl(int mode);
int CRYPTO_is_mem_check_on(void);

/* for applications */
#define MemCheck_start() CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON)
#define MemCheck_stop()	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_OFF)

/* for library-internal use */
#define MemCheck_on()	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ENABLE)
#define MemCheck_off()	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_DISABLE)
#define is_MemCheck_on() CRYPTO_is_mem_check_on()

#define OPENSSL_malloc(num)	CRYPTO_malloc((int)num,__FILE__,__LINE__)
#define OPENSSL_strdup(str)	CRYPTO_strdup((str),__FILE__,__LINE__)
#define OPENSSL_realloc(addr,num) \
	CRYPTO_realloc((char *)addr,(int)num,__FILE__,__LINE__)
#define OPENSSL_realloc_clean(addr,old_num,num) \
	CRYPTO_realloc_clean(addr,old_num,num,__FILE__,__LINE__)
#define OPENSSL_remalloc(addr,num) \
	CRYPTO_remalloc((char **)addr,(int)num,__FILE__,__LINE__)
#define OPENSSL_freeFunc	CRYPTO_free
#define OPENSSL_free(addr)	CRYPTO_free(addr)

#define OPENSSL_malloc_locked(num) \
	CRYPTO_malloc_locked((int)num,__FILE__,__LINE__)
#define OPENSSL_free_locked(addr) CRYPTO_free_locked(addr)


const char *SSLeay_version(int type);
unsigned long SSLeay(void);

int OPENSSL_issetugid(void);

/* An opaque type representing an implementation of "ex_data" support */
typedef struct st_CRYPTO_EX_DATA_IMPL	CRYPTO_EX_DATA_IMPL;
/* Return an opaque pointer to the current "ex_data" implementation */
const CRYPTO_EX_DATA_IMPL *CRYPTO_get_ex_data_implementation(void);
/* Sets the "ex_data" implementation to be used (if it's not too late) */
int CRYPTO_set_ex_data_implementation(const CRYPTO_EX_DATA_IMPL *i);
/* Get a new "ex_data" class, and return the corresponding "class_index" */
int CRYPTO_ex_data_new_class(void);
/* Within a given class, get/register a new index */
int CRYPTO_get_ex_new_index(int class_index, long argl, void *argp,
		CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func,
		CRYPTO_EX_free *free_func);
/* Initialise/duplicate/free CRYPTO_EX_DATA variables corresponding to a given
 * class (invokes whatever per-class callbacks are applicable) */
int CRYPTO_new_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad);
int CRYPTO_dup_ex_data(int class_index, CRYPTO_EX_DATA *to,
		CRYPTO_EX_DATA *from);
void CRYPTO_free_ex_data(int class_index, void *obj, CRYPTO_EX_DATA *ad);
/* Get/set data in a CRYPTO_EX_DATA variable corresponding to a particular index
 * (relative to the class type involved) */
int CRYPTO_set_ex_data(CRYPTO_EX_DATA *ad, int idx, void *val);
void *CRYPTO_get_ex_data(const CRYPTO_EX_DATA *ad,int idx);
/* This function cleans up all "ex_data" state. It mustn't be called under
 * potential race-conditions. */
void CRYPTO_cleanup_all_ex_data(void);

int CRYPTO_get_new_lockid(char *name);

int CRYPTO_num_locks(void); /* return CRYPTO_NUM_LOCKS (shared libs!) */
void CRYPTO_lock(int mode, int type,const char *file,int line);
void CRYPTO_set_locking_callback(void (*func)(int mode,int type,
					      const char *file,int line));
void (*CRYPTO_get_locking_callback(void))(int mode,int type,const char *file,
		int line);
void CRYPTO_set_add_lock_callback(int (*func)(int *num,int mount,int type,
					      const char *file, int line));
int (*CRYPTO_get_add_lock_callback(void))(int *num,int mount,int type,
					  const char *file,int line);
void CRYPTO_set_id_callback(unsigned long (*func)(void));
unsigned long (*CRYPTO_get_id_callback(void))(void);
unsigned long CRYPTO_thread_id(void);
const char *CRYPTO_get_lock_name(int type);
int CRYPTO_add_lock(int *pointer,int amount,int type, const char *file,
		    int line);

void int_CRYPTO_set_do_dynlock_callback(
	void (*do_dynlock_cb)(int mode, int type, const char *file, int line));

int CRYPTO_get_new_dynlockid(void);
void CRYPTO_destroy_dynlockid(int i);
struct CRYPTO_dynlock_value *CRYPTO_get_dynlock_value(int i);
void CRYPTO_set_dynlock_create_callback(struct CRYPTO_dynlock_value *(*dyn_create_function)(const char *file, int line));
void CRYPTO_set_dynlock_lock_callback(void (*dyn_lock_function)(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line));
void CRYPTO_set_dynlock_destroy_callback(void (*dyn_destroy_function)(struct CRYPTO_dynlock_value *l, const char *file, int line));
struct CRYPTO_dynlock_value *(*CRYPTO_get_dynlock_create_callback(void))(const char *file,int line);
void (*CRYPTO_get_dynlock_lock_callback(void))(int mode, struct CRYPTO_dynlock_value *l, const char *file,int line);
void (*CRYPTO_get_dynlock_destroy_callback(void))(struct CRYPTO_dynlock_value *l, const char *file,int line);

/* CRYPTO_set_mem_functions includes CRYPTO_set_locked_mem_functions --
 * call the latter last if you need different functions */
int CRYPTO_set_mem_functions(void *(*m)(size_t),void *(*r)(void *,size_t), void (*f)(void *));
int CRYPTO_set_locked_mem_functions(void *(*m)(size_t), void (*free_func)(void *));
int CRYPTO_set_mem_ex_functions(void *(*m)(size_t,const char *,int),
                                void *(*r)(void *,size_t,const char *,int),
                                void (*f)(void *));
int CRYPTO_set_locked_mem_ex_functions(void *(*m)(size_t,const char *,int),
                                       void (*free_func)(void *));
int CRYPTO_set_mem_debug_functions(void (*m)(void *,int,const char *,int,int),
				   void (*r)(void *,void *,int,const char *,int,int),
				   void (*f)(void *,int),
				   void (*so)(long),
				   long (*go)(void));
void CRYPTO_set_mem_info_functions(
	int  (*push_info_fn)(const char *info, const char *file, int line),
	int  (*pop_info_fn)(void),
	int (*remove_all_info_fn)(void));
void CRYPTO_get_mem_functions(void *(**m)(size_t),void *(**r)(void *, size_t), void (**f)(void *));
void CRYPTO_get_locked_mem_functions(void *(**m)(size_t), void (**f)(void *));
void CRYPTO_get_mem_ex_functions(void *(**m)(size_t,const char *,int),
                                 void *(**r)(void *, size_t,const char *,int),
                                 void (**f)(void *));
void CRYPTO_get_locked_mem_ex_functions(void *(**m)(size_t,const char *,int),
                                        void (**f)(void *));
void CRYPTO_get_mem_debug_functions(void (**m)(void *,int,const char *,int,int),
				    void (**r)(void *,void *,int,const char *,int,int),
				    void (**f)(void *,int),
				    void (**so)(long),
				    long (**go)(void));

void *CRYPTO_malloc_locked(int num, const char *file, int line);
void CRYPTO_free_locked(void *);
void *CRYPTO_malloc(int num, const char *file, int line);
char *CRYPTO_strdup(const char *str, const char *file, int line);
void CRYPTO_free(void *);
void *CRYPTO_realloc(void *addr,int num, const char *file, int line);
void *CRYPTO_realloc_clean(void *addr,int old_num,int num,const char *file,
			   int line);
void *CRYPTO_remalloc(void *addr,int num, const char *file, int line);

void OPENSSL_cleanse(void *ptr, size_t len);

void CRYPTO_set_mem_debug_options(long bits);
long CRYPTO_get_mem_debug_options(void);

#define CRYPTO_push_info(info) \
        CRYPTO_push_info_(info, __FILE__, __LINE__);
int CRYPTO_push_info_(const char *info, const char *file, int line);
int CRYPTO_pop_info(void);
int CRYPTO_remove_all_info(void);


/* Default debugging functions (enabled by CRYPTO_malloc_debug_init() macro;
 * used as default in CRYPTO_MDEBUG compilations): */
/* The last argument has the following significance:
 *
 * 0:	called before the actual memory allocation has taken place
 * 1:	called after the actual memory allocation has taken place
 */
void CRYPTO_dbg_malloc(void *addr,int num,const char *file,int line,int before_p);
void CRYPTO_dbg_realloc(void *addr1,void *addr2,int num,const char *file,int line,int before_p);
void CRYPTO_dbg_free(void *addr,int before_p);
/* Tell the debugging code about options.  By default, the following values
 * apply:
 *
 * 0:                           Clear all options.
 * V_CRYPTO_MDEBUG_TIME (1):    Set the "Show Time" option.
 * V_CRYPTO_MDEBUG_THREAD (2):  Set the "Show Thread Number" option.
 * V_CRYPTO_MDEBUG_ALL (3):     1 + 2
 */
void CRYPTO_dbg_set_options(long bits);
long CRYPTO_dbg_get_options(void);

int CRYPTO_dbg_push_info(const char *info, const char *file, int line);
int CRYPTO_dbg_pop_info(void);
int CRYPTO_dbg_remove_all_info(void);

#ifndef OPENSSL_NO_FP_API
void CRYPTO_mem_leaks_fp(FILE *);
#endif
void CRYPTO_mem_leaks(struct bio_st *bio);
/* unsigned long order, char *file, int line, int num_bytes, char *addr */
typedef void *CRYPTO_MEM_LEAK_CB(unsigned long, const char *, int, int, void *);
void CRYPTO_mem_leaks_cb(CRYPTO_MEM_LEAK_CB *cb);

/* die if we have to */
void OpenSSLDie(const char *file,int line,const char *assertion);
#define OPENSSL_assert(e)       (void)((e) ? 0 : (OpenSSLDie(__FILE__, __LINE__, #e),1))

unsigned long *OPENSSL_ia32cap_loc(void);
#define OPENSSL_ia32cap (*(OPENSSL_ia32cap_loc()))
int OPENSSL_isservice(void);

#ifdef OPENSSL_FIPS
#define FIPS_ERROR_IGNORED(alg) OpenSSLDie(__FILE__, __LINE__, \
		alg " previous FIPS forbidden algorithm error ignored");

#define FIPS_BAD_ABORT(alg) OpenSSLDie(__FILE__, __LINE__, \
		#alg " Algorithm forbidden in FIPS mode");

#ifdef OPENSSL_FIPS_STRICT
#define FIPS_BAD_ALGORITHM(alg) FIPS_BAD_ABORT(alg)
#else
#define FIPS_BAD_ALGORITHM(alg) \
	{ \
	FIPSerr(FIPS_F_HASH_FINAL,FIPS_R_NON_FIPS_METHOD); \
	ERR_add_error_data(2, "Algorithm=", #alg); \
	return 0; \
	}
#endif

/* Low level digest API blocking macro */

#define FIPS_NON_FIPS_MD_Init(alg) \
	int alg##_Init(alg##_CTX *c) \
		{ \
		if (FIPS_mode()) \
			FIPS_BAD_ALGORITHM(alg) \
		return private_##alg##_Init(c); \
		} \
	int private_##alg##_Init(alg##_CTX *c)

/* For ciphers the API often varies from cipher to cipher and each needs to
 * be treated as a special case. Variable key length ciphers (Blowfish, RC4,
 * CAST) however are very similar and can use a blocking macro.
 */

#define FIPS_NON_FIPS_VCIPHER_Init(alg) \
	void alg##_set_key(alg##_KEY *key, int len, const unsigned char *data) \
		{ \
		if (FIPS_mode()) \
			FIPS_BAD_ABORT(alg) \
		private_##alg##_set_key(key, len, data); \
		} \
	void private_##alg##_set_key(alg##_KEY *key, int len, \
					const unsigned char *data)

#else

#define FIPS_NON_FIPS_VCIPHER_Init(alg) \
	void alg##_set_key(alg##_KEY *key, int len, const unsigned char *data)

#define FIPS_NON_FIPS_MD_Init(alg) \
	int alg##_Init(alg##_CTX *c) 

#endif /* def OPENSSL_FIPS */

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_CRYPTO_strings(void);

#define OPENSSL_HAVE_INIT	1
void OPENSSL_init(void);

/* Error codes for the CRYPTO functions. */

/* Function codes. */
#define CRYPTO_F_CRYPTO_GET_EX_NEW_INDEX		 100
#define CRYPTO_F_CRYPTO_GET_NEW_DYNLOCKID		 103
#define CRYPTO_F_CRYPTO_GET_NEW_LOCKID			 101
#define CRYPTO_F_CRYPTO_SET_EX_DATA			 102
#define CRYPTO_F_DEF_ADD_INDEX				 104
#define CRYPTO_F_DEF_GET_CLASS				 105
#define CRYPTO_F_INT_DUP_EX_DATA			 106
#define CRYPTO_F_INT_FREE_EX_DATA			 107
#define CRYPTO_F_INT_NEW_EX_DATA			 108

/* Reason codes. */
#define CRYPTO_R_NO_DYNLOCK_CREATE_CALLBACK		 100

#ifdef  __cplusplus
}
#endif
#endif

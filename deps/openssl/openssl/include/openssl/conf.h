/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef  HEADER_CONF_H
# define HEADER_CONF_H

# include <openssl/bio.h>
# include <openssl/lhash.h>
# include <openssl/stack.h>
# include <openssl/safestack.h>
# include <openssl/e_os2.h>

# include <openssl/ossl_typ.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct {
    char *section;
    char *name;
    char *value;
} CONF_VALUE;

DEFINE_STACK_OF(CONF_VALUE)
DEFINE_LHASH_OF(CONF_VALUE);

struct conf_st;
struct conf_method_st;
typedef struct conf_method_st CONF_METHOD;

struct conf_method_st {
    const char *name;
    CONF *(*create) (CONF_METHOD *meth);
    int (*init) (CONF *conf);
    int (*destroy) (CONF *conf);
    int (*destroy_data) (CONF *conf);
    int (*load_bio) (CONF *conf, BIO *bp, long *eline);
    int (*dump) (const CONF *conf, BIO *bp);
    int (*is_number) (const CONF *conf, char c);
    int (*to_int) (const CONF *conf, char c);
    int (*load) (CONF *conf, const char *name, long *eline);
};

/* Module definitions */

typedef struct conf_imodule_st CONF_IMODULE;
typedef struct conf_module_st CONF_MODULE;

DEFINE_STACK_OF(CONF_MODULE)
DEFINE_STACK_OF(CONF_IMODULE)

/* DSO module function typedefs */
typedef int conf_init_func (CONF_IMODULE *md, const CONF *cnf);
typedef void conf_finish_func (CONF_IMODULE *md);

# define CONF_MFLAGS_IGNORE_ERRORS       0x1
# define CONF_MFLAGS_IGNORE_RETURN_CODES 0x2
# define CONF_MFLAGS_SILENT              0x4
# define CONF_MFLAGS_NO_DSO              0x8
# define CONF_MFLAGS_IGNORE_MISSING_FILE 0x10
# define CONF_MFLAGS_DEFAULT_SECTION     0x20

int CONF_set_default_method(CONF_METHOD *meth);
void CONF_set_nconf(CONF *conf, LHASH_OF(CONF_VALUE) *hash);
LHASH_OF(CONF_VALUE) *CONF_load(LHASH_OF(CONF_VALUE) *conf, const char *file,
                                long *eline);
# ifndef OPENSSL_NO_STDIO
LHASH_OF(CONF_VALUE) *CONF_load_fp(LHASH_OF(CONF_VALUE) *conf, FILE *fp,
                                   long *eline);
# endif
LHASH_OF(CONF_VALUE) *CONF_load_bio(LHASH_OF(CONF_VALUE) *conf, BIO *bp,
                                    long *eline);
STACK_OF(CONF_VALUE) *CONF_get_section(LHASH_OF(CONF_VALUE) *conf,
                                       const char *section);
char *CONF_get_string(LHASH_OF(CONF_VALUE) *conf, const char *group,
                      const char *name);
long CONF_get_number(LHASH_OF(CONF_VALUE) *conf, const char *group,
                     const char *name);
void CONF_free(LHASH_OF(CONF_VALUE) *conf);
#ifndef OPENSSL_NO_STDIO
int CONF_dump_fp(LHASH_OF(CONF_VALUE) *conf, FILE *out);
#endif
int CONF_dump_bio(LHASH_OF(CONF_VALUE) *conf, BIO *out);

DEPRECATEDIN_1_1_0(void OPENSSL_config(const char *config_name))

#if OPENSSL_API_COMPAT < 0x10100000L
# define OPENSSL_no_config() \
    OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, NULL)
#endif

/*
 * New conf code.  The semantics are different from the functions above. If
 * that wasn't the case, the above functions would have been replaced
 */

struct conf_st {
    CONF_METHOD *meth;
    void *meth_data;
    LHASH_OF(CONF_VALUE) *data;
};

CONF *NCONF_new(CONF_METHOD *meth);
CONF_METHOD *NCONF_default(void);
CONF_METHOD *NCONF_WIN32(void);
void NCONF_free(CONF *conf);
void NCONF_free_data(CONF *conf);

int NCONF_load(CONF *conf, const char *file, long *eline);
# ifndef OPENSSL_NO_STDIO
int NCONF_load_fp(CONF *conf, FILE *fp, long *eline);
# endif
int NCONF_load_bio(CONF *conf, BIO *bp, long *eline);
STACK_OF(CONF_VALUE) *NCONF_get_section(const CONF *conf,
                                        const char *section);
char *NCONF_get_string(const CONF *conf, const char *group, const char *name);
int NCONF_get_number_e(const CONF *conf, const char *group, const char *name,
                       long *result);
#ifndef OPENSSL_NO_STDIO
int NCONF_dump_fp(const CONF *conf, FILE *out);
#endif
int NCONF_dump_bio(const CONF *conf, BIO *out);

#define NCONF_get_number(c,g,n,r) NCONF_get_number_e(c,g,n,r)

/* Module functions */

int CONF_modules_load(const CONF *cnf, const char *appname,
                      unsigned long flags);
int CONF_modules_load_file(const char *filename, const char *appname,
                           unsigned long flags);
void CONF_modules_unload(int all);
void CONF_modules_finish(void);
#if OPENSSL_API_COMPAT < 0x10100000L
# define CONF_modules_free() while(0) continue
#endif
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
                    int (*list_cb) (const char *elem, int len, void *usr),
                    void *arg);

void OPENSSL_load_builtin_modules(void);

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */

int ERR_load_CONF_strings(void);

/* Error codes for the CONF functions. */

/* Function codes. */
# define CONF_F_CONF_DUMP_FP                              104
# define CONF_F_CONF_LOAD                                 100
# define CONF_F_CONF_LOAD_FP                              103
# define CONF_F_CONF_PARSE_LIST                           119
# define CONF_F_DEF_LOAD                                  120
# define CONF_F_DEF_LOAD_BIO                              121
# define CONF_F_MODULE_INIT                               115
# define CONF_F_MODULE_LOAD_DSO                           117
# define CONF_F_MODULE_RUN                                118
# define CONF_F_NCONF_DUMP_BIO                            105
# define CONF_F_NCONF_DUMP_FP                             106
# define CONF_F_NCONF_GET_NUMBER_E                        112
# define CONF_F_NCONF_GET_SECTION                         108
# define CONF_F_NCONF_GET_STRING                          109
# define CONF_F_NCONF_LOAD                                113
# define CONF_F_NCONF_LOAD_BIO                            110
# define CONF_F_NCONF_LOAD_FP                             114
# define CONF_F_NCONF_NEW                                 111
# define CONF_F_STR_COPY                                  101

/* Reason codes. */
# define CONF_R_ERROR_LOADING_DSO                         110
# define CONF_R_LIST_CANNOT_BE_NULL                       115
# define CONF_R_MISSING_CLOSE_SQUARE_BRACKET              100
# define CONF_R_MISSING_EQUAL_SIGN                        101
# define CONF_R_MISSING_INIT_FUNCTION                     112
# define CONF_R_MODULE_INITIALIZATION_ERROR               109
# define CONF_R_NO_CLOSE_BRACE                            102
# define CONF_R_NO_CONF                                   105
# define CONF_R_NO_CONF_OR_ENVIRONMENT_VARIABLE           106
# define CONF_R_NO_SECTION                                107
# define CONF_R_NO_SUCH_FILE                              114
# define CONF_R_NO_VALUE                                  108
# define CONF_R_UNABLE_TO_CREATE_NEW_SECTION              103
# define CONF_R_UNKNOWN_MODULE_NAME                       113
# define CONF_R_VARIABLE_EXPANSION_TOO_LONG               116
# define CONF_R_VARIABLE_HAS_NO_VALUE                     104

# ifdef  __cplusplus
}
# endif
#endif

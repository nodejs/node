/* crypto/store/store.h */
/*
 * Written by Richard Levitte (richard@levitte.org) for the OpenSSL project
 * 2003.
 */
/* ====================================================================
 * Copyright (c) 2003 The OpenSSL Project.  All rights reserved.
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

#ifndef HEADER_STORE_H
# define HEADER_STORE_H

# include <openssl/opensslconf.h>

# ifdef OPENSSL_NO_STORE
#  error STORE is disabled.
# endif

# include <openssl/ossl_typ.h>
# ifndef OPENSSL_NO_DEPRECATED
#  include <openssl/evp.h>
#  include <openssl/bn.h>
#  include <openssl/x509.h>
# endif

#ifdef  __cplusplus
extern "C" {
#endif

/* Already defined in ossl_typ.h */
/* typedef struct store_st STORE; */
/* typedef struct store_method_st STORE_METHOD; */

/*
 * All the following functions return 0, a negative number or NULL on error.
 * When everything is fine, they return a positive value or a non-NULL
 * pointer, all depending on their purpose.
 */

/* Creators and destructor.   */
STORE *STORE_new_method(const STORE_METHOD *method);
STORE *STORE_new_engine(ENGINE *engine);
void STORE_free(STORE *ui);

/*
 * Give a user interface parametrised control commands.  This can be used to
 * send down an integer, a data pointer or a function pointer, as well as be
 * used to get information from a STORE.
 */
int STORE_ctrl(STORE *store, int cmd, long i, void *p, void (*f) (void));

/*
 * A control to set the directory with keys and certificates.  Used by the
 * built-in directory level method.
 */
# define STORE_CTRL_SET_DIRECTORY        0x0001
/*
 * A control to set a file to load.  Used by the built-in file level method.
 */
# define STORE_CTRL_SET_FILE             0x0002
/*
 * A control to set a configuration file to load.  Can be used by any method
 * that wishes to load a configuration file.
 */
# define STORE_CTRL_SET_CONF_FILE        0x0003
/*
 * A control to set a the section of the loaded configuration file.  Can be
 * used by any method that wishes to load a configuration file.
 */
# define STORE_CTRL_SET_CONF_SECTION     0x0004

/* Some methods may use extra data */
# define STORE_set_app_data(s,arg)       STORE_set_ex_data(s,0,arg)
# define STORE_get_app_data(s)           STORE_get_ex_data(s,0)
int STORE_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
                           CRYPTO_EX_dup *dup_func,
                           CRYPTO_EX_free *free_func);
int STORE_set_ex_data(STORE *r, int idx, void *arg);
void *STORE_get_ex_data(STORE *r, int idx);

/* Use specific methods instead of the built-in one */
const STORE_METHOD *STORE_get_method(STORE *store);
const STORE_METHOD *STORE_set_method(STORE *store, const STORE_METHOD *meth);

/* The standard OpenSSL methods. */
/*
 * This is the in-memory method.  It does everything except revoking and
 * updating, and is of course volatile.  It's used by other methods that have
 * an in-memory cache.
 */
const STORE_METHOD *STORE_Memory(void);
# if 0                          /* Not yet implemented */
/*
 * This is the directory store.  It does everything except revoking and
 * updating, and uses STORE_Memory() to cache things in memory.
 */
const STORE_METHOD *STORE_Directory(void);
/*
 * This is the file store.  It does everything except revoking and updating,
 * and uses STORE_Memory() to cache things in memory.  Certificates are added
 * to it with the store operation, and it will only get cached certificates.
 */
const STORE_METHOD *STORE_File(void);
# endif

/*
 * Store functions take a type code for the type of data they should store or
 * fetch
 */
typedef enum STORE_object_types {
    STORE_OBJECT_TYPE_X509_CERTIFICATE = 0x01, /* X509 * */
    STORE_OBJECT_TYPE_X509_CRL = 0x02, /* X509_CRL * */
    STORE_OBJECT_TYPE_PRIVATE_KEY = 0x03, /* EVP_PKEY * */
    STORE_OBJECT_TYPE_PUBLIC_KEY = 0x04, /* EVP_PKEY * */
    STORE_OBJECT_TYPE_NUMBER = 0x05, /* BIGNUM * */
    STORE_OBJECT_TYPE_ARBITRARY = 0x06, /* BUF_MEM * */
    STORE_OBJECT_TYPE_NUM = 0x06 /* The amount of known object types */
} STORE_OBJECT_TYPES;
/* List of text strings corresponding to the object types. */
extern const char *const STORE_object_type_string[STORE_OBJECT_TYPE_NUM + 1];

/*
 * Some store functions take a parameter list.  Those parameters come with
 * one of the following codes. The comments following the codes below
 * indicate what type the value should be a pointer to.
 */
typedef enum STORE_params {
    STORE_PARAM_EVP_TYPE = 0x01, /* int */
    STORE_PARAM_BITS = 0x02,    /* size_t */
    STORE_PARAM_KEY_PARAMETERS = 0x03, /* ??? */
    STORE_PARAM_KEY_NO_PARAMETERS = 0x04, /* N/A */
    STORE_PARAM_AUTH_PASSPHRASE = 0x05, /* char * */
    STORE_PARAM_AUTH_KRB5_TICKET = 0x06, /* void * */
    STORE_PARAM_TYPE_NUM = 0x06 /* The amount of known parameter types */
} STORE_PARAM_TYPES;
/*
 * Parameter value sizes.  -1 means unknown, anything else is the required
 * size.
 */
extern const int STORE_param_sizes[STORE_PARAM_TYPE_NUM + 1];

/*
 * Store functions take attribute lists.  Those attributes come with codes.
 * The comments following the codes below indicate what type the value should
 * be a pointer to.
 */
typedef enum STORE_attribs {
    STORE_ATTR_END = 0x00,
    STORE_ATTR_FRIENDLYNAME = 0x01, /* C string */
    STORE_ATTR_KEYID = 0x02,    /* 160 bit string (SHA1) */
    STORE_ATTR_ISSUERKEYID = 0x03, /* 160 bit string (SHA1) */
    STORE_ATTR_SUBJECTKEYID = 0x04, /* 160 bit string (SHA1) */
    STORE_ATTR_ISSUERSERIALHASH = 0x05, /* 160 bit string (SHA1) */
    STORE_ATTR_ISSUER = 0x06,   /* X509_NAME * */
    STORE_ATTR_SERIAL = 0x07,   /* BIGNUM * */
    STORE_ATTR_SUBJECT = 0x08,  /* X509_NAME * */
    STORE_ATTR_CERTHASH = 0x09, /* 160 bit string (SHA1) */
    STORE_ATTR_EMAIL = 0x0a,    /* C string */
    STORE_ATTR_FILENAME = 0x0b, /* C string */
    STORE_ATTR_TYPE_NUM = 0x0b, /* The amount of known attribute types */
    STORE_ATTR_OR = 0xff        /* This is a special separator, which
                                 * expresses the OR operation.  */
} STORE_ATTR_TYPES;
/*
 * Attribute value sizes.  -1 means unknown, anything else is the required
 * size.
 */
extern const int STORE_attr_sizes[STORE_ATTR_TYPE_NUM + 1];

typedef enum STORE_certificate_status {
    STORE_X509_VALID = 0x00,
    STORE_X509_EXPIRED = 0x01,
    STORE_X509_SUSPENDED = 0x02,
    STORE_X509_REVOKED = 0x03
} STORE_CERTIFICATE_STATUS;

/*
 * Engine store functions will return a structure that contains all the
 * necessary information, including revokation status for certificates.  This
 * is really not needed for application authors, as the ENGINE framework
 * functions will extract the OpenSSL-specific information when at all
 * possible.  However, for engine authors, it's crucial to know this
 * structure.
 */
typedef struct STORE_OBJECT_st {
    STORE_OBJECT_TYPES type;
    union {
        struct {
            STORE_CERTIFICATE_STATUS status;
            X509 *certificate;
        } x509;
        X509_CRL *crl;
        EVP_PKEY *key;
        BIGNUM *number;
        BUF_MEM *arbitrary;
    } data;
} STORE_OBJECT;
DECLARE_STACK_OF(STORE_OBJECT)
STORE_OBJECT *STORE_OBJECT_new(void);
void STORE_OBJECT_free(STORE_OBJECT *data);

/*
 * The following functions handle the storage. They return 0, a negative
 * number or NULL on error, anything else on success.
 */
X509 *STORE_get_certificate(STORE *e, OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[]);
int STORE_store_certificate(STORE *e, X509 *data, OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[]);
int STORE_modify_certificate(STORE *e, OPENSSL_ITEM search_attributes[],
                             OPENSSL_ITEM add_attributes[],
                             OPENSSL_ITEM modify_attributes[],
                             OPENSSL_ITEM delete_attributes[],
                             OPENSSL_ITEM parameters[]);
int STORE_revoke_certificate(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
int STORE_delete_certificate(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
void *STORE_list_certificate_start(STORE *e, OPENSSL_ITEM attributes[],
                                   OPENSSL_ITEM parameters[]);
X509 *STORE_list_certificate_next(STORE *e, void *handle);
int STORE_list_certificate_end(STORE *e, void *handle);
int STORE_list_certificate_endp(STORE *e, void *handle);
EVP_PKEY *STORE_generate_key(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
EVP_PKEY *STORE_get_private_key(STORE *e, OPENSSL_ITEM attributes[],
                                OPENSSL_ITEM parameters[]);
int STORE_store_private_key(STORE *e, EVP_PKEY *data,
                            OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[]);
int STORE_modify_private_key(STORE *e, OPENSSL_ITEM search_attributes[],
                             OPENSSL_ITEM add_sttributes[],
                             OPENSSL_ITEM modify_attributes[],
                             OPENSSL_ITEM delete_attributes[],
                             OPENSSL_ITEM parameters[]);
int STORE_revoke_private_key(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
int STORE_delete_private_key(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
void *STORE_list_private_key_start(STORE *e, OPENSSL_ITEM attributes[],
                                   OPENSSL_ITEM parameters[]);
EVP_PKEY *STORE_list_private_key_next(STORE *e, void *handle);
int STORE_list_private_key_end(STORE *e, void *handle);
int STORE_list_private_key_endp(STORE *e, void *handle);
EVP_PKEY *STORE_get_public_key(STORE *e, OPENSSL_ITEM attributes[],
                               OPENSSL_ITEM parameters[]);
int STORE_store_public_key(STORE *e, EVP_PKEY *data,
                           OPENSSL_ITEM attributes[],
                           OPENSSL_ITEM parameters[]);
int STORE_modify_public_key(STORE *e, OPENSSL_ITEM search_attributes[],
                            OPENSSL_ITEM add_sttributes[],
                            OPENSSL_ITEM modify_attributes[],
                            OPENSSL_ITEM delete_attributes[],
                            OPENSSL_ITEM parameters[]);
int STORE_revoke_public_key(STORE *e, OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[]);
int STORE_delete_public_key(STORE *e, OPENSSL_ITEM attributes[],
                            OPENSSL_ITEM parameters[]);
void *STORE_list_public_key_start(STORE *e, OPENSSL_ITEM attributes[],
                                  OPENSSL_ITEM parameters[]);
EVP_PKEY *STORE_list_public_key_next(STORE *e, void *handle);
int STORE_list_public_key_end(STORE *e, void *handle);
int STORE_list_public_key_endp(STORE *e, void *handle);
X509_CRL *STORE_generate_crl(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
X509_CRL *STORE_get_crl(STORE *e, OPENSSL_ITEM attributes[],
                        OPENSSL_ITEM parameters[]);
int STORE_store_crl(STORE *e, X509_CRL *data, OPENSSL_ITEM attributes[],
                    OPENSSL_ITEM parameters[]);
int STORE_modify_crl(STORE *e, OPENSSL_ITEM search_attributes[],
                     OPENSSL_ITEM add_sttributes[],
                     OPENSSL_ITEM modify_attributes[],
                     OPENSSL_ITEM delete_attributes[],
                     OPENSSL_ITEM parameters[]);
int STORE_delete_crl(STORE *e, OPENSSL_ITEM attributes[],
                     OPENSSL_ITEM parameters[]);
void *STORE_list_crl_start(STORE *e, OPENSSL_ITEM attributes[],
                           OPENSSL_ITEM parameters[]);
X509_CRL *STORE_list_crl_next(STORE *e, void *handle);
int STORE_list_crl_end(STORE *e, void *handle);
int STORE_list_crl_endp(STORE *e, void *handle);
int STORE_store_number(STORE *e, BIGNUM *data, OPENSSL_ITEM attributes[],
                       OPENSSL_ITEM parameters[]);
int STORE_modify_number(STORE *e, OPENSSL_ITEM search_attributes[],
                        OPENSSL_ITEM add_sttributes[],
                        OPENSSL_ITEM modify_attributes[],
                        OPENSSL_ITEM delete_attributes[],
                        OPENSSL_ITEM parameters[]);
BIGNUM *STORE_get_number(STORE *e, OPENSSL_ITEM attributes[],
                         OPENSSL_ITEM parameters[]);
int STORE_delete_number(STORE *e, OPENSSL_ITEM attributes[],
                        OPENSSL_ITEM parameters[]);
int STORE_store_arbitrary(STORE *e, BUF_MEM *data, OPENSSL_ITEM attributes[],
                          OPENSSL_ITEM parameters[]);
int STORE_modify_arbitrary(STORE *e, OPENSSL_ITEM search_attributes[],
                           OPENSSL_ITEM add_sttributes[],
                           OPENSSL_ITEM modify_attributes[],
                           OPENSSL_ITEM delete_attributes[],
                           OPENSSL_ITEM parameters[]);
BUF_MEM *STORE_get_arbitrary(STORE *e, OPENSSL_ITEM attributes[],
                             OPENSSL_ITEM parameters[]);
int STORE_delete_arbitrary(STORE *e, OPENSSL_ITEM attributes[],
                           OPENSSL_ITEM parameters[]);

/* Create and manipulate methods */
STORE_METHOD *STORE_create_method(char *name);
void STORE_destroy_method(STORE_METHOD *store_method);

/* These callback types are use for store handlers */
typedef int (*STORE_INITIALISE_FUNC_PTR) (STORE *);
typedef void (*STORE_CLEANUP_FUNC_PTR) (STORE *);
typedef STORE_OBJECT *(*STORE_GENERATE_OBJECT_FUNC_PTR)(STORE *,
                                                        STORE_OBJECT_TYPES
                                                        type,
                                                        OPENSSL_ITEM
                                                        attributes[],
                                                        OPENSSL_ITEM
                                                        parameters[]);
typedef STORE_OBJECT *(*STORE_GET_OBJECT_FUNC_PTR)(STORE *,
                                                   STORE_OBJECT_TYPES type,
                                                   OPENSSL_ITEM attributes[],
                                                   OPENSSL_ITEM parameters[]);
typedef void *(*STORE_START_OBJECT_FUNC_PTR)(STORE *, STORE_OBJECT_TYPES type,
                                             OPENSSL_ITEM attributes[],
                                             OPENSSL_ITEM parameters[]);
typedef STORE_OBJECT *(*STORE_NEXT_OBJECT_FUNC_PTR)(STORE *, void *handle);
typedef int (*STORE_END_OBJECT_FUNC_PTR) (STORE *, void *handle);
typedef int (*STORE_HANDLE_OBJECT_FUNC_PTR) (STORE *, STORE_OBJECT_TYPES type,
                                             OPENSSL_ITEM attributes[],
                                             OPENSSL_ITEM parameters[]);
typedef int (*STORE_STORE_OBJECT_FUNC_PTR) (STORE *, STORE_OBJECT_TYPES type,
                                            STORE_OBJECT *data,
                                            OPENSSL_ITEM attributes[],
                                            OPENSSL_ITEM parameters[]);
typedef int (*STORE_MODIFY_OBJECT_FUNC_PTR) (STORE *, STORE_OBJECT_TYPES type,
                                             OPENSSL_ITEM search_attributes[],
                                             OPENSSL_ITEM add_attributes[],
                                             OPENSSL_ITEM modify_attributes[],
                                             OPENSSL_ITEM delete_attributes[],
                                             OPENSSL_ITEM parameters[]);
typedef int (*STORE_GENERIC_FUNC_PTR) (STORE *, OPENSSL_ITEM attributes[],
                                       OPENSSL_ITEM parameters[]);
typedef int (*STORE_CTRL_FUNC_PTR) (STORE *, int cmd, long l, void *p,
                                    void (*f) (void));

int STORE_method_set_initialise_function(STORE_METHOD *sm,
                                         STORE_INITIALISE_FUNC_PTR init_f);
int STORE_method_set_cleanup_function(STORE_METHOD *sm,
                                      STORE_CLEANUP_FUNC_PTR clean_f);
int STORE_method_set_generate_function(STORE_METHOD *sm,
                                       STORE_GENERATE_OBJECT_FUNC_PTR
                                       generate_f);
int STORE_method_set_get_function(STORE_METHOD *sm,
                                  STORE_GET_OBJECT_FUNC_PTR get_f);
int STORE_method_set_store_function(STORE_METHOD *sm,
                                    STORE_STORE_OBJECT_FUNC_PTR store_f);
int STORE_method_set_modify_function(STORE_METHOD *sm,
                                     STORE_MODIFY_OBJECT_FUNC_PTR store_f);
int STORE_method_set_revoke_function(STORE_METHOD *sm,
                                     STORE_HANDLE_OBJECT_FUNC_PTR revoke_f);
int STORE_method_set_delete_function(STORE_METHOD *sm,
                                     STORE_HANDLE_OBJECT_FUNC_PTR delete_f);
int STORE_method_set_list_start_function(STORE_METHOD *sm,
                                         STORE_START_OBJECT_FUNC_PTR
                                         list_start_f);
int STORE_method_set_list_next_function(STORE_METHOD *sm,
                                        STORE_NEXT_OBJECT_FUNC_PTR
                                        list_next_f);
int STORE_method_set_list_end_function(STORE_METHOD *sm,
                                       STORE_END_OBJECT_FUNC_PTR list_end_f);
int STORE_method_set_update_store_function(STORE_METHOD *sm,
                                           STORE_GENERIC_FUNC_PTR);
int STORE_method_set_lock_store_function(STORE_METHOD *sm,
                                         STORE_GENERIC_FUNC_PTR);
int STORE_method_set_unlock_store_function(STORE_METHOD *sm,
                                           STORE_GENERIC_FUNC_PTR);
int STORE_method_set_ctrl_function(STORE_METHOD *sm,
                                   STORE_CTRL_FUNC_PTR ctrl_f);

STORE_INITIALISE_FUNC_PTR STORE_method_get_initialise_function(STORE_METHOD
                                                               *sm);
STORE_CLEANUP_FUNC_PTR STORE_method_get_cleanup_function(STORE_METHOD *sm);
STORE_GENERATE_OBJECT_FUNC_PTR STORE_method_get_generate_function(STORE_METHOD
                                                                  *sm);
STORE_GET_OBJECT_FUNC_PTR STORE_method_get_get_function(STORE_METHOD *sm);
STORE_STORE_OBJECT_FUNC_PTR STORE_method_get_store_function(STORE_METHOD *sm);
STORE_MODIFY_OBJECT_FUNC_PTR STORE_method_get_modify_function(STORE_METHOD
                                                              *sm);
STORE_HANDLE_OBJECT_FUNC_PTR STORE_method_get_revoke_function(STORE_METHOD
                                                              *sm);
STORE_HANDLE_OBJECT_FUNC_PTR STORE_method_get_delete_function(STORE_METHOD
                                                              *sm);
STORE_START_OBJECT_FUNC_PTR STORE_method_get_list_start_function(STORE_METHOD
                                                                 *sm);
STORE_NEXT_OBJECT_FUNC_PTR STORE_method_get_list_next_function(STORE_METHOD
                                                               *sm);
STORE_END_OBJECT_FUNC_PTR STORE_method_get_list_end_function(STORE_METHOD
                                                             *sm);
STORE_GENERIC_FUNC_PTR STORE_method_get_update_store_function(STORE_METHOD
                                                              *sm);
STORE_GENERIC_FUNC_PTR STORE_method_get_lock_store_function(STORE_METHOD *sm);
STORE_GENERIC_FUNC_PTR STORE_method_get_unlock_store_function(STORE_METHOD
                                                              *sm);
STORE_CTRL_FUNC_PTR STORE_method_get_ctrl_function(STORE_METHOD *sm);

/* Method helper structures and functions. */

/*
 * This structure is the result of parsing through the information in a list
 * of OPENSSL_ITEMs.  It stores all the necessary information in a structured
 * way.
 */
typedef struct STORE_attr_info_st STORE_ATTR_INFO;

/*
 * Parse a list of OPENSSL_ITEMs and return a pointer to a STORE_ATTR_INFO.
 * Note that we do this in the list form, since the list of OPENSSL_ITEMs can
 * come in blocks separated with STORE_ATTR_OR.  Note that the value returned
 * by STORE_parse_attrs_next() must be freed with STORE_ATTR_INFO_free().
 */
void *STORE_parse_attrs_start(OPENSSL_ITEM *attributes);
STORE_ATTR_INFO *STORE_parse_attrs_next(void *handle);
int STORE_parse_attrs_end(void *handle);
int STORE_parse_attrs_endp(void *handle);

/* Creator and destructor */
STORE_ATTR_INFO *STORE_ATTR_INFO_new(void);
int STORE_ATTR_INFO_free(STORE_ATTR_INFO *attrs);

/* Manipulators */
char *STORE_ATTR_INFO_get0_cstr(STORE_ATTR_INFO *attrs,
                                STORE_ATTR_TYPES code);
unsigned char *STORE_ATTR_INFO_get0_sha1str(STORE_ATTR_INFO *attrs,
                                            STORE_ATTR_TYPES code);
X509_NAME *STORE_ATTR_INFO_get0_dn(STORE_ATTR_INFO *attrs,
                                   STORE_ATTR_TYPES code);
BIGNUM *STORE_ATTR_INFO_get0_number(STORE_ATTR_INFO *attrs,
                                    STORE_ATTR_TYPES code);
int STORE_ATTR_INFO_set_cstr(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
                             char *cstr, size_t cstr_size);
int STORE_ATTR_INFO_set_sha1str(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
                                unsigned char *sha1str, size_t sha1str_size);
int STORE_ATTR_INFO_set_dn(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
                           X509_NAME *dn);
int STORE_ATTR_INFO_set_number(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
                               BIGNUM *number);
int STORE_ATTR_INFO_modify_cstr(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
                                char *cstr, size_t cstr_size);
int STORE_ATTR_INFO_modify_sha1str(STORE_ATTR_INFO *attrs,
                                   STORE_ATTR_TYPES code,
                                   unsigned char *sha1str,
                                   size_t sha1str_size);
int STORE_ATTR_INFO_modify_dn(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
                              X509_NAME *dn);
int STORE_ATTR_INFO_modify_number(STORE_ATTR_INFO *attrs,
                                  STORE_ATTR_TYPES code, BIGNUM *number);

/*
 * Compare on basis of a bit pattern formed by the STORE_ATTR_TYPES values in
 * each contained attribute.
 */
int STORE_ATTR_INFO_compare(const STORE_ATTR_INFO *const *a,
                            const STORE_ATTR_INFO *const *b);
/*
 * Check if the set of attributes in a is within the range of attributes set
 * in b.
 */
int STORE_ATTR_INFO_in_range(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b);
/* Check if the set of attributes in a are also set in b. */
int STORE_ATTR_INFO_in(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b);
/* Same as STORE_ATTR_INFO_in(), but also checks the attribute values. */
int STORE_ATTR_INFO_in_ex(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b);

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_STORE_strings(void);

/* Error codes for the STORE functions. */

/* Function codes. */
# define STORE_F_MEM_DELETE                               134
# define STORE_F_MEM_GENERATE                             135
# define STORE_F_MEM_LIST_END                             168
# define STORE_F_MEM_LIST_NEXT                            136
# define STORE_F_MEM_LIST_START                           137
# define STORE_F_MEM_MODIFY                               169
# define STORE_F_MEM_STORE                                138
# define STORE_F_STORE_ATTR_INFO_GET0_CSTR                139
# define STORE_F_STORE_ATTR_INFO_GET0_DN                  140
# define STORE_F_STORE_ATTR_INFO_GET0_NUMBER              141
# define STORE_F_STORE_ATTR_INFO_GET0_SHA1STR             142
# define STORE_F_STORE_ATTR_INFO_MODIFY_CSTR              143
# define STORE_F_STORE_ATTR_INFO_MODIFY_DN                144
# define STORE_F_STORE_ATTR_INFO_MODIFY_NUMBER            145
# define STORE_F_STORE_ATTR_INFO_MODIFY_SHA1STR           146
# define STORE_F_STORE_ATTR_INFO_SET_CSTR                 147
# define STORE_F_STORE_ATTR_INFO_SET_DN                   148
# define STORE_F_STORE_ATTR_INFO_SET_NUMBER               149
# define STORE_F_STORE_ATTR_INFO_SET_SHA1STR              150
# define STORE_F_STORE_CERTIFICATE                        170
# define STORE_F_STORE_CTRL                               161
# define STORE_F_STORE_DELETE_ARBITRARY                   158
# define STORE_F_STORE_DELETE_CERTIFICATE                 102
# define STORE_F_STORE_DELETE_CRL                         103
# define STORE_F_STORE_DELETE_NUMBER                      104
# define STORE_F_STORE_DELETE_PRIVATE_KEY                 105
# define STORE_F_STORE_DELETE_PUBLIC_KEY                  106
# define STORE_F_STORE_GENERATE_CRL                       107
# define STORE_F_STORE_GENERATE_KEY                       108
# define STORE_F_STORE_GET_ARBITRARY                      159
# define STORE_F_STORE_GET_CERTIFICATE                    109
# define STORE_F_STORE_GET_CRL                            110
# define STORE_F_STORE_GET_NUMBER                         111
# define STORE_F_STORE_GET_PRIVATE_KEY                    112
# define STORE_F_STORE_GET_PUBLIC_KEY                     113
# define STORE_F_STORE_LIST_CERTIFICATE_END               114
# define STORE_F_STORE_LIST_CERTIFICATE_ENDP              153
# define STORE_F_STORE_LIST_CERTIFICATE_NEXT              115
# define STORE_F_STORE_LIST_CERTIFICATE_START             116
# define STORE_F_STORE_LIST_CRL_END                       117
# define STORE_F_STORE_LIST_CRL_ENDP                      154
# define STORE_F_STORE_LIST_CRL_NEXT                      118
# define STORE_F_STORE_LIST_CRL_START                     119
# define STORE_F_STORE_LIST_PRIVATE_KEY_END               120
# define STORE_F_STORE_LIST_PRIVATE_KEY_ENDP              155
# define STORE_F_STORE_LIST_PRIVATE_KEY_NEXT              121
# define STORE_F_STORE_LIST_PRIVATE_KEY_START             122
# define STORE_F_STORE_LIST_PUBLIC_KEY_END                123
# define STORE_F_STORE_LIST_PUBLIC_KEY_ENDP               156
# define STORE_F_STORE_LIST_PUBLIC_KEY_NEXT               124
# define STORE_F_STORE_LIST_PUBLIC_KEY_START              125
# define STORE_F_STORE_MODIFY_ARBITRARY                   162
# define STORE_F_STORE_MODIFY_CERTIFICATE                 163
# define STORE_F_STORE_MODIFY_CRL                         164
# define STORE_F_STORE_MODIFY_NUMBER                      165
# define STORE_F_STORE_MODIFY_PRIVATE_KEY                 166
# define STORE_F_STORE_MODIFY_PUBLIC_KEY                  167
# define STORE_F_STORE_NEW_ENGINE                         133
# define STORE_F_STORE_NEW_METHOD                         132
# define STORE_F_STORE_PARSE_ATTRS_END                    151
# define STORE_F_STORE_PARSE_ATTRS_ENDP                   172
# define STORE_F_STORE_PARSE_ATTRS_NEXT                   152
# define STORE_F_STORE_PARSE_ATTRS_START                  171
# define STORE_F_STORE_REVOKE_CERTIFICATE                 129
# define STORE_F_STORE_REVOKE_PRIVATE_KEY                 130
# define STORE_F_STORE_REVOKE_PUBLIC_KEY                  131
# define STORE_F_STORE_STORE_ARBITRARY                    157
# define STORE_F_STORE_STORE_CERTIFICATE                  100
# define STORE_F_STORE_STORE_CRL                          101
# define STORE_F_STORE_STORE_NUMBER                       126
# define STORE_F_STORE_STORE_PRIVATE_KEY                  127
# define STORE_F_STORE_STORE_PUBLIC_KEY                   128

/* Reason codes. */
# define STORE_R_ALREADY_HAS_A_VALUE                      127
# define STORE_R_FAILED_DELETING_ARBITRARY                132
# define STORE_R_FAILED_DELETING_CERTIFICATE              100
# define STORE_R_FAILED_DELETING_KEY                      101
# define STORE_R_FAILED_DELETING_NUMBER                   102
# define STORE_R_FAILED_GENERATING_CRL                    103
# define STORE_R_FAILED_GENERATING_KEY                    104
# define STORE_R_FAILED_GETTING_ARBITRARY                 133
# define STORE_R_FAILED_GETTING_CERTIFICATE               105
# define STORE_R_FAILED_GETTING_KEY                       106
# define STORE_R_FAILED_GETTING_NUMBER                    107
# define STORE_R_FAILED_LISTING_CERTIFICATES              108
# define STORE_R_FAILED_LISTING_KEYS                      109
# define STORE_R_FAILED_MODIFYING_ARBITRARY               138
# define STORE_R_FAILED_MODIFYING_CERTIFICATE             139
# define STORE_R_FAILED_MODIFYING_CRL                     140
# define STORE_R_FAILED_MODIFYING_NUMBER                  141
# define STORE_R_FAILED_MODIFYING_PRIVATE_KEY             142
# define STORE_R_FAILED_MODIFYING_PUBLIC_KEY              143
# define STORE_R_FAILED_REVOKING_CERTIFICATE              110
# define STORE_R_FAILED_REVOKING_KEY                      111
# define STORE_R_FAILED_STORING_ARBITRARY                 134
# define STORE_R_FAILED_STORING_CERTIFICATE               112
# define STORE_R_FAILED_STORING_KEY                       113
# define STORE_R_FAILED_STORING_NUMBER                    114
# define STORE_R_NOT_IMPLEMENTED                          128
# define STORE_R_NO_CONTROL_FUNCTION                      144
# define STORE_R_NO_DELETE_ARBITRARY_FUNCTION             135
# define STORE_R_NO_DELETE_NUMBER_FUNCTION                115
# define STORE_R_NO_DELETE_OBJECT_FUNCTION                116
# define STORE_R_NO_GENERATE_CRL_FUNCTION                 117
# define STORE_R_NO_GENERATE_OBJECT_FUNCTION              118
# define STORE_R_NO_GET_OBJECT_ARBITRARY_FUNCTION         136
# define STORE_R_NO_GET_OBJECT_FUNCTION                   119
# define STORE_R_NO_GET_OBJECT_NUMBER_FUNCTION            120
# define STORE_R_NO_LIST_OBJECT_ENDP_FUNCTION             131
# define STORE_R_NO_LIST_OBJECT_END_FUNCTION              121
# define STORE_R_NO_LIST_OBJECT_NEXT_FUNCTION             122
# define STORE_R_NO_LIST_OBJECT_START_FUNCTION            123
# define STORE_R_NO_MODIFY_OBJECT_FUNCTION                145
# define STORE_R_NO_REVOKE_OBJECT_FUNCTION                124
# define STORE_R_NO_STORE                                 129
# define STORE_R_NO_STORE_OBJECT_ARBITRARY_FUNCTION       137
# define STORE_R_NO_STORE_OBJECT_FUNCTION                 125
# define STORE_R_NO_STORE_OBJECT_NUMBER_FUNCTION          126
# define STORE_R_NO_VALUE                                 130

#ifdef  __cplusplus
}
#endif
#endif

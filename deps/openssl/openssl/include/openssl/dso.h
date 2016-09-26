/* dso.h */
/*
 * Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL project
 * 2000.
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

#ifndef HEADER_DSO_H
# define HEADER_DSO_H

# include <openssl/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif

/* These values are used as commands to DSO_ctrl() */
# define DSO_CTRL_GET_FLAGS      1
# define DSO_CTRL_SET_FLAGS      2
# define DSO_CTRL_OR_FLAGS       3

/*
 * By default, DSO_load() will translate the provided filename into a form
 * typical for the platform (more specifically the DSO_METHOD) using the
 * dso_name_converter function of the method. Eg. win32 will transform "blah"
 * into "blah.dll", and dlfcn will transform it into "libblah.so". The
 * behaviour can be overriden by setting the name_converter callback in the
 * DSO object (using DSO_set_name_converter()). This callback could even
 * utilise the DSO_METHOD's converter too if it only wants to override
 * behaviour for one or two possible DSO methods. However, the following flag
 * can be set in a DSO to prevent *any* native name-translation at all - eg.
 * if the caller has prompted the user for a path to a driver library so the
 * filename should be interpreted as-is.
 */
# define DSO_FLAG_NO_NAME_TRANSLATION            0x01
/*
 * An extra flag to give if only the extension should be added as
 * translation.  This is obviously only of importance on Unix and other
 * operating systems where the translation also may prefix the name with
 * something, like 'lib', and ignored everywhere else. This flag is also
 * ignored if DSO_FLAG_NO_NAME_TRANSLATION is used at the same time.
 */
# define DSO_FLAG_NAME_TRANSLATION_EXT_ONLY      0x02

/*
 * The following flag controls the translation of symbol names to upper case.
 * This is currently only being implemented for OpenVMS.
 */
# define DSO_FLAG_UPCASE_SYMBOL                  0x10

/*
 * This flag loads the library with public symbols. Meaning: The exported
 * symbols of this library are public to all libraries loaded after this
 * library. At the moment only implemented in unix.
 */
# define DSO_FLAG_GLOBAL_SYMBOLS                 0x20

typedef void (*DSO_FUNC_TYPE) (void);

typedef struct dso_st DSO;

/*
 * The function prototype used for method functions (or caller-provided
 * callbacks) that transform filenames. They are passed a DSO structure
 * pointer (or NULL if they are to be used independantly of a DSO object) and
 * a filename to transform. They should either return NULL (if there is an
 * error condition) or a newly allocated string containing the transformed
 * form that the caller will need to free with OPENSSL_free() when done.
 */
typedef char *(*DSO_NAME_CONVERTER_FUNC)(DSO *, const char *);
/*
 * The function prototype used for method functions (or caller-provided
 * callbacks) that merge two file specifications. They are passed a DSO
 * structure pointer (or NULL if they are to be used independantly of a DSO
 * object) and two file specifications to merge. They should either return
 * NULL (if there is an error condition) or a newly allocated string
 * containing the result of merging that the caller will need to free with
 * OPENSSL_free() when done. Here, merging means that bits and pieces are
 * taken from each of the file specifications and added together in whatever
 * fashion that is sensible for the DSO method in question.  The only rule
 * that really applies is that if the two specification contain pieces of the
 * same type, the copy from the first string takes priority.  One could see
 * it as the first specification is the one given by the user and the second
 * being a bunch of defaults to add on if they're missing in the first.
 */
typedef char *(*DSO_MERGER_FUNC)(DSO *, const char *, const char *);

typedef struct dso_meth_st {
    const char *name;
    /*
     * Loads a shared library, NB: new DSO_METHODs must ensure that a
     * successful load populates the loaded_filename field, and likewise a
     * successful unload OPENSSL_frees and NULLs it out.
     */
    int (*dso_load) (DSO *dso);
    /* Unloads a shared library */
    int (*dso_unload) (DSO *dso);
    /* Binds a variable */
    void *(*dso_bind_var) (DSO *dso, const char *symname);
    /*
     * Binds a function - assumes a return type of DSO_FUNC_TYPE. This should
     * be cast to the real function prototype by the caller. Platforms that
     * don't have compatible representations for different prototypes (this
     * is possible within ANSI C) are highly unlikely to have shared
     * libraries at all, let alone a DSO_METHOD implemented for them.
     */
    DSO_FUNC_TYPE (*dso_bind_func) (DSO *dso, const char *symname);
/* I don't think this would actually be used in any circumstances. */
# if 0
    /* Unbinds a variable */
    int (*dso_unbind_var) (DSO *dso, char *symname, void *symptr);
    /* Unbinds a function */
    int (*dso_unbind_func) (DSO *dso, char *symname, DSO_FUNC_TYPE symptr);
# endif
    /*
     * The generic (yuck) "ctrl()" function. NB: Negative return values
     * (rather than zero) indicate errors.
     */
    long (*dso_ctrl) (DSO *dso, int cmd, long larg, void *parg);
    /*
     * The default DSO_METHOD-specific function for converting filenames to a
     * canonical native form.
     */
    DSO_NAME_CONVERTER_FUNC dso_name_converter;
    /*
     * The default DSO_METHOD-specific function for converting filenames to a
     * canonical native form.
     */
    DSO_MERGER_FUNC dso_merger;
    /* [De]Initialisation handlers. */
    int (*init) (DSO *dso);
    int (*finish) (DSO *dso);
    /* Return pathname of the module containing location */
    int (*pathbyaddr) (void *addr, char *path, int sz);
    /* Perform global symbol lookup, i.e. among *all* modules */
    void *(*globallookup) (const char *symname);
} DSO_METHOD;

/**********************************************************************/
/* The low-level handle type used to refer to a loaded shared library */

struct dso_st {
    DSO_METHOD *meth;
    /*
     * Standard dlopen uses a (void *). Win32 uses a HANDLE. VMS doesn't use
     * anything but will need to cache the filename for use in the dso_bind
     * handler. All in all, let each method control its own destiny.
     * "Handles" and such go in a STACK.
     */
    STACK_OF(void) *meth_data;
    int references;
    int flags;
    /*
     * For use by applications etc ... use this for your bits'n'pieces, don't
     * touch meth_data!
     */
    CRYPTO_EX_DATA ex_data;
    /*
     * If this callback function pointer is set to non-NULL, then it will be
     * used in DSO_load() in place of meth->dso_name_converter. NB: This
     * should normally set using DSO_set_name_converter().
     */
    DSO_NAME_CONVERTER_FUNC name_converter;
    /*
     * If this callback function pointer is set to non-NULL, then it will be
     * used in DSO_load() in place of meth->dso_merger. NB: This should
     * normally set using DSO_set_merger().
     */
    DSO_MERGER_FUNC merger;
    /*
     * This is populated with (a copy of) the platform-independant filename
     * used for this DSO.
     */
    char *filename;
    /*
     * This is populated with (a copy of) the translated filename by which
     * the DSO was actually loaded. It is NULL iff the DSO is not currently
     * loaded. NB: This is here because the filename translation process may
     * involve a callback being invoked more than once not only to convert to
     * a platform-specific form, but also to try different filenames in the
     * process of trying to perform a load. As such, this variable can be
     * used to indicate (a) whether this DSO structure corresponds to a
     * loaded library or not, and (b) the filename with which it was actually
     * loaded.
     */
    char *loaded_filename;
};

DSO *DSO_new(void);
DSO *DSO_new_method(DSO_METHOD *method);
int DSO_free(DSO *dso);
int DSO_flags(DSO *dso);
int DSO_up_ref(DSO *dso);
long DSO_ctrl(DSO *dso, int cmd, long larg, void *parg);

/*
 * This function sets the DSO's name_converter callback. If it is non-NULL,
 * then it will be used instead of the associated DSO_METHOD's function. If
 * oldcb is non-NULL then it is set to the function pointer value being
 * replaced. Return value is non-zero for success.
 */
int DSO_set_name_converter(DSO *dso, DSO_NAME_CONVERTER_FUNC cb,
                           DSO_NAME_CONVERTER_FUNC *oldcb);
/*
 * These functions can be used to get/set the platform-independant filename
 * used for a DSO. NB: set will fail if the DSO is already loaded.
 */
const char *DSO_get_filename(DSO *dso);
int DSO_set_filename(DSO *dso, const char *filename);
/*
 * This function will invoke the DSO's name_converter callback to translate a
 * filename, or if the callback isn't set it will instead use the DSO_METHOD's
 * converter. If "filename" is NULL, the "filename" in the DSO itself will be
 * used. If the DSO_FLAG_NO_NAME_TRANSLATION flag is set, then the filename is
 * simply duplicated. NB: This function is usually called from within a
 * DSO_METHOD during the processing of a DSO_load() call, and is exposed so
 * that caller-created DSO_METHODs can do the same thing. A non-NULL return
 * value will need to be OPENSSL_free()'d.
 */
char *DSO_convert_filename(DSO *dso, const char *filename);
/*
 * This function will invoke the DSO's merger callback to merge two file
 * specifications, or if the callback isn't set it will instead use the
 * DSO_METHOD's merger.  A non-NULL return value will need to be
 * OPENSSL_free()'d.
 */
char *DSO_merge(DSO *dso, const char *filespec1, const char *filespec2);
/*
 * If the DSO is currently loaded, this returns the filename that it was
 * loaded under, otherwise it returns NULL. So it is also useful as a test as
 * to whether the DSO is currently loaded. NB: This will not necessarily
 * return the same value as DSO_convert_filename(dso, dso->filename), because
 * the DSO_METHOD's load function may have tried a variety of filenames (with
 * and/or without the aid of the converters) before settling on the one it
 * actually loaded.
 */
const char *DSO_get_loaded_filename(DSO *dso);

void DSO_set_default_method(DSO_METHOD *meth);
DSO_METHOD *DSO_get_default_method(void);
DSO_METHOD *DSO_get_method(DSO *dso);
DSO_METHOD *DSO_set_method(DSO *dso, DSO_METHOD *meth);

/*
 * The all-singing all-dancing load function, you normally pass NULL for the
 * first and third parameters. Use DSO_up and DSO_free for subsequent
 * reference count handling. Any flags passed in will be set in the
 * constructed DSO after its init() function but before the load operation.
 * If 'dso' is non-NULL, 'flags' is ignored.
 */
DSO *DSO_load(DSO *dso, const char *filename, DSO_METHOD *meth, int flags);

/* This function binds to a variable inside a shared library. */
void *DSO_bind_var(DSO *dso, const char *symname);

/* This function binds to a function inside a shared library. */
DSO_FUNC_TYPE DSO_bind_func(DSO *dso, const char *symname);

/*
 * This method is the default, but will beg, borrow, or steal whatever method
 * should be the default on any particular platform (including
 * DSO_METH_null() if necessary).
 */
DSO_METHOD *DSO_METHOD_openssl(void);

/*
 * This method is defined for all platforms - if a platform has no DSO
 * support then this will be the only method!
 */
DSO_METHOD *DSO_METHOD_null(void);

/*
 * If DSO_DLFCN is defined, the standard dlfcn.h-style functions (dlopen,
 * dlclose, dlsym, etc) will be used and incorporated into this method. If
 * not, this method will return NULL.
 */
DSO_METHOD *DSO_METHOD_dlfcn(void);

/*
 * If DSO_DL is defined, the standard dl.h-style functions (shl_load,
 * shl_unload, shl_findsym, etc) will be used and incorporated into this
 * method. If not, this method will return NULL.
 */
DSO_METHOD *DSO_METHOD_dl(void);

/* If WIN32 is defined, use DLLs. If not, return NULL. */
DSO_METHOD *DSO_METHOD_win32(void);

/* If VMS is defined, use shared images. If not, return NULL. */
DSO_METHOD *DSO_METHOD_vms(void);

/*
 * This function writes null-terminated pathname of DSO module containing
 * 'addr' into 'sz' large caller-provided 'path' and returns the number of
 * characters [including trailing zero] written to it. If 'sz' is 0 or
 * negative, 'path' is ignored and required amount of charachers [including
 * trailing zero] to accomodate pathname is returned. If 'addr' is NULL, then
 * pathname of cryptolib itself is returned. Negative or zero return value
 * denotes error.
 */
int DSO_pathbyaddr(void *addr, char *path, int sz);

/*
 * This function should be used with caution! It looks up symbols in *all*
 * loaded modules and if module gets unloaded by somebody else attempt to
 * dereference the pointer is doomed to have fatal consequences. Primary
 * usage for this function is to probe *core* system functionality, e.g.
 * check if getnameinfo(3) is available at run-time without bothering about
 * OS-specific details such as libc.so.versioning or where does it actually
 * reside: in libc itself or libsocket.
 */
void *DSO_global_lookup(const char *name);

/* If BeOS is defined, use shared images. If not, return NULL. */
DSO_METHOD *DSO_METHOD_beos(void);

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_DSO_strings(void);

/* Error codes for the DSO functions. */

/* Function codes. */
# define DSO_F_BEOS_BIND_FUNC                             144
# define DSO_F_BEOS_BIND_VAR                              145
# define DSO_F_BEOS_LOAD                                  146
# define DSO_F_BEOS_NAME_CONVERTER                        147
# define DSO_F_BEOS_UNLOAD                                148
# define DSO_F_DLFCN_BIND_FUNC                            100
# define DSO_F_DLFCN_BIND_VAR                             101
# define DSO_F_DLFCN_LOAD                                 102
# define DSO_F_DLFCN_MERGER                               130
# define DSO_F_DLFCN_NAME_CONVERTER                       123
# define DSO_F_DLFCN_UNLOAD                               103
# define DSO_F_DL_BIND_FUNC                               104
# define DSO_F_DL_BIND_VAR                                105
# define DSO_F_DL_LOAD                                    106
# define DSO_F_DL_MERGER                                  131
# define DSO_F_DL_NAME_CONVERTER                          124
# define DSO_F_DL_UNLOAD                                  107
# define DSO_F_DSO_BIND_FUNC                              108
# define DSO_F_DSO_BIND_VAR                               109
# define DSO_F_DSO_CONVERT_FILENAME                       126
# define DSO_F_DSO_CTRL                                   110
# define DSO_F_DSO_FREE                                   111
# define DSO_F_DSO_GET_FILENAME                           127
# define DSO_F_DSO_GET_LOADED_FILENAME                    128
# define DSO_F_DSO_GLOBAL_LOOKUP                          139
# define DSO_F_DSO_LOAD                                   112
# define DSO_F_DSO_MERGE                                  132
# define DSO_F_DSO_NEW_METHOD                             113
# define DSO_F_DSO_PATHBYADDR                             140
# define DSO_F_DSO_SET_FILENAME                           129
# define DSO_F_DSO_SET_NAME_CONVERTER                     122
# define DSO_F_DSO_UP_REF                                 114
# define DSO_F_GLOBAL_LOOKUP_FUNC                         138
# define DSO_F_PATHBYADDR                                 137
# define DSO_F_VMS_BIND_SYM                               115
# define DSO_F_VMS_LOAD                                   116
# define DSO_F_VMS_MERGER                                 133
# define DSO_F_VMS_UNLOAD                                 117
# define DSO_F_WIN32_BIND_FUNC                            118
# define DSO_F_WIN32_BIND_VAR                             119
# define DSO_F_WIN32_GLOBALLOOKUP                         142
# define DSO_F_WIN32_GLOBALLOOKUP_FUNC                    143
# define DSO_F_WIN32_JOINER                               135
# define DSO_F_WIN32_LOAD                                 120
# define DSO_F_WIN32_MERGER                               134
# define DSO_F_WIN32_NAME_CONVERTER                       125
# define DSO_F_WIN32_PATHBYADDR                           141
# define DSO_F_WIN32_SPLITTER                             136
# define DSO_F_WIN32_UNLOAD                               121

/* Reason codes. */
# define DSO_R_CTRL_FAILED                                100
# define DSO_R_DSO_ALREADY_LOADED                         110
# define DSO_R_EMPTY_FILE_STRUCTURE                       113
# define DSO_R_FAILURE                                    114
# define DSO_R_FILENAME_TOO_BIG                           101
# define DSO_R_FINISH_FAILED                              102
# define DSO_R_INCORRECT_FILE_SYNTAX                      115
# define DSO_R_LOAD_FAILED                                103
# define DSO_R_NAME_TRANSLATION_FAILED                    109
# define DSO_R_NO_FILENAME                                111
# define DSO_R_NO_FILE_SPECIFICATION                      116
# define DSO_R_NULL_HANDLE                                104
# define DSO_R_SET_FILENAME_FAILED                        112
# define DSO_R_STACK_ERROR                                105
# define DSO_R_SYM_FAILURE                                106
# define DSO_R_UNLOAD_FAILED                              107
# define DSO_R_UNSUPPORTED                                108

#ifdef  __cplusplus
}
#endif
#endif

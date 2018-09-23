/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
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
 * typical for the platform using the dso_name_converter function of the
 * method. Eg. win32 will transform "blah" into "blah.dll", and dlfcn will
 * transform it into "libblah.so". This callback could even utilise the
 * DSO_METHOD's converter too if it only wants to override behaviour for
 * one or two possible DSO methods. However, the following flag can be
 * set in a DSO to prevent *any* native name-translation at all - eg. if
 * the caller has prompted the user for a path to a driver library so the
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
 * Don't unload the DSO when we call DSO_free()
 */
# define DSO_FLAG_NO_UNLOAD_ON_FREE              0x04
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
typedef struct dso_meth_st DSO_METHOD;

/*
 * The function prototype used for method functions (or caller-provided
 * callbacks) that transform filenames. They are passed a DSO structure
 * pointer (or NULL if they are to be used independently of a DSO object) and
 * a filename to transform. They should either return NULL (if there is an
 * error condition) or a newly allocated string containing the transformed
 * form that the caller will need to free with OPENSSL_free() when done.
 */
typedef char *(*DSO_NAME_CONVERTER_FUNC)(DSO *, const char *);
/*
 * The function prototype used for method functions (or caller-provided
 * callbacks) that merge two file specifications. They are passed a DSO
 * structure pointer (or NULL if they are to be used independently of a DSO
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

DSO *DSO_new(void);
int DSO_free(DSO *dso);
int DSO_flags(DSO *dso);
int DSO_up_ref(DSO *dso);
long DSO_ctrl(DSO *dso, int cmd, long larg, void *parg);

/*
 * These functions can be used to get/set the platform-independent filename
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
 * The all-singing all-dancing load function, you normally pass NULL for the
 * first and third parameters. Use DSO_up_ref and DSO_free for subsequent
 * reference count handling. Any flags passed in will be set in the
 * constructed DSO after its init() function but before the load operation.
 * If 'dso' is non-NULL, 'flags' is ignored.
 */
DSO *DSO_load(DSO *dso, const char *filename, DSO_METHOD *meth, int flags);

/* This function binds to a function inside a shared library. */
DSO_FUNC_TYPE DSO_bind_func(DSO *dso, const char *symname);

/*
 * This method is the default, but will beg, borrow, or steal whatever method
 * should be the default on any particular platform (including
 * DSO_METH_null() if necessary).
 */
DSO_METHOD *DSO_METHOD_openssl(void);

/*
 * This function writes null-terminated pathname of DSO module containing
 * 'addr' into 'sz' large caller-provided 'path' and returns the number of
 * characters [including trailing zero] written to it. If 'sz' is 0 or
 * negative, 'path' is ignored and required amount of characters [including
 * trailing zero] to accommodate pathname is returned. If 'addr' is NULL, then
 * pathname of cryptolib itself is returned. Negative or zero return value
 * denotes error.
 */
int DSO_pathbyaddr(void *addr, char *path, int sz);

/*
 * Like DSO_pathbyaddr() but instead returns a handle to the DSO for the symbol
 * or NULL on error.
 */
DSO *DSO_dsobyaddr(void *addr, int flags);

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

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */

int ERR_load_DSO_strings(void);

/* Error codes for the DSO functions. */

/* Function codes. */
# define DSO_F_DLFCN_BIND_FUNC                            100
# define DSO_F_DLFCN_LOAD                                 102
# define DSO_F_DLFCN_MERGER                               130
# define DSO_F_DLFCN_NAME_CONVERTER                       123
# define DSO_F_DLFCN_UNLOAD                               103
# define DSO_F_DL_BIND_FUNC                               104
# define DSO_F_DL_LOAD                                    106
# define DSO_F_DL_MERGER                                  131
# define DSO_F_DL_NAME_CONVERTER                          124
# define DSO_F_DL_UNLOAD                                  107
# define DSO_F_DSO_BIND_FUNC                              108
# define DSO_F_DSO_CONVERT_FILENAME                       126
# define DSO_F_DSO_CTRL                                   110
# define DSO_F_DSO_FREE                                   111
# define DSO_F_DSO_GET_FILENAME                           127
# define DSO_F_DSO_GLOBAL_LOOKUP                          139
# define DSO_F_DSO_LOAD                                   112
# define DSO_F_DSO_MERGE                                  132
# define DSO_F_DSO_NEW_METHOD                             113
# define DSO_F_DSO_PATHBYADDR                             105
# define DSO_F_DSO_SET_FILENAME                           129
# define DSO_F_DSO_UP_REF                                 114
# define DSO_F_VMS_BIND_SYM                               115
# define DSO_F_VMS_LOAD                                   116
# define DSO_F_VMS_MERGER                                 133
# define DSO_F_VMS_UNLOAD                                 117
# define DSO_F_WIN32_BIND_FUNC                            101
# define DSO_F_WIN32_GLOBALLOOKUP                         142
# define DSO_F_WIN32_JOINER                               135
# define DSO_F_WIN32_LOAD                                 120
# define DSO_F_WIN32_MERGER                               134
# define DSO_F_WIN32_NAME_CONVERTER                       125
# define DSO_F_WIN32_PATHBYADDR                           109
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
# define DSO_R_NULL_HANDLE                                104
# define DSO_R_SET_FILENAME_FAILED                        112
# define DSO_R_STACK_ERROR                                105
# define DSO_R_SYM_FAILURE                                106
# define DSO_R_UNLOAD_FAILED                              107
# define DSO_R_UNSUPPORTED                                108

# ifdef  __cplusplus
}
# endif
#endif

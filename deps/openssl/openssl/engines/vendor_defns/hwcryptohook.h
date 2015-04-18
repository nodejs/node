/*-
 * ModExp / RSA (with/without KM) plugin API
 *
 * The application will load a dynamic library which
 * exports entrypoint(s) defined in this file.
 *
 * This set of entrypoints provides only a multithreaded,
 * synchronous-within-each-thread, facility.
 *
 *
 * This file is Copyright 1998-2000 nCipher Corporation Limited.
 *
 * Redistribution and use in source and binary forms, with opr without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions, and the following
 *    disclaimer, in the documentation and/or other materials provided
 *    with the distribution
 *
 * IN NO EVENT SHALL NCIPHER CORPORATION LIMITED (`NCIPHER') AND/OR
 * ANY OTHER AUTHORS OR DISTRIBUTORS OF THIS FILE BE LIABLE for any
 * damages arising directly or indirectly from this file, its use or
 * this licence.  Without prejudice to the generality of the
 * foregoing: all liability shall be excluded for direct, indirect,
 * special, incidental, consequential or other damages or any loss of
 * profits, business, revenue goodwill or anticipated savings;
 * liability shall be excluded even if nCipher or anyone else has been
 * advised of the possibility of damage.  In any event, if the
 * exclusion of liability is not effective, the liability of nCipher
 * or any author or distributor shall be limited to the lesser of the
 * price paid and 1,000 pounds sterling. This licence only fails to
 * exclude or limit liability for death or personal injury arising out
 * of negligence, and only to the extent that such an exclusion or
 * limitation is not effective.
 *
 * NCIPHER AND THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ALL
 * AND ANY WARRANTIES (WHETHER EXPRESS OR IMPLIED), including, but not
 * limited to, any implied warranties of merchantability, fitness for
 * a particular purpose, satisfactory quality, and/or non-infringement
 * of any third party rights.
 *
 * US Government use: This software and documentation is Commercial
 * Computer Software and Computer Software Documentation, as defined in
 * sub-paragraphs (a)(1) and (a)(5) of DFAR 252.227-7014, "Rights in
 * Noncommercial Computer Software and Noncommercial Computer Software
 * Documentation."  Use, duplication or disclosure by the Government is
 * subject to the terms and conditions specified here.
 *
 * By using or distributing this file you will be accepting these
 * terms and conditions, including the limitation of liability and
 * lack of warranty.  If you do not wish to accept these terms and
 * conditions, DO NOT USE THE FILE.
 *
 *
 * The actual dynamically loadable plugin, and the library files for
 * static linking, which are also provided in some distributions, are
 * not covered by the licence described above.  You should have
 * received a separate licence with terms and conditions for these
 * library files; if you received the library files without a licence,
 * please contact nCipher.
 *
 *
 * $Id: hwcryptohook.h,v 1.1 2002/10/11 17:10:59 levitte Exp $
 */

#ifndef HWCRYPTOHOOK_H
# define HWCRYPTOHOOK_H

# include <sys/types.h>
# include <stdio.h>

# ifndef HWCRYPTOHOOK_DECLARE_APPTYPES
#  define HWCRYPTOHOOK_DECLARE_APPTYPES 1
# endif

# define HWCRYPTOHOOK_ERROR_FAILED   -1
# define HWCRYPTOHOOK_ERROR_FALLBACK -2
# define HWCRYPTOHOOK_ERROR_MPISIZE  -3

# if HWCRYPTOHOOK_DECLARE_APPTYPES

/*-
 * These structs are defined by the application and opaque to the
 * crypto plugin.  The application may define these as it sees fit.
 * Default declarations are provided here, but the application may
 *  #define HWCRYPTOHOOK_DECLARE_APPTYPES 0
 * to prevent these declarations, and instead provide its own
 * declarations of these types.  (Pointers to them must still be
 * ordinary pointers to structs or unions, or the resulting combined
 * program will have a type inconsistency.)
 */
typedef struct HWCryptoHook_MutexValue HWCryptoHook_Mutex;
typedef struct HWCryptoHook_CondVarValue HWCryptoHook_CondVar;
typedef struct HWCryptoHook_PassphraseContextValue
 HWCryptoHook_PassphraseContext;
typedef struct HWCryptoHook_CallerContextValue HWCryptoHook_CallerContext;

# endif                         /* HWCRYPTOHOOK_DECLARE_APPTYPES */

/*-
 * These next two structs are opaque to the application.  The crypto
 * plugin will return pointers to them; the caller simply manipulates
 * the pointers.
 */
typedef struct HWCryptoHook_Context *HWCryptoHook_ContextHandle;
typedef struct HWCryptoHook_RSAKey *HWCryptoHook_RSAKeyHandle;

typedef struct {
    char *buf;
    size_t size;
} HWCryptoHook_ErrMsgBuf;
/*-
 * Used for error reporting.  When a HWCryptoHook function fails it
 * will return a sentinel value (0 for pointer-valued functions, or a
 * negative number, usually HWCRYPTOHOOK_ERROR_FAILED, for
 * integer-valued ones).  It will, if an ErrMsgBuf is passed, also put
 * an error message there.
 *
 * size is the size of the buffer, and will not be modified.  If you
 * pass 0 for size you must pass 0 for buf, and nothing will be
 * recorded (just as if you passed 0 for the struct pointer).
 * Messages written to the buffer will always be null-terminated, even
 * when truncated to fit within size bytes.
 *
 * The contents of the buffer are not defined if there is no error.
 */

typedef struct HWCryptoHook_MPIStruct {
    unsigned char *buf;
    size_t size;
} HWCryptoHook_MPI;
/*-
 * When one of these is returned, a pointer is passed to the function.
 * At call, size is the space available.  Afterwards it is updated to
 * be set to the actual length (which may be more than the space available,
 * if there was not enough room and the result was truncated).
 * buf (the pointer) is not updated.
 *
 * size is in bytes and may be zero at call or return, but must be a
 * multiple of the limb size.  Zero limbs at the MS end are not
 * permitted.
 */

# define HWCryptoHook_InitFlags_FallbackModExp    0x0002UL
# define HWCryptoHook_InitFlags_FallbackRSAImmed  0x0004UL
/*-
 * Enable requesting fallback to software in case of problems with the
 * hardware support.  This indicates to the crypto provider that the
 * application is prepared to fall back to software operation if the
 * ModExp* or RSAImmed* functions return HWCRYPTOHOOK_ERROR_FALLBACK.
 * Without this flag those calls will never return
 * HWCRYPTOHOOK_ERROR_FALLBACK.  The flag will also cause the crypto
 * provider to avoid repeatedly attempting to contact dead hardware
 * within a short interval, if appropriate.
 */

# define HWCryptoHook_InitFlags_SimpleForkCheck   0x0010UL
/*-
 * Without _SimpleForkCheck the library is allowed to assume that the
 * application will not fork and call the library in the child(ren).
 *
 * When it is specified, this is allowed.  However, after a fork
 * neither parent nor child may unload any loaded keys or call
 * _Finish.  Instead, they should call exit (or die with a signal)
 * without calling _Finish.  After all the children have died the
 * parent may unload keys or call _Finish.
 *
 * This flag only has any effect on UN*X platforms.
 */

typedef struct {
    unsigned long flags;
    void *logstream;            /* usually a FILE*.  See below. */
    size_t limbsize;            /* bignum format - size of radix type, must
                                 * be power of 2 */
    int mslimbfirst;            /* 0 or 1 */
    int msbytefirst;            /* 0 or 1; -1 = native */
  /*-
   * All the callback functions should return 0 on success, or a
   * nonzero integer (whose value will be visible in the error message
   * put in the buffer passed to the call).
   *
   * If a callback is not available pass a null function pointer.
   *
   * The callbacks may not call down again into the crypto plugin.
   */
  /*-
   * For thread-safety.  Set everything to 0 if you promise only to be
   * singlethreaded.  maxsimultaneous is the number of calls to
   * ModExp[Crt]/RSAImmed{Priv,Pub}/RSA.  If you don't know what to
   * put there then say 0 and the hook library will use a default.
   *
   * maxmutexes is a small limit on the number of simultaneous mutexes
   * which will be requested by the library.  If there is no small
   * limit, set it to 0.  If the crypto plugin cannot create the
   * advertised number of mutexes the calls to its functions may fail.
   * If a low number of mutexes is advertised the plugin will try to
   * do the best it can.  Making larger numbers of mutexes available
   * may improve performance and parallelism by reducing contention
   * over critical sections.  Unavailability of any mutexes, implying
   * single-threaded operation, should be indicated by the setting
   * mutex_init et al to 0.
   */
    int maxmutexes;
    int maxsimultaneous;
    size_t mutexsize;
    int (*mutex_init) (HWCryptoHook_Mutex *,
                       HWCryptoHook_CallerContext * cactx);
    int (*mutex_acquire) (HWCryptoHook_Mutex *);
    void (*mutex_release) (HWCryptoHook_Mutex *);
    void (*mutex_destroy) (HWCryptoHook_Mutex *);
   /*-
    * For greater efficiency, can use condition vars internally for
    * synchronisation.  In this case maxsimultaneous is ignored, but
    * the other mutex stuff must be available.  In singlethreaded
    * programs, set everything to 0.
    */
    size_t condvarsize;
    int (*condvar_init) (HWCryptoHook_CondVar *,
                         HWCryptoHook_CallerContext * cactx);
    int (*condvar_wait) (HWCryptoHook_CondVar *, HWCryptoHook_Mutex *);
    void (*condvar_signal) (HWCryptoHook_CondVar *);
    void (*condvar_broadcast) (HWCryptoHook_CondVar *);
    void (*condvar_destroy) (HWCryptoHook_CondVar *);
   /*-
    * The semantics of acquiring and releasing mutexes and broadcasting
    * and waiting on condition variables are expected to be those from
    * POSIX threads (pthreads).  The mutexes may be (in pthread-speak)
    * fast mutexes, recursive mutexes, or nonrecursive ones.
    *
    * The _release/_signal/_broadcast and _destroy functions must
    * always succeed when given a valid argument; if they are given an
    * invalid argument then the program (crypto plugin + application)
    * has an internal error, and they should abort the program.
    */
    int (*getpassphrase) (const char *prompt_info,
                          int *len_io, char *buf,
                          HWCryptoHook_PassphraseContext * ppctx,
                          HWCryptoHook_CallerContext * cactx);
   /*-
    * Passphrases and the prompt_info, if they contain high-bit-set
    * characters, are UTF-8.  The prompt_info may be a null pointer if
    * no prompt information is available (it should not be an empty
    * string).  It will not contain text like `enter passphrase';
    * instead it might say something like `Operator Card for John
    * Smith' or `SmartCard in nFast Module #1, Slot #1'.
    *
    * buf points to a buffer in which to return the passphrase; on
    * entry *len_io is the length of the buffer.  It should be updated
    * by the callback.  The returned passphrase should not be
    * null-terminated by the callback.
    */
    int (*getphystoken) (const char *prompt_info,
                         const char *wrong_info,
                         HWCryptoHook_PassphraseContext * ppctx,
                         HWCryptoHook_CallerContext * cactx);
   /*-
    * Requests that the human user physically insert a different
    * smartcard, DataKey, etc.  The plugin should check whether the
    * currently inserted token(s) are appropriate, and if they are it
    * should not make this call.
    *
    * prompt_info is as before.  wrong_info is a description of the
    * currently inserted token(s) so that the user is told what
    * something is.  wrong_info, like prompt_info, may be null, but
    * should not be an empty string.  Its contents should be
    * syntactically similar to that of prompt_info.
    */
   /*-
    * Note that a single LoadKey operation might cause several calls to
    * getpassphrase and/or requestphystoken.  If requestphystoken is
    * not provided (ie, a null pointer is passed) then the plugin may
    * not support loading keys for which authorisation by several cards
    * is required.  If getpassphrase is not provided then cards with
    * passphrases may not be supported.
    *
    * getpassphrase and getphystoken do not need to check that the
    * passphrase has been entered correctly or the correct token
    * inserted; the crypto plugin will do that.  If this is not the
    * case then the crypto plugin is responsible for calling these
    * routines again as appropriate until the correct token(s) and
    * passphrase(s) are supplied as required, or until any retry limits
    * implemented by the crypto plugin are reached.
    *
    * In either case, the application must allow the user to say `no'
    * or `cancel' to indicate that they do not know the passphrase or
    * have the appropriate token; this should cause the callback to
    * return nonzero indicating error.
    */
    void (*logmessage) (void *logstream, const char *message);
   /*-
    * A log message will be generated at least every time something goes
    * wrong and an ErrMsgBuf is filled in (or would be if one was
    * provided).  Other diagnostic information may be written there too,
    * including more detailed reasons for errors which are reported in an
    * ErrMsgBuf.
    *
    * When a log message is generated, this callback is called.  It
    * should write a message to the relevant logging arrangements.
    *
    * The message string passed will be null-terminated and may be of arbitrary
    * length.  It will not be prefixed by the time and date, nor by the
    * name of the library that is generating it - if this is required,
    * the logmessage callback must do it.  The message will not have a
    * trailing newline (though it may contain internal newlines).
    *
    * If a null pointer is passed for logmessage a default function is
    * used.  The default function treats logstream as a FILE* which has
    * been converted to a void*.  If logstream is 0 it does nothing.
    * Otherwise it prepends the date and time and library name and
    * writes the message to logstream.  Each line will be prefixed by a
    * descriptive string containing the date, time and identity of the
    * crypto plugin.  Errors on the logstream are not reported
    * anywhere, and the default function doesn't flush the stream, so
    * the application must set the buffering how it wants it.
    *
    * The crypto plugin may also provide a facility to have copies of
    * log messages sent elsewhere, and or for adjusting the verbosity
    * of the log messages; any such facilities will be configured by
    * external means.
    */
} HWCryptoHook_InitInfo;

typedef
HWCryptoHook_ContextHandle HWCryptoHook_Init_t(const HWCryptoHook_InitInfo *
                                               initinfo, size_t initinfosize,
                                               const HWCryptoHook_ErrMsgBuf *
                                               errors,
                                               HWCryptoHook_CallerContext *
                                               cactx);
extern HWCryptoHook_Init_t HWCryptoHook_Init;

/*-
 * Caller should set initinfosize to the size of the HWCryptoHook struct,
 * so it can be extended later.
 *
 * On success, a message for display or logging by the server,
 * including the name and version number of the plugin, will be filled
 * in into *errors; on failure *errors is used for error handling, as
 * usual.
 */

/*-
 * All these functions return 0 on success, HWCRYPTOHOOK_ERROR_FAILED
 * on most failures.  HWCRYPTOHOOK_ERROR_MPISIZE means at least one of
 * the output MPI buffer(s) was too small; the sizes of all have been
 * set to the desired size (and for those where the buffer was large
 * enough, the value may have been copied in), and no error message
 * has been recorded.
 *
 * You may pass 0 for the errors struct.  In any case, unless you set
 * _NoStderr at init time then messages may be reported to stderr.
 */

/*-
 * The RSAImmed* functions (and key managed RSA) only work with
 * modules which have an RSA patent licence - currently that means KM
 * units; the ModExp* ones work with all modules, so you need a patent
 * licence in the software in the US.  They are otherwise identical.
 */

typedef
void HWCryptoHook_Finish_t(HWCryptoHook_ContextHandle hwctx);
extern HWCryptoHook_Finish_t HWCryptoHook_Finish;
/* You must not have any calls going or keys loaded when you call this. */

typedef
int HWCryptoHook_RandomBytes_t(HWCryptoHook_ContextHandle hwctx,
                               unsigned char *buf, size_t len,
                               const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_RandomBytes_t HWCryptoHook_RandomBytes;

typedef
int HWCryptoHook_ModExp_t(HWCryptoHook_ContextHandle hwctx,
                          HWCryptoHook_MPI a,
                          HWCryptoHook_MPI p,
                          HWCryptoHook_MPI n,
                          HWCryptoHook_MPI * r,
                          const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_ModExp_t HWCryptoHook_ModExp;

typedef
int HWCryptoHook_RSAImmedPub_t(HWCryptoHook_ContextHandle hwctx,
                               HWCryptoHook_MPI m,
                               HWCryptoHook_MPI e,
                               HWCryptoHook_MPI n,
                               HWCryptoHook_MPI * r,
                               const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_RSAImmedPub_t HWCryptoHook_RSAImmedPub;

typedef
int HWCryptoHook_ModExpCRT_t(HWCryptoHook_ContextHandle hwctx,
                             HWCryptoHook_MPI a,
                             HWCryptoHook_MPI p,
                             HWCryptoHook_MPI q,
                             HWCryptoHook_MPI dmp1,
                             HWCryptoHook_MPI dmq1,
                             HWCryptoHook_MPI iqmp,
                             HWCryptoHook_MPI * r,
                             const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_ModExpCRT_t HWCryptoHook_ModExpCRT;

typedef
int HWCryptoHook_RSAImmedPriv_t(HWCryptoHook_ContextHandle hwctx,
                                HWCryptoHook_MPI m,
                                HWCryptoHook_MPI p,
                                HWCryptoHook_MPI q,
                                HWCryptoHook_MPI dmp1,
                                HWCryptoHook_MPI dmq1,
                                HWCryptoHook_MPI iqmp,
                                HWCryptoHook_MPI * r,
                                const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_RSAImmedPriv_t HWCryptoHook_RSAImmedPriv;

/*-
 * The RSAImmed* and ModExp* functions may return E_FAILED or
 * E_FALLBACK for failure.
 *
 * E_FAILED means the failure is permanent and definite and there
 *    should be no attempt to fall back to software.  (Eg, for some
 *    applications, which support only the acceleration-only
 *    functions, the `key material' may actually be an encoded key
 *    identifier, and doing the operation in software would give wrong
 *    answers.)
 *
 * E_FALLBACK means that doing the computation in software would seem
 *    reasonable.  If an application pays attention to this and is
 *    able to fall back, it should also set the Fallback init flags.
 */

typedef
int HWCryptoHook_RSALoadKey_t(HWCryptoHook_ContextHandle hwctx,
                              const char *key_ident,
                              HWCryptoHook_RSAKeyHandle * keyhandle_r,
                              const HWCryptoHook_ErrMsgBuf * errors,
                              HWCryptoHook_PassphraseContext * ppctx);
extern HWCryptoHook_RSALoadKey_t HWCryptoHook_RSALoadKey;
/*-
 * The key_ident is a null-terminated string configured by the
 * user via the application's usual configuration mechanisms.
 * It is provided to the user by the crypto provider's key management
 * system.  The user must be able to enter at least any string of between
 * 1 and 1023 characters inclusive, consisting of printable 7-bit
 * ASCII characters.  The provider should avoid using
 * any characters except alphanumerics and the punctuation
 * characters  _ - + . / @ ~  (the user is expected to be able
 * to enter these without quoting).  The string may be case-sensitive.
 * The application may allow the user to enter other NULL-terminated strings,
 * and the provider must cope (returning an error if the string is not
 * valid).
 *
 * If the key does not exist, no error is recorded and 0 is returned;
 * keyhandle_r will be set to 0 instead of to a key handle.
 */

typedef
int HWCryptoHook_RSAGetPublicKey_t(HWCryptoHook_RSAKeyHandle k,
                                   HWCryptoHook_MPI * n,
                                   HWCryptoHook_MPI * e,
                                   const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_RSAGetPublicKey_t HWCryptoHook_RSAGetPublicKey;
/*-
 * The crypto plugin will not store certificates.
 *
 * Although this function for acquiring the public key value is
 * provided, it is not the purpose of this API to deal fully with the
 * handling of the public key.
 *
 * It is expected that the crypto supplier's key generation program
 * will provide general facilities for producing X.509
 * self-certificates and certificate requests in PEM format.  These
 * will be given to the user so that they can configure them in the
 * application, send them to CAs, or whatever.
 *
 * In case this kind of certificate handling is not appropriate, the
 * crypto supplier's key generation program should be able to be
 * configured not to generate such a self-certificate or certificate
 * request.  Then the application will need to do all of this, and
 * will need to store and handle the public key and certificates
 * itself.
 */

typedef
int HWCryptoHook_RSAUnloadKey_t(HWCryptoHook_RSAKeyHandle k,
                                const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_RSAUnloadKey_t HWCryptoHook_RSAUnloadKey;
/* Might fail due to locking problems, or other serious internal problems. */

typedef
int HWCryptoHook_RSA_t(HWCryptoHook_MPI m,
                       HWCryptoHook_RSAKeyHandle k,
                       HWCryptoHook_MPI * r,
                       const HWCryptoHook_ErrMsgBuf * errors);
extern HWCryptoHook_RSA_t HWCryptoHook_RSA;
/* RSA private key operation (sign or decrypt) - raw, unpadded. */

#endif                          /* HWCRYPTOHOOK_H */

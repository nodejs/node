/**
 * \file platform.h
 *
 * \brief This file contains the definitions and functions of the
 *        Mbed TLS platform abstraction layer.
 *
 *        The platform abstraction layer removes the need for the library
 *        to directly link to standard C library functions or operating
 *        system services, making the library easier to port and embed.
 *        Application developers and users of the library can provide their own
 *        implementations of these functions, or implementations specific to
 *        their platform, which can be statically linked to the library or
 *        dynamically configured at runtime.
 *
 *        When all compilation options related to platform abstraction are
 *        disabled, this header just defines `mbedtls_xxx` function names
 *        as aliases to the standard `xxx` function.
 *
 *        Most modules in the library and example programs are expected to
 *        include this header.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_PLATFORM_H
#define MBEDTLS_PLATFORM_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_HAVE_TIME)
#include "mbedtls/platform_time.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \name SECTION: Module settings
 *
 * The configuration options you can set for this module are in this section.
 * Either change them in mbedtls_config.h or define them on the compiler command line.
 * \{
 */

/* The older Microsoft Windows common runtime provides non-conforming
 * implementations of some standard library functions, including snprintf
 * and vsnprintf. This affects MSVC and MinGW builds.
 */
#if defined(__MINGW32__) || (defined(_MSC_VER) && _MSC_VER <= 1900)
#define MBEDTLS_PLATFORM_HAS_NON_CONFORMING_SNPRINTF
#define MBEDTLS_PLATFORM_HAS_NON_CONFORMING_VSNPRINTF
#endif

#if !defined(MBEDTLS_PLATFORM_NO_STD_FUNCTIONS)
#include <stdio.h>
#include <stdlib.h>
#if defined(MBEDTLS_HAVE_TIME)
#include <time.h>
#endif
#if !defined(MBEDTLS_PLATFORM_STD_SNPRINTF)
#if defined(MBEDTLS_PLATFORM_HAS_NON_CONFORMING_SNPRINTF)
#define MBEDTLS_PLATFORM_STD_SNPRINTF   mbedtls_platform_win32_snprintf /**< The default \c snprintf function to use.  */
#else
#define MBEDTLS_PLATFORM_STD_SNPRINTF   snprintf /**< The default \c snprintf function to use.  */
#endif
#endif
#if !defined(MBEDTLS_PLATFORM_STD_VSNPRINTF)
#if defined(MBEDTLS_PLATFORM_HAS_NON_CONFORMING_VSNPRINTF)
#define MBEDTLS_PLATFORM_STD_VSNPRINTF   mbedtls_platform_win32_vsnprintf /**< The default \c vsnprintf function to use.  */
#else
#define MBEDTLS_PLATFORM_STD_VSNPRINTF   vsnprintf /**< The default \c vsnprintf function to use.  */
#endif
#endif
#if !defined(MBEDTLS_PLATFORM_STD_PRINTF)
#define MBEDTLS_PLATFORM_STD_PRINTF   printf /**< The default \c printf function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_FPRINTF)
#define MBEDTLS_PLATFORM_STD_FPRINTF fprintf /**< The default \c fprintf function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_CALLOC)
#define MBEDTLS_PLATFORM_STD_CALLOC   calloc /**< The default \c calloc function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_FREE)
#define MBEDTLS_PLATFORM_STD_FREE       free /**< The default \c free function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_SETBUF)
#define MBEDTLS_PLATFORM_STD_SETBUF   setbuf /**< The default \c setbuf function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_EXIT)
#define MBEDTLS_PLATFORM_STD_EXIT      exit /**< The default \c exit function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_TIME)
#define MBEDTLS_PLATFORM_STD_TIME       time    /**< The default \c time function to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_EXIT_SUCCESS)
#define MBEDTLS_PLATFORM_STD_EXIT_SUCCESS  EXIT_SUCCESS /**< The default exit value to use. */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_EXIT_FAILURE)
#define MBEDTLS_PLATFORM_STD_EXIT_FAILURE  EXIT_FAILURE /**< The default exit value to use. */
#endif
#if defined(MBEDTLS_FS_IO)
#if !defined(MBEDTLS_PLATFORM_STD_NV_SEED_READ)
#define MBEDTLS_PLATFORM_STD_NV_SEED_READ   mbedtls_platform_std_nv_seed_read
#endif
#if !defined(MBEDTLS_PLATFORM_STD_NV_SEED_WRITE)
#define MBEDTLS_PLATFORM_STD_NV_SEED_WRITE  mbedtls_platform_std_nv_seed_write
#endif
#if !defined(MBEDTLS_PLATFORM_STD_NV_SEED_FILE)
#define MBEDTLS_PLATFORM_STD_NV_SEED_FILE   "seedfile"
#endif
#endif /* MBEDTLS_FS_IO */
#else /* MBEDTLS_PLATFORM_NO_STD_FUNCTIONS */
#if defined(MBEDTLS_PLATFORM_STD_MEM_HDR)
#include MBEDTLS_PLATFORM_STD_MEM_HDR
#endif
#endif /* MBEDTLS_PLATFORM_NO_STD_FUNCTIONS */

/* Enable certain documented defines only when generating doxygen to avoid
 * an "unrecognized define" error. */
#if defined(__DOXYGEN__) && !defined(MBEDTLS_PLATFORM_STD_CALLOC)
#define MBEDTLS_PLATFORM_STD_CALLOC
#endif

#if defined(__DOXYGEN__) && !defined(MBEDTLS_PLATFORM_STD_FREE)
#define MBEDTLS_PLATFORM_STD_FREE
#endif

/** \} name SECTION: Module settings */

/*
 * The function pointers for calloc and free.
 * Please see MBEDTLS_PLATFORM_STD_CALLOC and MBEDTLS_PLATFORM_STD_FREE
 * in mbedtls_config.h for more information about behaviour and requirements.
 */
#if defined(MBEDTLS_PLATFORM_MEMORY)
#if defined(MBEDTLS_PLATFORM_FREE_MACRO) && \
    defined(MBEDTLS_PLATFORM_CALLOC_MACRO)
#undef mbedtls_free
#undef mbedtls_calloc
#define mbedtls_free       MBEDTLS_PLATFORM_FREE_MACRO
#define mbedtls_calloc     MBEDTLS_PLATFORM_CALLOC_MACRO
#else
/* For size_t */
#include <stddef.h>
extern void *mbedtls_calloc(size_t n, size_t size);
extern void mbedtls_free(void *ptr);

/**
 * \brief               This function dynamically sets the memory-management
 *                      functions used by the library, during runtime.
 *
 * \param calloc_func   The \c calloc function implementation.
 * \param free_func     The \c free function implementation.
 *
 * \return              \c 0.
 */
int mbedtls_platform_set_calloc_free(void *(*calloc_func)(size_t, size_t),
                                     void (*free_func)(void *));
#endif /* MBEDTLS_PLATFORM_FREE_MACRO && MBEDTLS_PLATFORM_CALLOC_MACRO */
#else /* !MBEDTLS_PLATFORM_MEMORY */
#undef mbedtls_free
#undef mbedtls_calloc
#define mbedtls_free       free
#define mbedtls_calloc     calloc
#endif /* MBEDTLS_PLATFORM_MEMORY && !MBEDTLS_PLATFORM_{FREE,CALLOC}_MACRO */

/*
 * The function pointers for fprintf
 */
#if defined(MBEDTLS_PLATFORM_FPRINTF_ALT)
/* We need FILE * */
#include <stdio.h>
extern int (*mbedtls_fprintf)(FILE *stream, const char *format, ...);

/**
 * \brief                This function dynamically configures the fprintf
 *                       function that is called when the
 *                       mbedtls_fprintf() function is invoked by the library.
 *
 * \param fprintf_func   The \c fprintf function implementation.
 *
 * \return               \c 0.
 */
int mbedtls_platform_set_fprintf(int (*fprintf_func)(FILE *stream, const char *,
                                                     ...));
#else
#undef mbedtls_fprintf
#if defined(MBEDTLS_PLATFORM_FPRINTF_MACRO)
#define mbedtls_fprintf    MBEDTLS_PLATFORM_FPRINTF_MACRO
#else
#define mbedtls_fprintf    fprintf
#endif /* MBEDTLS_PLATFORM_FPRINTF_MACRO */
#endif /* MBEDTLS_PLATFORM_FPRINTF_ALT */

/*
 * The function pointers for printf
 */
#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
extern int (*mbedtls_printf)(const char *format, ...);

/**
 * \brief               This function dynamically configures the snprintf
 *                      function that is called when the mbedtls_snprintf()
 *                      function is invoked by the library.
 *
 * \param printf_func   The \c printf function implementation.
 *
 * \return              \c 0 on success.
 */
int mbedtls_platform_set_printf(int (*printf_func)(const char *, ...));
#else /* !MBEDTLS_PLATFORM_PRINTF_ALT */
#undef mbedtls_printf
#if defined(MBEDTLS_PLATFORM_PRINTF_MACRO)
#define mbedtls_printf     MBEDTLS_PLATFORM_PRINTF_MACRO
#else
#define mbedtls_printf     printf
#endif /* MBEDTLS_PLATFORM_PRINTF_MACRO */
#endif /* MBEDTLS_PLATFORM_PRINTF_ALT */

/*
 * The function pointers for snprintf
 *
 * The snprintf implementation should conform to C99:
 * - it *must* always correctly zero-terminate the buffer
 *   (except when n == 0, then it must leave the buffer untouched)
 * - however it is acceptable to return -1 instead of the required length when
 *   the destination buffer is too short.
 */
#if defined(MBEDTLS_PLATFORM_HAS_NON_CONFORMING_SNPRINTF)
/* For Windows (inc. MSYS2), we provide our own fixed implementation */
int mbedtls_platform_win32_snprintf(char *s, size_t n, const char *fmt, ...);
#endif

#if defined(MBEDTLS_PLATFORM_SNPRINTF_ALT)
extern int (*mbedtls_snprintf)(char *s, size_t n, const char *format, ...);

/**
 * \brief                 This function allows configuring a custom
 *                        \c snprintf function pointer.
 *
 * \param snprintf_func   The \c snprintf function implementation.
 *
 * \return                \c 0 on success.
 */
int mbedtls_platform_set_snprintf(int (*snprintf_func)(char *s, size_t n,
                                                       const char *format, ...));
#else /* MBEDTLS_PLATFORM_SNPRINTF_ALT */
#undef mbedtls_snprintf
#if defined(MBEDTLS_PLATFORM_SNPRINTF_MACRO)
#define mbedtls_snprintf   MBEDTLS_PLATFORM_SNPRINTF_MACRO
#else
#define mbedtls_snprintf   MBEDTLS_PLATFORM_STD_SNPRINTF
#endif /* MBEDTLS_PLATFORM_SNPRINTF_MACRO */
#endif /* MBEDTLS_PLATFORM_SNPRINTF_ALT */

/*
 * The function pointers for vsnprintf
 *
 * The vsnprintf implementation should conform to C99:
 * - it *must* always correctly zero-terminate the buffer
 *   (except when n == 0, then it must leave the buffer untouched)
 * - however it is acceptable to return -1 instead of the required length when
 *   the destination buffer is too short.
 */
#if defined(MBEDTLS_PLATFORM_HAS_NON_CONFORMING_VSNPRINTF)
#include <stdarg.h>
/* For Older Windows (inc. MSYS2), we provide our own fixed implementation */
int mbedtls_platform_win32_vsnprintf(char *s, size_t n, const char *fmt, va_list arg);
#endif

#if defined(MBEDTLS_PLATFORM_VSNPRINTF_ALT)
#include <stdarg.h>
extern int (*mbedtls_vsnprintf)(char *s, size_t n, const char *format, va_list arg);

/**
 * \brief   Set your own snprintf function pointer
 *
 * \param   vsnprintf_func   The \c vsnprintf function implementation
 *
 * \return  \c 0
 */
int mbedtls_platform_set_vsnprintf(int (*vsnprintf_func)(char *s, size_t n,
                                                         const char *format, va_list arg));
#else /* MBEDTLS_PLATFORM_VSNPRINTF_ALT */
#undef mbedtls_vsnprintf
#if defined(MBEDTLS_PLATFORM_VSNPRINTF_MACRO)
#define mbedtls_vsnprintf   MBEDTLS_PLATFORM_VSNPRINTF_MACRO
#else
#define mbedtls_vsnprintf   vsnprintf
#endif /* MBEDTLS_PLATFORM_VSNPRINTF_MACRO */
#endif /* MBEDTLS_PLATFORM_VSNPRINTF_ALT */

/*
 * The function pointers for setbuf
 */
#if defined(MBEDTLS_PLATFORM_SETBUF_ALT)
#include <stdio.h>
/**
 * \brief                  Function pointer to call for `setbuf()` functionality
 *                         (changing the internal buffering on stdio calls).
 *
 * \note                   The library calls this function to disable
 *                         buffering when reading or writing sensitive data,
 *                         to avoid having extra copies of sensitive data
 *                         remaining in stdio buffers after the file is
 *                         closed. If this is not a concern, for example if
 *                         your platform's stdio doesn't have any buffering,
 *                         you can set mbedtls_setbuf to a function that
 *                         does nothing.
 *
 *                         The library always calls this function with
 *                         `buf` equal to `NULL`.
 */
extern void (*mbedtls_setbuf)(FILE *stream, char *buf);

/**
 * \brief                  Dynamically configure the function that is called
 *                         when the mbedtls_setbuf() function is called by the
 *                         library.
 *
 * \param   setbuf_func   The \c setbuf function implementation
 *
 * \return                 \c 0
 */
int mbedtls_platform_set_setbuf(void (*setbuf_func)(
                                    FILE *stream, char *buf));
#else
#undef mbedtls_setbuf
#if defined(MBEDTLS_PLATFORM_SETBUF_MACRO)
/**
 * \brief                  Macro defining the function for the library to
 *                         call for `setbuf` functionality (changing the
 *                         internal buffering on stdio calls).
 *
 * \note                   See extra comments on the mbedtls_setbuf() function
 *                         pointer above.
 *
 * \return                 \c 0 on success, negative on error.
 */
#define mbedtls_setbuf    MBEDTLS_PLATFORM_SETBUF_MACRO
#else
#define mbedtls_setbuf    setbuf
#endif /* MBEDTLS_PLATFORM_SETBUF_MACRO */
#endif /* MBEDTLS_PLATFORM_SETBUF_ALT */

/*
 * The function pointers for exit
 */
#if defined(MBEDTLS_PLATFORM_EXIT_ALT)
extern void (*mbedtls_exit)(int status);

/**
 * \brief             This function dynamically configures the exit
 *                    function that is called when the mbedtls_exit()
 *                    function is invoked by the library.
 *
 * \param exit_func   The \c exit function implementation.
 *
 * \return            \c 0 on success.
 */
int mbedtls_platform_set_exit(void (*exit_func)(int status));
#else
#undef mbedtls_exit
#if defined(MBEDTLS_PLATFORM_EXIT_MACRO)
#define mbedtls_exit   MBEDTLS_PLATFORM_EXIT_MACRO
#else
#define mbedtls_exit   exit
#endif /* MBEDTLS_PLATFORM_EXIT_MACRO */
#endif /* MBEDTLS_PLATFORM_EXIT_ALT */

/*
 * The default exit values
 */
#if defined(MBEDTLS_PLATFORM_STD_EXIT_SUCCESS)
#define MBEDTLS_EXIT_SUCCESS MBEDTLS_PLATFORM_STD_EXIT_SUCCESS
#else
#define MBEDTLS_EXIT_SUCCESS 0
#endif
#if defined(MBEDTLS_PLATFORM_STD_EXIT_FAILURE)
#define MBEDTLS_EXIT_FAILURE MBEDTLS_PLATFORM_STD_EXIT_FAILURE
#else
#define MBEDTLS_EXIT_FAILURE 1
#endif

/*
 * The function pointers for reading from and writing a seed file to
 * Non-Volatile storage (NV) in a platform-independent way
 *
 * Only enabled when the NV seed entropy source is enabled
 */
#if defined(MBEDTLS_ENTROPY_NV_SEED)
#if !defined(MBEDTLS_PLATFORM_NO_STD_FUNCTIONS) && defined(MBEDTLS_FS_IO)
/* Internal standard platform definitions */
int mbedtls_platform_std_nv_seed_read(unsigned char *buf, size_t buf_len);
int mbedtls_platform_std_nv_seed_write(unsigned char *buf, size_t buf_len);
#endif

#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
extern int (*mbedtls_nv_seed_read)(unsigned char *buf, size_t buf_len);
extern int (*mbedtls_nv_seed_write)(unsigned char *buf, size_t buf_len);

/**
 * \brief   This function allows configuring custom seed file writing and
 *          reading functions.
 *
 * \param   nv_seed_read_func   The seed reading function implementation.
 * \param   nv_seed_write_func  The seed writing function implementation.
 *
 * \return  \c 0 on success.
 */
int mbedtls_platform_set_nv_seed(
    int (*nv_seed_read_func)(unsigned char *buf, size_t buf_len),
    int (*nv_seed_write_func)(unsigned char *buf, size_t buf_len)
    );
#else
#undef mbedtls_nv_seed_read
#undef mbedtls_nv_seed_write
#if defined(MBEDTLS_PLATFORM_NV_SEED_READ_MACRO) && \
    defined(MBEDTLS_PLATFORM_NV_SEED_WRITE_MACRO)
#define mbedtls_nv_seed_read    MBEDTLS_PLATFORM_NV_SEED_READ_MACRO
#define mbedtls_nv_seed_write   MBEDTLS_PLATFORM_NV_SEED_WRITE_MACRO
#else
#define mbedtls_nv_seed_read    mbedtls_platform_std_nv_seed_read
#define mbedtls_nv_seed_write   mbedtls_platform_std_nv_seed_write
#endif
#endif /* MBEDTLS_PLATFORM_NV_SEED_ALT */
#endif /* MBEDTLS_ENTROPY_NV_SEED */

#if !defined(MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT)

/**
 * \brief   The platform context structure.
 *
 * \note    This structure may be used to assist platform-specific
 *          setup or teardown operations.
 */
typedef struct mbedtls_platform_context {
    char MBEDTLS_PRIVATE(dummy); /**< A placeholder member, as empty structs are not portable. */
}
mbedtls_platform_context;

#else
#include "platform_alt.h"
#endif /* !MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT */

/**
 * \brief   This function performs any platform-specific initialization
 *          operations.
 *
 * \note    This function should be called before any other library functions.
 *
 *          Its implementation is platform-specific, and unless
 *          platform-specific code is provided, it does nothing.
 *
 * \note    The usage and necessity of this function is dependent on the platform.
 *
 * \param   ctx     The platform context.
 *
 * \return  \c 0 on success.
 */
int mbedtls_platform_setup(mbedtls_platform_context *ctx);
/**
 * \brief   This function performs any platform teardown operations.
 *
 * \note    This function should be called after every other Mbed TLS module
 *          has been correctly freed using the appropriate free function.
 *
 *          Its implementation is platform-specific, and unless
 *          platform-specific code is provided, it does nothing.
 *
 * \note    The usage and necessity of this function is dependent on the platform.
 *
 * \param   ctx     The platform context.
 *
 */
void mbedtls_platform_teardown(mbedtls_platform_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* platform.h */

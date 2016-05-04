/*
**********************************************************************
*   Copyright (C) 1997-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File UMUTEX.H
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97  aliu        Creation.
*   04/07/99  srl         rewrite - C interface, multiple mutices
*   05/13/99  stephen     Changed to umutex (from cmutex)
******************************************************************************
*/

#ifndef UMUTEX_H
#define UMUTEX_H

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "putilimp.h"



// Forward Declarations. UMutex is not in the ICU namespace (yet) because
//                       there are some remaining references from plain C.
struct UMutex;
struct UConditionVar;

U_NAMESPACE_BEGIN
struct UInitOnce;
U_NAMESPACE_END

// Stringify macros, to allow #include of user supplied atomic & mutex files.
#define U_MUTEX_STR(s) #s
#define U_MUTEX_XSTR(s) U_MUTEX_STR(s)

/****************************************************************************
 *
 *   Low Level Atomic Operations.
 *      Compiler dependent. Not operating system dependent.
 *
 ****************************************************************************/
#if defined (U_USER_ATOMICS_H)
#include U_MUTEX_XSTR(U_USER_ATOMICS_H)

#elif U_HAVE_STD_ATOMICS

//  C++11 atomics are available.

#include <atomic>

U_NAMESPACE_BEGIN

typedef std::atomic<int32_t> u_atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) ATOMIC_VAR_INIT(val)

inline int32_t umtx_loadAcquire(u_atomic_int32_t &var) {
    return var.load(std::memory_order_acquire);
}

inline void umtx_storeRelease(u_atomic_int32_t &var, int32_t val) {
    var.store(val, std::memory_order_release);
}

inline int32_t umtx_atomic_inc(u_atomic_int32_t *var) {
    return var->fetch_add(1) + 1;
}

inline int32_t umtx_atomic_dec(u_atomic_int32_t *var) {
    return var->fetch_sub(1) - 1;
}
U_NAMESPACE_END

#elif U_PLATFORM_HAS_WIN32_API

// MSVC compiler. Reads and writes of volatile variables have
//                acquire and release memory semantics, respectively.
//                This is a Microsoft extension, not standard C++ behavior.
//
//   Update:      can't use this because of MinGW, built with gcc.
//                Original plan was to use gcc atomics for MinGW, but they
//                aren't supported, so we fold MinGW into this path.

# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# ifndef NOMINMAX
# define NOMINMAX
# endif
# include <windows.h>

U_NAMESPACE_BEGIN
typedef volatile LONG u_atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) val

inline int32_t umtx_loadAcquire(u_atomic_int32_t &var) {
    return InterlockedCompareExchange(&var, 0, 0);
}

inline void umtx_storeRelease(u_atomic_int32_t &var, int32_t val) {
    InterlockedExchange(&var, val);
}


inline int32_t umtx_atomic_inc(u_atomic_int32_t *var) {
    return InterlockedIncrement(var);
}

inline int32_t umtx_atomic_dec(u_atomic_int32_t *var) {
    return InterlockedDecrement(var);
}
U_NAMESPACE_END


#elif U_HAVE_CLANG_ATOMICS
/*
 *  Clang __c11 atomic built-ins
 */

U_NAMESPACE_BEGIN
typedef _Atomic(int32_t) u_atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) val

inline int32_t umtx_loadAcquire(u_atomic_int32_t &var) {
     return __c11_atomic_load(&var, __ATOMIC_ACQUIRE);
}

inline void umtx_storeRelease(u_atomic_int32_t &var, int32_t val) {
   return __c11_atomic_store(&var, val, __ATOMIC_RELEASE);
}

inline int32_t umtx_atomic_inc(u_atomic_int32_t *var) {
    return __c11_atomic_fetch_add(var, 1, __ATOMIC_SEQ_CST) + 1;
}

inline int32_t umtx_atomic_dec(u_atomic_int32_t *var) {
    return __c11_atomic_fetch_sub(var, 1, __ATOMIC_SEQ_CST) - 1;
}
U_NAMESPACE_END


#elif U_HAVE_GCC_ATOMICS
/*
 * gcc atomic ops. These are available on several other compilers as well.
 */

U_NAMESPACE_BEGIN
typedef int32_t u_atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) val

inline int32_t umtx_loadAcquire(u_atomic_int32_t &var) {
    int32_t val = var;
    __sync_synchronize();
    return val;
}

inline void umtx_storeRelease(u_atomic_int32_t &var, int32_t val) {
    __sync_synchronize();
    var = val;
}

inline int32_t umtx_atomic_inc(u_atomic_int32_t *p)  {
   return __sync_add_and_fetch(p, 1);
}

inline int32_t umtx_atomic_dec(u_atomic_int32_t *p)  {
   return __sync_sub_and_fetch(p, 1);
}
U_NAMESPACE_END

#else

/*
 * Unknown Platform. Use out-of-line functions, which in turn use mutexes.
 *                   Slow but correct.
 */

#define U_NO_PLATFORM_ATOMICS

U_NAMESPACE_BEGIN
typedef int32_t u_atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) val

U_COMMON_API int32_t U_EXPORT2 
umtx_loadAcquire(u_atomic_int32_t &var);

U_COMMON_API void U_EXPORT2 
umtx_storeRelease(u_atomic_int32_t &var, int32_t val);

U_COMMON_API int32_t U_EXPORT2 
umtx_atomic_inc(u_atomic_int32_t *p);

U_COMMON_API int32_t U_EXPORT2 
umtx_atomic_dec(u_atomic_int32_t *p);

U_NAMESPACE_END

#endif  /* Low Level Atomic Ops Platfrom Chain */



/*************************************************************************************************
 *
 *  UInitOnce Definitions.
 *     These are platform neutral.
 *
 *************************************************************************************************/

U_NAMESPACE_BEGIN

struct UInitOnce {
    u_atomic_int32_t   fState;
    UErrorCode       fErrCode;
    void reset() {fState = 0;};
    UBool isReset() {return umtx_loadAcquire(fState) == 0;};
// Note: isReset() is used by service registration code.
//                 Thread safety of this usage needs review.
};

#define U_INITONCE_INITIALIZER {ATOMIC_INT32_T_INITIALIZER(0), U_ZERO_ERROR}


U_COMMON_API UBool U_EXPORT2 umtx_initImplPreInit(UInitOnce &);
U_COMMON_API void  U_EXPORT2 umtx_initImplPostInit(UInitOnce &);

template<class T> void umtx_initOnce(UInitOnce &uio, T *obj, void (T::*fp)()) {
    if (umtx_loadAcquire(uio.fState) == 2) {
        return;
    }
    if (umtx_initImplPreInit(uio)) {
        (obj->*fp)();
        umtx_initImplPostInit(uio);
    }
}


// umtx_initOnce variant for plain functions, or static class functions.
//               No context parameter.
inline void umtx_initOnce(UInitOnce &uio, void (*fp)()) {
    if (umtx_loadAcquire(uio.fState) == 2) {
        return;
    }
    if (umtx_initImplPreInit(uio)) {
        (*fp)();
        umtx_initImplPostInit(uio);
    }
}

// umtx_initOnce variant for plain functions, or static class functions.
//               With ErrorCode, No context parameter.
inline void umtx_initOnce(UInitOnce &uio, void (*fp)(UErrorCode &), UErrorCode &errCode) {
    if (U_FAILURE(errCode)) {
        return;
    }
    if (umtx_loadAcquire(uio.fState) != 2 && umtx_initImplPreInit(uio)) {
        // We run the initialization.
        (*fp)(errCode);
        uio.fErrCode = errCode;
        umtx_initImplPostInit(uio);
    } else {
        // Someone else already ran the initialization.
        if (U_FAILURE(uio.fErrCode)) {
            errCode = uio.fErrCode;
        }
    }
}

// umtx_initOnce variant for plain functions, or static class functions,
//               with a context parameter.
template<class T> void umtx_initOnce(UInitOnce &uio, void (*fp)(T), T context) {
    if (umtx_loadAcquire(uio.fState) == 2) {
        return;
    }
    if (umtx_initImplPreInit(uio)) {
        (*fp)(context);
        umtx_initImplPostInit(uio);
    }
}

// umtx_initOnce variant for plain functions, or static class functions,
//               with a context parameter and an error code.
template<class T> void umtx_initOnce(UInitOnce &uio, void (*fp)(T, UErrorCode &), T context, UErrorCode &errCode) {
    if (U_FAILURE(errCode)) {
        return;
    }
    if (umtx_loadAcquire(uio.fState) != 2 && umtx_initImplPreInit(uio)) {
        // We run the initialization.
        (*fp)(context, errCode);
        uio.fErrCode = errCode;
        umtx_initImplPostInit(uio);
    } else {
        // Someone else already ran the initialization.
        if (U_FAILURE(uio.fErrCode)) {
            errCode = uio.fErrCode;
        }
    }
}

U_NAMESPACE_END



/*************************************************************************************************
 *
 *  Mutex Definitions. Platform Dependent, #if platform chain follows.
 *         TODO:  Add a C++11 version.
 *                Need to convert all mutex using files to C++ first.
 *
 *************************************************************************************************/

#if defined(U_USER_MUTEX_H)
// #inlcude "U_USER_MUTEX_H"
#include U_MUTEX_XSTR(U_USER_MUTEX_H)

#elif U_PLATFORM_HAS_WIN32_API

/* Windows Definitions.
 *    Windows comes first in the platform chain.
 *    Cygwin (and possibly others) have both WIN32 and POSIX APIs. Prefer Win32 in this case.
 */


/* For CRITICAL_SECTION */

/*
 *   Note: there is an earlier include of windows.h in this file, but it is in
 *         different conditionals.
 *         This one is needed if we are using C++11 for atomic ops, but
 *         win32 APIs for Critical Sections.
 */

# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# ifndef NOMINMAX
# define NOMINMAX
# endif
# include <windows.h>


typedef struct UMutex {
    icu::UInitOnce    fInitOnce;
    CRITICAL_SECTION  fCS;
} UMutex;

/* Initializer for a static UMUTEX. Deliberately contains no value for the
 *  CRITICAL_SECTION.
 */
#define U_MUTEX_INITIALIZER {U_INITONCE_INITIALIZER}

struct UConditionVar {
    HANDLE           fEntryGate;
    HANDLE           fExitGate;
    int32_t          fWaitCount;
};

#define U_CONDITION_INITIALIZER {NULL, NULL, 0}
    


#elif U_PLATFORM_IMPLEMENTS_POSIX

/*
 *  POSIX platform
 */

#include <pthread.h>

struct UMutex {
    pthread_mutex_t  fMutex;
};
typedef struct UMutex UMutex;
#define U_MUTEX_INITIALIZER  {PTHREAD_MUTEX_INITIALIZER}

struct UConditionVar {
    pthread_cond_t   fCondition;
};
#define U_CONDITION_INITIALIZER {PTHREAD_COND_INITIALIZER}

#else

/*
 *  Unknow platform type.
 *      This is an error condition. ICU requires mutexes.
 */

#error Unknown Platform.

#endif



/**************************************************************************************
 *
 *  Mutex Implementation function declaratations.
 *     Declarations are platform neutral.
 *     Implementations, in umutex.cpp, are platform specific.
 *
 ************************************************************************************/

/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_INTERNAL void U_EXPORT2 umtx_lock(UMutex* mutex);

/* Unlock a mutex.
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_INTERNAL void U_EXPORT2 umtx_unlock (UMutex* mutex);

/*
 * Wait on a condition variable.
 * The calling thread will unlock the mutex and wait on the condition variable.
 * The mutex must be locked by the calling thread when invoking this function.
 *
 * @param cond the condition variable to wait on.
 * @param mutex the associated mutex.
 */

U_INTERNAL void U_EXPORT2 umtx_condWait(UConditionVar *cond, UMutex *mutex);


/*
 * Broadcast wakeup of all threads waiting on a Condition.
 * The associated mutex must be locked by the calling thread when calling
 * this function; this is a temporary ICU restriction.
 * 
 * @param cond the condition variable.
 */
U_INTERNAL void U_EXPORT2 umtx_condBroadcast(UConditionVar *cond);

/*
 * Signal a condition variable, waking up one waiting thread.
 * CAUTION: Do not use. Place holder only. Not implemented for Windows.
 */
U_INTERNAL void U_EXPORT2 umtx_condSignal(UConditionVar *cond);

#endif /* UMUTEX_H */
/*eof*/

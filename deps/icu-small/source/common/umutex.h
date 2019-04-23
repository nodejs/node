// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
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

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "unicode/uobject.h"

#include "putilimp.h"

#if defined(U_USER_ATOMICS_H) || defined(U_USER_MUTEX_H)
// Support for including an alternate implementation of atomic & mutex operations has been withdrawn.
// See issue ICU-20185.
#error U_USER_ATOMICS and U_USER_MUTEX_H are not supported
#endif


// Export an explicit template instantiation of std::atomic<int32_t>.
// When building DLLs for Windows this is required as it is used as a data member of the exported SharedObject class.
// See digitlst.h, pluralaffix.h, datefmt.h, and others for similar examples.
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN && !defined(U_IN_DOXYGEN)
#if defined(__clang__) || defined(_MSC_VER)
  #if defined(__clang__)
    // Suppress the warning that the explicit instantiation after explicit specialization has no effect.
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Winstantiation-after-specialization"
  #endif
template struct U_COMMON_API std::atomic<int32_t>;
  #if defined(__clang__)
    #pragma clang diagnostic pop
  #endif
#elif defined(__GNUC__)
// For GCC this class is already exported/visible, so no need for U_COMMON_API.
template struct std::atomic<int32_t>;
#endif
#endif


U_NAMESPACE_BEGIN

/****************************************************************************
 *
 *   Low Level Atomic Operations, ICU wrappers for.
 *
 ****************************************************************************/

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


/*************************************************************************************************
 *
 *  UInitOnce Definitions.
 *
 *************************************************************************************************/

struct UInitOnce {
    u_atomic_int32_t   fState;
    UErrorCode       fErrCode;
    void reset() {fState = 0;}
    UBool isReset() {return umtx_loadAcquire(fState) == 0;}
// Note: isReset() is used by service registration code.
//                 Thread safety of this usage needs review.
};

#define U_INITONCE_INITIALIZER {ATOMIC_INT32_T_INITIALIZER(0), U_ZERO_ERROR}


U_COMMON_API UBool U_EXPORT2 umtx_initImplPreInit(UInitOnce &);
U_COMMON_API void  U_EXPORT2 umtx_initImplPostInit(UInitOnce &);

template<class T> void umtx_initOnce(UInitOnce &uio, T *obj, void (U_CALLCONV T::*fp)()) {
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
inline void umtx_initOnce(UInitOnce &uio, void (U_CALLCONV *fp)()) {
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
inline void umtx_initOnce(UInitOnce &uio, void (U_CALLCONV *fp)(UErrorCode &), UErrorCode &errCode) {
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
template<class T> void umtx_initOnce(UInitOnce &uio, void (U_CALLCONV *fp)(T), T context) {
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
template<class T> void umtx_initOnce(UInitOnce &uio, void (U_CALLCONV *fp)(T, UErrorCode &), T context, UErrorCode &errCode) {
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


/*************************************************************************************************
 *
 * ICU Mutex wrappers.  Originally wrapped operating system mutexes, giving the rest of ICU a
 * platform independent set of mutex operations.  Now vestigial, wrapping std::mutex only.
 * For internal ICU use only.
 *
 *************************************************************************************************/

struct UMutex : public icu::UMemory {
    UMutex() = default;
    ~UMutex() = default;
    UMutex(const UMutex &other) = delete;
    UMutex &operator =(const UMutex &other) = delete;

    std::mutex   fMutex = {};    // Note: struct - pubic members - because most access is from
    //                           //       plain C style functions (umtx_lock(), etc.)
};


struct UConditionVar : public icu::UMemory {
	U_COMMON_API UConditionVar();
	U_COMMON_API ~UConditionVar();
    UConditionVar(const UConditionVar &other) = delete;
    UConditionVar &operator =(const UConditionVar &other) = delete;

    std::condition_variable_any fCV;
};

#define U_MUTEX_INITIALIZER {}
#define U_CONDITION_INITIALIZER {}

// Implementation notes for UConditionVar:
//
// Use an out-of-line constructor to reduce problems with the ICU dependency checker.
// On Linux, the default constructor of std::condition_variable_any
// produces an in-line reference to global operator new(), which the
// dependency checker flags for any file that declares a UConditionVar. With
// an out-of-line constructor, the dependency is constrained to umutex.o
//
// Do not export (U_COMMON_API) the entire class, but only the constructor
// and destructor, to avoid Windows build problems with attempting to export the
// std::condition_variable_any.

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
 *
 * @param cond the condition variable.
 */
U_INTERNAL void U_EXPORT2 umtx_condBroadcast(UConditionVar *cond);

/*
 * Signal a condition variable, waking up one waiting thread.
 */
U_INTERNAL void U_EXPORT2 umtx_condSignal(UConditionVar *cond);


U_NAMESPACE_END

#endif /* UMUTEX_H */
/*eof*/

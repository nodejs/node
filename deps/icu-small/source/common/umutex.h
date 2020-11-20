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
#include <type_traits>

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
//
// Similar story for std::atomic<std::mutex *>, and the exported UMutex class.
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN && !defined(U_IN_DOXYGEN)
#if defined(__clang__) || defined(_MSC_VER)
  #if defined(__clang__)
    // Suppress the warning that the explicit instantiation after explicit specialization has no effect.
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Winstantiation-after-specialization"
  #endif
template struct U_COMMON_API std::atomic<int32_t>;
template struct U_COMMON_API std::atomic<std::mutex *>;
  #if defined(__clang__)
    #pragma clang diagnostic pop
  #endif
#elif defined(__GNUC__)
// For GCC this class is already exported/visible, so no need for U_COMMON_API.
template struct std::atomic<int32_t>;
template struct std::atomic<std::mutex *>;
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

// UMutex should be constexpr-constructible, so that no initialization code
// is run during startup.
// This works on all C++ libraries except MS VS before VS2019.
#if (defined(_CPPLIB_VER) && !defined(_MSVC_STL_VERSION)) || \
    (defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION < 142)
    // (VS std lib older than VS2017) || (VS std lib version < VS2019)
#   define UMUTEX_CONSTEXPR
#else
#   define UMUTEX_CONSTEXPR constexpr
#endif

/**
 * UMutex - ICU Mutex class.
 *
 * This is the preferred Mutex class for use within ICU implementation code.
 * It is a thin wrapper over C++ std::mutex, with these additions:
 *    - Static instances are safe, not triggering static construction or destruction,
 *      and the associated order of construction or destruction issues.
 *    - Plumbed into u_cleanup() for destructing the underlying std::mutex,
 *      which frees any OS level resources they may be holding.
 *
 * Limitations:
 *    - Static or global instances only. Cannot be heap allocated. Cannot appear as a
 *      member of another class.
 *    - No condition variables or other advanced features. If needed, you will need to use
 *      std::mutex and std::condition_variable directly. For an example, see unifiedcache.cpp
 *
 * Typical Usage:
 *    static UMutex myMutex;
 *
 *    {
 *       Mutex lock(myMutex);
 *       ...    // Do stuff that is protected by myMutex;
 *    }         // myMutex is released when lock goes out of scope.
 */

class U_COMMON_API UMutex {
public:
    UMUTEX_CONSTEXPR UMutex() {}
    ~UMutex() = default;

    UMutex(const UMutex &other) = delete;
    UMutex &operator =(const UMutex &other) = delete;
    void *operator new(size_t) = delete;

    // requirements for C++ BasicLockable, allows UMutex to work with std::lock_guard
    void lock() {
        std::mutex *m = fMutex.load(std::memory_order_acquire);
        if (m == nullptr) { m = getMutex(); }
        m->lock();
    }
    void unlock() { fMutex.load(std::memory_order_relaxed)->unlock(); }

    static void cleanup();

private:
    alignas(std::mutex) char fStorage[sizeof(std::mutex)] {};
    std::atomic<std::mutex *> fMutex { nullptr };

    /** All initialized UMutexes are kept in a linked list, so that they can be found,
     * and the underlying std::mutex destructed, by u_cleanup().
     */
    UMutex *fListLink { nullptr };
    static UMutex *gListHead;

    /** Out-of-line function to lazily initialize a UMutex on first use.
     * Initial fast check is inline, in lock().  The returned value may never
     * be nullptr.
     */
    std::mutex *getMutex();
};


/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_CAPI void U_EXPORT2 umtx_lock(UMutex* mutex);

/* Unlock a mutex.
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_CAPI void U_EXPORT2 umtx_unlock (UMutex* mutex);


U_NAMESPACE_END

#endif /* UMUTEX_H */
/*eof*/

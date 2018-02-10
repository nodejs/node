// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File umutex.cpp
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97    aliu        Creation.
*   04/07/99    srl         updated
*   05/13/99    stephen     Changed to umutex (from cmutex).
*   11/22/99    aliu        Make non-global mutex autoinitialize [j151]
******************************************************************************
*/

#include "umutex.h"

#include "unicode/utypes.h"
#include "uassert.h"
#include "cmemory.h"


// The ICU global mutex. Used when ICU implementation code passes NULL for the mutex pointer.
static UMutex   globalMutex = U_MUTEX_INITIALIZER;

/*
 * ICU Mutex wrappers.  Wrap operating system mutexes, giving the rest of ICU a
 * platform independent set of mutex operations.  For internal ICU use only.
 */

#if defined(U_USER_MUTEX_CPP)
// Build time user mutex hook: #include "U_USER_MUTEX_CPP"
#include U_MUTEX_XSTR(U_USER_MUTEX_CPP)

#elif U_PLATFORM_USES_ONLY_WIN32_API

#if defined U_NO_PLATFORM_ATOMICS
#error ICU on Win32 requires support for low level atomic operations.
// Visual Studio, gcc, clang are OK. Shouldn't get here.
#endif


// This function is called when a test of a UInitOnce::fState reveals that
//   initialization has not completed, that we either need to call the
//   function on this thread, or wait for some other thread to complete.
//
// The actual call to the init function is made inline by template code
//   that knows the C++ types involved. This function returns TRUE if
//   the caller needs to call the Init function.
//

U_NAMESPACE_BEGIN

U_COMMON_API UBool U_EXPORT2 umtx_initImplPreInit(UInitOnce &uio) {
    for (;;) {
        int32_t previousState = InterlockedCompareExchange(
            (LONG volatile *) // this is the type given in the API doc for this function.
                &uio.fState,  //  Destination
            1,            //  Exchange Value
            0);           //  Compare value

        if (previousState == 0) {
            return true;   // Caller will next call the init function.
                           // Current state == 1.
        } else if (previousState == 2) {
            // Another thread already completed the initialization.
            //   We can simply return FALSE, indicating no
            //   further action is needed by the caller.
            return FALSE;
        } else {
            // Another thread is currently running the initialization.
            // Wait until it completes.
            do {
                Sleep(1);
                previousState = umtx_loadAcquire(uio.fState);
            } while (previousState == 1);
        }
    }
}

// This function is called by the thread that ran an initialization function,
// just after completing the function.

U_COMMON_API void U_EXPORT2 umtx_initImplPostInit(UInitOnce &uio) {
    umtx_storeRelease(uio.fState, 2);
}

U_NAMESPACE_END

static void winMutexInit(CRITICAL_SECTION *cs) {
    InitializeCriticalSection(cs);
    return;
}

U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    CRITICAL_SECTION *cs = &mutex->fCS;
    umtx_initOnce(mutex->fInitOnce, winMutexInit, cs);
    EnterCriticalSection(cs);
}

U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    LeaveCriticalSection(&mutex->fCS);
}


U_CAPI void U_EXPORT2
umtx_condBroadcast(UConditionVar *condition) {
    // We require that the associated mutex be held by the caller,
    //  so access to fWaitCount is protected and safe. No other thread can
    //  call condWait() while we are here.
    if (condition->fWaitCount == 0) {
        return;
    }
    ResetEvent(condition->fExitGate);
    SetEvent(condition->fEntryGate);
}

U_CAPI void U_EXPORT2
umtx_condSignal(UConditionVar * /* condition */) {
    // Function not implemented. There is no immediate requirement from ICU to have it.
    // Once ICU drops support for Windows XP and Server 2003, ICU Condition Variables will be
    // changed to be thin wrappers on native Windows CONDITION_VARIABLEs, and this function
    // becomes trivial to provide.
    U_ASSERT(FALSE);
}

U_CAPI void U_EXPORT2
umtx_condWait(UConditionVar *condition, UMutex *mutex) {
    if (condition->fEntryGate == NULL) {
        // Note: because the associated mutex must be locked when calling
        //       wait, we know that there can not be multiple threads
        //       running here with the same condition variable.
        //       Meaning that lazy initialization is safe.
        U_ASSERT(condition->fExitGate == NULL);
        condition->fEntryGate = CreateEvent(NULL,   // Security Attributes
                                            TRUE,   // Manual Reset
                                            FALSE,  // Initially reset
                                            NULL);  // Name.
        U_ASSERT(condition->fEntryGate != NULL);
        condition->fExitGate = CreateEvent(NULL, TRUE, TRUE, NULL);
        U_ASSERT(condition->fExitGate != NULL);
    }

    condition->fWaitCount++;
    umtx_unlock(mutex);
    WaitForSingleObject(condition->fEntryGate, INFINITE);
    umtx_lock(mutex);
    condition->fWaitCount--;
    if (condition->fWaitCount == 0) {
        // All threads that were waiting at the entry gate have woken up
        // and moved through. Shut the entry gate and open the exit gate.
        ResetEvent(condition->fEntryGate);
        SetEvent(condition->fExitGate);
    } else {
        umtx_unlock(mutex);
        WaitForSingleObject(condition->fExitGate, INFINITE);
        umtx_lock(mutex);
    }
}


#elif U_PLATFORM_IMPLEMENTS_POSIX

//-------------------------------------------------------------------------------------------
//
//  POSIX specific definitions
//
//-------------------------------------------------------------------------------------------

# include <pthread.h>

// Each UMutex consists of a pthread_mutex_t.
// All are statically initialized and ready for use.
// There is no runtime mutex initialization code needed.

U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    int sysErr = pthread_mutex_lock(&mutex->fMutex);
    (void)sysErr;   // Suppress unused variable warnings.
    U_ASSERT(sysErr == 0);
}


U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    int sysErr = pthread_mutex_unlock(&mutex->fMutex);
    (void)sysErr;   // Suppress unused variable warnings.
    U_ASSERT(sysErr == 0);
}


U_CAPI void U_EXPORT2
umtx_condWait(UConditionVar *cond, UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    int sysErr = pthread_cond_wait(&cond->fCondition, &mutex->fMutex);
    (void)sysErr;
    U_ASSERT(sysErr == 0);
}

U_CAPI void U_EXPORT2
umtx_condBroadcast(UConditionVar *cond) {
    int sysErr = pthread_cond_broadcast(&cond->fCondition);
    (void)sysErr;
    U_ASSERT(sysErr == 0);
}

U_CAPI void U_EXPORT2
umtx_condSignal(UConditionVar *cond) {
    int sysErr = pthread_cond_signal(&cond->fCondition);
    (void)sysErr;
    U_ASSERT(sysErr == 0);
}



U_NAMESPACE_BEGIN

static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initCondition = PTHREAD_COND_INITIALIZER;


// This function is called when a test of a UInitOnce::fState reveals that
//   initialization has not completed, that we either need to call the
//   function on this thread, or wait for some other thread to complete.
//
// The actual call to the init function is made inline by template code
//   that knows the C++ types involved. This function returns TRUE if
//   the caller needs to call the Init function.
//
U_COMMON_API UBool U_EXPORT2
umtx_initImplPreInit(UInitOnce &uio) {
    pthread_mutex_lock(&initMutex);
    int32_t state = uio.fState;
    if (state == 0) {
        umtx_storeRelease(uio.fState, 1);
        pthread_mutex_unlock(&initMutex);
        return TRUE;   // Caller will next call the init function.
    } else {
        while (uio.fState == 1) {
            // Another thread is currently running the initialization.
            // Wait until it completes.
            pthread_cond_wait(&initCondition, &initMutex);
        }
        pthread_mutex_unlock(&initMutex);
        U_ASSERT(uio.fState == 2);
        return FALSE;
    }
}



// This function is called by the thread that ran an initialization function,
// just after completing the function.
//   Some threads may be waiting on the condition, requiring the broadcast wakeup.
//   Some threads may be racing to test the fState variable outside of the mutex,
//   requiring the use of store/release when changing its value.

U_COMMON_API void U_EXPORT2
umtx_initImplPostInit(UInitOnce &uio) {
    pthread_mutex_lock(&initMutex);
    umtx_storeRelease(uio.fState, 2);
    pthread_cond_broadcast(&initCondition);
    pthread_mutex_unlock(&initMutex);
}

U_NAMESPACE_END

// End of POSIX specific umutex implementation.

#else  // Platform #define chain.

#error Unknown Platform

#endif  // Platform #define chain.


//-------------------------------------------------------------------------------
//
//   Atomic Operations, out-of-line versions.
//                      These are conditional, only defined if better versions
//                      were not available for the platform.
//
//                      These versions are platform neutral.
//
//--------------------------------------------------------------------------------

#if defined U_NO_PLATFORM_ATOMICS
static UMutex   gIncDecMutex = U_MUTEX_INITIALIZER;

U_NAMESPACE_BEGIN

U_COMMON_API int32_t U_EXPORT2
umtx_atomic_inc(u_atomic_int32_t *p)  {
    int32_t retVal;
    umtx_lock(&gIncDecMutex);
    retVal = ++(*p);
    umtx_unlock(&gIncDecMutex);
    return retVal;
}


U_COMMON_API int32_t U_EXPORT2
umtx_atomic_dec(u_atomic_int32_t *p) {
    int32_t retVal;
    umtx_lock(&gIncDecMutex);
    retVal = --(*p);
    umtx_unlock(&gIncDecMutex);
    return retVal;
}

U_COMMON_API int32_t U_EXPORT2
umtx_loadAcquire(u_atomic_int32_t &var) {
    umtx_lock(&gIncDecMutex);
    int32_t val = var;
    umtx_unlock(&gIncDecMutex);
    return val;
}

U_COMMON_API void U_EXPORT2
umtx_storeRelease(u_atomic_int32_t &var, int32_t val) {
    umtx_lock(&gIncDecMutex);
    var = val;
    umtx_unlock(&gIncDecMutex);
}

U_NAMESPACE_END
#endif

//--------------------------------------------------------------------------
//
//  Deprecated functions for setting user mutexes.
//
//--------------------------------------------------------------------------

U_DEPRECATED void U_EXPORT2
u_setMutexFunctions(const void * /*context */, UMtxInitFn *, UMtxFn *,
                    UMtxFn *,  UMtxFn *, UErrorCode *status) {
    if (U_SUCCESS(*status)) {
        *status = U_UNSUPPORTED_ERROR;
    }
    return;
}



U_DEPRECATED void U_EXPORT2
u_setAtomicIncDecFunctions(const void * /*context */, UMtxAtomicFn *, UMtxAtomicFn *,
                           UErrorCode *status) {
    if (U_SUCCESS(*status)) {
        *status = U_UNSUPPORTED_ERROR;
    }
    return;
}

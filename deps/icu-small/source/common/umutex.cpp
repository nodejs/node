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

U_NAMESPACE_BEGIN


#if defined(U_USER_MUTEX_CPP)
// Support for including an alternate implementation of mutexes has been withdrawn.
// See issue ICU-20185.
#error U_USER_MUTEX_CPP not supported
#endif

/*************************************************************************************************
 *
 *  ICU Mutex wrappers.
 *
 *************************************************************************************************/

// The ICU global mutex. Used when ICU implementation code passes NULL for the mutex pointer.
static UMutex *globalMutex() {
    static UMutex m = U_MUTEX_INITIALIZER;
    return &m;
}

U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == nullptr) {
        mutex = globalMutex();
    }
    mutex->fMutex.lock();
}


U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == nullptr) {
        mutex = globalMutex();
    }
    mutex->fMutex.unlock();
}

UConditionVar::UConditionVar() : fCV() {
}

UConditionVar::~UConditionVar() {
}

U_CAPI void U_EXPORT2
umtx_condWait(UConditionVar *cond, UMutex *mutex) {
    if (mutex == nullptr) {
        mutex = globalMutex();
    }
    cond->fCV.wait(mutex->fMutex);
}


U_CAPI void U_EXPORT2
umtx_condBroadcast(UConditionVar *cond) {
    cond->fCV.notify_all();
}


U_CAPI void U_EXPORT2
umtx_condSignal(UConditionVar *cond) {
    cond->fCV.notify_one();
}


/*************************************************************************************************
 *
 *  UInitOnce Implementation
 *
 *************************************************************************************************/

static std::mutex &initMutex() {
    static std::mutex m;
    return m;
}

static std::condition_variable &initCondition() {
    static std::condition_variable cv;
    return cv;
}


// This function is called when a test of a UInitOnce::fState reveals that
//   initialization has not completed, that we either need to call the init
//   function on this thread, or wait for some other thread to complete.
//
// The actual call to the init function is made inline by template code
//   that knows the C++ types involved. This function returns true if
//   the caller needs to call the Init function.
//
U_COMMON_API UBool U_EXPORT2
umtx_initImplPreInit(UInitOnce &uio) {
    std::unique_lock<std::mutex> lock(initMutex());

    if (umtx_loadAcquire(uio.fState) == 0) {
        umtx_storeRelease(uio.fState, 1);
        return true;      // Caller will next call the init function.
    } else {
        while (umtx_loadAcquire(uio.fState) == 1) {
            // Another thread is currently running the initialization.
            // Wait until it completes.
            initCondition().wait(lock);
        }
        U_ASSERT(uio.fState == 2);
        return false;
    }
}


// This function is called by the thread that ran an initialization function,
// just after completing the function.
//   Some threads may be waiting on the condition, requiring the broadcast wakeup.
//   Some threads may be racing to test the fState variable outside of the mutex,
//   requiring the use of store/release when changing its value.

U_COMMON_API void U_EXPORT2
umtx_initImplPostInit(UInitOnce &uio) {
    {
        std::unique_lock<std::mutex> lock(initMutex());
        umtx_storeRelease(uio.fState, 2);
    }
    initCondition().notify_all();
}

U_NAMESPACE_END

/*************************************************************************************************
 *
 *  Deprecated functions for setting user mutexes.
 *
 *************************************************************************************************/

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

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*/
//----------------------------------------------------------------------------
// File:     mutex.h
//
// Lightweight C++ wrapper for umtx_ C mutex functions
//
// Author:   Alan Liu  1/31/97
// History:
// 06/04/97   helena         Updated setImplementation as per feedback from 5/21 drop.
// 04/07/1999  srl               refocused as a thin wrapper
//
//----------------------------------------------------------------------------
#ifndef MUTEX_H
#define MUTEX_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "umutex.h"

U_NAMESPACE_BEGIN

/**
  * Mutex is a helper class for convenient locking and unlocking of a UMutex.
  *
  * Creating a local scope Mutex will lock a UMutex, holding the lock until the Mutex
  * goes out of scope.
  *
  *  If no UMutex is specified, the ICU global mutex is implied.
  *
  *  For example:
  *
  *  static UMutex myMutex;
  *
  *  void Function(int arg1, int arg2)
  *  {
  *     static Object* foo;      // Shared read-write object
  *     Mutex mutex(&myMutex);   // or no args for the global lock
  *     foo->Method();
  *     // When 'mutex' goes out of scope and gets destroyed here, the lock is released
  *  }
  *
  *  Note:  Do NOT use the form 'Mutex mutex();' as that merely forward-declares a function
  *         returning a Mutex. This is a common mistake which silently slips through the
  *         compiler!!
  */

class U_COMMON_API Mutex : public UMemory {
public:
    Mutex(UMutex *mutex = nullptr) : fMutex(mutex) {
        umtx_lock(fMutex);
    }
    ~Mutex() {
        umtx_unlock(fMutex);
    }

    Mutex(const Mutex &other) = delete; // forbid assigning of this class
    Mutex &operator=(const Mutex &other) = delete; // forbid copying of this class
    void *operator new(size_t s) = delete;  // forbid heap allocation. Locals only.

private:
    UMutex   *fMutex;
};


U_NAMESPACE_END

#endif //_MUTEX_
//eof

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

//----------------------------------------------------------------------------
// Code within that accesses shared static or global data should
// should instantiate a Mutex object while doing so. You should make your own 
// private mutex where possible.

// For example:
// 
// UMutex myMutex;
// 
// void Function(int arg1, int arg2)
// {
//    static Object* foo;     // Shared read-write object
//    Mutex mutex(&myMutex);  // or no args for the global lock
//    foo->Method();
//    // When 'mutex' goes out of scope and gets destroyed here, the lock is released
// }
//
// Note:  Do NOT use the form 'Mutex mutex();' as that merely forward-declares a function
//        returning a Mutex. This is a common mistake which silently slips through the
//        compiler!!
//

class U_COMMON_API Mutex : public UMemory {
public:
  inline Mutex(UMutex *mutex = NULL);
  inline ~Mutex();

private:
  UMutex   *fMutex;

  Mutex(const Mutex &other); // forbid copying of this class
  Mutex &operator=(const Mutex &other); // forbid copying of this class
};

inline Mutex::Mutex(UMutex *mutex)
  : fMutex(mutex)
{
  umtx_lock(fMutex);
}

inline Mutex::~Mutex()
{
  umtx_unlock(fMutex);
}

U_NAMESPACE_END

#endif //_MUTEX_
//eof

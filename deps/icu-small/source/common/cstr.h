/*
******************************************************************************
*
*   Copyright (C) 2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File: cstr.h
*/

#ifndef CSTR_H
#define CSTR_H

#include "unicode/unistr.h"
#include "unicode/uobject.h"
#include "unicode/utypes.h"

#include "charstr.h"

/**
 * ICU-internal class CStr, a small helper class to facilitate passing UnicodStrings
 * to functions needing (const char *) strings, such as printf().
 *
 * It is intended primarily for use in debugging or in tests. Uses platform
 * default code page conversion, which will do the best job possible,
 * but may be lossy, depending on the platform.
 *
 * Example Usage:
 *   UnicodeString s = whatever;
 *   printf("%s", CStr(s)());
 *
 *   The explicit call to the CStr() constructor creates a temporary object.
 *   Operator () on the temporary object returns a (const char *) pointer.
 *   The lifetime of the (const char *) data is that of the temporary object,
 *   which works well when passing it as a parameter to another function, such as printf.
 */

U_NAMESPACE_BEGIN

class U_COMMON_API CStr : public UMemory {
  public:
    CStr(const UnicodeString &in);
    ~CStr();
    const char * operator ()() const;

  private:
    CharString s;
    CStr(const CStr &other);               //  Forbid copying of this class.
    CStr &operator =(const CStr &other);   //  Forbid assignment.
};

U_NAMESPACE_END

#endif

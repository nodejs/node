// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedpluralrules.h
*/

#ifndef __SHARED_PLURALRULES_H__
#define __SHARED_PLURALRULES_H__

#include "unicode/utypes.h"
#include "sharedobject.h"

U_NAMESPACE_BEGIN

class PluralRules;

class U_I18N_API SharedPluralRules : public SharedObject {
public:
    SharedPluralRules(PluralRules *prToAdopt) : ptr(prToAdopt) { }
    virtual ~SharedPluralRules();
    const PluralRules *operator->() const { return ptr; }
    const PluralRules &operator*() const { return *ptr; }
private:
    PluralRules *ptr;
    SharedPluralRules(const SharedPluralRules &);
    SharedPluralRules &operator=(const SharedPluralRules &);
};

U_NAMESPACE_END

#endif

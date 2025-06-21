// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharednumberformat.h
*/

#ifndef __SHARED_NUMBERFORMAT_H__
#define __SHARED_NUMBERFORMAT_H__

#include "unicode/utypes.h"
#include "sharedobject.h"
#include "unifiedcache.h"

U_NAMESPACE_BEGIN

class NumberFormat;

class U_I18N_API SharedNumberFormat : public SharedObject {
public:
    SharedNumberFormat(NumberFormat *nfToAdopt) : ptr(nfToAdopt) { }
    virtual ~SharedNumberFormat();
    const NumberFormat *get() const { return ptr; }
    const NumberFormat *operator->() const { return ptr; }
    const NumberFormat &operator*() const { return *ptr; }
private:
    NumberFormat *ptr;
    SharedNumberFormat(const SharedNumberFormat &) = delete;
    SharedNumberFormat &operator=(const SharedNumberFormat &) = delete;
};

template<> U_I18N_API
const SharedNumberFormat *LocaleCacheKey<SharedNumberFormat>::createObject(
        const void * /*unused*/, UErrorCode &status) const;

U_NAMESPACE_END

#endif

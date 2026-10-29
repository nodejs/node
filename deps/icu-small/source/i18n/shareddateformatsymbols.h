// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* shareddateformatsymbols.h
*/

#ifndef __SHARED_DATEFORMATSYMBOLS_H__
#define __SHARED_DATEFORMATSYMBOLS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "sharedobject.h"
#include "unicode/dtfmtsym.h"
#include "unifiedcache.h"

U_NAMESPACE_BEGIN


class U_I18N_API_CLASS SharedDateFormatSymbols : public SharedObject {
public:
    U_I18N_API SharedDateFormatSymbols(const Locale& loc, const char* type, UErrorCode& status)
        : dfs(loc, type, status) {}
    U_I18N_API virtual ~SharedDateFormatSymbols();
    U_I18N_API const DateFormatSymbols& get() const { return dfs; }
private:
    DateFormatSymbols dfs;
    SharedDateFormatSymbols(const SharedDateFormatSymbols &) = delete;
    SharedDateFormatSymbols &operator=(const SharedDateFormatSymbols &) = delete;
};

template<> U_I18N_API
const SharedDateFormatSymbols *
        LocaleCacheKey<SharedDateFormatSymbols>::createObject(
            const void * /*unusedContext*/, UErrorCode &status) const;

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif

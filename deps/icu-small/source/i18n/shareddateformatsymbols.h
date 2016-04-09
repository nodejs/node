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
#include "sharedobject.h"
#include "unicode/dtfmtsym.h"

U_NAMESPACE_BEGIN


class U_I18N_API SharedDateFormatSymbols : public SharedObject {
public:
    SharedDateFormatSymbols(
            const Locale &loc, const char *type, UErrorCode &status)
            : dfs(loc, type, status) { }
    virtual ~SharedDateFormatSymbols();
    const DateFormatSymbols &get() const { return dfs; }
private:
    DateFormatSymbols dfs;
    SharedDateFormatSymbols(const SharedDateFormatSymbols &);
    SharedDateFormatSymbols &operator=(const SharedDateFormatSymbols &);
};

U_NAMESPACE_END

#endif

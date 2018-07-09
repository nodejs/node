// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/ufieldpositer.h"
#include "unicode/fpositer.h"
#include "unicode/localpointer.h"

U_NAMESPACE_USE


U_CAPI UFieldPositionIterator* U_EXPORT2
ufieldpositer_open(UErrorCode* status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    FieldPositionIterator* fpositer = new FieldPositionIterator();
    if (fpositer == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    }
    return (UFieldPositionIterator*)fpositer;
}


U_CAPI void U_EXPORT2
ufieldpositer_close(UFieldPositionIterator *fpositer)
{
    delete (FieldPositionIterator*)fpositer;
}


U_CAPI int32_t U_EXPORT2
ufieldpositer_next(UFieldPositionIterator *fpositer,
                   int32_t *beginIndex, int32_t *endIndex)
{
    FieldPosition fp;
    int32_t field = -1;
    if (((FieldPositionIterator*)fpositer)->next(fp)) {
        field = fp.getField();
        if (beginIndex) {
            *beginIndex = fp.getBeginIndex();
        }
        if (endIndex) {
            *endIndex = fp.getEndIndex();
        }
    }
    return field;
}


#endif /* #if !UCONFIG_NO_FORMATTING */

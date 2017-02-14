// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2000-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
 *  ucnv_cb.c:
 *  External APIs for the ICU's codeset conversion library
 *  Helena Shih
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   7/28/2000   srl         Implementation
 */

/**
 * @name Character Conversion C API
 *
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucnv_cb.h"
#include "ucnv_bld.h"
#include "ucnv_cnv.h"
#include "cmemory.h"

/* need to update the offsets when the target moves. */
/* Note: Recursion may occur in the cb functions, be sure to update the offsets correctly
if you don't use ucnv_cbXXX functions.  Make sure you don't use the same callback within
the same call stack if the complexity arises. */
U_CAPI void  U_EXPORT2
ucnv_cbFromUWriteBytes (UConverterFromUnicodeArgs *args,
                       const char* source,
                       int32_t length,
                       int32_t offsetIndex,
                       UErrorCode * err)
{
    if(U_FAILURE(*err)) {
        return;
    }

    ucnv_fromUWriteBytes(
        args->converter,
        source, length,
        &args->target, args->targetLimit,
        &args->offsets, offsetIndex,
        err);
}

U_CAPI void  U_EXPORT2
ucnv_cbFromUWriteUChars(UConverterFromUnicodeArgs *args,
                             const UChar** source,
                             const UChar*  sourceLimit,
                             int32_t offsetIndex,
                             UErrorCode * err)
{
    /*
    This is a fun one.  Recursion can occur - we're basically going to
    just retry shoving data through the same converter. Note, if you got
    here through some kind of invalid sequence, you maybe should emit a
    reset sequence of some kind and/or call ucnv_reset().  Since this
    IS an actual conversion, take care that you've changed the callback
    or the data, or you'll get an infinite loop.

    Please set the err value to something reasonable before calling
    into this.
    */

    char *oldTarget;

    if(U_FAILURE(*err))
    {
        return;
    }

    oldTarget = args->target;

    ucnv_fromUnicode(args->converter,
        &args->target,
        args->targetLimit,
        source,
        sourceLimit,
        NULL, /* no offsets */
        FALSE, /* no flush */
        err);

    if(args->offsets)
    {
        while (args->target != oldTarget)  /* if it moved at all.. */
        {
            *(args->offsets)++ = offsetIndex;
            oldTarget++;
        }
    }

    /*
    Note, if you did something like used a Stop subcallback, things would get interesting.
    In fact, here's where we want to return the partially consumed in-source!
    */
    if(*err == U_BUFFER_OVERFLOW_ERROR)
    /* && (*source < sourceLimit && args->target >= args->targetLimit)
    -- S. Hrcek */
    {
        /* Overflowed the target.  Now, we'll write into the charErrorBuffer.
        It's a fixed size. If we overflow it... Hmm */
        char *newTarget;
        const char *newTargetLimit;
        UErrorCode err2 = U_ZERO_ERROR;

        int8_t errBuffLen;

        errBuffLen  = args->converter->charErrorBufferLength;

        /* start the new target at the first free slot in the errbuff.. */
        newTarget = (char *)(args->converter->charErrorBuffer + errBuffLen);

        newTargetLimit = (char *)(args->converter->charErrorBuffer +
            sizeof(args->converter->charErrorBuffer));

        if(newTarget >= newTargetLimit)
        {
            *err = U_INTERNAL_PROGRAM_ERROR;
            return;
        }

        /* We're going to tell the converter that the errbuff len is empty.
        This prevents the existing errbuff from being 'flushed' out onto
        itself.  If the errbuff is needed by the converter this time,
        we're hosed - we're out of space! */

        args->converter->charErrorBufferLength = 0;

        ucnv_fromUnicode(args->converter,
                         &newTarget,
                         newTargetLimit,
                         source,
                         sourceLimit,
                         NULL,
                         FALSE,
                         &err2);

        /* We can go ahead and overwrite the  length here. We know just how
        to recalculate it. */

        args->converter->charErrorBufferLength = (int8_t)(
            newTarget - (char*)args->converter->charErrorBuffer);

        if((newTarget >= newTargetLimit) || (err2 == U_BUFFER_OVERFLOW_ERROR))
        {
            /* now we're REALLY in trouble.
            Internal program error - callback shouldn't have written this much
            data!
            */
            *err = U_INTERNAL_PROGRAM_ERROR;
            return;
        }
        /*else {*/
            /* sub errs could be invalid/truncated/illegal chars or w/e.
            These might want to be passed on up.. But the problem is, we already
            need to pass U_BUFFER_OVERFLOW_ERROR. That has to override these
            other errs.. */

            /*
            if(U_FAILURE(err2))
            ??
            */
        /*}*/
    }
}

U_CAPI void  U_EXPORT2
ucnv_cbFromUWriteSub (UConverterFromUnicodeArgs *args,
                           int32_t offsetIndex,
                           UErrorCode * err)
{
    UConverter *converter;
    int32_t length;

    if(U_FAILURE(*err)) {
        return;
    }
    converter = args->converter;
    length = converter->subCharLen;

    if(length == 0) {
        return;
    }

    if(length < 0) {
        /*
         * Write/convert the substitution string. Its real length is -length.
         * Unlike the escape callback, we need not change the converter's
         * callback function because ucnv_setSubstString() verified that
         * the string can be converted, so we will not get a conversion error
         * and will not recurse.
         * At worst we should get a U_BUFFER_OVERFLOW_ERROR.
         */
        const UChar *source = (const UChar *)converter->subChars;
        ucnv_cbFromUWriteUChars(args, &source, source - length, offsetIndex, err);
        return;
    }

    if(converter->sharedData->impl->writeSub!=NULL) {
        converter->sharedData->impl->writeSub(args, offsetIndex, err);
    }
    else if(converter->subChar1!=0 && (uint16_t)converter->invalidUCharBuffer[0]<=(uint16_t)0xffu) {
        /*
        TODO: Is this untestable because the MBCS converter has a writeSub function to call
        and the other converters don't use subChar1?
        */
        ucnv_cbFromUWriteBytes(args,
                               (const char *)&converter->subChar1, 1,
                               offsetIndex, err);
    }
    else {
        ucnv_cbFromUWriteBytes(args,
                               (const char *)converter->subChars, length,
                               offsetIndex, err);
    }
}

U_CAPI void  U_EXPORT2
ucnv_cbToUWriteUChars (UConverterToUnicodeArgs *args,
                            const UChar* source,
                            int32_t length,
                            int32_t offsetIndex,
                            UErrorCode * err)
{
    if(U_FAILURE(*err)) {
        return;
    }

    ucnv_toUWriteUChars(
        args->converter,
        source, length,
        &args->target, args->targetLimit,
        &args->offsets, offsetIndex,
        err);
}

U_CAPI void  U_EXPORT2
ucnv_cbToUWriteSub (UConverterToUnicodeArgs *args,
                         int32_t offsetIndex,
                       UErrorCode * err)
{
    static const UChar kSubstituteChar1 = 0x1A, kSubstituteChar = 0xFFFD;

    /* could optimize this case, just one uchar */
    if(args->converter->invalidCharLength == 1 && args->converter->subChar1 != 0) {
        ucnv_cbToUWriteUChars(args, &kSubstituteChar1, 1, offsetIndex, err);
    } else {
        ucnv_cbToUWriteUChars(args, &kSubstituteChar, 1, offsetIndex, err);
    }
}

#endif

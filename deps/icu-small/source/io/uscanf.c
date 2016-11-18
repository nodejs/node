// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1998-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File uscanf.c
*
* Modification History:
*
*   Date        Name        Description
*   12/02/98    stephen        Creation.
*   03/13/99    stephen     Modified for new C API.
******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_CONVERSION

#include "unicode/putil.h"
#include "unicode/ustdio.h"
#include "unicode/ustring.h"
#include "uscanf.h"
#include "ufile.h"
#include "ufmt_cmn.h"

#include "cmemory.h"
#include "cstring.h"


U_CAPI int32_t U_EXPORT2
u_fscanf(UFILE        *f,
         const char    *patternSpecification,
         ... )
{
    va_list ap;
    int32_t converted;

    va_start(ap, patternSpecification);
    converted = u_vfscanf(f, patternSpecification, ap);
    va_end(ap);

    return converted;
}

U_CAPI int32_t U_EXPORT2
u_fscanf_u(UFILE        *f,
           const UChar    *patternSpecification,
           ... )
{
    va_list ap;
    int32_t converted;

    va_start(ap, patternSpecification);
    converted = u_vfscanf_u(f, patternSpecification, ap);
    va_end(ap);

    return converted;
}

U_CAPI int32_t  U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_vfscanf(UFILE        *f,
          const char    *patternSpecification,
          va_list        ap)
{
    int32_t converted;
    UChar *pattern;
    UChar patBuffer[UFMT_DEFAULT_BUFFER_SIZE];
    int32_t size = (int32_t)uprv_strlen(patternSpecification) + 1;

    /* convert from the default codepage to Unicode */
    if (size >= MAX_UCHAR_BUFFER_SIZE(patBuffer)) {
        pattern = (UChar *)uprv_malloc(size * sizeof(UChar));
        if(pattern == 0) {
            return 0;
        }
    }
    else {
        pattern = patBuffer;
    }
    u_charsToUChars(patternSpecification, pattern, size);

    /* do the work */
    converted = u_vfscanf_u(f, pattern, ap);

    /* clean up */
    if (pattern != patBuffer) {
        uprv_free(pattern);
    }

    return converted;
}

U_CAPI int32_t  U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_vfscanf_u(UFILE       *f,
            const UChar *patternSpecification,
            va_list     ap)
{
    return u_scanf_parse(f, patternSpecification, ap);
}

#endif /* #if !UCONFIG_NO_FORMATTING */

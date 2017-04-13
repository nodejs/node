// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File locbund.h
*
* Modification History:
*
*   Date        Name        Description
*   10/16/98    stephen     Creation.
*   02/25/99    stephen     Modified for new C API.
*******************************************************************************
*/

#ifndef LOCBUND_H
#define LOCBUND_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unum.h"

#define ULOCALEBUNDLE_NUMBERFORMAT_COUNT ((int32_t)UNUM_SPELLOUT)

typedef struct ULocaleBundle {
    char            *fLocale;

    UNumberFormat   *fNumberFormat[ULOCALEBUNDLE_NUMBERFORMAT_COUNT];
    UBool           isInvariantLocale;
} ULocaleBundle;


/**
 * Initialize a ULocaleBundle, initializing all formatters to 0.
 * @param result A ULocaleBundle to initialize.
 * @param loc The locale of the ULocaleBundle.
 * @return A pointer to a ULocaleBundle, or 0 if <TT>loc</TT> was invalid.
 */
U_CAPI ULocaleBundle *
u_locbund_init(ULocaleBundle *result, const char *loc);

/**
 * Create a new ULocaleBundle, initializing all formatters to 0.
 * @param loc The locale of the ULocaleBundle.
 * @return A pointer to a ULocaleBundle, or 0 if <TT>loc</TT> was invalid.
 */
/*U_CAPI  ULocaleBundle *
u_locbund_new(const char *loc);*/

/**
 * Create a deep copy of this ULocaleBundle;
 * @param bundle The ULocaleBundle to clone.
 * @return A new ULocaleBundle.
 */
/*U_CAPI ULocaleBundle *
u_locbund_clone(const ULocaleBundle *bundle);*/

/**
 * Delete the specified ULocaleBundle, freeing all associated memory.
 * @param bundle The ULocaleBundle to delete
 */
U_CAPI void
u_locbund_close(ULocaleBundle *bundle);

/**
 * Get the NumberFormat used to format and parse numbers in a ULocaleBundle.
 * @param bundle The ULocaleBundle to use
 * @return A pointer to the NumberFormat used for number formatting and parsing.
 */
U_CAPI UNumberFormat *
u_locbund_getNumberFormat(ULocaleBundle *bundle, UNumberFormatStyle style);

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif

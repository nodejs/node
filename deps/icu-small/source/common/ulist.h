// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*   Copyright (C) 2009-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
******************************************************************************
*/

#ifndef ULIST_H
#define ULIST_H

#include "unicode/utypes.h"
#include "unicode/uenum.h"

struct UList;
typedef struct UList UList;

U_CAPI UList * U_EXPORT2 ulist_createEmptyList(UErrorCode *status);

U_CAPI void U_EXPORT2 ulist_addItemEndList(UList *list, const void *data, UBool forceDelete, UErrorCode *status);

U_CAPI void U_EXPORT2 ulist_addItemBeginList(UList *list, const void *data, UBool forceDelete, UErrorCode *status);

U_CAPI UBool U_EXPORT2 ulist_containsString(const UList *list, const char *data, int32_t length);

U_CAPI UBool U_EXPORT2 ulist_removeString(UList *list, const char *data);

U_CAPI void *U_EXPORT2 ulist_getNext(UList *list);

U_CAPI int32_t U_EXPORT2 ulist_getListSize(const UList *list);

U_CAPI void U_EXPORT2 ulist_resetList(UList *list);

U_CAPI void U_EXPORT2 ulist_deleteList(UList *list);

/*
 * The following are for use when creating UEnumeration object backed by UList.
 */
U_CAPI void U_EXPORT2 ulist_close_keyword_values_iterator(UEnumeration *en);

U_CAPI int32_t U_EXPORT2 ulist_count_keyword_values(UEnumeration *en, UErrorCode *status);

U_CAPI const char * U_EXPORT2 ulist_next_keyword_value(UEnumeration* en, int32_t *resultLength, UErrorCode* status);

U_CAPI void U_EXPORT2 ulist_reset_keyword_values_iterator(UEnumeration* en, UErrorCode* status);

U_CAPI UList * U_EXPORT2 ulist_getListFromEnum(UEnumeration *en);

#endif

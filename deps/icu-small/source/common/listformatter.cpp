// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2013-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformatter.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#include "unicode/listformatter.h"
#include "unicode/simpleformatter.h"
#include "mutex.h"
#include "hash.h"
#include "cstring.h"
#include "ulocimp.h"
#include "charstr.h"
#include "ucln_cmn.h"
#include "uresimp.h"
#include "resource.h"

U_NAMESPACE_BEGIN

struct ListFormatInternal : public UMemory {
    SimpleFormatter twoPattern;
    SimpleFormatter startPattern;
    SimpleFormatter middlePattern;
    SimpleFormatter endPattern;

ListFormatInternal(
        const UnicodeString& two,
        const UnicodeString& start,
        const UnicodeString& middle,
        const UnicodeString& end,
        UErrorCode &errorCode) :
        twoPattern(two, 2, 2, errorCode),
        startPattern(start, 2, 2, errorCode),
        middlePattern(middle, 2, 2, errorCode),
        endPattern(end, 2, 2, errorCode) {}

ListFormatInternal(const ListFormatData &data, UErrorCode &errorCode) :
        twoPattern(data.twoPattern, errorCode),
        startPattern(data.startPattern, errorCode),
        middlePattern(data.middlePattern, errorCode),
        endPattern(data.endPattern, errorCode) { }

ListFormatInternal(const ListFormatInternal &other) :
    twoPattern(other.twoPattern),
    startPattern(other.startPattern),
    middlePattern(other.middlePattern),
    endPattern(other.endPattern) { }
};



static Hashtable* listPatternHash = NULL;
static UMutex listFormatterMutex = U_MUTEX_INITIALIZER;
static const char *STANDARD_STYLE = "standard";

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_listformatter_cleanup() {
    delete listPatternHash;
    listPatternHash = NULL;
    return TRUE;
}

static void U_CALLCONV
uprv_deleteListFormatInternal(void *obj) {
    delete static_cast<ListFormatInternal *>(obj);
}

U_CDECL_END

ListFormatter::ListFormatter(const ListFormatter& other) :
        owned(other.owned), data(other.data) {
    if (other.owned != NULL) {
        owned = new ListFormatInternal(*other.owned);
        data = owned;
    }
}

ListFormatter& ListFormatter::operator=(const ListFormatter& other) {
    if (this == &other) {
        return *this;
    }
    delete owned;
    if (other.owned) {
        owned = new ListFormatInternal(*other.owned);
        data = owned;
    } else {
        owned = NULL;
        data = other.data;
    }
    return *this;
}

void ListFormatter::initializeHash(UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }

    listPatternHash = new Hashtable();
    if (listPatternHash == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    listPatternHash->setValueDeleter(uprv_deleteListFormatInternal);
    ucln_common_registerCleanup(UCLN_COMMON_LIST_FORMATTER, uprv_listformatter_cleanup);

}

const ListFormatInternal* ListFormatter::getListFormatInternal(
        const Locale& locale, const char *style, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    CharString keyBuffer(locale.getName(), errorCode);
    keyBuffer.append(':', errorCode).append(style, errorCode);
    UnicodeString key(keyBuffer.data(), -1, US_INV);
    ListFormatInternal* result = NULL;
    {
        Mutex m(&listFormatterMutex);
        if (listPatternHash == NULL) {
            initializeHash(errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
        result = static_cast<ListFormatInternal*>(listPatternHash->get(key));
    }
    if (result != NULL) {
        return result;
    }
    result = loadListFormatInternal(locale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }

    {
        Mutex m(&listFormatterMutex);
        ListFormatInternal* temp = static_cast<ListFormatInternal*>(listPatternHash->get(key));
        if (temp != NULL) {
            delete result;
            result = temp;
        } else {
            listPatternHash->put(key, result, errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
    }
    return result;
}

static const UChar solidus = 0x2F;
static const UChar aliasPrefix[] = { 0x6C,0x69,0x73,0x74,0x50,0x61,0x74,0x74,0x65,0x72,0x6E,0x2F }; // "listPattern/"
enum {
    kAliasPrefixLen = UPRV_LENGTHOF(aliasPrefix),
    kStyleLenMax = 24 // longest currently is 14
};

struct ListFormatter::ListPatternsSink : public ResourceSink {
    UnicodeString two, start, middle, end;
#if ((U_PLATFORM == U_PF_AIX) || (U_PLATFORM == U_PF_OS390)) && (U_CPLUSPLUS_VERSION < 11)
    char aliasedStyle[kStyleLenMax+1];
    ListPatternsSink() {
      uprv_memset(aliasedStyle, 0, kStyleLenMax+1);
    }
#else
    char aliasedStyle[kStyleLenMax+1] = {0};

    ListPatternsSink() {}
#endif
    virtual ~ListPatternsSink();

    void setAliasedStyle(UnicodeString alias) {
        int32_t startIndex = alias.indexOf(aliasPrefix, kAliasPrefixLen, 0);
        if (startIndex < 0) {
            return;
        }
        startIndex += kAliasPrefixLen;
        int32_t endIndex = alias.indexOf(solidus, startIndex);
        if (endIndex < 0) {
            endIndex = alias.length();
        }
        alias.extract(startIndex, endIndex-startIndex, aliasedStyle, kStyleLenMax+1, US_INV);
        aliasedStyle[kStyleLenMax] = 0;
    }

    void handleValueForPattern(ResourceValue &value, UnicodeString &pattern, UErrorCode &errorCode) {
        if (pattern.isEmpty()) {
            if (value.getType() == URES_ALIAS) {
                if (aliasedStyle[0] == 0) {
                    setAliasedStyle(value.getAliasUnicodeString(errorCode));
                }
            } else {
                pattern = value.getUnicodeString(errorCode);
            }
        }
    }

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
            UErrorCode &errorCode) {
        aliasedStyle[0] = 0;
        if (value.getType() == URES_ALIAS) {
            setAliasedStyle(value.getAliasUnicodeString(errorCode));
            return;
        }
        ResourceTable listPatterns = value.getTable(errorCode);
        for (int i = 0; U_SUCCESS(errorCode) && listPatterns.getKeyAndValue(i, key, value); ++i) {
            if (uprv_strcmp(key, "2") == 0) {
                handleValueForPattern(value, two, errorCode);
            } else if (uprv_strcmp(key, "end") == 0) {
                handleValueForPattern(value, end, errorCode);
            } else if (uprv_strcmp(key, "middle") == 0) {
                handleValueForPattern(value, middle, errorCode);
            } else if (uprv_strcmp(key, "start") == 0) {
                handleValueForPattern(value, start, errorCode);
            }
        }
    }
};

// Virtual destructors must be defined out of line.
ListFormatter::ListPatternsSink::~ListPatternsSink() {}

ListFormatInternal* ListFormatter::loadListFormatInternal(
        const Locale& locale, const char * style, UErrorCode& errorCode) {
    UResourceBundle* rb = ures_open(NULL, locale.getName(), &errorCode);
    rb = ures_getByKeyWithFallback(rb, "listPattern", rb, &errorCode);
    if (U_FAILURE(errorCode)) {
        ures_close(rb);
        return NULL;
    }
    ListFormatter::ListPatternsSink sink;
    char currentStyle[kStyleLenMax+1];
    uprv_strncpy(currentStyle, style, kStyleLenMax);
    currentStyle[kStyleLenMax] = 0;

    for (;;) {
        ures_getAllItemsWithFallback(rb, currentStyle, sink, errorCode);
        if (U_FAILURE(errorCode) || sink.aliasedStyle[0] == 0 || uprv_strcmp(currentStyle, sink.aliasedStyle) == 0) {
            break;
        }
        uprv_strcpy(currentStyle, sink.aliasedStyle);
    }
    ures_close(rb);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    if (sink.two.isEmpty() || sink.start.isEmpty() || sink.middle.isEmpty() || sink.end.isEmpty()) {
        errorCode = U_MISSING_RESOURCE_ERROR;
        return NULL;
    }
    ListFormatInternal* result = new ListFormatInternal(sink.two, sink.start, sink.middle, sink.end, errorCode);
    if (result == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    if (U_FAILURE(errorCode)) {
        delete result;
        return NULL;
    }
    return result;
}

ListFormatter* ListFormatter::createInstance(UErrorCode& errorCode) {
    Locale locale;  // The default locale.
    return createInstance(locale, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, UErrorCode& errorCode) {
    return createInstance(locale, STANDARD_STYLE, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, const char *style, UErrorCode& errorCode) {
    Locale tempLocale = locale;
    const ListFormatInternal* listFormatInternal = getListFormatInternal(tempLocale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    ListFormatter* p = new ListFormatter(listFormatInternal);
    if (p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return p;
}

ListFormatter::ListFormatter(const ListFormatData& listFormatData, UErrorCode &errorCode) {
    owned = new ListFormatInternal(listFormatData, errorCode);
    data = owned;
}

ListFormatter::ListFormatter(const ListFormatInternal* listFormatterInternal) : owned(NULL), data(listFormatterInternal) {
}

ListFormatter::~ListFormatter() {
    delete owned;
}

/**
 * Joins first and second using the pattern pat.
 * On entry offset is an offset into first or -1 if offset unspecified.
 * On exit offset is offset of second in result if recordOffset was set
 * Otherwise if it was >=0 it is set to point into result where it used
 * to point into first. On exit, result is the join of first and second
 * according to pat. Any previous value of result gets replaced.
 */
static void joinStringsAndReplace(
        const SimpleFormatter& pat,
        const UnicodeString& first,
        const UnicodeString& second,
        UnicodeString &result,
        UBool recordOffset,
        int32_t &offset,
        UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    const UnicodeString *params[2] = {&first, &second};
    int32_t offsets[2];
    pat.formatAndReplace(
            params,
            UPRV_LENGTHOF(params),
            result,
            offsets,
            UPRV_LENGTHOF(offsets),
            errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    if (offsets[0] == -1 || offsets[1] == -1) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return;
    }
    if (recordOffset) {
        offset = offsets[1];
    } else if (offset >= 0) {
        offset += offsets[0];
    }
}

UnicodeString& ListFormatter::format(
        const UnicodeString items[],
        int32_t nItems,
        UnicodeString& appendTo,
        UErrorCode& errorCode) const {
    int32_t offset;
    return format(items, nItems, appendTo, -1, offset, errorCode);
}

UnicodeString& ListFormatter::format(
        const UnicodeString items[],
        int32_t nItems,
        UnicodeString& appendTo,
        int32_t index,
        int32_t &offset,
        UErrorCode& errorCode) const {
    offset = -1;
    if (U_FAILURE(errorCode)) {
        return appendTo;
    }
    if (data == NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return appendTo;
    }

    if (nItems <= 0) {
        return appendTo;
    }
    if (nItems == 1) {
        if (index == 0) {
            offset = appendTo.length();
        }
        appendTo.append(items[0]);
        return appendTo;
    }
    UnicodeString result(items[0]);
    if (index == 0) {
        offset = 0;
    }
    joinStringsAndReplace(
            nItems == 2 ? data->twoPattern : data->startPattern,
            result,
            items[1],
            result,
            index == 1,
            offset,
            errorCode);
    if (nItems > 2) {
        for (int32_t i = 2; i < nItems - 1; ++i) {
             joinStringsAndReplace(
                     data->middlePattern,
                     result,
                     items[i],
                     result,
                     index == i,
                     offset,
                     errorCode);
        }
        joinStringsAndReplace(
                data->endPattern,
                result,
                items[nItems - 1],
                result,
                index == nItems - 1,
                offset,
                errorCode);
    }
    if (U_SUCCESS(errorCode)) {
        if (offset >= 0) {
            offset += appendTo.length();
        }
        appendTo += result;
    }
    return appendTo;
}

U_NAMESPACE_END

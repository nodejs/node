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

#include "cmemory.h"
#include "unicode/fpositer.h"  // FieldPositionIterator
#include "unicode/listformatter.h"
#include "unicode/simpleformatter.h"
#include "unicode/ulistformatter.h"
#include "unicode/uscript.h"
#include "fphdlimp.h"
#include "mutex.h"
#include "hash.h"
#include "cstring.h"
#include "uarrsort.h"
#include "ulocimp.h"
#include "charstr.h"
#include "ucln_in.h"
#include "uresimp.h"
#include "resource.h"
#include "formattedval_impl.h"

U_NAMESPACE_BEGIN

namespace {

class PatternHandler : public UObject {
public:
    PatternHandler(const UnicodeString& two, const UnicodeString& end, UErrorCode& errorCode) :
        twoPattern(two, 2, 2, errorCode),
        endPattern(end, 2, 2, errorCode) {  }

    PatternHandler(const SimpleFormatter& two, const SimpleFormatter& end) :
        twoPattern(two),
        endPattern(end) { }

    virtual ~PatternHandler();

    virtual PatternHandler* clone() const { return new PatternHandler(twoPattern, endPattern); }

    virtual const SimpleFormatter& getTwoPattern(const UnicodeString&) const {
        return twoPattern;
    }

    virtual const SimpleFormatter& getEndPattern(const UnicodeString&) const {
        return endPattern;
    }

protected:
    SimpleFormatter twoPattern;
    SimpleFormatter endPattern;
};

PatternHandler::~PatternHandler() {
}

class ContextualHandler : public PatternHandler {
public:
    ContextualHandler(bool (*testFunc)(const UnicodeString& text),
                      const UnicodeString& thenTwo,
                      const UnicodeString& elseTwo,
                      const UnicodeString& thenEnd,
                      const UnicodeString& elseEnd,
                      UErrorCode& errorCode) :
        PatternHandler(elseTwo, elseEnd, errorCode),
        test(testFunc),
        thenTwoPattern(thenTwo, 2, 2, errorCode),
        thenEndPattern(thenEnd, 2, 2, errorCode) {  }

    ContextualHandler(bool (*testFunc)(const UnicodeString& text),
                      const SimpleFormatter& thenTwo, SimpleFormatter elseTwo,
                      const SimpleFormatter& thenEnd, SimpleFormatter elseEnd) :
      PatternHandler(elseTwo, elseEnd),
      test(testFunc),
      thenTwoPattern(thenTwo),
      thenEndPattern(thenEnd) { }

    ~ContextualHandler() override;

    PatternHandler* clone() const override {
        return new ContextualHandler(
            test, thenTwoPattern, twoPattern, thenEndPattern, endPattern);
    }

    const SimpleFormatter& getTwoPattern(
        const UnicodeString& text) const override {
        return (test)(text) ? thenTwoPattern : twoPattern;
    }

    const SimpleFormatter& getEndPattern(
        const UnicodeString& text) const override {
        return (test)(text) ? thenEndPattern : endPattern;
    }

private:
    bool (*test)(const UnicodeString&);
    SimpleFormatter thenTwoPattern;
    SimpleFormatter thenEndPattern;
};

ContextualHandler::~ContextualHandler() {
}

static const char16_t *spanishY = u"{0} y {1}";
static const char16_t *spanishE = u"{0} e {1}";
static const char16_t *spanishO = u"{0} o {1}";
static const char16_t *spanishU = u"{0} u {1}";
static const char16_t *hebrewVav = u"{0} \u05D5{1}";
static const char16_t *hebrewVavDash = u"{0} \u05D5-{1}";

// Condiction to change to e.
// Starts with "hi" or "i" but not with "hie" nor "hia"
static bool shouldChangeToE(const UnicodeString& text) {
    int32_t len = text.length();
    if (len == 0) { return false; }
    // Case insensitive match hi but not hie nor hia.
    if ((text[0] == u'h' || text[0] == u'H') &&
            ((len > 1) && (text[1] == u'i' || text[1] == u'I')) &&
            ((len == 2) || !(text[2] == u'a' || text[2] == u'A' || text[2] == u'e' || text[2] == u'E'))) {
        return true;
    }
    // Case insensitive for "start with i"
    if (text[0] == u'i' || text[0] == u'I') { return true; }
    return false;
}

// Condiction to change to u.
// Starts with "o", "ho", and "8". Also "11" by itself.
// re: ^((o|ho|8).*|11)$
static bool shouldChangeToU(const UnicodeString& text) {
    int32_t len = text.length();
    if (len == 0) { return false; }
    // Case insensitive match o.* and 8.*
    if (text[0] == u'o' || text[0] == u'O' || text[0] == u'8') { return true; }
    // Case insensitive match ho.*
    if ((text[0] == u'h' || text[0] == u'H') &&
            ((len > 1) && (text[1] == 'o' || text[1] == u'O'))) {
        return true;
    }
    // match "^11$" and "^11 .*"
    if ((len >= 2) && text[0] == u'1' && text[1] == u'1' && (len == 2 || text[2] == u' ')) { return true; }
    return false;
}

// Condiction to change to VAV follow by a dash.
// Starts with non Hebrew letter.
static bool shouldChangeToVavDash(const UnicodeString& text) {
    if (text.isEmpty()) { return false; }
    UErrorCode status = U_ZERO_ERROR;
    return uscript_getScript(text.char32At(0), &status) != USCRIPT_HEBREW;
}

PatternHandler* createPatternHandler(
        const char* lang, const UnicodeString& two, const UnicodeString& end,
    UErrorCode& status) {
    if (uprv_strcmp(lang, "es") == 0) {
        // Spanish
        UnicodeString spanishYStr(TRUE, spanishY, -1);
        bool twoIsY = two == spanishYStr;
        bool endIsY = end == spanishYStr;
        if (twoIsY || endIsY) {
            UnicodeString replacement(TRUE, spanishE, -1);
            return new ContextualHandler(
                shouldChangeToE,
                twoIsY ? replacement : two, two,
                endIsY ? replacement : end, end, status);
        }
        UnicodeString spanishOStr(TRUE, spanishO, -1);
        bool twoIsO = two == spanishOStr;
        bool endIsO = end == spanishOStr;
        if (twoIsO || endIsO) {
            UnicodeString replacement(TRUE, spanishU, -1);
            return new ContextualHandler(
                shouldChangeToU,
                twoIsO ? replacement : two, two,
                endIsO ? replacement : end, end, status);
        }
    } else if (uprv_strcmp(lang, "he") == 0 || uprv_strcmp(lang, "iw") == 0) {
        // Hebrew
        UnicodeString hebrewVavStr(TRUE, hebrewVav, -1);
        bool twoIsVav = two == hebrewVavStr;
        bool endIsVav = end == hebrewVavStr;
        if (twoIsVav || endIsVav) {
            UnicodeString replacement(TRUE, hebrewVavDash, -1);
            return new ContextualHandler(
                shouldChangeToVavDash,
                twoIsVav ? replacement : two, two,
                endIsVav ? replacement : end, end, status);
        }
    }
    return new PatternHandler(two, end, status);
}

}  // namespace

struct ListFormatInternal : public UMemory {
    SimpleFormatter startPattern;
    SimpleFormatter middlePattern;
    LocalPointer<PatternHandler> patternHandler;

ListFormatInternal(
        const UnicodeString& two,
        const UnicodeString& start,
        const UnicodeString& middle,
        const UnicodeString& end,
        const Locale& locale,
        UErrorCode &errorCode) :
        startPattern(start, 2, 2, errorCode),
        middlePattern(middle, 2, 2, errorCode),
        patternHandler(createPatternHandler(locale.getLanguage(), two, end, errorCode), errorCode) { }

ListFormatInternal(const ListFormatData &data, UErrorCode &errorCode) :
        startPattern(data.startPattern, errorCode),
        middlePattern(data.middlePattern, errorCode),
        patternHandler(createPatternHandler(
            data.locale.getLanguage(), data.twoPattern, data.endPattern, errorCode), errorCode) { }

ListFormatInternal(const ListFormatInternal &other) :
    startPattern(other.startPattern),
    middlePattern(other.middlePattern),
    patternHandler(other.patternHandler->clone()) { }
};


#if !UCONFIG_NO_FORMATTING
class FormattedListData : public FormattedValueFieldPositionIteratorImpl {
public:
    FormattedListData(UErrorCode& status) : FormattedValueFieldPositionIteratorImpl(5, status) {}
    virtual ~FormattedListData();
};

FormattedListData::~FormattedListData() = default;

UPRV_FORMATTED_VALUE_SUBCLASS_AUTO_IMPL(FormattedList)
#endif


static Hashtable* listPatternHash = nullptr;

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_listformatter_cleanup() {
    delete listPatternHash;
    listPatternHash = nullptr;
    return TRUE;
}

static void U_CALLCONV
uprv_deleteListFormatInternal(void *obj) {
    delete static_cast<ListFormatInternal *>(obj);
}

U_CDECL_END

ListFormatter::ListFormatter(const ListFormatter& other) :
        owned(other.owned), data(other.data) {
    if (other.owned != nullptr) {
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
        owned = nullptr;
        data = other.data;
    }
    return *this;
}

void ListFormatter::initializeHash(UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }

    listPatternHash = new Hashtable();
    if (listPatternHash == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    listPatternHash->setValueDeleter(uprv_deleteListFormatInternal);
    ucln_i18n_registerCleanup(UCLN_I18N_LIST_FORMATTER, uprv_listformatter_cleanup);

}

const ListFormatInternal* ListFormatter::getListFormatInternal(
        const Locale& locale, const char *style, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }
    CharString keyBuffer(locale.getName(), errorCode);
    keyBuffer.append(':', errorCode).append(style, errorCode);
    UnicodeString key(keyBuffer.data(), -1, US_INV);
    ListFormatInternal* result = nullptr;
    static UMutex listFormatterMutex;
    {
        Mutex m(&listFormatterMutex);
        if (listPatternHash == nullptr) {
            initializeHash(errorCode);
            if (U_FAILURE(errorCode)) {
                return nullptr;
            }
        }
        result = static_cast<ListFormatInternal*>(listPatternHash->get(key));
    }
    if (result != nullptr) {
        return result;
    }
    result = loadListFormatInternal(locale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }

    {
        Mutex m(&listFormatterMutex);
        ListFormatInternal* temp = static_cast<ListFormatInternal*>(listPatternHash->get(key));
        if (temp != nullptr) {
            delete result;
            result = temp;
        } else {
            listPatternHash->put(key, result, errorCode);
            if (U_FAILURE(errorCode)) {
                return nullptr;
            }
        }
    }
    return result;
}

#if !UCONFIG_NO_FORMATTING
static const char* typeWidthToStyleString(UListFormatterType type, UListFormatterWidth width) {
    switch (type) {
        case ULISTFMT_TYPE_AND:
            switch (width) {
                case ULISTFMT_WIDTH_WIDE:
                    return "standard";
                case ULISTFMT_WIDTH_SHORT:
                    return "standard-short";
                case ULISTFMT_WIDTH_NARROW:
                    return "standard-narrow";
                default:
                    return nullptr;
            }
            break;

        case ULISTFMT_TYPE_OR:
            switch (width) {
                case ULISTFMT_WIDTH_WIDE:
                    return "or";
                case ULISTFMT_WIDTH_SHORT:
                    return "or-short";
                case ULISTFMT_WIDTH_NARROW:
                    return "or-narrow";
                default:
                    return nullptr;
            }
            break;

        case ULISTFMT_TYPE_UNITS:
            switch (width) {
                case ULISTFMT_WIDTH_WIDE:
                    return "unit";
                case ULISTFMT_WIDTH_SHORT:
                    return "unit-short";
                case ULISTFMT_WIDTH_NARROW:
                    return "unit-narrow";
                default:
                    return nullptr;
            }
    }

    return nullptr;
}
#endif

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
    UResourceBundle* rb = ures_open(nullptr, locale.getName(), &errorCode);
    rb = ures_getByKeyWithFallback(rb, "listPattern", rb, &errorCode);
    if (U_FAILURE(errorCode)) {
        ures_close(rb);
        return nullptr;
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
        return nullptr;
    }
    if (sink.two.isEmpty() || sink.start.isEmpty() || sink.middle.isEmpty() || sink.end.isEmpty()) {
        errorCode = U_MISSING_RESOURCE_ERROR;
        return nullptr;
    }

    ListFormatInternal* result = new ListFormatInternal(sink.two, sink.start, sink.middle, sink.end, locale, errorCode);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    if (U_FAILURE(errorCode)) {
        delete result;
        return nullptr;
    }
    return result;
}

ListFormatter* ListFormatter::createInstance(UErrorCode& errorCode) {
    Locale locale;  // The default locale.
    return createInstance(locale, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, UErrorCode& errorCode) {
#if !UCONFIG_NO_FORMATTING
    return createInstance(locale, ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, errorCode);
#else
    return createInstance(locale, "standard", errorCode);
#endif
}

#if !UCONFIG_NO_FORMATTING
ListFormatter* ListFormatter::createInstance(
        const Locale& locale, UListFormatterType type, UListFormatterWidth width, UErrorCode& errorCode) {
    const char* style = typeWidthToStyleString(type, width);
    if (style == nullptr) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    return createInstance(locale, style, errorCode);
}
#endif

ListFormatter* ListFormatter::createInstance(const Locale& locale, const char *style, UErrorCode& errorCode) {
    const ListFormatInternal* listFormatInternal = getListFormatInternal(locale, style, errorCode);
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }
    ListFormatter* p = new ListFormatter(listFormatInternal);
    if (p == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return p;
}

ListFormatter::ListFormatter(const ListFormatData& listFormatData, UErrorCode &errorCode) {
    owned = new ListFormatInternal(listFormatData, errorCode);
    data = owned;
}

ListFormatter::ListFormatter(const ListFormatInternal* listFormatterInternal) : owned(nullptr), data(listFormatterInternal) {
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
        int32_t *offsetFirst,
        int32_t *offsetSecond,
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
    if (offsetFirst != nullptr) *offsetFirst = offsets[0];
    if (offsetSecond != nullptr) *offsetSecond = offsets[1];
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
  return format_(items, nItems, appendTo, index, offset, nullptr, errorCode);
}

#if !UCONFIG_NO_FORMATTING
FormattedList ListFormatter::formatStringsToValue(
        const UnicodeString items[],
        int32_t nItems,
        UErrorCode& errorCode) const {
    LocalPointer<FormattedListData> result(new FormattedListData(errorCode), errorCode);
    if (U_FAILURE(errorCode)) {
        return FormattedList(errorCode);
    }
    UnicodeString string;
    int32_t offset;
    auto handler = result->getHandler(errorCode);
    handler.setCategory(UFIELD_CATEGORY_LIST);
    format_(items, nItems, string, -1, offset, &handler, errorCode);
    handler.getError(errorCode);
    result->appendString(string, errorCode);
    if (U_FAILURE(errorCode)) {
        return FormattedList(errorCode);
    }

    // Add span fields and sort
    ConstrainedFieldPosition cfpos;
    cfpos.constrainField(UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD);
    int32_t i = 0;
    handler.setCategory(UFIELD_CATEGORY_LIST_SPAN);
    while (result->nextPosition(cfpos, errorCode)) {
        handler.addAttribute(i++, cfpos.getStart(), cfpos.getLimit());
    }
    handler.getError(errorCode);
    if (U_FAILURE(errorCode)) {
        return FormattedList(errorCode);
    }
    result->sort();

    return FormattedList(result.orphan());
}
#endif

UnicodeString& ListFormatter::format_(
        const UnicodeString items[],
        int32_t nItems,
        UnicodeString& appendTo,
        int32_t index,
        int32_t &offset,
        FieldPositionHandler* handler,
        UErrorCode& errorCode) const {
#if !UCONFIG_NO_FORMATTING
    offset = -1;
    if (U_FAILURE(errorCode)) {
        return appendTo;
    }
    if (data == nullptr) {
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
        if (handler != nullptr) {
            handler->addAttribute(ULISTFMT_ELEMENT_FIELD,
                                  appendTo.length(),
                                  appendTo.length() + items[0].length());
        }
        appendTo.append(items[0]);
        return appendTo;
    }
    UnicodeString result(items[0]);
    if (index == 0) {
        offset = 0;
    }
    int32_t offsetFirst = 0;
    int32_t offsetSecond = 0;
    int32_t prefixLength = 0;
    // for n items, there are 2 * (n + 1) boundary including 0 and the upper
    // edge.
    MaybeStackArray<int32_t, 10> offsets((handler != nullptr) ? 2 * (nItems + 1): 0);
    if (nItems == 2) {
        joinStringsAndReplace(
                data->patternHandler->getTwoPattern(items[1]),
                result,
                items[1],
                result,
                index == 1,
                offset,
                &offsetFirst,
                &offsetSecond,
                errorCode);
    } else {
        joinStringsAndReplace(
                data->startPattern,
                result,
                items[1],
                result,
                index == 1,
                offset,
                &offsetFirst,
                &offsetSecond,
                errorCode);
    }
    if (handler != nullptr) {
        offsets[0] = 0;
        prefixLength += offsetFirst;
        offsets[1] = offsetSecond - prefixLength;
    }
    if (nItems > 2) {
        for (int32_t i = 2; i < nItems - 1; ++i) {
             joinStringsAndReplace(
                     data->middlePattern,
                     result,
                     items[i],
                     result,
                     index == i,
                     offset,
                     &offsetFirst,
                     &offsetSecond,
                     errorCode);
            if (handler != nullptr) {
                prefixLength += offsetFirst;
                offsets[i] = offsetSecond - prefixLength;
            }
        }
        joinStringsAndReplace(
                data->patternHandler->getEndPattern(items[nItems - 1]),
                result,
                items[nItems - 1],
                result,
                index == nItems - 1,
                offset,
                &offsetFirst,
                &offsetSecond,
                errorCode);
        if (handler != nullptr) {
            prefixLength += offsetFirst;
            offsets[nItems - 1] = offsetSecond - prefixLength;
        }
    }
    if (handler != nullptr) {
        // If there are already some data in appendTo, we need to adjust the index
        // by shifting that lenght while insert into handler.
        int32_t shift = appendTo.length() + prefixLength;
        // Output the ULISTFMT_ELEMENT_FIELD in the order of the input elements
        for (int32_t i = 0; i < nItems; ++i) {
            offsets[i + nItems] = offsets[i] + items[i].length() + shift;
            offsets[i] += shift;
            handler->addAttribute(
                ULISTFMT_ELEMENT_FIELD,  // id
                offsets[i],  // index
                offsets[i + nItems]);  // limit
        }
        // The locale pattern may reorder the items (such as in ur-IN locale),
        // so we cannot assume the array is in accendning order.
        // To handle the edging case, just insert the two ends into the array
        // and sort. Then we output ULISTFMT_LITERAL_FIELD if the indecies
        // between the even and odd position are not the same in the sorted array.
        offsets[2 * nItems] = shift - prefixLength;
        offsets[2 * nItems + 1] = result.length() + shift - prefixLength;
        uprv_sortArray(offsets.getAlias(), 2 * (nItems + 1), sizeof(int32_t),
               uprv_int32Comparator, nullptr,
               false, &errorCode);
        for (int32_t i = 0; i <= nItems; ++i) {
          if (offsets[i * 2] != offsets[i * 2 + 1]) {
            handler->addAttribute(
                ULISTFMT_LITERAL_FIELD,  // id
                offsets[i * 2],  // index
                offsets[i * 2 + 1]);  // limit
          }
        }
    }
    if (U_SUCCESS(errorCode)) {
        if (offset >= 0) {
            offset += appendTo.length();
        }
        appendTo += result;
    }
#endif
    return appendTo;
}

U_NAMESPACE_END

/*
*******************************************************************************
*
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uniset_closure.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011may30
*   created by: Markus W. Scherer
*
*   UnicodeSet::closeOver() and related methods moved here from uniset_props.cpp
*   to simplify dependencies.
*   In particular, this depends on the BreakIterator, but the BreakIterator
*   code also builds UnicodeSets from patterns and needs uniset_props.
*/

#include "unicode/brkiter.h"
#include "unicode/locid.h"
#include "unicode/parsepos.h"
#include "unicode/uniset.h"
#include "cmemory.h"
#include "ruleiter.h"
#include "ucase.h"
#include "util.h"
#include "uvector.h"

// initial storage. Must be >= 0
// *** same as in uniset.cpp ! ***
#define START_EXTRA 16

U_NAMESPACE_BEGIN

// TODO memory debugging provided inside uniset.cpp
// could be made available here but probably obsolete with use of modern
// memory leak checker tools
#define _dbgct(me)

//----------------------------------------------------------------
// Constructors &c
//----------------------------------------------------------------

UnicodeSet::UnicodeSet(const UnicodeString& pattern,
                       uint32_t options,
                       const SymbolTable* symbols,
                       UErrorCode& status) :
    len(0), capacity(START_EXTRA), list(0), bmpSet(0), buffer(0),
    bufferCapacity(0), patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0)
{
    if(U_SUCCESS(status)){
        list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
        /* test for NULL */
        if(list == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }else{
            allocateStrings(status);
            applyPattern(pattern, options, symbols, status);
        }
    }
    _dbgct(this);
}

UnicodeSet::UnicodeSet(const UnicodeString& pattern, ParsePosition& pos,
                       uint32_t options,
                       const SymbolTable* symbols,
                       UErrorCode& status) :
    len(0), capacity(START_EXTRA), list(0), bmpSet(0), buffer(0),
    bufferCapacity(0), patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0)
{
    if(U_SUCCESS(status)){
        list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
        /* test for NULL */
        if(list == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }else{
            allocateStrings(status);
            applyPattern(pattern, pos, options, symbols, status);
        }
    }
    _dbgct(this);
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

UnicodeSet& UnicodeSet::applyPattern(const UnicodeString& pattern,
                                     uint32_t options,
                                     const SymbolTable* symbols,
                                     UErrorCode& status) {
    ParsePosition pos(0);
    applyPattern(pattern, pos, options, symbols, status);
    if (U_FAILURE(status)) return *this;

    int32_t i = pos.getIndex();

    if (options & USET_IGNORE_SPACE) {
        // Skip over trailing whitespace
        ICU_Utility::skipWhitespace(pattern, i, TRUE);
    }

    if (i != pattern.length()) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return *this;
}

UnicodeSet& UnicodeSet::applyPattern(const UnicodeString& pattern,
                              ParsePosition& pos,
                              uint32_t options,
                              const SymbolTable* symbols,
                              UErrorCode& status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (isFrozen()) {
        status = U_NO_WRITE_PERMISSION;
        return *this;
    }
    // Need to build the pattern in a temporary string because
    // _applyPattern calls add() etc., which set pat to empty.
    UnicodeString rebuiltPat;
    RuleCharacterIterator chars(pattern, symbols, pos);
    applyPattern(chars, symbols, rebuiltPat, options, &UnicodeSet::closeOver, status);
    if (U_FAILURE(status)) return *this;
    if (chars.inVariable()) {
        // syntaxError(chars, "Extra chars in variable value");
        status = U_MALFORMED_SET;
        return *this;
    }
    setPattern(rebuiltPat);
    return *this;
}

// USetAdder implementation
// Does not use uset.h to reduce code dependencies
static void U_CALLCONV
_set_add(USet *set, UChar32 c) {
    ((UnicodeSet *)set)->add(c);
}

static void U_CALLCONV
_set_addRange(USet *set, UChar32 start, UChar32 end) {
    ((UnicodeSet *)set)->add(start, end);
}

static void U_CALLCONV
_set_addString(USet *set, const UChar *str, int32_t length) {
    ((UnicodeSet *)set)->add(UnicodeString((UBool)(length<0), str, length));
}

//----------------------------------------------------------------
// Case folding API
//----------------------------------------------------------------

// add the result of a full case mapping to the set
// use str as a temporary string to avoid constructing one
static inline void
addCaseMapping(UnicodeSet &set, int32_t result, const UChar *full, UnicodeString &str) {
    if(result >= 0) {
        if(result > UCASE_MAX_STRING_LENGTH) {
            // add a single-code point case mapping
            set.add(result);
        } else {
            // add a string case mapping from full with length result
            str.setTo((UBool)FALSE, full, result);
            set.add(str);
        }
    }
    // result < 0: the code point mapped to itself, no need to add it
    // see ucase.h
}

UnicodeSet& UnicodeSet::closeOver(int32_t attribute) {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    if (attribute & (USET_CASE_INSENSITIVE | USET_ADD_CASE_MAPPINGS)) {
        const UCaseProps *csp = ucase_getSingleton();
        {
            UnicodeSet foldSet(*this);
            UnicodeString str;
            USetAdder sa = {
                foldSet.toUSet(),
                _set_add,
                _set_addRange,
                _set_addString,
                NULL, // don't need remove()
                NULL // don't need removeRange()
            };

            // start with input set to guarantee inclusion
            // USET_CASE: remove strings because the strings will actually be reduced (folded);
            //            therefore, start with no strings and add only those needed
            if (attribute & USET_CASE_INSENSITIVE) {
                foldSet.strings->removeAllElements();
            }

            int32_t n = getRangeCount();
            UChar32 result;
            const UChar *full;
            int32_t locCache = 0;

            for (int32_t i=0; i<n; ++i) {
                UChar32 start = getRangeStart(i);
                UChar32 end   = getRangeEnd(i);

                if (attribute & USET_CASE_INSENSITIVE) {
                    // full case closure
                    for (UChar32 cp=start; cp<=end; ++cp) {
                        ucase_addCaseClosure(csp, cp, &sa);
                    }
                } else {
                    // add case mappings
                    // (does not add long s for regular s, or Kelvin for k, for example)
                    for (UChar32 cp=start; cp<=end; ++cp) {
                        result = ucase_toFullLower(csp, cp, NULL, NULL, &full, "", &locCache);
                        addCaseMapping(foldSet, result, full, str);

                        result = ucase_toFullTitle(csp, cp, NULL, NULL, &full, "", &locCache);
                        addCaseMapping(foldSet, result, full, str);

                        result = ucase_toFullUpper(csp, cp, NULL, NULL, &full, "", &locCache);
                        addCaseMapping(foldSet, result, full, str);

                        result = ucase_toFullFolding(csp, cp, &full, 0);
                        addCaseMapping(foldSet, result, full, str);
                    }
                }
            }
            if (strings != NULL && strings->size() > 0) {
                if (attribute & USET_CASE_INSENSITIVE) {
                    for (int32_t j=0; j<strings->size(); ++j) {
                        str = *(const UnicodeString *) strings->elementAt(j);
                        str.foldCase();
                        if(!ucase_addStringCaseClosure(csp, str.getBuffer(), str.length(), &sa)) {
                            foldSet.add(str); // does not map to code points: add the folded string itself
                        }
                    }
                } else {
                    Locale root("");
#if !UCONFIG_NO_BREAK_ITERATION
                    UErrorCode status = U_ZERO_ERROR;
                    BreakIterator *bi = BreakIterator::createWordInstance(root, status);
                    if (U_SUCCESS(status)) {
#endif
                        const UnicodeString *pStr;

                        for (int32_t j=0; j<strings->size(); ++j) {
                            pStr = (const UnicodeString *) strings->elementAt(j);
                            (str = *pStr).toLower(root);
                            foldSet.add(str);
#if !UCONFIG_NO_BREAK_ITERATION
                            (str = *pStr).toTitle(bi, root);
                            foldSet.add(str);
#endif
                            (str = *pStr).toUpper(root);
                            foldSet.add(str);
                            (str = *pStr).foldCase();
                            foldSet.add(str);
                        }
#if !UCONFIG_NO_BREAK_ITERATION
                    }
                    delete bi;
#endif
                }
            }
            *this = foldSet;
        }
    }
    return *this;
}

U_NAMESPACE_END

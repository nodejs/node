// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************
* Copyright (c) 2002-2014, International Business Machines Corporation
* and others.  All Rights Reserved.
*****************************************************************
* Date        Name        Description
* 06/06/2002  aliu        Creation.
*****************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/uobject.h"
#include "unicode/uscript.h"

#include "anytrans.h"
#include "hash.h"
#include "mutex.h"
#include "nultrans.h"
#include "putilimp.h"
#include "tridpars.h"
#include "uinvchar.h"
#include "uvector.h"

//------------------------------------------------------------
// Constants

static const UChar TARGET_SEP = 45; // '-'
static const UChar VARIANT_SEP = 47; // '/'
static const UChar ANY[] = {65,110,121,0}; // "Any"
static const UChar NULL_ID[] = {78,117,108,108,0}; // "Null"
static const UChar LATIN_PIVOT[] = {45,76,97,116,105,110,59,76,97,116,105,110,45,0}; // "-Latin;Latin-"

//------------------------------------------------------------

U_CDECL_BEGIN
/**
 * Deleter function for Transliterator*.
 */
static void U_CALLCONV
_deleteTransliterator(void *obj) {
    delete (icu::Transliterator*) obj;
}
U_CDECL_END

//------------------------------------------------------------

U_NAMESPACE_BEGIN

//------------------------------------------------------------
// ScriptRunIterator

/**
 * Returns a series of ranges corresponding to scripts. They will be
 * of the form:
 *
 * ccccSScSSccccTTcTcccc   - c = common, S = first script, T = second
 * |            |          - first run (start, limit)
 *          |           |  - second run (start, limit)
 *
 * That is, the runs will overlap. The reason for this is so that a
 * transliterator can consider common characters both before and after
 * the scripts.
 */
class ScriptRunIterator : public UMemory {
private:
    const Replaceable& text;
    int32_t textStart;
    int32_t textLimit;

public:
    /**
     * The code of the current run, valid after next() returns.  May
     * be USCRIPT_INVALID_CODE if and only if the entire text is
     * COMMON/INHERITED.
     */
    UScriptCode scriptCode;

    /**
     * The start of the run, inclusive, valid after next() returns.
     */
    int32_t start;

    /**
     * The end of the run, exclusive, valid after next() returns.
     */
    int32_t limit;

    /**
     * Constructs a run iterator over the given text from start
     * (inclusive) to limit (exclusive).
     */
    ScriptRunIterator(const Replaceable& text, int32_t start, int32_t limit);

    /**
     * Returns TRUE if there are any more runs.  TRUE is always
     * returned at least once.  Upon return, the caller should
     * examine scriptCode, start, and limit.
     */
    UBool next();

    /**
     * Adjusts internal indices for a change in the limit index of the
     * given delta.  A positive delta means the limit has increased.
     */
    void adjustLimit(int32_t delta);

private:
    ScriptRunIterator(const ScriptRunIterator &other); // forbid copying of this class
    ScriptRunIterator &operator=(const ScriptRunIterator &other); // forbid copying of this class
};

ScriptRunIterator::ScriptRunIterator(const Replaceable& theText,
                                     int32_t myStart, int32_t myLimit) :
    text(theText)
{
    textStart = myStart;
    textLimit = myLimit;
    limit = myStart;
}

UBool ScriptRunIterator::next() {
    UChar32 ch;
    UScriptCode s;
    UErrorCode ec = U_ZERO_ERROR;

    scriptCode = USCRIPT_INVALID_CODE; // don't know script yet
    start = limit;

    // Are we done?
    if (start == textLimit) {
        return FALSE;
    }

    // Move start back to include adjacent COMMON or INHERITED
    // characters
    while (start > textStart) {
        ch = text.char32At(start - 1); // look back
        s = uscript_getScript(ch, &ec);
        if (s == USCRIPT_COMMON || s == USCRIPT_INHERITED) {
            --start;
        } else {
            break;
        }
    }

    // Move limit ahead to include COMMON, INHERITED, and characters
    // of the current script.
    while (limit < textLimit) {
        ch = text.char32At(limit); // look ahead
        s = uscript_getScript(ch, &ec);
        if (s != USCRIPT_COMMON && s != USCRIPT_INHERITED) {
            if (scriptCode == USCRIPT_INVALID_CODE) {
                scriptCode = s;
            } else if (s != scriptCode) {
                break;
            }
        }
        ++limit;
    }

    // Return TRUE even if the entire text is COMMON / INHERITED, in
    // which case scriptCode will be USCRIPT_INVALID_CODE.
    return TRUE;
}

void ScriptRunIterator::adjustLimit(int32_t delta) {
    limit += delta;
    textLimit += delta;
}

//------------------------------------------------------------
// AnyTransliterator

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(AnyTransliterator)

AnyTransliterator::AnyTransliterator(const UnicodeString& id,
                                     const UnicodeString& theTarget,
                                     const UnicodeString& theVariant,
                                     UScriptCode theTargetScript,
                                     UErrorCode& ec) :
    Transliterator(id, NULL),
    targetScript(theTargetScript)
{
    cache = uhash_open(uhash_hashLong, uhash_compareLong, NULL, &ec);
    if (U_FAILURE(ec)) {
        return;
    }
    uhash_setValueDeleter(cache, _deleteTransliterator);

    target = theTarget;
    if (theVariant.length() > 0) {
        target.append(VARIANT_SEP).append(theVariant);
    }
}

AnyTransliterator::~AnyTransliterator() {
    uhash_close(cache);
}

/**
 * Copy constructor.
 */
AnyTransliterator::AnyTransliterator(const AnyTransliterator& o) :
    Transliterator(o),
    target(o.target),
    targetScript(o.targetScript)
{
    // Don't copy the cache contents
    UErrorCode ec = U_ZERO_ERROR;
    cache = uhash_open(uhash_hashLong, uhash_compareLong, NULL, &ec);
    if (U_FAILURE(ec)) {
        return;
    }
    uhash_setValueDeleter(cache, _deleteTransliterator);
}

/**
 * Transliterator API.
 */
Transliterator* AnyTransliterator::clone() const {
    return new AnyTransliterator(*this);
}

/**
 * Implements {@link Transliterator#handleTransliterate}.
 */
void AnyTransliterator::handleTransliterate(Replaceable& text, UTransPosition& pos,
                                            UBool isIncremental) const {
    int32_t allStart = pos.start;
    int32_t allLimit = pos.limit;

    ScriptRunIterator it(text, pos.contextStart, pos.contextLimit);

    while (it.next()) {
        // Ignore runs in the ante context
        if (it.limit <= allStart) continue;

        // Try to instantiate transliterator from it.scriptCode to
        // our target or target/variant
        Transliterator* t = getTransliterator(it.scriptCode);

        if (t == NULL) {
            // We have no transliterator.  Do nothing, but keep
            // pos.start up to date.
            pos.start = it.limit;
            continue;
        }

        // If the run end is before the transliteration limit, do
        // a non-incremental transliteration.  Otherwise do an
        // incremental one.
        UBool incremental = isIncremental && (it.limit >= allLimit);

        pos.start = uprv_max(allStart, it.start);
        pos.limit = uprv_min(allLimit, it.limit);
        int32_t limit = pos.limit;
        t->filteredTransliterate(text, pos, incremental);
        int32_t delta = pos.limit - limit;
        allLimit += delta;
        it.adjustLimit(delta);

        // We're done if we enter the post context
        if (it.limit >= allLimit) break;
    }

    // Restore limit.  pos.start is fine where the last transliterator
    // left it, or at the end of the last run.
    pos.limit = allLimit;
}

Transliterator* AnyTransliterator::getTransliterator(UScriptCode source) const {

    if (source == targetScript || source == USCRIPT_INVALID_CODE) {
        return NULL;
    }

    Transliterator* t = NULL;
    {
        Mutex m(NULL);
        t = (Transliterator*) uhash_iget(cache, (int32_t) source);
    }
    if (t == NULL) {
        UErrorCode ec = U_ZERO_ERROR;
        UnicodeString sourceName(uscript_getName(source), -1, US_INV);
        UnicodeString id(sourceName);
        id.append(TARGET_SEP).append(target);

        t = Transliterator::createInstance(id, UTRANS_FORWARD, ec);
        if (U_FAILURE(ec) || t == NULL) {
            delete t;

            // Try to pivot around Latin, our most common script
            id = sourceName;
            id.append(LATIN_PIVOT, -1).append(target);
            t = Transliterator::createInstance(id, UTRANS_FORWARD, ec);
            if (U_FAILURE(ec) || t == NULL) {
                delete t;
                t = NULL;
            }
        }

        if (t != NULL) {
            Transliterator *rt = NULL;
            {
                Mutex m(NULL);
                rt = static_cast<Transliterator *> (uhash_iget(cache, (int32_t) source));
                if (rt == NULL) {
                    // Common case, no race to cache this new transliterator.
                    uhash_iput(cache, (int32_t) source, t, &ec);
                } else {
                    // Race case, some other thread beat us to caching this transliterator.
                    Transliterator *temp = rt;
                    rt = t;    // Our newly created transliterator that lost the race & now needs deleting.
                    t  = temp; // The transliterator from the cache that we will return.
                }
            }
            delete rt;    // will be non-null only in case of races.
        }
    }
    return t;
}

/**
 * Return the script code for a given name, or -1 if not found.
 */
static UScriptCode scriptNameToCode(const UnicodeString& name) {
    char buf[128];
    UScriptCode code;
    UErrorCode ec = U_ZERO_ERROR;
    int32_t nameLen = name.length();
    UBool isInvariant = uprv_isInvariantUString(name.getBuffer(), nameLen);

    if (isInvariant) {
        name.extract(0, nameLen, buf, (int32_t)sizeof(buf), US_INV);
        buf[127] = 0;   // Make sure that we NULL terminate the string.
    }
    if (!isInvariant || uscript_getCode(buf, &code, 1, &ec) != 1 || U_FAILURE(ec))
    {
        code = USCRIPT_INVALID_CODE;
    }
    return code;
}

/**
 * Registers standard transliterators with the system.  Called by
 * Transliterator during initialization.  Scan all current targets and
 * register those that are scripts T as Any-T/V.
 */
void AnyTransliterator::registerIDs() {

    UErrorCode ec = U_ZERO_ERROR;
    Hashtable seen(TRUE, ec);

    int32_t sourceCount = Transliterator::_countAvailableSources();
    for (int32_t s=0; s<sourceCount; ++s) {
        UnicodeString source;
        Transliterator::_getAvailableSource(s, source);

        // Ignore the "Any" source
        if (source.caseCompare(ANY, 3, 0 /*U_FOLD_CASE_DEFAULT*/) == 0) continue;

        int32_t targetCount = Transliterator::_countAvailableTargets(source);
        for (int32_t t=0; t<targetCount; ++t) {
            UnicodeString target;
            Transliterator::_getAvailableTarget(t, source, target);

            // Only process each target once
            if (seen.geti(target) != 0) continue;
            ec = U_ZERO_ERROR;
            seen.puti(target, 1, ec);

            // Get the script code for the target.  If not a script, ignore.
            UScriptCode targetScript = scriptNameToCode(target);
            if (targetScript == USCRIPT_INVALID_CODE) continue;

            int32_t variantCount = Transliterator::_countAvailableVariants(source, target);
            // assert(variantCount >= 1);
            for (int32_t v=0; v<variantCount; ++v) {
                UnicodeString variant;
                Transliterator::_getAvailableVariant(v, source, target, variant);

                UnicodeString id;
                TransliteratorIDParser::STVtoID(UnicodeString(TRUE, ANY, 3), target, variant, id);
                ec = U_ZERO_ERROR;
                AnyTransliterator* t = new AnyTransliterator(id, target, variant,
                                                             targetScript, ec);
                if (U_FAILURE(ec)) {
                    delete t;
                } else {
                    Transliterator::_registerInstance(t);
                    Transliterator::_registerSpecialInverse(target, UnicodeString(TRUE, NULL_ID, 4), FALSE);
                }
            }
        }
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

//eof

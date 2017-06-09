// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ************************************************************************************
 * Copyright (C) 2006-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 ************************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "brkeng.h"
#include "cmemory.h"
#include "dictbe.h"
#include "unicode/uchar.h"
#include "unicode/uniset.h"
#include "unicode/chariter.h"
#include "unicode/ures.h"
#include "unicode/udata.h"
#include "unicode/putil.h"
#include "unicode/ustring.h"
#include "unicode/uscript.h"
#include "unicode/ucharstrie.h"
#include "unicode/bytestrie.h"
#include "charstr.h"
#include "dictionarydata.h"
#include "mutex.h"
#include "uvector.h"
#include "umutex.h"
#include "uresimp.h"
#include "ubrkimpl.h"

U_NAMESPACE_BEGIN

/*
 ******************************************************************
 */

LanguageBreakEngine::LanguageBreakEngine() {
}

LanguageBreakEngine::~LanguageBreakEngine() {
}

/*
 ******************************************************************
 */

LanguageBreakFactory::LanguageBreakFactory() {
}

LanguageBreakFactory::~LanguageBreakFactory() {
}

/*
 ******************************************************************
 */

UnhandledEngine::UnhandledEngine(UErrorCode &/*status*/) {
    for (int32_t i = 0; i < UPRV_LENGTHOF(fHandled); ++i) {
        fHandled[i] = 0;
    }
}

UnhandledEngine::~UnhandledEngine() {
    for (int32_t i = 0; i < UPRV_LENGTHOF(fHandled); ++i) {
        if (fHandled[i] != 0) {
            delete fHandled[i];
        }
    }
}

UBool
UnhandledEngine::handles(UChar32 c, int32_t breakType) const {
    return (breakType >= 0 && breakType < UPRV_LENGTHOF(fHandled)
        && fHandled[breakType] != 0 && fHandled[breakType]->contains(c));
}

int32_t
UnhandledEngine::findBreaks( UText *text,
                                 int32_t startPos,
                                 int32_t endPos,
                                 UBool reverse,
                                 int32_t breakType,
                                 UStack &/*foundBreaks*/ ) const {
    if (breakType >= 0 && breakType < UPRV_LENGTHOF(fHandled)) {
        UChar32 c = utext_current32(text);
        if (reverse) {
            while((int32_t)utext_getNativeIndex(text) > startPos && fHandled[breakType]->contains(c)) {
                c = utext_previous32(text);
            }
        }
        else {
            while((int32_t)utext_getNativeIndex(text) < endPos && fHandled[breakType]->contains(c)) {
                utext_next32(text);            // TODO:  recast loop to work with post-increment operations.
                c = utext_current32(text);
            }
        }
    }
    return 0;
}

void
UnhandledEngine::handleCharacter(UChar32 c, int32_t breakType) {
    if (breakType >= 0 && breakType < UPRV_LENGTHOF(fHandled)) {
        if (fHandled[breakType] == 0) {
            fHandled[breakType] = new UnicodeSet();
            if (fHandled[breakType] == 0) {
                return;
            }
        }
        if (!fHandled[breakType]->contains(c)) {
            UErrorCode status = U_ZERO_ERROR;
            // Apply the entire script of the character.
            int32_t script = u_getIntPropertyValue(c, UCHAR_SCRIPT);
            fHandled[breakType]->applyIntPropertyValue(UCHAR_SCRIPT, script, status);
        }
    }
}

/*
 ******************************************************************
 */

ICULanguageBreakFactory::ICULanguageBreakFactory(UErrorCode &/*status*/) {
    fEngines = 0;
}

ICULanguageBreakFactory::~ICULanguageBreakFactory() {
    if (fEngines != 0) {
        delete fEngines;
    }
}

U_NAMESPACE_END
U_CDECL_BEGIN
static void U_CALLCONV _deleteEngine(void *obj) {
    delete (const icu::LanguageBreakEngine *) obj;
}
U_CDECL_END
U_NAMESPACE_BEGIN

static UMutex gBreakEngineMutex = U_MUTEX_INITIALIZER;

const LanguageBreakEngine *
ICULanguageBreakFactory::getEngineFor(UChar32 c, int32_t breakType) {
    const LanguageBreakEngine *lbe = NULL;
    UErrorCode  status = U_ZERO_ERROR;

    Mutex m(&gBreakEngineMutex);

    if (fEngines == NULL) {
        UStack  *engines = new UStack(_deleteEngine, NULL, status);
        if (U_FAILURE(status) || engines == NULL) {
            // Note: no way to return error code to caller.
            delete engines;
            return NULL;
        }
        fEngines = engines;
    } else {
        int32_t i = fEngines->size();
        while (--i >= 0) {
            lbe = (const LanguageBreakEngine *)(fEngines->elementAt(i));
            if (lbe != NULL && lbe->handles(c, breakType)) {
                return lbe;
            }
        }
    }

    // We didn't find an engine. Create one.
    lbe = loadEngineFor(c, breakType);
    if (lbe != NULL) {
        fEngines->push((void *)lbe, status);
    }
    return lbe;
}

const LanguageBreakEngine *
ICULanguageBreakFactory::loadEngineFor(UChar32 c, int32_t breakType) {
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode code = uscript_getScript(c, &status);
    if (U_SUCCESS(status)) {
        DictionaryMatcher *m = loadDictionaryMatcherFor(code, breakType);
        if (m != NULL) {
            const LanguageBreakEngine *engine = NULL;
            switch(code) {
            case USCRIPT_THAI:
                engine = new ThaiBreakEngine(m, status);
                break;
            case USCRIPT_LAO:
                engine = new LaoBreakEngine(m, status);
                break;
            case USCRIPT_MYANMAR:
                engine = new BurmeseBreakEngine(m, status);
                break;
            case USCRIPT_KHMER:
                engine = new KhmerBreakEngine(m, status);
                break;

#if !UCONFIG_NO_NORMALIZATION
                // CJK not available w/o normalization
            case USCRIPT_HANGUL:
                engine = new CjkBreakEngine(m, kKorean, status);
                break;

            // use same BreakEngine and dictionary for both Chinese and Japanese
            case USCRIPT_HIRAGANA:
            case USCRIPT_KATAKANA:
            case USCRIPT_HAN:
                engine = new CjkBreakEngine(m, kChineseJapanese, status);
                break;
#if 0
            // TODO: Have to get some characters with script=common handled
            // by CjkBreakEngine (e.g. U+309B). Simply subjecting
            // them to CjkBreakEngine does not work. The engine has to
            // special-case them.
            case USCRIPT_COMMON:
            {
                UBlockCode block = ublock_getCode(code);
                if (block == UBLOCK_HIRAGANA || block == UBLOCK_KATAKANA)
                   engine = new CjkBreakEngine(dict, kChineseJapanese, status);
                break;
            }
#endif
#endif

            default:
                break;
            }
            if (engine == NULL) {
                delete m;
            }
            else if (U_FAILURE(status)) {
                delete engine;
                engine = NULL;
            }
            return engine;
        }
    }
    return NULL;
}

DictionaryMatcher *
ICULanguageBreakFactory::loadDictionaryMatcherFor(UScriptCode script, int32_t /* brkType */) {
    UErrorCode status = U_ZERO_ERROR;
    // open root from brkitr tree.
    UResourceBundle *b = ures_open(U_ICUDATA_BRKITR, "", &status);
    b = ures_getByKeyWithFallback(b, "dictionaries", b, &status);
    int32_t dictnlength = 0;
    const UChar *dictfname =
        ures_getStringByKeyWithFallback(b, uscript_getShortName(script), &dictnlength, &status);
    if (U_FAILURE(status)) {
        ures_close(b);
        return NULL;
    }
    CharString dictnbuf;
    CharString ext;
    const UChar *extStart = u_memrchr(dictfname, 0x002e, dictnlength);  // last dot
    if (extStart != NULL) {
        int32_t len = (int32_t)(extStart - dictfname);
        ext.appendInvariantChars(UnicodeString(FALSE, extStart + 1, dictnlength - len - 1), status);
        dictnlength = len;
    }
    dictnbuf.appendInvariantChars(UnicodeString(FALSE, dictfname, dictnlength), status);
    ures_close(b);

    UDataMemory *file = udata_open(U_ICUDATA_BRKITR, ext.data(), dictnbuf.data(), &status);
    if (U_SUCCESS(status)) {
        // build trie
        const uint8_t *data = (const uint8_t *)udata_getMemory(file);
        const int32_t *indexes = (const int32_t *)data;
        const int32_t offset = indexes[DictionaryData::IX_STRING_TRIE_OFFSET];
        const int32_t trieType = indexes[DictionaryData::IX_TRIE_TYPE] & DictionaryData::TRIE_TYPE_MASK;
        DictionaryMatcher *m = NULL;
        if (trieType == DictionaryData::TRIE_TYPE_BYTES) {
            const int32_t transform = indexes[DictionaryData::IX_TRANSFORM];
            const char *characters = (const char *)(data + offset);
            m = new BytesDictionaryMatcher(characters, transform, file);
        }
        else if (trieType == DictionaryData::TRIE_TYPE_UCHARS) {
            const UChar *characters = (const UChar *)(data + offset);
            m = new UCharsDictionaryMatcher(characters, file);
        }
        if (m == NULL) {
            // no matcher exists to take ownership - either we are an invalid
            // type or memory allocation failed
            udata_close(file);
        }
        return m;
    } else if (dictfname != NULL) {
        // we don't have a dictionary matcher.
        // returning NULL here will cause us to fail to find a dictionary break engine, as expected
        status = U_ZERO_ERROR;
        return NULL;
    }
    return NULL;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

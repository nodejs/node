/*
**********************************************************************
*   Copyright (C) 2012-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"

#include "unicode/uchar.h"
#include "unicode/utf16.h"

#include "identifier_info.h"
#include "mutex.h"
#include "scriptset.h"
#include "ucln_in.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

static UnicodeSet *ASCII;
static ScriptSet *JAPANESE;
static ScriptSet *CHINESE;
static ScriptSet *KOREAN;
static ScriptSet *CONFUSABLE_WITH_LATIN;
static UInitOnce gIdentifierInfoInitOnce = U_INITONCE_INITIALIZER;


U_CDECL_BEGIN
static UBool U_CALLCONV
IdentifierInfo_cleanup(void) {
    delete ASCII;
    ASCII = NULL;
    delete JAPANESE;
    JAPANESE = NULL;
    delete CHINESE;
    CHINESE = NULL;
    delete KOREAN;
    KOREAN = NULL;
    delete CONFUSABLE_WITH_LATIN;
    CONFUSABLE_WITH_LATIN = NULL;
    gIdentifierInfoInitOnce.reset();
    return TRUE;
}

static void U_CALLCONV
IdentifierInfo_init(UErrorCode &status) {
    ASCII    = new UnicodeSet(0, 0x7f);
    JAPANESE = new ScriptSet();
    CHINESE  = new ScriptSet();
    KOREAN   = new ScriptSet();
    CONFUSABLE_WITH_LATIN = new ScriptSet();
    if (ASCII == NULL || JAPANESE == NULL || CHINESE == NULL || KOREAN == NULL
            || CONFUSABLE_WITH_LATIN == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    ASCII->freeze();
    JAPANESE->set(USCRIPT_LATIN, status).set(USCRIPT_HAN, status).set(USCRIPT_HIRAGANA, status)
             .set(USCRIPT_KATAKANA, status);
    CHINESE->set(USCRIPT_LATIN, status).set(USCRIPT_HAN, status).set(USCRIPT_BOPOMOFO, status);
    KOREAN->set(USCRIPT_LATIN, status).set(USCRIPT_HAN, status).set(USCRIPT_HANGUL, status);
    CONFUSABLE_WITH_LATIN->set(USCRIPT_CYRILLIC, status).set(USCRIPT_GREEK, status)
              .set(USCRIPT_CHEROKEE, status);
    ucln_i18n_registerCleanup(UCLN_I18N_IDENTIFIER_INFO, IdentifierInfo_cleanup);
}
U_CDECL_END


IdentifierInfo::IdentifierInfo(UErrorCode &status):
         fIdentifier(NULL), fRequiredScripts(NULL), fScriptSetSet(NULL),
         fCommonAmongAlternates(NULL), fNumerics(NULL), fIdentifierProfile(NULL) {
    umtx_initOnce(gIdentifierInfoInitOnce, &IdentifierInfo_init, status);
    if (U_FAILURE(status)) {
        return;
    }

    fIdentifier = new UnicodeString();
    fRequiredScripts = new ScriptSet();
    fScriptSetSet = uhash_open(uhash_hashScriptSet, uhash_compareScriptSet, NULL, &status);
    uhash_setKeyDeleter(fScriptSetSet, uhash_deleteScriptSet);
    fCommonAmongAlternates = new ScriptSet();
    fNumerics = new UnicodeSet();
    fIdentifierProfile = new UnicodeSet(0, 0x10FFFF);

    if (U_SUCCESS(status) && (fIdentifier == NULL || fRequiredScripts == NULL || fScriptSetSet == NULL ||
                              fCommonAmongAlternates == NULL || fNumerics == NULL || fIdentifierProfile == NULL)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}

IdentifierInfo::~IdentifierInfo() {
    delete fIdentifier;
    delete fRequiredScripts;
    uhash_close(fScriptSetSet);
    delete fCommonAmongAlternates;
    delete fNumerics;
    delete fIdentifierProfile;
}


IdentifierInfo &IdentifierInfo::clear() {
    fRequiredScripts->resetAll();
    uhash_removeAll(fScriptSetSet);
    fNumerics->clear();
    fCommonAmongAlternates->resetAll();
    return *this;
}


IdentifierInfo &IdentifierInfo::setIdentifierProfile(const UnicodeSet &identifierProfile) {
    *fIdentifierProfile = identifierProfile;
    return *this;
}


const UnicodeSet &IdentifierInfo::getIdentifierProfile() const {
    return *fIdentifierProfile;
}


IdentifierInfo &IdentifierInfo::setIdentifier(const UnicodeString &identifier, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    *fIdentifier = identifier;
    clear();
    ScriptSet scriptsForCP;
    UChar32 cp;
    for (int32_t i = 0; i < identifier.length(); i += U16_LENGTH(cp)) {
        cp = identifier.char32At(i);
        // Store a representative character for each kind of decimal digit
        if (u_charType(cp) == U_DECIMAL_DIGIT_NUMBER) {
            // Just store the zero character as a representative for comparison. Unicode guarantees it is cp - value
            fNumerics->add(cp - (UChar32)u_getNumericValue(cp));
        }
        UScriptCode extensions[500];
        int32_t extensionsCount = uscript_getScriptExtensions(cp, extensions, UPRV_LENGTHOF(extensions), &status);
        if (U_FAILURE(status)) {
            return *this;
        }
        scriptsForCP.resetAll();
        for (int32_t j=0; j<extensionsCount; j++) {
            scriptsForCP.set(extensions[j], status);
        }
        scriptsForCP.reset(USCRIPT_COMMON, status);
        scriptsForCP.reset(USCRIPT_INHERITED, status);
        switch (scriptsForCP.countMembers()) {
          case 0: break;
          case 1:
            // Single script, record it.
            fRequiredScripts->Union(scriptsForCP);
            break;
          default:
            if (!fRequiredScripts->intersects(scriptsForCP)
                    && !uhash_geti(fScriptSetSet, &scriptsForCP)) {
                // If the set hasn't been added already, add it
                //    (Add a copy, fScriptSetSet takes ownership of the copy.)
                uhash_puti(fScriptSetSet, new ScriptSet(scriptsForCP), 1, &status);
            }
            break;
        }
    }
    // Now make a final pass through ScriptSetSet to remove alternates that came before singles.
    // [Kana], [Kana Hira] => [Kana]
    // This is relatively infrequent, so doesn't have to be optimized.
    // We also compute any commonalities among the alternates.
    if (uhash_count(fScriptSetSet) > 0) {
        fCommonAmongAlternates->setAll();
        for (int32_t it = UHASH_FIRST;;) {
            const UHashElement *nextHashEl = uhash_nextElement(fScriptSetSet, &it);
            if (nextHashEl == NULL) {
                break;
            }
            ScriptSet *next = static_cast<ScriptSet *>(nextHashEl->key.pointer);
            // [Kana], [Kana Hira] => [Kana]
            if (fRequiredScripts->intersects(*next)) {
                uhash_removeElement(fScriptSetSet, nextHashEl);
            } else {
                fCommonAmongAlternates->intersect(*next);
                // [[Arab Syrc Thaa]; [Arab Syrc]] => [[Arab Syrc]]
                for (int32_t otherIt = UHASH_FIRST;;) {
                    const UHashElement *otherHashEl = uhash_nextElement(fScriptSetSet, &otherIt);
                    if (otherHashEl == NULL) {
                        break;
                    }
                    ScriptSet *other = static_cast<ScriptSet *>(otherHashEl->key.pointer);
                    if (next != other && next->contains(*other)) {
                        uhash_removeElement(fScriptSetSet, nextHashEl);
                        break;
                    }
                }
            }
        }
    }
    if (uhash_count(fScriptSetSet) == 0) {
        fCommonAmongAlternates->resetAll();
    }
    return *this;
}


const UnicodeString *IdentifierInfo::getIdentifier() const {
    return fIdentifier;
}

const ScriptSet *IdentifierInfo::getScripts() const {
    return fRequiredScripts;
}

const UHashtable *IdentifierInfo::getAlternates() const {
    return fScriptSetSet;
}


const UnicodeSet *IdentifierInfo::getNumerics() const {
    return fNumerics;
}

const ScriptSet *IdentifierInfo::getCommonAmongAlternates() const {
    return fCommonAmongAlternates;
}

#if !UCONFIG_NO_NORMALIZATION

URestrictionLevel IdentifierInfo::getRestrictionLevel(UErrorCode &status) const {
    if (!fIdentifierProfile->containsAll(*fIdentifier) || getNumerics()->size() > 1) {
        return USPOOF_UNRESTRICTIVE;
    }
    if (ASCII->containsAll(*fIdentifier)) {
        return USPOOF_ASCII;
    }
    // This is a bit tricky. We look at a number of factors.
    // The number of scripts in the text.
    // Plus 1 if there is some commonality among the alternates (eg [Arab Thaa]; [Arab Syrc])
    // Plus number of alternates otherwise (this only works because we only test cardinality up to 2.)

    // Note: the requiredScripts set omits COMMON and INHERITED; they are taken out at the
    //       time it is created, in setIdentifier().
    int32_t cardinalityPlus = fRequiredScripts->countMembers() +
            (fCommonAmongAlternates->countMembers() == 0 ? uhash_count(fScriptSetSet) : 1);
    if (cardinalityPlus < 2) {
        return USPOOF_SINGLE_SCRIPT_RESTRICTIVE;
    }
    if (containsWithAlternates(*JAPANESE, *fRequiredScripts) || containsWithAlternates(*CHINESE, *fRequiredScripts)
            || containsWithAlternates(*KOREAN, *fRequiredScripts)) {
        return USPOOF_HIGHLY_RESTRICTIVE;
    }
    if (cardinalityPlus == 2 &&
            fRequiredScripts->test(USCRIPT_LATIN, status) &&
            !fRequiredScripts->intersects(*CONFUSABLE_WITH_LATIN)) {
        return USPOOF_MODERATELY_RESTRICTIVE;
    }
    return USPOOF_MINIMALLY_RESTRICTIVE;
}

#endif /* !UCONFIG_NO_NORMALIZATION */

int32_t IdentifierInfo::getScriptCount() const {
    // Note: Common and Inherited scripts were removed by setIdentifier(), and do not appear in fRequiredScripts.
    int32_t count = fRequiredScripts->countMembers() +
            (fCommonAmongAlternates->countMembers() == 0 ? uhash_count(fScriptSetSet) : 1);
    return count;
}



UBool IdentifierInfo::containsWithAlternates(const ScriptSet &container, const ScriptSet &containee) const {
    if (!container.contains(containee)) {
        return FALSE;
    }
    for (int32_t iter = UHASH_FIRST; ;) {
        const UHashElement *hashEl = uhash_nextElement(fScriptSetSet, &iter);
        if (hashEl == NULL) {
            break;
        }
        ScriptSet *alternatives = static_cast<ScriptSet *>(hashEl->key.pointer);
        if (!container.intersects(*alternatives)) {
            return false;
        }
    }
    return true;
}

UnicodeString &IdentifierInfo::displayAlternates(UnicodeString &dest, const UHashtable *alternates, UErrorCode &status) {
    UVector sorted(status);
    if (U_FAILURE(status)) {
        return dest;
    }
    for (int32_t pos = UHASH_FIRST; ;) {
        const UHashElement *el = uhash_nextElement(alternates, &pos);
        if (el == NULL) {
            break;
        }
        ScriptSet *ss = static_cast<ScriptSet *>(el->key.pointer);
        sorted.addElement(ss, status);
    }
    sorted.sort(uhash_compareScriptSet, status);
    UnicodeString separator = UNICODE_STRING_SIMPLE("; ");
    for (int32_t i=0; i<sorted.size(); i++) {
        if (i>0) {
            dest.append(separator);
        }
        ScriptSet *ss = static_cast<ScriptSet *>(sorted.elementAt(i));
        ss->displayScripts(dest);
    }
    return dest;
}

U_NAMESPACE_END

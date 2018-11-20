// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2009-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/ures.h"
#include "unicode/putil.h"
#include "unicode/uloc.h"
#include "ustr_imp.h"
#include "cmemory.h"
#include "cstring.h"
#include "putilimp.h"
#include "uinvchar.h"
#include "ulocimp.h"
#include "uassert.h"


/* struct holding a single variant */
typedef struct VariantListEntry {
    const char              *variant;
    struct VariantListEntry *next;
} VariantListEntry;

/* struct holding a single attribute value */
typedef struct AttributeListEntry {
    const char              *attribute;
    struct AttributeListEntry *next;
} AttributeListEntry;

/* struct holding a single extension */
typedef struct ExtensionListEntry {
    const char                  *key;
    const char                  *value;
    struct ExtensionListEntry   *next;
} ExtensionListEntry;

#define MAXEXTLANG 3
typedef struct ULanguageTag {
    char                *buf;   /* holding parsed subtags */
    const char          *language;
    const char          *extlang[MAXEXTLANG];
    const char          *script;
    const char          *region;
    VariantListEntry    *variants;
    ExtensionListEntry  *extensions;
    const char          *privateuse;
    const char          *grandfathered;
} ULanguageTag;

#define MINLEN 2
#define SEP '-'
#define PRIVATEUSE 'x'
#define LDMLEXT 'u'

#define LOCALE_SEP '_'
#define LOCALE_EXT_SEP '@'
#define LOCALE_KEYWORD_SEP ';'
#define LOCALE_KEY_TYPE_SEP '='

#define ISALPHA(c) uprv_isASCIILetter(c)
#define ISNUMERIC(c) ((c)>='0' && (c)<='9')

static const char EMPTY[] = "";
static const char LANG_UND[] = "und";
static const char PRIVATEUSE_KEY[] = "x";
static const char _POSIX[] = "_POSIX";
static const char POSIX_KEY[] = "va";
static const char POSIX_VALUE[] = "posix";
static const char LOCALE_ATTRIBUTE_KEY[] = "attribute";
static const char PRIVUSE_VARIANT_PREFIX[] = "lvariant";
static const char LOCALE_TYPE_YES[] = "yes";

#define LANG_UND_LEN 3

static const char* const GRANDFATHERED[] = {
/*  grandfathered   preferred */
    "art-lojban",   "jbo",
    "cel-gaulish",  "xtg-x-cel-gaulish",
    "en-GB-oed",    "en-GB-x-oed",
    "i-ami",        "ami",
    "i-bnn",        "bnn",
    "i-default",    "en-x-i-default",
    "i-enochian",   "und-x-i-enochian",
    "i-hak",        "hak",
    "i-klingon",    "tlh",
    "i-lux",        "lb",
    "i-mingo",      "see-x-i-mingo",
    "i-navajo",     "nv",
    "i-pwn",        "pwn",
    "i-tao",        "tao",
    "i-tay",        "tay",
    "i-tsu",        "tsu",
    "no-bok",       "nb",
    "no-nyn",       "nn",
    "sgn-be-fr",    "sfb",
    "sgn-be-nl",    "vgt",
    "sgn-ch-de",    "sgg",
    "zh-guoyu",     "cmn",
    "zh-hakka",     "hak",
    "zh-min",       "nan-x-zh-min",
    "zh-min-nan",   "nan",
    "zh-xiang",     "hsn",
    NULL,           NULL
};

static const char DEPRECATEDLANGS[][4] = {
/*  deprecated  new */
    "iw",       "he",
    "ji",       "yi",
    "in",       "id"
};

/*
* -------------------------------------------------
*
* These ultag_ functions may be exposed as APIs later
*
* -------------------------------------------------
*/

static ULanguageTag*
ultag_parse(const char* tag, int32_t tagLen, int32_t* parsedLen, UErrorCode* status);

static void
ultag_close(ULanguageTag* langtag);

static const char*
ultag_getLanguage(const ULanguageTag* langtag);

#if 0
static const char*
ultag_getJDKLanguage(const ULanguageTag* langtag);
#endif

static const char*
ultag_getExtlang(const ULanguageTag* langtag, int32_t idx);

static int32_t
ultag_getExtlangSize(const ULanguageTag* langtag);

static const char*
ultag_getScript(const ULanguageTag* langtag);

static const char*
ultag_getRegion(const ULanguageTag* langtag);

static const char*
ultag_getVariant(const ULanguageTag* langtag, int32_t idx);

static int32_t
ultag_getVariantsSize(const ULanguageTag* langtag);

static const char*
ultag_getExtensionKey(const ULanguageTag* langtag, int32_t idx);

static const char*
ultag_getExtensionValue(const ULanguageTag* langtag, int32_t idx);

static int32_t
ultag_getExtensionsSize(const ULanguageTag* langtag);

static const char*
ultag_getPrivateUse(const ULanguageTag* langtag);

#if 0
static const char*
ultag_getGrandfathered(const ULanguageTag* langtag);
#endif

/*
* -------------------------------------------------
*
* Language subtag syntax validation functions
*
* -------------------------------------------------
*/

static UBool
_isAlphaString(const char* s, int32_t len) {
    int32_t i;
    for (i = 0; i < len; i++) {
        if (!ISALPHA(*(s + i))) {
            return FALSE;
        }
    }
    return TRUE;
}

static UBool
_isNumericString(const char* s, int32_t len) {
    int32_t i;
    for (i = 0; i < len; i++) {
        if (!ISNUMERIC(*(s + i))) {
            return FALSE;
        }
    }
    return TRUE;
}

static UBool
_isAlphaNumericString(const char* s, int32_t len) {
    int32_t i;
    for (i = 0; i < len; i++) {
        if (!ISALPHA(*(s + i)) && !ISNUMERIC(*(s + i))) {
            return FALSE;
        }
    }
    return TRUE;
}

static UBool
_isLanguageSubtag(const char* s, int32_t len) {
    /*
     * language      = 2*3ALPHA            ; shortest ISO 639 code
     *                 ["-" extlang]       ; sometimes followed by
     *                                     ;   extended language subtags
     *               / 4ALPHA              ; or reserved for future use
     *               / 5*8ALPHA            ; or registered language subtag
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len >= 2 && len <= 8 && _isAlphaString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isExtlangSubtag(const char* s, int32_t len) {
    /*
     * extlang       = 3ALPHA              ; selected ISO 639 codes
     *                 *2("-" 3ALPHA)      ; permanently reserved
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len == 3 && _isAlphaString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isScriptSubtag(const char* s, int32_t len) {
    /*
     * script        = 4ALPHA              ; ISO 15924 code
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len == 4 && _isAlphaString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isRegionSubtag(const char* s, int32_t len) {
    /*
     * region        = 2ALPHA              ; ISO 3166-1 code
     *               / 3DIGIT              ; UN M.49 code
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len == 2 && _isAlphaString(s, len)) {
        return TRUE;
    }
    if (len == 3 && _isNumericString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isVariantSubtag(const char* s, int32_t len) {
    /*
     * variant       = 5*8alphanum         ; registered variants
     *               / (DIGIT 3alphanum)
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len >= 5 && len <= 8 && _isAlphaNumericString(s, len)) {
        return TRUE;
    }
    if (len == 4 && ISNUMERIC(*s) && _isAlphaNumericString(s + 1, 3)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isPrivateuseVariantSubtag(const char* s, int32_t len) {
    /*
     * variant       = 1*8alphanum         ; registered variants
     *               / (DIGIT 3alphanum)
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len >= 1 && len <= 8 && _isAlphaNumericString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isExtensionSingleton(const char* s, int32_t len) {
    /*
     * extension     = singleton 1*("-" (2*8alphanum))
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len == 1 && ISALPHA(*s) && (uprv_tolower(*s) != PRIVATEUSE)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isExtensionSubtag(const char* s, int32_t len) {
    /*
     * extension     = singleton 1*("-" (2*8alphanum))
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len >= 2 && len <= 8 && _isAlphaNumericString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isExtensionSubtags(const char* s, int32_t len) {
    const char *p = s;
    const char *pSubtag = NULL;

    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }

    while ((p - s) < len) {
        if (*p == SEP) {
            if (pSubtag == NULL) {
                return FALSE;
            }
            if (!_isExtensionSubtag(pSubtag, (int32_t)(p - pSubtag))) {
                return FALSE;
            }
            pSubtag = NULL;
        } else if (pSubtag == NULL) {
            pSubtag = p;
        }
        p++;
    }
    if (pSubtag == NULL) {
        return FALSE;
    }
    return _isExtensionSubtag(pSubtag, (int32_t)(p - pSubtag));
}

static UBool
_isPrivateuseValueSubtag(const char* s, int32_t len) {
    /*
     * privateuse    = "x" 1*("-" (1*8alphanum))
     */
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len >= 1 && len <= 8 && _isAlphaNumericString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

static UBool
_isPrivateuseValueSubtags(const char* s, int32_t len) {
    const char *p = s;
    const char *pSubtag = NULL;

    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }

    while ((p - s) < len) {
        if (*p == SEP) {
            if (pSubtag == NULL) {
                return FALSE;
            }
            if (!_isPrivateuseValueSubtag(pSubtag, (int32_t)(p - pSubtag))) {
                return FALSE;
            }
            pSubtag = NULL;
        } else if (pSubtag == NULL) {
            pSubtag = p;
        }
        p++;
    }
    if (pSubtag == NULL) {
        return FALSE;
    }
    return _isPrivateuseValueSubtag(pSubtag, (int32_t)(p - pSubtag));
}

U_CFUNC UBool
ultag_isUnicodeLocaleKey(const char* s, int32_t len) {
    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }
    if (len == 2 && _isAlphaNumericString(s, len)) {
        return TRUE;
    }
    return FALSE;
}

U_CFUNC UBool
ultag_isUnicodeLocaleType(const char*s, int32_t len) {
    const char* p;
    int32_t subtagLen = 0;

    if (len < 0) {
        len = (int32_t)uprv_strlen(s);
    }

    for (p = s; len > 0; p++, len--) {
        if (*p == SEP) {
            if (subtagLen < 3) {
                return FALSE;
            }
            subtagLen = 0;
        } else if (ISALPHA(*p) || ISNUMERIC(*p)) {
            subtagLen++;
            if (subtagLen > 8) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    return (subtagLen >= 3);
}
/*
* -------------------------------------------------
*
* Helper functions
*
* -------------------------------------------------
*/

static UBool
_addVariantToList(VariantListEntry **first, VariantListEntry *var) {
    UBool bAdded = TRUE;

    if (*first == NULL) {
        var->next = NULL;
        *first = var;
    } else {
        VariantListEntry *prev, *cur;
        int32_t cmp;

        /* variants order should be preserved */
        prev = NULL;
        cur = *first;
        while (TRUE) {
            if (cur == NULL) {
                prev->next = var;
                var->next = NULL;
                break;
            }

            /* Checking for duplicate variant */
            cmp = uprv_compareInvCharsAsAscii(var->variant, cur->variant);
            if (cmp == 0) {
                /* duplicated variant */
                bAdded = FALSE;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }

    return bAdded;
}

static UBool
_addAttributeToList(AttributeListEntry **first, AttributeListEntry *attr) {
    UBool bAdded = TRUE;

    if (*first == NULL) {
        attr->next = NULL;
        *first = attr;
    } else {
        AttributeListEntry *prev, *cur;
        int32_t cmp;

        /* reorder variants in alphabetical order */
        prev = NULL;
        cur = *first;
        while (TRUE) {
            if (cur == NULL) {
                prev->next = attr;
                attr->next = NULL;
                break;
            }
            cmp = uprv_compareInvCharsAsAscii(attr->attribute, cur->attribute);
            if (cmp < 0) {
                if (prev == NULL) {
                    *first = attr;
                } else {
                    prev->next = attr;
                }
                attr->next = cur;
                break;
            }
            if (cmp == 0) {
                /* duplicated variant */
                bAdded = FALSE;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }

    return bAdded;
}


static UBool
_addExtensionToList(ExtensionListEntry **first, ExtensionListEntry *ext, UBool localeToBCP) {
    UBool bAdded = TRUE;

    if (*first == NULL) {
        ext->next = NULL;
        *first = ext;
    } else {
        ExtensionListEntry *prev, *cur;
        int32_t cmp;

        /* reorder variants in alphabetical order */
        prev = NULL;
        cur = *first;
        while (TRUE) {
            if (cur == NULL) {
                prev->next = ext;
                ext->next = NULL;
                break;
            }
            if (localeToBCP) {
                /* special handling for locale to bcp conversion */
                int32_t len, curlen;

                len = (int32_t)uprv_strlen(ext->key);
                curlen = (int32_t)uprv_strlen(cur->key);

                if (len == 1 && curlen == 1) {
                    if (*(ext->key) == *(cur->key)) {
                        cmp = 0;
                    } else if (*(ext->key) == PRIVATEUSE) {
                        cmp = 1;
                    } else if (*(cur->key) == PRIVATEUSE) {
                        cmp = -1;
                    } else {
                        cmp = *(ext->key) - *(cur->key);
                    }
                } else if (len == 1) {
                    cmp = *(ext->key) - LDMLEXT;
                } else if (curlen == 1) {
                    cmp = LDMLEXT - *(cur->key);
                } else {
                    cmp = uprv_compareInvCharsAsAscii(ext->key, cur->key);
                    /* Both are u extension keys - we need special handling for 'attribute' */
                    if (cmp != 0) {
                        if (uprv_strcmp(cur->key, LOCALE_ATTRIBUTE_KEY) == 0) {
                            cmp = 1;
                        } else if (uprv_strcmp(ext->key, LOCALE_ATTRIBUTE_KEY) == 0) {
                            cmp = -1;
                        }
                    }
                }
            } else {
                cmp = uprv_compareInvCharsAsAscii(ext->key, cur->key);
            }
            if (cmp < 0) {
                if (prev == NULL) {
                    *first = ext;
                } else {
                    prev->next = ext;
                }
                ext->next = cur;
                break;
            }
            if (cmp == 0) {
                /* duplicated extension key */
                bAdded = FALSE;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }

    return bAdded;
}

static void
_initializeULanguageTag(ULanguageTag* langtag) {
    int32_t i;

    langtag->buf = NULL;

    langtag->language = EMPTY;
    for (i = 0; i < MAXEXTLANG; i++) {
        langtag->extlang[i] = NULL;
    }

    langtag->script = EMPTY;
    langtag->region = EMPTY;

    langtag->variants = NULL;
    langtag->extensions = NULL;

    langtag->grandfathered = EMPTY;
    langtag->privateuse = EMPTY;
}

static int32_t
_appendLanguageToLanguageTag(const char* localeID, char* appendAt, int32_t capacity, UBool strict, UErrorCode* status) {
    char buf[ULOC_LANG_CAPACITY];
    UErrorCode tmpStatus = U_ZERO_ERROR;
    int32_t len, i;
    int32_t reslen = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }

    len = uloc_getLanguage(localeID, buf, sizeof(buf), &tmpStatus);
    if (U_FAILURE(tmpStatus) || tmpStatus == U_STRING_NOT_TERMINATED_WARNING) {
        if (strict) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        len = 0;
    }

    /* Note: returned language code is in lower case letters */

    if (len == 0) {
        if (reslen < capacity) {
            uprv_memcpy(appendAt + reslen, LANG_UND, uprv_min(LANG_UND_LEN, capacity - reslen));
        }
        reslen += LANG_UND_LEN;
    } else if (!_isLanguageSubtag(buf, len)) {
            /* invalid language code */
        if (strict) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        if (reslen < capacity) {
            uprv_memcpy(appendAt + reslen, LANG_UND, uprv_min(LANG_UND_LEN, capacity - reslen));
        }
        reslen += LANG_UND_LEN;
    } else {
        /* resolve deprecated */
        for (i = 0; i < UPRV_LENGTHOF(DEPRECATEDLANGS); i += 2) {
            if (uprv_compareInvCharsAsAscii(buf, DEPRECATEDLANGS[i]) == 0) {
                uprv_strcpy(buf, DEPRECATEDLANGS[i + 1]);
                len = (int32_t)uprv_strlen(buf);
                break;
            }
        }
        if (reslen < capacity) {
            uprv_memcpy(appendAt + reslen, buf, uprv_min(len, capacity - reslen));
        }
        reslen += len;
    }
    u_terminateChars(appendAt, capacity, reslen, status);
    return reslen;
}

static int32_t
_appendScriptToLanguageTag(const char* localeID, char* appendAt, int32_t capacity, UBool strict, UErrorCode* status) {
    char buf[ULOC_SCRIPT_CAPACITY];
    UErrorCode tmpStatus = U_ZERO_ERROR;
    int32_t len;
    int32_t reslen = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }

    len = uloc_getScript(localeID, buf, sizeof(buf), &tmpStatus);
    if (U_FAILURE(tmpStatus) || tmpStatus == U_STRING_NOT_TERMINATED_WARNING) {
        if (strict) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        return 0;
    }

    if (len > 0) {
        if (!_isScriptSubtag(buf, len)) {
            /* invalid script code */
            if (strict) {
                *status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return 0;
        } else {
            if (reslen < capacity) {
                *(appendAt + reslen) = SEP;
            }
            reslen++;

            if (reslen < capacity) {
                uprv_memcpy(appendAt + reslen, buf, uprv_min(len, capacity - reslen));
            }
            reslen += len;
        }
    }
    u_terminateChars(appendAt, capacity, reslen, status);
    return reslen;
}

static int32_t
_appendRegionToLanguageTag(const char* localeID, char* appendAt, int32_t capacity, UBool strict, UErrorCode* status) {
    char buf[ULOC_COUNTRY_CAPACITY];
    UErrorCode tmpStatus = U_ZERO_ERROR;
    int32_t len;
    int32_t reslen = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }

    len = uloc_getCountry(localeID, buf, sizeof(buf), &tmpStatus);
    if (U_FAILURE(tmpStatus) || tmpStatus == U_STRING_NOT_TERMINATED_WARNING) {
        if (strict) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        return 0;
    }

    if (len > 0) {
        if (!_isRegionSubtag(buf, len)) {
            /* invalid region code */
            if (strict) {
                *status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return 0;
        } else {
            if (reslen < capacity) {
                *(appendAt + reslen) = SEP;
            }
            reslen++;

            if (reslen < capacity) {
                uprv_memcpy(appendAt + reslen, buf, uprv_min(len, capacity - reslen));
            }
            reslen += len;
        }
    }
    u_terminateChars(appendAt, capacity, reslen, status);
    return reslen;
}

static int32_t
_appendVariantsToLanguageTag(const char* localeID, char* appendAt, int32_t capacity, UBool strict, UBool *hadPosix, UErrorCode* status) {
    char buf[ULOC_FULLNAME_CAPACITY];
    UErrorCode tmpStatus = U_ZERO_ERROR;
    int32_t len, i;
    int32_t reslen = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }

    len = uloc_getVariant(localeID, buf, sizeof(buf), &tmpStatus);
    if (U_FAILURE(tmpStatus) || tmpStatus == U_STRING_NOT_TERMINATED_WARNING) {
        if (strict) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        return 0;
    }

    if (len > 0) {
        char *p, *pVar;
        UBool bNext = TRUE;
        VariantListEntry *var;
        VariantListEntry *varFirst = NULL;

        pVar = NULL;
        p = buf;
        while (bNext) {
            if (*p == SEP || *p == LOCALE_SEP || *p == 0) {
                if (*p == 0) {
                    bNext = FALSE;
                } else {
                    *p = 0; /* terminate */
                }
                if (pVar == NULL) {
                    if (strict) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        break;
                    }
                    /* ignore empty variant */
                } else {
                    /* ICU uses upper case letters for variants, but
                       the canonical format is lowercase in BCP47 */
                    for (i = 0; *(pVar + i) != 0; i++) {
                        *(pVar + i) = uprv_tolower(*(pVar + i));
                    }

                    /* validate */
                    if (_isVariantSubtag(pVar, -1)) {
                        if (uprv_strcmp(pVar,POSIX_VALUE) || len != (int32_t)uprv_strlen(POSIX_VALUE)) {
                            /* emit the variant to the list */
                            var = (VariantListEntry*)uprv_malloc(sizeof(VariantListEntry));
                            if (var == NULL) {
                                *status = U_MEMORY_ALLOCATION_ERROR;
                                break;
                            }
                            var->variant = pVar;
                            if (!_addVariantToList(&varFirst, var)) {
                                /* duplicated variant */
                                uprv_free(var);
                                if (strict) {
                                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                                    break;
                                }
                            }
                        } else {
                            /* Special handling for POSIX variant, need to remember that we had it and then */
                            /* treat it like an extension later. */
                            *hadPosix = TRUE;
                        }
                    } else if (strict) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        break;
                    } else if (_isPrivateuseValueSubtag(pVar, -1)) {
                        /* Handle private use subtags separately */
                        break;
                    }
                }
                /* reset variant starting position */
                pVar = NULL;
            } else if (pVar == NULL) {
                pVar = p;
            }
            p++;
        }

        if (U_SUCCESS(*status)) {
            if (varFirst != NULL) {
                int32_t varLen;

                /* write out validated/normalized variants to the target */
                var = varFirst;
                while (var != NULL) {
                    if (reslen < capacity) {
                        *(appendAt + reslen) = SEP;
                    }
                    reslen++;
                    varLen = (int32_t)uprv_strlen(var->variant);
                    if (reslen < capacity) {
                        uprv_memcpy(appendAt + reslen, var->variant, uprv_min(varLen, capacity - reslen));
                    }
                    reslen += varLen;
                    var = var->next;
                }
            }
        }

        /* clean up */
        var = varFirst;
        while (var != NULL) {
            VariantListEntry *tmpVar = var->next;
            uprv_free(var);
            var = tmpVar;
        }

        if (U_FAILURE(*status)) {
            return 0;
        }
    }

    u_terminateChars(appendAt, capacity, reslen, status);
    return reslen;
}

static int32_t
_appendKeywordsToLanguageTag(const char* localeID, char* appendAt, int32_t capacity, UBool strict, UBool hadPosix, UErrorCode* status) {
    char buf[ULOC_KEYWORD_AND_VALUES_CAPACITY];
    char attrBuf[ULOC_KEYWORD_AND_VALUES_CAPACITY] = { 0 };
    int32_t attrBufLength = 0;
    UEnumeration *keywordEnum = NULL;
    int32_t reslen = 0;

    keywordEnum = uloc_openKeywords(localeID, status);
    if (U_FAILURE(*status) && !hadPosix) {
        uenum_close(keywordEnum);
        return 0;
    }
    if (keywordEnum != NULL || hadPosix) {
        /* reorder extensions */
        int32_t len;
        const char *key;
        ExtensionListEntry *firstExt = NULL;
        ExtensionListEntry *ext;
        AttributeListEntry *firstAttr = NULL;
        AttributeListEntry *attr;
        char *attrValue;
        char extBuf[ULOC_KEYWORD_AND_VALUES_CAPACITY];
        char *pExtBuf = extBuf;
        int32_t extBufCapacity = sizeof(extBuf);
        const char *bcpKey=nullptr, *bcpValue=nullptr;
        UErrorCode tmpStatus = U_ZERO_ERROR;
        int32_t keylen;
        UBool isBcpUExt;

        while (TRUE) {
            key = uenum_next(keywordEnum, NULL, status);
            if (key == NULL) {
                break;
            }
            len = uloc_getKeywordValue(localeID, key, buf, sizeof(buf), &tmpStatus);
            /* buf must be null-terminated */
            if (U_FAILURE(tmpStatus) || tmpStatus == U_STRING_NOT_TERMINATED_WARNING) {
                if (strict) {
                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                    break;
                }
                /* ignore this keyword */
                tmpStatus = U_ZERO_ERROR;
                continue;
            }

            keylen = (int32_t)uprv_strlen(key);
            isBcpUExt = (keylen > 1);

            /* special keyword used for representing Unicode locale attributes */
            if (uprv_strcmp(key, LOCALE_ATTRIBUTE_KEY) == 0) {
                if (len > 0) {
                    int32_t i = 0;
                    while (TRUE) {
                        attrBufLength = 0;
                        for (; i < len; i++) {
                            if (buf[i] != '-') {
                                attrBuf[attrBufLength++] = buf[i];
                            } else {
                                i++;
                                break;
                            }
                        }
                        if (attrBufLength > 0) {
                            attrBuf[attrBufLength] = 0;

                        } else if (i >= len){
                            break;
                        }

                        /* create AttributeListEntry */
                        attr = (AttributeListEntry*)uprv_malloc(sizeof(AttributeListEntry));
                        if (attr == NULL) {
                            *status = U_MEMORY_ALLOCATION_ERROR;
                            break;
                        }
                        attrValue = (char*)uprv_malloc(attrBufLength + 1);
                        if (attrValue == NULL) {
                            *status = U_MEMORY_ALLOCATION_ERROR;
                            break;
                        }
                        uprv_strcpy(attrValue, attrBuf);
                        attr->attribute = attrValue;

                        if (!_addAttributeToList(&firstAttr, attr)) {
                            uprv_free(attr);
                            uprv_free(attrValue);
                            if (strict) {
                                *status = U_ILLEGAL_ARGUMENT_ERROR;
                                break;
                            }
                        }
                    }
                    /* for a place holder ExtensionListEntry */
                    bcpKey = LOCALE_ATTRIBUTE_KEY;
                    bcpValue = NULL;
                }
            } else if (isBcpUExt) {
                bcpKey = uloc_toUnicodeLocaleKey(key);
                if (bcpKey == NULL) {
                    if (strict) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        break;
                    }
                    continue;
                }

                /* we've checked buf is null-terminated above */
                bcpValue = uloc_toUnicodeLocaleType(key, buf);
                if (bcpValue == NULL) {
                    if (strict) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        break;
                    }
                    continue;
                }
                if (bcpValue == buf) {
                    /*
                    When uloc_toUnicodeLocaleType(key, buf) returns the
                    input value as is, the value is well-formed, but has
                    no known mapping. This implementation normalizes the
                    the value to lower case
                    */
                    int32_t bcpValueLen = static_cast<int32_t>(uprv_strlen(bcpValue));
                    if (bcpValueLen < extBufCapacity) {
                        uprv_strcpy(pExtBuf, bcpValue);
                        T_CString_toLowerCase(pExtBuf);

                        bcpValue = pExtBuf;

                        pExtBuf += (bcpValueLen + 1);
                        extBufCapacity -= (bcpValueLen + 1);
                    } else {
                        if (strict) {
                            *status = U_ILLEGAL_ARGUMENT_ERROR;
                            break;
                        }
                        continue;
                    }
                }
            } else {
                if (*key == PRIVATEUSE) {
                    if (!_isPrivateuseValueSubtags(buf, len)) {
                        if (strict) {
                            *status = U_ILLEGAL_ARGUMENT_ERROR;
                            break;
                        }
                        continue;
                    }
                } else {
                    if (!_isExtensionSingleton(key, keylen) || !_isExtensionSubtags(buf, len)) {
                        if (strict) {
                            *status = U_ILLEGAL_ARGUMENT_ERROR;
                            break;
                        }
                        continue;
                    }
                }
                bcpKey = key;
                if ((len + 1) < extBufCapacity) {
                    uprv_memcpy(pExtBuf, buf, len);
                    bcpValue = pExtBuf;

                    pExtBuf += len;

                    *pExtBuf = 0;
                    pExtBuf++;

                    extBufCapacity -= (len + 1);
                } else {
                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                    break;
                }
            }

            /* create ExtensionListEntry */
            ext = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
            if (ext == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                break;
            }
            ext->key = bcpKey;
            ext->value = bcpValue;

            if (!_addExtensionToList(&firstExt, ext, TRUE)) {
                uprv_free(ext);
                if (strict) {
                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                    break;
                }
            }
        }

        /* Special handling for POSIX variant - add the keywords for POSIX */
        if (hadPosix) {
            /* create ExtensionListEntry for POSIX */
            ext = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
            if (ext == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                goto cleanup;
            }
            ext->key = POSIX_KEY;
            ext->value = POSIX_VALUE;

            if (!_addExtensionToList(&firstExt, ext, TRUE)) {
                uprv_free(ext);
            }
        }

        if (U_SUCCESS(*status) && (firstExt != NULL || firstAttr != NULL)) {
            UBool startLDMLExtension = FALSE;
            for (ext = firstExt; ext; ext = ext->next) {
                if (!startLDMLExtension && uprv_strlen(ext->key) > 1) {
                    /* first LDML u singlton extension */
                   if (reslen < capacity) {
                       *(appendAt + reslen) = SEP;
                   }
                   reslen++;
                   if (reslen < capacity) {
                       *(appendAt + reslen) = LDMLEXT;
                   }
                   reslen++;

                   startLDMLExtension = TRUE;
                }

                /* write out the sorted BCP47 attributes, extensions and private use */
                if (uprv_strcmp(ext->key, LOCALE_ATTRIBUTE_KEY) == 0) {
                    /* write the value for the attributes */
                    for (attr = firstAttr; attr; attr = attr->next) {
                        if (reslen < capacity) {
                            *(appendAt + reslen) = SEP;
                        }
                        reslen++;
                        len = (int32_t)uprv_strlen(attr->attribute);
                        if (reslen < capacity) {
                            uprv_memcpy(appendAt + reslen, attr->attribute, uprv_min(len, capacity - reslen));
                        }
                        reslen += len;
                    }
                } else {
                    if (reslen < capacity) {
                        *(appendAt + reslen) = SEP;
                    }
                    reslen++;
                    len = (int32_t)uprv_strlen(ext->key);
                    if (reslen < capacity) {
                        uprv_memcpy(appendAt + reslen, ext->key, uprv_min(len, capacity - reslen));
                    }
                    reslen += len;
                    if (reslen < capacity) {
                        *(appendAt + reslen) = SEP;
                    }
                    reslen++;
                    len = (int32_t)uprv_strlen(ext->value);
                    if (reslen < capacity) {
                        uprv_memcpy(appendAt + reslen, ext->value, uprv_min(len, capacity - reslen));
                    }
                    reslen += len;
                }
            }
        }
cleanup:
        /* clean up */
        ext = firstExt;
        while (ext != NULL) {
            ExtensionListEntry *tmpExt = ext->next;
            uprv_free(ext);
            ext = tmpExt;
        }

        attr = firstAttr;
        while (attr != NULL) {
            AttributeListEntry *tmpAttr = attr->next;
            char *pValue = (char *)attr->attribute;
            uprv_free(pValue);
            uprv_free(attr);
            attr = tmpAttr;
        }

        uenum_close(keywordEnum);

        if (U_FAILURE(*status)) {
            return 0;
        }
    }

    return u_terminateChars(appendAt, capacity, reslen, status);
}

/**
 * Append keywords parsed from LDML extension value
 * e.g. "u-ca-gregory-co-trad" -> {calendar = gregorian} {collation = traditional}
 * Note: char* buf is used for storing keywords
 */
static void
_appendLDMLExtensionAsKeywords(const char* ldmlext, ExtensionListEntry** appendTo, char* buf, int32_t bufSize, UBool *posixVariant, UErrorCode *status) {
    const char *pTag;   /* beginning of current subtag */
    const char *pKwds;  /* beginning of key-type pairs */
    UBool variantExists = *posixVariant;

    ExtensionListEntry *kwdFirst = NULL;    /* first LDML keyword */
    ExtensionListEntry *kwd, *nextKwd;

    AttributeListEntry *attrFirst = NULL;   /* first attribute */
    AttributeListEntry *attr, *nextAttr;

    int32_t len;
    int32_t bufIdx = 0;

    char attrBuf[ULOC_KEYWORD_AND_VALUES_CAPACITY];
    int32_t attrBufIdx = 0;

    /* Reset the posixVariant value */
    *posixVariant = FALSE;

    pTag = ldmlext;
    pKwds = NULL;

    /* Iterate through u extension attributes */
    while (*pTag) {
        /* locate next separator char */
        for (len = 0; *(pTag + len) && *(pTag + len) != SEP; len++);

        if (ultag_isUnicodeLocaleKey(pTag, len)) {
            pKwds = pTag;
            break;
        }

        /* add this attribute to the list */
        attr = (AttributeListEntry*)uprv_malloc(sizeof(AttributeListEntry));
        if (attr == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            goto cleanup;
        }

        if (len < (int32_t)sizeof(attrBuf) - attrBufIdx) {
            uprv_memcpy(&attrBuf[attrBufIdx], pTag, len);
            attrBuf[attrBufIdx + len] = 0;
            attr->attribute = &attrBuf[attrBufIdx];
            attrBufIdx += (len + 1);
        } else {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            goto cleanup;
        }

        if (!_addAttributeToList(&attrFirst, attr)) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            uprv_free(attr);
            goto cleanup;
        }

        /* next tag */
        pTag += len;
        if (*pTag) {
            /* next to the separator */
            pTag++;
        }
    }

    if (attrFirst) {
        /* emit attributes as an LDML keyword, e.g. attribute=attr1-attr2 */

        if (attrBufIdx > bufSize) {
            /* attrBufIdx == <total length of attribute subtag> + 1 */
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            goto cleanup;
        }

        kwd = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
        if (kwd == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            goto cleanup;
        }

        kwd->key = LOCALE_ATTRIBUTE_KEY;
        kwd->value = buf;

        /* attribute subtags sorted in alphabetical order as type */
        attr = attrFirst;
        while (attr != NULL) {
            nextAttr = attr->next;

            /* buffer size check is done above */
            if (attr != attrFirst) {
                *(buf + bufIdx) = SEP;
                bufIdx++;
            }

            len = static_cast<int32_t>(uprv_strlen(attr->attribute));
            uprv_memcpy(buf + bufIdx, attr->attribute, len);
            bufIdx += len;

            attr = nextAttr;
        }
        *(buf + bufIdx) = 0;
        bufIdx++;

        if (!_addExtensionToList(&kwdFirst, kwd, FALSE)) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            uprv_free(kwd);
            goto cleanup;
        }

        /* once keyword entry is created, delete the attribute list */
        attr = attrFirst;
        while (attr != NULL) {
            nextAttr = attr->next;
            uprv_free(attr);
            attr = nextAttr;
        }
        attrFirst = NULL;
    }

    if (pKwds) {
        const char *pBcpKey = NULL;     /* u extenstion key subtag */
        const char *pBcpType = NULL;    /* beginning of u extension type subtag(s) */
        int32_t bcpKeyLen = 0;
        int32_t bcpTypeLen = 0;
        UBool isDone = FALSE;

        pTag = pKwds;
        /* BCP47 representation of LDML key/type pairs */
        while (!isDone) {
            const char *pNextBcpKey = NULL;
            int32_t nextBcpKeyLen = 0;
            UBool emitKeyword = FALSE;

            if (*pTag) {
                /* locate next separator char */
                for (len = 0; *(pTag + len) && *(pTag + len) != SEP; len++);

                if (ultag_isUnicodeLocaleKey(pTag, len)) {
                    if (pBcpKey) {
                        emitKeyword = TRUE;
                        pNextBcpKey = pTag;
                        nextBcpKeyLen = len;
                    } else {
                        pBcpKey = pTag;
                        bcpKeyLen = len;
                    }
                } else {
                    U_ASSERT(pBcpKey != NULL);
                    /* within LDML type subtags */
                    if (pBcpType) {
                        bcpTypeLen += (len + 1);
                    } else {
                        pBcpType = pTag;
                        bcpTypeLen = len;
                    }
                }

                /* next tag */
                pTag += len;
                if (*pTag) {
                    /* next to the separator */
                    pTag++;
                }
            } else {
                /* processing last one */
                emitKeyword = TRUE;
                isDone = TRUE;
            }

            if (emitKeyword) {
                const char *pKey = NULL;    /* LDML key */
                const char *pType = NULL;   /* LDML type */

                char bcpKeyBuf[9];          /* BCP key length is always 2 for now */

                U_ASSERT(pBcpKey != NULL);

                if (bcpKeyLen >= (int32_t)sizeof(bcpKeyBuf)) {
                    /* the BCP key is invalid */
                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                    goto cleanup;
                }

                uprv_strncpy(bcpKeyBuf, pBcpKey, bcpKeyLen);
                bcpKeyBuf[bcpKeyLen] = 0;

                /* u extension key to LDML key */
                pKey = uloc_toLegacyKey(bcpKeyBuf);
                if (pKey == NULL) {
                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                    goto cleanup;
                }
                if (pKey == bcpKeyBuf) {
                    /*
                    The key returned by toLegacyKey points to the input buffer.
                    We normalize the result key to lower case.
                    */
                    T_CString_toLowerCase(bcpKeyBuf);
                    if (bufSize - bufIdx - 1 >= bcpKeyLen) {
                        uprv_memcpy(buf + bufIdx, bcpKeyBuf, bcpKeyLen);
                        pKey = buf + bufIdx;
                        bufIdx += bcpKeyLen;
                        *(buf + bufIdx) = 0;
                        bufIdx++;
                    } else {
                        *status = U_BUFFER_OVERFLOW_ERROR;
                        goto cleanup;
                    }
                }

                if (pBcpType) {
                    char bcpTypeBuf[128];       /* practically long enough even considering multiple subtag type */
                    if (bcpTypeLen >= (int32_t)sizeof(bcpTypeBuf)) {
                        /* the BCP type is too long */
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        goto cleanup;
                    }

                    uprv_strncpy(bcpTypeBuf, pBcpType, bcpTypeLen);
                    bcpTypeBuf[bcpTypeLen] = 0;

                    /* BCP type to locale type */
                    pType = uloc_toLegacyType(pKey, bcpTypeBuf);
                    if (pType == NULL) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        goto cleanup;
                    }
                    if (pType == bcpTypeBuf) {
                        /*
                        The type returned by toLegacyType points to the input buffer.
                        We normalize the result type to lower case.
                        */
                        /* normalize to lower case */
                        T_CString_toLowerCase(bcpTypeBuf);
                        if (bufSize - bufIdx - 1 >= bcpTypeLen) {
                            uprv_memcpy(buf + bufIdx, bcpTypeBuf, bcpTypeLen);
                            pType = buf + bufIdx;
                            bufIdx += bcpTypeLen;
                            *(buf + bufIdx) = 0;
                            bufIdx++;
                        } else {
                            *status = U_BUFFER_OVERFLOW_ERROR;
                            goto cleanup;
                        }
                    }
                } else {
                    /* typeless - default type value is "yes" */
                    pType = LOCALE_TYPE_YES;
                }

                /* Special handling for u-va-posix, since we want to treat this as a variant,
                   not as a keyword */
                if (!variantExists && !uprv_strcmp(pKey, POSIX_KEY) && !uprv_strcmp(pType, POSIX_VALUE) ) {
                    *posixVariant = TRUE;
                } else {
                    /* create an ExtensionListEntry for this keyword */
                    kwd = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
                    if (kwd == NULL) {
                        *status = U_MEMORY_ALLOCATION_ERROR;
                        goto cleanup;
                    }

                    kwd->key = pKey;
                    kwd->value = pType;

                    if (!_addExtensionToList(&kwdFirst, kwd, FALSE)) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        uprv_free(kwd);
                        goto cleanup;
                    }
                }

                pBcpKey = pNextBcpKey;
                bcpKeyLen = pNextBcpKey != NULL ? nextBcpKeyLen : 0;
                pBcpType = NULL;
                bcpTypeLen = 0;
            }
        }
    }

    kwd = kwdFirst;
    while (kwd != NULL) {
        nextKwd = kwd->next;
        _addExtensionToList(appendTo, kwd, FALSE);
        kwd = nextKwd;
    }

    return;

cleanup:
    attr = attrFirst;
    while (attr != NULL) {
        nextAttr = attr->next;
        uprv_free(attr);
        attr = nextAttr;
    }

    kwd = kwdFirst;
    while (kwd != NULL) {
        nextKwd = kwd->next;
        uprv_free(kwd);
        kwd = nextKwd;
    }
}


static int32_t
_appendKeywords(ULanguageTag* langtag, char* appendAt, int32_t capacity, UErrorCode* status) {
    int32_t reslen = 0;
    int32_t i, n;
    int32_t len;
    ExtensionListEntry *kwdFirst = NULL;
    ExtensionListEntry *kwd;
    const char *key, *type;
    char *kwdBuf = NULL;
    int32_t kwdBufLength = capacity;
    UBool posixVariant = FALSE;

    if (U_FAILURE(*status)) {
        return 0;
    }

    kwdBuf = (char*)uprv_malloc(kwdBufLength);
    if (kwdBuf == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }

    /* Determine if variants already exists */
    if (ultag_getVariantsSize(langtag)) {
        posixVariant = TRUE;
    }

    n = ultag_getExtensionsSize(langtag);

    /* resolve locale keywords and reordering keys */
    for (i = 0; i < n; i++) {
        key = ultag_getExtensionKey(langtag, i);
        type = ultag_getExtensionValue(langtag, i);
        if (*key == LDMLEXT) {
            _appendLDMLExtensionAsKeywords(type, &kwdFirst, kwdBuf, kwdBufLength, &posixVariant, status);
            if (U_FAILURE(*status)) {
                break;
            }
        } else {
            kwd = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
            if (kwd == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                break;
            }
            kwd->key = key;
            kwd->value = type;
            if (!_addExtensionToList(&kwdFirst, kwd, FALSE)) {
                uprv_free(kwd);
                *status = U_ILLEGAL_ARGUMENT_ERROR;
                break;
            }
        }
    }

    if (U_SUCCESS(*status)) {
        type = ultag_getPrivateUse(langtag);
        if ((int32_t)uprv_strlen(type) > 0) {
            /* add private use as a keyword */
            kwd = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
            if (kwd == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
            } else {
                kwd->key = PRIVATEUSE_KEY;
                kwd->value = type;
                if (!_addExtensionToList(&kwdFirst, kwd, FALSE)) {
                    uprv_free(kwd);
                    *status = U_ILLEGAL_ARGUMENT_ERROR;
                }
            }
        }
    }

    /* If a POSIX variant was in the extensions, write it out before writing the keywords. */

    if (U_SUCCESS(*status) && posixVariant) {
        len = (int32_t) uprv_strlen(_POSIX);
        if (reslen < capacity) {
            uprv_memcpy(appendAt + reslen, _POSIX, uprv_min(len, capacity - reslen));
        }
        reslen += len;
    }

    if (U_SUCCESS(*status) && kwdFirst != NULL) {
        /* write out the sorted keywords */
        UBool firstValue = TRUE;
        kwd = kwdFirst;
        do {
            if (reslen < capacity) {
                if (firstValue) {
                    /* '@' */
                    *(appendAt + reslen) = LOCALE_EXT_SEP;
                    firstValue = FALSE;
                } else {
                    /* ';' */
                    *(appendAt + reslen) = LOCALE_KEYWORD_SEP;
                }
            }
            reslen++;

            /* key */
            len = (int32_t)uprv_strlen(kwd->key);
            if (reslen < capacity) {
                uprv_memcpy(appendAt + reslen, kwd->key, uprv_min(len, capacity - reslen));
            }
            reslen += len;

            /* '=' */
            if (reslen < capacity) {
                *(appendAt + reslen) = LOCALE_KEY_TYPE_SEP;
            }
            reslen++;

            /* type */
            len = (int32_t)uprv_strlen(kwd->value);
            if (reslen < capacity) {
                uprv_memcpy(appendAt + reslen, kwd->value, uprv_min(len, capacity - reslen));
            }
            reslen += len;

            kwd = kwd->next;
        } while (kwd);
    }

    /* clean up */
    kwd = kwdFirst;
    while (kwd != NULL) {
        ExtensionListEntry *tmpKwd = kwd->next;
        uprv_free(kwd);
        kwd = tmpKwd;
    }

    uprv_free(kwdBuf);

    if (U_FAILURE(*status)) {
        return 0;
    }

    return u_terminateChars(appendAt, capacity, reslen, status);
}

static int32_t
_appendPrivateuseToLanguageTag(const char* localeID, char* appendAt, int32_t capacity, UBool strict, UBool hadPosix, UErrorCode* status) {
    (void)hadPosix;
    char buf[ULOC_FULLNAME_CAPACITY];
    char tmpAppend[ULOC_FULLNAME_CAPACITY];
    UErrorCode tmpStatus = U_ZERO_ERROR;
    int32_t len, i;
    int32_t reslen = 0;

    if (U_FAILURE(*status)) {
        return 0;
    }

    len = uloc_getVariant(localeID, buf, sizeof(buf), &tmpStatus);
    if (U_FAILURE(tmpStatus) || tmpStatus == U_STRING_NOT_TERMINATED_WARNING) {
        if (strict) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        return 0;
    }

    if (len > 0) {
        char *p, *pPriv;
        UBool bNext = TRUE;
        UBool firstValue = TRUE;
        UBool writeValue;

        pPriv = NULL;
        p = buf;
        while (bNext) {
            writeValue = FALSE;
            if (*p == SEP || *p == LOCALE_SEP || *p == 0) {
                if (*p == 0) {
                    bNext = FALSE;
                } else {
                    *p = 0; /* terminate */
                }
                if (pPriv != NULL) {
                    /* Private use in the canonical format is lowercase in BCP47 */
                    for (i = 0; *(pPriv + i) != 0; i++) {
                        *(pPriv + i) = uprv_tolower(*(pPriv + i));
                    }

                    /* validate */
                    if (_isPrivateuseValueSubtag(pPriv, -1)) {
                        if (firstValue) {
                            if (!_isVariantSubtag(pPriv, -1)) {
                                writeValue = TRUE;
                            }
                        } else {
                            writeValue = TRUE;
                        }
                    } else if (strict) {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        break;
                    } else {
                        break;
                    }

                    if (writeValue) {
                        if (reslen < capacity) {
                            tmpAppend[reslen++] = SEP;
                        }

                        if (firstValue) {
                            if (reslen < capacity) {
                                tmpAppend[reslen++] = *PRIVATEUSE_KEY;
                            }

                            if (reslen < capacity) {
                                tmpAppend[reslen++] = SEP;
                            }

                            len = (int32_t)uprv_strlen(PRIVUSE_VARIANT_PREFIX);
                            if (reslen < capacity) {
                                uprv_memcpy(tmpAppend + reslen, PRIVUSE_VARIANT_PREFIX, uprv_min(len, capacity - reslen));
                            }
                            reslen += len;

                            if (reslen < capacity) {
                                tmpAppend[reslen++] = SEP;
                            }

                            firstValue = FALSE;
                        }

                        len = (int32_t)uprv_strlen(pPriv);
                        if (reslen < capacity) {
                            uprv_memcpy(tmpAppend + reslen, pPriv, uprv_min(len, capacity - reslen));
                        }
                        reslen += len;
                    }
                }
                /* reset private use starting position */
                pPriv = NULL;
            } else if (pPriv == NULL) {
                pPriv = p;
            }
            p++;
        }

        if (U_FAILURE(*status)) {
            return 0;
        }
    }

    if (U_SUCCESS(*status)) {
        len = reslen;
        if (reslen < capacity) {
            uprv_memcpy(appendAt, tmpAppend, uprv_min(len, capacity - reslen));
        }
    }

    u_terminateChars(appendAt, capacity, reslen, status);

    return reslen;
}

/*
* -------------------------------------------------
*
* ultag_ functions
*
* -------------------------------------------------
*/

/* Bit flags used by the parser */
#define LANG 0x0001
#define EXTL 0x0002
#define SCRT 0x0004
#define REGN 0x0008
#define VART 0x0010
#define EXTS 0x0020
#define EXTV 0x0040
#define PRIV 0x0080

/**
 * Ticket #12705 - Visual Studio 2015 Update 3 contains a new code optimizer which has problems optimizing
 * this function. (See https://blogs.msdn.microsoft.com/vcblog/2016/05/04/new-code-optimizer/ )
 * As a workaround, we will turn off optimization just for this function on VS2015 Update 3 and above.
 */
#if (defined(_MSC_VER) && (_MSC_VER >= 1900) && defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 190024210))
#pragma optimize( "", off )
#endif

static ULanguageTag*
ultag_parse(const char* tag, int32_t tagLen, int32_t* parsedLen, UErrorCode* status) {
    ULanguageTag *t;
    char *tagBuf;
    int16_t next;
    char *pSubtag, *pNext, *pLastGoodPosition;
    int32_t subtagLen;
    int32_t extlangIdx;
    ExtensionListEntry *pExtension;
    char *pExtValueSubtag, *pExtValueSubtagEnd;
    int32_t i;
    UBool privateuseVar = FALSE;
    int32_t grandfatheredLen = 0;

    if (parsedLen != NULL) {
        *parsedLen = 0;
    }

    if (U_FAILURE(*status)) {
        return NULL;
    }

    if (tagLen < 0) {
        tagLen = (int32_t)uprv_strlen(tag);
    }

    /* copy the entire string */
    tagBuf = (char*)uprv_malloc(tagLen + 1);
    if (tagBuf == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    uprv_memcpy(tagBuf, tag, tagLen);
    *(tagBuf + tagLen) = 0;

    /* create a ULanguageTag */
    t = (ULanguageTag*)uprv_malloc(sizeof(ULanguageTag));
    if (t == NULL) {
        uprv_free(tagBuf);
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    _initializeULanguageTag(t);
    t->buf = tagBuf;

    if (tagLen < MINLEN) {
        /* the input tag is too short - return empty ULanguageTag */
        return t;
    }

    /* check if the tag is grandfathered */
    for (i = 0; GRANDFATHERED[i] != NULL; i += 2) {
        if (uprv_stricmp(GRANDFATHERED[i], tagBuf) == 0) {
            int32_t newTagLength;

            grandfatheredLen = tagLen;  /* back up for output parsedLen */
            newTagLength = static_cast<int32_t>(uprv_strlen(GRANDFATHERED[i+1]));
            if (tagLen < newTagLength) {
                uprv_free(tagBuf);
                tagBuf = (char*)uprv_malloc(newTagLength + 1);
                if (tagBuf == NULL) {
                    *status = U_MEMORY_ALLOCATION_ERROR;
                    ultag_close(t);
                    return NULL;
                }
                t->buf = tagBuf;
                tagLen = newTagLength;
            }
            uprv_strcpy(t->buf, GRANDFATHERED[i + 1]);
            break;
        }
    }

    /*
     * langtag      =   language
     *                  ["-" script]
     *                  ["-" region]
     *                  *("-" variant)
     *                  *("-" extension)
     *                  ["-" privateuse]
     */

    next = LANG | PRIV;
    pNext = pLastGoodPosition = tagBuf;
    extlangIdx = 0;
    pExtension = NULL;
    pExtValueSubtag = NULL;
    pExtValueSubtagEnd = NULL;

    while (pNext) {
        char *pSep;

        pSubtag = pNext;

        /* locate next separator char */
        pSep = pSubtag;
        while (*pSep) {
            if (*pSep == SEP) {
                break;
            }
            pSep++;
        }
        if (*pSep == 0) {
            /* last subtag */
            pNext = NULL;
        } else {
            pNext = pSep + 1;
        }
        subtagLen = (int32_t)(pSep - pSubtag);

        if (next & LANG) {
            if (_isLanguageSubtag(pSubtag, subtagLen)) {
                *pSep = 0;  /* terminate */
                t->language = T_CString_toLowerCase(pSubtag);

                pLastGoodPosition = pSep;
                next = EXTL | SCRT | REGN | VART | EXTS | PRIV;
                continue;
            }
        }
        if (next & EXTL) {
            if (_isExtlangSubtag(pSubtag, subtagLen)) {
                *pSep = 0;
                t->extlang[extlangIdx++] = T_CString_toLowerCase(pSubtag);

                pLastGoodPosition = pSep;
                if (extlangIdx < 3) {
                    next = EXTL | SCRT | REGN | VART | EXTS | PRIV;
                } else {
                    next = SCRT | REGN | VART | EXTS | PRIV;
                }
                continue;
            }
        }
        if (next & SCRT) {
            if (_isScriptSubtag(pSubtag, subtagLen)) {
                char *p = pSubtag;

                *pSep = 0;

                /* to title case */
                *p = uprv_toupper(*p);
                p++;
                for (; *p; p++) {
                    *p = uprv_tolower(*p);
                }

                t->script = pSubtag;

                pLastGoodPosition = pSep;
                next = REGN | VART | EXTS | PRIV;
                continue;
            }
        }
        if (next & REGN) {
            if (_isRegionSubtag(pSubtag, subtagLen)) {
                *pSep = 0;
                t->region = T_CString_toUpperCase(pSubtag);

                pLastGoodPosition = pSep;
                next = VART | EXTS | PRIV;
                continue;
            }
        }
        if (next & VART) {
            if (_isVariantSubtag(pSubtag, subtagLen) ||
               (privateuseVar && _isPrivateuseVariantSubtag(pSubtag, subtagLen))) {
                VariantListEntry *var;
                UBool isAdded;

                var = (VariantListEntry*)uprv_malloc(sizeof(VariantListEntry));
                if (var == NULL) {
                    *status = U_MEMORY_ALLOCATION_ERROR;
                    goto error;
                }
                *pSep = 0;
                var->variant = T_CString_toUpperCase(pSubtag);
                isAdded = _addVariantToList(&(t->variants), var);
                if (!isAdded) {
                    /* duplicated variant entry */
                    uprv_free(var);
                    break;
                }
                pLastGoodPosition = pSep;
                next = VART | EXTS | PRIV;
                continue;
            }
        }
        if (next & EXTS) {
            if (_isExtensionSingleton(pSubtag, subtagLen)) {
                if (pExtension != NULL) {
                    if (pExtValueSubtag == NULL || pExtValueSubtagEnd == NULL) {
                        /* the previous extension is incomplete */
                        uprv_free(pExtension);
                        pExtension = NULL;
                        break;
                    }

                    /* terminate the previous extension value */
                    *pExtValueSubtagEnd = 0;
                    pExtension->value = T_CString_toLowerCase(pExtValueSubtag);

                    /* insert the extension to the list */
                    if (_addExtensionToList(&(t->extensions), pExtension, FALSE)) {
                        pLastGoodPosition = pExtValueSubtagEnd;
                    } else {
                        /* stop parsing here */
                        uprv_free(pExtension);
                        pExtension = NULL;
                        break;
                    }
                }

                /* create a new extension */
                pExtension = (ExtensionListEntry*)uprv_malloc(sizeof(ExtensionListEntry));
                if (pExtension == NULL) {
                    *status = U_MEMORY_ALLOCATION_ERROR;
                    goto error;
                }
                *pSep = 0;
                pExtension->key = T_CString_toLowerCase(pSubtag);
                pExtension->value = NULL;   /* will be set later */

                /*
                 * reset the start and the end location of extension value
                 * subtags for this extension
                 */
                pExtValueSubtag = NULL;
                pExtValueSubtagEnd = NULL;

                next = EXTV;
                continue;
            }
        }
        if (next & EXTV) {
            if (_isExtensionSubtag(pSubtag, subtagLen)) {
                if (pExtValueSubtag == NULL) {
                    /* if the start postion of this extension's value is not yet,
                        this one is the first value subtag */
                    pExtValueSubtag = pSubtag;
                }

                /* Mark the end of this subtag */
                pExtValueSubtagEnd = pSep;
                next = EXTS | EXTV | PRIV;

                continue;
            }
        }
        if (next & PRIV) {
            if (uprv_tolower(*pSubtag) == PRIVATEUSE) {
                char *pPrivuseVal;

                if (pExtension != NULL) {
                    /* Process the last extension */
                    if (pExtValueSubtag == NULL || pExtValueSubtagEnd == NULL) {
                        /* the previous extension is incomplete */
                        uprv_free(pExtension);
                        pExtension = NULL;
                        break;
                    } else {
                        /* terminate the previous extension value */
                        *pExtValueSubtagEnd = 0;
                        pExtension->value = T_CString_toLowerCase(pExtValueSubtag);

                        /* insert the extension to the list */
                        if (_addExtensionToList(&(t->extensions), pExtension, FALSE)) {
                            pLastGoodPosition = pExtValueSubtagEnd;
                            pExtension = NULL;
                        } else {
                        /* stop parsing here */
                            uprv_free(pExtension);
                            pExtension = NULL;
                            break;
                        }
                    }
                }

                /* The rest of part will be private use value subtags */
                if (pNext == NULL) {
                    /* empty private use subtag */
                    break;
                }
                /* back up the private use value start position */
                pPrivuseVal = pNext;

                /* validate private use value subtags */
                while (pNext) {
                    pSubtag = pNext;
                    pSep = pSubtag;
                    while (*pSep) {
                        if (*pSep == SEP) {
                            break;
                        }
                        pSep++;
                    }
                    if (*pSep == 0) {
                        /* last subtag */
                        pNext = NULL;
                    } else {
                        pNext = pSep + 1;
                    }
                    subtagLen = (int32_t)(pSep - pSubtag);

                    if (uprv_strncmp(pSubtag, PRIVUSE_VARIANT_PREFIX, uprv_strlen(PRIVUSE_VARIANT_PREFIX)) == 0) {
                        *pSep = 0;
                        next = VART;
                        privateuseVar = TRUE;
                        break;
                    } else if (_isPrivateuseValueSubtag(pSubtag, subtagLen)) {
                        pLastGoodPosition = pSep;
                    } else {
                        break;
                    }
                }

                if (next == VART) {
                    continue;
                }

                if (pLastGoodPosition - pPrivuseVal > 0) {
                    *pLastGoodPosition = 0;
                    t->privateuse = T_CString_toLowerCase(pPrivuseVal);
                }
                /* No more subtags, exiting the parse loop */
                break;
            }
            break;
        }

        /* If we fell through here, it means this subtag is illegal - quit parsing */
        break;
    }

    if (pExtension != NULL) {
        /* Process the last extension */
        if (pExtValueSubtag == NULL || pExtValueSubtagEnd == NULL) {
            /* the previous extension is incomplete */
            uprv_free(pExtension);
        } else {
            /* terminate the previous extension value */
            *pExtValueSubtagEnd = 0;
            pExtension->value = T_CString_toLowerCase(pExtValueSubtag);
            /* insert the extension to the list */
            if (_addExtensionToList(&(t->extensions), pExtension, FALSE)) {
                pLastGoodPosition = pExtValueSubtagEnd;
            } else {
                uprv_free(pExtension);
            }
        }
    }

    if (parsedLen != NULL) {
        *parsedLen = (grandfatheredLen > 0) ? grandfatheredLen : (int32_t)(pLastGoodPosition - t->buf);
    }

    return t;

error:
    ultag_close(t);
    return NULL;
}

/**
* Ticket #12705 - Turn optimization back on.
*/
#if (defined(_MSC_VER) && (_MSC_VER >= 1900) && defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 190024210))
#pragma optimize( "", on )
#endif

static void
ultag_close(ULanguageTag* langtag) {

    if (langtag == NULL) {
        return;
    }

    uprv_free(langtag->buf);

    if (langtag->variants) {
        VariantListEntry *curVar = langtag->variants;
        while (curVar) {
            VariantListEntry *nextVar = curVar->next;
            uprv_free(curVar);
            curVar = nextVar;
        }
    }

    if (langtag->extensions) {
        ExtensionListEntry *curExt = langtag->extensions;
        while (curExt) {
            ExtensionListEntry *nextExt = curExt->next;
            uprv_free(curExt);
            curExt = nextExt;
        }
    }

    uprv_free(langtag);
}

static const char*
ultag_getLanguage(const ULanguageTag* langtag) {
    return langtag->language;
}

#if 0
static const char*
ultag_getJDKLanguage(const ULanguageTag* langtag) {
    int32_t i;
    for (i = 0; DEPRECATEDLANGS[i] != NULL; i += 2) {
        if (uprv_compareInvCharsAsAscii(DEPRECATEDLANGS[i], langtag->language) == 0) {
            return DEPRECATEDLANGS[i + 1];
        }
    }
    return langtag->language;
}
#endif

static const char*
ultag_getExtlang(const ULanguageTag* langtag, int32_t idx) {
    if (idx >= 0 && idx < MAXEXTLANG) {
        return langtag->extlang[idx];
    }
    return NULL;
}

static int32_t
ultag_getExtlangSize(const ULanguageTag* langtag) {
    int32_t size = 0;
    int32_t i;
    for (i = 0; i < MAXEXTLANG; i++) {
        if (langtag->extlang[i]) {
            size++;
        }
    }
    return size;
}

static const char*
ultag_getScript(const ULanguageTag* langtag) {
    return langtag->script;
}

static const char*
ultag_getRegion(const ULanguageTag* langtag) {
    return langtag->region;
}

static const char*
ultag_getVariant(const ULanguageTag* langtag, int32_t idx) {
    const char *var = NULL;
    VariantListEntry *cur = langtag->variants;
    int32_t i = 0;
    while (cur) {
        if (i == idx) {
            var = cur->variant;
            break;
        }
        cur = cur->next;
        i++;
    }
    return var;
}

static int32_t
ultag_getVariantsSize(const ULanguageTag* langtag) {
    int32_t size = 0;
    VariantListEntry *cur = langtag->variants;
    while (TRUE) {
        if (cur == NULL) {
            break;
        }
        size++;
        cur = cur->next;
    }
    return size;
}

static const char*
ultag_getExtensionKey(const ULanguageTag* langtag, int32_t idx) {
    const char *key = NULL;
    ExtensionListEntry *cur = langtag->extensions;
    int32_t i = 0;
    while (cur) {
        if (i == idx) {
            key = cur->key;
            break;
        }
        cur = cur->next;
        i++;
    }
    return key;
}

static const char*
ultag_getExtensionValue(const ULanguageTag* langtag, int32_t idx) {
    const char *val = NULL;
    ExtensionListEntry *cur = langtag->extensions;
    int32_t i = 0;
    while (cur) {
        if (i == idx) {
            val = cur->value;
            break;
        }
        cur = cur->next;
        i++;
    }
    return val;
}

static int32_t
ultag_getExtensionsSize(const ULanguageTag* langtag) {
    int32_t size = 0;
    ExtensionListEntry *cur = langtag->extensions;
    while (TRUE) {
        if (cur == NULL) {
            break;
        }
        size++;
        cur = cur->next;
    }
    return size;
}

static const char*
ultag_getPrivateUse(const ULanguageTag* langtag) {
    return langtag->privateuse;
}

#if 0
static const char*
ultag_getGrandfathered(const ULanguageTag* langtag) {
    return langtag->grandfathered;
}
#endif


/*
* -------------------------------------------------
*
* Locale/BCP47 conversion APIs, exposed as uloc_*
*
* -------------------------------------------------
*/
U_CAPI int32_t U_EXPORT2
uloc_toLanguageTag(const char* localeID,
                   char* langtag,
                   int32_t langtagCapacity,
                   UBool strict,
                   UErrorCode* status) {
    /* char canonical[ULOC_FULLNAME_CAPACITY]; */ /* See #6822 */
    char canonical[256];
    int32_t reslen = 0;
    UErrorCode tmpStatus = U_ZERO_ERROR;
    UBool hadPosix = FALSE;
    const char* pKeywordStart;

    /* Note: uloc_canonicalize returns "en_US_POSIX" for input locale ID "".  See #6835 */
    canonical[0] = 0;
    if (uprv_strlen(localeID) > 0) {
        uloc_canonicalize(localeID, canonical, sizeof(canonical), &tmpStatus);
        if (tmpStatus != U_ZERO_ERROR) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
    }

    /* For handling special case - private use only tag */
    pKeywordStart = locale_getKeywordsStart(canonical);
    if (pKeywordStart == canonical) {
        UEnumeration *kwdEnum;
        int kwdCnt = 0;
        UBool done = FALSE;

        kwdEnum = uloc_openKeywords((const char*)canonical, &tmpStatus);
        if (kwdEnum != NULL) {
            kwdCnt = uenum_count(kwdEnum, &tmpStatus);
            if (kwdCnt == 1) {
                const char *key;
                int32_t len = 0;

                key = uenum_next(kwdEnum, &len, &tmpStatus);
                if (len == 1 && *key == PRIVATEUSE) {
                    char buf[ULOC_KEYWORD_AND_VALUES_CAPACITY];
                    buf[0] = PRIVATEUSE;
                    buf[1] = SEP;
                    len = uloc_getKeywordValue(localeID, key, &buf[2], sizeof(buf) - 2, &tmpStatus);
                    if (U_SUCCESS(tmpStatus)) {
                        if (_isPrivateuseValueSubtags(&buf[2], len)) {
                            /* return private use only tag */
                            reslen = len + 2;
                            uprv_memcpy(langtag, buf, uprv_min(reslen, langtagCapacity));
                            u_terminateChars(langtag, langtagCapacity, reslen, status);
                            done = TRUE;
                        } else if (strict) {
                            *status = U_ILLEGAL_ARGUMENT_ERROR;
                            done = TRUE;
                        }
                        /* if not strict mode, then "und" will be returned */
                    } else {
                        *status = U_ILLEGAL_ARGUMENT_ERROR;
                        done = TRUE;
                    }
                }
            }
            uenum_close(kwdEnum);
            if (done) {
                return reslen;
            }
        }
    }

    reslen += _appendLanguageToLanguageTag(canonical, langtag, langtagCapacity, strict, status);
    reslen += _appendScriptToLanguageTag(canonical, langtag + reslen, langtagCapacity - reslen, strict, status);
    reslen += _appendRegionToLanguageTag(canonical, langtag + reslen, langtagCapacity - reslen, strict, status);
    reslen += _appendVariantsToLanguageTag(canonical, langtag + reslen, langtagCapacity - reslen, strict, &hadPosix, status);
    reslen += _appendKeywordsToLanguageTag(canonical, langtag + reslen, langtagCapacity - reslen, strict, hadPosix, status);
    reslen += _appendPrivateuseToLanguageTag(canonical, langtag + reslen, langtagCapacity - reslen, strict, hadPosix, status);

    return reslen;
}


U_CAPI int32_t U_EXPORT2
uloc_forLanguageTag(const char* langtag,
                    char* localeID,
                    int32_t localeIDCapacity,
                    int32_t* parsedLength,
                    UErrorCode* status) {
    ULanguageTag *lt;
    int32_t reslen = 0;
    const char *subtag, *p;
    int32_t len;
    int32_t i, n;
    UBool noRegion = TRUE;

    lt = ultag_parse(langtag, -1, parsedLength, status);
    if (U_FAILURE(*status)) {
        return 0;
    }

    /* language */
    subtag = ultag_getExtlangSize(lt) > 0 ? ultag_getExtlang(lt, 0) : ultag_getLanguage(lt);
    if (uprv_compareInvCharsAsAscii(subtag, LANG_UND) != 0) {
        len = (int32_t)uprv_strlen(subtag);
        if (len > 0) {
            if (reslen < localeIDCapacity) {
                uprv_memcpy(localeID, subtag, uprv_min(len, localeIDCapacity - reslen));
            }
            reslen += len;
        }
    }

    /* script */
    subtag = ultag_getScript(lt);
    len = (int32_t)uprv_strlen(subtag);
    if (len > 0) {
        if (reslen < localeIDCapacity) {
            *(localeID + reslen) = LOCALE_SEP;
        }
        reslen++;

        /* write out the script in title case */
        p = subtag;
        while (*p) {
            if (reslen < localeIDCapacity) {
                if (p == subtag) {
                    *(localeID + reslen) = uprv_toupper(*p);
                } else {
                    *(localeID + reslen) = *p;
                }
            }
            reslen++;
            p++;
        }
    }

    /* region */
    subtag = ultag_getRegion(lt);
    len = (int32_t)uprv_strlen(subtag);
    if (len > 0) {
        if (reslen < localeIDCapacity) {
            *(localeID + reslen) = LOCALE_SEP;
        }
        reslen++;
        /* write out the retion in upper case */
        p = subtag;
        while (*p) {
            if (reslen < localeIDCapacity) {
                *(localeID + reslen) = uprv_toupper(*p);
            }
            reslen++;
            p++;
        }
        noRegion = FALSE;
    }

    /* variants */
    n = ultag_getVariantsSize(lt);
    if (n > 0) {
        if (noRegion) {
            if (reslen < localeIDCapacity) {
                *(localeID + reslen) = LOCALE_SEP;
            }
            reslen++;
        }

        for (i = 0; i < n; i++) {
            subtag = ultag_getVariant(lt, i);
            if (reslen < localeIDCapacity) {
                *(localeID + reslen) = LOCALE_SEP;
            }
            reslen++;
            /* write out the variant in upper case */
            p = subtag;
            while (*p) {
                if (reslen < localeIDCapacity) {
                    *(localeID + reslen) = uprv_toupper(*p);
                }
                reslen++;
                p++;
            }
        }
    }

    /* keywords */
    n = ultag_getExtensionsSize(lt);
    subtag = ultag_getPrivateUse(lt);
    if (n > 0 || uprv_strlen(subtag) > 0) {
        if (reslen == 0 && n > 0) {
            /* need a language */
            if (reslen < localeIDCapacity) {
                uprv_memcpy(localeID + reslen, LANG_UND, uprv_min(LANG_UND_LEN, localeIDCapacity - reslen));
            }
            reslen += LANG_UND_LEN;
        }
        len = _appendKeywords(lt, localeID + reslen, localeIDCapacity - reslen, status);
        reslen += len;
    }

    ultag_close(lt);
    return u_terminateChars(localeID, localeIDCapacity, reslen, status);
}

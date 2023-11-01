// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  loclikely.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010feb25
*   created by: Markus W. Scherer
*
*   Code for likely and minimized locale subtags, separated out from other .cpp files
*   that then do not depend on resource bundle code and likely-subtags data.
*/

#include "unicode/bytestream.h"
#include "unicode/utypes.h"
#include "unicode/locid.h"
#include "unicode/putil.h"
#include "unicode/uchar.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/uscript.h"
#include "bytesinkutil.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "loclikelysubtags.h"
#include "ulocimp.h"
#include "ustr_imp.h"

/**
 * Append a tag to a buffer, adding the separator if necessary.  The buffer
 * must be large enough to contain the resulting tag plus any separator
 * necessary. The tag must not be a zero-length string.
 *
 * @param tag The tag to add.
 * @param tagLength The length of the tag.
 * @param buffer The output buffer.
 * @param bufferLength The length of the output buffer.  This is an input/output parameter.
 **/
static void U_CALLCONV
appendTag(
    const char* tag,
    int32_t tagLength,
    char* buffer,
    int32_t* bufferLength,
    UBool withSeparator) {

    if (withSeparator) {
        buffer[*bufferLength] = '_';
        ++(*bufferLength);
    }

    uprv_memmove(
        &buffer[*bufferLength],
        tag,
        tagLength);

    *bufferLength += tagLength;
}

/**
 * Create a tag string from the supplied parameters.  The lang, script and region
 * parameters may be nullptr pointers. If they are, their corresponding length parameters
 * must be less than or equal to 0.
 *
 * If any of the language, script or region parameters are empty, and the alternateTags
 * parameter is not nullptr, it will be parsed for potential language, script and region tags
 * to be used when constructing the new tag.  If the alternateTags parameter is nullptr, or
 * it contains no language tag, the default tag for the unknown language is used.
 *
 * If the length of the new string exceeds the capacity of the output buffer, 
 * the function copies as many bytes to the output buffer as it can, and returns
 * the error U_BUFFER_OVERFLOW_ERROR.
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * Note that this function can return the warning U_STRING_NOT_TERMINATED_WARNING if
 * the tag string fits in the output buffer, but the null terminator doesn't.
 *
 * @param lang The language tag to use.
 * @param langLength The length of the language tag.
 * @param script The script tag to use.
 * @param scriptLength The length of the script tag.
 * @param region The region tag to use.
 * @param regionLength The length of the region tag.
 * @param trailing Any trailing data to append to the new tag.
 * @param trailingLength The length of the trailing data.
 * @param alternateTags A string containing any alternate tags.
 * @param sink The output sink receiving the tag string.
 * @param err A pointer to a UErrorCode for error reporting.
 **/
static void U_CALLCONV
createTagStringWithAlternates(
    const char* lang,
    int32_t langLength,
    const char* script,
    int32_t scriptLength,
    const char* region,
    int32_t regionLength,
    const char* trailing,
    int32_t trailingLength,
    const char* alternateTags,
    icu::ByteSink& sink,
    UErrorCode* err) {

    if (U_FAILURE(*err)) {
        goto error;
    }
    else if (langLength >= ULOC_LANG_CAPACITY ||
             scriptLength >= ULOC_SCRIPT_CAPACITY ||
             regionLength >= ULOC_COUNTRY_CAPACITY) {
        goto error;
    }
    else {
        /**
         * ULOC_FULLNAME_CAPACITY will provide enough capacity
         * that we can build a string that contains the language,
         * script and region code without worrying about overrunning
         * the user-supplied buffer.
         **/
        char tagBuffer[ULOC_FULLNAME_CAPACITY];
        int32_t tagLength = 0;
        UBool regionAppended = false;

        if (langLength > 0) {
            appendTag(
                lang,
                langLength,
                tagBuffer,
                &tagLength,
                /*withSeparator=*/false);
        }
        else if (alternateTags == nullptr) {
            /*
             * Use the empty string for an unknown language, if
             * we found no language.
             */
        }
        else {
            /*
             * Parse the alternateTags string for the language.
             */
            char alternateLang[ULOC_LANG_CAPACITY];
            int32_t alternateLangLength = sizeof(alternateLang);

            alternateLangLength =
                uloc_getLanguage(
                    alternateTags,
                    alternateLang,
                    alternateLangLength,
                    err);
            if(U_FAILURE(*err) ||
                alternateLangLength >= ULOC_LANG_CAPACITY) {
                goto error;
            }
            else if (alternateLangLength == 0) {
                /*
                 * Use the empty string for an unknown language, if
                 * we found no language.
                 */
            }
            else {
                appendTag(
                    alternateLang,
                    alternateLangLength,
                    tagBuffer,
                    &tagLength,
                    /*withSeparator=*/false);
            }
        }

        if (scriptLength > 0) {
            appendTag(
                script,
                scriptLength,
                tagBuffer,
                &tagLength,
                /*withSeparator=*/true);
        }
        else if (alternateTags != nullptr) {
            /*
             * Parse the alternateTags string for the script.
             */
            char alternateScript[ULOC_SCRIPT_CAPACITY];

            const int32_t alternateScriptLength =
                uloc_getScript(
                    alternateTags,
                    alternateScript,
                    sizeof(alternateScript),
                    err);

            if (U_FAILURE(*err) ||
                alternateScriptLength >= ULOC_SCRIPT_CAPACITY) {
                goto error;
            }
            else if (alternateScriptLength > 0) {
                appendTag(
                    alternateScript,
                    alternateScriptLength,
                    tagBuffer,
                    &tagLength,
                    /*withSeparator=*/true);
            }
        }

        if (regionLength > 0) {
            appendTag(
                region,
                regionLength,
                tagBuffer,
                &tagLength,
                /*withSeparator=*/true);

            regionAppended = true;
        }
        else if (alternateTags != nullptr) {
            /*
             * Parse the alternateTags string for the region.
             */
            char alternateRegion[ULOC_COUNTRY_CAPACITY];

            const int32_t alternateRegionLength =
                uloc_getCountry(
                    alternateTags,
                    alternateRegion,
                    sizeof(alternateRegion),
                    err);
            if (U_FAILURE(*err) ||
                alternateRegionLength >= ULOC_COUNTRY_CAPACITY) {
                goto error;
            }
            else if (alternateRegionLength > 0) {
                appendTag(
                    alternateRegion,
                    alternateRegionLength,
                    tagBuffer,
                    &tagLength,
                    /*withSeparator=*/true);

                regionAppended = true;
            }
        }

        /**
         * Copy the partial tag from our internal buffer to the supplied
         * target.
         **/
        sink.Append(tagBuffer, tagLength);

        if (trailingLength > 0) {
            if (*trailing != '@') {
                sink.Append("_", 1);
                if (!regionAppended) {
                    /* extra separator is required */
                    sink.Append("_", 1);
                }
            }

            /*
             * Copy the trailing data into the supplied buffer.
             */
            sink.Append(trailing, trailingLength);
        }

        return;
    }

error:

    /**
     * An overflow indicates the locale ID passed in
     * is ill-formed.  If we got here, and there was
     * no previous error, it's an implicit overflow.
     **/
    if (*err ==  U_BUFFER_OVERFLOW_ERROR ||
        U_SUCCESS(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }
}

/**
 * Parse the language, script, and region subtags from a tag string, and copy the
 * results into the corresponding output parameters. The buffers are null-terminated,
 * unless overflow occurs.
 *
 * The langLength, scriptLength, and regionLength parameters are input/output
 * parameters, and must contain the capacity of their corresponding buffers on
 * input.  On output, they will contain the actual length of the buffers, not
 * including the null terminator.
 *
 * If the length of any of the output subtags exceeds the capacity of the corresponding
 * buffer, the function copies as many bytes to the output buffer as it can, and returns
 * the error U_BUFFER_OVERFLOW_ERROR.  It will not parse any more subtags once overflow
 * occurs.
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param localeID The locale ID to parse.
 * @param lang The language tag buffer.
 * @param langLength The length of the language tag.
 * @param script The script tag buffer.
 * @param scriptLength The length of the script tag.
 * @param region The region tag buffer.
 * @param regionLength The length of the region tag.
 * @param err A pointer to a UErrorCode for error reporting.
 * @return The number of chars of the localeID parameter consumed.
 **/
static int32_t U_CALLCONV
parseTagString(
    const char* localeID,
    char* lang,
    int32_t* langLength,
    char* script,
    int32_t* scriptLength,
    char* region,
    int32_t* regionLength,
    UErrorCode* err)
{
    const char* position = localeID;
    int32_t subtagLength = 0;

    if(U_FAILURE(*err) ||
       localeID == nullptr ||
       lang == nullptr ||
       langLength == nullptr ||
       script == nullptr ||
       scriptLength == nullptr ||
       region == nullptr ||
       regionLength == nullptr) {
        goto error;
    }

    subtagLength = ulocimp_getLanguage(position, &position, *err).extract(lang, *langLength, *err);

    /*
     * Note that we explicit consider U_STRING_NOT_TERMINATED_WARNING
     * to be an error, because it indicates the user-supplied tag is
     * not well-formed.
     */
    if(U_FAILURE(*err)) {
        goto error;
    }

    *langLength = subtagLength;

    /*
     * If no language was present, use the empty string instead.
     * Otherwise, move past any separator.
     */
    if (_isIDSeparator(*position)) {
        ++position;
    }

    subtagLength = ulocimp_getScript(position, &position, *err).extract(script, *scriptLength, *err);

    if(U_FAILURE(*err)) {
        goto error;
    }

    *scriptLength = subtagLength;

    if (*scriptLength > 0) {
        /*
         * Move past any separator.
         */
        if (_isIDSeparator(*position)) {
            ++position;
        }    
    }

    subtagLength = ulocimp_getCountry(position, &position, *err).extract(region, *regionLength, *err);

    if(U_FAILURE(*err)) {
        goto error;
    }

    *regionLength = subtagLength;

    if (*regionLength <= 0 && *position != 0 && *position != '@') {
        /* back up over consumed trailing separator */
        --position;
    }

exit:

    return (int32_t)(position - localeID);

error:

    /**
     * If we get here, we have no explicit error, it's the result of an
     * illegal argument.
     **/
    if (!U_FAILURE(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }

    goto exit;
}

#define CHECK_TRAILING_VARIANT_SIZE(trailing, trailingLength) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t count = 0; \
    int32_t i; \
    for (i = 0; i < trailingLength; i++) { \
        if (trailing[i] == '-' || trailing[i] == '_') { \
            count = 0; \
            if (count > 8) { \
                goto error; \
            } \
        } else if (trailing[i] == '@') { \
            break; \
        } else if (count > 8) { \
            goto error; \
        } else { \
            count++; \
        } \
    } \
} UPRV_BLOCK_MACRO_END

static UBool
_uloc_addLikelySubtags(const char* localeID,
                       icu::ByteSink& sink,
                       UErrorCode* err) {
    char lang[ULOC_LANG_CAPACITY];
    int32_t langLength = sizeof(lang);
    char script[ULOC_SCRIPT_CAPACITY];
    int32_t scriptLength = sizeof(script);
    char region[ULOC_COUNTRY_CAPACITY];
    int32_t regionLength = sizeof(region);
    const char* trailing = "";
    int32_t trailingLength = 0;
    int32_t trailingIndex = 0;

    if(U_FAILURE(*err)) {
        goto error;
    }
    if (localeID == nullptr) {
        goto error;
    }

    trailingIndex = parseTagString(
        localeID,
        lang,
        &langLength,
        script,
        &scriptLength,
        region,
        &regionLength,
        err);
    if(U_FAILURE(*err)) {
        /* Overflow indicates an illegal argument error */
        if (*err == U_BUFFER_OVERFLOW_ERROR) {
            *err = U_ILLEGAL_ARGUMENT_ERROR;
        }

        goto error;
    }
    if (langLength > 3) {
        goto error;
    }

    /* Find the length of the trailing portion. */
    while (_isIDSeparator(localeID[trailingIndex])) {
        trailingIndex++;
    }
    trailing = &localeID[trailingIndex];
    trailingLength = (int32_t)uprv_strlen(trailing);

    CHECK_TRAILING_VARIANT_SIZE(trailing, trailingLength);
    {
        const icu::XLikelySubtags* likelySubtags = icu::XLikelySubtags::getSingleton(*err);
        if(U_FAILURE(*err)) {
            goto error;
        }
        // We need to keep l on the stack because lsr may point into internal
        // memory of l.
        icu::Locale l = icu::Locale::createFromName(localeID);
        if (l.isBogus()) {
            goto error;
        }
        icu::LSR lsr = likelySubtags->makeMaximizedLsrFrom(l, true, *err);
        if(U_FAILURE(*err)) {
            goto error;
        }
        const char* language = lsr.language;
        if (uprv_strcmp(language, "und") == 0) {
            language = "";
        }
        createTagStringWithAlternates(
            language,
            (int32_t)uprv_strlen(language),
            lsr.script,
            (int32_t)uprv_strlen(lsr.script),
            lsr.region,
            (int32_t)uprv_strlen(lsr.region),
            trailing,
            trailingLength,
            nullptr,
            sink,
            err);
        if(U_FAILURE(*err)) {
            goto error;
        }
    }
    return true;

error:

    if (!U_FAILURE(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return false;
}

// Add likely subtags to the sink
// return true if the value in the sink is produced by a match during the lookup
// return false if the value in the sink is the same as input because there are
// no match after the lookup.
static UBool _ulocimp_addLikelySubtags(const char*, icu::ByteSink&, UErrorCode*);

static void
_uloc_minimizeSubtags(const char* localeID,
                      icu::ByteSink& sink,
                      bool favorScript,
                      UErrorCode* err) {
    icu::CharString maximizedTagBuffer;

    char lang[ULOC_LANG_CAPACITY];
    int32_t langLength = sizeof(lang);
    char script[ULOC_SCRIPT_CAPACITY];
    int32_t scriptLength = sizeof(script);
    char region[ULOC_COUNTRY_CAPACITY];
    int32_t regionLength = sizeof(region);
    const char* trailing = "";
    int32_t trailingLength = 0;
    int32_t trailingIndex = 0;

    if(U_FAILURE(*err)) {
        goto error;
    }
    else if (localeID == nullptr) {
        goto error;
    }

    trailingIndex =
        parseTagString(
            localeID,
            lang,
            &langLength,
            script,
            &scriptLength,
            region,
            &regionLength,
            err);
    if(U_FAILURE(*err)) {

        /* Overflow indicates an illegal argument error */
        if (*err == U_BUFFER_OVERFLOW_ERROR) {
            *err = U_ILLEGAL_ARGUMENT_ERROR;
        }

        goto error;
    }

    /* Find the spot where the variants or the keywords begin, if any. */
    while (_isIDSeparator(localeID[trailingIndex])) {
        trailingIndex++;
    }
    trailing = &localeID[trailingIndex];
    trailingLength = (int32_t)uprv_strlen(trailing);

    CHECK_TRAILING_VARIANT_SIZE(trailing, trailingLength);

    {
        const icu::XLikelySubtags* likelySubtags = icu::XLikelySubtags::getSingleton(*err);
        if(U_FAILURE(*err)) {
            goto error;
        }
        icu::LSR lsr = likelySubtags->minimizeSubtags(
            {lang, langLength},
            {script, scriptLength},
            {region, regionLength},
            favorScript,
            *err);
        if(U_FAILURE(*err)) {
            goto error;
        }
        const char* language = lsr.language;
        if (uprv_strcmp(language, "und") == 0) {
            language = "";
        }
        createTagStringWithAlternates(
            language,
            (int32_t)uprv_strlen(language),
            lsr.script,
            (int32_t)uprv_strlen(lsr.script),
            lsr.region,
            (int32_t)uprv_strlen(lsr.region),
            trailing,
            trailingLength,
            nullptr,
            sink,
            err);
        if(U_FAILURE(*err)) {
            goto error;
        }
        return;
    }

error:

    if (!U_FAILURE(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }
}

U_CAPI int32_t U_EXPORT2
uloc_addLikelySubtags(const char* localeID,
                      char* maximizedLocaleID,
                      int32_t maximizedLocaleIDCapacity,
                      UErrorCode* status) {
    if (U_FAILURE(*status)) {
        return 0;
    }

    icu::CheckedArrayByteSink sink(
            maximizedLocaleID, maximizedLocaleIDCapacity);

    ulocimp_addLikelySubtags(localeID, sink, status);
    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*status)) {
        return sink.Overflowed() ? reslen : -1;
    }

    if (sink.Overflowed()) {
        *status = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(
                maximizedLocaleID, maximizedLocaleIDCapacity, reslen, status);
    }

    return reslen;
}

static UBool
_ulocimp_addLikelySubtags(const char* localeID,
                          icu::ByteSink& sink,
                          UErrorCode* status) {
    icu::CharString localeBuffer;
    {
        icu::CharStringByteSink localeSink(&localeBuffer);
        ulocimp_canonicalize(localeID, localeSink, status);
    }
    if (U_SUCCESS(*status)) {
        return _uloc_addLikelySubtags(localeBuffer.data(), sink, status);
    } else {
        return false;
    }
}

U_CAPI void U_EXPORT2
ulocimp_addLikelySubtags(const char* localeID,
                         icu::ByteSink& sink,
                         UErrorCode* status) {
    _ulocimp_addLikelySubtags(localeID, sink, status);
}

U_CAPI int32_t U_EXPORT2
uloc_minimizeSubtags(const char* localeID,
                     char* minimizedLocaleID,
                     int32_t minimizedLocaleIDCapacity,
                     UErrorCode* status) {
    if (U_FAILURE(*status)) {
        return 0;
    }

    icu::CheckedArrayByteSink sink(
            minimizedLocaleID, minimizedLocaleIDCapacity);

    ulocimp_minimizeSubtags(localeID, sink, false, status);
    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*status)) {
        return sink.Overflowed() ? reslen : -1;
    }

    if (sink.Overflowed()) {
        *status = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(
                minimizedLocaleID, minimizedLocaleIDCapacity, reslen, status);
    }

    return reslen;
}

U_CAPI void U_EXPORT2
ulocimp_minimizeSubtags(const char* localeID,
                        icu::ByteSink& sink,
                        bool favorScript,
                        UErrorCode* status) {
    icu::CharString localeBuffer;
    {
        icu::CharStringByteSink localeSink(&localeBuffer);
        ulocimp_canonicalize(localeID, localeSink, status);
    }
    _uloc_minimizeSubtags(localeBuffer.data(), sink, favorScript, status);
}

// Pairs of (language subtag, + or -) for finding out fast if common languages
// are LTR (minus) or RTL (plus).
static const char LANG_DIR_STRING[] =
        "root-en-es-pt-zh-ja-ko-de-fr-it-ar+he+fa+ru-nl-pl-th-tr-";

// Implemented here because this calls ulocimp_addLikelySubtags().
U_CAPI UBool U_EXPORT2
uloc_isRightToLeft(const char *locale) {
    UErrorCode errorCode = U_ZERO_ERROR;
    char script[8];
    int32_t scriptLength = uloc_getScript(locale, script, UPRV_LENGTHOF(script), &errorCode);
    if (U_FAILURE(errorCode) || errorCode == U_STRING_NOT_TERMINATED_WARNING ||
            scriptLength == 0) {
        // Fastpath: We know the likely scripts and their writing direction
        // for some common languages.
        errorCode = U_ZERO_ERROR;
        char lang[8];
        int32_t langLength = uloc_getLanguage(locale, lang, UPRV_LENGTHOF(lang), &errorCode);
        if (U_FAILURE(errorCode) || errorCode == U_STRING_NOT_TERMINATED_WARNING) {
            return false;
        }
        if (langLength > 0) {
            const char* langPtr = uprv_strstr(LANG_DIR_STRING, lang);
            if (langPtr != nullptr) {
                switch (langPtr[langLength]) {
                case '-': return false;
                case '+': return true;
                default: break;  // partial match of a longer code
                }
            }
        }
        // Otherwise, find the likely script.
        errorCode = U_ZERO_ERROR;
        icu::CharString likely;
        {
            icu::CharStringByteSink sink(&likely);
            ulocimp_addLikelySubtags(locale, sink, &errorCode);
        }
        if (U_FAILURE(errorCode) || errorCode == U_STRING_NOT_TERMINATED_WARNING) {
            return false;
        }
        scriptLength = uloc_getScript(likely.data(), script, UPRV_LENGTHOF(script), &errorCode);
        if (U_FAILURE(errorCode) || errorCode == U_STRING_NOT_TERMINATED_WARNING ||
                scriptLength == 0) {
            return false;
        }
    }
    UScriptCode scriptCode = (UScriptCode)u_getPropertyValueEnum(UCHAR_SCRIPT, script);
    return uscript_isRightToLeft(scriptCode);
}

U_NAMESPACE_BEGIN

UBool
Locale::isRightToLeft() const {
    return uloc_isRightToLeft(getBaseName());
}

U_NAMESPACE_END

// The following must at least allow for rg key value (6) plus terminator (1).
#define ULOC_RG_BUFLEN 8

U_CAPI int32_t U_EXPORT2
ulocimp_getRegionForSupplementalData(const char *localeID, UBool inferRegion,
                                     char *region, int32_t regionCapacity, UErrorCode* status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    char rgBuf[ULOC_RG_BUFLEN];
    UErrorCode rgStatus = U_ZERO_ERROR;

    // First check for rg keyword value
    icu::CharString rg;
    {
        icu::CharStringByteSink sink(&rg);
        ulocimp_getKeywordValue(localeID, "rg", sink, &rgStatus);
    }
    int32_t rgLen = rg.length();
    if (U_FAILURE(rgStatus) || rgLen < 3 || rgLen > 7) {
        rgLen = 0;
    } else {
        // chop off the subdivision code (which will generally be "zzzz" anyway)
        const char* const data = rg.data();
        if (uprv_isASCIILetter(data[0])) {
            rgLen = 2;
            rgBuf[0] = uprv_toupper(data[0]);
            rgBuf[1] = uprv_toupper(data[1]);
        } else {
            // assume three-digit region code
            rgLen = 3;
            uprv_memcpy(rgBuf, data, rgLen);
        }
    }

    if (rgLen == 0) {
        // No valid rg keyword value, try for unicode_region_subtag
        rgLen = uloc_getCountry(localeID, rgBuf, ULOC_RG_BUFLEN, status);
        if (U_FAILURE(*status)) {
            rgLen = 0;
        } else if (rgLen == 0 && inferRegion) {
            // no unicode_region_subtag but inferRegion true, try likely subtags
            rgStatus = U_ZERO_ERROR;
            icu::CharString locBuf;
            {
                icu::CharStringByteSink sink(&locBuf);
                ulocimp_addLikelySubtags(localeID, sink, &rgStatus);
            }
            if (U_SUCCESS(rgStatus)) {
                rgLen = uloc_getCountry(locBuf.data(), rgBuf, ULOC_RG_BUFLEN, status);
                if (U_FAILURE(*status)) {
                    rgLen = 0;
                }
            }
        }
    }

    rgBuf[rgLen] = 0;
    uprv_strncpy(region, rgBuf, regionCapacity);
    return u_terminateChars(region, regionCapacity, rgLen, status);
}


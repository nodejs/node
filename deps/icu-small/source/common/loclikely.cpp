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
#include "ulocimp.h"
#include "ustr_imp.h"

/**
 * These are the canonical strings for unknown languages, scripts and regions.
 **/
static const char* const unknownLanguage = "und";
static const char* const unknownScript = "Zzzz";
static const char* const unknownRegion = "ZZ";

/**
 * This function looks for the localeID in the likelySubtags resource.
 *
 * @param localeID The tag to find.
 * @param buffer A buffer to hold the matching entry
 * @param bufferLength The length of the output buffer
 * @return A pointer to "buffer" if found, or a null pointer if not.
 */
static const char*  U_CALLCONV
findLikelySubtags(const char* localeID,
                  char* buffer,
                  int32_t bufferLength,
                  UErrorCode* err) {
    const char* result = NULL;

    if (!U_FAILURE(*err)) {
        int32_t resLen = 0;
        const UChar* s = NULL;
        UErrorCode tmpErr = U_ZERO_ERROR;
        icu::LocalUResourceBundlePointer subtags(ures_openDirect(NULL, "likelySubtags", &tmpErr));
        if (U_SUCCESS(tmpErr)) {
            icu::CharString und;
            if (localeID != NULL) {
                if (*localeID == '\0') {
                    localeID = unknownLanguage;
                } else if (*localeID == '_') {
                    und.append(unknownLanguage, *err);
                    und.append(localeID, *err);
                    if (U_FAILURE(*err)) {
                        return NULL;
                    }
                    localeID = und.data();
                }
            }
            s = ures_getStringByKey(subtags.getAlias(), localeID, &resLen, &tmpErr);

            if (U_FAILURE(tmpErr)) {
                /*
                 * If a resource is missing, it's not really an error, it's
                 * just that we don't have any data for that particular locale ID.
                 */
                if (tmpErr != U_MISSING_RESOURCE_ERROR) {
                    *err = tmpErr;
                }
            }
            else if (resLen >= bufferLength) {
                /* The buffer should never overflow. */
                *err = U_INTERNAL_PROGRAM_ERROR;
            }
            else {
                u_UCharsToChars(s, buffer, resLen + 1);
                if (resLen >= 3 &&
                    uprv_strnicmp(buffer, unknownLanguage, 3) == 0 &&
                    (resLen == 3 || buffer[3] == '_')) {
                    uprv_memmove(buffer, buffer + 3, resLen - 3 + 1);
                }
                result = buffer;
            }
        } else {
            *err = tmpErr;
        }
    }

    return result;
}

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
 * parameters may be NULL pointers. If they are, their corresponding length parameters
 * must be less than or equal to 0.
 *
 * If any of the language, script or region parameters are empty, and the alternateTags
 * parameter is not NULL, it will be parsed for potential language, script and region tags
 * to be used when constructing the new tag.  If the alternateTags parameter is NULL, or
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
        else if (alternateTags == NULL) {
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
        else if (alternateTags != NULL) {
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
        else if (alternateTags != NULL) {
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
 * Create a tag string from the supplied parameters.  The lang, script and region
 * parameters may be NULL pointers. If they are, their corresponding length parameters
 * must be less than or equal to 0.  If the lang parameter is an empty string, the
 * default value for an unknown language is written to the output buffer.
 *
 * If the length of the new string exceeds the capacity of the output buffer, 
 * the function copies as many bytes to the output buffer as it can, and returns
 * the error U_BUFFER_OVERFLOW_ERROR.
 *
 * If an illegal argument is provided, the function returns the error
 * U_ILLEGAL_ARGUMENT_ERROR.
 *
 * @param lang The language tag to use.
 * @param langLength The length of the language tag.
 * @param script The script tag to use.
 * @param scriptLength The length of the script tag.
 * @param region The region tag to use.
 * @param regionLength The length of the region tag.
 * @param trailing Any trailing data to append to the new tag.
 * @param trailingLength The length of the trailing data.
 * @param sink The output sink receiving the tag string.
 * @param err A pointer to a UErrorCode for error reporting.
 **/
static void U_CALLCONV
createTagString(
    const char* lang,
    int32_t langLength,
    const char* script,
    int32_t scriptLength,
    const char* region,
    int32_t regionLength,
    const char* trailing,
    int32_t trailingLength,
    icu::ByteSink& sink,
    UErrorCode* err)
{
    createTagStringWithAlternates(
                lang,
                langLength,
                script,
                scriptLength,
                region,
                regionLength,
                trailing,
                trailingLength,
                NULL,
                sink,
                err);
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
       localeID == NULL ||
       lang == NULL ||
       langLength == NULL ||
       script == NULL ||
       scriptLength == NULL ||
       region == NULL ||
       regionLength == NULL) {
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
        if (uprv_strnicmp(script, unknownScript, *scriptLength) == 0) {
            /**
             * If the script part is the "unknown" script, then don't return it.
             **/
            *scriptLength = 0;
        }

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

    if (*regionLength > 0) {
        if (uprv_strnicmp(region, unknownRegion, *regionLength) == 0) {
            /**
             * If the region part is the "unknown" region, then don't return it.
             **/
            *regionLength = 0;
        }
    } else if (*position != 0 && *position != '@') {
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

static UBool U_CALLCONV
createLikelySubtagsString(
    const char* lang,
    int32_t langLength,
    const char* script,
    int32_t scriptLength,
    const char* region,
    int32_t regionLength,
    const char* variants,
    int32_t variantsLength,
    icu::ByteSink& sink,
    UErrorCode* err) {
    /**
     * ULOC_FULLNAME_CAPACITY will provide enough capacity
     * that we can build a string that contains the language,
     * script and region code without worrying about overrunning
     * the user-supplied buffer.
     **/
    char likelySubtagsBuffer[ULOC_FULLNAME_CAPACITY];

    if(U_FAILURE(*err)) {
        goto error;
    }

    /**
     * Try the language with the script and region first.
     **/
    if (scriptLength > 0 && regionLength > 0) {

        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                langLength,
                script,
                scriptLength,
                region,
                regionLength,
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != NULL) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        NULL,
                        0,
                        NULL,
                        0,
                        NULL,
                        0,
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    /**
     * Try the language with just the script.
     **/
    if (scriptLength > 0) {

        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                langLength,
                script,
                scriptLength,
                NULL,
                0,
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != NULL) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        NULL,
                        0,
                        NULL,
                        0,
                        region,
                        regionLength,
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    /**
     * Try the language with just the region.
     **/
    if (regionLength > 0) {

        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                langLength,
                NULL,
                0,
                region,
                regionLength,
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != NULL) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        NULL,
                        0,
                        script,
                        scriptLength,
                        NULL,
                        0,
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    /**
     * Finally, try just the language.
     **/
    {
        const char* likelySubtags = NULL;

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink sink(&tagBuffer);
            createTagString(
                lang,
                langLength,
                NULL,
                0,
                NULL,
                0,
                NULL,
                0,
                sink,
                err);
        }
        if(U_FAILURE(*err)) {
            goto error;
        }

        likelySubtags =
            findLikelySubtags(
                tagBuffer.data(),
                likelySubtagsBuffer,
                sizeof(likelySubtagsBuffer),
                err);
        if(U_FAILURE(*err)) {
            goto error;
        }

        if (likelySubtags != NULL) {
            /* Always use the language tag from the
               maximal string, since it may be more
               specific than the one provided. */
            createTagStringWithAlternates(
                        NULL,
                        0,
                        script,
                        scriptLength,
                        region,
                        regionLength,
                        variants,
                        variantsLength,
                        likelySubtags,
                        sink,
                        err);
            return true;
        }
    }

    return false;

error:

    if (!U_FAILURE(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }

    return false;
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
    UBool success = false;

    if(U_FAILURE(*err)) {
        goto error;
    }
    if (localeID == NULL) {
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

    /* Find the length of the trailing portion. */
    while (_isIDSeparator(localeID[trailingIndex])) {
        trailingIndex++;
    }
    trailing = &localeID[trailingIndex];
    trailingLength = (int32_t)uprv_strlen(trailing);

    CHECK_TRAILING_VARIANT_SIZE(trailing, trailingLength);

    success =
        createLikelySubtagsString(
            lang,
            langLength,
            script,
            scriptLength,
            region,
            regionLength,
            trailing,
            trailingLength,
            sink,
            err);

    if (!success) {
        const int32_t localIDLength = (int32_t)uprv_strlen(localeID);

        /*
         * If we get here, we need to return localeID.
         */
        sink.Append(localeID, localIDLength);
    }

    return success;

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
    UBool successGetMax = false;

    if(U_FAILURE(*err)) {
        goto error;
    }
    else if (localeID == NULL) {
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
        icu::CharString base;
        {
            icu::CharStringByteSink baseSink(&base);
            createTagString(
                lang,
                langLength,
                script,
                scriptLength,
                region,
                regionLength,
                NULL,
                0,
                baseSink,
                err);
        }

        /**
         * First, we need to first get the maximization
         * from AddLikelySubtags.
         **/
        {
            icu::CharStringByteSink maxSink(&maximizedTagBuffer);
            successGetMax = _ulocimp_addLikelySubtags(base.data(), maxSink, err);
        }
    }

    if(U_FAILURE(*err)) {
        goto error;
    }

    if (!successGetMax) {
        /**
         * If we got here, return the locale ID parameter unchanged.
         **/
        const int32_t localeIDLength = (int32_t)uprv_strlen(localeID);
        sink.Append(localeID, localeIDLength);
        return;
    }

    // In the following, the lang, script, region are referring to those in
    // the maximizedTagBuffer, not the one in the localeID.
    langLength = sizeof(lang);
    scriptLength = sizeof(script);
    regionLength = sizeof(region);
    parseTagString(
        maximizedTagBuffer.data(),
        lang,
        &langLength,
        script,
        &scriptLength,
        region,
        &regionLength,
        err);
    if(U_FAILURE(*err)) {
        goto error;
    }

    /**
     * Start first with just the language.
     **/
    {
        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink tagSink(&tagBuffer);
            createLikelySubtagsString(
                lang,
                langLength,
                NULL,
                0,
                NULL,
                0,
                NULL,
                0,
                tagSink,
                err);
        }

        if(U_FAILURE(*err)) {
            goto error;
        }
        else if (!tagBuffer.isEmpty() &&
                 uprv_strnicmp(
                    maximizedTagBuffer.data(),
                    tagBuffer.data(),
                    tagBuffer.length()) == 0) {

            createTagString(
                        lang,
                        langLength,
                        NULL,
                        0,
                        NULL,
                        0,
                        trailing,
                        trailingLength,
                        sink,
                        err);
            return;
        }
    }

    /**
     * Next, try the language and region.
     **/
    if (regionLength > 0) {

        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink tagSink(&tagBuffer);
            createLikelySubtagsString(
                lang,
                langLength,
                NULL,
                0,
                region,
                regionLength,
                NULL,
                0,
                tagSink,
                err);
        }

        if(U_FAILURE(*err)) {
            goto error;
        }
        else if (!tagBuffer.isEmpty() &&
                 uprv_strnicmp(
                    maximizedTagBuffer.data(),
                    tagBuffer.data(),
                    tagBuffer.length()) == 0) {

            createTagString(
                        lang,
                        langLength,
                        NULL,
                        0,
                        region,
                        regionLength,
                        trailing,
                        trailingLength,
                        sink,
                        err);
            return;
        }
    }

    /**
     * Finally, try the language and script.  This is our last chance,
     * since trying with all three subtags would only yield the
     * maximal version that we already have.
     **/
    if (scriptLength > 0) {
        icu::CharString tagBuffer;
        {
            icu::CharStringByteSink tagSink(&tagBuffer);
            createLikelySubtagsString(
                lang,
                langLength,
                script,
                scriptLength,
                NULL,
                0,
                NULL,
                0,
                tagSink,
                err);
        }

        if(U_FAILURE(*err)) {
            goto error;
        }
        else if (!tagBuffer.isEmpty() &&
                 uprv_strnicmp(
                    maximizedTagBuffer.data(),
                    tagBuffer.data(),
                    tagBuffer.length()) == 0) {

            createTagString(
                        lang,
                        langLength,
                        script,
                        scriptLength,
                        NULL,
                        0,
                        trailing,
                        trailingLength,
                        sink,
                        err);
            return;
        }
    }

    {
        /**
         * If we got here, return the max + trail.
         **/
        createTagString(
                    lang,
                    langLength,
                    script,
                    scriptLength,
                    region,
                    regionLength,
                    trailing,
                    trailingLength,
                    sink,
                    err);
        return;
    }

error:

    if (!U_FAILURE(*err)) {
        *err = U_ILLEGAL_ARGUMENT_ERROR;
    }
}

static int32_t
do_canonicalize(const char*    localeID,
         char* buffer,
         int32_t bufferCapacity,
         UErrorCode* err)
{
    int32_t canonicalizedSize = uloc_canonicalize(
        localeID,
        buffer,
        bufferCapacity,
        err);

    if (*err == U_STRING_NOT_TERMINATED_WARNING ||
        *err == U_BUFFER_OVERFLOW_ERROR) {
        return canonicalizedSize;
    }
    else if (U_FAILURE(*err)) {

        return -1;
    }
    else {
        return canonicalizedSize;
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
    PreflightingLocaleIDBuffer localeBuffer;
    do {
        localeBuffer.requestedCapacity = do_canonicalize(localeID, localeBuffer.getBuffer(),
            localeBuffer.getCapacity(), status);
    } while (localeBuffer.needToTryAgain(status));
    
    if (U_SUCCESS(*status)) {
        return _uloc_addLikelySubtags(localeBuffer.getBuffer(), sink, status);
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

    ulocimp_minimizeSubtags(localeID, sink, status);
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
                        UErrorCode* status) {
    PreflightingLocaleIDBuffer localeBuffer;
    do {
        localeBuffer.requestedCapacity = do_canonicalize(localeID, localeBuffer.getBuffer(),
            localeBuffer.getCapacity(), status);
    } while (localeBuffer.needToTryAgain(status));
    
    _uloc_minimizeSubtags(localeBuffer.getBuffer(), sink, status);
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
            if (langPtr != NULL) {
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
    int32_t rgLen = uloc_getKeywordValue(localeID, "rg", rgBuf, ULOC_RG_BUFLEN, &rgStatus);
    if (U_FAILURE(rgStatus) || rgLen != 6) {
        rgLen = 0;
    } else {
        // rgBuf guaranteed to be zero terminated here, with text len 6
        char *rgPtr = rgBuf;
        for (; *rgPtr!= 0; rgPtr++) {
            *rgPtr = uprv_toupper(*rgPtr);
        }
        rgLen = (uprv_strcmp(rgBuf+2, "ZZZZ") == 0)? 2: 0;
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


/*
***************************************************************************
* Copyright (C) 2008-2015, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*   file name:  uspoof.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2008Feb13
*   created by: Andy Heninger
*
*   Unicode Spoof Detection
*/

#ifndef USPOOF_H
#define USPOOF_H

#include "unicode/utypes.h"
#include "unicode/uset.h"
#include "unicode/parseerr.h"
#include "unicode/localpointer.h"

#if !UCONFIG_NO_NORMALIZATION


#if U_SHOW_CPLUSPLUS_API
#include "unicode/unistr.h"
#include "unicode/uniset.h"
#endif


/**
 * \file
 * \brief Unicode Security and Spoofing Detection, C API.
 *
 * These functions are intended to check strings, typically
 * identifiers of some type, such as URLs, for the presence of
 * characters that are likely to be visually confusing - 
 * for cases where the displayed form of an identifier may
 * not be what it appears to be.
 *
 * Unicode Technical Report #36, http://unicode.org/reports/tr36, and
 * Unicode Technical Standard #39, http://unicode.org/reports/tr39
 * "Unicode security considerations", give more background on 
 * security an spoofing issues with Unicode identifiers.
 * The tests and checks provided by this module implement the recommendations
 * from those Unicode documents.
 *
 * The tests available on identifiers fall into two general categories:
 *   -#  Single identifier tests.  Check whether an identifier is
 *       potentially confusable with any other string, or is suspicious
 *       for other reasons.
 *   -#  Two identifier tests.  Check whether two specific identifiers are confusable.
 *       This does not consider whether either of strings is potentially
 *       confusable with any string other than the exact one specified.
 *
 * The steps to perform confusability testing are
 *   -#  Open a USpoofChecker.
 *   -#  Configure the USPoofChecker for the desired set of tests.  The tests that will
 *       be performed are specified by a set of USpoofChecks flags.
 *   -#  Perform the checks using the pre-configured USpoofChecker.  The results indicate
 *       which (if any) of the selected tests have identified possible problems with the identifier.
 *       Results are reported as a set of USpoofChecks flags;  this mirrors the form in which
 *       the set of tests to perform was originally specified to the USpoofChecker.
 *
 * A USpoofChecker may be used repeatedly to perform checks on any number of identifiers.
 *
 * Thread Safety: The test functions for checking a single identifier, or for testing 
 * whether two identifiers are possible confusable, are thread safe.  
 * They may called concurrently, from multiple threads, using the same USpoofChecker instance.
 *
 * More generally, the standard ICU thread safety rules apply:  functions that take a
 * const USpoofChecker parameter are thread safe.  Those that take a non-const 
 * USpoofChecier are not thread safe.
 *
 *
 * Descriptions of the available checks.
 *
 * When testing whether pairs of identifiers are confusable, with the uspoof_areConfusable()
 * family of functions, the relevant tests are
 *
 *   -# USPOOF_SINGLE_SCRIPT_CONFUSABLE:  All of the characters from the two identifiers are
 *      from a single script, and the two identifiers are visually confusable.
 *   -# USPOOF_MIXED_SCRIPT_CONFUSABLE:  At least one of the identifiers contains characters
 *      from more than one script, and the two identifiers are visually confusable.
 *   -# USPOOF_WHOLE_SCRIPT_CONFUSABLE: Each of the two identifiers is of a single script, but
 *      the two identifiers are from different scripts, and they are visually confusable.
 *
 * The safest approach is to enable all three of these checks as a group.
 *
 * USPOOF_ANY_CASE is a modifier for the above tests.  If the identifiers being checked can
 * be of mixed case and are used in a case-sensitive manner, this option should be specified.
 *
 * If the identifiers being checked are used in a case-insensitive manner, and if they are
 * displayed to users in lower-case form only, the USPOOF_ANY_CASE option should not be
 * specified.  Confusabality issues involving upper case letters will not be reported.
 *
 * When performing tests on a single identifier, with the uspoof_check() family of functions,
 * the relevant tests are:
 *
 *    -# USPOOF_MIXED_SCRIPT_CONFUSABLE: the identifier contains characters from multiple
 *       scripts, and there exists an identifier of a single script that is visually confusable.
 *    -# USPOOF_WHOLE_SCRIPT_CONFUSABLE: the identifier consists of characters from a single
 *       script, and there exists a visually confusable identifier.
 *       The visually confusable identifier also consists of characters from a single script.
 *       but not the same script as the identifier being checked.
 *    -# USPOOF_ANY_CASE: modifies the mixed script and whole script confusables tests.  If
 *       specified, the checks will consider confusable characters of any case.  If this flag is not
 *       set, the test is performed assuming case folded identifiers.
 *    -# USPOOF_SINGLE_SCRIPT: check that the identifier contains only characters from a
 *       single script.  (Characters from the 'common' and 'inherited' scripts are ignored.)
 *       This is not a test for confusable identifiers
 *    -# USPOOF_INVISIBLE: check an identifier for the presence of invisible characters,
 *       such as zero-width spaces, or character sequences that are
 *       likely not to display, such as multiple occurrences of the same
 *       non-spacing mark.  This check does not test the input string as a whole
 *       for conformance to any particular syntax for identifiers.
 *    -# USPOOF_CHAR_LIMIT: check that an identifier contains only characters from a specified set
 *       of acceptable characters.  See uspoof_setAllowedChars() and
 *       uspoof_setAllowedLocales().
 *
 *  Note on Scripts:
 *     Characters from the Unicode Scripts "Common" and "Inherited" are ignored when considering
 *     the script of an identifier. Common characters include digits and symbols that
 *     are normally used with text from more than one script.
 *
 *  Identifier Skeletons:  A skeleton is a transformation of an identifier, such that
 *  all identifiers that are confusable with each other have the same skeleton.
 *  Using skeletons, it is possible to build a dictionary data structure for
 *  a set of identifiers, and then quickly test whether a new identifier is
 *  confusable with an identifier already in the set.  The uspoof_getSkeleton()
 *  family of functions will produce the skeleton from an identifier.
 *
 *  Note that skeletons are not guaranteed to be stable between versions 
 *  of Unicode or ICU, so an applications should not rely on creating a permanent,
 *  or difficult to update, database of skeletons.  Instabilities result from
 *  identifying new pairs or sequences of characters that are visually
 *  confusable, and thus must be mapped to the same skeleton character(s).
 *
 *  Skeletons are computed using the algorithm and data describe in Unicode UAX 39.
 *  The latest proposed update, UAX 39 Version 8 draft 1, says "the tables SL, SA, and ML
 *  were still problematic, and discouraged from use in [Uniocde] 7.0.
 *  They were thus removed from version 8.0"
 *
 *  In light of this, the default mapping data included with ICU 55 uses the
 *  Unicode 7 MA (Multi script Any case) table data for the other type options
 *  (Single Script, Any Case), (Single Script, Lower Case) and (Multi Script, Lower Case).
 */

struct USpoofChecker;
typedef struct USpoofChecker USpoofChecker; /**< typedef for C of USpoofChecker */

/**
 * Enum for the kinds of checks that USpoofChecker can perform.
 * These enum values are used both to select the set of checks that
 * will be performed, and to report results from the check function.
 *
 * @stable ICU 4.2
 */
typedef enum USpoofChecks {
    /**   Single script confusable test.
      *   When testing whether two identifiers are confusable, report that they are if
      *   both are from the same script and they are visually confusable.
      *   Note: this test is not applicable to a check of a single identifier.
      */
    USPOOF_SINGLE_SCRIPT_CONFUSABLE =   1,

    /** Mixed script confusable test.
     *  When checking a single identifier, report a problem if
     *    the identifier contains multiple scripts, and
     *    is confusable with some other identifier in a single script
     *  When testing whether two identifiers are confusable, report that they are if
     *    the two IDs are visually confusable, 
     *    and at least one contains characters from more than one script.
     */
    USPOOF_MIXED_SCRIPT_CONFUSABLE  =   2,

    /** Whole script confusable test.
     *  When checking a single identifier, report a problem if
     *    The identifier is of a single script, and
     *    there exists a confusable identifier in another script.
     *  When testing whether two identifiers are confusable, report that they are if
     *    each is of a single script, 
     *    the scripts of the two identifiers are different, and
     *    the identifiers are visually confusable.
     */
    USPOOF_WHOLE_SCRIPT_CONFUSABLE  =   4,
    
    /** Any Case Modifier for confusable identifier tests.
        If specified, consider all characters, of any case, when looking for confusables.
        If USPOOF_ANY_CASE is not specified, identifiers being checked are assumed to have been
        case folded.  Upper case confusable characters will not be checked.
        Selects between Lower Case Confusable and
        Any Case Confusable.   */
    USPOOF_ANY_CASE                 =   8,

    /**
      * Check that an identifier is no looser than the specified RestrictionLevel.
      * The default if uspoof_setRestrctionLevel() is not called is HIGHLY_RESTRICTIVE.
      *
      * If USPOOF_AUX_INFO is enabled the actual restriction level of the
      * identifier being tested will also be returned by uspoof_check().
      *
      * @see URestrictionLevel
      * @see uspoof_setRestrictionLevel
      * @see USPOOF_AUX_INFO
      *
      * @stable ICU 51
      */
    USPOOF_RESTRICTION_LEVEL        = 16,

#ifndef U_HIDE_DEPRECATED_API 
    /** Check that an identifier contains only characters from a
      * single script (plus chars from the common and inherited scripts.)
      * Applies to checks of a single identifier check only.
      * @deprecated ICU 51  Use RESTRICTION_LEVEL instead.
      */
    USPOOF_SINGLE_SCRIPT            =  USPOOF_RESTRICTION_LEVEL,
#endif  /* U_HIDE_DEPRECATED_API */
   
    /** Check an identifier for the presence of invisible characters,
      * such as zero-width spaces, or character sequences that are
      * likely not to display, such as multiple occurrences of the same
      * non-spacing mark.  This check does not test the input string as a whole
      * for conformance to any particular syntax for identifiers.
      */
    USPOOF_INVISIBLE                =  32,

    /** Check that an identifier contains only characters from a specified set
      * of acceptable characters.  See uspoof_setAllowedChars() and
      * uspoof_setAllowedLocales().
      */
    USPOOF_CHAR_LIMIT               =  64,

   /**
     * Check that an identifier does not include decimal digits from
     * more than one numbering system.
     * 
     * @stable ICU 51
     */
    USPOOF_MIXED_NUMBERS            = 128,

   /**
     * Enable all spoof checks.
     * 
     * @stable ICU 4.6
     */
    USPOOF_ALL_CHECKS               = 0xFFFF,

    /**
      * Enable the return of auxillary (non-error) information in the
      * upper bits of the check results value. 
      *
      * If this "check" is not enabled, the results of uspoof_check() will be zero when an
      * identifier passes all of the enabled checks.
      *
      * If this "check" is enabled, (uspoof_check() & USPOOF_ALL_CHECKS) will be zero
      * when an identifier passes all checks.
      *
      * @stable ICU 51
      */
    USPOOF_AUX_INFO                  = 0x40000000

    } USpoofChecks;
    
    
    /**
     * Constants from UAX #39 for use in setRestrictionLevel(), and
     * for returned identifier restriction levels in check results.
     * @stable ICU 51
     */
    typedef enum URestrictionLevel {
        /**
         * Only ASCII characters: U+0000..U+007F
         * 
         * @stable ICU 51
         */
        USPOOF_ASCII = 0x10000000,
        /**
          * All characters in each identifier must be from a single script.
          *
          * @stable ICU 53
          */
        USPOOF_SINGLE_SCRIPT_RESTRICTIVE = 0x20000000,
        /**
         * All characters in each identifier must be from a single script, or from the combinations: Latin + Han +
         * Hiragana + Katakana; Latin + Han + Bopomofo; or Latin + Han + Hangul. Note that this level will satisfy the
         * vast majority of Latin-script users; also that TR36 has ASCII instead of Latin.
         * 
         * @stable ICU 51
         */
        USPOOF_HIGHLY_RESTRICTIVE = 0x30000000,
        /**
         * Allow Latin with other scripts except Cyrillic, Greek, Cherokee Otherwise, the same as Highly Restrictive
         * 
         * @stable ICU 51
         */
        USPOOF_MODERATELY_RESTRICTIVE = 0x40000000,
        /**
         * Allow arbitrary mixtures of scripts. Otherwise, the same as Moderately Restrictive.
         * 
         * @stable ICU 51
         */
        USPOOF_MINIMALLY_RESTRICTIVE = 0x50000000,
        /**
         * Any valid identifiers, including characters outside of the Identifier Profile.
         * 
         * @stable ICU 51
         */
        USPOOF_UNRESTRICTIVE = 0x60000000,
        /**
          * Mask for selecting the Restriction Level bits from the return value of uspoof_check().
          * 
          * @stable ICU 53
          */
         USPOOF_RESTRICTION_LEVEL_MASK = 0x7F000000 
    } URestrictionLevel;

/**
 *  Create a Unicode Spoof Checker, configured to perform all 
 *  checks except for USPOOF_LOCALE_LIMIT and USPOOF_CHAR_LIMIT.
 *  Note that additional checks may be added in the future,
 *  resulting in the changes to the default checking behavior.
 *
 *  @param status  The error code, set if this function encounters a problem.
 *  @return        the newly created Spoof Checker
 *  @stable ICU 4.2
 */
U_STABLE USpoofChecker * U_EXPORT2
uspoof_open(UErrorCode *status);


/**
 * Open a Spoof checker from its serialized form, stored in 32-bit-aligned memory.
 * Inverse of uspoof_serialize().
 * The memory containing the serialized data must remain valid and unchanged
 * as long as the spoof checker, or any cloned copies of the spoof checker,
 * are in use.  Ownership of the memory remains with the caller.
 * The spoof checker (and any clones) must be closed prior to deleting the
 * serialized data.
 *
 * @param data a pointer to 32-bit-aligned memory containing the serialized form of spoof data
 * @param length the number of bytes available at data;
 *               can be more than necessary
 * @param pActualLength receives the actual number of bytes at data taken up by the data;
 *                      can be NULL
 * @param pErrorCode ICU error code
 * @return the spoof checker.
 *
 * @see uspoof_open
 * @see uspoof_serialize
 * @stable ICU 4.2
 */
U_STABLE USpoofChecker * U_EXPORT2
uspoof_openFromSerialized(const void *data, int32_t length, int32_t *pActualLength,
                          UErrorCode *pErrorCode);

/**
  * Open a Spoof Checker from the source form of the spoof data.
  * The two inputs correspond to the Unicode data files confusables.txt
  * and confusablesWholeScript.txt as described in Unicode UAX #39.
  * The syntax of the source data is as described in UAX #39 for
  * these files, and the content of these files is acceptable input.
  *
  * The character encoding of the (char *) input text is UTF-8.
  *
  * @param confusables a pointer to the confusable characters definitions,
  *                    as found in file confusables.txt from unicode.org.
  * @param confusablesLen The length of the confusables text, or -1 if the
  *                    input string is zero terminated.
  * @param confusablesWholeScript
  *                    a pointer to the whole script confusables definitions,
  *                    as found in the file confusablesWholeScript.txt from unicode.org.
  * @param confusablesWholeScriptLen The length of the whole script confusables text, or
  *                    -1 if the input string is zero terminated.
  * @param errType     In the event of an error in the input, indicates
  *                    which of the input files contains the error.
  *                    The value is one of USPOOF_SINGLE_SCRIPT_CONFUSABLE or
  *                    USPOOF_WHOLE_SCRIPT_CONFUSABLE, or
  *                    zero if no errors are found.
  * @param pe          In the event of an error in the input, receives the position
  *                    in the input text (line, offset) of the error.
  * @param status      an in/out ICU UErrorCode.  Among the possible errors is
  *                    U_PARSE_ERROR, which is used to report syntax errors
  *                    in the input.
  * @return            A spoof checker that uses the rules from the input files.
  * @stable ICU 4.2
  */
U_STABLE USpoofChecker * U_EXPORT2
uspoof_openFromSource(const char *confusables,  int32_t confusablesLen,
                      const char *confusablesWholeScript, int32_t confusablesWholeScriptLen,
                      int32_t *errType, UParseError *pe, UErrorCode *status);


/**
  * Close a Spoof Checker, freeing any memory that was being held by
  *   its implementation.
  * @stable ICU 4.2
  */
U_STABLE void U_EXPORT2
uspoof_close(USpoofChecker *sc);

#if U_SHOW_CPLUSPLUS_API

U_NAMESPACE_BEGIN

/**
 * \class LocalUSpoofCheckerPointer
 * "Smart pointer" class, closes a USpoofChecker via uspoof_close().
 * For most methods see the LocalPointerBase base class.
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @stable ICU 4.4
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUSpoofCheckerPointer, USpoofChecker, uspoof_close);

U_NAMESPACE_END

#endif

/**
 * Clone a Spoof Checker.  The clone will be set to perform the same checks
 *   as the original source.
 *
 * @param sc       The source USpoofChecker
 * @param status   The error code, set if this function encounters a problem.
 * @return
 * @stable ICU 4.2
 */
U_STABLE USpoofChecker * U_EXPORT2
uspoof_clone(const USpoofChecker *sc, UErrorCode *status);


/**
 * Specify the set of checks that will be performed by the check
 * functions of this Spoof Checker.
 *
 * @param sc       The USpoofChecker
 * @param checks         The set of checks that this spoof checker will perform.
 *                 The value is a bit set, obtained by OR-ing together
 *                 values from enum USpoofChecks.
 * @param status   The error code, set if this function encounters a problem.
 * @stable ICU 4.2
 *
 */
U_STABLE void U_EXPORT2
uspoof_setChecks(USpoofChecker *sc, int32_t checks, UErrorCode *status);

/**
 * Get the set of checks that this Spoof Checker has been configured to perform.
 * 
 * @param sc       The USpoofChecker
 * @param status   The error code, set if this function encounters a problem.
 * @return         The set of checks that this spoof checker will perform.
 *                 The value is a bit set, obtained by OR-ing together
 *                 values from enum USpoofChecks.
 * @stable ICU 4.2
 *
 */
U_STABLE int32_t U_EXPORT2
uspoof_getChecks(const USpoofChecker *sc, UErrorCode *status);

/**
  * Set the loosest restriction level allowed. The default if this function 
  * is not called is HIGHLY_RESTRICTIVE.
  * Calling this function also enables the RESTRICTION_LEVEL check.
  * @param restrictionLevel The loosest restriction level allowed.
  * @see URestrictionLevel
  * @stable ICU 51
  */
U_STABLE void U_EXPORT2
uspoof_setRestrictionLevel(USpoofChecker *sc, URestrictionLevel restrictionLevel);


/**
  * Get the Restriction Level that will be tested if the checks include RESTRICTION_LEVEL.
  *
  * @return The restriction level
  * @see URestrictionLevel
  * @stable ICU 51
  */
U_STABLE URestrictionLevel U_EXPORT2
uspoof_getRestrictionLevel(const USpoofChecker *sc);

/**
 * Limit characters that are acceptable in identifiers being checked to those 
 * normally used with the languages associated with the specified locales.
 * Any previously specified list of locales is replaced by the new settings.
 *
 * A set of languages is determined from the locale(s), and
 * from those a set of acceptable Unicode scripts is determined.
 * Characters from this set of scripts, along with characters from
 * the "common" and "inherited" Unicode Script categories
 * will be permitted.
 *
 * Supplying an empty string removes all restrictions;
 * characters from any script will be allowed.
 *
 * The USPOOF_CHAR_LIMIT test is automatically enabled for this
 * USpoofChecker when calling this function with a non-empty list
 * of locales.
 *
 * The Unicode Set of characters that will be allowed is accessible
 * via the uspoof_getAllowedChars() function.  uspoof_setAllowedLocales()
 * will <i>replace</i> any previously applied set of allowed characters.
 *
 * Adjustments, such as additions or deletions of certain classes of characters,
 * can be made to the result of uspoof_setAllowedLocales() by
 * fetching the resulting set with uspoof_getAllowedChars(),
 * manipulating it with the Unicode Set API, then resetting the
 * spoof detectors limits with uspoof_setAllowedChars()
 *
 * @param sc           The USpoofChecker 
 * @param localesList  A list list of locales, from which the language
 *                     and associated script are extracted.  The locales
 *                     are comma-separated if there is more than one.
 *                     White space may not appear within an individual locale,
 *                     but is ignored otherwise.
 *                     The locales are syntactically like those from the
 *                     HTTP Accept-Language header.
 *                     If the localesList is empty, no restrictions will be placed on
 *                     the allowed characters.
 *
 * @param status       The error code, set if this function encounters a problem.
 * @stable ICU 4.2
 */
U_STABLE void U_EXPORT2
uspoof_setAllowedLocales(USpoofChecker *sc, const char *localesList, UErrorCode *status);

/**
 * Get a list of locales for the scripts that are acceptable in strings
 *  to be checked.  If no limitations on scripts have been specified,
 *  an empty string will be returned.
 *
 *  uspoof_setAllowedChars() will reset the list of allowed to be empty.
 *
 *  The format of the returned list is the same as that supplied to 
 *  uspoof_setAllowedLocales(), but returned list may not be identical 
 *  to the originally specified string; the string may be reformatted, 
 *  and information other than languages from
 *  the originally specified locales may be omitted.
 *
 * @param sc           The USpoofChecker 
 * @param status       The error code, set if this function encounters a problem.
 * @return             A string containing a list of  locales corresponding
 *                     to the acceptable scripts, formatted like an
 *                     HTTP Accept Language value.
 *  
 * @stable ICU 4.2
 */
U_STABLE const char * U_EXPORT2
uspoof_getAllowedLocales(USpoofChecker *sc, UErrorCode *status);


/**
 * Limit the acceptable characters to those specified by a Unicode Set.
 *   Any previously specified character limit is
 *   is replaced by the new settings.  This includes limits on
 *   characters that were set with the uspoof_setAllowedLocales() function.
 *
 * The USPOOF_CHAR_LIMIT test is automatically enabled for this
 * USpoofChecker by this function.
 *
 * @param sc       The USpoofChecker 
 * @param chars    A Unicode Set containing the list of
 *                 characters that are permitted.  Ownership of the set
 *                 remains with the caller.  The incoming set is cloned by
 *                 this function, so there are no restrictions on modifying
 *                 or deleting the USet after calling this function.
 * @param status   The error code, set if this function encounters a problem.
 * @stable ICU 4.2
 */
U_STABLE void U_EXPORT2
uspoof_setAllowedChars(USpoofChecker *sc, const USet *chars, UErrorCode *status);


/**
 * Get a USet for the characters permitted in an identifier.
 * This corresponds to the limits imposed by the Set Allowed Characters
 * functions. Limitations imposed by other checks will not be
 * reflected in the set returned by this function.
 *
 * The returned set will be frozen, meaning that it cannot be modified
 * by the caller.
 *
 * Ownership of the returned set remains with the Spoof Detector.  The
 * returned set will become invalid if the spoof detector is closed,
 * or if a new set of allowed characters is specified.
 *
 *
 * @param sc       The USpoofChecker 
 * @param status   The error code, set if this function encounters a problem.
 * @return         A USet containing the characters that are permitted by
 *                 the USPOOF_CHAR_LIMIT test.
 * @stable ICU 4.2
 */
U_STABLE const USet * U_EXPORT2
uspoof_getAllowedChars(const USpoofChecker *sc, UErrorCode *status);


#if U_SHOW_CPLUSPLUS_API
/**
 * Limit the acceptable characters to those specified by a Unicode Set.
 *   Any previously specified character limit is
 *   is replaced by the new settings.    This includes limits on
 *   characters that were set with the uspoof_setAllowedLocales() function.
 *
 * The USPOOF_CHAR_LIMIT test is automatically enabled for this
 * USoofChecker by this function.
 *
 * @param sc       The USpoofChecker 
 * @param chars    A Unicode Set containing the list of
 *                 characters that are permitted.  Ownership of the set
 *                 remains with the caller.  The incoming set is cloned by
 *                 this function, so there are no restrictions on modifying
 *                 or deleting the UnicodeSet after calling this function.
 * @param status   The error code, set if this function encounters a problem.
 * @stable ICU 4.2
 */
U_STABLE void U_EXPORT2
uspoof_setAllowedUnicodeSet(USpoofChecker *sc, const icu::UnicodeSet *chars, UErrorCode *status);


/**
 * Get a UnicodeSet for the characters permitted in an identifier.
 * This corresponds to the limits imposed by the Set Allowed Characters / 
 * UnicodeSet functions. Limitations imposed by other checks will not be
 * reflected in the set returned by this function.
 *
 * The returned set will be frozen, meaning that it cannot be modified
 * by the caller.
 *
 * Ownership of the returned set remains with the Spoof Detector.  The
 * returned set will become invalid if the spoof detector is closed,
 * or if a new set of allowed characters is specified.
 *
 *
 * @param sc       The USpoofChecker 
 * @param status   The error code, set if this function encounters a problem.
 * @return         A UnicodeSet containing the characters that are permitted by
 *                 the USPOOF_CHAR_LIMIT test.
 * @stable ICU 4.2
 */
U_STABLE const icu::UnicodeSet * U_EXPORT2
uspoof_getAllowedUnicodeSet(const USpoofChecker *sc, UErrorCode *status);
#endif


/**
 * Check the specified string for possible security issues.
 * The text to be checked will typically be an identifier of some sort.
 * The set of checks to be performed is specified with uspoof_setChecks().
 * 
 * @param sc      The USpoofChecker 
 * @param id      The identifier to be checked for possible security issues,
 *                in UTF-16 format.
 * @param length  the length of the string to be checked, expressed in
 *                16 bit UTF-16 code units, or -1 if the string is 
 *                zero terminated.
 * @param position      An out parameter.
 *                Originally, the index of the first string position that failed a check.
 *                Now, always returns zero.
 *                This parameter may be null.
 * @param status  The error code, set if an error occurred while attempting to
 *                perform the check.
 *                Spoofing or security issues detected with the input string are
 *                not reported here, but through the function's return value.
 * @return        An integer value with bits set for any potential security
 *                or spoofing issues detected.  The bits are defined by
 *                enum USpoofChecks.  (returned_value & USPOOF_ALL_CHECKS)
 *                will be zero if the input string passes all of the
 *                enabled checks.
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_check(const USpoofChecker *sc,
                         const UChar *id, int32_t length, 
                         int32_t *position,
                         UErrorCode *status);


/**
 * Check the specified string for possible security issues.
 * The text to be checked will typically be an identifier of some sort.
 * The set of checks to be performed is specified with uspoof_setChecks().
 * 
 * @param sc      The USpoofChecker 
 * @param id      A identifier to be checked for possible security issues, in UTF8 format.
 * @param length  the length of the string to be checked, or -1 if the string is 
 *                zero terminated.
 * @param position      An out parameter.
 *                Originally, the index of the first string position that failed a check.
 *                Now, always returns zero.
 *                This parameter may be null.
 *                @deprecated ICU 51
 * @param status  The error code, set if an error occurred while attempting to
 *                perform the check.
 *                Spoofing or security issues detected with the input string are
 *                not reported here, but through the function's return value.
 *                If the input contains invalid UTF-8 sequences,
 *                a status of U_INVALID_CHAR_FOUND will be returned.
 * @return        An integer value with bits set for any potential security
 *                or spoofing issues detected.  The bits are defined by
 *                enum USpoofChecks.  (returned_value & USPOOF_ALL_CHECKS)
 *                will be zero if the input string passes all of the
 *                enabled checks.
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_checkUTF8(const USpoofChecker *sc,
                 const char *id, int32_t length,
                 int32_t *position,
                 UErrorCode *status);


#if U_SHOW_CPLUSPLUS_API
/**
 * Check the specified string for possible security issues.
 * The text to be checked will typically be an identifier of some sort.
 * The set of checks to be performed is specified with uspoof_setChecks().
 * 
 * @param sc      The USpoofChecker 
 * @param id      A identifier to be checked for possible security issues.
 * @param position      An out parameter.
 *                Originally, the index of the first string position that failed a check.
 *                Now, always returns zero.
 *                This parameter may be null.
 *                @deprecated ICU 51
 * @param status  The error code, set if an error occurred while attempting to
 *                perform the check.
 *                Spoofing or security issues detected with the input string are
 *                not reported here, but through the function's return value.
 * @return        An integer value with bits set for any potential security
 *                or spoofing issues detected.  The bits are defined by
 *                enum USpoofChecks.  (returned_value & USPOOF_ALL_CHECKS)
 *                will be zero if the input string passes all of the
 *                enabled checks.
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_checkUnicodeString(const USpoofChecker *sc,
                          const icu::UnicodeString &id, 
                          int32_t *position,
                          UErrorCode *status);

#endif


/**
 * Check the whether two specified strings are visually confusable.
 * The types of confusability to be tested - single script, mixed script,
 * or whole script - are determined by the check options set for the
 * USpoofChecker.
 *
 * The tests to be performed are controlled by the flags
 *   USPOOF_SINGLE_SCRIPT_CONFUSABLE 
 *   USPOOF_MIXED_SCRIPT_CONFUSABLE  
 *   USPOOF_WHOLE_SCRIPT_CONFUSABLE
 * At least one of these tests must be selected.
 * 
 * USPOOF_ANY_CASE is a modifier for the tests.  Select it if the identifiers
 *   may be of mixed case.
 * If identifiers are case folded for comparison and
 * display to the user, do not select the USPOOF_ANY_CASE option.
 *
 *
 * @param sc      The USpoofChecker
 * @param id1     The first of the two identifiers to be compared for 
 *                confusability.  The strings are in UTF-16 format.
 * @param length1 the length of the first identifer, expressed in
 *                16 bit UTF-16 code units, or -1 if the string is 
 *                nul terminated.
 * @param id2     The second of the two identifiers to be compared for 
 *                confusability.  The identifiers are in UTF-16 format.
 * @param length2 The length of the second identifiers, expressed in
 *                16 bit UTF-16 code units, or -1 if the string is 
 *                nul terminated.
 * @param status  The error code, set if an error occurred while attempting to
 *                perform the check.
 *                Confusability of the identifiers is not reported here,
 *                but through this function's return value.
 * @return        An integer value with bit(s) set corresponding to
 *                the type of confusability found, as defined by
 *                enum USpoofChecks.  Zero is returned if the identifiers
 *                are not confusable.
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_areConfusable(const USpoofChecker *sc,
                     const UChar *id1, int32_t length1,
                     const UChar *id2, int32_t length2,
                     UErrorCode *status);



/**
 * Check the whether two specified strings are visually confusable.
 * The types of confusability to be tested - single script, mixed script,
 * or whole script - are determined by the check options set for the
 * USpoofChecker.
 *
 * @param sc      The USpoofChecker
 * @param id1     The first of the two identifiers to be compared for 
 *                confusability.  The strings are in UTF-8 format.
 * @param length1 the length of the first identifiers, in bytes, or -1 
 *                if the string is nul terminated.
 * @param id2     The second of the two identifiers to be compared for 
 *                confusability.  The strings are in UTF-8 format.
 * @param length2 The length of the second string in bytes, or -1 
 *                if the string is nul terminated.
 * @param status  The error code, set if an error occurred while attempting to
 *                perform the check.
 *                Confusability of the strings is not reported here,
 *                but through this function's return value.
 * @return        An integer value with bit(s) set corresponding to
 *                the type of confusability found, as defined by
 *                enum USpoofChecks.  Zero is returned if the strings
 *                are not confusable.
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_areConfusableUTF8(const USpoofChecker *sc,
                         const char *id1, int32_t length1,
                         const char *id2, int32_t length2,
                         UErrorCode *status);




#if U_SHOW_CPLUSPLUS_API
/**
 * Check the whether two specified strings are visually confusable.
 * The types of confusability to be tested - single script, mixed script,
 * or whole script - are determined by the check options set for the
 * USpoofChecker.
 *
 * @param sc      The USpoofChecker
 * @param s1     The first of the two identifiers to be compared for 
 *                confusability.  The strings are in UTF-8 format.
 * @param s2     The second of the two identifiers to be compared for 
 *                confusability.  The strings are in UTF-8 format.
 * @param status  The error code, set if an error occurred while attempting to
 *                perform the check.
 *                Confusability of the identifiers is not reported here,
 *                but through this function's return value.
 * @return        An integer value with bit(s) set corresponding to
 *                the type of confusability found, as defined by
 *                enum USpoofChecks.  Zero is returned if the identifiers
 *                are not confusable.
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_areConfusableUnicodeString(const USpoofChecker *sc,
                                  const icu::UnicodeString &s1,
                                  const icu::UnicodeString &s2,
                                  UErrorCode *status);
#endif


/**
  *  Get the "skeleton" for an identifier.
  *  Skeletons are a transformation of the input identifier;
  *  Two identifiers are confusable if their skeletons are identical.
  *  See Unicode UAX #39 for additional information.
  *
  *  Using skeletons directly makes it possible to quickly check
  *  whether an identifier is confusable with any of some large
  *  set of existing identifiers, by creating an efficiently
  *  searchable collection of the skeletons.
  *
  * @param sc      The USpoofChecker
  * @param type    The type of skeleton, corresponding to which
  *                of the Unicode confusable data tables to use.
  *                The default is Mixed-Script, Lowercase.
  *                Allowed options are USPOOF_SINGLE_SCRIPT_CONFUSABLE and
  *                USPOOF_ANY_CASE.  The two flags may be ORed.
  * @param id      The input identifier whose skeleton will be computed.
  * @param length  The length of the input identifier, expressed in 16 bit
  *                UTF-16 code units, or -1 if the string is zero terminated.
  * @param dest    The output buffer, to receive the skeleton string.
  * @param destCapacity  The length of the output buffer, in 16 bit units.
  *                The destCapacity may be zero, in which case the function will
  *                return the actual length of the skeleton.
  * @param status  The error code, set if an error occurred while attempting to
  *                perform the check.
  * @return        The length of the skeleton string.  The returned length
  *                is always that of the complete skeleton, even when the
  *                supplied buffer is too small (or of zero length)
  *                
  * @stable ICU 4.2
  */
U_STABLE int32_t U_EXPORT2
uspoof_getSkeleton(const USpoofChecker *sc,
                   uint32_t type,
                   const UChar *id,  int32_t length,
                   UChar *dest, int32_t destCapacity,
                   UErrorCode *status);
    
/**
  *  Get the "skeleton" for an identifier.
  *  Skeletons are a transformation of the input identifier;
  *  Two identifiers are confusable if their skeletons are identical.
  *  See Unicode UAX #39 for additional information.
  *
  *  Using skeletons directly makes it possible to quickly check
  *  whether an identifier is confusable with any of some large
  *  set of existing identifiers, by creating an efficiently
  *  searchable collection of the skeletons.
  *
  * @param sc      The USpoofChecker 
  * @param type    The type of skeleton, corresponding to which
  *                of the Unicode confusable data tables to use.
  *                The default is Mixed-Script, Lowercase.
  *                Allowed options are USPOOF_SINGLE_SCRIPT_CONFUSABLE and
  *                USPOOF_ANY_CASE.  The two flags may be ORed.
  * @param id      The UTF-8 format identifier whose skeleton will be computed.
  * @param length  The length of the input string, in bytes,
  *                or -1 if the string is zero terminated.
  * @param dest    The output buffer, to receive the skeleton string.
  * @param destCapacity  The length of the output buffer, in bytes.
  *                The destCapacity may be zero, in which case the function will
  *                return the actual length of the skeleton.
  * @param status  The error code, set if an error occurred while attempting to
  *                perform the check.  Possible Errors include U_INVALID_CHAR_FOUND
  *                   for invalid UTF-8 sequences, and
  *                   U_BUFFER_OVERFLOW_ERROR if the destination buffer is too small
  *                   to hold the complete skeleton.
  * @return        The length of the skeleton string, in bytes.  The returned length
  *                is always that of the complete skeleton, even when the
  *                supplied buffer is too small (or of zero length)
  *                
  * @stable ICU 4.2
  */   
U_STABLE int32_t U_EXPORT2
uspoof_getSkeletonUTF8(const USpoofChecker *sc,
                       uint32_t type,
                       const char *id,  int32_t length,
                       char *dest, int32_t destCapacity,
                       UErrorCode *status);
    
#if U_SHOW_CPLUSPLUS_API
/**
  *  Get the "skeleton" for an identifier.
  *  Skeletons are a transformation of the input identifier;
  *  Two identifiers are confusable if their skeletons are identical.
  *  See Unicode UAX #39 for additional information.
  *
  *  Using skeletons directly makes it possible to quickly check
  *  whether an identifier is confusable with any of some large
  *  set of existing identifiers, by creating an efficiently
  *  searchable collection of the skeletons.
  *
  * @param sc      The USpoofChecker.
  * @param type    The type of skeleton, corresponding to which
  *                of the Unicode confusable data tables to use.
  *                The default is Mixed-Script, Lowercase.
  *                Allowed options are USPOOF_SINGLE_SCRIPT_CONFUSABLE and
  *                USPOOF_ANY_CASE.  The two flags may be ORed.
  * @param id      The input identifier whose skeleton will be computed.
  * @param dest    The output identifier, to receive the skeleton string.
  * @param status  The error code, set if an error occurred while attempting to
  *                perform the check.
  * @return        A reference to the destination (skeleton) string.
  *                
  * @stable ICU 4.2
  */   
U_I18N_API icu::UnicodeString & U_EXPORT2
uspoof_getSkeletonUnicodeString(const USpoofChecker *sc,
                                uint32_t type,
                                const icu::UnicodeString &id,
                                icu::UnicodeString &dest,
                                UErrorCode *status);
#endif   /* U_SHOW_CPLUSPLUS_API */

/**
  * Get the set of Candidate Characters for Inclusion in Identifiers, as defined
  * in Unicode UAX #31, http://www.unicode.org/reports/tr31/#Table_Candidate_Characters_for_Inclusion_in_Identifiers
  *
  * The returned set is frozen. Ownership of the set remains with the ICU library; it must not
  * be deleted by the caller.
  *
  * @param status The error code, set if a problem occurs while creating the set.
  *
  * @stable ICU 51
  */
U_STABLE const USet * U_EXPORT2
uspoof_getInclusionSet(UErrorCode *status);

/**
  * Get the set of characters from Recommended Scripts for Inclusion in Identifiers, as defined
  * in Unicode UAX #31, http://www.unicode.org/reports/tr31/#Table_Recommended_Scripts
  *
  * The returned set is frozen. Ownership of the set remains with the ICU library; it must not
  * be deleted by the caller.
  *
  * @param status The error code, set if a problem occurs while creating the set.
  *
  * @stable ICU 51
  */
U_STABLE const USet * U_EXPORT2
uspoof_getRecommendedSet(UErrorCode *status);

#if U_SHOW_CPLUSPLUS_API

/**
  * Get the set of Candidate Characters for Inclusion in Identifiers, as defined
  * in Unicode UAX #31, http://www.unicode.org/reports/tr31/#Table_Candidate_Characters_for_Inclusion_in_Identifiers
  *
  * The returned set is frozen. Ownership of the set remains with the ICU library; it must not
  * be deleted by the caller.
  *
  * @param status The error code, set if a problem occurs while creating the set.
  *
  * @stable ICU 51
  */
U_STABLE const icu::UnicodeSet * U_EXPORT2
uspoof_getInclusionUnicodeSet(UErrorCode *status);

/**
  * Get the set of characters from Recommended Scripts for Inclusion in Identifiers, as defined
  * in Unicode UAX #31, http://www.unicode.org/reports/tr31/#Table_Recommended_Scripts
  *
  * The returned set is frozen. Ownership of the set remains with the ICU library; it must not
  * be deleted by the caller.
  *
  * @param status The error code, set if a problem occurs while creating the set.
  *
  * @stable ICU 51
  */
U_STABLE const icu::UnicodeSet * U_EXPORT2
uspoof_getRecommendedUnicodeSet(UErrorCode *status);

#endif /* U_SHOW_CPLUSPLUS_API */

/**
 * Serialize the data for a spoof detector into a chunk of memory.
 * The flattened spoof detection tables can later be used to efficiently
 * instantiate a new Spoof Detector.
 *
 * The serialized spoof checker includes only the data compiled from the
 * Unicode data tables by uspoof_openFromSource(); it does not include 
 * include any other state or configuration that may have been set.
 *
 * @param sc   the Spoof Detector whose data is to be serialized.
 * @param data a pointer to 32-bit-aligned memory to be filled with the data,
 *             can be NULL if capacity==0
 * @param capacity the number of bytes available at data,
 *                 or 0 for preflighting
 * @param status an in/out ICU UErrorCode; possible errors include:
 * - U_BUFFER_OVERFLOW_ERROR if the data storage block is too small for serialization
 * - U_ILLEGAL_ARGUMENT_ERROR  the data or capacity parameters are bad
 * @return the number of bytes written or needed for the spoof data
 *
 * @see utrie2_openFromSerialized()
 * @stable ICU 4.2
 */
U_STABLE int32_t U_EXPORT2
uspoof_serialize(USpoofChecker *sc,
                 void *data, int32_t capacity,
                 UErrorCode *status);


#endif

#endif   /* USPOOF_H */

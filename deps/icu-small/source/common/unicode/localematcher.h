// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

// localematcher.h
// created: 2019may08 Markus W. Scherer

#ifndef __LOCALEMATCHER_H__
#define __LOCALEMATCHER_H__

#include "unicode/utypes.h"

#if U_SHOW_CPLUSPLUS_API

#include "unicode/locid.h"
#include "unicode/stringpiece.h"
#include "unicode/uobject.h"

/**
 * \file
 * \brief C++ API: Locale matcher: User's desired locales vs. application's supported locales.
 */

#ifndef U_HIDE_DRAFT_API

/**
 * Builder option for whether the language subtag or the script subtag is most important.
 *
 * @see Builder#setFavorSubtag(FavorSubtag)
 * @draft ICU 65
 */
enum ULocMatchFavorSubtag {
    /**
     * Language differences are most important, then script differences, then region differences.
     * (This is the default behavior.)
     *
     * @draft ICU 65
     */
    ULOCMATCH_FAVOR_LANGUAGE,
    /**
     * Makes script differences matter relatively more than language differences.
     *
     * @draft ICU 65
     */
    ULOCMATCH_FAVOR_SCRIPT
};
#ifndef U_IN_DOXYGEN
typedef enum ULocMatchFavorSubtag ULocMatchFavorSubtag;
#endif

/**
 * Builder option for whether all desired locales are treated equally or
 * earlier ones are preferred.
 *
 * @see Builder#setDemotionPerDesiredLocale(Demotion)
 * @draft ICU 65
 */
enum ULocMatchDemotion {
    /**
     * All desired locales are treated equally.
     *
     * @draft ICU 65
     */
    ULOCMATCH_DEMOTION_NONE,
    /**
     * Earlier desired locales are preferred.
     *
     * <p>From each desired locale to the next,
     * the distance to any supported locale is increased by an additional amount
     * which is at least as large as most region mismatches.
     * A later desired locale has to have a better match with some supported locale
     * due to more than merely having the same region subtag.
     *
     * <p>For example: <code>Supported={en, sv}  desired=[en-GB, sv]</code>
     * yields <code>Result(en-GB, en)</code> because
     * with the demotion of sv its perfect match is no better than
     * the region distance between the earlier desired locale en-GB and en=en-US.
     *
     * <p>Notes:
     * <ul>
     *   <li>In some cases, language and/or script differences can be as small as
     *       the typical region difference. (Example: sr-Latn vs. sr-Cyrl)
     *   <li>It is possible for certain region differences to be larger than usual,
     *       and larger than the demotion.
     *       (As of CLDR 35 there is no such case, but
     *        this is possible in future versions of the data.)
     * </ul>
     *
     * @draft ICU 65
     */
    ULOCMATCH_DEMOTION_REGION
};
#ifndef U_IN_DOXYGEN
typedef enum ULocMatchDemotion ULocMatchDemotion;
#endif

struct UHashtable;

U_NAMESPACE_BEGIN

struct LSR;

class LocaleDistance;
class LocaleLsrIterator;
class UVector;
class XLikelySubtags;

/**
 * Immutable class that picks the best match between a user's desired locales and
 * an application's supported locales.
 * Movable but not copyable.
 *
 * <p>Example:
 * <pre>
 * UErrorCode errorCode = U_ZERO_ERROR;
 * LocaleMatcher matcher = LocaleMatcher::Builder().setSupportedLocales("fr, en-GB, en").build(errorCode);
 * Locale *bestSupported = matcher.getBestLocale(Locale.US, errorCode);  // "en"
 * </pre>
 *
 * <p>A matcher takes into account when languages are close to one another,
 * such as Danish and Norwegian,
 * and when regional variants are close, like en-GB and en-AU as opposed to en-US.
 *
 * <p>If there are multiple supported locales with the same (language, script, region)
 * likely subtags, then the current implementation returns the first of those locales.
 * It ignores variant subtags (except for pseudolocale variants) and extensions.
 * This may change in future versions.
 *
 * <p>For example, the current implementation does not distinguish between
 * de, de-DE, de-Latn, de-1901, de-u-co-phonebk.
 *
 * <p>If you prefer one equivalent locale over another, then provide only the preferred one,
 * or place it earlier in the list of supported locales.
 *
 * <p>Otherwise, the order of supported locales may have no effect on the best-match results.
 * The current implementation compares each desired locale with supported locales
 * in the following order:
 * 1. Default locale, if supported;
 * 2. CLDR "paradigm locales" like en-GB and es-419;
 * 3. other supported locales.
 * This may change in future versions.
 *
 * <p>Often a product will just need one matcher instance, built with the languages
 * that it supports. However, it may want multiple instances with different
 * default languages based on additional information, such as the domain.
 *
 * <p>This class is not intended for public subclassing.
 *
 * @draft ICU 65
 */
class U_COMMON_API LocaleMatcher : public UMemory {
public:
    /**
     * Data for the best-matching pair of a desired and a supported locale.
     * Movable but not copyable.
     *
     * @draft ICU 65
     */
    class U_COMMON_API Result : public UMemory {
    public:
        /**
         * Move constructor; might modify the source.
         * This object will have the same contents that the source object had.
         *
         * @param src Result to move contents from.
         * @draft ICU 65
         */
        Result(Result &&src) U_NOEXCEPT;

        /**
         * Destructor.
         *
         * @draft ICU 65
         */
        ~Result();

        /**
         * Move assignment; might modify the source.
         * This object will have the same contents that the source object had.
         *
         * @param src Result to move contents from.
         * @draft ICU 65
         */
        Result &operator=(Result &&src) U_NOEXCEPT;

        /**
         * Returns the best-matching desired locale.
         * nullptr if the list of desired locales is empty or if none matched well enough.
         *
         * @return the best-matching desired locale, or nullptr.
         * @draft ICU 65
         */
        inline const Locale *getDesiredLocale() const { return desiredLocale; }

        /**
         * Returns the best-matching supported locale.
         * If none matched well enough, this is the default locale.
         * The default locale is nullptr if the list of supported locales is empty and
         * no explicit default locale is set.
         *
         * @return the best-matching supported locale, or nullptr.
         * @draft ICU 65
         */
        inline const Locale *getSupportedLocale() const { return supportedLocale; }

        /**
         * Returns the index of the best-matching desired locale in the input Iterable order.
         * -1 if the list of desired locales is empty or if none matched well enough.
         *
         * @return the index of the best-matching desired locale, or -1.
         * @draft ICU 65
         */
        inline int32_t getDesiredIndex() const { return desiredIndex; }

        /**
         * Returns the index of the best-matching supported locale in the
         * constructor’s or builder’s input order (“set” Collection plus “added” locales).
         * If the matcher was built from a locale list string, then the iteration order is that
         * of a LocalePriorityList built from the same string.
         * -1 if the list of supported locales is empty or if none matched well enough.
         *
         * @return the index of the best-matching supported locale, or -1.
         * @draft ICU 65
         */
        inline int32_t getSupportedIndex() const { return supportedIndex; }

        /**
         * Takes the best-matching supported locale and adds relevant fields of the
         * best-matching desired locale, such as the -t- and -u- extensions.
         * May replace some fields of the supported locale.
         * The result is the locale that should be used for date and number formatting, collation, etc.
         * Returns the root locale if getSupportedLocale() returns nullptr.
         *
         * <p>Example: desired=ar-SA-u-nu-latn, supported=ar-EG, resolved locale=ar-SA-u-nu-latn
         *
         * @return a locale combining the best-matching desired and supported locales.
         * @draft ICU 65
         */
        Locale makeResolvedLocale(UErrorCode &errorCode) const;

    private:
        Result(const Locale *desired, const Locale *supported,
               int32_t desIndex, int32_t suppIndex, UBool owned) :
                desiredLocale(desired), supportedLocale(supported),
                desiredIndex(desIndex), supportedIndex(suppIndex),
                desiredIsOwned(owned) {}

        Result(const Result &other) = delete;
        Result &operator=(const Result &other) = delete;

        const Locale *desiredLocale;
        const Locale *supportedLocale;
        int32_t desiredIndex;
        int32_t supportedIndex;
        UBool desiredIsOwned;

        friend class LocaleMatcher;
    };

    /**
     * LocaleMatcher builder.
     * Movable but not copyable.
     *
     * @see LocaleMatcher#builder()
     * @draft ICU 65
     */
    class U_COMMON_API Builder : public UMemory {
    public:
        /**
         * Constructs a builder used in chaining parameters for building a LocaleMatcher.
         *
         * @return a new Builder object
         * @draft ICU 65
         */
        Builder() {}

        /**
         * Move constructor; might modify the source.
         * This builder will have the same contents that the source builder had.
         *
         * @param src Builder to move contents from.
         * @draft ICU 65
         */
        Builder(Builder &&src) U_NOEXCEPT;

        /**
         * Destructor.
         *
         * @draft ICU 65
         */
        ~Builder();

        /**
         * Move assignment; might modify the source.
         * This builder will have the same contents that the source builder had.
         *
         * @param src Builder to move contents from.
         * @draft ICU 65
         */
        Builder &operator=(Builder &&src) U_NOEXCEPT;

        /**
         * Parses an Accept-Language string
         * (<a href="https://tools.ietf.org/html/rfc2616#section-14.4">RFC 2616 Section 14.4</a>),
         * such as "af, en, fr;q=0.9", and sets the supported locales accordingly.
         * Allows whitespace in more places but does not allow "*".
         * Clears any previously set/added supported locales first.
         *
         * @param locales the Accept-Language string of locales to set
         * @return this Builder object
         * @draft ICU 65
         */
        Builder &setSupportedLocalesFromListString(StringPiece locales);

        /**
         * Copies the supported locales, preserving iteration order.
         * Clears any previously set/added supported locales first.
         * Duplicates are allowed, and are not removed.
         *
         * @param locales the list of locale
         * @return this Builder object
         * @draft ICU 65
         */
        Builder &setSupportedLocales(Locale::Iterator &locales);

        /**
         * Copies the supported locales from the begin/end range, preserving iteration order.
         * Clears any previously set/added supported locales first.
         * Duplicates are allowed, and are not removed.
         *
         * Each of the iterator parameter values must be an
         * input iterator whose value is convertible to const Locale &.
         *
         * @param begin Start of range.
         * @param end Exclusive end of range.
         * @return this Builder object
         * @draft ICU 65
         */
        template<typename Iter>
        Builder &setSupportedLocales(Iter begin, Iter end) {
            if (U_FAILURE(errorCode_)) { return *this; }
            clearSupportedLocales();
            while (begin != end) {
                addSupportedLocale(*begin++);
            }
            return *this;
        }

        /**
         * Copies the supported locales from the begin/end range, preserving iteration order.
         * Calls the converter to convert each *begin to a Locale or const Locale &.
         * Clears any previously set/added supported locales first.
         * Duplicates are allowed, and are not removed.
         *
         * Each of the iterator parameter values must be an
         * input iterator whose value is convertible to const Locale &.
         *
         * @param begin Start of range.
         * @param end Exclusive end of range.
         * @param converter Converter from *begin to const Locale & or compatible.
         * @return this Builder object
         * @draft ICU 65
         */
        template<typename Iter, typename Conv>
        Builder &setSupportedLocalesViaConverter(Iter begin, Iter end, Conv converter) {
            if (U_FAILURE(errorCode_)) { return *this; }
            clearSupportedLocales();
            while (begin != end) {
                addSupportedLocale(converter(*begin++));
            }
            return *this;
        }

        /**
         * Adds another supported locale.
         * Duplicates are allowed, and are not removed.
         *
         * @param locale another locale
         * @return this Builder object
         * @draft ICU 65
         */
        Builder &addSupportedLocale(const Locale &locale);

        /**
         * Sets the default locale; if nullptr, or if it is not set explicitly,
         * then the first supported locale is used as the default locale.
         *
         * @param defaultLocale the default locale (will be copied)
         * @return this Builder object
         * @draft ICU 65
         */
        Builder &setDefaultLocale(const Locale *defaultLocale);

        /**
         * If ULOCMATCH_FAVOR_SCRIPT, then the language differences are smaller than script
         * differences.
         * This is used in situations (such as maps) where
         * it is better to fall back to the same script than a similar language.
         *
         * @param subtag the subtag to favor
         * @return this Builder object
         * @draft ICU 65
         */
        Builder &setFavorSubtag(ULocMatchFavorSubtag subtag);

        /**
         * Option for whether all desired locales are treated equally or
         * earlier ones are preferred (this is the default).
         *
         * @param demotion the demotion per desired locale to set.
         * @return this Builder object
         * @draft ICU 65
         */
        Builder &setDemotionPerDesiredLocale(ULocMatchDemotion demotion);

        /**
         * Sets the UErrorCode if an error occurred while setting parameters.
         * Preserves older error codes in the outErrorCode.
         *
         * @param outErrorCode Set to an error code if it does not contain one already
         *                  and an error occurred while setting parameters.
         *                  Otherwise unchanged.
         * @return TRUE if U_FAILURE(outErrorCode)
         * @draft ICU 65
         */
        UBool copyErrorTo(UErrorCode &outErrorCode) const;

        /**
         * Builds and returns a new locale matcher.
         * This builder can continue to be used.
         *
         * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
         *                  or else the function returns immediately. Check for U_FAILURE()
         *                  on output or use with function chaining. (See User Guide for details.)
         * @return new LocaleMatcher.
         * @draft ICU 65
         */
        LocaleMatcher build(UErrorCode &errorCode) const;

    private:
        friend class LocaleMatcher;

        Builder(const Builder &other) = delete;
        Builder &operator=(const Builder &other) = delete;

        void clearSupportedLocales();
        bool ensureSupportedLocaleVector();

        UErrorCode errorCode_ = U_ZERO_ERROR;
        UVector *supportedLocales_ = nullptr;
        int32_t thresholdDistance_ = -1;
        ULocMatchDemotion demotion_ = ULOCMATCH_DEMOTION_REGION;
        Locale *defaultLocale_ = nullptr;
        ULocMatchFavorSubtag favor_ = ULOCMATCH_FAVOR_LANGUAGE;
    };

    // FYI No public LocaleMatcher constructors in C++; use the Builder.

    /**
     * Move copy constructor; might modify the source.
     * This matcher will have the same settings that the source matcher had.
     * @param src source matcher
     * @draft ICU 65
     */
    LocaleMatcher(LocaleMatcher &&src) U_NOEXCEPT;

    /**
     * Destructor.
     * @draft ICU 65
     */
    ~LocaleMatcher();

    /**
     * Move assignment operator; might modify the source.
     * This matcher will have the same settings that the source matcher had.
     * The behavior is undefined if *this and src are the same object.
     * @param src source matcher
     * @return *this
     * @draft ICU 65
     */
    LocaleMatcher &operator=(LocaleMatcher &&src) U_NOEXCEPT;

    /**
     * Returns the supported locale which best matches the desired locale.
     *
     * @param desiredLocale Typically a user's language.
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return the best-matching supported locale.
     * @draft ICU 65
     */
    const Locale *getBestMatch(const Locale &desiredLocale, UErrorCode &errorCode) const;

    /**
     * Returns the supported locale which best matches one of the desired locales.
     *
     * @param desiredLocales Typically a user's languages, in order of preference (descending).
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return the best-matching supported locale.
     * @draft ICU 65
     */
    const Locale *getBestMatch(Locale::Iterator &desiredLocales, UErrorCode &errorCode) const;

    /**
     * Parses an Accept-Language string
     * (<a href="https://tools.ietf.org/html/rfc2616#section-14.4">RFC 2616 Section 14.4</a>),
     * such as "af, en, fr;q=0.9",
     * and returns the supported locale which best matches one of the desired locales.
     * Allows whitespace in more places but does not allow "*".
     *
     * @param desiredLocaleList Typically a user's languages, as an Accept-Language string.
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return the best-matching supported locale.
     * @draft ICU 65
     */
    const Locale *getBestMatchForListString(StringPiece desiredLocaleList, UErrorCode &errorCode) const;

    /**
     * Returns the best match between the desired locale and the supported locales.
     * If the result's desired locale is not nullptr, then it is the address of the input locale.
     * It has not been cloned.
     *
     * @param desiredLocale Typically a user's language.
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return the best-matching pair of the desired and a supported locale.
     * @draft ICU 65
     */
    Result getBestMatchResult(const Locale &desiredLocale, UErrorCode &errorCode) const;

    /**
     * Returns the best match between the desired and supported locales.
     * If the result's desired locale is not nullptr, then it is a clone of
     * the best-matching desired locale. The Result object owns the clone.
     *
     * @param desiredLocales Typically a user's languages, in order of preference (descending).
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return the best-matching pair of a desired and a supported locale.
     * @draft ICU 65
     */
    Result getBestMatchResult(Locale::Iterator &desiredLocales, UErrorCode &errorCode) const;

#ifndef U_HIDE_INTERNAL_API
    /**
     * Returns a fraction between 0 and 1, where 1 means that the languages are a
     * perfect match, and 0 means that they are completely different.
     *
     * <p>This is mostly an implementation detail, and the precise values may change over time.
     * The implementation may use either the maximized forms or the others ones, or both.
     * The implementation may or may not rely on the forms to be consistent with each other.
     *
     * <p>Callers should construct and use a matcher rather than match pairs of locales directly.
     *
     * @param desired Desired locale.
     * @param supported Supported locale.
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return value between 0 and 1, inclusive.
     * @internal (has a known user)
     */
    double internalMatch(const Locale &desired, const Locale &supported, UErrorCode &errorCode) const;
#endif  // U_HIDE_INTERNAL_API

private:
    LocaleMatcher(const Builder &builder, UErrorCode &errorCode);
    LocaleMatcher(const LocaleMatcher &other) = delete;
    LocaleMatcher &operator=(const LocaleMatcher &other) = delete;

    int32_t getBestSuppIndex(LSR desiredLSR, LocaleLsrIterator *remainingIter, UErrorCode &errorCode) const;

    const XLikelySubtags &likelySubtags;
    const LocaleDistance &localeDistance;
    int32_t thresholdDistance;
    int32_t demotionPerDesiredLocale;
    ULocMatchFavorSubtag favorSubtag;

    // These are in input order.
    const Locale ** supportedLocales;
    LSR *lsrs;
    int32_t supportedLocalesLength;
    // These are in preference order: 1. Default locale 2. paradigm locales 3. others.
    UHashtable *supportedLsrToIndex;  // Map<LSR, Integer> stores index+1 because 0 is "not found"
    // Array versions of the supportedLsrToIndex keys and values.
    // The distance lookup loops over the supportedLSRs and returns the index of the best match.
    const LSR **supportedLSRs;
    int32_t *supportedIndexes;
    int32_t supportedLSRsLength;
    Locale *ownedDefaultLocale;
    const Locale *defaultLocale;
    int32_t defaultLocaleIndex;
};

U_NAMESPACE_END

#endif  // U_HIDE_DRAFT_API
#endif  // U_SHOW_CPLUSPLUS_API
#endif  // __LOCALEMATCHER_H__

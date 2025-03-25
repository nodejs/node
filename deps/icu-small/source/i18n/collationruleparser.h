// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationruleparser.h
*
* created on: 2013apr10
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONRULEPARSER_H__
#define __COLLATIONRULEPARSER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"

struct UParseError;

U_NAMESPACE_BEGIN

struct CollationData;
struct CollationTailoring;

class Locale;
class Normalizer2;

struct CollationSettings;

class U_I18N_API CollationRuleParser : public UMemory {
public:
    /** Special reset positions for collation rules. */
    enum Position {
        FIRST_TERTIARY_IGNORABLE,
        LAST_TERTIARY_IGNORABLE,
        FIRST_SECONDARY_IGNORABLE,
        LAST_SECONDARY_IGNORABLE,
        FIRST_PRIMARY_IGNORABLE,
        LAST_PRIMARY_IGNORABLE,
        FIRST_VARIABLE,
        LAST_VARIABLE,
        FIRST_REGULAR,
        LAST_REGULAR,
        FIRST_IMPLICIT,
        LAST_IMPLICIT,
        FIRST_TRAILING,
        LAST_TRAILING
    };

    /**
     * Constants used for reset positions in collation rules.
     * U+FFFE cannot be tailored via rule syntax.
     */
    static constexpr char16_t RESET_POSITION_LEAD = 0xfffe;
    static constexpr char16_t RESET_POSITION_BASE = 0x2800;

    class U_I18N_API Sink : public UObject {
    public:
        virtual ~Sink();

        /**
         * Adds a reset rule.
         * @param strength The collation strength (e.g., UCOL_PRIMARY).
         * @param str The UnicodeString for the rule.
         * @param errorReason The error message, if any.
         * @param errorCode The error code to track.
         */
        virtual void addReset(int32_t strength, const UnicodeString &str,
                              const char *&errorReason, UErrorCode &errorCode) = 0;

        /**
         * Adds a relation rule with the given strength and strings.
         * @param strength The collation strength (e.g., UCOL_PRIMARY).
         * @param prefix The prefix UnicodeString.
         * @param str The UnicodeString for the rule.
         * @param extension The extension UnicodeString.
         * @param errorReason The error message, if any.
         * @param errorCode The error code to track.
         */
        virtual void addRelation(int32_t strength, const UnicodeString &prefix,
                                 const UnicodeString &str, const UnicodeString &extension,
                                 const char *&errorReason, UErrorCode &errorCode) = 0;

        /**
         * Suppresses contractions for a given UnicodeSet.
         * @param set The set of Unicode characters to suppress.
         * @param errorReason The error message, if any.
         * @param errorCode The error code to track.
         */
        virtual void suppressContractions(const UnicodeSet &set, const char *&errorReason,
                                          UErrorCode &errorCode);

        /**
         * Optimizes the given UnicodeSet.
         * @param set The set of Unicode characters to optimize.
         * @param errorReason The error message, if any.
         * @param errorCode The error code to track.
         */
        virtual void optimize(const UnicodeSet &set, const char *&errorReason,
                              UErrorCode &errorCode);
    };

    class U_I18N_API Importer : public UObject {
    public:
        virtual ~Importer();

        /**
         * Retrieves the collation rules for a given locale.
         * @param localeID The locale identifier.
         * @param collationType The collation type.
         * @param rules The string to store the rules.
         * @param errorReason The error message, if any.
         * @param errorCode The error code to track.
         */
        virtual void getRules(
                const char *localeID, const char *collationType,
                UnicodeString &rules,
                const char *&errorReason, UErrorCode &errorCode) = 0;
    };

    /**
     * Constructor.
     * The Sink must be set before parsing.
     * The Importer can be set, otherwise [import locale] syntax is not supported.
     */
    CollationRuleParser(const CollationData *base, UErrorCode &errorCode);
    ~CollationRuleParser();

    /**
     * Sets the pointer to a Sink object.
     * The pointer is aliased: Pointer copy without cloning or taking ownership.
     * @param sinkAlias The Sink object to be set.
     */
    void setSink(Sink *sinkAlias) {
        sink = sinkAlias;
    }

    /**
     * Sets the pointer to an Importer object.
     * The pointer is aliased: Pointer copy without cloning or taking ownership.
     * @param importerAlias The Importer object to be set.
     */
    void setImporter(Importer *importerAlias) {
        importer = importerAlias;
    }

    /**
     * Parses the collation rule string.
     * @param ruleString The rule string to be parsed.
     * @param outSettings The parsed collation settings.
     * @param outParseError The parse error details.
     * @param errorCode The error code to track.
     */
    void parse(const UnicodeString &ruleString,
               CollationSettings &outSettings,
               UParseError *outParseError,
               UErrorCode &errorCode);

    /**
     * Gets the last error reason.
     * @return The last error message.
     */
    const char *getErrorReason() const { return errorReason; }

    /**
     * Gets a script or reorder code from its string representation.
     * @param word The string representation.
     * @return The script/reorder code, or -1 if not recognized.
     */
    static int32_t getReorderCode(const char *word);

private:
    static constexpr int32_t STRENGTH_MASK = 0xf;
    static constexpr int32_t STARRED_FLAG = 0x10;
    static constexpr int32_t OFFSET_SHIFT = 8;

    void parse(const UnicodeString &ruleString, UErrorCode &errorCode);
    void parseRuleChain(UErrorCode &errorCode);
    int32_t parseResetAndPosition(UErrorCode &errorCode);
    int32_t parseRelationOperator(UErrorCode &errorCode);
    void parseRelationStrings(int32_t strength, int32_t i, UErrorCode &errorCode);
    void parseStarredCharacters(int32_t strength, int32_t i, UErrorCode &errorCode);
    int32_t parseTailoringString(int32_t i, UnicodeString &raw, UErrorCode &errorCode);
    int32_t parseString(int32_t i, UnicodeString &raw, UErrorCode &errorCode);

    int32_t parseSpecialPosition(int32_t i, UnicodeString &str, UErrorCode &errorCode);
    void parseSetting(UErrorCode &errorCode);
    void parseReordering(const UnicodeString &raw, UErrorCode &errorCode);
    static UColAttributeValue getOnOffValue(const UnicodeString &s);

    int32_t parseUnicodeSet(int32_t i, UnicodeSet &set, UErrorCode &errorCode);
    int32_t readWords(int32_t i, UnicodeString &raw) const;
    int32_t skipComment(int32_t i) const;

    void setParseError(const char *reason, UErrorCode &errorCode);
    void setErrorContext();

    static bool isSyntaxChar(UChar32 c);
    int32_t skipWhiteSpace(int32_t i) const;

    const Normalizer2 &nfd, &nfc;

    const UnicodeString *rules;
    const CollationData *const baseData;
    CollationSettings *settings;
    UParseError *parseError;
    const char *errorReason;

    Sink *sink;
    Importer *importer;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONRULEPARSER_H__

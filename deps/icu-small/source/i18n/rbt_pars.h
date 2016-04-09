/*
**********************************************************************
* Copyright (C) 1999-2011, International Business Machines Corporation
* and others. All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   11/17/99    aliu        Creation.
**********************************************************************
*/
#ifndef RBT_PARS_H
#define RBT_PARS_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION
#ifdef __cplusplus

#include "unicode/uobject.h"
#include "unicode/parseerr.h"
#include "unicode/unorm.h"
#include "rbt.h"
#include "hash.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

class TransliterationRuleData;
class UnicodeFunctor;
class ParseData;
class RuleHalf;
class ParsePosition;
class StringMatcher;

class TransliteratorParser : public UMemory {

 public:

    /**
     * A Vector of TransliterationRuleData objects, one for each discrete group
     * of rules in the rule set
     */
    UVector dataVector;

    /**
     * PUBLIC data member.
     * A Vector of UnicodeStrings containing all of the ID blocks in the rule set
     */
    UVector idBlockVector;

    /**
     * PUBLIC data member containing the parsed compound filter, if any.
     */
    UnicodeSet* compoundFilter;

 private:

    /**
     * The current data object for which we are parsing rules
     */
    TransliterationRuleData* curData;

    UTransDirection direction;

    /**
     * Parse error information.
     */
    UParseError parseError;

    /**
     * Temporary symbol table used during parsing.
     */
    ParseData* parseData;

    /**
     * Temporary vector of matcher variables.  When parsing is complete, this
     * is copied into the array data.variables.  As with data.variables,
     * element 0 corresponds to character data.variablesBase.
     */
    UVector variablesVector;

    /**
     * Temporary table of variable names.  When parsing is complete, this is
     * copied into data.variableNames.
     */
    Hashtable variableNames;

    /**
     * String of standins for segments.  Used during the parsing of a single
     * rule.  segmentStandins.charAt(0) is the standin for "$1" and corresponds
     * to StringMatcher object segmentObjects.elementAt(0), etc.
     */
    UnicodeString segmentStandins;

    /**
     * Vector of StringMatcher objects for segments.  Used during the
     * parsing of a single rule.
     * segmentStandins.charAt(0) is the standin for "$1" and corresponds
     * to StringMatcher object segmentObjects.elementAt(0), etc.
     */
    UVector segmentObjects;

    /**
     * The next available stand-in for variables.  This starts at some point in
     * the private use area (discovered dynamically) and increments up toward
     * <code>variableLimit</code>.  At any point during parsing, available
     * variables are <code>variableNext..variableLimit-1</code>.
     */
    UChar variableNext;

    /**
     * The last available stand-in for variables.  This is discovered
     * dynamically.  At any point during parsing, available variables are
     * <code>variableNext..variableLimit-1</code>.
     */
    UChar variableLimit;

    /**
     * When we encounter an undefined variable, we do not immediately signal
     * an error, in case we are defining this variable, e.g., "$a = [a-z];".
     * Instead, we save the name of the undefined variable, and substitute
     * in the placeholder char variableLimit - 1, and decrement
     * variableLimit.
     */
    UnicodeString undefinedVariableName;

    /**
     * The stand-in character for the 'dot' set, represented by '.' in
     * patterns.  This is allocated the first time it is needed, and
     * reused thereafter.
     */
    UChar dotStandIn;

public:

    /**
     * Constructor.
     */
    TransliteratorParser(UErrorCode &statusReturn);

    /**
     * Destructor.
     */
    ~TransliteratorParser();

    /**
     * Parse the given string as a sequence of rules, separated by newline
     * characters ('\n'), and cause this object to implement those rules.  Any
     * previous rules are discarded.  Typically this method is called exactly
     * once after construction.
     *
     * Parse the given rules, in the given direction.  After this call
     * returns, query the public data members for results.  The caller
     * owns the 'data' and 'compoundFilter' data members after this
     * call returns.
     * @param rules      rules, separated by ';'
     * @param direction  either FORWARD or REVERSE.
     * @param pe         Struct to recieve information on position
     *                   of error if an error is encountered
     * @param ec         Output param set to success/failure code.
     */
    void parse(const UnicodeString& rules,
               UTransDirection direction,
               UParseError& pe,
               UErrorCode& ec);

    /**
     * Return the compound filter parsed by parse().  Caller owns result.
     * @return the compound filter parsed by parse().
     */
    UnicodeSet* orphanCompoundFilter();

private:

    /**
     * Return a representation of this transliterator as source rules.
     * @param rules      Output param to receive the rules.
     * @param direction  either FORWARD or REVERSE.
     */
    void parseRules(const UnicodeString& rules,
                    UTransDirection direction,
                    UErrorCode& status);

    /**
     * MAIN PARSER.  Parse the next rule in the given rule string, starting
     * at pos.  Return the index after the last character parsed.  Do not
     * parse characters at or after limit.
     *
     * Important:  The character at pos must be a non-whitespace character
     * that is not the comment character.
     *
     * This method handles quoting, escaping, and whitespace removal.  It
     * parses the end-of-rule character.  It recognizes context and cursor
     * indicators.  Once it does a lexical breakdown of the rule at pos, it
     * creates a rule object and adds it to our rule list.
     * @param rules      Output param to receive the rules.
     * @param pos        the starting position.
     * @param limit      pointer past the last character of the rule.
     * @return           the index after the last character parsed.
     */
    int32_t parseRule(const UnicodeString& rule, int32_t pos, int32_t limit, UErrorCode& status);

    /**
     * Set the variable range to [start, end] (inclusive).
     * @param start    the start value of the range.
     * @param end      the end value of the range.
     */
    void setVariableRange(int32_t start, int32_t end, UErrorCode& status);

    /**
     * Assert that the given character is NOT within the variable range.
     * If it is, return FALSE.  This is neccesary to ensure that the
     * variable range does not overlap characters used in a rule.
     * @param ch     the given character.
     * @return       True, if the given character is NOT within the variable range.
     */
    UBool checkVariableRange(UChar32 ch) const;

    /**
     * Set the maximum backup to 'backup', in response to a pragma
     * statement.
     * @param backup    the new value to be set.
     */
    void pragmaMaximumBackup(int32_t backup);

    /**
     * Begin normalizing all rules using the given mode, in response
     * to a pragma statement.
     * @param mode    the given mode.
     */
    void pragmaNormalizeRules(UNormalizationMode mode);

    /**
     * Return true if the given rule looks like a pragma.
     * @param pos offset to the first non-whitespace character
     * of the rule.
     * @param limit pointer past the last character of the rule.
     * @return true if the given rule looks like a pragma.
     */
    static UBool resemblesPragma(const UnicodeString& rule, int32_t pos, int32_t limit);

    /**
     * Parse a pragma.  This method assumes resemblesPragma() has
     * already returned true.
     * @param pos offset to the first non-whitespace character
     * of the rule.
     * @param limit pointer past the last character of the rule.
     * @return the position index after the final ';' of the pragma,
     * or -1 on failure.
     */
    int32_t parsePragma(const UnicodeString& rule, int32_t pos, int32_t limit, UErrorCode& status);

    /**
     * Called by main parser upon syntax error.  Search the rule string
     * for the probable end of the rule.  Of course, if the error is that
     * the end of rule marker is missing, then the rule end will not be found.
     * In any case the rule start will be correctly reported.
     * @param parseErrorCode error code.
     * @param msg error description.
     * @param start position of first character of current rule.
     * @return start position of first character of current rule.
     */
    int32_t syntaxError(UErrorCode parseErrorCode, const UnicodeString&, int32_t start,
                        UErrorCode& status);

    /**
     * Parse a UnicodeSet out, store it, and return the stand-in character
     * used to represent it.
     *
     * @param rule    the rule for UnicodeSet.
     * @param pos     the position in pattern at which to start parsing.
     * @return        the stand-in character used to represent it.
     */
    UChar parseSet(const UnicodeString& rule,
                   ParsePosition& pos,
                   UErrorCode& status);

    /**
     * Generate and return a stand-in for a new UnicodeFunctor.  Store
     * the matcher (adopt it).
     * @param adopted the UnicodeFunctor to be adopted.
     * @return        a stand-in for a new UnicodeFunctor.
     */
    UChar generateStandInFor(UnicodeFunctor* adopted, UErrorCode& status);

    /**
     * Return the standin for segment seg (1-based).
     * @param seg    the given segment.
     * @return       the standIn character for the given segment.
     */
    UChar getSegmentStandin(int32_t seg, UErrorCode& status);

    /**
     * Set the object for segment seg (1-based).
     * @param seg      the given segment.
     * @param adopted  the StringMatcher to be adopted.
     */
    void setSegmentObject(int32_t seg, StringMatcher* adopted, UErrorCode& status);

    /**
     * Return the stand-in for the dot set.  It is allocated the first
     * time and reused thereafter.
     * @return    the stand-in for the dot set.
     */
    UChar getDotStandIn(UErrorCode& status);

    /**
     * Append the value of the given variable name to the given
     * UnicodeString.
     * @param name    the variable name to be appended.
     * @param buf     the given UnicodeString to append to.
     */
    void appendVariableDef(const UnicodeString& name,
                           UnicodeString& buf,
                           UErrorCode& status);

    /**
     * Glue method to get around access restrictions in C++.
     */
    /*static Transliterator* createBasicInstance(const UnicodeString& id,
                                               const UnicodeString* canonID);*/

    friend class RuleHalf;

    // Disallowed methods; no impl.
    /**
     * Copy constructor
     */
    TransliteratorParser(const TransliteratorParser&);

    /**
     * Assignment operator
     */
    TransliteratorParser& operator=(const TransliteratorParser&);
};

U_NAMESPACE_END

#endif /* #ifdef __cplusplus */

/**
 * Strip/convert the following from the transliterator rules:
 * comments
 * newlines
 * white space at the beginning and end of a line
 * unescape \u notation
 *
 * The target must be equal in size as the source.
 * @internal
 */
U_CAPI int32_t
utrans_stripRules(const UChar *source, int32_t sourceLen, UChar *target, UErrorCode *status);

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

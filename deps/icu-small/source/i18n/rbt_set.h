// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (C) 1999-2007, International Business Machines Corporation
* and others. All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   11/17/99    aliu        Creation.
**********************************************************************
*/
#ifndef RBT_SET_H
#define RBT_SET_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/uobject.h"
#include "unicode/utrans.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

class Replaceable;
class TransliterationRule;
class TransliterationRuleData;
class UnicodeFilter;
class UnicodeString;
class UnicodeSet;

/**
 * A set of rules for a <code>RuleBasedTransliterator</code>.
 * @author Alan Liu
 */
class TransliterationRuleSet : public UMemory {
    /**
     * Vector of rules, in the order added.  This is used while the
     * rule set is getting built.  After that, freeze() reorders and
     * indexes the rules into rules[].  Any given rule is stored once
     * in ruleVector, and one or more times in rules[].  ruleVector
     * owns and deletes the rules.
     */
    UVector* ruleVector;

    /**
     * Sorted and indexed table of rules.  This is created by freeze()
     * from the rules in ruleVector.  It contains alias pointers to
     * the rules in ruleVector.  It is zero before freeze() is called
     * and non-zero thereafter.
     */
    TransliterationRule** rules;

    /**
     * Index table.  For text having a first character c, compute x = c&0xFF.
     * Now use rules[index[x]..index[x+1]-1].  This index table is created by
     * freeze().  Before freeze() is called it contains garbage.
     */
    int32_t index[257];

    /**
     * Length of the longest preceding context
     */
    int32_t maxContextLength;

public:

    /**
     * Construct a new empty rule set.
     * @param status    Output parameter filled in with success or failure status.
     */
    TransliterationRuleSet(UErrorCode& status);

    /**
     * Copy constructor.
     */
    TransliterationRuleSet(const TransliterationRuleSet&);

    /**
     * Destructor.
     */
    virtual ~TransliterationRuleSet();

    /**
     * Change the data object that this rule belongs to.  Used
     * internally by the TransliterationRuleData copy constructor.
     * @param data    the new data value to be set.
     */
    void setData(const TransliterationRuleData* data);

    /**
     * Return the maximum context length.
     * @return the length of the longest preceding context.
     */
    virtual int32_t getMaximumContextLength(void) const;

    /**
     * Add a rule to this set.  Rules are added in order, and order is
     * significant.  The last call to this method must be followed by
     * a call to <code>freeze()</code> before the rule set is used.
     * This method must <em>not</em> be called after freeze() has been
     * called.
     *
     * @param adoptedRule the rule to add
     */
    virtual void addRule(TransliterationRule* adoptedRule,
                         UErrorCode& status);

    /**
     * Check this for masked rules and index it to optimize performance.
     * The sequence of operations is: (1) add rules to a set using
     * <code>addRule()</code>; (2) freeze the set using
     * <code>freeze()</code>; (3) use the rule set.  If
     * <code>addRule()</code> is called after calling this method, it
     * invalidates this object, and this method must be called again.
     * That is, <code>freeze()</code> may be called multiple times,
     * although for optimal performance it shouldn't be.
     * @param parseError A pointer to UParseError to receive information about errors
     *                   occurred.
     * @param status     Output parameter filled in with success or failure status.
     */
    virtual void freeze(UParseError& parseError, UErrorCode& status);

    /**
     * Transliterate the given text with the given UTransPosition
     * indices.  Return true if the transliteration should continue
     * or false if it should halt (because of a U_PARTIAL_MATCH match).
     * Note that false is only ever returned if isIncremental is true.
     * @param text the text to be transliterated
     * @param index the position indices, which will be updated
     * @param isIncremental if true, assume new text may be inserted
     * at index.limit, and return false if thre is a partial match.
     * @return true unless a U_PARTIAL_MATCH has been obtained,
     * indicating that transliteration should stop until more text
     * arrives.
     */
    UBool transliterate(Replaceable& text,
                        UTransPosition& index,
                        UBool isIncremental);

    /**
     * Create rule strings that represents this rule set.
     * @param result string to receive the rule strings.  Current
     * contents will be deleted.
     * @param escapeUnprintable  True, will escape the unprintable characters
     * @return    A reference to 'result'.
     */
    virtual UnicodeString& toRules(UnicodeString& result,
                                   UBool escapeUnprintable) const;

    /**
     * Return the set of all characters that may be modified
     * (getTarget=false) or emitted (getTarget=true) by this set.
     */
    UnicodeSet& getSourceTargetSet(UnicodeSet& result,
                   UBool getTarget) const;

private:

    TransliterationRuleSet &operator=(const TransliterationRuleSet &other); // forbid copying of this class
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

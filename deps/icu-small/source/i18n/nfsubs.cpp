/*
******************************************************************************
*   Copyright (C) 1997-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
******************************************************************************
*   file name:  nfsubs.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
* Modification history
* Date        Name      Comments
* 10/11/2001  Doug      Ported from ICU4J
*/

#include <stdio.h>
#include "utypeinfo.h"  // for 'typeid' to work

#include "nfsubs.h"
#include "digitlst.h"

#if U_HAVE_RBNF

static const UChar gLessThan = 0x003c;
static const UChar gEquals = 0x003d;
static const UChar gGreaterThan = 0x003e;
static const UChar gPercent = 0x0025;
static const UChar gPound = 0x0023;
static const UChar gZero = 0x0030;
static const UChar gSpace = 0x0020;

static const UChar gEqualsEquals[] =
{
    0x3D, 0x3D, 0
}; /* "==" */
static const UChar gGreaterGreaterGreaterThan[] =
{
    0x3E, 0x3E, 0x3E, 0
}; /* ">>>" */
static const UChar gGreaterGreaterThan[] =
{
    0x3E, 0x3E, 0
}; /* ">>" */

U_NAMESPACE_BEGIN

class SameValueSubstitution : public NFSubstitution {
public:
    SameValueSubstitution(int32_t pos,
        const NFRuleSet* ruleset,
        const UnicodeString& description,
        UErrorCode& status);
    virtual ~SameValueSubstitution();

    virtual int64_t transformNumber(int64_t number) const { return number; }
    virtual double transformNumber(double number) const { return number; }
    virtual double composeRuleValue(double newRuleValue, double /*oldRuleValue*/) const { return newRuleValue; }
    virtual double calcUpperBound(double oldUpperBound) const { return oldUpperBound; }
    virtual UChar tokenChar() const { return (UChar)0x003d; } // '='

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

SameValueSubstitution::~SameValueSubstitution() {}

class MultiplierSubstitution : public NFSubstitution {
    double divisor;
    int64_t ldivisor;

public:
    MultiplierSubstitution(int32_t _pos,
        double _divisor,
        const NFRuleSet* _ruleSet,
        const UnicodeString& description,
        UErrorCode& status)
        : NFSubstitution(_pos, _ruleSet, description, status), divisor(_divisor)
    {
        ldivisor = util64_fromDouble(divisor);
        if (divisor == 0) {
            status = U_PARSE_ERROR;
        }
    }
    virtual ~MultiplierSubstitution();

    virtual void setDivisor(int32_t radix, int32_t exponent, UErrorCode& status) {
        divisor = uprv_pow(radix, exponent);
        ldivisor = util64_fromDouble(divisor);

        if(divisor == 0) {
            status = U_PARSE_ERROR;
        }
    }

    virtual UBool operator==(const NFSubstitution& rhs) const;

    virtual int64_t transformNumber(int64_t number) const {
        return number / ldivisor;
    }

    virtual double transformNumber(double number) const {
        if (getRuleSet()) {
            return uprv_floor(number / divisor);
        } else {
            return number/divisor;
        }
    }

    virtual double composeRuleValue(double newRuleValue, double /*oldRuleValue*/) const {
        return newRuleValue * divisor;
    }

    virtual double calcUpperBound(double /*oldUpperBound*/) const { return divisor; }

    virtual UChar tokenChar() const { return (UChar)0x003c; } // '<'

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

MultiplierSubstitution::~MultiplierSubstitution() {}

class ModulusSubstitution : public NFSubstitution {
    double divisor;
    int64_t  ldivisor;
    const NFRule* ruleToUse;
public:
    ModulusSubstitution(int32_t pos,
        double _divisor,
        const NFRule* rulePredecessor,
        const NFRuleSet* ruleSet,
        const UnicodeString& description,
        UErrorCode& status);
    virtual ~ModulusSubstitution();

    virtual void setDivisor(int32_t radix, int32_t exponent, UErrorCode& status) {
        divisor = uprv_pow(radix, exponent);
        ldivisor = util64_fromDouble(divisor);

        if (divisor == 0) {
            status = U_PARSE_ERROR;
        }
    }

    virtual UBool operator==(const NFSubstitution& rhs) const;

    virtual void doSubstitution(int64_t number, UnicodeString& toInsertInto, int32_t pos, int32_t recursionCount, UErrorCode& status) const;
    virtual void doSubstitution(double number, UnicodeString& toInsertInto, int32_t pos, int32_t recursionCount, UErrorCode& status) const;

    virtual int64_t transformNumber(int64_t number) const { return number % ldivisor; }
    virtual double transformNumber(double number) const { return uprv_fmod(number, divisor); }

    virtual UBool doParse(const UnicodeString& text,
        ParsePosition& parsePosition,
        double baseValue,
        double upperBound,
        UBool lenientParse,
        Formattable& result) const;

    virtual double composeRuleValue(double newRuleValue, double oldRuleValue) const {
        return oldRuleValue - uprv_fmod(oldRuleValue, divisor) + newRuleValue;
    }

    virtual double calcUpperBound(double /*oldUpperBound*/) const { return divisor; }

    virtual UBool isModulusSubstitution() const { return TRUE; }

    virtual UChar tokenChar() const { return (UChar)0x003e; } // '>'

	virtual void toString(UnicodeString& result) const;

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

ModulusSubstitution::~ModulusSubstitution() {}

class IntegralPartSubstitution : public NFSubstitution {
public:
    IntegralPartSubstitution(int32_t _pos,
        const NFRuleSet* _ruleSet,
        const UnicodeString& description,
        UErrorCode& status)
        : NFSubstitution(_pos, _ruleSet, description, status) {}
    virtual ~IntegralPartSubstitution();

    virtual int64_t transformNumber(int64_t number) const { return number; }
    virtual double transformNumber(double number) const { return uprv_floor(number); }
    virtual double composeRuleValue(double newRuleValue, double oldRuleValue) const { return newRuleValue + oldRuleValue; }
    virtual double calcUpperBound(double /*oldUpperBound*/) const { return DBL_MAX; }
    virtual UChar tokenChar() const { return (UChar)0x003c; } // '<'

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

IntegralPartSubstitution::~IntegralPartSubstitution() {}

class FractionalPartSubstitution : public NFSubstitution {
    UBool byDigits;
    UBool useSpaces;
    enum { kMaxDecimalDigits = 8 };
public:
    FractionalPartSubstitution(int32_t pos,
        const NFRuleSet* ruleSet,
        const UnicodeString& description,
        UErrorCode& status);
    virtual ~FractionalPartSubstitution();

    virtual UBool operator==(const NFSubstitution& rhs) const;

    virtual void doSubstitution(double number, UnicodeString& toInsertInto, int32_t pos, int32_t recursionCount, UErrorCode& status) const;
    virtual void doSubstitution(int64_t /*number*/, UnicodeString& /*toInsertInto*/, int32_t /*_pos*/, int32_t /*recursionCount*/, UErrorCode& /*status*/) const {}
    virtual int64_t transformNumber(int64_t /*number*/) const { return 0; }
    virtual double transformNumber(double number) const { return number - uprv_floor(number); }

    virtual UBool doParse(const UnicodeString& text,
        ParsePosition& parsePosition,
        double baseValue,
        double upperBound,
        UBool lenientParse,
        Formattable& result) const;

    virtual double composeRuleValue(double newRuleValue, double oldRuleValue) const { return newRuleValue + oldRuleValue; }
    virtual double calcUpperBound(double /*oldUpperBound*/) const { return 0.0; }
    virtual UChar tokenChar() const { return (UChar)0x003e; } // '>'

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

FractionalPartSubstitution::~FractionalPartSubstitution() {}

class AbsoluteValueSubstitution : public NFSubstitution {
public:
    AbsoluteValueSubstitution(int32_t _pos,
        const NFRuleSet* _ruleSet,
        const UnicodeString& description,
        UErrorCode& status)
        : NFSubstitution(_pos, _ruleSet, description, status) {}
    virtual ~AbsoluteValueSubstitution();

    virtual int64_t transformNumber(int64_t number) const { return number >= 0 ? number : -number; }
    virtual double transformNumber(double number) const { return uprv_fabs(number); }
    virtual double composeRuleValue(double newRuleValue, double /*oldRuleValue*/) const { return -newRuleValue; }
    virtual double calcUpperBound(double /*oldUpperBound*/) const { return DBL_MAX; }
    virtual UChar tokenChar() const { return (UChar)0x003e; } // '>'

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

AbsoluteValueSubstitution::~AbsoluteValueSubstitution() {}

class NumeratorSubstitution : public NFSubstitution {
    double denominator;
    int64_t ldenominator;
    UBool withZeros;
public:
    static inline UnicodeString fixdesc(const UnicodeString& desc) {
        if (desc.endsWith(LTLT, 2)) {
            UnicodeString result(desc, 0, desc.length()-1);
            return result;
        }
        return desc;
    }
    NumeratorSubstitution(int32_t _pos,
        double _denominator,
        NFRuleSet* _ruleSet,
        const UnicodeString& description,
        UErrorCode& status)
        : NFSubstitution(_pos, _ruleSet, fixdesc(description), status), denominator(_denominator)
    {
        ldenominator = util64_fromDouble(denominator);
        withZeros = description.endsWith(LTLT, 2);
    }
    virtual ~NumeratorSubstitution();

    virtual UBool operator==(const NFSubstitution& rhs) const;

    virtual int64_t transformNumber(int64_t number) const { return number * ldenominator; }
    virtual double transformNumber(double number) const { return uprv_round(number * denominator); }

    virtual void doSubstitution(int64_t /*number*/, UnicodeString& /*toInsertInto*/, int32_t /*_pos*/, int32_t /*recursionCount*/, UErrorCode& /*status*/) const {}
    virtual void doSubstitution(double number, UnicodeString& toInsertInto, int32_t pos, int32_t recursionCount, UErrorCode& status) const;
    virtual UBool doParse(const UnicodeString& text,
        ParsePosition& parsePosition,
        double baseValue,
        double upperBound,
        UBool /*lenientParse*/,
        Formattable& result) const;

    virtual double composeRuleValue(double newRuleValue, double oldRuleValue) const { return newRuleValue / oldRuleValue; }
    virtual double calcUpperBound(double /*oldUpperBound*/) const { return denominator; }
    virtual UChar tokenChar() const { return (UChar)0x003c; } // '<'
private:
    static const UChar LTLT[2];

public:
    static UClassID getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
};

NumeratorSubstitution::~NumeratorSubstitution() {}

NFSubstitution*
NFSubstitution::makeSubstitution(int32_t pos,
                                 const NFRule* rule,
                                 const NFRule* predecessor,
                                 const NFRuleSet* ruleSet,
                                 const RuleBasedNumberFormat* formatter,
                                 const UnicodeString& description,
                                 UErrorCode& status)
{
    // if the description is empty, return a NullSubstitution
    if (description.length() == 0) {
        return NULL;
    }

    switch (description.charAt(0)) {
        // if the description begins with '<'...
    case gLessThan:
        // throw an exception if the rule is a negative number
        // rule
        if (rule->getBaseValue() == NFRule::kNegativeNumberRule) {
            // throw new IllegalArgumentException("<< not allowed in negative-number rule");
            status = U_PARSE_ERROR;
            return NULL;
        }

        // if the rule is a fraction rule, return an
        // IntegralPartSubstitution
        else if (rule->getBaseValue() == NFRule::kImproperFractionRule
            || rule->getBaseValue() == NFRule::kProperFractionRule
            || rule->getBaseValue() == NFRule::kMasterRule) {
            return new IntegralPartSubstitution(pos, ruleSet, description, status);
        }

        // if the rule set containing the rule is a fraction
        // rule set, return a NumeratorSubstitution
        else if (ruleSet->isFractionRuleSet()) {
            return new NumeratorSubstitution(pos, (double)rule->getBaseValue(),
                formatter->getDefaultRuleSet(), description, status);
        }

        // otherwise, return a MultiplierSubstitution
        else {
            return new MultiplierSubstitution(pos, rule->getDivisor(), ruleSet,
                description, status);
        }

        // if the description begins with '>'...
    case gGreaterThan:
        // if the rule is a negative-number rule, return
        // an AbsoluteValueSubstitution
        if (rule->getBaseValue() == NFRule::kNegativeNumberRule) {
            return new AbsoluteValueSubstitution(pos, ruleSet, description, status);
        }

        // if the rule is a fraction rule, return a
        // FractionalPartSubstitution
        else if (rule->getBaseValue() == NFRule::kImproperFractionRule
            || rule->getBaseValue() == NFRule::kProperFractionRule
            || rule->getBaseValue() == NFRule::kMasterRule) {
            return new FractionalPartSubstitution(pos, ruleSet, description, status);
        }

        // if the rule set owning the rule is a fraction rule set,
        // throw an exception
        else if (ruleSet->isFractionRuleSet()) {
            // throw new IllegalArgumentException(">> not allowed in fraction rule set");
            status = U_PARSE_ERROR;
            return NULL;
        }

        // otherwise, return a ModulusSubstitution
        else {
            return new ModulusSubstitution(pos, rule->getDivisor(), predecessor,
                ruleSet, description, status);
        }

        // if the description begins with '=', always return a
        // SameValueSubstitution
    case gEquals:
        return new SameValueSubstitution(pos, ruleSet, description, status);

        // and if it's anything else, throw an exception
    default:
        // throw new IllegalArgumentException("Illegal substitution character");
        status = U_PARSE_ERROR;
    }
    return NULL;
}

NFSubstitution::NFSubstitution(int32_t _pos,
                               const NFRuleSet* _ruleSet,
                               const UnicodeString& description,
                               UErrorCode& status)
                               : pos(_pos), ruleSet(NULL), numberFormat(NULL)
{
    // the description should begin and end with the same character.
    // If it doesn't that's a syntax error.  Otherwise,
    // makeSubstitution() was the only thing that needed to know
    // about these characters, so strip them off
    UnicodeString workingDescription(description);
    if (description.length() >= 2
        && description.charAt(0) == description.charAt(description.length() - 1))
    {
        workingDescription.remove(description.length() - 1, 1);
        workingDescription.remove(0, 1);
    }
    else if (description.length() != 0) {
        // throw new IllegalArgumentException("Illegal substitution syntax");
        status = U_PARSE_ERROR;
        return;
    }

    if (workingDescription.length() == 0) {
        // if the description was just two paired token characters
        // (i.e., "<<" or ">>"), it uses the rule set it belongs to to
        // format its result
        this->ruleSet = _ruleSet;
    }
    else if (workingDescription.charAt(0) == gPercent) {
        // if the description contains a rule set name, that's the rule
        // set we use to format the result: get a reference to the
        // names rule set
        this->ruleSet = _ruleSet->getOwner()->findRuleSet(workingDescription, status);
    }
    else if (workingDescription.charAt(0) == gPound || workingDescription.charAt(0) ==gZero) {
        // if the description begins with 0 or #, treat it as a
        // DecimalFormat pattern, and initialize a DecimalFormat with
        // that pattern (then set it to use the DecimalFormatSymbols
        // belonging to our formatter)
        const DecimalFormatSymbols* sym = _ruleSet->getOwner()->getDecimalFormatSymbols();
        if (!sym) {
            status = U_MISSING_RESOURCE_ERROR;
            return;
        }
        DecimalFormat *tempNumberFormat = new DecimalFormat(workingDescription, *sym, status);
        /* test for NULL */
        if (!tempNumberFormat) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if (U_FAILURE(status)) {
            delete tempNumberFormat;
            return;
        }
        this->numberFormat = tempNumberFormat;
    }
    else if (workingDescription.charAt(0) == gGreaterThan) {
        // if the description is ">>>", this substitution bypasses the
        // usual rule-search process and always uses the rule that precedes
        // it in its own rule set's rule list (this is used for place-value
        // notations: formats where you want to see a particular part of
        // a number even when it's 0)

        // this causes problems when >>> is used in a frationalPartSubstitution
        // this->ruleSet = NULL;
        this->ruleSet = _ruleSet;
        this->numberFormat = NULL;
    }
    else {
        // and of the description is none of these things, it's a syntax error

        // throw new IllegalArgumentException("Illegal substitution syntax");
        status = U_PARSE_ERROR;
    }
}

NFSubstitution::~NFSubstitution()
{
    delete numberFormat;
    numberFormat = NULL;
}

/**
 * Set's the substitution's divisor.  Used by NFRule.setBaseValue().
 * A no-op for all substitutions except multiplier and modulus
 * substitutions.
 * @param radix The radix of the divisor
 * @param exponent The exponent of the divisor
 */
void
NFSubstitution::setDivisor(int32_t /*radix*/, int32_t /*exponent*/, UErrorCode& /*status*/) {
  // a no-op for all substitutions except multiplier and modulus substitutions
}

void
NFSubstitution::setDecimalFormatSymbols(const DecimalFormatSymbols &newSymbols, UErrorCode& /*status*/) {
    if (numberFormat != NULL) {
        numberFormat->setDecimalFormatSymbols(newSymbols);
    }
}

//-----------------------------------------------------------------------
// boilerplate
//-----------------------------------------------------------------------

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(NFSubstitution)

/**
 * Compares two substitutions for equality
 * @param The substitution to compare this one to
 * @return true if the two substitutions are functionally equivalent
 */
UBool
NFSubstitution::operator==(const NFSubstitution& rhs) const
{
  // compare class and all of the fields all substitutions have
  // in common
  // this should be called by subclasses before their own equality tests
  return typeid(*this) == typeid(rhs)
  && pos == rhs.pos
  && (ruleSet == NULL) == (rhs.ruleSet == NULL)
  // && ruleSet == rhs.ruleSet causes circularity, other checks to make instead?
  && (numberFormat == NULL
      ? (rhs.numberFormat == NULL)
      : (*numberFormat == *rhs.numberFormat));
}

/**
 * Returns a textual description of the substitution
 * @return A textual description of the substitution.  This might
 * not be identical to the description it was created from, but
 * it'll produce the same result.
 */
void
NFSubstitution::toString(UnicodeString& text) const
{
  // use tokenChar() to get the character at the beginning and
  // end of the substitutin token.  In between them will go
  // either the name of the rule set it uses, or the pattern of
  // the DecimalFormat it uses
  text.remove();
  text.append(tokenChar());

  UnicodeString temp;
  if (ruleSet != NULL) {
    ruleSet->getName(temp);
  } else if (numberFormat != NULL) {
    numberFormat->toPattern(temp);
  }
  text.append(temp);
  text.append(tokenChar());
}

//-----------------------------------------------------------------------
// formatting
//-----------------------------------------------------------------------

/**
 * Performs a mathematical operation on the number, formats it using
 * either ruleSet or decimalFormat, and inserts the result into
 * toInsertInto.
 * @param number The number being formatted.
 * @param toInsertInto The string we insert the result into
 * @param pos The position in toInsertInto where the owning rule's
 * rule text begins (this value is added to this substitution's
 * position to determine exactly where to insert the new text)
 */
void
NFSubstitution::doSubstitution(int64_t number, UnicodeString& toInsertInto, int32_t _pos, int32_t recursionCount, UErrorCode& status) const
{
    if (ruleSet != NULL) {
        // perform a transformation on the number that is dependent
        // on the type of substitution this is, then just call its
        // rule set's format() method to format the result
        ruleSet->format(transformNumber(number), toInsertInto, _pos + this->pos, recursionCount, status);
    } else if (numberFormat != NULL) {
        // or perform the transformation on the number (preserving
        // the result's fractional part if the formatter it set
        // to show it), then use that formatter's format() method
        // to format the result
        double numberToFormat = transformNumber((double)number);
        if (numberFormat->getMaximumFractionDigits() == 0) {
            numberToFormat = uprv_floor(numberToFormat);
        }

        UnicodeString temp;
        numberFormat->format(numberToFormat, temp, status);
        toInsertInto.insert(_pos + this->pos, temp);
    }
}

/**
 * Performs a mathematical operation on the number, formats it using
 * either ruleSet or decimalFormat, and inserts the result into
 * toInsertInto.
 * @param number The number being formatted.
 * @param toInsertInto The string we insert the result into
 * @param pos The position in toInsertInto where the owning rule's
 * rule text begins (this value is added to this substitution's
 * position to determine exactly where to insert the new text)
 */
void
NFSubstitution::doSubstitution(double number, UnicodeString& toInsertInto, int32_t _pos, int32_t recursionCount, UErrorCode& status) const {
    // perform a transformation on the number being formatted that
    // is dependent on the type of substitution this is
    double numberToFormat = transformNumber(number);

    if (uprv_isInfinite(numberToFormat)) {
        // This is probably a minus rule. Combine it with an infinite rule.
        const NFRule *infiniteRule = ruleSet->findDoubleRule(uprv_getInfinity());
        infiniteRule->doFormat(numberToFormat, toInsertInto, _pos + this->pos, recursionCount, status);
        return;
    }

    // if the result is an integer, from here on out we work in integer
    // space (saving time and memory and preserving accuracy)
    if (numberToFormat == uprv_floor(numberToFormat) && ruleSet != NULL) {
        ruleSet->format(util64_fromDouble(numberToFormat), toInsertInto, _pos + this->pos, recursionCount, status);

        // if the result isn't an integer, then call either our rule set's
        // format() method or our DecimalFormat's format() method to
        // format the result
    } else {
        if (ruleSet != NULL) {
            ruleSet->format(numberToFormat, toInsertInto, _pos + this->pos, recursionCount, status);
        } else if (numberFormat != NULL) {
            UnicodeString temp;
            numberFormat->format(numberToFormat, temp);
            toInsertInto.insert(_pos + this->pos, temp);
        }
    }
}


    //-----------------------------------------------------------------------
    // parsing
    //-----------------------------------------------------------------------

#ifdef RBNF_DEBUG
#include <stdio.h>
#endif

/**
 * Parses a string using the rule set or DecimalFormat belonging
 * to this substitution.  If there's a match, a mathematical
 * operation (the inverse of the one used in formatting) is
 * performed on the result of the parse and the value passed in
 * and returned as the result.  The parse position is updated to
 * point to the first unmatched character in the string.
 * @param text The string to parse
 * @param parsePosition On entry, ignored, but assumed to be 0.
 * On exit, this is updated to point to the first unmatched
 * character (or 0 if the substitution didn't match)
 * @param baseValue A partial parse result that should be
 * combined with the result of this parse
 * @param upperBound When searching the rule set for a rule
 * matching the string passed in, only rules with base values
 * lower than this are considered
 * @param lenientParse If true and matching against rules fails,
 * the substitution will also try matching the text against
 * numerals using a default-costructed NumberFormat.  If false,
 * no extra work is done.  (This value is false whenever the
 * formatter isn't in lenient-parse mode, but is also false
 * under some conditions even when the formatter _is_ in
 * lenient-parse mode.)
 * @return If there's a match, this is the result of composing
 * baseValue with whatever was returned from matching the
 * characters.  This will be either a Long or a Double.  If there's
 * no match this is new Long(0) (not null), and parsePosition
 * is left unchanged.
 */
UBool
NFSubstitution::doParse(const UnicodeString& text,
                        ParsePosition& parsePosition,
                        double baseValue,
                        double upperBound,
                        UBool lenientParse,
                        Formattable& result) const
{
#ifdef RBNF_DEBUG
    fprintf(stderr, "<nfsubs> %x bv: %g ub: %g\n", this, baseValue, upperBound);
#endif
    // figure out the highest base value a rule can have and match
    // the text being parsed (this varies according to the type of
    // substitutions: multiplier, modulus, and numerator substitutions
    // restrict the search to rules with base values lower than their
    // own; same-value substitutions leave the upper bound wherever
    // it was, and the others allow any rule to match
    upperBound = calcUpperBound(upperBound);

    // use our rule set to parse the text.  If that fails and
    // lenient parsing is enabled (this is always false if the
    // formatter's lenient-parsing mode is off, but it may also
    // be false even when the formatter's lenient-parse mode is
    // on), then also try parsing the text using a default-
    // constructed NumberFormat
    if (ruleSet != NULL) {
        ruleSet->parse(text, parsePosition, upperBound, result);
        if (lenientParse && !ruleSet->isFractionRuleSet() && parsePosition.getIndex() == 0) {
            UErrorCode status = U_ZERO_ERROR;
            NumberFormat* fmt = NumberFormat::createInstance(status);
            if (U_SUCCESS(status)) {
                fmt->parse(text, result, parsePosition);
            }
            delete fmt;
        }

        // ...or use our DecimalFormat to parse the text
    } else if (numberFormat != NULL) {
        numberFormat->parse(text, result, parsePosition);
    }

    // if the parse was successful, we've already advanced the caller's
    // parse position (this is the one function that doesn't have one
    // of its own).  Derive a parse result and return it as a Long,
    // if possible, or a Double
    if (parsePosition.getIndex() != 0) {
        UErrorCode status = U_ZERO_ERROR;
        double tempResult = result.getDouble(status);

        // composeRuleValue() produces a full parse result from
        // the partial parse result passed to this function from
        // the caller (this is either the owning rule's base value
        // or the partial result obtained from composing the
        // owning rule's base value with its other substitution's
        // parse result) and the partial parse result obtained by
        // matching the substitution (which will be the same value
        // the caller would get by parsing just this part of the
        // text with RuleBasedNumberFormat.parse() ).  How the two
        // values are used to derive the full parse result depends
        // on the types of substitutions: For a regular rule, the
        // ultimate result is its multiplier substitution's result
        // times the rule's divisor (or the rule's base value) plus
        // the modulus substitution's result (which will actually
        // supersede part of the rule's base value).  For a negative-
        // number rule, the result is the negative of its substitution's
        // result.  For a fraction rule, it's the sum of its two
        // substitution results.  For a rule in a fraction rule set,
        // it's the numerator substitution's result divided by
        // the rule's base value.  Results from same-value substitutions
        // propagate back upard, and null substitutions don't affect
        // the result.
        tempResult = composeRuleValue(tempResult, baseValue);
        result.setDouble(tempResult);
        return TRUE;
        // if the parse was UNsuccessful, return 0
    } else {
        result.setLong(0);
        return FALSE;
    }
}

    /**
     * Returns true if this is a modulus substitution.  (We didn't do this
     * with instanceof partially because it causes source files to
     * proliferate and partially because we have to port this to C++.)
     * @return true if this object is an instance of ModulusSubstitution
     */
UBool
NFSubstitution::isModulusSubstitution() const {
    return FALSE;
}

//===================================================================
// SameValueSubstitution
//===================================================================

/**
 * A substitution that passes the value passed to it through unchanged.
 * Represented by == in rule descriptions.
 */
SameValueSubstitution::SameValueSubstitution(int32_t _pos,
                        const NFRuleSet* _ruleSet,
                        const UnicodeString& description,
                        UErrorCode& status)
: NFSubstitution(_pos, _ruleSet, description, status)
{
    if (0 == description.compare(gEqualsEquals, 2)) {
        // throw new IllegalArgumentException("== is not a legal token");
        status = U_PARSE_ERROR;
    }
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SameValueSubstitution)

//===================================================================
// MultiplierSubstitution
//===================================================================

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MultiplierSubstitution)

UBool MultiplierSubstitution::operator==(const NFSubstitution& rhs) const
{
    return NFSubstitution::operator==(rhs) &&
        divisor == ((const MultiplierSubstitution*)&rhs)->divisor;
}


//===================================================================
// ModulusSubstitution
//===================================================================

/**
 * A substitution that divides the number being formatted by the its rule's
 * divisor and formats the remainder.  Represented by "&gt;&gt;" in a
 * regular rule.
 */
ModulusSubstitution::ModulusSubstitution(int32_t _pos,
                                         double _divisor,
                                         const NFRule* predecessor,
                                         const NFRuleSet* _ruleSet,
                                         const UnicodeString& description,
                                         UErrorCode& status)
 : NFSubstitution(_pos, _ruleSet, description, status)
 , divisor(_divisor)
 , ruleToUse(NULL)
{
  ldivisor = util64_fromDouble(_divisor);

  // the owning rule's divisor controls the behavior of this
  // substitution: rather than keeping a backpointer to the rule,
  // we keep a copy of the divisor

  if (ldivisor == 0) {
      status = U_PARSE_ERROR;
  }

  if (0 == description.compare(gGreaterGreaterGreaterThan, 3)) {
    // the >>> token doesn't alter how this substituion calculates the
    // values it uses for formatting and parsing, but it changes
    // what's done with that value after it's obtained: >>> short-
    // circuits the rule-search process and goes straight to the
    // specified rule to format the substitution value
    ruleToUse = predecessor;
  }
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(ModulusSubstitution)

UBool ModulusSubstitution::operator==(const NFSubstitution& rhs) const
{
  return NFSubstitution::operator==(rhs) &&
  divisor == ((const ModulusSubstitution*)&rhs)->divisor &&
  ruleToUse == ((const ModulusSubstitution*)&rhs)->ruleToUse;
}

//-----------------------------------------------------------------------
// formatting
//-----------------------------------------------------------------------


/**
 * If this is a &gt;&gt;&gt; substitution, use ruleToUse to fill in
 * the substitution.  Otherwise, just use the superclass function.
 * @param number The number being formatted
 * @toInsertInto The string to insert the result of this substitution
 * into
 * @param pos The position of the rule text in toInsertInto
 */
void
ModulusSubstitution::doSubstitution(int64_t number, UnicodeString& toInsertInto, int32_t _pos, int32_t recursionCount, UErrorCode& status) const
{
    // if this isn't a >>> substitution, just use the inherited version
    // of this function (which uses either a rule set or a DecimalFormat
    // to format its substitution value)
    if (ruleToUse == NULL) {
        NFSubstitution::doSubstitution(number, toInsertInto, _pos, recursionCount, status);

        // a >>> substitution goes straight to a particular rule to
        // format the substitution value
    } else {
        int64_t numberToFormat = transformNumber(number);
        ruleToUse->doFormat(numberToFormat, toInsertInto, _pos + getPos(), recursionCount, status);
    }
}

/**
* If this is a &gt;&gt;&gt; substitution, use ruleToUse to fill in
* the substitution.  Otherwise, just use the superclass function.
* @param number The number being formatted
* @toInsertInto The string to insert the result of this substitution
* into
* @param pos The position of the rule text in toInsertInto
*/
void
ModulusSubstitution::doSubstitution(double number, UnicodeString& toInsertInto, int32_t _pos, int32_t recursionCount, UErrorCode& status) const
{
    // if this isn't a >>> substitution, just use the inherited version
    // of this function (which uses either a rule set or a DecimalFormat
    // to format its substitution value)
    if (ruleToUse == NULL) {
        NFSubstitution::doSubstitution(number, toInsertInto, _pos, recursionCount, status);

        // a >>> substitution goes straight to a particular rule to
        // format the substitution value
    } else {
        double numberToFormat = transformNumber(number);

        ruleToUse->doFormat(numberToFormat, toInsertInto, _pos + getPos(), recursionCount, status);
    }
}

//-----------------------------------------------------------------------
// parsing
//-----------------------------------------------------------------------

/**
 * If this is a &gt;&gt;&gt; substitution, match only against ruleToUse.
 * Otherwise, use the superclass function.
 * @param text The string to parse
 * @param parsePosition Ignored on entry, updated on exit to point to
 * the first unmatched character.
 * @param baseValue The partial parse result prior to calling this
 * routine.
 */
UBool
ModulusSubstitution::doParse(const UnicodeString& text,
                             ParsePosition& parsePosition,
                             double baseValue,
                             double upperBound,
                             UBool lenientParse,
                             Formattable& result) const
{
    // if this isn't a >>> substitution, we can just use the
    // inherited parse() routine to do the parsing
    if (ruleToUse == NULL) {
        return NFSubstitution::doParse(text, parsePosition, baseValue, upperBound, lenientParse, result);

        // but if it IS a >>> substitution, we have to do it here: we
        // use the specific rule's doParse() method, and then we have to
        // do some of the other work of NFRuleSet.parse()
    } else {
        ruleToUse->doParse(text, parsePosition, FALSE, upperBound, result);

        if (parsePosition.getIndex() != 0) {
            UErrorCode status = U_ZERO_ERROR;
            double tempResult = result.getDouble(status);
            tempResult = composeRuleValue(tempResult, baseValue);
            result.setDouble(tempResult);
        }

        return TRUE;
    }
}
/**
 * Returns a textual description of the substitution
 * @return A textual description of the substitution.  This might
 * not be identical to the description it was created from, but
 * it'll produce the same result.
 */
void
ModulusSubstitution::toString(UnicodeString& text) const
{
  // use tokenChar() to get the character at the beginning and
  // end of the substitutin token.  In between them will go
  // either the name of the rule set it uses, or the pattern of
  // the DecimalFormat it uses

  if ( ruleToUse != NULL ) { // Must have been a >>> substitution.
      text.remove();
      text.append(tokenChar());
      text.append(tokenChar());
      text.append(tokenChar());
  } else { // Otherwise just use the super-class function.
	  NFSubstitution::toString(text);
  }
}
//===================================================================
// IntegralPartSubstitution
//===================================================================

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IntegralPartSubstitution)


//===================================================================
// FractionalPartSubstitution
//===================================================================


    /**
     * Constructs a FractionalPartSubstitution.  This object keeps a flag
     * telling whether it should format by digits or not.  In addition,
     * it marks the rule set it calls (if any) as a fraction rule set.
     */
FractionalPartSubstitution::FractionalPartSubstitution(int32_t _pos,
                             const NFRuleSet* _ruleSet,
                             const UnicodeString& description,
                             UErrorCode& status)
 : NFSubstitution(_pos, _ruleSet, description, status)
 , byDigits(FALSE)
 , useSpaces(TRUE)

{
    // akk, ruleSet can change in superclass constructor
    if (0 == description.compare(gGreaterGreaterThan, 2) ||
        0 == description.compare(gGreaterGreaterGreaterThan, 3) ||
        _ruleSet == getRuleSet()) {
        byDigits = TRUE;
        if (0 == description.compare(gGreaterGreaterGreaterThan, 3)) {
            useSpaces = FALSE;
        }
    } else {
        // cast away const
        ((NFRuleSet*)getRuleSet())->makeIntoFractionRuleSet();
    }
}

//-----------------------------------------------------------------------
// formatting
//-----------------------------------------------------------------------

/**
 * If in "by digits" mode, fills in the substitution one decimal digit
 * at a time using the rule set containing this substitution.
 * Otherwise, uses the superclass function.
 * @param number The number being formatted
 * @param toInsertInto The string to insert the result of formatting
 * the substitution into
 * @param pos The position of the owning rule's rule text in
 * toInsertInto
 */
void
FractionalPartSubstitution::doSubstitution(double number, UnicodeString& toInsertInto,
                                           int32_t _pos, int32_t recursionCount, UErrorCode& status) const
{
  // if we're not in "byDigits" mode, just use the inherited
  // doSubstitution() routine
  if (!byDigits) {
    NFSubstitution::doSubstitution(number, toInsertInto, _pos, recursionCount, status);

    // if we're in "byDigits" mode, transform the value into an integer
    // by moving the decimal point eight places to the right and
    // pulling digits off the right one at a time, formatting each digit
    // as an integer using this substitution's owning rule set
    // (this is slower, but more accurate, than doing it from the
    // other end)
  } else {
    //          int32_t numberToFormat = (int32_t)uprv_round(transformNumber(number) * uprv_pow(10, kMaxDecimalDigits));
    //          // this flag keeps us from formatting trailing zeros.  It starts
    //          // out false because we're pulling from the right, and switches
    //          // to true the first time we encounter a non-zero digit
    //          UBool doZeros = FALSE;
    //          for (int32_t i = 0; i < kMaxDecimalDigits; i++) {
    //              int64_t digit = numberToFormat % 10;
    //              if (digit != 0 || doZeros) {
    //                  if (doZeros && useSpaces) {
    //                      toInsertInto.insert(_pos + getPos(), gSpace);
    //                  }
    //                  doZeros = TRUE;
    //                  getRuleSet()->format(digit, toInsertInto, _pos + getPos());
    //              }
    //              numberToFormat /= 10;
    //          }

    DigitList dl;
    dl.set(number);
    dl.roundFixedPoint(20);     // round to 20 fraction digits.
    dl.reduce();                // Removes any trailing zeros.

    UBool pad = FALSE;
    for (int32_t didx = dl.getCount()-1; didx>=dl.getDecimalAt(); didx--) {
      // Loop iterates over fraction digits, starting with the LSD.
      //   include both real digits from the number, and zeros
      //   to the left of the MSD but to the right of the decimal point.
      if (pad && useSpaces) {
        toInsertInto.insert(_pos + getPos(), gSpace);
      } else {
        pad = TRUE;
      }
      int64_t digit = didx>=0 ? dl.getDigit(didx) - '0' : 0;
      getRuleSet()->format(digit, toInsertInto, _pos + getPos(), recursionCount, status);
    }

    if (!pad) {
      // hack around lack of precision in digitlist. if we would end up with
      // "foo point" make sure we add a " zero" to the end.
      getRuleSet()->format((int64_t)0, toInsertInto, _pos + getPos(), recursionCount, status);
    }
  }
}

//-----------------------------------------------------------------------
// parsing
//-----------------------------------------------------------------------

/**
 * If in "by digits" mode, parses the string as if it were a string
 * of individual digits; otherwise, uses the superclass function.
 * @param text The string to parse
 * @param parsePosition Ignored on entry, but updated on exit to point
 * to the first unmatched character
 * @param baseValue The partial parse result prior to entering this
 * function
 * @param upperBound Only consider rules with base values lower than
 * this when filling in the substitution
 * @param lenientParse If true, try matching the text as numerals if
 * matching as words doesn't work
 * @return If the match was successful, the current partial parse
 * result; otherwise new Long(0).  The result is either a Long or
 * a Double.
 */

UBool
FractionalPartSubstitution::doParse(const UnicodeString& text,
                ParsePosition& parsePosition,
                double baseValue,
                double /*upperBound*/,
                UBool lenientParse,
                Formattable& resVal) const
{
    // if we're not in byDigits mode, we can just use the inherited
    // doParse()
    if (!byDigits) {
        return NFSubstitution::doParse(text, parsePosition, baseValue, 0, lenientParse, resVal);

        // if we ARE in byDigits mode, parse the text one digit at a time
        // using this substitution's owning rule set (we do this by setting
        // upperBound to 10 when calling doParse() ) until we reach
        // nonmatching text
    } else {
        UnicodeString workText(text);
        ParsePosition workPos(1);
        double result = 0;
        int32_t digit;
//          double p10 = 0.1;

        DigitList dl;
        NumberFormat* fmt = NULL;
        while (workText.length() > 0 && workPos.getIndex() != 0) {
            workPos.setIndex(0);
            Formattable temp;
            getRuleSet()->parse(workText, workPos, 10, temp);
            UErrorCode status = U_ZERO_ERROR;
            digit = temp.getLong(status);
//            digit = temp.getType() == Formattable::kLong ?
//               temp.getLong() :
//            (int32_t)temp.getDouble();

            if (lenientParse && workPos.getIndex() == 0) {
                if (!fmt) {
                    status = U_ZERO_ERROR;
                    fmt = NumberFormat::createInstance(status);
                    if (U_FAILURE(status)) {
                        delete fmt;
                        fmt = NULL;
                    }
                }
                if (fmt) {
                    fmt->parse(workText, temp, workPos);
                    digit = temp.getLong(status);
                }
            }

            if (workPos.getIndex() != 0) {
                dl.append((char)('0' + digit));
//                  result += digit * p10;
//                  p10 /= 10;
                parsePosition.setIndex(parsePosition.getIndex() + workPos.getIndex());
                workText.removeBetween(0, workPos.getIndex());
                while (workText.length() > 0 && workText.charAt(0) == gSpace) {
                    workText.removeBetween(0, 1);
                    parsePosition.setIndex(parsePosition.getIndex() + 1);
                }
            }
        }
        delete fmt;

        result = dl.getCount() == 0 ? 0 : dl.getDouble();
        result = composeRuleValue(result, baseValue);
        resVal.setDouble(result);
        return TRUE;
    }
}

UBool
FractionalPartSubstitution::operator==(const NFSubstitution& rhs) const
{
  return NFSubstitution::operator==(rhs) &&
  ((const FractionalPartSubstitution*)&rhs)->byDigits == byDigits;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FractionalPartSubstitution)


//===================================================================
// AbsoluteValueSubstitution
//===================================================================

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(AbsoluteValueSubstitution)

//===================================================================
// NumeratorSubstitution
//===================================================================

void
NumeratorSubstitution::doSubstitution(double number, UnicodeString& toInsertInto, int32_t apos, int32_t recursionCount, UErrorCode& status) const {
    // perform a transformation on the number being formatted that
    // is dependent on the type of substitution this is

    double numberToFormat = transformNumber(number);
    int64_t longNF = util64_fromDouble(numberToFormat);

    const NFRuleSet* aruleSet = getRuleSet();
    if (withZeros && aruleSet != NULL) {
        // if there are leading zeros in the decimal expansion then emit them
        int64_t nf =longNF;
        int32_t len = toInsertInto.length();
        while ((nf *= 10) < denominator) {
            toInsertInto.insert(apos + getPos(), gSpace);
            aruleSet->format((int64_t)0, toInsertInto, apos + getPos(), recursionCount, status);
        }
        apos += toInsertInto.length() - len;
    }

    // if the result is an integer, from here on out we work in integer
    // space (saving time and memory and preserving accuracy)
    if (numberToFormat == longNF && aruleSet != NULL) {
        aruleSet->format(longNF, toInsertInto, apos + getPos(), recursionCount, status);

        // if the result isn't an integer, then call either our rule set's
        // format() method or our DecimalFormat's format() method to
        // format the result
    } else {
        if (aruleSet != NULL) {
            aruleSet->format(numberToFormat, toInsertInto, apos + getPos(), recursionCount, status);
        } else {
            UnicodeString temp;
            getNumberFormat()->format(numberToFormat, temp, status);
            toInsertInto.insert(apos + getPos(), temp);
        }
    }
}

UBool
NumeratorSubstitution::doParse(const UnicodeString& text,
                               ParsePosition& parsePosition,
                               double baseValue,
                               double upperBound,
                               UBool /*lenientParse*/,
                               Formattable& result) const
{
    // we don't have to do anything special to do the parsing here,
    // but we have to turn lenient parsing off-- if we leave it on,
    // it SERIOUSLY messes up the algorithm

    // if withZeros is true, we need to count the zeros
    // and use that to adjust the parse result
    UErrorCode status = U_ZERO_ERROR;
    int32_t zeroCount = 0;
    UnicodeString workText(text);

    if (withZeros) {
        ParsePosition workPos(1);
        Formattable temp;

        while (workText.length() > 0 && workPos.getIndex() != 0) {
            workPos.setIndex(0);
            getRuleSet()->parse(workText, workPos, 1, temp); // parse zero or nothing at all
            if (workPos.getIndex() == 0) {
                // we failed, either there were no more zeros, or the number was formatted with digits
                // either way, we're done
                break;
            }

            ++zeroCount;
            parsePosition.setIndex(parsePosition.getIndex() + workPos.getIndex());
            workText.remove(0, workPos.getIndex());
            while (workText.length() > 0 && workText.charAt(0) == gSpace) {
                workText.remove(0, 1);
                parsePosition.setIndex(parsePosition.getIndex() + 1);
            }
        }

        workText = text;
        workText.remove(0, (int32_t)parsePosition.getIndex());
        parsePosition.setIndex(0);
    }

    // we've parsed off the zeros, now let's parse the rest from our current position
    NFSubstitution::doParse(workText, parsePosition, withZeros ? 1 : baseValue, upperBound, FALSE, result);

    if (withZeros) {
        // any base value will do in this case.  is there a way to
        // force this to not bother trying all the base values?

        // compute the 'effective' base and prescale the value down
        int64_t n = result.getLong(status); // force conversion!
        int64_t d = 1;
        int32_t pow = 0;
        while (d <= n) {
            d *= 10;
            ++pow;
        }
        // now add the zeros
        while (zeroCount > 0) {
            d *= 10;
            --zeroCount;
        }
        // d is now our true denominator
        result.setDouble((double)n/(double)d);
    }

    return TRUE;
}

UBool
NumeratorSubstitution::operator==(const NFSubstitution& rhs) const
{
    return NFSubstitution::operator==(rhs) &&
        denominator == ((const NumeratorSubstitution*)&rhs)->denominator;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(NumeratorSubstitution)

const UChar NumeratorSubstitution::LTLT[] = { 0x003c, 0x003c };

U_NAMESPACE_END

/* U_HAVE_RBNF */
#endif

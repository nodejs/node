// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File plurrule.cpp
*/

#include <math.h>
#include <stdio.h>

#include "unicode/utypes.h"
#include "unicode/localpointer.h"
#include "unicode/plurrule.h"
#include "unicode/upluralrules.h"
#include "unicode/ures.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "digitlst.h"
#include "hash.h"
#include "locutil.h"
#include "mutex.h"
#include "patternprops.h"
#include "plurrule_impl.h"
#include "putilimp.h"
#include "ucln_in.h"
#include "ustrfmt.h"
#include "uassert.h"
#include "uvectr32.h"
#include "sharedpluralrules.h"
#include "unifiedcache.h"
#include "digitinterval.h" 
#include "visibledigits.h"


#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

static const UChar PLURAL_KEYWORD_OTHER[]={LOW_O,LOW_T,LOW_H,LOW_E,LOW_R,0};
static const UChar PLURAL_DEFAULT_RULE[]={LOW_O,LOW_T,LOW_H,LOW_E,LOW_R,COLON,SPACE,LOW_N,0};
static const UChar PK_IN[]={LOW_I,LOW_N,0};
static const UChar PK_NOT[]={LOW_N,LOW_O,LOW_T,0};
static const UChar PK_IS[]={LOW_I,LOW_S,0};
static const UChar PK_MOD[]={LOW_M,LOW_O,LOW_D,0};
static const UChar PK_AND[]={LOW_A,LOW_N,LOW_D,0};
static const UChar PK_OR[]={LOW_O,LOW_R,0};
static const UChar PK_VAR_N[]={LOW_N,0};
static const UChar PK_VAR_I[]={LOW_I,0};
static const UChar PK_VAR_F[]={LOW_F,0};
static const UChar PK_VAR_T[]={LOW_T,0};
static const UChar PK_VAR_V[]={LOW_V,0};
static const UChar PK_WITHIN[]={LOW_W,LOW_I,LOW_T,LOW_H,LOW_I,LOW_N,0};
static const UChar PK_DECIMAL[]={LOW_D,LOW_E,LOW_C,LOW_I,LOW_M,LOW_A,LOW_L,0};
static const UChar PK_INTEGER[]={LOW_I,LOW_N,LOW_T,LOW_E,LOW_G,LOW_E,LOW_R,0};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PluralRules)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PluralKeywordEnumeration)

PluralRules::PluralRules(UErrorCode& /*status*/)
:   UObject(),
    mRules(NULL)
{
}

PluralRules::PluralRules(const PluralRules& other)
: UObject(other),
    mRules(NULL)
{
    *this=other;
}

PluralRules::~PluralRules() {
    delete mRules;
}

SharedPluralRules::~SharedPluralRules() {
    delete ptr;
}

PluralRules*
PluralRules::clone() const {
    return new PluralRules(*this);
}

PluralRules&
PluralRules::operator=(const PluralRules& other) {
    if (this != &other) {
        delete mRules;
        if (other.mRules==NULL) {
            mRules = NULL;
        }
        else {
            mRules = new RuleChain(*other.mRules);
        }
    }

    return *this;
}

StringEnumeration* PluralRules::getAvailableLocales(UErrorCode &status) {
    StringEnumeration *result = new PluralAvailableLocalesEnumeration(status);
    if (result == NULL && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        delete result;
        result = NULL;
    }
    return result;
}


PluralRules* U_EXPORT2
PluralRules::createRules(const UnicodeString& description, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }

    PluralRuleParser parser;
    PluralRules *newRules = new PluralRules(status);
    if (U_SUCCESS(status) && newRules == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    parser.parse(description, newRules, status);
    if (U_FAILURE(status)) {
        delete newRules;
        newRules = NULL;
    }
    return newRules;
}


PluralRules* U_EXPORT2
PluralRules::createDefaultRules(UErrorCode& status) {
    return createRules(UnicodeString(TRUE, PLURAL_DEFAULT_RULE, -1), status);
}

/******************************************************************************/
/* Create PluralRules cache */

template<> U_I18N_API
const SharedPluralRules *LocaleCacheKey<SharedPluralRules>::createObject(
        const void * /*unused*/, UErrorCode &status) const {
    const char *localeId = fLoc.getName();
    PluralRules *pr = PluralRules::internalForLocale(
            localeId, UPLURAL_TYPE_CARDINAL, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    SharedPluralRules *result = new SharedPluralRules(pr);
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        delete pr;
        return NULL;
    }
    result->addRef();
    return result;
}

/* end plural rules cache */
/******************************************************************************/

const SharedPluralRules* U_EXPORT2
PluralRules::createSharedInstance(
        const Locale& locale, UPluralType type, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (type != UPLURAL_TYPE_CARDINAL) {
        status = U_UNSUPPORTED_ERROR;
        return NULL;
    }
    const SharedPluralRules *result = NULL;
    UnifiedCache::getByLocale(locale, result, status);
    return result;
}

PluralRules* U_EXPORT2
PluralRules::forLocale(const Locale& locale, UErrorCode& status) {
    return forLocale(locale, UPLURAL_TYPE_CARDINAL, status);
}

PluralRules* U_EXPORT2
PluralRules::forLocale(const Locale& locale, UPluralType type, UErrorCode& status) {
    if (type != UPLURAL_TYPE_CARDINAL) {
        return internalForLocale(locale, type, status);
    }
    const SharedPluralRules *shared = createSharedInstance(
            locale, type, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    PluralRules *result = (*shared)->clone();
    shared->removeRef();
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

PluralRules* U_EXPORT2
PluralRules::internalForLocale(const Locale& locale, UPluralType type, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (type >= UPLURAL_TYPE_COUNT) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    PluralRules *newObj = new PluralRules(status);
    if (newObj==NULL || U_FAILURE(status)) {
        delete newObj;
        return NULL;
    }
    UnicodeString locRule = newObj->getRuleFromResource(locale, type, status);
    // TODO: which errors, if any, should be returned?
    if (locRule.length() == 0) {
        // Locales with no specific rules (all numbers have the "other" category
        //   will return a U_MISSING_RESOURCE_ERROR at this point. This is not
        //   an error.
        locRule =  UnicodeString(PLURAL_DEFAULT_RULE);
        status = U_ZERO_ERROR;
    }
    PluralRuleParser parser;
    parser.parse(locRule, newObj, status);
        //  TODO: should rule parse errors be returned, or
        //        should we silently use default rules?
        //        Original impl used default rules.
        //        Ask the question to ICU Core.

    return newObj;
}

UnicodeString
PluralRules::select(int32_t number) const {
    return select(FixedDecimal(number));
}

UnicodeString
PluralRules::select(double number) const {
    return select(FixedDecimal(number));
}

UnicodeString
PluralRules::select(const FixedDecimal &number) const {
    if (mRules == NULL) {
        return UnicodeString(TRUE, PLURAL_DEFAULT_RULE, -1);
    }
    else {
        return mRules->select(number);
    }
}

UnicodeString
PluralRules::select(const VisibleDigitsWithExponent &number) const {
    if (number.getExponent() != NULL) {
        return UnicodeString(TRUE, PLURAL_DEFAULT_RULE, -1);
    }
    return select(FixedDecimal(number.getMantissa()));
}



StringEnumeration*
PluralRules::getKeywords(UErrorCode& status) const {
    if (U_FAILURE(status))  return NULL;
    StringEnumeration* nameEnumerator = new PluralKeywordEnumeration(mRules, status);
    if (U_FAILURE(status)) {
      delete nameEnumerator;
      return NULL;
    }

    return nameEnumerator;
}

double
PluralRules::getUniqueKeywordValue(const UnicodeString& /* keyword */) {
  // Not Implemented.
  return UPLRULES_NO_UNIQUE_VALUE;
}

int32_t
PluralRules::getAllKeywordValues(const UnicodeString & /* keyword */, double * /* dest */,
                                 int32_t /* destCapacity */, UErrorCode& error) {
    error = U_UNSUPPORTED_ERROR;
    return 0;
}

    
static double scaleForInt(double d) {
    double scale = 1.0;
    while (d != floor(d)) {
        d = d * 10.0;
        scale = scale * 10.0;
    }
    return scale;
}

static int32_t
getSamplesFromString(const UnicodeString &samples, double *dest,
                        int32_t destCapacity, UErrorCode& status) {
    int32_t sampleCount = 0;
    int32_t sampleStartIdx = 0;
    int32_t sampleEndIdx = 0;

    //std::string ss;  // TODO: debugging.
    // std::cout << "PluralRules::getSamples(), samples = \"" << samples.toUTF8String(ss) << "\"\n";
    for (sampleCount = 0; sampleCount < destCapacity && sampleStartIdx < samples.length(); ) {
        sampleEndIdx = samples.indexOf(COMMA, sampleStartIdx);
        if (sampleEndIdx == -1) {
            sampleEndIdx = samples.length();
        }
        const UnicodeString &sampleRange = samples.tempSubStringBetween(sampleStartIdx, sampleEndIdx);
        // ss.erase();
        // std::cout << "PluralRules::getSamples(), samplesRange = \"" << sampleRange.toUTF8String(ss) << "\"\n";
        int32_t tildeIndex = sampleRange.indexOf(TILDE);
        if (tildeIndex < 0) {
            FixedDecimal fixed(sampleRange, status);
            double sampleValue = fixed.source;
            if (fixed.visibleDecimalDigitCount == 0 || sampleValue != floor(sampleValue)) {
                dest[sampleCount++] = sampleValue;
            }
        } else {
            
            FixedDecimal fixedLo(sampleRange.tempSubStringBetween(0, tildeIndex), status);
            FixedDecimal fixedHi(sampleRange.tempSubStringBetween(tildeIndex+1), status);
            double rangeLo = fixedLo.source;
            double rangeHi = fixedHi.source;
            if (U_FAILURE(status)) {
                break;
            }
            if (rangeHi < rangeLo) {
                status = U_INVALID_FORMAT_ERROR;
                break;
            }

            // For ranges of samples with fraction decimal digits, scale the number up so that we
            //   are adding one in the units place. Avoids roundoffs from repetitive adds of tenths.

            double scale = scaleForInt(rangeLo); 
            double t = scaleForInt(rangeHi);
            if (t > scale) {
                scale = t;
            }
            rangeLo *= scale;
            rangeHi *= scale;
            for (double n=rangeLo; n<=rangeHi; n+=1) {
                // Hack Alert: don't return any decimal samples with integer values that
                //    originated from a format with trailing decimals.
                //    This API is returning doubles, which can't distinguish having displayed
                //    zeros to the right of the decimal.
                //    This results in test failures with values mapping back to a different keyword.
                double sampleValue = n/scale;
                if (!(sampleValue == floor(sampleValue) && fixedLo.visibleDecimalDigitCount > 0)) {
                    dest[sampleCount++] = sampleValue;
                }
                if (sampleCount >= destCapacity) {
                    break;
                }
            }
        }
        sampleStartIdx = sampleEndIdx + 1;
    }
    return sampleCount;
}


int32_t
PluralRules::getSamples(const UnicodeString &keyword, double *dest,
                        int32_t destCapacity, UErrorCode& status) {
    RuleChain *rc = rulesForKeyword(keyword);
    if (rc == NULL || destCapacity == 0 || U_FAILURE(status)) {
        return 0;
    }
    int32_t numSamples = getSamplesFromString(rc->fIntegerSamples, dest, destCapacity, status);
    if (numSamples == 0) { 
        numSamples = getSamplesFromString(rc->fDecimalSamples, dest, destCapacity, status);
    }
    return numSamples;
}
    

RuleChain *PluralRules::rulesForKeyword(const UnicodeString &keyword) const {
    RuleChain *rc;
    for (rc = mRules; rc != NULL; rc = rc->fNext) {
        if (rc->fKeyword == keyword) {
            break;
        }
    }
    return rc;
}


UBool
PluralRules::isKeyword(const UnicodeString& keyword) const {
    if (0 == keyword.compare(PLURAL_KEYWORD_OTHER, 5)) {
        return true;
    }
    return rulesForKeyword(keyword) != NULL;
}

UnicodeString
PluralRules::getKeywordOther() const {
    return UnicodeString(TRUE, PLURAL_KEYWORD_OTHER, 5);
}

UBool
PluralRules::operator==(const PluralRules& other) const  {
    const UnicodeString *ptrKeyword;
    UErrorCode status= U_ZERO_ERROR;

    if ( this == &other ) {
        return TRUE;
    }
    LocalPointer<StringEnumeration> myKeywordList(getKeywords(status));
    LocalPointer<StringEnumeration> otherKeywordList(other.getKeywords(status));
    if (U_FAILURE(status)) {
        return FALSE;
    }

    if (myKeywordList->count(status)!=otherKeywordList->count(status)) {
        return FALSE;
    }
    myKeywordList->reset(status);
    while ((ptrKeyword=myKeywordList->snext(status))!=NULL) {
        if (!other.isKeyword(*ptrKeyword)) {
            return FALSE;
        }
    }
    otherKeywordList->reset(status);
    while ((ptrKeyword=otherKeywordList->snext(status))!=NULL) {
        if (!this->isKeyword(*ptrKeyword)) {
            return FALSE;
        }
    }
    if (U_FAILURE(status)) {
        return FALSE;
    }

    return TRUE;
}


void
PluralRuleParser::parse(const UnicodeString& ruleData, PluralRules *prules, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(ruleIndex == 0);    // Parsers are good for a single use only!
    ruleSrc = &ruleData;

    while (ruleIndex< ruleSrc->length()) {
        getNextToken(status);
        if (U_FAILURE(status)) {
            return;
        }
        checkSyntax(status);
        if (U_FAILURE(status)) {
            return;
        }
        switch (type) {
        case tAnd:
            U_ASSERT(curAndConstraint != NULL);
            curAndConstraint = curAndConstraint->add();
            break;
        case tOr:
            {
                U_ASSERT(currentChain != NULL);
                OrConstraint *orNode=currentChain->ruleHeader;
                while (orNode->next != NULL) {
                    orNode = orNode->next;
                }
                orNode->next= new OrConstraint();
                orNode=orNode->next;
                orNode->next=NULL;
                curAndConstraint = orNode->add();
            }
            break;
        case tIs:
            U_ASSERT(curAndConstraint != NULL);
            U_ASSERT(curAndConstraint->value == -1);
            U_ASSERT(curAndConstraint->rangeList == NULL);
            break;
        case tNot:
            U_ASSERT(curAndConstraint != NULL);
            curAndConstraint->negated=TRUE;
            break;

        case tNotEqual:
            curAndConstraint->negated=TRUE;
            U_FALLTHROUGH;
        case tIn:
        case tWithin:
        case tEqual:
            U_ASSERT(curAndConstraint != NULL);
            curAndConstraint->rangeList = new UVector32(status);
            curAndConstraint->rangeList->addElement(-1, status);  // range Low
            curAndConstraint->rangeList->addElement(-1, status);  // range Hi
            rangeLowIdx = 0;
            rangeHiIdx  = 1;
            curAndConstraint->value=PLURAL_RANGE_HIGH;
            curAndConstraint->integerOnly = (type != tWithin);
            break;
        case tNumber:
            U_ASSERT(curAndConstraint != NULL);
            if ( (curAndConstraint->op==AndConstraint::MOD)&&
                 (curAndConstraint->opNum == -1 ) ) {
                curAndConstraint->opNum=getNumberValue(token);
            }
            else {
                if (curAndConstraint->rangeList == NULL) {
                    // this is for an 'is' rule
                    curAndConstraint->value = getNumberValue(token);
                } else {
                    // this is for an 'in' or 'within' rule
                    if (curAndConstraint->rangeList->elementAti(rangeLowIdx) == -1) {
                        curAndConstraint->rangeList->setElementAt(getNumberValue(token), rangeLowIdx);
                        curAndConstraint->rangeList->setElementAt(getNumberValue(token), rangeHiIdx);
                    }
                    else {
                        curAndConstraint->rangeList->setElementAt(getNumberValue(token), rangeHiIdx);
                        if (curAndConstraint->rangeList->elementAti(rangeLowIdx) > 
                                curAndConstraint->rangeList->elementAti(rangeHiIdx)) {
                            // Range Lower bound > Range Upper bound.
                            // U_UNEXPECTED_TOKEN seems a little funny, but it is consistently
                            // used for all plural rule parse errors.
                            status = U_UNEXPECTED_TOKEN;
                            break;
                        }
                    }
                }
            }
            break;
        case tComma:
            // TODO: rule syntax checking is inadequate, can happen with badly formed rules.
            //       Catch cases like "n mod 10, is 1" here instead.
            if (curAndConstraint == NULL || curAndConstraint->rangeList == NULL) {
                status = U_UNEXPECTED_TOKEN;
                break;
            }
            U_ASSERT(curAndConstraint->rangeList->size() >= 2);
            rangeLowIdx = curAndConstraint->rangeList->size();
            curAndConstraint->rangeList->addElement(-1, status);  // range Low
            rangeHiIdx = curAndConstraint->rangeList->size();
            curAndConstraint->rangeList->addElement(-1, status);  // range Hi
            break;
        case tMod:
            U_ASSERT(curAndConstraint != NULL);
            curAndConstraint->op=AndConstraint::MOD;
            break;
        case tVariableN:
        case tVariableI:
        case tVariableF:
        case tVariableT:
        case tVariableV:
            U_ASSERT(curAndConstraint != NULL);
            curAndConstraint->digitsType = type;
            break;
        case tKeyword:
            {
            RuleChain *newChain = new RuleChain;
            if (newChain == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                break;
            }
            newChain->fKeyword = token;
            if (prules->mRules == NULL) {
                prules->mRules = newChain;
            } else {
                // The new rule chain goes at the end of the linked list of rule chains,
                //   unless there is an "other" keyword & chain. "other" must remain last.
                RuleChain *insertAfter = prules->mRules;
                while (insertAfter->fNext!=NULL && 
                       insertAfter->fNext->fKeyword.compare(PLURAL_KEYWORD_OTHER, 5) != 0 ){
                    insertAfter=insertAfter->fNext;
                }
                newChain->fNext = insertAfter->fNext;
                insertAfter->fNext = newChain;
            }
            OrConstraint *orNode = new OrConstraint();
            newChain->ruleHeader = orNode;
            curAndConstraint = orNode->add();
            currentChain = newChain;
            }
            break;

        case tInteger:
            for (;;) {
                getNextToken(status);
                if (U_FAILURE(status) || type == tSemiColon || type == tEOF || type == tAt) {
                    break;
                }
                if (type == tEllipsis) {
                    currentChain->fIntegerSamplesUnbounded = TRUE;
                    continue;
                }
                currentChain->fIntegerSamples.append(token);
            }
            break;

        case tDecimal:
            for (;;) {
                getNextToken(status);
                if (U_FAILURE(status) || type == tSemiColon || type == tEOF || type == tAt) {
                    break;
                }
                if (type == tEllipsis) {
                    currentChain->fDecimalSamplesUnbounded = TRUE;
                    continue;
                }
                currentChain->fDecimalSamples.append(token);
            }
            break;
                
        default:
            break;
        }
        prevType=type;
        if (U_FAILURE(status)) {
            break;
        }
    }
}

UnicodeString
PluralRules::getRuleFromResource(const Locale& locale, UPluralType type, UErrorCode& errCode) {
    UnicodeString emptyStr;

    if (U_FAILURE(errCode)) {
        return emptyStr;
    }
    LocalUResourceBundlePointer rb(ures_openDirect(NULL, "plurals", &errCode));
    if(U_FAILURE(errCode)) {
        return emptyStr;
    }
    const char *typeKey;
    switch (type) {
    case UPLURAL_TYPE_CARDINAL:
        typeKey = "locales";
        break;
    case UPLURAL_TYPE_ORDINAL:
        typeKey = "locales_ordinals";
        break;
    default:
        // Must not occur: The caller should have checked for valid types.
        errCode = U_ILLEGAL_ARGUMENT_ERROR;
        return emptyStr;
    }
    LocalUResourceBundlePointer locRes(ures_getByKey(rb.getAlias(), typeKey, NULL, &errCode));
    if(U_FAILURE(errCode)) {
        return emptyStr;
    }
    int32_t resLen=0;
    const char *curLocaleName=locale.getName();
    const UChar* s = ures_getStringByKey(locRes.getAlias(), curLocaleName, &resLen, &errCode);

    if (s == NULL) {
        // Check parent locales.
        UErrorCode status = U_ZERO_ERROR;
        char parentLocaleName[ULOC_FULLNAME_CAPACITY];
        const char *curLocaleName=locale.getName();
        uprv_strcpy(parentLocaleName, curLocaleName);

        while (uloc_getParent(parentLocaleName, parentLocaleName,
                                       ULOC_FULLNAME_CAPACITY, &status) > 0) {
            resLen=0;
            s = ures_getStringByKey(locRes.getAlias(), parentLocaleName, &resLen, &status);
            if (s != NULL) {
                errCode = U_ZERO_ERROR;
                break;
            }
            status = U_ZERO_ERROR;
        }
    }
    if (s==NULL) {
        return emptyStr;
    }

    char setKey[256];
    u_UCharsToChars(s, setKey, resLen + 1);
    // printf("\n PluralRule: %s\n", setKey);

    LocalUResourceBundlePointer ruleRes(ures_getByKey(rb.getAlias(), "rules", NULL, &errCode));
    if(U_FAILURE(errCode)) {
        return emptyStr;
    }
    LocalUResourceBundlePointer setRes(ures_getByKey(ruleRes.getAlias(), setKey, NULL, &errCode));
    if (U_FAILURE(errCode)) {
        return emptyStr;
    }

    int32_t numberKeys = ures_getSize(setRes.getAlias());
    UnicodeString result;
    const char *key=NULL;
    for(int32_t i=0; i<numberKeys; ++i) {   // Keys are zero, one, few, ...
        UnicodeString rules = ures_getNextUnicodeString(setRes.getAlias(), &key, &errCode);
        UnicodeString uKey(key, -1, US_INV);
        result.append(uKey);
        result.append(COLON);
        result.append(rules);
        result.append(SEMI_COLON);
    }
    return result;
}


UnicodeString
PluralRules::getRules() const {
    UnicodeString rules;
    if (mRules != NULL) {
        mRules->dumpRules(rules);
    }
    return rules;
}


AndConstraint::AndConstraint() {
    op = AndConstraint::NONE;
    opNum=-1;
    value = -1;
    rangeList = NULL;
    negated = FALSE;
    integerOnly = FALSE;
    digitsType = none;
    next=NULL;
}


AndConstraint::AndConstraint(const AndConstraint& other) {
    this->op = other.op;
    this->opNum=other.opNum;
    this->value=other.value;
    this->rangeList=NULL;
    if (other.rangeList != NULL) {
        UErrorCode status = U_ZERO_ERROR;
        this->rangeList = new UVector32(status);
        this->rangeList->assign(*other.rangeList, status);
    }
    this->integerOnly=other.integerOnly;
    this->negated=other.negated;
    this->digitsType = other.digitsType;
    if (other.next==NULL) {
        this->next=NULL;
    }
    else {
        this->next = new AndConstraint(*other.next);
    }
}

AndConstraint::~AndConstraint() {
    delete rangeList;
    if (next!=NULL) {
        delete next;
    }
}


UBool
AndConstraint::isFulfilled(const FixedDecimal &number) {
    UBool result = TRUE;
    if (digitsType == none) {
        // An empty AndConstraint, created by a rule with a keyword but no following expression.
        return TRUE;
    }
    double n = number.get(digitsType);  // pulls n | i | v | f value for the number.
                                        // Will always be positive.
                                        // May be non-integer (n option only)
    do {
        if (integerOnly && n != uprv_floor(n)) {
            result = FALSE;
            break;
        }

        if (op == MOD) {
            n = fmod(n, opNum);
        }
        if (rangeList == NULL) {
            result = value == -1 ||    // empty rule
                     n == value;       //  'is' rule
            break;
        }
        result = FALSE;                // 'in' or 'within' rule
        for (int32_t r=0; r<rangeList->size(); r+=2) {
            if (rangeList->elementAti(r) <= n && n <= rangeList->elementAti(r+1)) {
                result = TRUE;
                break;
            }
        }
    } while (FALSE);

    if (negated) {
        result = !result;
    }
    return result;
}


AndConstraint*
AndConstraint::add()
{
    this->next = new AndConstraint();
    return this->next;
}

OrConstraint::OrConstraint() {
    childNode=NULL;
    next=NULL;
}

OrConstraint::OrConstraint(const OrConstraint& other) {
    if ( other.childNode == NULL ) {
        this->childNode = NULL;
    }
    else {
        this->childNode = new AndConstraint(*(other.childNode));
    }
    if (other.next == NULL ) {
        this->next = NULL;
    }
    else {
        this->next = new OrConstraint(*(other.next));
    }
}

OrConstraint::~OrConstraint() {
    if (childNode!=NULL) {
        delete childNode;
    }
    if (next!=NULL) {
        delete next;
    }
}

AndConstraint*
OrConstraint::add()
{
    OrConstraint *curOrConstraint=this;
    {
        while (curOrConstraint->next!=NULL) {
            curOrConstraint = curOrConstraint->next;
        }
        U_ASSERT(curOrConstraint->childNode == NULL);
        curOrConstraint->childNode = new AndConstraint();
    }
    return curOrConstraint->childNode;
}

UBool
OrConstraint::isFulfilled(const FixedDecimal &number) {
    OrConstraint* orRule=this;
    UBool result=FALSE;

    while (orRule!=NULL && !result) {
        result=TRUE;
        AndConstraint* andRule = orRule->childNode;
        while (andRule!=NULL && result) {
            result = andRule->isFulfilled(number);
            andRule=andRule->next;
        }
        orRule = orRule->next;
    }

    return result;
}


RuleChain::RuleChain(): fKeyword(), fNext(NULL), ruleHeader(NULL), fDecimalSamples(), fIntegerSamples(), 
                        fDecimalSamplesUnbounded(FALSE), fIntegerSamplesUnbounded(FALSE) {
}

RuleChain::RuleChain(const RuleChain& other) : 
        fKeyword(other.fKeyword), fNext(NULL), ruleHeader(NULL), fDecimalSamples(other.fDecimalSamples),
        fIntegerSamples(other.fIntegerSamples), fDecimalSamplesUnbounded(other.fDecimalSamplesUnbounded), 
        fIntegerSamplesUnbounded(other.fIntegerSamplesUnbounded) {
    if (other.ruleHeader != NULL) {
        this->ruleHeader = new OrConstraint(*(other.ruleHeader));
    }
    if (other.fNext != NULL ) {
        this->fNext = new RuleChain(*other.fNext);
    }
}

RuleChain::~RuleChain() {
    delete fNext;
    delete ruleHeader;
}


UnicodeString
RuleChain::select(const FixedDecimal &number) const {
    if (!number.isNanOrInfinity) {
        for (const RuleChain *rules = this; rules != NULL; rules = rules->fNext) {
             if (rules->ruleHeader->isFulfilled(number)) {
                 return rules->fKeyword;
             }
        }
    }
    return UnicodeString(TRUE, PLURAL_KEYWORD_OTHER, 5);
}

static UnicodeString tokenString(tokenType tok) {
    UnicodeString s;
    switch (tok) {
      case tVariableN:
        s.append(LOW_N); break;
      case tVariableI:
        s.append(LOW_I); break;
      case tVariableF:
        s.append(LOW_F); break;
      case tVariableV:
        s.append(LOW_V); break;
      case tVariableT:
        s.append(LOW_T); break;
      default:
        s.append(TILDE);
    }
    return s;
}

void
RuleChain::dumpRules(UnicodeString& result) {
    UChar digitString[16];

    if ( ruleHeader != NULL ) {
        result +=  fKeyword;
        result += COLON;
        result += SPACE;
        OrConstraint* orRule=ruleHeader;
        while ( orRule != NULL ) {
            AndConstraint* andRule=orRule->childNode;
            while ( andRule != NULL ) {
                if ((andRule->op==AndConstraint::NONE) &&  (andRule->rangeList==NULL) && (andRule->value == -1)) {
                    // Empty Rules.
                } else if ( (andRule->op==AndConstraint::NONE) && (andRule->rangeList==NULL) ) {
                    result += tokenString(andRule->digitsType);
                    result += UNICODE_STRING_SIMPLE(" is ");
                    if (andRule->negated) {
                        result += UNICODE_STRING_SIMPLE("not ");
                    }
                    uprv_itou(digitString,16, andRule->value,10,0);
                    result += UnicodeString(digitString);
                }
                else {
                    result += tokenString(andRule->digitsType);
                    result += SPACE;
                    if (andRule->op==AndConstraint::MOD) {
                        result += UNICODE_STRING_SIMPLE("mod ");
                        uprv_itou(digitString,16, andRule->opNum,10,0);
                        result += UnicodeString(digitString);
                    }
                    if (andRule->rangeList==NULL) {
                        if (andRule->negated) {
                            result += UNICODE_STRING_SIMPLE(" is not ");
                            uprv_itou(digitString,16, andRule->value,10,0);
                            result += UnicodeString(digitString);
                        }
                        else {
                            result += UNICODE_STRING_SIMPLE(" is ");
                            uprv_itou(digitString,16, andRule->value,10,0);
                            result += UnicodeString(digitString);
                        }
                    }
                    else {
                        if (andRule->negated) {
                            if ( andRule->integerOnly ) {
                                result += UNICODE_STRING_SIMPLE(" not in ");
                            }
                            else {
                                result += UNICODE_STRING_SIMPLE(" not within ");
                            }
                        }
                        else {
                            if ( andRule->integerOnly ) {
                                result += UNICODE_STRING_SIMPLE(" in ");
                            }
                            else {
                                result += UNICODE_STRING_SIMPLE(" within ");
                            }
                        }
                        for (int32_t r=0; r<andRule->rangeList->size(); r+=2) {
                            int32_t rangeLo = andRule->rangeList->elementAti(r);
                            int32_t rangeHi = andRule->rangeList->elementAti(r+1);
                            uprv_itou(digitString,16, rangeLo, 10, 0);
                            result += UnicodeString(digitString);
                            result += UNICODE_STRING_SIMPLE("..");
                            uprv_itou(digitString,16, rangeHi, 10,0);
                            result += UnicodeString(digitString);
                            if (r+2 < andRule->rangeList->size()) {
                                result += UNICODE_STRING_SIMPLE(", ");
                            }
                        }
                    }
                }
                if ( (andRule=andRule->next) != NULL) {
                    result += UNICODE_STRING_SIMPLE(" and ");
                }
            }
            if ( (orRule = orRule->next) != NULL ) {
                result += UNICODE_STRING_SIMPLE(" or ");
            }
        }
    }
    if ( fNext != NULL ) {
        result += UNICODE_STRING_SIMPLE("; ");
        fNext->dumpRules(result);
    }
}


UErrorCode
RuleChain::getKeywords(int32_t capacityOfKeywords, UnicodeString* keywords, int32_t& arraySize) const {
    if ( arraySize < capacityOfKeywords-1 ) {
        keywords[arraySize++]=fKeyword;
    }
    else {
        return U_BUFFER_OVERFLOW_ERROR;
    }

    if ( fNext != NULL ) {
        return fNext->getKeywords(capacityOfKeywords, keywords, arraySize);
    }
    else {
        return U_ZERO_ERROR;
    }
}

UBool
RuleChain::isKeyword(const UnicodeString& keywordParam) const {
    if ( fKeyword == keywordParam ) {
        return TRUE;
    }

    if ( fNext != NULL ) {
        return fNext->isKeyword(keywordParam);
    }
    else {
        return FALSE;
    }
}


PluralRuleParser::PluralRuleParser() : 
        ruleIndex(0), token(), type(none), prevType(none), 
        curAndConstraint(NULL), currentChain(NULL), rangeLowIdx(-1), rangeHiIdx(-1)  
{
}

PluralRuleParser::~PluralRuleParser() {
}


int32_t
PluralRuleParser::getNumberValue(const UnicodeString& token) {
    int32_t i;
    char digits[128];

    i = token.extract(0, token.length(), digits, UPRV_LENGTHOF(digits), US_INV);
    digits[i]='\0';

    return((int32_t)atoi(digits));
}


void
PluralRuleParser::checkSyntax(UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return;
    }
    if (!(prevType==none || prevType==tSemiColon)) {
        type = getKeyType(token, type);  // Switch token type from tKeyword if we scanned a reserved word,
                                               //   and we are not at the start of a rule, where a
                                               //   keyword is expected.
    }

    switch(prevType) {
    case none:
    case tSemiColon:
        if (type!=tKeyword && type != tEOF) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tVariableN:
    case tVariableI:
    case tVariableF:
    case tVariableT:
    case tVariableV:
        if (type != tIs && type != tMod && type != tIn &&
            type != tNot && type != tWithin && type != tEqual && type != tNotEqual) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tKeyword:
        if (type != tColon) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tColon:
        if (!(type == tVariableN ||
              type == tVariableI ||
              type == tVariableF ||
              type == tVariableT ||
              type == tVariableV ||
              type == tAt)) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tIs:
        if ( type != tNumber && type != tNot) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tNot:
        if (type != tNumber && type != tIn && type != tWithin) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tMod:
    case tDot2:
    case tIn:
    case tWithin:
    case tEqual:
    case tNotEqual:
        if (type != tNumber) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tAnd:
    case tOr:
        if ( type != tVariableN &&
             type != tVariableI &&
             type != tVariableF &&
             type != tVariableT &&
             type != tVariableV) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tComma:
        if (type != tNumber) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    case tNumber:
        if (type != tDot2  && type != tSemiColon && type != tIs       && type != tNot    &&
            type != tIn    && type != tEqual     && type != tNotEqual && type != tWithin && 
            type != tAnd   && type != tOr        && type != tComma    && type != tAt     && 
            type != tEOF)
        {
            status = U_UNEXPECTED_TOKEN;
        }
        // TODO: a comma following a number that is not part of a range will be allowed.
        //       It's not the only case of this sort of thing. Parser needs a re-write.
        break;
    case tAt:
        if (type != tDecimal && type != tInteger) {
            status = U_UNEXPECTED_TOKEN;
        }
        break;
    default:
        status = U_UNEXPECTED_TOKEN;
        break;
    }
}


/*
 *  Scan the next token from the input rules.
 *     rules and returned token type are in the parser state variables.
 */
void
PluralRuleParser::getNextToken(UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return;
    }

    UChar ch;
    while (ruleIndex < ruleSrc->length()) {
        ch = ruleSrc->charAt(ruleIndex);
        type = charType(ch);
        if (type != tSpace) {
            break;
        }
        ++(ruleIndex);
    }
    if (ruleIndex >= ruleSrc->length()) {
        type = tEOF;
        return;
    }
    int32_t curIndex= ruleIndex;
        
    switch (type) {
      case tColon:
      case tSemiColon:
      case tComma:
      case tEllipsis:
      case tTilde:   // scanned '~'
      case tAt:      // scanned '@'
      case tEqual:   // scanned '='
      case tMod:     // scanned '%'
        // Single character tokens.
        ++curIndex;
        break;

      case tNotEqual:  // scanned '!'
        if (ruleSrc->charAt(curIndex+1) == EQUALS) {
            curIndex += 2;
        } else {
            type = none;
            curIndex += 1;
        }
        break;

      case tKeyword:
         while (type == tKeyword && ++curIndex < ruleSrc->length()) {
             ch = ruleSrc->charAt(curIndex);
             type = charType(ch);
         }
         type = tKeyword;
         break;

      case tNumber:
         while (type == tNumber && ++curIndex < ruleSrc->length()) {
             ch = ruleSrc->charAt(curIndex);
             type = charType(ch);
         }
         type = tNumber;
         break;

       case tDot:
         // We could be looking at either ".." in a range, or "..." at the end of a sample.
         if (curIndex+1 >= ruleSrc->length() || ruleSrc->charAt(curIndex+1) != DOT) {
             ++curIndex;
             break; // Single dot
         }
         if (curIndex+2 >= ruleSrc->length() || ruleSrc->charAt(curIndex+2) != DOT) {
             curIndex += 2;
             type = tDot2;
             break; // double dot
         }
         type = tEllipsis;
         curIndex += 3;
         break;     // triple dot

       default:
         status = U_UNEXPECTED_TOKEN;
         ++curIndex;
         break;
    }

    U_ASSERT(ruleIndex <= ruleSrc->length());
    U_ASSERT(curIndex <= ruleSrc->length());
    token=UnicodeString(*ruleSrc, ruleIndex, curIndex-ruleIndex);
    ruleIndex = curIndex;
}

tokenType
PluralRuleParser::charType(UChar ch) {
    if ((ch>=U_ZERO) && (ch<=U_NINE)) {
        return tNumber;
    }
    if (ch>=LOW_A && ch<=LOW_Z) {
        return tKeyword;
    }
    switch (ch) {
    case COLON:
        return tColon;
    case SPACE:
        return tSpace;
    case SEMI_COLON:
        return tSemiColon;
    case DOT:
        return tDot;
    case COMMA:
        return tComma;
    case EXCLAMATION:
        return tNotEqual;
    case EQUALS:
        return tEqual;
    case PERCENT_SIGN:
        return tMod;
    case AT:
        return tAt;
    case ELLIPSIS:
        return tEllipsis;
    case TILDE:
        return tTilde;
    default :
        return none;
    }
}


//  Set token type for reserved words in the Plural Rule syntax.

tokenType 
PluralRuleParser::getKeyType(const UnicodeString &token, tokenType keyType)
{
    if (keyType != tKeyword) {
        return keyType;
    }

    if (0 == token.compare(PK_VAR_N, 1)) {
        keyType = tVariableN;
    } else if (0 == token.compare(PK_VAR_I, 1)) {
        keyType = tVariableI;
    } else if (0 == token.compare(PK_VAR_F, 1)) {
        keyType = tVariableF;
    } else if (0 == token.compare(PK_VAR_T, 1)) {
        keyType = tVariableT;
    } else if (0 == token.compare(PK_VAR_V, 1)) {
        keyType = tVariableV;
    } else if (0 == token.compare(PK_IS, 2)) {
        keyType = tIs;
    } else if (0 == token.compare(PK_AND, 3)) {
        keyType = tAnd;
    } else if (0 == token.compare(PK_IN, 2)) {
        keyType = tIn;
    } else if (0 == token.compare(PK_WITHIN, 6)) {
        keyType = tWithin;
    } else if (0 == token.compare(PK_NOT, 3)) {
        keyType = tNot;
    } else if (0 == token.compare(PK_MOD, 3)) {
        keyType = tMod;
    } else if (0 == token.compare(PK_OR, 2)) {
        keyType = tOr;
    } else if (0 == token.compare(PK_DECIMAL, 7)) {
        keyType = tDecimal;
    } else if (0 == token.compare(PK_INTEGER, 7)) {
        keyType = tInteger;
    }
    return keyType;
}


PluralKeywordEnumeration::PluralKeywordEnumeration(RuleChain *header, UErrorCode& status)
        : pos(0), fKeywordNames(status) {
    if (U_FAILURE(status)) {
        return;
    }
    fKeywordNames.setDeleter(uprv_deleteUObject);
    UBool  addKeywordOther=TRUE;
    RuleChain *node=header;
    while(node!=NULL) {
        fKeywordNames.addElement(new UnicodeString(node->fKeyword), status);
        if (U_FAILURE(status)) {
            return;
        }
        if (0 == node->fKeyword.compare(PLURAL_KEYWORD_OTHER, 5)) {
            addKeywordOther= FALSE;
        }
        node=node->fNext;
    }

    if (addKeywordOther) {
        fKeywordNames.addElement(new UnicodeString(PLURAL_KEYWORD_OTHER), status);
    }
}

const UnicodeString*
PluralKeywordEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && pos < fKeywordNames.size()) {
        return (const UnicodeString*)fKeywordNames.elementAt(pos++);
    }
    return NULL;
}

void
PluralKeywordEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
PluralKeywordEnumeration::count(UErrorCode& /*status*/) const {
       return fKeywordNames.size();
}

PluralKeywordEnumeration::~PluralKeywordEnumeration() {
}

FixedDecimal::FixedDecimal(const VisibleDigits &digits) {
    digits.getFixedDecimal(
            source, intValue, decimalDigits,
            decimalDigitsWithoutTrailingZeros,
            visibleDecimalDigitCount, hasIntegerValue);
    isNegative = digits.isNegative();
    isNanOrInfinity = digits.isNaNOrInfinity();
}

FixedDecimal::FixedDecimal(double n, int32_t v, int64_t f) {
    init(n, v, f);
    // check values. TODO make into unit test.
    //            
    //            long visiblePower = (int) Math.pow(10, v);
    //            if (decimalDigits > visiblePower) {
    //                throw new IllegalArgumentException();
    //            }
    //            double fraction = intValue + (decimalDigits / (double) visiblePower);
    //            if (fraction != source) {
    //                double diff = Math.abs(fraction - source)/(Math.abs(fraction) + Math.abs(source));
    //                if (diff > 0.00000001d) {
    //                    throw new IllegalArgumentException();
    //                }
    //            }
}

FixedDecimal::FixedDecimal(double n, int32_t v) {
    // Ugly, but for samples we don't care.
    init(n, v, getFractionalDigits(n, v));
}

FixedDecimal::FixedDecimal(double n) {
    init(n);
}

FixedDecimal::FixedDecimal() {
    init(0, 0, 0);
}


// Create a FixedDecimal from a UnicodeString containing a number.
//    Inefficient, but only used for samples, so simplicity trumps efficiency.

FixedDecimal::FixedDecimal(const UnicodeString &num, UErrorCode &status) {
    CharString cs;
    cs.appendInvariantChars(num, status);
    DigitList dl;
    dl.set(cs.toStringPiece(), status);
    if (U_FAILURE(status)) {
        init(0, 0, 0);
        return;
    }
    int32_t decimalPoint = num.indexOf(DOT);
    double n = dl.getDouble();
    if (decimalPoint == -1) {
        init(n, 0, 0);
    } else {
        int32_t v = num.length() - decimalPoint - 1;
        init(n, v, getFractionalDigits(n, v));
    }
}


FixedDecimal::FixedDecimal(const FixedDecimal &other) {
    source = other.source;
    visibleDecimalDigitCount = other.visibleDecimalDigitCount;
    decimalDigits = other.decimalDigits;
    decimalDigitsWithoutTrailingZeros = other.decimalDigitsWithoutTrailingZeros;
    intValue = other.intValue;
    hasIntegerValue = other.hasIntegerValue;
    isNegative = other.isNegative;
    isNanOrInfinity = other.isNanOrInfinity;
}


void FixedDecimal::init(double n) {
    int32_t numFractionDigits = decimals(n);
    init(n, numFractionDigits, getFractionalDigits(n, numFractionDigits));
}


void FixedDecimal::init(double n, int32_t v, int64_t f) {
    isNegative = n < 0.0;
    source = fabs(n);
    isNanOrInfinity = uprv_isNaN(source) || uprv_isPositiveInfinity(source);
    if (isNanOrInfinity) {
        v = 0;
        f = 0;
        intValue = 0;
        hasIntegerValue = FALSE;
    } else {
        intValue = (int64_t)source;
        hasIntegerValue = (source == intValue);
    }

    visibleDecimalDigitCount = v;
    decimalDigits = f;
    if (f == 0) {
         decimalDigitsWithoutTrailingZeros = 0;
    } else {
        int64_t fdwtz = f;
        while ((fdwtz%10) == 0) {
            fdwtz /= 10;
        }
        decimalDigitsWithoutTrailingZeros = fdwtz;
    }
}


//  Fast path only exact initialization. Return true if successful.
//     Note: Do not multiply by 10 each time through loop, rounding cruft can build
//           up that makes the check for an integer result fail.
//           A single multiply of the original number works more reliably.
static int32_t p10[] = {1, 10, 100, 1000, 10000};
UBool FixedDecimal::quickInit(double n) {
    UBool success = FALSE;
    n = fabs(n);
    int32_t numFractionDigits;
    for (numFractionDigits = 0; numFractionDigits <= 3; numFractionDigits++) {
        double scaledN = n * p10[numFractionDigits];
        if (scaledN == floor(scaledN)) {
            success = TRUE;
            break;
        }
    }
    if (success) {
        init(n, numFractionDigits, getFractionalDigits(n, numFractionDigits));
    }
    return success;
}



int32_t FixedDecimal::decimals(double n) {
    // Count the number of decimal digits in the fraction part of the number, excluding trailing zeros.
    // fastpath the common cases, integers or fractions with 3 or fewer digits
    n = fabs(n);
    for (int ndigits=0; ndigits<=3; ndigits++) {
        double scaledN = n * p10[ndigits];
        if (scaledN == floor(scaledN)) {
            return ndigits;
        }
    }

    // Slow path, convert with sprintf, parse converted output.
    char  buf[30] = {0};
    sprintf(buf, "%1.15e", n);
    // formatted number looks like this: 1.234567890123457e-01
    int exponent = atoi(buf+18);
    int numFractionDigits = 15;
    for (int i=16; ; --i) {
        if (buf[i] != '0') {
            break;
        }
        --numFractionDigits; 
    }
    numFractionDigits -= exponent;   // Fraction part of fixed point representation.
    return numFractionDigits;
}


// Get the fraction digits of a double, represented as an integer.
//    v is the number of visible fraction digits in the displayed form of the number.
//       Example: n = 1001.234, v = 6, result = 234000
//    TODO: need to think through how this is used in the plural rule context.
//          This function can easily encounter integer overflow, 
//          and can easily return noise digits when the precision of a double is exceeded.

int64_t FixedDecimal::getFractionalDigits(double n, int32_t v) {
    if (v == 0 || n == floor(n) || uprv_isNaN(n) || uprv_isPositiveInfinity(n)) {
        return 0;
    }
    n = fabs(n);
    double fract = n - floor(n);
    switch (v) {
      case 1: return (int64_t)(fract*10.0 + 0.5);
      case 2: return (int64_t)(fract*100.0 + 0.5);
      case 3: return (int64_t)(fract*1000.0 + 0.5);
      default:
          double scaled = floor(fract * pow(10.0, (double)v) + 0.5);
          if (scaled > U_INT64_MAX) {
              return U_INT64_MAX;
          } else {
              return (int64_t)scaled;
          }
      }
}


void FixedDecimal::adjustForMinFractionDigits(int32_t minFractionDigits) {
    int32_t numTrailingFractionZeros = minFractionDigits - visibleDecimalDigitCount;
    if (numTrailingFractionZeros > 0) {
        for (int32_t i=0; i<numTrailingFractionZeros; i++) {
            // Do not let the decimalDigits value overflow if there are many trailing zeros.
            // Limit the value to 18 digits, the most that a 64 bit int can fully represent.
            if (decimalDigits >= 100000000000000000LL) {
                break;
            }
            decimalDigits *= 10;
        }
        visibleDecimalDigitCount += numTrailingFractionZeros;
    }
}
        

double FixedDecimal::get(tokenType operand) const {
    switch(operand) {
        case tVariableN: return source;
        case tVariableI: return (double)intValue;
        case tVariableF: return (double)decimalDigits;
        case tVariableT: return (double)decimalDigitsWithoutTrailingZeros; 
        case tVariableV: return visibleDecimalDigitCount;
        default:
             U_ASSERT(FALSE);  // unexpected.
             return source;
    }
}

int32_t FixedDecimal::getVisibleFractionDigitCount() const {
    return visibleDecimalDigitCount;
}



PluralAvailableLocalesEnumeration::PluralAvailableLocalesEnumeration(UErrorCode &status) {
    fLocales = NULL;
    fRes = NULL;
    fOpenStatus = status;
    if (U_FAILURE(status)) {
        return;
    }
    fOpenStatus = U_ZERO_ERROR;
    LocalUResourceBundlePointer rb(ures_openDirect(NULL, "plurals", &fOpenStatus));
    fLocales = ures_getByKey(rb.getAlias(), "locales", NULL, &fOpenStatus);
}

PluralAvailableLocalesEnumeration::~PluralAvailableLocalesEnumeration() {
    ures_close(fLocales);
    ures_close(fRes);
    fLocales = NULL;
    fRes = NULL;
}

const char *PluralAvailableLocalesEnumeration::next(int32_t *resultLength, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (U_FAILURE(fOpenStatus)) {
        status = fOpenStatus;
        return NULL;
    }
    fRes = ures_getNextResource(fLocales, fRes, &status);
    if (fRes == NULL || U_FAILURE(status)) {
        if (status == U_INDEX_OUTOFBOUNDS_ERROR) {
            status = U_ZERO_ERROR;
        }
        return NULL;
    }
    const char *result = ures_getKey(fRes);
    if (resultLength != NULL) {
        *resultLength = uprv_strlen(result);
    }
    return result;
}


void PluralAvailableLocalesEnumeration::reset(UErrorCode &status) {
    if (U_FAILURE(status)) {
       return;
    }
    if (U_FAILURE(fOpenStatus)) {
        status = fOpenStatus;
        return;
    }
    ures_resetIterator(fLocales);
}

int32_t PluralAvailableLocalesEnumeration::count(UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return 0;
    }
    if (U_FAILURE(fOpenStatus)) {
        status = fOpenStatus;
        return 0;
    }
    return ures_getSize(fLocales);
}

U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof

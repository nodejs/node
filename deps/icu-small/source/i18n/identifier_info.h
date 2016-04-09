/*
**********************************************************************
*   Copyright (C) 2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* indentifier_info.h
* 
* created on: 2013 Jan 7
* created by: Andy Heninger
*/

#ifndef __IDENTIFIER_INFO_H__
#define __IDENTIFIER_INFO_H__

#include "unicode/utypes.h"

#include "unicode/uniset.h"
#include "unicode/uspoof.h"
#include "uhash.h"

U_NAMESPACE_BEGIN

class ScriptSet;

// TODO(andy): review consistency of reference vs pointer arguments to the funcions.

/**
 * This class analyzes a possible identifier for script and identifier status. Use it by calling setIdentifierProfile
 * then setIdentifier. Available methods include:
 * <ol>
 * <li>call getScripts for the specific scripts in the identifier. The identifier contains at least one character in
 * each of these.
 * <li>call getAlternates to get cases where a character is not limited to a single script. For example, it could be
 * either Katakana or Hiragana.
 * <li>call getCommonAmongAlternates to find out if any scripts are common to all the alternates.
 * <li>call getNumerics to get a representative character (with value zero) for each of the decimal number systems in
 * the identifier.
 * <li>call getRestrictionLevel to see what the UTS36 restriction level is.
 * </ol>
 * 
 * This is a port from ICU4J of class com.ibm.icu.text.IdentifierInfo
 */
class U_I18N_API IdentifierInfo : public UMemory {

  public:
    /**
     * Create an identifier info object. Subsequently, call setIdentifier(), etc.
     * @internal
     */
    IdentifierInfo(UErrorCode &status);

    /**
      * Destructor
      */
    virtual ~IdentifierInfo();

  private:
    /* Disallow copying for now. Can be added if there's a need. */
    IdentifierInfo(const IdentifierInfo &other);

  public:
     
    /**
     * Set the identifier profile: the characters that are to be allowed in the identifier.
     * 
     * @param identifierProfile the characters that are to be allowed in the identifier
     * @return this
     * @internal
     */
    IdentifierInfo &setIdentifierProfile(const UnicodeSet &identifierProfile);

    /**
     * Get the identifier profile: the characters that are to be allowed in the identifier.
     * 
     * @return The characters that are to be allowed in the identifier.
     * @internal
     */
    const UnicodeSet &getIdentifierProfile() const;


    /**
     * Set an identifier to analyze. Afterwards, call methods like getScripts()
     * 
     * @param identifier the identifier to analyze
     * @param status Errorcode, set if errors occur.
     * @return this
     * @internal
     */
    IdentifierInfo &setIdentifier(const UnicodeString &identifier, UErrorCode &status);


    /**
     * Get the identifier that was analyzed. The returned string is owned by the ICU library,
     * and must not be deleted by the caller.
     * 
     * @return the identifier that was analyzed.
     * @internal
     */
    const UnicodeString *getIdentifier() const;
    

    /**
     * Get the scripts found in the identifiers.
     * 
     * @return the set of explicit scripts.
     * @internal
     */
    const ScriptSet *getScripts() const;

    /**
     * Get the set of alternate scripts found in the identifiers. That is, when a character can be in two scripts, then
     * the set consisting of those scripts will be returned.
     * 
     * @return a uhash, with each key being of type (ScriptSet *). 
     *         This is a set, not a map, so the value stored in the uhash is not relevant.
     *         (It is, in fact, 1).
     *         Ownership of the uhash and its contents remains with the IndetifierInfo object, 
     *         and remains valid until a new identifer is set or until the object is deleted.
     * @internal
     */
    const UHashtable *getAlternates() const;

    /**
     * Get the representative characters (zeros) for the numerics found in the identifier.
     * 
     * @return the set of explicit scripts.
     * @internal
     */
    const UnicodeSet *getNumerics() const;

    /**
     * Find out which scripts are in common among the alternates.
     * 
     * @return the set of scripts that are in common among the alternates.
     * @internal
     */
    const ScriptSet *getCommonAmongAlternates() const;

    /**
      * Get the number of scripts appearing in the identifier.
      *   Note: Common and Inherited scripts are omitted from the count.
      *   Note: Result may be high when the identifier contains characters
      *         with alternate scripts. The distinction between
      *         0, 1 and > 1 will remain valid, however.
      * @return the number of scripts.
      */
    int32_t getScriptCount() const;

#if !UCONFIG_NO_NORMALIZATION

    /**
     * Find the "tightest" restriction level that the identifier satisfies.
     * 
     * @return the restriction level.
     * @internal
     */
    URestrictionLevel getRestrictionLevel(UErrorCode &status) const;

#endif /*!UCONFIG_NO_NORMALIZATION */

    UnicodeString toString() const;

    /**
     * Produce a readable string of alternates.
     * 
     * @param alternates a UHashtable of UScriptSets.
     *        Keys only, no meaningful values in the UHash.
     * @return display form
     * @internal
     */
    static UnicodeString &displayAlternates(UnicodeString &dest, const UHashtable *alternates, UErrorCode &status);

  private:

    IdentifierInfo  & clear();
    UBool             containsWithAlternates(const ScriptSet &container, const ScriptSet &containee) const;

    UnicodeString     *fIdentifier;
    ScriptSet         *fRequiredScripts;
    UHashtable        *fScriptSetSet;
    ScriptSet         *fCommonAmongAlternates;
    UnicodeSet        *fNumerics;
    UnicodeSet        *fIdentifierProfile;
};

U_NAMESPACE_END

#endif // __IDENTIFIER_INFO_H__


// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* scriptset.h
*
* created on: 2013 Jan 7
* created by: Andy Heninger
*/

#ifndef __SCRIPTSET_H__
#define __SCRIPTSET_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/uscript.h"

#include "uelement.h"

U_NAMESPACE_BEGIN

//-------------------------------------------------------------------------------
//
//  ScriptSet - A bit set representing a set of scripts.
//
//              This class was originally used exclusively with script sets appearing
//              as part of the spoof check whole script confusable binary data. Its
//              use has since become more general, but the continued use to wrap
//              prebuilt binary data does constrain the design.
//
//-------------------------------------------------------------------------------
class U_I18N_API ScriptSet: public UMemory {
  public:
    static constexpr int32_t SCRIPT_LIMIT = 224;  // multiple of 32!

    ScriptSet();
    ScriptSet(const ScriptSet &other);
    ~ScriptSet();

    bool operator == (const ScriptSet &other) const;
    bool operator != (const ScriptSet &other) const {return !(*this == other);}
    ScriptSet & operator = (const ScriptSet &other);

    UBool      test(UScriptCode script, UErrorCode &status) const;
    ScriptSet &Union(const ScriptSet &other);
    ScriptSet &set(UScriptCode script, UErrorCode &status);
    ScriptSet &reset(UScriptCode script, UErrorCode &status);
    ScriptSet &intersect(const ScriptSet &other);
    ScriptSet &intersect(UScriptCode script, UErrorCode &status);
    UBool      intersects(const ScriptSet &other) const;  // Sets contain at least one script in common.
    UBool      contains(const ScriptSet &other) const;    // All set bits in other are also set in this.

    ScriptSet &setAll();
    ScriptSet &resetAll();
    int32_t countMembers() const;
    int32_t hashCode() const;
    int32_t nextSetBit(int32_t script) const;

    UBool isEmpty() const;

    UnicodeString &displayScripts(UnicodeString &dest) const; // append script names to dest string.
    ScriptSet & parseScripts(const UnicodeString &scriptsString, UErrorCode &status);  // Replaces ScriptSet contents.

    // Wraps around UScript::getScriptExtensions() and adds the corresponding scripts to this instance.
    void setScriptExtensions(UChar32 codePoint, UErrorCode& status);

  private:
    uint32_t  bits[SCRIPT_LIMIT / 32];
};

U_NAMESPACE_END

U_CAPI int32_t U_EXPORT2
uhash_compareScriptSet(const UElement key1, const UElement key2);

U_CAPI int32_t U_EXPORT2
uhash_hashScriptSet(const UElement key);

U_CAPI void U_EXPORT2
uhash_deleteScriptSet(void *obj);

U_CAPI UBool U_EXPORT2
uhash_equalsScriptSet(const UElement key1, const UElement key2);

#endif // __SCRIPTSET_H_

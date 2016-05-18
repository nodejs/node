/*
**********************************************************************
*   Copyright (C) 2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* scriptset.cpp
*
* created on: 2013 Jan 7
* created by: Andy Heninger
*/

#include "unicode/utypes.h"

#include "unicode/uchar.h"
#include "unicode/unistr.h"

#include "scriptset.h"
#include "uassert.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

//----------------------------------------------------------------------------
//
//  ScriptSet implementation
//
//----------------------------------------------------------------------------
ScriptSet::ScriptSet() {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        bits[i] = 0;
    }
}

ScriptSet::~ScriptSet() {
}

ScriptSet::ScriptSet(const ScriptSet &other) {
    *this = other;
}


ScriptSet & ScriptSet::operator =(const ScriptSet &other) {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        bits[i] = other.bits[i];
    }
    return *this;
}


UBool ScriptSet::operator == (const ScriptSet &other) const {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        if (bits[i] != other.bits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

UBool ScriptSet::test(UScriptCode script, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (script < 0 || script >= (int32_t)sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    return ((bits[index] & bit) != 0);
}


ScriptSet &ScriptSet::set(UScriptCode script, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (script < 0 || script >= (int32_t)sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    bits[index] |= bit;
    return *this;
}

ScriptSet &ScriptSet::reset(UScriptCode script, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (script < 0 || script >= (int32_t)sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    bits[index] &= ~bit;
    return *this;
}



ScriptSet &ScriptSet::Union(const ScriptSet &other) {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        bits[i] |= other.bits[i];
    }
    return *this;
}

ScriptSet &ScriptSet::intersect(const ScriptSet &other) {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        bits[i] &= other.bits[i];
    }
    return *this;
}

ScriptSet &ScriptSet::intersect(UScriptCode script, UErrorCode &status) {
    ScriptSet t;
    t.set(script, status);
    if (U_SUCCESS(status)) {
        this->intersect(t);
    }
    return *this;
}

UBool ScriptSet::intersects(const ScriptSet &other) const {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        if ((bits[i] & other.bits[i]) != 0) {
            return true;
        }
    }
    return false;
}

UBool ScriptSet::contains(const ScriptSet &other) const {
    ScriptSet t(*this);
    t.intersect(other);
    return (t == other);
}


ScriptSet &ScriptSet::setAll() {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        bits[i] = 0xffffffffu;
    }
    return *this;
}


ScriptSet &ScriptSet::resetAll() {
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        bits[i] = 0;
    }
    return *this;
}

int32_t ScriptSet::countMembers() const {
    // This bit counter is good for sparse numbers of '1's, which is
    //  very much the case that we will usually have.
    int32_t count = 0;
    for (uint32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        uint32_t x = bits[i];
        while (x > 0) {
            count++;
            x &= (x - 1);    // and off the least significant one bit.
        }
    }
    return count;
}

int32_t ScriptSet::hashCode() const {
    int32_t hash = 0;
    for (int32_t i=0; i<UPRV_LENGTHOF(bits); i++) {
        hash ^= bits[i];
    }
    return hash;
}

int32_t ScriptSet::nextSetBit(int32_t fromIndex) const {
    // TODO: Wants a better implementation.
    if (fromIndex < 0) {
        return -1;
    }
    UErrorCode status = U_ZERO_ERROR;
    for (int32_t scriptIndex = fromIndex; scriptIndex < (int32_t)sizeof(bits)*8; scriptIndex++) {
        if (test((UScriptCode)scriptIndex, status)) {
            return scriptIndex;
        }
    }
    return -1;
}

UnicodeString &ScriptSet::displayScripts(UnicodeString &dest) const {
    UBool firstTime = TRUE;
    for (int32_t i = nextSetBit(0); i >= 0; i = nextSetBit(i + 1)) {
        if (!firstTime) {
            dest.append((UChar)0x20);
        }
        firstTime = FALSE;
        const char *scriptName = uscript_getShortName((UScriptCode(i)));
        dest.append(UnicodeString(scriptName, -1, US_INV));
    }
    return dest;
}

ScriptSet &ScriptSet::parseScripts(const UnicodeString &scriptString, UErrorCode &status) {
    resetAll();
    if (U_FAILURE(status)) {
        return *this;
    }
    UnicodeString oneScriptName;
    for (int32_t i=0; i<scriptString.length();) {
        UChar32 c = scriptString.char32At(i);
        i = scriptString.moveIndex32(i, 1);
        if (!u_isUWhiteSpace(c)) {
            oneScriptName.append(c);
            if (i < scriptString.length()) {
                continue;
            }
        }
        if (oneScriptName.length() > 0) {
            char buf[40];
            oneScriptName.extract(0, oneScriptName.length(), buf, sizeof(buf)-1, US_INV);
            buf[sizeof(buf)-1] = 0;
            int32_t sc = u_getPropertyValueEnum(UCHAR_SCRIPT, buf);
            if (sc == UCHAR_INVALID_CODE) {
                status = U_ILLEGAL_ARGUMENT_ERROR;
            } else {
                this->set((UScriptCode)sc, status);
            }
            if (U_FAILURE(status)) {
                return *this;
            }
            oneScriptName.remove();
        }
    }
    return *this;
}

U_NAMESPACE_END

U_CAPI UBool U_EXPORT2
uhash_equalsScriptSet(const UElement key1, const UElement key2) {
    icu::ScriptSet *s1 = static_cast<icu::ScriptSet *>(key1.pointer);
    icu::ScriptSet *s2 = static_cast<icu::ScriptSet *>(key2.pointer);
    return (*s1 == *s2);
}

U_CAPI int8_t U_EXPORT2
uhash_compareScriptSet(UElement key0, UElement key1) {
    icu::ScriptSet *s0 = static_cast<icu::ScriptSet *>(key0.pointer);
    icu::ScriptSet *s1 = static_cast<icu::ScriptSet *>(key1.pointer);
    int32_t diff = s0->countMembers() - s1->countMembers();
    if (diff != 0) return diff;
    int32_t i0 = s0->nextSetBit(0);
    int32_t i1 = s1->nextSetBit(0);
    while ((diff = i0-i1) == 0 && i0 > 0) {
        i0 = s0->nextSetBit(i0+1);
        i1 = s1->nextSetBit(i1+1);
    }
    return (int8_t)diff;
}

U_CAPI int32_t U_EXPORT2
uhash_hashScriptSet(const UElement key) {
    icu::ScriptSet *s = static_cast<icu::ScriptSet *>(key.pointer);
    return s->hashCode();
}

U_CAPI void U_EXPORT2
uhash_deleteScriptSet(void *obj) {
    icu::ScriptSet *s = static_cast<icu::ScriptSet *>(obj);
    delete s;
}

/*
**********************************************************************
*   Copyright (c) 2001-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   11/19/2001  aliu        Creation.
**********************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/utf16.h"
#include "esctrn.h"
#include "util.h"

U_NAMESPACE_BEGIN

static const UChar UNIPRE[] = {85,43,0}; // "U+"
static const UChar BS_u[] = {92,117,0}; // "\\u"
static const UChar BS_U[] = {92,85,0}; // "\\U"
static const UChar XMLPRE[] = {38,35,120,0}; // "&#x"
static const UChar XML10PRE[] = {38,35,0}; // "&#"
static const UChar PERLPRE[] = {92,120,123,0}; // "\\x{"
static const UChar SEMI[] = {59,0}; // ";"
static const UChar RBRACE[] = {125,0}; // "}"

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(EscapeTransliterator)

/**
 * Factory methods
 */
static Transliterator* _createEscUnicode(const UnicodeString& ID, Transliterator::Token /*context*/) {
    // Unicode: "U+10FFFF" hex, min=4, max=6
    return new EscapeTransliterator(ID, UnicodeString(TRUE, UNIPRE, 2), UnicodeString(), 16, 4, TRUE, NULL);
}
static Transliterator* _createEscJava(const UnicodeString& ID, Transliterator::Token /*context*/) {
    // Java: "\\uFFFF" hex, min=4, max=4
    return new EscapeTransliterator(ID, UnicodeString(TRUE, BS_u, 2), UnicodeString(), 16, 4, FALSE, NULL);
}
static Transliterator* _createEscC(const UnicodeString& ID, Transliterator::Token /*context*/) {
    // C: "\\uFFFF" hex, min=4, max=4; \\U0010FFFF hex, min=8, max=8
    return new EscapeTransliterator(ID, UnicodeString(TRUE, BS_u, 2), UnicodeString(), 16, 4, TRUE,
             new EscapeTransliterator(UnicodeString(), UnicodeString(TRUE, BS_U, 2), UnicodeString(), 16, 8, TRUE, NULL));
}
static Transliterator* _createEscXML(const UnicodeString& ID, Transliterator::Token /*context*/) {
    // XML: "&#x10FFFF;" hex, min=1, max=6
    return new EscapeTransliterator(ID, UnicodeString(TRUE, XMLPRE, 3), UnicodeString(SEMI[0]), 16, 1, TRUE, NULL);
}
static Transliterator* _createEscXML10(const UnicodeString& ID, Transliterator::Token /*context*/) {
    // XML10: "&1114111;" dec, min=1, max=7 (not really "Any-Hex")
    return new EscapeTransliterator(ID, UnicodeString(TRUE, XML10PRE, 2), UnicodeString(SEMI[0]), 10, 1, TRUE, NULL);
}
static Transliterator* _createEscPerl(const UnicodeString& ID, Transliterator::Token /*context*/) {
    // Perl: "\\x{263A}" hex, min=1, max=6
    return new EscapeTransliterator(ID, UnicodeString(TRUE, PERLPRE, 3), UnicodeString(RBRACE[0]), 16, 1, TRUE, NULL);
}

/**
 * Registers standard variants with the system.  Called by
 * Transliterator during initialization.
 */
void EscapeTransliterator::registerIDs() {
    Token t = integerToken(0);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex/Unicode"), _createEscUnicode, t);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex/Java"), _createEscJava, t);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex/C"), _createEscC, t);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex/XML"), _createEscXML, t);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex/XML10"), _createEscXML10, t);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex/Perl"), _createEscPerl, t);

    Transliterator::_registerFactory(UNICODE_STRING_SIMPLE("Any-Hex"), _createEscJava, t);
}

/**
 * Constructs an escape transliterator with the given ID and
 * parameters.  See the class member documentation for details.
 */
EscapeTransliterator::EscapeTransliterator(const UnicodeString& newID,
                         const UnicodeString& _prefix, const UnicodeString& _suffix,
                         int32_t _radix, int32_t _minDigits,
                         UBool _grokSupplementals,
                         EscapeTransliterator* adoptedSupplementalHandler) :
    Transliterator(newID, NULL)
{
    this->prefix = _prefix;
    this->suffix = _suffix;
    this->radix = _radix;
    this->minDigits = _minDigits;
    this->grokSupplementals = _grokSupplementals;
    this->supplementalHandler = adoptedSupplementalHandler;
}

/**
 * Copy constructor.
 */
EscapeTransliterator::EscapeTransliterator(const EscapeTransliterator& o) :
    Transliterator(o),
    prefix(o.prefix),
    suffix(o.suffix),
    radix(o.radix),
    minDigits(o.minDigits),
    grokSupplementals(o.grokSupplementals) {
    supplementalHandler = (o.supplementalHandler != 0) ?
        new EscapeTransliterator(*o.supplementalHandler) : NULL;
}

EscapeTransliterator::~EscapeTransliterator() {
    delete supplementalHandler;
}

/**
 * Transliterator API.
 */
Transliterator* EscapeTransliterator::clone() const {
    return new EscapeTransliterator(*this);
}

/**
 * Implements {@link Transliterator#handleTransliterate}.
 */
void EscapeTransliterator::handleTransliterate(Replaceable& text,
                                               UTransPosition& pos,
                                               UBool /*isIncremental*/) const
{
    /* TODO: Verify that isIncremental can be ignored */
    int32_t start = pos.start;
    int32_t limit = pos.limit;

    UnicodeString buf(prefix);
    int32_t prefixLen = prefix.length();
    UBool redoPrefix = FALSE;

    while (start < limit) {
        int32_t c = grokSupplementals ? text.char32At(start) : text.charAt(start);
        int32_t charLen = grokSupplementals ? U16_LENGTH(c) : 1;

        if ((c & 0xFFFF0000) != 0 && supplementalHandler != NULL) {
            buf.truncate(0);
            buf.append(supplementalHandler->prefix);
            ICU_Utility::appendNumber(buf, c, supplementalHandler->radix,
                                  supplementalHandler->minDigits);
            buf.append(supplementalHandler->suffix);
            redoPrefix = TRUE;
        } else {
            if (redoPrefix) {
                buf.truncate(0);
                buf.append(prefix);
                redoPrefix = FALSE;
            } else {
                buf.truncate(prefixLen);
            }
            ICU_Utility::appendNumber(buf, c, radix, minDigits);
            buf.append(suffix);
        }

        text.handleReplaceBetween(start, start + charLen, buf);
        start += buf.length();
        limit += buf.length() - charLen;
    }

    pos.contextLimit += limit - pos.limit;
    pos.limit = limit;
    pos.start = start;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

//eof

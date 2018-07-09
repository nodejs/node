// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (c) 2001-2011, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 *   Date        Name        Description
 *   11/19/2001  aliu        Creation.
 **********************************************************************
 */

#ifndef ICU_UTIL_H
#define ICU_UTIL_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"

//--------------------------------------------------------------------
// class ICU_Utility
// i18n utility functions, scoped into the class ICU_Utility.
//--------------------------------------------------------------------

U_NAMESPACE_BEGIN

class UnicodeMatcher;

class U_COMMON_API ICU_Utility /* not : public UObject because all methods are static */ {
 public:

    /**
     * Append a number to the given UnicodeString in the given radix.
     * Standard digits '0'-'9' are used and letters 'A'-'Z' for
     * radices 11 through 36.
     * @param result the digits of the number are appended here
     * @param n the number to be converted to digits; may be negative.
     * If negative, a '-' is prepended to the digits.
     * @param radix a radix from 2 to 36 inclusive.
     * @param minDigits the minimum number of digits, not including
     * any '-', to produce.  Values less than 2 have no effect.  One
     * digit is always emitted regardless of this parameter.
     * @return a reference to result
     */
    static UnicodeString& appendNumber(UnicodeString& result, int32_t n,
                                       int32_t radix = 10,
                                       int32_t minDigits = 1);

    /** Returns a bogus UnicodeString by value. */
    static inline UnicodeString makeBogusString() {
        UnicodeString result;
        result.setToBogus();
        return result;
    }

    /**
     * Return true if the character is NOT printable ASCII.
     *
     * This method should really be in UnicodeString (or similar).  For
     * now, we implement it here and share it with friend classes.
     */
    static UBool isUnprintable(UChar32 c);

    /**
     * Escape unprintable characters using \uxxxx notation for U+0000 to
     * U+FFFF and \Uxxxxxxxx for U+10000 and above.  If the character is
     * printable ASCII, then do nothing and return FALSE.  Otherwise,
     * append the escaped notation and return TRUE.
     */
    static UBool escapeUnprintable(UnicodeString& result, UChar32 c);

    /**
     * Returns the index of a character, ignoring quoted text.
     * For example, in the string "abc'hide'h", the 'h' in "hide" will not be
     * found by a search for 'h'.
     * @param text text to be searched
     * @param start the beginning index, inclusive; <code>0 <= start
     * <= limit</code>.
     * @param limit the ending index, exclusive; <code>start <= limit
     * <= text.length()</code>.
     * @param c character to search for
     * @return Offset of the first instance of c, or -1 if not found.
     */
//?FOR FUTURE USE.  DISABLE FOR NOW for coverage reasons.
//    static int32_t quotedIndexOf(const UnicodeString& text,
//                                 int32_t start, int32_t limit,
//                                 UChar c);

    /**
     * Skip over a sequence of zero or more white space characters at pos.
     * @param advance if true, advance pos to the first non-white-space
     * character at or after pos, or str.length(), if there is none.
     * Otherwise leave pos unchanged.
     * @return the index of the first non-white-space character at or
     * after pos, or str.length(), if there is none.
     */
    static int32_t skipWhitespace(const UnicodeString& str, int32_t& pos,
                                  UBool advance = FALSE);

    /**
     * Skip over Pattern_White_Space in a Replaceable.
     * Skipping may be done in the forward or
     * reverse direction.  In either case, the leftmost index will be
     * inclusive, and the rightmost index will be exclusive.  That is,
     * given a range defined as [start, limit), the call
     * skipWhitespace(text, start, limit) will advance start past leading
     * whitespace, whereas the call skipWhitespace(text, limit, start),
     * will back up limit past trailing whitespace.
     * @param text the text to be analyzed
     * @param pos either the start or limit of a range of 'text', to skip
     * leading or trailing whitespace, respectively
     * @param stop either the limit or start of a range of 'text', to skip
     * leading or trailing whitespace, respectively
     * @return the new start or limit, depending on what was passed in to
     * 'pos'
     */
//?FOR FUTURE USE.  DISABLE FOR NOW for coverage reasons.
//?    static int32_t skipWhitespace(const Replaceable& text,
//?                                  int32_t pos, int32_t stop);

    /**
     * Parse a single non-whitespace character 'ch', optionally
     * preceded by whitespace.
     * @param id the string to be parsed
     * @param pos INPUT-OUTPUT parameter.  On input, pos[0] is the
     * offset of the first character to be parsed.  On output, pos[0]
     * is the index after the last parsed character.  If the parse
     * fails, pos[0] will be unchanged.
     * @param ch the non-whitespace character to be parsed.
     * @return true if 'ch' is seen preceded by zero or more
     * whitespace characters.
     */
    static UBool parseChar(const UnicodeString& id, int32_t& pos, UChar ch);

    /**
     * Parse a pattern string starting at offset pos.  Keywords are
     * matched case-insensitively.  Spaces may be skipped and may be
     * optional or required.  Integer values may be parsed, and if
     * they are, they will be returned in the given array.  If
     * successful, the offset of the next non-space character is
     * returned.  On failure, -1 is returned.
     * @param pattern must only contain lowercase characters, which
     * will match their uppercase equivalents as well.  A space
     * character matches one or more required spaces.  A '~' character
     * matches zero or more optional spaces.  A '#' character matches
     * an integer and stores it in parsedInts, which the caller must
     * ensure has enough capacity.
     * @param parsedInts array to receive parsed integers.  Caller
     * must ensure that parsedInts.length is >= the number of '#'
     * signs in 'pattern'.
     * @return the position after the last character parsed, or -1 if
     * the parse failed
     */
    static int32_t parsePattern(const UnicodeString& rule, int32_t pos, int32_t limit,
                                const UnicodeString& pattern, int32_t* parsedInts);
        
    /**
     * Parse a pattern string within the given Replaceable and a parsing
     * pattern.  Characters are matched literally and case-sensitively
     * except for the following special characters:
     *
     * ~  zero or more Pattern_White_Space chars
     *
     * If end of pattern is reached with all matches along the way,
     * pos is advanced to the first unparsed index and returned.
     * Otherwise -1 is returned.
     * @param pat pattern that controls parsing
     * @param text text to be parsed, starting at index
     * @param index offset to first character to parse
     * @param limit offset after last character to parse
     * @return index after last parsed character, or -1 on parse failure.
     */
    static int32_t parsePattern(const UnicodeString& pat,
                                const Replaceable& text,
                                int32_t index,
                                int32_t limit);

    /**
     * Parse an integer at pos, either of the form \d+ or of the form
     * 0x[0-9A-Fa-f]+ or 0[0-7]+, that is, in standard decimal, hex,
     * or octal format.
     * @param pos INPUT-OUTPUT parameter.  On input, the first
     * character to parse.  On output, the character after the last
     * parsed character.
     */
    static int32_t parseInteger(const UnicodeString& rule, int32_t& pos, int32_t limit);

    /**
     * Parse a Unicode identifier from the given string at the given
     * position.  Return the identifier, or an empty string if there
     * is no identifier.
     * @param str the string to parse
     * @param pos INPUT-OUPUT parameter.  On INPUT, pos is the
     * first character to examine.  It must be less than str.length(),
     * and it must not point to a whitespace character.  That is, must
     * have pos < str.length() and
     * !UCharacter::isWhitespace(str.char32At(pos)).  On
     * OUTPUT, the position after the last parsed character.
     * @return the Unicode identifier, or an empty string if there is
     * no valid identifier at pos.
     */
    static UnicodeString parseUnicodeIdentifier(const UnicodeString& str, int32_t& pos);

    /**
     * Parse an unsigned 31-bit integer at the given offset.  Use
     * UCharacter.digit() to parse individual characters into digits.
     * @param text the text to be parsed
     * @param pos INPUT-OUTPUT parameter.  On entry, pos is the
     * offset within text at which to start parsing; it should point
     * to a valid digit.  On exit, pos is the offset after the last
     * parsed character.  If the parse failed, it will be unchanged on
     * exit.  Must be >= 0 on entry.
     * @param radix the radix in which to parse; must be >= 2 and <=
     * 36.
     * @return a non-negative parsed number, or -1 upon parse failure.
     * Parse fails if there are no digits, that is, if pos does not
     * point to a valid digit on entry, or if the number to be parsed
     * does not fit into a 31-bit unsigned integer.
     */
    static int32_t parseNumber(const UnicodeString& text,
                               int32_t& pos, int8_t radix);

    static void appendToRule(UnicodeString& rule,
                             UChar32 c,
                             UBool isLiteral,
                             UBool escapeUnprintable,
                             UnicodeString& quoteBuf);
    
    static void appendToRule(UnicodeString& rule,
                             const UnicodeString& text,
                             UBool isLiteral,
                             UBool escapeUnprintable,
                             UnicodeString& quoteBuf);

    static void appendToRule(UnicodeString& rule,
                             const UnicodeMatcher* matcher,
                             UBool escapeUnprintable,
                             UnicodeString& quoteBuf);

private:
    // do not instantiate
    ICU_Utility();
};

U_NAMESPACE_END

#endif
//eof

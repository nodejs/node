//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
/*
--------------------------------------------------------------------------------------------------------------------------------
Original
--------------------------------------------------------------------------------------------------------------------------------

Pattern ::= Disjunction
Disjunction ::= Alternative | Alternative '|' Disjunction
Alternative ::= [empty] | Alternative Term
Term ::= Assertion | Atom | Atom Quantifier
Assertion ::= '^' | '$' | '\' 'b' | '\' 'B' | '(' '?' '=' Disjunction ')' | '(' '?' '!' Disjunction ')'
Quantifier ::= QuantifierPrefix | QuantifierPrefix '?'
QuantifierPrefix ::= '*' | '+' | '?' | '{' DecimalDigits '}' | '{' DecimalDigits ',' '}' | '{' DecimalDigits ',' DecimalDigits '}'
Atom ::= PatternCharacter | '.' | '\' AtomEscape | CharacterClass | '(' Disjunction ')' | '(' '?' ':' Disjunction ')'
PatternCharacter ::= SourceCharacter but not any of { '^', '$', '\', '.', '*', '+', '?', '(', ')', '[', ']', '{', '}', '|' }
AtomEscape ::= DecimalEscape | CharacterEscape | CharacterClassEscape
CharacterEscape ::= ControlEscape | 'c' ControlLetter | HexEscapeSequence | UnicodeEscapeSequence | IdentityEscape
ControlEscape ::= one of { 'f', 'n', 'r', 't', 'v' }
ControlLetter ::= one of { 'a'..'z', 'A'..'Z' }
IdentityEscape ::= (SourceCharacter but not IdentifierPart) | <ZWJ> | <ZWNJ>
DecimalEscape ::= DecimalIntegerLiteral [lookahead not in DecimalDigit]
CharacterClassEscape :: one of { 'd', 'D', 's', 'S', 'w', 'W' }
CharacterClass ::= '[' [lookahead not in {'^'}] ClassRanges ']' | '[' '^' ClassRanges ']'
ClassRanges ::= [empty] | NonemptyClassRanges
NonemptyClassRanges ::= ClassAtom | ClassAtom NonemptyClassRangesNoDash | ClassAtom '-' ClassAtom ClassRanges
NonemptyClassRangesNoDash ::= ClassAtom | ClassAtomNoDash NonemptyClassRangesNoDash | ClassAtomNoDash '-' ClassAtom ClassRanges
ClassAtom ::= '-' | ClassAtomNoDash
ClassAtomNoDash ::= SourceCharacter but not one of { '\', ']', '-' } | '\' ClassEscape
ClassEscape ::= DecimalEscape | 'b' | CharacterEscape | CharacterClassEscape
SourceCharacter ::= <unicode character>
HexEscapeSequence ::= 'x' HexDigit HexDigit
UnicodeEscapeSequence ::= 'u' HexDigit HexDigit HexDigit HexDigit | 'u' '{' HexDigits '}'
HexDigit ::= one of { '0'..'9', 'a'..'f', 'A'..'F' }
IdentifierStart ::= UnicodeLetter | '$' | '_' | '\' UnicodeEscapeSequence
IdentifierPart ::= IdentifierStart | UnicodeCombiningMark | UnicodeDigit | UnicodeConnectorPunctuation | <ZWNJ> | <ZWJ>
UnicodeLetter ::= <unicode Uppercase letter> | <unicode Lowercase letter> | <unicode Titlecase letter> | <unicode Modifier letter> | <unicode Other letter> | <unicode Letter number>
UnicodeCombiningMark = <unicode Non-spacing mark> | <unicode combining spacing mark>
UnicodeDigit ::= <unicode Decimal number>
UnicodeConnectorPunctuation ::= <unicode Connector punctuation>
DecimalIntegerLiteral ::= '0' | NonZeroDigit DecimalDigits?
DecimalDigits ::= DecimalDigit | DecimalDigits DecimalDigit
DecimalDigit ::= one of { '0'..'9' }
NonZeroDigit ::= one of { '1'..'9' }

------------------
Annex B Deviations
------------------

1. The assertions (?= ) and (?! ) are treated as though they have a surrounding non-capture group, and hence can be quantified.
   Other assertions are not quantifiable.

QuantifiableAssertion [added] ::= '(' '?' '=' Disjunction ')' | '(' '?' '!' Disjunction ')'
Assertion [replaced] ::= '^' | '$' | '\' 'b' | '\' 'B' | QuantifiableAssertion
Term ::= ... | QuantifiableAssertion Quantifier [added]

--------------------------------------------------------------------------------------------------------------------------------
Left factored
--------------------------------------------------------------------------------------------------------------------------------

Pattern ::= Disjunction
Disjunction ::= Alternative ('|' Alternative)*
    FOLLOW(Disjunction) = { <eof>, ')' }
Alternative ::= Term*
    FOLLOW(Alternative) = { <eof>, ')', '|' }
Term ::= ( '^' | '$' | '\' 'b' | '\' 'B' | '(' '?' '=' Disjunction ')' | '(' '?' '!' Disjunction ')' | Atom Quantifier? )
       | ( PatternCharacter | '.' | '\' AtomEscape | CharacterClass | '(' Disjunction ')' | '(' '?' ':' Disjunction ')' )
           ( '*' | '+' | '?' | '{' DecimalDigits (',' DecimalDigits?)? '}' ) '?'?
PatternCharacter ::= <unicode character> but not any of { '^', '$', '\', '.', '*', '+', '?', '(', ')', '[', ']', '{', '}', '|' }
AtomEscape ::= DecimalEscape | CharacterEscape | CharacterClassEscape
CharacterEscape ::= ControlEscape | 'c' ControlLetter | HexEscapeSequence | UnicodeEscapeSequence | IdentityEscape
ControlEscape ::= one of { 'f', 'n', 'r', 't', 'v' }
ControlLetter ::= one of { 'a'..'z', 'A'..'Z' }
IdentityEscape ::= <unicode character> but not <unicode Uppercase letter>, <unicode Lowercase letter>, <unicode Titlecase letter>, <unicode Modifier letter>, <unicode Other letter>, <unicode Letter number>, '$', '_', <unicode Non-spacing mark>, <unicode combining spacing mark>, <unicode Decimal number>, <unicode Connector punctuation>
DecimalEscape ::= DecimalIntegerLiteral [lookahead not in DecimalDigit]
CharacterClassEscape :: one of { 'd', 'D', 's', 'S', 'w', 'W' }
CharacterClass ::= '[' [lookahead not in {'^'}] ClassRanges ']' | '[' '^' ClassRanges ']'
ClassRanges ::= [empty] | NonemptyClassRanges
NonemptyClassRanges ::= ClassAtom | ClassAtom NonemptyClassRangesNoDash | ClassAtom '-' ClassAtom ClassRanges
NonemptyClassRangesNoDash ::= ClassAtom | ClassAtomNoDash NonemptyClassRangesNoDash | ClassAtomNoDash '-' ClassAtom ClassRanges
ClassAtom ::= '-' | ClassAtomNoDash
ClassAtomNoDash ::= SourceCharacter but not one of { '\', ']', '-' } | '\' ClassEscape
ClassEscape ::= DecimalEscape | 'b' | CharacterEscape | CharacterClassEscape
HexEscapeSequence ::= 'x' HexDigit{2}
UnicodeEscapeSequence ::= 'u' HexDigit{4} | 'u' '{' HexDigits{4-6} '}'
HexDigit ::= one of { '0'..'9', 'a'..'f', 'A'..'F' }
DecimalIntegerLiteral ::= '0' | NonZeroDigit DecimalDigits?
DecimalDigits ::= DecimalDigit+
DecimalDigit ::= one of { '0'..'9' }
NonZeroDigit ::= one of { '1'..'9' }

------------------
Annex B Deviations
------------------

QuantifiableAssertion [added] ::= '(' '?' '=' Disjunction ')' | '(' '?' '!' Disjunction ')'
Term ::= ... | '(' '?' '=' Disjunction ')' [removed] | '(' '?' '!' Disjunction ')' [removed] | QuantifiableAssertion Quantifier? [added]

*/

#include "ParserPch.h"

namespace UnifiedRegex
{
    ParseError::ParseError(bool isBody, CharCount pos, CharCount encodedPos, HRESULT error)
        : isBody(isBody), pos(pos), encodedPos(encodedPos), error(error)
    {
    }

    template <typename P, const bool IsLiteral>
    Parser<P, IsLiteral>::Parser
        ( Js::ScriptContext* scriptContext
        , ArenaAllocator* ctAllocator
        , StandardChars<EncodedChar>* standardEncodedChars
        , StandardChars<Char>* standardChars
        , bool isFromExternalSource
#if ENABLE_REGEX_CONFIG_OPTIONS
        , DebugWriter* w
#endif
        )
        : scriptContext(scriptContext)
        , ctAllocator(ctAllocator)
        , standardEncodedChars(standardEncodedChars)
        , standardChars(standardChars)
#if ENABLE_REGEX_CONFIG_OPTIONS
        , w(w)
#endif
        , input(0)
        , inputLim(0)
        , next(0)
        , inBody(false)
        , numGroups(1)
        , nextGroupId(1) // implicit overall group always takes index 0
        , litbuf(0)
        , litbufLen(0)
        , litbufNext(0)
        , surrogatePairList(nullptr)
        , currentSurrogatePairNode(nullptr)
        , tempLocationOfSurrogatePair(nullptr)
        , tempLocationOfRange(nullptr)
        , codePointAtTempLocation(0)
        , unicodeFlagPresent(false)
        , caseInsensitiveFlagPresent(false)
        , positionAfterLastSurrogate(nullptr)
        , valueOfLastSurrogate(INVALID_CODEPOINT)
        , deferredIfNotUnicodeError(nullptr)
        , deferredIfUnicodeError(nullptr)
    {
        if (isFromExternalSource)
            FromExternalSource();
    }

    //
    // Input buffer management
    //

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::SetPosition(const EncodedChar* input, const EncodedChar* inputLim, bool inBody)
    {
        this->input = input;
        this->inputLim = inputLim;
        next = input;
        this->inBody = inBody;
        RestoreMultiUnits(0);
    }

    template <typename P, const bool IsLiteral>
    inline CharCount Parser<P, IsLiteral>::Pos()
    {
        CharCount nextOffset = Chars<EncodedChar>::OSB(next, input);
        Assert(nextOffset >= m_cMultiUnits);
        return nextOffset - (CharCount)m_cMultiUnits;
    }

    template <typename P, const bool IsLiteral>
    inline bool Parser<P, IsLiteral>::IsEOF()
    {
        return next >= inputLim;
    }

    template <typename P, const bool IsLiteral>
    inline bool Parser<P, IsLiteral>::ECCanConsume(CharCount n = 1)
    {
        return next + n <= inputLim;
    }

    template <typename P, const bool IsLiteral>
    inline typename P::EncodedChar Parser<P, IsLiteral>::ECLookahead(CharCount n = 0)
    {
        // Ok to look ahead to terminating 0
        Assert(next + n <= inputLim);
        return next[n];
    }

    template <typename P, const bool IsLiteral>
    inline typename P::EncodedChar Parser<P, IsLiteral>::ECLookback(CharCount n = 0)
    {
        // Ok to look ahead to terminating 0
        Assert(n + input <= next);
        return *(next - n);
    }

    template <typename P, const bool IsLiteral>
    inline void Parser<P, IsLiteral>::ECConsume(CharCount n = 1)
    {
        Assert(next + n <= inputLim);
#if DBG
        for (CharCount i = 0; i < n; i++)
            Assert(!IsMultiUnitChar(next[i]));
#endif
        next += n;
    }

    template <typename P, const bool IsLiteral>
    inline void Parser<P, IsLiteral>::ECConsumeMultiUnit(CharCount n = 1)
    {
        Assert(next + n <= inputLim);
        next += n;
    }

    template <typename P, const bool IsLiteral>
    inline void Parser<P, IsLiteral>::ECRevert(CharCount n = 1)
    {
        Assert(n + input <= next);
        next -= n;
    }

    //
    // Helpers
    //
    template <typename P, const bool IsLiteral>
    int Parser<P, IsLiteral>::TryParseExtendedUnicodeEscape(Char& c, bool& previousSurrogatePart, bool trackSurrogatePair = false)
    {
        if (!scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
        {
            return 0;
        }

        if (!ECCanConsume(2) || ECLookahead(0) !='{' || !standardEncodedChars->IsHex(ECLookahead(1)))
        {
            return 0;
        }

        // The first character is mandatory to consume escape sequence, so we check for it above, at this stage we can set it as we already checked.
        codepoint_t codePoint = standardEncodedChars->DigitValue(ECLookahead(1));

        int i = 2;

        while(ECCanConsume(i + 1) && standardEncodedChars->IsHex(ECLookahead(i)))
        {
            codePoint <<= 4;
            codePoint += standardEncodedChars->DigitValue(ECLookahead(i));

            if (codePoint > 0x10FFFF)
            {
                return 0;
            }
            i++;
        }

        if(!ECCanConsume(i + 1) || ECLookahead(i) != '}')
        {
            return 0;
        }

        uint consumptionNumber = i + 1;
        Assert(consumptionNumber >= 3);

        if (!previousSurrogatePart && trackSurrogatePair && this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
        {
            // Current location
            TrackIfSurrogatePair(codePoint, (next - 1), consumptionNumber + 1);
        }

        wchar_t other;
        // Generally if this code point is a single character, then we take it and return.
        // If the character is made up of two characters then we emit the first and backtrack to the start of th escape sequence;
        // Following that we check if we have already seen the first character, and if so emit the second and consume the entire escape sequence.
        if (codePoint < 0x10000)
        {
            c = UTC(codePoint);
            ECConsumeMultiUnit(consumptionNumber);
        }
        else if (previousSurrogatePart)
        {
            previousSurrogatePart = false;
            Js::NumberUtilities::CodePointAsSurrogatePair(codePoint, &other, &c);
            ECConsumeMultiUnit(consumptionNumber);
        }
        else
        {
            previousSurrogatePart = true;
            Js::NumberUtilities::CodePointAsSurrogatePair(codePoint, &c, &other);
            Assert(ECLookback(1) == 'u' && ECLookback(2) == '\\');
            ECRevert(2);
        }

        return consumptionNumber;
    }

    // This function has the following 'knowledge':
    //     - A codepoint that might be part of a surrogate pair or part of one
    //     - A location where that codepoint is located
    //     - Previously tracked part of surrogate pair (tempLocationOfSurrogatePair, and codePointAtTempLocation), which is always a surrogate lower part.
    //     - A pointer to the current location of the linked list
    //     - If a previous location is tracked, then it is of a parsed character (not given character) before current, and not same to current
    //       This can't be asserted directly, and has to be followed by callers. Term pass can reset with each iteration, as well as this method in cases it needs.
    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>:: TrackIfSurrogatePair(codepoint_t codePoint,  const EncodedChar* location, uint32 consumptionLength)
    {
        Assert(codePoint < 0x110000);
        Assert(location != nullptr);
        Assert(location != this->tempLocationOfSurrogatePair);

        if (Js::NumberUtilities::IsSurrogateLowerPart(codePoint))
        {
            this->tempLocationOfSurrogatePair = location;
            this->codePointAtTempLocation = codePoint;
        }
        else
        {
            if(Js::NumberUtilities::IsSurrogateUpperPart(codePoint) && this->tempLocationOfSurrogatePair != nullptr)
            {
                Assert(Js::NumberUtilities::IsSurrogateLowerPart(codePointAtTempLocation));
                consumptionLength = (uint32)(location - this->tempLocationOfSurrogatePair) + consumptionLength;
                codePoint = Js::NumberUtilities::SurrogatePairAsCodePoint(codePointAtTempLocation, codePoint);
                location = this->tempLocationOfSurrogatePair;
            }
            // At this point we can clear previous location, and then if codePoint is bigger than 0xFFFF store it, as we either received it or combined it above
            this->tempLocationOfSurrogatePair = nullptr;
            this->codePointAtTempLocation = 0;
        }

        if (codePoint > 0xFFFF)
        {
            this->positionAfterLastSurrogate = location + consumptionLength;
            this->valueOfLastSurrogate = codePoint;

            // When parsing without AST we aren't given an allocator. In addition, only the 2 lines above are used during Pass 0;
            // while the bottom is used during Pass 1 (which isn't done when ParseNoAST)
            if(this->ctAllocator != nullptr)
            {
                SurrogatePairTracker* node = Anew(this->ctAllocator, SurrogatePairTracker, location, this->tempLocationOfRange, codePoint, consumptionLength, m_cMultiUnits);
                if (surrogatePairList == nullptr)
                {
                    Assert(currentSurrogatePairNode == nullptr);
                    surrogatePairList = node;
                    currentSurrogatePairNode = node;
                }
                else
                {
                    Assert(currentSurrogatePairNode != nullptr);
                    currentSurrogatePairNode->next = node;
                    currentSurrogatePairNode = node;
                }
            }
        }
    }
    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::CreateSurrogatePairAtom(wchar_t lower, wchar_t upper)
    {
        MatchLiteralNode * literalNode = Anew(this->ctAllocator, MatchLiteralNode, 0, 0);
        MatchCharNode lowerNode(lower);
        MatchCharNode upperNode(upper);
        AccumLiteral(literalNode, &lowerNode);
        AccumLiteral(literalNode, &upperNode);

        return literalNode;
    }

    // This function will create appropriate pairs of ranges and add them to the disjunction node.
    // Terms used in comments:
    //      - A minor codePoint is smaller than major codePoint, and both define a range of codePoints above 0x10000; to avoid confusion between lower/upper denoting codeUnits composing the surrogate pair.
    //      - A boundary is a mod 0x400 alignment marker due to the nature of surrogate pairs representation. So the codepoint 0x10300 lies between boundaries 0x10000 and 0x10400.
    //      - A prefix is the range set used to represent the values from minorCodePoint to the first boundary above minorCodePoint if applicable.
    //      - A suffix is the range set used to represent the values from first boundary below majorCodePoint to the majorCodePoint if applicable.
    //      - A full range is the range set used to represent the values from first boundary above minorCodePoint to first boundary below majorCodePoint if applicable.
    // The algorithm works as follows:
    //      1. Determine minorBoundary (minorCodePoint - mod 0x400 +0x400) and majorBoundary (majorCodePoint - mod 0x400). minorBoundary > minorCodePoint and majorBoundary < majorCodePoint
    //      2. Based on the codePoints and the boundaries, prefix, suffix, and full range is determined. Here are the rules:
    //          2-a. If minorBoundary > majorBoundary, we have an inner boundary range, output just that.
    //          2-b. If minorBoundary - 0x400u != minorCodepoint (i.e. codePoint doesn't lie right on a boundary to be part of a full range) we have a prefix.
    //          2-c. If majorBoundary + 0x3FFu != majorCodepoint (i.e. codePoint doesn't lie right before a boundary to be part of a full range) we have a suffix.
    //          2-d. We have a full range, if the two boundaries don't equal, OR the codePoints lie on the range boundaries opposite to what constitutes a prefix/suffix.
    // Visual representation for sample range 0x10300 - 0x10900
    //          |     [ _   |    ^ ]     |
    //      0x10000      0x10800      0x11000
    // [ ] - denote the actual range
    //  _  - minorBoundary
    //  ^  - majorBoundary
    //  |  - other boundaries
    // prefix is between [ and _
    // suffix is between ^ and ]
    // full range is between _ and ^
    template <typename P, const bool IsLiteral>
    AltNode* Parser<P, IsLiteral>::AppendSurrogateRangeToDisjunction(codepoint_t minorCodePoint, codepoint_t majorCodePoint, AltNode *lastAltNode)
    {
        Assert(minorCodePoint < majorCodePoint);
        Assert(minorCodePoint >= 0x10000u);
        Assert(majorCodePoint >= 0x10000u);
        Assert(scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled() && unicodeFlagPresent);

        wchar_t lowerMinorCodeUnit, upperMinorCodeUnit, lowerMajorCodeUnit, upperMajorCodeUnit;
        Js::NumberUtilities::CodePointAsSurrogatePair(minorCodePoint, &lowerMinorCodeUnit, &upperMinorCodeUnit);
        Js::NumberUtilities::CodePointAsSurrogatePair(majorCodePoint, &lowerMajorCodeUnit, &upperMajorCodeUnit);

        // These boundaries represent whole range boundaries, as in 0x10000, 0x10400, 0x10800 etc
        // minor boundary is the first boundary strictly above minorCodePoint
        // major boundary is the first boundary below or equal to majorCodePoint
        codepoint_t minorBoundary = minorCodePoint - (minorCodePoint % 0x400u) + 0x400u;
        codepoint_t majorBoundary = majorCodePoint - (majorCodePoint % 0x400u);

        Assert(minorBoundary >= 0x10000);
        Assert(majorBoundary >= 0x10000);

        AltNode* tailToAdd = nullptr;

        // If the minor boundary is higher than major boundary, that means we have a range within the boundary and is less than 0x400
        // Ex: 0x10430 - 0x10700 will have minor boundary of 0x10800 and major of 0x10400
        // This pair will be represented in single range set.
        const bool singleRange = minorBoundary > majorBoundary;
        if (singleRange)
        {
            Assert(majorCodePoint - minorCodePoint < 0x400u);
            Assert(lowerMinorCodeUnit == lowerMajorCodeUnit);

            MatchCharNode* lowerCharNode = Anew(ctAllocator, MatchCharNode, lowerMinorCodeUnit);
            MatchSetNode* setNode = Anew(ctAllocator, MatchSetNode, false, false);
            setNode->set.SetRange(ctAllocator, (Char)upperMinorCodeUnit, (Char)upperMajorCodeUnit);
            ConcatNode* concatNode = Anew(ctAllocator, ConcatNode, lowerCharNode, Anew(ctAllocator, ConcatNode, setNode, nullptr));

            tailToAdd = Anew(ctAllocator, AltNode, concatNode, nullptr);
        }
        else
        {
            Node* prefixNode = nullptr, *suffixNode = nullptr;
            const bool twoConsecutiveRanges = minorBoundary == majorBoundary;

            // For minorBoundary,
            if (minorBoundary - minorCodePoint == 1) // Single character in minor range
            {
                // The prefix is only a surrogate pair atom
                prefixNode = CreateSurrogatePairAtom(lowerMinorCodeUnit, upperMinorCodeUnit);
            }
            else if (minorCodePoint != minorBoundary - 0x400u) // Minor range isn't full
            {
                Assert(minorBoundary - minorCodePoint < 0x400u);
                MatchCharNode* lowerCharNode = Anew(ctAllocator, MatchCharNode, (Char)lowerMinorCodeUnit);
                MatchSetNode* upperSetNode = Anew(ctAllocator, MatchSetNode, false);
                upperSetNode->set.SetRange(ctAllocator, (Char)upperMinorCodeUnit, (Char)0xDFFFu);
                prefixNode = Anew(ctAllocator, ConcatNode, lowerCharNode, Anew(ctAllocator, ConcatNode, upperSetNode, nullptr));
            }
            else // Full minor range
            {
                minorBoundary -= 0x400u;
            }

            if (majorBoundary == majorCodePoint) // Single character in major range
            {
                // The suffix is only a surrogate pair atom
                suffixNode = CreateSurrogatePairAtom(lowerMajorCodeUnit, upperMajorCodeUnit);
                majorBoundary -= 0x400u;
            }
            else if (majorBoundary + 0x3FFu != majorCodePoint) // Major range isn't full
            {
                Assert(majorCodePoint - majorBoundary < 0x3FFu);
                MatchCharNode* lowerCharNode = Anew(ctAllocator, MatchCharNode, (Char)lowerMajorCodeUnit);
                MatchSetNode* upperSetNode = Anew(ctAllocator, MatchSetNode, false, false);
                upperSetNode->set.SetRange(ctAllocator, (Char)0xDC00u, (Char)upperMajorCodeUnit);
                suffixNode = Anew(ctAllocator, ConcatNode, lowerCharNode, Anew(ctAllocator, ConcatNode, upperSetNode, nullptr));
                majorBoundary -= 0x400u;
            }

            const bool nonFullConsecutiveRanges = twoConsecutiveRanges && prefixNode != nullptr && suffixNode != nullptr;
            if (nonFullConsecutiveRanges)
            {
                Assert(suffixNode != nullptr);
                Assert(minorCodePoint != minorBoundary - 0x400u);
                Assert(majorBoundary + 0x3FFu != majorCodePoint);

                // If the minor boundary is equal to major boundary, that means we have a cross boundary range that only needs 2 nodes for prefix/suffix.
                // We can only cross one boundary.
                Assert(majorCodePoint - minorCodePoint < 0x800u);
                tailToAdd = Anew(ctAllocator, AltNode, prefixNode, Anew(ctAllocator, AltNode, suffixNode, nullptr));
            }
            else
            {
                // We have 3 sets of ranges, comprising of prefix, full and suffix.
                Assert(majorCodePoint - minorCodePoint >= 0x400u);
                Assert((prefixNode != nullptr && suffixNode != nullptr) // Spanning more than two ranges
                    || (prefixNode == nullptr && minorBoundary == minorCodePoint) // Two consecutive ranges and the minor is full
                    || (suffixNode == nullptr && majorBoundary + 0x3FFu == majorCodePoint)); // Two consecutive ranges and the major is full

                Node* lowerOfFullRange;
                wchar_t lowerMinorBoundary, lowerMajorBoundary, ignore;
                Js::NumberUtilities::CodePointAsSurrogatePair(minorBoundary, &lowerMinorBoundary, &ignore);

                bool singleFullRange = majorBoundary == minorBoundary;
                if (singleFullRange)
                {
                    // The lower part of the full range is simple a surrogate lower char
                    lowerOfFullRange = Anew(ctAllocator, MatchCharNode, (Char)lowerMinorBoundary);
                }
                else
                {

                    Js::NumberUtilities::CodePointAsSurrogatePair(majorBoundary, &lowerMajorBoundary, &ignore);
                    MatchSetNode* setNode = Anew(ctAllocator, MatchSetNode, false, false);
                    setNode->set.SetRange(ctAllocator, (Char)lowerMinorBoundary, (Char)lowerMajorBoundary);
                    lowerOfFullRange = setNode;
                }
                MatchSetNode* fullUpperRange = Anew(ctAllocator, MatchSetNode, false, false);
                fullUpperRange->set.SetRange(ctAllocator, (Char)0xDC00u, (Char)0xDFFFu);

                // These are added in the following order [full] [prefix][suffix]
                // This is doing by prepending, so in reverse.
                if (suffixNode != nullptr)
                {
                    tailToAdd = Anew(ctAllocator, AltNode, suffixNode, tailToAdd);
                }
                if (prefixNode != nullptr)
                {
                    tailToAdd = Anew(ctAllocator, AltNode, prefixNode, tailToAdd);
                }
                tailToAdd = Anew(ctAllocator, AltNode, Anew(ctAllocator, ConcatNode, lowerOfFullRange, Anew(ctAllocator, ConcatNode, fullUpperRange, nullptr)), tailToAdd);
            }
        }

        if (lastAltNode != nullptr)
        {
            Assert(lastAltNode->tail == nullptr);
            lastAltNode->tail = tailToAdd;
        }

        return tailToAdd;
    }

    template <typename P, const bool IsLiteral>
    AltNode* Parser<P, IsLiteral>::AppendSurrogatePairToDisjunction(codepoint_t codePoint, AltNode *lastAltNode)
    {
        wchar_t lower, upper;
        Js::NumberUtilities::CodePointAsSurrogatePair(codePoint, &lower, &upper);

        AltNode* tailNode = Anew(ctAllocator, AltNode, CreateSurrogatePairAtom(lower, upper), nullptr);

        if (lastAltNode != nullptr)
        {
            lastAltNode->tail = tailNode;
        }

        return tailNode;
    }

    //
    // Errors
    //

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::Fail(HRESULT error)
    {
        throw ParseError(inBody, Pos(), Chars<EncodedChar>::OSB(next, input), error);
    }

    // This doesn't throw, but stores first error code for throwing later
    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::DeferredFailIfUnicode(HRESULT error)
    {
        Assert(this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled());
        if (this->deferredIfUnicodeError == nullptr)
        {
            this->deferredIfUnicodeError = Anew(ctAllocator, ParseError, inBody, Pos(), Chars<EncodedChar>::OSB(next, input), error);
        }
    }

    // This doesn't throw, but stores first error code for throwing later
    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::DeferredFailIfNotUnicode(HRESULT error)
    {
        Assert(this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled());
        if (this->deferredIfNotUnicodeError == nullptr)
        {
            this->deferredIfNotUnicodeError = Anew(ctAllocator, ParseError, inBody, Pos(), Chars<EncodedChar>::OSB(next, input), error);
        }
    }

    template <typename P, const bool IsLiteral>
    inline void Parser<P, IsLiteral>::ECMust(EncodedChar ec, HRESULT error)
    {
        // We never look for 0
        Assert(ec != 0);
        if (ECLookahead() != ec)
            Fail(error);
        ECConsume();
    }

    template <typename P, const bool IsLiteral>
    inline wchar_t Parser<P, IsLiteral>::NextChar()
    {
        Assert(!IsEOF());
        // Could be an embedded 0
        Char c = ReadFull<true>(next, inputLim);
        // No embedded newlines in literals
        if (IsLiteral && standardChars->IsNewline(c))
            Fail(ERRnoSlash);
        return c;
    }

    //
    // Patterns/Disjunctions/Alternatives
    //

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::PatternPass0()
    {
        this->positionAfterLastSurrogate = nullptr;
        this->deferredIfNotUnicodeError = nullptr;
        this->deferredIfUnicodeError = nullptr;
        DisjunctionPass0(0);
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::PatternPass1()
    {
        return DisjunctionPass1();
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::UnionNodes(Node* prev, Node* curr)
    {
        if (prev->tag == Node::MatchChar)
        {
            MatchCharNode* prevChar = (MatchCharNode*)prev;
            if (curr->tag == Node::MatchChar)
            {
                MatchCharNode* currChar = (MatchCharNode*)curr;
                if (prevChar->cs[0] == currChar->cs[0])
                    // Just ignore current node
                    return prevChar;
                else
                {
                    // Union chars into new set
                    MatchSetNode* setNode = Anew(ctAllocator, MatchSetNode, false);
                    setNode->set.Set(ctAllocator, prevChar->cs[0]);
                    setNode->set.Set(ctAllocator, currChar->cs[0]);
                    return setNode;
                }
            }
            else if (curr->tag == Node::MatchSet)
            {
                MatchSetNode* currSet = (MatchSetNode*)curr;
                if (currSet->isNegation)
                    // Can't merge
                    return 0;
                else
                {
                    // Union chars into new set
                    MatchSetNode* setNode = Anew(ctAllocator, MatchSetNode, false);
                    setNode->set.Set(ctAllocator, prevChar->cs[0]) ;
                    setNode->set.UnionInPlace(ctAllocator, currSet->set);
                    return setNode;
                }
            }
            else
                // Can't merge
                return 0;
        }
        else if (prev->tag == Node::MatchSet)
        {
            MatchSetNode* prevSet = (MatchSetNode*)prev;
            if (prevSet->isNegation)
                // Can't merge
                return 0;
            else if (curr->tag == Node::MatchChar)
            {
                MatchCharNode* currChar = (MatchCharNode*)curr;
                // Include char in prev set
                prevSet->set.Set(ctAllocator, currChar->cs[0]);
                return prevSet;
            }
            else if (curr->tag == Node::MatchSet)
            {
                MatchSetNode* currSet = (MatchSetNode*)curr;
                if (currSet->isNegation)
                    // Can't merge
                    return 0;
                else
                {
                    // Include chars in prev set
                    prevSet->set.UnionInPlace(ctAllocator, currSet->set);
                    return prevSet;
                }
            }
            else
                // Can't merge
                return 0;
        }
        else
            // Can't merge
            return 0;
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::DisjunctionPass0(int depth)
    {
        AlternativePass0(depth);
        while (true)
        {
            // Could be terminating 0
            if (ECLookahead() != '|')
                return;
            ECConsume();
            AlternativePass0(depth);
        }
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::DisjunctionPass1()
    {
        // Maintain the invariants:
        //  - alt lists have two or more items
        //  - alt list items are never alt lists (so we must inline them)
        //  - an alt list never contains two consecutive match-character/match-set nodes
        //    (so we must union consecutive items into a single set)
        Node* node = AlternativePass1();
        AltNode* last = 0;
        // First node may be an alternative
        if (node->tag == Node::Alt)
        {
            last = (AltNode*)node;
            while (last->tail != 0)
                last = last->tail;
        }
        while (true)
        {
            // Could be terminating 0
            if (ECLookahead() != '|')
                return node;
            ECConsume(); // '|'
            Node* next = AlternativePass1();
            AnalysisAssert(next != nullptr);
            Node* revisedPrev = UnionNodes(last == 0 ? node : last->head, next);
            if (revisedPrev != 0)
            {
                // Can merge next into previously seen alternative
                if (last == 0)
                    node = revisedPrev;
                else
                    last->head = revisedPrev;
            }
            else if (next->tag == Node::Alt)
            {
                AltNode* nextList = (AltNode*)next;
                // Append inner list to current list
                revisedPrev = UnionNodes(last == 0 ? node : last->head, nextList->head);
                if (revisedPrev != 0)
                {
                    // Can merge head of list into previously seen alternative
                    if (last ==0)
                        node = revisedPrev;
                    else
                        last->head = revisedPrev;
                    nextList = nextList->tail;
                }
                AnalysisAssert(nextList != nullptr);
                if (last == 0)
                    node = Anew(ctAllocator, AltNode, node, nextList);
                else
                    last->tail = nextList;
                while (nextList->tail != 0)
                    nextList = nextList->tail;
                last = nextList;
            }
            else
            {
                // Append node
                AltNode* cons = Anew(ctAllocator, AltNode, next, 0);
                if (last == 0)
                    node = Anew(ctAllocator, AltNode, node, cons);
                else
                    last->tail = cons;
                last = cons;
            }
        }
    }

    template <typename P, const bool IsLiteral>
    inline bool Parser<P, IsLiteral>::IsEndOfAlternative()
    {
        EncodedChar ec = ECLookahead();
        // Could be terminating 0, but embedded 0 is part of alternative
        return (ec == 0 && IsEOF()) || ec == ')' || ec == '|' || (IsLiteral && ec == '/');
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::EnsureLitbuf(CharCount size)
    {
        if (litbufLen - litbufNext < size)
        {
            CharCount newLen = max(litbufLen, initLitbufSize);
            while (newLen < litbufNext + size)
                newLen *= 2;
            litbuf = (Char*)ctAllocator->Realloc(litbuf, litbufLen * sizeof(Char), newLen * sizeof(Char));
            litbufLen = newLen;
        }
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::AccumLiteral(MatchLiteralNode* deferredLiteralNode, Node* charOrLiteralNode)
    {
        Assert(charOrLiteralNode->tag == Node::MatchChar || charOrLiteralNode->tag == Node::MatchLiteral);
        CharCount addLen = charOrLiteralNode->LiteralLength();
        Assert(addLen > 0);

        if (deferredLiteralNode->length == 0)
        {
            // Start a new literal
            EnsureLitbuf(addLen);
            deferredLiteralNode->offset = litbufNext;
            deferredLiteralNode->length = addLen;
            charOrLiteralNode->AppendLiteral(litbufNext, litbufLen, litbuf);
        }
        else if (deferredLiteralNode->offset + deferredLiteralNode->length == litbufNext)
        {
            // Keep growing the current literal
            EnsureLitbuf(addLen);
            charOrLiteralNode->AppendLiteral(litbufNext, litbufLen, litbuf);
            deferredLiteralNode->length += addLen;

        }
        else if (charOrLiteralNode->tag == Node::MatchLiteral && deferredLiteralNode->offset + deferredLiteralNode->length == ((MatchLiteralNode*)charOrLiteralNode)->offset)
        {
            // Absorb next literal into current literal since they are adjacent
            deferredLiteralNode->length += addLen;
        }
        else
        {
            // Abandon current literal and start a fresh one (leaves gap)
            EnsureLitbuf(deferredLiteralNode->length + addLen);
            js_wmemcpy_s(litbuf + litbufNext, litbufLen - litbufNext, litbuf + deferredLiteralNode->offset, deferredLiteralNode->length);
            deferredLiteralNode->offset = litbufNext;
            litbufNext += deferredLiteralNode->length;
            charOrLiteralNode->AppendLiteral(litbufNext, litbufLen, litbuf);
            deferredLiteralNode->length += addLen;
        }
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::FinalTerm(Node* node, MatchLiteralNode* deferredLiteralNode)
    {
        if (node == deferredLiteralNode)
        {
#if DBG
            if (deferredLiteralNode->length == 0)
                Assert(false);
#endif
            Assert(deferredLiteralNode->offset < litbufNext);
            Assert(deferredLiteralNode->offset + deferredLiteralNode->length <= litbufNext);
            if (deferredLiteralNode->length == 1)
            {
                node = Anew(ctAllocator, MatchCharNode, litbuf[deferredLiteralNode->offset]);
                if (deferredLiteralNode->offset + deferredLiteralNode->length == litbufNext)
                    // Reclaim last added character
                    litbufNext--;
                // else: leave a gap in the literal buffer
            }
            else
                node = Anew(ctAllocator, MatchLiteralNode, *deferredLiteralNode);
            deferredLiteralNode->offset = 0;
            deferredLiteralNode->length = 0;
        }
        return node;
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::AlternativePass0(int depth)
    {
        while (!IsEndOfAlternative())
            TermPass0(depth);
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::AlternativePass1()
    {
        if (IsEndOfAlternative())
            return Anew(ctAllocator, SimpleNode, Node::Empty);

        MatchCharNode deferredCharNode(0);
        MatchLiteralNode deferredLiteralNode(0, 0);

        // Maintain the invariants:
        //  - concat lists have two or more items
        //  - concat list items are never concat lists
        //  - a concat list never contains two consecutive match-character/match-literal nodes
        bool previousSurrogatePart = false;
        Node* node = TermPass1(&deferredCharNode, previousSurrogatePart);
        AnalysisAssert(node != nullptr);
        ConcatNode* last = 0;
        // First node may be a concat
        if (node->tag == Node::Concat)
        {
            last = (ConcatNode*)node;
            while (last->tail != 0)
                last = last->tail;
        }

        if (last == 0)
        {
            if (node->LiteralLength() > 0)
            {
                // Begin a new literal
                AccumLiteral(&deferredLiteralNode, node);
                node = &deferredLiteralNode;
            }
        }
        else
        {
            if (last->head->LiteralLength() > 0)
            {
                // Begin a new literal
                AccumLiteral(&deferredLiteralNode, last->head);
                last->head = &deferredLiteralNode;
            }
        }

        while (!IsEndOfAlternative())
        {
            Node* next = TermPass1(&deferredCharNode, previousSurrogatePart);
            AnalysisAssert(next != nullptr);
            if (next->LiteralLength() > 0)
            {
                // Begin a new literal or grow the existing literal
                AccumLiteral(&deferredLiteralNode, next);
                if (last == 0)
                {
                    if (node != &deferredLiteralNode)
                    {
                        // So far we have first item and the current literal
                        ConcatNode* cons = Anew(ctAllocator, ConcatNode, &deferredLiteralNode, 0);
                        node = Anew(ctAllocator, ConcatNode, node, cons);
                        last = cons;
                    }
                    // else: keep growing first literal
                }
                else
                {
                    if (last->head != &deferredLiteralNode)
                    {
                        // Append a new literal node
                        ConcatNode* cons = Anew(ctAllocator, ConcatNode, &deferredLiteralNode, 0);
                        last->tail = cons;
                        last = cons;
                    }
                    // else: keep growing current literal
                }
            }
            else if (next->tag == Node::Concat)
            {
                // Append this list to accumulated list
                ConcatNode* nextList = (ConcatNode*)next;
                if (nextList->head->LiteralLength() > 0 &&
                    ((last == 0 && node == &deferredLiteralNode) ||
                     (last != 0 && last->head == &deferredLiteralNode)))
                {
                    // Absorb the next character or literal into the current literal
                    // (may leave a gab in litbuf)
                    AccumLiteral(&deferredLiteralNode, nextList->head);
                    nextList = nextList->tail;
                    // List has at least two items
                    AnalysisAssert(nextList != 0);
                    // List should be in canonical form, so no consecutive chars/literals
                    Assert(nextList->head->LiteralLength() == 0);
                }
                if (last == 0)
                    node = Anew(ctAllocator, ConcatNode, FinalTerm(node, &deferredLiteralNode), nextList);
                else
                {
                    last->head = FinalTerm(last->head, &deferredLiteralNode);
                    last->tail = nextList;
                }
                while (nextList->tail != 0)
                    nextList = nextList->tail;
                last = nextList;
                // No outstanding literals
                Assert(deferredLiteralNode.length == 0);
                if (last->head->LiteralLength() > 0)
                {
                    // If the list ends with a literal, transfer it into deferredLiteralNode
                    // so we can continue accumulating (won't leave a gab in litbuf)
                    AccumLiteral(&deferredLiteralNode, last->head);
                    // Can discard MatchLiteralNode since it lives in compile-time allocator
                    last->head = &deferredLiteralNode;
                }
            }
            else
            {
                // Append this node to accumulated list
                ConcatNode* cons = Anew(ctAllocator, ConcatNode, next, 0);
                if (last == 0)
                    node = Anew(ctAllocator, ConcatNode, FinalTerm(node, &deferredLiteralNode), cons);
                else
                {
                    last->head = FinalTerm(last->head, &deferredLiteralNode);
                    last->tail = cons;
                }
                last = cons;
                // No outstanding literals
                Assert(deferredLiteralNode.length == 0);
            }
        }
        if (last == 0)
            node = FinalTerm(node, &deferredLiteralNode);
        else
            last->head = FinalTerm(last->head, &deferredLiteralNode);
        // No outstanding literals
        Assert(deferredLiteralNode.length == 0);

        return node;
    }

    //
    // Terms
    //

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::NewLoopNode(CharCount lower, CharCountOrFlag upper, bool isGreedy, Node* body)
    {
        //
        // NOTE: We'd like to represent r? (i.e. r{0,1}) as r|<empty> since the loop representation has high overhead.
        //       HOWEVER if r contains a group definition and could match empty then we must execute as a loop
        //       so that group bindings are correctly reset on no progress (e.g.: /(a*)?/.exec("")). Thus we defer
        //       this optimization until pass 4 of the optimizer, at which point we know whether r could match empty.
        //
        if (lower == 1 && upper == 1)
            return body;
        else if (lower == 0 && upper == 0)
            // Loop is equivalent to empty. If the loop body contains group definitions they will have already been
            // counted towards the overall number of groups. The matcher will initialize their contents to
            // undefined, and since the loop body would never execute the inner groups could never be updated from
            // undefined.
            return Anew(ctAllocator, SimpleNode, Node::Empty);
        else
            return Anew(ctAllocator, LoopNode, lower, upper, isGreedy, body);
    }

    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::AtQuantifier()
    {
        // Could be terminating 0
        switch (ECLookahead())
        {
        case '*':
        case '+':
        case '?':
            return true;
        case '{':
            {
                CharCount lookahead = 1;
                while (ECCanConsume(lookahead + 1) && standardEncodedChars->IsDigit(ECLookahead(lookahead)))
                    lookahead++;
                if (lookahead == 1 || !ECCanConsume(lookahead + 1))
                    return false;
                switch (ECLookahead(lookahead))
                {
                case ',':
                    lookahead++;
                    if (ECCanConsume(lookahead + 1) && ECLookahead(lookahead) == '}')
                        return true;
                    else
                    {
                        CharCount saved = lookahead;
                        while (ECCanConsume(lookahead + 1) && standardEncodedChars->IsDigit(ECLookahead(lookahead)))
                            lookahead++;
                        if (lookahead == saved)
                            return false;
                        return ECCanConsume(lookahead + 1) && ECLookahead(lookahead) == '}';
                    }
                case '}':
                    return true;
                default:
                    return false;
                }
            }
        default:
            return false;
        }
    }

    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::OptNonGreedy()
    {
        // Could be terminating 0
        if (ECLookahead() != '?')
            return true;
        ECConsume();
        return false;
    }

    template <typename P, const bool IsLiteral>
    CharCount Parser<P, IsLiteral>::RepeatCount()
    {
        CharCount n = 0;
        int digits = 0;
        while (true)
        {
            // Could be terminating 0
            EncodedChar ec = ECLookahead();
            if (!standardEncodedChars->IsDigit(ec))
            {
                if (digits == 0)
                    Fail(JSERR_RegExpSyntax);
                return n;
            }
            if (n > MaxCharCount / 10)
                Fail(JSERR_RegExpSyntax);
            n *= 10;
            if (n > MaxCharCount - standardEncodedChars->DigitValue(ec))
                Fail(JSERR_RegExpSyntax);
            n += standardEncodedChars->DigitValue(ec);
            digits++;
            ECConsume();
        }
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::TermPass0(int depth)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);
        // Either we have a location at the start, or the end, never both. As in between it should have been cleared if surrogate pair
        // Or must be cleared if we didn't perform the check
        bool clearLocationIfPresent = this->tempLocationOfSurrogatePair != nullptr;

        switch (ECLookahead())
        {
        case '^':
        case '$':
            ECConsume();
            return;
        case '\\':
            ECConsume();
            if (AtomEscapePass0())
                return;
            break;
        case '(':
            // Can't combine into a single codeunit because of group present
            this->tempLocationOfSurrogatePair = nullptr;
            this->codePointAtTempLocation = 0;
            clearLocationIfPresent = false;
            ECConsume();
            switch (ECLookahead())
            {
            case '?':
                if (!ECCanConsume(2))
                    Fail(JSERR_RegExpSyntax);
                switch (ECLookahead(1))
                {
                case '=':
                case '!':
                case ':':
                    ECConsume(2);
                    break;
                default:
                    numGroups++;
                    break;
                }
                break;
            default:
                numGroups++;
                break;
            }
            DisjunctionPass0(depth + 1);
            ECMust(')', JSERR_RegExpNoParen);
            break;
        case '.':
            ECConsume();
            break;
        case '[':
            ECConsume();
            this->tempLocationOfSurrogatePair = nullptr;
            this->codePointAtTempLocation = 0;
            this->tempLocationOfRange = next;
            CharacterClassPass0();
            this->tempLocationOfRange = nullptr;
            ECMust(']', JSERR_RegExpNoBracket);
            break;
        case ')':
        case '|':
            Fail(JSERR_RegExpSyntax);
            break;
        case ']':
        case '}':
            NextChar();
            break;
        case '*':
        case '+':
        case '?':
        case '{':
            if (AtQuantifier())
                Fail(JSERR_RegExpBadQuant);
            else
                ECConsume();
            break;
        case 0:
            if (IsEOF())
                // Terminating 0
                Fail(JSERR_RegExpSyntax);
            // else fall-through for embedded 0
        default:
            {
                const EncodedChar* current = next;
                Char c = NextChar();
                if (scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                {
                    TrackIfSurrogatePair(c, current, (uint32)(next - current));
                }
                // Closing '/' in literals should be caught explicitly
                Assert(!IsLiteral || c != '/');
            }
            break;
        }

        if (clearLocationIfPresent && this->tempLocationOfSurrogatePair != nullptr)
        {
            this->tempLocationOfSurrogatePair = nullptr;
            this->codePointAtTempLocation = 0;
        }

        if (AtQuantifier())
        {
            switch (ECLookahead())
            {
            case '*':
            case '+':
            case '?':
                ECConsume();
                OptNonGreedy();
                break;
            case '{':
                {
                    ECConsume();
                    CharCount lower = RepeatCount();
                    switch (ECLookahead())
                    {
                    case ',':
                        ECConsume();
                        if (ECLookahead() == '}')
                        {
                            ECConsume();
                            OptNonGreedy();
                        }
                        else
                        {
                            CharCount upper = RepeatCount();
                            if (upper < lower)
                                Fail(JSERR_RegExpSyntax);
                            Assert(ECLookahead() == '}');
                            ECConsume();
                            OptNonGreedy();
                        }
                        break;
                    case '}':
                        ECConsume();
                        OptNonGreedy();
                        break;
                    default:
                        Assert(false);
                        break;
                    }
                    break;
                }
            default:
                Assert(false);
                break;
            }
        }
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::TermPass1(MatchCharNode* deferredCharNode, bool& previousSurrogatePart)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        Node* node = 0;
        bool containsSurrogatePair = false;
        switch (ECLookahead())
        {
        case '^':
            ECConsume();
            return Anew(ctAllocator, SimpleNode, Node::BOL); // No quantifier allowed
        case '$':
            ECConsume();
            return Anew(ctAllocator, SimpleNode, Node::EOL); // No quantifier allowed
        case '\\':
            ECConsume();
            if(SurrogatePairPass1(node, deferredCharNode, previousSurrogatePart))
            {
                break; // For quantifier
            }
            else if (AtomEscapePass1(node, deferredCharNode, previousSurrogatePart))
            {
                return node; // No quantifier allowed
            }
            break; // else: fall-through for opt quantifier
        case '(':
            ECConsume();
            switch (ECLookahead())
            {
            case '?':
                switch (ECLookahead(1))
                {
                case '=':
                    ECConsume(2); // ?=
                    node = DisjunctionPass1();
                    Assert(ECLookahead() == ')');
                    ECConsume(); // )
                    node = Anew(ctAllocator, AssertionNode, false, node);
                    break; // As per Annex B, allow this to be quantifiable
                case '!':
                    ECConsume(2); // ?!
                    node = DisjunctionPass1();
                    Assert(ECLookahead() == ')');
                    ECConsume(); // )
                    node = Anew(ctAllocator, AssertionNode, true, node);
                    break; // As per Annex B, allow this to be quantifiable
                case ':':
                    ECConsume(2); // ?:
                    node = DisjunctionPass1();
                    Assert(ECLookahead() == ')');
                    ECConsume(); // )
                    break; // fall-through for opt quantifier
                default:
                    {
                        // ? not yet consumed
                        int thisGroupId = nextGroupId++;
                        node = DisjunctionPass1();
                        Assert(ECLookahead() == ')');
                        ECConsume(); // )
                        node = Anew(ctAllocator, DefineGroupNode, thisGroupId, node);
                        break; // fall-through for opt quantifier
                    }
                }
                break;
            default:
                {
                    // next char not yet consumed
                    int thisGroupId = nextGroupId++;
                    node = DisjunctionPass1();
                    Assert(ECLookahead() == ')');
                    ECConsume(); // )
                    node = Anew(ctAllocator, DefineGroupNode, thisGroupId, node);
                    break; // fall-through for opt quantifier
                }
            }
            break;
        case '.':
            {
                ECConsume();
                node = GetNodeWithValidCharacterSet('.');
                break; // fall-through for opt quantifier
            }
        case '[':
            ECConsume();
            if (unicodeFlagPresent)
            {
                containsSurrogatePair = this->currentSurrogatePairNode != nullptr && this->currentSurrogatePairNode->rangeLocation == next;
            }

            node = containsSurrogatePair ? CharacterClassPass1<true>() : CharacterClassPass1<false>();

            Assert(ECLookahead() == ']');
            ECConsume(); // ]
            break; // fall-through for opt quantifier
#if DBG
        case ')':
        case '|':
            Assert(false);
            break;
#endif
        case ']':
            // SPEC DEVIATION: This should be syntax error, instead accept as itself
            deferredCharNode->cs[0] = NextChar();
            node = deferredCharNode;
            break; // fall-through for opt quantifier
#if DBG
        case '*':
        case '+':
        case '?':
            AssertMsg(false, "Allowed only in the escaped form. These should be caught by TermPass0.");
            break;
#endif
        case 0:
            if (IsEOF())
                // Terminating 0
                Fail(JSERR_RegExpSyntax);
            // else fall-through for embedded 0
        default:
            if(SurrogatePairPass1(node, deferredCharNode, previousSurrogatePart))
            {
                break; //For quantifier
            }
            else
            {
                deferredCharNode->cs[0] = NextChar();
                node = deferredCharNode;
                break; // fall-through for opt quantifier
            }
        }

        Assert(node != 0);

        if (AtQuantifier())
        {
            switch (ECLookahead())
            {
            case '*':
                if (node == deferredCharNode)
                    node = Anew(ctAllocator, MatchCharNode, *deferredCharNode);
                ECConsume();
                return NewLoopNode(0, CharCountFlag, OptNonGreedy(), node);
            case '+':
                if (node == deferredCharNode)
                    node = Anew(ctAllocator, MatchCharNode, *deferredCharNode);
                ECConsume();
                return NewLoopNode(1, CharCountFlag, OptNonGreedy(), node);
            case '?':
                if (node == deferredCharNode)
                    node = Anew(ctAllocator, MatchCharNode, *deferredCharNode);
                ECConsume();
                return NewLoopNode(0, 1, OptNonGreedy(), node);
            case '{':
                {
                    if (node == deferredCharNode)
                        node = Anew(ctAllocator, MatchCharNode, *deferredCharNode);
                    ECConsume();
                    CharCount lower = RepeatCount();
                    switch (ECLookahead())
                    {
                    case ',':
                        ECConsume();
                        if (ECLookahead() == '}')
                        {
                            ECConsume();
                            return NewLoopNode(lower, CharCountFlag, OptNonGreedy(), node);
                        }
                        else
                        {
                            CharCount upper = RepeatCount();
                            Assert(lower <= upper);
                            Assert(ECLookahead() == '}');
                            ECConsume(); // }
                            return NewLoopNode(lower, upper, OptNonGreedy(), node);
                        }
                    case '}':
                        ECConsume();
                        return NewLoopNode(lower, lower, OptNonGreedy(), node);
                    default:
                        Assert(false);
                        break;
                    }
                    break;
                }
            default:
                Assert(false);
                break;
            }
        }

        return node;
    }

#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::AtomEscapePass0()
    {
        EncodedChar ec = ECLookahead();
        if (ec == 0 && IsEOF())
        {
            // Terminating 0
            Fail(JSERR_RegExpSyntax);
            return false;
        }
        else if (standardEncodedChars->IsDigit(ec))
        {
            do
            {
                ECConsume();
            }
            while (standardEncodedChars->IsDigit(ECLookahead())); // terminating 0 is not a digit
            return false;
        }
        else if (ECLookahead() == 'c')
        {
            if (standardEncodedChars->IsLetter(ECLookahead(1))) // terminating 0 is not a letter
                ECConsume(2);
            return false;
        }
        else
        {
            const EncodedChar *current = next;
            // An escaped '/' is ok
            Char c = NextChar();
            switch (c)
            {
            case 'b':
            case 'B':
                return true;
            // case 'c': handled as special case above
            case 'x':
                if (ECCanConsume(2) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)))
                    ECConsume(2);
                break;
            case 'u':
                bool surrogateEncountered = false;
                int lengthOfSurrogate = TryParseExtendedUnicodeEscape(c, surrogateEncountered, true);
                if (lengthOfSurrogate > 0)
                {
                    if (surrogateEncountered)
                    {
                        // If we don't have an allocator, we don't create nodes
                        // Asserts in place as extra checks for when we do have an allocator
                        Assert(this->ctAllocator == nullptr || this->currentSurrogatePairNode != nullptr);
                        Assert(this->ctAllocator == nullptr || current == this->currentSurrogatePairNode->location);
                        ECConsume(lengthOfSurrogate);
                    }
                    //Don't fall through
                    break;
                }
                else if (ECCanConsume(4) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)) &&
                    standardEncodedChars->IsHex(ECLookahead(2)) &&
                    standardEncodedChars->IsHex(ECLookahead(3)))
                {
                    if (this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                    {
                        codepoint_t value = (standardEncodedChars->DigitValue(ECLookahead(0)) << 12) |
                                            (standardEncodedChars->DigitValue(ECLookahead(1)) << 8) |
                                            (standardEncodedChars->DigitValue(ECLookahead(2)) << 4) |
                                            (standardEncodedChars->DigitValue(ECLookahead(3)));
                        TrackIfSurrogatePair(value, (next - 1), 5);
                    }
                    ECConsume(4);
                }
                break;
            }
            // embedded 0 is ok

            return false;
        }
    }
#pragma warning(pop)

    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::AtomEscapePass1(Node*& node, MatchCharNode* deferredCharNode, bool& previousSurrogatePart)
    {
        Assert(!IsEOF()); // checked for terminating 0 in pass 0
        if (standardEncodedChars->IsDigit(ECLookahead()))
        {
            // As per Annex B, allow octal escapes as well as group references, disambiguate based on known
            // number of groups.
            if (ECLookahead() == '0')
            {
                // fall through for octal
            }
            else
            {
                // Could be a group reference, but only if between 1 and 5 digits which resolve to a valid group number
                int n = 0;
                CharCount digits = 0;
                do
                {
                    n = n * 10 + (int)standardEncodedChars->DigitValue(ECLookahead(digits));
                    digits++;
                }
                while (digits < 5 && ECCanConsume(digits + 1) && standardEncodedChars->IsDigit(ECLookahead(digits)));
                if (n >= numGroups || ECCanConsume(digits + 1) && standardEncodedChars->IsDigit(ECLookahead(digits)))
                {
                    if (standardEncodedChars->IsOctal(ECLookahead()))
                    {
                        // fall through for octal
                    }
                    else
                    {
                        // \8 and \9 are identity escapes
                        deferredCharNode->cs[0] = Chars<EncodedChar>::CTW(ECLookahead());
                        ECConsume();
                        node = deferredCharNode;
                        return false; // not an assertion
                    }
                }
                else
                {
                    ECConsume(digits);
                    node = Anew(ctAllocator, MatchGroupNode, n);
                    return false; // not an assertion
                }
            }

            // Must be between 1 and 3 octal digits
            Assert(standardEncodedChars->IsOctal(ECLookahead())); // terminating 0 is not an octal
            uint n = 0;
            CharCount digits = 0;
            do
            {
                uint m = n * 8  + standardEncodedChars->DigitValue(ECLookahead());
                if (m > Chars<uint8>::MaxUChar) // Regex octal codes only support single byte (ASCII) characters.
                    break;
                n = m;
                ECConsume();
                digits++;
            }
            while (digits < 3 && standardEncodedChars->IsOctal(ECLookahead())); // terminating 0 is not an octal

            deferredCharNode->cs[0] = UTC((UChar)n);
            node = deferredCharNode;
            return false; // not an assertion
        }
        else if (ECLookahead() == 'c')
        {
            Char c;
            if (standardEncodedChars->IsLetter(ECLookahead(1))) // terminating 0 is not a letter
            {
                c = UTC(Chars<EncodedChar>::CTU(ECLookahead(1)) % 32);
                ECConsume(2);
            }
            else
            {
                // SPEC DEVIATION: For non-letters or EOF, take the leading '\' to be itself, and
                //                 don't consume the 'c' or letter.
                c = '\\';
            }
            deferredCharNode->cs[0] = c;
            node = deferredCharNode;
            return false; // not an assertion
        }
        else
        {
            Char c = NextChar();
            switch (c)
            {
            case 'b':
                node = Anew(ctAllocator, WordBoundaryNode, false);
                return true; // Is an assertion
            case 'B':
                node = Anew(ctAllocator, WordBoundaryNode, true);
                return true; // Is an assertion
            case 'f':
                c = '\f';
                break; // fall-through for identity escape
            case 'n':
                c = '\n';
                break; // fall-through for identity escape
            case 'r':
                c = '\r';
                break; // fall-through for identity escape
            case 't':
                c = '\t';
                break; // fall-through for identity escape
            case 'v':
                c = '\v';
                break; // fall-through for identity escape
            case 'd':
                {
                    MatchSetNode *setNode = Anew(ctAllocator, MatchSetNode, false, false);
                    standardChars->SetDigits(ctAllocator, setNode->set);
                    node = setNode;
                    return false; // not an assertion
                }
            case 'D':
                {
                    node = GetNodeWithValidCharacterSet('D');
                    return false; // not an assertion
                }
            case 's':
                {
                    MatchSetNode *setNode = Anew(ctAllocator, MatchSetNode, false, false);
                    standardChars->SetWhitespace(ctAllocator, setNode->set);
                    node = setNode;
                    return false; // not an assertion
                }
            case 'S':
                {
                    node = GetNodeWithValidCharacterSet('S');
                    return false; // not an assertion
                }
            case 'w':
                {
                    MatchSetNode *setNode = Anew(ctAllocator, MatchSetNode, false, false);
                    standardChars->SetWordChars(ctAllocator, setNode->set);
                    node = setNode;
                    return false; // not an assertion
                }
            case 'W':
                {
                    node = GetNodeWithValidCharacterSet('W');
                    return false; // not an assertion
                }
            // case 'c': handled as special case above
            case 'x':
                if (ECCanConsume(2) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)))
                {
                    c = UTC((standardEncodedChars->DigitValue(ECLookahead(0)) << 4) |
                        (standardEncodedChars->DigitValue(ECLookahead(1))));
                    ECConsume(2);
                    // fall-through for identity escape
                }
                // Take to be identity escape if ill-formed as per Annex B
                break;
            case 'u':
                if (unicodeFlagPresent && TryParseExtendedUnicodeEscape(c, previousSurrogatePart) > 0)
                    break;
                else if (ECCanConsume(4) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)) &&
                    standardEncodedChars->IsHex(ECLookahead(2)) &&
                    standardEncodedChars->IsHex(ECLookahead(3)))
                {
                    c = UTC((standardEncodedChars->DigitValue(ECLookahead(0)) << 12) |
                            (standardEncodedChars->DigitValue(ECLookahead(1)) << 8) |
                            (standardEncodedChars->DigitValue(ECLookahead(2)) << 4) |
                            (standardEncodedChars->DigitValue(ECLookahead(3))));
                    ECConsume(4);
                    // fall-through for identity escape
                }
                // Take to be identity escape if ill-formed as per Annex B
                break;
            default:
                // As per Annex B, allow anything other than newlines and above. Embedded 0 is ok
                break;
            }

            // Must be an identity escape
            deferredCharNode->cs[0] = c;
            node = deferredCharNode;
            return false; // not an assertion
        }
    }

    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::SurrogatePairPass1(Node*& node, MatchCharNode* deferredCharNode, bool& previousSurrogatePart)
    {
        if (!this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled() || !unicodeFlagPresent)
        {
            return false;
        }

        if (this->currentSurrogatePairNode != nullptr && this->currentSurrogatePairNode->location == this->next)
        {
            AssertMsg(!this->currentSurrogatePairNode->IsInsideRange(), "Should not be calling this pass if we are currently inside a range.");
            wchar_t lower, upper;

            uint tableIndex = 0, actualHigh = 0;
            codepoint_t equivClass[CaseInsensitive::EquivClassSize];

            if (caseInsensitiveFlagPresent && CaseInsensitive::RangeToEquivClass(tableIndex, this->currentSurrogatePairNode->value, this->currentSurrogatePairNode->value, actualHigh, equivClass))
            {
                Node *equivNode[CaseInsensitive::EquivClassSize];
                int indexForNextNode = 0;
                for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                {
                    bool alreadyAdded = false;

                    for (int j = 0; j < i; j++)
                    {
                        if (equivClass[i] == equivClass[j])
                        {
                            alreadyAdded = true;
                            break;
                        }
                    }

                    if (!alreadyAdded)
                    {
                        if (Js::NumberUtilities::IsInSupplementaryPlane(equivClass[i]))
                        {
                            Js::NumberUtilities::CodePointAsSurrogatePair(equivClass[i], &lower, &upper);
                            equivNode[indexForNextNode] = CreateSurrogatePairAtom(lower, upper);
                        }
                        else
                        {
                            equivNode[indexForNextNode] = Anew(ctAllocator, MatchCharNode, (Char)equivClass[i]);
                        }
                        indexForNextNode ++;
                    }
                }
                Assert(indexForNextNode > 0);
                if (indexForNextNode == 1)
                {
                    node = equivNode[0];
                }
                else
                {
                    AltNode *altNode = Anew(ctAllocator, AltNode, equivNode[0], nullptr);
                    AltNode *altNodeTail = altNode;
                    for (int i = 1; i < indexForNextNode; i++)
                    {
                        altNodeTail->tail = Anew(ctAllocator, AltNode, equivNode[i], nullptr);
                        altNodeTail = altNodeTail->tail;
                    }
                    node = altNode;
                }
            }
            else
            {
                Js::NumberUtilities::CodePointAsSurrogatePair(this->currentSurrogatePairNode->value, &lower, &upper);
                node = CreateSurrogatePairAtom(lower, upper);
            }

            previousSurrogatePart = false;

            Assert(ECCanConsume(this->currentSurrogatePairNode->length));
            ECConsumeMultiUnit(this->currentSurrogatePairNode->length);
            RestoreMultiUnits(this->currentSurrogatePairNode->multiUnits);
            this->currentSurrogatePairNode = this->currentSurrogatePairNode->next;

            return true;
        }

        return false;
    }

    //
    // Classes
    //

    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::AtSecondSingletonClassAtom()
    {
        Assert(ECLookahead() == '-');
        if (ECLookahead(1) == '\\')
        {
            switch (ECLookahead(2))
            {
            case 'd':
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                // These all denote non-singleton sets
                return false;
            default:
                // fall-through for singleton
                break;
            }
        }
        return true;
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::CharacterClassPass0()
    {
        // Could be terminating 0
        if (ECLookahead() == '^')
            ECConsume();

        EncodedChar nextChar = ECLookahead();
        const EncodedChar* current;
        codepoint_t lastCodepoint = INVALID_CODEPOINT;
        codepoint_t pendingRangeStart = INVALID_CODEPOINT;
        codepoint_t pendingRangeEnd = INVALID_CODEPOINT;
        bool previousSurrogatePart = false;
        while(nextChar != ']')
        {
            current = next;

            if (nextChar == '\0' && IsEOF())
            {
                // Report as unclosed '['
                Fail(JSERR_RegExpNoBracket);
                return;
            } // Otherwise embedded '\0' is ok
            else if (nextChar == '\\')
            {
                // Consume, as classescapepass0 expects for it to be consumed
                Char outChar = NextChar();
                // If previousSurrogatePart = true upon leaving this method, then we are going to pass through here twice
                // This is because \u{} escape sequence was encountered that is actually 2 characters, the second time we will pass consuming entire character
                if (ClassEscapePass0(outChar, previousSurrogatePart))
                {
                    lastCodepoint = outChar;
                }
                else
                {
                    // Last codepoint isn't a singleton, so no codepoint tracking for the sake of ranges is needed.
                    lastCodepoint = INVALID_CODEPOINT;
                    // Unless we have a possible range end, cancel our range tracking.
                    if (pendingRangeEnd == INVALID_CODEPOINT)
                    {
                        pendingRangeStart = INVALID_CODEPOINT;
                    }
                }
            }
            else if (nextChar == '-')
            {
                if (pendingRangeStart != INVALID_CODEPOINT || lastCodepoint == INVALID_CODEPOINT)
                {
                    // '-' is the upper part of the range, with pendingRangeStart codepoint is the lower. Set  lastCodePoint to '-' to check at the end of the while statement
                    // OR
                    // '-' is just a char, consume it and set as last char
                    lastCodepoint = NextChar();
                }
                else
                {
                    pendingRangeStart = this->next == this->positionAfterLastSurrogate
                        ? this->valueOfLastSurrogate
                        : lastCodepoint;
                    lastCodepoint = INVALID_CODEPOINT;
                    NextChar();
                }

                // If we have a pattern of the form [\ud800-\udfff], we need this to be interpreted as a range.
                // In order to achieve this, the two variables that we use to track surrogate pairs, namely
                // tempLocationOfSurrogatePair and previousSurrogatePart, need to be in a certain state.
                //
                // We need to reset tempLocationOfSurrogatePair as it points to the first Unicode escape (\ud800)
                // when we're here. We need to clear it in order not to have a surrogate pair when we process the
                // second escape (\udfff).
                //
                // previousSurrogatePart is used when we have a code point in the \u{...} extended format and the
                // character is in a supplementary plane. However, there is no need to change its value here. When
                // such an escape sequence is encountered, the first call to ClassEscapePass0() sets the variable
                // to true, but it rewinds the input back to the beginning of the escape sequence. The next
                // iteration of the loop here will again call ClassEscape0() with the same character and the
                // variable will this time be set to false. Therefore, the variable will always be false here.
                tempLocationOfSurrogatePair = nullptr;
                Assert(!previousSurrogatePart);
            }
            else
            {
                lastCodepoint = NextChar();

                if (scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                {
                    TrackIfSurrogatePair(lastCodepoint, current, (uint32)(next - current));
                }
            }

            if (!scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
            {
                // This is much easier to handle.
                if (pendingRangeStart != INVALID_CODEPOINT && lastCodepoint != INVALID_CODEPOINT)
                {
                    Assert(pendingRangeStart < 0x10000);
                    Assert(pendingRangeEnd == INVALID_CODEPOINT);
                    if (pendingRangeStart > lastCodepoint)
                    {
                        Fail(JSERR_RegExpBadRange);
                    }
                    pendingRangeStart = lastCodepoint = INVALID_CODEPOINT;
                }
            }
            // We have a candidate for range end, and we have a range start.
            else if (pendingRangeStart != INVALID_CODEPOINT && (lastCodepoint != INVALID_CODEPOINT || pendingRangeEnd != INVALID_CODEPOINT))
            {
                // The following will be true at the end of each surrogate pair parse.
                // Note, the escape sequence \u{} is a two time parse, so this will be true on the second time around.
                if (this->next == this->positionAfterLastSurrogate)
                {
                    lastCodepoint = this->valueOfLastSurrogate;
                    Assert(!previousSurrogatePart);
                    pendingRangeEnd = lastCodepoint;

                    lastCodepoint = INVALID_CODEPOINT;
                }
                // If we the next character is the end of range ']', then we can't have a surrogate pair.
                // The current character is the range end, if we don't already have a candidate.
                else if (ECLookahead() == ']' && pendingRangeEnd == INVALID_CODEPOINT)
                {
                    pendingRangeEnd = lastCodepoint;
                }

                //If we get here, and pendingRangeEnd is set. Then one of the above has caused it to be set, or the previous iteration of the loop.
                if (pendingRangeEnd != INVALID_CODEPOINT)
                {
                    wchar_t leftSingleChar, rightSingleChar, ignore;

                    if (pendingRangeStart >= 0x10000)
                    {
                        Js::NumberUtilities::CodePointAsSurrogatePair(pendingRangeStart, &ignore, &leftSingleChar);
                    }
                    else
                    {
                        leftSingleChar = (wchar_t)pendingRangeStart;
                    }

                    if (pendingRangeEnd >= 0x10000)
                    {
                        Js::NumberUtilities::CodePointAsSurrogatePair(pendingRangeEnd, &rightSingleChar, &ignore);
                    }
                    else
                    {
                        rightSingleChar = (wchar_t)pendingRangeEnd;
                    }

                    // Here it is a bit tricky, we don't know if we have a unicode option specified.
                    // If it is, then \ud800\udc00 - \ud800\udc01 is valid, otherwise invalid.
                    if (pendingRangeStart < 0x10000 && pendingRangeEnd < 0x10000 && pendingRangeStart > pendingRangeEnd)
                    {
                        Fail(JSERR_RegExpBadRange);
                    }
                    else
                    {
                        if(leftSingleChar > rightSingleChar)
                        {
                            DeferredFailIfNotUnicode(JSERR_RegExpBadRange);
                        }

                        if (pendingRangeStart > pendingRangeEnd)
                        {
                            DeferredFailIfUnicode(JSERR_RegExpBadRange);
                        }
                    }

                    pendingRangeStart = pendingRangeEnd = INVALID_CODEPOINT;
                }
                // The current char < 0x10000 is a candidate for the range end, but we need to iterate one more time.
                else
                {
                    pendingRangeEnd = lastCodepoint;
                }
            }
            nextChar = ECLookahead();
        }

        // We should never have a pendingRangeEnd set when we exit the loop
        Assert(pendingRangeEnd == INVALID_CODEPOINT);
    }


    template <typename P, const bool IsLiteral>
    template <bool containsSurrogates>
    Node* Parser<P, IsLiteral>::CharacterClassPass1()
    {
        Assert(containsSurrogates ? unicodeFlagPresent : true);

        CharSet<codepoint_t> codePointSet;

        MatchSetNode defferedSetNode(false, false);
        MatchCharNode defferedCharNode(0);

        bool isNegation = false;

        if (ECLookahead() == '^')
        {
            isNegation = true;
            ECConsume();
        }

        // We aren't expecting any terminating null characters, only embedded ones that should treated as valid characters.
        // CharacterClassPass0 should have taken care of terminating null.
        codepoint_t pendingCodePoint = INVALID_CODEPOINT;
        codepoint_t pendingRangeStart = INVALID_CODEPOINT;
        EncodedChar nextChar = ECLookahead();
        bool previousWasASurrogate = false;
        while(nextChar != ']')
        {
            codepoint_t codePointToSet = INVALID_CODEPOINT;

            // Consume ahead of time if we have two backslashes, both cases below (previously Tracked surrogate pair, and ClassEscapePass1) assume it is.
            if (nextChar == '\\')
            {
                ECConsume();
            }
            // These if-blocks are the logical ClassAtomPass1, they weren't grouped into a method to simplify dealing with multiple out parameters.
            if (containsSurrogates && this->currentSurrogatePairNode != nullptr && this->currentSurrogatePairNode->location == this->next)
            {
                codePointToSet = pendingCodePoint;

                pendingCodePoint = this->currentSurrogatePairNode->value;
                Assert(ECCanConsume(this->currentSurrogatePairNode->length));
                ECConsumeMultiUnit(this->currentSurrogatePairNode->length);
                RestoreMultiUnits(this->currentSurrogatePairNode->multiUnits);
                this->currentSurrogatePairNode = this->currentSurrogatePairNode->next;
            }
            else if (nextChar == '\\')
            {
                Node* returnedNode = ClassEscapePass1(&defferedCharNode, &defferedSetNode, previousWasASurrogate);

                if (returnedNode->tag == Node::MatchSet)
                {
                    codePointToSet = pendingCodePoint;
                    pendingCodePoint = INVALID_CODEPOINT;
                    if (pendingRangeStart != INVALID_CODEPOINT)
                    {
                        codePointSet.Set(ctAllocator, '-');
                    }
                    pendingRangeStart = INVALID_CODEPOINT;
                    codePointSet.UnionInPlace(ctAllocator, defferedSetNode.set);
                }
                else
                {
                    // Just a character
                    codePointToSet = pendingCodePoint;
                    pendingCodePoint = defferedCharNode.cs[0];
                }
            }
            else if (nextChar == '-')
            {
                if (pendingRangeStart != INVALID_CODEPOINT || pendingCodePoint == INVALID_CODEPOINT || ECLookahead(1) == ']')
                {
                    // - is just a char, or end of a range.
                    codePointToSet = pendingCodePoint;
                    pendingCodePoint = '-';
                    ECConsume();
                }
                else
                {
                    pendingRangeStart = pendingCodePoint;
                    ECConsume();
                }
            }
            else
            {
                // Just a character, consume it
                codePointToSet = pendingCodePoint;
                pendingCodePoint = NextChar();
            }

            if (codePointToSet != INVALID_CODEPOINT)
            {
                if (pendingRangeStart != INVALID_CODEPOINT)
                {
                    if (pendingRangeStart > pendingCodePoint)
                    {
                        //We have no unicodeFlag, but current range contains surrogates, thus we may end up having to throw a "Syntax" error here
                        //This breaks the notion of Pass0 check for valid syntax, because we don't know if we have a unicode option
                        Assert(!unicodeFlagPresent);
                        Fail(JSERR_RegExpBadRange);
                    }
                    codePointSet.SetRange(ctAllocator, pendingRangeStart, pendingCodePoint);
                    pendingRangeStart = pendingCodePoint = INVALID_CODEPOINT;
                }
                else
                {
                    codePointSet.Set(ctAllocator, codePointToSet);
                }
            }

            nextChar = ECLookahead();
        }

        if (pendingCodePoint != INVALID_CODEPOINT)
        {
            codePointSet.Set(ctAllocator, pendingCodePoint);
        }

        // At this point, we have a complete set of codepoints representing the range.
        // Before performing translation of any kind, we need to do some case filling.
        // At the point of this comment, there are no case mappings going cross-plane between simple
        // characters (< 0x10000) and supplementary characters (>= 0x10000)
        // However, it might still be the case, and this has to be handled.

        // On the other hand, we don't want to prevent optimizations that expect non-casefolded sets from happening.
        // At least for simple characters.

        // The simple case, is when the unicode flag isn't specified, we can go ahead and return the simple set.
        // Negations and case mappings will be handled later.
        if (!unicodeFlagPresent)
        {
            Assert(codePointSet.SimpleCharCount() == codePointSet.Count());
            MatchSetNode *simpleToReturn = Anew(ctAllocator, MatchSetNode, isNegation);
            codePointSet.CloneSimpleCharsTo(ctAllocator, simpleToReturn->set);
            return simpleToReturn;
        }

        // Everything past here must be under the flag
        Assert(scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled());

        if (codePointSet.IsEmpty())
        {
            return Anew(ctAllocator, MatchSetNode, false, false);
        }

        Node* prefixNode = nullptr;
        Node* suffixNode = nullptr;

        CharSet<codepoint_t> *toUseForTranslation = &codePointSet;

        // If a singleton, return a simple character
        bool isSingleton = !this->caseInsensitiveFlagPresent && !isNegation && codePointSet.IsSingleton();
        if (isSingleton)
        {
            codepoint_t singleton = codePointSet.Singleton();
            Node* toReturn = nullptr;

            if (singleton < 0x10000)
            {
                toReturn = Anew(ctAllocator, MatchCharNode, (wchar_t)singleton);
            }
            else
            {
                Assert(unicodeFlagPresent);
                wchar_t lowerSurrogate, upperSurrogate;
                Js::NumberUtilities::CodePointAsSurrogatePair(singleton, &lowerSurrogate, &upperSurrogate);
                toReturn = CreateSurrogatePairAtom(lowerSurrogate, upperSurrogate);
            }

            codePointSet.Clear(ctAllocator);
            return toReturn;
        }

        if (!this->caseInsensitiveFlagPresent)
        {
            // If negation, we want to complement the simple chars.
            // When a set is negated, optimizations skip checking if applicable, so we can go ahead and negate it here.
            CharSet<codepoint_t> negatedSet;

            if (isNegation)
            {
                // Complement all characters, and use it as the set toTranslate
                codePointSet.ToComplement(ctAllocator, negatedSet);
            }

            toUseForTranslation = isNegation ? &negatedSet : &codePointSet;

            if (isNegation)
            {
                // Clear this, as we will no longer need this.
                codePointSet.FreeBody(ctAllocator);
            }
        }
        else
        {
            CharSet<codepoint_t> caseEquivalent;
            codePointSet.ToEquivClass(ctAllocator, caseEquivalent);
            // Equiv set can't have a reduced count of chars
            Assert(caseEquivalent.Count() >= codePointSet.Count());

            // Here we have a regex that has both case insensitive and unicode options.
            // The range might also be negated. If it is negated, we can go ahead and negate
            // the entire set as well as fill in cases, as optimizations wouldn't kick in anyways.
            if (isNegation)
            {
                codePointSet.Clear(ctAllocator);
                caseEquivalent.ToComplement(ctAllocator, codePointSet);
                caseEquivalent.FreeBody(ctAllocator);
            }
            else
            {
                codePointSet.CloneFrom(ctAllocator, caseEquivalent);
            }

            Assert(toUseForTranslation == &codePointSet);
        }

        uint totalCodePointsCount = toUseForTranslation->Count();
        uint simpleCharsCount = toUseForTranslation->SimpleCharCount();
        if (totalCodePointsCount == simpleCharsCount)
        {
            MatchSetNode *simpleToReturn = Anew(ctAllocator, MatchSetNode, isNegation);
            toUseForTranslation->CloneSimpleCharsTo(ctAllocator, simpleToReturn->set);
            return simpleToReturn;
        }

        if  (simpleCharsCount > 0)
        {
            if (!toUseForTranslation->ContainSurrogateCodeUnits())
            {
                MatchSetNode *node = Anew(ctAllocator, MatchSetNode, false, false);
                toUseForTranslation->CloneSimpleCharsTo(ctAllocator, node->set);
                prefixNode = node;
            }
            else
            {
                MatchSetNode *node = Anew(ctAllocator, MatchSetNode, false, false);
                toUseForTranslation->CloneNonSurrogateCodeUnitsTo(ctAllocator, node->set);
                prefixNode = node;
                node = Anew(ctAllocator, MatchSetNode, false, false);
                toUseForTranslation->CloneSurrogateCodeUnitsTo(ctAllocator, node->set);
                suffixNode = node;
            }
        }

        Assert(unicodeFlagPresent);
        AltNode *headToReturn = prefixNode == nullptr ? nullptr : Anew(ctAllocator, AltNode, prefixNode, nullptr);
        AltNode *currentTail = headToReturn;

        codepoint_t charRangeSearchIndex = 0x10000, lowerCharOfRange = 0, upperCharOfRange = 0;

        while (toUseForTranslation->GetNextRange(charRangeSearchIndex, &lowerCharOfRange, &upperCharOfRange))
        {
            if (lowerCharOfRange == upperCharOfRange)
            {
                currentTail = this->AppendSurrogatePairToDisjunction(lowerCharOfRange, currentTail);
            }
            else
            {
                currentTail = this->AppendSurrogateRangeToDisjunction(lowerCharOfRange, upperCharOfRange, currentTail);
            }

            if (headToReturn == nullptr)
            {
                headToReturn = currentTail;
            }

            AnalysisAssert(currentTail != nullptr);
            while (currentTail->tail != nullptr)
            {
                currentTail = currentTail->tail;
            }
            charRangeSearchIndex = upperCharOfRange + 1;
        }

        if (suffixNode != nullptr)
        {
            currentTail->tail = Anew(ctAllocator, AltNode, suffixNode, nullptr);
        }
        toUseForTranslation->Clear(ctAllocator);

        if (headToReturn != nullptr && headToReturn->tail == nullptr)
        {
            return headToReturn->head;
        }
        return headToReturn;
    }

#pragma warning(push)
#pragma warning(disable:4702)   // unreachable code
    template <typename P, const bool IsLiteral>
    bool Parser<P, IsLiteral>::ClassEscapePass0(Char& singleton, bool& previousSurrogatePart)
    {
        // Could be terminating 0
        EncodedChar ec = ECLookahead();
        if (ec == 0 && IsEOF())
        {
            Fail(JSERR_RegExpSyntax);
            return false;
        }
        else if (standardEncodedChars->IsOctal(ec))
        {
            uint n = 0;
            CharCount digits = 0;
            do
            {
                uint m = n * 8  + standardEncodedChars->DigitValue(ECLookahead());
                if (m > Chars<uint8>::MaxUChar) //Regex octal codes only support single byte (ASCII) characters.
                    break;
                n = m;
                ECConsume();
                digits++;
            }
            while (digits < 3 && standardEncodedChars->IsOctal(ECLookahead())); // terminating 0 is not octal
            singleton = UTC((UChar)n);
            // Clear possible pair
            this->tempLocationOfSurrogatePair = nullptr;
            return true;
        }
        else
        {
            const EncodedChar* location = this->tempLocationOfSurrogatePair;
            // Clear it for now, otherwise to many branches to clear it on.
            this->tempLocationOfSurrogatePair = nullptr;
            // An escaped '/' is ok
            Char c = NextChar();
            switch (c)
            {
            case 'b':
                singleton = '\b';
                return true;
            case 'f':
                singleton = '\f';
                return true;
            case 'n':
                singleton = '\n';
                return true;
            case 'r':
                singleton = '\r';
                return true;
            case 't':
                singleton = '\t';
                return true;
            case 'v':
                singleton = '\v';
                return true;
            case 'd':
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                return false;
            case 'c':
                if (standardEncodedChars->IsLetter(ECLookahead())) // terminating 0 is not a letter
                {
                    singleton = UTC(Chars<EncodedChar>::CTU(ECLookahead()) % 32);
                    ECConsume();
                }
                else
                {
                    if (!IsEOF())
                    {
                        EncodedChar ec = ECLookahead();
                        switch (ec)
                        {
                        case '-':
                        case ']':
                            singleton = c;
                            break;
                        default:
                            singleton = UTC(Chars<EncodedChar>::CTU(ec) % 32);
                            ECConsume();
                            break;
                        }
                    }
                    else
                        singleton = c;
                }
                return true;
            case 'x':
                if (ECCanConsume(2) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)))
                {
                    singleton = UTC((standardEncodedChars->DigitValue(ECLookahead(0)) << 4) |
                            (standardEncodedChars->DigitValue(ECLookahead(1))));
                    ECConsume(2);
                }
                else
                    singleton = c;
                return true;
            case 'u':
                this->tempLocationOfSurrogatePair = location;
                if (this->TryParseExtendedUnicodeEscape(singleton, previousSurrogatePart, true) > 0)
                    return true;
                else if (ECCanConsume(4) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)) &&
                    standardEncodedChars->IsHex(ECLookahead(2)) &&
                    standardEncodedChars->IsHex(ECLookahead(3)))
                {
                    singleton = UTC((standardEncodedChars->DigitValue(ECLookahead(0)) << 12) |
                            (standardEncodedChars->DigitValue(ECLookahead(1)) << 8) |
                            (standardEncodedChars->DigitValue(ECLookahead(2)) << 4) |
                            (standardEncodedChars->DigitValue(ECLookahead(3))));
                    if (this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                    {
                        // Current location
                        TrackIfSurrogatePair(singleton, (next - 1), 5);
                    }
                    // The above if statement, if true, will clear tempLocationOfSurrogatePair if needs to.
                    ECConsume(4);
                }
                else
                    singleton = c;
                return true;
            default:
                // embedded 0 is ok
                singleton = c;
                return true;
            }
        }
    }
#pragma warning(pop)

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::ClassEscapePass1(MatchCharNode* deferredCharNode, MatchSetNode* deferredSetNode, bool& previousSurrogatePart)
    {
        // Checked for terminating 0 is pass 0
        Assert(!IsEOF());
        if (standardEncodedChars->IsOctal(ECLookahead()))
        {
            // As per Annex B, allow octal escapes instead of just \0 (and \8 and \9 are identity escapes).
            // Must be between 1 and 3 octal digits.
            uint n = 0;
            CharCount digits = 0;
            do
            {
                uint m = n * 8  + standardEncodedChars->DigitValue(ECLookahead());
                if (m > Chars<uint8>::MaxUChar) //Regex octal codes only support single byte (ASCII) characters.
                    break;
                n = m;
                ECConsume();
                digits++;
            }
            while (digits < 3 && standardEncodedChars->IsOctal(ECLookahead())); // terminating 0 is not octal
            deferredCharNode->cs[0] = UTC((UChar)n);
            return deferredCharNode;
        }
        else
        {
            Char c = NextChar();
            switch (c)
            {
            case 'b':
                c = '\b';
                break; // fall-through for identity escape
            case 'f':
                c = '\f';
                break; // fall-through for identity escape
            case 'n':
                c = '\n';
                break; // fall-through for identity escape
            case 'r':
                c = '\r';
                break; // fall-through for identity escape
            case 't':
                c = '\t';
                break; // fall-through for identity escape
            case 'v':
                c = '\v';
                break; // fall-through for identity escape
            case 'd':
                standardChars->SetDigits(ctAllocator, deferredSetNode->set);
                return deferredSetNode;
            case 'D':
                standardChars->SetNonDigits(ctAllocator, deferredSetNode->set);
                return deferredSetNode;
            case 's':
                standardChars->SetWhitespace(ctAllocator, deferredSetNode->set);
                return deferredSetNode;
            case 'S':
                standardChars->SetNonWhitespace(ctAllocator, deferredSetNode->set);
                return deferredSetNode;
            case 'w':
                standardChars->SetWordChars(ctAllocator, deferredSetNode->set);
                return deferredSetNode;
            case 'W':
                standardChars->SetNonWordChars(ctAllocator, deferredSetNode->set);
                return deferredSetNode;
            case 'c':
                if (standardEncodedChars->IsLetter(ECLookahead())) // terminating 0 is not a letter
                {
                    c = UTC(Chars<EncodedChar>::CTU(ECLookahead()) % 32);
                    ECConsume();
                    // fall-through for identity escape
                }
                else
                {
                    // SPEC DEVIATION: For non-letters, still take lower 5 bits, e.g. [\c1] == [\x11].
                    //                 However, '-', ']', and EOF make the \c just a 'c'.
                    if (!IsEOF())
                    {
                        EncodedChar ec = ECLookahead();
                        switch (ec)
                        {
                        case '-':
                        case ']':
                            // fall-through for identity escape with 'c'
                            break;
                        default:
                            c = UTC(Chars<EncodedChar>::CTU(ec) % 32);
                            ECConsume();
                            // fall-through for identity escape
                            break;
                        }
                    }
                    // else: fall-through for identity escape with 'c'
                }
                break;
            case 'x':
                if (ECCanConsume(2) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)))
                {
                    c = UTC((standardEncodedChars->DigitValue(ECLookahead(0)) << 4) |
                            (standardEncodedChars->DigitValue(ECLookahead(1))));
                    ECConsume(2);
                    // fall-through for identity escape
                }
                // Take to be identity escape if ill-formed as per Annex B
                break;
            case 'u':
                if (unicodeFlagPresent && TryParseExtendedUnicodeEscape(c, previousSurrogatePart) > 0)
                    break;
                else if (ECCanConsume(4) &&
                    standardEncodedChars->IsHex(ECLookahead(0)) &&
                    standardEncodedChars->IsHex(ECLookahead(1)) &&
                    standardEncodedChars->IsHex(ECLookahead(2)) &&
                    standardEncodedChars->IsHex(ECLookahead(3)))
                {
                    c = UTC((standardEncodedChars->DigitValue(ECLookahead(0)) << 12) |
                            (standardEncodedChars->DigitValue(ECLookahead(1)) << 8) |
                            (standardEncodedChars->DigitValue(ECLookahead(2)) << 4) |
                            (standardEncodedChars->DigitValue(ECLookahead(3))));
                    ECConsume(4);
                    // fall-through for identity escape
                }
                // Take to be identity escape if ill-formed as per Annex B.
                break;
            default:
                // As per Annex B, allow anything other than newlines and above. Embedded 0 is ok.
                break;
            }

            // Must be an identity escape
            deferredCharNode->cs[0] = c;
            return deferredCharNode;
        }
    }

    //
    // Options
    //

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::Options(RegexFlags& flags)
    {
        while (true)
        {
            // Could be terminating 0
            EncodedChar ec = ECLookahead();
            CharCount consume;
            Char c;

            if (ec == 0)
                // Embedded 0 not valid
                return;
            else if (IsLiteral &&
                ec == '\\' &&
                ECCanConsume(6) &&
                ECLookahead(1) == 'u' &&
                standardEncodedChars->IsHex(ECLookahead(2)) &&
                standardEncodedChars->IsHex(ECLookahead(3)) &&
                standardEncodedChars->IsHex(ECLookahead(4)) &&
                standardEncodedChars->IsHex(ECLookahead(5)))
            {
                if (scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                {
                    Fail(JSERR_RegExpSyntax);
                    return;
                }
                else
                {
                    uint32 n = (standardEncodedChars->DigitValue(ECLookahead(2)) << 12) |
                               (standardEncodedChars->DigitValue(ECLookahead(3)) << 8) |
                               (standardEncodedChars->DigitValue(ECLookahead(4)) << 4) |
                               (standardEncodedChars->DigitValue(ECLookahead(5)));
                    c = UTC(n);
                    consume = 6;
                }
            }
            else
            {
                c = Chars<EncodedChar>::CTW(ec);
                consume = 1;
            }

            switch (c) {
            case 'i':
                if ((flags & IgnoreCaseRegexFlag) != 0)
                {
                    Fail(JSERR_RegExpSyntax);
                }
                flags = (RegexFlags)(flags | IgnoreCaseRegexFlag);
                break;
            case 'g':
                if ((flags & GlobalRegexFlag) != 0)
                {
                    Fail(JSERR_RegExpSyntax);
                }
                flags = (RegexFlags)(flags | GlobalRegexFlag);
                break;
            case 'm':
                if ((flags & MultilineRegexFlag) != 0)
                {
                    Fail(JSERR_RegExpSyntax);
                }
                flags = (RegexFlags)(flags | MultilineRegexFlag);
                break;
            case 'u':
                // If we don't have unicode enabled, fall through to default
                if (scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                {
                    if ((flags & UnicodeRegexFlag) != 0)
                    {
                        Fail(JSERR_RegExpSyntax);
                    }
                    flags = (RegexFlags)(flags | UnicodeRegexFlag);
                    // For telemetry
                    CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(UnicodeRegexFlagCount, scriptContext);

                    break;
                }
            case 'y':
                if (scriptContext->GetConfig()->IsES6RegExStickyEnabled())
                {
                    if ((flags & StickyRegexFlag) != 0)
                    {
                        Fail(JSERR_RegExpSyntax);
                    }
                    flags = (RegexFlags)(flags | StickyRegexFlag);
                    // For telemetry
                    CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(StickyRegexFlagCount, scriptContext);

                    break;
                }
            default:
                if (standardChars->IsWord(c))
                {
                    // Outer context could never parse this character. Signal the syntax error as
                    // being part of the regex.
                    Fail(JSERR_RegExpSyntax);
                }
                return;
            }

            ECConsume(consume);
        }
    }

    //
    // Entry points
    //

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::ParseDynamic
        ( const EncodedChar* body
        , const EncodedChar* bodyLim
        , const EncodedChar* opts
        , const EncodedChar* optsLim
        , RegexFlags& flags )
    {
        Assert(!IsLiteral);
        Assert(body != 0);
        Assert(bodyLim >= body && *bodyLim == 0);
        Assert(opts == 0 || (optsLim >= opts && *optsLim == 0));

        // Body, pass 0
        SetPosition(body, bodyLim, true);

        PatternPass0();
        if (!IsEOF())
            Fail(JSERR_RegExpSyntax);

        // Options
        if (opts != 0)
        {
            SetPosition(opts, optsLim, false);
            Options(flags);
            if (!IsEOF())
                Fail(JSERR_RegExpSyntax);
            this->unicodeFlagPresent = (flags & UnifiedRegex::UnicodeRegexFlag) == UnifiedRegex::UnicodeRegexFlag;
            this->caseInsensitiveFlagPresent = (flags & UnifiedRegex::IgnoreCaseRegexFlag) == UnifiedRegex::IgnoreCaseRegexFlag;
            Assert(!this->unicodeFlagPresent || scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled());
        }
        else
        {
            this->unicodeFlagPresent = false;
            this->caseInsensitiveFlagPresent = false;
        }

        // If this HR has been set, that means we have an earlier failure than the one caught above.
        if (this->deferredIfNotUnicodeError != nullptr && !this->unicodeFlagPresent)
        {
            throw ParseError(*deferredIfNotUnicodeError);
        }
        else if(this->deferredIfUnicodeError != nullptr && this->unicodeFlagPresent)
        {
            throw ParseError(*deferredIfUnicodeError);
        }

        this->currentSurrogatePairNode = this->surrogatePairList;

        // Body, pass 1
        SetPosition(body, bodyLim, true);
        Node* root = PatternPass1();
        Assert(IsEOF());


        return root;
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::ParseLiteral
        ( const EncodedChar* input
        , const EncodedChar* inputLim
        , CharCount& outBodyEncodedChars
        , CharCount& outTotalEncodedChars
        , CharCount& outBodyChars
        , CharCount& outTotalChars
        , RegexFlags& flags )
    {
        Assert(IsLiteral);
        Assert(input != 0);
        Assert(inputLim >= input); // *inputLim need not be 0 because of deferred parsing

        // To handle surrogate pairs properly under unicode option, we will collect information on location of the pairs
        // during pass 0, regardless if the option is present. (We aren't able to get it at that time)
        // During pass 1, we will use that information to correctly create appropriate nodes.

        // Body, pass 0
        SetPosition(input, inputLim, true);

        PatternPass0();
        outBodyEncodedChars = Chars<EncodedChar>::OSB(next, input);
        outBodyChars = Pos();

        // Options are needed for the next pass
        ECMust('/', ERRnoSlash);
        Options(flags);
        this->unicodeFlagPresent = (flags & UnifiedRegex::UnicodeRegexFlag) == UnifiedRegex::UnicodeRegexFlag;
        this->caseInsensitiveFlagPresent = (flags & UnifiedRegex::IgnoreCaseRegexFlag) == UnifiedRegex::IgnoreCaseRegexFlag;
        Assert(!this->unicodeFlagPresent || scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled());

        // If this HR has been set, that means we have an earlier failure than the one caught above.
        if (this->deferredIfNotUnicodeError != nullptr && !this->unicodeFlagPresent)
        {
            throw ParseError(*deferredIfNotUnicodeError);
        }
        else if(this->deferredIfUnicodeError != nullptr && this->unicodeFlagPresent)
        {
            throw ParseError(*deferredIfUnicodeError);
        }

        // Used below to proceed to the end of the regex
        const EncodedChar *pastOptions = next;

        this->currentSurrogatePairNode = this->surrogatePairList;

        // Body, pass 1
        SetPosition(input, inputLim, true);
        Node* root = PatternPass1();
        Assert(outBodyEncodedChars == Chars<EncodedChar>::OSB(next, input));
        Assert(outBodyChars == Pos());

        next = pastOptions;
        outTotalEncodedChars = Chars<EncodedChar>::OSB(next, input);
        outTotalChars = Pos();

        return root;
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::ParseLiteralNoAST
        ( const EncodedChar* input
        , const EncodedChar* inputLim
        , CharCount& outBodyEncodedChars
        , CharCount& outTotalEncodedChars
        , CharCount& outBodyChars
        , CharCount& outTotalChars )
    {
        Assert(IsLiteral);
        Assert(input != 0);
        Assert(inputLim >= input); // *inputLim need not be 0 because of deferred parsing

        // Body, pass 0
        SetPosition(input, inputLim, true);
        PatternPass0();
        outBodyEncodedChars = Chars<EncodedChar>::OSB(next, input);
        outBodyChars = Pos();

        // Options
        ECMust('/', ERRnoSlash);
        RegexFlags dummyFlags = NoRegexFlags;
        Options(dummyFlags);
        this->unicodeFlagPresent = (dummyFlags & UnifiedRegex::UnicodeRegexFlag) == UnifiedRegex::UnicodeRegexFlag;
        this->caseInsensitiveFlagPresent = (dummyFlags & UnifiedRegex::IgnoreCaseRegexFlag) == UnifiedRegex::IgnoreCaseRegexFlag;
        outTotalEncodedChars = Chars<EncodedChar>::OSB(next, input);
        outTotalChars = Pos();

        // If this HR has been set, that means we have an earlier failure than the one caught above.
        if (this->deferredIfNotUnicodeError != nullptr && !this->unicodeFlagPresent)
        {
            throw ParseError(*deferredIfNotUnicodeError);
        }
        else if(this->deferredIfUnicodeError != nullptr && this->unicodeFlagPresent)
        {
            throw ParseError(*deferredIfUnicodeError);
        }
    }

    template <typename P, const bool IsLiteral>
    template <const bool buildAST>
    RegexPattern * Parser<P, IsLiteral>::CompileProgram
        ( Node* root,
          const EncodedChar*& currentCharacter,
          const CharCount totalLen,
          const CharCount bodyChars,
          const CharCount totalChars,
          const RegexFlags flags )
    {
        Assert(IsLiteral);

        Program* program = nullptr;

        if (buildAST)
        {
            const auto recycler = this->scriptContext->GetRecycler();
            program = Program::New(recycler, flags);
            this->CaptureSourceAndGroups(recycler, program, currentCharacter, bodyChars);
        }

        currentCharacter += totalLen;
        Assert(GetMultiUnits() == totalLen - totalChars);

        if (!buildAST)
        {
            return nullptr;
        }

        RegexPattern* pattern = RegexPattern::New(this->scriptContext, program, true);

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexStats* stats = 0;
        if (REGEX_CONFIG_FLAG(RegexProfile))
        {
            stats = this->scriptContext->GetRegexStatsDatabase()->GetRegexStats(pattern);
            this->scriptContext->GetRegexStatsDatabase()->EndProfile(stats, RegexStats::Parse);
        }
        if (REGEX_CONFIG_FLAG(RegexTracing))
        {
            DebugWriter* tw = this->scriptContext->GetRegexDebugWriter();
            tw->Print(L"// REGEX COMPILE ");
            pattern->Print(tw);
            tw->EOL();
        }
        if (REGEX_CONFIG_FLAG(RegexProfile))
            this->scriptContext->GetRegexStatsDatabase()->BeginProfile();
#endif

        ArenaAllocator* rtAllocator = this->scriptContext->RegexAllocator();
        Compiler::Compile
            ( this->scriptContext
              , ctAllocator
              , rtAllocator
              , standardChars
              , program
              , root
              , this->GetLitbuf()
              , pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
              , w
              , stats
#endif
                );

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (REGEX_CONFIG_FLAG(RegexProfile))
            this->scriptContext->GetRegexStatsDatabase()->EndProfile(stats, RegexStats::Compile);
#endif

#ifdef PROFILE_EXEC
        this->scriptContext->ProfileEnd(Js::RegexCompilePhase);
#endif

        return pattern;
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::CaptureEmptySourceAndNoGroups(Program* program)
    {
        Assert(program->source == 0);

        program->source = L"";
        program->sourceLen = 0;

        program->numGroups = 1;

        // Remaining to set during compilation: litbuf, litbufLen, numLoops, insts, instsLen, entryPointLabel
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::CaptureSourceAndGroups(Recycler* recycler, Program* program, const EncodedChar* body, CharCount bodyChars)
    {
        Assert(program->source == 0);
        Assert(body != 0);

        // Program will own source string
        program->source = RecyclerNewArrayLeaf(recycler, Char, bodyChars + 1);
        // Don't need to zero out since we're writing to the buffer right here
        ConvertToUnicode(program->source, bodyChars, body);
        program->source[bodyChars] = 0;
        program->sourceLen = bodyChars;

        program->numGroups = nextGroupId;

        // Remaining to set during compilation: litbuf, litbufLen, numLoops, insts, instsLen, entryPointLabel
    }

    template <typename P, const bool IsLiteral>
    Node* Parser<P, IsLiteral>::GetNodeWithValidCharacterSet(EncodedChar cc)
    {
        Node* nodeToReturn = nullptr;
        if (this->scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled() && this->unicodeFlagPresent)
        {
            MatchSetNode *lowerRangeNode = Anew(ctAllocator, MatchSetNode, false, false);
            lowerRangeNode->set.SetRange(ctAllocator, (Char)0xD800, (Char)0xDBFF);
            MatchSetNode *upperRangeNode = Anew(ctAllocator, MatchSetNode, false, false);
            upperRangeNode->set.SetRange(ctAllocator, (Char)0xDC00, (Char)0xDFFF);

            ConcatNode* surrogateRangePairNode = Anew(ctAllocator, ConcatNode, lowerRangeNode, Anew(ctAllocator, ConcatNode, upperRangeNode, nullptr));

            // The MatchSet node will be split into [0-D7FFDC00-FFFF] (minus special characters like newline, whitespace, etc.) as a prefix, and a suffix of [D800-DBFF]
            // i.e. The MatchSet node can be with [0-D7FFDC00-FFFF] (minus special characters like newline, whitespace, etc.) OR [D800-DBFF]
            MatchSetNode* partialPrefixSetNode = Anew(ctAllocator, MatchSetNode, false, false);
            switch (cc)
            {
            case '.':
                standardChars->SetNonNewline(ctAllocator, partialPrefixSetNode->set);
                break;
            case 'S':
                standardChars->SetNonWhitespace(ctAllocator, partialPrefixSetNode->set);
                break;
            case 'D':
                standardChars->SetNonDigits(ctAllocator, partialPrefixSetNode->set);
                break;
            case 'W':
                standardChars->SetNonWordChars(ctAllocator, partialPrefixSetNode->set);
                break;
            default:
                AssertMsg(false, "");
            }

            partialPrefixSetNode->set.SubtractRange(ctAllocator, (Char)0xD800u, (Char)0xDBFFu);

            MatchSetNode* partialSuffixSetNode = Anew(ctAllocator, MatchSetNode, false, false);
            partialSuffixSetNode->set.SetRange(ctAllocator, (Char)0xD800u, (Char)0xDBFFu);

            AltNode* altNode = Anew(ctAllocator, AltNode, partialPrefixSetNode, Anew(ctAllocator, AltNode, surrogateRangePairNode, Anew(ctAllocator, AltNode, partialSuffixSetNode, nullptr)));
            nodeToReturn = altNode;
        }
        else
        {
            MatchSetNode* setNode = Anew(ctAllocator, MatchSetNode, false, false);
            switch (cc)
            {
            case '.':
                standardChars->SetNonNewline(ctAllocator, setNode->set);
                break;
            case 'S':
                standardChars->SetNonWhitespace(ctAllocator, setNode->set);
                break;
            case 'D':
                standardChars->SetNonDigits(ctAllocator, setNode->set);
                break;
            case 'W':
                standardChars->SetNonWordChars(ctAllocator, setNode->set);
                break;
            default:
                AssertMsg(false, "");
            }
            nodeToReturn = setNode;
        }

        return nodeToReturn;
    }

    template <typename P, const bool IsLiteral>
    void Parser<P, IsLiteral>::FreeBody()
    {
        if (litbuf != 0)
        {
            ctAllocator->Free(litbuf, litbufLen);
            litbuf = 0;
            litbufLen = 0;
            litbufNext = 0;
        }
    }

    //
    // Template instantiation
    //

    template <typename P, const bool IsLiteral>
    void UnifiedRegexParserForceInstantiation()
    {
        typedef typename P::EncodedChar EncodedChar;
        Parser<P, IsLiteral> p
            ( 0
            , 0
            , 0
            , 0
            , false
#if ENABLE_REGEX_CONFIG_OPTIONS
            , 0
#endif
            );

        RegexFlags f;
        CharCount a, b, c, d;
        const EncodedChar* cp = 0;
        p.ParseDynamic(0, 0, 0, 0, f);
        p.ParseLiteral(0, 0, a, b, c, d, f);
        p.ParseLiteralNoAST(0, 0, a, b, c, d);
        p.CompileProgram<true>(0, cp, a, b, c, f);
        p.CompileProgram<false>(0, cp, a, b, c, f);
        p.CaptureEmptySourceAndNoGroups(0);
        p.CaptureSourceAndGroups(0, 0, 0, 0);
        p.FreeBody();
    }

    void UnifiedRegexParserForceAllInstantiations()
    {
        UnifiedRegexParserForceInstantiation<NullTerminatedUnicodeEncodingPolicy, false>();
        UnifiedRegexParserForceInstantiation<NullTerminatedUnicodeEncodingPolicy, true>();
        UnifiedRegexParserForceInstantiation<NullTerminatedUTF8EncodingPolicy, false>();
        UnifiedRegexParserForceInstantiation<NullTerminatedUTF8EncodingPolicy, true>();
        UnifiedRegexParserForceInstantiation<NotNullTerminatedUTF8EncodingPolicy, false>();
        UnifiedRegexParserForceInstantiation<NotNullTerminatedUTF8EncodingPolicy, true>();
    }
}

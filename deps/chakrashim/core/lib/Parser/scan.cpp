//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

/*****************************************************************************
*
*  The following table speeds various tests of characters, such as whether
*  a given character can be part of an identifier, and so on.
*/

int CountNewlines(LPCOLESTR psz, int cch)
{
    int cln = 0;

    while (0 != *psz && 0 != cch--)
    {
        switch (*psz++)
        {
        case OLESTR('\xD'):
            if (*psz == OLESTR('\xA'))
            {
                ++psz;
                if (0 == cch--)
                    break;
            }
            // fall-through
        case OLESTR('\xA'):
            cln++;
            break;
        }
    }

    return cln;
}

template< typename CharT >
struct AorW
{
};

// Specialization for UTF8Char
template<>
struct AorW< UTF8Char >
{
    // Expressing the args as "arrays of size N" ensures that the both args
    // are the same length. If not, we get a compile time error.
    template< size_t N >
    static const UTF8Char* Choose( const char (&a)[N], const wchar_t (&w)[N] )
    {
        // The reinterpret_cast is necessary to go from signed to unsigned char
        return reinterpret_cast< const UTF8Char* >(a);
    }

    template< size_t N >
    static const bool Test(const char (&a)[N], const wchar_t (&w)[N], LPCUTF8 value)
    {
        return 0 == memcmp(a, value, (N - 1) * sizeof(utf8char_t));
    }

    template< size_t N >
    static const bool Test(const char (&a)[N], const wchar_t (&w)[N], LPCUTF8 start, LPCUTF8 end)
    {
        return (end - start == N - 1) && (0 == memcmp(a, start, (N - 1) * sizeof(utf8char_t)));
    }
};

// Specialization for OLECHAR
template<>
struct AorW< OLECHAR >
{
    template< size_t N >
    static const wchar_t* Choose( const char (&a)[N], const wchar_t (&w)[N] )
    {
        return w;
    }

    template < size_t N >
    static bool Test(const char (&a)[N], const wchar_t (&w)[N], const wchar_t *value)
    {
        return 0 == memcmp(w, value, (N - 1) * sizeof(wchar_t));
    }

    template < size_t N >
    static bool Test(const char (&a)[N], const wchar_t (&w)[N], const wchar_t *start, const wchar_t *end)
    {
        return (end - start == N - 1) && (0 == memcmp(w, start, (N - 1) * sizeof(wchar_t)));
    }
};

BOOL Token::IsKeyword() const
{
    // keywords (but not future reserved words)
    return (tk <= tkYIELD);
}

tokens Token::SetRegex(UnifiedRegex::RegexPattern *const pattern, Parser *const parser)
{
    Assert(parser);

    if(pattern)
        parser->RegisterRegexPattern(pattern);
    this->u.pattern = pattern;
    return tk = tkRegExp;
}

IdentPtr Token::CreateIdentifier(HashTbl * hashTbl)
{
    Assert(this->u.pid == nullptr);
    if (this->u.pchMin)
    {
        Assert(IsIdentifier());
        IdentPtr pid = hashTbl->PidHashNameLen(this->u.pchMin, this->u.length);
        this->u.pid = pid;
        return pid;
    }

    Assert(IsReservedWord());

    IdentPtr pid = hashTbl->PidFromTk(tk);
    this->u.pid = pid;
    return pid;
}

template <typename EncodingPolicy>
Scanner<EncodingPolicy>::Scanner(Parser* parser, HashTbl *phtbl, Token *ptoken, ErrHandler *perr, Js::ScriptContext* scriptContext)
{
    AssertMem(phtbl);
    AssertMem(ptoken);
    AssertMem(perr);
    m_parser = parser;
    m_phtbl = phtbl;
    m_ptoken = ptoken;
    m_cMinLineMultiUnits = 0;
    m_perr = perr;
    m_fHadEol = FALSE;

    m_doubleQuoteOnLastTkStrCon = FALSE;
    m_OctOrLeadingZeroOnLastTKNumber = false;

    m_fStringTemplateDepth = 0;

    m_scanState = ScanStateNormal;
    m_scriptContext = scriptContext;

    m_line = 0;
    m_startLine = 0;
    m_pchStartLine = NULL;

    m_ichMinError = 0;
    m_ichLimError = 0;

    m_tempChBuf.m_pscanner = this;
    m_tempChBufSecondary.m_pscanner = this;

    m_iecpLimTokPrevious = (size_t)-1;

    this->charClassifier = scriptContext->GetCharClassifier();

    this->es6UnicodeMode = scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled();

    m_fYieldIsKeyword = false;
    m_fAwaitIsKeyword = false;
}

template <typename EncodingPolicy>
Scanner<EncodingPolicy>::~Scanner(void)
{
}

/*****************************************************************************
*
*  Initializes the scanner to prepare to scan the given source text.
*/
template <typename EncodingPolicy>
void Scanner<EncodingPolicy>::SetText(EncodedCharPtr pszSrc, size_t offset, size_t length, charcount_t charOffset, ULONG grfscr, ULONG lineNumber)
{
    // Save the start of the script and add the offset to get the point where we should start scanning.
    m_pchBase = pszSrc;
    m_pchLast = m_pchBase + offset + length;
    m_pchPrevLine = m_currentCharacter = m_pchMinLine = m_pchMinTok = pszSrc + offset;

    RestoreMultiUnits(offset - charOffset);

    // Absorb any byte order mark at the start
    if(offset == 0)
    {
        switch( PeekFull(m_currentCharacter, m_pchLast) )
        {
        case 0xFFEE:    // "Opposite" endian BOM
            // We do not support big-endian encodings
            // fall-through

        case 0xFEFF:    // "Correct" BOM
            ReadFull<true>(m_currentCharacter, m_pchLast);
            break;
        }
    }

    m_line = lineNumber;
    m_startLine = lineNumber;
    m_pchStartLine = m_currentCharacter;
    m_ptoken->tk = tkNone;
    m_fHtmlComments = (grfscr & fscrHtmlComments) != 0;
    m_fHadEol = FALSE;
    m_fSyntaxColor = (grfscr & fscrSyntaxColor) != 0;
    m_DeferredParseFlags = ScanFlagNone;
}

template <typename EncodingPolicy>
void Scanner<EncodingPolicy>::PrepareForBackgroundParse(Js::ScriptContext *scriptContext)
{
    scriptContext->GetThreadContext()->GetStandardChars((EncodedChar*)0);
    scriptContext->GetThreadContext()->GetStandardChars((wchar_t*)0);
}

//-----------------------------------------------------------------------------
// Number of code points from 'first' up to, but not including the next
// newline character, embedded NUL, or 'last', depending on which comes first.
//
// This is used to determine a length of BSTR, which can't contain a NUL character.
//-----------------------------------------------------------------------------
template <typename EncodingPolicy>
charcount_t Scanner<EncodingPolicy>::LineLength(EncodedCharPtr first, EncodedCharPtr last)
{
    charcount_t result = 0;
    EncodedCharPtr p = first;

    for (;;)
    {
        switch( ReadFull<false>(p, last) )
        {
            case kchNWL: // _C_NWL
            case kchRET:
            case kchLS:
            case kchPS:
            case kchNUL: // _C_NUL
                return result;
        }
        result++;
    }
}

template <typename EncodingPolicy>
charcount_t Scanner<EncodingPolicy>::UpdateLine(long &line, EncodedCharPtr start, EncodedCharPtr last, charcount_t ichStart, charcount_t ichEnd)
{
    EncodedCharPtr p = start;
    charcount_t ich = ichStart;
    long current = line;
    charcount_t lastStart = ichStart;

    while (ich < ichEnd)
    {
        ich++;
        switch (ReadFull<false>(p, last))
        {
        case kchRET:
            if (PeekFull(p, last) == kchNWL)
            {
                ich++;
                ReadFull<false>(p, last);
            }
            // fall-through

        case kchNWL:
        case kchLS:
        case kchPS:
            current++;
            lastStart = ich;
            break;

        case kchNUL:
            goto done;
        }
    }

done:
    line = current;
    return lastStart;
}

template <typename EncodingPolicy>
bool Scanner<EncodingPolicy>::TryReadEscape(EncodedCharPtr& startingLocation, EncodedCharPtr endOfSource, codepoint_t *outChar)
{
    Assert(outChar != nullptr);
    Assert(startingLocation <= endOfSource);

    EncodedCharPtr currentLocation = startingLocation;
    codepoint_t charToOutput = 0x0;

    // '\' is Assumed as there is only one caller
    // Read 'u' characters
    if (currentLocation >= endOfSource || ReadFirst(currentLocation, endOfSource) != 'u')
    {
        return false;
    }

    bool expectCurly = false;

    if (currentLocation < endOfSource && PeekFirst(currentLocation, endOfSource) == '{' && es6UnicodeMode)
    {
        expectCurly = true;
        // Move past the character
        ReadFirst(currentLocation, endOfSource);
    }

    uint i = 0;
    OLECHAR ch = 0;
    int hexValue = 0;
    uint maxHexDigits = (expectCurly ? MAXUINT32 : 4u);

    for(; i < maxHexDigits && currentLocation < endOfSource; i++)
    {
        if (!Js::NumberUtilities::FHexDigit(ch = ReadFirst(currentLocation, endOfSource), &hexValue))
        {
            break;
        }

        charToOutput = charToOutput * 0x10 + hexValue;

        if (charToOutput > 0x10FFFF)
        {
            return false;
        }
    }

    //At least 4 characters have to be read
    if (i == 0 || (i != 4 && !expectCurly))
    {
        return false;
    }

    Assert(expectCurly ? es6UnicodeMode : true);

    if (expectCurly && ch != '}')
    {
        return false;
    }

    *outChar = charToOutput;
    startingLocation = currentLocation;
    return true;
}

template <typename EncodingPolicy>
template <bool bScan>
bool Scanner<EncodingPolicy>::TryReadCodePointRest(codepoint_t lower, EncodedCharPtr& startingLocation, EncodedCharPtr endOfSource, codepoint_t *outChar, bool *outContainsMultiUnitChar)

{
    Assert(outChar != nullptr);
    Assert(outContainsMultiUnitChar != nullptr);
    Assert(es6UnicodeMode);
    Assert(Js::NumberUtilities::IsSurrogateLowerPart(lower));

    EncodedCharPtr currentLocation = startingLocation;
    *outChar = lower;

    if (currentLocation < endOfSource)
    {
        size_t restorePoint = m_cMultiUnits;
        codepoint_t upper = ReadFull<bScan>(currentLocation, endOfSource);

        if (Js::NumberUtilities::IsSurrogateUpperPart(upper))
        {
            *outChar = Js::NumberUtilities::SurrogatePairAsCodePoint(lower, upper);

            if (IsMultiUnitChar(static_cast<OLECHAR>(upper)))
            {
                *outContainsMultiUnitChar = true;
            }

            startingLocation = currentLocation;
        }
        else
        {
            RestoreMultiUnits(restorePoint);
        }
    }

    return true;
}

template <typename EncodingPolicy>
template <bool bScan>
__inline bool Scanner<EncodingPolicy>::TryReadCodePoint(EncodedCharPtr &startingLocation, EncodedCharPtr endOfSource, codepoint_t *outChar, bool *hasEscape, bool *outContainsMultiUnitChar)
{
    Assert(outChar != nullptr);
    Assert(outContainsMultiUnitChar != nullptr);

    if (startingLocation >= endOfSource)
    {
        return false;
    }

    codepoint_t ch = ReadFull<bScan>(startingLocation, endOfSource);
    if (FBigChar(ch))
    {
        if (IsMultiUnitChar(static_cast<OLECHAR>(ch)))
        {
            *outContainsMultiUnitChar = true;
        }

        if (es6UnicodeMode && Js::NumberUtilities::IsSurrogateLowerPart(ch))
        {
            return TryReadCodePointRest<bScan>(ch, startingLocation, endOfSource, outChar, outContainsMultiUnitChar);
        }
    }
    else if (ch == '\\' && TryReadEscape(startingLocation, endOfSource, &ch))
    {
        *hasEscape = true;
    }

    *outChar = ch;
    return true;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanIdentifier(bool identifyKwds, EncodedCharPtr *pp)
{
    EncodedCharPtr p = *pp;
    EncodedCharPtr pchMin = p;

    // JS6 allows unicode characters in the form of \uxxxx escape sequences
    // to be part of the identifier.
    bool fHasEscape = false;
    bool fHasMultiChar = false;

    codepoint_t codePoint = INVALID_CODEPOINT;
    size_t multiUnitsBeforeLast = m_cMultiUnits;

    // Check if we started the id
    if (!TryReadCodePoint<true>(p, m_pchLast, &codePoint, &fHasEscape, &fHasMultiChar))
    {
        // If no chars. could be scanned as part of the identifier, return error.
        return tkScanError;
    }

    Assert(codePoint < 0x110000u);
    if (!charClassifier->IsIdStart(codePoint))
    {
        // Put back the last character
        RestoreMultiUnits(multiUnitsBeforeLast);

        // If no chars. could be scanned as part of the identifier, return error.
        return tkScanError;
    }

    return ScanIdentifierContinue(identifyKwds, fHasEscape, fHasMultiChar, pchMin, p, pp);
}

template <typename EncodingPolicy>
BOOL Scanner<EncodingPolicy>::FastIdentifierContinue(EncodedCharPtr&p, EncodedCharPtr last)
{
    if (MultiUnitEncoding)
    {
        while (p < last)
        {
            EncodedChar currentChar = *p;
            if (IsMultiUnitChar(currentChar))
            {
                // multi unit character, we may not have reach the end yet
                return FALSE;
            }
            Assert(currentChar != '\\' || !charClassifier->IsIdContinueFast<false>(currentChar));
            if (!charClassifier->IsIdContinueFast<false>(currentChar))
            {
                // only reach the end of the identifier if it is not the start of an escape sequence
                return currentChar != '\\';
            }
            p++;
        }
        // We have reach the end of the identifier.
        return TRUE;
    }

    // Not fast path for non multi unit encoding
    return false;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanIdentifierContinue(bool identifyKwds, bool fHasEscape, bool fHasMultiChar,
    EncodedCharPtr pchMin, EncodedCharPtr p, EncodedCharPtr *pp)
{
    EncodedCharPtr last = m_pchLast;

    while (true)
    {
        // Fast path for utf8, non-multi unit char and not escape
        if (FastIdentifierContinue(p, last))
        {
            break;
        }

        // Slow path that has to deal with multi unit encoding
        codepoint_t codePoint = INVALID_CODEPOINT;
        EncodedCharPtr pchBeforeLast = p;
        size_t multiUnitsBeforeLast = m_cMultiUnits;
        if (TryReadCodePoint<true>(p, last, &codePoint, &fHasEscape, &fHasMultiChar))
        {
            Assert(codePoint < 0x110000u);
            if (charClassifier->IsIdContinue(codePoint))
            {
                continue;
            }
        }

        // Put back the last character
        p = pchBeforeLast;
        RestoreMultiUnits(multiUnitsBeforeLast);
        break;
    }

    Assert(p - pchMin > 0 && p - pchMin <= LONG_MAX);

    *pp = p;

    if (!identifyKwds)
    {
        return tkID;
    }

    // During syntax coloring, scanner doesn't need to convert the escape sequence to get actual characters, it just needs the classification information
    // So call up hashtables custom method to check if the string scanned is identifier or keyword.
    // Do the same for deferred parsing, but use a custom method that only tokenizes JS keywords.
    if ((m_DeferredParseFlags & ScanFlagSuppressIdPid) != 0)
    {
        m_ptoken->SetIdentifier(NULL);
        if (!fHasEscape)
        {
            // If there are no escape, that the main scan loop would have found the keyword already
            // So we can just assume it is an ID
            DebugOnly(long cch = UnescapeToTempBuf(pchMin, p));
            DebugOnly(tokens tk = m_phtbl->TkFromNameLen(m_tempChBuf.m_prgch, cch, IsStrictMode()));
            Assert(tk == tkID || (tk == tkYIELD && !m_fYieldIsKeyword) || (tk == tkAWAIT && !m_fAwaitIsKeyword));
            return tkID;
        }
        long cch = UnescapeToTempBuf(pchMin, p);
        tokens tk = m_phtbl->TkFromNameLen(m_tempChBuf.m_prgch, cch, IsStrictMode());
        return (!m_fYieldIsKeyword && tk == tkYIELD) || (!m_fAwaitIsKeyword && tk == tkAWAIT) ? tkID : tk;
    }
    else if (m_fSyntaxColor)
    {
        m_ptoken->SetIdentifier(NULL);
        // We always need to check TkFromNameLenColor because
        // the main Scan switch doesn't detect all non-keyword that needs coloring
        // (e.g. int)
        long cch = UnescapeToTempBuf(pchMin, p);
        return m_phtbl->TkFromNameLenColor(m_tempChBuf.m_prgch, cch);
    }

    // UTF16 Scanner are only for syntax coloring, so it shouldn't come here.
    if (MultiUnitEncoding && !fHasMultiChar && !fHasEscape)
    {
        Assert(sizeof(EncodedChar) == 1);

        // If there are no escape, that the main scan loop would have found the keyword already
        // So we can just assume it is an ID
        DebugOnly(long cch = UnescapeToTempBuf(pchMin, p));
        DebugOnly(tokens tk = m_phtbl->TkFromNameLen(m_tempChBuf.m_prgch, cch, IsStrictMode()));
        Assert(tk == tkID || (tk == tkYIELD && !m_fYieldIsKeyword) || (tk == tkAWAIT && !m_fAwaitIsKeyword));

        m_ptoken->SetIdentifier(reinterpret_cast<const char *>(pchMin), (long)(p - pchMin));
        return tkID;
    }

    IdentPtr pid = PidOfIdentiferAt(pchMin, p, fHasEscape, fHasMultiChar);
    m_ptoken->SetIdentifier(pid);

    if (!fHasEscape)
    {
        // If it doesn't have escape, then Scan() should have taken care of keywords (except
        // yield if m_fYieldIsKeyword is false, in which case yield is treated as an identifier, and except
        // await if m_fAwaitIsKeyword is false, in which case await is treated as an identifier).
        // We don't have to check if the name is reserved word and return it as an Identifier
        Assert(pid->Tk(IsStrictMode()) == tkID
            || (pid->Tk(IsStrictMode()) == tkYIELD && !m_fYieldIsKeyword)
            || (pid->Tk(IsStrictMode()) == tkAWAIT && !m_fAwaitIsKeyword));
        return tkID;
    }
    tokens tk = pid->Tk(IsStrictMode());
    return tk == tkID || (tk == tkYIELD && !m_fYieldIsKeyword) || (tk == tkAWAIT && !m_fAwaitIsKeyword) ? tkID : tkNone;
}

template <typename EncodingPolicy>
IdentPtr Scanner<EncodingPolicy>::PidAt(size_t iecpMin, size_t iecpLim)
{
    Assert(iecpMin < AdjustedLength() && iecpLim <= AdjustedLength() && iecpLim > iecpMin);
    return PidOfIdentiferAt(m_pchBase + iecpMin, m_pchBase + iecpLim);
}

template <typename EncodingPolicy>
ulong Scanner<EncodingPolicy>::UnescapeToTempBuf(EncodedCharPtr p, EncodedCharPtr last)
{
    m_tempChBuf.Init();
    while( p < last )
    {
        codepoint_t codePoint;
        bool hasEscape, isMultiChar;
        bool gotCodePoint = TryReadCodePoint<false>(p, last, &codePoint, &hasEscape, &isMultiChar);
        Assert(gotCodePoint);
        Assert(codePoint < 0x110000);
        if (codePoint < 0x10000)
        {
            m_tempChBuf.AppendCh((OLECHAR)codePoint);
        }
        else
        {
            wchar_t lower, upper;
            Js::NumberUtilities::CodePointAsSurrogatePair(codePoint, &lower, &upper);
            m_tempChBuf.AppendCh(lower);
            m_tempChBuf.AppendCh(upper);
        }
    }
    return m_tempChBuf.m_ichCur;
}

template <typename EncodingPolicy>
IdentPtr Scanner<EncodingPolicy>::PidOfIdentiferAt(EncodedCharPtr p, EncodedCharPtr last)
{
    long cch = UnescapeToTempBuf(p, last);
    return m_phtbl->PidHashNameLen(m_tempChBuf.m_prgch, cch);
}

template <typename EncodingPolicy>
IdentPtr Scanner<EncodingPolicy>::PidOfIdentiferAt(EncodedCharPtr p, EncodedCharPtr last, bool fHadEscape, bool fHasMultiChar)
{
    // If there is an escape sequence in the JS6 identifier or it is a UTF8
    // source then we have to convert it to the equivalent char so we use a
    // buffer for translation.
    if ((MultiUnitEncoding && fHasMultiChar) || fHadEscape)
    {
        return PidOfIdentiferAt(p, last);
    }
    else if (MultiUnitEncoding)
    {
        Assert(sizeof(EncodedChar) == 1);
        return m_phtbl->PidHashNameLen(reinterpret_cast<const char *>(p), (long)(last - p));
    }
    else
    {
        Assert(sizeof(EncodedChar) == 2);
        return m_phtbl->PidHashNameLen(reinterpret_cast< const wchar_t * >(p), (long)(last - p));
    }
}

template <typename EncodingPolicy>
typename Scanner<EncodingPolicy>::EncodedCharPtr Scanner<EncodingPolicy>::FScanNumber(EncodedCharPtr p, double *pdbl, bool& likelyInt)
{
    EncodedCharPtr last = m_pchLast;
    EncodedCharPtr pchT;
    likelyInt = true;
    // Reset
    m_OctOrLeadingZeroOnLastTKNumber = false;
    if ('0' == PeekFirst(p, last))
    {
        switch(PeekFirst(p + 1, last))
        {
        case '.':
        case 'e':
        case 'E':
            likelyInt = false;
            // Floating point
            goto LFloat;

        case 'x':
        case 'X':
            // Hex
            *pdbl = Js::NumberUtilities::DblFromHex(p + 2, &pchT);
            if (pchT == p + 2)
            {
                // "Octal zero token "0" followed by an identifier token beginning with character 'x'/'X'
                *pdbl = 0;
                return p + 1;
            }
            else
                return pchT;
        case 'o':
        case 'O':
            if (!this->m_scriptContext->GetConfig()->IsES6NumericLiteralEnabled())
            {
                goto LDefaultFScanNumber;
            }
            // Octal
            *pdbl = Js::NumberUtilities::DblFromOctal(p + 2, &pchT);
            if (pchT == p + 2)
            {
                // "Octal zero token "0" followed by an identifier token beginning with character 'o'/'O'
                *pdbl = 0;
                return p + 1;
            }
            return pchT;

        case 'b':
        case 'B':
            if (!this->m_scriptContext->GetConfig()->IsES6NumericLiteralEnabled())
            {
                goto LDefaultFScanNumber;
            }
            // Binary
            *pdbl = Js::NumberUtilities::DblFromBinary(p + 2, &pchT);
            if (pchT == p + 2)
            {
                // "Octal zero token "0" followed by an identifier token beginning with character 'b'/'B'
                *pdbl = 0;
                return p + 1;
            }
            return  pchT;

        default:
LDefaultFScanNumber :
            // Octal
            *pdbl = Js::NumberUtilities::DblFromOctal(p, &pchT);
            Assert(pchT > p);

#if !SOURCERELEASE
            // If an octal literal is malformed then it is in fact a decimal literal.
#endif // !SOURCERELEASE
            if(*pdbl != 0 || pchT > p + 1)
                m_OctOrLeadingZeroOnLastTKNumber = true; //report as an octal or hex for JSON when leading 0. Just '0' is ok

            switch (*pchT)
            {
            case '8':
            case '9':
                //            case 'e':
                //            case 'E':
                //            case '.':
                m_OctOrLeadingZeroOnLastTKNumber = false;  //08...  or 09....
                goto LFloat;
            }
            return pchT;
        }
    }
    else
    {
LFloat:
        *pdbl = Js::NumberUtilities::StrToDbl(p, &pchT, likelyInt);
        Assert(pchT == p || !Js::NumberUtilities::IsNan(*pdbl));
        return pchT;
    }
}

template <typename EncodingPolicy>
BOOL Scanner<EncodingPolicy>::oFScanNumber(double *pdbl, bool& likelyInt)
{
    EncodedCharPtr pchT;
    m_OctOrLeadingZeroOnLastTKNumber = false;
    likelyInt = true;
    if  ('0' == *m_currentCharacter)
    {
        switch (m_currentCharacter[1])
        {
        case '.':
        case 'e':
        case 'E':
            likelyInt = false;
            // Floating point.
            goto LFloat;

        case 'x':
        case 'X':
            // Hex.
            *pdbl = Js::NumberUtilities::DblFromHex<EncodedChar>(m_currentCharacter + 2, &pchT);
            if (pchT == m_currentCharacter + 2)
            {
                // "Octal zero token "0" followed by an identifier token beginning with character 'x'/'X'
                *pdbl = 0;
                m_currentCharacter++;
            }
            else
                m_currentCharacter = pchT;
            break;
        case 'o':
        case 'O':
            if (!this->m_scriptContext->GetConfig()->IsES6NumericLiteralEnabled())
            {
                goto LDefaultoFScanNumber;
            }
            *pdbl = Js::NumberUtilities::DblFromOctal(m_currentCharacter + 2, &pchT);
            if (pchT == m_currentCharacter + 2)
            {
                // "Octal zero token "0" followed by an identifier token beginning with character 'o'/'O'
                *pdbl = 0;
                m_currentCharacter++;
            }
            else
                m_currentCharacter = pchT;
            break;

        case 'b':
        case 'B':
            if (!this->m_scriptContext->GetConfig()->IsES6NumericLiteralEnabled())
            {
                goto LDefaultoFScanNumber;
            }

            *pdbl = Js::NumberUtilities::DblFromBinary(m_currentCharacter + 2, &pchT);
            if (pchT == m_currentCharacter + 2)
            {
                // "Octal zero token "0" followed by an identifier token beginning with character 'b'/'B'
                *pdbl = 0;
                m_currentCharacter++;
            }
            else
                m_currentCharacter = pchT;
            break;

        default:
LDefaultoFScanNumber :
            // Octal.
            *pdbl = Js::NumberUtilities::DblFromOctal(m_currentCharacter, &pchT);
            Assert(pchT > m_currentCharacter);


#if !SOURCERELEASE
            // If an octal literal is malformed then it is in fact a decimal literal.
#endif // !SOURCERELEASE
            if(*pdbl != 0 || pchT > m_currentCharacter + 1)
                m_OctOrLeadingZeroOnLastTKNumber = true; //report as an octal or hex for JSON when leading 0. Just '0' is ok
            switch (*pchT)
            {
            case '8':
            case '9':
                //            case 'e':
                //            case 'E':
                //            case '.':
                m_OctOrLeadingZeroOnLastTKNumber = false;  //08...  or 09....
                goto LFloat;
            }

            m_currentCharacter = pchT;
            break;
        }
    }
    else
    {
LFloat:
        // Let StrToDbl do all the work.

        *pdbl = Js::NumberUtilities::StrToDbl(m_currentCharacter, &pchT, likelyInt);
        if (pchT == m_currentCharacter)
            return FALSE;
        m_currentCharacter = pchT;
        Assert(!Js::NumberUtilities::IsNan(*pdbl));
    }

    return TRUE;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::TryRescanRegExp()
{
    EncodedCharPtr current = m_currentCharacter;
    tokens result = RescanRegExp();
    if (result == tkScanError)
        m_currentCharacter = current;
    return result;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::RescanRegExp()
{
#if DEBUG
    switch (m_ptoken->tk)
    {
    case tkDiv:
        Assert(m_currentCharacter == m_pchMinTok + 1);
        break;
    case tkAsgDiv:
        Assert(m_currentCharacter == m_pchMinTok + 2);
        break;
    default:
        AssertMsg(FALSE, "Who is calling RescanRegExp?");
        break;
    }
#endif //DEBUG

    m_currentCharacter = m_pchMinTok;
    if (*m_currentCharacter != '/')
        Error(ERRnoSlash);
    m_currentCharacter++;

    tokens tk = tkNone;

    {
        ArenaAllocator alloc(L"RescanRegExp", m_parser->GetAllocator()->GetPageAllocator(), m_parser->GetAllocator()->outOfMemoryFunc);
        tk = ScanRegExpConstant(&alloc);
    }

    return tk;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::RescanRegExpNoAST()
{
#if DEBUG
    switch (m_ptoken->tk)
    {
    case tkDiv:
        Assert(m_currentCharacter == m_pchMinTok + 1);
        break;
    case tkAsgDiv:
        Assert(m_currentCharacter == m_pchMinTok + 2);
        break;
    default:
        AssertMsg(FALSE, "Who is calling RescanRegExpNoParseTree?");
        break;
    }
#endif //DEBUG

    m_currentCharacter = m_pchMinTok;
    if (*m_currentCharacter != '/')
        Error(ERRnoSlash);
    m_currentCharacter++;

    tokens tk = tkNone;

    {
        ArenaAllocator alloc(L"RescanRegExp", m_parser->GetAllocator()->GetPageAllocator(), m_parser->GetAllocator()->outOfMemoryFunc);
        {
            tk = ScanRegExpConstantNoAST(&alloc);
        }
    }

    return tk;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::RescanRegExpTokenizer()
{
#if DEBUG
    switch (m_ptoken->tk)
    {
    case tkDiv:
        Assert(m_currentCharacter == m_pchMinTok + 1);
        break;
    case tkAsgDiv:
        Assert(m_currentCharacter == m_pchMinTok + 2);
        break;
    default:
        AssertMsg(FALSE, "Who is calling RescanRegExpNoParseTree?");
        break;
    }
#endif //DEBUG

    m_currentCharacter = m_pchMinTok;
    if (*m_currentCharacter != '/')
        Error(ERRnoSlash);
    m_currentCharacter++;

    tokens tk = tkNone;

    ThreadContext *threadContext = ThreadContext::GetContextForCurrentThread();
    threadContext->EnsureRecycler();
    Js::TempArenaAllocatorObject *alloc = threadContext->GetTemporaryAllocator(L"RescanRegExp");
    __try
    {
        tk = ScanRegExpConstantNoAST(alloc->GetAllocator());
    }
    __finally
    {
        threadContext->ReleaseTemporaryAllocator(alloc);
    }

    return tk;
}

template <typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanRegExpConstant(ArenaAllocator* alloc)
{
    if (m_parser && m_parser->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackRegex);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackRegex);
    }

    // SEE ALSO: RegexHelper::PrimCompileDynamic()

#ifdef PROFILE_EXEC
    m_scriptContext->ProfileBegin(Js::RegexCompilePhase);
#endif
    ArenaAllocator* ctAllocator = alloc;
    UnifiedRegex::StandardChars<EncodedChar>* standardEncodedChars = m_scriptContext->GetThreadContext()->GetStandardChars((EncodedChar*)0);
    UnifiedRegex::StandardChars<wchar_t>* standardChars = m_scriptContext->GetThreadContext()->GetStandardChars((wchar_t*)0);
#if ENABLE_REGEX_CONFIG_OPTIONS
    UnifiedRegex::DebugWriter *w = 0;
    if (REGEX_CONFIG_FLAG(RegexDebug))
        w = m_scriptContext->GetRegexDebugWriter();
    if (REGEX_CONFIG_FLAG(RegexProfile))
        m_scriptContext->GetRegexStatsDatabase()->BeginProfile();
#endif
    UnifiedRegex::Node* root = 0;
    charcount_t totalLen = 0, bodyChars = 0, totalChars = 0, bodyLen = 0;
    UnifiedRegex::RegexFlags flags = UnifiedRegex::NoRegexFlags;
    UnifiedRegex::Parser<EncodingPolicy, true> parser
            ( m_scriptContext
            , ctAllocator
            , standardEncodedChars
            , standardChars
            , IsFromExternalSource()
#if ENABLE_REGEX_CONFIG_OPTIONS
            , w
#endif
            );
    try
    {
        root = parser.ParseLiteral(m_currentCharacter, m_pchLast, bodyLen, totalLen, bodyChars, totalChars, flags);
    }
    catch (UnifiedRegex::ParseError e)
    {
#ifdef PROFILE_EXEC
        m_scriptContext->ProfileEnd(Js::RegexCompilePhase);
#endif
        if (m_fSyntaxColor)
            return ScanError(m_currentCharacter + e.encodedPos, tkRegExp);

        m_currentCharacter += e.encodedPos;
        Error(e.error);
    }

    UnifiedRegex::RegexPattern* pattern;
    if (m_parser->IsBackgroundParser())
    {
        // Avoid allocating pattern from recycler on background thread. The main thread will create the pattern
        // and hook it to this parse node.
        pattern = parser.CompileProgram<false>(root, m_currentCharacter, totalLen, bodyChars, totalChars, flags);
    }
    else
    {
        pattern = parser.CompileProgram<true>(root, m_currentCharacter, totalLen, bodyChars, totalChars, flags);
    }
    RestoreMultiUnits(m_cMultiUnits + parser.GetMultiUnits()); // m_currentCharacter changed, sync MultiUnits

    return m_ptoken->SetRegex(pattern, m_parser);
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanRegExpConstantNoAST(ArenaAllocator* alloc)
{
    if (m_parser && m_parser->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackRegex);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackRegex);
    }

    ThreadContext *threadContext = m_fSyntaxColor ? ThreadContext::GetContextForCurrentThread() : m_scriptContext->GetThreadContext();
    UnifiedRegex::StandardChars<EncodedChar>* standardEncodedChars = threadContext->GetStandardChars((EncodedChar*)0);
    UnifiedRegex::StandardChars<wchar_t>* standardChars = threadContext->GetStandardChars((wchar_t*)0);
    charcount_t totalLen = 0, bodyChars = 0, totalChars = 0, bodyLen = 0;
    UnifiedRegex::Parser<EncodingPolicy, true> parser
            ( m_scriptContext
            , alloc
            , standardEncodedChars
            , standardChars
            , IsFromExternalSource()
#if ENABLE_REGEX_CONFIG_OPTIONS
            , 0
#endif
            );
    try
    {
        parser.ParseLiteralNoAST(m_currentCharacter, m_pchLast, bodyLen, totalLen, bodyChars, totalChars);
    }
    catch (UnifiedRegex::ParseError e)
    {
        if (m_fSyntaxColor)
            return ScanError(m_currentCharacter + e.encodedPos, tkRegExp);

        m_currentCharacter += e.encodedPos;
        Error(e.error);
        // never reached
    }

    UnifiedRegex::RegexPattern* pattern = parser.CompileProgram<false>(nullptr, m_currentCharacter, totalLen, bodyChars, totalChars, UnifiedRegex::NoRegexFlags);
    Assert(pattern == nullptr);  // BuildAST == false, CompileProgram should return nullptr
    RestoreMultiUnits(m_cMultiUnits + parser.GetMultiUnits()); // m_currentCharacter changed, sync MultiUnits

    return (m_ptoken->tk = tkRegExp);

}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanStringTemplateBegin(EncodedCharPtr *pp)
{
    // String template must begin with a string constant followed by '`' or '${'
    ScanStringConstant<true, true>('`', pp);

    OLECHAR ch;
    EncodedCharPtr last = m_pchLast;

    ch = ReadFirst(*pp, last);

    if (ch == '`')
    {
        // Simple string template - no substitutions
        return tkStrTmplBasic;
    }
    else if (ch == '$')
    {
        ch = ReadFirst(*pp, last);

        if (ch == '{')
        {
            // Next token after expr should be tkStrTmplMid or tkStrTmplEnd.
            // In string template scanning mode, we expect the next char to be '}'
            // and will treat it as the beginning of tkStrTmplEnd or tkStrTmplMid
            m_fStringTemplateDepth++;

            // Regular string template begin - next is first substitution
            return tkStrTmplBegin;
        }
    }

    // Error - make sure pointer stays at the last character of the error token instead of after it in the error case
    (*pp)--;
    return ScanError(m_currentCharacter, tkStrTmplBegin);
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanStringTemplateMiddleOrEnd(EncodedCharPtr *pp)
{
    // String template middle and end tokens must begin with a string constant
    ScanStringConstant<true, true>('`', pp);

    OLECHAR ch;
    EncodedCharPtr last = m_pchLast;

    ch = ReadFirst(*pp, last);

    if (ch == '`')
    {
        // No longer in string template scanning mode
        m_fStringTemplateDepth--;

        // This is the last part of the template ...`
        return tkStrTmplEnd;
    }
    else if (ch == '$')
    {
        ch = ReadFirst(*pp, last);

        if (ch == '{')
        {
            // This is just another middle part of the template }...${
            return tkStrTmplMid;
        }
    }

    // Error - make sure pointer stays at the last character of the error token instead of after it in the error case
    (*pp)--;
    return ScanError(m_currentCharacter, tkStrTmplEnd);
}

/*****************************************************************************
*
*  Parses a string constant. Note that the string value is stored in
*  a volatile buffer (or allocated on the heap if too long), and thus
*  the string should be saved off before the next token is scanned.
*/

template<typename EncodingPolicy>
template<bool stringTemplateMode, bool createRawString>
tokens Scanner<EncodingPolicy>::ScanStringConstant(OLECHAR delim, EncodedCharPtr *pp)
{
    static_assert((stringTemplateMode && createRawString) || (!stringTemplateMode && !createRawString), "stringTemplateMode and createRawString must have the same value");

    OLECHAR ch, c, rawch;
    int wT;
    EncodedCharPtr p = *pp;
    EncodedCharPtr last = m_pchLast;

    if (stringTemplateMode)
    {
        Assert(m_scriptContext->GetConfig()->IsES6StringTemplateEnabled());
    }

    // Reset
    m_OctOrLeadingZeroOnLastTKNumber = false;
    m_EscapeOnLastTkStrCon = FALSE;

    m_tempChBuf.Init();

    // Use template parameter to gate raw string creation.
    // If createRawString is false, all these operations should be no-ops
    if (createRawString)
    {
        m_tempChBufSecondary.Init();
    }

    for (;;)
    {
        switch ((rawch = ch = ReadFirst(p, last)))
        {
        case kchRET:
            if (stringTemplateMode)
            {
                if (PeekFirst(p, last) == kchNWL)
                {
                    // Eat the <LF> char, ignore return
                    ReadFirst(p, last);
                }

                // Both <CR> and <CR><LF> are normalized to <LF> in template cooked and raw values
                ch = rawch = kchNWL;
            }

LEcmaLineBreak:
            // Fall through
        case kchNWL:
            if (stringTemplateMode)
            {
                // Notify the scanner to update current line, number of lines etc
                NotifyScannedNewLine();
                break;
            }

            m_currentCharacter = p - 1;
            if (m_fSyntaxColor)
            {
                *pp = p - 1;
                return ScanError(p - 1, tkStrCon);
            }
            Error(ERRnoStrEnd);

        case '"':
        case '\'':
            if (ch == delim)
                goto LBreak;
            break;

        case '`':
            // In string template scan mode, don't consume the '`' - we need to differentiate
            // between a closed string template and the expression open sequence - ${
            if (stringTemplateMode)
            {
                p--;
                goto LBreak;
            }

            // If we aren't scanning for a string template, do the default thing
            goto LMainDefault;

        case '$':
            // If we are parsing a string literal part of a string template, ${ indicates we need to switch
            // to parsing an expression.
            if (stringTemplateMode && PeekFirst(p, last) == '{')
            {
                // Rewind to the $ and return
                p--;
                goto LBreak;
            }

            // If we aren't scanning for a string template, do the default thing
            goto LMainDefault;

        case kchNUL:
            if (p >= last)
            {
                m_currentCharacter = p - 1;
                if (m_fSyntaxColor)
                {
                    *pp = p - 1;
                    return ScanError(p - 1, tkStrCon);
                }
                Error(ERRnoStrEnd);
            }
            break;

        default:
LMainDefault:
            if (IsMultiUnitChar(ch))
            {
                if ((ch == kchLS || ch == kchPS))
                {
                    goto LEcmaLineBreak;
                }

                rawch = ch = ReadRest<true>(ch, p, last);
                switch (ch)
                {
                case kchLS: // 0x2028, classifies as new line
                case kchPS: // 0x2029, classifies as new line
                    goto LEcmaLineBreak;
                }
            }
            break;

        case kchBSL:
            // In raw mode '\\' is not an escape character, just add the char into the raw buffer.
            m_tempChBufSecondary.AppendCh<createRawString>(ch);

            m_EscapeOnLastTkStrCon=TRUE;

            // In raw mode, we append the raw char itself and not the escaped value so save the char.
            rawch = ch = ReadFirst(p, last);
            codepoint_t codePoint = 0;
            uint errorType = (uint)ERRbadHexDigit;
            switch (ch)
            {
            case 'b':
                ch = 0x08;
                break;
            case 't':
                ch = 0x09;
                break;
            case 'v':
                ch = 0x0B; //Only in ES5 mode
                break; //same as default
            case 'n':
                ch = 0x0A;
                break;
            case 'f':
                ch = 0x0C;
                break;
            case 'r':
                ch = 0x0D;
                break;
            case 'x':
                // Insert the 'x' here before jumping to parse the hex digits.
                m_tempChBufSecondary.AppendCh<createRawString>(ch);

                // 2 hex digits
                ch = 0;
                goto LTwoHex;
            case 'u':
                // Raw string just inserts a 'u' here.
                m_tempChBufSecondary.AppendCh<createRawString>(ch);

                ch = 0;
                if (Js::NumberUtilities::FHexDigit(c = ReadFirst(p, last), &wT))
                    goto LFourHex;
                else if (c != '{' || !this->es6UnicodeMode)
                    goto ReturnScanError;

                Assert(c == '{');
                // c should definitely be a '{' which should be appended to the raw string.
                m_tempChBufSecondary.AppendCh<createRawString>(c);

                //At least one digit is expected
                if (!Js::NumberUtilities::FHexDigit(c = ReadFirst(p, last), &wT))
                {
                    goto ReturnScanError;
                }

                m_tempChBufSecondary.AppendCh<createRawString>(c);

                codePoint = static_cast<codepoint_t>(wT);

                while(Js::NumberUtilities::FHexDigit(c = ReadFirst(p, last), &wT))
                {
                    m_tempChBufSecondary.AppendCh<createRawString>(c);
                    codePoint <<= 4;
                    codePoint += static_cast<codepoint_t>(wT);

                    if (codePoint > 0x10FFFF)
                    {
                        errorType = (uint)ERRInvalidCodePoint;
                        goto ReturnScanError;
                    }
                }

                if (c != '}')
                {
                    errorType = (uint)ERRMissingCurlyBrace;
                    goto ReturnScanError;
                }

                Assert(codePoint <= 0x10FFFF);

                if (codePoint >= 0x10000)
                {
                    OLECHAR lower = 0;
                    Js::NumberUtilities::CodePointAsSurrogatePair(codePoint, &lower, &ch);
                    m_tempChBuf.AppendCh(lower);
                }
                else
                {
                    ch = (wchar_t)codePoint;
                }

                // In raw mode we want the last hex character or the closing curly. c should hold one or the other.
                if (createRawString)
                    rawch = c;

                break;
LFourHex:
                codePoint = 0x0;
                // Append first hex digit character to the raw string.
                m_tempChBufSecondary.AppendCh<createRawString>(c);

                codePoint += static_cast<codepoint_t>(wT * 0x1000);
                if (!Js::NumberUtilities::FHexDigit(c = ReadFirst(p, last), &wT))
                    goto ReturnScanError;

                // Append fourth (or second) hex digit character to the raw string.
                m_tempChBufSecondary.AppendCh<createRawString>(c);

                codePoint += static_cast<codepoint_t>(wT * 0x0100);

LTwoHex:
                // This code path doesn't expect curly.
                if (!Js::NumberUtilities::FHexDigit(c = ReadFirst(p, last), &wT))
                    goto ReturnScanError;

                // Append first hex digit character to the raw string.
                m_tempChBufSecondary.AppendCh<createRawString>(c);

                codePoint += static_cast<codepoint_t>(wT * 0x0010);

                if (!Js::NumberUtilities::FHexDigit(c = ReadFirst(p, last), &wT))
                    goto ReturnScanError;

                codePoint += static_cast<codepoint_t>(wT);

                // In raw mode we want the last hex character or the closing curly. c should hold one or the other.
                if (createRawString)
                    rawch = c;

                if (codePoint < 0x10000)
                {
                    ch = static_cast<OLECHAR>(codePoint);
                }
                else
                {
                    goto ReturnScanError;
                }
                break;
            case '0':
            case '1':
            case '2':
            case '3':
                // 1 to 3 octal digits

                ch -= '0';

                // Octal escape sequences are not allowed inside string template literals
                if (stringTemplateMode)
                {
                    c = PeekFirst(p, last);
                    if (ch != 0 || (c >= '0' && c <= '7'))
                    {
                        errorType = (uint)ERRES5NoOctal;
                        goto ReturnScanError;
                    }
                    break;
                }

                wT = (c = ReadFirst(p, last)) - '0';
                if ((wchar_t)wT > 7)
                {
                    if (ch != 0 || ((wchar_t)wT <= 9))
                    {
                        m_OctOrLeadingZeroOnLastTKNumber = true;
                    }
                    p--;
                    break;
                }

                m_OctOrLeadingZeroOnLastTKNumber = true;
                ch = static_cast< OLECHAR >(ch * 8 + wT);
                goto LOneOctal;
            case '4':
            case '5':
            case '6':
            case '7':
                // 1 to 2 octal digits

                // Octal escape sequences are not allowed inside string template literals
                if (stringTemplateMode)
                {
                    errorType = (uint)ERRES5NoOctal;
                    goto ReturnScanError;
                }

                ch -= '0';

                m_OctOrLeadingZeroOnLastTKNumber = true;

LOneOctal:
                wT = (c = ReadFirst(p, last)) - '0';
                if ((wchar_t)wT > 7)
                {
                    p--;
                    break;
                }

                ch = static_cast< OLECHAR >(ch * 8 + wT);
                break;

            case kchRET:        // 0xD
                if (stringTemplateMode)
                {
                    // If this is \<CR><LF> we can eat the <LF> right now
                    if (PeekFirst(p, last) == kchNWL)
                    {
                        // Eat the <LF> char, ignore return
                        ReadFirst(p, last);
                    }

                    // Both \<CR> and \<CR><LF> are normalized to \<LF> in template raw string
                    rawch = kchNWL;
                }
            case kchLS:         // 0x2028, classifies as new line
            case kchPS:         // 0x2029, classifies as new line
            case kchNWL:        // 0xA
LEcmaEscapeLineBreak:
                if (stringTemplateMode)
                {
                    // We're going to ignore the line continuation tokens for the cooked strings, but we need to append the token for raw strings
                    m_tempChBufSecondary.AppendCh<createRawString>(rawch);

                    // Template literal strings ignore all escaped line continuation tokens
                    NotifyScannedNewLine();
                    continue;
                }

                m_currentCharacter = p;
                ScanNewLine(ch);
                p = m_currentCharacter;
                if (m_fSyntaxColor && *p == 0)
                {
                    // Special case for multi-line strings during colorization.
                    m_scanState = delim == '"' ?  ScanStateMultiLineDoubleQuoteString : ScanStateMultiLineSingleQuoteString;
                    *pp = p;
                    return tkStrCon;
                }
                continue;

            case 0:
                if (p >= last)
                {
                    errorType = (uint)ERRnoStrEnd;

ReturnScanError:
                    m_currentCharacter = p - 1;
                    if (m_fSyntaxColor)
                    {
                        *pp = p - 1;
                        return ScanError(p - 1, tkStrCon);
                    }
                    Error(errorType);
                }
                else if (stringTemplateMode)
                {
                    // Escaped null character is translated into 0x0030 for raw template literals
                    rawch = 0x0030;
                }
                break;

            default:
                if (IsMultiUnitChar(ch))
                {
                    rawch = ch = ReadRest<true>(ch, p, last);
                    switch (ch)
                    {
                    case kchLS:
                    case kchPS:
                        goto LEcmaEscapeLineBreak;
                    }
                }
                break;
            }
            break;
        }

        m_tempChBuf.AppendCh(ch);
        m_tempChBufSecondary.AppendCh<createRawString>(rawch);
    }

LBreak:
    bool createPid = true;

    if (m_fSyntaxColor || (m_DeferredParseFlags & ScanFlagSuppressStrPid) != 0)
    {
        createPid = false;

        if ((m_tempChBuf.m_ichCur == 10) && (0 == memcmp(L"use strict", m_tempChBuf.m_prgch, m_tempChBuf.m_ichCur * sizeof(OLECHAR))))
        {
            createPid = true;
        }
    }

    if (createPid)
    {
        m_ptoken->SetIdentifier(m_phtbl->PidHashNameLen(m_tempChBuf.m_prgch, m_tempChBuf.m_ichCur));
    }
    else
    {
        m_ptoken->SetIdentifier(NULL);
    }

    m_scanState = ScanStateNormal;
    m_doubleQuoteOnLastTkStrCon = '"' == delim;
    *pp = p;

    return tkStrCon;
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanStringConstant(OLECHAR delim, EncodedCharPtr *pp)
{
    return ScanStringConstant<false, false>(delim, pp);
}

/*****************************************************************************
*
*  Consume a C-style comment.
*/
template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::SkipComment(EncodedCharPtr *pp, /* out */ bool* containTypeDef)
{
    Assert(containTypeDef != nullptr);
    EncodedCharPtr p = *pp;
    *containTypeDef = false;
    EncodedCharPtr last = m_pchLast;
    OLECHAR ch;

    for (;;)
    {
        switch((ch = ReadFirst(p, last)))
        {
        case '*':
            if (*p == '/')
            {
                *pp = p + 1;
                if (m_fSyntaxColor)
                {
                    m_scanState = ScanStateNormal;
                    return tkComment;
                }
                return tkNone;
            }
            break;
        case kchLS:         // 0x2028, classifies as new line
        case kchPS:         // 0x2029, classifies as new line
LEcmaLineBreak:
            goto LLineBreak;

        case kchRET:
        case kchNWL:
LLineBreak:
            m_fHadEol = TRUE;
            m_currentCharacter = p;
            ScanNewLine(ch);
            p = m_currentCharacter;
            break;

        case kchNUL:
            if (p >= last)
            {
                m_currentCharacter = p - 1;
                *pp = p - 1;
                if (m_fSyntaxColor)
                {
                    m_scanState = ScanStateMultiLineComment;
                    return tkComment;
                }
                Error(ERRnoCmtEnd);
            }
            break;

        default:
            if (IsMultiUnitChar(ch))
            {
                ch = ReadRest<true>(ch, p, last);
                switch (ch)
                {
                case kchLS:
                case kchPS:
                    goto LEcmaLineBreak;
                }
            }
            break;
        }
    }
}

/*****************************************************************************
*
*  We've encountered a newline - update various counters and things.
*/
template<typename EncodingPolicy>
void Scanner<EncodingPolicy>::ScanNewLine(uint ch)
{
    if (ch == '\r' && PeekNextChar() == '\n')
    {
        ReadNextChar();
    }

    NotifyScannedNewLine();
}

/*****************************************************************************
*
*  We've encountered a newline - update various counters and things.
*/
template<typename EncodingPolicy>
void Scanner<EncodingPolicy>::NotifyScannedNewLine()
{
    // update in scanner:  previous line, current line, number of lines.
    m_line++;
    m_pchPrevLine = m_pchMinLine;
    m_pchMinLine = m_currentCharacter;
    m_cMinLineMultiUnits = m_cMultiUnits;
}

/*****************************************************************************
*
*  Delivers a token stream.
*/


template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanForcingPid()
{
    if (m_DeferredParseFlags != ScanFlagNone)
    {
        BYTE deferredParseFlagsSave = m_DeferredParseFlags;
        m_DeferredParseFlags = ScanFlagNone;
        tokens result = tkEOF;
        __try
        {
           result = Scan();
        }
        __finally
        {
            m_DeferredParseFlags = deferredParseFlagsSave;
        }
        return result;
    }
    return Scan();
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::Scan()
{
    return ScanCore(true);
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanNoKeywords()
{
    return ScanCore(false);
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanAhead()
{
    return ScanNoKeywords();
}

template<typename EncodingPolicy>
tokens Scanner<EncodingPolicy>::ScanCore(bool identifyKwds)
{
    codepoint_t ch;
    OLECHAR firstChar;
    OLECHAR secondChar;
    EncodedCharPtr pchT;
    size_t multiUnits = 0;
    EncodedCharPtr p = m_currentCharacter;
    EncodedCharPtr last = m_pchLast;
    // store the last token
    m_tkPrevious = m_ptoken->tk;
    m_iecpLimTokPrevious = IecpLimTok();    // Introduced for use by lambda parsing to find correct span of expression lambdas

    if (p >= last)
    {
        m_pchMinTok = p;
        m_cMinTokMultiUnits = m_cMultiUnits;
        goto LEof;
    }
    tokens token;
    m_fHadEol = FALSE;
    CharTypes chType;
    charcount_t commentStartLine;

    if (m_scanState && *p != 0)
    {
        if (m_fSyntaxColor)
        {
            firstChar = 0;
            secondChar = 0;
            m_pchMinTok = p;
            m_cMinTokMultiUnits = m_cMultiUnits;
            switch (m_scanState)
            {
            case ScanStateMultiLineComment:
                goto LMultiLineComment;
            case ScanStateMultiLineSingleQuoteString:
                ch = '\'';
                m_scanState = ScanStateNormal;
                goto LScanStringConstant;
            case ScanStateMultiLineDoubleQuoteString:
                ch = '"';
                m_scanState = ScanStateNormal;
                goto LScanStringConstant;
            }
        }
        if (m_scanState == ScanStateStringTemplateMiddleOrEnd)
        {
            AssertMsg(m_fStringTemplateDepth > 0,
                "Shouldn't be trying to parse a string template end or middle token if we aren't scanning a string template");
            AssertMsg(m_scriptContext->GetConfig()->IsES6StringTemplateEnabled(),
                "Shouldn't be in string template parse mode if string templates are not enabled.");

            m_scanState = ScanStateNormal;

            pchT = p;
            token = ScanStringTemplateMiddleOrEnd(&pchT);
            p = pchT;

            goto LDone;
        }
    }

    for (;;)
    {
LLoop:
        m_pchMinTok = p;
        m_cMinTokMultiUnits = m_cMultiUnits;
        ch = ReadFirst(p, last);
#if DEBUG
        chType = this->charClassifier->GetCharType((OLECHAR)ch);
#endif
        switch (ch)
        {
        default:
LLoopDefault:
            if (ch == kchLS ||
                ch == kchPS )
            {
                goto LNewLine;
            }
            {
                BOOL isMultiUnit = IsMultiUnitChar((OLECHAR)ch);
                if (isMultiUnit)
                {
                    ch = ReadRest<true>((OLECHAR)ch, p, last);
                }

                if (es6UnicodeMode && Js::NumberUtilities::IsSurrogateLowerPart(ch))
                {
                    codepoint_t upper = PeekFull(p, last);

                    if (Js::NumberUtilities::IsSurrogateUpperPart(upper))
                    {
                        ch = Js::NumberUtilities::SurrogatePairAsCodePoint(ch, upper);
                        ReadFull<true>(p, last);
                    }
                }

                if (this->charClassifier->IsIdStart(ch))
                {
                    // We treat IDContinue as an error.
                    token = ScanIdentifierContinue(identifyKwds, false, !!isMultiUnit, m_pchMinTok, p, &p);
                    break;
                }
            }

            chType = this->charClassifier->GetCharType(ch);
            switch (chType)
            {
            case _C_WSP: continue;
            case _C_NWL: goto LNewLine;
            // All other types (except errors) are handled by the outer switch.
            }
            Assert(chType == _C_LET || chType == _C_ERR || chType == _C_UNK || chType == _C_BKQ || chType == _C_SHP || chType == _C_AT || chType == _C_DIG);
            if (m_fSyntaxColor)
            {
                // No need to decrement the current position pointer as scanner will continue with scan next character onwards
                return ScanError(p, tkID);
            }
            m_currentCharacter = p - 1;
            Error(ERRillegalChar);
            continue;

        case '\0':
            // Put back the null in case we get called again.
            p--;
LEof:
            token = tkEOF;

            if (p + 1 < last)
            {
                if (m_fSyntaxColor)
                {
                    return ScanError(p + 1, tkID);
                }

                // A \0 prior to the end of the text is an invalid character.
                Error(ERRillegalChar);
            }
            break;

        case 0x0009:
        case 0x000B:
        case 0x000C:
        case 0x0020:
            Assert(chType == _C_WSP);
            continue;

        case '.':
            if (!Js::NumberUtilities::IsDigit(*p))
            {
                // Not a double
                if (m_scriptContext->GetConfig()->IsES6SpreadEnabled() && PeekFirst(p, last) == '.' && PeekFirst(p + 1, last) == '.')
                {
                    token = tkEllipsis;
                    p += 2;
                }
                else
                {
                    token = tkDot;
                }
                break;
            }
            // May be a double, fall through
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            {
                double dbl;
                Assert(chType == _C_DIG || chType == _C_DOT);
                p = m_pchMinTok;
                RestoreMultiUnits(m_cMinTokMultiUnits);
                bool likelyInt = true;
                pchT = FScanNumber(p, &dbl, likelyInt);
                if (p == pchT)
                {
                    Assert(PeekFirst(p, last) != '.');
                    if (m_fSyntaxColor)
                    {
                        return ScanError(m_currentCharacter + 1, tkFltCon);
                    }

                    Error(ERRbadNumber);
                }
                Assert(!Js::NumberUtilities::IsNan(dbl));

                p = pchT;

                long value;
                if (likelyInt && Js::NumberUtilities::FDblIsLong(dbl, &value))
                {
                    m_ptoken->SetLong(value);
                    token = tkIntCon;
                }
                else
                {
                    token = tkFltCon;
                    m_ptoken->SetDouble(dbl, likelyInt);
                }

                break;
            }
        case '(': Assert(chType == _C_LPR); token = tkLParen; break;
        case ')': Assert(chType == _C_RPR); token = tkRParen; break;
        case ',': Assert(chType == _C_CMA); token = tkComma; break;
        case ';': Assert(chType == _C_SMC); token = tkSColon; break;
        case '[': Assert(chType == _C_LBR); token = tkLBrack; break;
        case ']': Assert(chType == _C_RBR); token = tkRBrack; break;
        case '~': Assert(chType == _C_TIL); token = tkTilde; break;
        case '?': Assert(chType == _C_QUE); token = tkQMark; break;
        case '{': Assert(chType == _C_LC); token = tkLCurly; break;

        case '\r':
        case '\n':
LNewLine:
            m_currentCharacter = p;
            ScanNewLine(ch);
            p = m_currentCharacter;
            m_fHadEol = TRUE;
            continue;

LReserved:
            {
                // We will derive the PID from the token
                Assert(token < tkID);
                m_ptoken->SetIdentifier(NULL);
                goto LDone;
            }

LEval:
            {
                token = tkID;
                if (!this->m_parser) goto LIdentifier;
                m_ptoken->SetIdentifier(this->m_parser->GetEvalPid());
                goto LDone;
            }

LArguments:
            {
                token = tkID;
                if (!this->m_parser) goto LIdentifier;
                m_ptoken->SetIdentifier(this->m_parser->GetArgumentsPid());
                goto LDone;
            }

LTarget:
            {
                token = tkID;
                if (!this->m_parser) goto LIdentifier;
                m_ptoken->SetIdentifier(this->m_parser->GetTargetPid());
                goto LDone;
            }

#include "kwd-swtch.h"
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z':
        // Lower-case letters handled in kwd-swtch.h above during reserved word recognition.
        case '$': case '_':
LIdentifier:
            Assert(this->charClassifier->IsIdStart(ch));
            Assert(ch < 0x10000 && !IsMultiUnitChar((OLECHAR)ch));
            token = ScanIdentifierContinue(identifyKwds, false, false, m_pchMinTok, p, &p);
            break;

        case '`':
            Assert(chType == _C_BKQ);

            if (m_scriptContext->GetConfig()->IsES6StringTemplateEnabled())
            {
                pchT = p;
                token = ScanStringTemplateBegin(&pchT);
                p = pchT;
            }
            else
            {
                goto LLoopDefault;
            }
            break;

        case '}':
            Assert(chType == _C_RC);
            token = tkRCurly;
            break;

        case '\\':
            pchT = p - 1;
            token = ScanIdentifier(identifyKwds, &pchT);
            if (tkScanError == token)
            {
                m_currentCharacter = p;
                if (m_fSyntaxColor)
                    return ScanError(p, tkID);

                Error(ERRillegalChar);
            }
            p = pchT;
            break;


        case ':':
            token = tkColon;
            break;

        case '=':
            token = tkAsg;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkEQ;
                if (PeekFirst(p, last) == '=')
                {
                    p++;
                    token = tkEqv;
                }
                break;
            case '>':
                if (m_scriptContext->GetConfig()->IsES6LambdaEnabled())
                {
                    p++;
                    token = tkDArrow;
                }
                break;
            }
            break;
        case '!':
            token = tkBang;
            if (PeekFirst(p, last) == '=')
            {
                p++;
                token = tkNE;
                if (PeekFirst(p, last) == '=')
                {
                    p++;
                    token = tkNEqv;
                }
            }
            break;
        case '+':
            token = tkAdd;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkAsgAdd;
                break;
            case '+':
                p++;
                token = tkInc;
                break;
            }
            break;
        case '-':
            token = tkSub;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkAsgSub;
                break;
            case '-':
                p++;
                token = tkDec;
                if (m_fHtmlComments)
                {
                    int i = 0;
                    while ('-' == PeekFirst(p + i, last)) //Have already seen --, skip any further - characters
                        i++;
                    if ('>' == PeekFirst(p + i++, last)) //This means we've got a --------------------------->.
                    {
                        //If that precedes an EOF or }NWL (disregarding whitespace), then it is a comment.
                        OLECHAR nextChar;
                        nextChar = NextNonWhiteChar(&p[i], last);
                        if (nextChar == 0)
                        {
                            //Treat the -----------------------------> EOF as if it were EOF
                            token = tkEOF;
                            ++p;
                        }
                        else if (nextChar == '}')
                        {
                            CharTypes nextNextCharType = this->charClassifier->GetCharType(NextNonWhiteCharPlusOne(&p[i], last));
                            if (nextNextCharType == _C_NWL
                                // Corner case: If we have reached the end of the source, either we are at the end of the file or the end of
                                // a deferred function. We treat this case as NWL.
                                // TODO(tcare): Update to ES6 spec. Tracked in Bug 1164686
                                || (last == m_pchLast && nextNextCharType == _C_NUL))
                            {
                                //Treat the -----------------------------> }NWL as if it were }NWL
                                p += i;
                                continue;
                            }
                        }
                    }
                }
                break;
            }
            break;
        case '*':
            token = tkStar;
            switch(PeekFirst(p, last))
            {
            case '=' :
                p++;
                token = tkAsgMul;
                break;
            case '*' :
                if (!m_scriptContext->GetConfig()->IsES7ExponentiationOperatorEnabled())
                {
                    break;
                }
                p++;
                token = tkExpo;
                if (PeekFirst(p, last) == '=')
                {
                    p++;
                    token = tkAsgExpo;
                }
            }
            break;
        case '/':
            token = tkDiv;
            switch(PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkAsgDiv;
                break;
            case '/':
                if (p >= last)
                {
                    AssertMsg(m_fHtmlComments, "Do we have other line comment cases scanning pass last?");

                    // Effective source length may have excluded HTMLCommentSuffix "//... -->". If we are scanning
                    // those, we have passed "last" already. Move back and return EOF.
                    p = last;
                    goto LEof;
                }
                ch = *++p;
                firstChar = (OLECHAR)ch;
LSkipLineComment:
                pchT = NULL;
                for (;;)
                {
                    switch ((ch = ReadFirst(p, last)))
                    {
                    case kchLS:         // 0x2028, classifies as new line
                    case kchPS:         // 0x2029, classifies as new line
LEcmaCommentLineBreak:
                        // kchPS and kchLS are more than one unit in UTF-8.
                        if (pchT)
                        {
                            // kchPS and kchLS are more than one unit in UTF-8.
                            p = pchT;
                        }
                        else
                        {
                            // But only a single code unit in UTF16
                            p--;
                        }
                        RestoreMultiUnits(multiUnits);
                        goto LCommentLineBreak;

                    case kchNWL:
                    case kchRET:
                        p--;
LCommentLineBreak:
                        if (m_fSyntaxColor)
                        {
                            token = tkComment;
                            goto LDone;
                        }
                        // Subtract the comment length from the total char count for the purpose
                        // of deciding whether to defer AST and byte code generation.
                        m_parser->ReduceDeferredScriptLength((ULONG)(p - m_pchMinTok));
                        break;
                    case kchNUL:
                        if (p >= last)
                        {
                            p--;
                            goto LCommentLineBreak;
                        }
                        continue;

                    default:
                        if (IsMultiUnitChar((OLECHAR)ch))
                        {
                            pchT = p - 1;
                            multiUnits = m_cMultiUnits;
                            switch (ch = ReadRest<true>((OLECHAR)ch, p, last))
                            {
                                case kchLS:
                                case kchPS:
                                    goto LEcmaCommentLineBreak;
                            }
                        }
                        continue;
                    }

                    break;
                }

                continue;

            case '*':
                ch = *++p;
                firstChar = (OLECHAR)ch;
                if ((p + 1) < last)
                {
                    secondChar = (OLECHAR)(*(p + 1));
                }
                else
                {
                    secondChar = '\0';
                }


LMultiLineComment:
                pchT = p;
                commentStartLine = m_line;
                bool containTypeDef;
                if (tkNone == (token = SkipComment(&pchT, &containTypeDef)))
                {
                    // Subtract the comment length from the total char count for the purpose
                    // of deciding whether to defer AST and byte code generation.
                    m_parser->ReduceDeferredScriptLength((ULONG)(pchT - m_pchMinTok));
                    p = pchT;
                    goto LLoop;
                }
                p = pchT;
                break;
            }
            break;
        case '%':
            Assert(chType == _C_PCT);
            token = tkPct;
            if (PeekFirst(p, last) == '=')
            {
                p++;
                token = tkAsgMod;
            }
            break;
        case '<':
            Assert(chType == _C_LT);
            token = tkLT;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkLE;
                break;
            case '<':
                p++;
                token = tkLsh;
                if (PeekFirst(p, last) == '=')
                {
                    p++;
                    token = tkAsgLsh;
                    break;
                }
                break;
            case '!':
                if (m_fHtmlComments && PeekFirst(p + 1, last) == '-' && PeekFirst(p + 2, last) == '-')
                {
                    // This is a "<!--" comment - treat as //
                    if (p >= last)
                    {
                        // Effective source length may have excluded HTMLCommentSuffix "<!-- ... -->". If we are scanning
                        // those, we have passed "last" already. Move back and return EOF.
                        p = last;
                        goto LEof;
                    }
                    firstChar = '!';
                    goto LSkipLineComment;
                }
                break;
            }
            break;
        case '>':
            Assert(chType == _C_GT);
            token = tkGT;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkGE;
                break;
            case '>':
                p++;
                token = tkRsh;
                switch (PeekFirst(p, last))
                {
                case '=':
                    p++;
                    token = tkAsgRsh;
                    break;
                case '>':
                    p++;
                    token = tkRs2;
                    if (*p == '=')
                    {
                        p++;
                        token = tkAsgRs2;
                    }
                    break;
                }
                break;
            }
            break;
        case '^':
            Assert(chType == _C_XOR);
            token = tkXor;
            if (PeekFirst(p, last) == '=')
            {
                p++;
                token = tkAsgXor;
            }
            break;
        case '|':
            Assert(chType == _C_BAR);
            token = tkOr;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkAsgOr;
                break;
            case '|':
                p++;
                token = tkLogOr;
                break;
            }
            break;
        case '&':
            Assert(chType == _C_AMP);
            token = tkAnd;
            switch (PeekFirst(p, last))
            {
            case '=':
                p++;
                token = tkAsgAnd;
                break;
            case '&':
                p++;
                token = tkLogAnd;
                break;
            }
            break;
        case '\'':
        case '"':
            Assert(chType == _C_QUO || chType == _C_APO);
LScanStringConstant:
            pchT = p;
            token = ScanStringConstant((OLECHAR)ch, &pchT);
            p = pchT;
            break;
        }

        break;
    }

LDone:
    m_currentCharacter = p;
    return (m_ptoken->tk = token);
}

template <typename EncodingPolicy>
IdentPtr Scanner<EncodingPolicy>::GetSecondaryBufferAsPid()
{
    bool createPid = true;

    if (m_fSyntaxColor || (m_DeferredParseFlags & ScanFlagSuppressStrPid) != 0)
    {
        createPid = false;
    }

    if (createPid)
    {
        return m_phtbl->PidHashNameLen(m_tempChBufSecondary.m_prgch, m_tempChBufSecondary.m_ichCur);
    }
    else
    {
        return nullptr;
    }
}

template <typename EncodingPolicy>
LPCOLESTR Scanner<EncodingPolicy>::StringFromLong(long lw)
{
    _ltow_s(lw, m_tempChBuf.m_prgch, m_tempChBuf.m_cchMax, 10);
    return m_tempChBuf.m_prgch;
}

template <typename EncodingPolicy>
IdentPtr Scanner<EncodingPolicy>::PidFromLong(long lw)
{
    return m_phtbl->PidHashName(StringFromLong(lw));
}

template <typename EncodingPolicy>
LPCOLESTR Scanner<EncodingPolicy>::StringFromDbl(double dbl)
{
    if (!Js::NumberUtilities::FDblToStr(dbl, m_tempChBuf.m_prgch, m_tempChBuf.m_cchMax))
    {
        Error(ERRnoMemory);
    }
    return m_tempChBuf.m_prgch;
}

template <typename EncodingPolicy>
IdentPtr Scanner<EncodingPolicy>::PidFromDbl(double dbl)
{
    return m_phtbl->PidHashName(StringFromDbl(dbl));
}


template <typename EncodingPolicy>
void Scanner<EncodingPolicy>::Capture(_Out_ RestorePoint* restorePoint)
{
    Capture(restorePoint, 0, 0);
}

template <typename EncodingPolicy>
void Scanner<EncodingPolicy>::Capture(_Out_ RestorePoint* restorePoint, uint functionIdIncrement, size_t lengthDecr)
{
    restorePoint->m_ichMinTok = this->IchMinTok();
    restorePoint->m_ichMinLine = this->IchMinLine();
    restorePoint->m_cMinTokMultiUnits = this->m_cMinTokMultiUnits;
    restorePoint->m_cMinLineMultiUnits = this->m_cMinLineMultiUnits;
    restorePoint->m_line = this->m_line;
    restorePoint->m_fHadEol = this->m_fHadEol;

    restorePoint->functionIdIncrement = functionIdIncrement;
    restorePoint->lengthDecr = lengthDecr;

#ifdef DEBUG
    restorePoint->m_cMultiUnits = this->m_cMultiUnits;
#endif
}

template <typename EncodingPolicy>
void Scanner<EncodingPolicy>::SeekTo(const RestorePoint& restorePoint)
{
    this->m_currentCharacter = this->m_pchBase + restorePoint.m_ichMinTok + restorePoint.m_cMinTokMultiUnits;
    this->m_pchMinLine = this->m_pchBase + restorePoint.m_ichMinLine + restorePoint.m_cMinLineMultiUnits;
    this->m_cMinLineMultiUnits = restorePoint.m_cMinLineMultiUnits;
    this->RestoreMultiUnits(restorePoint.m_cMinTokMultiUnits);
    this->Scan();

    this->m_line = restorePoint.m_line;
    this->m_fHadEol = restorePoint.m_fHadEol;

    this->m_parser->ReduceDeferredScriptLength(restorePoint.lengthDecr);

    Assert(this->m_cMultiUnits == restorePoint.m_cMultiUnits);
}

template <typename EncodingPolicy>
void Scanner<EncodingPolicy>::SeekTo(const RestorePoint& restorePoint, uint *nextFunctionId)
{
    SeekTo(restorePoint);
    *nextFunctionId += restorePoint.functionIdIncrement;
}

// Called by CompileScriptException::ProcessError to retrieve a BSTR for the line on which an error occurred.
template<typename EncodingPolicy>
HRESULT Scanner<EncodingPolicy>::SysAllocErrorLine(long ichMinLine, __out BSTR* pbstrLine)
{
    if( !pbstrLine )
    {
        return E_POINTER;
    }

    // If we overflow the string, we have a serious problem...
    if (ichMinLine < 0 || static_cast<size_t>(ichMinLine) > AdjustedLength() )
    {
        return E_UNEXPECTED;
    }

    EncodedCharPtr pStart = static_cast<size_t>(ichMinLine) == IchMinLine() ? m_pchMinLine : m_pchBase + CharacterOffsetToUnitOffset(m_pchBase, m_currentCharacter, m_pchLast, ichMinLine);
    EncodedCharPtr pEnd = AdjustedLast();

    // Determine the length by scanning for the next newline
    charcount_t cch = LineLength(pStart, pEnd);
    Assert(cch <= LONG_MAX);

    *pbstrLine = SysAllocStringLen(NULL, cch);
    if (!*pbstrLine)
    {
        return E_OUTOFMEMORY;
    }

    ConvertToUnicode(*pbstrLine, cch, pStart);
    return S_OK;
}

template class Scanner<NullTerminatedUnicodeEncodingPolicy>;
template class Scanner<NullTerminatedUTF8EncodingPolicy>;
template class Scanner<NotNullTerminatedUTF8EncodingPolicy>;

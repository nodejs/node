//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class DelayLoadWindowsGlobalization;
}
#include "Windows.Globalization.h"


int CountNewlines(LPCOLESTR psz, int cch = -1);

class Parser;
struct ParseContext;

struct Token
{
private:
    union
    {
        struct
        {
            IdentPtr pid;
            const char * pchMin;
            long length;
        };
        long lw;
        struct
        {
            double dbl;
            // maybeInt will be true if the number did not contain 'e', 'E' , or '.'
            // notably important in asm.js where the '.' has semantic importance
            bool maybeInt;
        };
        UnifiedRegex::RegexPattern* pattern;
        struct
        {
            charcount_t ichMin;
            charcount_t ichLim;
        };
    } u;
    IdentPtr CreateIdentifier(HashTbl * hashTbl);
public:
    Token() : tk(tkLim) {}
    tokens tk;

    BOOL IsIdentifier() const
    {
        return tk == tkID;
    }

    IdentPtr GetStr() const
    {
        Assert(tk == tkStrCon || tk == tkStrTmplBasic || tk == tkStrTmplBegin || tk == tkStrTmplMid || tk == tkStrTmplEnd);
        return u.pid;
    }
    IdentPtr GetIdentifier(HashTbl * hashTbl)
    {
        Assert(IsIdentifier() || IsReservedWord());
        if (u.pid)
        {
            return u.pid;
        }
        return CreateIdentifier(hashTbl);
    }

    long GetLong() const
    {
        Assert(tk == tkIntCon);
        return u.lw;
    }

    double GetDouble() const
    {
        Assert(tk == tkFltCon);
        return u.dbl;
    }

    bool GetDoubleMayBeInt() const
    {
        Assert(tk == tkFltCon);
        return u.maybeInt;
    }
    UnifiedRegex::RegexPattern * GetRegex()
    {
        Assert(tk == tkRegExp);
        return u.pattern;
    }

    // NOTE: THESE ROUTINES DEPEND ON THE ORDER THAT OPERATORS
    // ARE DECLARED IN kwd-xxx.h FILES.

    BOOL IsReservedWord() const
    {
        // Keywords and future reserved words (does not include operators)
        return tk < tkID;
    }

    BOOL IsKeyword() const;

    BOOL IsFutureReservedWord(const BOOL isStrictMode) const
    {
        // Reserved words that are not keywords
        return tk >= tkENUM && tk <= (isStrictMode ? tkSTATIC : tkENUM);
    }

    BOOL IsOperator() const
    {
        return tk >= tkComma && tk < tkLParen;
    }

    // UTF16 Scanner are only for syntax coloring.  Only support
    // defer pid creation for UTF8
    void SetIdentifier(const char * pchMin, long len)
    {
        this->u.pid = nullptr;
        this->u.pchMin = pchMin;
        this->u.length = len;
    }
    void SetIdentifier(IdentPtr pid)
    {
        this->u.pid = pid;
        this->u.pchMin = nullptr;
    }

    void SetLong(long value)
    {
        this->u.lw = value;
    }

    void SetDouble(double dbl, bool maybeInt)
    {
        this->u.dbl = dbl;
        this->u.maybeInt = maybeInt;
    }

    tokens SetRegex(UnifiedRegex::RegexPattern *const pattern, Parser *const parser);
};

typedef BYTE UTF8Char;
typedef UTF8Char* UTF8CharPtr;

class NullTerminatedUnicodeEncodingPolicy
{
public:
    typedef OLECHAR EncodedChar;
    typedef const OLECHAR *EncodedCharPtr;

protected:
    static const bool MultiUnitEncoding = false;
    static const size_t m_cMultiUnits = 0;

    static BOOL IsMultiUnitChar(OLECHAR ch) { return FALSE; }
    // See comment below regarding unused 'last' parameter
    static OLECHAR ReadFirst(EncodedCharPtr &p, EncodedCharPtr last) { return *p++; }
    template <bool bScan>
    static OLECHAR ReadRest(OLECHAR ch, EncodedCharPtr &p, EncodedCharPtr last) { return ch; }
    template <bool bScan>
    static OLECHAR ReadFull(EncodedCharPtr &p, EncodedCharPtr last) { return *p++; }
    static OLECHAR PeekFirst(EncodedCharPtr p, EncodedCharPtr last) { return *p; }
    static OLECHAR PeekFull(EncodedCharPtr p, EncodedCharPtr last) { return *p; }
    static void RestoreMultiUnits(size_t multiUnits) { }
    static size_t CharacterOffsetToUnitOffset(EncodedCharPtr start, EncodedCharPtr current, EncodedCharPtr last, charcount_t offset) { return offset; }

    static void ConvertToUnicode(__out_ecount_full(cch) LPOLESTR pch, charcount_t cch, EncodedCharPtr pu)
    {
        js_memcpy_s(pch, cch * sizeof(OLECHAR), pu, cch * sizeof(OLECHAR));
    }

public:
    void FromExternalSource() { }
    bool IsFromExternalSource() { return false; }
};

template <bool nullTerminated>
class UTF8EncodingPolicyBase
{
public:
    typedef utf8char_t EncodedChar;
    typedef LPCUTF8 EncodedCharPtr;

protected:
    static const bool MultiUnitEncoding = true;

    size_t m_cMultiUnits;
    utf8::DecodeOptions m_decodeOptions;

    UTF8EncodingPolicyBase(): m_cMultiUnits(0), m_decodeOptions(utf8::doAllowThreeByteSurrogates) { }

    static BOOL IsMultiUnitChar(OLECHAR ch) { return ch > 0x7f; }
    // Note when nullTerminated is false we still need to increment the character pointer because the scanner "puts back" this virtual null character by decrementing the pointer
    static OLECHAR ReadFirst(EncodedCharPtr &p, EncodedCharPtr last) { return (nullTerminated || p < last) ? static_cast< OLECHAR >(*p++) : (p++, 0); }

    // "bScan" indicates if this ReadFull is part of scanning. Pass true during scanning and ReadFull will update
    // related Scanner state. The caller is supposed to sync result "p" to Scanner's current position. Pass false
    // otherwise and this doesn't affect Scanner state.
    template <bool bScan>
    OLECHAR ReadFull(EncodedCharPtr &p, EncodedCharPtr last)
    {
        EncodedChar ch = (nullTerminated || p < last) ? *p++ : (p++, 0);
        return !IsMultiUnitChar(ch) ? static_cast< OLECHAR >(ch) : ReadRest<bScan>(ch, p, last);
    }

    static OLECHAR PeekFirst(EncodedCharPtr p, EncodedCharPtr last) { return (nullTerminated || p < last) ? static_cast< OLECHAR >(*p) : 0; }

    OLECHAR PeekFull(EncodedCharPtr p, EncodedCharPtr last)
    {
        OLECHAR result = PeekFirst(p, last);
        if (IsMultiUnitChar(result))
        {
            result = ReadFull<false>(p, last);
        }
        return result;
    }

    // "bScan" indicates if this ReadRest is part of scanning. Pass true during scanning and ReadRest will update
    // related Scanner state. The caller is supposed to sync result "p" to Scanner's current position. Pass false
    // otherwise and this doesn't affect Scanner state.
    template <bool bScan>
    OLECHAR ReadRest(OLECHAR ch, EncodedCharPtr &p, EncodedCharPtr last)
    {
        EncodedCharPtr s;
        if (bScan)
        {
            s = p;
        }
        OLECHAR result = utf8::DecodeTail(ch, p, last, m_decodeOptions);
        if (bScan)
        {
            // If we are scanning, update m_cMultiUnits counter.
            m_cMultiUnits += p - s;
        }
        return result;
    }
    void RestoreMultiUnits(size_t multiUnits) { m_cMultiUnits = multiUnits; }

    size_t CharacterOffsetToUnitOffset(EncodedCharPtr start, EncodedCharPtr current, EncodedCharPtr last, charcount_t offset)
    {
        // Note: current may be before or after last. If last is the null terminator, current should be within [start, last].
        // But if we excluded HTMLCommentSuffix for the source, last is before "// -->\0". Scanner may stop at null
        // terminator past last, then current is after last.
        Assert(current >= start);
        size_t currentUnitOffset = current - start;
        Assert(currentUnitOffset > m_cMultiUnits);
        Assert(currentUnitOffset - m_cMultiUnits < LONG_MAX);
        charcount_t currentCharacterOffset = charcount_t(currentUnitOffset - m_cMultiUnits);

        // If the offset is the current character offset then just return the current unit offset.
        if (currentCharacterOffset == offset) return currentUnitOffset;

        // If we have not encountered any multi-unit characters and we are moving backward the
        // character index and unit index are 1:1 so just return offset
        if (m_cMultiUnits == 0 && offset <= currentCharacterOffset) return offset;

        // Use local decode options
        utf8::DecodeOptions decodeOptions = IsFromExternalSource() ? utf8::doDefault : utf8::doAllowThreeByteSurrogates;

        if (offset > currentCharacterOffset)
        {
            // If we are looking for an offset past current, current must be within [start, last]. We don't expect seeking
            // scanner position past last.
            Assert(current <= last);

            // If offset > currentOffset we already know the current character offset. The unit offset is the
            // unit index of offset - currentOffset characters from current.
            charcount_t charsLeft = offset - currentCharacterOffset;
            return currentUnitOffset + utf8::CharacterIndexToByteIndex(current, last - current, charsLeft, decodeOptions);
        }

        // If all else fails calculate the index from the start of the buffer.
        return utf8::CharacterIndexToByteIndex(start, currentUnitOffset, offset, decodeOptions);
    }

    void ConvertToUnicode(__out_ecount_full(cch) LPOLESTR pch, charcount_t cch, EncodedCharPtr pu)
    {
        m_decodeOptions = (utf8::DecodeOptions)(m_decodeOptions & ~utf8::doSecondSurrogatePair);
        utf8::DecodeInto(pch, pu, cch, m_decodeOptions);
    }


public:
    // If we get UTF8 source buffer, turn off doAllowThreeByteSurrogates but allow invalid WCHARs without replacing them with replacement 'g_chUnknown'.
    void FromExternalSource() { m_decodeOptions = (utf8::DecodeOptions)(m_decodeOptions & ~utf8::doAllowThreeByteSurrogates | utf8::doAllowInvalidWCHARs); }
    bool IsFromExternalSource() { return (m_decodeOptions & utf8::doAllowThreeByteSurrogates) == 0; }
};

typedef UTF8EncodingPolicyBase<true> NullTerminatedUTF8EncodingPolicy;
typedef UTF8EncodingPolicyBase<false> NotNullTerminatedUTF8EncodingPolicy;

interface IScanner
{
    virtual void GetErrorLineInfo(__out long& ichMin, __out long& ichLim, __out long& line, __out long& ichMinLine) = 0;
    virtual HRESULT SysAllocErrorLine(long ichMinLine, __out BSTR* pbstrLine) = 0;
};

// Flags that can be provided to the Scan functions.
// These can be bitwise OR'ed.
enum ScanFlag
{
    ScanFlagNone = 0,
    ScanFlagSuppressStrPid = 1,   // Force strings to always have pid
    ScanFlagSuppressIdPid = 2     // Force identifiers to always have pid
};

typedef HRESULT (*CommentCallback)(void *data, OLECHAR firstChar, OLECHAR secondChar, bool containTypeDef, charcount_t min, charcount_t lim, bool adjacent, bool multiline, charcount_t startLine, charcount_t endLine);

// Restore point defined using a relative offset rather than a pointer.
struct RestorePoint
{
    charcount_t m_ichMinTok;
    charcount_t m_ichMinLine;
    size_t m_cMinTokMultiUnits;
    size_t m_cMinLineMultiUnits;
    charcount_t m_line;
    uint functionIdIncrement;
    size_t lengthDecr;
    BOOL m_fHadEol;

#ifdef DEBUG
    size_t m_cMultiUnits;
#endif

    RestorePoint()
        : m_ichMinTok((charcount_t)-1),
          m_ichMinLine((charcount_t)-1),
          m_cMinTokMultiUnits((size_t)-1),
          m_cMinLineMultiUnits((size_t)-1),
          m_line((charcount_t)-1),
          functionIdIncrement(0),
          lengthDecr(0),
          m_fHadEol(FALSE)
#ifdef DEBUG
          , m_cMultiUnits((size_t)-1)
#endif
    {
    };
};

template <typename EncodingPolicy>
class Scanner : public IScanner, public EncodingPolicy
{
    friend Parser;
    typedef typename EncodingPolicy::EncodedChar EncodedChar;
    typedef typename EncodingPolicy::EncodedCharPtr EncodedCharPtr;

public:
    static Scanner * Create(Parser* parser, HashTbl *phtbl, Token *ptoken, ErrHandler *perr, Js::ScriptContext *scriptContext)
    {
        return HeapNewNoThrow(Scanner, parser, phtbl, ptoken, perr, scriptContext);
    }
    void Release(void)
    {
        delete this;
    }

    tokens Scan();
    tokens ScanNoKeywords();
    tokens ScanForcingPid();
    void SetText(EncodedCharPtr psz, size_t offset, size_t length, charcount_t characterOffset, ULONG grfscr, ULONG lineNumber = 0);
    void PrepareForBackgroundParse(Js::ScriptContext *scriptContext);

    enum ScanState
    {
        ScanStateNormal = 0,
        ScanStateMultiLineComment = 1,
        ScanStateMultiLineSingleQuoteString = 2,
        ScanStateMultiLineDoubleQuoteString = 3,
        ScanStateStringTemplateMiddleOrEnd = 4,
    };

    ScanState GetScanState() { return m_scanState; }
    void SetScanState(ScanState state) { m_scanState = state; }

    bool SetYieldIsKeyword(bool fYieldIsKeyword)
    {
        bool fPrevYieldIsKeyword = m_fYieldIsKeyword;
        m_fYieldIsKeyword = fYieldIsKeyword;
        return fPrevYieldIsKeyword;
    }
    bool YieldIsKeyword()
    {
        return m_fYieldIsKeyword;
    }

    bool SetAwaitIsKeyword(bool fAwaitIsKeyword)
    {
        bool fPrevAwaitIsKeyword = m_fAwaitIsKeyword;
        m_fAwaitIsKeyword = fAwaitIsKeyword;
        return fPrevAwaitIsKeyword;
    }
    bool AwaitIsKeyword()
    {
        return m_fAwaitIsKeyword;
    }

    tokens TryRescanRegExp();
    tokens RescanRegExp();
    tokens RescanRegExpNoAST();
    tokens RescanRegExpTokenizer();

    BOOL FHadNewLine(void)
    {
        return m_fHadEol;
    }
    IdentPtr PidFromLong(long lw);
    IdentPtr PidFromDbl(double dbl);

    LPCOLESTR StringFromLong(long lw);
    LPCOLESTR StringFromDbl(double dbl);

    IdentPtr GetSecondaryBufferAsPid();

    bool BindDeferredPidRefs() const
    {
        return m_scriptContext->GetConfig()->BindDeferredPidRefs();
    }

    BYTE SetDeferredParse(BOOL defer)
    {
        BYTE fOld = m_DeferredParseFlags;
        if (defer)
        {
            m_DeferredParseFlags |= ScanFlagSuppressStrPid;
            if (!this->BindDeferredPidRefs())
            {
                m_DeferredParseFlags |= ScanFlagSuppressIdPid;
            }
        }
        else
        {
            m_DeferredParseFlags = ScanFlagNone;
        }
        return fOld;
    }

    void SetDeferredParseFlags(BYTE flags)
    {
        m_DeferredParseFlags = flags;
    }

    // the functions IsDoubleQuoteOnLastTkStrCon() and IsHexOrOctOnLastTKNumber() works only with a scanner without lookahead
    // Both functions are used to get more info on the last token for specific diffs necessary for JSON parsing.


    //Single quotes are not legal in JSON strings. Make distinction between single quote string constant and single quote string
    BOOL IsDoubleQuoteOnLastTkStrCon()
    {
        return m_doubleQuoteOnLastTkStrCon;
    }

    // True if all chars of last string constant are ascii
    BOOL IsEscapeOnLastTkStrCon()
    {
      return m_EscapeOnLastTkStrCon;
    }


    bool IsOctOrLeadingZeroOnLastTKNumber()
    {
        return m_OctOrLeadingZeroOnLastTKNumber;
    }

    // Returns the character offset of the first token. The character offset is the offset the first character of the token would
    // have if the entire file was converted to Unicode (UTF16-LE).
    charcount_t IchMinTok(void) const
    {
        Assert(m_pchMinTok - m_pchBase >= 0);
        Assert(m_pchMinTok - m_pchBase <= LONG_MAX);
        return static_cast< charcount_t >(m_pchMinTok - m_pchBase - m_cMinTokMultiUnits);
    }

    // Returns the character offset of the character immediately following the token. The character offset is the offset the first
    // character of the token would have if the entire file was converted to Unicode (UTF16-LE).
    charcount_t IchLimTok(void) const
    {
        Assert(m_currentCharacter - m_pchBase >= 0);
        Assert(m_currentCharacter - m_pchBase <= LONG_MAX);
        return static_cast< charcount_t >(m_currentCharacter - m_pchBase - m_cMultiUnits);
    }

    void SetErrorPosition(charcount_t ichMinError, charcount_t ichLimError)
    {
        Assert(ichLimError > 0 || ichMinError == 0);
        m_ichMinError = ichMinError;
        m_ichLimError = ichLimError;
    }

    charcount_t IchMinError(void) const
    {
        return m_ichLimError ? m_ichMinError : IchMinTok();
    }

    charcount_t IchLimError(void) const
    {
        return m_ichLimError ? m_ichLimError : IchLimTok();
    }

    // Returns the encoded unit offset of first character of the token. For example, in a UTF-8 encoding this is the offset into
    // the UTF-8 buffer. In Unicode this is the same as IchMinTok().
    size_t IecpMinTok(void) const
    {
        return static_cast< size_t >(m_pchMinTok  - m_pchBase);
    }

    // Returns the encoded unit offset of the character immediately following the token. For example, in a UTF-8 encoding this is
    // the offset into the UTF-8 buffer. In Unicode this is the same as IchLimTok().
    size_t IecpLimTok(void) const
    {
        return static_cast< size_t >(m_currentCharacter - m_pchBase);
    }

    size_t IecpLimTokPrevious() const
    {
        AssertMsg(m_iecpLimTokPrevious != (size_t)-1, "IecpLimTokPrevious() cannot be called before scanning a token");
        return m_iecpLimTokPrevious;
    }

    IdentPtr PidAt(size_t iecpMin, size_t iecpLim);

    // Returns the character offset within the stream of the first character on the current line.
    charcount_t IchMinLine(void) const
    {
        Assert(m_pchMinLine - m_pchBase >= 0);
        Assert(m_pchMinLine - m_pchBase <= LONG_MAX);
        return static_cast<charcount_t>(m_pchMinLine - m_pchBase - m_cMinLineMultiUnits);
    }

    // Returns the current line number
    charcount_t LineCur(void) { return m_line; }

    tokens ErrorToken() { return m_errorToken; }

    void SetCurrentCharacter(charcount_t offset, ULONG lineNumber = 0)
    {
        DebugOnly(m_iecpLimTokPrevious = (size_t)-1);
        size_t length = m_pchLast - m_pchBase;
        if (offset > length) offset = static_cast< charcount_t >(length);
        size_t ibOffset = CharacterOffsetToUnitOffset(m_pchBase, m_currentCharacter, m_pchLast, offset);
        m_currentCharacter = m_pchBase + ibOffset;
        Assert(ibOffset >= offset);
        RestoreMultiUnits(ibOffset - offset);
        m_line = lineNumber;
    }

    // IScanner methods
    virtual void GetErrorLineInfo(__out long& ichMin, __out long& ichLim, __out long& line, __out long& ichMinLine)
    {
        ichMin = this->IchMinError();
        ichLim = this->IchLimError();
        line   = this->LineCur();
        ichMinLine = this->IchMinLine();
        if (m_ichLimError && m_ichMinError < (charcount_t)ichMinLine)
        {
            line = m_startLine;
            ichMinLine = UpdateLine(line, m_pchStartLine, m_pchLast, 0, ichMin);
        }
    }

    virtual HRESULT SysAllocErrorLine(long ichMinLine, __out BSTR* pbstrLine);
    charcount_t UpdateLine(long &line, EncodedCharPtr start, EncodedCharPtr last, charcount_t ichStart, charcount_t ichEnd);
    class TemporaryBuffer
    {
        friend Scanner<EncodingPolicy>;

    private:
        // Keep a reference to the scanner.
        // We will use it to signal an error if we fail to allocate the buffer.
        Scanner<EncodingPolicy>* m_pscanner;
        ulong m_cchMax;
        ulong m_ichCur;
        __field_ecount(m_cchMax) OLECHAR *m_prgch;
        byte m_rgbInit[256];

    public:
        TemporaryBuffer()
        {
            m_pscanner = nullptr;
            m_prgch = (OLECHAR*)m_rgbInit;
            m_cchMax = _countof(m_rgbInit) / sizeof(OLECHAR);
            m_ichCur = 0;
        }

        ~TemporaryBuffer()
        {
            if (m_prgch != (OLECHAR*)m_rgbInit)
            {
                free(m_prgch);
            }
        }

        void Init()
        {
            m_ichCur = 0;
        }

        void AppendCh(uint ch)
        {
            return AppendCh<true>(ch);
        }

        template<bool performAppend> void AppendCh(uint ch)
        {
            if (performAppend)
            {
                if (m_ichCur >= m_cchMax)
                {
                    Grow();
                }

                Assert(m_ichCur < m_cchMax);
                __analysis_assume(m_ichCur < m_cchMax);

                m_prgch[m_ichCur++] = static_cast<OLECHAR>(ch);
            }
        }

        void Grow()
        {
            Assert(m_pscanner != nullptr);
            byte *prgbNew;
            byte *prgbOld = (byte *)m_prgch;

            unsigned long cbNew;
            if (FAILED(ULongMult(m_cchMax, sizeof(OLECHAR) * 2, &cbNew)))
            {
                m_pscanner->Error(ERRnoMemory);
            }

            if (prgbOld == m_rgbInit)
            {
                if (nullptr == (prgbNew = static_cast<byte*>(malloc(cbNew))))
                    m_pscanner->Error(ERRnoMemory);
                js_memcpy_s(prgbNew, cbNew, prgbOld, m_ichCur * sizeof(OLECHAR));
            }
            else if (nullptr == (prgbNew = static_cast<byte*>(realloc(prgbOld, cbNew))))
            {
                m_pscanner->Error(ERRnoMemory);
            }

            m_prgch = (OLECHAR*)prgbNew;
            m_cchMax = cbNew / sizeof(OLECHAR);
        }
    };

    void Capture(_Out_ RestorePoint* restorePoint);
    void SeekTo(const RestorePoint& restorePoint);

    void Capture(_Out_ RestorePoint* restorePoint, uint functionIdIncrement, size_t lengthDecr);
    void SeekTo(const RestorePoint& restorePoint, uint *nextFunctionId);

    void SetNextStringTemplateIsTagged(BOOL value)
    {
        this->m_fNextStringTemplateIsTagged = value;
    }

private:
    Parser *m_parser;
    HashTbl *m_phtbl;
    Token *m_ptoken;
    EncodedCharPtr m_pchBase;          // beginning of source
    EncodedCharPtr m_pchLast;          // The end of source
    EncodedCharPtr m_pchMinLine;       // beginning of current line
    EncodedCharPtr m_pchMinTok;        // beginning of current token
    EncodedCharPtr m_currentCharacter; // current character
    EncodedCharPtr m_pchPrevLine;      // beginning of previous line
    size_t m_cMinTokMultiUnits;        // number of multi-unit characters previous to m_pchMinTok
    size_t m_cMinLineMultiUnits;       // number of multi-unit characters previous to m_pchMinLine
    ErrHandler *m_perr;                // error handler to use
    uint16 m_fStringTemplateDepth;     // we should treat } as string template middle starting character (depth instead of flag)
    BOOL m_fHadEol;
    BOOL m_fHtmlComments : 1;
    BOOL m_doubleQuoteOnLastTkStrCon :1;
    bool m_OctOrLeadingZeroOnLastTKNumber :1;
    BOOL m_fSyntaxColor : 1;            // whether we're just syntax coloring
    BOOL m_EscapeOnLastTkStrCon:1;
    BOOL m_fNextStringTemplateIsTagged:1;   // the next string template scanned has a tag (must create raw strings)
    BYTE m_DeferredParseFlags:2;            // suppressStrPid and suppressIdPid
    charcount_t m_ichCheck;             // character at which completion is to be computed.
    bool es6UnicodeMode;                // True if ES6Unicode Extensions are enabled.
    bool m_fYieldIsKeyword;             // Whether to treat 'yield' as an identifier or keyword
    bool m_fAwaitIsKeyword;             // Whether to treat 'await' as an identifier or keyword

    // Temporary buffer.
    TemporaryBuffer m_tempChBuf;
    TemporaryBuffer m_tempChBufSecondary;

    charcount_t m_line;
    ScanState m_scanState;
    tokens m_errorToken;

    charcount_t m_ichMinError;
    charcount_t m_ichLimError;
    charcount_t m_startLine;
    EncodedCharPtr m_pchStartLine;

    Js::ScriptContext* m_scriptContext;
    const Js::CharClassifier *charClassifier;

    tokens m_tkPrevious;
    size_t m_iecpLimTokPrevious;

    Scanner(Parser* parser, HashTbl *phtbl, Token *ptoken, ErrHandler *perr, Js::ScriptContext *scriptContext);
    ~Scanner(void);

    tokens ScanCore(bool identifyKwds);
    tokens ScanAhead();

    tokens ScanError(EncodedCharPtr pchCur, tokens errorToken)
    {
        m_currentCharacter = pchCur;
        m_errorToken = errorToken;
        return m_ptoken->tk = tkScanError;
    }

    __declspec(noreturn) void Error(HRESULT hr)
    {
        Assert(FAILED(hr));
        m_pchMinTok = m_currentCharacter;
        m_cMinTokMultiUnits = m_cMultiUnits;
        AssertMem(m_perr);
        m_perr->Throw(hr);
    }

    const EncodedCharPtr PchBase(void)
    {
        return m_pchBase;
    }
    const EncodedCharPtr PchMinTok(void)
    {
        return m_pchMinTok;
    }

    template<bool stringTemplateMode, bool createRawString> tokens ScanStringConstant(OLECHAR delim, EncodedCharPtr *pp);
    tokens ScanStringConstant(OLECHAR delim, EncodedCharPtr *pp);

    tokens ScanStringTemplateBegin(EncodedCharPtr *pp);
    tokens ScanStringTemplateMiddleOrEnd(EncodedCharPtr *pp);

    void ScanNewLine(uint ch);
    void NotifyScannedNewLine();
    charcount_t LineLength(EncodedCharPtr first, EncodedCharPtr last);

    tokens ScanIdentifier(bool identifyKwds, EncodedCharPtr *pp);
    BOOL FastIdentifierContinue(EncodedCharPtr&p, EncodedCharPtr last);
    tokens ScanIdentifierContinue(bool identifyKwds, bool fHasEscape, bool fHasMultiChar, EncodedCharPtr pchMin, EncodedCharPtr p, EncodedCharPtr *pp);
    tokens SkipComment(EncodedCharPtr *pp, /* out */ bool* containTypeDef);
    tokens ScanRegExpConstant(ArenaAllocator* alloc);
    tokens ScanRegExpConstantNoAST(ArenaAllocator* alloc);
    BOOL oFScanNumber(double *pdbl, bool& likelyInt);
    EncodedCharPtr FScanNumber(EncodedCharPtr p, double *pdbl, bool& likelyInt);
    IdentPtr PidOfIdentiferAt(EncodedCharPtr p, EncodedCharPtr last, bool fHadEscape, bool fHasMultiChar);
    IdentPtr PidOfIdentiferAt(EncodedCharPtr p, EncodedCharPtr last);
    ulong UnescapeToTempBuf(EncodedCharPtr p, EncodedCharPtr last);

    void SaveSrcPos(void)
    {
        m_pchMinTok = m_currentCharacter;
    }
    OLECHAR PeekNextChar(void)
    {
        return PeekFull(m_currentCharacter, m_pchLast);
    }
    OLECHAR ReadNextChar(void)
    {
        return ReadFull<true>(m_currentCharacter, m_pchLast);
    }
    OLECHAR NextNonWhiteChar(EncodedCharPtr p, EncodedCharPtr last)
    {
        OLECHAR ch;
        do
        {
            ch = ReadFull<false>(p, last);
        }
        while (this->charClassifier->IsWhiteSpace(ch));
        return ch;
    }
    OLECHAR NextNonWhiteCharPlusOne(EncodedCharPtr p, EncodedCharPtr last)
    {
        OLECHAR ch;
        do
        {
            ch = ReadFull<false>(p, last);
        }
        while (this->charClassifier->IsWhiteSpace(ch));
        return ReadFull<false>(p, last);
    }

    EncodedCharPtr AdjustedLast() const
    {
        return m_pchLast;
    }

    size_t AdjustedLength() const
    {
        return AdjustedLast() - m_pchBase;
    }

    bool IsStrictMode() const
    {
        return this->m_parser != NULL && this->m_parser->IsStrictMode();
    }

    // This function expects the first character to be a 'u'
    // It will attempt to return a codepoint represented by a single escape point (either of the form \uXXXX or \u{any number of hex characters, s.t. value < 0x110000}
    bool TryReadEscape(EncodedCharPtr &startingLocation, EncodedCharPtr endOfSource, codepoint_t *outChar = nullptr);

    template <bool bScan>
    bool TryReadCodePointRest(codepoint_t lower, EncodedCharPtr &startingLocation, EncodedCharPtr endOfSource, codepoint_t *outChar, bool *outContainsMultiUnitChar);

    template <bool bScan>
    __inline bool TryReadCodePoint(EncodedCharPtr &startingLocation, EncodedCharPtr endOfSource, codepoint_t *outChar, bool *hasEscape, bool *outContainsMultiUnitChar);

    __inline BOOL IsIdContinueNext(EncodedCharPtr startingLocation, EncodedCharPtr endOfSource)
    {
        codepoint_t nextCodepoint;
        bool ignore;

        if (TryReadCodePoint<false>(startingLocation, endOfSource, &nextCodepoint, &ignore, &ignore))
        {
            return charClassifier->IsIdContinue(nextCodepoint);
        }

        return false;
    }

};

typedef Scanner<NullTerminatedUTF8EncodingPolicy> UTF8Scanner;

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // Trigrams
    // ----------------------------------------------------------------------

    TrigramInfo::TrigramInfo(__in_ecount(PatternLength) char* pat1,__in_ecount(PatternLength) char* pat2, Recycler* recycler)
    {
        isTrigramPattern=true;
        hasCachedResultString = false;

        int k;
        triPat1=0;
        triPat2=0;
        resultCount=0;
        for (k=3;k<PatternLength;k++) {
            triPat1=(triPat1<<4)+pat1[k];
            triPat2=(triPat2<<4)+pat2[k];
        }
    }

    void TrigramAlphabet::InitTrigramMap() {
        input=NULL;
        // set up mapping from 9 bits to trigram
        for (int i=0;i<TrigramMapSize;i++) {
            int t1=i>>6;
            int t2=(i>>3)&0x7;
            int t3=i&0x7;
            if ((t1>=AlphaCount)||(t2>=AlphaCount)||(t3>=AlphaCount)) {
                trigramMap[i]=TrigramNotInPattern;
            }
            else {
                // number of trigram
                trigramMap[i]=(char)((t1<<4)+(t2<<2)+t3);
            }
        }

        for (int j=0;j<TrigramCount;j++) {
            trigramStarts[j].count=0;
        }
    }

    bool TrigramAlphabet::AddStarts(__in_xcount(TrigramInfo::PatternLength) char* pat1,__in_xcount(TrigramInfo::PatternLength) char* pat2, RegexPattern* pattern)
    {
        for (int k=0;k<TrigramCount;k++) {
            char t1=1<<(k>>4);
            char t2=1<<((k>>2)&0x3);
            char t3=1<<(k&0x3);
            if ((t1&pat1[0])&&(t2&pat1[1])&&(t3&pat1[2])) {
                if ((t1&pat2[0])&&(t2&pat2[1])&&(t3&pat2[2])) {
                    return false;
                }
                else {
                    TrigramStart* trigramStart=(&trigramStarts[k]);
                    if (trigramStart->count>=TrigramStart::MaxPatPerStart) {
                        return false;
                    }
                    else {
                        PatternTri* tri= &(trigramStart->patterns[trigramStart->count++]);
                        tri->pattern=pattern;
                        tri->encodedPattern=pattern->rep.unified.trigramInfo->triPat1;
                    }
                }
            }
            else if ((t1&pat2[0])&&(t2&pat2[1])&&(t3&pat2[2])) {
                TrigramStart* trigramStart=(&trigramStarts[k]);
                if (trigramStart->count>=TrigramStart::MaxPatPerStart) {
                    return false;
                }
                else {
                    PatternTri* tri= &(trigramStart->patterns[trigramStart->count++]);
                    tri->pattern=pattern;
                    tri->encodedPattern=pattern->rep.unified.trigramInfo->triPat2;
                }
            }
        }
        return true;
    }

    void TrigramAlphabet::MegaMatch(__in_ecount(inputLen) const wchar_t* input,int inputLen) {
        this->input=input;
        this->inputLen=inputLen;
        if (inputLen<TrigramInfo::PatternLength) {
            return;
        }
        // prime the pump
        unsigned char c1=alphaBits[input[0]&UpperCaseMask];
        unsigned char c2=alphaBits[input[1]&UpperCaseMask];
        unsigned char c3=alphaBits[input[2]&UpperCaseMask];
        // pump
        for (int k=3;k<inputLen-5;k++) {
            int index=(c1<<6)+(c2<<3)+c3;
            if (index<TrigramMapSize) {
                int t=trigramMap[index];
                if (t!=TrigramNotInPattern) {
                    int count=trigramStarts[t].count;
                    if (count>0) {
                        int inputMask=0;
                        bool validInput=true;
                        for (int j=0;j<5;j++) {
                            // ascii check
                            if (input[k+j]<128) {
                                int bits=alphaBits[input[k+j]&UpperCaseMask];
                                if (bits==BitsNotInAlpha) {
                                    validInput=false;
                                    break;
                                }
                                inputMask=(inputMask<<AlphaCount)+(1<<bits);
                            }
                            else {
                                validInput=false;
                                break;
                            }
                        }
                        if (validInput) {
                            for (int j=0;j<count;j++) {
                                PatternTri* tri= &(trigramStarts[t].patterns[j]);
                                if ((inputMask&(tri->encodedPattern))==inputMask) {
                                    if (tri->pattern->rep.unified.trigramInfo->resultCount<TrigramInfo::MaxResults) {
                                        tri->pattern->rep.unified.trigramInfo->offsets[tri->pattern->rep.unified.trigramInfo->resultCount++]=k-3;
                                    }
                                    else {
                                        tri->pattern->rep.unified.trigramInfo->isTrigramPattern=false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            c1=c2;
            c2=c3;
            c3=alphaBits[input[k]&UpperCaseMask];
        }
    }

    // ----------------------------------------------------------------------
    // OctoquadIdentifier
    // ----------------------------------------------------------------------

    bool OctoquadIdentifier::Qualifies(const Program *const program)
    {
        return (program->flags & (GlobalRegexFlag | IgnoreCaseRegexFlag)) == (GlobalRegexFlag | IgnoreCaseRegexFlag);
    }

    OctoquadIdentifier::OctoquadIdentifier(
        const int numCodes,
        char (&codeToChar)[TrigramAlphabet::AlphaCount],
        char (&charToCode)[TrigramAlphabet::AsciiTableSize])
        : numCodes(numCodes),
        codeToChar(codeToChar),
        charToCode(charToCode),
        currPatternLength(0),
        currPatternNum(-1)
    {
        // 'patternBits' will be initialized as necessary
    }

    int OctoquadIdentifier::GetOrAddCharCode(const Char c)
    {
        if (c >= static_cast<Char>('A') && c <= static_cast<Char>('Z'))
        {
            for (int i = 0; i < numCodes; i++)
            {
                if (codeToChar[i] == static_cast<char>(c))
                    return i;
            }
            if (numCodes == TrigramAlphabet::AlphaCount)
                return -1;
            codeToChar[numCodes] = static_cast<char>(c);
            charToCode[c] = static_cast<char>(numCodes);
            return numCodes++;
        }
        else
            return -1;
    }

    bool OctoquadIdentifier::BeginConcat()
    {
        if (currPatternNum >= 0 && currPatternLength != TrigramInfo::PatternLength)
            return false;
        if (currPatternNum >= NumPatterns)
            return false;
        currPatternNum++;
        currPatternLength = 0;
        return true;
    }

    bool OctoquadIdentifier::CouldAppend(const CharCount n) const
    {
        return n <= static_cast<CharCount>(TrigramInfo::PatternLength - currPatternLength);
    }

    bool OctoquadIdentifier::AppendChar(Char c)
    {
        if (currPatternLength >= TrigramInfo::PatternLength || currPatternNum < 0 || currPatternNum >= NumPatterns)
            return false;
        int code = GetOrAddCharCode(c);
        if (code < 0)
            return false;
        patternBits[currPatternNum][currPatternLength++] = 1 << code;
        return true;
    }

    bool OctoquadIdentifier::BeginUnions()
    {
        if(currPatternLength >= TrigramInfo::PatternLength || currPatternNum < 0 || currPatternNum >= NumPatterns)
            return false;
        patternBits[currPatternNum][currPatternLength] = 0;
        return true;
    }

    bool OctoquadIdentifier::UnionChar(Char c)
    {
        if (currPatternLength >= TrigramInfo::PatternLength || currPatternNum < 0 || currPatternNum >= NumPatterns)
            return false;
        int code = GetOrAddCharCode(c);
        if (code < 0)
            return false;
        patternBits[currPatternNum][currPatternLength] |= 1 << code;
        return true;
    }

    void OctoquadIdentifier::EndUnions()
    {
        Assert(currPatternLength < TrigramInfo::PatternLength);
        ++currPatternLength;
    }

    bool OctoquadIdentifier::IsOctoquad()
    {
        return
            numCodes == TrigramAlphabet::AlphaCount &&
            currPatternLength == TrigramInfo::PatternLength &&
            currPatternNum == NumPatterns - 1;
    }

    void OctoquadIdentifier::SetTrigramAlphabet(Js::ScriptContext * scriptContext,
        __in_xcount(regex::TrigramAlphabet::AlphaCount) char* alpha
        , __in_xcount(regex::TrigramAlphabet::AsciiTableSize) char* alphaBits)
    {
        ArenaAllocator* alloc = scriptContext->RegexAllocator();
        TrigramAlphabet * trigramAlphabet = AnewStruct(alloc, UnifiedRegex::TrigramAlphabet);
        for (uint i = 0; i < UnifiedRegex::TrigramAlphabet::AsciiTableSize; i++) {
            trigramAlphabet->alphaBits[i] = UnifiedRegex::TrigramAlphabet::BitsNotInAlpha;
        }
        for (uint i = 0; i < UnifiedRegex::TrigramAlphabet::AlphaCount; i++) {
            trigramAlphabet->alpha[i] = alpha[i];
            trigramAlphabet->alphaBits[alpha[i]] = alphaBits[alpha[i]];
        }
        trigramAlphabet->InitTrigramMap();
        scriptContext->SetTrigramAlphabet(trigramAlphabet);
    }

    void OctoquadIdentifier::InitializeTrigramInfo(Js::ScriptContext* scriptContext, RegexPattern* const pattern)
    {
        if(!scriptContext->GetTrigramAlphabet())
        {
            this->SetTrigramAlphabet(scriptContext, codeToChar, charToCode);
        }
        const auto recycler = scriptContext->GetRecycler();
        pattern->rep.unified.trigramInfo = RecyclerNew(recycler, TrigramInfo, patternBits[0], patternBits[1], recycler);
        pattern->rep.unified.trigramInfo->isTrigramPattern =
            scriptContext->GetTrigramAlphabet()->AddStarts(patternBits[0], patternBits[1], pattern);
    }

    // ----------------------------------------------------------------------
    // OctoquadMatcher
    // ----------------------------------------------------------------------

    OctoquadMatcher::OctoquadMatcher(const StandardChars<Char>* standardChars, CaseInsensitive::MappingSource mappingSource, OctoquadIdentifier* identifier)
    {
        for (int i = 0; i < TrigramAlphabet::AlphaCount; i++)
            codeToChar[i] = (Char)identifier->codeToChar[i];

        for (int i = 0; i < TrigramAlphabet::AsciiTableSize; i++)
            charToBits[i] = 0;

        for (int i = 0; i < TrigramAlphabet::AlphaCount; i++)
        {
            Char equivs[CaseInsensitive::EquivClassSize];
            standardChars->ToEquivs(mappingSource, codeToChar[i], equivs);
            for (int j = 0; j < CaseInsensitive::EquivClassSize; j++)
            {
                if (CTU(equivs[j]) < TrigramAlphabet::AsciiTableSize)
                    charToBits[CTU(equivs[j])] = 1 << i;
            }
        }

        for (int i = 0; i < OctoquadIdentifier::NumPatterns; i++)
        {
            patterns[i] = 0;
            for (int j = 0; j < TrigramInfo::PatternLength; j++)
            {
                patterns[i] <<= 4;
                patterns[i] |= (uint32)identifier->patternBits[i][j];
            }
        }
    }

    OctoquadMatcher *OctoquadMatcher::New(
        Recycler* recycler,
        const StandardChars<Char>* standardChars,
        CaseInsensitive::MappingSource mappingSource,
        OctoquadIdentifier* identifier)
    {
        return RecyclerNewLeaf(recycler, OctoquadMatcher, standardChars, mappingSource, identifier);
    }

    // It exploits the fact that each quad of bits has at most only one bit set.
    __inline bool oneBitSetInEveryQuad(uint32 x)
    {
        x -= 0x11111111;
        return (x & 0x88888888u) == 0;
    }

    bool OctoquadMatcher::Match
        ( const Char* const input
        , const CharCount inputLength
        , CharCount& offset
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        )
    {
        Assert(TrigramInfo::PatternLength == 8);
        Assert(OctoquadIdentifier::NumPatterns == 2);

        if (inputLength < TrigramInfo::PatternLength)
            return false;
        if (offset > inputLength - TrigramInfo::PatternLength)
            return false;

        uint32 v = 0;
        for (int i = 0; i < TrigramInfo::PatternLength; i++)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (stats != 0)
                stats->numCompares++;
#endif
            v <<= 4;
            if (CTU(input[offset + i]) < TrigramAlphabet::AsciiTableSize)
                v |= charToBits[CTU(input[offset + i])];
        }

        const uint32 lp = patterns[0];
        const uint32 rp = patterns[1];
        CharCount next = offset + TrigramInfo::PatternLength;

        while (true)
        {
            if (oneBitSetInEveryQuad(v & lp) || oneBitSetInEveryQuad(v & rp))
            {
                offset = next - TrigramInfo::PatternLength;
                return true;
            }
            if (next >= inputLength)
                return false;
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (stats != 0)
                stats->numCompares++;
#endif
            v <<= 4;
            if (CTU(input[next]) < TrigramAlphabet::AsciiTableSize)
                v |= charToBits[CTU(input[next])];
            next++;
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void OctoquadMatcher::Print(DebugWriter* w) const
    {
        for (int i = 0; i < OctoquadIdentifier::NumPatterns; i++)
        {
            if (i > 0)
                w->Print(L"|");
            for (int j = 0; j < TrigramInfo::PatternLength; j++)
            {
                uint8 v = (patterns[i] >> ((TrigramInfo::PatternLength - j - 1) * TrigramAlphabet::AlphaCount)) & 0xf;
                int n = 0;
                uint8 x = v;
                while (x > 0)
                {
                    x &= x-1;
                    n++;
                }
                if (n != 1)
                    w->Print(L"[");
                for (int k = 0; k < TrigramAlphabet::AlphaCount; k++)
                {
                    if ((v & 1) == 1)
                        w->PrintEscapedChar(codeToChar[k]);
                    v >>= 1;
                }
                if (n != 1)
                    w->Print(L"]");
            }
        }
    }
#endif
}





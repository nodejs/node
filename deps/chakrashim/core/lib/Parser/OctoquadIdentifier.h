//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// Matchers for pattern of form:
//    pattern ::= atom{8} '|' atom{8}
//    atom ::= A | [...charset drawn from A's...]
// where:
//   - A is a set of exactly four ASCII characters
//   - The pattern ignores case
//   - The pattern includes the global flag
// An example pattern would be "ABCdABCd|aDcAbBcD".
#pragma once

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // Trigrams
    // ----------------------------------------------------------------------

    struct TrigramInfo {
        static const int PatternLength=8;
        static const int MaxResults=32;
        bool isTrigramPattern;
        bool hasCachedResultString;
        int triPat1;
        int triPat2;
        int resultCount;
        int offsets[MaxResults];
        Js::JavascriptString * cachedResult[MaxResults];

        TrigramInfo(__in_ecount(PatternLength) char* pat1,__in_ecount(PatternLength) char* pat2, Recycler* recycler);
    };

    struct PatternTri {
        RegexPattern* pattern;
        int encodedPattern;
    };

    struct TrigramStart {
        static const int MaxPatPerStart=12;
        int count;
        PatternTri patterns[MaxPatPerStart];
    };

    struct TrigramAlphabet {
        static const int AlphaCount=4;
        static const int AsciiTableSize=128;
        static const int BitsNotInAlpha=4;
        static const int TrigramMapSize=221;
        static const int TrigramNotInPattern=65;
        static const char LowerCaseBit=0x20;
        static const char UpperCaseMask=0x5f;
        static const int TrigramCount=64;
        static const int MaxCachedStarts=48;

        TrigramStart trigramStarts[TrigramCount];

        char alpha[AlphaCount];
        char alphaBits[AsciiTableSize];
        char trigramMap[TrigramMapSize];
        const wchar_t* input;
        int inputLen;

        void InitTrigramMap();
        bool AddStarts(__in_xcount(TrigramInfo::PatternLength) char* pat1,__in_xcount(TrigramInfo::PatternLength) char* pat2, RegexPattern* pattern);
        void MegaMatch(__in_ecount(inputLen) const wchar_t* input,int inputLen);
    };

    // ----------------------------------------------------------------------
    // OctoquadIdentifier
    // ----------------------------------------------------------------------

    class OctoquadIdentifier : private Chars<wchar_t>
    {
        friend class OctoquadMatcher;
    public:
        static const int NumPatterns = 2;

    private:
        // Number of characters in the alphabet encountered so far
        int numCodes;

        // Maps a character code to the character
        char (&codeToChar)[TrigramAlphabet::AlphaCount];

        // Maps a character to its code 0-3. This array is passed into the constructor and only indexes for characters in the
        // alphabet are updated.
        char (&charToCode)[TrigramAlphabet::AsciiTableSize];


        // For each octoquad pattern, each byte contains a 4-bit pattern. One character will be represented as 0x1, 0x2, 0x4, or
        // 0x8 since it's a quad alphabet. A character class in the pattern can cause the bit pattern to be a combination of the
        // character bits.
        char patternBits[NumPatterns][TrigramInfo::PatternLength];

        int currPatternLength;
        int currPatternNum;

        void SetTrigramAlphabet(Js::ScriptContext * scriptContext,
            __in_xcount(regex::TrigramAlphabet::AlphaCount) char* alpha,
            __in_xcount(regex::TrigramAlphabet::AsciiTableSize) char* alphaBits);
    public:
        static bool Qualifies(const Program *const program);

        OctoquadIdentifier(
            const int numCodes,
            char (&codeToChar)[TrigramAlphabet::AlphaCount],
            char (&charToCode)[TrigramAlphabet::AsciiTableSize]);

        // Returns -1 if character not in quad alphabet and the alphabet is full
        int GetOrAddCharCode(const Char c);

        bool BeginConcat();
        bool CouldAppend(const CharCount n) const;
        bool AppendChar(Char c);
        bool BeginUnions();
        bool UnionChar(Char c);
        void EndUnions();
        bool IsOctoquad();

        void InitializeTrigramInfo(Js::ScriptContext* scriptContext, RegexPattern* const pattern);

    };

    // ----------------------------------------------------------------------
    // OctoquadMatcher
    // ----------------------------------------------------------------------

    class OctoquadMatcher : private Chars<wchar_t>
    {
    private:
        OctoquadMatcher(const StandardChars<Char>* standardChars, CaseInsensitive::MappingSource mappingSource, OctoquadIdentifier* identifier);

        Char codeToChar[TrigramAlphabet::AlphaCount];

        // Maps characters (0..AsciTableSize-1) to 0 if not in alphabet, or 0x1, 0x2, 0x4 or 0x8.
        // Allocated and filled only if invoke Match below.
        uint8 charToBits[TrigramAlphabet::AsciiTableSize];

        uint32 patterns[OctoquadIdentifier::NumPatterns];

    public:
        static OctoquadMatcher *New(Recycler* recycler, const StandardChars<Char>* standardChars, CaseInsensitive::MappingSource mappingSource, OctoquadIdentifier* identifier);

        bool Match
            ( const Char* const input
            , const CharCount inputLength
            , CharCount& offset
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
#endif
            );

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif

    };
}

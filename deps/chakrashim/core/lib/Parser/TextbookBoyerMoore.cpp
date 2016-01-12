//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    template <typename C>
    void TextbookBoyerMooreSetup<C>::Init()
    {
        Assert(patLen > 0);
        for (uint i = 0; i < MaxCharMapLinearChars; i++)
        {
            lastOcc[i] = -1;
        }

        numLinearChars = 1;

        // Always put the last character in the first index
        linearChar[0] = pat[patLen - 1];
        for (CharCount i = 0; i < patLen; i++)
        {
            if (numLinearChars <= MaxCharMapLinearChars)
            {
                uint j = 0;
                for (; j < numLinearChars; j++)
                {
                    if (linearChar[j] == pat[i])
                    {
                        lastOcc[j] = i;
                        break;
                    }
                }
                if (j == numLinearChars)
                {
                    if (numLinearChars < MaxCharMapLinearChars)
                    {
                        linearChar[numLinearChars] = pat[i];
                        lastOcc[numLinearChars] = i;
                    }
                    numLinearChars++;
                }
            }

            if (numLinearChars > MaxCharMapLinearChars)
            {
                break;
            }
        }
        if (numLinearChars <= MaxCharMapLinearChars)
        {
            scheme = LinearScheme;
        }
        else
        {
            scheme = DefaultScheme;
        }
    }

    template <typename C>
    void TextbookBoyerMoore<C>::Setup(ArenaAllocator* allocator, TextbookBoyerMooreSetup<C> const& info)
    {
        Assert(info.GetScheme() == TextbookBoyerMooreSetup<C>::DefaultScheme);
        this->Setup(allocator, info.pat, info.patLen, 1);
    }

    template <typename C>
    void TextbookBoyerMoore<C>::Setup(ArenaAllocator * allocator, const Char * pat, CharCount patLen, int skip)
    {
        // character c |-> index of last occurrence of c in pat, otherwise -1
        for (CharCount i = 0; i < patLen; i++)
        {
            for (int j = 0; j < skip; j++)
                lastOccurrence.Set(allocator, pat[i * skip + j], i);
        }
        goodSuffix = TextbookBoyerMooreSetup<C>::GetGoodSuffix(allocator, pat, patLen, skip);
    }

    template <typename C>
    int32 * TextbookBoyerMooreSetup<C>::GetGoodSuffix(ArenaAllocator* allocator, const Char * pat, CharCount patLen, int skip)
    {
        // pat offset q |-> longest prefix of pat which is a proper suffix of pat[0..q]
        // (thanks to equivalence classes being in canonical order we only need to look at the first
        //  character of each skip grouping in the pattern)
        int32* prefix = AnewArray(allocator, int32, patLen);
        prefix[0] = 0;
        int32 k = 0;
        for (CharCount q = 1; q < patLen; q++)
        {
            while (k > 0 && pat[k * skip] != pat[q * skip])
                k = prefix[k - 1];
            if (pat[k * skip] == pat[q * skip])
                k++;
            prefix[q] = k;
        }

        // As above, but for rev(pat)
        int32* revPrefix = AnewArray(allocator, int32, patLen);
        revPrefix[0] = 0;
        k = 0;
        for (CharCount q = 1; q < patLen; q++)
        {
            while (k > 0 && pat[(patLen - k - 1) * skip] != pat[(patLen - q - 1) * skip])
                k = revPrefix[k - 1];
            if (pat[(patLen - k - 1) * skip] == pat[(patLen - q - 1) * skip])
                k++;
            revPrefix[q] = k;
        }

        // pat prefix length l |-> least shift s.t. pat[0..l-1] is not mismatched
        int32 * goodSuffix = AnewArray(allocator, int32, patLen + 1);
        for (CharCount j = 0; j <= patLen; j++)
            goodSuffix[j] = patLen - prefix[patLen - 1];
        for (CharCount l = 1; l <= patLen; l++)
        {
            CharCount j = patLen - revPrefix[l - 1];
            int32 s = l - revPrefix[l - 1];
            if (goodSuffix[j] > s)
                goodSuffix[j] = s;
        }
        // shift above one to the left
        for (CharCount j = 0; j < patLen; j++)
            goodSuffix[j] = goodSuffix[j + 1];

        AdeleteArray(allocator, patLen, prefix);
        AdeleteArray(allocator, patLen, revPrefix);

        return goodSuffix;
    }

    template <typename C>
    void TextbookBoyerMoore<C>::FreeBody(ArenaAllocator* allocator, CharCount patLen)
    {
        if(goodSuffix)
        {
            AdeleteArray(allocator, patLen + 1, goodSuffix);
#if DBG
            goodSuffix = 0;
#endif
        }
        lastOccurrence.FreeBody(allocator);
    }

    template <uint equivClassSize, uint compareCount>
    static bool MatchPatternAt(uint inputChar, wchar_t const * pat, CharCount index);

    template <>
    static bool MatchPatternAt<1, 1>(uint inputChar, wchar_t  const* pat, CharCount index)
    {
        return inputChar == pat[index];
    }

    template <>
    static bool MatchPatternAt<CaseInsensitive::EquivClassSize, CaseInsensitive::EquivClassSize>(uint inputChar, wchar_t const * pat, CharCount index)
    {
        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        return inputChar == pat[index * CaseInsensitive::EquivClassSize]
            || inputChar == pat[index * CaseInsensitive::EquivClassSize + 1]
            || inputChar == pat[index * CaseInsensitive::EquivClassSize + 2]
            || inputChar == pat[index * CaseInsensitive::EquivClassSize + 3];
    }

    template <>
    static bool MatchPatternAt<CaseInsensitive::EquivClassSize, 1>(uint inputChar, wchar_t const * pat, CharCount index)
    {
        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        return inputChar == pat[index * 4];
    }

    template <typename C>
    template <uint equivClassSize, uint lastPatCharEquivClass>
    bool TextbookBoyerMoore<C>::Match
        ( const Char *const input
        , const CharCount inputLength
        , CharCount& inputOffset
        , const Char* pat
        , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        ) const
    {

        Assert(input != 0);
        Assert(inputOffset <= inputLength);

        if (inputLength < patLen)
            return false;

        CharCount offset = inputOffset;

        const CharCount endOffset = inputLength - (patLen - 1);
        const int32* const localGoodSuffix = goodSuffix;
        const LastOccMap* const localLastOccurrence = &lastOccurrence;

        const CharCount lastPatCharIndex = (patLen - 1);

        while (offset < endOffset)
        {
            // A separate tight loop to find the last character
            while (true)
            {
                uint inputChar = Chars<Char>::CTU(input[offset + lastPatCharIndex]);
                if (MatchPatternAt<equivClassSize, lastPatCharEquivClass>(inputChar, pat, lastPatCharIndex))
                {
                    // Found a match. Break out of this loop and go to the match pattern loop
                    break;
                }
                // Negative case is more common,
                // Write the checks so that we have a super tight loop
                int lastOcc;
                if (inputChar < localLastOccurrence->GetDirectMapSize())
                {
                    if (!localLastOccurrence->IsInDirectMap(inputChar))
                    {
                        offset += patLen;
                        if (offset >= endOffset)
                        {
                            return false;
                        }
                        continue;
                    }
                    lastOcc = localLastOccurrence->GetDirectMap(inputChar);
                }
                else if (!localLastOccurrence->GetNonDirect(inputChar, lastOcc))
                {
                    offset += patLen;
                    if (offset >= endOffset)
                    {
                        return false;
                    }
                    continue;
                }
                Assert((int)lastPatCharIndex - lastOcc >= localGoodSuffix[lastPatCharIndex]);
                offset += lastPatCharIndex - lastOcc;
                if (offset >= endOffset)
                {
                    return false;
                }
            }

            // CONSIDER: we can remove this check if we stop using TextbookBoyerMoore for one char pattern
            if (lastPatCharIndex == 0)
            {
                inputOffset = offset;
                return true;
            }

            // Match the rest of the pattern
            int32 j = lastPatCharIndex - 1;
            while (true)
            {
#if ENABLE_REGEX_CONFIG_OPTIONS
                if (stats != 0)
                    stats->numCompares++;
#endif
                uint inputChar = Chars<Char>::CTU(input[offset + j]);
                if (!MatchPatternAt<equivClassSize, equivClassSize>(inputChar, pat, j))
                {
                    const int32 e = j - localLastOccurrence->Get((Char)inputChar);
                    offset += e > localGoodSuffix[j] ? e : localGoodSuffix[j];
                    break;
                }
                if (--j < 0)
                {
                    inputOffset = offset;
                    return true;
                }
            }
        }
        return false;
    }

    // Specialized linear char map version
    template <typename C>
    void TextbookBoyerMooreWithLinearMap<C>::Setup(ArenaAllocator * allocator, TextbookBoyerMooreSetup<C> const& setup)
    {
        Assert(setup.GetScheme() == TextbookBoyerMooreSetup<C>::LinearScheme);
        lastOccurrence.Set(setup.numLinearChars, setup.linearChar, setup.lastOcc);
        goodSuffix = TextbookBoyerMooreSetup<C>::GetGoodSuffix(allocator, setup.pat, setup.patLen);
    }

    template <typename C>
    void TextbookBoyerMooreWithLinearMap<C>::FreeBody(ArenaAllocator* allocator, CharCount patLen)
    {
        if(goodSuffix)
        {
            AdeleteArray(allocator, patLen + 1, goodSuffix);
#if DBG
            goodSuffix = 0;
#endif
        }
    }

    template <typename C>
    template <uint equivClassSize>
    bool TextbookBoyerMooreWithLinearMap<C>::Match
        ( const Char *const input
        , const CharCount inputLength
        , CharCount& inputOffset
        , const Char* pat
        , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        ) const
    {
        CompileAssert(equivClassSize == 1);
        Assert(input != 0);
        Assert(inputOffset <= inputLength);

        if (inputLength < patLen)
            return false;

        const int32* const localGoodSuffix = goodSuffix;
        const LastOccMap* const localLastOccurrence = &lastOccurrence;

        CharCount offset = inputOffset;

        const CharCount lastPatCharIndex = (patLen - 1);
        const CharCount endOffset = inputLength - lastPatCharIndex;

        // Using int size instead of Char value is faster
        const uint lastPatChar = pat[lastPatCharIndex];
        Assert(lastPatChar == localLastOccurrence->GetChar(0));

        while (offset < endOffset)
        {
            // A separate tight loop to find the last character
            while (true)
            {
#if ENABLE_REGEX_CONFIG_OPTIONS
                if (stats != 0)
                    stats->numCompares++;
#endif
                uint inputChar = Chars<Char>::CTU(input[offset + lastPatCharIndex]);
                if (inputChar == lastPatChar)
                {
                    // Found a match. Break out of this loop and go to the match pattern loop
                    break;
                }
                // Negative case is more common,
                // Write the checks so that we have a super tight loop
                Assert(inputChar != localLastOccurrence->GetChar(0));
                int32 lastOcc;
                if (localLastOccurrence->GetChar(1) != inputChar)
                {
                    if (localLastOccurrence->GetChar(2) != inputChar)
                    {
                        if (localLastOccurrence->GetChar(3) != inputChar)
                        {
                            offset += patLen;
                            if (offset >= endOffset)
                            {
                                return false;
                            }
                            continue;
                        }
                        lastOcc = localLastOccurrence->GetLastOcc(3);
                    }
                    else
                    {
                        lastOcc = localLastOccurrence->GetLastOcc(2);
                    }
                }
                else
                {
                    lastOcc = localLastOccurrence->GetLastOcc(1);
                }
                Assert((int)lastPatCharIndex - lastOcc >= localGoodSuffix[lastPatCharIndex]);
                offset += lastPatCharIndex - lastOcc;
                if (offset >= endOffset)
                {
                    return false;
                }
            }

            // CONSIDER: we can remove this check if we stop using
            // TextbookBoyerMoore for one char pattern
            if (lastPatCharIndex == 0)
            {
                inputOffset = offset;
                return true;
            }

            // Match the rest of the pattern
            int32 j = lastPatCharIndex - 1;
            while (true)
            {
#if ENABLE_REGEX_CONFIG_OPTIONS
                if (stats != 0)
                    stats->numCompares++;
#endif
                uint inputChar = Chars<Char>::CTU(input[offset + j]);
                if (inputChar != pat[j])
                {
                    int goodSuffix = localGoodSuffix[j];
                    Assert(patLen <= MaxCharCount);
                    if (goodSuffix == (int)patLen)
                    {
                        offset += patLen;
                    }
                    else
                    {
                        const int32 e = j - localLastOccurrence->Get(inputChar);
                        offset += e > goodSuffix ? e : goodSuffix;
                    }
                    break;
                }
                if (--j < 0)
                {
                    inputOffset = offset;
                    return true;
                }
            }
        }
        return false;
    }

    // explicit instantiation
    template struct TextbookBoyerMooreSetup<wchar_t>;
    template class TextbookBoyerMoore<wchar_t>;
    template class TextbookBoyerMooreWithLinearMap<wchar_t>;

    template
    bool TextbookBoyerMoore<wchar_t>::Match<1>
        ( const Char *const input
        , const CharCount inputLength
        , CharCount& inputOffset
        , const Char* pat
        , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        ) const;

    template
    bool TextbookBoyerMoore<wchar_t>::Match<CaseInsensitive::EquivClassSize>
        ( const Char *const input
        , const CharCount inputLength
        , CharCount& inputOffset
        , const Char* pat
        , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        ) const;

    template
    bool TextbookBoyerMoore<wchar_t>::Match<CaseInsensitive::EquivClassSize, 1>
        ( const Char *const input
        , const CharCount inputLength
        , CharCount& inputOffset
        , const Char* pat
        , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        ) const;

    template
    bool TextbookBoyerMooreWithLinearMap<wchar_t>::Match<1>
        ( const Char *const input
        , const CharCount inputLength
        , CharCount& inputOffset
        , const Char* pat
        , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        ) const;
}

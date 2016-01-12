//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// From Cormen, Leiserson and Rivest, ch 34.

#pragma once

namespace UnifiedRegex
{
    template <typename C>
    class TextbookBoyerMooreWithLinearMap : private Chars<C>
    {
        template <typename C>
        friend struct TextbookBoyerMooreSetup;
    private:
        typedef CharMap<Char, int32, CharMapScheme_Linear> LastOccMap;

        // NOTE: We don't store the actual pattern here since it may be moved between
        //       constructing the scanner and running it.

        LastOccMap lastOccurrence;
        int32 *goodSuffix;

    public:

        inline TextbookBoyerMooreWithLinearMap() : lastOccurrence(-1), goodSuffix(0) {}

        // Construct Boyer-Moore tables for pattern pat:
        //  - pat must be of length patLen * skip
        //  - if skip is > 1, then each consecutive skip characters of pattern are assumed to represent
        //    an equivalence class of characters in canonical order (i.e. all chars in a class are represented
        //    by the same sequence of skip chars)
        //  - otherwise this is a regular exact-match pattern
        void Setup(ArenaAllocator* allocator, TextbookBoyerMooreSetup<C> const& setup);

        void FreeBody(ArenaAllocator* allocator, CharCount patLen);

        // NOTE: In the following pat and patLen must be the same as passed to Setup above

        // For skip = 1
        template <uint equivClassSize>
        bool Match
            ( const Char *const input
            , const CharCount inputLength
            , CharCount& inputOffset
            , const Char* pat
            , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
#endif
            ) const;

#if ENABLE_REGEX_CONFIG_OPTIONS
        static wchar_t const * GetName() { return L"linear map Boyer-Moore"; }
#endif
     };

    template <typename C>
    class TextbookBoyerMoore : private Chars<C>
    {
        template <typename C>
        friend struct TextbookBoyerMooreSetup;
    private:
        typedef CharMap<Char, int32> LastOccMap;

        // NOTE: We don't store the actual pattern here since it may be moved between
        //       constructing the scanner and running it.

        LastOccMap lastOccurrence;
        int32 *goodSuffix;

    public:

        inline TextbookBoyerMoore() : lastOccurrence(-1), goodSuffix(0) {}

        // Construct Boyer-Moore tables for pattern pat:
        //  - pat must be of length patLen * skip
        //  - if skip is > 1, then each consecutive skip characters of pattern are assumed to represent
        //    an equivalence class of characters in canonical order (i.e. all chars in a class are represented
        //    by the same sequence of skip chars)
        //  - otherwise this is a regular exact-match pattern
        void Setup(ArenaAllocator* allocator, TextbookBoyerMooreSetup<C> const& setup);
        void Setup(ArenaAllocator * allocator, const Char * pat, CharCount patLen, int skip);

        void FreeBody(ArenaAllocator* allocator, CharCount patLen);

        // NOTE: In the following pat and patLen must be the same as passed to Setup above
        template <uint equivClassSize>
        __inline bool Match
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
            return Match<equivClassSize, equivClassSize>(input, inputLength, inputOffset, pat, patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
                , stats
#endif
                );
        }

        template <uint equivClassSize, uint lastPatCharEquivClass>
        bool Match
            ( const Char *const input
            , const CharCount inputLength
            , CharCount& inputOffset
            , const Char* pat
            , const CharCount patLen
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
#endif
            ) const;

#if ENABLE_REGEX_CONFIG_OPTIONS
        static wchar_t const * GetName() { return L"full map Boyer-Moore"; }
#endif
    };

    template <typename C>
    struct TextbookBoyerMooreSetup : private Chars<C>
    {
        friend class TextbookBoyerMoore<C>;
        friend class TextbookBoyerMooreWithLinearMap<C>;

        enum Scheme
        {
            DefaultScheme,
            LinearScheme
        };

        TextbookBoyerMooreSetup(Char const * pat, CharCount patLen) : pat(pat), patLen(patLen) { Init(); }

        Scheme GetScheme() const { return scheme; }

        static int32 * GetGoodSuffix(ArenaAllocator* allocator, const Char * pat, CharCount patLen, int skip = 1);
    private:
        void Init();


        Scheme scheme;
        Char const * const pat;
        CharCount const patLen;
        uint numLinearChars;
        Char linearChar[MaxCharMapLinearChars];
        int32 lastOcc[MaxCharMapLinearChars];
    };
}

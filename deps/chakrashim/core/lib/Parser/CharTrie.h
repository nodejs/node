//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // CharTrie
    // ----------------------------------------------------------------------

    // FORWARD
    struct CharTrieEntry;

    class CharTrie : private Chars<wchar_t>
    {
        friend class RuntimeCharTrie;

        static const int initCapacity = 4;

        CharTrieEntry* children;
        bool isAccepting;
        int capacity;
        int count;
        // Array of capacity entries, first count are used, in increasing character order

        __inline bool Find(Char c, int& outi);

    public:
        inline CharTrie() : isAccepting(false), capacity(0), count(0), children(0) {}
        inline void Reset() { isAccepting = false; capacity = 0; count = 0; children = 0; }
        void FreeBody(ArenaAllocator* allocator);
        inline int Count() const { return count; }
        inline bool IsAccepting() const { return isAccepting; }
        inline void SetAccepting() { isAccepting = true; }
        CharTrie* Add(ArenaAllocator* allocator, Char c);
        bool IsDepthZero() const;
        bool IsDepthOne() const;
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    struct CharTrieEntry : private Chars<wchar_t>
    {
        Char c;
        CharTrie node;
    };


    // ----------------------------------------------------------------------
    // RuntimeCharTrie
    // ----------------------------------------------------------------------

    // FORWARD
    struct RuntimeCharTrieEntry;

    class RuntimeCharTrie : private Chars<wchar_t>
    {
        int count;
        // Array of count entries, in increasing character order
        RuntimeCharTrieEntry* children;

    public:
        inline RuntimeCharTrie() : count(0), children(0) {}
        void FreeBody(ArenaAllocator* allocator);
        void CloneFrom(ArenaAllocator* allocator, const CharTrie& other);

        bool Match
            ( const Char* const input
            , const CharCount inputLength
            , CharCount &inputOffset
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
#endif
            ) const;
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    struct RuntimeCharTrieEntry : private Chars<wchar_t>
    {
        Char c;
        RuntimeCharTrie node;
    };


}

//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // CharTrie
    // ----------------------------------------------------------------------
    __inline bool CharTrie::Find(Char c, int& outi)
    {
        if (count == 0)
        {
            outi = 0;
            return false;
        }
        int l = 0;
        int h = count - 1;
        while (true)
        {
            int m = (l + h) / 2;
            if (children[m].c == c)
            {
                outi = m;
                return true;
            }
            else if (CTU(children[m].c) < CTU(c))
            {
                l = m + 1;
                if (l > h)
                {
                    outi = l;
                    return false;
                }
            }
            else
            {
                h = m - 1;
                if (h < l)
                {
                    outi = m;
                    return false;
                }
            }
        }
        return false;
    }

    void CharTrie::FreeBody(ArenaAllocator* allocator)
    {
        for (int i = 0; i < count; i++)
            children[i].node.FreeBody(allocator);
        if (capacity > 0)
            AdeleteArray(allocator, capacity, children);
#if DBG
        count = 0;
        capacity = 0;
        children = 0;
#endif
    }

    CharTrie* CharTrie::Add(ArenaAllocator* allocator, Char c)
    {
        int i;
        if (!Find(c, i))
        {
            if (capacity <= count)
            {
                int newCapacity = max(capacity * 2, initCapacity);
                children = (CharTrieEntry*)allocator->Realloc(children, capacity * sizeof(CharTrieEntry), newCapacity * sizeof(CharTrieEntry));
                capacity = newCapacity;
            }

            for (int j = count; j > i; j--)
            {
                children[j].c = children[j - 1].c;
                children[j].node = children[j - 1].node;
            }
            children[i].c = c;
            children[i].node.Reset();
            count++;
        }
        return &children[i].node;
    }

    bool CharTrie::IsDepthZero() const
    {
        return isAccepting && count == 0;
    }

    bool CharTrie::IsDepthOne() const
    {
        if (isAccepting)
            return 0;
        for (int i = 0; i < count; i++)
        {
            if (!children[i].node.IsDepthZero())
                return false;
        }
        return true;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void CharTrie::Print(DebugWriter* w) const
    {
        w->Indent();
        if (isAccepting)
            w->PrintEOL(L"<accept>");
        for (int i = 0; i < count; i++)
        {
            w->PrintQuotedChar(children[i].c);
            w->EOL();
            children[i].node.Print(w);
        }
        w->Unindent();
    }
#endif

    // ----------------------------------------------------------------------
    // RuntimeCharTrie
    // ----------------------------------------------------------------------
    bool RuntimeCharTrie::Match
        (const Char* const input
            , const CharCount inputLength
            , CharCount& inputOffset
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
#endif
            ) const
    {
        const RuntimeCharTrie* curr = this;
        while (true)
        {
            if (curr->count == 0)
                return true;
            if (inputOffset >= inputLength)
                return false;
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (stats != 0)
                stats->numCompares++;
#endif

#if 0
            int l = 0;
            int h = curr->count - 1;
            while (true)
            {
                if (l > h)
                    return false;
                int m = (l + h) / 2;
                if (curr->children[m].c == input[inputOffset])
                {
                    inputOffset++;
                    curr = &curr->children[m].node;
                    break;
                }
                else if (CTU(curr->children[m].c) < CTU(input[inputOffset]))
                    l = m + 1;
                else
                    h = m - 1;
            }
#else
            int i = 0;
            while (true)
            {
                if (curr->children[i].c == input[inputOffset])
                {
                    inputOffset++;
                    curr = &curr->children[i].node;
                    break;
                }
                else if (curr->children[i].c > input[inputOffset])
                    return false;
                else if (++i >= curr->count)
                    return false;
            }
#endif
        }
    }

    void RuntimeCharTrie::FreeBody(ArenaAllocator* allocator)
    {
        for (int i = 0; i < count; i++)
            children[i].node.FreeBody(allocator);
        if (count > 0)
            AdeleteArray(allocator, count, children);
#if DBG
        count = 0;
        children = 0;
#endif
    }

    void RuntimeCharTrie::CloneFrom(ArenaAllocator* allocator, const CharTrie& other)
    {
        count = other.count;
        if (count > 0)
        {
            children = AnewArray(allocator, RuntimeCharTrieEntry, count);
            for (int i = 0; i < count; i++)
            {
                children[i].c = other.children[i].c;
                children[i].node.CloneFrom(allocator,  other.children[i].node);
            }
        }
        else
            children = 0;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void RuntimeCharTrie::Print(DebugWriter* w) const
    {
        w->Indent();
        for (int i = 0; i < count; i++)
        {
            w->PrintQuotedChar(children[i].c);
            w->EOL();
            children[i].node.Print(w);
        }
        w->Unindent();
    }
#endif

}

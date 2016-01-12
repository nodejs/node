//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // CharBitVec
    // ----------------------------------------------------------------------

    inline uint32 popcnt(uint32 x)
    {
        // sum set bits in every bit pair
        x -= (x >> 1) & 0x55555555u;
        // sum pairs into quads
        x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
        // sum quads into octets
        x = (x + (x >> 4)) & 0x0f0f0f0fu;
        // sum octets into topmost octet
        x *= 0x01010101u;
        return x >> 24;
    }

    uint CharBitvec::Count() const
    {
        uint n = 0;
        for (int w = 0; w < vecSize; w++)
        {
            n += popcnt(vec[w]);
        }
        return n;
    }

    int CharBitvec::NextSet(int k) const
    {
        if (k < 0 || k >= Size)
            return -1;
        uint w = k / wordSize;
        uint o = k % wordSize;
        uint32 v = vec[w] >> o;
        do
        {
            if (v == 0)
            {
                k += wordSize - o;
                break;
            }
            else if ((v & 0x1) != 0)
                return k;
            else
            {
                v >>= 1;
                o++;
                k++;
            }
        }
        while (o < wordSize);

        w++;
        while (w < vecSize)
        {
            o = 0;
            v = vec[w];
            do
            {
                if (v == 0)
                {
                    k += wordSize - o;
                    break;
                }
                else if ((v & 0x1) != 0)
                    return k;
                else
                {
                    v >>= 1;
                    o++;
                    k++;
                }

            }
            while (o < wordSize);
            w++;
        }
        return -1;
    }

    int CharBitvec::NextClear(int k) const
    {
        if (k < 0 || k >= Size)
            return -1;
        uint w = k / wordSize;
        uint o = k % wordSize;
        uint32 v = vec[w] >> o;
        do
        {
            if (v == ones)
            {
                k += wordSize - o;
                break;
            }
            else if ((v & 0x1) == 0)
                return k;
            else
            {
                v >>= 1;
                o++;
                k++;
            }
        }
        while (o < wordSize);

        w++;
        while (w < vecSize)
        {
            o = 0;
            v = vec[w];
            do
            {
                if (v == ones)
                {
                    k += wordSize - o;
                    break;
                }
                else if ((v & 0x1) == 0)
                    return k;
                else
                {
                    v >>= 1;
                    o++;
                    k++;
                }

            }
            while (o < wordSize);
            w++;
        }
        return -1;
    }

    template <typename C>
    void CharBitvec::ToComplement(ArenaAllocator* allocator, uint base, CharSet<C>& result) const
    {
        int hi = -1;
        while (true)
        {
            // Find the next range of clear bits in vector
            int li = NextClear(hi + 1);
            if (li < 0)
                return;
            hi = NextSet(li + 1);
            if (hi < 0)
                hi = Size - 1;
            else
            {
                Assert(hi > 0);
                hi--;
            }

            // Add range as characters
            result.SetRange(allocator, Chars<C>::ITC(base + li), Chars<C>::ITC(base + hi));
        }
    }

    template <typename C>
    void CharBitvec::ToEquivClass(ArenaAllocator* allocator, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset) const
    {
        int hi = -1;
        while (true)
        {
            // Find the next range of set bits in vector
            int li = NextSet(hi + 1);
            if (li < 0)
                return;
            hi = NextClear(li + 1);
            if (hi < 0)
                hi = Size - 1;
            else
            {
                Assert(hi > 0);
                hi--;
            }

            // Convert to character codes
            uint l = base + li + baseOffset;
            uint h = base + hi + baseOffset;

            do
            {
                uint acth;
                C equivl[CaseInsensitive::EquivClassSize];
                CaseInsensitive::RangeToEquivClass(tblidx, l, h, acth, equivl);
                uint n = acth - l;
                for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                {
                    result.SetRange(allocator, equivl[i], Chars<C>::Shift(equivl[i], n));
                }

                // Go around again for rest of this range
                l = acth + 1;
            }
            while (l <= h);
        }
    }

    // ----------------------------------------------------------------------
    // CharSetNode
    // ----------------------------------------------------------------------

    inline CharSetNode* CharSetNode::For(ArenaAllocator* allocator, int level)
    {
        if (level == 0)
            return Anew(allocator, CharSetLeaf);
        else
            return Anew(allocator, CharSetInner);
    }

    // ----------------------------------------------------------------------
    // CharSetFull
    // ----------------------------------------------------------------------

    CharSetFull CharSetFull::Instance;

    CharSetFull* const CharSetFull::TheFullNode = &CharSetFull::Instance;

    CharSetFull::CharSetFull() {}

    void CharSetFull::FreeSelf(ArenaAllocator* allocator)
    {
        Assert(this == TheFullNode);
        // Never allocated
    }

    CharSetNode* CharSetFull::Clone(ArenaAllocator* allocator) const
    {
        // Always shared
        return (CharSetNode*)this;
    }

    CharSetNode* CharSetFull::Set(ArenaAllocator* allocator, uint level, uint l, uint h)
    {
        return this;
    }

    CharSetNode* CharSetFull::ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h)
    {
        AssertMsg(h <= lim(level), "The range for clearing provided is invalid for this level.");
        AssertMsg(l <= h, "Can't clear where lover is bigger than the higher.");
        if (l == 0 && h == lim(level))
        {
            return nullptr;
        }

        CharSetNode* toReturn = For(allocator, level);

        if (l > 0)
        {
            AssertVerify(toReturn->Set(allocator, level, 0, l - 1) == toReturn);
        }

        if (h < lim(level))
        {
            AssertVerify(toReturn->Set(allocator, level, h + 1, lim(level)) == toReturn);
        }

        return toReturn;
    }

    CharSetNode* CharSetFull::UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other)
    {
        return this;
    }

    bool CharSetFull::Get(uint level, uint k) const
    {
        return true;
    }

    void CharSetFull::ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const
    {
        // Empty, so add nothing
    }

    void CharSetFull::ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<wchar_t>& result) const
    {
        this->ToEquivClass<wchar_t>(allocator, level, base, tblidx, result);
    }

    void CharSetFull::ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const
    {
        this->ToEquivClass<codepoint_t>(allocator, level, base, tblidx, result, baseOffset);
    }

    template <typename C>
    void CharSetFull::ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset) const
    {
        uint l = base + (CharSetNode::levels - 1 == level ? 0xff : 0) + baseOffset;
        uint h = base + lim(level) + baseOffset;

        do
        {
            uint acth;
            C equivl[CaseInsensitive::EquivClassSize];
            CaseInsensitive::RangeToEquivClass(tblidx, l, h, acth, equivl);
            uint n = acth - l;
            for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
            {
                result.SetRange(allocator, equivl[i], Chars<C>::Shift(equivl[i], n));
            }

            // Go around again for rest of this range
            l = acth + 1;
        }
        while (l <= h);
    }

    bool CharSetFull::IsSubsetOf(uint level, const CharSetNode* other) const
    {
        Assert(other != nullptr);
        return other == TheFullNode;
    }

    bool CharSetFull::IsEqualTo(uint level, const CharSetNode* other) const
    {
        Assert(other != nullptr);
        return other == TheFullNode;
    }

    uint CharSetFull::Count(uint level) const
    {
        return lim(level) + 1;
    }

    _Success_(return)
    bool CharSetFull::GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const
    {
        Assert(searchCharStart < this->Count(level));

        *outLowerChar = searchCharStart;
        *outHigherChar = (Char)this->Count(level) - 1;

        return true;
    }

#if DBG
    bool CharSetFull::IsLeaf() const
    {
        return false;
    }
#endif

    // ----------------------------------------------------------------------
    // CharSetInner
    // ----------------------------------------------------------------------

    CharSetInner::CharSetInner()
    {
        for (uint i = 0; i < branchingPerInnerLevel; i++)
            children[i] = 0;
    }

    void CharSetInner::FreeSelf(ArenaAllocator* allocator)
    {
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != 0)
            {
                children[i]->FreeSelf(allocator);
#if DBG
                children[i] = 0;
#endif
            }
        }
        Adelete(allocator, this);
    }

    CharSetNode* CharSetInner::Clone(ArenaAllocator* allocator) const
    {
        CharSetInner* res = Anew(allocator, CharSetInner);
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != 0)
                res->children[i] = children[i]->Clone(allocator);
        }
        return res;
    }

    CharSetNode* CharSetInner::ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h)
    {
        Assert(level > 0);
        AssertMsg(h <= lim(level), "The range for clearing provided is invalid for this level.");
        AssertMsg(l <= h, "Can't clear where lover is bigger than the higher.");
        if (l == 0 && h == lim(level))
        {
            return nullptr;
        }

        uint lowerIndex = innerIdx(level, l);
        uint higherIndex = innerIdx(level--, h);
        l = l & lim(level);
        h = h & lim(level);
        if (lowerIndex == higherIndex)
        {
            if (children[lowerIndex] != nullptr)
            {
                children[lowerIndex] = children[lowerIndex]->ClearRange(allocator, level, l, h);
            }
        }
        else
        {
            if (children[lowerIndex] != nullptr)
            {
                children[lowerIndex] = children[lowerIndex]->ClearRange(allocator, level, l, lim(level));
            }

            for (uint i = lowerIndex + 1; i < higherIndex; i++)
            {
                if (children[i] != nullptr)
                {
                    children[i]->FreeSelf(allocator);
                }

                children[i] = nullptr;
            }

            if (children[higherIndex] != nullptr)
            {
                children[higherIndex] = children[higherIndex]->ClearRange(allocator, level, 0, h);
            }
        }
        for (int i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != nullptr)
            {
                return this;
            }
        }

        return nullptr;
    }

    CharSetNode* CharSetInner::Set(ArenaAllocator* allocator, uint level, uint l, uint h)
    {
        Assert(level > 0);
        uint li = innerIdx(level, l);
        uint hi = innerIdx(level--, h);
        bool couldBeFull = true;
        if (li == hi)
        {
            if (children[li] == nullptr)
            {
                if (remain(level, l) == 0 && remain(level, h + 1) == 0)
                    children[li] = CharSetFull::TheFullNode;
                else
                {
                    children[li] = For(allocator, level);
                    children[li] = children[li]->Set(allocator, level, l, h);
                    couldBeFull = false;
                }
            }
            else
                children[li] = children[li]->Set(allocator, level, l, h);
        }
        else
        {
            if (children[li] == nullptr)
            {
                if (remain(level, l) == 0)
                    children[li] = CharSetFull::TheFullNode;
                else
                {
                    children[li] = For(allocator, level);
                    children[li] = children[li]->Set(allocator, level, l, lim(level));
                    couldBeFull = false;
                }
            }
            else
                children[li] = children[li]->Set(allocator, level, l, lim(level));
            for (uint i = li + 1; i < hi; i++)
            {
                if (children[i] != nullptr)
                    children[i]->FreeSelf(allocator);
                children[i] = CharSetFull::TheFullNode;
            }
            if (children[hi] == nullptr)
            {
                if (remain(level, h + 1) == 0)
                    children[hi] = CharSetFull::TheFullNode;
                else
                {
                    children[hi] = For(allocator, level);
                    children[hi] = children[hi]->Set(allocator, level, 0, h);
                    couldBeFull = false;
                }
            }
            else
                children[hi] = children[hi]->Set(allocator, level, 0, h);
        }
        if (couldBeFull)
        {
            for (uint i = 0; i < branchingPerInnerLevel; i++)
            {
                if (children[i] != CharSetFull::TheFullNode)
                    return this;
            }
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    CharSetNode* CharSetInner::UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other)
    {
        Assert(level > 0);
        Assert(other != nullptr && other != CharSetFull::TheFullNode && !other->IsLeaf());
        CharSetInner* otherInner = (CharSetInner*)other;
        level--;
        bool isFull = true;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (otherInner->children[i] != nullptr)
            {
                if (otherInner->children[i] == CharSetFull::TheFullNode)
                {
                    if (children[i] != nullptr)
                        children[i]->FreeSelf(allocator);
                    children[i] = CharSetFull::TheFullNode;
                }
                else
                {
                    if (children[i] == nullptr)
                        children[i] = For(allocator, level);
                    children[i] = children[i]->UnionInPlace(allocator, level, otherInner->children[i]);
                    if (children[i] != CharSetFull::TheFullNode)
                        isFull = false;
                }
            }
            else if (children[i] != CharSetFull::TheFullNode)
                isFull = false;
        }
        if (isFull)
        {
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    bool CharSetInner::Get(uint level, uint k) const
    {
        Assert(level > 0);
        uint i = innerIdx(level--, k);
        if (children[i] == nullptr)
            return false;
        else
            return children[i]->Get(level, k);
    }

    void CharSetInner::ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const
    {
        Assert(level > 0);
        level--;
        uint delta = lim(level) + 1;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] == nullptr)
                // Caution: Part of the range for this child may overlap with direct vector
                result.SetRange(allocator, UTC(max(base, directSize)), UTC(base + delta - 1));
            else
                children[i]->ToComplement(allocator, level, base, result);
            base += delta;
        }
    }

    void CharSetInner::ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<wchar_t>& result) const
    {
        Assert(level > 0);
        level--;
        uint delta = lim(level) + 1;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != nullptr)
            {
                children[i]->ToEquivClassW(allocator, level, base, tblidx, result);
            }
            base += delta;
        }
    }

    void CharSetInner::ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const
    {
        Assert(level > 0);
        level--;
        uint delta = lim(level) + 1;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != nullptr)
            {
                children[i]->ToEquivClassCP(allocator, level, base, tblidx, result, baseOffset);
            }
            base += delta;
        }
    }

    bool CharSetInner::IsSubsetOf(uint level, const CharSetNode* other) const
    {
        Assert(level > 0);
        Assert(other != nullptr && !other->IsLeaf());
        if (other == CharSetFull::TheFullNode)
            return true;
        level--;
        const CharSetInner* otherInner = (CharSetInner*)other;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != nullptr)
            {
                if (otherInner->children[i] == nullptr)
                    return false;
                if (children[i]->IsSubsetOf(level, otherInner->children[i]))
                    return false;
            }
        }
        return true;
    }

    bool CharSetInner::IsEqualTo(uint level, const CharSetNode* other) const
    {
        Assert(level > 0);
        Assert(other != nullptr && !other->IsLeaf());
        if (other == CharSetFull::TheFullNode)
            return false;
        level--;
        const CharSetInner* otherInner = (CharSetInner*)other;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != 0)
            {
                if (otherInner->children[i] == nullptr)
                    return false;
                if (children[i]->IsSubsetOf(level, otherInner->children[i]))
                    return false;
            }
        }
        return true;
    }

    uint CharSetInner::Count(uint level) const
    {
        uint n = 0;
        Assert(level >  0);
        level--;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {
            if (children[i] != nullptr)
                n += children[i]->Count(level);
        }
        return n;
    }

    _Success_(return)
    bool CharSetInner::GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const
    {
        Assert(searchCharStart < this->lim(level) + 1);
        uint innerIndex = innerIdx(level--, searchCharStart);

        Char currentLowChar = 0, currentHighChar = 0;

        for (; innerIndex < branchingPerInnerLevel; innerIndex++)
        {
            if (children[innerIndex] != nullptr && children[innerIndex]->GetNextRange(level, (Char)remain(level, searchCharStart), &currentLowChar, &currentHighChar))
            {
                break;
            }

            if (innerIndex < branchingPerInnerLevel - 1)
            {
                searchCharStart = (Char)indexToValue(level + 1, innerIndex + 1, 0);
            }
        }

        if (innerIndex == branchingPerInnerLevel)
        {
            return false;
        }

        currentLowChar = (Char)indexToValue(level + 1, innerIndex, currentLowChar);
        currentHighChar = (Char)indexToValue(level + 1, innerIndex, currentHighChar);

        innerIndex += 1;

        for (; remain(level, currentHighChar) == lim(level) && innerIndex < branchingPerInnerLevel; innerIndex++)
        {
            Char tempLower, tempHigher;
            if (children[innerIndex] == nullptr || !children[innerIndex]->GetNextRange(level, 0x0, &tempLower, &tempHigher) || remain(level, tempLower) != 0)
            {
                break;
            }

            currentHighChar = (Char)indexToValue(level + 1, innerIndex, tempHigher);
        }

        *outLowerChar = currentLowChar;
        *outHigherChar = currentHighChar;

        return true;
    }

#if DBG
    bool CharSetInner::IsLeaf() const
    {
        return false;
    }
#endif

    // ----------------------------------------------------------------------
    // CharSetLeaf
    // ----------------------------------------------------------------------

    CharSetLeaf::CharSetLeaf()
    {
        vec.Clear();
    }

    void CharSetLeaf::FreeSelf(ArenaAllocator* allocator)
    {
        Adelete(allocator, this);
    }

    CharSetNode* CharSetLeaf::Clone(ArenaAllocator* allocator) const
    {
        return Anew(allocator, CharSetLeaf, *this);
    }

    CharSetNode* CharSetLeaf::Set(ArenaAllocator* allocator, uint level, uint l, uint h)
    {
        Assert(level == 0);
        vec.SetRange(leafIdx(l), leafIdx(h));
        if (vec.IsFull())
        {
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    CharSetNode* CharSetLeaf::ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h)
    {
        Assert(level == 0);
        AssertMsg(h <= lim(level), "The range for clearing provided is invalid for this level.");
        AssertMsg(l <= h, "Can't clear where lover is bigger than the higher.");
        if (l == 0 && h == lim(level))
        {
            return nullptr;
        }

        vec.ClearRange(leafIdx(l), leafIdx(h));

        if (vec.IsEmpty())
        {
            FreeSelf(allocator);
            return nullptr;
        }

        return this;
    }

    CharSetNode* CharSetLeaf::UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other)
    {
        Assert(level == 0);
        Assert(other != nullptr && other->IsLeaf());
        CharSetLeaf* otherLeaf = (CharSetLeaf*)other;
        if (vec.UnionInPlaceFullCheck(otherLeaf->vec))
        {
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    bool CharSetLeaf::Get(uint level, uint k) const
    {
        Assert(level == 0);
        return vec.Get(leafIdx(k));
    }

    void CharSetLeaf::ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const
    {
        Assert(level == 0);
        vec.ToComplement<wchar_t>(allocator, base, result);
    }

    void CharSetLeaf::ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<wchar_t>& result) const
    {
        this->ToEquivClass<wchar_t>(allocator, level, base, tblidx, result);
    }

    void CharSetLeaf::ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const
    {
        this->ToEquivClass<codepoint_t>(allocator, level, base, tblidx, result, baseOffset);
    }

    template <typename C>
    void CharSetLeaf::ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset) const
    {
        Assert(level == 0);
        vec.ToEquivClass<C>(allocator, base, tblidx, result, baseOffset);
    }

    bool CharSetLeaf::IsSubsetOf(uint level, const CharSetNode* other) const
    {
        Assert(level == 0);
        Assert(other != nullptr);
        if (other == CharSetFull::TheFullNode)
            return true;
        Assert(other->IsLeaf());
        CharSetLeaf* otherLeaf = (CharSetLeaf*)other;
        return vec.IsSubsetOf(otherLeaf->vec);
    }

    bool CharSetLeaf::IsEqualTo(uint level, const CharSetNode* other) const
    {
        Assert(level == 0);
        Assert(other != nullptr);
        if (other == CharSetFull::TheFullNode)
            return false;
        Assert(other->IsLeaf());
        CharSetLeaf* otherLeaf = (CharSetLeaf*)other;
        return vec.IsSubsetOf(otherLeaf->vec);
    }

    uint CharSetLeaf::Count(uint level) const
    {
        Assert(level == 0);
        return vec.Count();
    }

    _Success_(return)
    bool CharSetLeaf::GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const
    {
        Assert(searchCharStart < lim(level) + 1);
        int nextSet = vec.NextSet(searchCharStart);

        if (nextSet == -1)
        {
            return false;
        }

        *outLowerChar = (wchar_t)nextSet;

        int nextClear = vec.NextClear(nextSet);

        *outHigherChar = UTC(nextClear == -1 ? lim(level) : nextClear - 1);

        return true;
    }

#if DBG
    bool CharSetLeaf::IsLeaf() const
    {
        return true;
    }
#endif

    // ----------------------------------------------------------------------
    // CharSet<wchar_t>
    // ----------------------------------------------------------------------

    void CharSet<wchar_t>::SwitchRepresentations(ArenaAllocator* allocator)
    {
        Assert(IsCompact());
        uint existCount = this->GetCompactLength();
        __assume(existCount <= MaxCompact);
        if (existCount <= MaxCompact)
        {
            Char existCs[MaxCompact];
            for (uint i = 0; i < existCount; i++)
            {
                existCs[i] = GetCompactChar(i);
            }
            rep.full.root = nullptr;
            rep.full.direct.Clear();
            for (uint i = 0; i < existCount; i++)
                Set(allocator, existCs[i]);
        }
    }

    void CharSet<wchar_t>::Sort()
    {
        Assert(IsCompact());
        __assume(this->GetCompactLength() <= MaxCompact);
        for (uint i = 1; i < this->GetCompactLength(); i++)
        {
            uint curr = GetCompactCharU(i);
            for (uint j = 0; j < i; j++)
            {
                if (GetCompactCharU(j) > curr)
                {
                    for (int k = i; k > (int)j; k--)
                    {
                        this->ReplaceCompactCharU(k, this->GetCompactCharU(k - 1));
                    }
                    this->ReplaceCompactCharU(j, curr);
                    break;
                }
            }
        }
    }

    CharSet<wchar_t>::CharSet()
    {
        Assert(sizeof(Node*) == sizeof(size_t));
        Assert(sizeof(CompactRep) == sizeof(FullRep));
        rep.compact.countPlusOne = 1;
        for (int i = 0; i < MaxCompact; i++)
            rep.compact.cs[i] = emptySlot;
    }

    void CharSet<wchar_t>::FreeBody(ArenaAllocator* allocator)
    {
        if (!IsCompact() && rep.full.root != nullptr)
        {
            rep.full.root->FreeSelf(allocator);
#if DBG
            rep.full.root = nullptr;
#endif
        }
    }

    void CharSet<wchar_t>::Clear(ArenaAllocator* allocator)
    {
        if (!IsCompact() && rep.full.root != nullptr)
            rep.full.root->FreeSelf(allocator);
        rep.compact.countPlusOne = 1;
        for (int i = 0; i < MaxCompact; i++)
            rep.compact.cs[i] = emptySlot;
    }

    void CharSet<wchar_t>::CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other)
    {
        Clear(allocator);
        Assert(IsCompact());
        if (other.IsCompact())
        {
            this->SetCompactLength(other.GetCompactLength());
            for (uint i = 0; i < other.GetCompactLength(); i++)
            {
                this->ReplaceCompactCharU(i, other.GetCompactCharU(i));
            }
        }
        else
        {
            rep.full.root = other.rep.full.root == nullptr ? nullptr : other.rep.full.root->Clone(allocator);
            rep.full.direct.CloneFrom(other.rep.full.direct);
        }
    }

    void CharSet<wchar_t>::CloneNonSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<Char>& other)
    {
        if (this->IsCompact())
        {
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {
                Char c = this->GetCompactChar(i);
                uint uChar = CTU(c);
                if (uChar < 0xD800 || uChar > 0xDFFF)
                {
                    other.Set(allocator, c);
                }
            }
        }
        else
        {
            other.rep.full.direct.CloneFrom(rep.full.direct);
            if (rep.full.root == nullptr)
            {
                other.rep.full.root = nullptr;
            }
            else
            {
                other.rep.full.root = rep.full.root->Clone(allocator);
                other.rep.full.root->ClearRange(allocator, CharSetNode::levels - 1, 0xD800, 0XDFFF);
            }
        }
    }

    void CharSet<wchar_t>::CloneSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<Char>& other)
    {
        if (this->IsCompact())
        {
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {
                Char c = this->GetCompactChar(i);
                uint uChar = CTU(c);
                if (0xD800 <= uChar && uChar <= 0xDFFF)
                {
                    other.Set(allocator, c);
                }
            }
        }
        else
        {
            other.rep.full.direct.CloneFrom(rep.full.direct);
            if (rep.full.root == nullptr)
            {
                other.rep.full.root = nullptr;
            }
            else
            {
                other.rep.full.root = rep.full.root->Clone(allocator);
                other.rep.full.root->ClearRange(allocator, CharSetNode::levels - 1, 0, 0xD7FF);
            }
        }
    }


    void CharSet<wchar_t>::SubtractRange(ArenaAllocator* allocator, Char lowerChar, Char higherChar)
    {
        uint lowerValue = CTU(lowerChar);
        uint higherValue = CTU(higherChar);

        if (higherValue < lowerValue)
            return;

        if (IsCompact())
        {
            for (uint i = 0; i < this->GetCompactLength(); )
            {
                uint value = this->GetCompactCharU(i);

                if (value >= lowerValue && value <= higherValue)
                {
                    this->RemoveCompactChar(i);
                }
                else
                {
                    i++;
                }
            }
        }
        else if(lowerValue == 0 && higherValue == MaxUChar)
        {
            this->Clear(allocator);
        }
        else
        {
            if (lowerValue < CharSetNode::directSize)
            {
                uint maxDirectValue = min(higherValue, CharSetNode::directSize - 1);
                rep.full.direct.ClearRange(lowerValue, maxDirectValue);
            }

            if (rep.full.root != nullptr)
            {
                rep.full.root = rep.full.root->ClearRange(allocator, CharSetNode::levels - 1, lowerValue, higherValue);
            }
        }
    }

    void CharSet<wchar_t>::SetRange(ArenaAllocator* allocator, Char lc, Char hc)
    {
        uint l = CTU(lc);
        uint h = CTU(hc);
        if (h < l)
            return;

        if (IsCompact())
        {
            if (h - l < MaxCompact)
            {
                do
                {
                    uint i;
                    for (i = 0; i < this->GetCompactLength(); i++)
                    {
                        __assume(l <= MaxUChar);
                        if (l <= MaxUChar && i < MaxCompact)
                        {
                            if (this->GetCompactCharU(i) == l)
                                break;
                        }
                    }
                    if (i == this->GetCompactLength())
                    {
                        // Character not already in compact set
                        if (i < MaxCompact)
                        {
                            this->AddCompactCharU(l);
                        }
                        else
                            // Must switch representations
                            break;
                    }
                    l++;
                }
                while (l <= h);
                if (h < l)
                    // All chars are now in compact set
                    return;
                // else: fall-through to general case for remaining chars
            }
            // else: no use even trying

            SwitchRepresentations(allocator);
        }

        Assert(!IsCompact());

        if (l == 0 && h == MaxUChar)
        {
            rep.full.direct.SetRange(0, CharSetNode::directSize - 1);
            if (rep.full.root != nullptr)
                rep.full.root->FreeSelf(allocator);
            rep.full.root = CharSetFull::TheFullNode;
        }
        else
        {
            if (l < CharSetNode::directSize)
            {
                if (h < CharSetNode::directSize)
                {
                    rep.full.direct.SetRange(l, h);
                    return;
                }
                rep.full.direct.SetRange(l, CharSetNode::directSize - 1);
                l = CharSetNode::directSize;
            }

            if (rep.full.root == nullptr)
                rep.full.root = Anew(allocator, CharSetInner);
            rep.full.root = rep.full.root->Set(allocator, CharSetNode::levels - 1, l, h);
        }
    }

    void CharSet<wchar_t>::SetRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {
        for (int i = 0; i < numSortedPairs * 2; i += 2)
        {
            Assert(i == 0 || sortedPairs[i-1] < sortedPairs[i]);
            Assert(sortedPairs[i] <= sortedPairs[i+1]);
            SetRange(allocator, sortedPairs[i], sortedPairs[i+1]);
        }
    }

    void CharSet<wchar_t>::SetNotRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {
        if (numSortedPairs == 0)
            SetRange(allocator, MinChar, MaxChar);
        else
        {
            if (sortedPairs[0] != MinChar)
                SetRange(allocator, MinChar, sortedPairs[0] - 1);
            for (int i = 1; i < numSortedPairs * 2 - 1; i += 2)
                SetRange(allocator, sortedPairs[i] + 1, sortedPairs[i+1] - 1);
            if (sortedPairs[numSortedPairs * 2 - 1] != MaxChar)
                SetRange(allocator, sortedPairs[numSortedPairs * 2 - 1] + 1, MaxChar);
        }
    }

    void CharSet<wchar_t>::UnionInPlace(ArenaAllocator* allocator, const CharSet<Char>& other)
    {
        if (other.IsCompact())
        {
            for (uint i = 0; i < other.GetCompactLength(); i++)
            {
                Set(allocator, other.GetCompactChar(i));
            }
            return;
        }

        if (IsCompact())
            SwitchRepresentations(allocator);

        Assert(!IsCompact() && !other.IsCompact());

        rep.full.direct.UnionInPlace(other.rep.full.direct);

        if (other.rep.full.root != nullptr)
        {
            if (other.rep.full.root == CharSetFull::TheFullNode)
            {
                if (rep.full.root != nullptr)
                    rep.full.root->FreeSelf(allocator);
                rep.full.root = CharSetFull::TheFullNode;
            }
            else
            {
                if (rep.full.root == nullptr)
                    rep.full.root = Anew(allocator, CharSetInner);
                rep.full.root = rep.full.root->UnionInPlace(allocator, CharSetNode::levels - 1, other.rep.full.root);
            }
        }
    }
    _Success_(return)
    bool CharSet<wchar_t>::GetNextRange(Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar)
    {
        int count = this->Count();
        if (count == 0)
        {
            return false;
        }
        else if (count == 1)
        {
            Char singleton = this->Singleton();
            if (singleton < searchCharStart)
            {
                return false;
            }

            *outLowerChar = *outHigherChar = singleton;

            return true;
        }

        if (IsCompact())
        {
            this->Sort();
            uint i = 0;
            size_t compactLength = this->GetCompactLength();
            for (; i < compactLength; i++)
            {
                Char nextChar = this->GetCompactChar(i);
                if (nextChar >= searchCharStart)
                {
                    *outLowerChar = *outHigherChar = nextChar;
                    break;
                }
            }

            if (i == compactLength)
            {
                return false;
            }

            i++;

            for (; i < compactLength; i++)
            {
                Char nextChar = this->GetCompactChar(i);
                if (nextChar != *outHigherChar + 1)
                {
                    return true;
                }
                *outHigherChar += 1;
            }

            return true;
        }
        else
        {
            bool found = false;
            if (CTU(searchCharStart) < CharSetNode::directSize)
            {
                int nextSet = rep.full.direct.NextSet(searchCharStart);

                if (nextSet != -1)
                {
                    found = true;

                    *outLowerChar = (wchar_t)nextSet;

                    int nextClear = rep.full.direct.NextClear(nextSet);

                    if (nextClear != -1)
                    {
                        *outHigherChar = UTC(nextClear - 1);
                        return true;
                    }

                    *outHigherChar = CharSetNode::directSize - 1;
                }
            }

            if (rep.full.root == nullptr)
            {
                return found;
            }
            Char tempLowChar = 0, tempHighChar = 0;

            if (found)
            {
                searchCharStart = *outHigherChar + 1;
            }
            else
            {
                searchCharStart = searchCharStart > CharSetNode::directSize ? searchCharStart : CharSetNode::directSize;
            }

            if (rep.full.root->GetNextRange(CharSetNode::levels - 1, searchCharStart, &tempLowChar, &tempHighChar) && (!found || tempLowChar == *outHigherChar + 1))
            {
                if (!found)
                {
                    *outLowerChar = tempLowChar;
                }
                *outHigherChar = tempHighChar;
                return true;
            }

            return found;
        }
    }

    bool CharSet<wchar_t>::Get_helper(uint k) const
    {
        Assert(!IsCompact());
        CharSetNode* curr = rep.full.root;
        for (int level = CharSetNode::levels - 1; level > 0; level--)
        {
            if (curr == CharSetFull::TheFullNode)
                return true;
            CharSetInner* inner = (CharSetInner*)curr;
            uint i = CharSetNode::innerIdx(level, k);
            if (inner->children[i] == 0)
                return false;
            else
                curr = inner->children[i];
        }
        if (curr == CharSetFull::TheFullNode)
            return true;
        CharSetLeaf* leaf = (CharSetLeaf*)curr;
        return leaf->vec.Get(CharSetNode::leafIdx(k));
    }

    void CharSet<wchar_t>::ToComplement(ArenaAllocator* allocator, CharSet<Char>& result)
    {
        if (IsCompact())
        {
            Sort();
            if (this->GetCompactLength() > 0)
            {
                if (this->GetCompactCharU(0) > 0)
                    result.SetRange(allocator, UTC(0), UTC(this->GetCompactCharU(0) - 1));
                for (uint i = 0; i < this->GetCompactLength() - 1; i++)
                {
                    result.SetRange(allocator, UTC(this->GetCompactCharU(i) + 1), UTC(this->GetCompactCharU(i + 1) - 1));
                }
                if (this->GetCompactCharU(this->GetCompactLength() - 1) < MaxUChar)
                {
                    result.SetRange(allocator, UTC(this->GetCompactCharU(this->GetCompactLength() - 1) + 1), UTC(MaxUChar));
                }
            }
            else if (this->GetCompactLength() == 0)
            {
                result.SetRange(allocator, UTC(0), UTC(MaxUChar));
            }
        }
        else
        {
            rep.full.direct.ToComplement<wchar_t>(allocator, 0, result);
            if (rep.full.root == nullptr)
                result.SetRange(allocator, UTC(CharSetNode::directSize), MaxChar);
            else
                rep.full.root->ToComplement(allocator, CharSetNode::levels - 1, 0, result);
        }
    }

    void CharSet<wchar_t>::ToEquivClass(ArenaAllocator* allocator, CharSet<Char>& result)
    {
        uint tblidx = 0;
        if (IsCompact())
        {
            Sort();
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {
                uint acth;
                Char equivs[CaseInsensitive::EquivClassSize];
                if (CaseInsensitive::RangeToEquivClass(tblidx, this->GetCompactCharU(i), this->GetCompactCharU(i), acth, equivs))
                {
                    for (int j = 0; j < CaseInsensitive::EquivClassSize; j++)
                    {
                        result.Set(allocator, equivs[j]);
                    }
                }
                else
                {
                    result.Set(allocator, this->GetCompactChar(i));
                }
            }
        }
        else
        {
            rep.full.direct.ToEquivClass<wchar_t>(allocator, 0, tblidx, result);
            if (rep.full.root != nullptr)
            {
                rep.full.root->ToEquivClassW(allocator, CharSetNode::levels - 1, 0, tblidx, result);
            }
        }
    }

    void CharSet<wchar_t>::ToEquivClassCP(ArenaAllocator* allocator, CharSet<codepoint_t>& result, codepoint_t baseOffset)
    {
        uint tblidx = 0;
        if (IsCompact())
        {
            Sort();
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {
                uint acth;
                codepoint_t equivs[CaseInsensitive::EquivClassSize];
                if (CaseInsensitive::RangeToEquivClass(tblidx, this->GetCompactCharU(i) + baseOffset, this->GetCompactCharU(i) + baseOffset, acth, equivs))
                {
                    for (int j = 0; j < CaseInsensitive::EquivClassSize; j++)
                    {
                        result.Set(allocator, equivs[j]);
                    }
                }
                else
                {
                    result.Set(allocator, this->GetCompactChar(i) + baseOffset);
                }
            }
        }
        else
        {
            rep.full.direct.ToEquivClass<codepoint_t>(allocator, 0, tblidx, result, baseOffset);
            if (rep.full.root != nullptr)
            {
                rep.full.root->ToEquivClassCP(allocator, CharSetNode::levels - 1, 0, tblidx, result, baseOffset);
            }
        }
    }

    int CharSet<wchar_t>::GetCompactEntries(uint max, __out_ecount(max) Char* entries) const
    {
        Assert(max <= MaxCompact);
        if (!IsCompact())
            return -1;

        uint count = min(max, (uint)(this->GetCompactLength()));
        __analysis_assume(count <= max);
        for (uint i = 0; i < count; i++)
        {
            // Bug in oacr. it can't figure out count is less than or equal to max
#pragma warning(suppress: 22102)
            entries[i] = this->GetCompactChar(i);
        }
        return static_cast<int>(rep.compact.countPlusOne - 1);
    }

    bool CharSet<wchar_t>::IsSubsetOf(const CharSet<Char>& other) const
    {
        if (IsCompact())
        {
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {
                if (!other.Get(this->GetCompactChar(i)))
                    return false;
            }
            return true;
        }
        else
        {
            if (other.IsCompact())
                return false;
            if (!rep.full.direct.IsSubsetOf(other.rep.full.direct))
                return false;
            if (rep.full.root == nullptr)
                return true;
            if (other.rep.full.root == nullptr)
                return false;
            return rep.full.root->IsSubsetOf(CharSetNode::levels - 1, other.rep.full.root);
        }
    }

    bool CharSet<wchar_t>::IsEqualTo(const CharSet<Char>& other) const
    {
        if (IsCompact())
        {
            if (!other.IsCompact())
                return false;
            if (rep.compact.countPlusOne != other.rep.compact.countPlusOne)
                return false;
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {
                if (!other.Get(this->GetCompactChar(i)))
                    return false;
            }
            return true;
        }
        else
        {
            if (other.IsCompact())
                return false;
            if (!rep.full.direct.IsEqualTo(other.rep.full.direct))
                return false;
            if ((rep.full.root == nullptr) != (other.rep.full.root == nullptr))
                return false;
            if (rep.full.root == nullptr)
                return true;
            return rep.full.root->IsEqualTo(CharSetNode::levels - 1, other.rep.full.root);
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    // CAUTION: This method is very slow.
    void CharSet<wchar_t>::Print(DebugWriter* w) const
    {
        w->Print(L"[");
        int start = -1;
        for (uint i = 0; i < NumChars; i++)
        {
            if (Get(UTC(i)))
            {
                if (start < 0)
                {
                    start = i;
                    w->PrintEscapedChar(UTC(i));
                }
            }
            else
            {
                if (start >= 0)
                {
                    if (i > (uint)(start + 1))
                    {
                        if (i  > (uint)(start + 2))
                            w->Print(L"-");
                        w->PrintEscapedChar(UTC(i - 1));
                    }
                    start = -1;
                }
            }
        }
        if (start >= 0)
        {
            if ((uint)start < MaxUChar - 1)
                w->Print(L"-");
            w->PrintEscapedChar(MaxChar);
        }
        w->Print(L"]");
    }
#endif

    // ----------------------------------------------------------------------
    // CharSet<codepoint_t>
    // ----------------------------------------------------------------------
    CharSet<codepoint_t>::CharSet()
    {
#if DBG
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].IsEmpty();
        }
#endif
    }

    void CharSet<codepoint_t>::FreeBody(ArenaAllocator* allocator)
    {
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].FreeBody(allocator);
        }
    }

    void CharSet<codepoint_t>::Clear(ArenaAllocator* allocator)
    {
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].Clear(allocator);
        }
    }

    void CharSet<codepoint_t>::CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other)
    {
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].Clear(allocator);
            this->characterPlanes[i].CloneFrom(allocator, other.characterPlanes[i]);
        }
    }

    void CharSet<codepoint_t>::CloneSimpleCharsTo(ArenaAllocator* allocator, CharSet<wchar_t>& other) const
    {
        other.CloneFrom(allocator, this->characterPlanes[0]);
    }

    void CharSet<codepoint_t>::SetRange(ArenaAllocator* allocator, Char lc, Char hc)
    {
        Assert(lc <= hc);

        int lowerIndex = this->CharToIndex(lc);
        int upperIndex = this->CharToIndex(hc);

        if (lowerIndex == upperIndex)
        {
            this->characterPlanes[lowerIndex].SetRange(allocator, this->RemoveOffset(lc), this->RemoveOffset(hc));
        }
        else
        {
            // Do the partial ranges
            wchar_t partialLower = this->RemoveOffset(lc);
            wchar_t partialHigher = this->RemoveOffset(hc);

            if (partialLower != 0)
            {
                this->characterPlanes[lowerIndex].SetRange(allocator, partialLower, Chars<wchar_t>::MaxUChar);
                lowerIndex++;
            }

            for(; lowerIndex < upperIndex; lowerIndex++)
            {
                this->characterPlanes[lowerIndex].SetRange(allocator, 0, Chars<wchar_t>::MaxUChar);
            }

            this->characterPlanes[upperIndex].SetRange(allocator, 0, partialHigher);
        }
    }

    void CharSet<codepoint_t>::SetRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {
        for (int i = 0; i < numSortedPairs * 2; i += 2)
        {
            Assert(i == 0 || sortedPairs[i-1] < sortedPairs[i]);
            Assert(sortedPairs[i] <= sortedPairs[i+1]);
            SetRange(allocator, sortedPairs[i], sortedPairs[i+1]);
        }
    }
    void CharSet<codepoint_t>::SetNotRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {
        if (numSortedPairs == 0)
        {
            for (int i = 0; i < NumberOfPlanes; i++)
            {
                this->characterPlanes[i].SetRange(allocator, 0, Chars<wchar_t>::MaxUChar);
            }
        }
        else
        {
            if (sortedPairs[0] != MinChar)
            {
                SetRange(allocator, MinChar, sortedPairs[0] - 1);
            }

            for (int i = 1; i < numSortedPairs * 2 - 1; i += 2)
            {
                SetRange(allocator, sortedPairs[i] + 1, sortedPairs[i+1] - 1);
            }

            if (sortedPairs[numSortedPairs * 2 - 1] != MaxChar)
            {
                SetRange(allocator, sortedPairs[numSortedPairs * 2 - 1] + 1, MaxChar);
            }
        }
    }
    void CharSet<codepoint_t>::UnionInPlace(ArenaAllocator* allocator, const  CharSet<Char>& other)
    {
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].UnionInPlace(allocator, other.characterPlanes[i]);
        }
    }

    void CharSet<codepoint_t>::UnionInPlace(ArenaAllocator* allocator, const  CharSet<wchar_t>& other)
    {
        this->characterPlanes[0].UnionInPlace(allocator, other);
    }

    _Success_(return)
    bool CharSet<codepoint_t>::GetNextRange(Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar)
    {
        Assert(outLowerChar != nullptr);
        Assert(outHigherChar != nullptr);
        if (searchCharStart >= 0x110000)
        {
            return false;
        }

        wchar_t currentLowChar = 1, currentHighChar = 0;
        int index = this->CharToIndex(searchCharStart);
        wchar_t offsetLessSearchCharStart = this->RemoveOffset(searchCharStart);

        for (; index < NumberOfPlanes; index++)
        {
            if (this->characterPlanes[index].GetNextRange(offsetLessSearchCharStart, &currentLowChar, &currentHighChar))
            {
                break;
            }
            offsetLessSearchCharStart = 0x0;
        }

        if (index == NumberOfPlanes)
        {
            return false;
        }
        Assert(currentHighChar >= currentLowChar);
        // else found range
        *outLowerChar = this->AddOffset(currentLowChar, index);
        *outHigherChar = this->AddOffset(currentHighChar, index);

        // Check if range crosses plane boundaries
        index ++;
        for (; index < NumberOfPlanes; index++)
        {
            if (!this->characterPlanes[index].GetNextRange(0x0, &currentLowChar, &currentHighChar) || *outHigherChar + 1 != this->AddOffset(currentLowChar, index))
            {
                break;
            }
            Assert(this->AddOffset(currentHighChar, index) > *outHigherChar);
            *outHigherChar = this->AddOffset(currentHighChar, index);
        }

        return true;
    }

    void CharSet<codepoint_t>::ToComplement(ArenaAllocator* allocator, CharSet<Char>& result)
    {
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].ToComplement(allocator, result.characterPlanes[i]);
        }
    }

    void CharSet<codepoint_t>::ToSimpleComplement(ArenaAllocator* allocator, CharSet<Char>& result)
    {
        this->characterPlanes[0].ToComplement(allocator, result.characterPlanes[0]);
    }

    void CharSet<codepoint_t>::ToSimpleComplement(ArenaAllocator* allocator, CharSet<wchar_t>& result)
    {
        this->characterPlanes[0].ToComplement(allocator, result);
    }

    void CharSet<codepoint_t>::ToEquivClass(ArenaAllocator* allocator, CharSet<Char>& result)
    {
        for (int i = 0; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].ToEquivClassCP(allocator, result, AddOffset(0, i));
        }
    }

    void CharSet<codepoint_t>::ToSurrogateEquivClass(ArenaAllocator* allocator, CharSet<Char>& result)
    {
        this->CloneSimpleCharsTo(allocator, result.characterPlanes[0]);
        for (int i = 1; i < NumberOfPlanes; i++)
        {
            this->characterPlanes[i].ToEquivClassCP(allocator, result, AddOffset(0, i));
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void CharSet<codepoint_t>::Print(DebugWriter* w) const
    {
        w->Print(L"Characters 0 - 65535");

        for (int i = 0; i < NumberOfPlanes; i++)
        {
            int base = (i + 1) * 0x10000;
            w->Print(L"Characters %d - %d", base, base + 0xFFFF);
            this->characterPlanes[i].Print(w);
        }
    }
#endif

    // ----------------------------------------------------------------------
    // RuntimeCharSet<wchar_t>
    // ----------------------------------------------------------------------

    RuntimeCharSet<wchar_t>::RuntimeCharSet()
    {
        root = nullptr;
        direct.Clear();
    }

    void RuntimeCharSet<wchar_t>::FreeBody(ArenaAllocator* allocator)
    {
        if (root != nullptr)
        {
            root->FreeSelf(allocator);
#if DBG
            root = nullptr;
#endif
        }
    }

    void RuntimeCharSet<wchar_t>::CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other)
    {
        Assert(root == nullptr);
        Assert(direct.Count() == 0);
        if (other.IsCompact())
        {
            for (uint i = 0; i < other.GetCompactLength(); i++)
            {
                uint k = other.GetCompactCharU(i);
                if (k < CharSetNode::directSize)
                    direct.Set(k);
                else
                {
                    if (root == nullptr)
                        root = Anew(allocator, CharSetInner);
#if DBG
                    CharSetNode* newRoot =
#endif
                    root->Set(allocator, CharSetNode::levels - 1, k, k);
#if DBG
                    // NOTE: Since we can add at most MaxCompact characters, we can never fill a leaf or inner node,
                    //       thus we will never need to reallocated nodes
                    Assert(newRoot == root);
#endif
                }
            }
        }
        else
        {
            root = other.rep.full.root == nullptr ? nullptr : other.rep.full.root->Clone(allocator);
            direct.CloneFrom(other.rep.full.direct);
        }
    }

    bool RuntimeCharSet<wchar_t>::Get_helper(uint k) const
    {
        CharSetNode* curr = root;
        for (int level = CharSetNode::levels - 1; level > 0; level--)
        {
            if (curr == CharSetFull::TheFullNode)
                return true;
            CharSetInner* inner = (CharSetInner*)curr;
            uint i = CharSetNode::innerIdx(level, k);
            if (inner->children[i] == 0)
                return false;
            else
                curr = inner->children[i];
        }
        if (curr == CharSetFull::TheFullNode)
            return true;
        CharSetLeaf* leaf = (CharSetLeaf*)curr;
        return leaf->vec.Get(CharSetNode::leafIdx(k));
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    // CAUTION: This method is very slow.
    void RuntimeCharSet<wchar_t>::Print(DebugWriter* w) const
    {
        w->Print(L"[");
        int start = -1;
        for (uint i = 0; i < NumChars; i++)
        {
            if (Get(UTC(i)))
            {
                if (start < 0)
                {
                    start = i;
                    w->PrintEscapedChar(UTC(i));
                }
            }
            else
            {
                if (start >= 0)
                {
                    if (i > (uint)(start + 1))
                    {
                        if (i  > (uint)(start + 2))
                            w->Print(L"-");
                        w->PrintEscapedChar(UTC(i - 1));
                    }
                    start = -1;
                }
            }
        }
        if (start >= 0)
        {
            if ((uint)start < MaxUChar - 1)
                w->Print(L"-");
            w->PrintEscapedChar(MaxChar);
        }
        w->Print(L"]");
    }
#endif

}

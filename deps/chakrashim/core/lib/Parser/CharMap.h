//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    enum CharMapScheme
    {
         CharMapScheme_Linear,
         CharMapScheme_Full
    };
    template <typename C, typename V, CharMapScheme scheme = CharMapScheme_Full>
    class CharMap {};

    template <typename V, CharMapScheme scheme>
    class CharMap<char, V, scheme> : private Chars<char>
    {
    private:
        V map[NumChars];

    public:
        CharMap(V defv)
        {
            for (int i = 0; i < NumChars; i++)
                map[i] = defv;
        }

        void FreeBody(ArenaAllocator* allocator)
        {
        }

        inline void Set(ArenaAllocator* allocator, Char k, V v)
        {
            map[CTU(k)] = v;
        }

        inline V Get(Char k) const
        {
            return map[CTU(k)];
        }
    };

    static const uint MaxCharMapLinearChars = 4;

    template <typename V>
    class CharMap<wchar_t, V, CharMapScheme_Linear> : private Chars<wchar_t>
    {
        template <typename C>
        friend class TextbookBoyerMooreWithLinearMap;
    private:
        V defv;
        uint map[MaxCharMapLinearChars];
        V lastOcc[MaxCharMapLinearChars];

    public:
        CharMap(V defv) : defv(defv)
        {
            for (uint i = 0; i < MaxCharMapLinearChars; i++)
            {
                map[i] = 0;
                lastOcc[i] = defv;
            }
        }

        inline void Set(uint numLinearChars, Char const * map, V const * lastOcc)
        {
            Assert(numLinearChars <= MaxCharMapLinearChars);
            for (uint i = 0; i < numLinearChars; i++)
            {
                this->map[i] = CTU(map[i]);
                this->lastOcc[i] = lastOcc[i];
            }
        }

        uint GetChar(uint index) const
        {
            Assert(index < MaxCharMapLinearChars);
            __analysis_assume(index < MaxCharMapLinearChars);
            return map[index];
        }

        V GetLastOcc(uint index) const
        {
            Assert(index < MaxCharMapLinearChars);
            __analysis_assume(index < MaxCharMapLinearChars);
            return lastOcc[index];
        }

        inline V Get(uint inputChar) const
        {
            if (map[0] == inputChar)
                return lastOcc[0];
            if (map[1] == inputChar)
                return lastOcc[1];
            if (map[2] == inputChar)
                return lastOcc[2];
            if (map[3] == inputChar)
                return lastOcc[3];
            return defv;
        }

        inline V Get(Char k) const
        {
            return Get(CTU(k));
        }
    };


    template <typename V, CharMapScheme scheme>
    class CharMap<wchar_t, V, scheme> : private Chars<wchar_t>
    {
    private:
        static const int directBits = Chars<char>::CharWidth;
        static const int directSize = Chars<char>::NumChars;
        static const int bitsPerLevel = 4;
        static const int branchingPerLevel = 1 << bitsPerLevel;
        static const uint mask = branchingPerLevel - 1;
        static const int levels = CharWidth / bitsPerLevel;

        inline static uint innerIdx(int level, uint v)
        {
            return (v >> (level * bitsPerLevel)) & mask;
        }

        inline static uint leafIdx(uint v)
        {
            return v & mask;
        }

        struct Node
        {
            virtual void FreeSelf(ArenaAllocator* allocator) = 0;
            virtual void Set(ArenaAllocator* allocator, V defv, int level, uint k, V v) = 0;
            virtual V Get(V defv, int level, uint k) const = 0;

            static inline Node* For(ArenaAllocator* allocator, int level, V defv)
            {
                if (level == 0)
                    return Anew(allocator, Leaf, defv);
                else
                    return Anew(allocator, Inner);
            }
        };

        struct Inner : Node
        {
            Node* children[branchingPerLevel];

            Inner()
            {
                for (int i = 0; i < branchingPerLevel; i++)
                    children[i] = 0;
            }

            void FreeSelf(ArenaAllocator* allocator) override
            {
                for (int i = 0; i < branchingPerLevel; i++)
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

            void Set(ArenaAllocator* allocator, V defv, int level, uint k, V v) override
            {
                Assert(level > 0);
                uint i = innerIdx(level--, k);
                if (children[i] == 0)
                {
                    if (v == defv)
                        return;
                    children[i] = For(allocator, level, defv);
                }
                children[i]->Set(allocator, defv, level, k, v);
            }

            V Get(V defv, int level, uint k) const override
            {
                Assert(level > 0);
                uint i = innerIdx(level--, k);
                if (children[i] == 0)
                    return defv;
                else
                    return children[i]->Get(defv, level, k);
            }
        };

        struct Leaf : Node
        {
            V values[branchingPerLevel];

            Leaf(V defv)
            {
                for (int i = 0; i < branchingPerLevel; i++)
                    values[i] = defv;
            }

            void FreeSelf(ArenaAllocator* allocator) override
            {
                Adelete(allocator, this);
            }

            void Set(ArenaAllocator* allocator, V defv, int level, uint k, V v) override
            {
                Assert(level == 0);
                values[leafIdx(k)] = v;
            }

            V Get(V defv, int level, uint k) const override
            {
                Assert(level == 0);
                return values[leafIdx(k)];
            }
        };

        BVStatic<directSize> isInMap;
        V defv;
        V directMap[directSize];
        Node* root;

    public:
        CharMap(V defv)
            : defv(defv)
            , root(0)
        {
            for (int i = 0; i < directSize; i++)
                directMap[i] = defv;
        }

        void FreeBody(ArenaAllocator* allocator)
        {
            if (root != 0)
            {
                root->FreeSelf(allocator);
#if DBG
                root = 0;
#endif
            }
        }

        void Set(ArenaAllocator* allocator, Char kc, V v)
        {
            uint k = CTU(kc);
            if (k < directSize)
            {
                isInMap.Set(k);
                directMap[k] = v;
            }
            else
            {
                if (root == 0)
                {
                    if (v == defv)
                        return;
                    root = Anew(allocator, Inner);
                }
                root->Set(allocator, defv, levels - 1, k, v);
            }
        }

        bool GetNonDirect(uint k, V& lastOcc) const
        {
            Assert(k >= directSize);
            if (root == nullptr)
            {
                return false;
            }
            Node* curr = root;
            for (int level = levels - 1; level > 0; level--)
            {
                Inner* inner = (Inner*)curr;
                uint i = innerIdx(level, k);
                if (inner->children[i] == 0)
                    return false;
                else
                    curr = inner->children[i];
            }
            Leaf* leaf = (Leaf*)curr;
            lastOcc = leaf->values[leafIdx(k)];
            return true;
        }

        uint GetDirectMapSize() const { return directSize; }
        BOOL IsInDirectMap(uint c) const { Assert(c < directSize); return isInMap.Test(c); }
        V GetDirectMap(uint c) const
        {
            Assert(c < directSize);
            __analysis_assume(c < directSize);
            return directMap[c];
        }
        __inline V Get(Char kc) const
        {
            if (CTU(kc) < GetDirectMapSize())
            {
                if (!IsInDirectMap(CTU(kc)))
                {
                    return defv;
                }
                return GetDirectMap(CTU(kc));
            }
            else
            {
                V lastOcc;
                if (!GetNonDirect(CTU(kc), lastOcc))
                {
                    return defv;
                }
                return lastOcc;
            }
        }
    };
}

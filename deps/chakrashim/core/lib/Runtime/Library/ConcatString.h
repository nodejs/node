//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Base class for concat strings.
    // Concat string is a virtual string, or a non-leaf node in concat string tree.
    // It does not hold characters by itself but has one or more child nodes.
    // Only leaf nodes (which are not concat strings) hold the actual characters.
    // The flattening happens on demand (call GetString() or GetSz()),
    // until then we don't create actual big wchar_t* buffer, just keep concat tree as a tree.
    // The result of flattening the concat string tree is concat of all leaf nodes from left to right.
    // Usage pattern:
    //   // Create concat tree using one of non-abstract derived classes.
    //   JavascriptString* result = concatTree->GetString();    // At this time we flatten the tree into 1 actual wchat_t* string.
    class ConcatStringBase abstract : public LiteralString // vtable will be switched to LiteralString's vtable after flattening
    {
        friend JavascriptString;

    protected:
        ConcatStringBase(StaticType* stringTypeStatic);
        DEFINE_VTABLE_CTOR_ABSTRACT(ConcatStringBase, LiteralString);

        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer,
            StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth) = 0;
        void CopyImpl(_Out_writes_(m_charLength) wchar_t *const buffer,
            int itemCount, _In_reads_(itemCount) JavascriptString * const * items,
            StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth);

        // Subclass can call this to implement GetSz and use the actual type to avoid virtual call to Copy.
        template <typename ConcatStringType> const wchar_t * GetSzImpl();
    public:
        virtual const wchar_t* GetSz() = 0;     // Force subclass to call GetSzImpl with the real type to avoid virtual calls
        using JavascriptString::Copy;
        virtual bool IsTree() const override sealed;
    };

    // Concat string with N (or less) child nodes.
    // Use it when you know the number of children at compile time.
    // There could be less nodes than N. When we find 1st NULL node going incrementing the index,
    // we see this as indication that the rest on the right are empty and stop iterating.
    // Usage pattern:
    //   ConcatStringN<3>* tree = ConcatStringN<3>::New(scriptContext);
    //   tree->SetItem(0, javascriptString1);
    //   tree->SetItem(1, javascriptString2);
    //   tree->SetItem(2, javascriptString3);
    template <int N>
    class ConcatStringN : public ConcatStringBase
    {
        friend JavascriptString;

    protected:
        ConcatStringN(StaticType* stringTypeStatic, bool doZeroSlotsAndLength = true);
        DEFINE_VTABLE_CTOR(ConcatStringN<N>, ConcatStringBase);
        DECLARE_CONCRETE_STRING_CLASS;

        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth) override
        {
            __super::CopyImpl(buffer, N, m_slots, nestedStringTreeCopyInfos, recursionDepth);
        }
        virtual int GetRandomAccessItemsFromConcatString(Js::JavascriptString * const *& items) const
        {
            items = m_slots;
            return N;
        }

    public:
        static ConcatStringN<N>* New(ScriptContext* scriptContext);
        const wchar_t * GetSz() override sealed;
        void SetItem(_In_range_(0, N - 1) int index, JavascriptString* value);

    protected:
        JavascriptString* m_slots[N];   // These contain the child nodes. 1 slot is per 1 item (JavascriptString*).
    };

    // Concat string that uses binary tree, each node has 2 children.
    // Usage pattern:
    //   ConcatString* str = ConcatString::New(javascriptString1, javascriptString2);
    // Note: it's preferred you would use the following for concats, that would figure out whether concat string is optimal or create a new string is better.
    //   JavascriptString::Concat(javascriptString1, javascriptString2);
    class ConcatString sealed : public ConcatStringN<2>
    {
        ConcatString(JavascriptString* a, JavascriptString* b);
    protected:
        DEFINE_VTABLE_CTOR(ConcatString, ConcatStringN<2>);
        DECLARE_CONCRETE_STRING_CLASS;
    public:
        static ConcatString* New(JavascriptString* a, JavascriptString* b);
        static const int MaxDepth = 1000;

        JavascriptString *LeftString() const { Assert(!IsFinalized()); return m_slots[0]; }
        JavascriptString *RightString() const { Assert(!IsFinalized()); return m_slots[1]; }
    };

    // Concat string with any number of child nodes, can grow dynamically.
    // Usage pattern:
    //   ConcatStringBuilder* tree = ConcatStringBuider::New(scriptContext, 5);
    //   tree->Append(javascriptString1);
    //   tree->Append(javascriptString2);
    //   ...
    // Implementation details:
    // - uses chunks, max chunk size is specified by c_maxChunkSlotCount, until we fit into that, we realloc, otherwise create new chunk.
    // - We use chunks in order to avoid big allocations, we don't expect lots of reallocs, that why chunk size is relatively big.
    // - the chunks are linked using m_prevChunk field. flattening happens from left to right, i.e. first we need to get
    //   head chunk -- the one that has m_prevChunk == NULL.
    class ConcatStringBuilder sealed : public ConcatStringBase
    {
        friend JavascriptString;
        ConcatStringBuilder(ScriptContext* scriptContext, int initialSlotCount);
        ConcatStringBuilder(const ConcatStringBuilder& other);
        void AllocateSlots(int requestedSlotCount);
        ConcatStringBuilder* GetHead() const;

    protected:
        DEFINE_VTABLE_CTOR(ConcatStringBuilder, ConcatStringBase);
        DECLARE_CONCRETE_STRING_CLASS;
        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth) override sealed;

    public:
        static ConcatStringBuilder* New(ScriptContext* scriptContext, int initialSlotCount);
        const wchar_t * GetSz() override sealed;
        void Append(JavascriptString* str);

    private:
        // MAX number of slots in one chunk. Until we fit into this, we realloc, otherwise create new chunk.
        static const int c_maxChunkSlotCount = 1024;
        int GetItemCount() const;

        JavascriptString** m_slots; // Array of child nodes.
        int m_slotCount;   // Number of allocated slots (1 slot holds 1 item) in this chunk.
        int m_count;       // Actual number of items in this chunk.
        ConcatStringBuilder* m_prevChunk;
    };

    // Concat string that wraps another string.
    // Use it when you need to wrap something with e.g. { and }.
    // Usage pattern:
    //   result = ConcatStringWrapping<L'{', L'}'>::New(result);
    template <wchar_t L, wchar_t R>
    class ConcatStringWrapping sealed : public ConcatStringBase
    {
        friend JavascriptString;
        ConcatStringWrapping(JavascriptString* inner);
        JavascriptString* GetFirstItem() const;
        JavascriptString* GetLastItem() const;

    protected:
        DEFINE_VTABLE_CTOR(ConcatStringWrapping, ConcatStringBase);
        DECLARE_CONCRETE_STRING_CLASS;
        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth) override sealed
        {
            const_cast<ConcatStringWrapping *>(this)->EnsureAllSlots();
            __super::CopyImpl(buffer, _countof(m_slots), m_slots, nestedStringTreeCopyInfos, recursionDepth);
        }
        virtual int GetRandomAccessItemsFromConcatString(Js::JavascriptString * const *& items) const override sealed
        {
            const_cast<ConcatStringWrapping *>(this)->EnsureAllSlots();
            items = m_slots;
            return _countof(m_slots);
        }
    public:
        static ConcatStringWrapping<L, R>* New(JavascriptString* inner);
        const wchar_t * GetSz() override sealed;
    private:
        void EnsureAllSlots()
        {
            m_slots[0] = this->GetFirstItem();
            m_slots[1] = m_inner;
            m_slots[2] = this->GetLastItem();
        }
        JavascriptString * m_inner;

        // Use the padding space for the concat
        JavascriptString * m_slots[3];
    };

    // Make sure the padding doesn't add tot he size of ConcatStringWrapping
#if defined(_M_X64_OR_ARM64)
    CompileAssert(sizeof(ConcatStringWrapping<L'"', L'"'>) == 64);
#else
    CompileAssert(sizeof(ConcatStringWrapping<L'"', L'"'>) == 32);
#endif

    // Concat string with N child nodes. Use it when you don't know the number of children at compile time.
    // Usage pattern:
    //   ConcatStringMulti* tree = ConcatStringMulti::New(3, scriptContext);
    //   tree->SetItem(0, javascriptString1);
    //   tree->SetItem(1, javascriptString2);
    //   tree->SetItem(2, javascriptString3);
    class ConcatStringMulti sealed : public ConcatStringBase
    {
        friend JavascriptString;

    protected:
        ConcatStringMulti(uint slotCount, JavascriptString * a1, JavascriptString * a2, StaticType* stringTypeStatic);
        DEFINE_VTABLE_CTOR(ConcatStringMulti, ConcatStringBase);
        DECLARE_CONCRETE_STRING_CLASS;

        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth) override
        {
            Assert(IsFilled());
            __super::CopyImpl(buffer, slotCount, m_slots, nestedStringTreeCopyInfos, recursionDepth);
        }
        virtual int GetRandomAccessItemsFromConcatString(Js::JavascriptString * const *& items) const
        {
            Assert(IsFilled());
            items = m_slots;
            return slotCount;
        }

    public:
        static ConcatStringMulti * New(uint slotCount, JavascriptString * a1, JavascriptString * a2, ScriptContext* scriptContext);
        const wchar_t * GetSz() override sealed;
        static bool Is(Var var);
        static ConcatStringMulti * FromVar(Var value);
        static size_t GetAllocSize(uint slotCount);
        void SetItem(_In_range_(0, slotCount - 1) uint index, JavascriptString* value);

        static uint32 GetOffsetOfSlotCount() { return offsetof(ConcatStringMulti, slotCount); }
        static uint32 GetOffsetOfSlots() { return offsetof(ConcatStringMulti, m_slots); }
    protected:
        uint slotCount;
        JavascriptString* m_slots[];   // These contain the child nodes.

#if DBG
        bool IsFilled() const;
#endif
    };
}


